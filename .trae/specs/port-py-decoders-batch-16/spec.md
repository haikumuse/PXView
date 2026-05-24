# Python→C 解码器移植规格 — Batch 16

本批次包含 5 个解码器：`spi-fast`、`swi`、`t55xx`、`tdm_audio`、`timing`。

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |


## 1. spi-fast (SPI Ultra-Fast)

### 1.1 元数据

| 字段 | Python 值 | C 值 |
|------|-----------|------|
| id | `spi-fast` | `spi_fast_c` |
| name | `SPI-Fast` | `SPI-Fast(C)` |
| longname | `Serial Peripheral Interface` | `Serial Peripheral Interface (C)` |
| desc | `Full-duplex, synchronous, serial bus.(Ultra-fast version)` | `SPI protocol decoder ultra-fast version (C implementation)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['logic']` | `{"logic", NULL}` |
| outputs | `['spi']` | `{"spi", NULL}` |
| tags | `['Embedded/industrial']` | `{"Embedded/industrial", NULL}` |

### 1.2 通道定义

**channels (1个, 必选):**

| 索引 | id | name | desc | type | idn |
|------|----|------|------|------|-----|
| 0 | clk | CLK | Clock(串行时钟) | SRD_CHANNEL_SCLK | `dec_spi_fast_chan_clk` |

**optional_channels (3个):**

| 索引 | id | name | desc | type | idn |
|------|----|------|------|------|-----|
| 1 | miso | MISO | Master in, slave out(主入从出) | SRD_CHANNEL_SDATA | `dec_spi_fast_opt_chan_miso` |
| 2 | mosi | MOSI | Master out, slave in(主出从入) | SRD_CHANNEL_SDATA | `dec_spi_fast_opt_chan_mosi` |
| 3 | cs | CS# | Chip-select(片选信号) | SRD_CHANNEL_SCS | `dec_spi_fast_opt_chan_cs` |

### 1.3 选项定义

| id | desc | 类型 | 默认值 | 可选值 | idn |
|----|------|------|--------|--------|-----|
| cs_polarity | CS# polarity(片选极性) | string | `"active-low"` | `("active-low", "active-high")` | `dec_spi_fast_opt_cs_polarity` |
| cpol | Clock polarity(时钟极性) | int | `0` | `(0, 1)` | `dec_spi_fast_opt_cpol` |
| cpha | Clock phase(时钟相位) | int | `0` | `(0, 1)` | `dec_spi_fast_opt_cpha` |
| bitorder | Bit order(位序) | string | `"msb-first"` | `("msb-first", "lsb-first")` | `dec_spi_fast_opt_bitorder` |
| wordsize | Word size(字长) | int | `8` | N/A (范围1-64) | `dec_spi_fast_opt_wordsize` |
| format | Data format(数据格式) | string | `"hex"` | `("ascii", "dec", "hex", "oct", "bin")` | `dec_spi_fast_opt_format` |
| show_data_point | Show data point(数据点显示) | string | `"no"` | `("yes", "no")` | `dec_spi_fast_opt_show_data_point` |

### 1.4 注解定义

```c
enum spi_fast_ann {
    ANN_MISO_DATA = 0,     // MISO data
    ANN_MOSI_DATA = 1,     // MOSI data
    ANN_ATK_DATA_POINT = 2, // ATK Data point
    ANN_ATK_RISING_EDGE = 3, // ATK Rising edge
    ANN_ATK_FALLING_EDGE = 4, // ATK Falling edge
    NUM_ANN = 5,
};
```

**ann_labels (注意第一列必须为空字符串):**
```c
static const char *spi_fast_ann_labels[][3] = {
    {"", "miso-data", "MISO data"},
    {"", "mosi-data", "MOSI data"},
    {"", "atk-data-point", "ATK Data point"},
    {"", "atk-rising-edge", "ATK Rising edge"},
    {"", "atk-falling-edge", "ATK Falling edge"},
};
```

**annotation_rows:**
```c
static const int row_miso_classes[] = {ANN_MISO_DATA};
static const int row_mosi_classes[] = {ANN_MOSI_DATA};
static const int row_atk_classes[] = {ANN_ATK_DATA_POINT, ANN_ATK_RISING_EDGE, ANN_ATK_FALLING_EDGE};

static const struct srd_c_ann_row spi_fast_ann_rows[] = {
    {"miso-data-vals", "MISO data", row_miso_classes, 1},
    {"mosi-data-vals", "MOSI data", row_mosi_classes, 1},
    {"atk-signs", "ATK signs", row_atk_classes, 3},
};
```

**binary:**
```c
static const struct srd_c_binary spi_fast_binary[] = {
    {"miso", "MISO"},
    {"mosi", "MOSI"},
};
```

### 1.5 状态机与解码逻辑

**核心逻辑:**

Python 版本的 `spi-fast` 与标准 `spi` 解码器类似，但简化了部分逻辑（无 TRANSFER 输出，无 BITS python 输出）。主要流程：

<!-- Updated: 上面的描述"无 BITS python 输出"是错误的！Python spi-fast 在 putdata() 中
     确实输出了 BITS 和 DATA（第176-177行）：
       self.put(ss, es, self.out_python, ['BITS', si_bits, so_bits])
       self.put(ss, es, self.out_python, ['DATA', si, so])
     C 实现必须输出 BITS v2 格式和 DATA 17字节格式，与 spi_c.c 完全一致，
     以确保上层解码器（依赖 'spi' 协议的解码器）可以正常堆叠。
     详见 c_decoder_utils.h 中的 BITS v2 格式说明。 -->

1. **初始化**: 检查通道可用性（CLK 必选，MISO/MOSI 至少一个）
2. **主循环**: 等待 CLK 边沿 + CS 边沿（如果有的话）
3. **CS 变化处理**: CS-CHANGE 事件 → 如果 CS 激活则开始新传输，CS 失效则输出 TRANSFER
4. **时钟边沿处理**: 根据 CPOL/CPHA 确定采样边沿
5. **位收集**: 按 bitorder 移位收集位，达到 wordsize 后输出数据

