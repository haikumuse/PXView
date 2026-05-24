# SPI 上层协议解码器移植规格书 — Batch 28

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层协议输出范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |
| ds3231_c.c | 上层recv_proto范本 | I2C上层解码器、多寄存器块读写、STOP/START REPEAT处理 |
| c_decoder_utils.h | BITS v2格式文档 | BITS消息格式的权威定义和解析示例代码 |

## 概述

本批次将 2 个 Python SPI 上层协议解码器移植为 C 实现。所有解码器均为 SPI 协议的上层解码器（`inputs=['spi']`），通过 `recv_proto()` 回调接收 SPI 底层解码器输出的 python 协议数据，而非直接对 logic 信号进行 `decode()`。

### 移植目标

| # | Python id | C 文件名 | C id | 描述 |
|---|-----------|----------|------|------|
| 1 | `x2444m` | `x2444m_c.c` | `x2444m_c` | Xicor X2444M/P 非易失性静态 RAM 协议 |
| 2 | `rgb_led_spi` | `rgb_led_spi_c.c` | `rgb_led_spi_c` | RGB LED 灯串 SPI 协议 |

---

## SPI 上层解码器核心架构

### recv_proto 机制

SPI 上层解码器**不实现** `decode()` 函数（保留空函数体），而是通过 `recv_proto` 回调接收底层 SPI C 解码器（`spi_c`）发送的 python 协议数据。

**SPI C 解码器输出的 python 命令：**

| 命令 (cmd) | data 内容 | 说明 |
|------------|-----------|------|
| `"DATA"` | `[flags(1B)][mosi_bytes(8B)][miso_bytes(8B)]` | 每个 SPI word 完成时发送 |
| `"CS-CHANGE"` | `[old_cs(1B)] [new_cs(1B)]` 或空 | CS 片选信号变化 |
| `"BITS"` | BITS v2格式（含per-bit时间戳），见下方详细布局 | 每个 word 的位级数据 |
| `"TRANSFER"` | 空 | 一次 CS 有效的完整传输结束 |

**关键：SPI DATA 命令的 data 编码：**
```c
// data[0]: flags (bit0=have_mosi, bit1=have_miso)
// data[1..8]: mosi 值 (uint64_t 小端序)
// data[9..16]: miso 值 (uint64_t 小端序)
// 总计: 17字节
```

**CS-CHANGE 命令的 data 编码：**
```c
// data[0]: 前一个 CS 值 (对二进制信号等于 1-new_cs)，首次为 0xFF
// data[1]: 当前 CS 值
// CS asserted 判断：cs_polarity=active-low 时 cs=0 为 asserted
```
<!-- Updated: 修正data[0]描述。原文"旧CS值的反码(1-cs)"有误导——实际存储的是1-new_cs，
     对二进制信号恰好等于prev_cs，并非"旧值的反码"。与spi_c.c第472行cs_data[0]=(1-cs)一致 -->

**BITS 命令的 data 编码（BITS v2格式，含per-bit时间戳）：**
```c
// data[0]                           = have_mosi (bit0) | have_miso (bit1)
// data[1]                           = mosi_bit_count (uint8_t)
// data[2 .. 2+mosi_count*17-1]      = MOSI bits, 每个17字节:
//     [value(1B)][start_sample(8B LE)][end_sample(8B LE)]
// data[2+mosi_count*17]             = 0x00 (reserved/alignment)
// data[2+mosi_count*17+1]           = miso_bit_count (uint8_t)
// data[2+mosi_count*17+2 ..]        = MISO bits, 每个17字节:
//     [value(1B)][start_sample(8B LE)][end_sample(8B LE)]
```
<!-- Updated: BITS格式已从旧版(仅bit值列表)更新为v2(含per-bit ss/es时间戳)，
     与spi_c.c和i2c_c.c实际输出一致，详见c_decoder_utils.h -->

### recv_proto 函数签名

