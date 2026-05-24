# Python→C 解码器移植规格 — Batch 09

本批次包含 5 个解码器的 Python→C 移植规格：**jitter, lfast, maple_bus, miller, morse**。

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |

## 1. jitter — 时序抖动计算

### 1.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `jitter` |
| name | `Jitter` |
| longname | `Timing jitter calculation` |
| desc | `Retrieves the timing jitter between two digital signals.` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Clock/timing', 'Util']` |

**channels**:
| # | id | name | desc | idn |
|---|-----|------|------|-----|
| 0 | clk | Clock | Clock reference channel | `dec_jitter_chan_clk` |
| 1 | sig | Resulting signal | Resulting signal controlled by the clock | `dec_jitter_chan_sig` |

**options**:
| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| clk_polarity | Clock edge polarity | `'rising'` | `('rising', 'falling', 'both')` | `dec_jitter_opt_clk_polarity` |
| sig_polarity | Resulting signal edge polarity | `'rising'` | `('rising', 'falling', 'both')` | `dec_jitter_opt_sig_polarity` |

**annotations** (3):
| # | id | label |
|---|-----|-------|
| 0 | jitter | Jitter value |
| 1 | clk_missed | Clock missed |
| 2 | sig_missed | Signal missed |

**annotation_rows** (3):
| id | label | classes |
|----|-------|---------|
| jitter | Jitter values | (0,) |
| clk_missed | Clock missed | (1,) |
| sig_missed | Signal missed | (2,) |

**binary** (1):
| # | id | label |
|---|-----|-------|
| 0 | ascii-float | Jitter values as newline-separated ASCII floats |

### 1.2 Python 解码逻辑分析

**状态机**: `CLK` ↔ `SIG`（双状态交替）

核心流程：
1. 等待 CLK 和 SIG 通道的任意边沿：`self.wait([{0: 'e'}, {1: 'e'}])`
2. 在 `CLK` 状态：检测 clock 边沿 → 记录 `clk_start` → 切换到 `SIG`
3. 在 `SIG` 状态：检测 signal 边沿 → 记录 `sig_start` → 计算 jitter → 切换到 `CLK`
4. 每个样本可前进两步状态机（内层 while True 循环）
5. 边沿检测使用 `edge_detector` 字典，根据 polarity 选项决定检测上升/下降/双边沿

**关键算法**:
- Jitter 计算：`delta = (sig_start - clk_start) / samplerate`
- 时间格式化：自动选择 fs/ps/ns/μs/ms/s 单位
- 丢失检测：在 CLK 状态等待时检测到 SIG 边沿 → sig_missed++；在 SIG 状态等待时检测到 CLK 边沿 → clk_missed++
- Binary 输出：将 delta 格式化为 ASCII float + '\n'

**Edge Cases**:
- `clk_start == samplenum` 时跳过（已处理的同一样本）
- samplerate 为 0 时抛出 SamplerateError

### 1.3 C 实现规划

**文件名**: `jitter_c.c`

**struct jitter_priv**:
```c
struct jitter_priv {
    int state;              // 0=CLK, 1=SIG
    uint64_t samplerate;
    int oldclk, oldsig;
    uint64_t clk_start;
    uint64_t sig_start;
    int clk_missed;
    int sig_missed;
    int clk_edge_type;      // 0=rising, 1=falling, 2=both
    int sig_edge_type;      // 0=rising, 1=falling, 2=both
    int out_ann;
    int out_binary;
};
```

**srd_c_decoder**:
- `.id = "jitter_c"`, `.name = "Jitter(C)"`
- 2 channels, 2 options (string options with values list)
- 3 annotations, 3 annotation_rows
- 1 binary class

**关键实现要点**:
1. **双通道边沿等待**: 使用 `c_cond_edge(cb, 0); c_cond_or(cb); c_cond_edge(cb, 1);` 等待两个通道的任意边沿
2. **边沿检测函数**: 实现 `is_edge(old, new, type)` 辅助函数，根据 polarity 选项判断是否为有效边沿
3. **samplerate guard**: 在 `metadata` 回调中获取 samplerate，在 `decode()` 开头检查
4. **时间格式化**: 实现 `format_jitter(delta, buf)` 辅助函数，自动选择单位
5. **Binary 输出**: 使用 `c_decoder_put_binary()` 输出 ASCII float
6. **内层循环**: 在 C 中用 `while(1)` + break 模拟 Python 的内层 while True 循环

