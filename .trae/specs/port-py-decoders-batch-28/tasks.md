# 移植任务清单 — Batch 28

## 任务总览

| 任务ID | 解码器 | 文件名 | 优先级 | 预估复杂度 |
|--------|--------|--------|--------|-----------|
| T1 | x2444m | `x2444m_c.c` | 高 | 中等 |
| T2 | rgb_led_spi | `rgb_led_spi_c.c` | 高 | 简单 |
| T3 | Build 集成 | `CMakeLists.txt` | 高 | 简单 |

---

## T1: x2444m_c — Xicor X2444M/P 非易失性静态 RAM

### T1.1 创建文件

- [ ] 创建 `libsigrokdecode/c_decoders/x2444m_c.c`

### T1.2 定义 annotation enum

```c
enum {
    ANN_WRDS = 0,   // Write disable
    ANN_STO,        // Store RAM data in EEPROM
    ANN_SLEEP,      // Enter sleep mode
    ANN_WRITE,      // Write data into RAM
    ANN_WREN,       // Write enable
    ANN_RCL,        // Recall EEPROM data into RAM
    ANN_READ,       // Data read from RAM (addr 0x86)
    ANN_READ2,      // Data read from RAM (addr 0x87)
    NUM_ANN,
};
```

### T1.3 定义状态结构体

```c
typedef struct {
    int cs_asserted;
    int cmd_digit;
    uint8_t addr;
    uint64_t addr_start;
    uint64_t read_value;
    uint64_t write_value;
    int out_ann;
} x2444m_state;
```

### T1.4 定义寄存器查找表

```c
typedef struct {
    const char *name;
    int ann_idx;
    int has_value;
} x2444m_register;

static const x2444m_register x2444m_regs[8] = {
    {"WRDS",  ANN_WRDS,  0},  // 0x80
    {"STO",   ANN_STO,   0},  // 0x81
    {"SLEEP", ANN_SLEEP, 0},  // 0x82
    {"WRITE", ANN_WRITE, 1},  // 0x83
    {"WREN",  ANN_WREN,  0},  // 0x84
    {"RCL",   ANN_RCL,   0},  // 0x85
    {"READ",  ANN_READ,  1},  // 0x86
    {"READ",  ANN_READ2, 1},  // 0x87
};
```

### T1.5 定义静态元数据

- [ ] `x2444m_ann_labels[8][3]` — 第一列 `""`，第二列命令缩写，第三列描述
- [ ] `x2444m_row_cmds_classes[]` — 包含 ANN_WRDS, ANN_STO, ANN_SLEEP, ANN_WREN, ANN_RCL，以 -1 结尾
- [ ] `x2444m_row_data_classes[]` — 包含 ANN_WRITE, ANN_READ, ANN_READ2，以 -1 结尾
- [ ] `x2444m_ann_rows[]` — 2 行：`{"cmds", "Commands", ...}` 和 `{"data", "Data", ...}`
- [ ] `x2444m_inputs[] = {"spi", NULL}`
- [ ] `x2444m_tags[] = {"IC", "Memory", NULL}`

### T1.6 实现 SPI DATA 解析辅助函数

```c
static void x2444m_parse_spi_data(const unsigned char *data, uint64_t data_len,
                                   uint64_t *mosi_val, uint64_t *miso_val,
                                   int *have_mosi, int *have_miso)
```

### T1.7 实现命令处理函数

```c
static void x2444m_process_command(struct srd_decoder_inst *di,
    x2444m_state *s, uint64_t es)
```

逻辑：
1. `idx = s->addr & 0x07`，查 `x2444m_regs[idx]`
2. 若 `cmd_digit == 1`：简单命令，输出 `[reg->name, reg->name[0]]`
3. 若 `cmd_digit > 1`：
   - 若 `reg->name == "READ"` → `value = read_value`
   - 否则 → `value = write_value`
   - `addr_field = (s->addr >> 3) & 0x0f`
   - 输出 4 级文本：`"%s: 0x%x => 0x%x"`, `"%c: 0x%x => 0x%x"`, `"%c"`, `"@%04x"`

### T1.8 实现 recv_proto 回调

```c
static void x2444m_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
```

逻辑：
1. **CS-CHANGE**：
   - 解析 `data[1]`（new_cs），判断 CS asserted/deasserted
   - CS asserted → 重置 `cmd_digit=0`, `read_value=0`, `write_value=0`
   - CS deasserted → 调用 `x2444m_process_command()`
2. **DATA**：
   - 若 `!cs_asserted` 则忽略
   - 解析 MOSI/MISO 值
   - `cmd_digit == 0` → 存 `addr` 和 `addr_start`
   - `cmd_digit > 0` → 累加 `read_value` 和 `write_value`
   - `cmd_digit++`

### T1.9 实现生命周期函数

- [ ] `x2444m_reset()` — `g_malloc0` 分配状态，memset 清零，设置初始值
- [ ] `x2444m_start()` — 注册 `SRD_OUTPUT_ANN`，无 options
- [ ] `x2444m_decode()` — 空函数体 `(void)di;`
- [ ] `x2444m_destroy()` — `g_free` 释放状态

### T1.10 定义 srd_c_decoder 结构体

