# Python → C 解码器移植规格书 — Batch 31

## 1. 概述

本规格书涵盖 5 个 UART 上层协议解码器从 Python 移植到 C 的详细规划。这 5 个解码器均以 `inputs=['uart']` 为输入源，通过 `recv_proto()` 回调接收 UART 下层解码器推送的协议数据，而非直接操作 logic 采样数据。

### 1.1 移植目标解码器

| # | Python ID | C ID | 协议名称 | 复杂度 | recv_proto |
|---|-----------|------|----------|--------|------------|
| 1 | `sbus_futaba` | `sbus_futaba_c` | Futaba SBUS 遥控协议 | ★★★★ | ✅ (DATA/FRAME/IDLE/BREAK) |
| 2 | `scs` | `scs_c` | SCS 家庭自动化总线 | ★☆☆☆ | ✅ (仅DATA) |
| 3 | `ufcs` | `ufcs_c` | UFCS 统一快充协议 | ★★★☆ | ✅ (仅DATA) |
| 4 | `amulet_ascii` | `amulet_ascii_c` | Amulet LCD ASCII 控制协议 | ★★★★★ | ✅ (仅DATA) |
| 5 | `streletz` | `streletz_c` | Streletz 安防系统串行协议 | ★★☆☆ | ✅ (FRAME) |

### 1.2 关键架构：recv_proto 模式

UART 上层解码器**不实现 `decode()` 函数**（或留空），而是通过 `recv_proto` 回调接收下层 UART 解码器推送的数据：

