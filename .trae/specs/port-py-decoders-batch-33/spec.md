# Python → C 解码器移植规格书 (Batch 33)

## 概述

本规格书描述将 5 个 Python 协议解码器移植为 C 解码器的详细方案。这 5 个解码器均为**上层解码器**（upper-layer decoders），它们不直接解析原始逻辑信号，而是通过 `recv_proto()` 回调接收下层解码器输出的协议数据，再进行高层协议解析。

### 移植目标

| # | Python 解码器 | C 文件名 | 依赖输入 | 下层 C 解码器 |
|---|---|---|---|---|
| 1 | `onewire_network` | `onewire_network_c.c` | `onewire_link` | `onewire_c` |
| 2 | `ds2408` | `ds2408_c.c` | `onewire_network` | `onewire_network_c` |
| 3 | `ds243x` | `ds243x_c.c` | `onewire_network` | `onewire_network_c` |
| 4 | `ds28ea00` | `ds28ea00_c.c` | `onewire_network` | `onewire_network_c` |
| 5 | `eeprom93xx` | `eeprom93xx_c.c` | `microwire` | `microwire_c` |

### 依赖链

<!-- Updated: 已验证所有依赖的C解码器均存在。onewire_c.c 和 microwire_c.c 均已实现，输出协议名与spec一致。C解码器依赖规则满足：所有下层解码器均有C实现。 -->

```
onewire_c (link layer, 已有)
  └─ onewire_network_c (network layer, 本次移植)
       ├─ ds2408_c (DS2408 设备, 本次移植)
       ├─ ds243x_c (DS243x EEPROM, 本次移植)
       └─ ds28ea00_c (DS28EA00 温度计, 本次移植)

microwire_c (已有)
  └─ eeprom93xx_c (93xx EEPROM, 本次移植)
```

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| i2c_c.c | 底层协议输出范本 | START/STOP条件检测、c_decoder_put_python()输出I2C协议数据 |
| uart_c.c | 底层协议输出范本 | c_decoder_put_python()输出协议数据、双通道(RX/TX)独立状态 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |

## 核心架构：上层 C 解码器模式

### recv_proto 回调机制

上层 C 解码器的核心是 `recv_proto` 回调函数。当底层解码器调用 `c_decoder_put_python()` 时，解码引擎会自动将协议数据转发给所有注册了该输入的上层解码器的 `recv_proto` 函数。

**函数签名**（定义在 `libsigrokdecode.h`）：
```c
void (*recv_proto)(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

**参数说明**：
- `di`：解码器实例
- `start_sample` / `end_sample`：该协议事件对应的采样点范围
- `cmd`：协议命令字符串（如 `"BIT"`, `"RESET/PRESENCE"`, `"ROM"`, `"DATA"` 等）
- `data`：协议数据字节缓冲区
- `data_len`：数据长度

### 调度逻辑（c_decoder_api.c 第 406-416 行）

```c
if (pdo->output_type == SRD_OUTPUT_PYTHON) {
    if (di->next_di) {
        GSList *l;
        for (l = di->next_di; l; l = l->next) {
            struct srd_decoder_inst *next_di = l->data;
            if (next_di->is_c_inst && next_di->c_dec_inst) {
                if (next_di->c_dec_inst->recv_proto) {
                    next_di->c_dec_inst->recv_proto(next_di,
                        start_sample, end_sample, cmd, data, data_len);
                }
            }
        }
    }
}
```

### 上层解码器模板

参考 `lm75_c.c` 和 `ds1307_c.c`，上层 C 解码器的标准模式为：

1. **`decode()` 函数为空** — 不直接解析原始信号
2. **所有逻辑在 `recv_proto()` 中** — 通过状态机处理下层传来的协议事件
3. **`channels = NULL` / `num_channels = 0`** — 不需要直接连接信号通道
4. **`inputs` 指定下层协议** — 如 `"onewire_link"`, `"onewire_network"`, `"microwire"`

---

## 解码器 1：onewire_network_c

### Python 原始元数据

```python
id = 'onewire_network'
name = '1-Wire network layer'
longname = '1-Wire serial communication bus (network layer)'
desc = 'Bidirectional, half-duplex, asynchronous serial bus.'
license = 'gplv2+'
inputs = ['onewire_link']
outputs = ['onewire_network']
tags = ['Embedded/industrial']
annotations = (
    ('text', 'Human-readable text'),
)
```

### 下层 onewire_c 输出的协议命令

从 `onewire_c.c` 源码分析，onewire_c 通过 `c_decoder_put_python()` 输出以下命令：

<!-- Updated: 已验证 onewire_c.c 实际输出，与spec一致。onewire_c 输出协议名为 "onewire_link"，cmd 为 "BIT" 和 "RESET/PRESENCE" -->

| 命令字符串 | data 内容 | 含义 |
|---|---|---|
| `"BIT"` | 1 字节，值为 0 或 1 | 单个 bit 值 |
| `"RESET/PRESENCE"` | 1 字节，值为 1（有设备） | 复位/应答检测 |

### Python 解码逻辑分析

**状态机**：
```
COMMAND → GET ROM / SEARCH ROM / TRANSPORT / COMMAND ERROR
GET ROM → TRANSPORT
SEARCH ROM → TRANSPORT
TRANSPORT → (持续接收 DATA)
COMMAND ERROR → (持续接收错误数据)
```

**ROM 命令字典**：
```python
command = {
    0x33: ['Read ROM'             , 'GET ROM'   ],
    0x0f: ['Conditional read ROM' , 'GET ROM'   ],
    0xcc: ['Skip ROM'             , 'TRANSPORT' ],
    0x55: ['Match ROM'            , 'GET ROM'   ],
    0xf0: ['Search ROM'           , 'SEARCH ROM'],
    0xec: ['Conditional search ROM', 'SEARCH ROM'],
    0x3c: ['Overdrive skip ROM'   , 'TRANSPORT' ],
    0x69: ['Overdrive match ROM'  , 'GET ROM'   ],
    0xa5: ['Resume'               , 'TRANSPORT' ],
    0x96: ['DS2408: Disable Test Mode', 'GET ROM'],
}
```

**关键函数**：

1. **`onewire_collect(length, val, ss, es)`** — 普通数据收集器
   - 将 bit 逐位收集到 `self.data`，LSB first
   - 收集满 `length` 位后返回 1，否则返回 0
   - `self.data = self.data & ~(1 << self.bit_cnt) | (val << self.bit_cnt)`

2. **`onewire_search(length, val, ss, es)`** — 搜索收集器
   - 三态循环：P（原始位）→ N（补码位）→ D（方向位）
   - 每个搜索周期接收 3 个 bit：原始地址位、补码地址位、方向位
   - 收集满 `length` 位后返回 1

### C 实现方案

#### 元数据映射

```c
.id = "onewire_network_c",
.name = "1-Wire network layer(C)",
.longname = "1-Wire serial communication bus (network layer)(C)",
.desc = "Bidirectional, half-duplex, asynchronous serial bus. (C implementation)",
.license = "gplv2+",
.inputs = {"onewire_link"},      // 对应 onewire_c 的 output
.outputs = {"onewire_network"},  // 供 ds2408/ds243x/ds28ea00 使用
.tags = {"Embedded/industrial"},
.channels = NULL,
.num_channels = 0,
```

#### 注解定义

```c
#define ANN_TEXT  0
#define NUM_ANN   1