```c
static void xxx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

### 上层解码器结构体模板

```c
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    .longname = "...",
    .desc = "... (C implementation)",
    .license = "gplv2+",
    .channels = NULL,           // 上层解码器无物理通道
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = xxx_options,
    .num_options = N,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = M,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,       // {"spi", NULL}
    .num_inputs = 1,
    .outputs = NULL,            // 上层解码器通常无输出
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = xxx_tags,
    .num_tags = 1,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,       // 空函数体
    .destroy = xxx_destroy,
    .recv_proto = xxx_recv_proto,  // 核心回调
};
```

---

## 解码器 1: x2444m_c — Xicor X2444M/P 非易失性静态 RAM

### Python 元数据提取

| 字段 | Python 值 |
|------|-----------|
| id | `x2444m` |
| name | `X2444M/P` |
| longname | `Xicor X2444M/P` |
| desc | `Xicor X2444M/P nonvolatile static RAM protocol.` |
| license | `gplv2+` |
| inputs | `['spi']` |
| outputs | `[]` |
| tags | `['IC', 'Memory']` |
| channels | 无 |
| optional_channels | 无 |
| options | 无 |
| binary | 无 |

### Python annotations 映射

Python 有 8 个 annotation class（注意最后两个都是 `read`）：

| idx | Python tuple | C enum | C ann_label[0] | C ann_label[1] | C ann_label[2] |
|-----|-------------|--------|----------------|----------------|----------------|
| 0 | `('wrds', 'Write disable')` | `ANN_WRDS` | `""` | `"WRDS"` | `"Write disable"` |
| 1 | `('sto', 'Store RAM data in EEPROM')` | `ANN_STO` | `""` | `"STO"` | `"Store RAM data in EEPROM"` |
| 2 | `('sleep', 'Enter sleep mode')` | `ANN_SLEEP` | `""` | `"SLEEP"` | `"Enter sleep mode"` |
| 3 | `('write', 'Write data into RAM')` | `ANN_WRITE` | `""` | `"WRITE"` | `"Write data into RAM"` |
| 4 | `('wren', 'Write enable')` | `ANN_WREN` | `""` | `"WREN"` | `"Write enable"` |
| 5 | `('rcl', 'Recall EEPROM data into RAM')` | `ANN_RCL` | `""` | `"RCL"` | `"Recall EEPROM data into RAM"` |
| 6 | `('read', 'Data read from RAM')` | `ANN_READ` | `""` | `"READ"` | `"Data read from RAM"` |
| 7 | `('read', 'Data read from RAM')` | `ANN_READ2` | `""` | `"READ"` | `"Data read from RAM"` |

**注意：** Python 中 idx 6 和 7 的 id 都是 `read`，但功能不同（idx 6 对应地址 0x86，idx 7 对应地址 0x87）。C 实现中需要区分，使用 `ANN_READ` 和 `ANN_READ2`。

### Annotation Rows 设计

```c
static const int x2444m_row_cmds_classes[] = {ANN_WRDS, ANN_STO, ANN_SLEEP, ANN_WREN, ANN_RCL, -1};
static const int x2444m_row_data_classes[] = {ANN_WRITE, ANN_READ, ANN_READ2, -1};

