# SPI上层协议解码器移植检查清单

本检查清单用于每个解码器移植完成后的质量验证。每个解码器需通过所有检查项才能视为完成。

---

## 通用检查项（适用于所有5个解码器）

### 文件规范

- [ ] 文件名格式正确：`{decoder_id}_c.c`（`-`替换为`_`）
- [ ] 文件位于 `libsigrokdecode/c_decoders/` 目录
- [ ] 包含标准头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 无编译warning（使用 `-Wall -Wextra` 级别检查）

### srd_c_decoder结构体

- [ ] `.id` 格式为 `"xxx_c"`（如 `"nes_gamepad_c"`）
- [ ] `.name` 格式为 `"XXX(C)"`（如 `"NES gamepad(C)"`）
- [ ] `.longname` 包含完整名称和 `"(C)"` 后缀
- [ ] `.desc` 包含 `"(C implementation)"` 说明
- [ ] `.license` 与Python原版一致
- [ ] `.channels = NULL`, `.num_channels = 0`（上层解码器无通道）
- [ ] `.optional_channels = NULL`, `.num_optional_channels = 0`
- [ ] `.inputs` 包含 `{"spi", NULL}`
- [ ] `.outputs` 为 `NULL` 或 `{NULL}`，`.num_outputs = 0`
- [ ] `.binary = NULL`, `.num_binary = 0`（除非Python原版有binary输出）
- [ ] `.recv_proto` 指向正确的回调函数
- [ ] `.decode` 指向空函数（不是NULL）

### annotation规范

- [ ] `ann_labels[][3]` 第一列（索引0）全部为 `""`（空字符串）
- [ ] `ann_labels` 行数 = `NUM_ANN` 枚举值
- [ ] 所有annotation class（0到NUM_ANN-1）都映射到至少一个annotation_row
- [ ] `row_classes[]` 数组以 `-1` 结尾
- [ ] `annotation_rows` 中的 `count` 字段与对应 `row_classes` 数组长度一致（不含-1终止符）
- [ ] `num_annotation_rows` 等于 `annotation_rows` 数组长度
- [ ] `num_annotations` 等于 `NUM_ANN`

### 回调函数

- [ ] `reset()` — 使用 `g_malloc0()` 分配私有数据，`memset()` 清零
- [ ] `reset()` — 首次调用时检查 `c_decoder_get_private(di)` 是否为NULL
- [ ] `start()` — 调用 `c_decoder_register_output(di, SRD_OUTPUT_ANN, "xxx")`
- [ ] `start()` — 读取所有options并初始化状态
- [ ] `decode()` — 空函数体 `(void)di;`
- [ ] `destroy()` — `g_free()` 释放私有数据，`c_decoder_set_private(di, NULL)`
- [ ] `recv_proto()` — 开头检查 `c_decoder_get_private(di)` 非NULL

### 导出函数

- [ ] `srd_c_decoder_entry()` — 初始化所有options的 `.def` 默认值
- [ ] `srd_c_decoder_entry()` — 为string类型options创建 `.values` GSList
- [ ] `srd_c_decoder_entry()` — 返回 `&xxx_c_decoder` 指针
- [ ] `srd_c_decoder_api_version()` — 返回 `SRD_C_DECODER_API_VERSION`
- [ ] 两个导出函数都有 `SRD_C_DECODER_EXPORT` 前缀

### 构建集成

- [ ] `CMakeLists.txt` 的 `C_DECODERS` 列表中已添加解码器名
- [ ] `build_incremental.cmd` 编译成功
- [ ] 输出DLL存在于 `build.dir/decoders/c_decoders/`

---

## nes_gamepad_c 专项检查

### 元数据一致性

- [ ] `.id = "nes_gamepad_c"`
- [ ] `.name = "NES gamepad(C)"`
- [ ] `.tags` 包含 `{"Retro computing", NULL}`
- [ ] `.num_options = 1`（variant选项）
- [ ] variant选项的 `idn = "dec_nes_gamepad_opt_variant"`

### 解码逻辑