```c
void (*recv_proto)(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

UART 解码器通过 `c_decoder_put_python()` 推送以下协议消息：

| cmd 字符串 | data 内容 | 说明 |
|------------|-----------|------|
| `"DATA"` | `[byte_val, rxtx]` | UART 数据字节 + 方向(RX=0/TX=1) |
| `"FRAME"` | `[byte_val, rxtx, valid]` | UART 帧完成 + 有效性 |
| `"STARTBIT"` | `[bit_val]` | 起始位 |
| `"STOPBIT"` | `[bit_val]` | 停止位 |
| `"PARITYBIT"` | `[bit_val]` | 校验位 |
| `"IDLE"` | `[rxtx]` | 空闲期 <!-- Updated: 原标注"无"已过时，uart_c.c第406行已实现IDLE输出，data含1字节rxtx --> |
| `"BREAK"` | `[rxtx]` | 断路条件 <!-- Updated: 原标注"无"已过时，uart_c.c第414行已实现BREAK输出，data含1字节rxtx --> |
| `"INVALID STARTBIT"` | `[bit_val]` | 无效起始位 |
| `"INVALID STOPBIT"` | `[bit_val]` | 无效停止位 |
| `"PARITY ERROR"` | `[expected, actual]` | 校验错误 |

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| uart_c.c | 底层协议输出范本 | c_decoder_put_python()输出协议数据、双通道(RX/TX)独立状态 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |

---

## 2. 解码器详细规格

### 2.1 sbus_futaba_c — Futaba SBUS 遥控协议

#### 2.1.1 Python 元数据提取

| 属性 | Python 值 | C 映射 |
|------|-----------|--------|
| id | `sbus_futaba` | `sbus_futaba_c` |
| name | `SBUS (Futaba)` | `SBUS(C)` |
| longname | `Futaba SBUS (Serial bus)` | `Futaba SBUS Serial Bus (C)` |
| desc | `Serial bus for hobby remote control by Futaba` | 同上 + `(C implementation)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['uart']` | `{"uart"}` |
| outputs | `['sbus_futaba']` | `{"sbus_futaba"}` |
| tags | `['Remote Control']` | `{"Remote Control"}` |

#### 2.1.2 Options

| Option ID | 类型 | 默认值 | 描述 | values |
|-----------|------|--------|------|--------|
| `prop_val_min` | int | 0 | 比例通道值下界 | - |
| `prop_val_max` | int | 2047 | 比例通道值上界 | - |

#### 2.1.3 Annotations

| 枚举值 | ID | 标签 (long/short) |
|--------|-----|-------------------|
| ANN_HEADER = 0 | header | `"", "Header"` |
| ANN_PROPORTIONAL = 1 | proportional | `"", "Proportional"` |
| ANN_DIGITAL = 2 | digital | `"", "Digital"` |
| ANN_FRAME_LOST = 3 | framelost | `"", "Frame Lost"` |
| ANN_FAILSAFE = 4 | failsafe | `"", "Failsafe"` |
| ANN_FOOTER = 5 | footer | `"", "Footer"` |
| ANN_WARN = 6 | warning | `"", "Warning"` |

**NUM_ANN = 7**

#### 2.1.4 Annotation Rows

| Row ID | 描述 | 包含的 ann classes |
|--------|------|-------------------|
| framing | Framing | HEADER, FOOTER, FRAME_LOST, FAILSAFE |
| channels | Channels | PROPORTIONAL, DIGITAL |
| warnings | Warnings | WARN |

#### 2.1.5 协议解析逻辑

SBUS 消息结构（25 字节，UART 100kbps 8E2 反转信号）：

```
[Header: 0x0F] [16 × 11-bit proportional channels = 176 bits] [2 × 1-bit digital channels] [Flags: framelost(1b), failsafe(1b), MSB(4b)] [Footer: 0x00]
```

**状态机**：
- 收集 UART DATA bits → `bits_accum[]`（每个 bit 记录 `(value, ss, es)`）
- 收到 FRAME → 调用 `flush_accum_bits()` 解析已收集的 bits
- 收到 IDLE/BREAK → 刷新并重置

**字段解析流程**（`flush_accum_bits`）：
1. **Header** (8 bits): 期望值 `0x0F`，否则发 WARN
2. **16 个比例通道** (每个 11 bits): LSB-first bitpack，检查值范围
3. **2 个数字通道** (每个 1 bit)
4. **标志位** (2 bits): framelost, failsafe
5. **标志填充** (4 bits): MSB flags，期望全 0
6. **Footer** (8 bits): 期望值 `0x00`，否则发 WARN
7. 完成后若有多余 bits → 发 WARN

#### 2.1.6 C 实现关键设计

**私有状态结构**：
```c
typedef struct {
    // bits accumulator
    uint8_t bit_vals[256];    // bit values
    uint64_t bit_ss[256];     // bit start samples
    uint64_t bit_es[256];     // bit end samples
    int num_bits;             // accumulated bit count

    int sent_fields;          // fields already annotated
    int msg_complete;         // message fully parsed
    int failed;               // error flag (0=ok, 1=failed)
    char fail_text[64];       // failure reason text

    // options
    int prop_val_min;
    int prop_val_max;

    int out_ann;
    int out_python;
} sbus_state;
```

**recv_proto 处理逻辑**：
```c
static void sbus_recv_proto(struct srd_decoder_inst *di,
    uint64_t ss, uint64_t es,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    sbus_state *s = (sbus_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") == 0) {
        // data[0] = byte value, data[1] = rxtx
        // Python 版本获取的是 bits 数组，但 UART C 解码器推送的是字节值
        // 需要将字节拆分为 bits 存入 accum
        uint8_t byte_val = data[0];
        for (int i = 0; i < 8; i++) {
            s->bit_vals[s->num_bits] = (byte_val >> i) & 1; // LSB-first
            s->bit_ss[s->num_bits] = ss;
            s->bit_es[s->num_bits] = es;
            s->num_bits++;
        }
    } else if (strcmp(cmd, "FRAME") == 0) {
        // data[0] = byte_val, data[1] = rxtx, data[2] = valid
        if (data_len >= 3 && data[2] == 0)
            s->failed = 1;
        sbus_flush_accum_bits(di, s);
    } else if (strcmp(cmd, "IDLE") == 0) {
        // uart_c.c第406行已输出"IDLE"命令，data[0]=rxtx (RX=0/TX=1)
        // <!-- Updated: 原标注"暂不触发"已过时，uart_c.c已实现IDLE输出。data含1字节rxtx -->
        sbus_handle_idle(di, s, ss, es);
    } else if (strcmp(cmd, "BREAK") == 0) {
        // uart_c.c第414行已输出"BREAK"命令，data[0]=rxtx (RX=0/TX=1)
        // <!-- Updated: 原标注"暂不触发"已过时，uart_c.c已实现BREAK输出。data含1字节rxtx -->
        sbus_handle_break(di, s, ss, es);
    }
}
```

**注意**：Python 版本中 `handle_bits` 接收的是 bits 数组（每个 UART 数据位），但 C 版本的 UART 解码器通过 `c_decoder_put_python("DATA", ...)` 推送的是**字节值**而非 bits。因此 C 版本需要将字节拆分为 8 个 bits（LSB-first 顺序）存入 accumulator。**重要限制**：C 版本 UART DATA 输出仅含 `byte_val + rxtx`，不含 Python 版本中的 `databits` 数组（逐位值列表），因此 C 版本无法获取每个数据位的独立采样位置。对于 sbus_futaba 这类需要逐 bit 处理的解码器，C 版本只能从字节值重建 bits，且所有重建的 bits 共享同一 ss/es 区间。 <!-- Updated: IDLE/BREAK已由uart_c.c实现（第406/414行），sbus_futaba_c的IDLE/BREAK分支可正常触发。但DATA的逐位采样位置限制仍然存在 -->

**bitpack_lsb 实现**：
```c
static uint32_t bitpack_lsb(uint8_t *bits, int count)
{
    uint32_t val = 0;
    for (int i = 0; i < count && i < 32; i++)
        val |= ((uint32_t)(bits[i] & 1)) << i;
    return val;
}
```

---

### 2.2 scs_c — SCS 家庭自动化总线

#### 2.2.1 Python 元数据提取

| 属性 | Python 值 | C 映射 |
|------|-----------|--------|
| id | `scs` | `scs_c` |
| name | `SCS` | `SCS(C)` |
| longname | `Sistema Cablaggio Semplificato` | `Sistema Cablaggio Semplificato (C)` |
| desc | `fieldbus network protocol for home automation` | 同上 + `(C implementation)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['uart']` | `{"uart"}` |
| outputs | `[]` | `NULL` |
| tags | `['Embedded/industrial', 'Networking']` | `{"Embedded/industrial"}` |

#### 2.2.2 Options

无（Python `options = ()`）

#### 2.2.3 Annotations

| 枚举值 | ID | 标签 |
|--------|-----|------|
| ANN_SCS = 0 | scs | `"", "SCS"` |

**NUM_ANN = 1**

#### 2.2.4 Annotation Rows

无需多行（仅 1 个 annotation），但为规范起见设置 1 行：

| Row ID | 描述 | 包含的 ann classes |
|--------|------|-------------------|
| scs | SCS | ANN_SCS |

#### 2.2.5 协议解析逻辑

SCS 电报格式（7 字节）：

```
[0xA8: init] [addr] [??] [request] [??] [CRC] [0xA3: term]
```

**状态机**（极简）：
- `telegram_idx = 0`: 检查是否为 `0xA8`（init 字节）
- `telegram_idx = 1`: 地址字节，初始化 CRC
- `telegram_idx = 2`: 未知字节，XOR 到 CRC
- `telegram_idx = 3`: 请求字节，XOR 到 CRC
- `telegram_idx = 4`: 未知字节，XOR 到 CRC
- `telegram_idx = 5`: CRC 校验，比较计算值
- `telegram_idx = 6`: 终止字节，重置

**注意**：Python 版本仅处理 `ptype == 'DATA'`，忽略 FRAME/IDLE/BREAK。

#### 2.2.6 C 实现关键设计

**私有状态结构**：
```c
typedef struct {
    int telegram_idx;
    uint8_t crc;
    int out_ann;
} scs_state;
```

**recv_proto 处理逻辑**：
```c
static void scs_recv_proto(struct srd_decoder_inst *di,
    uint64_t ss, uint64_t es,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    scs_state *s = (scs_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") != 0) return;
    if (data_len < 1) return;

    uint8_t val = data[0];

    if (s->telegram_idx == 0 && val == 0xa8) {
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_SCS, "init");
    } else if (s->telegram_idx == 1) {
        s->crc = val;
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_SCS, "addr");
    } else if (s->telegram_idx == 2) {
        s->crc ^= val;
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_SCS, "??");
    } else if (s->telegram_idx == 3) {
        s->crc ^= val;
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_SCS, "request");
    } else if (s->telegram_idx == 4) {
        s->crc ^= val;
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_SCS, "??");
    } else if (s->telegram_idx == 5) {
        const char *crc_text = (s->crc == val) ? "good crc" : "bad crc";
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_SCS, crc_text);
    } else if (s->telegram_idx == 6) {
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_SCS, "term");
        s->telegram_idx = -1;
    }

    s->telegram_idx++;
}
```

---

### 2.3 ufcs_c — UFCS 统一快充协议

#### 2.3.1 Python 元数据提取

| 属性 | Python 值 | C 映射 |
|------|-----------|--------|
| id | `ufcs` | `ufcs_c` |
| name | `UFCS` | `UFCS(C)` |
| longname | `Universal Fast Charging Specification` | `Universal Fast Charging Specification (C)` |
| desc | `Universal fast charging specification... T/TAF 083-2021` | 同上 + `(C implementation)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['uart']` | `{"uart"}` |
| outputs | `[]` | `NULL` |
| tags | `['PC/Mobile']` | `{"PC/Mobile"}` |

#### 2.3.2 Options

| Option ID | 类型 | 默认值 | 描述 | values |
|-----------|------|--------|------|--------|
| `fulltext` | string | `"no"` | 完整文本解码 | `{"yes", "no"}` |

#### 2.3.3 Annotations

| 枚举值 | ID | 标签 |
|--------|-----|------|
| ANN_TYPE = 0 | type | `"", "Packet Type"` |
| ANN_TRAINING = 1 | training | `"", "Training"` |
| ANN_HEADER = 2 | header | `"", "Header"` |
| ANN_DATA = 3 | data | `"", "Data"` |
| ANN_CRC = 4 | crc | `"", "Checksum"` |
| ANN_WARNINGS = 5 | warnings | `"", "Warnings"` |
| ANN_SRC = 6 | src | `"", "Source Message"` |
| ANN_SNK = 7 | snk | `"", "Sink Message"` |
| ANN_PAYLOAD = 8 | payload | `"", "Payload"` |
| ANN_TEXT = 9 | text | `"", "Plain text"` |
| ANN_CABLE = 10 | cable | `"", "Cable Message"` |
| ANN_RESERVED = 11 | reserved | `"", "Reserved"` |

**NUM_ANN = 12**

#### 2.3.4 Annotation Rows

| Row ID | 描述 | 包含的 ann classes |
|--------|------|-------------------|
| phase | Parts | TRAINING, HEADER, DATA, CRC |
| payload | Payload | PAYLOAD |
| type | Type | TYPE, SRC, SNK, CABLE, RESERVED |
| warnings | Warnings | WARNINGS |
| text | Full text | TEXT |

#### 2.3.5 协议解析逻辑

UFCS 包格式：

```
[0xAA: SOP] [Head0] [Head1] [CMD] [DataLen] [Data0..DataN] [CRC8]
```

**包头解析** (4 字节 Head)：
- `head[0]`: bit[4:1] = msg_id, bit[7:5] = power_role, bit[0] = msg_type(0=ctrl, 1=data)
- `head[1]`: bit[7:3] = rev, bit[2:0] = msg_type_flag
- `head[2]`: CMD type
- `head[3]`: data length (仅 data message)

**控制消息类型** (data_len == 0)：
PING(0), ACK(1), NCK(2), ACCEPT(3), SOFT_RESET(4), POWER_READY(5), GET_OUTPUT_CAP(6), GET_SOURCE_INFO(7), GET_SINK_INFO(8), GET_CABLE_INFO(9), GET_DEVICE_INFO(10), GET_ERROR_INFO(11), DETECT_CABLE_INFO(12), START_CABLE_DETECT(13), END_CABLE_DETECT(14), EXIT_UFCS_MODE(15)

**数据消息类型** (data_len > 0)：
OUTPUT_CAP(1), REQUEST(2), SOURCE_INFO(3), SINK_INFO(4), CABLE_INFO(5), DEVICE_INFO(6), ERROR_INFO(7), CONFIG_WATCHDOG(8), REFUSE(9), Verify_Request(10), Verify_Response(11), Test_Request(255)

**CRC8**：多项式 `0x29`，对除最后一个字节外的所有字节计算。

**SOP 检测**：`0xAA` 作为包起始标记，收到后重置状态。

#### 2.3.6 C 实现关键设计

**私有状态结构**：
```c
#define UFCS_MAX_PKT 136  // 4 header + 128 data + 1 crc + 3 margin

