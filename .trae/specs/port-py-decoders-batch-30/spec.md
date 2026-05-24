# Python → C 解码器移植规格书 — Batch 30

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| uart_c.c | 底层协议输出范本 | c_decoder_put_python()输出协议数据、双通道(RX/TX)独立状态 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |

## 概述

本批次将 5 个 UART 上层协议 Python 解码器移植为 C 实现。所有解码器的 `inputs=['uart']`，即它们不直接读取 logic 信号，而是通过 `recv_proto()` 回调接收 UART C 解码器输出的 Python protocol 数据。

### 移植目标解码器

| # | Python ID | C ID | 协议名称 | 复杂度 | 优先级 |
|---|-----------|------|----------|--------|--------|
| 1 | `j1708` | `j1708_c` | J1708 (SAE J1708) | ★★★☆ | 高 |
| 2 | `midi` | `midi_c` | MIDI (Musical Instrument Digital Interface) | ★★★★★ | 高 |
| 3 | `modbus` | `modbus_c` | Modbus RTU | ★★★★ | 高 |
| 4 | `pan1321` | `pan1321_c` | Panasonic PAN1321 Bluetooth SPP | ★★☆☆ | 中 |
| 5 | `pn532` | `pn532_c` | PN532 NFC Transceiver | ★★★☆ | 中 |

---

## 核心架构：UART 上层解码器的 recv_proto() 模式

### 与底层解码器的根本区别

底层解码器（如 `uart_c`、`i2c_c`、`spi_c`）直接读取 logic 信号：
- 实现 `decode()` 函数
- 使用 `c_cond_rise/fall/edge/skip/wait` 条件构建器
- 通过 `c_decoder_get_pin()` 读取引脚电平
- 通过 `c_decoder_get_samplerate()` 获取采样率

上层解码器（如本批次的 5 个）接收下层解码器的 Python protocol 输出：
- **不实现** `decode()` 函数（或实现为空函数）
- **必须实现** `recv_proto()` 回调函数
- 不需要 channels/optional_channels
- 不需要 samplerate（时间信息由 recv_proto 参数提供）
- `inputs` 数组设为 `{"uart"}` 而非 `{"logic"}`

### recv_proto() 函数签名