- [ ] recv_proto仅处理 `"DATA"` 命令
- [ ] 正确解析MISO值（data[8..15]为uint64 LE）
- [ ] miso==0xFF → ANN_NO_PRESS, "No button is pressed"
- [ ] miso==0x00 → ANN_NOT_CONNECTED, "Gamepad is not connected"
- [ ] 其他值 → 按位解析8个按钮，bit=0为按下
- [ ] 按钮名称顺序正确：A, B, Select, Start, North, South, West, East
- [ ] 多按钮按下时用 " + " 连接

### 边界情况

- [ ] data_len < 17时不崩溃（提前返回）
- [ ] 所有8位都为0（0x00）→ 未连接
- [ ] 所有8位都为1（0xFF）→ 无按钮按下
- [ ] 单按钮按下（如0xFE → A按下）

---

## ssi32_c 专项检查

### 元数据一致性

- [ ] `.id = "ssi32_c"`
- [ ] `.name = "SSI32(C)"`
- [ ] `.tags` 包含 `{"Embedded/industrial", NULL}`
- [ ] `.num_options = 1`（msgsize选项）
- [ ] msgsize选项的 `idn = "dec_ssi32_opt_msgsize"`

### 解码逻辑

- [ ] `CS-CHANGE` → 重置数据收集
- [ ] `DATA` → 收集MOSI/MISO字节
- [ ] 第一个字节 `& 0x80` → ACK帧（4字节）
- [ ] 第一个字节 `& 0x80 == 0` → CTRL帧（msgsize字节）
- [ ] ACK帧：输出 `> ACK:0x%02x` (ANN_ACK_TX) 和 `< ACK:0x%02x` (ANN_ACK_RX)
- [ ] CTRL帧TX：`> CTRL:0x%02x, LUN:0x%02x, SIZE:0x%02x, CRC:0x%02x[, DATA:0x...]`
- [ ] CTRL帧RX：`< CTRL:0x%02x, LUN:0x%02x, SIZE:0x%02x, CRC:0x%02x[, DATA:0x...]`
- [ ] tx_size/rx_size从mosi_bytes[2]/miso_bytes[2]获取
- [ ] es_array正确记录每个字节的end_sample

### 边界情况

- [ ] msgsize选项默认值为64
- [ ] 数据字节数不足时不崩溃
- [ ] tx_size或rx_size为0时不输出DATA部分
- [ ] SSI32_MAX_BYTES足够大（≥128）

---

## nrf24l01_c 专项检查

### 元数据一致性

- [ ] `.id = "nrf24l01_c"`
- [ ] `.name = "nRF24L01(+)(C)"`
- [ ] `.tags` 包含 `{"IC", "Wireless/RF", NULL}`
- [ ] `.num_options = 1`（chip选项）
- [ ] chip选项的 `idn = "dec_nrf24l01_opt_chip"`

### 寄存器表

- [ ] 26个nRF24L01寄存器全部定义（0x00-0x17, 0x1c, 0x1d）
- [ ] 5个xn297扩展寄存器定义（0x19, 0x1a, 0x1b, 0x1e, 0x1f）
- [ ] 寄存器size正确（如RX_ADDR_P0=5, RX_ADDR_P1=5, TX_ADDR=5）
- [ ] chip_type==1时包含xn297扩展寄存器

### 命令解析

- [ ] R_REGISTER (0x00-0x1F) — cmd & 0xe0 == 0x00
- [ ] W_REGISTER (0x20-0x3F) — cmd & 0xe0 == 0x20
- [ ] ACTIVATE (0x50)
- [ ] R_RX_PAYLOAD (0x61)
- [ ] R_RX_PL_WID (0x60)
- [ ] W_TX_PAYLOAD (0xA0)
- [ ] W_TX_PAYLOAD_NOACK (0xB0)
- [ ] W_ACK_PAYLOAD (0xA8-0xAF) — (b & 0xF8) == 0xA8
- [ ] FLUSH_TX (0xE1)
- [ ] FLUSH_RX (0xE2)
- [ ] REUSE_TX_PL (0xE3)
- [ ] NOP (0xFF)
- [ ] xn297: CE_FSPI_ON (0xFD), CE_FSPI_OFF (0xFC), RST_FSPI (0x53)
- [ ] 未知命令 → 输出warning

