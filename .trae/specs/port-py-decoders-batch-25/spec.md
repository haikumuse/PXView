# SPI 上层 Python 解码器移植为 C 解码器 — 详细规格

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层协议输出范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |
| c_decoder_utils.h | BITS v2格式文档 | per-bit时间戳的BITS消息格式定义和解析示例 | <!-- Updated: 添加BITS v2格式参考 -->

## 1. 项目概述

将 5 个基于 SPI 的 Python 上层协议解码器移植为 C 解码器。这些解码器 `inputs=['spi']`，不直接解析原始信号，而是通过 `recv_proto()` 回调接收 SPI 底层解码器输出的结构化数据。

### 移植目标列表

| # | Python id | C 文件名 | C id | 芯片名称 | 复杂度 |
|---|-----------|----------|------|----------|--------|
| 1 | enc28j60 | enc28j60_c.c | enc28j60_c | Microchip ENC28J60 以太网控制器 | 高 |
| 2 | ltc242x | ltc242x_c.c | ltc242x_c | LTC2421/LTC2422 20-bit ADC | 低 |
| 3 | max6954 | max6954_c.c | max6954_c | Maxim MAX6954 LED 显示驱动 | 中 |
| 4 | max7219 | max7219_c.c | max7219_c | Maxim MAX7219/MAX7221 LED 驱动 | 中 |
| 5 | mrf24j40 | mrf24j40_c.c | mrf24j40_c | Microchip MRF24J40 802.15.4 RF | 高 |

---

## 2. 架构模式：SPI 上层解码器的 recv_proto 机制

### 2.1 SPI 底层解码器输出格式

SPI C 解码器 (`spi_c.c`) 通过 `c_decoder_put_python()` 向上层解码器输出以下 cmd 类型：

| cmd 字符串 | data 内容 | 说明 |
|------------|-----------|------|
| `"DATA"` | `[flag, mosi_8bytes, miso_8bytes]` (17字节) | flag: bit0=have_mosi, bit1=have_miso; mosi/miso 为 uint64_t 小端编码 |
| `"BITS"` | BITS v2 格式（per-bit ss/es时间戳），详见c_decoder_utils.h | 位级数据 | <!-- Updated: BITS格式已升级为v2，包含per-bit时间戳 -->
| `"CS-CHANGE"` | `[old_cs, new_cs]` (2字节) 或 NULL | CS 片选信号变化 |
| `"TRANSFER"` | NULL | 一次完整传输结束 |

### 2.2 DATA 格式详细解析

```c
// SPI DATA 输出格式 (17字节):
// data[0]     = flag: bit0=have_mosi, bit1=have_miso
// data[1..8]  = mosi 值 (uint64_t 小端, 8字节)
// data[9..16] = miso 值 (uint64_t 小端, 8字节)
//
// 解析示例:
int have_mosi = data[0] & 1;
int have_miso = (data[0] >> 1) & 1;
uint64_t mosi_val = 0, miso_val = 0;
for (int i = 0; i < 8; i++) {
    mosi_val |= ((uint64_t)data[1 + i]) << (8 * i);
    miso_val |= ((uint64_t)data[9 + i]) << (8 * i);
}
uint8_t mosi_byte = (uint8_t)(mosi_val & 0xFF);
uint8_t miso_byte = (uint8_t)(miso_val & 0xFF);
```

### 2.3 CS-CHANGE 格式详细解析

```c
// CS-CHANGE 输出格式:
// data=NULL, data_len=0: 无CS线的初始通知
// data[0]=old_cs, data[1]=new_cs: CS信号变化
//   new_cs=0 → CS asserted (片选有效)
//   new_cs=1 → CS deasserted (片选释放)
```

### 2.4 BITS 格式详细解析（v2，per-bit时间戳）

<!-- Updated: BITS格式已升级为v2，包含per-bit ss/es时间戳，旧格式已废弃 -->

```c
// BITS v2 输出格式 (详见 c_decoder_utils.h):
// data[0]                       = have_mosi (bit0) | have_miso (bit1)
// data[1]                       = mosi_bit_count (uint8_t)
// data[2 .. 2+mosi_count*17-1]  = MOSI bits, each 17 bytes:
//     [value(1B)][start_sample(8B LE)][end_sample(8B LE)]
// data[2+mosi_count*17]         = 0x00 (reserved / alignment)
// data[2+mosi_count*17+1]       = miso_bit_count (uint8_t)
// data[2+mosi_count*17+2 ..]    = MISO bits, each 17 bytes:
//     [value(1B)][start_sample(8B LE)][end_sample(8B LE)]
//
// 解析示例:
uint8_t flags = data[0];
int have_mosi = flags & 1;
int have_miso = (flags >> 1) & 1;
int mosi_cnt = data[1];
int pos = 2;
for (int i = 0; i < mosi_cnt; i++) {
    uint8_t val = data[pos];
    uint64_t ss = 0, es = 0;
    memcpy(&ss, data + pos + 1, 8);   // LE on LE host
    memcpy(&es, data + pos + 9, 8);
    pos += 17;
}
pos++;  // skip reserved byte
int miso_cnt = data[pos++];
for (int i = 0; i < miso_cnt; i++) {
    uint8_t val = data[pos];
    uint64_t ss = 0, es = 0;
    memcpy(&ss, data + pos + 1, 8);
    memcpy(&es, data + pos + 9, 8);
    pos += 17;
}
```

### 2.5 recv_proto 回调函数签名

