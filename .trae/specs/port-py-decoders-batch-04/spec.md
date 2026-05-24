# Python 解码器移植到 C — Batch 04 规格说明书

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |
| uart_c.c | 底层协议输出范本 | c_decoder_put_python()输出协议数据、双通道(RX/TX)独立状态 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |

## 概述

本文档是将 5 个 Python 协议解码器移植为 C 解码器的详细规格。每个解码器都包含完整的 Python 源码分析、C 实现计划、状态机细节和关键注意事项。

**注意**：`sent` Python 解码器不存在于代码库中，已替换为 `modbus`（TIER 1 工业协议解码器）。

### 移植清单

| # | 解码器 | Python 路径 | C 输出路径 | 复杂度 |
|---|--------|-------------|-----------|--------|
| 1 | TMC | `libsigrokdecode/decoders/tmc/pd.py` | `libsigrokdecode/c_decoders/tmc_c.c` | 中等 |
| 2 | Modbus | `libsigrokdecode/decoders/modbus/pd.py` | `libsigrokdecode/c_decoders/modbus_c.c` | 高 |
| 3 | SLE44xx | `libsigrokdecode/decoders/sle44xx/pd.py` | `libsigrokdecode/c_decoders/sle44xx_c.c` | 中高 |
| 4 | PJDL | `libsigrokdecode/decoders/pjdl/pd.py` | `libsigrokdecode/c_decoders/pjdl_c.c` | 高 |
| 5 | OneWire Link | `libsigrokdecode/decoders/onewire_link/pd.py` | `libsigrokdecode/c_decoders/onewire_link_c.c` | 中等 |

---

## 1. TMC (Titan Micro Circuit) 解码器

### 1.1 Python 解码器元数据

```python
id = "tmc"
name = "TMC"
longname = "Titan Micro Circuit"
desc = "Bus for TM1636/37/38 7-segment digital tubes."
license = "gplv2+"
inputs = ["logic"]
outputs = ["tmc"]
tags = ['Embedded/industrial']
```

### 1.2 通道定义

| 索引 | id | name | desc | 类型 |
|------|-----|------|------|------|
| 0 | clk | CLK | Clock line | 必需 |
| 1 | dio | DIO | Data line | 必需 |
| 2 | stb | STB | Strobe line | 可选 |

### 1.3 选项定义

| id | desc | default | values |
|----|------|---------|--------|
| radix | Number format | "Hex" | ("Hex", "Dec", "Oct", "Bin") |

### 1.4 注解定义

**AnnProtocol 枚举：**
| 值 | id | 标签文本 |
|----|-----|---------|
| 0 | START | ["Start", "S"] |
| 1 | STOP | ["Stop", "P"] |
| 2 | ACK | ["ACK", "A"] |
| 3 | NACK | ["NACK", "N"] |
| 4 | COMMAND | ["Command", "Cmd", "C"] |
| 5 | DATA | ["Data", "D"] |
| 6 | BIT | ["Bit", "B"] |

**AnnInfo 枚举：**
| 值 | id | 标签文本 |
|----|-----|---------|
| 7 | WARN | ["Warnings", "Warn", "W"] |

**注解行：**
| id | label | 包含的注解类 |
|----|-------|-------------|
| bits | Bits | (6,) |
| data | Cmd/Data | (0,1,2,3,4,5) |
| warnings | Warnings | (7,) |

**二进制输出：**
| id | desc |
|----|------|
| DATA | "Data", "D" |

### 1.5 Python 输出格式

OUTPUT_PYTHON 格式：
- `["START", None]` — START 条件
- `["COMMAND", byte_value]` — 命令字节
- `["DATA", byte_value]` — 数据字节
- `["STOP", None]` — STOP 条件
- `["ACK", None]` — ACK 位
- `["NACK", None]` — NACK 位
- `["BITS", bits_list]` — 位列表，每个元素为 [bit_value, ss, es]

OUTPUT_META: 输出 bitrate (int 类型)

<!-- Updated: C实现使用c_decoder_register_output_meta()注册META输出，使用c_decoder_put_meta_int()输出bitrate值 -->
```c
// 注册META输出（在start()中）
s->out_bitrate = c_decoder_register_output_meta(di, SRD_OUTPUT_META, "tmc", "int", "Bitrate", "Bitrate from Start bit to Stop bit");

// 输出bitrate（在handle_bitrate()中）
c_decoder_put_meta_int(di, s->ss_byte, samplenum, s->out_bitrate, bitrate);
```

### 1.6 状态机分析

**总线类型：**
- `WIRE2`（2线模式）：CLK + DIO，无 STB。START 条件为 CLK=高时 DIO 下降沿。STOP 条件为 CLK=高时 DIO 上升沿。
- `WIRE3`（3线模式）：CLK + DIO + STB。START 条件为 STB 下降沿。STOP 条件为 STB 上升沿。

**状态：**
1. `FIND START` — 等待 START 条件
   - 条件 0: `{CLK:'h', STB:'f'}` → wire3
   - 条件 1: `{CLK:'l', STB:'f'}` → wire3
   - 条件 2: `{CLK:'h', DIO:'f'}` → wire2
   - 匹配条件 0 或 1 → wire3 模式，调用 handle_start
   - 匹配条件 2 → wire2 模式，调用 handle_start

2. `FIND DATA` — 等待数据位或 STOP 条件
   - 条件 0: `{STB:'r'}` → STOP (wire3)
   - 条件 1: `{CLK:'h', DIO:'r'}` → STOP (wire2)
   - 条件 2: `{CLK:'r'}` → CLK 上升沿，调用 handle_data

3. `FIND ACK` — 等待 ACK/NACK 位（仅 wire2）
   - 条件: `{CLK:'f'}` → CLK 下降沿，调用 handle_ack

4. `FIND STOP` — 等待 STOP 条件（未在代码中实际到达，因为 FIND DATA 中已处理 STOP）

**Wire2 数据处理流程：**
- 每个 CLK 上升沿采样 DIO 位
- LSB-first 传输，8 位组成一个字节
- 第 9 个 CLK 脉冲为 ACK/NACK
- databyte 通过 `>>= 1` 和 `|= (dio << 7)` 构建
- 第一个字节为 COMMAND，后续为 DATA
- 位注解在处理下一个位时输出前一个位