static const char *own_ann_labels[][3] = {
    {"", "text", "Human-readable text"},
};

static const int own_row_text_classes[] = {ANN_TEXT};
static const struct srd_c_ann_row own_ann_rows[] = {
    {"text", "Text", own_row_text_classes, 1},
};
```

#### 私有状态结构

```c
enum own_state {
    STATE_COMMAND,
    STATE_GET_ROM,
    STATE_SEARCH_ROM,
    STATE_TRANSPORT,
    STATE_COMMAND_ERROR,
};

enum own_search_phase {
    SEARCH_PHASE_P,  // 接收原始地址位
    SEARCH_PHASE_N,  // 接收补码地址位
    SEARCH_PHASE_D,  // 接收方向位
};

struct own_priv {
    enum own_state state;
    int bit_cnt;
    enum own_search_phase search;
    uint64_t data_p;     // 搜索：原始地址
    uint64_t data_n;     // 搜索：补码地址
    uint64_t data;       // 收集的数据
    uint64_t rom;        // 64-bit ROM 地址
    uint64_t ss_block;   // 块起始采样点
    uint64_t es_block;   // 块结束采样点
    int out_ann;
    int out_python;
};
```

#### ROM 命令查找表

```c
struct rom_command {
    uint8_t code;
    const char *name;
    enum own_state next_state;
};

