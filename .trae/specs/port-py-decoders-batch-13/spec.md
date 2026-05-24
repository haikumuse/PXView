# Python → C Decoder 移植规格书 (Batch 13)

## 概述

本文档详细描述 5 个 Python 协议解码器到 C 实现的移植规格。目标解码器：
- **z80** — Zilog Z80 CPU 反汇编器
- **adat** — ADAT lightpipe 音频协议
- **arm_etmv3** — ARM ETMv3 嵌入式跟踪宏单元
- **aud** — Renesas/Hitachi Advanced User Debugger
- **avr_pdi** — Atmel PDI (Program and Debug Interface)

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |
| uart_c.c | 底层协议输出范本 | c_decoder_put_python()输出协议数据、双通道(RX/TX)独立状态 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |


## 1. Z80 解码器

### 1.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `z80` |
| name | `Z80` |
| longname | `Zilog Z80 CPU` |
| desc | `Zilog Z80 microprocessor disassembly.` |
| license | `gplv3+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Retro computing']` |

### 1.2 Channels

**Required channels (11):**

| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0-7 | d0-d7 | D0-D7 | Data bus line 0-7 | (无idn) |
| 8 | m1 | /M1 | Machine cycle 1 | dec_z80_chan_m1 |
| 9 | rd | /RD | Memory or I/O read | dec_z80_chan_rd |
| 10 | wr | /WR | Memory or I/O write | dec_z80_chan_wr |

**Optional channels (18):**

| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 11 | mreq | /MREQ | Memory request | dec_z80_opt_chan_mreq |
| 12 | iorq | /IORQ | I/O request | dec_z80_opt_chan_iorq |
| 13-28 | a0-a15 | A0-A15 | Address bus line 0-15 | (无idn) |

### 1.3 Annotations (9 classes)

| 索引 | id | name |
|------|-----|------|
| 0 | addr | Memory or I/O address |
| 1 | memrd | Byte read from memory |
| 2 | memwr | Byte written to memory |
| 3 | iord | Byte read from I/O port |
| 4 | iowr | Byte written to I/O port |
| 5 | instr | Z80 CPU instruction |
| 6 | rop | Value of input operand |
| 7 | wop | Value of output operand |
| 8 | warn | Warning message |

### 1.4 Annotation Rows (5 rows)

| row id | name | classes |
|--------|------|---------|
| addrbus | Address bus | (0,) |
| databus | Data bus | (1, 2, 3, 4) |
| instructions | Instructions | (5,) |
| operands | Operands | (6, 7) |
| warnings | Warnings | (8,) |

### 1.5 Options

无 options。

### 1.6 解码逻辑分析

#### 核心状态机

Z80 解码器是一个**复杂的多层状态机**，使用函数指针作为状态（Python 中 `self.op_state = self.state_IDLE` 等）。状态包括：

1. **state_IDLE** — 等待 FETCH cycle，初始化指令解码参数
2. **state_PRE1** — 处理第一级前缀 (0xCB, 0xED, 0xDD, 0xFD)
3. **state_PRE2** — 处理 DD CB / FD CB 双前缀的 displacement
4. **state_PREDIS** — displacement 已获取，等待 opcode
5. **state_OPCODE** — opcode 已获取，查表确定指令属性
6. **state_POSTDIS** — 获取 post-displacement
7. **state_IMM1** — 获取第一字节立即数
8. **state_IMM2** — 获取第二字节立即数
9. **state_ROP1** — 读取第一字节操作数
10. **state_ROP2** — 读取第二字节操作数
11. **state_WOP1** — 写入第一字节操作数
12. **state_WOP2** — 写入第二字节操作数
13. **state_RESTART** — 指令结束，返回 IDLE

#### Bus Cycle 检测

Z80 的 bus cycle 由控制信号组合决定：

```c
// Cycle 类型枚举
enum z80_cycle {
    CYCLE_NONE, CYCLE_MEMRD, CYCLE_MEMWR,
    CYCLE_IORD, CYCLE_IOWR, CYCLE_FETCH, CYCLE_INTACK
};

// 检测逻辑：
// MREQ=0 && RD=0 && M1=0 → FETCH
// MREQ=0 && RD=0 && M1=1 → MEMRD
// MREQ=0 && WR=0 → MEMWR
// IORQ=0 && M1=0 → INTACK
// IORQ=0 && RD=0 → IORD
// IORQ=0 && WR=0 → IOWR
```

**关键点：** MREQ 和 IORQ 是 optional channels，默认值分别为 1（asserted）和 1（not asserted）。当 MREQ 未连接时默认为 1（不 assert），当 IORQ 未连接时默认为 1（不 assert）。

#### 指令表

Python 使用 `tables.py` 中的字典，包含 5 个指令表：
- `main_instructions` — 无前缀指令 (0x00-0xFF)
- `extended_instructions` — ED 前缀指令
- `bit_instructions` — CB 前缀指令
- `index_instructions` — DD/FD 前缀指令
- `index_bit_instructions` — DD CB / FD CB 前缀指令