**Wire3 数据处理流程：**
- 每个 CLK 上升沿采样 DIO 位
- 8 位组成一个字节后，在 STB 上升沿或下一个 CLK 上升沿时输出
- 无 ACK/NACK 机制
- handle_byte_wire3() 输出所有位和字节注解

**Bitrate 计算：**
- 从 pdu_start 到当前 samplenum 的时间
- `bitrate = pdu_bits / elapsed`
- 通过 OUTPUT_META 输出

### 1.7 C 实现计划

**通道索引常量：**
```c
#define CH_CLK 0
#define CH_DIO 1
#define CH_STB 2
```

**注解枚举：**
```c
enum tmc_ann {
    ANN_START = 0,
    ANN_STOP,
    ANN_ACK,
    ANN_NACK,
    ANN_COMMAND,
    ANN_DATA,
    ANN_BIT,
    ANN_WARN,
    NUM_ANN
};
```

**私有数据结构：**
```c
struct tmc_priv {
    int state;           // 状态机状态
    int bustype;         // 0=WIRE2, 1=WIRE3
    int bitcount;        // 当前位计数
    uint8_t databyte;    // 正在组装的数据字节
    uint64_t ss_byte;    // 当前字节起始样本
    uint64_t ss_ack;     // ACK 位起始样本
    uint64_t ss;         // 当前注解起始
    uint64_t es;         // 当前注解结束
    uint64_t pdu_start;  // PDU 起始样本
    int pdu_bits;        // PDU 位计数
    int bytecount;       // 字节计数
    int radix;           // 0=Hex, 1=Dec, 2=Oct, 3=Bin
    int out_ann;
    int out_python;
    int out_binary;
    int out_bitrate;
    // 位记录数组（用于 BITS 输出）
    struct { int val; uint64_t ss; uint64_t es; } bits[8];
};
```

**状态枚举：**
```c
enum tmc_state {
    STATE_FIND_START,
    STATE_FIND_DATA,
    STATE_FIND_ACK,
    STATE_FIND_STOP
};
```

**关键函数签名：**
```c
static void tmc_reset(struct srd_decoder_inst *di);
static void tmc_start(struct srd_decoder_inst *di);
static void tmc_decode(struct srd_decoder_inst *di);
static void tmc_destroy(struct srd_decoder_inst *di);
static void tmc_handle_start(struct srd_decoder_inst *di, uint64_t samplenum);
static void tmc_handle_data(struct srd_decoder_inst *di, uint64_t samplenum, uint64_t matched);
static void tmc_handle_data_wire2(struct srd_decoder_inst *di, uint64_t samplenum);
static void tmc_handle_data_wire3(struct srd_decoder_inst *di, uint64_t samplenum);
static void tmc_handle_ack(struct srd_decoder_inst *di, uint64_t samplenum);
static void tmc_handle_stop(struct srd_decoder_inst *di, uint64_t samplenum);
static void tmc_handle_bitrate(struct srd_decoder_inst *di, uint64_t samplenum);
```

### 1.8 关键实现注意事项

1. **总线类型自动检测**：在 FIND START 状态通过匹配条件自动判断 wire2/wire3
2. **LSB-first 传输**：databyte 通过右移和或运算构建，与 I2C 的 MSB-first 不同
3. **Wire2 的 ACK**：第 9 个 CLK 脉冲，在 CLK 下降沿读取 ACK/NACK
4. **Wire3 无 ACK**：直接在 STOP 条件时输出最后一个字节
5. **位注解延迟输出**：前一个位的 es 在处理当前位时才确定
6. **需要 samplerate**：用于 bitrate 计算，需要在 metadata 回调中获取
7. **radix 选项**：需要格式化数据值为 Hex/Dec/Oct/Bin
8. **Python 输出**：需要 `c_decoder_put_python` 输出 START/COMMAND/DATA/STOP/ACK/NACK/BITS
9. **binary 输出**：每个数据/命令字节输出为 1 字节 binary

---

## 2. Modbus RTU 解码器

### 2.1 Python 解码器元数据

```python
id = 'modbus'
name = 'Modbus'
longname = 'Modbus RTU over RS232/RS485'
desc = 'Modbus RTU protocol for industrial applications.'
license = 'gplv3+'
inputs = ['uart']
outputs = ['modbus']
tags = ['Embedded/industrial']
```

### 2.2 通道定义

**无直接通道** — 输入来自 UART 解码器的 Python 输出。

### 2.3 选项定义

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| scchannel | Server -> client channel | 'RX' | ('RX', 'TX') | dec_modbus_opt_scchannel |
| cschannel | Client -> server channel | 'TX' | ('RX', 'TX') | dec_modbus_opt_cschannel |
| framegap | Inter-frame bit gap | 28 | (整数) | dec_modbus_opt_framegap |

### 2.4 注解定义

| 索引 | id | 标签 |
|------|-----|------|
| 0 | sc-server-id | SC server ID |
| 1 | sc-function | SC function |
| 2 | sc-crc | SC CRC |
| 3 | sc-address | SC address |
| 4 | sc-data | SC data |
| 5 | sc-length | SC length |
| 6 | sc-error | SC error |
| 7 | cs-server-id | CS server ID |
| 8 | cs-function | CS function |
| 9 | cs-crc | CS CRC |
| 10 | cs-address | CS address |
| 11 | cs-data | CS data |
| 12 | cs-length | CS length |
| 13 | cs-error | CS error |
| 14 | error-indication | Error indication |

**注解行：**
| id | label | 包含的注解类 |
|----|-------|-------------|
| sc | Server->client | (0,1,2,3,4,5,6) |
| cs | Client->server | (7,8,9,10,11,12,13) |
| error-indicators | Errors in frame | (14,) |

### 2.5 Python 输入格式

从 UART 解码器接收数据格式：`[ptype, rxtx, pdata]`
- ptype: 'STARTBIT', 'DATA', 'STOPBIT'
- rxtx: 0=RX, 1=TX
- pdata: 字节数据

### 2.6 核心逻辑分析

**Modbus ADU 结构：**
- Server->Client (SC): `[server_id, function, ...data..., CRC_lo, CRC_hi]`
- Client->Server (CS): `[server_id, function, ...data..., CRC_lo, CRC_hi]`

**帧间隔检测：**
- 使用 bitlength * framegap 来判断帧间间隔
- bitlength 从 STARTBIT 或 STOPBIT 的持续时间推导
- 超过间隔时间则关闭当前 ADU，开始新帧

