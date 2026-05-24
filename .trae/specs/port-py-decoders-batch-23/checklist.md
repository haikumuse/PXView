# 移植验证清单 — Batch 23

## 通用检查项 (适用于所有 5 个解码器)

### 文件结构

- [ ] 文件名格式正确: `{decoder_id}_c.c`
- [ ] 文件位于 `libsigrokdecode/c_decoders/` 目录
- [ ] 包含正确的头文件: `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<glib.h>`, `"libsigrokdecode.h"`
- [ ] 无编译警告 (`-Wall -Wextra`)

### srd_c_decoder 结构体

- [ ] `.id` 格式为 `"{python_id}_c"` (如 `"a7105_c"`)
- [ ] `.name` 格式为 `"{PythonName}(C)"` (如 `"A7105(C)"`)
- [ ] `.longname` 格式为 `"{Full Name} (C)"` (如 `"AMICCOM A7105 (C)"`)
- [ ] `.desc` 末尾添加 `" (C implementation)"`
- [ ] `.license` 与 Python 版本一致
- [ ] `.channels = NULL`, `.num_channels = 0` (SPI 上层无直接通道)
- [ ] `.optional_channels = NULL`, `.num_optional_channels = 0`
- [ ] `.inputs` 为 `{"spi", NULL}`, `.num_inputs = 1`
- [ ] `.outputs = NULL`, `.num_outputs = 0`
- [ ] `.binary = NULL`, `.num_binary = 0`
- [ ] `.decode` 为空函数体
- [ ] `.recv_proto` 指向正确的回调函数

### Annotations

- [ ] `ann_labels` 第一列为空字符串 `""`
- [ ] `ann_labels` 每行 3 列: `{"", "id", "Full Label"}`
- [ ] `NUM_ANN` enum 值正确，等于 annotation class 总数
- [ ] annotation_rows 中 classes 数组以 `-1` 结尾 (如果使用旧格式) 或 count 正确
- [ ] 所有 annotation class 都被映射到某个 row
- [ ] annotation_rows 的 `count` 字段与 classes 数组长度一致

### 生命周期函数

- [ ] `reset`: 使用 `g_malloc0` 分配 private data (首次), `memset` 清零 (后续)
- [ ] `reset`: 通过 `c_decoder_get_private` 检查是否已分配
- [ ] `start`: 调用 `c_decoder_register_output(di, SRD_OUTPUT_ANN, "{python_id}")`
- [ ] `start`: 读取选项默认值 (如有选项)
- [ ] `destroy`: 使用 `g_free` 释放 private data
- [ ] `destroy`: 调用 `c_decoder_set_private(di, NULL)` 清空指针

### 导出函数

- [ ] `srd_c_decoder_entry`: 使用 `SRD_C_DECODER_EXPORT` 前缀
- [ ] `srd_c_decoder_entry`: 设置选项默认值 (如有选项)
- [ ] `srd_c_decoder_entry`: 返回 `&xxx_c_decoder` 指针
- [ ] `srd_c_decoder_api_version`: 使用 `SRD_C_DECODER_EXPORT` 前缀
- [ ] `srd_c_decoder_api_version`: 返回 `SRD_C_DECODER_API_VERSION`

### recv_proto 实现

- [ ] 从 `c_decoder_get_private(di)` 获取 state 指针
- [ ] 检查 state 指针非 NULL
- [ ] 正确处理 `"DATA"` 消息 (字节级)
- [ ] 正确处理 `"BITS"` 消息 (位级)
- [ ] 正确处理 `"CS-CHANGE"` 消息
- [ ] 正确处理 `"TRANSFER"` 消息
- [ ] 使用 `strcmp` 比较 cmd 字符串
- [ ] 使用 `C_ANN_PUT` 宏输出 annotation
- [ ] data_len 检查: 访问 data 前检查 `data_len > 0`

---

## ad5626_c 专项检查

### 元数据