### finish_command逻辑

- [ ] R_REGISTER → 解码MISO字节为寄存器值（ANN_REG）
- [ ] W_REGISTER → 解码MOSI字节为寄存器值，合并命令名（ANN_CMD）
- [ ] R_RX_PAYLOAD → 解码MISO字节为RX payload（ANN_RX）
- [ ] W_TX_PAYLOAD → 解码MOSI字节为TX payload（ANN_TX）
- [ ] W_TX_PAYLOAD_NOACK → 解码MOSI字节为TX payload（ANN_TX）
- [ ] W_ACK_PAYLOAD → 解码MOSI字节为ACK payload for pipe N（ANN_TX）
- [ ] R_RX_PL_WID → 输出payload宽度（ANN_REG）
- [ ] ACTIVATE → 检查0x8c/0x73，可能变为DEACTIVATE
- [ ] RST_FSPI → 检查0x5a/0xa5，变为RST_FSPI_HOLD/RST_FSPI_RELS
- [ ] 多字节寄存器LSByte first（reversed输出）

### CS-CHANGE处理

- [ ] CS上升沿（释放）时处理已收集命令
- [ ] TRANSFER事件同上处理
- [ ] 首次DATA前需cs_was_released为true
- [ ] 超过max字节数时输出"excess byte" warning
- [ ] 不足min字节数时输出"missing data bytes" warning

### 数据格式化

- [ ] always_hex=True时：每个字节 `%02X`
- [ ] always_hex=False时：可打印字符直接显示，不可打印 `\x%02X`
- [ ] 双行annotation格式：`label = "{$}"` + `@data`

---

## nrf905_c 专项检查

### 元数据一致性

- [ ] `.id = "nrf905_c"`
- [ ] `.name = "nRF905(C)"`
- [ ] `.license = "mit"`（注意：Python原版为MIT许可）
- [ ] `.tags` 包含 `{"IC", "Wireless/RF", NULL}`
- [ ] `.num_options = 0`（无选项）

### 配置寄存器字段

- [ ] CFG_REG[0] — CH_NO(8bit)
- [ ] CFG_REG[1] — AUTO_RETRAN(1bit), RX_RED_PWR(1bit), PA_PWR(2bit), HFREQ_PLL(1bit), CH_NO_8(1bit)
- [ ] CFG_REG[2] — TX_AFW(3bit), RX_AFW(3bit)
- [ ] CFG_REG[3] — RW_PW(6bit)
- [ ] CFG_REG[4] — TX_PW(6bit)
- [ ] CFG_REG[5] — RX_ADDR_0(8bit)
- [ ] CFG_REG[6] — RX_ADDR_1(8bit)
- [ ] CFG_REG[7] — RX_ADDR_2(8bit)
- [ ] CFG_REG[8] — RX_ADDR_3(8bit)
- [ ] CFG_REG[9] — CRC_MODE(1bit), CRC_EN(1bit), XOR(3bit), UP_CLK_EN(1bit), UP_CLK_FREQ(2bit)
- [ ] CHN_CFG — PA_PWR(2bit), HFREQ_PLL(1bit)
- [ ] STAT_REG — AM(1bit), DR(1bit)

### 命令处理

- [ ] W_CONFIG (0x00-0x0F) — (cmd & 0xF0) == 0x00
- [ ] R_CONFIG (0x10-0x1F) — (cmd & 0xF0) == 0x10
- [ ] W_TX_PAYLOAD (0x20)
- [ ] R_TX_PAYLOAD (0x21)
- [ ] W_TX_ADDRESS (0x22)
- [ ] R_TX_ADDRESS (0x23)
- [ ] R_RX_PAYLOAD (0x24)
- [ ] CHANNEL_CONFIG (0x80-0x8F) — (cmd & 0xF0) == 0x80

### extract_bits函数

- [ ] `begin = 7 - start_bit` 正确
- [ ] `end = begin + num_bits` 正确
- [ ] 边界检查：begin < 0 或 end > 8 返回0

### CS-CHANGE处理