**CRC-16 计算（Modbus CRC）：**
```python
result = 0xFFFF
magic_number = 0xA001
for byte in data:
    result ^= byte
    for i in range(8):
        LSB = result & 1
        result >>= 1
        if LSB:
            result ^= 0xA001
return (result & 0xFF, (result >> 8) & 0xFF)
```

**支持的 Modbus 功能码：**

| 功能码 | 名称 | 方向 | 最小长度 |
|--------|------|------|---------|
| 1 | Read Coils | CS/SC | 8 |
| 2 | Read Discrete Inputs | CS/SC | 8 |
| 3 | Read Holding Registers | CS/SC | 8 |
| 4 | Read Input Registers | CS/SC | 8 |
| 5 | Write Single Coil | CS/SC | 8 |
| 6 | Write Single Register | CS/SC | 8 |
| 7 | Read Exception Status | CS/SC | 5 |
| 8 | Diagnostics | CS/SC | 8 |
| 11 | Get Comm Event Counter | CS/SC | 8 |
| 12 | Get Comm Event Log | CS/SC | 11 |
| 15 | Write Multiple Coils | CS/SC | 9 |
| 16 | Write Multiple Registers | CS/SC | 9 |
| 17 | Report Server ID | CS/SC | 7 |
| 22 | Mask Write Register | CS/SC | 10 |
| 23 | Read/Write Multiple Registers | CS | 13 |
| 0x80+ | Error Response | SC | 5 |

**错误码：**
| 代码 | 描述 |
|------|------|
| 1 | Illegal Function |
| 2 | Illegal Data Address |
| 3 | Illegal Data Value |
| 4 | Slave Device Failure |
| 5 | Acknowledge |
| 6 | Slave Device Busy |
| 8 | Memory Parity Error |
| 10 | Gateway Path Unavailable |
| 11 | Gateway Target Device failed to respond |

### 2.7 C 实现计划

**注解枚举：**
```c
enum modbus_ann {
    ANN_SC_SERVER_ID = 0,
    ANN_SC_FUNCTION,
    ANN_SC_CRC,
    ANN_SC_ADDRESS,
    ANN_SC_DATA,
    ANN_SC_LENGTH,
    ANN_SC_ERROR,
    ANN_CS_SERVER_ID,
    ANN_CS_FUNCTION,
    ANN_CS_CRC,
    ANN_CS_ADDRESS,
    ANN_CS_DATA,
    ANN_CS_LENGTH,
    ANN_CS_ERROR,
    ANN_ERROR_INDICATION,
    NUM_ANN
};
```

**私有数据结构：**
```c
#define MODBUS_MAX_DATA 260

struct modbus_adu {
    int active;            // ADU 是否活跃
    int start_new_frame;   // 是否需要开始新帧
    int has_error;         // 帧是否包含错误
    int last_byte_put;     // 最后注解的字节索引
    int minimum_length;    // 最小帧长度
    uint64_t start_ss;     // 帧起始样本
    uint64_t last_read;    // 最后读取样本
    int data_count;        // 数据字节数
    struct {
        uint8_t data;
        uint64_t start;
        uint64_t end;
    } bytes[MODBUS_MAX_DATA];
    int prefix;            // 0=sc, 1=cs
};

struct modbus_priv {
    int sc_channel;        // SC 通道 (0=RX, 1=TX)
    int cs_channel;        // CS 通道 (0=RX, 1=TX)
    int framegap;          // 帧间隔位数
    uint64_t bitlength;    // 位长度（样本数）
    int bitlength_known;   // bitlength 是否已知
    struct modbus_adu adu_sc;
    struct modbus_adu adu_cs;
    int out_ann;
};
```

**关键函数签名：**
```c
static void modbus_reset(struct srd_decoder_inst *di);
static void modbus_start(struct srd_decoder_inst *di);
static void modbus_decode(struct srd_decoder_inst *di);
static void modbus_destroy(struct srd_decoder_inst *di);
static void modbus_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
// ADU 处理
static void modbus_adu_add_data(struct srd_decoder_inst *di,
    struct modbus_adu *adu, uint64_t ss, uint64_t es, uint8_t byte_val);
static void modbus_adu_close(struct srd_decoder_inst *di,
    struct modbus_adu *adu, uint64_t error_end);
static uint16_t modbus_calc_crc(struct modbus_adu *adu, int last_byte);
static void modbus_adu_puti(struct srd_decoder_inst *di,
    struct modbus_adu *adu, int byte_to_put, const char *ann_str, const char *message);
// SC 解析
static void modbus_parse_sc(struct srd_decoder_inst *di, struct modbus_adu *adu);
// CS 解析
static void modbus_parse_cs(struct srd_decoder_inst *di, struct modbus_adu *adu);
```

### 2.8 关键实现注意事项

1. **输入来自 UART 解码器**：这是堆叠解码器，输入不是 logic 而是 uart。C 实现需要通过 `recv_proto` 回调接收 UART 解码器的输出。
2. **双向解码**：同一数据可能同时被解析为 SC 和 CS，取决于选项配置
3. **CRC-16 Modbus**：需要精确实现，初始值 0xFFFF，多项式 0xA001
4. **帧间隔检测**：使用 bitlength * framegap，bitlength 从 STARTBIT/STOPBIT 推导
5. **puti 机制**：Python 版本使用 No_more_data 异常作为流控制，C 版本需要用返回值替代
6. **putl 机制**：输出最后一个字节，需要格式化字符串
7. **half_word**：读取 16 位值（大端序）
8. **错误处理**：帧过短、CRC 错误、未知功能码等
9. **注解前缀**：SC 使用 `sc-` 前缀，CS 使用 `cs-` 前缀，映射到不同的注解类
10. **Python 版本使用异常做流控制**：C 版本需要改为返回值检查模式

### 2.9 与 Python 版本的关键差异

1. **输入机制**：Python 版本通过 `decode(ss, es, data)` 接收 UART 输出；C 版本需要通过 `recv_proto` 回调
2. **异常流控制**：Python 用 `No_more_data` 异常控制解析流程；C 需要改为返回值
3. **字符串注解**：Python 用 `puta(start, end, ann_str, message)` 通过字符串查找注解类；C 需要直接使用注解枚举
4. **ADU 类继承**：Python 用 `Modbus_ADU_SC` 和 `Modbus_ADU_CS` 继承 `Modbus_ADU`；C 用结构体 + 函数指针或前缀参数

---

## 3. SLE44xx 解码器

### 3.1 Python 解码器元数据

