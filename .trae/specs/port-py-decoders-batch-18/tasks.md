# 任务分解 — Batch 18: 5 个 I2C 上层解码器移植

## 总览

5 个解码器按复杂度排序（建议实现顺序）:

| 序号 | 解码器 | 复杂度 | 预估代码行数 | 特殊难点 |
|------|--------|--------|-------------|----------|
| 1 | bh1750 | ★★☆ | ~300 | 光照计算、MTreg 管理、选项处理 |
| 2 | atsha204a | ★★★ | ~500 | 大量 opcode/param1/param2/data 格式化、字节缓冲区 |
| 3 | ad5593r | ★★★★ | ~600 | Pointer Byte 寄存器映射、16-bit 数据解析、大量寄存器字段 |
| 4 | adxl345 | ★★★★ | ~700 | SPI→I2C 协议适配、寄存器缩放因子、16-bit 轴数据组合 |
| 5 | eeprom24xx | ★★★★★ | ~800 | 17+ 状态、多种读写操作、芯片配置表、binary 输出 |

---

## Task 1: bh1750_c — 光照传感器解码器

### 1.1 创建文件骨架
- [ ] 创建 `libsigrokdecode/c_decoders/bh1750_c.c`
- [ ] 包含标准头文件: `stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 定义注释枚举 (25 个: ANN_ADDR_GND=0 到 ANN_MTIME=24)
- [ ] 定义状态枚举: `BH1750_IDLE`, `BH1750_ADDRESS_SLAVE`, `BH1750_REGISTER_ADDRESS`, `BH1750_REGISTER_DATA`

### 1.2 实现私有数据结构
```c
typedef struct {
    enum bh1750_state state;
    int is_write;
    uint8_t addr;
    uint8_t reg;
    uint8_t mode;
    int mtreg;
    uint64_t ss, es, ssb, ssd;
    uint8_t data_bytes[4];
    int num_data;
    int out_ann;
} bh1750_state;
```

### 1.3 实现注释标签和行
- [ ] `bh1750_ann_labels[25][3]` — 第一列 `""`
- [ ] `bh1750_ann_rows[4]` — bits, regs, info, warnings

### 1.4 实现选项
- [ ] `radix` 选项: string, default "Hex", values: "Hex"/"Dec"/"Oct"/"Bin"
- [ ] `params` 选项: string, default "Typical", values: "Typical"/"Maximal"/"Minimal"

### 1.5 实现核心逻辑
- [ ] `bh1750_recv_proto()` — 状态机
- [ ] `check_addr()` — 验证 slave 地址 0x23/0x5C
- [ ] `handle_register()` — 处理命令/寄存器字节
- [ ] `handle_mtreg_high()` / `handle_mtreg_low()` — 更新 MTreg
- [ ] `handle_data()` — 处理读数据，计算光照
- [ ] `calculate_sensitivity()` — 计算灵敏度
- [ ] `calculate_light()` — 计算光照值

### 1.6 注册输出 + 入口函数

---

## Task 2: atsha204a_c — 加密认证解码器

### 2.1 创建文件骨架
- [ ] 创建 `libsigrokdecode/c_decoders/atsha204a_c.c`
- [ ] 定义注释枚举 (9 个), 状态枚举 (4 个)

### 2.2 实现私有数据结构
- [ ] 字节缓冲区: `atsha204a_byte_entry bytes[256]` + `num_bytes`

### 2.3 实现常量表
- [ ] `WORD_ADDR`, `OPCODES`, `ZONES`, `STATUS` 映射

### 2.4 实现核心逻辑
- [ ] `atsha204a_recv_proto()` — 收集字节，STOP 时解析
- [ ] `output_tx_bytes()` — 解析 TX 帧
- [ ] `output_rx_bytes()` — 解析 RX 帧
- [ ] `put_param1()` — 12 种 Param1 格式
- [ ] `put_param2()` — 9 种 Param2 格式
- [ ] `put_data()` — 8 种 Data 格式
- [ ] `put_status()` — 状态码注释

---

## Task 3: ad5593r_c — ADC/DAC 解码器

### 3.1 创建文件骨架
- [ ] 创建 `libsigrokdecode/c_decoders/ad5593r_c.c`
- [ ] 定义注释枚举 (6 个), 状态枚举 (5 个)

### 3.2 实现私有数据结构 + 寄存器字段结构

### 3.3 实现常量表
- [ ] `CONFIG_MODE_BITS_MAP`, `REG_SEL_RD_MAP`
- [ ] Pointer Byte 寄存器字段定义数组
- [ ] Data Byte 寄存器字段定义数组

### 3.4 实现核心逻辑
- [ ] `ad5593r_recv_proto()` — 状态机
- [ ] `handle_pointer_byte()` — 解析 Pointer Byte
- [ ] `handle_data_bytes()` — 解析 16-bit 数据
- [ ] `decode_field()` — 解析单个字段
- [ ] 辅助解析函数: `bit_indices()`, `disabled_enabled()`, `vref_range()`, `dac_chn()`, `adc_chn()`

### 3.5 实现选项: Vref (double, default 2.5)

---

## Task 4: adxl345_c — 加速度计解码器 (SPI→I2C 适配)

### 4.1 创建文件骨架
- [ ] 创建 `libsigrokdecode/c_decoders/adxl345_c.c`
- [ ] 定义注释枚举 (6 个), 状态枚举 (5 个)

### 4.2 实现私有数据结构

### 4.3 实现常量表
- [ ] `registers` 映射 (0x00-0x39)
- [ ] `rate_code` 映射 (16 个)
- [ ] `fifo_modes` 映射 (4 个)

### 4.4 I2C 协议适配
- [ ] I2C 地址: 0x53
- [ ] Register Address Byte: bit7=R/W, bit6=MB, bit5:0=Address

### 4.5 实现核心逻辑
- [ ] `adxl345_recv_proto()` — 状态机
- [ ] 20+ 个寄存器处理函数
- [ ] `handle_reg_with_scaling_factor()` — 通用缩放因子
- [ ] `get_axis_value()` — 16-bit 轴数据组合

---

## Task 5: eeprom24xx_c — EEPROM 解码器

### 5.1 创建文件骨架
- [ ] 创建 `libsigrokdecode/c_decoders/eeprom24xx_c.c`
- [ ] 定义注释枚举 (21 个), 状态枚举 (19 个), binary 枚举 (1 个)

### 5.2 实现私有数据结构
- [ ] packet 缓冲区 + bytebuf 缓冲区

### 5.3 实现芯片配置表
- [ ] 15 个芯片配置项的 C 结构体数组

### 5.4 实现选项
- [ ] `chip` 选项: string, default "generic"
- [ ] `addr_counter` 选项: int, default 0

### 5.5 实现核心逻辑
- [ ] `eeprom24xx_recv_proto()` — 19 状态状态机
- [ ] ACK/NACK 包处理
- [ ] `put_control_word()`, `put_word_addr()`, `put_data_byte()`, `put_data_bytes()`
- [ ] `put_operation()` — 5 种操作类型
- [ ] `decide_on_seq_or_rnd_read()`
- [ ] Binary 输出

---

## Task 6: CMakeLists.txt 更新

- [ ] 在 `C_DECODERS` 列表中添加 5 个解码器名

## Task 7: 编译验证

- [ ] 运行 `build_incremental.cmd` 确认无编译错误
- [ ] 确认 5 个 DLL 生成
- [ ] 确认 PXView.exe 正常运行