**关键差异 vs 标准 spi_c:**
- spi-fast 不输出 BITS python 数据，只输出 DATA 和 CS-CHANGE
<!-- Updated: 此描述错误！spi-fast Python 版本确实输出 BITS 和 DATA。
     C 实现必须输出 BITS v2 格式（per-bit ss/es 时间戳）和 DATA 17字节格式。
     参考 spi_c.c 中 spi_put_data() 的实现。 -->
- spi-fast 没有 TRANSFER 输出（但 Python 版本有，C 版本应保留以兼容上层解码器）
- spi-fast 的 ATK 注解用于标记采样点

**Condition Builder 使用:**

```c
// 主等待条件：CLK边沿 + CS边沿(可选)
srd_cond_builder *cb = c_cond_new();
c_cond_edge(cb, CLK);  // 或 c_cond_rise/c_cond_fall 根据采样边沿
if (s->have_cs) {
    c_cond_or(cb);
    c_cond_edge(cb, CS);
}
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

### 1.6 私有数据结构

```c
struct spi_fast_priv {
    uint64_t samplerate;
    int have_miso;
    int have_mosi;
    int have_cs;       // 0=无, >0=条件索引
    int cs_active;

    // 选项
    int cpol;
    int cpha;
    int bit_order;     // 0=msb-first, 1=lsb-first
    int wordsize;
    int format;        // 0=ascii, 1=dec, 2=hex, 3=oct, 4=bin
    int show_data_point;
    int cs_polarity;   // 0=active-low, 1=active-high
    int bw;            // <!-- Updated: 字宽字节数 = (wordsize + 7) / 8，与 spi_c.c 一致 -->

    // 位收集
    int bit_count;
    uint64_t miso_byte;
    uint64_t mosi_byte;
    uint64_t start_sample;
    uint64_t last_bit_sample;  // <!-- Updated: 需要记录最后一个 bit 的采样位置 -->
    int cs_was_deasserted;

    // <!-- Updated: BITS v2 格式需要的 per-bit 时间戳跟踪，参考 spi_c.c -->
    uint64_t miso_bits_ss[64];   // MISO 每个 bit 的起始 sample
    uint64_t miso_bits_es[64];   // MISO 每个 bit 的结束 sample
    int miso_bits_val[64];       // MISO 每个 bit 的值
    uint64_t mosi_bits_ss[64];   // MOSI 每个 bit 的起始 sample
    uint64_t mosi_bits_es[64];   // MOSI 每个 bit 的结束 sample
    int mosi_bits_val[64];       // MOSI 每个 bit 的值

    // <!-- Updated: TRANSFER 输出需要的字节累积，参考 spi_c.c -->
    uint64_t misobytes_val[256];
    int misobytes_cnt;
    uint64_t mosibytes_val[256];
    int mosibytes_cnt;
    uint64_t transfer_start;

    // 输出
    int out_ann;
    int out_python;
    int out_binary;
    int out_bitrate;   // <!-- Updated: SRD_OUTPUT_META 输出，参考 spi_c.c -->
};
```

### 1.7 关键代码片段

**格式化数据值:**
```c
static void spi_fast_format_value(uint64_t val, int wordsize, int format,
                                   char *buf, int bufsize)
{
    switch (format) {
    case 0: // ascii
        if (val >= 32 && val <= 126)
            snprintf(buf, bufsize, "%c", (char)val);
        else
            snprintf(buf, bufsize, "%02llX", (unsigned long long)val);
        break;
    case 1: // dec
        snprintf(buf, bufsize, "%llu", (unsigned long long)val);
        break;
    case 2: // hex
        snprintf(buf, bufsize, "%02llX", (unsigned long long)val);
        break;
    case 3: // oct
        snprintf(buf, bufsize, "%03llo", (unsigned long long)val);
        break;
    case 4: // bin
        for (int i = wordsize - 1; i >= 0; i--)
            buf[wordsize - 1 - i] = ((val >> i) & 1) + '0';
        buf[wordsize] = '\0';
        break;
    }
}
```

**CS 判定:**
```c
static int spi_fast_cs_asserted(struct spi_fast_priv *s, int cs_val)
{
    if (s->cs_polarity == 0) // active-low
        return (cs_val == 0);
    else // active-high
        return (cs_val == 1);
}
```

### 1.8 samplerate 守卫

```c
static void spi_fast_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    struct spi_fast_priv *s = (struct spi_fast_priv *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE)
        s->samplerate = value;
}