**关键代码片段 — 边沿检测**:
```c
static int is_edge(int old_val, int new_val, int edge_type)
{
    switch (edge_type) {
    case 0: return (!old_val && new_val);   // rising
    case 1: return (old_val && !new_val);   // falling
    case 2: return (old_val != new_val);    // both
    default: return 0;
    }
}
```

**关键代码片段 — 主解码循环**:
```c
while (1) {
    srd_cond_builder *cb = c_cond_new();
    c_cond_edge(cb, 0);
    c_cond_or(cb);
    c_cond_edge(cb, 1);
    ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;

    int clk = c_decoder_get_pin(di, 0, samplenum);
    int sig = c_decoder_get_pin(di, 1, samplenum);

    // Inner loop: can advance 2 states per sample
    while (1) {
        if (s->state == 0) { // CLK
            if (s->clk_start == samplenum) break;
            if (is_edge(s->oldclk, clk, s->clk_edge_type)) {
                s->clk_start = samplenum;
                s->state = 1; // SIG
            } else {
                // Check for missed signal
                if (s->sig_start != 0 && s->sig_start != samplenum
                    && is_edge(s->oldsig, sig, s->sig_edge_type)) {
                    s->sig_missed++;
                    // output missed signal annotation
                }
                break;
            }
        }
        if (s->state == 1) { // SIG
            if (s->sig_start == samplenum) break;
            if (is_edge(s->oldsig, sig, s->sig_edge_type)) {
                s->sig_start = samplenum;
                double delta = (double)(s->sig_start - s->clk_start) / s->samplerate;
                // output jitter annotation + binary
                s->state = 0; // CLK
            } else {
                // Check for missed clock
                if (s->clk_start != samplenum
                    && is_edge(s->oldclk, clk, s->clk_edge_type)) {
                    s->clk_missed++;
                    // output missed clock annotation
                }
                break;
            }
        }
    }
    s->oldclk = clk;
    s->oldsig = sig;
}
```

**Option 初始化** (在 `srd_c_decoder_entry()` 中):
```c
// clk_polarity option
GSList *pol_vals = NULL;
pol_vals = g_slist_append(pol_vals, g_variant_new_string("rising"));
pol_vals = g_slist_append(pol_vals, g_variant_new_string("falling"));
pol_vals = g_slist_append(pol_vals, g_variant_new_string("both"));
options[0].id = "clk_polarity";
options[0].idn = "dec_jitter_opt_clk_polarity";
options[0].desc = "Clock edge polarity";
options[0].def = g_variant_new_string("rising");
options[0].values = pol_vals;
```

---

## 2. lfast — NXP LFAST 接口

### 2.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `lfast` |
| name | `LFAST` |
| longname | `NXP LFAST interface` |
| desc | `Differential high-speed P2P interface` |
| inputs | `['logic']` |
| outputs | `['lfast']` |
| tags | `['Embedded/industrial']` |

**channels**:
| # | id | name | desc | idn |
|---|-----|------|------|-----|
| 0 | data | Data | TXP or RXP | `dec_lfast_chan_data` |

**annotations** (9):
| # | id | label |
|---|-----|-------|
| 0 | bit | Bits |
| 1 | sync | Sync Pattern |
| 2 | header_pl_size | Payload Size |
| 3 | header_ch_type | Logical Channel Type |
| 4 | header_cts | Clear To Send |
| 5 | payload | Payload |
| 6 | ctrl_data | Control Data |
| 7 | sleep | Sleep Bit |
| 8 | warning | Warning |

**annotation_rows** (3):
| id | label | classes |
|----|-------|---------|
| bits | Bits | (0,) |
| fields | Fields | (1, 2, 3, 4, 5, 6, 7) |
| warnings | Warnings | (8,) |

### 2.2 Python 解码逻辑分析

**状态机**: `state_sync(0)` → `state_header(1)` → `state_payload(2)` → `state_sleepbit(3)` → reset

