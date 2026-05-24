# 移植任务分解 — SPI 上层 Python 解码器 → C 解码器

## 任务总览

5 个解码器按复杂度从低到高排序实施，每个解码器独立完成。

---

## Task 1: ltc242x_c — LTC2421/LTC2422 20-bit ADC（低复杂度）

### 1.1 创建文件

- **文件路径**: `libsigrokdecode/c_decoders/ltc242x_c.c`

### 1.2 实现步骤

1. **编写文件头部** — `#include` 和 license 注释
2. **定义 annotation 枚举** — `ANN_CH0_VOLTAGE=0, ANN_CH1_VOLTAGE, NUM_ANN`
3. **定义状态结构体** `ltc242x_state`
   - `out_ann`, `data` (uint32_t), `ss`, `es`, `vref` (double)
4. **定义静态数据**
   - `ltc242x_ann_labels` — 2 行，第一列空字符串
   - `ltc242x_ann_rows` — 2 行 (ch0_voltages, ch1_voltages)
   - `ltc242x_inputs` = `{"spi", NULL}`
   - `ltc242x_tags` = `{"IC", "Analog/digital", NULL}`
   - `ltc242x_options` — 1 个选项 `vref` (double, default=1.5)
5. **实现 `ltc242x_reset()`** — 分配私有数据，清零状态
6. **实现 `ltc242x_start()`** — 注册 `SRD_OUTPUT_ANN`，读取 `vref` 选项
7. **实现 `ltc242x_decode()`** — 空函数 `(void)di;`
8. **实现 `ltc242x_handle_voltage()`** — 电压计算和输出
   - `raw = data & 0x3FFFFF`
   - `voltage = -(0x200000 - raw) / 0xFFFFF * vref`
   - `channel = (data >> 22) & 1`
   - 输出 `%.6fV` 和 `%.2fV` 两种格式
9. **实现 `ltc242x_recv_proto()`**
   - `CS-CHANGE`: cs_old=0,cs_new=1 → 完成读取，调用 handle_voltage; cs_old=1,cs_new=0 → 记录起始
   - `BITS`: 解析 MISO bits，逐位累积到 `s->data`
   - **关键**: Python 使用 `reversed(miso)`，C 中需要从 BITS 数据提取 miso 部分并按顺序处理
10. **实现 `ltc242x_destroy()`** — 释放私有数据
11. **定义 `ltc242x_c_decoder` 结构体** — 设置 `.recv_proto = ltc242x_recv_proto`
12. **实现 `srd_c_decoder_entry()`** — 初始化 `vref` 选项默认值
13. **实现 `srd_c_decoder_api_version()`**

### 1.3 关键注意事项

- BITS 格式解析：`data[0]=have_mosi`, `data[1..ws]=mosi_bits`, `data[ws+1]=have_miso`, `data[ws+2..]=miso_bits`
- Python `reversed(miso)` 遍历 + `data |= bit[0]; data <<= 1` 等价于 C 中顺序遍历 miso_bits 执行相同操作
- CS-CHANGE 时 `data >>= 1` 修正多余的左移
- `vref` 选项使用 `g_variant_new_double(1.5)` 初始化

### 1.4 验证

- 编译无错误
- 在 PXView 中选择 SPI → ltc242x_c 解码器栈
- 验证 CH0/CH1 电压显示正确

---

## Task 2: max7219_c — MAX7219/MAX7221 LED 驱动（中复杂度）

### 2.1 创建文件

- **文件路径**: `libsigrokdecode/c_decoders/max7219_c.c`

### 2.2 实现步骤

1. **编写文件头部**
2. **定义 annotation 枚举** — `ANN_REG=0, ANN_DIGIT, ANN_WARNING, NUM_ANN`
3. **定义状态结构体** `max7219_state`
   - `out_ann`, `pos`, `cs_asserted`, `addr` (uint8_t), `addr_start`, `cs_start`
4. **定义静态数据**
   - `max7219_ann_labels` — 3 行
   - `max7219_ann_rows` — 2 行 (commands, warnings)
   - `max7219_inputs` = `{"spi", NULL}`
   - `max7219_tags` = `{"Display", NULL}`
5. **实现辅助函数**
   - `max7219_decode_intensity()` — 0→min, 15→max, 其他→数值字符串
