# Python → C 解码器移植规格书 — Batch 23

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层协议输出范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理、BITS v2格式输出、DATA 17字节格式 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |

## 1. 概述

本规格书涵盖 5 个 SPI 上层协议解码器从 Python 到 C 的移植。所有解码器均堆叠在 `spi` 解码器之上（`inputs=['spi']`），使用 `recv_proto()` 回调接收 SPI 协议数据，而非直接 `decode()` 原始信号。

### 移植目标

| # | Python id | C id | 芯片 | 复杂度 |
|---|-----------|------|------|--------|
| 1 | `a7105` | `a7105_c` | AMICCOM A7105 2.4GHz FSK/GFSK Transceiver | ★★★★ |
| 2 | `ad5626` | `ad5626_c` | Analog Devices AD5626 12-bit nanoDAC | ★★ |
| 3 | `ad79x0` | `ad79x0_c` | Analog Devices AD7910/AD7920 12-bit ADC | ★★★ |
| 4 | `ade77xx` | `ade77xx_c` | Analog Devices ADE77xx Poly Phase Energy Metering IC | ★★★★ |
| 5 | `adf435x` | `adf435x_c` | Analog Devices ADF4350/1 Wideband Synthesizer with VCO | ★★★★★ |

### 通用约束

- **C 标准**: C11 (`-std=c11`)
- **依赖**: `glib-2.0`, `libsigrokdecode.h`
- **文件命名**: `{decoder_id}_c.c`
- **输出目录**: `libsigrokdecode/c_decoders/`
- **CMake 注册**: 添加到 `CMakeLists.txt` 的 `C_DECODERS` 列表
- **API 版本**: `SRD_C_DECODER_API_VERSION = 3`

---

## 2. SPI 上层解码器架构

### 2.1 recv_proto() 回调机制

SPI 上层解码器**不实现** `decode()` 函数（保留空函数体），而是通过 `recv_proto()` 回调接收下层 SPI 解码器输出的协议数据。

```c
// recv_proto 签名
void (*recv_proto)(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

### 2.2 SPI 协议消息类型

SPI C 解码器 (`spi_c.c`) 输出以下协议消息：

| cmd | data 格式 | 说明 |
|-----|-----------|------|
| `"DATA"` | `[flags(1)][mosi_val(8 LE)][miso_val(8 LE)]` = 17字节 | 一个字节传输完成。flags: bit0=have_mosi, bit1=have_miso <!-- Updated: 确认DATA格式为17字节，mosi/miso各8字节LE uint64 --> |
| `"BITS"` | BITS v2 格式（见下文） | 位级数据，含 per-bit 时间戳 <!-- Updated: BITS v2 格式已实现，含 per-bit ss/es --> |
| `"CS-CHANGE"` | `[cs_old(1)][cs_new(1)]` | CS 片选信号变化。cs_old/cs_new: 0=asserted(low), 1=released(high)。首次 cs_old=0xFF |
| `"TRANSFER"` | 无 data | 一次完整传输结束 |

### 2.3 DATA 消息解析

```c
// DATA 消息解析示例
static void parse_spi_data(const unsigned char *data, uint64_t data_len,
    int *have_mosi, int *have_miso, uint64_t *mosi_val, uint64_t *miso_val)
{
    int pos = 0;
    uint8_t flags = data[pos++];
    *have_mosi = (flags & 1) ? 1 : 0;
    *have_miso = (flags & 2) ? 1 : 0;

    *mosi_val = 0;
    for (int i = 0; i < 8; i++)
        *mosi_val |= ((uint64_t)data[pos++]) << (8 * i);

    *miso_val = 0;
    for (int i = 0; i < 8; i++)
        *miso_val |= ((uint64_t)data[pos++]) << (8 * i);
}
```

### 2.4 BITS 消息解析

<!-- Updated: BITS v2 格式已实现，含 per-bit ss/es 时间戳。spi_c.c 和 i2c_c.c 已输出此格式 -->

**BITS v2 格式**（spi_c.c / i2c_c.c 实际输出格式）：
```
data[0] = have_mosi (bit0) | have_miso (bit1)
data[1] = mosi_bit_count (uint8_t)
data[2..2+count*17-1] = per bit: [value(1B)][ss(8B LE)][es(8B LE)]
data[2+count*17] = 0x00 (reserved/alignment)
data[2+count*17+1] = miso_bit_count (uint8_t)
data[2+count*17+2..] = per bit: [value(1B)][ss(8B LE)][es(8B LE)]
```

每个 bit 占 17 字节：1 字节值 + 8 字节起始采样 (LE uint64) + 8 字节结束采样 (LE uint64)。

```c
// BITS v2 消息解析示例
typedef struct {
    uint8_t value;
    uint64_t ss;
    uint64_t es;
} spi_bit_info;