// decode() 开头
if (!s->samplerate) {
    // 等待 metadata 回调设置 samplerate
    // 使用 c_cond_wait 等待任意边沿，在循环中检查 samplerate
}
```

<!-- Updated: spi-fast 的 Python 输出格式必须与 spi_c.c 完全一致！
     1. BITS v2 格式（per-bit ss/es 时间戳）— 必须实现！
        格式详见 c_decoder_utils.h：
        data[0] = have_mosi (bit0) | have_miso (bit1)
        data[1] = mosi_bit_count (uint8_t)
        data[2..2+count*17-1] = per bit: [value(1B)][ss(8B LE)][es(8B LE)]
        data[2+count*17] = 0x00 (reserved/alignment)
        data[2+count*17+1] = miso_bit_count (uint8_t)
        data[2+count*17+2..] = per bit: [value(1B)][ss(8B LE)][es(8B LE)]
        使用 c_decoder_put_python(di, ss, es, out_python, "BITS", bits_data, bpos)

     2. DATA 17字节格式 — 必须实现！
        data[0] = (have_mosi ? 1 : 0) | (have_miso ? 2 : 0)
        data[1..8] = mosi_val (LE uint64)
        data[9..16] = miso_val (LE uint64)
        使用 c_decoder_put_python(di, ss, es, out_python, "DATA", data_data, 17)

     3. CS-CHANGE 输出 — 必须实现！
        data[0] = old_cs (0xFF if first), data[1] = new_cs
        使用 c_decoder_put_python(di, ss, es, out_python, "CS-CHANGE", cs_data, 2)

     4. TRANSFER 输出 — Python 版本有，C 版本应保留
        使用 c_decoder_put_python(di, ss, es, out_python, "TRANSFER", ...)

     5. 初始化时使用 c_cond_wait_current() 获取当前采样位置，
        然后读取 CS 引脚初始状态。参考 spi_c.c 的实现。

     参考 spi_c.c 中 spi_put_data() 和 spi_decode() 的完整实现。 -->

### 1.9 srd_c_decoder_entry 初始化

```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    // cs_polarity
    spi_fast_options[0].idn = "dec_spi_fast_opt_cs_polarity";
    spi_fast_options[0].def = g_variant_new_string("active-low");
    GSList *cs_vals = NULL;
    cs_vals = g_slist_append(cs_vals, g_variant_new_string("active-low"));
    cs_vals = g_slist_append(cs_vals, g_variant_new_string("active-high"));
    spi_fast_options[0].values = cs_vals;

    // cpol
    spi_fast_options[1].idn = "dec_spi_fast_opt_cpol";
    spi_fast_options[1].def = g_variant_new_uint64(0);
    GSList *cpol_vals = NULL;
    cpol_vals = g_slist_append(cpol_vals, g_variant_new_uint64(0));
    cpol_vals = g_slist_append(cpol_vals, g_variant_new_uint64(1));
    spi_fast_options[1].values = cpol_vals;

    // cpha (同 cpol)
    // bitorder
    // wordsize: g_variant_new_uint64(8), 无 values 列表
    // format: string 选项
    // show_data_point: string yes/no

    return &spi_fast_c_decoder;
}
```

---

## 2. swi (Infineon SWI)

### 2.1 元数据

| 字段 | Python 值 | C 值 |
|------|-----------|------|
| id | `swi` | `swi_c` |
| name | `SWI` | `SWI(C)` |
| longname | `Toy Decoder` | `Infineon SWI(C)` |
| desc | `A very simple decoder` | `Infineon Single Wire Interface protocol (C implementation)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['logic']` | `{"logic", NULL}` |
| outputs | `[]` | `{NULL}` |
| tags | `['Clock/timing', 'Util']` | `{"Clock/timing", "Util", NULL}` |

### 2.2 通道定义

**channels (1个):**

| 索引 | id | name | desc | type | idn |
|------|----|------|------|------|-----|
| 0 | swi | SWI | SWI channel | SRD_CHANNEL_SDATA | `dec_swi_chan_swi` |

### 2.3 选项定义

无选项（Python 版本 `options = ()`）。

### 2.4 注解定义

```c
enum swi_ann {
    ANN_BAUD_RATE = 0,  // Bauds (B1/B3)
    ANN_BITS = 1,       // Bits (0/1)
    ANN_BYTES = 2,      // Words (START/Unicast/Broadcast/ACK/Inv/N/hex)
    ANN_ERR = 3,        // Errors
    ANN_MEAN = 4,       // Meanings (Enum Start/Packet Header/etc)
    ANN_PBYTES = 5,     // Byte data (hex values)
    ANN_NMBR = 6,       // Numbers (UID/ODC/Sig/Msg/Rnd)
    NUM_ANN = 7,
};
```

**ann_labels:**
```c
static const char *swi_ann_labels[][3] = {
    {"", "baud_rate", "Bauds"},
    {"", "bits", "Bits"},
    {"", "bytes", "Words"},
    {"", "err", "Errors"},
    {"", "mean", "Means"},
    {"", "pbytes", "Byte"},
    {"", "nmbr", "Number"},
};
```

**annotation_rows:**
```c
static const int row_bauds_classes[] = {ANN_BAUD_RATE};
static const int row_bits_classes[] = {ANN_BITS};
static const int row_data_classes[] = {ANN_BYTES};
static const int row_errors_classes[] = {ANN_ERR};
static const int row_meanings_classes[] = {ANN_MEAN};
static const int row_meanings_data_classes[] = {ANN_PBYTES};
static const int row_numbs_classes[] = {ANN_NMBR};

static const struct srd_c_ann_row swi_ann_rows[] = {
    {"bauds", "Timing", row_bauds_classes, 1},
    {"bits_a", "Bits", row_bits_classes, 1},
    {"data", "Words", row_data_classes, 1},
    {"errors", "Errors", row_errors_classes, 1},
    {"meanings", "Meaning", row_meanings_classes, 1},
    {"meanings_data", "Data", row_meanings_data_classes, 1},
    {"numbs", "Numbers", row_numbs_classes, 1},
};
```

### 2.5 解码逻辑分析

**核心协议:**
- 基于边沿间隔编码，1 baud = 4.47μs
- 12-bit word 结构：2 bit training + 10 bit data
- B1 = 1 baud 间隔，B3 = 3 baud 间隔
- Word 间需要 5 baud 间隔
- 第13个 baud 决定是否 inverted（B3 = inverted）

**状态机:**

```
WAIT_FOR_5_BAUD_GAP → COLLECT_13_BAUDS → PARSE_WORD → (继续循环)
```

实际上 Python 版本没有显式状态机，而是用循环逻辑：

1. 等待边沿 (`wait({0: 'e'})`)
2. 计算 bauds = round(时间差 / 4.47μs)
3. 检查间隔是否为 1 或 3 baud（有效数据间隔）
4. 检查前一个间隔是否 >= 5 baud（word 间隔）
5. 收集 13 个 baud 间隔组成一个 word
6. 解析 word：training bits + data bits + invert bit
7. 根据 word_type (01=unicast, 10=broadcast) 分发解析

**关键算法:**

```c
// 计算 bauds
static int swi_calculate_bauds(uint64_t sampleN, uint64_t prevSampleN, uint64_t samplerate, int *halfRate)
{
    double t = (double)(sampleN - prevSampleN) / (double)samplerate;
    int bauds = (int)round(t / 4.47e-6);
    if (bauds % 2 == 0) {
        bauds /= 2;
        if (*halfRate < 1 && bauds == 1) {
            *halfRate = 1;
            // 输出 <HALF_BAUD> 注解
        }
    }
    return bauds;
}

// 计算 bit 值
static int swi_calculate_bit(int baud, int invert)
{
    return ((baud == 3 && !invert) || (baud == 1 && invert)) ? 1 : 0;
}
```

### 2.6 私有数据结构

```c
#define SWI_MAX_WORDS 256
#define SWI_MAX_PACKETS 256
#define SWI_MAX_UID_DATA 64

struct swi_priv {
    uint64_t samplerate;
    int strt;
    int halfRate;