```python
id = 'sle44xx'
name = 'SLE 44xx'
longname = 'SLE44xx memory card'
desc = 'SLE 4418/28/32/42 memory card serial protocol'
license = 'gplv2+'
inputs = ['logic']
outputs = []
tags = ['Memory']
```

### 3.2 通道定义

| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | rst | RST | Reset line | dec_sle44xx_chan_rst |
| 1 | clk | CLK | Clock line | dec_sle44xx_chan_clk |
| 2 | io | I/O | I/O data line | dec_sle44xx_chan_io |

### 3.3 选项定义

无选项。

### 3.4 注解定义

| 索引 | id | 标签文本 |
|------|-----|---------|
| 0 | reset_sym | Reset Symbol |
| 1 | intr_sym | Interrupt Symbol |
| 2 | start_sym | Start Symbol |
| 3 | stop_sym | Stop Symbol |
| 4 | bit_sym | Bit Symbol |
| 5 | atr_byte | ATR Byte |
| 6 | cmd_byte | Command Byte |
| 7 | out_byte | Outgoing Byte |
| 8 | proc_byte | Processing Byte |
| 9 | atr_data | ATR data |
| 10 | cmd_data | Command data |
| 11 | out_data | Outgoing data |
| 12 | proc_data | Processing data |

**注解行：**
| id | label | 包含的注解类 |
|----|-------|-------------|
| symbols | Symbols | (0,1,2,3,4) |
| fields | Fields | (5,6,7,8) |
| operations | Operations | (9,10,11,12) |

**二进制输出：**
| id | desc |
|----|------|
| bytes | Bytes |

### 3.5 状态机分析

**等待条件（9 个条件）：**

| 索引 | 名称 | 条件 | 含义 |
|------|------|------|------|
| 0 | COND_RESET_START | `{RST:'r'}` | RST 上升沿 |
| 1 | COND_RESET_STOP | `{RST:'f'}` | RST 下降沿 |
| 2 | COND_RSTCLK_START | `{RST:'h', CLK:'r'}` | RST 高时 CLK 上升沿 |
| 3 | COND_RSTCLK_STOP | `{RST:'h', CLK:'f'}` | RST 高时 CLK 下降沿 |
| 4 | COND_DATA_START | `{RST:'l', CLK:'r'}` | RST 低时 CLK 上升沿 |
| 5 | COND_DATA_STOP | `{RST:'l', CLK:'f'}` | RST 低时 CLK 下降沿 |
| 6 | COND_CMD_START | `{CLK:'h', IO:'f'}` | CLK 高时 IO 下降沿（START） |
| 7 | COND_CMD_STOP | `{CLK:'h', IO:'r'}` | CLK 高时 IO 上升沿（STOP） |
| 8 | COND_PROC_IOH | `{RST:'l', IO:'r'}` | RST 低时 IO 上升沿（处理完成） |

**状态：**
- `ATR` — Answer To Reset 阶段（RST 下降后，有 CLK 脉冲）
- `CMD` — 命令阶段（3 字节：CTRL, ADDR, DATA）
- `DATA` — 数据阶段（CMD 后的第一个数据位时决定是 OUT 还是 PROC）
- `OUT` — 输出数据阶段
- `PROC` — 内部处理阶段
- `None` — 未确定状态

**复位处理：**
- RST 上升沿 → flush_queued，记录 ss_reset
- RST 下降沿 → 判断是否有 CLK 脉冲
  - 有 CLK 脉冲 → RESET（进入 ATR 状态）
  - 无 CLK 脉冲 → INTERRUPT（状态设为 None）

**ATR 处理：**
- 收集 4 个字节后 flush
- 每个字节标注为 ATR_BYTE

**命令处理：**
- START 条件（CLK 高时 IO 下降沿）→ flush，进入 CMD 状态
- STOP 条件（CLK 高时 IO 上升沿）→ 进入 DATA 状态
- 收集 3 个字节（CTRL, ADDR, DATA）后解析命令

**命令码表：**

| 代码 | 描述 | 简写 | 输出长度 | 需要处理 |
|------|------|------|---------|---------|
| 0x30 | read main memory | RD-M | max_addr - addr | 否 |
| 0x31 | read security memory | RD-S | 4 | 否 |
| 0x33 | compare verification data | CMP-V | - | 是 |
| 0x34 | read protection memory | RD-P | 4 | 否 |
| 0x38 | update main memory | WR-M | - | 是 |
| 0x39 | update security memory | WR-S | - | 是 |
| 0x3c | write protection memory | WR-P | - | 是 |

**数据处理：**
- OUT 状态：收集输出数据字节，达到 out_len 后 flush
- PROC 状态：跟踪 CLK 脉冲数和 IO 状态，IO 变高时结束处理

**位处理细节：**
- 每个位被调用两次（CLK 上升沿和下降沿）
- 上升沿：记录位的 ss
- 下降沿：记录位的 es，输出位注解
- 8 位组成一个字节（LSB-first，使用 bitpack_lsb）

### 3.6 C 实现计划

**注解枚举：**
```c
enum sle44xx_ann {
    ANN_RESET_SYM = 0,
    ANN_INTR_SYM,
    ANN_START_SYM,
    ANN_STOP_SYM,
    ANN_BIT_SYM,
    ANN_ATR_BYTE,
    ANN_CMD_BYTE,
    ANN_OUT_BYTE,
    ANN_PROC_BYTE,
    ANN_ATR_DATA,
    ANN_CMD_DATA,
    ANN_OUT_DATA,
    ANN_PROC_DATA,
    NUM_ANN
};
```

**私有数据结构：**
```c
#define SLE44XX_MAX_ADDR 256
#define SLE44XX_MAX_BYTES 260

struct sle44xx_priv {
    int state;           // 当前状态：ATR/CMD/DATA/OUT/PROC/-1
    uint64_t samplerate;
    int max_addr;         // 默认 256

    // 位收集
    struct { int val; uint64_t ss; uint64_t es; } bits[8];
    int bit_count;

    // ATR 字节收集
    struct { uint8_t data; uint64_t ss; uint64_t es; } atr_bytes[4];
    int atr_count;

    // CMD 字节收集
    struct { uint8_t data; uint64_t ss; uint64_t es; } cmd_bytes[3];
    int cmd_count;

    // 命令解析结果
    int cmd_proc;         // 是否需要处理阶段
    int out_len;          // 输出数据长度

    // OUT 字节收集
    struct { uint8_t data; uint64_t ss; uint64_t es; } out_bytes[SLE44XX_MAX_BYTES];
    int out_count;

    // PROC 状态
    struct {
        uint64_t ss;
        uint64_t es;
        int clk_count;
        int io_high;
    } proc_state;

    int out_ann;
    int out_binary;
};
```

