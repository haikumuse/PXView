# 移植任务分解 — Batch 21

## 总览

5 个 I2C 上层解码器移植任务，按复杂度从低到高排序执行。

| 优先级 | 解码器 | 预估工作量 | 文件 |
|--------|--------|-----------|------|
| P1 | tcs3472x_c | 2-3h | `libsigrokdecode/c_decoders/tcs3472x_c.c` |
| P2 | rtc8564_c | 3-4h | `libsigrokdecode/c_decoders/rtc8564_c.c` |
| P3 | tpm_tis_i2c_c | 3-4h | `libsigrokdecode/c_decoders/tpm_tis_i2c_c.c` |
| P4 | st25dv_c | 4-5h | `libsigrokdecode/c_decoders/st25dv_c.c` |
| P5 | ssd1306_c | 6-8h | `libsigrokdecode/c_decoders/ssd1306_c.c` |

---

## Task 1: tcs3472x_c — TCS3472x 颜色传感器

### 子任务

#### 1.1 创建文件骨架
- 创建 `libsigrokdecode/c_decoders/tcs3472x_c.c`
- 包含标准头文件和版权声明
- 定义 `NUM_ANN = 1`（仅 1 个 annotation class: register）

#### 1.2 定义元数据
- `id = "tcs3472x_c"`, `name = "TCS3472X(C)"`
- `inputs = {"i2c", NULL}`
- `outputs = NULL / 0`
- `tags = {"Embedded/industrial", NULL}`
- `license = "gplv2+"`
- 定义 `device_address` 选项（默认 "0x29"，可选 "0x29"/"0x39"）

#### 1.3 定义 ann_labels 和 annotation_rows
```c
static const char *tcs3472x_ann_labels[][3] = {
    {"", "register", "Register"},
};
static const int tcs3472x_row_regs_classes[] = {0};
static const struct srd_c_ann_row tcs3472x_ann_rows[] = {
    {"registers", "Data", tcs3472x_row_regs_classes, 1},
};
```

#### 1.4 实现状态机
- 状态枚举: `INITIAL, START, ADDR_WRITE, ACK_ADDR_WRITE, ACK_DATA_WRITE, DATA_WRITE_CMD, DATA_WRITE, ADDR_READ, ACK_ADDR_READ, ACK_DATA_READ, DATA_READ`
- 命令字节解析: 提取 bit7(R/W)、bit6(自动递增)、bit5-0(寄存器地址)
- 寄存器表: 20 个寄存器定义（ENABLE, ATIME, WTIME, AILTL, AILTH, AIHTL, AIHTH, PERS, CONFIG, CONTROL, ID, STATUS, CDATAL, CDATAH, RDATAL, RDATAH, GDATAL, GDATAH, BDATAL, BDATAH）
- I2C 地址过滤: 使用 `device_address` 选项

#### 1.5 实现寄存器格式化
- ENABLE (0x00): 显示 PON, AEN, WEN, AIEN 位状态
- ATIME (0x01): 显示积分时间值
- PERS (0x0C): 显示持久性滤波器设置
- CONFIG (0x0D): 显示 WLONG 位
- CONTROL (0x0F): 显示 AGAIN 增益设置
- 颜色数据寄存器: 显示 16 位值

#### 1.6 实现生命周期回调
- `reset()`: 分配/清零私有数据
- `start()`: 注册 output，读取选项
- `decode()`: 空实现
- `destroy()`: 释放私有数据

#### 1.7 实现导出函数
- `srd_c_decoder_entry()`: 设置选项默认值
- `srd_c_decoder_api_version()`

#### 1.8 修改 CMakeLists.txt
- 在 `C_DECODERS` 列表中添加 `tcs3472x_c`

### 验证
- 编译通过
- 加载 I2C + tcs3472x_c 解码器，验证寄存器读写注解

---

## Task 2: rtc8564_c — Epson RTC-8564 实时时钟

### 子任务

#### 2.1 创建文件骨架
- 创建 `libsigrokdecode/c_decoders/rtc8564_c.c`
- 定义 `NUM_ANN = 16`