    // 边沿历史
    uint64_t pastNs[1024];  // 动态增长用 ring buffer 或 realloc
    int pastVs[1024];       // 对应的 pin 值
    int log_count;

    // Word 历史
    struct swi_word {
        uint64_t startN;
        uint64_t endN;
        int type_int;
        int data_int;
        char bit_string[16];
        int inverted;
    } pastWords[SWI_MAX_WORDS];
    int word_count;
    int lastHdrIdx;
    int packetClass;
    int recieveData;

    // Packet 历史
    struct swi_packet {
        uint64_t startN;
        uint64_t endN;
        int recieve;
        int first_two_bytes;
        int last_byte;
        int packetClass;
        int recieve2;
    } pastPackets[SWI_MAX_PACKETS];
    int packet_count;
    int readPacketSeq;
    int polling;
    int readOdcNumber;

    // Enumerate 状态
    char pastBits[256];
    int pastBits_len;
    struct { uint64_t startN; int data; } pastUidData[SWI_MAX_UID_DATA];
    int uidData_count;
    uint64_t startUidByte;
    int bitsIdx;
    int enumIdx;

    int out_ann;
};
```

### 2.7 Condition Builder 使用

```c
// 等待任意边沿
srd_cond_builder *cb = c_cond_new();
c_cond_edge(cb, 0);
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

### 2.8 复杂部分：Broadcast/Unicast 解析

**Broadcast 解析 (`parse_broadcast`):**
- word[3]==0: Initialize
- word[4][2:8]=="000011": Enumerate/Select → 调用 `parse_enumerate`
- word[4][2:8]=="000010": Packet Header
- word[4][2:8]=="000101": Packet Class
- word[4][2:4]=="01"/"10": Selected device byte
- ECCE 命令解析

**Unicast 解析 (`parse_unicast`):**
- lastHdrIdx==2: 收集第一个字节
- lastHdrIdx==3: 收集第二个字节
- lastHdrIdx==4: 收集第三个字节并组装 packet → 调用 `parse_packet`

**Packet 解析 (`parse_packet`):**
- packetClass==0: `parse_packet_p0` (UID, polling)
- packetClass==1: `parse_packet_p1` (read, request, ECCE)
- ODC 读取序列（48+17+18 步）
- ECCE 认证（C/Z/X challenge）

### 2.9 关键代码片段

**Word 解析核心循环:**
```c
// 收集13个baud组成一个word
uint64_t data_ns[13];
int data_bauds[13];
data_ns[0] = s->pastNs[s->log_count - 1];
data_bauds[0] = start_bauds;

for (int i = 1; i < 13; i++) {
    // 等待下一个边沿
    srd_cond_builder *cb = c_cond_new();
    c_cond_edge(cb, 0);
    ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;

    int v = c_decoder_get_pin(di, 0, samplenum);
    int bauds = swi_calculate_bauds(samplenum, s->pastNs[s->log_count-1], s->samplerate, &s->halfRate);

    if (bauds == 1) {
        C_ANN_PUT(di, s->pastNs[s->log_count-1], samplenum, s->out_ann, ANN_BAUD_RATE, "B1");
    } else if (bauds == 3) {
        C_ANN_PUT(di, s->pastNs[s->log_count-1], samplenum, s->out_ann, ANN_BAUD_RATE, "B3");
    } else {
        // 非有效baud，跳出
        break;
    }

    data_ns[i] = s->pastNs[s->log_count-1];
    data_bauds[i] = bauds;
    // save_log
}
```

### 2.10 samplerate 守卫

```c
// decode() 开头
if (!s->samplerate) {
    // 必须有 samplerate 才能计算 bauds
    return;
}
```

---

## 3. t55xx (T55xx RFID)

### 3.1 元数据

| 字段 | Python 值 | C 值 |
|------|-----------|------|
| id | `t55xx` | `t55xx_c` |
| name | `T55xx` | `T55xx(C)` |
| longname | `RFID T55xx` | `T55xx RFID (C)` |
| desc | `T55xx 100-150kHz RFID protocol.` | `T55xx 100-150kHz RFID protocol (C implementation)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['logic']` | `{"logic", NULL}` |
| outputs | `[]` | `{NULL}` |
| tags | `['IC', 'RFID']` | `{"IC", "RFID", NULL}` |

### 3.2 通道定义

**channels (1个):**

| 索引 | id | name | desc | type | idn |
|------|----|------|------|------|-----|
| 0 | data | Data | Data line | SRD_CHANNEL_SDATA | `dec_t55xx_chan_data` |

### 3.3 选项定义

| id | desc | 类型 | 默认值 | idn |
|----|------|------|--------|-----|
| coilfreq | Coil frequency | uint64 | `125000` | `dec_t55xx_opt_coilfreq` |
| start_gap | Start gap min | uint64 | `20` | `dec_t55xx_opt_start_gap` |
| w_gap | Write gap min | uint64 | `20` | `dec_t55xx_opt_w_gap` |
| w_one_min | Write one min | uint64 | `48` | `dec_t55xx_opt_w_one_min` |
| w_one_max | Write one max | uint64 | `63` | `dec_t55xx_opt_w_one_max` |
| w_zero_min | Write zero min | uint64 | `16` | `dec_t55xx_opt_w_zero_min` |
| w_zero_max | Write zero max | uint64 | `31` | `dec_t55xx_opt_w_zero_max` |
| em4100_decode | EM4100 decode | string | `"on"` | `dec_t55xx_opt_em4100_decode` |

### 3.4 注解定义

```c
enum t55xx_ann {
    ANN_BIT_VALUE = 0,       // Bit value (0/1)
    ANN_START_GAP = 1,       // Start gap
    ANN_WRITE_GAP = 2,       // Write gap
    ANN_WRITE_MODE_EXIT = 3, // Write mode exit
    ANN_BIT = 4,             // Bit
    ANN_OPCODE = 5,          // Opcode
    ANN_LOCK = 6,            // Lock
    ANN_DATA = 7,            // Data
    ANN_PASSWORD = 8,        // Password
    ANN_ADDRESS = 9,         // Address
    ANN_BITRATE = 10,        // Bitrate/Decode
    NUM_ANN = 11,
};
```