static const struct rom_command rom_commands[] = {
    {0x33, "Read ROM",                STATE_GET_ROM   },
    {0x0f, "Conditional read ROM",    STATE_GET_ROM   },
    {0xcc, "Skip ROM",                STATE_TRANSPORT },
    {0x55, "Match ROM",               STATE_GET_ROM   },
    {0xf0, "Search ROM",              STATE_SEARCH_ROM},
    {0xec, "Conditional search ROM",  STATE_SEARCH_ROM},
    {0x3c, "Overdrive skip ROM",      STATE_TRANSPORT },
    {0x69, "Overdrive match ROM",     STATE_GET_ROM   },
    {0xa5, "Resume",                  STATE_TRANSPORT },
    {0x96, "DS2408: Disable Test Mode", STATE_GET_ROM },
};
#define NUM_ROM_COMMANDS 10
```

#### recv_proto 实现

```c
static void onewire_network_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    struct own_priv *s = (struct own_priv *)c_decoder_get_private(di);
    if (!s) return;

    // 处理 RESET/PRESENCE
    if (strcmp(cmd, "RESET/PRESENCE") == 0) {
        uint8_t val = (data && data_len > 0) ? data[0] : 0;
        char text[64];
        snprintf(text, sizeof(text), "Reset/presence: %s",
                 val ? "true" : "false");
        C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_TEXT, text);
        c_decoder_put_python(di, start_sample, end_sample, s->out_python,
                             "RESET/PRESENCE", &val, 1);
        s->state = STATE_COMMAND;
        s->bit_cnt = 0;
        s->search = SEARCH_PHASE_P;
        s->data = 0;
        return;
    }

    // 只处理 BIT
    if (strcmp(cmd, "BIT") != 0) return;

    uint8_t val = (data && data_len > 0) ? data[0] : 0;

    switch (s->state) {
    case STATE_COMMAND: {
        if (s->bit_cnt == 0) s->ss_block = start_sample;
        s->data = s->data & ~((uint64_t)1 << s->bit_cnt) |
                  ((uint64_t)val << s->bit_cnt);
        s->bit_cnt++;
        if (s->bit_cnt == 8) {
            s->es_block = end_sample;
            s->data &= 0xFF;
            s->bit_cnt = 0;

            // 查找 ROM 命令
            int found = 0;
            for (int i = 0; i < NUM_ROM_COMMANDS; i++) {
                if (rom_commands[i].code == (uint8_t)s->data) {
                    char text[128];
                    snprintf(text, sizeof(text),
                             "ROM command: 0x%02x '%s'",
                             (uint8_t)s->data, rom_commands[i].name);
                    C_ANN_PUT(di, s->ss_block, s->es_block,
                              s->out_ann, ANN_TEXT, text);
                    s->state = rom_commands[i].next_state;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                char text[128];
                snprintf(text, sizeof(text),
                         "ROM command: 0x%02x '%s'",
                         (uint8_t)s->data, "unrecognized");
                C_ANN_PUT(di, s->ss_block, s->es_block,
                          s->out_ann, ANN_TEXT, text);
                s->state = STATE_COMMAND_ERROR;
            }
        }
        break;
    }

    case STATE_GET_ROM: {
        if (s->bit_cnt == 0) s->ss_block = start_sample;
        s->data = s->data & ~((uint64_t)1 << s->bit_cnt) |
                  ((uint64_t)val << s->bit_cnt);
        s->bit_cnt++;
        if (s->bit_cnt == 64) {
            s->es_block = end_sample;
            s->rom = s->data & 0xFFFFFFFFFFFFFFFFULL;
            s->bit_cnt = 0;
            s->data = 0;

            char text[128];
            snprintf(text, sizeof(text), "ROM: 0x%016llx",
                     (unsigned long long)s->rom);
            C_ANN_PUT(di, s->ss_block, s->es_block,
                      s->out_ann, ANN_TEXT, text);

            // 输出 ROM 协议数据给上层 (8字节, LSB first)
            unsigned char rom_bytes[8];
            for (int i = 0; i < 8; i++)
                rom_bytes[i] = (unsigned char)(s->rom >> (8 * i));
            c_decoder_put_python(di, s->ss_block, s->es_block,
                                 s->out_python, "ROM", rom_bytes, 8);

            s->state = STATE_TRANSPORT;
        }
        break;
    }

    case STATE_SEARCH_ROM: {
        // 三态搜索: P(原始) → N(补码) → D(方向)
        if (s->bit_cnt == 0 && s->search == SEARCH_PHASE_P)
            s->ss_block = start_sample;

        if (s->search == SEARCH_PHASE_P) {
            s->data_p = s->data_p & ~((uint64_t)1 << s->bit_cnt) |
                        ((uint64_t)val << s->bit_cnt);
            s->search = SEARCH_PHASE_N;
        } else if (s->search == SEARCH_PHASE_N) {
            s->data_n = s->data_n & ~((uint64_t)1 << s->bit_cnt) |
                        ((uint64_t)val << s->bit_cnt);
            s->search = SEARCH_PHASE_D;
        } else { // SEARCH_PHASE_D
            s->data = s->data & ~((uint64_t)1 << s->bit_cnt) |
                      ((uint64_t)val << s->bit_cnt);
            s->search = SEARCH_PHASE_P;
            s->bit_cnt++;
        }

        if (s->bit_cnt == 64) {
            s->es_block = end_sample;
            s->data_p &= 0xFFFFFFFFFFFFFFFFULL;
            s->data_n &= 0xFFFFFFFFFFFFFFFFULL;
            s->data   &= 0xFFFFFFFFFFFFFFFFULL;
            s->rom = s->data;

            char text[128];
            snprintf(text, sizeof(text), "ROM: 0x%016llx",
                     (unsigned long long)s->rom);
            C_ANN_PUT(di, s->ss_block, s->es_block,
                      s->out_ann, ANN_TEXT, text);

            unsigned char rom_bytes[8];
            for (int i = 0; i < 8; i++)
                rom_bytes[i] = (unsigned char)(s->rom >> (8 * i));
            c_decoder_put_python(di, s->ss_block, s->es_block,
                                 s->out_python, "ROM", rom_bytes, 8);

            s->search = SEARCH_PHASE_P;
            s->bit_cnt = 0;
            s->state = STATE_TRANSPORT;
        }
        break;
    }

    case STATE_TRANSPORT: {
        if (s->bit_cnt == 0) s->ss_block = start_sample;
        s->data = s->data & ~((uint64_t)1 << s->bit_cnt) |
                  ((uint64_t)val << s->bit_cnt);
        s->bit_cnt++;
        if (s->bit_cnt == 8) {
            s->es_block = end_sample;
            uint8_t byte_val = (uint8_t)(s->data & 0xFF);
            s->bit_cnt = 0;
            s->data = 0;

            char text[64];
            snprintf(text, sizeof(text), "Data: 0x%02x", byte_val);
            C_ANN_PUT(di, s->ss_block, s->es_block,
                      s->out_ann, ANN_TEXT, text);
            c_decoder_put_python(di, s->ss_block, s->es_block,
                                 s->out_python, "DATA", &byte_val, 1);
        }
        break;
    }

    case STATE_COMMAND_ERROR: {
        if (s->bit_cnt == 0) s->ss_block = start_sample;
        s->data = s->data & ~((uint64_t)1 << s->bit_cnt) |
                  ((uint64_t)val << s->bit_cnt);
        s->bit_cnt++;
        if (s->bit_cnt == 8) {
            s->es_block = end_sample;
            s->data &= 0xFF;
            s->bit_cnt = 0;

            char text[64];
            snprintf(text, sizeof(text),
                     "ROM error data: 0x%02x", (uint8_t)s->data);
            C_ANN_PUT(di, s->ss_block, s->es_block,
                      s->out_ann, ANN_TEXT, text);
        }
        break;
    }
    }
}
```

#### srd_c_decoder 结构体

```c
struct srd_c_decoder onewire_network_c_decoder = {
    .id = "onewire_network_c",
    .name = "1-Wire network layer(C)",
    .longname = "1-Wire serial communication bus (network layer)(C)",
    .desc = "Bidirectional, half-duplex, asynchronous serial bus. (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = own_ann_labels,
    .num_annotation_rows = 1,
    .annotation_rows = own_ann_rows,
    .inputs = own_inputs,
    .num_inputs = 1,
    .outputs = own_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = own_tags,
    .num_tags = 1,
    .reset = onewire_network_reset,
    .start = onewire_network_start,
    .decode = onewire_network_decode,  // 空函数
    .destroy = onewire_network_destroy,
    .recv_proto = onewire_network_recv_proto,
};
```

---

## 解码器 2：ds2408_c

### Python 原始元数据

```python
id = 'ds2408'
name = 'DS2408'
longname = 'Maxim DS2408'
desc = '1-Wire 8-channel addressable switch.'
license = 'gplv2+'
inputs = ['onewire_network']
outputs = []
tags = ['Embedded/industrial', 'IC']
annotations = (
    ('text', 'Human-readable text'),
)
```

### 下层 onewire_network_c 输出的协议命令

| 命令字符串 | data 内容 | 含义 |
|---|---|---|
| `"RESET/PRESENCE"` | 1 字节 (0/1) | 复位/应答 |
| `"ROM"` | 8 字节 (LSB first) | 64-bit ROM 地址 |
| `"DATA"` | 1 字节 | 传输层数据字节 |

### Python 解码逻辑分析

**功能命令字典**：
```python
command = {
    0xf0: 'Read PIO Registers',
    0xf5: 'Channel Access Read',
    0x5a: 'Channel Access Write',
    0xcc: 'Write Conditional Search Register',
    0xc3: 'Reset Activity Latches',
    0x3c: 'Disable Test Mode',
}
```

**状态机**：基于 `self.bytes` 列表累积接收到的 DATA 字节，根据 `self.bytes[0]`（功能命令）决定解析逻辑。

**各命令解析**：

1. **0xF0 Read PIO Registers**：
   - bytes[1..2]：目标地址（TA1, TA2，小端序）
   - bytes[3+]：PIO 寄存器数据

2. **0xF5 Channel Access Read**：
   - bytes[2+]：PIO 采样数据

3. **0x5A Channel Access Write**：
   - bytes[1]：数据字节
   - bytes[2]：数据字节的反码校验（`bytes[2] == bytes[1] ^ 0xFF`）
   - bytes[3+]：0xAA=成功，0xFF=失败

4. **0xCC Write Conditional Search Register**：
   - bytes[1..2]：目标地址
   - bytes[3+]：数据

5. **0xC3 Reset Activity Latches**：
   - bytes[2+]：0xAA=成功，其他=无效

### C 实现方案

#### 私有状态结构

```c
#define DS2408_MAX_BYTES 256