```c
static void xxx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

参数说明：
- `start_sample` / `end_sample`：当前协议数据单元的采样范围
- `cmd`：UART 输出的协议命令字符串，如 `"DATA"`, `"STARTBIT"`, `"STOPBIT"`, `"FRAME"`, `"PARITYBIT"`, `"PARITY ERROR"`, `"INVALID STARTBIT"`, `"INVALID STOPBIT"`
- `data`：协议附加数据字节数组
- `data_len`：data 数组长度

### UART C 解码器输出的 Python Protocol 命令

从 `uart_c.c` 源码中提取的所有 `c_decoder_put_python` 调用：

| cmd 字符串 | data 格式 | 说明 |
|-----------|-----------|------|
| `"DATA"` | `data[0]=byte_value, data[1]=rxtx` | 数据字节（rxtx: 0=RX, 1=TX） |
| `"FRAME"` | `data[0]=byte_value, data[1]=rxtx, data[2]=frame_valid` | 完整帧 |
| `"STARTBIT"` | `data[0]=start_bit_value` | 起始位 |
| `"STOPBIT"` | `data[0]=stop_bit_value` | 停止位 |
| `"PARITYBIT"` | `data[0]=parity_bit_value` | 校验位 |
| `"PARITY ERROR"` | `data[0]=expected, data[1]=actual` | 校验错误 |
| `"INVALID STARTBIT"` | `data[0]=start_bit_value` | 无效起始位 |
| `"INVALID STOPBIT"` | `data[0]=stop_bit_value` | 无效停止位 |
| `"IDLE"` | `data[0]=rxtx(0=RX,1=TX)` | 空闲检测 <!-- Updated: IDLE已由uart_c.c实现(handle_idle函数)，原spec遗漏此命令 --> |
| `"BREAK"` | `data[0]=rxtx(0=RX,1=TX)` | Break条件 <!-- Updated: BREAK已由uart_c.c实现(handle_break函数)，原spec遗漏此命令 --> |

### 上层解码器在 srd_c_decoder 结构体中的关键设置

```c
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    // ...
    .channels = NULL,           // 上层解码器不需要通道
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .inputs = xxx_inputs,       // {"uart"}
    .num_inputs = 1,
    .outputs = xxx_outputs,     // 视解码器而定
    .num_outputs = N,           // 视解码器而定
    // ...
    .decode = xxx_decode,       // 空函数，仅 (void)di;
    .recv_proto = xxx_recv_proto, // 核心回调
};
```

---

## 解码器 #1：J1708 → j1708_c

### 协议概述

SAE J1708 是一种用于重型车辆（卡车/客车）的串行通信协议，基于 UART 9600bps 8N1 LSB-first。J1708 使用共享介质（仅 RX 线），消息由 MID（Message Identifier）+ Payload + Checksum 组成。

### Python 元数据提取

```python
id = 'j1708'
name = 'J1708'
longname = 'J1708'
desc = 'J1708'
license = 'gplv2+'
inputs = ['uart']
outputs = []
tags = ['Automotive']
```

### Python Options

| ID | 描述 | 默认值 | 可选值 |
|----|------|--------|--------|
| `message_break` | 消息间隔（bit times） | 2 | (2, 10, 12) |

### Python Annotations (6个)

| 索引 | ID | 描述 |
|------|----|------|
| 0 | `datum` | J1708 消息数据 |
| 1 | `info` | 协议信息 |
| 2 | `error` | 协议违规或错误 |
| 3 | `inline_error` | 内联协议违规或错误 |
| 4 | `delay` | 消息间延迟 [bit times] |
| 5 | `bus_access` | 总线访问时间违规 [bit times] |

### Python Annotation Rows (4个)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `fields` | RX Fields | (1,) |
| `data` | RX Data | (0, 3) |
| `errors` | RX Errors | (2, 5) |
| `delays` | RX Message Delays | (4,) |

### Python Binary (3个)

| 索引 | ID | 描述 |
|------|----|------|
| 0 | `mid` | J1708 MID |
| 1 | `payload` | J1708 Payload |
| 2 | `crc` | J1708 Checksum |

### 解码逻辑分析

**状态机**：Python 版使用 `SimpleUartFsm` 实现 UART 级别的状态机（WaitForStartBit → WaitForData → WaitForStopBit），但在 C 版本中不需要，因为 UART C 解码器已经完成了帧解析。C 版本只需关注上层消息组装逻辑。

**核心流程**：
1. 仅处理 RX 数据（`rxtx == 0`），忽略 TX
2. 忽略 `FRAME`、`BREAK`、`INVALID STOPBIT` 类型
3. 通过 STARTBIT/DATA/STOPBIT 事件跟踪消息边界
4. 当检测到 `message_break` 个 bit times 的间隔时，认为前一条消息结束
5. 验证 checksum（二进制补码校验和）
6. 输出 MID、Payload、CRC 字段

**Checksum 算法**：
```python
val = ~reduce(lambda x, y: (x + y) & 0xFF, list(msg)) + 1
return struct.pack('B', val & 0xFF)
```

**C 版本简化策略**：
- 不需要 UART FSM（已由 uart_c 处理）
- 直接在 `recv_proto()` 中处理 `"DATA"` 命令
- 通过采样间隔计算消息分隔
- 需要获取 `bit_width`：推荐通过 `STARTBIT`/`STOPBIT` 的采样范围推算（与 Python 版本使用 `metadata()` 获取 `samplerate` 后计算等效）
- <!-- Updated: Python版本使用metadata()回调获取samplerate计算bit_width，C版本推荐通过STARTBIT/STOPBIT推算bit_width -->

### C 实现关键代码片段

```c
#define J1708_BAUD 9600
#define MIN_BUS_ACCESS_BIT_TIMES 12
#define MAX_MSG_LEN 256

enum j1708_state {
    J1708_IDLE,
    J1708_IN_MESSAGE,
};

enum j1708_ann {
    ANN_DATUM = 0,
    ANN_INFO,
    ANN_ERROR,
    ANN_INLINE_ERROR,
    ANN_DELAY,
    ANN_BUS_ACCESS,
    NUM_ANN,
};

typedef struct {
    enum j1708_state state;
    uint8_t data[MAX_MSG_LEN];
    int data_len;
    uint64_t first_startbit_ss;
    uint64_t prev_stopbit_es;
    uint64_t last_valid_msg_stopbit_es;
    double bit_width;
    int message_break;
    int out_ann;
    int out_bin; // <!-- Updated: Python J1708注册了OUTPUT_BINARY，C版本也需要注册并输出MID/payload/CRC二进制数据 -->
} j1708_state;

static void j1708_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    j1708_state *s = (j1708_state *)c_decoder_get_private(di);
    if (!s) return;

    // 获取 bit_width：通过 STARTBIT/STOPBIT 的采样范围推算
    // <!-- Updated: 补充STARTBIT/STOPBIT处理逻辑，用于计算bit_width -->
    if (s->bit_width == 0) {
        if (strcmp(cmd, "STARTBIT") == 0 || strcmp(cmd, "STOPBIT") == 0) {
            s->bit_width = (double)(end_sample - start_sample);
            return; // 在知道 bit_width 之前不处理 DATA
        } else if (strcmp(cmd, "DATA") != 0) {
            return;
        }
    }

    // 仅处理 DATA 类型，仅 RX
    if (strcmp(cmd, "DATA") != 0) return;
    if (data_len < 2) return;
    uint8_t byte_val = data[0];
    uint8_t rxtx = data[1];
    if (rxtx != 0) return; // J1708 仅 RX

    // 检查消息间隔
    if (s->prev_stopbit_es > 0 && s->bit_width > 0) {
        double delay_bits = (double)(start_sample - s->prev_stopbit_es) / s->bit_width;
        if ((int)delay_bits > s->message_break) {
            // 刷新前一条消息
            j1708_flush_message(di, s);
        }
        // 输出延迟信息
        if (s->last_valid_msg_stopbit_es > 0) {
            double inter_delay = (double)(start_sample - s->last_valid_msg_stopbit_es) / s->bit_width;
            char buf[32];
            snprintf(buf, sizeof(buf), "%05.1f", inter_delay);
            C_ANN_PUT(di, s->last_valid_msg_stopbit_es, start_sample, s->out_ann, ANN_DELAY, buf);
            if (inter_delay < MIN_BUS_ACCESS_BIT_TIMES) {
                C_ANN_PUT(di, s->last_valid_msg_stopbit_es, start_sample, s->out_ann, ANN_BUS_ACCESS, buf);
            }
        }
    }

    // 记录第一个字节的位置
    if (s->data_len == 0) {
        s->first_startbit_ss = start_sample;
    }
    s->data[s->data_len++] = byte_val;
    s->prev_stopbit_es = end_sample;
}