static int parse_spi_bits_v2(const unsigned char *data, uint64_t data_len,
    int *have_mosi, int *have_miso,
    spi_bit_info *mosi_bits, int *mosi_count,
    spi_bit_info *miso_bits, int *miso_count,
    int max_bits)
{
    int pos = 0;
    uint8_t flags = data[pos++];
    *have_mosi = (flags & 1) ? 1 : 0;
    *have_miso = (flags & 2) ? 1 : 0;

    // MOSI bits
    *mosi_count = (int)data[pos++];
    int mc = (*mosi_count < max_bits) ? *mosi_count : max_bits;
    for (int i = 0; i < mc && pos + 17 <= (int)data_len; i++) {
        mosi_bits[i].value = data[pos++];
        mosi_bits[i].ss = 0;
        for (int b = 0; b < 8; b++)
            mosi_bits[i].ss |= ((uint64_t)data[pos++]) << (8 * b);
        mosi_bits[i].es = 0;
        for (int b = 0; b < 8; b++)
            mosi_bits[i].es |= ((uint64_t)data[pos++]) << (8 * b);
    }
    // 跳过剩余 MOSI bits
    pos += (*mosi_count - mc) * 17;

    // reserved + MISO count
    if (pos < (int)data_len && data[pos] == 0x00) pos++;
    *miso_count = (pos < (int)data_len) ? (int)data[pos++] : 0;

    // MISO bits
    int mic = (*miso_count < max_bits) ? *miso_count : max_bits;
    for (int i = 0; i < mic && pos + 17 <= (int)data_len; i++) {
        miso_bits[i].value = data[pos++];
        miso_bits[i].ss = 0;
        for (int b = 0; b < 8; b++)
            miso_bits[i].ss |= ((uint64_t)data[pos++]) << (8 * b);
        miso_bits[i].es = 0;
        for (int b = 0; b < 8; b++)
            miso_bits[i].es |= ((uint64_t)data[pos++]) << (8 * b);
    }

    return pos;
}
```

### 2.5 CS-CHANGE 消息解析

```c
// CS-CHANGE 消息解析
// data[0] = cs_old (0xFF 表示首次, 否则 0=asserted, 1=released)
// data[1] = cs_new (0=asserted, 1=released)
uint8_t cs_old = data[0];
uint8_t cs_new = data[1];
// CS 上升沿 (cs_old=0, cs_new=1) = 传输完成
// CS 下降沿 (cs_old=1, cs_new=0) = 传输开始
```

### 2.6 C 解码器结构体模板

```c
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "Xxx(C)",
    .longname = "Full Name (C)",
    .desc = "Description. (C implementation)",
    .license = "gplv2+",
    .channels = NULL,           // SPI 上层解码器无直接通道
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = xxx_options,
    .num_options = N,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = N,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,       // {"spi", NULL}
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = xxx_tags,
    .num_tags = N,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,       // 空函数体
    .destroy = xxx_destroy,
    .recv_proto = xxx_recv_proto,  // 核心回调
};
```

---

## 3. 各解码器详细规格

---

### 3.1 A7105 — AMICCOM A7105 2.4GHz FSK/GFSK Transceiver

#### 3.1.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `a7105` |
| name | `A7105` |
| longname | `AMICCOM A7105` |
| desc | `2.4GHz FSK/GFSK Transceiver with 2K ~ 500Kbps data rate.` |
| license | `gplv2+` |
| inputs | `['spi']` |
| outputs | `[]` |
| tags | `['IC', 'Wireless/RF']` |
| options | `()` (无选项) |
| channels | 无 |
| optional_channels | 无 |

#### 3.1.2 Annotations

| # | id | label |
|---|----|-------|
| 0 | `cmd` | Commands sent to the device |
| 1 | `tx-data` | Payload sent to the device |
| 2 | `rx-data` | Payload read from the device |
| 3 | `warning` | Warnings |

#### 3.1.3 Annotation Rows

| row id | label | annotation classes |
|--------|-------|--------------------|
| `commands` | Commands | (0, 1, 2) |
| `warnings` | Warnings | (3,) |

#### 3.1.4 C 元数据映射

```c
.id = "a7105_c",
.name = "A7105(C)",
.longname = "AMICCOM A7105 (C)",
.desc = "2.4GHz FSK/GFSK Transceiver with 2K ~ 500Kbps data rate. (C implementation)",
.license = "gplv2+",
```

#### 3.1.5 寄存器映射

A7105 有 52 个 8-bit 寄存器 (0x00-0x33)：

```c
static const struct {
    const char *name;
    int size;
} a7105_regs[] = {
    [0x00] = {"MODE", 1},
    [0x01] = {"MODE_CTRL", 1},
    [0x02] = {"CALC", 1},
    [0x03] = {"FIFO_I", 1},
    [0x04] = {"FIFO_II", 1},
    [0x05] = {"FIFO_DATA", 1},
    [0x06] = {"ID_DATA", 1},
    [0x07] = {"RC_OSC_I", 1},
    // ... 0x08-0x32 省略，完整列表见源码
    [0x33] = {"UNKNOWN", 1},
};
```

#### 3.1.6 解码逻辑分析

**命令解析** (`parse_command`):

| 命令字节 | 命令名 | 附加数据 | min bytes | max bytes |
|----------|--------|----------|-----------|-----------|
| `0x05` | `W_TX_FIFO` | None | 1 | 32 |
| `0x45` | `R_RX_FIFO` | None | 1 | 32 |
| `0x06` | `W_ID` | None | 1 | 4 |
| `0x46` | `R_ID` | None | 1 | 4 |
| `0x00-0x3F` (bit7=0, bit6=0) | `W_REGISTER` | reg_addr = b & 0x3F | 1 | 1 |
| `0x40-0x7F` (bit7=0, bit6=1) | `R_REGISTER` | reg_addr = b & 0x3F | 1 | 1 |
| `0x80` | `SLEEP_MODE` | None | 0 | 0 |
| `0x90` | `IDLE_MODE` | None | 0 | 0 |
| `0xA0` | `STANDBY_MODE` | None | 0 | 0 |
| `0xB0` | `PLL_MODE` | None | 0 | 0 |
| `0xC0` | `RX_MODE` | None | 0 | 0 |
| `0xD0` | `TX_MODE` | None | 0 | 0 |
| `0xE0` | `FIFO_WRITE_PTR_RESET` | None | 0 | 0 |
| `0xF0` | `FIFO_READ_PTR_RESET` | None | 0 | 0 |

**状态机**:

```
CS 下降沿 → 开始收集字节
  第一个字节 → 命令字节 (decode_command)
  后续字节 → 数据字节 (收集到 mb 列表)
CS 上升沿 → 完成命令 (finish_command)
TRANSFER → 同 CS 上升沿处理
```

**finish_command 逻辑**:
- `R_REGISTER`: 从 MISO 字节解码寄存器值
- `W_REGISTER`: 从 MOSI 字节解码寄存器值
- `R_RX_FIFO`: MISO 字节作为 RX FIFO 数据
- `W_TX_FIFO`: MOSI 字节作为 TX FIFO 数据
- `R_ID`: MISO 字节作为 ID 数据
- `W_ID`: MOSI 字节作为 ID 数据

**输出格式**: `Cmd {COMMAND}: {REG_NAME} = "{$}"` 或 `@{HEX_DATA}`

#### 3.1.7 C 实现关键代码

```c
enum {
    ANN_CMD = 0,
    ANN_TX_DATA,
    ANN_RX_DATA,
    ANN_WARN,
    NUM_ANN,
};

enum a7105_state_val {
    A7105_IDLE,
    A7105_CMD_RECEIVED,
};

