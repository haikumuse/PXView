# Python → C 解码器移植规格 — Batch 15

## 概述

本文档定义了将 5 个 Python 协议解码器移植为 C 解码器的详细规格。目标解码器：

| # | Python ID | C 文件名 | C ID | 复杂度 |
|---|-----------|----------|------|--------|
| 1 | `mipi_dsi` | `mipi_dsi_c.c` | `mipi_dsi_c` | 复杂 |
| 2 | `pxx1` | `pxx1_c.c` | `pxx1_c` | 复杂 |
| 3 | `qi` | `qi_c.c` | `qi_c` | 中等 |
| 4 | `rc_encode` | `rc_encode_c.c` | `rc_encode_c` | 中等 |
| 5 | `sdq` | `sdq_c.c` | `sdq_c` | 简单 |

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |


## 通用 C 解码器实现规范

### 文件命名
- 源文件：`libsigrokdecode/c_decoders/{decoder_id}_c.c`
- 文件名中 `-` 替换为 `_`（如 `mipi_dsi` → `mipi_dsi_c.c`）

### struct srd_c_decoder 规范
- `.id` = `"{python_id}_c"`（如 `"mipi_dsi_c"`）
- `.name` = `"{PythonName}(C)"`（如 `"MIPI_DSI(C)"`）
- `.longname` = Python longname + `" (C)"`
- `.desc` = Python desc + `" (C implementation)"`

### ann_labels 规范
- 第一列必须为 `""`（空字符串），API 自动处理 i+7 偏移
- 第二列为 row id，第三列为显示名称
- 格式：`static const char *xxx_ann_labels[][3] = { {"", "id", "Name"}, ... };`

### annotation_rows 规范
- 所有 annotation class 必须映射到 annotation_rows
- 使用 `static const int xxx_row_xxx_classes[]` 定义每行包含的 class
- 使用 `static const struct srd_c_ann_row xxx_ann_rows[]` 定义行

### samplerate 守卫
- 实现 `metadata` 回调保存 samplerate
- 在 `decode()` 入口检查 samplerate 是否有效，无效则 return

### Condition Builder API
```c
srd_cond_builder *cb = c_cond_new();
c_cond_rise(cb, ch);     // 上升沿
c_cond_fall(cb, ch);     // 下降沿
c_cond_edge(cb, ch);     // 任一边沿
c_cond_high(cb, ch);     // 高电平
c_cond_low(cb, ch);      // 低电平
c_cond_skip(cb, count);  // 跳过采样数
c_cond_or(cb);           // 分隔 OR 条件组
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

### 输出 API
- `C_ANN_PUT(di, ss, es, out_id, cls, ...)` — 注解输出
- `C_ANN_PUT_VAL(di, ss, es, out_id, cls, val, ...)` — 带数值的注解
- `c_decoder_put_python(di, ss, es, out_id, cmd, data, len)` — Python 协议输出
- `c_decoder_put_binary(di, ss, es, out_id, bin_class, size, data)` — 二进制输出
- `c_decoder_put_logic(di, ss, es, out_id, channel_mask, values, num_channels)` — 逻辑信号输出 <!-- Updated: SRD_OUTPUT_LOGIC + c_decoder_put_logic() 已实现 -->

<!-- Updated: 以下 API 已实现，可在本批次解码器中使用：
     - c_cond_wait_current(di, &samplenum) — 等效于 Python self.wait({})，
       获取当前采样位置而不前进。适用于初始化时读取引脚状态。
       参考 spi_c.c 中的使用方式。
     - c_decoder_get_initial_pin(di, ch) — 获取初始引脚值（old_pins_array），
       等效于 Python self.oldpin。适用于解码器启动时需要知道初始信号状态。 -->

### Options 初始化
- 在 `srd_c_decoder_entry()` 中用 `g_variant_new_*()` 初始化
- 字符串选项用 `g_variant_new_string()`
- 整数选项用 `g_variant_new_int64()`
- 双精度选项用 `g_variant_new_double()`
- 枚举值列表用 `GSList` + `g_slist_append()`

### Build 集成
- 在 `CMakeLists.txt` 的 `C_DECODERS` 列表末尾添加新解码器名

---

## 1. MIPI DSI 解码器 (`mipi_dsi_c`)

### 1.1 Python 元数据提取

| 属性 | 值 |
|------|-----|
| id | `MIPI_DSI` |
| name | `MIPI_DSI` |
| longname | `MIPI Display Serial Interface` |
| desc | `MIPI Display Serial Interface low power communication` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['mipi_dsi']` |
| tags | `['Embedded/industrial']` |

**Channels:**

| # | id | name | desc | type | idn |
|---|----|------|------|------|-----|
| 0 | D0N | D0N | LP data 0 neg | 8 (SRD_CHANNEL_SDATA) | dec_mipi_dsi_chan_D0N |
| 1 | D0P | D0P | LP data 0 pos | 108 (SRD_CHANNEL_ADATA) | dec_mipi_dsi_chan_D0P |

**Options:** 无

**Annotations (13个):**

| Index | type | id | Name |
|-------|------|----|------|
| 0 | 111 | LP-00 | LP-00 |
| 1 | 110 | LP-01 | LP-01 |
| 2 | 109 | LP-10 | LP-10 |
| 3 | 108 | LP-11 | LP-11 |
| 4 | 7 | EscapeMode | Escape mode |
| 5 | 6 | BTA | Bi-directional Data Lane Turnaround |
| 6 | 112 | LPDT | LPDT |
| 7 | 0 | DI | Data identifier |
| 8 | 12 | ECC | ECC |
| 9 | 11 | WC | Word count |
| 10 | 107 | CRC | CheckSUM |
| 11 | 5 | Stop | Stop condition |
| 12 | 1 | Idle | Idle |

**Annotation Rows:**

| Row id | Name | Classes |
|--------|------|---------|
| LPData | LPData | 0, 1, 2, 3 |
| LP | LP | 4, 5, 6, 7, 8, 9, 10 |

**注意：** Python annotations 中 class 11 (Stop) 和 12 (Idle) 未映射到 annotation_rows。C 实现中**必须**将所有 class 映射到 rows。建议将 Stop 和 Idle 加入 LP row。

**Binary:** 无

### 1.2 状态机分析

Python 解码器有 3 个主状态 + 3 个子状态：

```
FIND START ──→ FIND MODE ──→ FIND DATA
                  │               │
                  │ (3 sub-states)│
                  │               ├──→ handle_data() → FIND DATA (循环)
                  │               └──→ handle_stop() → FIND START
                  └──→ (ESC/BTA 判断)
```

**FIND START:**
- 等待条件：`{0: 'f', 1: 'h'}` — D0N 下降沿 AND D0P 高电平
- 触发后调用 `handle_start()`，转入 FIND MODE

**FIND MODE (3个子状态):**
- `state0`: 等待 `{0: 'l', 1: 'l'}` — D0N 低 AND D0P 低
- `state1`: 等待 `[{0: 'h', 1: 'l'}, {0: 'l', 1: 'h'}]` — 两个 OR 条件
  - 条件0: D0N 高 AND D0P 低 → ESC Mode (d0n==1)
  - 条件1: D0N 低 AND D0P 高 → BTA (d0n==0)