struct ds2408_priv {
    uint8_t bytes[DS2408_MAX_BYTES];
    int num_bytes;
    uint64_t ss;         // 当前事件起始
    uint64_t es;         // 当前事件结束
    uint64_t ss_block;   // 块起始
    int out_ann;
};
```

#### 功能命令查找表

```c
struct ds2408_command {
    uint8_t code;
    const char *name;
};

static const struct ds2408_command ds2408_commands[] = {
    {0xf0, "Read PIO Registers"},
    {0xf5, "Channel Access Read"},
    {0x5a, "Channel Access Write"},
    {0xcc, "Write Conditional Search Register"},
    {0xc3, "Reset Activity Latches"},
    {0x3c, "Disable Test Mode"},
};
#define NUM_DS2408_COMMANDS 6
```

#### recv_proto 核心逻辑

```c
static void ds2408_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    struct ds2408_priv *s = (struct ds2408_priv *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    if (strcmp(cmd, "RESET/PRESENCE") == 0) {
        uint8_t val = (data && data_len > 0) ? data[0] : 0;
        char text[64];
        snprintf(text, sizeof(text), "Reset/presence: %s",
                 val ? "true" : "false");
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_TEXT, text);
        s->num_bytes = 0;
        return;
    }

    if (strcmp(cmd, "ROM") == 0) {
        // data 为 8 字节 ROM (LSB first)
        uint64_t rom = 0;
        for (int i = 0; i < 8 && i < (int)data_len; i++)
            rom |= ((uint64_t)data[i] << (8 * i));
        uint8_t family = rom & 0xFF;
        char text[128];
        snprintf(text, sizeof(text),
                 "ROM: 0x%016llx (family code 0x%02x)",
                 (unsigned long long)rom, family);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_TEXT, text);
        s->num_bytes = 0;
        return;
    }

    if (strcmp(cmd, "DATA") != 0) return;

    uint8_t val = (data && data_len > 0) ? data[0] : 0;

    if (s->num_bytes < DS2408_MAX_BYTES)
        s->bytes[s->num_bytes++] = val;

    if (s->num_bytes == 1) {
        // 功能命令字节
        s->ss_block = start_sample;
        int found = 0;
        for (int i = 0; i < NUM_DS2408_COMMANDS; i++) {
            if (ds2408_commands[i].code == val) {
                char text[128];
                snprintf(text, sizeof(text), "%s (0x%02x)",
                         ds2408_commands[i].name, val);
                C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_TEXT, text);
                found = 1;
                break;
            }
        }
        if (!found) {
            char text[64];
            snprintf(text, sizeof(text),
                     "Unrecognized command: 0x%02x", val);
            C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_TEXT, text);
        }
    } else {
        // 根据功能命令解析后续字节
        ds2408_handle_data(di, s, val, start_sample, end_sample);
    }
}
```

#### ds2408_handle_data 详细逻辑

```c
static void ds2408_handle_data(struct srd_decoder_inst *di,
    struct ds2408_priv *s, uint8_t val,
    uint64_t ss, uint64_t es)
{
    uint8_t cmd = s->bytes[0];
    char text[256];

    switch (cmd) {
    case 0xf0: // Read PIO Registers
        if (s->num_bytes == 2) {
            s->ss_block = ss;
        } else if (s->num_bytes == 3) {
            uint16_t addr = s->bytes[1] | (s->bytes[2] << 8);
            snprintf(text, sizeof(text), "Target address: 0x%04x", addr);
            C_ANN_PUT(di, s->ss_block, es, s->out_ann, ANN_TEXT, text);
        } else if (s->num_bytes > 3) {
            snprintf(text, sizeof(text), "Data: 0x%02x", val);
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_TEXT, text);
        }
        break;

    case 0xf5: // Channel Access Read
        if (s->num_bytes == 2) {
            s->ss_block = ss;
        } else if (s->num_bytes > 2) {
            snprintf(text, sizeof(text), "PIO sample: 0x%02x", val);
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_TEXT, text);
        }
        break;

    case 0x5a: // Channel Access Write
        if (s->num_bytes == 2) {
            s->ss_block = ss;
        } else if (s->num_bytes == 3) {
            if (val == (s->bytes[1] ^ 0xFF)) {
                snprintf(text, sizeof(text),
                    "Data: 0x%02x (bit-inversion correct: 0x%02x)",
                    s->bytes[1], val);
            } else {
                snprintf(text, sizeof(text),
                    "Data error: second byte (0x%02x) is not "
                    "bit-inverse of first (0x%02x)",
                    val, s->bytes[1]);
            }
            C_ANN_PUT(di, s->ss_block, es, s->out_ann, ANN_TEXT, text);
        } else if (s->num_bytes > 3) {
            if (val == 0xaa) {
                C_ANN_PUT(di, ss, es, s->out_ann, ANN_TEXT, "Success");
            } else if (val == 0xff) {
                C_ANN_PUT(di, ss, es, s->out_ann, ANN_TEXT, "Fail New State");
            }
        }
        break;

    case 0xcc: // Write Conditional Search Register
        if (s->num_bytes == 2) {
            s->ss_block = ss;
        } else if (s->num_bytes == 3) {
            uint16_t addr = s->bytes[1] | (s->bytes[2] << 8);
            snprintf(text, sizeof(text), "Target address: 0x%04x", addr);
            C_ANN_PUT(di, s->ss_block, es, s->out_ann, ANN_TEXT, text);
        } else if (s->num_bytes > 3) {
            snprintf(text, sizeof(text), "Data: 0x%02x", val);
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_TEXT, text);
        }
        break;

    case 0xc3: // Reset Activity Latches
        if (s->num_bytes == 2) {
            s->ss_block = ss;
        } else if (s->num_bytes > 2) {
            if (val == 0xaa) {
                C_ANN_PUT(di, ss, es, s->out_ann, ANN_TEXT, "Success");
            } else {
                C_ANN_PUT(di, ss, es, s->out_ann, ANN_TEXT, "Invalid byte");
            }
        }
        break;
    }
}
```

---

## 解码器 3：ds243x_c

### Python 原始元数据

```python
id = 'ds243x'
name = 'DS243x'
longname = 'Maxim DS2432/3'
desc = 'Maxim DS243x series 1-Wire EEPROM protocol.'
license = 'gplv2+'
inputs = ['onewire_network']
outputs = []
tags = ['IC', 'Memory']
annotations = (
    ('text', 'Human-readable text'),
)
binary = (
    ('mem_read', 'Data read from memory'),
)
```

### Python 解码逻辑分析

**家族代码识别**：
```python
family_codes = {
    0x33: ('DS2432', commands_2432),
    0x23: ('DS2433', commands_2433),
}
```

**DS2432 命令集**：
```python
commands_2432 = {
    0x0f: 'Write scratchpad',
    0xaa: 'Read scratchpad',
    0x55: 'Copy scratchpad',
    0xf0: 'Read memory',
    0x5a: 'Load first secret',
    0x33: 'Compute next secret',
    0xa5: 'Read authenticated page',
}
```

**DS2433 命令集**：
```python
commands_2433 = {
    0x0f: 'Write scratchpad',
    0xaa: 'Read scratchpad',
    0x55: 'Copy scratchpad',
    0xf0: 'Read memory',
}
```

**CRC-16 校验**：
```python
def crc16(byte_array):
    reverse = 0xa001
    crc = 0x0000
    for byte in byte_array:
        for bit in range(8):
            if (byte ^ crc) & 1:
                crc = (crc >> 1) ^ reverse
            else:
                crc >>= 1
            byte >>= 1
    crc ^= 0xffff
    return crc
