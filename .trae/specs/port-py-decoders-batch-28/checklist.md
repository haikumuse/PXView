# 移植验证清单 — Batch 28

## 通用检查项（适用于所有解码器）

### 文件结构

- [ ] 文件名格式正确：`{decoder_id}_c.c`（`-` 替换为 `_`）
- [ ] 头文件包含完整：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 文件末尾有换行符

### srd_c_decoder 结构体

- [ ] `.id` 格式为 `"{python_id}_c"`（如 `x2444m_c`, `rgb_led_spi_c`）
- [ ] `.name` 包含 `(C)` 后缀
- [ ] `.longname` 包含 `(C)` 后缀
- [ ] `.desc` 末尾包含 `(C implementation)` 说明
- [ ] `.license` 为 `"gplv2+"`
- [ ] `.channels = NULL`, `.num_channels = 0`（上层解码器无物理通道）
- [ ] `.optional_channels = NULL`, `.num_optional_channels = 0`
- [ ] `.inputs` 为 `{"spi", NULL}`，`.num_inputs = 1`
- [ ] `.outputs = NULL`, `.num_outputs = 0`（上层解码器通常无输出）
- [ ] `.binary = NULL`, `.num_binary = 0`
- [ ] `.recv_proto` 指向正确的回调函数
- [ ] `.decode` 指向空函数体函数
- [ ] `.reset` / `.start` / `.destroy` 均已实现

### Annotation 检查

- [ ] `ann_labels` 第一列全部为 `""`（空字符串）
- [ ] `ann_labels` 行数 = `NUM_ANN`
- [ ] 所有 annotation class 都映射到某个 annotation_row
- [ ] `annotation_rows` 中每个 row 的 class 列表以 `-1` 结尾
- [ ] `num_annotation_rows` 与 `annotation_rows` 数组长度一致
- [ ] `num_annotations` = `NUM_ANN`

### 生命周期函数

- [ ] `reset()` 中使用 `g_malloc0` 分配状态（首次），`memset` 清零
- [ ] `start()` 中通过 `c_decoder_register_output(di, SRD_OUTPUT_ANN, "...")` 注册输出
- [ ] `decode()` 函数体为空 `(void)di;`
- [ ] `destroy()` 中 `g_free` 释放状态，`c_decoder_set_private(di, NULL)`

### 导出函数

- [ ] `srd_c_decoder_entry()` 返回解码器结构体指针
- [ ] `srd_c_decoder_api_version()` 返回 `SRD_C_DECODER_API_VERSION`
- [ ] 两个函数均有 `SRD_C_DECODER_EXPORT` 前缀
- [ ] Options 默认值在 `srd_c_decoder_entry()` 中初始化（如有 options）

### recv_proto 回调

- [ ] 函数签名正确：`(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 获取 private state 前检查 NULL
- [ ] 使用 `strcmp(cmd, "DATA")` 和 `strcmp(cmd, "CS-CHANGE")` 判断命令类型
- [ ] DATA 命令解析：`data[0]`=have_mosi, `data[1..8]`=mosi(LE uint64), `data[9]`=have_miso, `data[10..17]`=miso(LE uint64)
- [ ] CS-CHANGE 命令解析：`data[0]`=old_cs_inv, `data[1]`=new_cs
- [ ] 不处理 BITS 和 TRANSFER 命令（除非需要）

---

## x2444m_c 专项检查

### 元数据一致性

- [ ] `.id = "x2444m_c"`
- [ ] `.name = "X2444M/P(C)"`
- [ ] `.longname = "Xicor X2444M/P (C)"`
- [ ] `.desc = "Xicor X2444M/P nonvolatile static RAM protocol. (C implementation)"`
- [ ] `.tags = {"IC", "Memory", NULL}`, `.num_tags = 2`
- [ ] `.options = NULL`, `.num_options = 0`

### Annotation 完整性

- [ ] 8 个 annotation class：WRDS, STO, SLEEP, WRITE, WREN, RCL, READ, READ2
- [ ] `ann_labels` 第二列：`"WRDS"`, `"STO"`, `"SLEEP"`, `"WRITE"`, `"WREN"`, `"RCL"`, `"READ"`, `"READ"`
- [ ] `ann_labels` 第三列：`"Write disable"`, `"Store RAM data in EEPROM"`, `"Enter sleep mode"`, `"Write data into RAM"`, `"Write enable"`, `"Recall EEPROM data into RAM"`, `"Data read from RAM"`, `"Data read from RAM"`
- [ ] 2 个 annotation_rows：`cmds`(5 classes) + `data`(3 classes)

### 寄存器查找表

- [ ] 8 个条目对应地址 0x80~0x87
- [ ] 索引方式：`addr & 0x07`（等价于 Python 的 `addr & 0x87` 后取低 3 位）
- [ ] WRDS/STO/SLEEP/WREN/RCL 标记为 `has_value=0`
- [ ] WRITE/READ/READ2 标记为 `has_value=1`

### 状态机逻辑

- [ ] CS asserted 时重置：`cmd_digit=0`, `read_value=0`, `write_value=0`
- [ ] DATA + `cmd_digit==0`：存储 `addr`（MOSI 值）和 `addr_start`
- [ ] DATA + `cmd_digit>0`：`read_value = (read_value << 8) | miso_byte`，`write_value = (write_value << 8) | mosi_byte`
- [ ] CS deasserted + `cmd_digit==1`：输出简单命令（2 级文本）
- [ ] CS deasserted + `cmd_digit>1`：
  - [ ] READ 命令使用 `read_value`
  - [ ] WRITE 命令使用 `write_value`
  - [ ] 地址字段 = `(addr >> 3) & 0x0f`
  - [ ] 输出 4 级文本：long, short, tiny, value

### 边界条件

- [ ] `data_len` 不足时安全返回（不越界访问）
- [ ] CS 未 asserted 时收到 DATA 被忽略
- [ ] `cmd_digit` 为 0 时 CS deasserted 不产生输出（不应发生但需防御）

---

## rgb_led_spi_c 专项检查

### 元数据一致性

- [ ] `.id = "rgb_led_spi_c"`
- [ ] `.name = "RGB LED(SPI)(C)"`
- [ ] `.longname = "RGB LED string decoder (SPI) (C)"`
- [ ] `.desc = "RGB LED string protocol (RGB values clocked over SPI). (C implementation)"`
- [ ] `.tags = {"Display", NULL}`, `.num_tags = 1`
- [ ] `.options = NULL`, `.num_options = 0`

### Annotation 完整性

- [ ] 1 个 annotation class：`ANN_RGB`
- [ ] `ann_labels[0] = {"", "RGB", "RGB values"}`
- [ ] 1 个 annotation_row：`{"rgb", "RGB values", {ANN_RGB, -1}, 1}`

### 状态机逻辑

- [ ] 只处理 `"DATA"` 命令
- [ ] 只使用 MOSI 值（`have_mosi` 检查）
- [ ] `byte_count == 0` 时记录 `ss_cmd = start_sample`
- [ ] 每个字节存入 `mosi_bytes[byte_count]`，`byte_count++`
- [ ] `byte_count == 3` 时：
  - [ ] `red = mosi_bytes[0]`, `green = mosi_bytes[1]`, `blue = mosi_bytes[2]`
  - [ ] `rgb_value = (red << 16) | (green << 8) | blue`
  - [ ] 输出 `#%.6x` 格式
  - [ ] `ss = ss_cmd`, `es = end_sample`
  - [ ] 重置 `byte_count = 0`