typedef struct {
    uint64_t ss_block;
    uint64_t es_block;
    int dataidx;              // byte position in packet
    uint8_t datapkt[UFCS_MAX_PKT]; // packet buffer
    int plen;                 // expected packet length
    uint64_t bytepos_ss[UFCS_MAX_PKT]; // per-byte start sample
    uint64_t bytepos_es[UFCS_MAX_PKT]; // per-byte end sample
    char text[1024];          // full text string
    int fulltext;             // option: full text mode
    int out_ann;
} ufcs_state;
```

**recv_proto 处理逻辑**：
```c
static void ufcs_recv_proto(struct srd_decoder_inst *di,
    uint64_t ss, uint64_t es,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ufcs_state *s = (ufcs_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") != 0) return;
    if (data_len < 1) return;

    s->ss_block = ss;
    s->es_block = es;

    uint8_t val = data[0];

    // SOP detection
    if (val == 0xaa) {
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_DATA, "SOP:0xaa", "SOP");
        ufcs_reset_state(s);
        return;
    }

    // append data
    if (s->dataidx < UFCS_MAX_PKT) {
        s->datapkt[s->dataidx] = val;
        s->bytepos_ss[s->dataidx] = ss;
        s->bytepos_es[s->dataidx] = es;
    }

    // determine packet length at index 3
    if (s->dataidx == 3) {
        if ((s->datapkt[1] & 1) == 1)
            s->plen = s->datapkt[3] + 5;  // data msg
        else
            s->plen = 4;  // ctrl msg
    }

    s->dataidx++;

    // packet complete
    if (s->dataidx == s->plen && s->plen > 0) {
        ufcs_decode_pkt(di, s);
    }
}
```

**CRC8 计算**：
```c
static uint8_t ufcs_compute_crc8(uint8_t *data, int len)
{
    const uint8_t CRC_8_POLY = 0x29;
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ CRC_8_POLY;
            else
                crc = crc << 1;
        }
    }
    return crc & 0xFF;
}
```

---

### 2.4 amulet_ascii_c — Amulet LCD ASCII 控制协议

#### 2.4.1 Python 元数据提取

| 属性 | Python 值 | C 映射 |
|------|-----------|--------|
| id | `amulet_ascii` | `amulet_ascii_c` |
| name | `Amulet ASCII` | `Amulet ASCII(C)` |
| longname | `Amulet LCD ASCII` | `Amulet LCD ASCII (C)` |
| desc | `Amulet Technologies LCD controller ASCII protocol.` | 同上 + `(C implementation)` |
| license | `gplv3+` | `gplv3+` |
| inputs | `['uart']` | `{"uart"}` |
| outputs | `[]` | `NULL` |
| tags | `['Display']` | `{"Display"}` |

#### 2.4.2 Options

| Option ID | 类型 | 默认值 | 描述 | values |
|-----------|------|--------|------|--------|
| `ms_chan` | string | `"RX"` | Master→Slave 通道 | `{"RX", "TX"}` |
| `sm_chan` | string | `"TX"` | Slave→Master 通道 | `{"RX", "TX"}` |

#### 2.4.3 Annotations

Python 版本通过 `lists.py` 中的 `cmds` OrderedDict 动态生成 annotation classes。共 30 个命令 + 3 个通用类型 = 33 个 annotations：

**命令 annotations**（从 lists.py cmds OrderedDict 提取）：

| 枚举值 | 命令码 | ID | 描述 |
|--------|--------|-----|------|
| ANN_PAGE = 0 | 0xA0 | page | Jump to page |
| ANN_GBV = 1 | 0xD0 | gbv | Get byte variable |
| ANN_GWV = 2 | 0xD1 | gwv | Get word variable |
| ANN_GSV = 3 | 0xD2 | gsv | Get string variable |
| ANN_GLV = 4 | 0xD3 | glv | Get label variable |
| ANN_GRPC = 5 | 0xD4 | grpc | Get RPC buffer |
| ANN_SBV = 6 | 0xD5 | sbv | Set byte variable |
| ANN_SWV = 7 | 0xD6 | swv | Set word variable |
| ANN_SSV = 8 | 0xD7 | ssv | Set string variable |
| ANN_RPC = 9 | 0xD8 | rpc | Invoke RPC |
| ANN_LINE = 10 | 0xD9 | line | Draw line |
| ANN_RECT = 11 | 0xDA | rect | Draw rectangle |
| ANN_FRECT = 12 | 0xDB | frect | Draw filled rectangle |
| ANN_PIXEL = 13 | 0xDC | pixel | Draw pixel |
| ANN_GBVA = 14 | 0xDD | gbva | Get byte variable array |
| ANN_GWVA = 15 | 0xDE | gwva | Get word variable array |
| ANN_SBVA = 16 | 0xDF | sbva | Set byte variable array |
| ANN_GBVR = 17 | 0xE0 | gbvr | Get byte variable reply |
| ANN_GWVR = 18 | 0xE1 | gwvr | Get word variable reply |
| ANN_GSVR = 19 | 0xE2 | gsvr | Get string variable reply |
| ANN_GLVR = 20 | 0xE3 | glvr | Get label variable reply |
| ANN_GRPCR = 21 | 0xE4 | grpcr | Get RPC buffer reply |
| ANN_SBVR = 22 | 0xE5 | sbvr | Set byte variable reply |
| ANN_SWVR = 23 | 0xE6 | swvr | Set word variable reply |
| ANN_SSVR = 24 | 0xE7 | ssvr | Set string variable reply |
| ANN_RPCR = 25 | 0xE8 | rpcr | Invoke RPC reply |
| ANN_LINER = 26 | 0xE9 | liner | Draw line reply |
| ANN_RECTR = 27 | 0xEA | rectr | Draw rectangle reply |
| ANN_FRECTR = 28 | 0xEB | frectr | Draw filled rectangle reply |
| ANN_PIXELR = 29 | 0xEC | pixelr | Draw pixel reply |
| ANN_GBVAR = 30 | 0xED | gbvar | Get byte variable array reply |
| ANN_GWVAR = 31 | 0xEE | gwvar | Get word variable array reply |
| ANN_SBVAR = 32 | 0xEF | sbvar | Set byte variable array reply |
| ANN_ACK = 33 | 0xF0 | ack | Acknowledgment |
| ANN_NACK = 34 | 0xF1 | nack | Negative acknowledgment |
| ANN_SWVA = 35 | 0xF2 | swva | Set word variable array |
| ANN_SWVAR = 36 | 0xF3 | swvar | Set word variable array reply |
| ANN_GCV = 37 | 0xF4 | gcv | Get color variable |
| ANN_GCVR = 38 | 0xF5 | gcvr | Get color variable reply |
| ANN_SCV = 39 | 0xF6 | scv | Set color variable |
| ANN_SCVR = 40 | 0xF7 | scvr | Set color variable reply |

**通用 annotations**：

| 枚举值 | ID | 标签 |
|--------|-----|------|
| ANN_BIT = 41 | bit | `"", "Bit"` |
| ANN_FIELD = 42 | field | `"", "Field"` |
| ANN_WARN = 43 | warning | `"", "Warning"` |

**NUM_ANN = 44** (L=41 个命令 + BIT + FIELD + WARN = 44)

**L = 41** (cmds dict 中的条目数)

#### 2.4.4 Annotation Rows

| Row ID | 描述 | 包含的 ann classes |
|--------|------|-------------------|
| bits | Bits | (ANN_BIT,) = (41,) |
| fields | Fields | (ANN_FIELD,) = (42,) |
| commands | Commands | (0..40) = 41 个命令 |
| warnings | Warnings | (ANN_WARN,) = (43,) |

#### 2.4.5 协议解析逻辑

Amulet ASCII 协议是**命令驱动的状态机**。每个命令由命令字节（0xA0, 0xD0-0xF7）触发，然后根据命令类型解析后续字节。

**命令字节映射**（lists.py 中的 cmds dict）：

```c
// 命令码 → (短名称, 描述) 映射
static const struct {
    uint8_t code;
    const char *shortname;
    const char *desc;
} amulet_cmds[] = {
    {0xA0, "PAGE", "Jump to page"},
    {0xD0, "GBV", "Get byte variable"},
    {0xD1, "GWV", "Get word variable"},
    // ... 完整 41 条
    {0xF7, "SCVR", "Set color variable reply"},
};
```

**状态机核心**：
1. 无状态时，收到命令字节 → 设置 `state = 命令码`，`cmdstate = 1`
2. 后续字节根据 `state` 分发到对应 handler
3. handler 完成后重置 `state = None`
4. 若当前处于命令解析中收到 0xD0-0xF7 范围的新命令字节（且不在 `cmds_with_high_bytes` 中），则中止当前命令

**cmds_with_high_bytes**（允许包含高字节的命令）：
`0xA0 (PAGE)`, `0xD7 (SSV)`, `0xE7 (SSVR)`, `0xE2 (GSVR)`, `0xE3 (GLVR)`

#### 2.4.6 C 实现关键设计

**这是最复杂的解码器**，因为 Python 版本使用了大量动态方法分发（`getattr(self, 'handle_xxx')`）。C 版本需要用 switch-case 或函数指针表替代。

**私有状态结构**：
```c
typedef struct {
    uint8_t state;           // current command byte (0 = idle)
    int cmdstate;            // byte index within current command
    uint64_t ss;             // current byte start sample
    uint64_t es;             // current byte end sample
    uint64_t ss_cmd;         // command start sample
    uint64_t es_cmd;         // command end sample
    uint64_t ss_field;       // field start sample
    uint64_t es_field;       // field end sample
    int addr;                // parsed address
    int value;               // parsed value
    char str_val[256];       // string value buffer
    int checksum;            // checksum accumulator
    uint8_t page[2];         // page index bytes
    int flags;               // RPC flags
    int coords[4];           // drawing coordinates
    int ms_chan;             // master->slave channel (0=RX, 1=TX)
    int sm_chan;             // slave->master channel (0=RX, 1=TX)
    int out_ann;
} amulet_state;
```

**recv_proto 处理**：
```c
static void amulet_recv_proto(struct srd_decoder_inst *di,
    uint64_t ss, uint64_t es,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    amulet_state *s = (amulet_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") != 0) return;
    if (data_len < 1) return;

    s->ss = ss;
    s->es = es;

    uint8_t pdata = data[0];

    // Check for command abort by high byte
    int abort_current = (pdata >= 0xD0 && pdata <= 0xF7) &&
                        !amulet_is_high_byte_cmd(s->state) &&
                        s->state != 0;

    if (abort_current) {
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_WARN,
                  "Command aborted by invalid byte", "Abort");
        s->state = pdata;
        amulet_emit_cmd_byte(di, s);
        s->cmdstate = 1;
    }

    if (s->state == 0) {
        s->state = pdata;
        amulet_emit_cmd_byte(di, s);
        s->cmdstate = 1;
    }

    // Dispatch to command handler
    amulet_handle_command(di, s, pdata);
}
```

**命令处理函数指针表**：
```c
typedef void (*amulet_cmd_handler)(struct srd_decoder_inst *di, amulet_state *s, uint8_t pdata);