```

### C 实现方案

#### 私有状态结构

```c
#define DS243X_MAX_BYTES 64

struct ds243x_priv {
    uint8_t bytes[DS243X_MAX_BYTES];
    int num_bytes;
    uint8_t family_code;
    char family[16];
    int out_ann;
    int out_binary;
    uint64_t ss;
    uint64_t es;
    uint64_t ss_block;
};
```

#### CRC-16 实现

```c
static uint16_t ds243x_crc16(const uint8_t *buf, int len)
{
    uint16_t crc = 0x0000;
    for (int i = 0; i < len; i++) {
        uint8_t byte = buf[i];
        for (int bit = 0; bit < 8; bit++) {
            if ((byte ^ crc) & 1)
                crc = (crc >> 1) ^ 0xa001;
            else
                crc >>= 1;
            byte >>= 1;
        }
    }
    return crc ^ 0xffff;
}
```

#### recv_proto 核心逻辑

- 接收 `RESET/PRESENCE` → 清空 bytes
- 接收 `ROM` → 提取 family_code，识别设备型号，选择命令集
- 接收 `DATA` → 根据 bytes[0]（功能命令）和累积字节数解析各命令

**各命令详细解析**（按 Python 原始逻辑）：

1. **0x0F Write scratchpad**：bytes[1..2]=地址, bytes[3..10]=数据(8字节), bytes[11..12]=CRC-16
2. **0xAA Read scratchpad**：bytes[1..2]=地址, bytes[3]=E/S, bytes[4..11]=数据(8字节), bytes[12..13]=CRC-16
3. **0x55 Copy scratchpad**：bytes[1..3]=授权模式(TA1,TA2,E/S), bytes[4..23]=MAC(20字节), 后续=0xAA/0x55/0x00
4. **0xF0 Read memory**：bytes[1..2]=地址, bytes[3+]=数据, 输出 binary
5. **0x5A Load first secret**：bytes[1..3]=授权模式, 后续=0xAA/0x55
6. **0x33 Compute next secret**：bytes[1..2]=地址, 后续=0xAA/0x55
7. **0xA5 Read authenticated page**：bytes[1..2]=地址, bytes[3..34]=数据(32字节), bytes[35]=padding, bytes[36..37]=CRC-16, bytes[38..57]=MAC(20字节), bytes[58..59]=MAC CRC, 后续=0xAA/0x55

---

## 解码器 4：ds28ea00_c

### Python 原始元数据

```python
id = 'ds28ea00'
name = 'DS28EA00'
longname = 'Maxim DS28EA00 1-Wire digital thermometer'
desc = '1-Wire digital thermometer with Sequence Detect and PIO.'
license = 'gplv2+'
inputs = ['onewire_network']
outputs = []
tags = ['IC', 'Sensor']
annotations = (
    ('text', 'Human-readable text'),
)
```

### Python 解码逻辑分析

**功能命令字典**：
```python
command = {
    0x4e: 'Write scratchpad',
    0xbe: 'Read scratchpad',
    0x48: 'Copy scratchpad',
    0x44: 'Convert temperature',
    0xb4: 'Read power mode',
    0xb8: 'Recall EEPROM',
    0xf5: 'PIO access read',
    0xA5: 'PIO access write',
    0x99: 'Chain',
}
```

**状态机**：
```
ROM → COMMAND → READ SCRATCHPAD / CONVERT TEMPERATURE / 其他
```

**注意**：Python 版本中 `self.state = command[val].upper()` 将命令名转为大写作为状态名。大部分命令的后续处理标记为 `TODO`。

### C 实现方案

#### 私有状态结构

```c
enum ds28ea00_state {
    DS28EA00_ROM,
    DS28EA00_COMMAND,
    DS28EA00_READ_SCRATCHPAD,
    DS28EA00_CONVERT_TEMPERATURE,
    DS28EA00_WRITE_SCRATCHPAD,
    DS28EA00_COPY_SCRATCHPAD,
    DS28EA00_READ_POWER_MODE,
    DS28EA00_RECALL_EEPROM,
    DS28EA00_PIO_ACCESS_READ,
    DS28EA00_PIO_ACCESS_WRITE,
    DS28EA00_CHAIN,
};