- `state2`: 等待 `{0: 'l', 1: 'l'}` — D0N 低 AND D0P 低
  - 触发后调用 `handle_esc_bta(d0n, d0p)`，转入 FIND DATA

**FIND DATA:**
- 等待 `[{0: 'h', 1: 'l'}, {0: 'l', 1: 'h'}]` — 两个 OR 条件
- 然后等待 `[{0: 'l', 1: 'l'}, {0: 'h', 1: 'h'}]` — 两个 OR 条件
- 如果 matched & (1<<0)（第一个条件匹配，即 D0N 低 D0P 低）→ handle_data()
- 否则（D0N 高 D0P 高）→ handle_stop()

**handle_data() 逻辑:**
- 数据位 LSB first：`databyte >>= 1; if d0p: databyte |= 0x80`
- 累积 8 位后输出一个 DATA 注解
- 输出格式：`['0x%02X' % d]`

**handle_esc_bta() 逻辑:**
- 如果 d0n == 1 → ESC Mode 注解
- 否则 → BTA 注解

**handle_stop() 逻辑:**
- 输出 STOP 注解
- 回到 FIND START

### 1.3 C 实现规划

#### 状态枚举
```c
enum mipi_dsi_state {
    STATE_FIND_START,
    STATE_FIND_MODE_S0,   // Find Mode state0
    STATE_FIND_MODE_S1,   // Find Mode state1
    STATE_FIND_MODE_S2,   // Find Mode state2
    STATE_FIND_DATA_EDGE, // 等待第一个边沿
    STATE_FIND_DATA_VALID, // 等待验证条件
};

enum mipi_dsi_ann {
    ANN_LP00 = 0,
    ANN_LP01,
    ANN_LP10,
    ANN_LP11,
    ANN_ESCAPE_MODE,
    ANN_BTA,
    ANN_LPDT,
    ANN_DI,
    ANN_ECC,
    ANN_WC,
    ANN_CRC,
    ANN_STOP,
    ANN_IDLE,
    NUM_ANN,
};
```

#### 私有数据结构
```c
struct mipi_dsi_priv {
    int state;
    uint64_t samplerate;
    int out_ann;
    int out_python;

    // 数据位累积
    int bitcount;
    uint8_t databyte;
    uint64_t ss;        // 当前注解起始
    uint64_t es;        // 当前注解结束
    uint64_t ss_byte;   // 字节起始

    // ESC/BTA 判断时保存的引脚值
    uint8_t saved_d0n;
    uint8_t saved_d0p;

    // FIND DATA 中保存的引脚值
    uint8_t data_d0n;
    uint8_t data_d0p;
};
```

#### 关键代码片段 — FIND START
```c
case STATE_FIND_START: {
    cb = c_cond_new();
    c_cond_fall(cb, 0);  // D0N 下降沿
    c_cond_high(cb, 1);  // D0P 高电平
    ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;
    // handle_start
    s->ss = s->es = samplenum;
    s->bitcount = 0;
    s->databyte = 0;
    s->state = STATE_FIND_MODE_S0;
    break;
}
```

#### 关键代码片段 — FIND MODE state1 (OR 条件)
```c
case STATE_FIND_MODE_S1: {
    cb = c_cond_new();
    c_cond_high(cb, 0);  // D0N 高
    c_cond_low(cb, 1);   // D0P 低
    c_cond_or(cb);
    c_cond_low(cb, 0);   // D0N 低
    c_cond_high(cb, 1);  // D0P 高
    ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;
    // 保存引脚值用于后续判断
    s->saved_d0n = c_decoder_get_pin(di, 0, samplenum);
    s->saved_d0p = c_decoder_get_pin(di, 1, samplenum);
    s->state = STATE_FIND_MODE_S2;
    break;
}
```

#### 关键代码片段 — FIND DATA (两步等待)
```c
case STATE_FIND_DATA_EDGE: {
    // 第一步：等待 [{0:'h',1:'l'}, {0:'l',1:'h'}]
    cb = c_cond_new();
    c_cond_high(cb, 0); c_cond_low(cb, 1);
    c_cond_or(cb);
    c_cond_low(cb, 0); c_cond_high(cb, 1);
    ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;
    s->data_d0n = c_decoder_get_pin(di, 0, samplenum);
    s->data_d0p = c_decoder_get_pin(di, 1, samplenum);
    s->state = STATE_FIND_DATA_VALID;
    break;
}
case STATE_FIND_DATA_VALID: {
    // 第二步：等待 [{0:'l',1:'l'}, {0:'h',1:'h'}]
    cb = c_cond_new();
    c_cond_low(cb, 0); c_cond_low(cb, 1);
    c_cond_or(cb);
    c_cond_high(cb, 0); c_cond_high(cb, 1);
    ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;
    // matched bit 0 = 第一个条件(D0N低D0P低) → data
    if (matched & (1ULL << 0)) {
        // handle_data
        s->databyte >>= 1;
        if (s->data_d0p) s->databyte |= 0x80;
        if (s->bitcount == 0) s->ss_byte = samplenum;
        if (s->bitcount < 7) {
            s->bitcount++;
            s->state = STATE_FIND_DATA_EDGE;
        } else {
            char h[8];
            snprintf(h, sizeof(h), "0x%02X", s->databyte);
            C_ANN_PUT(di, s->ss, samplenum, s->out_ann, ANN_DI, h);
            s->bitcount = 0;
            s->databyte = 0;
            s->ss = samplenum;
            s->state = STATE_FIND_DATA_EDGE;
        }
    } else {
        // handle_stop
        C_ANN_PUT(di, s->ss, samplenum, s->out_ann, ANN_STOP, "Stop", "S");
        s->state = STATE_FIND_START;
    }
    break;
}
```

#### C 解码器结构体
```c
struct srd_c_decoder mipi_dsi_c_decoder = {
    .id = "mipi_dsi_c",
    .name = "MIPI_DSI(C)",
    .longname = "MIPI Display Serial Interface (C)",
    .desc = "MIPI Display Serial Interface low power communication (C implementation)",
    .license = "gplv2+",
    .channels = mipi_dsi_channels,
    .num_channels = 2,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = mipi_dsi_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = mipi_dsi_ann_rows,
    .inputs = mipi_dsi_inputs,
    .num_inputs = 1,
    .outputs = mipi_dsi_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = mipi_dsi_tags,
    .num_tags = 1,
    .reset = mipi_dsi_reset,
    .start = mipi_dsi_start,
    .decode = mipi_dsi_decode,
    .destroy = mipi_dsi_destroy,
};
```

### 1.4 注意事项
- Python 中 proto 字典的 DATA 注解使用 `proto['DATA'][0]` = 2，但 annotations 列表中 index 2 对应 `LP-10`。这是因为 Python 代码中 `self.putx([proto[cmd][0], proto[cmd][1:]])` 使用的是 proto 字典的 index，而非 annotations 列表的 index。C 实现中**必须使用 annotations 列表的实际 index**。
- 实际映射：ESC Mode → ANN_ESCAPE_MODE (4), BTA → ANN_BTA (5), DATA → ANN_DI (7), STOP → ANN_STOP (11)
- Python 中 Stop 和 Idle 未映射到 annotation_rows，C 中需将它们加入 LP row
- LP-00/01/10/11 注解在 Python 中声明但未在 decode() 中使用，C 中保留声明