typedef struct {
    enum a7105_state_val state;
    int first;           // 是否为命令字节
    int cs_was_released; // CS 是否已释放
    int requirements_met;

    // 当前命令
    char cmd_name[32];   // 命令名
    int cmd_reg;         // W/R_REGISTER 的寄存器地址
    int cmd_min;         // 最小数据字节数
    int cmd_max;         // 最大数据字节数

    // 多字节数据收集
    uint8_t mosi_bytes[32];
    uint8_t miso_bytes[32];
    int mb_count;
    uint64_t mb_ss;      // 数据起始 sample
    uint64_t mb_es;      // 数据结束 sample

    uint64_t ss, es;
    int out_ann;
} a7105_state;

static void a7105_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    a7105_state *s = (a7105_state *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    if (strcmp(cmd, "TRANSFER") == 0) {
        if (s->state == A7105_CMD_RECEIVED && s->mb_count >= s->cmd_min) {
            a7105_finish_command(di, s);
        }
        a7105_reset_cmd(s);
        s->cs_was_released = 1;
    } else if (strcmp(cmd, "CS-CHANGE") == 0) {
        uint8_t cs_old = (data_len > 0) ? data[0] : 0xFF;
        uint8_t cs_new = (data_len > 1) ? data[1] : 0;

        if (cs_old == 0 && cs_new == 1) {
            // CS 上升沿 - 传输完成
            if (s->state == A7105_CMD_RECEIVED && s->mb_count >= s->cmd_min) {
                a7105_finish_command(di, s);
            }
            a7105_reset_cmd(s);
            s->cs_was_released = 1;
        }
    } else if (strcmp(cmd, "DATA") == 0 && s->cs_was_released) {
        int have_mosi, have_miso;
        uint64_t mosi_val, miso_val;
        parse_spi_data(data, data_len, &have_mosi, &have_miso, &mosi_val, &miso_val);

        if (s->first) {
            s->first = 0;
            a7105_decode_command(s, (uint8_t)mosi_val);
        } else {
            if (s->mb_count < s->cmd_max) {
                if (s->mb_count == 0) s->mb_ss = start_sample;
                s->mb_es = end_sample;
                s->mosi_bytes[s->mb_count] = (uint8_t)mosi_val;
                s->miso_bytes[s->mb_count] = (uint8_t)miso_val;
                s->mb_count++;
            }
        }
    }
}
```

---

### 3.2 AD5626 — Analog Devices AD5626 12-bit nanoDAC

#### 3.2.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `ad5626` |
| name | `AD5626` |
| longname | `Analog Devices AD5626` |
| desc | `Analog Devices AD5626 12-bit nanoDAC.` |
| license | `gplv2+` |
| inputs | `['spi']` |
| outputs | `[]` |
| tags | `['IC', 'Analog/digital']` |
| options | 无 |
| channels | 无 |
| optional_channels | 无 |

#### 3.2.2 Annotations

| # | id | label |
|---|----|-------|
| 0 | `voltage` | Voltage |

#### 3.2.3 Annotation Rows

只有一行，包含 annotation class 0。

#### 3.2.4 C 元数据映射

```c
.id = "ad5626_c",
.name = "AD5626(C)",
.longname = "Analog Devices AD5626 (C)",
.desc = "Analog Devices AD5626 12-bit nanoDAC. (C implementation)",
.license = "gplv2+",
```

#### 3.2.5 解码逻辑分析

**极简解码器** — 只处理 BITS 和 CS-CHANGE：

1. CS 下降沿 (1→0): 记录起始 sample
2. BITS: 从 MOSI bit 流中收集数据，MSB first
   - 每个 bit: `data = data | bit[0]; data <<= 1;`
3. CS 上升沿 (0→1): 完成一次 DAC 写入
   - `data >>= 1` (修正最后一次多余左移)
   - `data /= 1000` (转换为电压值)
   - 输出 `%.3fV`

**关键**: AD5626 使用 **BITS** 级别数据，不是 DATA 字节级别。C 版本需要处理 BITS 消息。BITS v2 格式已提供 per-bit 时间戳，但 AD5626 只需 bit 值来计算数据，时间戳非必需 <!-- Updated: BITS v2 已实现，AD5626 只需 bit 值 -->

#### 3.2.6 C 实现关键代码

```c
enum {
    ANN_VOLTAGE = 0,
    NUM_ANN,
};

typedef struct {
    uint32_t data;        // 收集的位数据
    uint64_t ss;          // 传输起始 sample
    int out_ann;
} ad5626_state;