每个条目格式：`(d, i, ro, wo, rep, format_string)`
- `d`: displacement 字节数
- `i`: 立即数字节数
- `ro`: 读取操作数字节数
- `wo`: 写入操作数字节数（负数表示 big endian）
- `rep`: 是否重复指令
- `format_string`: 汇编格式字符串

### 1.7 C 实现方案

#### 文件名
`z80_c.c`

#### struct srd_c_decoder

```c
.id = "z80_c",
.name = "Z80(C)",
.longname = "Zilog Z80 CPU(C)",
```

#### State struct

```c
struct z80_priv {
    // Bus cycle tracking
    int prev_cycle;
    int (*op_state)(struct srd_decoder_inst *di, struct z80_priv *s);

    // Bus data
    int bus_data;       // 当前数据总线值 (0-255), -1=未分配
    uint64_t addr_start;
    uint64_t data_start;
    uint64_t dasm_start;

    // Pending annotations
    int pend_addr;      // 待输出地址 (-1=无)
    int pend_data;      // 待输出数据
    int ann_data;       // 数据 annotation class
    int ann_dasm;       // 反汇编 annotation class

    // Instruction decode state
    int instr_len;
    int want_dis;
    int want_imm;
    int want_read;
    int want_write;
    int want_wr_be;     // write operand big-endian
    int op_repeat;
    int arg_dis;        // displacement (signed)
    int arg_imm;        // immediate value
    int arg_read;       // read operand value
    int arg_write;      // write operand value
    char arg_reg[4];    // register name ("IX", "IY", "")
    char mnemonic[64];  // format string
    int op_prefix;      // current prefix (0, 0xCB, 0xED, 0xDD, 0xFD, 0xDDCB, 0xFDCB)
    int instr_pend;
    int read_pend;
    int write_pend;

    int out_ann;
};
```

#### 指令表 C 实现

使用静态结构体数组，通过 opcode 索引查找：

```c
struct z80_instr {
    int8_t want_dis;
    int8_t want_imm;
    int8_t want_read;
    int8_t want_write;  // 负数 = big endian
    int8_t op_repeat;
    const char *mnemonic;
};

// 主指令表 [256]
static const struct z80_instr main_instr_table[256] = {
    [0x00] = {0, 0, 0, 0, 0, "NOP"},
    [0x01] = {0, 2, 0, 0, 0, "LD BC,{i:04H}h"},
    // ... 完整 256 条
};

// ED 前缀指令表 [256] (大部分为 NULL/Invalid)
// CB 前缀指令表 [256]
// DD/FD 前缀指令表 [256]
// DD CB / FD CB 前缀指令表 [256]
```

**关键：** 需要将 `tables.py` 中的所有 5 个指令表完整翻译为 C 静态数组。

#### Cycle 检测实现

```c
static int detect_cycle(const uint8_t *pins, int has_mreq, int has_iorq) {
    int mreq = has_mreq ? pins[11] : 1;  // 默认 asserted (active low)
    int iorq = has_iorq ? pins[12] : 1;  // 默认 not asserted
    int m1 = pins[8];
    int rd = pins[9];
    int wr = pins[10];

    if (mreq == 0) {  // MREQ asserted
        if (rd == 0) {
            return (m1 == 0) ? CYCLE_FETCH : CYCLE_MEMRD;
        } else if (wr == 0) {
            return CYCLE_MEMWR;
        }
    } else if (iorq == 0) {  // IORQ asserted
        if (m1 == 0) return CYCLE_INTACK;
        else if (rd == 0) return CYCLE_IORD;
        else if (wr == 0) return CYCLE_IOWR;
    }
    return CYCLE_NONE;
}
```

#### Bus 数据读取

```c
static int reduce_bus(const uint8_t *pins, int start, int end) {
    int val = 0;
    for (int i = start; i <= end; i++) {
        if (pins[i] == 0xFF) return -1;  // unassigned
        val = (val << 1) | pins[i];
    }
    return val;
}
```

#### Condition Builder 使用

Z80 解码器不使用 samplerate，不需要 `metadata` 回调。decode 循环使用 `self.wait()` 无条件等待任意变化：

```c
// decode() 主循环
while (1) {
    srd_cond_builder *cb = c_cond_new();
    // 等待任意引脚变化 - 使用 skip(1) 作为最小条件
    c_cond_skip(cb, 1);
    uint64_t samplenum, matched;
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;

    // 读取所有引脚
    uint8_t pins[29];
    for (int i = 0; i < 29; i++) {
        pins[i] = c_decoder_get_pin(di, i, samplenum);
    }

    // 检测 cycle 类型
    int cycle = detect_cycle(pins, has_mreq, has_iorq);
    // ... 处理 cycle 转换
}
```

**注意：** Z80 的 `self.wait()` 没有条件，这意味着它等待任意采样点变化。在 C 中，使用 `c_cond_skip(cb, 1)` 逐采样推进效率太低。优化方案：等待任意引脚的 edge：