- [ ] `.id = "ad5626_c"`
- [ ] `.name = "AD5626(C)"`
- [ ] `.longname = "Analog Devices AD5626 (C)"`
- [ ] `.license = "gplv2+"`
- [ ] `.tags = {"IC", "Analog/digital", NULL}`, `.num_tags = 2`
- [ ] `.num_annotations = 1` (只有 ANN_VOLTAGE)
- [ ] `.num_annotation_rows = 1`

### 解码逻辑

- [ ] CS 下降沿 (1→0) 记录 `ss = start_sample`
- [ ] BITS 消息: 从 MOSI bits 收集数据, MSB first
  - [ ] `data = data | bit_val; data <<= 1;` 循环正确
- [ ] CS 上升沿 (0→1) 完成转换:
  - [ ] `data >>= 1` (修正多余左移)
  - [ ] `voltage = data / 1000.0`
  - [ ] 输出 `"%.3fV"` 格式
  - [ ] 重置 `data = 0`
- [ ] BITS 消息中正确跳过 have_mosi/have_miso 标记

### 边界情况

- [ ] 无 CS 信号时的行为 (SPI 可选 CS)
- [ ] BITS 数据为空时的处理

---

## ad79x0_c 专项检查

### 元数据

- [ ] `.id = "ad79x0_c"`
- [ ] `.name = "AD79x0(C)"`
- [ ] `.longname = "Analog Devices AD79x0 (C)"`
- [ ] `.license = "gplv2+"`
- [ ] `.tags = {"IC", "Analog/digital", NULL}`, `.num_tags = 2`
- [ ] `.num_annotations = 3`
- [ ] `.num_annotation_rows = 3`
- [ ] `.options` 包含 `vref` 选项 (默认 1.5V)
- [ ] `.num_options = 1`

### 解码逻辑

- [ ] CS 下降沿: `start_sample = ss; samples_bit = -1`
- [ ] BITS 消息: 从 MISO bits 收集数据
  - [ ] 计算 `samples_bit` (从 samplerate 或 DATA 时间差)
- [ ] CS 上升沿:
  - [ ] `data >>= 1`
  - [ ] `nb_bits = (start_sample - ss) / samples_bit`
  - [ ] `nb_bits >= 10 && data == 0xFFF`: Power Up Mode + Invalid
  - [ ] `nb_bits >= 10 && data != 0xFFF`: Normal Mode
    - [ ] `nb_bits == 16`: Complete conversion
    - [ ] `nb_bits < 16`: Incomplete conversion
    - [ ] `vin = (data / 4095.0) * vref`
    - [ ] 电压输出两种格式: `"%.6fV"` 和 `"%.2fV"`
  - [ ] `nb_bits < 10`: Power Down Mode + Invalid

### 选项

- [ ] `vref` 选项类型为 `int` (通过 `c_decoder_get_option_int` 获取)
- [ ] `vref` 默认值为 1.5 (实际存储为 `g_variant_new_int64(15)` 然后除以 10？或 double?)
- [ ] **注意**: Python 版本 `vref` 默认值为 `1.5` (float)，C 版本需要确认如何处理 float 选项

### 边界情况

- [ ] `samples_bit == -1` 时 CS 上升沿的处理 (直接 return)
- [ ] `ss == -1` 时的处理

---

## a7105_c 专项检查

### 元数据

- [ ] `.id = "a7105_c"`
- [ ] `.name = "A7105(C)"`
- [ ] `.longname = "AMICCOM A7105 (C)"`
- [ ] `.license = "gplv2+"`
- [ ] `.tags = {"IC", "Wireless/RF", NULL}`, `.num_tags = 2`
- [ ] `.num_annotations = 4`
- [ ] `.num_annotation_rows = 2`

### 寄存器表

- [ ] 52 个寄存器 (0x00-0x33) 全部定义
- [ ] 寄存器名与 Python 版本一致
- [ ] 寄存器 size 字段正确 (全部为 1)

### 命令解析