static const struct srd_c_ann_row x2444m_ann_rows[] = {
    {"cmds", "Commands", x2444m_row_cmds_classes, 5},
    {"data", "Data", x2444m_row_data_classes, 3},
};
```

### Python 解码逻辑分析

**寄存器映射表：**

| 地址码 | 命令名 | ann idx | 值解码器 |
|--------|--------|---------|----------|
| 0x80 | WRDS (Write Disable) | 0 | 无值 |
| 0x81 | STO (Store) | 1 | 无值 |
| 0x82 | SLEEP | 2 | 无值 |
| 0x83 | WRITE | 3 | `0x%x` 格式 |
| 0x84 | WREN (Write Enable) | 4 | 无值 |
| 0x85 | RCL (Recall) | 5 | 无值 |
| 0x86 | READ | 6 | `0x%x` 格式 |
| 0x87 | READ | 7 | `0x%x` 格式 |

**状态机逻辑：**

1. **CS asserted 时**：重置状态，`cmd_digit=0`，`read_value=0`，`write_value=0`
2. **收到 DATA**：
   - `cmd_digit == 0`：第一个字节为命令/地址字节，存入 `self.addr`，记录 `addr_start`
   - `cmd_digit > 0`：后续字节累加到 `read_value`（从 MISO）和 `write_value`（从 MOSI）
   - `cmd_digit++`
3. **CS deasserted 时**（CS 从 asserted 变为 deasserted）：
   - `cmd_digit == 1`：仅一个字节，简单命令。取 `addr & 0x87` 查表得到命令名和 idx，输出 `putcmd()`
   - `cmd_digit > 1`：读写命令。取 `addr & 0x87` 查表，根据命令名选择 `read_value` 或 `write_value`，输出 `putreadwrite()`
     - 地址字段：`(addr >> 3) & 0x0f`（4 位地址）
     - 值字段：累加的多字节值

**putcmd 输出格式：**
```python
[idx, [reg, reg[0]]]
# 例如：["WRDS", "W"]
```

**putreadwrite 输出格式：**
```python
[idx, ['%s: %s => 0x%x' % (reg, addr, value),
       '%s: %s => 0x%x' % (reg[0], addr, value),
       reg[0],
       '@%04x' % value]]
# 例如：["READ: 0x3 => 0xab", "R: 0x3 => 0xab", "R", "@00ab"]
```

### C 实现状态机设计

```c
enum x2444m_state {
    X2444M_IDLE,
    X2444M_ACTIVE,  // CS asserted, 等待数据
};

typedef struct {
    enum x2444m_state state;
    int cs_asserted;
    int cmd_digit;
    uint8_t addr;           // 命令/地址字节
    uint64_t addr_start;    // 命令字节的起始 sample
    uint64_t read_value;    // MISO 累加值
    uint64_t write_value;   // MOSI 累加值
    int out_ann;
} x2444m_state;
```

### recv_proto 实现逻辑

```c
static void x2444m_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    x2444m_state *s = (x2444m_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "CS-CHANGE") == 0) {
        // data[0] = prev_cs (对二进制信号等于1-new_cs，初始为0xFF), data[1] = new_cs
        // 注意: 无CS引脚时data=NULL/data_len=0，此时应默认cs_asserted=1
        if (data_len < 2) {
            // 无CS引脚: CS始终为asserted
            s->cs_asserted = 1;
            return;
        }
        int new_cs = data[1];
        // SPI 默认 cs_polarity=active-low, cs=0 表示 asserted
        int now_asserted = (new_cs == 0);

        if (now_asserted && !s->cs_asserted) {
            // CS asserted: 重置状态
            s->cs_asserted = 1;
            s->cmd_digit = 0;
            s->read_value = 0;
            s->write_value = 0;
        } else if (!now_asserted && s->cs_asserted) {
            // CS deasserted: 处理命令
            s->cs_asserted = 0;
            x2444m_process_command(di, s, end_sample);
        }
    }
    else if (strcmp(cmd, "DATA") == 0) {
        if (!s->cs_asserted) return;
        // 解析 MOSI/MISO 值
        // data[0]=flags(bit0=have_mosi, bit1=have_miso), data[1..8]=mosi(LE uint64), data[9..16]=miso(LE uint64)
        if (data_len < 17) return;

        uint64_t mosi_val = 0, miso_val = 0;
        int have_mosi = data[0] & 1;
        int have_miso = (data[0] >> 1) & 1;

        if (have_mosi) {
            for (int i = 0; i < 8; i++)
                mosi_val |= ((uint64_t)data[1 + i]) << (8 * i);
        }
        if (have_miso) {
            for (int i = 0; i < 8; i++)
                miso_val |= ((uint64_t)data[9 + i]) << (8 * i);
        }

        if (s->cmd_digit == 0) {
            s->addr = (uint8_t)mosi_val;
            s->addr_start = start_sample;
        } else {
            s->read_value = (s->read_value << 8) | (uint8_t)miso_val;
            s->write_value = (s->write_value << 8) | (uint8_t)mosi_val;
        }
        s->cmd_digit++;
    }
}
```

### 寄存器查找表 C 实现

```c
typedef struct {
    const char *name;
    int ann_idx;
    int has_value;  // 0=无值(简单命令), 1=有值(读写命令)
} x2444m_register;