```c
void (*recv_proto)(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

参数说明：
- `di`: 解码器实例
- `start_sample` / `end_sample`: 当前数据的采样点范围
- `cmd`: 上层协议命令字符串（如 "DATA", "CS-CHANGE", "BITS", "TRANSFER"）
- `data`: 附加数据缓冲区
- `data_len`: 数据长度

### 2.6 上层解码器关键约束

1. **不实现 `decode()` 函数**：`decode()` 必须为空函数 `(void)di;`
2. **通过 `recv_proto()` 接收数据**：在 `srd_c_decoder` 结构体中设置 `.recv_proto = xxx_recv_proto`
3. **inputs 必须为 `{"spi", NULL}`**：表示依赖 SPI 解码器
4. **outputs 为空**：`{NULL}`，`num_outputs = 0`（这些是终端解码器）
5. **无 channels / optional_channels**：`channels = NULL, num_channels = 0`
6. **在 `start()` 中注册输出**：`c_decoder_register_output(di, SRD_OUTPUT_ANN, "decoder_name")`
7. **不需要 `c_decoder_register_output(di, SRD_OUTPUT_PYTHON, ...)`**：因为上层解码器不向下层输出 python 数据

---

## 3. 各解码器详细规格

---

### 3.1 enc28j60_c — Microchip ENC28J60 以太网控制器

#### 3.1.1 元数据映射

| 属性 | Python 值 | C 值 |
|------|-----------|------|
| id | enc28j60 | enc28j60_c |
| name | ENC28J60 | ENC28J60(C) |
| longname | Microchip ENC28J60 | Microchip ENC28J60 (C) |
| desc | Microchip ENC28J60 10Base-T Ethernet controller protocol. | Microchip ENC28J60 10Base-T Ethernet controller protocol. (C implementation) |
| license | mit | mit |
| inputs | ['spi'] | {"spi", NULL} |
| outputs | [] | {NULL}, num_outputs=0 |
| tags | ['Embedded/industrial', 'Networking'] | {"Embedded/industrial", "Networking", NULL}, num_tags=2 |
| channels | 无 | NULL, 0 |
| optional_channels | 无 | NULL, 0 |
| options | 无 | NULL, 0 |

#### 3.1.2 Annotations 映射

```c
enum {
    ANN_RCR = 0,      // Read Control Register
    ANN_RBM,          // Read Buffer Memory
    ANN_WCR,          // Write Control Register
    ANN_WBM,          // Write Buffer Memory
    ANN_BFS,          // Bit Field Set
    ANN_BFC,          // Bit Field Clear
    ANN_SRC,          // System Reset Command
    ANN_DATA,         // Data
    ANN_REG_ADDR,     // Register Address
    ANN_WARNING,      // Warning
    NUM_ANN,
};

static const char *enc28j60_ann_labels[][3] = {
    {"", "rcr", "Read Control Register"},
    {"", "rbm", "Read Buffer Memory"},
    {"", "wcr", "Write Control Register"},
    {"", "wbm", "Write Buffer Memory"},
    {"", "bfs", "Bit Field Set"},
    {"", "bfc", "Bit Field Clear"},
    {"", "src", "System Reset Command"},
    {"", "data", "Data"},
    {"", "reg-addr", "Register Address"},
    {"", "warning", "Warning"},
};
```

#### 3.1.3 Annotation Rows 映射

```c
static const int enc28j60_row_commands_classes[] = {
    ANN_RCR, ANN_RBM, ANN_WCR, ANN_WBM, ANN_BFS, ANN_BFC, ANN_SRC, -1
};
static const int enc28j60_row_fields_classes[] = { ANN_DATA, ANN_REG_ADDR, -1 };
static const int enc28j60_row_warnings_classes[] = { ANN_WARNING, -1 };

static const struct srd_c_ann_row enc28j60_ann_rows[] = {
    {"commands", "Commands", enc28j60_row_commands_classes, 7},
    {"fields", "Fields", enc28j60_row_fields_classes, 2},
    {"warnings", "Warnings", enc28j60_row_warnings_classes, 1},
};
```

#### 3.1.4 寄存器名称查找表

Python 版本使用 `lists.py` 中的 `REGS` 二维数组（4个bank × 32个寄存器）。C 版本需内联此数据：

```c
// Bank 0-3 寄存器名称表 (4 banks × 32 registers)
static const char *enc28j60_regs[4][32] = {
    { // Bank 0
        "ERDPTL", "ERDPTH", "EWRPTL", "EWRPTH", "ETXSTL", "ETXSTH",
        "ETXNDL", "ETXNDH", "ERXSTL", "ERXSTH", "ERXNDL", "ERXNDH",
        "ERXRDPTL", "ERXRDPTH", "ERXWRPTL", "ERXWRPTH", "EDMASTL",
        "EDMASTH", "EDMANDL", "EDMANDH", "EDMADSTL", "EDMADSTH",
        "EDMACSL", "EDMACSH", "\xe2\x80\x94", "\xe2\x80\x94",
        "Reserved", "EIE", "EIR", "ESTAT", "ECON2", "ECON1",
    },
    { // Bank 1
        "EHT0", "EHT1", "EHT2", "EHT3", "EHT4", "EHT5", "EHT6", "EHT7",
        "EPMM0", "EPMM1", "EPMM2", "EPMM3", "EPMM4", "EPMM5", "EPMM6", "EPMM7",
        "EPMCSL", "EPMCSH", "\xe2\x80\x94", "\xe2\x80\x94",
        "EPMOL", "EPMOH", "Reserved", "Reserved",
        "ERXFCON", "EPKTCNT", "Reserved", "EIE", "EIR", "ESTAT", "ECON2", "ECON1",
    },
    { // Bank 2
        "MACON1", "Reserved", "MACON3", "MACON4", "MABBIPG", "\xe2\x80\x94",
        "MAIPGL", "MAIPGH", "MACLCON1", "MACLCON2", "MAMXFLL", "MAMXFLH",
        "Reserved", "Reserved", "Reserved", "\xe2\x80\x94",
        "Reserved", "Reserved", "MICMD", "\xe2\x80\x94",
        "MIREGADR", "Reserved", "MIWRL", "MIWRH", "MIRDL", "MIRDH",
        "Reserved", "EIE", "EIR", "ESTAT", "ECON2", "ECON1",
    },
    { // Bank 3
        "MAADR5", "MAADR6", "MAADR3", "MAADR4", "MAADR1", "MAADR2",
        "EBSTSD", "EBSTCON", "EBSTCSL", "EBSTCSH", "MISTAT", "\xe2\x80\x94",
        "\xe2\x80\x94", "\xe2\x80\x94", "\xe2\x80\x94", "\xe2\x80\x94",
        "\xe2\x80\x94", "\xe2\x80\x94", "EREVID", "\xe2\x80\x94",
        "\xe2\x80\x94", "ECOCON", "Reserved", "EFLOCON", "EPAUSL", "EPAUSH",
        "Reserved", "EIE", "EIR", "ESTAT", "ECON2", "ECON1",
    },
};
```

> **注意**：Python 中的 `'—'` 是 U+2014 EM DASH，C 中用 UTF-8 编码 `"\xe2\x80\x94"` 表示。

#### 3.1.5 解码逻辑分析

ENC28J60 的 SPI 协议结构：
- CS# 低电平有效期间，MOSI 发送命令字节 + 数据字节
- 命令字节格式：`[OPCODE(3bit)][REG_ADDR(5bit)]`
- OPCODE 定义：
  - `0b000` = RCR (Read Control Register)
  - `0b001` = RBM (Read Buffer Memory)
  - `0b010` = WCR (Write Control Register)
  - `0b011` = WBM (Write Buffer Memory)
  - `0b100` = BFS (Bit Field Set)
  - `0b101` = BFC (Bit Field Clear)
  - `0b111` = SRC (System Reset Command)

状态机逻辑：
1. `CS-CHANGE` new_cs=0 → 开始收集字节，记录 cmd_ss
2. `DATA` → 累积 mosi/miso 字节到缓冲区，记录各字节范围
3. `CS-CHANGE` new_cs=1 → 命令结束，处理命令

需要追踪的状态：
- `bsel0`, `bsel1`：ECON1 寄存器的 Bank Select 位，决定当前寄存器 bank
- 当 WCR/BFS/BFC 写 ECON1 (addr=0x1F) 时更新 bsel0/bsel1
- SRC 命令重置 bsel0=0, bsel1=0

#### 3.1.6 状态结构体

```c
typedef struct {
    int out_ann;
    uint8_t mosi[256];
    uint8_t miso[256];
    uint64_t ranges_ss[256];
    uint64_t ranges_es[256];
    int byte_count;
    uint64_t cmd_ss;
    uint64_t cmd_es;
    int active;
    int bsel0;
    int bsel1;
    int bsel_known;  // 是否已知当前 bank
} enc28j60_state;
```

#### 3.1.7 recv_proto 实现伪代码

```c
static void enc28j60_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    enc28j60_state *s = (enc28j60_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "CS-CHANGE") == 0) {
        int new_cs = (data && data_len >= 2) ? data[1] : 0;
        if (new_cs == 0) {
            // CS asserted - 开始新命令
            s->active = 1;
            s->cmd_ss = start_sample;
            s->byte_count = 0;
        } else {
            // CS deasserted - 处理命令
            if (s->active) {
                s->cmd_es = end_sample;
                enc28j60_process_command(di, s);
                s->active = 0;
            }
        }
    } else if (strcmp(cmd, "DATA") == 0) {
        if (!s->active) return;
        // 解析 MOSI/MISO 字节
        int have_mosi = data[0] & 1;
        int have_miso = (data[0] >> 1) & 1;
        uint8_t mosi_byte = 0, miso_byte = 0;
        if (have_mosi) {
            mosi_byte = data[1]; // uint64_t 小端第一个字节
        }
        if (have_miso) {
            miso_byte = data[9];
        }
        if (s->byte_count < 256) {
            s->mosi[s->byte_count] = mosi_byte;
            s->miso[s->byte_count] = miso_byte;
            s->ranges_ss[s->byte_count] = start_sample;
            s->ranges_es[s->byte_count] = end_sample;
            s->byte_count++;
        }
    }
}
```

#### 3.1.8 命令处理函数

```c
#define OPCODE_MASK   0b11100000
#define REG_ADDR_MASK 0b00011111
#define REG_ADDR_ECON1 0x1F
#define BIT_ECON1_BSEL0 0b00000001
#define BIT_ECON1_BSEL1 0b00000010