struct ds28ea00_priv {
    enum ds28ea00_state state;
    uint64_t rom;
    uint64_t ss;
    uint64_t es;
    int out_ann;
};
```

#### recv_proto 核心逻辑

```c
static void ds28ea00_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    struct ds28ea00_priv *s = (struct ds28ea00_priv *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    if (strcmp(cmd, "RESET/PRESENCE") == 0) {
        uint8_t val = (data && data_len > 0) ? data[0] : 0;
        char text[64];
        snprintf(text, sizeof(text), "Reset/presence: %s",
                 val ? "true" : "false");
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_TEXT, text);
        s->state = DS28EA00_ROM;
        return;
    }

    if (strcmp(cmd, "ROM") == 0) {
        uint64_t rom = 0;
        for (int i = 0; i < 8 && i < (int)data_len; i++)
            rom |= ((uint64_t)data[i] << (8 * i));
        s->rom = rom;
        char text[128];
        snprintf(text, sizeof(text), "ROM: 0x%016llx",
                 (unsigned long long)rom);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_TEXT, text);
        s->state = DS28EA00_COMMAND;
        return;
    }

    if (strcmp(cmd, "DATA") != 0) return;

    uint8_t val = (data && data_len > 0) ? data[0] : 0;

    if (s->state == DS28EA00_COMMAND) {
        int found = 0;
        for (int i = 0; i < NUM_DS28EA00_COMMANDS; i++) {
            if (ds28ea00_commands[i].code == val) {
                char text[128];
                snprintf(text, sizeof(text),
                         "Function command: 0x%02x '%s'",
                         val, ds28ea00_commands[i].name);
                C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_TEXT, text);
                s->state = ds28ea00_commands[i].next_state;
                found = 1;
                break;
            }
        }
        if (!found) {
            char text[64];
            snprintf(text, sizeof(text),
                     "Unrecognized command: 0x%02x", val);
            C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_TEXT, text);
        }
    } else if (s->state == DS28EA00_READ_SCRATCHPAD) {
        char text[64];
        snprintf(text, sizeof(text), "Scratchpad data: 0x%02x", val);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_TEXT, text);
    } else if (s->state == DS28EA00_CONVERT_TEMPERATURE) {
        char text[64];
        snprintf(text, sizeof(text),
                 "Temperature conversion status: 0x%02x", val);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_TEXT, text);
    } else {
        // 其他状态: TODO
        char text[128];
        const char *state_name = ds28ea00_state_name(s->state);
        snprintf(text, sizeof(text), "TODO '%s': 0x%02x",
                 state_name, val);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_TEXT, text);
    }
}
```

---

## 解码器 5：eeprom93xx_c

### Python 原始元数据

```python
id = 'eeprom93xx'
name = '93xx EEPROM'
longname = '93xx Microwire EEPROM'
desc = '93xx series Microwire EEPROM protocol.'
license = 'gplv2+'
inputs = ['microwire']
outputs = []
tags = ['IC', 'Memory']
options = (
    {'id': 'addresssize', 'desc': 'Address size', 'default': 8},
    {'id': 'wordsize', 'desc': 'Word size', 'default': 16},
    {'id': 'format', 'desc': 'Data format', 'default': 'hex',
     'values': ('ascii', 'hex')},
)
annotations = (
    ('si-data', 'SI data'),
    ('so-data', 'SO data'),
    ('warning', 'Warning'),
)
annotation_rows = (
    ('data', 'Data', (0, 1)),
    ('warnings', 'Warnings', (2,)),
)
binary = (
    ('address', 'Address'),
    ('data', 'Data'),
)
```

### 下层 microwire_c 输出的协议数据

从 `microwire_c.c` 源码分析，microwire_c 通过 `c_decoder_put_python()` 输出：

<!-- Updated: 已验证 microwire_c.c 实际输出，与spec一致。microwire_c 输出协议名为 "microwire"，cmd 为 "microwire"，payload 为 mw_py_entry 数组 -->

```c
c_decoder_put_python(di, first->samplenum, last->samplenum,
                     p->out_python, "microwire",
                     (unsigned char *)pydata->data,
                     pydata->len * sizeof(struct mw_py_entry));