```c
struct srd_c_decoder x2444m_c_decoder = {
    .id = "x2444m_c",
    .name = "X2444M/P(C)",
    .longname = "Xicor X2444M/P (C)",
    .desc = "Xicor X2444M/P nonvolatile static RAM protocol. (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = x2444m_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = x2444m_ann_rows,
    .inputs = x2444m_inputs,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = x2444m_tags,
    .num_tags = 2,
    .reset = x2444m_reset,
    .start = x2444m_start,
    .decode = x2444m_decode,
    .destroy = x2444m_destroy,
    .recv_proto = x2444m_recv_proto,
};
```

### T1.11 实现导出函数

```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &x2444m_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
```

---

## T2: rgb_led_spi_c — RGB LED 灯串 SPI 协议

### T2.1 创建文件

- [ ] 创建 `libsigrokdecode/c_decoders/rgb_led_spi_c.c`

### T2.2 定义 annotation enum

```c
enum {
    ANN_RGB = 0,
    NUM_ANN,
};
```

### T2.3 定义状态结构体

```c
typedef struct {
    uint8_t mosi_bytes[3];
    int byte_count;
    uint64_t ss_cmd;
    int out_ann;
} rgb_led_spi_state;
```

### T2.4 定义静态元数据

- [ ] `rgb_led_spi_ann_labels[1][3] = {{"", "RGB", "RGB values"}}`
- [ ] `rgb_led_spi_row_rgb_classes[] = {ANN_RGB, -1}`
- [ ] `rgb_led_spi_ann_rows[] = {{"rgb", "RGB values", ..., 1}}`
- [ ] `rgb_led_spi_inputs[] = {"spi", NULL}`
- [ ] `rgb_led_spi_tags[] = {"Display", NULL}`

### T2.5 实现 recv_proto 回调

```c
static void rgb_led_spi_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
```

逻辑：
1. 只处理 `"DATA"` 命令，忽略其他
2. 解析 MOSI 值（`data[0]` = have_mosi, `data[1..8]` = mosi uint64 LE）
3. `byte_count == 0` → 记录 `ss_cmd = start_sample`
4. 存入 `mosi_bytes[byte_count]`，`byte_count++`
5. `byte_count != 3` → 返回等待
6. `byte_count == 3` → 计算 `rgb_value`，输出 `#%.6x`，重置 `byte_count = 0`

### T2.6 实现生命周期函数

- [ ] `rgb_led_spi_reset()` — `g_malloc0` 分配状态，memset 清零
- [ ] `rgb_led_spi_start()` — 注册 `SRD_OUTPUT_ANN`
- [ ] `rgb_led_spi_decode()` — 空函数体
- [ ] `rgb_led_spi_destroy()` — `g_free` 释放状态

### T2.7 定义 srd_c_decoder 结构体

```c
struct srd_c_decoder rgb_led_spi_c_decoder = {
    .id = "rgb_led_spi_c",
    .name = "RGB LED(SPI)(C)",
    .longname = "RGB LED string decoder (SPI) (C)",
    .desc = "RGB LED string protocol (RGB values clocked over SPI). (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = rgb_led_spi_ann_labels,
    .num_annotation_rows = 1,
    .annotation_rows = rgb_led_spi_ann_rows,
    .inputs = rgb_led_spi_inputs,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = rgb_led_spi_tags,
    .num_tags = 1,
    .reset = rgb_led_spi_reset,
    .start = rgb_led_spi_start,
    .decode = rgb_led_spi_decode,
    .destroy = rgb_led_spi_destroy,
    .recv_proto = rgb_led_spi_recv_proto,
};
```

### T2.8 实现导出函数

```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &rgb_led_spi_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
```

---

## T3: Build 集成

### T3.1 修改 CMakeLists.txt

在 `CMakeLists.txt` 第 837 行的 `C_DECODERS` 列表末尾添加 `x2444m_c` 和 `rgb_led_spi_c`：

```
set(C_DECODERS spi_c i2c_c uart_c ... wiegand_c ir_sirc_c x2444m_c rgb_led_spi_c)
```

### T3.2 编译验证

- [ ] 运行 `build_incremental.cmd`
- [ ] 确认 `build.dir/decoders/c_decoders/` 下生成 `x2444m_c.dll` 和 `rgb_led_spi_c.dll`
- [ ] 确认编译无 warning

### T3.3 运行时验证

- [ ] 启动 PXView，添加 SPI 解码器 + x2444m_c 解码器，验证解码输出
- [ ] 启动 PXView，添加 SPI 解码器 + rgb_led_spi_c 解码器，验证解码输出

---

## 任务依赖关系

```
T1.1 → T1.2 → T1.3 → T1.4 → T1.5 → T1.6 → T1.7 → T1.8 → T1.9 → T1.10 → T1.11
T2.1 → T2.2 → T2.3 → T2.4 → T2.5 → T2.6 → T2.7 → T2.8
T1 + T2 → T3
```

T1 和 T2 可并行执行，T3 依赖 T1 和 T2 完成。

---

## 风险点

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| SPI CS 极性假设 | CS-CHANGE 解析错误 | 参照 spi_c.c 的 CS 处理逻辑，active-low 时 cs=0 为 asserted |
| DATA 命令 data 编码理解偏差 | MOSI/MISO 值解析错误 | 严格按 spi_c.c 第 320-330 行的编码格式解析 |
| x2444m 的 `addr & 0x87` 掩码 | 命令识别错误 | Python 中 `addr & 0x87` 保留 bit7 和 bit2:0，C 中用 `addr & 0x07` 索引即可 |
| rgb_led_spi 不处理 CS-CHANGE | CS 断开后缓冲区未重置 | 可选增强：CS deasserted 时重置 byte_count |