static void ad5626_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ad5626_state *s = (ad5626_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "CS-CHANGE") == 0) {
        uint8_t cs_old = (data_len > 0) ? data[0] : 0xFF;
        uint8_t cs_new = (data_len > 1) ? data[1] : 0;

        if (cs_old == 0 && cs_new == 1) {
            // CS 上升沿 - 完成
            s->data >>= 1;
            double voltage = (double)s->data / 1000.0;
            char buf[32];
            snprintf(buf, sizeof(buf), "%.3fV", voltage);
            C_ANN_PUT(di, s->ss, end_sample, s->out_ann, ANN_VOLTAGE, buf);
            s->data = 0;
        } else if (cs_old == 1 && cs_new == 0) {
            // CS 下降沿 - 开始
            s->ss = start_sample;
        }
    } else if (strcmp(cmd, "BITS") == 0) {
        // BITS v2 格式: [flags(1)][mosi_count(1)][per-bit: value(1)+ss(8)+es(8)]...[0x00][miso_count(1)]...
        // AD5626 只需 MOSI bit 值
        int pos = 0;
        uint8_t flags = data[pos++];
        int have_mosi = flags & 1;
        if (have_mosi) {
            int mosi_count = (int)data[pos++];
            for (int i = 0; i < mosi_count && pos + 17 <= (int)data_len; i++) {
                uint8_t bit_val = data[pos];
                pos += 17; // 跳过 value(1) + ss(8) + es(8)
                s->data = s->data | bit_val;
                s->data <<= 1;
            }
        }
    }
}
```

---

### 3.3 AD79x0 — Analog Devices AD7910/AD7920 12-bit ADC

#### 3.3.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `ad79x0` |
| name | `AD79x0` |
| longname | `Analog Devices AD79x0` |
| desc | `Analog Devices AD7910/AD7920 12-bit ADC.` |
| license | `gplv2+` |
| inputs | `['spi']` |
| outputs | `[]` |
| tags | `['IC', 'Analog/digital']` |
| options | `{'id': 'vref', 'desc': 'Reference voltage (V)', 'default': 1.5}` |
| channels | 无 |
| optional_channels | 无 |

#### 3.3.2 Annotations

| # | id | label |
|---|----|-------|
| 0 | `mode` | Mode |
| 1 | `voltage` | Voltage |
| 2 | `validation` | Validation |

#### 3.3.3 Annotation Rows

| row id | label | annotation classes |
|--------|-------|--------------------|
| `modes` | Modes | (0,) |
| `voltages` | Voltages | (1,) |
| `data_validation` | Data validation | (2,) |

#### 3.3.4 C 元数据映射

```c
.id = "ad79x0_c",
.name = "AD79x0(C)",
.longname = "Analog Devices AD79x0 (C)",
.desc = "Analog Devices AD7910/AD7920 12-bit ADC. (C implementation)",
.license = "gplv2+",
```

#### 3.3.5 解码逻辑分析

**模式判断**:
- `nb_bits >= 10`: 正常模式 (Normal Mode)
  - `data == 0xFFF`: Power Up Mode (无效数据)
  - `nb_bits == 16`: Complete conversion
  - `nb_bits < 16`: Incomplete conversion
  - 电压计算: `vin = (data / (2^12 - 1)) * vref`
- `nb_bits < 10`: Power Down Mode (无效数据)

**BITS 处理**: 从 MISO bit 流收集数据，MSB first
- `samples_bit` = 第一个 bit 的宽度 (用于计算 nb_bits)
- `nb_bits = (cs_rise_ss - cs_fall_ss) / samples_bit`

**CS-CHANGE 处理**:
- CS 下降沿: 记录起始 sample
- CS 上升沿: 完成转换，计算模式/电压/验证

#### 3.3.6 C 实现关键代码

```c
enum {
    ANN_MODE = 0,
    ANN_VOLTAGE,
    ANN_VALIDATION,
    NUM_ANN,
};

typedef struct {
    int samplerate;
    int samples_bit;     // 每个 bit 的 sample 数
    uint64_t ss;         // 当前 bit 起始
    uint64_t start_sample; // CS 下降沿 sample
    int previous_state;  // 上一次状态
    uint32_t data;       // 收集的位数据
    int out_ann;
    double vref;         // 参考电压选项
} ad79x0_state;

// modes 映射
static const char *modes_normal[] = {"Normal Mode", "Normal", "Norm", "N"};
static const char *modes_powerdown[] = {"Power Down Mode", "Power Down", "PD"};
static const char *modes_powerup[] = {"Power Up Mode", "Power Up", "PU"};

// validation 映射
static const char *val_invalid[] = {"Invalid data", "Invalid", "N/A"};
static const char *val_incomplete[] = {"Incomplete conversion", "Incomplete", "I"};
static const char *val_complete[] = {"Complete conversion", "Complete", "C"};