static uint8_t j1708_checksum(uint8_t *msg, int len) {
    uint16_t sum = 0;
    for (int i = 0; i < len; i++)
        sum = (sum + msg[i]) & 0xFF;
    return (~sum + 1) & 0xFF;
}
```

### metadata 回调

J1708 需要 `bit_width` 来计算消息间隔。有两种方式获取：

**方式1（推荐）：通过 STARTBIT/STOPBIT 推算**
在 `recv_proto` 中处理 `STARTBIT`/`STOPBIT` 命令时计算 `bit_width = end_sample - start_sample`。
此方式不需要 `metadata()` 回调，且与 Python 版本行为一致。

**方式2：通过 metadata 回调获取 samplerate**
<!-- Updated: 方式1更优，与Python版本行为一致，metadata回调为备选方案 -->

```c
static void j1708_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    j1708_state *s = (j1708_state *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->bit_width = (double)value / (double)J1708_BAUD;
    }
}
```

---

## 解码器 #2：MIDI → midi_c

### 协议概述

MIDI (Musical Instrument Digital Interface) 基于 UART 31250bps 8N1 LSB-first。协议复杂，包含：
- Channel Voice Messages（0x80-0xEF）：Note On/Off, Control Change, Program Change 等
- System Common Messages（0xF1-0xF7）：MIDI Time Code, Song Position 等
- System Real-Time Messages（0xF8-0xFF）：Timing Clock, Start, Stop 等
- System Exclusive（0xF0）：厂商自定义消息

### Python 元数据提取

```python
id = 'midi'
name = 'MIDI'
longname = 'Musical Instrument Digital Interface'
desc = 'Musical Instrument Digital Interface (MIDI) protocol.'
license = 'gplv2+'
inputs = ['uart']
outputs = []
tags = ['Audio', 'PC']
```

### Python Annotations (3个)

| 索引 | ID | 描述 |
|------|----|------|
| 0 | `text-verbose` | 详细可读文本 |
| 1 | `text-sysreal-verbose` | SysReal 详细可读文本 |
| 2 | `text-error` | 错误文本 |

### Python Annotation Rows (2个)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `normal` | Normal | (0, 2) |
| `sys-real` | SysReal | (1,) |

### 解码逻辑分析

**核心状态机**：
- `IDLE` → 收到 status byte → 进入对应消息处理状态
- `HANDLE CHANNEL MSG`：处理通道消息（0x80-0xEF）
- `HANDLE SYSEX MSG`：处理系统独占消息（0xF0）
- `HANDLE SYSCOMMON MSG`：处理系统公共消息（0xF1-0xF6）
- `HANDLE SYSREALTIME MSG`：处理系统实时消息（0xF8-0xFF）
- `BUFFER GARBAGE MSG`：缓冲垃圾数据

**Running Status 机制**：MIDI 允许省略连续相同类型的 status byte，C 版本必须实现。

**SysRealtime 中断**：实时消息可以中断其他消息，处理完后恢复原状态。

**复杂度分析**：
- 通道消息类型：7 种（0x80-0xE0），每种有不同的数据字节数
- Control Change (0xB0) 有特殊处理：cc 0x78-0x7F 是 Channel Mode
- SysEx 消息可变长，需要厂商 ID 查找
- 需要大量查找表（status_bytes, control_functions, gm_instruments, chromatic_notes, percussion_notes, sysex_manufacturer_ids 等）

### C 实现关键代码片段

```c
#define MIDI_BAUD 31250

enum midi_state {
    MIDI_IDLE,
    MIDI_HANDLE_CHANNEL_MSG,
    MIDI_HANDLE_SYSEX_MSG,
    MIDI_HANDLE_SYSCOMMON_MSG,
    MIDI_HANDLE_SYSREALTIME_MSG,
    MIDI_BUFFER_GARBAGE,
};

enum midi_ann {
    ANN_TEXT_VERBOSE = 0,
    ANN_TEXT_SYSREAL_VERBOSE,
    ANN_TEXT_ERROR,
    NUM_ANN,
};