<!-- Updated: 也可使用 c_cond_wait_current(di, &samplenum) 等效 Python self.wait(None)，但 Z80 需要等待变化而非获取当前值，因此 edge 方案更合适 -->

```c
// 优化：等待控制信号变化
srd_cond_builder *cb = c_cond_new();
c_cond_edge(cb, 8);  // M1
c_cond_or(cb);
c_cond_edge(cb, 9);  // RD
c_cond_or(cb);
c_cond_edge(cb, 10); // WR
// 如果有 MREQ/IORQ 也加入
c_cond_or(cb);
c_cond_edge(cb, 11);
c_cond_or(cb);
c_cond_edge(cb, 12);
```

#### 格式化输出

Python 使用自定义 `AsmFormatter` 处理 `{i:04H}h` 格式。C 中需手动实现：

```c
static void format_hex(char *buf, size_t sz, int value, int width) {
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%0*X", width, value);
    // 如果首字符不是数字，加前导 '0'
    if (tmp[0] >= '0' && tmp[0] <= '9') {
        snprintf(buf, sz, "%sH", tmp);
    } else {
        snprintf(buf, sz, "0%sH", tmp);
    }
}
```

### 1.8 复杂度评估

**极高。** 这是本批次最复杂的解码器：
- 29 个 channels（11 required + 18 optional）
- 5 个指令表，总计约 1000+ 条指令
- 13 个状态的状态机
- 自定义格式化器
- 无 samplerate 依赖

**建议：** 优先实现核心状态机和主指令表，ED/CB/DD/FD 前缀指令表可后续补充。

---

## 2. ADAT 解码器

### 2.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `adat` |
| name | `ADAT` |
| longname | `ADAT lightpipe decoder` |
| desc | `Decodes the ADAT protocol` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Audio']` |

### 2.2 Channels

| 索引 | id | name | desc | type | idn |
|------|-----|------|------|------|-----|
| 0 | adat | ADAT | ADAT data line | SDATA | (无) |

### 2.3 Annotations (15 classes)

| 索引 | id | name |
|------|-----|------|
| 0 | bit | bit |
| 1 | sync | SYNC pad |
| 2 | user-bits | user bits |
| 3 | nibble | nibbles |
| 4 | error | error |
| 5 | channel | channel data |
| 6 | frame-user-data | frame user data |
| 7 | channel-0 | channel 0 data |
| 8 | channel-1 | channel 1 data |
| 9 | channel-2 | channel 2 data |
| 10 | channel-3 | channel 3 data |
| 11 | channel-4 | channel 4 data |
| 12 | channel-5 | channel 5 data |
| 13 | channel-6 | channel 6 data |
| 14 | channel-7 | channel 7 data |

### 2.4 Annotation Rows (12 rows)

| row id | name | classes |
|--------|------|---------|
| bits | Bits | (0,) |
| nibbles | Nibbles | (3, 4) |
| fields | Fields | (1, 2, 5) |
| user-data | Frame User Data | (6,) |
| channel0 | Channel 0 Data | (7,) |
| channel1 | Channel 1 Data | (8,) |
| channel2 | Channel 2 Data | (9,) |
| channel3 | Channel 3 Data | (10,) |
| channel4 | Channel 4 Data | (11,) |
| channel5 | Channel 5 Data | (12,) |
| channel6 | Channel 6 Data | (13,) |
| channel7 | Channel 7 Data | (14,) |

### 2.5 Options (3)

| id | desc | default | values | type |
|----|------|---------|--------|------|
| samplerate | audio sample rate | 48000 | - | int |
| sample_display | How to display the channel samples | decimal | (decimal, hexadecimal) | string |
| annotations | Which set of annotations to display | both | (intra-frame, per-frame, both) | string |

### 2.6 解码逻辑分析

#### 核心算法

ADAT 使用 NRZI 编码 + 4b/5b 编码：

1. **信号采样**：在每个信号 edge 处，根据距上一个 edge 的时间差计算 bit 数，用 NRZI 解码填充 signal buffer
2. **SYNC 检测**：查找 `1,0,0,0,0,0,0,0,0,0,0` (11 bits) 模式
3. **User bits 解码**：5 bits 一组，首 bit 必须为 1（4b/5b），后 4 bits 为 user data
4. **Channel data 解码**：每 channel 6 个 nibble（5 bits/nibble），首 bit 为 1（4b/5b），后 4 bits 为数据
5. **每帧 8 channels**，每 channel 24-bit 数据

#### 状态机

```
SYNC → USER BITS → CHANNEL DATA → (循环)
```

- **SYNC**：丢弃 bits 直到找到 sync pad 模式
- **USER BITS**：累积 5 bits，验证首 bit=1，提取 4-bit user data
- **CHANNEL DATA**：累积 5 bits/nibble，6 nibble/channel，8 channels/frame

#### Samplerate 依赖

- `bit_time = samplerate / (256 * audio_samplerate)`
- 最低采样率要求：`2.5 * 256 * audio_samplerate`
- 默认 audio_samplerate = 48000 → 最低 30.72 MHz