static const x2444m_register x2444m_regs[8] = {
    {"WRDS",  ANN_WRDS,  0},  // 0x80 & 0x87 = 0x80
    {"STO",   ANN_STO,   0},  // 0x81 & 0x87 = 0x81
    {"SLEEP", ANN_SLEEP, 0},  // 0x82 & 0x87 = 0x82
    {"WRITE", ANN_WRITE, 1},  // 0x83 & 0x87 = 0x83
    {"WREN",  ANN_WREN,  0},  // 0x84 & 0x87 = 0x84
    {"RCL",   ANN_RCL,   0},  // 0x85 & 0x87 = 0x85
    {"READ",  ANN_READ,  1},  // 0x86 & 0x87 = 0x86
    {"READ",  ANN_READ2, 1},  // 0x87 & 0x87 = 0x87
};

// 查找: idx = addr & 0x07 (因为 0x80~0x87 的低3位正好是0~7)
// 但更安全的做法: idx = (addr & 0x87) - 0x80
// 或直接: idx = addr & 0x07
```

### 命令处理函数

```c
static void x2444m_process_command(struct srd_decoder_inst *di,
    x2444m_state *s, uint64_t es)
{
    int idx = s->addr & 0x07;
    const x2444m_register *reg = &x2444m_regs[idx];

    if (s->cmd_digit == 1) {
        // 简单命令（仅地址字节）
        C_ANN_PUT(di, s->addr_start, es, s->out_ann, reg->ann_idx,
                  reg->name, reg->name);
    } else if (s->cmd_digit > 1) {
        // 读写命令
        uint64_t value;
        if (strcmp(reg->name, "READ") == 0)
            value = s->read_value;
        else
            value = s->write_value;

        int addr_field = (s->addr >> 3) & 0x0f;
        char long_str[128], short_str[128], tiny_str[16], val_str[16];

        snprintf(long_str, sizeof(long_str), "%s: 0x%x => 0x%x", reg->name, addr_field, (unsigned)value);
        snprintf(short_str, sizeof(short_str), "%c: 0x%x => 0x%x", reg->name[0], addr_field, (unsigned)value);
        snprintf(tiny_str, sizeof(tiny_str), "%c", reg->name[0]);
        snprintf(val_str, sizeof(val_str), "@%04x", (unsigned)value);

        C_ANN_PUT(di, s->addr_start, es, s->out_ann, reg->ann_idx,
                  long_str, short_str, tiny_str, val_str);
    }
}
```

### 关键实现注意事项

1. **CS 极性**：SPI 默认 `cs_polarity=active-low`，CS=0 时 asserted。但上层解码器不应硬编码此假设，应通过 CS-CHANGE 的 data 解析。当 `data[1]==0` 时 CS asserted（active-low），当 `data[1]==1` 时 CS deasserted。无CS引脚时 `data_len==0`，应默认 `cs_asserted=1`。
<!-- Updated: 补充无CS引脚时的处理方式，与spi_c.c中无CS引脚时发送data=NULL/data_len=0的行为一致 -->
2. **MOSI/MISO 解析**：DATA 命令的 data 是小端序 uint64_t 编码，需要逐字节拼装。
3. **Python 中 `self.addr & 0x87`**：掩码 0x87 保留了 bit7（0x80）和 bit2:0，用于区分 8 个命令。在 C 中用 `(addr & 0x07)` 即可索引寄存器表。
4. **Python 中 `decoder((self.addr >> 3) & 0x0f)`**：对于 WRDS/STO/SLEEP/WREN/RCL，decoder 函数返回空字符串；对于 WRITE/READ，返回 `'0x%x' % v`。C 实现中直接格式化输出即可。
5. **Python 的 `{$}` 占位符**：Python 代码中 `'%s: %s => {$}'` 使用了 sigrok 的特殊模板语法，`{$}` 会被替换为数值。C 实现中直接将数值格式化到字符串中。
6. **空 decode 函数**：上层解码器的 `decode()` 必须存在但函数体为空 `(void)di;`。

---

## 解码器 2: rgb_led_spi_c — RGB LED 灯串 SPI 协议

### Python 元数据提取

| 字段 | Python 值 |
|------|-----------|
| id | `rgb_led_spi` |
| name | `RGB LED (SPI)` |
| longname | `RGB LED string decoder (SPI)` |
| desc | `RGB LED string protocol (RGB values clocked over SPI).` |
| license | `gplv2+` |
| inputs | `['spi']` |
| outputs | `[]` |
| tags | `['Display']` |
| channels | 无 |
| optional_channels | 无 |
| options | 无 |
| binary | 无 |

### Python annotations 映射

| idx | Python tuple | C enum | C ann_label[0] | C ann_label[1] | C ann_label[2] |
|-----|-------------|--------|----------------|----------------|----------------|
| 0 | `('rgb', 'RGB values')` | `ANN_RGB` | `""` | `"RGB"` | `"RGB values"` |

### Annotation Rows 设计

```c
static const int rgb_led_spi_row_rgb_classes[] = {ANN_RGB, -1};