static void enc28j60_process_command(struct srd_decoder_inst *di, enc28j60_state *s)
{
    if (s->byte_count == 0) {
        s->active = 0;
        return;
    }

    uint8_t header = s->mosi[0];
    uint8_t opcode = header & OPCODE_MASK;

    switch (opcode) {
    case 0x00: enc28j60_process_rcr(di, s); break;
    case 0x20: enc28j60_process_rbm(di, s); break;
    case 0x40: enc28j60_process_wcr(di, s); break;
    case 0x60: enc28j60_process_wbm(di, s); break;
    case 0x80: enc28j60_process_bfs(di, s); break;
    case 0xA0: enc28j60_process_bfc(di, s); break;
    case 0xE0: enc28j60_process_src(di, s); break;
    default:
        // Unknown opcode
        C_ANN_PUT(di, s->cmd_ss, s->cmd_es, s->out_ann, ANN_WARNING,
                  "Warning: Unknown opcode.", "Warning");
        break;
    }
    s->active = 0;
}
```

各子命令处理函数需实现完整逻辑（详见 Python 源码 `pd.py`），关键点：
- RCR：2或3字节，MAC/MII 寄存器需要 dummy byte
- RBM：header 必须为 0x3A，读取 MISO 数据
- WCR：2字节，写 ECON1 时更新 bank
- WBM：header 必须为 0x7A，写入 MOSI 数据
- BFS/BFC：2字节，数据以二进制格式显示，修改 ECON1 bank 位
- SRC：1字节，重置 bank 为 0

---

### 3.2 ltc242x_c — LTC2421/LTC2422 20-bit ADC

#### 3.2.1 元数据映射

| 属性 | Python 值 | C 值 |
|------|-----------|------|
| id | ltc242x | ltc242x_c |
| name | LTC242x | LTC242x(C) |
| longname | Linear Technology LTC242x | Linear Technology LTC242x (C) |
| desc | Linear Technology LTC2421/LTC2422 1-/2-channel 20-bit ADC. | Linear Technology LTC2421/LTC2422 1-/2-channel 20-bit ADC. (C implementation) |
| license | gplv2+ | gplv2+ |
| inputs | ['spi'] | {"spi", NULL} |
| outputs | [] | {NULL}, num_outputs=0 |
| tags | ['IC', 'Analog/digital'] | {"IC", "Analog/digital", NULL}, num_tags=2 |
| channels | 无 | NULL, 0 |
| optional_channels | 无 | NULL, 0 |
| options | vref (default=1.5) | 1个选项 |

#### 3.2.2 Options 定义

```c
static struct srd_decoder_option ltc242x_options[] = {
    {"vref", NULL, "Reference voltage (V)", NULL, NULL},
};

// 在 srd_c_decoder_entry() 中初始化:
ltc242x_options[0].def = g_variant_new_double(1.5);
```

#### 3.2.3 Annotations 映射

```c
enum {
    ANN_CH0_VOLTAGE = 0,  // CH0 voltage
    ANN_CH1_VOLTAGE,      // CH1 voltage
    NUM_ANN,
};