<!-- Updated: MIPI DSI Python 解码器同时使用 SRD_OUTPUT_PYTHON 和 SRD_OUTPUT_BINARY 输出。
     - Python 输出：putp([cmd, None]) 用于 ESC Mode/BTA/STOP/DATA 事件。
      C 实现需使用 c_decoder_put_python(di, ss, es, out_python, cmd, data, len)。
     - Binary 输出：putb([cmd, None]) 用于 DATA 字节。
      C 实现需添加 out_binary 字段，注册 SRD_OUTPUT_BINARY 输出，
      并使用 c_decoder_put_binary(di, ss, es, out_binary, 0, 1, &byte_val)。
     - Python 解码器还注册了 SRD_OUTPUT_META (bitrate)，C 实现可选添加。
     - struct mipi_dsi_priv 需添加 out_binary 字段。
     - srd_c_decoder 结构体中 .binary 和 .num_binary 需相应更新。 -->

---

## 2. PXX1 解码器 (`pxx1_c`)

### 2.1 Python 元数据提取

| 属性 | 值 |
|------|-----|
| id | `pxx1` |
| name | `PXX1` |
| longname | `PXX1 modulation` |
| desc | `FrSky PXX1(R9M) Protcol` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['PXX1']` |

**Channels:**

| # | id | name | desc | idn |
|---|----|------|------|-----|
| 0 | data | Data | Data line | dec_pxx1_chan_data |

**Options:** 无

**Annotations (20个):**

| Index | id | Name |
|-------|----|------|
| 0 | byte | Byte |
| 1 | bit | Bit |
| 2 | bit_stuff | BitStuff |
| 3 | start_header | Start Header |
| 4 | model_id | Model ID |
| 5 | type | Type |
| 6 | range_check | RangeCheck |
| 7 | fail_safe | FailSafe |
| 8 | country_code | CountryCode |
| 9 | bind | Bind |
| 10 | flags2 | Flags2 |
| 11 | channels | Channels |
| 12 | reserved | Reserved |
| 13 | is_euplus | EU-PLUS |
| 14 | disable_sport | Disable SPort |
| 15 | power_level | Power Level |
| 16 | rx_highchan | Receive Hight Channel |
| 17 | telemetry_off | Telemetry Off |
| 18 | external_antena | ExternalAntena |
| 19 | CRC | CRC |

**Annotation Rows:**

| Row id | Name | Classes |
|--------|------|---------|
| bytes | Bytes | 0 |
| bits | Bits | 1, 2 |
| desc | Description | 3-19 |

**Binary:**
| Class | id | Name |
|-------|----|------|
| 0 | raw | RAW file |

### 2.2 状态机分析

PXX1 有 19 个线性状态：

```
state_wait_header → state_rx_model_id → state_rx_type → state_rx_range_check →
state_rx_fail_safe → state_rx_country_code → state_rx_bind → state_rx_flag2 →
state_rx_channels → state_rx_rsrv2 → state_rx_euplus → state_rx_disable_sport →
state_rx_powerlevel → state_rx_highchan → state_rx_telemetry_off →
state_rx_external_antena → state_rx_crc → state_rx_stop → state_error
```

**关键常量：**
- `PXX_HEADER = 0x7E`
- `PXX_SEND_BIND = 0x01`
- `PXX_SEND_FAILSAFE = (1 << 4)`
- `PXX_SEND_RANGECHECK = (1 << 5)`
- `transmit_type = ['FCC', 'EU', 'EU+', 'AU+']`

**decode() 主循环逻辑：**
1. 等待第一个下降沿 `{0: 'f'}`
2. 主循环：
   - 记录 `start_samplenum = samplenum`
   - 等待上升沿 `{0: 'r'}` → `end_samplenum`
   - 等待下降沿 `{0: 'f'}` → 计算 period 和 duty
   - `period = samplenum - start_samplenum`
   - `duty = end_samplenum - start_samplenum`
   - `period_t = period / samplerate`
   - 如果 `23us <= period_t <= 25us` → addBit(1)
   - 如果 `15us <= period_t <= 17us` → addBit(0)
   - 如果 `period_t >= 40us` → addBit(0) + breakRX()

**addBit(value) 逻辑（核心）：**
1. `bit_one_cnt += 1`
2. 如果 `bit_stuffing == False` 或 `bit_one_cnt < 6`：
   - `byte <<= 1; byte |= value; cur_bit += 1`
   - 输出 bit 注解
   - `state_word <<= 1; state_word |= value; state_bit += 1`
3. 否则（bit stuffing）：
   - 输出 bit_stuff 注解 "S"
   - `bstuff = 1`
4. 如果 `value == 0`：`bit_one_cnt = 0`
5. 如果 `cur_bit == 8`：调用 `addByte()`
6. 然后根据当前 state 检查 `state_bit` 是否达到目标位数

**各状态目标位数：**

| 状态 | 目标位数 | 输出 |
|------|----------|------|
| wait_header | 8 | Start Header / Header error |
| rx_model_id | 8 | Model ID: %d |
| rx_type | 2 | Type: FCC/EU/EU+/AU+ |
| rx_range_check | 1 | Range Check: On/Off |
| rx_fail_safe | 1 | FailSafe: On/Off |
| rx_country_code | 3 | CountryCode: %u |
| rx_bind | 1 | Bind: On/Off |
| rx_flag2 | 8 | Flag2: %u |
| rx_channels | 96 | Channels (1-8)/(9-16): [...] |
| rx_rsrv2 | 1 | Reserved |
| rx_euplus | 1 | EUPlus: Yes/No |
| rx_disable_sport | 1 | SPort: Disabled/Enable |
| rx_powerlevel | 2 | PowerLevel: %u |
| rx_highchan | 1 | RX HighChannel: Yes/No |
| rx_telemetry_off | 1 | Telemetry: Off/On |
| rx_external_antena | 1 | ExternalAntena: Yes/No |
| rx_crc | 16 | CRC: 0x%04X |
| rx_stop | 8 | Stop Header / Header error |

**rx_channels 特殊逻辑：**
- 每 4 bit 收集一个 nibble（但排除 bit stuffing 的位）
- 96 bit = 24 nibbles = 4 组通道
- 每组 6 nibbles：ch1 = nibble[3]<<8 | nibble[1]<<4 | nibble[0]; ch2 = nibble[4]<<8 | nibble[5]<<4 | nibble[2]
- 如果 ch1 > 2048 → 标记为 (9-16)

**breakRX() 逻辑：**
- 重置所有状态变量，回到 wait_header

### 2.3 C 实现规划

#### 状态枚举
```c
enum pxx1_state {
    STATE_WAIT_HEADER = 0,
    STATE_RX_MODEL_ID,
    STATE_RX_TYPE,
    STATE_RX_RANGE_CHECK,
    STATE_RX_FAIL_SAFE,
    STATE_RX_COUNTRY_CODE,
    STATE_RX_BIND,
    STATE_RX_FLAG2,
    STATE_RX_CHANNELS,
    STATE_RX_RSRV2,
    STATE_RX_EUPLUS,
    STATE_RX_DISABLE_SPORT,
    STATE_RX_POWERLEVEL,
    STATE_RX_HIGHCHAN,
    STATE_RX_TELEMETRY_OFF,
    STATE_RX_EXTERNAL_ANTENA,
    STATE_RX_CRC,
    STATE_RX_STOP,
    STATE_ERROR,
};
```

#### 私有数据结构
```c
struct pxx1_priv {
    uint64_t samplerate;
    int out_ann;
    int out_binary;