// 按 cmdstate 分阶段处理，每个命令一个 handler
static void amulet_handle_command(struct srd_decoder_inst *di, amulet_state *s, uint8_t pdata)
{
    switch (s->state) {
    case 0xA0: amulet_handle_page(di, s, pdata); break;
    case 0xD0: amulet_handle_gbv(di, s, pdata); break;
    case 0xD1: amulet_handle_gwv(di, s, pdata); break;
    // ... 所有 41 个命令
    case 0xF0: amulet_handle_ack(di, s, pdata); break;
    case 0xF1: amulet_handle_nack(di, s, pdata); break;
    default:
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_WARN,
                  "Unknown command", "Unknown");
        s->state = 0;
        break;
    }
}
```

**简化策略**：由于 Python 版本中许多 handler 逻辑相似（read 类、set_common 类、string 类），C 版本可以提取公共函数：
- `amulet_handle_read()` — GBV/GWV/GSV/GLV/GRPC/GBVA/GWVA/GCV/RPC 的公共读取逻辑
- `amulet_handle_set_common()` — SBV/SWV/SBVA/SWVA 的公共设置逻辑
- `amulet_handle_string()` — SSV/GSVR/GLVR/SSVR 的公共字符串处理

---

### 2.5 streletz_c — Streletz 安防系统串行协议

#### 2.5.1 Python 元数据提取

| 属性 | Python 值 | C 映射 |
|------|-----------|--------|
| id | `streletz` | `streletz_c` |
| name | `Streletz` | `Streletz(C)` |
| longname | `Streletz RS232 (Serial bus)` | `Streletz RS232 Serial Bus (C)` |
| desc | `Serial bus for guard system Streletz` | 同上 + `(C implementation)` |
| license | `mit` | `mit` |
| inputs | `['uart']` | `{"uart"}` |
| outputs | `['streletz']` | `{"streletz"}` |
| tags | `['Embedded/industrial']` | `{"Embedded/industrial"}` |

#### 2.5.2 Options

| Option ID | 类型 | 默认值 | 描述 | values |
|-----------|------|--------|------|--------|
| `header_tx` | int | 217 (0xD9) | 请求头字节 | - |
| `header_rx` | int | 157 (0x9D) | 响应头字节 | - |

#### 2.5.3 Annotations

| 枚举值 | ID | 标签 |
|--------|-----|------|
| ANN_HEADER = 0 | head | `"", "Header"` |
| ANN_DATASIZE = 1 | datasize | `"", "Data Size"` |
| ANN_CHECKSUM = 2 | checksum | `"", "Checksum"` |
| ANN_ANSWER = 3 | answer | `"", "Answer"` |
| ANN_COMMAND = 4 | command | `"", "Command"` |
| ANN_DATA_RX = 5 | rx-data | `"", "RX Data"` |
| ANN_DATA_TX = 6 | tx-data | `"", "TX Data"` |
| ANN_PACKET_RX = 7 | rx-packet | `"", "RX packet"` |
| ANN_PACKET_TX = 8 | tx-packet | `"", "TX packet"` |
| ANN_WARN = 9 | warning | `"", "Warning"` |

**NUM_ANN = 10**

#### 2.5.4 Annotation Rows

| Row ID | 描述 | 包含的 ann classes |
|--------|------|-------------------|
| framing | Framing | HEADER, DATASIZE, CHECKSUM |
| data | Data | ANSWER, COMMAND, DATA_RX, DATA_TX |
| warnings | Warnings | WARN |
| packets | Packets | PACKET_RX, PACKET_TX |

#### 2.5.5 协议解析逻辑

Streletz 包格式：

```
[Header] [DataSize] [DataType] [Data0..DataN] [Checksum]
```

- **Header**: TX=0xD9(217), RX=0x9D(157)（可配置）
- **DataSize**: 数据字节数 N，包总长 = 4 + N
- **DataType**: 命令码(TX) 或 应答码(RX)
- **Data**: N 字节数据
- **Checksum**: 全包 XOR 结果应为 0

**包长度**：`PACKETSIZE_MIN = 4`，`PACKETSIZE_MAX = 64`

**状态机**（`buf_pos` 驱动）：
- `buf_pos < 1`: 等待 header 匹配
- `buf_pos == 1`: 已收到 header
- `buf_pos == 2 (DATA_SIZE)`: 解析数据长度
- `buf_pos == 3 (DATA_TYPE)`: 解析数据类型（命令/应答）
- `buf_pos == 4 (DATA_START)`: 数据开始
- `buf_pos == packet_size`: 校验和字节

**Python 版本使用 `FRAME` 事件**（不是 `DATA`），即 `ptype == 'FRAME'` 时处理 `data_value`。

#### 2.5.6 C 实现关键设计

**私有状态结构**：
```c
#define STRELETZ_MAX_PKT 64
#define STRELETZ_MIN_PKT 4