核心流程：
1. 等待 data 通道边沿或超时：`self.wait([{0: 'e'}, {'skip': self.timeout}])`
2. **Sync 阶段**: 收集 16 bits，检查是否为 `0xA84B`
3. **Header 阶段**: 收集 8 bits，解析 payload_size(3 bits)、channel_type(4 bits)、CTS(1 bit)
4. **Payload 阶段**: 收集 `payload_size * 8` bits，区分数据通道和控制通道
5. **Sleepbit 阶段**: 检查 1 bit 的 LVDS sleep mode

**关键算法**:
- Bit 长度自动检测：第一个边沿间隔 = `bit_len`
- 边沿间 bit 计数：`bit_count = round((es - ss) / bit_len)`
- 超时机制：每个状态有不同超时值（sync=16.2*bit_len, header/payload=9.4*bit_len, sleepbit=1.4*bit_len）
- Python `bitpack()` 函数将 bit 列表打包为整数
- Payload 输出：数据通道(0b0100-0b1011)输出 `OUTPUT_PYTHON`

**Edge Cases**:
- 超时时检查 `matched & 0b10`（skip 匹配）
- `bit_len == 0` 时从第一个边沿推断
- `bit_count == 0` 时重置（bit time too short）
- `decimal.Decimal` 用于精确四舍五入

### 2.3 C 实现规划

**文件名**: `lfast_c.c`

**struct lfast_priv**:
```c
struct lfast_priv {
    int state;              // 0=sync, 1=header, 2=payload, 3=sleepbit
    uint64_t ss, es;        // current edge start/end
    uint64_t ss_bit, es_bit;
    uint64_t ss_sync, ss_header, ss_byte;
    uint64_t ss_payload, es_payload;
    uint64_t bit_len;
    uint64_t prev_bit_len;
    uint64_t timeout;
    uint8_t bits[64];       // bit buffer (max 64 bits for payload)
    int bit_count;
    int payload_size;       // expected bytes
    int ch_type_id;
    uint8_t payload_bytes[64]; // payload data
    int payload_byte_count;
    int out_ann;
    int out_python;
};
```

**关键实现要点**:
1. **超时等待**: `c_cond_edge(cb, 0); c_cond_or(cb); c_cond_skip(cb, timeout);`
2. **bitpack**: 实现简单 bit 数组→整数转换
3. **payload_sizes / channel_types / control_payloads**: 用 C 数组/查找表替代 Python 字典
4. **decimal 精度**: C 中用 `(uint64_t)((double)(es-ss)/bit_len + 0.5)` 替代 `decimal.Decimal`
5. **OUTPUT_PYTHON**: 使用 `c_decoder_put_python()` 输出 payload 数据
6. **timeout 检测**: 检查 `matched` 的第二位判断是否为超时

**关键代码片段 — payload_sizes 查找表**:
```c
static const char *payload_sizes[] = {
    "8 bit", "32 bit / 4 byte", "64 bit / 8 byte",
    "96 bit / 12 byte", "128 bit / 16 byte", "256 bit / 32 byte",
    "512 bit / 64 byte", "288 bit / 36 byte"
};
static const int payload_byte_sizes[] = {1, 4, 8, 12, 16, 32, 64, 36};
```

**关键代码片段 — 主循环结构**:
```c
while (1) {
    srd_cond_builder *cb = c_cond_new();
    c_cond_edge(cb, 0);
    if (s->timeout > 0) {
        c_cond_or(cb);
        c_cond_skip(cb, s->timeout);
    }
    ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;

    // Check for timeout (skip matched)
    int is_timeout = (matched & (1ULL << 1)) && !(matched & (1ULL << 0));

    if (is_timeout) {
        // Handle timeout per state
        if (s->state == 3) { // sleepbit
            handle_sleepbit(di, s, samplenum);
            reset_state(s);
            continue;
        }
        reset_state(s);
        continue;
    }

    // Process edge...
    // Determine bit_count, bit_value, process per state
}
```

---

## 3. maple_bus — SEGA Dreamcast Maple Bus

### 3.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `maple_bus` |
| name | `Maple bus` |
| longname | `SEGA Maple bus` |
| desc | `Maple bus peripheral protocol for SEGA Dreamcast.` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Retro computing']` |

**channels**:
| # | id | name | desc | idn |
|---|-----|------|------|-----|
| 0 | sdcka | SDCKA | Data/clock line A | `dec_maple_bus_chan_sdcka` |
| 1 | sdckb | SDCKB | Data/clock line B | `dec_maple_bus_chan_sdckb` |

