# Batch 37: 移植检查清单

## 通用检查项（适用于所有 6 个解码器）

### 文件结构
- [ ] 文件名格式正确：`{decoder_id}_c.c`（`-` 替换为 `_`）
- [ ] 包含必要头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 无编译警告（`-Wall -Wextra`）
- [ ] 无内存泄漏（`destroy` 正确释放所有动态内存）

### srd_c_decoder 结构体
- [ ] `.id` 格式为 `"xxx_c"`（下划线，非连字符）
- [ ] `.name` 格式为 `"XXX(C)"`（英文括号）
- [ ] `.longname` 包含 `(C)` 后缀
- [ ] `.desc` 注明 C implementation
- [ ] `.license` 与 Python 版本一致
- [ ] `.channels = NULL`, `.num_channels = 0`（上层解码器无直接通道）
- [ ] `.optional_channels = NULL`, `.num_optional_channels = 0`
- [ ] `.inputs` 正确声明输入协议
- [ ] `.outputs` 正确声明输出协议（或 NULL）
- [ ] `.tags` 正确声明
- [ ] `.num_annotations = NUM_ANN`
- [ ] `.ann_labels` 第一列为 `""`
- [ ] `.annotation_rows` 所有 ann class 都映射到某行
- [ ] `.binary` 正确声明（或 NULL）
- [ ] `.recv_proto` 函数指针正确赋值
- [ ] `.decode` 函数体为空 `(void)di;`

### srd_c_decoder_entry()
- [ ] 所有 option 默认值在此初始化
- [ ] 字符串 option 使用 `g_variant_new_string()`
- [ ] 整数 option 使用 `g_variant_new_int64()`
- [ ] 浮点 option 使用 `g_variant_new_double()`
- [ ] option values 列表使用 `g_slist_append()`
- [ ] 返回 `&xxx_c_decoder`

### srd_c_decoder_api_version()
- [ ] 返回 `SRD_C_DECODER_API_VERSION`