    // 位/字节累积
    uint8_t byte_val;
    int byte_cnt;
    int cur_bit;
    int bit_one_cnt;
    uint64_t byte_start;
    int bit_stuffing;

    // 状态机
    uint32_t state_word;     // 当前状态字累积
    uint64_t state_word_start; // 当前状态字起始
    int state_bit;           // 当前状态字位数
    int state;

    // Channels 解码
    uint8_t nibbles[24];     // 最多 24 个 nibble
    int nibble_cnt;
    int nibble_val;          // 当前 nibble 累积
    int nibble_bit_cnt;      // 当前 nibble 位数（排除 stuffing）

    // 时间测量
    uint64_t start_samplenum;
    uint64_t ss_block;
    uint64_t es_block;
};
```

#### 关键代码片段 — 主 decode 循环
```c
// 等待第一个下降沿
{
    srd_cond_builder *cb = c_cond_new();
    c_cond_fall(cb, 0);
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;
}

while (1) {
    uint64_t start_samplenum = samplenum;
    // 等待上升沿
    {
        srd_cond_builder *cb = c_cond_new();
        c_cond_rise(cb, 0);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;
    }
    uint64_t end_samplenum = samplenum;
    // 等待下降沿
    {
        srd_cond_builder *cb = c_cond_new();
        c_cond_fall(cb, 0);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;
    }
    s->ss_block = start_samplenum;
    s->es_block = samplenum;

    uint64_t period = samplenum - start_samplenum;
    double period_t = (double)period / (double)s->samplerate;

    if (period_t >= 0.000023 && period_t <= 0.000025) {
        pxx1_add_bit(di, 1);
    } else if (period_t >= 0.000015 && period_t <= 0.000017) {
        pxx1_add_bit(di, 0);
    } else if (period_t >= 0.000040) {
        // 修正 es_block
        s->es_block = s->ss_block + (uint64_t)((double)s->samplerate / 1000000.0 * 16.0);
        pxx1_add_bit(di, 0);
        pxx1_break_rx(di);
    }
}
```

#### 关键代码片段 — addBit 函数
```c
static void pxx1_add_bit(struct srd_decoder_inst *di, int value)
{
    struct pxx1_priv *s = (struct pxx1_priv *)c_decoder_get_private(di);
    int bstuff = 0;

    if (s->cur_bit == 0)
        s->byte_start = s->ss_block;

    if (s->state_bit == 0)
        s->state_word_start = s->ss_block;

    s->bit_one_cnt += 1;

    if (!s->bit_stuffing || s->bit_one_cnt < 6) {
        s->byte_val <<= 1;
        s->byte_val |= value;
        s->cur_bit += 1;
        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%X", value);
        C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, 1, bit_str);
        s->state_word <<= 1;
        s->state_word |= value;
        s->state_bit += 1;
    } else {
        bstuff = 1;
        C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, 2, "S");
    }

    if (value == 0)
        s->bit_one_cnt = 0;

    if (s->cur_bit == 8) {
        pxx1_add_byte(di, s->byte_val);
        s->cur_bit = 0;
        s->byte_val = 0;
    }

    pxx1_process_state(di, bstuff);
}
```

#### 关键代码片段 — Channels 解码
```c
// 在 STATE_RX_CHANNELS 状态处理中
if (bstuff == 0 && s->state_bit > 0 && s->state_bit % 4 == 0) {
    // 保存 nibble
    if (s->nibble_cnt < 24) {
        s->nibbles[s->nibble_cnt] = s->nibble_val;
        s->nibble_cnt++;
    }
    s->nibble_val = 0;
}

// 在 state_bit 达到 96 时
if (s->state_bit == 96) {
    char out_buf[256] = "";
    int is_upper = 0;
    int idx = 0;
    while (idx + 5 < s->nibble_cnt) {
        int ch1 = (s->nibbles[idx+3] << 8) | (s->nibbles[idx+1] << 4) | s->nibbles[idx];
        int ch2 = (s->nibbles[idx+4] << 8) | (s->nibbles[idx+5] << 4) | s->nibbles[idx+2];
        char pair[32];
        snprintf(pair, sizeof(pair), "%s%04u %04u", (idx > 0 ? " " : ""), ch1, ch2);
        strcat(out_buf, pair);
        if (ch1 > 2048) is_upper = 1;
        idx += 6;
    }
    char ann_text[300];
    snprintf(ann_text, sizeof(ann_text), "Channels %s: [%s]",
             is_upper ? "(9-16)" : "(1-8)", out_buf);
    C_ANN_PUT(di, s->state_word_start, s->es_block, s->out_ann, 11, ann_text);
    // 重置
    s->state_bit = 0;
    s->state_word = 0;
    s->state = STATE_RX_RSRV2;
}
```

### 2.4 注意事项
- Python 中 `byte_cnt > 18` 时关闭 bit_stuffing，C 中需同样实现
- Python 中 nibble 累积使用列表，C 中使用固定数组 `nibbles[24]`
- `transmit_type` 数组需要硬编码在 C 中
- Python 的 `breakRX()` 在 `period_t >= 40us` 时调用，同时修正 `es_block`
- samplerate 必须在 decode 入口检查

---

## 3. Qi 解码器 (`qi_c`)

### 3.1 Python 元数据提取

| 属性 | 值 |
|------|-----|
| id | `qi` |
| name | `Qi` |
| longname | `Qi charger protocol` |
| desc | `Protocol used by Qi receiver.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Embedded/industrial', 'Wireless/RF']` |

**Channels:**

| # | id | name | desc | idn |
|---|----|------|------|-----|
| 0 | qi | Qi | Demodulated Qi data line | dec_qi_chan_qi |

**Options:** 无

**Annotations (8个):**

| Index | id | Name |
|-------|----|------|
| 0 | bits | Bits |
| 1 | bytes-errors | Bit errors |
| 2 | bytes-start | Start bits |
| 3 | bytes-info | Info bits |
| 4 | bytes-data | Data bytes |
| 5 | packets-data | Packet data |
| 6 | packets-checksum-ok | Packet checksum |
| 7 | packets-checksum-err | Packet checksum |

**Annotation Rows:**

| Row id | Name | Classes |
|--------|------|---------|
| bits | Bits | 0 |
| bytes | Bytes | 1, 2, 3, 4 |
| packets | Packets | 5, 6, 7 |