**状态枚举：**
```c
enum sle44xx_state {
    STATE_NONE = -1,
    STATE_ATR = 0,
    STATE_CMD,
    STATE_DATA,
    STATE_OUT,
    STATE_PROC
};
```

**关键函数签名：**
```c
static void sle44xx_reset(struct srd_decoder_inst *di);
static void sle44xx_start(struct srd_decoder_inst *di);
static void sle44xx_decode(struct srd_decoder_inst *di);
static void sle44xx_destroy(struct srd_decoder_inst *di);
static void sle44xx_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void sle44xx_flush_queued(struct srd_decoder_inst *di);
static void sle44xx_handle_reset(struct srd_decoder_inst *di, uint64_t ss, uint64_t es, int has_clk);
static void sle44xx_handle_command(struct srd_decoder_inst *di, uint64_t ss, int is_start);
static void sle44xx_handle_data_bit(struct srd_decoder_inst *di, uint64_t ss, uint64_t es, int bit_val, int is_start_edge);
static void sle44xx_handle_data_byte(struct srd_decoder_inst *di, uint64_t ss, uint64_t es, uint8_t data);
```

### 3.7 关键实现注意事项

1. **9 个等待条件**：需要在一个 `c_cond_wait` 调用中组合所有条件
2. **位双重调用**：CLK 上升沿记录 ss，CLK 下降沿记录 es 并输出位注解
3. **RST 优先级最高**：RST 条件始终优先处理
4. **CMD/STOP 仅在非 OUT/PROC 状态下处理**：`is_outgoing` 和 `is_processing` 标志
5. **PROC IO 高电平检测**：条件 8 是无条件部分，即使不在 PROC 状态也会匹配
6. **需要 samplerate**：用于计算处理时间（微秒/毫秒）
7. **bitpack_lsb**：LSB-first 位打包，需要实现
8. **flush_queued**：在多个地方调用，输出 ATR/CMD/OUT/PROC 数据注解
9. **命令解析**：3 字节命令后立即输出 CMD_DATA 注解
10. **max_addr 默认 256**：0x30 命令的输出长度为 max_addr - addr

---

## 4. PJDL (Padded Jittering Data Link) 解码器

### 4.1 Python 解码器元数据

```python
id = 'pjdl'
name = 'PJDL'
longname = 'Padded Jittering Data Link'
desc = 'PJDL, a single wire serial link layer for PJON.'
license = 'gplv2+'
inputs = ['logic']
outputs = ['pjon_link']
tags = ['Embedded/industrial']
```

### 4.2 通道定义

| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | data | DATA | Single wire data | dec_pjdl_chan_data |

### 4.3 选项定义

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| mode | Communication mode | 1 | (1, 2, 3, 4) | dec_pjdl_opt_mode |
| idle_add_us | Added idle time (us) | 4 | (整数) | dec_pjdl_opt_idle_us |

### 4.4 注解定义

| 索引 | id | 标签 |
|------|-----|------|
| 0 | cs_busy | Carrier busy |
| 1 | cs_idle | Carrier idle |
| 2 | bit_pad | Pad bit |
| 3 | bit_low | Low bit |
| 4 | bit_data | Data bit |
| 5 | bit_short | Short data |
| 6 | sync_loss | Sync loss |
| 7 | byte | Data byte |
| 8 | frame_init | Frame init |
| 9 | frame_bytes | Frame bytes |
| 10 | frame_wait | Frame wait |

**注解行：**
| id | label | 包含的注解类 |
|----|-------|-------------|
| carriers | Carriers | (0, 1) |
| bits | Bits | (2, 3, 4, 5) |
| bytes | Bytes | (7, 8, 10) |
| frames | Frames | (9,) |
| warns | Warnings | (6,) |

### 4.5 通信模式时序

| 模式 | 数据位宽度 (us) | Pad 位宽度 (us) |
|------|----------------|----------------|
| 1 | 44 | 116 |
| 2 | 40 | 92 |
| 3 | 28 | 88 |
| 4 | 26 | 60 |

**容差：**
- 百分比容差：10%
- 绝对容差：1.5 us

**派生值：**
- byte_width = pad_width + 9 * data_width
- idle_width = byte_width + idle_add_us
- hold_high_width = 9 * time_tol_abs * usec_width

### 4.6 Python 输出格式

OUTPUT_PYTHON 格式：
- `['IDLE', 0]` — 载波空闲
- `['BUSY', True]` — 载波忙
- `['PAD_BIT', level]` — Pad 位
- `['DATA_BIT', level]` — 数据位
- `['SHORT_BIT', level]` — 短数据位
- `['SYNC_LOSS', text]` — 同步丢失
- `['SYNC_PAD', True]` — 同步 Pad（PAD + LOW DATA 组合）
- `['DATA_BYTE', byte_value]` — 数据字节
- `['FRAME_INIT', True]` — 帧初始化（3 个 SYNC_PAD）
- `['FRAME_DATA', data_list]` — 帧数据
- `['SYNC_RESP_WAIT', True]` — 等待同步响应

### 4.7 状态机分析

**PJDL 是基于时序的协议，不是传统的边沿触发状态机。**

**核心流程：**

1. **初始同步**：等待第一个低电平
2. **边沿搜索**：等待下一个边沿或超时（lookahead_width = 4 * data_width 样本）
3. **位宽度分类**：
   - `span_is_pad(span)` — 检查是否为 PAD 位宽度
   - `span_is_data(span)` — 检查是否为 1/2/3/4 倍数据位宽度
   - `span_is_short(span)` — 检查是否为短数据位宽度

4. **符号序列处理**：
   - PAD_BIT → 记录
   - SHORT_BIT → 记录
   - PAD_BIT + ZERO_BIT → 合并为 SYNC_PAD
   - 3 × SYNC_PAD → 合并为 FRAME_INIT
   - SHORT_BIT + SYNC_PAD → 合并为 WAIT_ACK（可挤压前面的 SHORT_BIT）
   - SYNC_PAD + 8 × DATA_BIT → 合并为 DATA_BYTE
   - WAIT_ACK + DATA_BYTE → flush 帧