typedef struct {
    uint8_t accum_bytes[STRELETZ_MAX_PKT];
    int accum_count;
    int rxtx;                  // 0=RX, 1=TX
    int packet_size;           // expected total packet size
    uint64_t packet_ss;
    uint64_t packet_es;
    uint64_t data_ss;
    uint64_t data_es;
    int buf_pos;               // current position in packet
    uint8_t checksum;
    uint8_t header_rx;         // response header byte
    uint8_t header_tx;         // request header byte
    int out_ann;
} streletz_state;
```

**recv_proto 处理逻辑**：
```c
static void streletz_recv_proto(struct srd_decoder_inst *di,
    uint64_t ss, uint64_t es,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    streletz_state *s = (streletz_state *)c_decoder_get_private(di);
    if (!s) return;

    // Python 版本只处理 FRAME 事件
    if (strcmp(cmd, "FRAME") != 0) return;
    if (data_len < 2) return;

    uint8_t byte_val = data[0];
    int rxtx = data[1];

    streletz_handle_byte(di, s, ss, es, byte_val, rxtx);
}
```

**handle_byte 核心逻辑**：
```c
static void streletz_handle_byte(struct srd_decoder_inst *di, streletz_state *s,
    uint64_t ss, uint64_t es, uint8_t byte_val, int rxtx)
{
    if (s->buf_pos > 0) {
        if (s->rxtx == rxtx) {
            s->buf_pos++;
            if (s->accum_count < STRELETZ_MAX_PKT)
                s->accum_bytes[s->accum_count++] = byte_val;
            s->checksum ^= byte_val;
        } else {
            streletz_reset_state(s);
        }
    }

    // Wait for header
    if (s->buf_pos < 1) {
        uint8_t expected = (rxtx == 1) ? s->header_tx : s->header_rx;
        if (byte_val == expected) {
            s->buf_pos = 1;
            s->rxtx = rxtx;
            s->accum_bytes[0] = byte_val;
            s->accum_count = 1;
            s->packet_ss = ss;
            s->checksum = byte_val;
            char hdr_str[16];
            snprintf(hdr_str, sizeof(hdr_str), "HEAD: 0x%02X", byte_val);
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_HEADER, hdr_str, "HEAD", "H");
        }
        return;
    }

    // Data size
    if (s->buf_pos == 2) {
        s->packet_size = STRELETZ_MIN_PKT + byte_val;
        if (s->packet_size > STRELETZ_MAX_PKT) {
            char warn_str[32];
            snprintf(warn_str, sizeof(warn_str), "Wrong DS: 0x%02X", byte_val);
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARN, warn_str, "WDS");
            streletz_reset_state(s);
            return;
        }
        char ds_str[16];
        snprintf(ds_str, sizeof(ds_str), "DS: 0x%02X", byte_val);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_DATASIZE, ds_str, "DS");
    }
    // Data type
    else if (s->buf_pos == 3) {
        if (rxtx == 1) {
            char cmd_str[16];
            snprintf(cmd_str, sizeof(cmd_str), "CMD: 0x%02X", byte_val);
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_COMMAND, cmd_str, "CMD");
        } else {
            char ans_str[16];
            snprintf(ans_str, sizeof(ans_str), "ANS: 0x%02X", byte_val);
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_ANSWER, ans_str, "ANS");
        }
    }
    // Data start
    else if (s->buf_pos == 4) {
        s->data_ss = ss;
    }

    // End of data block
    if (s->buf_pos == s->packet_size - 1) {
        s->data_es = es;
    }

    // Checksum byte: end of packet
    if (s->buf_pos == s->packet_size) {
        // ... checksum verification and packet annotation
        streletz_reset_state(s);
    }
}
```

---

## 3. 通用 C 解码器模板

### 3.1 文件命名

`libsigrokdecode/c_decoders/<decoder_id>_c.c`

### 3.2 标准 struct srd_c_decoder 布局

```c
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    .longname = "Full Protocol Name (C)",
    .desc = "Protocol description (C implementation)",
    .license = "gplv2+",     // 或 "mit"
    .channels = NULL,         // UART 上层解码器无直接 channel
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = xxx_options,
    .num_options = N,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = M,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,     // {"uart"}
    .num_inputs = 1,
    .outputs = xxx_outputs,   // 可能 NULL
    .num_outputs = 0/1,
    .binary = NULL,
    .num_binary = 0,
    .tags = xxx_tags,
    .num_tags = 1,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,     // 空函数或 NULL
    .destroy = xxx_destroy,
    .recv_proto = xxx_recv_proto,  // ★ 核心回调
};
```

### 3.3 recv_proto 签名

```c
static void xxx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