**annotations** (15):
| # | id | label |
|---|-----|-------|
| 0 | start | Start pattern |
| 1 | end | End pattern |
| 2 | start-with-crc | Start pattern with CRC |
| 3 | occupancy | SDCKB occupancy pattern |
| 4 | reset | RESET pattern |
| 5 | bit | Bit |
| 6 | size | Data size |
| 7 | source | Source AP |
| 8 | dest | Destination AP |
| 9 | command | Command |
| 10 | data | Data |
| 11 | checksum | Checksum |
| 12 | frame-error | Frame error |
| 13 | checksum-error | Checksum error |
| 14 | size-error | Size error |

**annotation_rows** (3):
| id | label | classes |
|----|-------|---------|
| bits | Bits | (0, 1, 2, 3, 4, 5) |
| fields | Fields | (6, 7, 8, 9, 10, 11) |
| warnings | Warnings | (12, 13, 14) |

**binary** (6):
| # | id | label |
|---|-----|-------|
| 0 | size | Data size |
| 1 | source | Source AP |
| 2 | dest | Destination AP |
| 3 | command | Command code |
| 4 | data | Data |
| 5 | checksum | Checksum |

### 3.2 Python 解码逻辑分析

**核心流程**（三个主要函数）:

1. **handle_start()**:
   - 等待 SDCKA=low, SDCKB=high
   - 计数 SDCKB 下降沿，等待 SDCKA 上升沿
   - count=4 → Start, count=6 → Start with CRC, count=8 → Occupancy, count≥14 → Reset

2. **handle_byte_or_stop()**:
   - 解码 4 个 bit 对（每个 bit 由 SDCKA 下降沿+SDCKB 下降沿组成）
   - SDCKA 下降沿时读取 SDCKB 值，SDCKB 下降沿时读取 SDCKA 值
   - 特殊情况：data=0 且 sdckb=0 时检测 End pattern
   - 4 bit 对组成 1 byte

3. **decode() 主循环**:
   - `while not handle_start(): pass`
   - `while handle_byte_or_stop(): pass`
   - 每帧：length=0, expected_length=4, checksum=0
   - Byte 0 = size → `expected_length = 4 * (data + 1)`
   - 最后一个 byte = checksum，验证 XOR

**关键算法**:
- `data = data * 2 + n`：MSB-first bit 累加
- `checksum = checksum ^ data`：XOR 校验
- `expected_length = 4 * (data + 1)`：从第一个 byte 计算帧长度

**Edge Cases**:
- End pattern 检测：counta=1, countb=0, data=0, sdckb=0
- Size error：`length != expected_length + 1`
- Checksum error：`data != checksum`

### 3.3 C 实现规划

**文件名**: `maple_bus_c.c`

**struct maple_priv**:
```c
struct maple_priv {
    uint64_t ss, es;
    int data;
    int length;
    int expected_length;
    int checksum;
    int pending_bit;
    uint64_t pending_bit_pos;
    int out_ann;
    int out_binary;
};
```

**关键实现要点**:
1. **双通道条件等待**: `c_cond_fall(cb, 1); c_cond_or(cb); c_cond_rise(cb, 0);` 等
2. **handle_start**: 需要嵌套循环计数 SDCKB 下降沿
3. **handle_byte_or_stop**: 4 个 bit 对的解码循环
4. **End pattern 检测**: 特殊条件 `counta==1 && countb==0 && data==0 && sdckb==0`
5. **Binary 输出**: 使用 `c_decoder_put_binary()` 输出各 byte

**关键代码片段 — handle_start**:
```c
static int handle_start(struct srd_decoder_inst *di, struct maple_priv *s)
{
    // Wait for SDCKA low, SDCKB high
    srd_cond_builder *cb = c_cond_new();
    c_cond_low(cb, 0);
    c_cond_or(cb);
    c_cond_high(cb, 1);
    // Note: need to wait for both conditions simultaneously
    // In practice, wait for edges and check pin values
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);

    int count = 0;
    while (1) {
        cb = c_cond_new();
        c_cond_fall(cb, 1);    // SDCKB falling
        c_cond_or(cb);
        c_cond_rise(cb, 0);    // SDCKA rising
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return 0;

        if (matched & (1ULL << 0)) count++;  // SDCKB fell
        if (matched & (1ULL << 1)) {         // SDCKA rose
            int sdckb = c_decoder_get_pin(di, 1, samplenum);
            if (sdckb == 1) {
                if (count == 4) { /* Start */ return 1; }
                else if (count == 6) { /* Start CRC */ return 1; }
                else if (count == 8) { /* Occupancy */ return 0; }
                else if (count >= 14) { /* Reset */ return 0; }
            }
            /* frame error */ return 0;
        }
    }
}
```