5. **数据位采样**：
   - 在 SYNC_PAD 下降沿后，以固定间隔采样 8 个数据位
   - 使用 `wait_until()` 在保持载波检测的同时等待到指定位采样点
   - 位采样点 = data_fall_time + data_width * (bit_index + 0.5)

6. **载波检测**：
   - HIGH → 立即结束 IDLE，切换到 BUSY
   - LOW 持续 byte_width → 结束 BUSY
   - LOW 持续 idle_width → 开始 IDLE

7. **帧刷新**：在 FRAME_INIT、IDLE、WAIT_ACK+DATA_BYTE 时触发

### 4.8 C 实现计划

**私有数据结构：**
```c
#define PJDL_MAX_SYMBOLS 1024
#define PJDL_MAX_FRAME_BYTES 256

struct pjdl_symbol {
    uint64_t ss;
    uint64_t es;
    int type;    // 符号类型
    int data;    // 符号数据
};

struct pjdl_priv {
    uint64_t samplerate;
    int mode;

    // 时序参数（样本数）
    double data_width;
    double pad_width;
    double byte_width;
    double idle_width;
    double add_idle_width;
    uint64_t hold_high_width;
    uint64_t lookahead_width;

    // 位宽度范围（样本数）
    uint64_t data_bit_1_range[2];
    uint64_t data_bit_2_range[2];
    uint64_t data_bit_3_range[2];
    uint64_t data_bit_4_range[2];
    uint64_t short_data_range[2];
    uint64_t pad_bit_range[2];

    // 载波检测
    int carrier_want_idle;
    int carrier_is_busy;
    int carrier_is_idle;
    uint64_t carrier_idle_ss;
    uint64_t carrier_busy_ss;

    // 边沿跟踪
    uint64_t edges[4];
    int edge_count;

    // 符号列表
    struct pjdl_symbol symbols[PJDL_MAX_SYMBOLS];
    int symbol_count;

    // 数据位收集
    int data_bits[8];
    int data_bit_count;
    uint64_t data_fall_time;

    // 帧字节收集
    uint8_t frame_bytes[PJDL_MAX_FRAME_BYTES];
    int frame_byte_count;

    int out_ann;
    int out_python;
};
```

**符号类型枚举：**
```c
enum pjdl_symbol_type {
    SYM_IDLE = 0,
    SYM_PAD_BIT,
    SYM_ZERO_BIT,
    SYM_DATA_BIT,
    SYM_SHORT_BIT,
    SYM_SYNC_PAD,
    SYM_DATA_BYTE,
    SYM_FRAME_INIT,
    SYM_WAIT_ACK,
};
```

**关键函数签名：**
```c
static void pjdl_reset(struct srd_decoder_inst *di);
static void pjdl_start(struct srd_decoder_inst *di);
static void pjdl_decode(struct srd_decoder_inst *di);
static void pjdl_destroy(struct srd_decoder_inst *di);
static void pjdl_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void pjdl_span_prepare(struct srd_decoder_inst *di);
static int pjdl_span_is_pad(struct pjdl_priv *s, uint64_t span);
static int pjdl_span_is_data(struct pjdl_priv *s, uint64_t span);
static int pjdl_span_is_short(struct pjdl_priv *s, uint64_t span);
static void pjdl_carrier_check(struct srd_decoder_inst *di, int level, uint64_t snum);
static void pjdl_frame_flush(struct srd_decoder_inst *di);
static void pjdl_symbols_collapse(struct pjdl_priv *s, int count, int symbol, int squeeze);
static int pjdl_symbols_has_prev(struct pjdl_priv *s, int *want_items, int count);
```

### 4.9 关键实现注意事项

1. **基于时序的协议**：不像 I2C/SPI 那样有明确的时钟信号，需要测量脉冲宽度
2. **需要 samplerate**：所有时序计算依赖采样率，至少 1MSa/s
3. **容差处理**：位宽度有 ±10% 和 ±1.5us 的容差
4. **符号序列匹配**：需要实现 symbols_has_prev、symbols_collapse 等符号操作
5. **载波检测**：在所有等待操作中都需要持续检测载波状态
6. **wait_until**：需要在等待指定位采样点时保持载波检测
7. **DATA 位采样**：在 SYNC_PAD 下降沿后以固定间隔采样，不是边沿触发
8. **HIGH 保持检测**：最后一个 DATA 位后可能没有下降沿，需要超时检测
9. **帧刷新时机**：FRAME_INIT、IDLE、WAIT_ACK+DATA_BYTE
10. **Python 版本有 _with_ann_carrier 和 _with_ann_sync_loss 控制开关**：C 版本可以始终输出
11. **lookahead_width**：4 * data_width 样本，用于边沿搜索超时
12. **浮点精度**：位采样点使用浮点计算，只在最后取整

### 4.10 与 Python 版本的关键差异

1. **Python 使用 self.symbols 列表动态操作**：C 需要固定大小数组或动态分配
2. **Python 的 symbols_collapse 使用列表切片**：C 需要数组移动
3. **Python 的 wait_until 使用 self.wait + carrier_check**：C 需要用 c_cond_wait + 手动载波检测
4. **Python 使用 bitpack() 辅助函数**：C 需要自行实现位打包
5. **Python 的 frame_flush 构建文本**：C 需要 snprintf 构建帧文本

---

## 5. OneWire Link 解码器

### 5.1 Python 解码器元数据

```python
id = 'onewire_link'
name = 'OneWire link layer'
longname = '1-Wire serial communication bus (link layer)'
desc = 'Bidirectional, half-duplex, asynchronous serial bus.'
license = 'gplv2+'
inputs = ['logic']
outputs = ['onewire_link']
tags = ['Embedded/industrial']
```

### 5.2 通道定义

| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | owr | OWR | 1-Wire signal line | dec_onewire_link_chan_owr |

### 5.3 选项定义

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| overdrive | Start in overdrive speed | 'no' | ('yes', 'no') | dec_onewire_link_opt_overdrive |

### 5.4 注解定义

| 索引 | id | 标签文本 |
|------|-----|---------|
| 0 | bit | Bit |
| 1 | warnings | Warnings |
| 2 | reset | Reset |
| 3 | presence | Presence |
| 4 | overdrive | Overdrive speed notifications |

**注解行：**
| id | label | 包含的注解类 |
|----|-------|-------------|
| bits | Bits | (0, 2, 3) |
| info | Info | (4,) |
| warnings | Warnings | (1,) |