**Binary:** 无

### 3.2 状态机分析

Qi 解码器有 2 个主状态：`IDLE` 和 `DATA`

**核心概念：**
- `bit_width = samplerate / 2000`（2kHz 通信速率）
- 差分编码：通过测量高低电平持续时间来解码 0 和 1
- 使用 `deque(maxlen=2)` 跟踪最近两个低电平持续时间

**decode() 逻辑：**
1. 读取初始引脚值
2. 调用 `handle_transition(samplenum, qi == 0)`
3. 主循环：等待任一边沿 `{0: 'e'}`，计算 `l = samplenum - prev`
4. 调用 `handle_transition(l, qi == 0)`

**handle_transition(l, htl) 逻辑：**
1. 将 `l`（低电平持续时间）加入 deque
2. 如果 deque 有 2 个元素且 `(l[-1] + l[-2])` 在 bit_width 容差内 → bit 1
   - 或者 `htl`（高到低跳变）且 `l * 2` 在容差内且 `l[-2] > 1.25 * bit_width` → bit 1
3. 否则如果 `l` 在 bit_width 容差内 → bit 0
4. 否则如果 `l > 1.25 * bit_width` → 回到 IDLE
5. 清空 deque

**add_bit(bit) 逻辑：**
1. 将 bit 加入 bits 列表，记录 samplenum
2. 如果 state == IDLE 且 bits[-5:] == [1,1,1,1,0]：
   - 转入 DATA 状态
   - `bytestart = bitsi[-2]`
   - 清空 bits，设置 bits = [0], bitsi = [samplenum]
   - 清空 packet
3. 如果 state == DATA 且 len(bits) == 11：
   - 调用 process_byte()
   - 清空 bits 和 bitsi

**process_byte() 逻辑：**
1. 检查 start bit（bits[0] == 0 → Start bit，否则 Start error）
2. 提取 data bits [1:9]，LSB first 转为 uint
3. 计算奇偶校验
4. 检查 parity bit（bits[9]）
5. 检查 stop bit（bits[10] == 1 → Stop bit，否则 Stop error）
6. 将 data 加入 packet
7. 如果 `packet_len(packet[0]) + 2 == len(packet)` → process_packet()

**packet_len(byte) 函数：**
```
0x00-0x1f → 1 + (byte - 0) / 32
0x20-0x7f → 2 + (byte - 32) / 16
0x80-0xdf → 8 + (byte - 128) / 8
0xe0-0xff → 20 + (byte - 224) / 4
```

**process_packet() 逻辑：**
- 根据 packet[0] 解析不同类型：
  - 0x01: Signal Strength
  - 0x02: End Power Transfer（含 end_codes 查表）
  - 0x03: Control Error（有符号）
  - 0x04: Received Power
  - 0x05: Charge Status
  - 0x06: Power Control Hold-off
  - 0x51: Configuration（解析 Power Class, Maximum Power 等）
  - 0x71: Identification（解析 Version, Manufacturer, Device）
  - 0x81: Extended Identification
  - 其他特定值: Proprietary
  - 其他: Unknown
- 最后检查 checksum（XOR 所有字节）

**checksum 计算：**
```c
static uint8_t qi_calc_checksum(const uint8_t *packet, int len)
{
    uint8_t cs = 0;
    for (int i = 0; i < len - 1; i++)
        cs ^= packet[i];
    return cs;
}
```

### 3.3 C 实现规划

#### 状态枚举
```c
enum qi_state {
    STATE_IDLE,
    STATE_DATA,
};
```

#### 私有数据结构
```c
struct qi_priv {
    uint64_t samplerate;
    double bit_width;
    int out_ann;

    int state;
    uint64_t lastbit;    // 上一个 bit 的 samplenum

    // deque 替代：保存最近两个低电平持续时间
    uint64_t deq[2];
    int deq_len;

    // 位累积
    int bits[12];          // 当前字节的位（最多 11 位）
    uint64_t bitsi[12];    // 每个位的 samplenum
    int bits_len;

    uint64_t bytestart;

    // 包累积
    uint8_t packet[32];    // Qi 包最大约 28 字节
    int packet_len_count;
    uint64_t bytesi[32];   // 每个字节的起始 samplenum
    int bytesi_len;

    uint64_t prev_samplenum;
};
```

#### 关键代码片段 — handle_transition
```c
static void qi_handle_transition(struct srd_decoder_inst *di,
                                  uint64_t l, int htl)
{
    struct qi_priv *s = (struct qi_priv *)c_decoder_get_private(di);

    // 加入 deque
    if (s->deq_len < 2) {
        s->deq[s->deq_len] = l;
        s->deq_len++;
    } else {
        s->deq[0] = s->deq[1];
        s->deq[1] = l;
    }

    double bw = s->bit_width;
    double lo = 0.75 * bw;
    double hi = 1.25 * bw;

    if (s->deq_len >= 2) {
        double sum = (double)(s->deq[s->deq_len-1] + s->deq[s->deq_len-2]);
        if (lo < sum && sum < hi) {
            qi_add_bit(di, 1);
            s->deq_len = 0;
            return;
        }
        if (htl && lo < l * 2.0 && l * 2.0 < hi &&
            (double)s->deq[s->deq_len-2] > hi) {
            qi_add_bit(di, 1);
            s->deq_len = 0;
            return;
        }
    }

    if (lo < (double)l && (double)l < hi) {
        qi_add_bit(di, 0);
        s->deq_len = 0;
    } else if ((double)l > hi) {
        // 回到 IDLE
        s->state = STATE_IDLE;
        s->bytesi_len = 0;
        s->packet_len_count = 0;
        s->bits_len = 0;
        s->deq_len = 0;
    }
}
```

#### 关键代码片段 — add_bit (IDLE 状态检测前导码)
```c
static void qi_add_bit(struct srd_decoder_inst *di, int bit)
{
    struct qi_priv *s = (struct qi_priv *)c_decoder_get_private(di);

    if (s->bits_len < 12) {
        s->bits[s->bits_len] = bit;
        s->bitsi[s->bits_len] = c_decoder_get_last_samplenum(di);
        s->bits_len++;
    }

    // IDLE 状态：检测前导码 [1,1,1,1,0]
    if (s->state == STATE_IDLE && s->bits_len >= 5) {
        int *b = s->bits;
        if (b[s->bits_len-5] == 1 && b[s->bits_len-4] == 1 &&
            b[s->bits_len-3] == 1 && b[s->bits_len-2] == 1 &&
            b[s->bits_len-1] == 0) {
            s->state = STATE_DATA;
            s->bytestart = s->bitsi[s->bits_len - 2];
            // 清空 bits，设置 start bit = 0
            s->bits[0] = 0;
            s->bitsi[0] = c_decoder_get_last_samplenum(di);
            s->bits_len = 1;
            s->packet_len_count = 0;
            s->bytesi_len = 0;
        }
    }
    // DATA 状态：累积 11 位
    else if (s->state == STATE_DATA && s->bits_len == 11) {
        qi_process_byte(di);
        s->bytestart = c_decoder_get_last_samplenum(di);
        s->bits_len = 0;
    }

    // 输出 bit 注解
    if (s->state != STATE_IDLE) {
        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%d", bit);
        C_ANN_PUT(di, s->lastbit, c_decoder_get_last_samplenum(di),
                  s->out_ann, 0, bit_str);
    }
    s->lastbit = c_decoder_get_last_samplenum(di);
}
```