typedef struct {
    enum midi_state state;
    uint8_t status_byte;
    int explicit_status_byte;
    uint8_t cmd[256];
    int cmd_len;
    uint64_t ss;
    uint64_t es;
    uint64_t ss_block;
    uint64_t es_block;
    int out_ann;
} midi_state;

// status_bytes 查找表（3级缩写）
static const char *status_bytes[][3] = {
    /* 0x80 */ {"note off", "note off", "N off"},
    /* 0x90 */ {"note on", "note on", "N on"},
    /* 0xA0 */ {"polyphonic key pressure / aftertouch", "key pressure", "KP"},
    /* 0xB0 */ {"control change", "ctrl chg", "CC"},
    /* 0xC0 */ {"program change", "prgm chg", "PC"},
    /* 0xD0 */ {"channel pressure / aftertouch", "channel pressure", "CP"},
    /* 0xE0 */ {"pitch bend change", "pitch bend", "PB"},
    /* 0xF0 */ {"system exclusive", "SysEx", "SE"},
    /* 0xF1 */ {"MIDI time code quarter frame", "MIDI time code", "MIDI time"},
    /* 0xF2 */ {"song position pointer", "song position", "song pos"},
    /* 0xF3 */ {"song select", "song select", "song sel"},
    /* 0xF4 */ {"undefined 0xf4", "undef 0xf4", "undef"},
    /* 0xF5 */ {"undefined 0xf5", "undef 0xf5", "undef"},
    /* 0xF6 */ {"tune request", "tune request", "tune req"},
    /* 0xF7 */ {"end of system exclusive (EOX)", "end of SysEx", "EOX"},
    /* 0xF8 */ {"timing clock", "timing clock", "clock"},
    /* 0xF9 */ {"undefined 0xf9", "undef 0xf9", "undef"},
    /* 0xFA */ {"start", "start", "s"},
    /* 0xFB */ {"continue", "continue", "cont"},
    /* 0xFC */ {"stop", "stop", "st"},
    /* 0xFD */ {"undefined 0xfd", "undef 0xfd", "undef"},
    /* 0xFE */ {"active sensing", "active sensing", "sensing"},
    /* 0xFF */ {"system reset", "reset", "rst"},
};

static void midi_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    midi_state *s = (midi_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") != 0) return;
    if (data_len < 2) return;
    uint8_t byte_val = data[0];
    // uint8_t rxtx = data[1]; // MIDI 不区分 RX/TX

    s->ss = start_sample;
    s->es = end_sample;

    enum midi_state new_state = s->state;

    if (byte_val >= 0x80 && byte_val != 0xF7) {
        new_state = midi_get_next_state(s, byte_val);
        if (new_state != MIDI_HANDLE_SYSREALTIME_MSG && s->state != MIDI_IDLE) {
            // 刷新前一条消息
            midi_handle_state(di, s, s->state, -1);
        }
        s->ss = start_sample;
        s->es = end_sample;
        if (new_state != MIDI_HANDLE_SYSREALTIME_MSG) {
            s->ss_block = start_sample;
        }
    } else if (s->state == MIDI_IDLE || s->state == MIDI_BUFFER_GARBAGE) {
        s->ss = start_sample;
        s->es = end_sample;
        if (s->state == MIDI_IDLE) s->ss_block = start_sample;
        new_state = midi_get_next_state(s, byte_val);
    } else {
        s->ss = start_sample;
        s->es = end_sample;
        new_state = s->state;
    }

    if (new_state != MIDI_HANDLE_SYSREALTIME_MSG)
        s->state = new_state;
    if (new_state == MIDI_BUFFER_GARBAGE)
        s->status_byte = 0;

    midi_handle_state(di, s, new_state, byte_val);
}
```

### 查找表策略

MIDI 解码器需要大量查找表。C 版本将使用静态数组实现：
- `status_bytes[256][3]`：状态字节名称（3级缩写）
- `control_functions[128][3]`：控制器功能名称
- `chromatic_notes[128]`：音名
- `percussion_notes[128]`：打击乐名称
- `gm_instruments[128]`：通用 MIDI 乐器名
- `sysex_manufacturer_ids`：使用线性搜索的结构体数组（因为 key 是变长元组）

---

## 解码器 #3：Modbus RTU → modbus_c

### 协议概述

Modbus RTU 是工业通信协议，基于 UART，支持 Client-Server 架构。消息格式：Slave ID + Function Code + Data + CRC16。CRC 使用 Modbus 专用算法（初始值 0xFFFF，多项式 0xA001）。

### Python 元数据提取

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

### Python Options (3个)

| ID | 描述 | 默认值 | 可选值 |
|----|------|--------|--------|
| `scchannel` | Server→Client 通道 | 'RX' | ('RX', 'TX') |
| `cschannel` | Client→Server 通道 | 'TX' | ('RX', 'TX') |
| `framegap` | 帧间 bit 间隔 | 28 | (整数) |

### Python Annotations (15个)

| 索引 | ID | 描述 |
|------|----|------|
| 0 | `sc-server-id` | SC 服务器 ID |
| 1 | `sc-function` | SC 功能码 |
| 2 | `sc-crc` | SC CRC |
| 3 | `sc-address` | SC 地址 |
| 4 | `sc-data` | SC 数据 |
| 5 | `sc-length` | SC 长度 |
| 6 | `sc-error` | SC 错误 |
| 7 | `cs-server-id` | CS 服务器 ID |
| 8 | `cs-function` | CS 功能码 |
| 9 | `cs-crc` | CS CRC |
| 10 | `cs-address` | CS 地址 |
| 11 | `cs-data` | CS 数据 |
| 12 | `cs-length` | CS 长度 |
| 13 | `cs-error` | CS 错误 |
| 14 | `error-indication` | 帧错误指示 |

### Python Annotation Rows (3个)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `sc` | Server→client | (0,1,2,3,4,5,6) |
| `cs` | Client→server | (7,8,9,10,11,12,13) |
| `error-indicators` | Errors in frame | (14,) |

### 解码逻辑分析

**双通道架构**：Modbus 同时解码 SC（Server→Client）和 CS（Client→Server），通过选项指定哪个 UART 通道对应哪个方向。

**帧分隔**：通过 `framegap`（默认 28 bit times）判断帧边界。需要知道 `bitlength`（通过 STARTBIT/STOPBIT 的采样范围推算）。

**ADU 类层次**：
- `Modbus_ADU`（基类）：通用 CRC 计算、数据解析框架
- `Modbus_ADU_SC`（Server→Client）：解析响应帧
- `Modbus_ADU_CS`（Client→Server）：解析请求帧

**功能码处理**：
- 1-4：读取数据命令
- 5：写单个 Coil
- 6：写单个寄存器
- 7：读异常状态
- 8：诊断
- 11-12：通信事件计数器/日志
- 15-16：写多个 Coil/寄存器
- 17：报告 Server ID
- 22：掩码写寄存器
- 23：读/写多个寄存器
- >0x80：错误响应

**CRC 算法**：
```c
static uint16_t modbus_crc(uint8_t *data, int len) {
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}
```

### C 实现关键代码片段

```c
#define MODBUS_MAX_FRAME 256

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
    NUM_ANN,
};