### recv_proto 实现
- [ ] 函数签名：`void (*recv_proto)(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 获取私有数据前检查 NULL
- [ ] 使用 `strcmp()` 匹配 cmd 字符串
- [ ] 使用 `C_ANN_PUT` 宏输出 annotation
- [ ] 使用 `c_decoder_put_python()` 输出 python 数据
- [ ] 使用 `c_decoder_put_binary()` 输出 binary 数据

### reset/start/destroy
- [ ] `reset`：首次调用时 `g_malloc0` 分配私有结构体
- [ ] `reset`：使用 `memset` 清零，然后初始化特定字段
- [ ] `start`：注册输出端口 `c_decoder_register_output()`
- [ ] `start`：读取 option 值
- [ ] `destroy`：释放私有结构体，设置 `c_decoder_set_private(di, NULL)`

---

## T37-1: ltar_smartdevice_decode_c.c 专项检查

- [ ] 6 个 annotation 正确定义
- [ ] 5 个 annotation row 正确定义
  - [ ] `frame-names`：ANN_FRAME_NAME
  - [ ] `frame-errors`：ANN_FRAME_ERROR
  - [ ] `frame-bit-names`：ANN_FRAME_BIT_NAME
  - [ ] `frame-bits-datas`：ANN_FRAME_BITS_DATA
  - [ ] `block-errors`：ANN_BLOCK_ERROR, ANN_BLOCK_DATA
- [ ] btype 查找表完整 (8 条)
- [ ] weapmode 查找表完整 (2 条)
- [ ] shieldstatus 查找表完整 (3 条)
- [ ] huntingdirection 查找表完整 (2 条)
- [ ] BLOCK 命令正确解析
- [ ] Block type 查找正确
- [ ] TAGGER-STATUS (0x02) 解码
  - [ ] BData0：Player Number (bits 0-2) + Team Number (bits 3-4)
  - [ ] BData1：Weapon Mode (bits 0-1) + Shield State (bits 2-3) + Hunting Direction (bit 4)
  - [ ] BData2：Health Remaining
  - [ ] BData3：Loaded Ammo
  - [ ] BData4：Remaining Ammo Low Byte
  - [ ] BData5：Remaining Ammo High Byte
  - [ ] BData6：Shield Time
  - [ ] BData7：Game Time Minutes
  - [ ] BData8：Game Time Seconds
- [ ] Checksum 验证：`0xFF - sum(all_byte_values)` 结果为 0
- [ ] Block 长度检查正确
- [ ] inputs = `{"ltar_smartdevice", NULL}`
- [ ] outputs = `{"ltar_smartdevice_decode", NULL}`
- [ ] 与 T36-1 (ltar_smartdevice_c) 的 python output 格式兼容

---

## T37-2: sipi_c.c 专项检查

- [ ] 7 个 annotation 正确定义
- [ ] 2 个 annotation row 正确定义
  - [ ] `fields`：ANN_HEADER_TAG, ANN_HEADER_CMD, ANN_HEADER_CH, ANN_ADDRESS, ANN_DATA, ANN_CRC
  - [ ] `warnings`：ANN_WARNING
- [ ] command_codes 查找表完整 (13 条)
  - [ ] 每条包含 code, name, addr_len, data_len
  - [ ] 所有 12 个有效命令码覆盖
- [ ] CRC-CCITT 实现正确
  - [ ] 初始值 0xFFFF
  - [ ] 多项式 0x1021
  - [ ] 与 Python `binascii.crc_hqx()` 结果一致
- [ ] bit_len 从字节级数据正确推算
- [ ] Header 解析正确
  - [ ] Tag (bits 15-13) 提取
  - [ ] Command Code (bits 12-8) 提取和查找
  - [ ] Reserved (bits 7-4) 检查非 0 → warning
  - [ ] Channel (bits 3-1) 提取
  - [ ] Reserved (bit 0) 检查非 0 → warning
- [ ] Payload 解析正确
  - [ ] 根据 addr_len 提取地址字节
  - [ ] 根据 data_len 提取数据字节
- [ ] 变长帧长度计算正确：2 (header) + addr_len + data_len + 2 (CRC)
- [ ] CRC 验证正确
  - [ ] 对整个帧（不含 CRC 本身）计算
  - [ ] 与帧末尾 2 字节比较
  - [ ] 不匹配时输出 warning
- [ ] inputs = `{"lfast", NULL}`
- [ ] outputs = `NULL`

---

## T37-3: tm1637_c.c 专项检查

- [ ] 16 个 annotation 正确定义
- [ ] 3 个 annotation row 正确定义
  - [ ] `bits`：ANN_RESERVED ~ ANN_ON (0-13)
  - [ ] `display`：ANN_DISPLAY_INFO (15)
  - [ ] `warnings`：ANN_WARN (14)
- [ ] 1 个 option 正确定义
  - [ ] `dpoint`：字符串选项，默认 `"Dot"`
  - [ ] option values：`"Dot"`, `"Colon"`
- [ ] 7-segment fonts 查找表完整 (~30 条)
  - [ ] 数字 0-9
  - [ ] 字母 A-F (或部分)
  - [ ] 空格、横线等特殊字符
- [ ] contrasts 数组完整 (8 条 PWM 级别)
- [ ] 状态机正确实现
  - [ ] IDLE → REG_CMD (on START)
  - [ ] REG_CMD → REG_DATA (on COMMAND)
  - [ ] REG_DATA → IDLE (on STOP)
- [ ] Data command (0x40) 解码
  - [ ] RW (bit 5)：Write/Read
  - [ ] ADDR (bits 1-2)：Auto/Fixed 地址模式
  - [ ] MODE (bit 0)：Normal/Test 模式
- [ ] Display command (0x80) 解码
  - [ ] SWITCH (bit 0)：OFF/ON
  - [ ] PWM (bits 1-3)：对比度级别
- [ ] Address command (0xC0) 解码
  - [ ] DIGIT (bits 0-2)：位置 0-7
- [ ] putd() 辅助函数正确实现
- [ ] putr() 辅助函数正确实现
- [ ] Display 缓冲和输出正确
- [ ] Auto 地址递增正确
- [ ] Decimal point 处理（Dot/Colon 选项）
- [ ] inputs = `{"tmc", NULL}`
- [ ] outputs = `{"tm1637", NULL}`

---

## T37-4: tm1638_c.c 专项检查

- [ ] 23 个 annotation 正确定义
- [ ] 5 个 annotation row 正确定义
  - [ ] `bits`：ANN_RESERVED ~ ANN_ON (0-18)
  - [ ] `display`：ANN_DISPLAY_INFO (20)
  - [ ] `leds`：ANN_LEDS_INFO (21)
  - [ ] `keys`：ANN_KEYS_INFO (22)
  - [ ] `warnings`：ANN_WARN (19)
- [ ] 1 个 option 正确定义（同 TM1637 的 dpoint）
- [ ] switches 查找表完整 (24 条)
  - [ ] K1/K2/K3 × KS1-KS6 映射到 S1-S24
- [ ] Address 4 bits 正确处理 (0-3, vs TM1637 的 0-2)
- [ ] 偶数地址 = 数字管，奇数地址 = LED
- [ ] handle_data_digit 复用 TM1637 逻辑
- [ ] handle_data_led 正确实现
  - [ ] bit 0 = Red
  - [ ] bit 1 = Green
  - [ ] LED 缓冲正确更新
- [ ] handle_data_keyboard 正确实现
  - [ ] Read 模式下按键数据解码
  - [ ] switches 查找表映射正确
- [ ] Display + LEDs + Keys info 输出正确
- [ ] 7-segment fonts 与 TM1637 一致
- [ ] inputs = `{"tmc", NULL}`
- [ ] outputs = `{"tm1638", NULL}`

---

## T37-5: pjon_c.c 专项检查

- [ ] 13 个 annotation 正确定义
- [ ] 3 个 annotation row 正确定义
  - [ ] `fields`：ANN_RX_INFO ~ ANN_END_CRC (0-5, 7-10)
  - [ ] `relations`：ANN_RELATION (11)
  - [ ] `warnings`：ANN_WARN (12)
- [ ] CRC-8 实现正确
  - [ ] 多项式 0x97
  - [ ] 与 Python 版本结果一致
- [ ] CRC-32 实现正确
  - [ ] 标准 Ethernet CRC-32
  - [ ] 与 Python `binascii.crc32()` 结果一致
- [ ] Header Config 解析正确 (8 个 flag 位)
  - [ ] bit 0: shared (bus_id)
  - [ ] bit 1: tx_info (sender address)
  - [ ] bit 2: sync_ack
  - [ ] bit 3: async_ack
  - [ ] bit 4: port (service ID)
  - [ ] bit 5: crc32
  - [ ] bit 6: len16
  - [ ] bit 7: pkt_id
- [ ] 字段描述系统正确构建
  - [ ] 固定字段：RX ID (1 byte), Header Config (1 byte)
  - [ ] 条件字段：根据 cfg flags 动态包含
  - [ ] Payload 长度 = pkt_len - overhead
- [ ] 字段处理 handlers 正确实现
  - [ ] pjon_handle_rx_id
  - [ ] pjon_handle_hdr_cfg
  - [ ] pjon_handle_pkt_len (1 or 2 bytes)
  - [ ] pjon_handle_meta_crc
  - [ ] pjon_handle_tx_info
  - [ ] pjon_handle_port
  - [ ] pjon_handle_pkt_id
  - [ ] pjon_handle_payload
  - [ ] pjon_handle_end_crc (1 or 4 bytes)
- [ ] Overhead 计算正确
- [ ] ACK 处理正确
  - [ ] SYNC_RESP_WAIT 切换 ACK 模式
  - [ ] ACK 字节累积和验证
- [ ] Relation 输出正确
  - [ ] 包含 RX ID 和 TX ID
  - [ ] 包含 payload 文本
- [ ] 帧刷新和重置逻辑正确
- [ ] inputs = `{"pjon_link", NULL}`
- [ ] outputs = `NULL`

---

## T37-6: tpm_fifo_tis_c.c 专项检查

- [ ] 6 个 annotation 正确定义
- [ ] 4 个 annotation row 正确定义
  - [ ] `register`：ANN_REG_READ, ANN_REG_WRITE (0-1)
  - [ ] `tpm`：ANN_TPM_CMD, ANN_TPM_RSP (2-3)
  - [ ] `warnings`：ANN_WARN (4)
  - [ ] `states`：ANN_STATE (5)
- [ ] TPM 命令码查找表完整 (~80 条)
- [ ] TPM 响应码查找表完整 (~70 条)
- [ ] TPM 寄存器常量正确定义
  - [ ] TPM_ACCESS_X (0x0000)
  - [ ] TPM_STS_X (0x0018)
  - [ ] TPM_DATA_FIFO_X (0x0024)
  - [ ] 其他寄存器地址
- [ ] 6 个 TPM 状态正确实现
  - [ ] TPM_STATE_UNKNOWN
  - [ ] TPM_STATE_IDLE
  - [ ] TPM_STATE_READY
  - [ ] TPM_STATE_RECEPTION
  - [ ] TPM_STATE_EXECUTION
  - [ ] TPM_STATE_COMPLETION
- [ ] 状态转换逻辑正确
  - [ ] Unknown → Idle (requestUse / activeLocality)
  - [ ] Idle → Ready (commandReady=1)
  - [ ] Ready → Reception (FIFO write start)
  - [ ] Reception → Execution (tpmGo=1)
  - [ ] Execution → Completion (dataAvail=1)
  - [ ] Completion → Idle (commandReady=1)
- [ ] STS 寄存器位域解析正确
  - [ ] stsValid (0x80)
  - [ ] commandReady (0x40)
  - [ ] tpmGo (0x20)
  - [ ] dataAvail (0x10)
  - [ ] Expect (0x08)
  - [ ] selfTestDone (0x04)
  - [ ] responseRetry (0x02)
- [ ] Command buffer 累积正确
  - [ ] 写入 FIFO 时累积
  - [ ] tpmGo 时输出 TPM Command annotation
- [ ] Response buffer 累积正确
  - [ ] 读取 FIFO 时累积
  - [ ] dataAvail 时输出 TPM Response annotation
- [ ] Register Read/Write annotation 正确
- [ ] Warning 输出正确
  - [ ] 异常状态转换
  - [ ] 未知寄存器地址
  - [ ] Buffer 溢出保护
- [ ] tpm_output_command 格式化正确
  - [ ] hex 字符串输出
  - [ ] 命令码名称查找
- [ ] tpm_output_response 格式化正确
  - [ ] hex 字符串输出
  - [ ] 响应码名称查找
- [ ] inputs = `{"tpm-tis", NULL}`
- [ ] outputs = `{"tpm", NULL}`

---

## 构建验证

- [ ] `build_incremental.cmd` 执行成功
- [ ] 6 个 DLL 生成到 `build.dir/decoders/c_decoders/`
- [ ] PXView.exe 可正常启动
- [ ] 解码器列表中可见 6 个新 C 解码器
- [ ] 选择对应下层解码器后可正常堆叠

## 功能验证（如有测试数据）

- [ ] ltar_smartdevice_decode_c：与 Python 版本输出对比
- [ ] sipi_c：与 Python 版本输出对比
- [ ] tm1637_c：与 Python 版本输出对比
- [ ] tm1638_c：与 Python 版本输出对比
- [ ] pjon_c：与 Python 版本输出对比
- [ ] tpm_fifo_tis_c：与 Python 版本输出对比