- [ ] `0x05` → `W_TX_FIFO` (min=1, max=32)
- [ ] `0x45` → `R_RX_FIFO` (min=1, max=32)
- [ ] `0x06` → `W_ID` (min=1, max=4)
- [ ] `0x46` → `R_ID` (min=1, max=4)
- [ ] `0x00-0x3F` (bit7=0, bit6=0) → `W_REGISTER` (reg=b&0x3F, min=1, max=1)
- [ ] `0x40-0x7F` (bit7=0, bit6=1) → `R_REGISTER` (reg=b&0x3F, min=1, max=1)
- [ ] `0x80` → `SLEEP_MODE` (min=0, max=0)
- [ ] `0x90` → `IDLE_MODE` (min=0, max=0)
- [ ] `0xA0` → `STANDBY_MODE` (min=0, max=0)
- [ ] `0xB0` → `PLL_MODE` (min=0, max=0)
- [ ] `0xC0` → `RX_MODE` (min=0, max=0)
- [ ] `0xD0` → `TX_MODE` (min=0, max=0)
- [ ] `0xE0` → `FIFO_WRITE_PTR_RESET` (min=0, max=0)
- [ ] `0xF0` → `FIFO_READ_PTR_RESET` (min=0, max=0)
- [ ] 未知命令 → 警告 "unknown command"

### finish_command 逻辑

- [ ] `R_REGISTER`: 从 MISO 字节解码，使用 `ANN_CMD` class
- [ ] `W_REGISTER`: 从 MOSI 字节解码，使用 `ANN_CMD` class
- [ ] `R_RX_FIFO`: MISO 字节，使用 `ANN_RX` class
- [ ] `W_TX_FIFO`: MOSI 字节，使用 `ANN_TX` class
- [ ] `R_ID`: MISO 字节，使用 `ANN_RX` class
- [ ] `W_ID`: MOSI 字节，使用 `ANN_TX` class

### 输出格式

- [ ] 寄存器命令: `"Cmd {CMD}: {REG_NAME} = \"{$\""` 和 `"@{HEX}"`
- [ ] FIFO/ID 命令: `"Cmd {CMD}: {LABEL} = \"{$\""` 和 `"@{HEX}"`
- [ ] 多字节寄存器 LSByte first (Python 版本 `reversed(data)`)

### 状态管理

- [ ] `cs_was_released` 标志正确维护
- [ ] `first` 标志在 CS 下降沿/TRANSFER 后重置
- [ ] `mb` 缓冲区溢出检查 (`mb_count < max`)
- [ ] 缺失数据字节警告 (`mb_count < min`)
- [ ] 过量字节警告 (`mb_count >= max`)

---

## ade77xx_c 专项检查

### 元数据

- [ ] `.id = "ade77xx_c"`
- [ ] `.name = "ADE77xx(C)"`
- [ ] `.longname = "Analog Devices ADE77xx (C)"`
- [ ] `.license = "mit"` (注意: 非 gplv2+)
- [ ] `.tags = {"Analog/digital", "IC", "Sensor", NULL}`, `.num_tags = 3`
- [ ] `.num_annotations = 3`
- [ ] `.num_annotation_rows = 3`

### 寄存器表完整性