### 3.4 decode() 函数

UART 上层解码器的 `decode()` 应为空函数：

```c
static void xxx_decode(struct srd_decoder_inst *di)
{
    (void)di;
    // UART 上层解码器通过 recv_proto 接收数据，不直接 decode
}
```

### 3.5 srd_c_decoder_entry() 规范

- 所有 option 的 `def` 值在此函数中初始化
- string option 的 `values` GSList 在此构建
- 返回 `&xxx_c_decoder` 指针

### 3.6 C_ANN_PUT 使用规范

```c
// 单文本
C_ANN_PUT(di, ss, es, out_ann, ANN_CLASS, "text");

// 长/短文本
C_ANN_PUT(di, ss, es, out_ann, ANN_CLASS, "Long text", "Short");

// 长/短/极短文本
C_ANN_PUT(di, ss, es, out_ann, ANN_CLASS, "Long text", "Short", "X");
```

### 3.7 c_decoder_put_python 使用规范

```c
// 无数据
c_decoder_put_python(di, ss, es, out_python, "CMD_NAME", NULL, 0);

// 有数据
unsigned char py_data[2] = {val, rxtx};
c_decoder_put_python(di, ss, es, out_python, "DATA", py_data, 2);
```

---

## 4. CMakeLists.txt 修改