static const struct srd_c_ann_row rgb_led_spi_ann_rows[] = {
    {"rgb", "RGB values", rgb_led_spi_row_rgb_classes, 1},
};
```

### Python 解码逻辑分析

**极简逻辑：**

1. 只处理 `DATA` 类型的包，忽略 `CS-CHANGE` 等
2. 收集连续 3 个 MOSI 字节作为 R、G、B 值
3. 第一个字节到达时记录 `ss_cmd`
4. 第三个字节到达时：
   - 计算 `rgb_value = (red << 16) | (green << 8) | blue`
   - 输出 annotation：`[0, ['#%.6x' % rgb_value]]`
   - 重置 `mosi_bytes` 列表

**Python 代码关键行：**
```python
if len(self.mosi_bytes) == 0:
    self.ss_cmd = ss
self.mosi_bytes.append(mosi)

if len(self.mosi_bytes) != 3:
    return

red, green, blue = self.mosi_bytes
rgb_value = int(red) << 16 | int(green) << 8 | int(blue)

self.es_cmd = es
self.putx([0, ['#%.6x' % rgb_value]])
self.mosi_bytes = []
```

### C 实现状态设计

```c
typedef struct {
    uint8_t mosi_bytes[3];  // 缓冲 3 个 MOSI 字节
    int byte_count;         // 已收集的字节数
    uint64_t ss_cmd;        // 第一个字节的起始 sample
    int out_ann;
} rgb_led_spi_state;
```

### recv_proto 实现逻辑

```c
static void rgb_led_spi_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    rgb_led_spi_state *s = (rgb_led_spi_state *)c_decoder_get_private(di);
    if (!s) return;

    // 只处理 DATA 命令
    if (strcmp(cmd, "DATA") != 0)
        return;

    if (data_len < 17) return;

    int have_mosi = data[0] & 1;
    if (!have_mosi) return;  // RGB LED 只关心 MOSI

    uint64_t mosi_val = 0;
    for (int i = 0; i < 8; i++)
        mosi_val |= ((uint64_t)data[1 + i]) << (8 * i);

    uint8_t byte_val = (uint8_t)(mosi_val & 0xFF);

    if (s->byte_count == 0)
        s->ss_cmd = start_sample;

    s->mosi_bytes[s->byte_count] = byte_val;
    s->byte_count++;

    if (s->byte_count != 3)
        return;

    // 3 字节收集完毕
    uint8_t red = s->mosi_bytes[0];
    uint8_t green = s->mosi_bytes[1];
    uint8_t blue = s->mosi_bytes[2];
    uint32_t rgb_value = ((uint32_t)red << 16) | ((uint32_t)green << 8) | (uint32_t)blue;

    char rgb_str[16];
    snprintf(rgb_str, sizeof(rgb_str), "#%.6x", rgb_value);

    C_ANN_PUT(di, s->ss_cmd, end_sample, s->out_ann, ANN_RGB, rgb_str);

    s->byte_count = 0;
}
```

### 关键实现注意事项

1. **仅使用 MOSI**：RGB LED 数据通过 MOSI 发送，不关心 MISO。
2. **每 3 字节一组**：严格按照 R、G、B 顺序收集 3 个字节。
3. **`#%.6x` 格式**：输出为 `#` 加 6 位十六进制数，如 `#ff0000`（红色）。
4. **ss_cmd 记录**：第一个字节的 `start_sample` 作为整组 RGB 的起始位置。
5. **CS-CHANGE 不影响**：Python 原始实现不处理 CS-CHANGE，C 实现同样忽略。但如果 CS 断开时应重置缓冲区，这是一个增强点（Python 原版没有此逻辑，C 版本可以添加以增强鲁棒性）。