### 2.7 C 实现方案

#### 文件名
`adat_c.c`

#### State struct

```c
struct adat_priv {
    uint64_t samplerate;
    double bit_time;
    int bit_time_int;
    int sample_display_hex;  // 0=decimal, 1=hexadecimal
    int annotations_mode;    // 0=intra-frame, 1=per-frame, 2=both

    // Signal buffer (环形缓冲区)
    int signal[512];         // 解码后的 bit 值
    uint64_t times[512];     // 对应的 sample 时间
    int signal_len;

    // 解码状态
    int state;               // 0=SYNC, 1=USER_BITS, 2=CHANNEL_DATA
    int channel_no;
    int nibble_no;
    uint32_t channel_data;
    uint64_t channel_start_time;
    uint32_t all_channels_data[8];
    uint64_t frame_start_time;
    uint32_t frame_user_data;

    int out_ann;
};
```

#### Condition Builder 使用

```c
// 等待 ADAT 信号 edge
srd_cond_builder *cb = c_cond_new();
c_cond_edge(cb, 0);
uint64_t samplenum, matched;
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

#### NRZI 解码关键代码

```c
// 在 edge 到来时，根据时间差计算 bit 数
uint64_t diff = samplenum - last_time;
int num_bits = (int)(diff / s->bit_time + 0.5);

for (int i = 0; i < num_bits; i++) {
    uint64_t t = last_time + (uint64_t)(s->bit_time * i + 0.5);
    int bit = (i == 0) ? 1 : 0;  // NRZI: 第一个 bit 是 1，后续是 0
    s->signal[s->signal_len] = bit;
    s->times[s->signal_len] = t;
    s->signal_len++;
}
```

#### Sign extend 辅助函数

```c
static int32_t sign_extend_24bit(uint32_t x) {
    if (x & 0x800000)
        return -(0x800000 - (x & 0x7fffff));
    return (int32_t)x;
}
```

### 2.8 复杂度评估

**中等。** 核心逻辑不复杂，但需要：
- samplerate 依赖 + 最低采样率检查
- NRZI 解码 + 4b/5b 验证
- 环形缓冲区管理
- 3 个 options
- 15 个 annotation classes

---

## 3. ARM ETMv3 解码器

### 3.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `arm_etmv3` |
| name | `ARM ETMv3` |
| longname | `ARM Embedded Trace Macroblock v3` |
| desc | `ARM ETM v3 instruction trace protocol.` |
| license | `gplv2+` |
| inputs | `['uart']` |
| outputs | `[]` |
| tags | `['Debug/trace']` |

### 3.2 Channels

**无 channels。** 此解码器 stack 在 `uart` 之上，接收 UART 解码后的字节流。

<!-- Updated: arm_etmv3 依赖 uart 输入，uart_c.c 已有 C 实现，因此 arm_etmv3_c 可直接依赖 uart_c，不阻塞 -->

### 3.3 Annotations (11 classes)

| 索引 | id | name |
|------|-----|------|
| 0 | trace | Trace info |
| 1 | branch | Branches |
| 2 | exception | Exceptions |
| 3 | execution | Instruction execution |
| 4 | data | Data access |
| 5 | pc | Program counter |
| 6 | instr_e | Executed instructions |
| 7 | instr_n | Not executed instructions |
| 8 | source | Source code |
| 9 | location | Current location |
| 10 | function | Current function |

### 3.4 Annotation Rows (8 rows)

| row id | name | classes |
|--------|------|---------|
| trace | Trace info | (0,) |
| flow | Code flow | (1, 2, 3) |
| data | Data access | (4,) |
| pc | Program counter | (5,) |
| instruction | Instructions | (6, 7) |
| source | Source code | (8,) |
| location | Current location | (9,) |
| function | Current function | (10,) |

### 3.5 Options (4)

| id | desc | default | values | type |
|----|------|---------|--------|------|
| objdump | objdump path | arm-none-eabi-objdump | - | string |
| objdump_opts | objdump options | -lSC | - | string |
| elffile | .elf path | (空) | - | string |
| branch_enc | Branch encoding | alternative | (alternative, original) | string |

### 3.6 解码逻辑分析

#### 输入格式

此解码器接收 UART 层的 Python output，格式为 `(ptype, rxtx, pdata)`，其中 `ptype='DATA'` 时 `pdata` 包含一个字节。

**关键：** C 解码器中，stack 在 uart_c 之上时，通过 `c_decoder_put_python` 接收数据。需要实现 `recv_proto` 回调来接收上层解码器的输出。

#### 包类型检测

`get_packet_type()` 函数根据第一个字节识别包类型：

```c
static const char *get_packet_type(uint8_t byte) {
    if (byte & 0x01) return "branch";
    if (byte == 0x00) return "a_sync";
    if (byte == 0x04) return "cyclecount";
    if (byte == 0x08) return "i_sync";
    if (byte == 0x0C) return "trigger";
    if ((byte & 0xF3) == 0x20 || (byte & 0xF3) == 0x40 || (byte & 0xF3) == 0x60)
        return "ooo_data";
    if (byte == 0x50) return "store_failed";
    if (byte == 0x70) return "i_sync";
    if ((byte & 0xDF) == 0x54 || (byte & 0xDF) == 0x58 || (byte & 0xDF) == 0x5C)
        return "ooo_place";
    if (byte == 0x3C) return "vmid";
    if ((byte & 0xD3) == 0x02) return "data";
    if ((byte & 0xFB) == 0x42) return "timestamp";
    if (byte == 0x62) return "data_suppressed";
    if (byte == 0x66) return "ignore";
    if ((byte & 0xEF) == 0x6A) return "value_not_traced";
    if (byte == 0x6E) return "context_id";
    if (byte == 0x76) return "exception_exit";
    if (byte == 0x7E) return "exception_entry";
    if ((byte & 0x81) == 0x80) return "p_header";
    return "unknown";
}
```

#### 关键处理函数

1. **handle_a_sync** — 检测同步包 `[0x00, 0x00, 0x00, 0x00, 0x80]`
2. **handle_i_sync** — 解析 I-Sync 包，更新 PC 和 CPU 状态
3. **handle_branch** — 解析分支地址，支持 varint 编码
4. **handle_p_header** — 解析指令执行状态
5. **handle_exception_entry/exit** — 异常处理

#### Varint 解析

```c
// 解析变长整数（top bit = continuation bit）
static int parse_varint(const uint8_t *bytes, int len, uint32_t *value, int *parsed_len) {
    uint32_t v = 0;
    for (int i = 0; i < len; i++) {
        v |= (bytes[i] & 0x7F) << (i * 7);
        if ((bytes[i] & 0x80) == 0) {
            *value = v;
            *parsed_len = i + 1;
            return 0;
        }
    }
    return -1;  // 不完整
}
```

#### objdump 集成

Python 版本使用 `subprocess` 调用 objdump 解析 ELF 文件。**C 版本应跳过此功能**，因为：
1. C 解码器 DLL 中不应调用外部进程
2. 此功能为可选增强，不影响核心解码
3. 可在后续版本中通过 Python wrapper 实现

**C 实现中 `objdump`、`objdump_opts`、`elffile` 三个 options 保留但忽略。**

### 3.7 C 实现方案

#### 文件名
`arm_etmv3_c.c`

#### State struct

```c
struct etmv3_priv {
    uint8_t buf[64];        // 当前包缓冲区
    int buf_len;
    uint8_t syncbuf[8];     // 同步检测缓冲区
    int syncbuf_len;
    uint64_t prevsample;
    uint64_t startsample;
    uint64_t byte_len;
    uint32_t last_branch;
    int cpu_state;          // 0=arm, 1=thumb, 2=jazelle
    uint32_t current_pc;