#### 2.2 定义元数据
- `id = "rtc8564_c"`, `name = "RTC-8564(C)"`
- `inputs = {"i2c", NULL}`, `outputs = NULL / 0`
- `tags = {"Clock/timing", NULL}`
- `license = "gplv2+"`
- 无选项

#### 2.3 定义 ann_labels (16 个)
```c
static const char *rtc8564_ann_labels[][3] = {
    {"", "reg-0x00", "Register 0x00"},
    {"", "reg-0x01", "Register 0x01"},
    {"", "reg-0x02", "Register 0x02"},
    {"", "reg-0x03", "Register 0x03"},
    {"", "reg-0x04", "Register 0x04"},
    {"", "reg-0x05", "Register 0x05"},
    {"", "reg-0x06", "Register 0x06"},
    {"", "reg-0x07", "Register 0x07"},
    {"", "reg-0x08", "Register 0x08"},
    {"", "read", "Read date/time"},
    {"", "write", "Write date/time"},
    {"", "bit-reserved", "Reserved bit"},
    {"", "bit-vl", "VL bit"},
    {"", "bit-century", "Century bit"},
    {"", "reg-read", "Register read"},
    {"", "reg-write", "Register write"},
};
```

#### 2.4 定义 annotation_rows (3 个)
- `bits`: indices 0-8, 11, 12, 13
- `regs`: indices 14, 15
- `date-time`: indices 9, 10

#### 2.5 实现 BCD 辅助函数
```c
static int bcd2int(uint8_t b) { return (b >> 4) * 10 + (b & 0x0f); }
```

#### 2.6 实现状态机 (6 个状态)
- `RTC8564_IDLE`
- `RTC8564_GET_SLAVE_ADDR`
- `RTC8564_GET_REG_ADDR`
- `RTC8564_WRITE_RTC_REGS`
- `RTC8564_READ_RTC_REGS`
- `RTC8564_READ_RTC_REGS2`

#### 2.7 实现寄存器处理函数
- `handle_reg_0x00`: Control register 1（空实现，与 Python 一致）
- `handle_reg_0x01`: Control register 2（TI/TP, AF, TF, AIE, TIE 位描述）
- `handle_reg_0x02`: Seconds + VL bit
- `handle_reg_0x03`: Minutes
- `handle_reg_0x04`: Hours
- `handle_reg_0x05`: Days
- `handle_reg_0x06`: Weekdays
- `handle_reg_0x07`: Months + Century bit
- `handle_reg_0x08`: Years
- 0x09-0x0F: 空实现（与 Python 一致）

#### 2.8 实现日期时间输出
- STOP 时格式化输出完整日期时间: `DD.MM.YY HH:MM:SS`
- 区分读/写操作

#### 2.9 实现生命周期回调和导出函数

#### 2.10 修改 CMakeLists.txt

### 验证
- 编译通过
- 加载 I2C + rtc8564_c，验证时间寄存器解析和日期时间输出

---

## Task 3: tpm_tis_i2c_c — TPM TIS 2.0 over I2C

### 子任务

#### 3.1 创建文件骨架
- 创建 `libsigrokdecode/c_decoders/tpm_tis_i2c_c.c`
- 定义 `NUM_ANN = 5`

#### 3.2 定义元数据
- `id = "tpm_tis_i2c_c"`, `name = "TPM TIS 2.0 I2C(C)"`
- `inputs = {"i2c", NULL}`
- `outputs = {"tpm-tis", NULL}`（与 Python 一致有输出）
- `tags = {"TPM", NULL}`
- `license = "gplv3+"`
- 无选项

#### 3.3 定义 ann_labels (5 个)
```c
static const char *tpm_tis_ann_labels[][3] = {
    {"", "address", "Address"},
    {"", "data-read", "Data (Read)"},
    {"", "data-write", "Data (Write)"},
    {"", "transaction", "Transaction"},
    {"", "warning", "Warning"},
};
```

#### 3.4 定义 annotation_rows (3 个)
- `protocol`: indices 0, 1, 2
- `transactions`: index 3
- `warnings`: index 4