**ann_labels:**
```c
static const char *t55xx_ann_labels[][3] = {
    {"", "bit_value", "Bit value"},
    {"", "start_gap", "Start gap"},
    {"", "write_gap", "Write gap"},
    {"", "write_mode_exit", "Write mode exit"},
    {"", "bit", "Bit"},
    {"", "opcode", "Opcode"},
    {"", "lock", "Lock"},
    {"", "data", "Data"},
    {"", "password", "Password"},
    {"", "address", "Address"},
    {"", "bitrate", "Bitrate"},
};
```

**annotation_rows:**
```c
static const int row_bits_classes[] = {ANN_BIT_VALUE};
static const int row_structure_classes[] = {ANN_START_GAP, ANN_WRITE_GAP, ANN_WRITE_MODE_EXIT, ANN_BIT};
static const int row_fields_classes[] = {ANN_OPCODE, ANN_LOCK, ANN_DATA, ANN_PASSWORD, ANN_ADDRESS};
static const int row_decode_classes[] = {ANN_BITRATE};

static const struct srd_c_ann_row t55xx_ann_rows[] = {
    {"bits", "Bits", row_bits_classes, 1},
    {"structure", "Structure", row_structure_classes, 4},
    {"fields", "Fields", row_fields_classes, 5},
    {"decode", "Decode", row_decode_classes, 1},
};
```

### 3.5 状态机与解码逻辑

**2 个状态:**
- `START_GAP`: 等待起始间隙
- `WRITE_GAP`: 等待写入间隙

**核心逻辑:**

1. 等待边沿变化 `{0: 'e'}`
2. 计算脉冲长度 `pl = samplenum - oldsamplenum`
3. 如果在 `WRITE_GAP` 状态且 `pl > writegap`，标记 gap_detected
4. 如果在 `START_GAP` 状态且 `pl > startgap`，标记 gap_detected 并切换到 `WRITE_GAP`
5. 当 gap_detected 时，检查前一个间隔是否在 write zero 或 write one 范围内
6. 如果超过 `nogap` (64个field clock周期)，退出写入模式

**关键阈值计算 (在 metadata 中):**
```c
s->field_clock = samplerate / coilfreq;
s->wzmax = w_zero_max * field_clock;
s->wzmin = w_zero_min * field_clock;
s->womax = w_one_max * field_clock;
s->womin = w_one_min * field_clock;
s->startgap = start_gap * field_clock;
s->writegap = w_gap * field_clock;
s->nogap = 64 * field_clock;
```

### 3.6 私有数据结构

```c
#define T55XX_MAX_BITS 70

struct t55xx_priv {
    uint64_t samplerate;
    uint64_t last_samplenum;
    uint64_t lastlast_samplenum;
    int state;  // 0=START_GAP, 1=WRITE_GAP

    // 位位置记录
    struct { int bit_val; uint64_t ss; uint64_t es; } bits_pos[T55XX_MAX_BITS];
    int bit_nr;

    // 阈值
    uint64_t field_clock;
    uint64_t wzmax, wzmin, womax, womin;
    uint64_t startgap, writegap, nogap;

    // 边沿追踪
    uint64_t oldsamplenum;
    uint64_t old_gap_start, old_gap_end;
    int gap_detected;

    // EM4100
    int em4100_decode1_partial;
    int em4100_decode;  // 0=off, 1=on

    // 字符串表
    // br_string, mod_str1, mod_str2, pskcf_str 作为静态数组

    int out_ann;
};
```

### 3.7 关键代码片段

**配置寄存器解码 (`decode_config`):**
```c
static void t55xx_decode_config(struct srd_decoder_inst *di, struct t55xx_priv *s, int idx)
{
    static const char *br_string[] = {"RF/8", "RF/16", "RF/32", "RF/40",
                                       "RF/50", "RF/64", "RF/100", "RF/128"};
    static const char *mod_str1[] = {"Direct", "Manchester", "Biphase", "Reserved"};
    static const char *mod_str2[] = {"Direct", "PSK1", "PSK2", "PSK3",
                                      "FSK1", "FSK2", "FSK1a", "FSK2a"};
    static const char *pskcf_str[] = {"RF/2", "RF/4", "RF/8", "Reserved"};

    // Safer Key (4 bits at idx)
    int safer_key = (s->bits_pos[idx].bit_val << 3) | (s->bits_pos[idx+1].bit_val << 2) |
                    (s->bits_pos[idx+2].bit_val << 1) | s->bits_pos[idx+3].bit_val;
    char buf[64];
    snprintf(buf, sizeof(buf), "Safer Key: %X", safer_key);
    C_ANN_PUT(di, s->bits_pos[idx].ss, s->bits_pos[idx+3].es, s->out_ann, ANN_BITRATE, buf);

    // Data Bit Rate (3 bits at idx+11)
    int bitrate = (s->bits_pos[idx+11].bit_val << 2) | (s->bits_pos[idx+12].bit_val << 1) |
                  s->bits_pos[idx+13].bit_val;
    snprintf(buf, sizeof(buf), "Data Bit Rate: %s", br_string[bitrate]);
    C_ANN_PUT(di, s->bits_pos[idx+11].ss, s->bits_pos[idx+13].es, s->out_ann, ANN_BITRATE, buf);

    // Modulation (5 bits at idx+15..19)
    int modulation1 = (s->bits_pos[idx+15].bit_val << 1) | s->bits_pos[idx+16].bit_val;
    int modulation2 = (s->bits_pos[idx+17].bit_val << 2) | (s->bits_pos[idx+18].bit_val << 1) |
                      s->bits_pos[idx+19].bit_val;
    const char *mod_string = (modulation1 == 0) ? mod_str2[modulation2] : mod_str1[modulation1];
    snprintf(buf, sizeof(buf), "Modulation: %s", mod_string);
    C_ANN_PUT(di, s->bits_pos[idx+15].ss, s->bits_pos[idx+19].es, s->out_ann, ANN_BITRATE, buf);

    // PSK-CF, AOR, Max-Block, PWD, ST-sequence terminator, POR delay
    // ... (类似模式)
}
```