static void ad79x0_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ad79x0_state *s = (ad79x0_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "CS-CHANGE") == 0) {
        uint8_t cs_old = (data_len > 0) ? data[0] : 0xFF;
        uint8_t cs_new = (data_len > 1) ? data[1] : 0;

        if (cs_old == 0 && cs_new == 1) {
            // CS 上升沿 - 完成转换
            if (s->samples_bit == -1) return;
            s->data >>= 1;
            int nb_bits = (int)((start_sample - s->ss) / s->samples_bit);

            if (nb_bits >= 10) {
                if (s->data == 0xFFF) {
                    C_ANN_PUT(di, s->start_sample, end_sample, s->out_ann, ANN_MODE, "Power Up Mode");
                    C_ANN_PUT(di, s->start_sample, end_sample, s->out_ann, ANN_VALIDATION, "Invalid data");
                    s->previous_state = 0;
                } else {
                    C_ANN_PUT(di, s->start_sample, end_sample, s->out_ann, ANN_MODE, "Normal Mode");
                    if (nb_bits == 16)
                        C_ANN_PUT(di, s->start_sample, end_sample, s->out_ann, ANN_VALIDATION, "Complete conversion");
                    else
                        C_ANN_PUT(di, s->start_sample, end_sample, s->out_ann, ANN_VALIDATION, "Incomplete conversion");
                    double vin = ((double)s->data / 4095.0) * s->vref;
                    char buf[64];
                    snprintf(buf, sizeof(buf), "%.6fV", vin);
                    C_ANN_PUT(di, s->start_sample, end_sample, s->out_ann, ANN_VOLTAGE, buf);
                    snprintf(buf, sizeof(buf), "%.2fV", vin);
                    // 两种格式输出
                }
            } else {
                C_ANN_PUT(di, s->start_sample, end_sample, s->out_ann, ANN_MODE, "Power Down Mode");
                C_ANN_PUT(di, s->start_sample, end_sample, s->out_ann, ANN_VALIDATION, "Invalid data");
                s->previous_state = 1;
            }
            s->ss = (uint64_t)-1;
            s->samples_bit = -1;
            s->data = 0;
        } else if (cs_old == 1 && cs_new == 0) {
            s->start_sample = start_sample;
            s->samples_bit = -1;
        }
    } else if (strcmp(cmd, "BITS") == 0) {
        // BITS v2 格式解析 MISO bits
        int pos = 0;
        uint8_t flags = data[pos++];
        int have_mosi = flags & 1;
        int have_miso = (flags >> 1) & 1;

        // 跳过 MOSI bits
        if (have_mosi) {
            int mosi_count = (int)data[pos++];
            pos += mosi_count * 17; // 跳过所有 MOSI bits (每个17字节)
        }

        // 解析 MISO bits
        if (have_miso) {
            if (pos < (int)data_len && data[pos] == 0x00) pos++; // reserved
            int miso_count = (pos < (int)data_len) ? (int)data[pos++] : 0;

            for (int i = 0; i < miso_count && pos + 17 <= (int)data_len; i++) {
                uint8_t bit_val = data[pos++];
                uint64_t bit_ss = 0, bit_es = 0;
                for (int b = 0; b < 8; b++)
                    bit_ss |= ((uint64_t)data[pos++]) << (8 * b);
                for (int b = 0; b < 8; b++)
                    bit_es |= ((uint64_t)data[pos++]) << (8 * b);

                if (s->samples_bit == -1 && i == 0) {
                    // 从第一个 bit 的 ss/es 计算 samples_per_bit
                    s->samples_bit = (int)(bit_es - bit_ss + 1);
                }
                s->data = s->data | bit_val;
                s->data <<= 1;
            }
        }
    }
}
```

**注意**: AD79x0 Python 版本使用 `data[2]` (MISO bits) 中的 `(ss, es)` 信息来计算 `samples_bit`。C 版本的 BITS v2 消息**已包含** per-bit 时间戳，因此可以直接从 BITS v2 数据中获取 `samples_bit`，无需使用 metadata 回调或 DATA 消息替代。 <!-- Updated: BITS v2 已包含 per-bit ss/es，AD79x0 可直接从 BITS v2 获取 samples_bit -->

---

### 3.4 ADE77xx — Analog Devices ADE77xx Poly Phase Energy Metering IC

#### 3.4.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `ade77xx` |
| name | `ADE77xx` |
| longname | `Analog Devices ADE77xx` |
| desc | `Poly phase multifunction energy metering IC protocol.` |
| license | `mit` |
| inputs | `['spi']` |
| outputs | `[]` |
| tags | `['Analog/digital', 'IC', 'Sensor']` |
| options | 无 |
| channels | 无 |
| optional_channels | 无 |

#### 3.4.2 Annotations

| # | id | label |
|---|----|-------|
| 0 | `read` | Register read commands |
| 1 | `write` | Register write commands |
| 2 | `warning` | Warnings |

#### 3.4.3 Annotation Rows

| row id | label | annotation classes |
|--------|-------|--------------------|
| `read` | Read | (0,) |
| `write` | Write | (1,) |
| `warnings` | Warnings | (2,) |

#### 3.4.4 C 元数据映射

```c
.id = "ade77xx_c",
.name = "ADE77xx(C)",
.longname = "Analog Devices ADE77xx (C)",
.desc = "Poly phase multifunction energy metering IC protocol. (C implementation)",
.license = "mit",
```

#### 3.4.5 寄存器映射 (lists.py)

ADE77xx 有 57 个寄存器，每个寄存器格式: `(name, desc, R/RW, bit_width, S/U, default)`

关键寄存器（完整列表见 `lists.py`）:

| addr | name | access | bits | type | default |
|------|------|--------|------|------|---------|
| 0x01 | AWATTHR | R | 16 | S | 0x0 |
| 0x0A | AIRMS | R | 24 | S | 0x0 |
| 0x13 | OPMODE | R/W | 8 | U | 0x4 |
| 0x18 | Mask | R/W | 24 | U | 0x0 |
| 0x7E | CHKSUM | R | 8 | U | None |
| 0x7F | Version | R | 8 | U | None |

C 版本需要将 `lists.py` 中的 `regs` OrderedDict 转换为 C 数组。

#### 3.4.6 解码逻辑分析

**命令格式**:
- 第一个字节: `cmd = mosi_bytes[0]`
  - `write = cmd & 0x80` (bit7: 1=write, 0=read)
  - `reg = cmd & 0x7f` (bit0-6: 寄存器地址)
- 后续字节: 数据 (1-3 字节，取决于寄存器位宽)
  - `expected = ceil(reg_bits / 8)`

**CS-CHANGE 处理**:
- CS 上升沿: 如果数据不完整，输出 "SHORT" 警告
- CS 下降沿: 重置数据

**DATA 处理**:
- 第一个字节: 命令字节
- 后续字节: 数据字节
- 收集到 `expected` 个数据字节后，解析并输出

**输出格式**: `{REG_NAME}: {$}` 或 `@{HEX_VALUE}`

#### 3.4.7 C 实现关键代码

```c
enum {
    ANN_READ = 0,
    ANN_WRITE,
    ANN_WARN,
    NUM_ANN,
};

// 寄存器定义
typedef struct {
    const char *name;
    const char *desc;
    const char *access;  // "R" or "R/W"
    int bits;            // 位宽
    int is_signed;       // 0=unsigned, 1=signed
    int has_default;     // 是否有默认值
    int default_val;
} ade77xx_reg_info;

// 寄存器表 (从 lists.py 转换)
static const ade77xx_reg_info ade77xx_regs[0x80] = {
    [0x01] = {"AWATTHR", "Watt-Hour Accumulation Register for Phase A", "R", 16, 1, 1, 0x0},
    [0x02] = {"BWATTHR", "Watt-Hour Accumulation Register for Phase B", "R", 16, 1, 1, 0x0},
    // ... 完整列表
    [0x7F] = {"Version", "Version of the Die", "R", 8, 0, 0, 0},
};

typedef struct {
    uint8_t mosi_bytes[4];
    uint8_t miso_bytes[4];
    int byte_count;
    int expected;        // 预期数据字节数
    uint64_t ss_cmd, es_cmd;
    int out_ann;
} ade77xx_state;