6. **实现 `max7219_reset()`**
7. **实现 `max7219_start()`** — 注册 `SRD_OUTPUT_ANN`
8. **实现 `max7219_decode()`** — 空函数
9. **实现 `max7219_handle_register()`**
   - addr 1-8 → ANN_DIGIT, 输出 "Digit N: XX"
   - addr 0x00 → "No-op"
   - addr 0x09 → "Decode: 0bXXXXXXXX"
   - addr 0x0A → "Intensity: min/max/N"
   - addr 0x0B → "Scan limit: N"
   - addr 0x0C → "Shutdown: on/off"
   - addr 0x0F → "Display test: on/off"
   - 其他 → ANN_WARNING "Unknown register XX"
10. **实现 `max7219_recv_proto()`**
    - `DATA`: pos=0 记录地址, pos=1 处理寄存器
    - `CS-CHANGE`: new_cs=0 → pos=0, cs_start=ss; new_cs=1 → 检查 pos 长度
11. **实现 `max7219_destroy()`**
12. **定义 `max7219_c_decoder` 结构体**
13. **实现入口函数**

### 2.3 关键注意事项

- Python 中 `self.cs_asserted = mosi` 在 CS-CHANGE 中，这里 `mosi` 实际是 `data1`（第二个数据字段），对应 CS 新状态
- CS-CHANGE 的 data 格式：`data[0]=old_cs, data[1]=new_cs`
- Short write 警告仅在 pos=1 时触发（pos=0 不警告，避免 CS 毛刺）
- Digit 注解使用 `ANN_DIGIT`，格式为 `"Digit %d: %02X"`

### 2.4 验证

- 编译无错误
- 验证 Digit 1-8 和控制寄存器正确解码
- 验证 Short write / Overlong write 警告

---

## Task 3: max6954_c — MAX6954 LED 显示驱动（中复杂度）

### 3.1 创建文件

- **文件路径**: `libsigrokdecode/c_decoders/max6954_c.c`

### 3.2 实现步骤

1. **编写文件头部**
2. **定义 annotation 枚举** — `ANN_REG=0, ANN_DIGIT, ANN_WARNING, NUM_ANN`
3. **定义状态结构体** `max6954_state` — 与 max7219 相同结构
4. **定义静态数据** — 同 max7219
5. **实现辅助函数**
   - `max6954_decode_intensity()` — 同 max7219
   - `max6954_decode_configuration()` — 多字段解码
   - `max6954_decode_digit_type()` — 8位独立解码
   - `max6954_decode_digit()` — 字符显示或十六进制
6. **实现 `max6954_reset()`**
7. **实现 `max6954_start()`**
8. **实现 `max6954_decode()`** — 空函数
9. **实现 `max6954_handle_register()`**
   - 使用 switch-case 处理约 50+ 寄存器
   - 按地址范围分组：
     - 0x00-0x0F: 控制寄存器
     - 0x10-0x17: Intensity 对
     - 0x20-0x2F: Digit P0
     - 0x40-0x4F: Digit P1
     - 0x60-0x6F: Digit Both
     - 0x88-0x8F: Key registers
10. **实现 `max6954_recv_proto()`** — 与 max7219 相同框架
11. **实现 `max6954_destroy()`**
12. **定义 `max6954_c_decoder` 结构体**
13. **实现入口函数**

### 3.3 关键注意事项

- MAX6954 寄存器数量远多于 MAX7219，需要完整的 switch-case
- `_decode_configuration()` 需要解析多个位字段
- `_decode_digit_type()` 需要解析 8 个独立位
- `_decode_digit()` 需要判断是否为可打印 ASCII 字母
- Python 中 `from string import ascii_letters`，C 中需手动判断 `isalpha()`
- 寄存器 0x0D-0x0F 标记为 "don't write"，但 Python 版本未特殊处理
- 0x06 和 0x88-0x8F 标记为 "not done"

### 3.4 验证

- 编译无错误
- 验证各类寄存器解码输出
- 验证 Digit Type 寄存器的 8 位独立解码
- 验证 Configuration 寄存器的多字段输出

---

## Task 4: enc28j60_c — ENC28J60 以太网控制器（高复杂度）

### 4.1 创建文件