#### 3.5 实现状态机 (10 个状态)
- `TPM_TIS_IDLE`
- `TPM_TIS_ADDR_WRITE`
- `TPM_TIS_ADDR_ACK`
- `TPM_TIS_REG_ADDR`
- `TPM_TIS_REG_ADDR_ACK`
- `TPM_TIS_WAIT_OP`
- `TPM_TIS_READ_ADDR_READ`
- `TPM_TIS_READ_ADDR_ACK`
- `TPM_TIS_READ_DATA`
- `TPM_TIS_WRITE_DATA`

#### 3.6 实现数据累积和格式化
- 读路径: 累积 DATA READ 字节直到 NACK → STOP
- 写路径: 累积 DATA WRITE 字节直到 STOP
- hex 格式化: 循环 `snprintf("%02X", byte)`
- 事务注解: `"Read/WRITE XX ->/<- HEXDATA"`

#### 3.7 实现错误处理
- 意外命令时输出 warning 注解并重置状态机
- 参考 Python 的 `ValueError` 异常处理

#### 3.8 实现生命周期回调和导出函数

#### 3.9 修改 CMakeLists.txt

### 验证
- 编译通过
- 加载 I2C + tpm_tis_i2c_c，验证 TIS 事务解析

---

## Task 4: st25dv_c — ST25DV NFC EEPROM

### 子任务

#### 4.1 创建文件骨架
- 创建 `libsigrokdecode/c_decoders/st25dv_c.c`
- 定义 `NUM_ANN = 5`

#### 4.2 定义元数据
- `id = "st25dv_c"`, `name = "ST25DV(C)"`
- `inputs = {"i2c", NULL}`
- `outputs = {"st25dv", NULL}`（与 Python 一致有输出）
- `tags = {"Embedded/industrial", NULL}`
- `license = "mit"`
- 无选项

#### 4.3 定义 ann_labels (5 个)
```c
static const char *st25dv_ann_labels[][3] = {
    {"", "sys", "System"},
    {"", "data", "Data"},
    {"", "read", "Read"},
    {"", "write", "Write"},
    {"", "error", "Error"},
};
```

#### 4.4 定义 annotation_rows (1 个)
- `regs`: indices 0, 1, 2, 3, 4

#### 4.5 实现寄存器定义表
- 30+ 个寄存器，每个包含 short_name, long_name, length, fields
- Field 定义: name, shift, mask
- 关键寄存器: GPO(0x0000), EH_MODE(0x0002), RF_MNGT(0x0003), RFZ1SS-4SS, I2CZSS, MB_MODE, GPO_DYN(0x2000), EH_CTRL_DYN(0x2002), MB_CTRL_DYN(0x2006), MAILBOX_RAM(0x2008)

#### 4.6 实现状态机 (9 个 step)
- step 0: BEFORE START
- step 1: BEFORE ADDRESS
- step 2: AFTER ADDR ACK
- step 3: BEFORE REG MSB
- step 4: AFTER REG MSB ACK
- step 5: BEFORE REG LSB
- step 6: AFTER REG LSB ACK
- step 7: BEFORE FIRST DATA
- step 8: BEFORE SECOND DATA

#### 4.7 实现 I2C 地址识别
- 0x53 (0xA6 >> 1): DATA 区域 → ann_data
- 0x57 (0xAE >> 1): SYSTEM 区域 → ann_sys
- 其他: → ann_error

#### 4.8 实现寄存器值注解
- 单字节寄存器: 格式化字段值
- 多字节寄存器: 累积数据后格式化
- 字段提取: `(value & mask) >> shift`

#### 4.9 实现生命周期回调和导出函数

#### 4.10 修改 CMakeLists.txt

### 验证
- 编译通过
- 加载 I2C + st25dv_c，验证寄存器读写和字段解析

---

## Task 5: ssd1306_c — Solomon SSD1306 OLED 控制器

### 子任务

#### 5.1 创建文件骨架
- 创建 `libsigrokdecode/c_decoders/ssd1306_c.c`
- 定义 `NUM_ANN = 51`（10 bit + 30 cmd + 5 special + 1 block + 1 warn = 47，需精确计算）