    // Location/function tracking (简化版，无 objdump)
    uint64_t current_loc_ss;
    uint64_t current_loc_es;
    char current_loc[256];
    uint64_t current_func_ss;
    uint64_t current_func_es;
    char current_func[256];

    // Branch encoding
    int branch_enc_alt;     // 1=alternative, 0=original

    int out_ann;
};
```

#### recv_proto 回调

```c
static void arm_etmv3_recv_proto(struct srd_decoder_inst *di,
    uint64_t ss, uint64_t es, const char *proto_type,
    const uint8_t *data, int data_len)
{
    struct etmv3_priv *s = (struct etmv3_priv *)c_decoder_get_private(di);

    // 只处理 DATA 类型
    if (strcmp(proto_type, "DATA") != 0 || data_len < 2) return;

    // <!-- Updated: uart_c.c DATA 输出格式为 data[0]=byte_value, data[1]=rxtx，与原 spec 描述相反 -->
    uint8_t byte = data[0];   // 实际字节值
    uint8_t rxtx = data[1];   // 0=RX, 1=TX
    // ... 处理字节流
}
```

#### uart_c.c 输出格式文档

<!-- Updated: uart_c.c 实际输出格式已与代码核对，修正了 DATA/FRAME/STARTBIT/STOPBIT/PARITYBIT 等命令的 data 布局 -->
arm_etmv3_c 通过 `recv_proto` 回调接收上层 uart_c 解码器的输出。uart_c.c 通过 `c_decoder_put_python()` 输出以下协议命令：

| cmd (proto_type) | data 布局 | 说明 |
|------------------|-----------|------|
| `"DATA"` | data[0]=byte_value, data[1]=rxtx (0=RX,1=TX) | 接收/发送的数据字节 |
| `"FRAME"` | data[0]=datavalue, data[1]=rxtx, data[2]=frame_valid (0/1) | 完整 UART 帧 |
| `"IDLE"` | data[0]=rxtx | 空闲检测 |
| `"BREAK"` | data[0]=rxtx | Break 条件检测 |
| `"STARTBIT"` | data[0]=start_bit_value | 有效起始位 |
| `"STOPBIT"` | data[0]=stop_bit_value | 有效停止位 |
| `"PARITYBIT"` | data[0]=parity_bit | 校验位 |
| `"INVALID STARTBIT"` | data[0]=start_bit_value | 起始位错误 |
| `"INVALID STOPBIT"` | data[0]=stop_bit_value | 停止位错误 |
| `"PARITY ERROR"` | data[0]=expected_parity, data[1]=actual_parity | 校验错误 |

**注意：** arm_etmv3_c 只需处理 `"DATA"` 命令，忽略其他命令类型。

### 3.8 复杂度评估

**高。** 原因：
- Stack 在 uart 之上，需要 `recv_proto` 回调
- 复杂的包类型检测和解析
- Varint 编码解析
- 分支地址解码（含 CPU 状态切换）
- 异常信息解析
- objdump 集成（C 版本跳过）

---

## 4. AUD 解码器

### 4.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `aud` |
| name | `AUD` |
| longname | `Advanced User Debugger` |
| desc | `Renesas/Hitachi Advanced User Debugger (AUD) protocol.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Debug/trace']` |