static void ade77xx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ade77xx_state *s = (ade77xx_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "CS-CHANGE") == 0) {
        uint8_t cs_old = (data_len > 0) ? data[0] : 0xFF;
        uint8_t cs_new = (data_len > 1) ? data[1] : 0;

        if (cs_old == 0 && cs_new == 1) {
            // CS 上升沿 - 检查是否短传输
            if (s->byte_count > 1 && s->byte_count - 1 < s->expected) {
                uint8_t cmd_byte = s->mosi_bytes[0];
                int write = cmd_byte & 0x80;
                int reg = cmd_byte & 0x7f;
                const ade77xx_reg_info *ri = &ade77xx_regs[reg];
                int idx = write ? ANN_WRITE : ANN_READ;
                char buf[128];
                snprintf(buf, sizeof(buf), "%s: SHORT", ri->name);
                C_ANN_PUT(di, s->ss_cmd, end_sample, s->out_ann, idx, buf);
                C_ANN_PUT(di, s->ss_cmd, end_sample, s->out_ann, ANN_WARN, "Short transfer!");
            }
            // 重置
            s->byte_count = 0;
            s->expected = 0;
        }
        return;
    }

    if (strcmp(cmd, "DATA") != 0) return;

    int have_mosi, have_miso;
    uint64_t mosi_val, miso_val;
    parse_spi_data(data, data_len, &have_mosi, &have_miso, &mosi_val, &miso_val);

    if (s->byte_count == 0) s->ss_cmd = start_sample;

    s->mosi_bytes[s->byte_count] = (uint8_t)mosi_val;
    s->miso_bytes[s->byte_count] = (uint8_t)miso_val;
    s->byte_count++;

    if (s->byte_count < 2) return;

    uint8_t cmd_byte = s->mosi_bytes[0];
    int write = cmd_byte & 0x80;
    int reg = cmd_byte & 0x7f;

    if (reg >= 0x80 || ade77xx_regs[reg].name == NULL) {
        C_ANN_PUT(di, s->ss_cmd, end_sample, s->out_ann, ANN_WARN, "Unknown register!");
        s->byte_count = 0;
        return;
    }

    const ade77xx_reg_info *ri = &ade77xx_regs[reg];
    s->expected = (ri->bits + 7) / 8;  // ceil(bits/8)

    if (s->byte_count - 1 != s->expected) return;

    s->es_cmd = end_sample;

    // 组装值
    uint32_t valo = 0, vali = 0;
    if (s->expected == 3) {
        valo = s->mosi_bytes[1] << 16 | s->mosi_bytes[2] << 8 | s->mosi_bytes[3];
        vali = s->miso_bytes[1] << 16 | s->miso_bytes[2] << 8 | s->miso_bytes[3];
    } else if (s->expected == 2) {
        valo = s->mosi_bytes[1] << 8 | s->mosi_bytes[2];
        vali = s->miso_bytes[1] << 8 | s->miso_bytes[2];
    } else {
        valo = s->mosi_bytes[1];
        vali = s->miso_bytes[1];
    }

    int idx = write ? ANN_WRITE : ANN_READ;
    char buf[128];
    snprintf(buf, sizeof(buf), "%s: {$}", ri->name);
    C_ANN_PUT(di, s->ss_cmd, s->es_cmd, s->out_ann, idx, buf);

    char hex_buf[16];
    snprintf(hex_buf, sizeof(hex_buf), "@%02X", valo);
    C_ANN_PUT(di, s->ss_cmd, s->es_cmd, s->out_ann, idx, hex_buf);

    s->byte_count = 0;
}
```

---

### 3.5 ADF435x — Analog Devices ADF4350/1 Wideband Synthesizer

#### 3.5.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `adf435x` |
| name | `ADF435x` |
| longname | `Analog Devices ADF4350/1` |
| desc | `Wideband synthesizer with integrated VCO.` |
| license | `gplv3+` |
| inputs | `['spi']` |
| outputs | `[]` |
| tags | `['Clock/timing', 'IC', 'Wireless/RF']` |
| options | 无 |
| channels | 无 |
| optional_channels | 无 |

#### 3.5.2 Annotations

| # | id | label |
|---|----|-------|
| 0 | `write` | Register write |
| 1 | `warning` | Warnings |

#### 3.5.3 Annotation Rows

| row id | label | annotation classes |
|--------|-------|--------------------|
| `writes` | Register writes | (0,) |
| `warnings` | Warnings | (1,) |

#### 3.5.4 C 元数据映射

```c
.id = "adf435x_c",
.name = "ADF435x(C)",
.longname = "Analog Devices ADF4350/1 (C)",
.desc = "Wideband synthesizer with integrated VCO. (C implementation)",
.license = "gplv3+",
```

#### 3.5.5 寄存器映射

ADF435x 使用 32-bit 寄存器，低 3 位为寄存器地址：

| reg | 名称 | 关键字段 |
|-----|------|----------|
| 0 | FRAC/INT | FRAC[14:3], INT[30:15] |
| 1 | MOD/Phase | MOD[14:3], Phase[26:15], Prescalar[27], Phase Adjust[28] |
| 2 | Control | Counter Reset[3], CP Three-State[4], Power-Down[5], PD Polarity[6], LDP[7], LDF[8], CP Current[12:9], Double Buffer[13], R Counter[23:14], RDIV2[24], Ref Doubler[25], MUXOUT[28:26], Low Noise/Spur[30:29] |
| 3 | Divider | Clock Divider[14:3], Clock Div Mode[16:15], CSR Enable[18], Charge Cancel[21], ABP[22], Band Select Clk Mode[23] |
| 4 | RF Output | Output Power[4:3], Output Enable[5], AUX Output Power[7:6], AUX Output Select[8], AUX Output Enable[9], MTLD[10], VCO Power-Down[11], Band Select Clk Div[19:12], RF Divider[22:20], Feedback Select[23] |
| 5 | LD Pin | LD Pin Mode[23:22] |

#### 3.5.6 解码逻辑分析

**最复杂的解码器** — 需要处理 32-bit 字和大量字段解析。

1. **BITS 收集**: 在 `BITS` 消息中收集 MOSI bit 流，MSB first
2. **TRANSFER 处理**: 传输完成后解析 32-bit 字
3. **32-bit 字解析**:
   - 检查 bit 数量是否为 32
   - 反转 bit 顺序 (MSB→LSB)
   - 提取低 3 位作为寄存器地址
   - 根据寄存器地址解析各字段
4. **字段解析**: 每个字段有 offset, width, name, 可选 parser 和 checker

**Python 中的 `bitpack_lsb`**: 将 bit 列表打包为整数值。C 版本需要自行实现。

#### 3.5.7 C 实现关键代码

```c
enum {
    ANN_REG = 0,
    ANN_WARN,
    NUM_ANN,
};

// 字段描述
typedef struct {
    int offset;     // bit 起始位置 (LSB 顺序)
    int width;      // bit 宽度
    const char *name;
    const char *(*parser)(int v);  // 可选解析函数
    const char *(*checker)(int v); // 可选检查函数
} adf435x_field_desc;

// Charge Pump Current 查找表
static const double cp_currents[] = {
    0.31, 0.63, 0.94, 1.25, 1.56, 1.88, 2.19, 2.50,
    2.81, 3.13, 3.44, 3.75, 4.06, 4.38, 4.69, 5.00,
};