typedef struct {
    uint8_t data[MODBUS_MAX_FRAME];
    uint64_t starts[MODBUS_MAX_FRAME]; // 每个字节的 start_sample
    uint64_t ends[MODBUS_MAX_FRAME];   // 每个字节的 end_sample
    int data_len;
    uint64_t last_read;
    int start_new_frame;
    int has_error;
    int last_byte_put;
    int minimum_length;
    int out_ann;
} modbus_adu;

typedef struct {
    modbus_adu adu_sc;
    modbus_adu adu_cs;
    double bitlength;
    int framegap;
    int sc_channel; // 0=RX, 1=TX
    int cs_channel; // 0=RX, 1=TX
    int out_ann;
    // 注意：Python modbus解码器未注册OUTPUT_PYTHON，C版本也不需要out_python
    // outputs=['modbus']仅用于协议标识，不实际输出Python protocol数据
    // <!-- Updated: Python modbus解码器未注册OUTPUT_PYTHON，C版本无需out_python字段 -->
} modbus_state;

static void modbus_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    modbus_state *s = (modbus_state *)c_decoder_get_private(di);
    if (!s) return;

    // 获取 bitlength
    if (s->bitlength == 0) {
        if (strcmp(cmd, "STARTBIT") == 0 || strcmp(cmd, "STOPBIT") == 0) {
            s->bitlength = (double)(end_sample - start_sample);
        } else {
            return; // 在知道 bitlength 之前不处理
        }
    }

    if (strcmp(cmd, "DATA") != 0) return;
    if (data_len < 2) return;

    uint8_t byte_val = data[0];
    uint8_t rxtx = data[1];

    // 分发到对应的 ADU
    if (rxtx == s->sc_channel)
        modbus_decode_adu(di, s, &s->adu_sc, start_sample, end_sample, byte_val, "sc-");
    if (rxtx == s->cs_channel)
        modbus_decode_adu(di, s, &s->adu_cs, start_sample, end_sample, byte_val, "cs-");
}
```

---

## 解码器 #4：PAN1321 → pan1321_c

### 协议概述

Panasonic PAN1321 是蓝牙 RF 模块，使用 Serial Port Profile (SPP) 通过 UART 通信。协议为 AT 命令格式，以 `\r\n` 结尾。

### Python 元数据提取

```python
id = 'pan1321'
name = 'PAN1321'
longname = 'Panasonic PAN1321'
desc = 'Bluetooth RF module with Serial Port Profile (SPP).'
license = 'gplv2+'
inputs = ['uart']
outputs = []
tags = ['Wireless/RF']
```

### Python Annotations (3个)

| 索引 | ID | 描述 |
|------|----|------|
| 0 | `text-verbose` | 详细可读文本 |
| 1 | `text` | 简短可读文本 |
| 2 | `warnings` | 警告文本 |

### Python Annotation Rows（未显式定义，默认单行）

Python 版本未定义 `annotation_rows`，C 版本需要定义。

### 解码逻辑分析

**核心流程**：
1. 仅处理 `"DATA"` 类型
2. 按 RX/TX 分别缓冲 ASCII 字符
3. 检测到 `\r\n` 时完成一条命令
4. RX 方向：设备回复（ROK, OK, ERR, 未知）
5. TX 方向：主机命令（AT+JAAC, AT+JPRO, AT+JRES, AT+JSDA, AT+JSEC, AT+JSLN, 其他）

**命令解析**：
- `AT+JAAC=<0|1>`：自动接受连接
- `AT+JPRO=<0|1>`：生产模式
- `AT+JRES`：软件复位
- `AT+JSDA=<len>,<data>`：发送数据
- `AT+JSEC=<secmode>,<linkkey>,<pintype>,<pinlen>,<pin>`：安全设置
- `AT+JSLN=<namelen>,<name>`：蓝牙名称

**复杂度**：低。主要是字符串匹配和参数解析。

### C 实现关键代码片段

```c
#define PAN1321_MAX_CMD 512