### 4.2 Channels (6)

| 索引 | id | name | desc | type | idn |
|------|-----|------|------|------|-----|
| 0 | audck | AUDCK | AUD clock | SCLK | dec_aud_chan_audck |
| 1 | naudsync | nAUDSYNC | AUD sync | SDATA | dec_aud_chan_naudsync |
| 2 | audata3 | AUDATA3 | AUD data line 3 | ADATA | dec_aud_chan_audata3 |
| 3 | audata2 | AUDATA2 | AUD data line 2 | ADATA | dec_aud_chan_audata2 |
| 4 | audata1 | AUDATA1 | AUD data line 1 | ADATA | dec_aud_chan_audata1 |
| 5 | audata0 | AUDATA0 | AUD data line 0 | ADATA | dec_aud_chan_audata0 |

### 4.3 Annotations (1 class)

| 索引 | id | name |
|------|-----|------|
| 0 | dest | Destination address |

### 4.4 Annotation Rows (1 row)

| row id | name | classes |
|--------|------|---------|
| addresses | Addresses | (0,) |

### 4.5 Options

无。

### 4.6 解码逻辑分析

#### 核心算法

AUD 是一个**非常简单**的解码器：

1. 等待 AUDCK 上升沿
2. 读取 4 条数据线组成 nibble（audata3 为 MSB）
3. 当 nAUDSYNC=1 时：
   - 如果已完成地址移位（ncnt == nmax），输出地址 annotation
   - 重置计数器，根据 nibble 值确定地址长度：
     - 0x08 → 1 nibble (4-bit 地址)
     - 0x09 → 2 nibbles (8-bit 地址)
     - 0x0A → 4 nibbles (16-bit 地址)
     - 0x0B → 8 nibbles (32-bit 地址)
     - 其他 → 未定义/idle
4. 当 nAUDSYNC=0 且 nmax > 0 时：
   - 将 nibble 移入地址寄存器

#### 关键点

- 地址从 lastaddr 开始，每次 sync=1 时重置为 lastaddr
- Nibble 移入顺序：从低位到高位（ncnt * 4 位偏移）
- 仅在 Branch Trace 模式下工作

### 4.7 C 实现方案

#### 文件名
`aud_c.c`

#### State struct

```c
struct aud_priv {
    int ncnt;           // 当前已移入的 nibble 计数
    int nmax;           // 目标 nibble 数量
    uint32_t addr;      // 当前地址
    uint32_t lastaddr;  // 上一次完成的地址
    uint64_t ss;        // annotation 起始 sample

    int out_ann;
};
```

#### Condition Builder 使用

```c
// 等待 AUDCK 上升沿
srd_cond_builder *cb = c_cond_new();
c_cond_rise(cb, 0);  // channel 0 = AUDCK
uint64_t samplenum, matched;
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

#### Nibble 重建

```c
static int reconstruct_nibble(const uint8_t *pins) {
    int nib = 0;
    nib |= pins[5];        // audata0 → bit 0
    nib |= pins[4] << 1;   // audata1 → bit 1
    nib |= pins[3] << 2;   // audata2 → bit 2
    nib |= pins[2] << 3;   // audata3 → bit 3
    return nib;
}
```

### 4.8 复杂度评估

**低。** 最简单的解码器之一：
- 6 个 channels
- 1 个 annotation class
- 无 options
- 无 samplerate 依赖
- 简单的 nibble 移位逻辑

---

## 5. AVR PDI 解码器

### 5.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `avr_pdi` |
| name | `AVR PDI` |
| longname | `Atmel Program and Debug Interface` |
| desc | `Atmel ATxmega Program and Debug Interface (PDI) protocol.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Debug/trace']` |

### 5.2 Channels (2)