- [ ] 0x01 AWATTHR - 16-bit Signed
- [ ] 0x02 BWATTHR - 16-bit Signed
- [ ] 0x03 CWATTHR - 16-bit Signed
- [ ] 0x04 AVARHR - 16-bit Signed
- [ ] 0x05 BVARHR - 16-bit Signed
- [ ] 0x06 CVARHR - 16-bit Signed
- [ ] 0x07 AVAHR - 16-bit Signed
- [ ] 0x08 BVAHR - 16-bit Signed
- [ ] 0x09 CVAHR - 16-bit Signed
- [ ] 0x0A AIRMS - 24-bit Signed
- [ ] 0x0B BIRMS - 24-bit Signed
- [ ] 0x0C CIRMS - 24-bit Signed
- [ ] 0x0D AVRMS - 24-bit Signed
- [ ] 0x0E BVRMS - 24-bit Signed
- [ ] 0x0F CVRMS - 24-bit Signed
- [ ] 0x10 FREQ - 12-bit Unsigned
- [ ] 0x11 TEMP - 8-bit Signed
- [ ] 0x12 WFORM - 24-bit Signed
- [ ] 0x13 OPMODE - 8-bit Unsigned (R/W)
- [ ] 0x14 MMODE - 8-bit Unsigned (R/W)
- [ ] 0x15 WAVMODE - 8-bit Unsigned (R/W)
- [ ] 0x16 COMPMODE - 8-bit Unsigned (R/W)
- [ ] 0x17 LCYCMODE - 8-bit Unsigned (R/W)
- [ ] 0x18 Mask - 24-bit Unsigned (R/W)
- [ ] 0x19 Status - 24-bit Unsigned
- [ ] 0x1A RSTATUS - 24-bit Unsigned
- [ ] 0x1B ZXTOUT - 16-bit Unsigned (R/W)
- [ ] 0x1C LINECYC - 16-bit Unsigned (R/W)
- [ ] 0x1D SAGCYC - 8-bit Unsigned (R/W)
- [ ] 0x1E SAGLVL - 8-bit Unsigned (R/W)
- [ ] 0x1F VPINTLVL - 8-bit Unsigned (R/W)
- [ ] 0x20 IPINTLVL - 8-bit Unsigned (R/W)
- [ ] 0x21 VPEAK - 8-bit Unsigned
- [ ] 0x22 IPEAK - 8-bit Unsigned
- [ ] 0x23 Gain - 8-bit Unsigned (R/W)
- [ ] 0x24 AVRMSGAIN - 12-bit Signed (R/W)
- [ ] 0x25 BVRMSGAIN - 12-bit Signed (R/W)
- [ ] 0x26 CVRMSGAIN - 12-bit Signed (R/W)
- [ ] 0x27 AIGAIN - 12-bit Signed (R/W)
- [ ] 0x28 BIGAIN - 12-bit Signed (R/W)
- [ ] 0x29 CIGAIN - 12-bit Signed (R/W)
- [ ] 0x2A AWG - 12-bit Signed (R/W)
- [ ] 0x2B BWG - 12-bit Signed (R/W)
- [ ] 0x2C CWG - 12-bit Signed (R/W)
- [ ] 0x2D AVARG - 12-bit Signed (R/W)
- [ ] 0x2E BVARG - 12-bit Signed (R/W)
- [ ] 0x2F CVARG - 12-bit Signed (R/W)
- [ ] 0x30 AVAG - 12-bit Signed (R/W)
- [ ] 0x31 BVAG - 12-bit Signed (R/W)
- [ ] 0x32 CVAG - 12-bit Signed (R/W)
- [ ] 0x33 AVRMSOS - 12-bit Signed (R/W)
- [ ] 0x34 BVRMSOS - 12-bit Signed (R/W)
- [ ] 0x35 CVRMSOS - 12-bit Signed (R/W)
- [ ] 0x36 AIRMSOS - 12-bit Signed (R/W)
- [ ] 0x37 BIRMSOS - 12-bit Signed (R/W)
- [ ] 0x38 CIRMSOS - 12-bit Signed (R/W)
- [ ] 0x39 AWATTOS - 12-bit Signed (R/W)
- [ ] 0x3A BWATTOS - 12-bit Signed (R/W)
- [ ] 0x3B CWATTOS - 12-bit Signed (R/W)
- [ ] 0x3C AVAROS - 12-bit Signed (R/W)
- [ ] 0x3D BVAROS - 12-bit Signed (R/W)
- [ ] 0x3E CVAROS - 12-bit Signed (R/W)
- [ ] 0x3F APHCAL - 7-bit Signed (R/W)
- [ ] 0x40 BPHCAL - 7-bit Signed (R/W)
- [ ] 0x41 CPHCAL - 7-bit Signed (R/W)
- [ ] 0x42 WDIV - 8-bit Unsigned (R/W)
- [ ] 0x43 VARDIV - 8-bit Unsigned (R/W)
- [ ] 0x44 VADIV - 8-bit Unsigned (R/W)
- [ ] 0x45 APCFNUM - 16-bit Unsigned (R/W)
- [ ] 0x46 APCFDEN - 12-bit Unsigned (R/W)
- [ ] 0x47 VARCFNUM - 16-bit Unsigned (R/W)
- [ ] 0x48 VARCFDEN - 12-bit Unsigned (R/W)
- [ ] 0x7E CHKSUM - 8-bit Unsigned
- [ ] 0x7F Version - 8-bit Unsigned