---

## 通用 C 解码器编码规范

### 文件结构

```c
// 1. 头文件
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

// 2. Annotation enum
enum { ANN_XXX = 0, ..., NUM_ANN };

// 3. 状态 struct
typedef struct { ... } xxx_state;

// 4. 静态数据：channels, options, ann_labels, ann_rows, inputs, tags

// 5. 辅助函数

// 6. recv_proto 回调

// 7. reset/start/decode/destroy 生命周期函数

// 8. srd_c_decoder 结构体

// 9. srd_c_decoder_entry() 和 srd_c_decoder_api_version()
```

### ann_labels 规则

- 第一列必须为 `""`（空字符串），API 自动处理 i+7 偏移
- 每行 3 个字符串：`{internal_id, short_label, long_description}`

### Option 初始化

在 `srd_c_decoder_entry()` 中初始化默认值和可选值列表：
- 字符串选项：`g_variant_new_string("default_value")`
- 整数选项：`g_variant_new_int64(default_value)`
- 浮点选项：`g_variant_new_double(default_value)`

### C_ANN_PUT 用法

```c
// 单文本
C_ANN_PUT(di, ss, es, out_ann, ANN_CLASS, "text");

// 多文本（long, short, tiny, ...）
C_ANN_PUT(di, ss, es, out_ann, ANN_CLASS, "long text", "short", "tiny");
```

### Build 集成

在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加新解码器名称：
```
set(C_DECODERS ... x2444m_c rgb_led_spi_c)
```

### 输出注册

上层解码器在 `start()` 中注册输出：
```c
s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "decoder_id");
```

**注意：** 上层解码器通常不需要注册 `SRD_OUTPUT_PYTHON` 或 `SRD_OUTPUT_BINARY`，除非它还要被更上层解码器使用。

---

## SPI DATA 命令解析工具函数（建议复用）

由于所有 SPI 上层解码器都需要解析 DATA 命令的 data 字段，建议在每个解码器内部实现一个辅助函数：

```c
static void spi_parse_data(const unsigned char *data, uint64_t data_len,
                           uint64_t *mosi_val, uint64_t *miso_val,
                           int *have_mosi, int *have_miso)
{
    *mosi_val = 0;
    *miso_val = 0;
    *have_mosi = 0;
    *have_miso = 0;

    if (data_len < 17) return;
    *have_mosi = data[0] & 1;
    *have_miso = (data[0] >> 1) & 1;
    if (*have_mosi) {
        for (int i = 0; i < 8; i++)
            *mosi_val |= ((uint64_t)data[1 + i]) << (8 * i);
    }
    if (*have_miso) {
        for (int i = 0; i < 8; i++)
            *miso_val |= ((uint64_t)data[9 + i]) << (8 * i);
    }
}
```

---

## 完整 C 代码骨架

### x2444m_c.c 骨架

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_WRDS = 0,
    ANN_STO,
    ANN_SLEEP,
    ANN_WRITE,
    ANN_WREN,
    ANN_RCL,
    ANN_READ,
    ANN_READ2,
    NUM_ANN,
};