#### 关键代码片段 — process_packet
```c
static void qi_process_packet(struct srd_decoder_inst *di)
{
    struct qi_priv *s = (struct qi_priv *)c_decoder_get_private(di);
    uint8_t *p = s->packet;
    int plen = s->packet_len_count;
    char text[256];

    if (p[0] == 0x01) {
        snprintf(text, sizeof(text), "Signal Strength: %d", p[1]);
    } else if (p[0] == 0x02) {
        const char *reason = (p[1] < 9) ? end_codes[p[1]] : "Reserved";
        snprintf(text, sizeof(text), "End Power Transfer: %s", reason);
    } else if (p[0] == 0x03) {
        int val = (p[1] < 128) ? p[1] : (int)(p[1] & 0x7f) - 128;
        snprintf(text, sizeof(text), "Control Error: %d", val);
    } else if (p[0] == 0x04) {
        snprintf(text, sizeof(text), "Received Power: %d", p[1]);
    } else if (p[0] == 0x05) {
        snprintf(text, sizeof(text), "Charge Status: %d", p[1]);
    } else if (p[0] == 0x06) {
        snprintf(text, sizeof(text), "Power Control Hold-off: %dms", p[1]);
    } else if (p[0] == 0x51) {
        int pc = (p[1] & 0xc0) >> 7;
        int mp = p[1] & 0x3f;
        int prop = (p[3] & 0x80) >> 7;
        int count = p[3] & 0x07;
        int ws = (p[4] & 0xf8) >> 3;
        int wo = p[4] & 0x07;
        snprintf(text, sizeof(text),
                 "Configuration: Power Class = %d, Maximum Power = %d, "
                 "Prop = %d, Count = %d, Window Size = %d, Window Offset = %d",
                 pc, mp, prop, count, ws, wo);
    } else if (p[0] == 0x71) {
        // Identification - 至少 8 字节
        snprintf(text, sizeof(text), "Identification: Version = %d.%d, "
                 "Manufacturer = %02x%02x, Device = %02x%02x%02x%02x",
                 (p[1] & 0xf0) >> 4, p[1] & 0x0f,
                 p[2], p[3], p[4] & ~0x80, p[5], p[6], p[7]);
    } else if (p[0] == 0x81) {
        // Extended Identification
        snprintf(text, sizeof(text), "Extended Identification: %02x%02x%02x%02x%02x%02x%02x%02x",
                 p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
    } else {
        // 检查是否为 proprietary
        static const uint8_t prop_ids[] = {0x18,0x19,0x28,0x29,0x38,0x48,0x58,0x68,0x78,0x85,0xa4,0xc4,0xe2};
        int is_prop = 0;
        for (int i = 0; i < 13; i++) {
            if (p[0] == prop_ids[i]) { is_prop = 1; break; }
        }
        snprintf(text, sizeof(text), "%s", is_prop ? "Proprietary" : "Unknown");
    }

    C_ANN_PUT(di, s->bytesi[0], s->bytesi[s->bytesi_len - 1],
              s->out_ann, 5, text);

    // Checksum 验证
    uint8_t cs = 0;
    for (int i = 0; i < plen - 1; i++) cs ^= p[i];
    if (cs == p[plen - 1]) {
        C_ANN_PUT(di, s->bytesi[s->bytesi_len - 1],
                  c_decoder_get_last_samplenum(di),
                  s->out_ann, 6, "Checksum OK", "OK");
    } else {
        C_ANN_PUT(di, s->bytesi[s->bytesi_len - 1],
                  c_decoder_get_last_samplenum(di),
                  s->out_ann, 7, "Checksum error", "ERR");
    }
}
```

### 3.4 注意事项
- `bit_width = samplerate / 2000` 在 metadata 回调中计算
- Python 使用 `collections.deque(maxlen=2)`，C 中用固定数组 + 计数器替代
- `bits_to_uint` 是 LSB first 转换：`reduce(lambda i, v: (i >> 1) | (v << (len(bits) - 1)), bits, 0)`
- Python 中 checksum 注解使用 class 6（无论 OK 还是 ERR），C 中分用 class 6 (OK) 和 class 7 (ERR)
- `end_codes` 数组需硬编码
- `packet_len()` 函数返回的是数据字节数（不含 header 和 checksum），完整包 = packet_len + 2 字节

---

## 4. RC Encode 解码器 (`rc_encode_c`)

### 4.1 Python 元数据提取

| 属性 | 值 |
|------|-----|
| id | `rc_encode` |
| name | `RC encode` |
| longname | `Remote control encoder` |
| desc | `PT2262/HX2262/SC5262 remote control encoder protocol.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['IC', 'IR']` |

**Channels:**

| # | id | name | desc | idn |
|---|----|------|------|-----|
| 0 | data | Data | Data line | dec_rc_encode_chan_data |

**Options:**

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| remote | Remote | none | none, maplin_l95ar | dec_rc_encode_opt_remote |

**Annotations (8个):**

| Index | id | Name |
|-------|----|------|
| 0 | bit-0 | Bit 0 |
| 1 | bit-1 | Bit 1 |
| 2 | bit-f | Bit f |
| 3 | bit-U | Bit U |
| 4 | bit-sync | Bit sync |
| 5 | pin | Pin |
| 6 | code-word-addr | Code word address |
| 7 | code-word-data | Code word data |

**Annotation Rows:**

| Row id | Name | Classes |
|--------|------|---------|
| bits | Bits | 0, 1, 2, 3, 4 |
| pins | Pins | 5 |
| code-words | Code words | 6, 7 |

**Binary:** 无

### 4.2 状态机分析

RC Encode 有 2 个状态：`IDLE` 和 `DECODING`（隐含 `DECODE_TIMEOUT`）

**核心概念：**
- PT2262 编码：每个逻辑位由 4 个脉冲组成
- 逻辑 0：短-长-短-长（`-___-___`）
- 逻辑 1：长-短-长-短（`---_---_`）
- 逻辑 F（float/三态）：短-长-长-短（`---_-___`）
- 同步位：长低电平（约 8 倍脉冲宽度）

**decode() 逻辑：**
1. 等待任一边沿 `{0: 'e'}`
2. 如果 `samplenumber_last` 为 None，设置初始值并 continue
3. 如果 `bit_count < 12`：
   - 收集 4 个脉冲宽度
   - 调用 `decode_bit()` 判断位类型
   - 输出位注解和引脚标签
4. 如果 `bit_count >= 12`（同步位）：
   - 如果 model != 'none'，调用 `decode_model()`
   - 等待 8 * samples 个采样（同步位长度）
   - 输出 Sync 注解
   - 重置状态