**关键代码片段 — byte annotation**:
```c
static void byte_annotation(struct srd_decoder_inst *di, struct maple_priv *s,
                            int bintype, uint8_t d)
{
    static const char *ann_names[][3] = {
        {"Size", "L"}, {"SrcAP", "S"}, {"DstAP", "D"},
        {"Cmd", "C"}, {"Data"}, {"Cksum", "K"}
    };
    char long_str[32], mid_str[16], short_str[8];
    snprintf(long_str, sizeof(long_str), "%s: %02X", ann_names[bintype][0], d);
    snprintf(mid_str, sizeof(mid_str), "%s: %02X", ann_names[bintype][1], d);
    snprintf(short_str, sizeof(short_str), "%02X", d);
    C_ANN_PUT(di, s->ss, s->es, s->out_ann, bintype + 6, long_str, mid_str, short_str);
}
```

---

## 4. miller — Miller 编码

### 4.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `miller` |
| name | `Miller` |
| longname | `Miller encoding` |
| desc | `Miller encoding protocol.` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Encoding']` |

**channels**:
| # | id | name | desc | idn |
|---|-----|------|------|-----|
| 0 | data | Data | Data signal | `dec_miller_chan_data` |

**options**:
| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| baudrate | Baud rate | `106000` | (int) | `dec_miller_opt_baudrate` |
| edge | Edge | `'falling'` | `('rising', 'falling', 'either')` | `dec_miller_opt_edge` |

**annotations** (2):
| # | id | label |
|---|-----|-------|
| 0 | bit | Bit |
| 1 | bitstring | Bitstring |

**annotation_rows** (2):
| id | label | classes |
|----|-------|---------|
| bit | Bit | (0,) |
| bitstring | Bitstring | (1,) |

**binary** (1):
| # | id | label |
|---|-----|-------|
| 0 | raw | Raw binary |

### 4.2 Python 解码逻辑分析

**Miller 编码规则**:
- Mark (1 bit): 边沿在 bit 中间
- Space (0 bit): mark 后无边沿，space 后有边沿在 bit 开始
- 传输结束：space 后跟一个 idle symbol（无边沿）

**decode_bits() 生成器**:
1. 计算 `timeunit = samplerate / baudrate`
2. 等待第一个边沿（`edgetype` 由 option 决定）
3. 初始 bit = 0
4. 循环等待边沿或超时（3 * timeunit）
5. 计算 `timedelta = round(sampledelta / timeunit, 0.5)` — 可得 1.0, 1.5, 2.0
6. 根据 `prevbit` 和 `timedelta` 解码：
   - prevbit=0 (space): 1.0→space(0), 1.5→mark(1), ≥2.0→idle(end)
   - prevbit=1 (mark): 1.0→mark(1), 1.5→space(0)+space(0), 2.0→space(0)+mark(1)

**decode_run()**:
- 收集 bits → 组成 bitstring → 每 4 bits 加空格
- 输出 binary: `bitvalue.to_bytes(numbytes, 'little')`

**Edge Cases**:
- `timedelta <= 0.5` → error
- `timedelta > 2.0` (after space) → idle + end of message
- 超时（3 * timeunit 无边沿）→ end of message

### 4.3 C 实现规划

**文件名**: `miller_c.c`

**struct miller_priv**:
```c
struct miller_priv {
    uint64_t samplerate;
    uint64_t timeunit;
    int edge_type;          // 0=rising, 1=falling, 2=either
    int prevbit;
    uint64_t prevedge;
    uint64_t expectedstart;
    // decode_run state
    uint8_t bits[256];
    int numbits;
    uint32_t bitvalue;
    uint64_t stringstart;
    uint64_t stringend;
    int out_ann;
    int out_binary;
};
```