- [ ] CS下降沿(assert) → 记录cmd_ss，开始收集
- [ ] CS上升沿(deassert) → 处理命令，reset
- [ ] DATA仅在CS asserted时收集

### STATUS处理

- [ ] 每个命令处理后都处理STATUS字节（MISO第一字节）
- [ ] STATUS输出为ANN_REG_RD

---

## rfm12_c 专项检查

### 元数据一致性

- [ ] `.id = "rfm12_c"`
- [ ] `.name = "RFM12(C)"`
- [ ] `.tags` 包含 `{"Wireless/RF", NULL}`
- [ ] `.num_options = 0`（无选项）

### 命令分发（17种）

- [ ] 0x80 → Configuration command
- [ ] 0x82 → Power management
- [ ] 0xA0-0xAF → Frequency setting（cmd[0] & 0xF0 == 0xA0）
- [ ] 0xC6 → Data rate
- [ ] 0x90-0x97 → Receiver control（cmd[0] & 0xF8 == 0x90）
- [ ] 0xC2 → Data filter
- [ ] 0xCA → FIFO and reset
- [ ] 0xCE → Synchron pattern
- [ ] 0xB0 → FIFO read
- [ ] 0xC4 → AFC
- [ ] 0x98-0x99 → Transceiver control（cmd[0] & 0xFE == 0x98）
- [ ] 0xCC → PLL setting
- [ ] 0xB8 → Transmitter register
- [ ] 0xFE → Software reset
- [ ] 0xE0-0xFF → Wake-up timer（cmd[0] & 0xE0 == 0xE0）
- [ ] 0xC8 → Low duty cycle
- [ ] 0xC0 → Low battery detector
- [ ] 0x00 → Status read
- [ ] 未知命令 → 输出"Unknown command"

### 状态跟踪初始化

- [ ] `last_status = {0x00, 0x00}`
- [ ] `last_config = 0x08`
- [ ] `last_power = 0x08`
- [ ] `last_freq = 0x680`
- [ ] `last_data_rate = 0x23`
- [ ] `last_fifo_and_reset = 0x80`
- [ ] `last_afc = 0xF7`
- [ ] `last_transceiver = 0x00`
- [ ] `last_pll = 0x77`

### 关键命令验证

- [ ] Configuration: 频率(315/433/868/915MHz)，电容(8.5-16pF)
- [ ] Power management: 8个bit位描述(er, ebb, et, es, ex, eb, ew, dc)
- [ ] Frequency: F值计算正确
- [ ] Data rate: rate = 10000 / 29.0 / (r + 1) / (1 + 7 * cs)
- [ ] FIFO read: 输出FIFO读取命令 + 返回数据
- [ ] Status read: 11个状态位解析

### 位级标注（如实现）

- [ ] `ann_to_row` 映射正确：[0,0,0,1,1,2]
- [ ] `row_pos` 初始化正确：[0, 8, 8]（2字节命令后）
- [ ] `putx()` 正确使用mosi_bits的ss/es
- [ ] `describe_bits()` 正确区分ANN_PARAMS和ANN_DISABLED
- [ ] `describe_return_bits()` 正确区分ANN_RETURN和ANN_DISABLED_RETURN
- [ ] `describe_changed_bits()` 正确输出ANN_INTERPRETATION
- [ ] `advance_ann()` 正确推进row_pos

---

## 运行时验证（所有解码器）

### 解码器加载

- [ ] PXView启动时无DLL加载错误
- [ ] 解码器列表中显示C版本名称（如"NES gamepad(C)"）
- [ ] 解码器可正确堆叠在SPI解码器之上
- [ ] 选项对话框正确显示所有选项及默认值

### 功能验证

- [ ] 使用对应硬件或demo信号文件测试
- [ ] annotation输出与Python版本功能等价
- [ ] 无内存泄漏（长时间运行稳定）
- [ ] CS-CHANGE/DATA/TRANSFER事件处理正确

### 与Python版本对比

- [ ] 相同输入下，annotation文本内容一致
- [ ] annotation颜色/行分配一致
- [ ] 选项配置效果一致
- [ ] 边界情况处理一致（未知命令、数据不足等）