**EM4100 解码:**
```c
static void t55xx_em4100_decode1(struct srd_decoder_inst *di, struct t55xx_priv *s, int idx)
{
    C_ANN_PUT(di, s->bits_pos[idx].ss, s->bits_pos[idx+8].es, s->out_ann, ANN_BITRATE,
              "EM4100 header", "EM header", "Header", "H");
    // 输出4个nibble
    t55xx_put4bits(di, s, idx+9);
    t55xx_put4bits(di, s, idx+14);
    t55xx_put4bits(di, s, idx+19);
    t55xx_put4bits(di, s, idx+24);
    // Partial nibble
    s->em4100_decode1_partial = (s->bits_pos[idx+29].bit_val << 3) |
                                 (s->bits_pos[idx+30].bit_val << 2) |
                                 (s->bits_pos[idx+31].bit_val << 1);
    C_ANN_PUT(di, s->bits_pos[idx+29].ss, s->bits_pos[idx+31].es, s->out_ann, ANN_BITRATE, "Partial nibble");
}
```

### 3.8 Condition Builder 使用

```c
// 等待任意边沿
srd_cond_builder *cb = c_cond_new();
c_cond_edge(cb, 0);
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

---

## 4. tdm_audio (TDM Multi-Channel Audio)

### 4.1 元数据

| 字段 | Python 值 | C 值 |
|------|-----------|------|
| id | `tdm_audio` | `tdm_audio_c` |
| name | `TDM audio` | `TDM audio(C)` |
| longname | `Time division multiplex audio` | `Time division multiplex audio (C)` |
| desc | `TDM multi-channel audio protocol.` | `TDM multi-channel audio protocol (C implementation)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['logic']` | `{"logic", NULL}` |
| outputs | `[]` | `{NULL}` |
| tags | `['Audio']` | `{"Audio", NULL}` |

### 4.2 通道定义

**channels (3个, 全部必选):**

| 索引 | id | name | desc | type | idn |
|------|----|------|------|------|-----|
| 0 | clock | Bitclk | Data bit clock | SRD_CHANNEL_SCLK | `dec_tdm_audio_chan_clock` |
| 1 | frame | Framesync | Frame sync | SRD_CHANNEL_SFS | `dec_tdm_audio_chan_frame` |
| 2 | data | Data | Serial data | SRD_CHANNEL_SDATA | `dec_tdm_audio_chan_data` |

### 4.3 选项定义

| id | desc | 类型 | 默认值 | 可选值 | idn |
|----|------|------|--------|--------|-----|
| bps | Bits per sample | uint64 | `16` | N/A | `dec_tdm_audio_opt_bps` |
| channels | Channels per frame | uint64 | `8` | `(1..8)` | `dec_tdm_audio_opt_channels` |
| edge | Clock edge to sample on | string | `"rising"` | `("rising", "falling")` | `dec_tdm_audio_opt_edge` |
| sampling_edge | Sampling Edge | string | `"first edge"` | `("first edge", "second edge")` | `dec_tdm_audio_opt_sampling_edge` |

### 4.4 注解定义

```c
#define TDM_AUDIO_MAX_CHANNELS 8

enum tdm_audio_ann {
    ANN_CH0 = 0,
    ANN_CH1 = 1,
    ANN_CH2 = 2,
    ANN_CH3 = 3,
    ANN_CH4 = 4,
    ANN_CH5 = 5,
    ANN_CH6 = 6,
    ANN_CH7 = 7,
    NUM_ANN = TDM_AUDIO_MAX_CHANNELS,
};
```

**ann_labels:**
```c
static const char *tdm_audio_ann_labels[][3] = {
    {"", "ch0", "Ch0"},
    {"", "ch1", "Ch1"},
    {"", "ch2", "Ch2"},
    {"", "ch3", "Ch3"},
    {"", "ch4", "Ch4"},
    {"", "ch5", "Ch5"},
    {"", "ch6", "Ch6"},
    {"", "ch7", "Ch7"},
};
```

**annotation_rows (每通道一行):**
```c
static const int row_ch0_classes[] = {ANN_CH0};
static const int row_ch1_classes[] = {ANN_CH1};
// ... 类似
static const struct srd_c_ann_row tdm_audio_ann_rows[] = {
    {"ch0-vals", "Ch0", row_ch0_classes, 1},
    {"ch1-vals", "Ch1", row_ch1_classes, 1},
    {"ch2-vals", "Ch2", row_ch2_classes, 1},
    {"ch3-vals", "Ch3", row_ch3_classes, 1},
    {"ch4-vals", "Ch4", row_ch4_classes, 1},
    {"ch5-vals", "Ch5", row_ch5_classes, 1},
    {"ch6-vals", "Ch6", row_ch6_classes, 1},
    {"ch7-vals", "Ch7", row_ch7_classes, 1},
};
```

### 4.5 解码逻辑

**无状态机！** 直接采样模式：

1. 等待时钟边沿（rising 或 falling，取决于选项）
2. 采样 data 引脚，移位到 data 寄存器
3. 检查 frame sync 信号变化（frame != lastframe && frame == 1）
4. 当 bitcount >= bitdepth 时输出通道数据
5. 重置计数器，继续下一个通道

**关键点:**
- `sampling_edge` 选项控制 frame sync 触发时第一个 bit 的处理方式
  - `"first edge"`: bitcount=1, data=data_pin（包含当前采样）
  - `"second edge"`: bitcount=0, data=0（从下一个边沿开始）
- 输出格式根据 bitdepth 选择：<=8 用 `%02x`，<=16 用 `%04x`，>16 用 `%08x`

### 4.6 私有数据结构

```c
struct tdm_audio_priv {
    uint64_t samplerate;
    int channels;
    int channel;
    int bitdepth;
    int bitcount;
    int samplecount;
    int lastsync;
    int lastframe;
    uint64_t data;
    uint64_t ss_block;
    int have_ss_block;

    // 选项
    int edge;           // 0=rising, 1=falling
    int sampling_edge;  // 0=first edge, 1=second edge

    int out_ann;
};
```

### 4.7 Condition Builder 使用

```c
// 等待时钟边沿
srd_cond_builder *cb = c_cond_new();
if (s->edge == 0)
    c_cond_rise(cb, 0);  // clock rising
else
    c_cond_fall(cb, 0);  // clock falling
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