**关键实现要点**:
1. **Edge type 条件**: 根据 option 选择 `c_cond_rise/fall/edge`
2. **超时等待**: `c_cond_edge(cb, 0); c_cond_or(cb); c_cond_skip(cb, 3*timeunit);`
3. **roundto(x, 0.5)**: `(int)(x * 2 + 0.5) / 2.0` 或使用整数运算
4. **timedelta 计算**: `roundto((double)(sampledelta) / timeunit, 0.5)` → 得到 1.0, 1.5, 2.0 等
5. **生成器模拟**: 用状态变量模拟 Python generator 的 yield 行为
6. **bitstring 格式化**: 每 4 bits 插入空格

**关键代码片段 — 条件构建**:
```c
static int wait_edge_or_timeout(struct srd_decoder_inst *di, struct miller_priv *s,
                                uint64_t *samplenum, uint64_t *matched)
{
    srd_cond_builder *cb = c_cond_new();
    switch (s->edge_type) {
    case 0: c_cond_rise(cb, 0); break;
    case 1: c_cond_fall(cb, 0); break;
    case 2: c_cond_edge(cb, 0); break;
    }
    c_cond_or(cb);
    c_cond_skip(cb, 3 * s->timeunit);
    int ret = c_cond_wait(cb, di, samplenum, matched);
    c_cond_free(cb);
    return ret;
}
```

**关键代码片段 — Miller 解码核心**:
```c
// timedelta rounding to nearest 0.5
double td_exact = (double)sampledelta / s->timeunit;
double timedelta = round(td_exact * 2.0) / 2.0;  // round to 0.5

if (s->prevbit == 0) { // space -> ???
    if (timedelta == 1.0) {
        // space (0)
        output_bit(di, s, 0, samplenum, samplenum + s->timeunit);
        s->prevbit = 0;
        s->expectedstart = samplenum + s->timeunit;
    } else if (timedelta == 1.5) {
        // mark (1)
        output_bit(di, s, 1, s->expectedstart, samplenum + s->timeunit/2);
        s->prevbit = 1;
        s->expectedstart = samplenum + s->timeunit/2;
    } else if (timedelta >= 2.0) {
        // idle - end of message
        return 0; // signal end
    }
} else { // mark -> ???
    if (timedelta == 1.0) {
        // mark (1)
        output_bit(di, s, 1, s->expectedstart, samplenum + s->timeunit/2);
        s->prevbit = 1;
        s->expectedstart = samplenum + s->timeunit/2;
    } else if (timedelta == 1.5) {
        // space (0) + space (0)
        output_bit(di, s, 0, s->expectedstart, samplenum);
        output_bit(di, s, 0, samplenum, samplenum + s->timeunit);
        s->prevbit = 0;
        s->expectedstart = samplenum + s->timeunit;
    } else if (timedelta == 2.0) {
        // space (0) + mark (1)
        output_bit(di, s, 0, s->expectedstart, s->expectedstart + s->timeunit);
        output_bit(di, s, 1, samplenum - s->timeunit/2, samplenum + s->timeunit/2);
        s->prevbit = 1;
        s->expectedstart = samplenum + s->timeunit/2;
    } else {
        // space + idle - end
        output_bit(di, s, 0, s->expectedstart, s->expectedstart + s->timeunit);
        return 0;
    }
}
```

---

## 5. morse — 摩尔斯电码

### 5.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `morse` |
| name | `Morse` |
| longname | `Morse code` |
| desc | `Demodulated morse code protocol.` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Encoding']` |

**channels**:
| # | id | name | desc | idn |
|---|-----|------|------|-----|
| 0 | data | Data | Data line | `dec_morse_chan_data` |

**options**:
| id | desc | default | idn |
|----|------|---------|-----|
| timeunit | Time unit (guess) | `0.1` (float, 秒) | `dec_morse_opt_timeunit` |

**annotations** (5):
| # | id | label |
|---|-----|-------|
| 0 | time | Time |
| 1 | units | Units |
| 2 | symbol | Symbol |
| 3 | letter | Letter |
| 4 | word | Word |

**annotation_rows** (5, 每个 annotation 独占一行):
| id | label | classes |
|----|-------|---------|
| time | Time | (0,) |
| units | Units | (1,) |
| symbol | Symbol | (2,) |
| letter | Letter | (3,) |
| word | Word | (4,) |