```

其中 `mw_py_entry` 结构为：
```c
struct mw_py_entry {
    uint64_t ss;   // bit 起始采样点
    uint64_t es;   // bit 结束采样点
    int si;        // SI bit 值
    int so;        // SO bit 值
};
```

**关键点**：microwire_c 将整个 CS 有效期间的 bit 数据作为一个 `"microwire"` 命令的 payload 一次性发送。Python 版本中 `data` 参数是一个 `mw_py_entry` 对象列表。

### Python 解码逻辑分析

**指令解码**（基于 SI 数据的前 2 位 opcode）：

| Opcode | SI[2:3] | 指令 | 说明 |
|---|---|---|---|
| 2 | 10 | READ | 读取字 |
| 1 | 01 | WRITE | 写入字 |
| 3 | 11 | ERASE | 擦除字 |
| 0 | 00 | 特殊 | 见下表 |

**Opcode=0 时的子指令**（基于 SI[2] 和 SI[3]）：

| SI[2] | SI[3] | 指令 | 说明 |
|---|---|---|---|
| 1 | 1 | WEN | 写使能 |
| 0 | 0 | WDS | 写禁止 |
| 1 | 0 | ERAL | 擦除全部 |
| 0 | 1 | WRAL | 写入全部 |

### C 实现方案

#### 元数据映射

```c
.id = "eeprom93xx_c",
.name = "93xx EEPROM(C)",
.longname = "93xx Microwire EEPROM (C)",
.desc = "93xx series Microwire EEPROM protocol. (C implementation)",
.license = "gplv2+",
.channels = NULL,
.num_channels = 0,
.options = eeprom93xx_options,
.num_options = 3,
.inputs = {"microwire"},
.outputs = NULL,
.num_outputs = 0,
.binary = eeprom93xx_binary,
.num_binary = 2,
.tags = {"IC", "Memory"},
```

#### 注解定义

```c
#define ANN_SI_DATA  0
#define ANN_SO_DATA  1
#define ANN_WARNING  2
#define NUM_ANN      3

static const char *eeprom93xx_ann_labels[][3] = {
    {"", "si-data", "SI data"},
    {"", "so-data", "SO data"},
    {"", "warning", "Warning"},
};

static const int eeprom93xx_row_data_classes[] = {ANN_SI_DATA, ANN_SO_DATA};
static const int eeprom93xx_row_warnings_classes[] = {ANN_WARNING};
static const struct srd_c_ann_row eeprom93xx_ann_rows[] = {
    {"data", "Data", eeprom93xx_row_data_classes, 2},
    {"warnings", "Warnings", eeprom93xx_row_warnings_classes, 1},
};

static const struct srd_decoder_binary eeprom93xx_binary[] = {
    {0, "address", "Address"},
    {1, "data", "Data"},
};
```

#### 选项定义

```c
static struct srd_decoder_option eeprom93xx_options[] = {
    {"addresssize", NULL, "Address size", NULL, NULL},
    {"wordsize", NULL, "Word size", NULL, NULL},
    {"format", NULL, "Data format", NULL, NULL},
};
```

#### 私有状态结构

```c
struct eeprom93xx_priv {
    int addresssize;   // 地址位数，默认 8
    int wordsize;      // 字长位数，默认 16
    int format;        // 0=hex, 1=ascii
    int out_ann;
    int out_binary;
};
```

#### recv_proto 核心逻辑

```c
// mw_py_entry 需要与 microwire_c.c 中定义一致
struct mw_py_entry {
    uint64_t ss;
    uint64_t es;
    int si;
    int so;
};

static void eeprom93xx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    struct eeprom93xx_priv *s = (struct eeprom93xx_priv *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "microwire") != 0) return;
    if (!data || data_len == 0) return;

    int num_entries = data_len / sizeof(struct mw_py_entry);
    struct mw_py_entry *entries = (struct mw_py_entry *)data;

    // 检查最小 bit 数: 2(opcode) + addresssize
    if (num_entries < 2 + s->addresssize) {
        C_ANN_PUT(di, start_sample, end_sample, s->out_ann,
                  ANN_WARNING, "Not enough packet bits");
        return;
    }

    // 提取 opcode (前 2 个 SI bit)
    int opcode = (entries[0].si << 1) | entries[1].si;

    if (opcode == 2) {
        // READ
        C_ANN_PUT(di, entries[0].ss, entries[1].es,
                  s->out_ann, ANN_SI_DATA, "Read word", "READ");
        eeprom93xx_put_address(di, s, entries, 2);
        // 读取所有字
        int word_start = 2 + s->addresssize;
        while (num_entries - word_start > 0) {
            if (num_entries - word_start < s->wordsize) {
                C_ANN_PUT(di, entries[word_start].ss,
                          entries[num_entries - 1].es,
                          s->out_ann, ANN_WARNING, "Not enough word bits");
                break;
            }
            eeprom93xx_put_word(di, s, 0, entries + word_start);
            word_start += s->wordsize;
        }
    } else if (opcode == 1) {
        // WRITE
        C_ANN_PUT(di, entries[0].ss, entries[1].es,
                  s->out_ann, ANN_SI_DATA, "Write word", "WRITE");
        eeprom93xx_put_address(di, s, entries, 2);
        if (num_entries < 2 + s->addresssize + s->wordsize) {
            C_ANN_PUT(di, entries[2 + s->addresssize].ss,
                      entries[num_entries - 1].ss,
                      s->out_ann, ANN_WARNING, "Not enough word bits");
        } else {
            eeprom93xx_put_word(di, s, 1,
                entries + 2 + s->addresssize);
        }
    } else if (opcode == 3) {
        // ERASE
        C_ANN_PUT(di, entries[0].ss, entries[1].es,
                  s->out_ann, ANN_SI_DATA, "Erase word", "ERASE");
        eeprom93xx_put_address(di, s, entries, 2);
    } else { // opcode == 0
        int bit2 = entries[2].si;
        int bit3 = entries[3].si;
        if (bit2 == 1 && bit3 == 1) {
            // WEN
            C_ANN_PUT(di, entries[0].ss,
                      entries[2 + s->addresssize - 1].es,
                      s->out_ann, ANN_SI_DATA, "Write enable", "WEN");
        } else if (bit2 == 0 && bit3 == 0) {
            // WDS
            C_ANN_PUT(di, entries[0].ss,
                      entries[2 + s->addresssize - 1].es,
                      s->out_ann, ANN_SI_DATA, "Write disable", "WDS");
        } else if (bit2 == 1 && bit3 == 0) {
            // ERAL
            C_ANN_PUT(di, entries[0].ss,
                      entries[2 + s->addresssize - 1].es,
                      s->out_ann, ANN_SI_DATA,
                      "Erase all memory", "Erase all", "ERAL");
        } else { // bit2==0 && bit3==1
            // WRAL
            C_ANN_PUT(di, entries[0].ss,
                      entries[2 + s->addresssize - 1].es,
                      s->out_ann, ANN_SI_DATA,
                      "Write all memory", "Write all", "WRAL");
            if (num_entries < 2 + s->addresssize + s->wordsize) {
                C_ANN_PUT(di, entries[2 + s->addresssize].ss,
                          entries[num_entries - 1].ss,
                          s->out_ann, ANN_WARNING, "Not enough word bits");
            } else {
                eeprom93xx_put_word(di, s, 1,
                    entries + 2 + s->addresssize);
            }
        }
    }
}
```

#### 辅助函数

```c
static void eeprom93xx_put_address(struct srd_decoder_inst *di,
    struct eeprom93xx_priv *s, struct mw_py_entry *entries, int start)
{
    uint16_t addr = 0;
    for (int b = 0; b < s->addresssize; b++)
        addr |= (entries[start + b].si << (s->addresssize - b - 1));