enum pan1321_ann {
    ANN_TEXT_VERBOSE = 0,
    ANN_TEXT,
    ANN_WARNINGS,
    NUM_ANN,
};

typedef struct {
    char cmd[2][PAN1321_MAX_CMD]; // [0]=RX, [1]=TX
    int cmd_len[2];
    uint64_t ss_block[2];
    int out_ann;
} pan1321_state;

static void pan1321_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    pan1321_state *s = (pan1321_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") != 0) return;
    if (data_len < 2) return;

    uint8_t byte_val = data[0];
    uint8_t rxtx = data[1];
    if (rxtx > 1) return;

    // 记录命令起始
    if (s->cmd_len[rxtx] == 0)
        s->ss_block[rxtx] = start_sample;

    // 缓冲字符
    if (s->cmd_len[rxtx] < PAN1321_MAX_CMD - 1) {
        s->cmd[rxtx][s->cmd_len[rxtx]++] = (char)byte_val;
        s->cmd[rxtx][s->cmd_len[rxtx]] = '\0';
    }

    // 检测 \r\n
    if (s->cmd_len[rxtx] >= 2 &&
        s->cmd[rxtx][s->cmd_len[rxtx]-2] == '\r' &&
        s->cmd[rxtx][s->cmd_len[rxtx]-1] == '\n') {
        // 移除 \r\n
        s->cmd[rxtx][s->cmd_len[rxtx]-2] = '\0';
        if (rxtx == 0)
            pan1321_handle_device_reply(di, s, s->cmd[rxtx]);
        else
            pan1321_handle_host_command(di, s, s->cmd[rxtx]);
        s->cmd_len[rxtx] = 0;
    }
}
```

---

## 解码器 #5：PN532 → pn532_c

### 协议概述

PN532 是 NFC 收发器芯片，通过 UART 通信。协议帧格式：Preamble + Start Code + Length + LCS + TFI + Data + DCS + Postamble。

### Python 元数据提取

```python
id = 'pn532'
name = 'PN532'
longname = 'PN532 nfc transceiver'
desc = 'PN532 chip command decoder'
license = 'gplv2+'
inputs = ['uart']
outputs = ['ISO14443']
tags = ['Automotive']
```

### Python Options (4个)

| ID | 描述 | 默认值 | 可选值 |
|----|------|--------|--------|
| `preamble` | 前导字节 | 0x00 | (整数) |
| `postamble` | 后导字节 | 0x00 | (整数) |
| `start frame` | 起始帧字节 | 0x00 | (整数) |
| `format` | 数据格式 | 'hex' | ('ascii','dec','hex','oct','bin') |

### Python Annotations (12个)

| 索引 | ID | 描述 |
|------|----|------|
| 0 | `start` | Start frame |
| 1 | `len` | Data length |
| 2 | `lcs` | Data length checksum |
| 3 | `tfi` | Frame identifier |
| 4 | `data` | Packet data |
| 5 | `dcs` | Data checksum |
| 6 | `end` | Postamble |
| 7 | `error` | Error description |
| 8 | `frame` | Frame type |
| 9 | `cmd` | Command |
| 10 | `preamble` | Preamble |
| 11 | `instruction` | Instruction |

### Python Annotation Rows (4个)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `data_vals` | Data | (0,1,2,3,4,5,6,10,11) |
| `frame_type` | Frame type | (8,) |
| `commands` | Commands | (9,) |
| `errors` | Errors | (7,) |

### 解码逻辑分析

**帧格式**：
```
00 00 FF LEN LCS TFI PD0 PD1 ... PDK DCS 00
|     |    |   |   |   |            |   |
|     |    |   |   |   |            |   Postamble
|     |    |   |   |   |            Data Checksum
|     |    |   |   |   Data bytes
|     |    |   |   TFI (0xD4=Host→PN532, 0xD5=PN532→Host)
|     |    |   Length Checksum (LEN + LCS = 0)
|     |    Length
|     Start Code
Preamble
```

**特殊帧**：
- ACK：`00 00 FF 00 FF 00`
- NACK：`00 00 FF FF 00 00`
- Extended frame：`00 00 FF FF FF ...`

**状态机**：
```
START_FRAME → LENGTH → TFI → DATA → CHECKSUM → END_FRAME
```

**Checksum 验证**：
- LCS：`LEN + LCS = 0`（模 256）
- DCS：`TFI + PD0 + PD1 + ... + PDK + DCS = 0`（模 256）

**命令解码**：根据 TFI 和第一个数据字节查找命令名称。

### C 实现关键代码片段

```c
#define PN532_MAX_DATA 256