### 解码逻辑

- [ ] 命令字节: bit7=write(1)/read(0), bit0-6=reg addr
- [ ] `expected = ceil(reg_bits / 8)` 正确计算
- [ ] 1 字节寄存器: `val = mosi_bytes[1]`
- [ ] 2 字节寄存器: `val = mosi_bytes[1] << 8 | mosi_bytes[2]`
- [ ] 3 字节寄存器: `val = mosi_bytes[1] << 16 | mosi_bytes[2] << 8 | mosi_bytes[3]`
- [ ] 未知寄存器 (reg 不在表中): 警告 "Unknown register!"
- [ ] 短传输 (CS 上升沿时数据不足): 警告 "Short transfer!"
- [ ] 读操作使用 MISO 字节，写操作使用 MOSI 字节
- [ ] 输出格式: `"{REG_NAME}: {$}"` 和 `"@{HEX}"`

---

## adf435x_c 专项检查

### 元数据

- [ ] `.id = "adf435x_c"`
- [ ] `.name = "ADF435x(C)"`
- [ ] `.longname = "Analog Devices ADF4350/1 (C)"`
- [ ] `.license = "gplv3+"` (注意: 非 gplv2+)
- [ ] `.tags = {"Clock/timing", "IC", "Wireless/RF", NULL}`, `.num_tags = 3`
- [ ] `.num_annotations = 2`
- [ ] `.num_annotation_rows = 2`

### 32-bit 字组装

- [ ] BITS 消息中 MOSI bits 正确收集 (MSB first)
- [ ] TRANSFER 消息触发字解码
- [ ] bit 数量不为 32 时: Frame error 警告
- [ ] 32-bit 字正确从 MSB bits 打包: `word = (word << 1) | bit`

### 寄存器地址提取

- [ ] 寄存器地址 = word & 0x7 (低 3 位)
- [ ] 输出: `"Register: {addr}"`, `"Reg: {addr}"`, `"[{addr}]"`

### Reg0 字段

- [ ] FRAC: bits[14:3], 12-bit, 无 parser
- [ ] INT: bits[30:15], 16-bit, checker: v < 23 → "Not Allowed"

### Reg1 字段

- [ ] MOD: bits[14:3], 12-bit
- [ ] Phase: bits[26:15], 12-bit
- [ ] Prescalar: bit[27], 1-bit, parser: 0→"4/5", 1→"8/9"
- [ ] Phase Adjust: bit[28], 1-bit, parser: 0→"Off", 1→"On"

### Reg2 字段 (最复杂)

- [ ] Counter Reset: bit[3], 1-bit, disabled_enabled
- [ ] Charge Pump Three-State: bit[4], 1-bit, disabled_enabled
- [ ] Power-Down: bit[5], 1-bit, disabled_enabled
- [ ] PD Polarity: bit[6], 1-bit, 0→"Negative", 1→"Positive"
- [ ] LDP: bit[7], 1-bit, 0→"10ns", 1→"6ns"
- [ ] LDF: bit[8], 1-bit, 0→"FRAC-N", 1→"INT-N"
- [ ] Charge Pump Current: bits[12:9], 4-bit, 16 级电流值
- [ ] Double Buffer: bit[13], 1-bit, disabled_enabled
- [ ] R Counter: bits[23:14], 10-bit
- [ ] RDIV2: bit[24], 1-bit, disabled_enabled
- [ ] Reference Doubler: bit[25], 1-bit, disabled_enabled
- [ ] MUXOUT: bits[28:26], 3-bit, 8 种模式
- [ ] Low Noise and Low Spur: bits[30:29], 2-bit, 4 种模式

