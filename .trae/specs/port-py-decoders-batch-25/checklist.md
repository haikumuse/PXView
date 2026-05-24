# 移植验证清单 — SPI 上层 Python 解码器 → C 解码器

## 通用验证项（适用于所有 5 个解码器）

### G1. 文件结构验证

- [ ] 文件路径正确：`libsigrokdecode/c_decoders/<id>_c.c`
- [ ] 文件名中 `-` 替换为 `_`（如 enc28j60_c.c, mrf24j40_c.c）
- [ ] 包含必要的头文件：`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<glib.h>`, `"libsigrokdecode.h"`

### G2. srd_c_decoder 结构体验证

- [ ] `.id` 格式为 `"<python_id>_c"`（如 `"enc28j60_c"`, `"ltc242x_c"`）
- [ ] `.name` 格式为 `"<CHIP_NAME>(C)"`（如 `"ENC28J60(C)"`, `"LTC242x(C)"`）
- [ ] `.longname` 包含 `(C)` 后缀
- [ ] `.desc` 包含 `(C implementation)` 后缀
- [ ] `.license` = `"gplv2+"`
- [ ] `.channels` = `NULL`, `.num_channels` = `0`
- [ ] `.optional_channels` = `NULL`, `.num_optional_channels` = `0`
- [ ] `.inputs` = `{"spi", NULL}`, `.num_inputs` = `1`
- [ ] `.outputs` = `{NULL}`, `.num_outputs` = `0`
- [ ] `.binary` = `NULL`, `.num_binary` = `0`
- [ ] `.decode` = 空函数（仅 `(void)di;`）
- [ ] `.recv_proto` = 正确的回调函数指针
- [ ] `.metadata` = `NULL`（SPI 上层解码器不需要 metadata）

### G3. Annotation 验证

- [ ] `ann_labels` 第一列必须为 `""` (空字符串)
- [ ] `ann_labels` 每行 3 个字符串：`{"", short_name, long_description}`
- [ ] `NUM_ANN` 枚举值正确，等于 annotation 总数
- [ ] 所有 annotation class 都映射到某个 annotation_row
- [ ] `annotation_rows` 中每个 `ann_classes` 数组以 `-1` 结尾
- [ ] `annotation_rows` 中 `num_ann_classes` 值正确（不含 `-1` 终止符的元素数）

### G4. 生命周期函数验证

- [ ] `reset()`: 使用 `g_malloc0()` 分配私有数据（首次），`memset()` 清零
- [ ] `start()`: 调用 `c_decoder_register_output(di, SRD_OUTPUT_ANN, "<name>")` 注册输出
- [ ] `start()`: 读取选项值（如有）
- [ ] `decode()`: 空函数体 `(void)di;`
- [ ] `destroy()`: 使用 `g_free()` 释放私有数据，设置 `c_decoder_set_private(di, NULL)`

### G5. 入口函数验证

- [ ] `srd_c_decoder_entry()` 返回解码器结构体指针
- [ ] `srd_c_decoder_entry()` 中初始化所有 options 的 `.def` 值
- [ ] `srd_c_decoder_entry()` 中初始化所有 options 的 `.values` 列表（如有可选值）
- [ ] `srd_c_decoder_api_version()` 返回 `SRD_C_DECODER_API_VERSION`
- [ ] 两个入口函数都有 `SRD_C_DECODER_EXPORT` 前缀

### G6. recv_proto 验证