enum pn532_state {
    PN532_START_FRAME,
    PN532_LENGTH,
    PN532_TFI,
    PN532_DATA,
    PN532_CHECKSUM,
    PN532_END_FRAME,
};

enum pn532_frame_type {
    FRAME_HOST_TO_PN532 = 0,
    FRAME_PN532_TO_HOST = 1,
    FRAME_ACK = 2,
    FRAME_NACK = 3,
    FRAME_ERROR = 4,
};

enum pn532_ann {
    ANN_START = 0,
    ANN_LEN,
    ANN_LCS,
    ANN_TFI,
    ANN_DATA,
    ANN_DCS,
    ANN_END,
    ANN_ERROR,
    ANN_FRAME,
    ANN_CMD,
    ANN_PREAMBLE,
    ANN_INSTRUCTION,
    NUM_ANN,
};

typedef struct {
    uint8_t byte_val;
    uint64_t ss;
    uint64_t es;
} pn532_byte_data;

typedef struct {
    enum pn532_state state;
    pn532_byte_data start_frame[3];
    int start_frame_idx;
    pn532_byte_data length[2];
    int length_idx;
    pn532_byte_data tfi;
    pn532_byte_data data_packet[PN532_MAX_DATA];
    int data_packet_len;
    int data_size;
    pn532_byte_data checksum;
    pn532_byte_data preamble;
    enum pn532_frame_type frame_type;
    int out_ann;
    int out_python; // 注意：Python pn532注册了OUTPUT_PYTHON但实际未调用self.put(..., self.out_python, ...)
    // C版本可保留out_python字段供未来上层解码器使用，但当前无需输出
    // <!-- Updated: Python pn532注册了OUTPUT_PYTHON但从未实际输出数据，C版本可保留但暂不输出 -->
    int format; // 0=hex, 1=ascii, 2=dec, 3=oct, 4=bin
} pn532_state;