**decode_bit(edges) 逻辑：**
- edges[0..3] 是 4 个脉冲的宽度
- 逻辑 0：edges[1] >= edges[0]*2 && edges[1] <= edges[0]*5 && edges[2] ≈ edges[0] && edges[3] ≈ edges[0]*3
- 逻辑 1：edges[0] >= edges[1]*2 && edges[0] ≈ edges[2] && edges[0] ≈ edges[3]*3
- 逻辑 F：edges[1] ≈ edges[0]*3 && edges[2] ≈ edges[0]*3 && edges[3] ≈ edges[0]
- 其他：U（未知）

**pinlabels(bit_count) 逻辑：**
- bit_count <= 6: `A{bit_count-1}`
- bit_count > 6: `A{bit_count-1}/D{12-bit_count}`

**decode_model('maplin_l95ar', bits) 逻辑：**
- 地址：A0-A5，0=on，1/f=off
- 按钮：A6/D5-A11/D0 组合判断

### 4.3 C 实现规划

#### 私有数据结构
```c
struct rc_encode_priv {
    uint64_t samplerate;
    int out_ann;

    uint64_t samplenumber_last;
    int have_last;           // 是否有上一个采样号

    uint64_t pulses[4];      // 4 个脉冲宽度
    int pulse_cnt;

    int bits[12];            // 0='0', 1='1', 2='f', 3='U'
    uint64_t bits_ss[12];    // 每个位的起始
    uint64_t bits_es[12];    // 每个位的结束
    int bit_count;

    uint64_t ss;             // 当前注解起始
    uint64_t es;             // 当前注解结束

    int state;               // IDLE / DECODING / DECODE_TIMEOUT
    int model;               // 0=none, 1=maplin_l95ar
};
```

#### 关键代码片段 — decode_bit
```c
// 返回: 0='0', 1='1', 2='f', 3='U'
static int rc_decode_bit(const uint64_t *edges)
{
    double lmin = 2.0, lmax = 5.0;
    double eqmin = 0.5, eqmax = 1.5;

    // 逻辑 0: -___-___ (短长短长)
    if ((double)edges[1] >= (double)edges[0] * lmin &&
        (double)edges[1] <= (double)edges[0] * lmax &&
        (double)edges[2] >= (double)edges[0] * eqmin &&
        (double)edges[2] <= (double)edges[0] * eqmax &&
        (double)edges[3] >= (double)edges[0] * lmin &&
        (double)edges[3] <= (double)edges[0] * lmax) {
        return 0;
    }
    // 逻辑 1: ---_---_ (长短长短)
    if ((double)edges[0] >= (double)edges[1] * lmin &&
        (double)edges[0] <= (double)edges[1] * lmax &&
        (double)edges[0] >= (double)edges[2] * eqmin &&
        (double)edges[0] <= (double)edges[2] * eqmax &&
        (double)edges[0] >= (double)edges[3] * lmin &&
        (double)edges[0] <= (double)edges[3] * lmax) {
        return 1;
    }
    // 逻辑 F: ---_-___ (短长长短)
    if ((double)edges[1] >= (double)edges[0] * lmin &&
        (double)edges[1] <= (double)edges[0] * lmax &&
        (double)edges[2] >= (double)edges[0] * lmin &&
        (double)edges[2] <= (double)edges[0] * lmax &&
        (double)edges[3] >= (double)edges[0] * eqmin &&
        (double)edges[3] <= (double)edges[0] * eqmax) {
        return 2;
    }
    return 3; // Unknown
}
```

#### 关键代码片段 — 主 decode 循环
```c
while (1) {
    srd_cond_builder *cb = c_cond_new();
    c_cond_edge(cb, 0);
    ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;

    if (!s->have_last) {
        s->samplenumber_last = samplenum;
        s->ss = samplenum;
        s->have_last = 1;
        continue;
    }

    if (s->bit_count < 12) {
        s->bit_count++;
        // 收集 4 个脉冲
        for (int i = 0; i < 4; i++) {
            if (i > 0) {
                cb = c_cond_new();
                c_cond_edge(cb, 0);
                ret = c_cond_wait(cb, di, &samplenum, &matched);
                c_cond_free(cb);
                if (ret != SRD_OK) return;
            }
            uint64_t samples = samplenum - s->samplenumber_last;
            s->pulses[i] = samples;
            s->samplenumber_last = samplenum;
        }
        s->es = samplenum;
        int bit_val = rc_decode_bit(s->pulses);
        s->bits[s->bit_count - 1] = bit_val;
        s->bits_ss[s->bit_count - 1] = s->ss;
        s->bits_es[s->bit_count - 1] = s->es;

        // 输出位注解
        static const char *bit_names[] = {"0", "1", "f", "U"};
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, bit_val, bit_names[bit_val]);

        // 输出引脚标签
        char pin_label[16];
        if (s->bit_count <= 6)
            snprintf(pin_label, sizeof(pin_label), "A%d", s->bit_count - 1);
        else
            snprintf(pin_label, sizeof(pin_label), "A%d/D%d",
                     s->bit_count - 1, 12 - s->bit_count);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, 5, pin_label);

        s->ss = samplenum;
    } else {
        // 同步位
        if (s->model == 1) {
            rc_decode_model(di);
        }
        uint64_t samples = samplenum - s->samplenumber_last;
        // 等待同步位结束
        {
            cb = c_cond_new();
            c_cond_skip(cb, 8 * samples);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;
        }
        s->es = samplenum;
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, 4, "Sync");
        // 重置
        rc_encode_reset_state(di);
        s->state = 2; // DECODE_TIMEOUT
    }

    if (s->state != 2) {
        s->samplenumber_last = samplenum;
    }
}
```

### 4.4 注意事项
- Python 中 `decode_bit` 使用浮点比较，C 中同样使用 double 比较
- `pinlabels` 函数需要正确处理 bit_count 从 1 开始（不是 0）
- `decode_model` 仅在 model == 'maplin_l95ar' 时调用
- Option `remote` 是字符串枚举类型，需在 `srd_c_decoder_entry()` 中用 `g_variant_new_string()` 初始化
- 同步位使用 `c_cond_skip()` 等待

---

## 5. SDQ 解码器 (`sdq_c`)

### 5.1 Python 元数据提取

| 属性 | 值 |
|------|-----|
| id | `sdq` |
| name | `SDQ` |
| longname | `Texas Instruments SDQ` |
| desc | `Texas Instruments SDQ. The SDQ protocol is also used by Apple.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Embedded/industrial']` |

**Channels:**

| # | id | name | desc | idn |
|---|----|------|------|-----|
| 0 | sdq | SDQ | Single wire SDQ data line. | dec_sdq_chan_sdq |

**Options:**

| id | desc | default | idn |
|----|------|---------|-----|
| bitrate | Bit rate | 98425 | dec_sdq_opt_bitrate |

**Annotations (3个):**

| Index | id | Name |
|-------|----|------|
| 0 | bit | Bit |
| 1 | byte | Byte |
| 2 | break | Break |

**Annotation Rows:**

| Row id | Name | Classes |
|--------|------|---------|
| bits | Bits | 0 |
| bytes | Bytes | 1 |
| breaks | Breaks | 2 |