| 索引 | id | name | desc | type | idn |
|------|-----|------|------|------|-----|
| 0 | reset | RESET | RESET / PDI_CLK | SCLK | dec_avr_pdi_chan_reset |
| 1 | data | DATA | PDI_DATA | SDATA | dec_avr_pdi_chan_data |

### 5.3 Annotations (15 classes)

| 索引 | id | name |
|------|-----|------|
| 0 | uart-bit | UART bit |
| 1 | start-bit | Start bit |
| 2 | data-bit | Data bit |
| 3 | parity-ok | Parity OK bit |
| 4 | parity-err | Parity error bit |
| 5 | stop-ok | Stop OK bit |
| 6 | stop-err | Stop error bit |
| 7 | break | BREAK condition |
| 8 | opcode | Instruction opcode |
| 9 | data-prog | Programmer data |
| 10 | data-dev | Device data |
| 11 | pdi-break | BREAK at PDI level |
| 12 | enable | Enable PDI |
| 13 | disable | Disable PDI |
| 14 | cmd-data | PDI command with data |

### 5.4 Annotation Rows (4 rows)

| row id | name | classes |
|--------|------|---------|
| uart_bits | UART bits | (0,) |
| uart_fields | UART fields | (1, 2, 3, 4, 5, 6, 7) |
| pdi_fields | PDI fields | (8, 9, 10, 11) |
| pdi_cmds | PDI Cmds | (12, 13, 14) |

### 5.5 Binary Output (1 class)

| 索引 | id | name |
|------|-----|------|
| 0 | bytes | PDI protocol bytes |

### 5.6 Options

无。

### 5.7 解码逻辑分析

#### 双层解码架构

AVR PDI 解码器包含两层：

**第一层：UART 帧**
- RESET pin 作为时钟（PDI_CLK）
- DATA pin 作为数据
- 上升沿采样数据，下降沿标记 bit 边界
- 帧格式：1 start bit + 8 data bits + 1 even parity bit + 1 stop bit
- BREAK 检测：连续 11+ 个 0 bit

**第二层：PDI 指令**
- 8 种 opcode：LDS, LD, STS, ST, LDCS, STCS, REPEAT, KEY
- 每个 opcode 有不同的操作数格式
- 支持 REPEAT 前缀
- Little-endian 多字节数据

#### UART 帧处理流程

```
handle_clk_edge() → handle_bits() → handle_byte()
```

1. **handle_clk_edge**：
   - 上升沿：采样 DATA pin
   - 下降沿：处理上一个 bit slot

2. **handle_bits**：
   - 检测 BREAK（连续 11+ 个 0）
   - 累积 11 bits 组成帧
   - 解析 start/data/parity/stop
   - 验证 parity 和 stop bit

3. **handle_byte**：
   - 解码 opcode 和操作数
   - 处理多字节数据项
   - 管理 REPEAT 计数

#### PDI 指令解码

```c
// Opcode 编码
enum pdi_opcode {
    OP_LDS = 0, OP_LD = 1, OP_STS = 2, OP_ST = 3,
    OP_LDCS = 4, OP_REPEAT = 5, OP_STCS = 6, OP_KEY = 7
};

// 从字节提取 opcode
opcode = (byteval & 0xe0) >> 5;
arg30 = byteval & 0x0f;
arg32 = (byteval & 0x0c) >> 2;
arg10 = byteval & 0x03;
```

#### 控制寄存器名称

```c
static const char *ctrl_reg_name(int reg) {
    switch (reg) {
        case 0: return "status";
        case 1: return "reset";
        case 2: return "ctrl";
        default: return NULL;  // 使用 "rN" 格式
    }
}
```

### 5.8 C 实现方案

#### 文件名
`avr_pdi_c.c`

#### State struct

```c
struct pdi_bit {
    int val;
    uint64_t ss;
    uint64_t es;
};

struct avr_pdi_priv {
    uint64_t samplerate;

    // Clock edge tracking
    uint64_t ss_last_fall;
    uint64_t ss_curr_fall;
    int data_sample;

    // UART frame bits
    struct pdi_bit bits[12];  // max 1+8+1+1 = 11 bits
    int bit_count;

    // BREAK detection
    int zero_count;
    uint64_t zero_ss;
    uint64_t break_ss;
    uint64_t break_es;

    // PDI instruction state
    int insn_rep_count;
    int insn_opcode;
    uint8_t insn_dat_bytes[8];
    int insn_dat_count;
    uint64_t insn_ss_data;
    uint64_t cmd_ss;
    char cmd_parts_nice[256];
    char cmd_parts_terse[256];
    int insn_write_counts;
    int insn_read_counts;
    int width_addr;
    int width_data;
    const char *ptr_txt;
    const char *ptr_txt_terse;
    int reg_num;
    const char *reg_txt;
    const char *reg_txt_terse;

    int out_ann;
    int out_binary;
};
```

#### Condition Builder 使用