### 4.8 关键代码片段

**通道数据输出:**
```c
if (s->have_ss_block && s->bitcount >= s->bitdepth) {
    s->bitcount = 0;
    int ch = s->channel % s->channels;

    char c1[32], c2[16], c3[8], v[16];
    snprintf(c1, sizeof(c1), "Channel %d", ch);
    snprintf(c2, sizeof(c2), "C%d", ch);
    snprintf(c3, sizeof(c3), "%d", ch);

    if (s->bitdepth <= 8)
        snprintf(v, sizeof(v), "%02llX", (unsigned long long)s->data);
    else if (s->bitdepth <= 16)
        snprintf(v, sizeof(v), "%04llX", (unsigned long long)s->data);
    else
        snprintf(v, sizeof(v), "%08llX", (unsigned long long)s->data);

    char ann_long[64], ann_mid[48], ann_short[32];
    snprintf(ann_long, sizeof(ann_long), "%s: %s", c1, v);
    snprintf(ann_mid, sizeof(ann_mid), "%s: %s", c2, v);
    snprintf(ann_short, sizeof(ann_short), "%s: %s", c3, v);
    C_ANN_PUT(di, s->ss_block, samplenum, s->out_ann, ch, ann_long, ann_mid, ann_short);

    s->data = 0;
    s->ss_block = samplenum;
    s->samplecount++;
    s->channel++;
}
```

**Frame sync 检测:**
```c
int frame = c_decoder_get_pin(di, 1, samplenum);
if (frame != s->lastframe && frame == 1) {
    s->channel = 0;
    if (s->sampling_edge == 0) {  // first edge
        s->bitcount = 1;
        s->data = data_val;
    } else {  // second edge
        s->bitcount = 0;
        s->data = 0;
    }
    if (!s->have_ss_block) {
        s->ss_block = samplenum;
        s->have_ss_block = 1;
    }
}
s->lastframe = frame;
```

---

## 5. timing (Timing Measurement)

### 5.1 元数据

| 字段 | Python 值 | C 值 |
|------|-----------|------|
| id | `timing` | `timing_c` |
| name | `Timing` | `Timing(C)` |
| longname | `Timing calculation with frequency and averaging` | `Timing calculation with frequency and averaging (C)` |
| desc | `Calculate time between edges.` | `Calculate time between edges (C implementation)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['logic']` | `{"logic", NULL}` |
| outputs | `[]` | `{NULL}` |
| tags | `['Clock/timing', 'Util']` | `{"Clock/timing", "Util", NULL}` |

### 5.2 通道定义

**channels (1个):**

| 索引 | id | name | desc | type | idn |
|------|----|------|------|------|-----|
| 0 | data | Data | Data line | SRD_CHANNEL_SDATA | `dec_timing_chan_data` |

### 5.3 选项定义

| id | desc | 类型 | 默认值 | 可选值 | idn |
|----|------|------|--------|--------|-----|
| avg_period | Averaging period | uint64 | `100` | N/A | `dec_timing_opt_avg_period` |
| edge | Edges to check | string | `"any"` | `("any", "rising", "falling")` | `dec_timing_opt_edge` |
| delta | Show delta from last | string | `"no"` | `("yes", "no")` | `dec_timing_opt_delta` |
| format | Format of 'time' annotation | string | `"full"` | `("full", "terse-auto", "terse-s", "terse-ms", "terse-us", "terse-ns", "terse-ps", "samples")` | `dec_timing_opt_format` |

### 5.4 注解定义

```c
enum timing_ann {
    ANN_TIME = 0,   // Time (full format)
    ANN_TERSE = 1,  // Terse (compact format)
    ANN_AVG = 2,    // Average
    ANN_DELTA = 3,  // Delta
    NUM_ANN = 4,
};
```

**ann_labels:**
```c
static const char *timing_ann_labels[][3] = {
    {"", "time", "Time"},
    {"", "terse", "Terse"},
    {"", "average", "Average"},
    {"", "delta", "Delta"},
};
```

**annotation_rows:**
```c
static const int row_times_classes[] = {ANN_TIME, ANN_TERSE};
static const int row_averages_classes[] = {ANN_AVG};
static const int row_deltas_classes[] = {ANN_DELTA};

static const struct srd_c_ann_row timing_ann_rows[] = {
    {"times", "Times", row_times_classes, 2},
    {"averages", "Averages", row_averages_classes, 1},
    {"deltas", "Deltas", row_deltas_classes, 1},
};
```

### 5.5 解码逻辑

**无状态机！** 直接边沿间隔测量：

1. 等待指定类型的边沿（any/rising/falling）
2. 计算与上一个边沿的间隔 `sa = es - ss`
3. 转换为时间 `t = sa / samplerate`
4. 根据 format 选项格式化输出
5. 如果 avg_period > 0，维护滑动窗口计算平均值
6. 如果 delta 选项开启，计算与上一次间隔的差值

### 5.6 私有数据结构

```c
#define TIMING_MAX_AVG 10000

struct timing_priv {
    uint64_t samplerate;

    // 选项
    int avg_period;
    int edge;       // 0=any, 1=rising, 2=falling
    int delta;      // 0=no, 1=yes
    int format;     // 0=full, 1=terse-auto, 2=terse-s, 3=terse-ms, 4=terse-us, 5=terse-ns, 6=terse-ps, 7=samples

    // 滑动平均
    double avg_buffer[TIMING_MAX_AVG];
    int avg_count;
    int avg_head;
    double avg_sum;

    // 上一次时间
    double last_t;
    uint64_t ss;
    int have_ss;

    int out_ann;
};
```

### 5.7 关键代码片段