在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加：

```cmake
sbus_futaba_c
scs_c
ufcs_c
amulet_ascii_c
streletz_c
```

---

## 5. 风险与注意事项

### 5.1 UART DATA vs FRAME 语义差异

- Python 版本中，`decode()` 收到的 `data` 是 `(ptype, rxtx, pdata)` 三元组
- C 版本中，`recv_proto` 收到的 `cmd` 是字符串，`data` 是原始字节
- **sbus_futaba**: 使用 DATA + FRAME + IDLE + BREAK 四种事件 <!-- Updated: 原标注"⚠️ C版本uart_c.c暂不输出IDLE/BREAK"已过时，uart_c.c第406/414行已实现IDLE/BREAK输出 -->
- **scs**: 仅使用 DATA
- **ufcs**: 仅使用 DATA
- **amulet_ascii**: 仅使用 DATA
- **streletz**: 仅使用 FRAME

### 5.2 Python bits vs C bytes

- Python UART 解码器的 DATA 输出包含 `bits` 数组（每个数据位的值）
- C UART 解码器的 DATA 输出仅包含 `byte_val + rxtx`（不含 databits 逐位数组）
- **sbus_futaba** 需要逐 bit 处理（11-bit 通道值），C 版本需从字节重建 bits
- **C 版本 UART DATA 输出不含 databits 的限制**：所有从字节重建的 bits 共享同一 ss/es 区间，无法实现 Python 版本中逐位的精确采样位置标注
- <!-- Updated: IDLE/BREAK已由uart_c.c实现，sbus_futaba_c的IDLE/BREAK处理分支可正常触发。但DATA的逐位采样位置限制仍然存在 -->

### 5.3 amulet_ascii 复杂度

- 41 个命令处理器，大量状态机逻辑
- Python 使用动态方法分发，C 需要静态 switch-case
- 建议分阶段实现：先实现核心命令（PAGE/GBV/SBV/ACK/NACK），再逐步添加其余

### 5.4 UFCS 数据消息解析

- Python 版本中 `get_dword()`/`get_short()` 等函数有 bug（变量名错误如 `emk`→`d`）
- C 版本应修正这些 bug，但保持输出格式一致