### 5.2 Python 解码逻辑分析

**三层解码架构**:

1. **decode_symbols()** — 符号层:
   - 等待上升沿开始
   - 等待边沿或超时（5 * samplerate * timeunit）
   - 计算电平持续时间 → 转换为时间单位数 `iunits = round(units)`
   - 符号映射：`(1,1)=dit`, `(1,3)=dah`, `(0,1)=intra-char gap`, `(0,3)=letter gap`, `(0,7)=word gap`
   - **自适应 timeunit**: `timeunit += (thisunit - timeunit) * 0.2 * max(0, 1 - 2*error)`
   - 超时 → yield None（flush word）

2. **decode_morse()** — 字母层:
   - 收集连续的 mark 符号（dit=1, dah=3）组成 sequence tuple
   - space ≥ 3 单位 → flush letter
   - 在 alphabet 字典中查找 sequence → 得到字母
   - None → flush word

3. **decode()** — 单词层:
   - 收集字母组成 word
   - None → flush word，输出 word annotation

**alphabet 字典**: 26 字母 + 10 数字 + 标点符号，key 为 tuple 如 (1,3) 表示 ".-" = 'a'

**关键算法**:
- `decode_ditdah(s)`: 字符串→tuple，如 ".-" → (1,3)
- `encode_ditdah(tpl)`: tuple→字符串，如 (1,3) → ".-"
- 自适应 timeunit：指数移动平均，权重受误差影响

**Edge Cases**:
- samplerate 为 0 时 fallback 到 1.0
- 未知符号：输出 `!! units*timeunit !!`
- 未知字母：输出 ditdah 字符串表示

### 5.3 C 实现规划

**文件名**: `morse_c.c`

**struct morse_priv**:
```c
struct morse_priv {
    uint64_t samplerate;
    double timeunit;
    int out_ann;
    int out_binary;
    // symbol decoding state
    int prev_val;
    uint64_t prev_time;
    // letter decoding state
    uint8_t sequence[8];    // max 8 elements (dit=1, dah=3)
    int seq_len;
    uint64_t letter_ss, letter_es;
    // word decoding state
    char word[256];
    int word_len;
    uint64_t word_ss, word_es;
};
```

**alphabet 查找表**: 使用 C 数组实现
```c
typedef struct {
    const char *code;   // e.g. ".-"
    const char *letter; // e.g. "a"
} morse_entry;

static const morse_entry morse_alphabet[] = {
    {".-", "a"}, {"-...", "b"}, {"-.-.", "c"}, {"-..", "d"},
    {".", "e"}, {"..-..", "é"}, {"..-.", "f"}, {"--.", "g"},
    {"....", "h"}, {"..", "i"}, {".---", "j"}, {"-.-", "k"},
    {".-..", "l"}, {"--", "m"}, {"-.", "n"}, {"---", "o"},
    {".--.", "p"}, {"--.-", "q"}, {".-.", "r"}, {"...", "s"},
    {"-", "t"}, {"..-", "u"}, {"...-", "v"}, {".--", "w"},
    {"-..-", "x"}, {"-.--", "y"}, {"--..", "z"},
    {".----", "1"}, {"..---", "2"}, {"...--", "3"}, {"....-", "4"},
    {".....", "5"}, {"-....", "6"}, {"--...", "7"}, {"---..", "8"},
    {"----.", "9"}, {"-----", "0"},
    // punctuation...
    {NULL, NULL}  // sentinel
};
```

**关键实现要点**:
1. **超时等待**: `c_cond_edge(cb, 0); c_cond_or(cb); c_cond_skip(cb, 5*samplerate*timeunit);`
2. **自适应 timeunit**: 在 C 中用 double 运算实现
3. **sequence → letter 查找**: 将 sequence 编码为字符串，在 morse_alphabet 中线性搜索
4. **三层生成器模拟**: 用状态变量 + 函数分解模拟 Python 的三层 generator
5. **timeunit option**: 使用 `c_decoder_get_option_double()` 获取 float option