- [ ] `byte_count < 3` 时返回等待

### 边界条件

- [ ] `data_len` 不足时安全返回
- [ ] `have_mosi == 0` 时忽略（无 MOSI 数据）
- [ ] 可选：CS deasserted 时重置 `byte_count`（Python 原版未实现，C 版可增强）

---

## Build 集成检查

### CMakeLists.txt

- [ ] `C_DECODERS` 列表已添加 `x2444m_c`
- [ ] `C_DECODERS` 列表已添加 `rgb_led_spi_c`
- [ ] 列表项之间以空格分隔
- [ ] 列表末尾的 `)` 未丢失

### 编译验证

- [ ] `build_incremental.cmd` 执行成功
- [ ] 无编译 error
- [ ] 无编译 warning
- [ ] `build.dir/decoders/c_decoders/x2444m_c.dll` 生成
- [ ] `build.dir/decoders/c_decoders/rgb_led_spi_c.dll` 生成

### 运行时验证

- [ ] PXView 启动无崩溃
- [ ] 解码器列表中可见 `X2444M/P(C)` 和 `RGB LED(SPI)(C)`
- [ ] SPI + x2444m_c 叠加解码器可正常添加
- [ ] SPI + rgb_led_spi_c 叠加解码器可正常添加
- [ ] 解码输出与 Python 版本一致（对比验证）

---

## Python 对比验证

### x2444m 对比要点

| 场景 | Python 输出 | C 应输出 |
|------|------------|----------|
| WRDS 命令（单字节 0x80） | `["WRDS", "W"]` | `C_ANN_PUT(ANN_WRDS, "WRDS", "W")` |
| STO 命令（单字节 0x81） | `["STO", "S"]` | `C_ANN_PUT(ANN_STO, "STO", "S")` |
| WRITE 命令（0x83 + data） | `["WRITE: 0x3 => 0xab", "W: 0x3 => 0xab", "W", "@00ab"]` | `C_ANN_PUT(ANN_WRITE, long, short, tiny, val)` |
| READ 命令（0x86 + data） | `["READ: 0x3 => 0xcd", "R: 0x3 => 0xcd", "R", "@00cd"]` | `C_ANN_PUT(ANN_READ, long, short, tiny, val)` |

### rgb_led_spi 对比要点

| 场景 | Python 输出 | C 应输出 |
|------|------------|----------|
| R=0xFF, G=0x00, B=0x00 | `["#ff0000"]` | `C_ANN_PUT(ANN_RGB, "#ff0000")` |
| R=0x00, G=0xFF, B=0x00 | `["#00ff00"]` | `C_ANN_PUT(ANN_RGB, "#00ff00")` |
| R=0x00, G=0x00, B=0xFF | `["#0000ff"]` | `C_ANN_PUT(ANN_RGB, "#0000ff")` |
| R=0xFF, G=0xFF, B=0xFF | `["#ffffff"]` | `C_ANN_PUT(ANN_RGB, "#ffffff")` |