**Binary:** 无

### 5.2 状态机分析

SDQ 解码器**无状态机**，是最简单的解码器之一。

**核心概念：**
- `bit_width = samplerate / bitrate`
- `half_bit_width = bit_width / 2`
- `break_threshold = bit_width * 1.2`
- 通过测量低脉冲宽度来区分 0、1 和 BREAK

**decode() 逻辑：**
1. 计算 `bit_width`, `half_bit_width`, `break_threshold`
2. 等待线路变高 `{0: 'h'}`
3. 主循环：
   - 等待下降沿 `{0: 'f'}` → `startsample = samplenum`
   - 如果 `bytepos == 0`：`bytepos = samplenum`
   - 等待上升沿 `{0: 'r'}`
   - `delta = samplenum - startsample`
   - 如果 `delta > break_threshold` → handle_break()
   - 否则如果 `delta > half_bit_width` → handle_bit(0)
   - 否则 → handle_bit(1)

**handle_bit(bit) 逻辑：**
1. 将 bit 加入 bits 列表
2. 输出 bit 注解（从 startsample 到 startsample + bit_width）
3. 如果 bits 满了（8 位）：
   - 用 `bitpack(bits)` 打包为字节
   - 输出 byte 注解（从 bytepos 到 startsample + bit_width）
   - 清空 bits，重置 bytepos

**handle_break() 逻辑：**
1. 输出 Break 注解
2. 清空 bits，重置 startsample 和 bytepos

**bitpack 函数（来自 common.srdhelper）：**
- LSB first：bits[0] 是最低位
- `byte = bits[0] | (bits[1] << 1) | ... | (bits[7] << 7)`

### 5.3 C 实现规划

#### 私有数据结构
```c
struct sdq_priv {
    uint64_t samplerate;
    int out_ann;

    double bit_width;
    double half_bit_width;
    double break_threshold;

    int bits[8];
    int bits_len;
    uint64_t startsample;
    uint64_t bytepos;
};
```

#### 关键代码片段 — 完整 decode
```c
static void sdq_decode(struct srd_decoder_inst *di)
{
    struct sdq_priv *s = (struct sdq_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched;
    int ret;

    if (s->samplerate == 0) return;

    int64_t bitrate = c_decoder_get_option_int(di, "bitrate", 98425);
    s->bit_width = (double)s->samplerate / (double)bitrate;
    s->half_bit_width = s->bit_width / 2.0;
    s->break_threshold = s->bit_width * 1.2;

    // 等待线路变高
    {
        srd_cond_builder *cb = c_cond_new();
        c_cond_high(cb, 0);
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;
    }

    while (1) {
        // 等待下降沿
        {
            srd_cond_builder *cb = c_cond_new();
            c_cond_fall(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;
        }
        s->startsample = samplenum;
        if (s->bytepos == 0)
            s->bytepos = samplenum;

        // 等待上升沿
        {
            srd_cond_builder *cb = c_cond_new();
            c_cond_rise(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;
        }

        uint64_t delta = samplenum - s->startsample;

        if ((double)delta > s->break_threshold) {
            // BREAK
            C_ANN_PUT(di, s->startsample, samplenum, s->out_ann, 2, "Break", "BR");
            s->bits_len = 0;
            s->startsample = samplenum;
            s->bytepos = 0;
        } else if ((double)delta > s->half_bit_width) {
            // Bit 0
            sdq_handle_bit(di, 0);
        } else {
            // Bit 1
            sdq_handle_bit(di, 1);
        }
    }
}
```

#### 关键代码片段 — handle_bit
```c
static void sdq_handle_bit(struct srd_decoder_inst *di, int bit)
{
    struct sdq_priv *s = (struct sdq_priv *)c_decoder_get_private(di);

    s->bits[s->bits_len] = bit;
    s->bits_len++;

    uint64_t bit_end = s->startsample + (uint64_t)s->bit_width;

    char bit_long[16], bit_short[4];
    snprintf(bit_long, sizeof(bit_long), "Bit: %d", bit);
    snprintf(bit_short, sizeof(bit_short), "%d", bit);
    C_ANN_PUT(di, s->startsample, bit_end, s->out_ann, 0, bit_long, bit_short);

    if (s->bits_len == 8) {
        // bitpack: LSB first
        uint8_t byte_val = 0;
        for (int i = 0; i < 8; i++)
            byte_val |= (s->bits[i] << i);

        char byte_long[16], byte_short[8];
        snprintf(byte_long, sizeof(byte_long), "Byte: 0x%02x", byte_val);
        snprintf(byte_short, sizeof(byte_short), "0x%02x", byte_val);
        C_ANN_PUT(di, s->bytepos, bit_end, s->out_ann, 1, byte_long, byte_short);

        s->bits_len = 0;
        s->bytepos = 0;
    }
}
```

#### Options 初始化
```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    sdq_options[0].idn = "dec_sdq_opt_bitrate";
    sdq_options[0].def = g_variant_new_int64(98425);
    return &sdq_c_decoder;
}
```

### 5.4 注意事项
- 这是最简单的解码器，无状态机
- `bitpack` 是 LSB first 打包
- Python 中 `bit_width` 使用 `float(samplerate) / float(options['bitrate'])`，C 中用 double
- 注解的时间范围：bit 注解从 startsample 到 startsample + bit_width；byte 注解从 bytepos 到 startsample + bit_width
- `break_threshold = bit_width * 1.2` 是硬编码的倍数

---

## CMakeLists.txt 修改

在 `CMakeLists.txt` 第 837 行的 `C_DECODERS` 列表末尾添加：

```
mipi_dsi_c pxx1_c qi_c rc_encode_c sdq_c
```

修改后的行应为：
```cmake
set(C_DECODERS spi_c i2c_c uart_c can_c jtag_c swd_c onewire_c i2s_c lin_c hdlc_c microwire_c mdio_c ps2_c dmx512_c nrzi_c ir_nec_c ir_rc5_c dcf77_c cec_c spdif_c usb_signalling_c 4b5b_c can_fd_c iso7816_c lpc_c dali_c c2_c graycode_c counter_c lm75_c ds1307_c ds3231_c numbers_and_state_c seven_segment_c pwm_c wiegand_c ir_sirc_c mipi_dsi_c pxx1_c qi_c rc_encode_c sdq_c)
```

<!-- Updated: C解码器依赖规则审查结果：
     - mipi_dsi: inputs=['logic'], outputs=['mipi_dsi'] — 无依赖阻塞。
       上层解码器依赖 mipi_dsi 协议时，需确保 Python 输出格式兼容。
     - pxx1: inputs=['logic'], outputs=[] — 无依赖问题。
     - qi: inputs=['logic'], outputs=[] — 无依赖问题。
     - rc_encode: inputs=['logic'], outputs=[] — 无依赖问题。
     - sdq: inputs=['logic'], outputs=[] — 无依赖问题。
     所有解码器均直接输入 logic，无 Python 解码器依赖阻塞。
     BITS v2 格式和 SPI DATA 17字节格式与本批次无关（无 spi/i2c 输出解码器）。 -->