static const char *ltc242x_ann_labels[][3] = {
    {"", "ch0_voltage", "CH0 voltage"},
    {"", "ch1_voltage", "CH1 voltage"},
};
```

#### 3.2.4 Annotation Rows 映射

```c
static const int ltc242x_row_ch0_classes[] = {ANN_CH0_VOLTAGE, -1};
static const int ltc242x_row_ch1_classes[] = {ANN_CH1_VOLTAGE, -1};

static const struct srd_c_ann_row ltc242x_ann_rows[] = {
    {"ch0_voltages", "CH0 voltages", ltc242x_row_ch0_classes, 1},
    {"ch1_voltages", "CH1 voltages", ltc242x_row_ch1_classes, 1},
};
```

#### 3.2.5 解码逻辑分析

LTC242x 协议特点：
- 使用 SPI BITS 输出（而非 DATA），逐位收集 MISO 数据
- CS# 低→高时完成一次转换数据读取
- 数据格式：24位，bit22=通道选择(CH0/CH1)，bit[21:0]=ADC 值
- 电压计算：`input_voltage = -(2^21 - data) / 0xFFFFF * vref`
- 输出两种格式：`%.6fV` 和 `%.2fV`

#### 3.2.6 状态结构体

```c
typedef struct {
    int out_ann;
    uint32_t data;       // 累积的位移数据
    uint64_t ss;         // CS# 下降沿起始采样点
    uint64_t es;         // CS# 上升沿结束采样点
    double vref;         // 参考电压
} ltc242x_state;
```

#### 3.2.7 recv_proto 实现关键逻辑

```c
static void ltc242x_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ltc242x_state *s = (ltc242x_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "CS-CHANGE") == 0) {
        if (!data || data_len < 2) return;
        int cs_old = data[0];
        int cs_new = data[1];
        if (cs_old == 0 && cs_new == 1) {
            // CS 上升沿 - 完成数据读取
            s->es = end_sample;
            s->data >>= 1;  // Python: self.data >>= 1
            ltc242x_handle_voltage(di, s);
            s->data = 0;
        } else if (cs_old == 1 && cs_new == 0) {
            // CS 下降沿 - 开始
            s->ss = start_sample;
        }
    } else if (strcmp(cmd, "BITS") == 0) {
        // BITS v2 格式 (详见 c_decoder_utils.h):
        // data[0] = have_mosi (bit0) | have_miso (bit1)
        // data[1] = mosi_bit_count
        // data[2..2+mosi_cnt*17-1] = per mosi bit: [value(1B)][ss(8B LE)][es(8B LE)]
        // data[2+mosi_cnt*17] = 0x00 (reserved)
        // data[2+mosi_cnt*17+1] = miso_bit_count
        // data[2+mosi_cnt*17+2..] = per miso bit: [value(1B)][ss(8B LE)][es(8B LE)]
        //
        // Python 逻辑: for bit in reversed(miso): data |= bit[0]; data <<= 1
        // 等价于: data = 0; for each bit in miso (first to last): data = (data | bit) << 1
        // 最后再 >>= 1 (在 CS-CHANGE 时)

        uint8_t flags = data[0];
        int have_miso = (flags >> 1) & 1;
        int mosi_cnt = data[1];
        int pos = 2;
        // 跳过 MOSI bits
        pos += mosi_cnt * 17;
        pos++;  // skip reserved byte
        int miso_cnt = data[pos++];
        if (have_miso && miso_cnt > 0) {
            for (int i = 0; i < miso_cnt && pos + 17 <= (int)data_len; i++) {
                int bit_val = data[pos]; // value byte
                pos += 17; // skip value + ss + es
                s->data = (s->data | bit_val) << 1;
            }
        }
    }
}
```

> **重要**：Python 版本使用 `BITS` 输出而非 `DATA`。BITS v2 格式已包含 per-bit 时间戳，解析方式见上方代码。Python 中 `reversed(miso)` 实际上是逆序遍历 bit 列表，每个 `bit[0]` 是该 bit 的值。 <!-- Updated: BITS格式已升级为v2 -->

实际 BITS v2 解析逻辑（与上方recv_proto中的代码一致）：
```c
// BITS v2 格式 (详见 c_decoder_utils.h):
// data[0] = have_mosi (bit0) | have_miso (bit1)
// data[1] = mosi_bit_count (uint8_t)
// data[2..2+mosi_cnt*17-1] = per mosi bit: [value(1B)][ss(8B LE)][es(8B LE)]
// data[2+mosi_cnt*17] = 0x00 (reserved/alignment)
// data[2+mosi_cnt*17+1] = miso_bit_count (uint8_t)
// data[2+mosi_cnt*17+2..] = per miso bit: [value(1B)][ss(8B LE)][es(8B LE)]
//
// Python 逻辑: for bit in reversed(miso): data |= bit[0]; data <<= 1
// 等价于: data = 0; for each bit in miso (first to last): data = (data | bit) << 1
// 最后再 >>= 1 (在 CS-CHANGE 时)

if (strcmp(cmd, "BITS") == 0) {
    uint8_t flags = data[0];
    int have_mosi = flags & 1;
    int have_miso = (flags >> 1) & 1;
    int mosi_cnt = data[1];
    int pos = 2;
    // 跳过 MOSI bits
    for (int i = 0; i < mosi_cnt && pos + 17 <= (int)data_len; i++) {
        pos += 17;
    }
    pos++;  // skip reserved byte
    int miso_cnt = data[pos++];
    if (have_miso) {
        for (int i = 0; i < miso_cnt && pos + 17 <= (int)data_len; i++) {
            int bit_val = data[pos];
            pos += 17;
            s->data = (s->data | bit_val) << 1;
        }
    }
}
``` <!-- Updated: BITS格式已升级为v2，旧v1格式已废弃 -->

#### 3.2.8 电压计算

```c
static void ltc242x_handle_voltage(struct srd_decoder_inst *di, ltc242x_state *s)
{
    uint32_t raw = s->data & 0x3FFFFF;  // 22-bit
    double input_voltage = -(double)(0x200000 - raw);  // -(2^21 - raw)
    input_voltage = (input_voltage / 0xFFFFF) * s->vref;

    int channel = (s->data >> 22) & 1;

    char v1[32], v2[32];
    snprintf(v1, sizeof(v1), "%.6fV", input_voltage);
    snprintf(v2, sizeof(v2), "%.2fV", input_voltage);
    C_ANN_PUT(di, s->ss, s->es, s->out_ann, channel, v1, v2);
}
```

---

### 3.3 max6954_c — Maxim MAX6954 LED 显示驱动

#### 3.3.1 元数据映射

| 属性 | Python 值 | C 值 |
|------|-----------|------|
| id | max6954 | max6954_c |
| name | MAX6954 | MAX6954(C) |
| longname | Maxim MAX6954 | Maxim MAX6954 (C) |
| desc | Maxim MAX6954 LED display driver. | Maxim MAX6954 LED display driver. (C implementation) |
| license | gplv2+ | gplv2+ |
| inputs | ['spi'] | {"spi", NULL} |
| outputs | [] | {NULL}, num_outputs=0 |
| tags | ['Display'] | {"Display", NULL}, num_tags=1 |
| channels | 无 | NULL, 0 |
| optional_channels | 无 | NULL, 0 |
| options | 无 | NULL, 0 |

#### 3.3.2 Annotations 映射

```c
enum {
    ANN_REG = 0,     // Register write
    ANN_DIGIT,       // Digit displayed
    ANN_WARNING,     // Warning
    NUM_ANN,
};