- **文件路径**: `libsigrokdecode/c_decoders/enc28j60_c.c`

### 4.2 实现步骤

1. **编写文件头部**
2. **定义 annotation 枚举** — 10 个 annotation
3. **定义状态结构体** `enc28j60_state`
   - `out_ann`, `mosi[256]`, `miso[256]`, `ranges_ss[256]`, `ranges_es[256]`, `byte_count`, `cmd_ss`, `cmd_es`, `active`, `bsel0`, `bsel1`, `bsel_known`
4. **定义寄存器名称表** — 4×32 的二维字符串数组
5. **定义静态数据** — 10 个 ann_labels, 3 个 ann_rows
6. **实现辅助函数**
   - `enc28j60_get_register_name()` — 根据 bank 和地址查表
   - `enc28j60_put_register_header()` — 输出寄存器地址注解
   - `enc28j60_put_data_byte()` — 输出数据字节注解
   - `enc28j60_put_command_warning()` — 输出命令警告
7. **实现 7 个命令处理函数**
   - `enc28j60_process_rcr()` — RCR: 检查长度，MAC/MII 需要 dummy byte
   - `enc28j60_process_rbm()` — RBM: 验证 header=0x3A，输出 MISO 数据
   - `enc28j60_process_wcr()` — WCR: 写 ECON1 时更新 bank
   - `enc28j60_process_wbm()` — WBM: 验证 header=0x7A，输出 MOSI 数据
   - `enc28j60_process_bfs()` — BFS: 二进制格式显示，更新 ECON1 bank 位
   - `enc28j60_process_bfc()` — BFC: 二进制格式显示，清除 ECON1 bank 位
   - `enc28j60_process_src()` — SRC: 重置 bank 为 0
8. **实现 `enc28j60_process_command()`** — 根据 opcode 分发到各处理函数
9. **实现 `enc28j60_reset()`**
10. **实现 `enc28j60_start()`** — 注册 `SRD_OUTPUT_ANN`
11. **实现 `enc28j60_decode()`** — 空函数
12. **实现 `enc28j60_recv_proto()`**
    - `CS-CHANGE`: new_cs=0 → 开始收集; new_cs=1 → 处理命令
    - `DATA`: 累积 mosi/miso 字节和范围
13. **实现 `enc28j60_destroy()`**
14. **定义 `enc28j60_c_decoder` 结构体**
15. **实现入口函数**

### 4.3 关键注意事项

- 寄存器名称表中的 `'—'` (U+2014 EM DASH) 需用 UTF-8 编码 `"\xe2\x80\x94"`
- Bank 追踪逻辑：WCR/BFS/BFC 写 ECON1 (addr=0x1F) 时更新 bsel0/bsel1
- SRC 命令重置 bsel0=0, bsel1=0
- RCR 命令的 MAC/MII 判断：寄存器名以 'M' 开头 → 需要 dummy byte (3字节)
- RBM header 必须为 0x3A (0b00111010)，WBM header 必须为 0x7A (0b01111010)
- Python 中 `self.miso.append(miso)` 实际是 bug（应该是 `miso`），但 C 版本应正确处理
- `putc` 使用 `cmd_ss/cmd_es` 范围，`putr` 使用 `range_ss/range_es` 范围
- 数据字节注解格式：非二进制用 `@%02X`，二进制用 `0b{08b}`

### 4.4 验证

- 编译无错误
- 验证 RCR/WCR/BFS/BFC/SRC 命令解码
- 验证 Bank 追踪（写 ECON1 后切换 bank）
- 验证 RBM/WBM 缓冲区读写
- 验证 MAC/MII 寄存器 dummy byte 检查

---

## Task 5: mrf24j40_c — MRF24J40 802.15.4 RF 收发器（高复杂度）

### 5.1 创建文件

- **文件路径**: `libsigrokdecode/c_decoders/mrf24j40_c.c`

### 5.2 实现步骤

1. **编写文件头部**
2. **定义 annotation 枚举** — 12 个 annotation
3. **定义状态结构体** `mrf24j40_state`
   - `out_ann`, `mosi_bytes[4]`, `miso_bytes[4]`, `byte_count`, `ss_cmd`, `es_cmd`, `ss_frame[2]`, `es_frame[2]`, `framecache[2][256]`, `framecache_len[2]`