**关键代码片段 — 符号解码**:
```c
// Wait for edge or timeout
srd_cond_builder *cb = c_cond_new();
c_cond_edge(cb, 0);
c_cond_or(cb);
c_cond_skip(cb, (uint64_t)(5.0 * s->samplerate * s->timeunit));
ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);

int is_timeout = (matched & (1ULL << 1)) && !(matched & (1ULL << 0));
int val = c_decoder_get_pin(di, 0, samplenum);
int pval = 1 - val;  // previous level
uint64_t curtime = samplenum;
double dt = (double)(curtime - s->prev_time) / s->samplerate;
double units = dt / s->timeunit;
int iunits = (int)(round(units));
if (iunits < 1) iunits = 1;
double error = fabs(units - iunits);

if (is_timeout) {
    // Flush word
    flush_word(di, s);
    s->prev_time = curtime;
    continue;
}

// Output time annotation
char time_str[32];
snprintf(time_str, sizeof(time_str), "%.3g", dt);
C_ANN_PUT(di, s->prev_time, curtime, s->out_ann, 0, time_str);

// Check symbol
int sval = pval;
int sunits = iunits;
// Valid symbols: (1,1), (1,3), (0,1), (0,3), (0,7)
if ((sval == 1 && (sunits == 1 || sunits == 3)) ||
    (sval == 0 && (sunits == 1 || sunits == 3 || sunits == 7))) {
    char units_str[32];
    snprintf(units_str, sizeof(units_str), "%.1f*%.3g", units, s->timeunit);
    C_ANN_PUT(di, s->prev_time, curtime, s->out_ann, 1, units_str);
    // Process symbol...
    process_symbol(di, s, sval, sunits, s->prev_time, curtime);
} else {
    char err_str[64];
    snprintf(err_str, sizeof(err_str), "!! %.1f*%.3g !!", units, s->timeunit);
    C_ANN_PUT(di, s->prev_time, curtime, s->out_ann, 1, err_str);
}

// Adaptive timeunit
double thisunit = dt / iunits;
double weight = 0.2 * fmax(0.0, 1.0 - 2.0 * error);
s->timeunit += (thisunit - s->timeunit) * weight;
s->prev_time = curtime;
```

**关键代码片段 — sequence 查找**:
```c
static const char *lookup_morse(uint8_t *seq, int seq_len)
{
    // Convert sequence to code string
    char code[16] = {0};
    for (int i = 0; i < seq_len && i < 15; i++) {
        code[i] = (seq[i] == 1) ? '.' : '-';
    }
    // Linear search in alphabet
    for (int i = 0; morse_alphabet[i].code != NULL; i++) {
        if (strcmp(code, morse_alphabet[i].code) == 0)
            return morse_alphabet[i].letter;
    }
    return NULL; // unknown
}
```

---

## 通用 C 解码器模板

所有 5 个解码器必须遵循以下模板结构：

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include "libsigrokdecode.h"

// 1. 状态枚举
// 2. 私有结构体
// 3. Channel 定义
// 4. Option 定义（数组）
// 5. Annotation labels（第一列必须为 ""）
// 6. Annotation row classes（以 -1 结尾）
// 7. Annotation rows
// 8. Inputs/Outputs/Tags
// 9. reset 回调
// 10. start 回调
// 11. metadata 回调（如需要 samplerate）
// 12. decode 回调
// 13. destroy 回调
// 14. srd_c_decoder 结构体
// 15. srd_c_decoder_entry() — 初始化 options
// 16. srd_c_decoder_api_version()
```

**ann_labels 格式**:
```c
static const char* xxx_ann_labels[][3] = {
    { "", "id-string", "Label String" },  // 第一列必须为 ""
    ...
};
```

**annotation_rows 格式**:
```c
static const int xxx_row_xxx_classes[] = { ANN_XX, ANN_YY, -1 };  // 以 -1 结尾
static const struct srd_c_ann_row xxx_ann_rows[] = {
    { "row-id", "Row Label", xxx_row_xxx_classes, N },  // N = 元素数（不含 -1）
};
```

**samplerate guard 模式**:
```c
static void xxx_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    struct xxx_priv *s = (struct xxx_priv *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->samplerate = value;
    }
}

// 在 decode() 开头:
if (!s->samplerate) {
    s->samplerate = c_decoder_get_samplerate(di);
}
if (!s->samplerate) return;
```

**CMakeLists.txt 修改**: 在 `C_DECODERS` 列表中添加:
```
jitter_c lfast_c maple_bus_c miller_c morse_c
```