static const char *max6954_ann_labels[][3] = {
    {"", "register", "Register write"},
    {"", "digit", "Digit displayed"},
    {"", "warning", "Warning"},
};
```

#### 3.3.3 Annotation Rows 映射

```c
static const int max6954_row_commands_classes[] = {ANN_REG, ANN_DIGIT, -1};
static const int max6954_row_warnings_classes[] = {ANN_WARNING, -1};

static const struct srd_c_ann_row max6954_ann_rows[] = {
    {"commands", "Commands", max6954_row_commands_classes, 2},
    {"warnings", "Warnings", max6954_row_warnings_classes, 1},
};
```

#### 3.3.4 寄存器查找表

MAX6954 有大量寄存器（约 50+），Python 版本使用 `registers` 字典。C 版本需要用 switch-case 或查找数组实现。

关键寄存器及解码函数：

| 地址 | 名称 | 解码逻辑 |
|------|------|----------|
| 0x00 | No-op | 空操作 |
| 0x01 | Decode Mode | `0b{08b}` 二进制格式 |
| 0x02 | Global Intensity | intensity: 0=min, 15=max, 其他=数值 |
| 0x03 | Scan limit | `1 + val` |
| 0x04 | Configuration | 多字段解码 |
| 0x05 | GPIO Data | 各 GPIO 位 |
| 0x06 | Port Configuration | "not done" |
| 0x07 | Display test | on/off |
| 0x08-0x0B | KEY_A/B/C/D Mask | `0b{08b}` |
| 0x0C | Digit Type | 8位独立解码 |
| 0x0D-0x0F | (don't write) | - |
| 0x10-0x17 | Intensity pairs | 双通道 intensity |
| 0x20-0x2F | Digit P0 | 字符显示 |
| 0x40-0x4F | Digit P1 | 字符显示 |
| 0x60-0x6F | Digit Both | 字符显示 |
| 0x88-0x8F | Key registers | "not done" |

#### 3.3.5 解码逻辑分析

MAX6954 SPI 协议：
- CS# 低有效期间，发送 2 字节：地址字节 + 数据字节
- 第 1 个 DATA 字节 = 地址
- 第 2 个 DATA 字节 = 数据值
- pos=0 时记录地址，pos=1 时处理寄存器写入

CS-CHANGE 处理：
- CS asserted → pos=0, cs_start=ss
- CS deasserted → 检查 pos：pos=1 短写警告，pos>2 过长写警告

#### 3.3.6 状态结构体

```c
typedef struct {
    int out_ann;
    int pos;            // 当前字节位置 (0=地址, 1=数据)
    int cs_asserted;    // CS 是否有效
    uint8_t addr;       // 当前地址
    uint64_t addr_start; // 地址字节起始采样点
    uint64_t cs_start;  // CS 有效起始采样点
} max6954_state;
```

#### 3.3.7 recv_proto 实现关键逻辑

```c
static void max6954_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    max6954_state *s = (max6954_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") == 0) {
        if (!s->cs_asserted) return;
        uint8_t mosi_byte = data[1]; // MOSI 第一个字节

        if (s->pos == 0) {
            s->addr = mosi_byte;
            s->addr_start = start_sample;
        } else if (s->pos == 1) {
            max6954_handle_register(di, s, s->addr, mosi_byte,
                                     s->addr_start, end_sample);
        }
        s->pos++;
    } else if (strcmp(cmd, "CS-CHANGE") == 0) {
        int new_cs = (data && data_len >= 2) ? data[1] : 0;
        s->cs_asserted = (new_cs == 0);
        if (s->cs_asserted) {
            s->pos = 0;
            s->cs_start = start_sample;
        } else {
            if (s->pos == 1) {
                C_ANN_PUT(di, s->cs_start, end_sample, s->out_ann, ANN_WARNING, "Short write");
            } else if (s->pos > 2) {
                C_ANN_PUT(di, s->cs_start, end_sample, s->out_ann, ANN_WARNING, "Overlong write");
            }
        }
    }
}
```

#### 3.3.8 辅助函数：二进制格式化

C 标准库 `snprintf` 不支持 `%b` 格式说明符，需使用自定义辅助函数：

```c
static void fmt_binary(uint8_t val, char *buf, int bufsize)
{
    if (bufsize < 11) { buf[0] = '\0'; return; }  /* "0b" + 8 digits + '\0' */
    buf[0] = '0'; buf[1] = 'b';
    for (int i = 7; i >= 0; i--) {
        buf[2 + (7 - i)] = (val & (1 << i)) ? '1' : '0';
    }
    buf[10] = '\0';
}
```

#### 3.3.9 寄存器处理函数

```c
static void max6954_handle_register(struct srd_decoder_inst *di, max6954_state *s,
    uint8_t addr, uint8_t val, uint64_t ss, uint64_t es)
{
    char buf[256];
    const char *name = NULL;

    // 使用 switch-case 或查找表获取寄存器名称和解码值
    switch (addr) {
    case 0x00: name = "No-op"; buf[0] = '\0'; break;
    case 0x01: name = "Decode Mode"; fmt_binary(val, buf, sizeof(buf)); break;
    case 0x02: name = "Global Intensity"; max6954_decode_intensity(val, buf); break;
    case 0x03: name = "Scan limit"; snprintf(buf, sizeof(buf), "%d", 1 + val); break;
    case 0x04: name = "Configuration"; max6954_decode_configuration(val, buf); break;
    // ... 其他寄存器
    default:
        snprintf(buf, sizeof(buf), "Unknown register %02X", addr);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARNING, buf);
        return;
    }

    char ann_text[512];
    if (buf[0])
        snprintf(ann_text, sizeof(ann_text), "%s: %s", name, buf);
    else
        snprintf(ann_text, sizeof(ann_text), "%s", name);
    C_ANN_PUT(di, ss, es, s->out_ann, ANN_REG, ann_text);
}
```

---

### 3.4 max7219_c — Maxim MAX7219/MAX7221 LED 驱动

#### 3.4.1 元数据映射

| 属性 | Python 值 | C 值 |
|------|-----------|------|
| id | max7219 | max7219_c |
| name | MAX7219 | MAX7219(C) |
| longname | Maxim MAX7219/MAX7221 | Maxim MAX7219/MAX7221 (C) |
| desc | Maxim MAX72xx series 8-digit LED display driver. | Maxim MAX72xx series 8-digit LED display driver. (C implementation) |
| license | gplv2+ | gplv2+ |
| inputs | ['spi'] | {"spi", NULL} |
| outputs | [] | {NULL}, num_outputs=0 |
| tags | ['Display'] | {"Display", NULL}, num_tags=1 |
| channels | 无 | NULL, 0 |
| optional_channels | 无 | NULL, 0 |
| options | 无 | NULL, 0 |

#### 3.4.2 Annotations 映射

```c
enum {
    ANN_REG = 0,     // Registers written to the device
    ANN_DIGIT,       // Digits displayed on the device
    ANN_WARNING,     // Human-readable warnings
    NUM_ANN,
};