// 各寄存器字段定义
static const adf435x_field_desc reg0_fields[] = {
    {3, 12, "FRAC", NULL, NULL},
    {15, 16, "INT", NULL, adf435x_check_int},
    {-1, 0, NULL, NULL, NULL}  // 终止标记
};

static const adf435x_field_desc reg1_fields[] = {
    {3, 12, "MOD", NULL, NULL},
    {15, 12, "Phase", NULL, NULL},
    {27, 1, "Prescalar", adf435x_parse_prescalar, NULL},
    {28, 1, "Phase Adjust", adf435x_parse_phase_adjust, NULL},
    {-1, 0, NULL, NULL, NULL}
};

// ... reg2-reg5 类似

static const adf435x_field_desc *reg_fields[] = {
    reg0_fields, reg1_fields, reg2_fields,
    reg3_fields, reg4_fields, reg5_fields,
};

typedef struct {
    // 32-bit 字的 bit 收集
    uint8_t bits[32];    // MSB 顺序
    int bit_count;
    // bit 时间戳 (用于 annotation 定位)
    uint64_t bit_ss[32];
    uint64_t bit_es[32];
    int out_ann;
} adf435x_state;

// 从收集的 bits 中提取字段值
static uint32_t adf435x_extract_field(uint32_t word, int offset, int width)
{
    uint32_t mask = (1U << width) - 1;
    return (word >> offset) & mask;
}

static void adf435x_decode_word(struct srd_decoder_inst *di, adf435x_state *s)
{
    if (s->bit_count != 32) {
        char buf[64];
        snprintf(buf, sizeof(buf), "Frame error: Bit count: want 32, got %d", s->bit_count);
        C_ANN_PUT(di, s->bit_ss[0], s->bit_es[s->bit_count-1], s->out_ann, ANN_WARN, buf);
        return;
    }

    // 将 MSB 顺序的 bits 打包为 32-bit 字
    uint32_t word = 0;
    for (int i = 0; i < 32; i++) {
        word = (word << 1) | s->bits[i];
    }

    // 提取寄存器地址 (低 3 位)
    int reg_addr = word & 0x7;
    char buf[32];
    snprintf(buf, sizeof(buf), "Register: %d", reg_addr);
    C_ANN_PUT(di, s->bit_ss[31], s->bit_es[31], s->out_ann, ANN_REG, buf);

    // 解析字段
    if (reg_addr < 0 || reg_addr > 5) return;
    const adf435x_field_desc *fields = reg_fields[reg_addr];
    if (!fields) return;

    for (int i = 0; fields[i].name != NULL; i++) {
        uint32_t val = adf435x_extract_field(word, fields[i].offset, fields[i].width);
        const char *formatted = NULL;
        char auto_buf[32];

        if (fields[i].parser) {
            formatted = fields[i].parser(val);
        } else {
            snprintf(auto_buf, sizeof(auto_buf), "%u", val);
            formatted = auto_buf;
        }

        if (formatted) {
            char text[128];
            snprintf(text, sizeof(text), "%s: %s", fields[i].name, formatted);
            // 计算 field 的 ss/es (从 bit 时间戳)
            int start_bit = fields[i].offset;
            int end_bit = fields[i].offset + fields[i].width - 1;
            // 注意: bit 时间戳是 MSB 顺序，需要映射
            C_ANN_PUT(di, s->bit_ss[31-end_bit], s->bit_es[31-start_bit],
                      s->out_ann, ANN_REG, text);
        }

        if (fields[i].checker) {
            const char *warn = fields[i].checker(val);
            if (warn) {
                C_ANN_PUT(di, s->bit_ss[31-fields[i].offset],
                          s->bit_es[31-fields[i].offset - fields[i].width + 1],
                          s->out_ann, ANN_WARN, warn);
            }
        }
    }
}

static void adf435x_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    adf435x_state *s = (adf435x_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "TRANSFER") == 0) {
        adf435x_decode_word(di, s);
        s->bit_count = 0;
    } else if (strcmp(cmd, "BITS") == 0) {
        // BITS v2 格式收集 MOSI bits (含 per-bit 时间戳)
        int pos = 0;
        uint8_t flags = data[pos++];
        int have_mosi = flags & 1;
        if (have_mosi) {
            int mosi_count = (int)data[pos++];
            for (int i = 0; i < mosi_count && s->bit_count < 32 && pos + 17 <= (int)data_len; i++) {
                s->bits[s->bit_count] = data[pos++];
                s->bit_ss[s->bit_count] = 0;
                for (int b = 0; b < 8; b++)
                    s->bit_ss[s->bit_count] |= ((uint64_t)data[pos++]) << (8 * b);
                s->bit_es[s->bit_count] = 0;
                for (int b = 0; b < 8; b++)
                    s->bit_es[s->bit_count] |= ((uint64_t)data[pos++]) << (8 * b);
                s->bit_count++;
            }
        }
    }
}
```

---

## 4. BITS 消息处理的关键问题

### 4.1 Python BITS 格式 vs C BITS v2 格式

<!-- Updated: BITS v2 格式已实现，以下内容已过时，保留供历史参考 -->

**Python**: `BITS` 消息的 `databyte` 是一个列表: `[(val, ss, es), ...]`，每个 bit 包含值和时间戳。

**C BITS v2**（已实现）: `BITS` 消息的 `data` 格式为:
```
data[0] = have_mosi (bit0) | have_miso (bit1)
data[1] = mosi_bit_count (uint8_t)
data[2..2+count*17-1] = per bit: [value(1B)][ss(8B LE)][es(8B LE)]
data[2+count*17] = 0x00 (reserved/alignment)
data[2+count*17+1] = miso_bit_count (uint8_t)
data[2+count*17+2..] = per bit: [value(1B)][ss(8B LE)][es(8B LE)]
```
每个 bit 包含值和时间戳，**与 Python 版本功能等价**。

### 4.2 影响（已更新）

- **AD5626**: 只需 bit 值来计算数据，BITS v2 时间戳非必需但可用
- **AD79x0**: 需要 `samples_bit` 来计算 `nb_bits`。BITS v2 已包含 per-bit 时间戳，可直接从第一个 bit 的 ss/es 计算 `samples_bit`，无需 metadata 回调或 DATA 消息替代 <!-- Updated: BITS v2 已解决此问题 -->
- **ADF435x**: 需要 bit 时间戳来精确定位 annotation。BITS v2 已提供 per-bit 时间戳，可精确计算每个字段的 ss/es，与 Python 版本功能等价 <!-- Updated: BITS v2 已解决此问题 -->

### 4.3 wordsize 推算（已不需要）

<!-- Updated: BITS v2 格式已明确包含 mosi_bit_count 和 miso_bit_count，无需推算 -->

BITS v2 格式已明确包含 `mosi_bit_count` 和 `miso_bit_count` 字段，无需从 `data_len` 推算 wordsize。

旧版推算方法（已过时，仅供历史参考）:
```
data_len = 1 + wordsize + (have_miso ? 1 + wordsize : 0)
如果 have_mosi=1, have_miso=1: wordsize = (data_len - 2) / 2
如果 have_mosi=1, have_miso=0: wordsize = data_len - 1
```

---

## 5. 通用辅助函数

### 5.1 SPI DATA 解析

```c
static void parse_spi_data(const unsigned char *data, uint64_t data_len,
    int *have_mosi, int *have_miso, uint64_t *mosi_val, uint64_t *miso_val)
{
    int pos = 0;
    uint8_t flags = data[pos++];
    *have_mosi = (flags & 1) ? 1 : 0;
    *have_miso = (flags & 2) ? 1 : 0;

    *mosi_val = 0;
    if (*have_mosi) {
        for (int i = 0; i < 8 && pos < (int)data_len; i++)
            *mosi_val |= ((uint64_t)data[pos++]) << (8 * i);
    }

    *miso_val = 0;
    if (*have_miso) {
        for (int i = 0; i < 8 && pos < (int)data_len; i++)
            *miso_val |= ((uint64_t)data[pos++]) << (8 * i);
    }
}
```

### 5.2 SPI BITS v2 解析

<!-- Updated: BITS v2 格式已实现，替换旧的推算式解析 -->

```c
// 解析 MOSI/MISO bits，返回 bit 信息（含时间戳）
typedef struct {
    uint8_t value;
    uint64_t ss;
    uint64_t es;
} spi_bit_info;