- [ ] 函数签名正确：`void xxx_recv_proto(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 开头获取私有数据并检查 NULL
- [ ] 正确处理 `"CS-CHANGE"` 事件
- [ ] 正确处理 `"DATA"` 事件
- [ ] 正确解析 SPI DATA 格式：`data[0]=flag, data[1..8]=mosi_le, data[9..16]=miso_le`
- [ ] 正确解析 SPI CS-CHANGE 格式：`data[0]=old_cs, data[1]=new_cs`
- [ ] 忽略不相关的事件类型（如 `"BITS"`, `"TRANSFER"`，除非需要）

### G7. 编译验证

- [ ] CMakeLists.txt 的 `C_DECODERS` 列表已添加新解码器名
- [ ] `build_incremental.cmd` 编译无错误
- [ ] `build.dir/decoders/c_decoders/` 下生成对应的 DLL 文件

---

## ltc242x_c 专项验证

### L1. 元数据验证

- [ ] `.id` = `"ltc242x_c"`
- [ ] `.name` = `"LTC242x(C)"`
- [ ] `.tags` = `{"IC", "Analog/digital", NULL}`, `.num_tags` = `2`
- [ ] `.num_options` = `1` (vref)
- [ ] `.num_annotations` = `2` (CH0 voltage, CH1 voltage)
- [ ] `.num_annotation_rows` = `2` (ch0_voltages, ch1_voltages)

### L2. 选项验证

- [ ] `vref` 选项 `.def` = `g_variant_new_double(1.5)`
- [ ] `start()` 中使用 `c_decoder_get_option_double(di, "vref", 1.5)` 读取

### L3. 解码逻辑验证

- [ ] BITS 事件正确解析 MISO 位数据
- [ ] CS-CHANGE cs_old=0,cs_new=1 时完成数据读取
- [ ] `data >>= 1` 修正多余的左移
- [ ] 电压计算：`-(0x200000 - (data & 0x3FFFFF)) / 0xFFFFF * vref`
- [ ] 通道选择：`(data >> 22) & 1`
- [ ] 输出两种格式：`%.6fV` 和 `%.2fV`
- [ ] CH0 使用 `ANN_CH0_VOLTAGE`，CH1 使用 `ANN_CH1_VOLTAGE`

---

## max7219_c 专项验证

### M7_1. 元数据验证

- [ ] `.id` = `"max7219_c"`
- [ ] `.name` = `"MAX7219(C)"`
- [ ] `.tags` = `{"Display", NULL}`, `.num_tags` = `1`
- [ ] `.num_options` = `0`
- [ ] `.num_annotations` = `3` (register, digit, warning)
- [ ] `.num_annotation_rows` = `2` (commands, warnings)

### M7_2. 解码逻辑验证

- [ ] CS-CHANGE new_cs=0 → `cs_asserted=1`, `pos=0`, `cs_start=ss`
- [ ] CS-CHANGE new_cs=1 → 检查 pos 长度
- [ ] DATA pos=0 → 记录地址和 `addr_start`
- [ ] DATA pos=1 → 处理寄存器
- [ ] addr 1-8 → `ANN_DIGIT`, 格式 `"Digit %d: %02X"`
- [ ] addr 0x00 → `ANN_REG`, "No-op"
- [ ] addr 0x09 → `ANN_REG`, "Decode: 0bXXXXXXXX"
- [ ] addr 0x0A → `ANN_REG`, intensity 解码
- [ ] addr 0x0B → `ANN_REG`, "Scan limit: N"
- [ ] addr 0x0C → `ANN_REG`, "Shutdown: on/off"
- [ ] addr 0x0F → `ANN_REG`, "Display test: on/off"
- [ ] 未知地址 → `ANN_WARNING`, "Unknown register XX"
- [ ] pos=1 且 CS deasserted → `ANN_WARNING`, "Short write"
- [ ] pos>2 且 CS deasserted → `ANN_WARNING`, "Overlong write"
- [ ] pos=0 且 CS deasserted → 不输出警告

---

## max6954_c 专项验证

### M6_1. 元数据验证

- [ ] `.id` = `"max6954_c"`
- [ ] `.name` = `"MAX6954(C)"`
- [ ] `.tags` = `{"Display", NULL}`, `.num_tags` = `1`
- [ ] `.num_options` = `0`
- [ ] `.num_annotations` = `3` (register, digit, warning)
- [ ] `.num_annotation_rows` = `2` (commands, warnings)

### M6_2. 寄存器解码验证

- [ ] 0x00 No-op: 空字符串值
- [ ] 0x01 Decode Mode: `0b%08b` 格式
- [ ] 0x02 Global Intensity: 0→min, 15→max, 其他→数值
- [ ] 0x03 Scan limit: `1 + val`
- [ ] 0x04 Configuration: 多字段解码（Shutdown, Blink rate, Blink, Reset blink, Clear data, Intensity control）
- [ ] 0x05 GPIO Data: P0-P4 各位状态
- [ ] 0x07 Display test: on/off
- [ ] 0x08-0x0B KEY masks: `0b%08b` 格式
- [ ] 0x0C Digit Type: 8 位独立解码或 All 14-seg / All 16/7-seg
- [ ] 0x10-0x17 Intensity pairs: 双通道 intensity
- [ ] 0x20-0x2F Digit P0: 字符显示
- [ ] 0x40-0x4F Digit P1: 字符显示
- [ ] 0x60-0x6F Digit Both: 字符显示
- [ ] 未知地址 → ANN_WARNING

### M6_3. 辅助函数验证

- [ ] `_decode_intensity()`: 0→"min", 15→"max", 其他→数值字符串
- [ ] `_decode_configuration()`: 正确解析所有位字段
- [ ] `_decode_digit_type()`: 0xFF→"All 14-seg", 0x00→"All 16/7-seg", 其他→逐位
- [ ] `_decode_digit()`: 可打印 ASCII 字母→`'X'`，其他→`0xXX`

---

## enc28j60_c 专项验证

### E1. 元数据验证

- [ ] `.id` = `"enc28j60_c"`
- [ ] `.name` = `"ENC28J60(C)"`
- [ ] `.tags` = `{"Embedded/industrial", "Networking", NULL}`, `.num_tags` = `2`
- [ ] `.num_options` = `0`
- [ ] `.num_annotations` = `10` (RCR, RBM, WCR, WBM, BFS, BFC, SRC, DATA, REG_ADDR, WARNING)
- [ ] `.num_annotation_rows` = `3` (commands, fields, warnings)

### E2. 寄存器名称表验证

- [ ] 4 个 bank × 32 个寄存器名称完整
- [ ] Bank 0: ERDPTL..ECON1
- [ ] Bank 1: EHT0..ECON1
- [ ] Bank 2: MACON1..ECON1
- [ ] Bank 3: MAADR5..ECON1
- [ ] `'—'` (U+2014) 使用 UTF-8 编码 `"\xe2\x80\x94"`
- [ ] `"Reserved"` 正确标记

### E3. 命令处理验证

- [ ] RCR (opcode 0x00):
  - [ ] 2 字节：非 MAC/MII 寄存器读取
  - [ ] 3 字节：MAC/MII 寄存器读取（含 dummy byte）
  - [ ] 长度不匹配时输出警告
  - [ ] MAC/MII 判断：寄存器名以 'M' 开头
  - [ ] 未知 bank 时跳过 MAC/MII 检查
- [ ] RBM (opcode 0x20):
  - [ ] header 必须为 0x3A，否则警告
  - [ ] 输出 MISO 数据字节
  - [ ] 显示长度信息
- [ ] WCR (opcode 0x40):
  - [ ] 必须为 2 字节，否则警告
  - [ ] 写 ECON1 (addr=0x1F) 时更新 bsel0/bsel1
- [ ] WBM (opcode 0x60):
  - [ ] header 必须为 0x7A，否则警告
  - [ ] 输出 MOSI 数据字节
- [ ] BFS (opcode 0x80):
  - [ ] 必须为 2 字节
  - [ ] 数据以二进制格式显示
  - [ ] ECON1 的 BSEL0/BSEL1 位被置 1
- [ ] BFC (opcode 0xA0):
  - [ ] 必须为 2 字节
  - [ ] 数据以二进制格式显示
  - [ ] ECON1 的 BSEL0/BSEL1 位被清 0
- [ ] SRC (opcode 0xE0):
  - [ ] 必须为 1 字节
  - [ ] 重置 bsel0=0, bsel1=0
- [ ] 未知 opcode → 警告

### E4. Bank 追踪验证

- [ ] 初始状态 bsel0/bsel1 未知
- [ ] WCR 写 ECON1 → 更新 bsel0/bsel1
- [ ] BFS 写 ECON1 → 置位 bsel0/bsel1
- [ ] BFC 写 ECON1 → 清除 bsel0/bsel1
- [ ] SRC → 重置 bsel0=0, bsel1=0
- [ ] bank 未知时寄存器名显示 "Reg Bank ? Addr XX"
- [ ] bank 已知时显示具体寄存器名

### E5. 注解范围验证

- [ ] 命令注解 (putc) 使用 `cmd_ss` 到 `cmd_es`
- [ ] 寄存器地址注解 (putr) 使用 `range_ss` 到 `range_es`
- [ ] 数据字节注解 (putr) 使用对应字节的范围

---

## mrf24j40_c 专项验证

### MR1. 元数据验证

- [ ] `.id` = `"mrf24j40_c"`
- [ ] `.name` = `"MRF24J40(C)"`
- [ ] `.tags` = `{"IC", "Wireless/RF", NULL}`, `.num_tags` = `2`
- [ ] `.num_options` = `0`
- [ ] `.num_annotations` = `12`
- [ ] `.num_annotation_rows` = `10`

### MR2. 寄存器名称表验证

- [ ] 短寄存器表 (0x00-0x3F) 完整：64 个名称
- [ ] 长寄存器查找函数覆盖所有区域
- [ ] lregs 表 (0x200-0x24C) 完整

### MR3. 短寄存器处理验证

- [ ] 地址解析：`reg = (mosi_bytes[0] >> 1) & 0x3F`
- [ ] 读/写判断：`write = mosi_bytes[0] & 0x1`
- [ ] 读操作使用 `miso_bytes[1]`
- [ ] 写操作使用 `mosi_bytes[1]`
- [ ] 寄存器名查找：`sregs[reg]`，未找到返回 "illegal"

### MR4. 长寄存器处理验证

- [ ] 地址解析：`dword = mosi_bytes[0] << 8 | mosi_bytes[1]`
- [ ] 读/写判断：`write = dword & (1 << 4)`
- [ ] 寄存器地址：`reg = (dword >> 5) & 0x3FF`
- [ ] 数据字节：写用 `mosi_bytes[2]`，读用 `miso_bytes[2]`
- [ ] 区域名称正确（TX, TX beacon, TX GTS1/2, RF control, Security keys, Reserved, RX）

### MR5. 帧缓存验证

- [ ] TX 帧触发：短寄存器写 TXNCON (0x1B) 且 bit0=1
- [ ] RX 帧触发：短寄存器写 RXFLUSH (0x0D) 且 bit0=1
- [ ] 长寄存器 TX 区域访问时累积帧数据到 `framecache[TX]`
- [ ] 长寄存器 RX 区域访问时累积帧数据到 `framecache[RX]`
- [ ] 帧输出格式：`"TX frame: XX XX XX ..."` 或 `"RX frame: XX XX XX ..."`
- [ ] 帧输出后清空对应 framecache

### MR6. TXSTAT 特殊处理验证

- [ ] 读取 TXSTAT (0x24) 时检查重试次数
- [ ] `numretries = (miso_bytes[1] & 0xC0) >> 6`
- [ ] numretries > 0 时输出重试注解
- [ ] `txfail = (miso_bytes[1] & 0x01) != 0`
- [ ] txfail=1 → `ANN_TX_FAIL`, "TX fail (>= 4 retries)"
- [ ] txfail=0 → `ANN_TX_RETRY_N`, "TX retries: N"
- [ ] CCAFAIL 检查：`(miso_bytes[1] & (1 << 5)) != 0`
- [ ] CCAFAIL → `ANN_CCAFAIL`, "CCAFAIL (channel busy)"

### MR7. CS-CHANGE 处理验证

- [ ] CS deasserted 且 byte_count 不在 {0, 2, 3} → "Misplaced CS!" 警告
- [ ] CS deasserted 且 byte_count 为 0/2/3 → 正常，不警告
- [ ] 警告后 reset_data() 清空缓冲区

### MR8. 字节计数验证

- [ ] 短寄存器：2 字节后立即处理
- [ ] 长寄存器：3 字节后处理
- [ ] byte_count 正确递增
- [ ] reset_data() 正确清空所有缓冲区

---

## 最终集成验证

### F1. 编译验证

- [ ] 所有 5 个 C 文件编译无错误
- [ ] 无编译器警告（特别是 format-truncation 类警告）
- [ ] 5 个 DLL 文件生成在 `build.dir/decoders/c_decoders/`

### F2. 运行时验证

- [ ] PXView 启动无崩溃
- [ ] 解码器选择列表中显示 5 个新 C 解码器
- [ ] 每个解码器可与 SPI 解码器正确堆叠
- [ ] 解码器选项界面正确显示（ltc242x 的 vref 选项）

### F3. 功能对比验证

- [ ] C 解码器输出与 Python 解码器输出一致
- [ ] Annotation 类型正确
- [ ] Annotation 行分配正确
- [ ] 警告信息正确触发