typedef struct {
    int cs_asserted;
    int cmd_digit;
    uint8_t addr;
    uint64_t addr_start;
    uint64_t read_value;
    uint64_t write_value;
    int out_ann;
} x2444m_state;

typedef struct {
    const char *name;
    int ann_idx;
    int has_value;
} x2444m_register;

static const x2444m_register x2444m_regs[8] = {
    {"WRDS",  ANN_WRDS,  0},
    {"STO",   ANN_STO,   0},
    {"SLEEP", ANN_SLEEP, 0},
    {"WRITE", ANN_WRITE, 1},
    {"WREN",  ANN_WREN,  0},
    {"RCL",   ANN_RCL,   0},
    {"READ",  ANN_READ,  1},
    {"READ",  ANN_READ2, 1},
};

static const char *x2444m_ann_labels[][3] = {
    {"", "WRDS",  "Write disable"},
    {"", "STO",   "Store RAM data in EEPROM"},
    {"", "SLEEP", "Enter sleep mode"},
    {"", "WRITE", "Write data into RAM"},
    {"", "WREN",  "Write enable"},
    {"", "RCL",   "Recall EEPROM data into RAM"},
    {"", "READ",  "Data read from RAM"},
    {"", "READ",  "Data read from RAM"},
};

static const int x2444m_row_cmds_classes[] = {ANN_WRDS, ANN_STO, ANN_SLEEP, ANN_WREN, ANN_RCL, -1};
static const int x2444m_row_data_classes[] = {ANN_WRITE, ANN_READ, ANN_READ2, -1};

static const struct srd_c_ann_row x2444m_ann_rows[] = {
    {"cmds", "Commands", x2444m_row_cmds_classes, 5},
    {"data", "Data", x2444m_row_data_classes, 3},
};

static const char *x2444m_inputs[] = {"spi", NULL};
static const char *x2444m_tags[] = {"IC", "Memory", NULL};

// ... (辅助函数、recv_proto、reset/start/decode/destroy、结构体、entry)
```

### rgb_led_spi_c.c 骨架

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum {
    ANN_RGB = 0,
    NUM_ANN,
};

typedef struct {
    uint8_t mosi_bytes[3];
    int byte_count;
    uint64_t ss_cmd;
    int out_ann;
} rgb_led_spi_state;

static const char *rgb_led_spi_ann_labels[][3] = {
    {"", "RGB", "RGB values"},
};

static const int rgb_led_spi_row_rgb_classes[] = {ANN_RGB, -1};

static const struct srd_c_ann_row rgb_led_spi_ann_rows[] = {
    {"rgb", "RGB values", rgb_led_spi_row_rgb_classes, 1},
};

static const char *rgb_led_spi_inputs[] = {"spi", NULL};
static const char *rgb_led_spi_tags[] = {"Display", NULL};

// ... (recv_proto、reset/start/decode/destroy、结构体、entry)
```

---

## 参考文件索引

| 文件 | 用途 |
|------|------|
| `libsigrokdecode/c_decoders/spi_c.c` | SPI 底层解码器，理解 python 输出格式 |
| `libsigrokdecode/c_decoders/lm75_c.c` | I2C 上层 recv_proto 范本 |
| `libsigrokdecode/c_decoders/ds1307_c.c` | I2C 上层 recv_proto 范本（更复杂状态机） |
| `libsigrokdecode/c_decoders/ds3231_c.c` | I2C 上层 recv_proto 范本（多寄存器块读写） |
| `libsigrokdecode/c_decoders/i2c_c.c` | I2C 底层解码器，理解 python 输出格式 |
| `libsigrokdecode/c_decoders/c_decoder_utils.h` | BITS v2 格式权威定义和解析示例 |
| `libsigrokdecode/c_decoder_api.c` | c_decoder_put_python → recv_proto 调用链 |
| `libsigrokdecode/libsigrokdecode.h` | srd_c_decoder 结构体定义 |
| `libsigrokdecode/decoders/x2444m/pd.py` | Python 原始实现 |
| `libsigrokdecode/decoders/rgb_led_spi/pd.py` | Python 原始实现 |