```c
// 等待 RESET pin (clock) 的 edge
srd_cond_builder *cb = c_cond_new();
c_cond_edge(cb, 0);  // channel 0 = RESET/PDI_CLK
uint64_t samplenum, matched;
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

#### Parity 检查

```c
static int count_ones(uint8_t val) {
    int count = 0;
    while (val) {
        count += val & 1;
        val >>= 1;
    }
    return count;
}

static int parity_even_ok(uint8_t data_val, int parity_bit) {
    return (count_ones(data_val) + parity_bit) % 2 == 0;
}
```

#### Binary 输出

```c
// 有效帧时输出 binary
if (valid_frame) {
    c_decoder_put_binary(di, byte_ss, byte_es, s->out_binary, 0, &data_val, 1);
}
```

### 5.9 复杂度评估

**中高。** 原因：
- 双层解码（UART + PDI 指令）
- BREAK 检测逻辑
- 8 种 opcode 的不同操作数格式
- REPEAT 前缀支持
- Binary 输出
- 但 channels 少（仅 2 个），无 options

---

## 通用实现注意事项

### Samplerate Guard 模式

所有需要 samplerate 的解码器（adat, avr_pdi）必须实现：

<!-- Updated: metadata 回调签名应为 (int key, uint64_t value)，而非 (int key, const void *value) -->
```c
static void xxx_metadata(struct srd_decoder_inst *di, int key, uint64_t value) {
    if (key == SRD_CONF_SAMPLERATE) {
        struct xxx_priv *s = (struct xxx_priv *)c_decoder_get_private(di);
        s->samplerate = value;
        // 计算派生参数
    }
}

// 在 decode() 开头
if (s->samplerate == 0) {
    s->samplerate = c_decoder_get_samplerate(di);
    if (s->samplerate == 0) return;
}
```

### ann_labels 格式

```c
// 第一列必须为空字符串 ""，API 处理 i+7 偏移
static const char *xxx_ann_labels[][3] = {
    {"", "id", "name"},   // class 0
    {"", "id", "name"},   // class 1
    // ...
};
```

### annotation_rows 中的 class 数组

```c
// 必须以 -1 结尾
static const int row_xxx_classes[] = {0, 1, 2, -1};

static const struct srd_c_ann_row xxx_ann_rows[] = {
    {"rowid", "Row Name", row_xxx_classes, 3},  // 3 = 元素数(不含 -1)
};
```

### CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加：

```
z80_c
adat_c
arm_etmv3_c
aud_c
avr_pdi_c
```

### recv_proto (仅 arm_etmv3)

```c
// 在 srd_c_decoder 结构体中设置
.recv_proto = arm_etmv3_recv_proto,
```

### Binary 输出 (仅 avr_pdi)

```c
// binary class 定义
static const char *avr_pdi_binary_labels[][3] = {
    {"", "bytes", "PDI protocol bytes"},
};

// 在 srd_c_decoder 中
.binary = avr_pdi_binary_labels,
.num_binary = 1,
```

---

## 各解码器实现优先级

| 优先级 | 解码器 | 复杂度 | 预估代码行数 |
|--------|--------|--------|-------------|
| 1 | aud | 低 | ~150 行 |
| 2 | adat | 中 | ~400 行 |
| 3 | avr_pdi | 中高 | ~500 行 |
| 4 | arm_etmv3 | 高 | ~600 行 |
| 5 | z80 | 极高 | ~2000+ 行 (含指令表) |

---

## 已实现的 C Decoder API 更新记录

<!-- Updated: 以下 API 已在代码中实现，spec 中引用时无需标注"暂不支持" -->

| API | 状态 | 说明 |
|-----|------|------|
| `c_decoder_put_logic()` | ✅ 已实现 | 输出 SRD_OUTPUT_LOGIC 类型数据，通过 `c_decoder_register_output(di, SRD_OUTPUT_LOGIC, ...)` 注册 |
| `SRD_OUTPUT_LOGIC` | ✅ 已实现 | 逻辑信号输出类型，用于上层解码器接收 |
| `c_cond_wait_current()` | ✅ 已实现 | 等效于 Python `self.wait({})` / `self.wait(None)`，获取当前采样位置 |
| `c_decoder_get_initial_pin()` | ✅ 已实现 | 获取解码开始前的初始引脚状态，返回 0xFF 表示未连接 |
| BITS v2 格式 | ✅ 已实现 | spi_c.c 和 i2c_c.c 已实现 per-bit ss/es 时间戳格式 |
| SPI DATA 17字节格式 | ✅ 已实现 | spi_c.c 已实现 data[0]=flags + data[1..8]=mosi + data[9..16]=miso |
| uart_c.c IDLE/BREAK | ✅ 已实现 | uart_c.c 已添加 IDLE 和 BREAK 检测输出 |
| ps2_c.c Python 输出 | ✅ 已实现 | ps2_c.c 已添加 `c_decoder_put_python()` 输出 |
| type_decoder.c heap bug | ✅ 已修复 | `free(str)` 已改为 `g_free(str)` |
| `recv_proto` 回调 | ✅ 已实现 | lm75_c.c 和 ds1307_c.c 已实现 recv_proto 回调 |