**精确计算**:
- bit 级别: 10 个 (index 0-9)
- 命令级别: 30 个 (index 10-39)，加上 zoomin/precharge/compins/vcomh/nop = 5 个 (index 40-44)
- 特殊: gddram(45), deviceaddress(46), controlbyte(47), last(48)
- 额外: write_block(49), warning(50)
- **总计: 51 个**

#### 5.2 定义元数据
- `id = "ssd1306_c"`, `name = "SSD1306(C)"`
- `inputs = {"i2c", NULL}`, `outputs = NULL / 0`
- `tags = {"Display", "IC", NULL}`
- `license = "gplv2+"`
- 无选项

#### 5.3 定义 ann_labels (51 个)
- 按顺序定义所有 51 个 annotation labels
- 第一列均为 `""`

#### 5.4 定义 annotation_rows (4 个)
- `bits`: indices 0-9
- `cmds`: indices 10-48
- `blockdata`: index 49
- `warnings`: index 50

#### 5.5 定义命令表
```c
typedef struct {
    uint8_t cmd_byte;
    int ann_id;
    const char *text_long;
    const char *text_medium;
    const char *text_short;
    int has_param;
} ssd1306_cmd_entry;

static const ssd1306_cmd_entry ssd1306_cmds[] = {
    {0x00, ANN_LC, "Set Lower Column Start Address", "Set L Col Start", "LC", 0},
    {0x10, ANN_HC, "Set Higher Column Start Address", "Set H Col Start", "HC", 0},
    /* ... 所有 30+ 命令 ... */
};
```

#### 5.6 实现状态机 (5 个状态 + 3 个子状态)
- 主状态: `IDLE, GET_SLAVE_ADDR, WRITE_CONTROL_BYTE, SSD_COMMAND, SSD_DATA`
- 子状态: `SUB_COMMAND, SUB_PARAMETER, SUB_PARAMETER2`

#### 5.7 实现 I2C 地址过滤
- 仅接受 0x3C 和 0x3D
- 非目标地址输出 warning

#### 5.8 实现控制字节处理
- 0x80: 后续为命令
- 0x40: 后续为数据
- 其他: 忽略

#### 5.9 实现命令处理
- 命令范围归一化: 0x00-0x0F → 0x00, 0x10-0x1F → 0x10, 0x40-0x7F → 0x40, 0xB0-0xB7 → 0xB0
- 参数处理: 部分命令需要 1-2 个参数字节
- blockstring 累积和输出

#### 5.10 实现参数处理函数
- `handle_par_0x00`: 低列起始地址
- `handle_par_0x10`: 高列起始地址
- `handle_par_0x20`: 显示模式
- `handle_par_0x21`: 设置列地址
- `handle_par_0x81`: 设置对比度
- `handle_par_0x8d`: 充电泵设置
- `handle_par_0xa3`: 垂直滚动区域
- `handle_par_0xa8`: 多路复用比
- `handle_par_0xb0`: 页起始地址
- `handle_par_0xd3`: 垂直偏移
- `handle_par_0xd5`: 显示时钟比
- `handle_par_0xd6`: 缩放
- `handle_par_0xd9`: 预充电周期
- `handle_par_0xda`: COM 引脚设置
- `handle_par_0xdb`: Vcomh 取消选择

#### 5.11 实现数据路径
- GDDRAM 数据写入注解

#### 5.12 实现生命周期回调和导出函数

#### 5.13 修改 CMakeLists.txt

### 验证
- 编译通过
- 加载 I2C + ssd1306_c，验证命令解析和数据写入注解

---

## 通用子任务（所有解码器共享）

### G.1 CMakeLists.txt 更新
在 `C_DECODERS` 列表中一次性添加所有 5 个解码器名称

### G.2 编译验证
```bash
build_incremental.cmd
```

### G.3 运行时验证
对每个解码器:
1. 打开 PXView
2. 加载包含 I2C 数据的 .sr 会话文件
3. 添加 I2C 解码器
4. 在 I2C 之上堆叠 C 解码器
5. 对比 Python 解码器和 C 解码器的注解输出
6. 验证关键事务的注解正确性