**时间格式化 (`normalize_time`):**
```c
static void timing_normalize_time(double t, char *buf, int bufsize)
{
    if (fabs(t) >= 1.0) {
        snprintf(buf, bufsize, "%.3f s  (%.3f Hz)", t, 1.0/t);
    } else if (fabs(t) >= 1e-3) {
        double khz = (1.0/t) / 1000.0;
        if (khz < 1.0)
            snprintf(buf, bufsize, "%.3f ms (%.3f Hz)", t*1000.0, 1.0/t);
        else
            snprintf(buf, bufsize, "%.3f ms (%.3f kHz)", t*1000.0, khz);
    } else if (fabs(t) >= 1e-6) {
        double mhz = (1.0/t) / 1e6;
        if (mhz < 1.0)
            snprintf(buf, bufsize, "%.3f us (%.3f kHz)", t*1e6, (1.0/t)/1000.0);
        else
            snprintf(buf, bufsize, "%.3f us (%.3f MHz)", t*1e6, mhz);
    } else if (fabs(t) >= 1e-9) {
        snprintf(buf, bufsize, "%.3f ns (%.3f MHz)", t*1e9, (1.0/t)/1e6);
    } else {
        snprintf(buf, bufsize, "%f", t);
    }
}
```

**Terse 格式化:**
```c
static void timing_terse_time(double t, int fmt, char *bufs[], int *buf_lens, int max_bufs)
{
    double scale = 0;
    const char *unit = "";
    switch (fmt) {
    case 1: // terse-auto
        if (fabs(t) >= 1e0) { scale = 1e0; unit = "s"; }
        else if (fabs(t) >= 1e-3) { scale = 1e3; unit = "ms"; }
        else if (fabs(t) >= 1e-6) { scale = 1e6; unit = "us"; }
        else if (fabs(t) >= 1e-9) { scale = 1e9; unit = "ns"; }
        else if (fabs(t) >= 1e-12) { scale = 1e12; unit = "ps"; }
        break;
    case 2: scale = 1e0; break;   // terse-s
    case 3: scale = 1e3; break;   // terse-ms
    case 4: scale = 1e6; break;   // terse-us
    case 5: scale = 1e9; break;   // terse-ns
    case 6: scale = 1e12; break;  // terse-ps
    }
    if (scale > 0) {
        t *= scale;
        // 两个输出：带单位和不带单位
        snprintf(bufs[0], 64, "%.0f%s", t, unit);
        snprintf(bufs[1], 64, "%.0f", t);
        *buf_lens = 2;
    }
}
```

**滑动窗口平均:**
```c
if (s->avg_period > 0) {
    if (t > 0) {
        s->avg_sum += t;
        s->avg_buffer[s->avg_head] = t;
        s->avg_head = (s->avg_head + 1) % s->avg_period;
        if (s->avg_count < s->avg_period)
            s->avg_count++;
        else
            s->avg_sum -= s->avg_buffer[s->avg_head];
    }
    double average = s->avg_sum / s->avg_count;
    char avg_buf[128];
    timing_normalize_time(average, avg_buf, sizeof(avg_buf));
    C_ANN_PUT(di, ss, es, s->out_ann, ANN_AVG, avg_buf);
}
```

### 5.8 Condition Builder 使用

```c
srd_cond_builder *cb = c_cond_new();
switch (s->edge) {
case 1: c_cond_rise(cb, 0); break;   // rising
case 2: c_cond_fall(cb, 0); break;   // falling
default: c_cond_edge(cb, 0); break;  // any
}
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

---

## 通用实现规范

### 文件命名

| 解码器 | C 文件名 | C decoder id |
|--------|----------|-------------|
| spi-fast | `spi_fast_c.c` | `spi_fast_c` |
| swi | `swi_c.c` | `swi_c` |
| t55xx | `t55xx_c.c` | `t55xx_c` |
| tdm_audio | `tdm_audio_c.c` | `tdm_audio_c` |
| timing | `timing_c.c` | `timing_c` |

### CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加：
```
spi_fast_c
swi_c
t55xx_c
tdm_audio_c
timing_c
```

### 通用模式

1. **reset 回调**: `g_malloc0` 分配私有数据，`memset` 清零，初始化状态
2. **start 回调**: 注册输出 (`c_decoder_register_output`)，读取选项
3. **metadata 回调**: 保存 samplerate，计算时间阈值
4. **decode 回调**: 主循环 `while(1)` + condition builder 等待
5. **destroy 回调**: `g_free` 释放私有数据

### samplerate 守卫模式

所有需要 samplerate 的解码器必须在 decode() 中检查：
```c
if (!s->samplerate) {
    // 方案1: 直接返回（简单但不优雅）
    return;
    // 方案2: 等待 metadata 回调设置后再继续
    // 使用 c_cond_wait 等待边沿，在循环中检查 samplerate
}
```

推荐方案2：在主循环开始时检查，如果 samplerate 为 0 则继续等待边沿但不处理数据。

### ann_labels 第一列规则

ann_labels 的第一列必须为空字符串 `""`，因为 API 内部使用 i+7 偏移处理。

### 所有 annotation class 必须映射到 annotation_rows

每个 ann enum 值必须出现在至少一个 annotation_row 的 classes 数组中。

<!-- Updated: 已实现的关键 API 补充说明：
     1. SRD_OUTPUT_LOGIC + c_decoder_put_logic() — 已实现。
        签名：c_decoder_put_logic(di, ss, es, out_id, channel_mask, values, num_channels)
        spi-fast 如需输出原始逻辑信号给其他解码器，可注册 SRD_OUTPUT_LOGIC 输出。
     2. c_cond_wait_current(di, &samplenum) — 已实现，等效于 Python self.wait({})。
        获取当前采样位置而不前进。spi-fast 必须在 decode() 开头使用此 API
        获取当前采样位置并读取 CS 初始状态。参考 spi_c.c。
     3. c_decoder_get_initial_pin(di, ch) — 已实现，等效于 Python self.oldpin。
        获取初始引脚值。spi-fast 可用此 API 获取 CS 初始状态。
     4. BITS v2 格式 — 已在 spi_c.c 和 i2c_c.c 中实现。spi-fast 必须使用此格式。
        详见 c_decoder_utils.h。
     5. SPI DATA 格式（17字节）— 已在 spi_c.c 中实现。spi-fast 必须使用此格式。
     6. C解码器依赖规则 — C解码器只能依赖已有C实现的底层解码器。
        - spi-fast: inputs=['logic'], outputs=['spi'] — 无依赖阻塞，但输出格式
          必须与 spi_c.c 完全一致，以便上层解码器可以堆叠。
        - swi/t55xx/tdm_audio/timing: inputs=['logic'], outputs=[] — 无依赖问题。 -->