### Reg3 字段

- [ ] Clock Divider: bits[14:3], 12-bit
- [ ] Clock Divider Mode: bits[16:15], 2-bit, 4 种模式
- [ ] CSR Enable: bit[18], 1-bit, disabled_enabled
- [ ] Charge Cancellation: bit[21], 1-bit, disabled_enabled
- [ ] ABP: bit[22], 1-bit, 0→"6ns (FRAC-N)", 1→"3ns (INT-N)"
- [ ] Band Select Clock Mode: bit[23], 1-bit, 0→"Low", 1→"High"

### Reg4 字段

- [ ] Output Power: bits[4:3], 2-bit, output_power parser
- [ ] Output Enable: bit[5], 1-bit, disabled_enabled
- [ ] AUX Output Power: bits[7:6], 2-bit, output_power parser
- [ ] AUX Output Select: bit[8], 1-bit, 0→"Divided Output", 1→"Fundamental"
- [ ] AUX Output Enable: bit[9], 1-bit, disabled_enabled
- [ ] MTLD: bit[10], 1-bit, disabled_enabled
- [ ] VCO Power-Down: bit[11], 1-bit, parser
- [ ] Band Select Clock Divider: bits[19:12], 8-bit
- [ ] RF Divider Select: bits[22:20], 3-bit, parser: "÷{2^v}"
- [ ] Feedback Select: bit[23], 1-bit, 0→"Divided", 1→"Fundamental"

### Reg5 字段

- [ ] LD Pin Mode: bits[23:22], 2-bit, 4 种模式

### Parser 函数

- [ ] `disabled_enabled(v)`: 0→"Disabled", 1→"Enabled"
- [ ] `output_power(v)`: 0→"-4dBm", 1→"-1dBm", 2→"+2dBm", 3→"+5dBm"
- [ ] `cp_current(v)`: 16 级, 格式 "X.XXmA @ 5.1kΩ"
- [ ] `muxout(v)`: 8 种模式
- [ ] `low_noise_spur(v)`: 4 种模式
- [ ] `clock_div_mode(v)`: 4 种模式
- [ ] `rf_divider(v)`: "÷1", "÷2", "÷4", ... "÷64"
- [ ] `ld_pin_mode(v)`: "Low", "Digital Lock Detect", "Low", "High"

### 边界情况

- [ ] reg_addr > 5: 无字段描述，只输出寄存器地址
- [ ] bit 数量 < 32: Frame error 警告
- [ ] bit 数量 > 32: 只取前 32 bits
- [ ] INT 值 < 23: 警告 "Not Allowed"

---

## CMakeLists.txt 修改检查

- [ ] 在 `C_DECODERS` 列表中添加了 `a7105_c`
- [ ] 在 `C_DECODERS` 列表中添加了 `ad5626_c`
- [ ] 在 `C_DECODERS` 列表中添加了 `ad79x0_c`
- [ ] 在 `C_DECODERS` 列表中添加了 `ade77xx_c`
- [ ] 在 `C_DECODERS` 列表中添加了 `adf435x_c`
- [ ] 添加位置在列表末尾，不破坏现有条目

---

## 构建验证

- [ ] `build_incremental.cmd` 执行成功
- [ ] 无编译错误
- [ ] 无编译警告
- [ ] 5 个 DLL 文件生成到 `build.dir/decoders/c_decoders/`
  - [ ] `a7105_c.dll`
  - [ ] `ad5626_c.dll`
  - [ ] `ad79x0_c.dll`
  - [ ] `ade77xx_c.dll`
  - [ ] `adf435x_c.dll`

---

## 运行时验证

- [ ] PXView 启动无崩溃
- [ ] 解码器列表中显示 5 个新 C 解码器
- [ ] 每个 C 解码器能正确堆叠在 SPI 解码器之上
- [ ] 与 Python 版本输出对比一致 (使用相同输入数据)