    char text[64];
    snprintf(text, sizeof(text), "Address: 0x%04x", addr);
    C_ANN_PUT(di, entries[start].ss, entries[start + s->addresssize - 1].es,
              s->out_ann, ANN_SI_DATA, text, "Addr: 0x%04x", "0x%04x");

    uint8_t addr_byte = (uint8_t)(addr & 0xFF);
    c_decoder_put_binary(di, entries[start].ss,
                         entries[start + s->addresssize - 1].es,
                         s->out_binary, 0, 1, &addr_byte);
}

static void eeprom93xx_put_word(struct srd_decoder_inst *di,
    struct eeprom93xx_priv *s, int si,
    struct mw_py_entry *entries)
{
    uint16_t word = 0;
    for (int b = 0; b < s->wordsize; b++) {
        int d = si ? entries[b].si : entries[b].so;
        word |= (d << (s->wordsize - b - 1));
    }
    int idx = si ? ANN_SI_DATA : ANN_SO_DATA;

    if (s->format == 1) { // ascii
        // 按 8 位一组显示字符
        char word_str[256] = {0};
        int pos = 0;
        for (int seg = 0; seg < s->wordsize; seg += 8) {
            uint8_t c = 0xff & (word >> seg);
            if (c >= 32 && c <= 126) {
                pos += snprintf(word_str + pos, sizeof(word_str) - pos,
                                "%c", c);
            } else {
                pos += snprintf(word_str + pos, sizeof(word_str) - pos,
                                "[%02X]", c);
            }
        }
        char text[128];
        snprintf(text, sizeof(text), "Data: %s", word_str);
        C_ANN_PUT(di, entries[0].ss, entries[s->wordsize - 1].es,
                  s->out_ann, idx, text, word_str);
    } else { // hex
        char text[64];
        snprintf(text, sizeof(text), "Data: 0x%04x", word);
        C_ANN_PUT(di, entries[0].ss, entries[s->wordsize - 1].es,
                  s->out_ann, idx, text, "0x%04x");

        uint8_t bdata[2] = {(uint8_t)((word & 0xff00) >> 8),
                            (uint8_t)(word & 0xff)};
        c_decoder_put_binary(di, entries[0].ss,
                             entries[s->wordsize - 1].es,
                             s->out_binary, 1, 2, bdata);
    }
}
```

---

## CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加 5 个新解码器：

```cmake
# 在 CMakeLists.txt 的 C_DECODERS 列表中追加：
onewire_network_c
ds2408_c
ds243x_c
ds28ea00_c
eeprom93xx_c
```

---

## 协议数据格式约定

### onewire_c → onewire_network_c 的数据格式

| 命令 | data 格式 | 说明 |
|---|---|---|
| `"BIT"` | 1 字节 (0x00 或 0x01) | 单个 bit 值 |
| `"RESET/PRESENCE"` | 1 字节 (0x00 或 0x01) | 0=无设备, 1=有设备 |

### onewire_network_c → ds2408_c / ds243x_c / ds28ea00_c 的数据格式

| 命令 | data 格式 | 说明 |
|---|---|---|
| `"RESET/PRESENCE"` | 1 字节 (0x00 或 0x01) | 复位/应答 |
| `"ROM"` | 8 字节 (LSB first) | 64-bit ROM 地址 |
| `"DATA"` | 1 字节 | 传输层数据 |

### microwire_c → eeprom93xx_c 的数据格式

| 命令 | data 格式 | 说明 |
|---|---|---|
| `"microwire"` | N × sizeof(mw_py_entry) 字节 | 整个 CS 周期的 bit 序列 |

---

## 风险与注意事项

1. **mw_py_entry 结构体对齐**：eeprom93xx_c 必须使用与 microwire_c.c 中完全相同的 `mw_py_entry` 结构定义，包括字段顺序和对齐方式。建议在 `eeprom93xx_c.c` 中重新定义该结构，确保与 microwire_c 一致。

<!-- Updated: 已验证 microwire_c.c 中 mw_py_entry 结构定义为 {uint64_t ss, uint64_t es, int si, int so}，sizeof 在典型 64 位平台上为 24 字节（8+8+4+4，无 padding）。eeprom93xx_c 中需使用完全相同的结构定义。注意：如果跨平台编译（32/64位），int 大小可能不同，建议改用固定宽度类型如 int32_t。 -->

2. **搜索 ROM 的三态逻辑**：onewire_network 的搜索收集器是三态循环（P→N→D），每个 ROM 位需要 3 个 BIT 事件。需确保 bit_cnt 的递增逻辑正确——只在 D 阶段递增。

3. **ds243x 的 CRC-16 校验**：CRC-16 算法需与 Python 版本完全一致（初始值 0x0000，多项式 0x8005 反转 0xA001，异或输出 0xFFFF）。

4. **ds243x 的数据累积**：ds243x 的某些命令（如 Read authenticated page）需要累积多达 60 个字节，需确保缓冲区足够大。

5. **onewire_network_c 的 output 协议**：ROM 数据以 8 字节 LSB first 格式输出，上层解码器需按此格式解析。

6. **eeprom93xx 的选项**：需要 3 个选项（addresssize, wordsize, format），在 `srd_c_decoder_entry()` 中初始化默认值和可选值列表。