static void pn532_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    pn532_state *s = (pn532_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") != 0) return;
    if (data_len < 1) return;

    pn532_byte_data bd;
    bd.byte_val = data[0];
    bd.ss = start_sample;
    bd.es = end_sample;

    switch (s->state) {
    case PN532_START_FRAME:
        pn532_handle_start_frame(di, s, &bd);
        break;
    case PN532_LENGTH:
        pn532_handle_length(di, s, &bd);
        break;
    case PN532_TFI:
        pn532_handle_tfi(di, s, &bd);
        break;
    case PN532_DATA:
        pn532_handle_data(di, s, &bd);
        break;
    case PN532_CHECKSUM:
        pn532_handle_checksum(di, s, &bd);
        break;
    case PN532_END_FRAME:
        pn532_handle_end_frame(di, s, &bd);
        break;
    }
}
```

---

## 通用实现规范

### 文件命名

所有 C 解码器文件放置在 `libsigrokdecode/c_decoders/` 目录下，命名格式：`{decoder_id}_c.c`

### srd_c_decoder 结构体字段规范

```c
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",                    // 必须以 "_c" 结尾
    .name = "XXX(C)",                 // 大写缩写 + "(C)"
    .longname = "Full Protocol Name (C)",
    .desc = "Protocol description (C implementation, faster than Python)",
    .license = "gplv2+",              // 或 "gplv3+" 视原 Python 版本
    .channels = NULL,                 // 上层解码器：NULL
    .num_channels = 0,                // 上层解码器：0
    .optional_channels = NULL,        // 上层解码器：NULL
    .num_optional_channels = 0,       // 上层解码器：0
    .options = xxx_options,           // 选项数组
    .num_options = N,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,     // 第一列必须为 ""
    .num_annotation_rows = M,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,             // {"uart"}
    .num_inputs = 1,
    .outputs = xxx_outputs,           // 视解码器而定
    .num_outputs = N,
    .binary = xxx_binary,             // 视解码器而定
    .num_binary = N,
    .tags = xxx_tags,
    .num_tags = N,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,             // 空函数
    .destroy = xxx_destroy,
    .recv_proto = xxx_recv_proto,     // 核心回调
    .metadata = xxx_metadata,         // 视需要（J1708 需要）
};
```

### ann_labels 规范

第一列必须为空字符串 `""`，后两列为缩写：

```c
static const char *xxx_ann_labels[][3] = {
    {"", "short-id", "Full description"},
    // ...
};
```

### annotation_rows 规范

每个 row 的 classes 数组以 `-1` 结尾：

```c
static const int xxx_row_xxx_classes[] = {ANN_XXX, ANN_YYY, -1};
static const struct srd_c_ann_row xxx_ann_rows[] = {
    {"row-id", "Row Name", xxx_row_xxx_classes, N},
    // ...
};
```

### srd_c_decoder_entry() 规范

选项默认值和可选值列表在 `srd_c_decoder_entry()` 中初始化：

```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    // 设置选项默认值
    xxx_options[0].def = g_variant_new_int64(default_value);
    // 设置选项可选值列表
    GSList *vals = NULL;
    vals = g_slist_append(vals, g_variant_new_string("value1"));
    vals = g_slist_append(vals, g_variant_new_string("value2"));
    xxx_options[0].values = vals;
    return &xxx_c_decoder;
}
```

### 输出 API 使用

- `C_ANN_PUT(di, ss, es, out_ann, ann_class, ...)`：输出 annotation
- `c_decoder_put_python(di, ss, es, out_python, cmd, data, data_len)`：输出 Python protocol
- `c_decoder_put_binary(di, ss, es, out_binary, bin_class, data_len, data)`：输出 binary
- `c_decoder_register_output(di, SRD_OUTPUT_ANN, "xxx")`：注册输出

### CMakeLists.txt 修改

将新解码器名称添加到 `C_DECODERS` 列表：

```cmake
set(C_DECODERS
    # ... 现有解码器 ...
    j1708_c
    midi_c
    modbus_c
    pan1321_c
    pn532_c
)
```

---

## Python → C 映射对照表

| Python 概念 | C 对应 |
|-------------|--------|
| `decode(ss, es, data)` | `recv_proto(di, ss, es, cmd, data, data_len)` |
| `ptype, rxtx, pdata = data` | `cmd` = ptype, `data[0]` = pdata[0], `data[1]` = rxtx |
| `self.put(ss, es, out_ann, [cls, [texts]])` | `C_ANN_PUT(di, ss, es, out_ann, cls, texts...)` |
| `self.register(srd.OUTPUT_ANN)` | `c_decoder_register_output(di, SRD_OUTPUT_ANN, "xxx")` |
| `self.register(srd.OUTPUT_BINARY)` | `c_decoder_register_output(di, SRD_OUTPUT_BINARY, "xxx")` |
| `self.register(srd.OUTPUT_PYTHON)` | `c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "xxx")` |
| `self.options['xxx']` | `c_decoder_get_option_int/string/double(di, "xxx", default)` |
| `self.samplerate` | `c_decoder_get_samplerate(di)` 或 metadata 回调 |
| `dict.get(key, default)` | 线性搜索查找表数组 |
| `bytearray.append(x)` | `s->data[s->data_len++] = x` |
| `getattr(self, 'handle_%s' % state)` | `switch(state)` 或函数指针数组 |

---

## 风险与注意事项

1. **MIDI 复杂度**：MIDI 解码器是本批次最复杂的，包含 Running Status、SysRealtime 中断、SysEx 厂商 ID 查找等机制。查找表数据量巨大（~800 行 Python），C 版本需要手动编码所有查找表。

2. **Modbus CRC**：必须精确实现 Modbus CRC-16 算法，初始值 0xFFFF，多项式 0xA001。

3. **J1708 Checksum**：二进制补码校验和，必须与 Python 版本完全一致。

4. **PN532 帧格式**：ACK/NACK 特殊帧需要特殊处理，Extended frame 格式暂未在 Python 版本中完整实现。

5. **字符串缓冲区**：C 版本需要预分配固定大小的缓冲区，注意防止溢出。建议使用 `snprintf` 替代 `sprintf`。

6. **内存管理**：所有私有状态通过 `g_malloc0` 分配，在 `destroy` 中通过 `g_free` 释放。

7. **Modbus 无 Python 输出**：Python modbus 解码器未注册 `OUTPUT_PYTHON`，C 版本的 `outputs=['modbus']` 仅用于协议标识，不需要 `c_decoder_put_python` 调用。 <!-- Updated: 新增Modbus Python输出说明 -->

8. **PN532 Python 输出未实现**：Python pn532 解码器注册了 `OUTPUT_PYTHON` 但从未实际调用 `self.put(..., self.out_python, ...)`，C 版本可保留 `out_python` 字段但暂不需要输出数据。 <!-- Updated: 新增PN532 Python输出说明 -->

9. **C 解码器依赖规则**：本批次所有 5 个解码器仅依赖 `uart_c`（已有 C 实现），不存在阻塞依赖。 <!-- Updated: 新增C解码器依赖规则说明 -->

10. **Modbus bitlength 获取方式**：Python 版本通过 `STARTBIT`/`STOPBIT` 的采样范围推算 `bitlength`，C 版本在 `recv_proto` 中处理 `STARTBIT`/`STOPBIT` 命令时计算。注意 `STARTBIT`/`STOPBIT` 的 `data` 格式与 `DATA` 不同（无 rxtx 字段）。 <!-- Updated: 新增Modbus bitlength获取方式说明 -->