### 5.5 时序参数

| 参数 | 正常模式 min | 正常模式 max | 过驱动模式 min | 过驱动模式 max |
|------|-------------|-------------|---------------|---------------|
| RSTL (Reset Low) | 480 us | 960 us | 48 us | 80 us |
| RSTH (Reset High) | 480 us | - | 48 us | - |
| PDH (Presence Detect High) | 15 us | 60 us | 2 us | 6 us |
| PDL (Presence Detect Low) | 60 us | 240 us | 8 us | 24 us |
| SLOT (Time Slot) | 60 us | 120 us | 6 us | 16 us |
| REC (Recovery) | 1 us | - | 1 us | - |
| LOWR (Low Read) | 1 us | 15 us | 1 us | 2 us |

### 5.6 Python 输出格式

OUTPUT_PYTHON 格式：
- `['BIT', bit_value]` — 数据位（0 或 1）
- `['RESET/PRESENCE', present_bool]` — 复位/存在检测

### 5.7 状态机分析

**状态：**
1. `INITIAL` — 初始状态，等待高电平
2. `IDLE` — 空闲高电平，等待下降沿
3. `LOW` — 低电平，判断是复位脉冲还是时间槽
4. `PRESENCE DETECT HIGH` — 等待从设备存在信号（下降沿）
5. `PRESENCE DETECT LOW` — 从设备存在信号（等待上升沿）
6. `SLOT` — 时间槽结束等待
7. `PRESENCE DETECT` — 存在检测结束等待

**详细流程：**

1. **INITIAL → IDLE**：等待 OWR 高电平
2. **IDLE → LOW**：等待 OWR 下降沿
   - 检查恢复时间（从上次上升沿到本次下降沿）
   - 如果恢复时间 < REC min → 警告
3. **LOW 状态**：等待 OWR 上升沿
   - 计算低电平持续时间
   - 如果 >= RSTL min (正常模式 480us) → 正常复位
     - 如果 > RSTL max → 警告
     - 如果当前在过驱动模式 → 退出过驱动
     - 输出 RESET 注解
     - 进入 PRESENCE DETECT HIGH
   - 如果在过驱动模式且 >= RSTL min (过驱动) 且 < RSTL max (过驱动) → 过驱动复位
     - 输出 RESET 注解
     - 进入 PRESENCE DETECT HIGH
   - 如果 < SLOT max → 读/写时间槽
     - 如果 < LOWR min → 警告
     - 如果 < LOWR max → bit = 1（短脉冲）
     - 否则 → bit = 0（长脉冲）
     - 进入 SLOT
   - 否则 → 错误信号，回到 IDLE
4. **PRESENCE DETECT HIGH**：等待下降沿或超时（PDH max）
   - 如果检测到下降沿且未超时 → 存在检测
     - 如果时间 < PDH min → 警告
     - 进入 PRESENCE DETECT LOW
   - 否则 → 无存在检测
     - 输出 Presence: false
     - 输出 Python `['RESET/PRESENCE', False]`
     - 回到 IDLE
5. **PRESENCE DETECT LOW**：等待上升沿
   - 计算存在信号持续时间
   - 如果 < PDL min → 警告
   - 如果 > PDL max → 警告
   - 如果 > RSTH min → 更新 rise
   - 进入 PRESENCE DETECT
6. **SLOT**：等待下降沿或超时（SLOT min）
   - 如果检测到下降沿且未超时 → 时间槽太短
     - 警告
     - 不输出无效位
     - fall = samplenum，进入 LOW
   - 否则 → 时间槽结束
     - 输出位注解
     - 输出 Python `['BIT', bit]`
     - 如果 bit_count >= 0：收集命令位
     - 如果 bit_count == 8：检查过驱动 ROM 命令
       - 0x3C 或 0x69 → 进入过驱动模式
     - 回到 IDLE
7. **PRESENCE DETECT**：等待下降沿或超时（RSTH min）
   - 如果检测到下降沿且未超时 → 存在检测太短
     - 输出从设备存在注解
     - 输出 Python `['RESET/PRESENCE', True]`
     - fall = samplenum，进入 LOW
   - 否则 → 存在检测完成
     - 输出 Presence: true
     - 输出 Python `['RESET/PRESENCE', True]`
     - 开始计数前 8 位获取 ROM 命令
     - 回到 IDLE

**过驱动模式切换：**
- 检测到 ROM 命令 0x3C 或 0x69 → 进入过驱动
- 正常复位脉冲 → 退出过驱动

**采样率检查：**
- 过驱动模式：最低 2MHz，建议 5MHz
- 正常模式：最低 400kHz，建议 1MHz

### 5.8 C 实现计划

**注意**：代码库中已存在 `onewire_c.c`，它是 `onewire_link` 的简化版 C 实现。新的 `onewire_link_c.c` 需要完整实现 Python 版本的所有功能。

**注解枚举：**
```c
#define ANN_BIT       0
#define ANN_WARN      1
#define ANN_RESET     2
#define ANN_PRESENCE  3
#define ANN_OVERDRIVE 4
#define NUM_ANN       5
```

**私有数据结构：**
```c
struct owlink_priv {
    int state;
    uint8_t byte_val;
    int bit_cnt;
    uint64_t ss_rise;
    uint64_t ss_fall;
    int overdrive;
    int present;
    int bit_val;
    uint64_t samplerate;
    int out_ann;
    int out_python;
};
```

**状态枚举：**
```c
enum owlink_state {
    STATE_INITIAL,
    STATE_IDLE,
    STATE_LOW,
    STATE_PRESENCE_DETECT_HIGH,
    STATE_PRESENCE_DETECT_LOW,
    STATE_SLOT,
    STATE_PRESENCE_DETECT
};
```

**时序阈值计算函数：**
```c
// 将微秒转换为样本数
static uint64_t us_to_samples(uint64_t samplerate, double us) {
    return (uint64_t)(us * samplerate / 1000000.0);
}
```

**关键函数签名：**
```c
static void owlink_reset(struct srd_decoder_inst *di);
static void owlink_start(struct srd_decoder_inst *di);
static void owlink_decode(struct srd_decoder_inst *di);
static void owlink_destroy(struct srd_decoder_inst *di);
static void owlink_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void owlink_checks(struct srd_decoder_inst *di);
```

### 5.9 关键实现注意事项