static const char *max7219_ann_labels[][3] = {
    {"", "register", "Registers written to the device"},
    {"", "digit", "Digits displayed on the device"},
    {"", "warnings", "Human-readable warnings"},
};
```

#### 3.4.3 Annotation Rows 映射

```c
static const int max7219_row_commands_classes[] = {ANN_REG, ANN_DIGIT, -1};
static const int max7219_row_warnings_classes[] = {ANN_WARNING, -1};

static const struct srd_c_ann_row max7219_ann_rows[] = {
    {"commands", "Commands", max7219_row_commands_classes, 2},
    {"warnings", "Warnings", max7219_row_warnings_classes, 1},
};
```

#### 3.4.4 寄存器查找表

```c
// MAX7219 寄存器 (比 MAX6954 简单得多)
static const char *max7219_reg_names[] = {
    "No-op",     // 0x00
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, // 0x01-0x08 是 digit
    "Decode",    // 0x09
    "Intensity", // 0x0A
    "Scan limit",// 0x0B
    "Shutdown",  // 0x0C
    NULL, NULL,  // 0x0D, 0x0E
    "Display test" // 0x0F
};
```

#### 3.4.5 解码逻辑分析

MAX7219 SPI 协议与 MAX6954 类似：
- CS# 低有效，2 字节：地址 + 数据
- 地址 0x01-0x08 = Digit 1-8
- 其他地址为控制寄存器

关键差异（相比 MAX6954）：
- Digit 地址 (1-8) 使用 ANN_DIGIT 而非 ANN_REG
- 寄存器数量少得多（仅 6 个控制寄存器）
- `_decode_intensity()` 函数与 MAX6954 相同

#### 3.4.6 状态结构体

```c
typedef struct {
    int out_ann;
    int pos;
    int cs_asserted;
    uint8_t addr;
    uint64_t addr_start;
    uint64_t cs_start;
} max7219_state;
```

#### 3.4.7 recv_proto 实现

与 MAX6954 结构几乎相同，区别在寄存器处理：

```c
static void max7219_handle_register(struct srd_decoder_inst *di, max7219_state *s,
    uint8_t addr, uint8_t val, uint64_t ss, uint64_t es)
{
    if (addr >= 1 && addr <= 8) {
        // Digit 显示
        char buf[64];
        snprintf(buf, sizeof(buf), "Digit %d: %02X", addr, val);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_DIGIT, buf);
    } else {
        // 控制寄存器
        char buf[256];
        switch (addr) {
        case 0x00: snprintf(buf, sizeof(buf), "No-op"); break;
        case 0x09: { char bin[11]; fmt_binary(val, bin, sizeof(bin)); snprintf(buf, sizeof(buf), "Decode: %s", bin); } break;
        case 0x0A: max7219_decode_intensity(val, buf); break;
        case 0x0B: snprintf(buf, sizeof(buf), "Scan limit: %d", 1 + val); break;
        case 0x0C: snprintf(buf, sizeof(buf), "Shutdown: %s", val ? "off" : "on"); break;
        case 0x0F: snprintf(buf, sizeof(buf), "Display test: %s", val ? "on" : "off"); break;
        default:
            snprintf(buf, sizeof(buf), "Unknown register %02X", addr);
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARNING, buf);
            return;
        }
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_REG, buf);
    }
}
```

---

### 3.5 mrf24j40_c — Microchip MRF24J40 802.15.4 RF 收发器

#### 3.5.1 元数据映射

| 属性 | Python 值 | C 值 |
|------|-----------|------|
| id | mrf24j40 | mrf24j40_c |
| name | MRF24J40 | MRF24J40(C) |
| longname | Microchip MRF24J40 | Microchip MRF24J40 (C) |
| desc | IEEE 802.15.4 2.4 GHz RF tranceiver chip. | IEEE 802.15.4 2.4 GHz RF tranceiver chip. (C implementation) |
| license | gplv2+ | gplv2+ |
| inputs | ['spi'] | {"spi", NULL} |
| outputs | [] | {NULL}, num_outputs=0 |
| tags | ['IC', 'Wireless/RF'] | {"IC", "Wireless/RF", NULL}, num_tags=2 |
| channels | 无 | NULL, 0 |
| optional_channels | 无 | NULL, 0 |
| options | 无 | NULL, 0 |

#### 3.5.2 Annotations 映射

```c
enum {
    ANN_SREAD = 0,     // Short register read
    ANN_SWRITE,        // Short register write
    ANN_LREAD,         // Long register read
    ANN_LWRITE,        // Long register write
    ANN_WARNING,       // Warning
    ANN_TX_FRAME,      // TX frame
    ANN_RX_FRAME,      // RX frame
    ANN_TX_RETRY_1,    // 1x TX retry
    ANN_TX_RETRY_2,    // 2x TX retry
    ANN_TX_RETRY_3,    // 3x TX retry
    ANN_TX_FAIL,       // TX fail
    ANN_CCAFAIL,       // CCAFAIL (channel busy)
    NUM_ANN,
};