4. **定义寄存器名称表**
   - `mrf24j40_sregs[64]` — 短寄存器名称
   - `mrf24j40_get_lreg_name()` — 长寄存器名称查找函数
5. **定义静态数据** — 12 个 ann_labels, 10 个 ann_rows
6. **实现辅助函数**
   - `mrf24j40_reset_data()` — 清空字节缓冲区
   - `mrf24j40_handle_short()` — 短寄存器处理
   - `mrf24j40_handle_long()` — 长寄存器处理
7. **实现 `mrf24j40_reset()`**
8. **实现 `mrf24j40_start()`** — 注册 `SRD_OUTPUT_ANN`
9. **实现 `mrf24j40_decode()`** — 空函数
10. **实现 `mrf24j40_recv_proto()`**
    - `CS-CHANGE`: 检查字节计数是否合法
    - `DATA`: 累积字节，短寄存器2字节处理，长寄存器3字节处理
11. **实现帧缓存追踪逻辑**
    - TX 帧：TXNCON bit0=1 时触发输出
    - RX 帧：RXFLUSH bit0=1 时触发输出
    - 长寄存器 TX/RX 区域访问时累积帧数据
12. **实现 TXSTAT 寄存器特殊处理**
    - 重试次数检查 (bit[7:6])
    - TX fail 检查 (bit0)
    - CCAFAIL 检查 (bit5)
13. **实现 `mrf24j40_destroy()`**
14. **定义 `mrf24j40_c_decoder` 结构体**
15. **实现入口函数**

### 5.3 关键注意事项

- 短寄存器地址格式：`byte0[6:1]` = 6位地址，`byte0[0]` = R/W
- 长寄存器地址格式：`dword = byte0 << 8 | byte1`，`dword[4]` = R/W，`dword[13:5]` = 10位地址
- 长寄存器区域划分：
  - 0x000-0x07F: TX
  - 0x080-0x0FF: TX beacon
  - 0x100-0x17F: TX GTS1
  - 0x180-0x1FF: TX GTS2
  - 0x200-0x27F: RF 控制 (lregs)
  - 0x280-0x2BF: Security keys
  - 0x2C0-0x2FF: Reserved
  - 0x300+: RX
- 帧缓存：TX 和 RX 各维护一个 `framecache` 数组
- TXSTAT 读取时的重试/fail/CCAFAIL 注解需要动态计算 annotation class index
  - `idx = 6 + numretries + txfail`
  - numretries=1→ANN_TX_RETRY_1, numretries=2→ANN_TX_RETRY_2, numretries=3→ANN_TX_RETRY_3
  - txfail=1→ANN_TX_FAIL
  - CCAFAIL→ANN_CCAFAIL
- CS-CHANGE 中间断开（byte_count 不为 0/2/3）输出 "Misplaced CS!" 警告

### 5.4 验证

- 编译无错误
- 验证短寄存器读写
- 验证长寄存器读写
- 验证 TX/RX 帧缓存和输出
- 验证 TXSTAT 重试/fail/CCAFAIL 注解

---

## Task 6: CMakeLists.txt 更新

### 6.1 修改内容

在 `CMakeLists.txt` 第 837 行的 `C_DECODERS` 列表末尾添加 5 个解码器：

```
enc28j60_c ltc242x_c max6954_c max7219_c mrf24j40_c
```

### 6.2 验证

- 运行 `build_incremental.cmd` 编译成功
- 检查 `build.dir/decoders/c_decoders/` 目录下生成 5 个 DLL 文件

---

## 实施顺序建议

```
Task 1 (ltc242x_c) → Task 2 (max7219_c) → Task 3 (max6954_c) → Task 4 (enc28j60_c) → Task 5 (mrf24j40_c) → Task 6 (CMakeLists.txt)
```

理由：
1. ltc242x 最简单，用于验证 SPI 上层 recv_proto 框架
2. max7219 结构简单但引入 CS-CHANGE + DATA 双事件处理
3. max6954 在 max7219 基础上增加大量寄存器解码
4. enc28j60 引入复杂状态机（bank 追踪、多命令类型）
5. mrf24j40 最复杂（短/长寄存器、帧缓存、TXSTAT 特殊处理）
6. 最后统一更新 CMakeLists.txt