1. **与现有 onewire_c.c 的关系**：现有 C 实现是简化版，缺少完整的存在检测流程和过驱动模式切换。新实现需要完整覆盖 Python 版本功能。
2. **wait_falling_timeout**：Python 版本使用 `{0:'f'}, {'skip':count}` 组合等待，C 版本需要用 `c_cond_fall` + `c_cond_skip` + `c_cond_or` 实现
3. **matched 位检查**：Python 用 `self.matched & (0b1 << 0)` 检查第一个条件，`self.matched & (0b1 << 1)` 检查第二个；C 版本用 `matched & 0b1` 和 `matched & 0b10`
4. **时序阈值**：需要根据 overdrive 标志动态切换
5. **采样率检查**：在 metadata 回调或 decode 开始时检查
6. **位收集和过驱动检测**：bit_count 从 -1 开始，存在检测后设为 0，收集 8 位后检查 ROM 命令
7. **Python 输出**：需要 `c_decoder_put_python` 输出 BIT 和 RESET/PRESENCE
8. **警告注解**：多个时序检查点需要输出警告

### 5.10 与现有 onewire_c.c 的差异

现有 `onewire_c.c` 的不足：
1. 没有完整的存在检测流程（PRESENCE DETECT HIGH/LOW/END）
2. 没有过驱动模式动态切换
3. 没有时序警告（恢复时间、脉冲宽度等）
4. 没有采样率检查
5. 位判断逻辑简化（只用短/长阈值，没有完整的时间槽逻辑）

新 `onewire_link_c.c` 需要：
1. 完整实现所有 7 个状态
2. 完整的过驱动模式支持
3. 所有警告注解
4. 采样率检查
5. 完整的时间槽和存在检测时序

---

## 通用 C 解码器框架参考

### 必需的头文件

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"
```

### 解码器导出结构

```c
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    .longname = "Full name(C)",
    .desc = "Description.(C implementation)",
    .license = "gplv2+",
    .channels = xxx_channels,
    .num_channels = N,
    .optional_channels = xxx_optional_channels,
    .num_optional_channels = M,
    .options = xxx_options,
    .num_options = K,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = R,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,
    .num_inputs = 1,
    .outputs = xxx_outputs,
    .num_outputs = 1,
    .binary = xxx_binary,
    .num_binary = B,
    .tags = xxx_tags,
    .num_tags = 1,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,
    .destroy = xxx_destroy,
    .metadata = xxx_metadata,  // 如果需要 samplerate
    .recv_proto = xxx_recv_proto,  // 如果是堆叠解码器
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    // 初始化选项默认值和值列表
    return &xxx_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
```

### 条件构建器 API

```c
srd_cond_builder *cb = c_cond_new();
c_cond_rise(cb, channel_index);   // 上升沿
c_cond_fall(cb, channel_index);   // 下降沿
c_cond_high(cb, channel_index);   // 高电平
c_cond_low(cb, channel_index);    // 低电平
c_cond_edge(cb, channel_index);   // 任意边沿
c_cond_noedge(cb, channel_index); // 无变化时匹配 <!-- Updated: 已实现 -->
c_cond_skip(cb, count);           // 跳过样本数
c_cond_or(cb);                    // 或条件（分隔多个匹配条件）
int ret = c_cond_wait(cb, di, &samplenum, &matched);
int ret = c_cond_wait_current(di, &samplenum);  // 等效Python self.wait({}) <!-- Updated: 已实现 -->
c_cond_free(cb);
// matched 的位对应各个 OR 分支的匹配结果
```

### 注解输出宏

```c
// 基本注解输出
C_ANN_PUT(di, ss, es, out_id, ann_class, "long text", "short text", "tiny text");

// 带数值的注解输出
C_ANN_PUT_VAL(di, ss, es, out_id, ann_class, numeric_value, "long text", "short");

// Python 输出
c_decoder_put_python(di, ss, es, out_python, "TYPE", &data, data_len);

// Binary 输出
c_decoder_put_binary(di, ss, es, out_binary, bin_class, size, &data);

// Logic 输出（用于上层解码器堆叠）
c_decoder_put_logic(di, ss, es, out_logic, channel_mask, values, num_channels);  // <!-- Updated: 已实现，用于SRD_OUTPUT_LOGIC输出 -->

// META 输出
c_decoder_register_output_meta(di, SRD_OUTPUT_META, "proto_id", "int", "name", "descr");  // <!-- Updated: 已实现，注册META输出 -->
c_decoder_put_meta_int(di, ss, es, out_meta, int_value);  // <!-- Updated: 已实现，输出int类型META -->
c_decoder_put_meta_double(di, ss, es, out_meta, double_value);  // <!-- Updated: 已实现，输出double类型META -->
```

### 其他 API

```c
// 引脚读取
uint8_t c_decoder_get_pin(di, ch, samplenum);  // 读取指定通道在指定采样号的值
uint8_t c_decoder_get_initial_pin(di, ch);  // 读取初始引脚值，等效Python self.initial_pins <!-- Updated: 已实现 -->
int c_decoder_has_channel(di, ch);  // 检查通道是否存在

// 采样信息
uint64_t c_decoder_get_samplerate(di);  // 获取采样率
uint64_t c_decoder_get_last_samplenum(di);  // 获取最后一个采样号 <!-- Updated: 已实现 -->
```

### 选项读取

```c
const char *str_val = c_decoder_get_option_string(di, "option_id", "default");
int64_t int_val = c_decoder_get_option_int(di, "option_id", default_int);
double dbl_val = c_decoder_get_option_double(di, "option_id", default_double);
```

### CMakeLists.txt 修改

在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加：
```
tmc_c
modbus_c
sle44xx_c
pjdl_c
onewire_link_c
```

---

## 移植优先级和依赖关系

| 解码器 | 优先级 | 依赖 | 预估代码行数 |
|--------|--------|------|-------------|
| onewire_link | 高（已有简化版参考） | 无 | ~350 |
| tmc | 高（中等复杂度） | 无 | ~400 |
| sle44xx | 中 | 无 | ~500 |
| modbus | 中（堆叠解码器） | uart_c | ~800 |
| pjdl | 低（最复杂） | 无 | ~700 |

**建议实现顺序**：onewire_link → tmc → sle44xx → modbus → pjdl

---

## 测试策略

每个解码器完成后需要：
1. 编译通过（无警告）
2. 在 PXView 中加载不崩溃
3. 使用对应协议的捕获文件验证注解输出
4. 与 Python 版本的注解输出对比