static const char *mrf24j40_ann_labels[][3] = {
    {"", "sread", "Short register read"},
    {"", "swrite", "Short register write"},
    {"", "lread", "Long register read"},
    {"", "lwrite", "Long register write"},
    {"", "warning", "Warning"},
    {"", "tx-frame", "TX frame"},
    {"", "rx-frame", "RX frame"},
    {"", "tx-retry-1", "1x TX retry"},
    {"", "tx-retry-2", "2x TX retry"},
    {"", "tx-retry-3", "3x TX retry"},
    {"", "tx-fail", "TX fail (too many retries)"},
    {"", "ccafail", "CCAFAIL (channel busy)"},
};
```

#### 3.5.3 Annotation Rows 映射

```c
static const int mrf24j40_row_reads_classes[] = {ANN_SREAD, ANN_LREAD, -1};
static const int mrf24j40_row_writes_classes[] = {ANN_SWRITE, ANN_LWRITE, -1};
static const int mrf24j40_row_warnings_classes[] = {ANN_WARNING, -1};
static const int mrf24j40_row_tx_classes[] = {ANN_TX_FRAME, -1};
static const int mrf24j40_row_rx_classes[] = {ANN_RX_FRAME, -1};
static const int mrf24j40_row_retry1_classes[] = {ANN_TX_RETRY_1, -1};
static const int mrf24j40_row_retry2_classes[] = {ANN_TX_RETRY_2, -1};
static const int mrf24j40_row_retry3_classes[] = {ANN_TX_RETRY_3, -1};
static const int mrf24j40_row_txfail_classes[] = {ANN_TX_FAIL, -1};
static const int mrf24j40_row_ccafail_classes[] = {ANN_CCAFAIL, -1};

static const struct srd_c_ann_row mrf24j40_ann_rows[] = {
    {"reads", "Reads", mrf24j40_row_reads_classes, 2},
    {"writes", "Writes", mrf24j40_row_writes_classes, 2},
    {"warnings", "Warnings", mrf24j40_row_warnings_classes, 1},
    {"tx-frames", "TX frames", mrf24j40_row_tx_classes, 1},
    {"rx-frames", "RX frames", mrf24j40_row_rx_classes, 1},
    {"tx-retries-1", "1x TX retries", mrf24j40_row_retry1_classes, 1},
    {"tx-retries-2", "2x TX retries", mrf24j40_row_retry2_classes, 1},
    {"tx-retries-3", "3x TX retries", mrf24j40_row_retry3_classes, 1},
    {"tx-fails", "TX fails", mrf24j40_row_txfail_classes, 1},
    {"ccafails", "CCAFAILs", mrf24j40_row_ccafail_classes, 1},
};
```

#### 3.5.4 寄存器名称查找表

需要将 `lists.py` 中的 `sregs` (短寄存器, 64个) 和 `lregs` (长寄存器, ~70个) 内联为 C 数组：

```c
// 短寄存器名称表 (0x00-0x3F)
static const char *mrf24j40_sregs[64] = {
    "RXMCR", "PANIDL", "PANIDH", "SADRL", "SADRH",
    "EADR0", "EADR1", "EADR2", "EADR3", "EADR4",
    "EADR5", "EADR6", "EADR7", "RXFLUSH", "Reserved", "Reserved",
    "ORDER", "TXMCR", "ACKTMOUT", "ESLOTG1", "SYMTICKL",
    "SYMTICKH", "PACON0", "PACON1", "PACON2", "Reserved",
    "TXBCON0", "TXNCON", "TXG1CON", "TXG2CON", "ESLOTG23",
    "ESLOTG45", "ESLOTG67", "TXPEND", "WAKECON", "FRMOFFSET",
    "TXSTAT", "TXBCON1", "GATECLK", "TXTIME", "HSYMTIMRL",
    "HSYMTIMRH", "SOFTRST", "Reserved", "SECCON0", "SECCON1",
    "TXSTBL", "Reserved", "RXSR", "INTSTAT", "INTCON",
    "GPIO", "TRISGPIO", "SLPACK", "RFCTL", "SECCR2",
    "BBREG0", "BBREG1", "BBREG2", "BBREG3", "BBREG4",
    "Reserved", "BBREG6", "CCAEDTH",
};

// 长寄存器名称查找 (使用 switch-case 或条件判断)
static const char *mrf24j40_get_lreg_name(uint16_t reg)
{
    if (reg < 0x080) return "TX";
    if (reg < 0x100) return "TX beacon";
    if (reg < 0x180) return "TX GTS1";
    if (reg < 0x200) return "TX GTS2";
    if (reg < 0x280) {
        // 查找 lregs 表
        switch (reg) {
        case 0x200: return "RFCON0";
        case 0x201: return "RFCON1";
        // ... 完整列表
        case 0x24C: return "UPNONCE12";
        default: return "illegal";
        }
    }
    if (reg < 0x2C0) return "Security keys";
    if (reg < 0x300) return "Reserved";
    return "RX";
}
```

#### 3.5.5 解码逻辑分析

MRF24J40 SPI 协议：
- 短寄存器访问：2 字节 (1字节地址 + 1字节数据)
  - 地址字节 bit7=0 表示短寄存器
  - bit0=1=写, bit0=0=读
  - bit[6:1]=寄存器地址
- 长寄存器访问：3 字节 (2字节地址 + 1字节数据)
  - 第1字节 bit7=1 表示长寄存器
  - 16位地址: byte0[7:5,3:1] + byte1[7:5,3:1] → 10位地址
  - bit4 of dword = 写/读标志

帧缓存追踪：
- 维护 TX 和 RX 两个帧缓存
- 短寄存器写入 TXNCON (bit0=1) 时触发 TX 帧输出
- 短寄存器写入 RXFLUSH (bit0=1) 时触发 RX 帧输出
- 长寄存器访问 TX/RX 区域时累积帧数据
- TXSTAT 寄存器读取时检查重试次数和 CCAFAIL

#### 3.5.6 状态结构体

```c
#define MRF24J40_MAX_FRAME 256