static int parse_spi_bits_v2(const unsigned char *data, uint64_t data_len,
    spi_bit_info *mosi_bits, int *mosi_count,
    spi_bit_info *miso_bits, int *miso_count,
    int max_bits)
{
    int pos = 0;
    uint8_t flags = data[pos++];
    int have_mosi = (flags & 1) ? 1 : 0;
    int have_miso = (flags >> 1) & 1;

    *mosi_count = 0;
    *miso_count = 0;

    // MOSI bits
    if (have_mosi) {
        *mosi_count = (int)data[pos++];
        int mc = (*mosi_count < max_bits) ? *mosi_count : max_bits;
        for (int i = 0; i < mc && pos + 17 <= (int)data_len; i++) {
            mosi_bits[i].value = data[pos++];
            mosi_bits[i].ss = 0;
            for (int b = 0; b < 8; b++)
                mosi_bits[i].ss |= ((uint64_t)data[pos++]) << (8 * b);
            mosi_bits[i].es = 0;
            for (int b = 0; b < 8; b++)
                mosi_bits[i].es |= ((uint64_t)data[pos++]) << (8 * b);
        }
        pos += (*mosi_count - mc) * 17; // 跳过未读的 bits
    }

    // reserved + MISO count
    if (pos < (int)data_len && data[pos] == 0x00) pos++;
    if (have_miso) {
        *miso_count = (pos < (int)data_len) ? (int)data[pos++] : 0;
        int mic = (*miso_count < max_bits) ? *miso_count : max_bits;
        for (int i = 0; i < mic && pos + 17 <= (int)data_len; i++) {
            miso_bits[i].value = data[pos++];
            miso_bits[i].ss = 0;
            for (int b = 0; b < 8; b++)
                miso_bits[i].ss |= ((uint64_t)data[pos++]) << (8 * b);
            miso_bits[i].es = 0;
            for (int b = 0; b < 8; b++)
                miso_bits[i].es |= ((uint64_t)data[pos++]) << (8 * b);
        }
    }

    return pos;
}
```

---

## 6. CMakeLists.txt 修改

在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加:

```cmake
a7105_c
ad5626_c
ad79x0_c
ade77xx_c
adf435x_c
```

---

## 7. 文件清单

| 文件路径 | 说明 |
|----------|------|
| `libsigrokdecode/c_decoders/a7105_c.c` | A7105 C 解码器 |
| `libsigrokdecode/c_decoders/ad5626_c.c` | AD5626 C 解码器 |
| `libsigrokdecode/c_decoders/ad79x0_c.c` | AD79x0 C 解码器 |
| `libsigrokdecode/c_decoders/ade77xx_c.c` | ADE77xx C 解码器 |
| `libsigrokdecode/c_decoders/adf435x_c.c` | ADF435x C 解码器 |
| `CMakeLists.txt` | 添加 5 个解码器到 C_DECODERS 列表 |

---

## 8. 风险与注意事项

1. **BITS v2 时间戳**: C 版本 BITS v2 消息已包含 bit 级时间戳，ADF435x 的字段级 annotation 可精确实现，AD79x0 的 samples_bit 可直接计算 <!-- Updated: BITS v2 已解决时间戳缺失问题 -->
2. **AD79x0 的 samples_bit**: BITS v2 已包含 per-bit 时间戳，可直接从第一个 bit 的 ss/es 计算，无需 metadata 回调 <!-- Updated: BITS v2 已解决此问题 -->
3. **ADF435x 复杂度**: 6 个寄存器、大量字段解析函数，是本批次最复杂的解码器。建议最后实现。
4. **ADE77xx 寄存器表**: 57 个寄存器定义需要完整转换，工作量较大。
5. **A7105 多字节命令**: W_TX_FIFO/R_RX_FIFO 最多 32 字节，需要动态缓冲区。
6. **输出格式兼容性**: Python 版本使用 `{$}` 和 `@` 格式标记，C 版本需要保持一致。
7. **SPI DATA 格式**: 确认为17字节 `[flags(1)][mosi_val(8 LE)][miso_val(8 LE)]`，mosi/miso 为 LE uint64 <!-- Updated: 确认DATA格式为17字节 -->