typedef struct {
    int out_ann;
    uint8_t mosi_bytes[4];
    uint8_t miso_bytes[4];
    int byte_count;
    uint64_t ss_cmd;
    uint64_t es_cmd;
    uint64_t ss_frame[2];  // [RX=0, TX=1]
    uint64_t es_frame[2];
    uint8_t framecache[2][MRF24J40_MAX_FRAME];
    int framecache_len[2];
} mrf24j40_state;
```

#### 3.5.7 recv_proto 实现关键逻辑

```c
static void mrf24j40_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    mrf24j40_state *s = (mrf24j40_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "CS-CHANGE") == 0) {
        if (data && data_len >= 2 && data[0] == 0 && data[1] == 1) {
            // CS deasserted mid-stream
            if (s->byte_count != 0 && s->byte_count != 2 && s->byte_count != 3) {
                C_ANN_PUT(di, s->ss_cmd, end_sample, s->out_ann, ANN_WARNING, "Misplaced CS!");
                mrf24j40_reset_data(s);
            }
        }
        return;
    }

    if (strcmp(cmd, "DATA") != 0) return;

    uint8_t mosi = data[1];  // MOSI byte
    uint8_t miso = data[9];  // MISO byte

    if (s->byte_count == 0)
        s->ss_cmd = start_sample;

    if (s->byte_count < 4) {
        s->mosi_bytes[s->byte_count] = mosi;
        s->miso_bytes[s->byte_count] = miso;
    }
    s->byte_count++;

    if (s->byte_count < 2) return;

    if (s->mosi_bytes[0] & 0x80) {
        // 长寄存器访问 - 需要3字节
        if (s->byte_count == 3) {
            s->es_cmd = end_sample;
            mrf24j40_handle_long(di, s);
            mrf24j40_reset_data(s);
        }
    } else {
        // 短寄存器访问 - 2字节
        s->es_cmd = end_sample;
        mrf24j40_handle_short(di, s);
        mrf24j40_reset_data(s);
    }
}
```

---

## 4. 通用 C 解码器模板

### 4.1 文件结构模板

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

// === 1. Annotation 枚举 ===
enum { ... NUM_ANN };

// === 2. 状态结构体 ===
typedef struct { ... } xxx_state;

// === 3. 静态数据 (channels, options, ann_labels, ann_rows, inputs, outputs, tags) ===

// === 4. 辅助函数 ===

// === 5. recv_proto 回调 ===
static void xxx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{ ... }

// === 6. 生命周期函数 ===
static void xxx_reset(struct srd_decoder_inst *di) { ... }
static void xxx_start(struct srd_decoder_inst *di) { ... }
static void xxx_decode(struct srd_decoder_inst *di) { (void)di; }
static void xxx_destroy(struct srd_decoder_inst *di) { ... }

// === 7. 解码器结构体 ===
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    .longname = "... (C)",
    .desc = "... (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = xxx_options,
    .num_options = N,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = N,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = xxx_tags,
    .num_tags = N,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,
    .destroy = xxx_destroy,
    .recv_proto = xxx_recv_proto,
};

// === 8. 入口函数 ===
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    // 初始化 options 默认值和可选值列表
    return &xxx_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
```

### 4.2 SPI 上层解码器通用 recv_proto 框架

```c
static void xxx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    xxx_state *s = (xxx_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "CS-CHANGE") == 0) {
        // 处理 CS 信号变化
        int new_cs = (data && data_len >= 2) ? data[1] : 0;
        if (new_cs == 0) {
            // CS asserted - 开始新事务
            s->active = 1;
            s->cmd_ss = start_sample;
            s->byte_count = 0;
        } else {
            // CS deasserted - 结束事务，处理命令
            if (s->active) {
                s->cmd_es = end_sample;
                xxx_process_command(di, s);
                s->active = 0;
            }
        }
    } else if (strcmp(cmd, "DATA") == 0) {
        if (!s->active) return;
        // 解析 MOSI/MISO 字节
        int have_mosi = data[0] & 1;
        int have_miso = (data[0] >> 1) & 1;
        uint8_t mosi = have_mosi ? data[1] : 0;
        uint8_t miso = have_miso ? data[9] : 0;
        // 累积字节...
    } else if (strcmp(cmd, "BITS") == 0) {
        // 某些解码器需要 BITS 级别数据
    }
}
```

---

## 5. CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加 5 个新解码器：

```cmake
set(C_DECODERS spi_c i2c_c uart_c can_c jtag_c swd_c onewire_c i2s_c lin_c hdlc_c microwire_c mdio_c ps2_c dmx512_c nrzi_c ir_nec_c ir_rc5_c dcf77_c cec_c spdif_c usb_signalling_c 4b5b_c can_fd_c iso7816_c lpc_c dali_c c2_c graycode_c counter_c lm75_c ds1307_c ds3231_c numbers_and_state_c seven_segment_c pwm_c wiegand_c ir_sirc_c enc28j60_c ltc242x_c max6954_c max7219_c mrf24j40_c)
```

---

## 6. 参考文件索引

| 文件 | 用途 |
|------|------|
| `libsigrokdecode/c_decoders/spi_c.c` | SPI 底层解码器 — 理解输出格式 |
| `libsigrokdecode/c_decoders/i2c_c.c` | I2C 底层解码器 — 理解 python 输出格式 |
| `libsigrokdecode/c_decoders/lm75_c.c` | I2C 上层解码器 — recv_proto 范本（I2C 上层） |
| `libsigrokdecode/c_decoders/ds3231_c.c` | I2C 上层解码器 — 复杂 recv_proto 范本 |
| `libsigrokdecode/decoders/enc28j60/pd.py` | Python 源码 |
| `libsigrokdecode/decoders/enc28j60/lists.py` | ENC28J60 寄存器名称表 |
| `libsigrokdecode/decoders/ltc242x/pd.py` | Python 源码 |
| `libsigrokdecode/decoders/max6954/pd.py` | Python 源码 |
| `libsigrokdecode/decoders/max7219/pd.py` | Python 源码 |
| `libsigrokdecode/decoders/mrf24j40/pd.py` | Python 源码 |
| `libsigrokdecode/decoders/mrf24j40/lists.py` | MRF24J40 寄存器名称表 |
| `libsigrokdecode/libsigrokdecode.h` | C 解码器 API 定义 |
