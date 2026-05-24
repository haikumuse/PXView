# Python→C 解码器移植规格 — Batch 17

## 概述

本批次移植 2 个 Python 解码器到 C 实现：

| # | Python ID | C ID | 名称 | 协议类型 |
|---|-----------|------|------|----------|
| 1 | `tlc5620` | `tlc5620_c` | TI TLC5620 8-bit Quad DAC | SPI-like 串行 DAC |
| 2 | `xy2-100` | `xy2_100_c` | XY2-100(E) / XY-200(E) 振镜定位协议 | 同步串行振镜控制 | <!-- Updated: C解码器ID和文件名使用下划线替代连字符，与现有C解码器命名规范一致 -->

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |

<!-- Updated: c_cond_wait_current()已实现，等效于Python的self.wait({})，可用于TLC5620中等待初始LDAC电平 -->
<!-- Updated: c_decoder_get_initial_pin()已实现，可用于获取LOAD/LDAC初始电平 -->


## 1. TLC5620 解码器详细规格

### 1.1 Python 元数据提取

| 属性 | Python 值 | C 映射 |
|------|----------|--------|
| `id` | `'tlc5620'` | `.id = "tlc5620_c"` |
| `name` | `'TI TLC5620'` | `.name = "TLC5620(C)"` |
| `longname` | `'Texas Instruments TLC5620'` | `.longname = "Texas Instruments TLC5620 (C)"` |
| `desc` | `'Texas Instruments TLC5620 8-bit quad DAC.'` | `.desc = "Texas Instruments TLC5620 8-bit quad DAC. (C implementation)"` |
| `license` | `'gplv2+'` | `.license = "gplv2+"` |
| `inputs` | `['logic']` | `.inputs = {"logic"}` |
| `outputs` | `[]` | `.outputs = NULL, .num_outputs = 0` |
| `tags` | `['IC', 'Analog/digital']` | `.tags = {"IC", "Analog/digital"}` |

### 1.2 通道定义

**必需通道 (channels):**

| 索引 | id | name | desc | idn | type |
|------|----|------|------|-----|------|
| 0 | `clk` | `CLK` | `Serial interface clock` | `dec_tlc5620_chan_clk` | `SRD_CHANNEL_SCLK` |
| 1 | `data` | `DATA` | `Serial interface data` | `dec_tlc5620_chan_data` | `SRD_CHANNEL_SDATA` |

**可选通道 (optional_channels):**

| 索引 | id | name | desc | idn | type |
|------|----|------|------|-----|------|
| 2 | `load` | `LOAD` | `Serial interface load control` | `dec_tlc5620_opt_chan_load` | `SRD_CHANNEL_COMMON` |
| 3 | `ldac` | `LDAC` | `Load DAC` | `dec_tlc5620_opt_chan_ldac` | `SRD_CHANNEL_COMMON` |

### 1.3 选项定义

| id | desc | 类型 | 默认值 | idn |
|----|------|------|--------|-----|
| `vref_a` | `Reference voltage DACA (V)` | double | 3.3 | `dec_tlc5620_opt_vref_a` |
| `vref_b` | `Reference voltage DACB (V)` | double | 3.3 | `dec_tlc5620_opt_vref_b` |
| `vref_c` | `Reference voltage DACC (V)` | double | 3.3 | `dec_tlc5620_opt_vref_c` |
| `vref_d` | `Reference voltage DACD (V)` | double | 3.3 | `dec_tlc5620_opt_vref_d` |

### 1.4 注解定义

**注解类 (annotations):**

| 索引 | id | 标签 | C enum |
|------|----|------|--------|
| 0 | `dac-select` | `DAC select` | `ANN_DAC_SELECT` |
| 1 | `gain` | `Gain` | `ANN_GAIN` |
| 2 | `value` | `DAC value` | `ANN_VALUE` |
| 3 | `data-latch` | `Data latch point` | `ANN_DATA_LATCH` |
| 4 | `ldac-fall` | `LDAC falling edge` | `ANN_LDAC_FALL` |
| 5 | `bit` | `Bit` | `ANN_BIT` |
| 6 | `reg-write` | `Register write` | `ANN_REG_WRITE` |
| 7 | `voltage-update` | `Voltage update` | `ANN_VOLTAGE_UPDATE` |
| 8 | `voltage-update-all` | `Voltage update (all DACs)` | `ANN_VOLTAGE_UPDATE_ALL` |
| 9 | `invalid-cmd` | `Invalid command` | `ANN_INVALID_CMD` |

**注解行 (annotation_rows):**

| id | desc | 包含的 ann class 索引 |
|----|------|----------------------|
| `bits` | `Bits` | (5,) |
| `fields` | `Fields` | (0, 1, 2) |
| `registers` | `Registers` | (6, 7) |
| `voltage-updates` | `Voltage updates` | (8,) |
| `events` | `Events` | (3, 4) |
| `errors` | `Errors` | (9,) |

### 1.5 解码逻辑分析

#### 1.5.1 协议概述

TLC5620 是 TI 的 8 位四通道 DAC。数据通过 SPI-like 串行接口写入：
- **CLK 下降沿**采样 DATA 引脚（MSB first）
- **LOAD 下降沿**锁存数据
- **LDAC 低电平**时立即更新输出电压；LDAC 高电平时仅写入寄存器，等 LDAC 下降沿时同时更新所有 DAC

#### 1.5.2 数据帧格式（11 bits）

```
Bit:   [0] [1] [2] [3] [4] [5] [6] [7] [8] [9] [10]
       |DAC sel| G |<------- DAC value ---------->|
        A=00    0=x1  8-bit value (MSB first)
        B=01    1=x2
        C=10
        D=11
```

- Bit 0-1: DAC 选择（A/B/C/D）
- Bit 2: 增益（0=x1, 1=x2）
- Bit 3-10: DAC 值（8-bit, MSB first）

#### 1.5.3 Python 状态机与等待条件

Python 使用 `self.wait([{0: 'f'}, {2: 'f'}, {3: 'f'}])` 同时等待三个条件：
1. CLK 下降沿（channel 0 falling）
2. LOAD 下降沿（channel 2 falling）
3. LDAC 下降沿（channel 3 falling）

然后通过 `self.matched` 位掩码判断哪个条件匹配，分别处理：
- `matched & 0b1` → CLK 下降沿 → `handle_new_dac_bit(data)` — 采样 DATA 引脚，追加到 bits 列表
- `matched & 0b10` → LOAD 下降沿 → `handle_falling_edge_load()` — 锁存并处理 11-bit 数据
- `matched & 0b100` → LDAC 下降沿 → `handle_falling_edge_ldac()` — 更新所有 DAC 电压

#### 1.5.4 关键算法

**handle_11bits():**
1. 如果 bits > 11，只取最后 11 位（TLC5620 忽略多余位）
2. 如果 bits < 11，报错 "Command too short"，返回 False
3. 解析 bit[0:2] → DAC 选择（00=A, 01=B, 10=C, 11=D）
4. 解析 bit[2] → 增益（0=x1, 1=x2）
5. 解析 bit[3:11] → DAC 值（8-bit MSB first）
6. 输出每个 bit 的注解
7. 返回 True

**handle_falling_edge_load():**
1. 调用 `handle_11bits()`
2. 输出 LOAD 下降沿注解
3. 计算电压：`V = Vref * (value / 256) * gain`
4. 如果 LDAC == 0：输出 "Setting {DAC} voltage to {V}" 注解（ANN_VOLTAGE_UPDATE）
5. 如果 LDAC == 1：输出 "Setting {DAC} register value to {V}" 注解（ANN_REG_WRITE）
6. 保存 DAC 值和增益到 `dacval` / `gains` 字典

**handle_falling_edge_ldac():**
1. 输出 LDAC 下降沿注解
2. 如果没有见过任何 register write（`ss_dac_first is None`），直接返回
3. 遍历所有 4 个 DAC（A/B/C/D），根据 Vref 和增益计算电压
4. 输出 "Updating voltages: DACA=xxV DACB=xxV DACC=xxV DACD=xxV" 注解（ANN_VOLTAGE_UPDATE_ALL）
5. 重置 `ss_dac_first`

#### 1.5.5 边界情况

- bits 数组超过 11 位时截断为最后 11 位
- bits 数组不足 11 位时报错
- LOAD 下降沿时 LDAC 状态决定是立即更新还是写入寄存器
- LDAC 下降沿时如果没有先前的 register write 则不输出注解
- 电压计算中 Vref 通过选项获取，每个 DAC 可独立配置
- `clock_width` 用于估算最后一个 bit 的结束位置

### 1.6 C 实现计划

#### 1.6.1 状态结构体

```c
#define TLC5620_MAX_BITS 16

typedef struct {
    uint64_t samplerate;
    int out_ann;

    /* Bit 采集 */
    int bits_count;
    int bits_value[TLC5620_MAX_BITS];   /* bit 值 (0/1) */
    uint64_t bits_ss[TLC5620_MAX_BITS]; /* bit 起始 sample */
    uint64_t bits_es[TLC5620_MAX_BITS]; /* bit 结束 sample */

    /* 11-bit 帧解析结果 */
    uint64_t ss_dac_first;  /* 第一个 DAC 帧的起始 sample */
    uint64_t ss_dac, es_dac;
    uint64_t ss_gain, es_gain;
    uint64_t ss_value, es_value;
    uint64_t clock_width;
    int dac_select;         /* 0=A, 1=B, 2=C, 3=D */
    int gain;               /* 1 or 2 */
    int dac_value;          /* 0-255 */
    int ldac;               /* LDAC 当前电平 */

    /* DAC 状态 */
    int dacval[4];          /* A/B/C/D 的值, -1 表示未知 */
    int gains[4];           /* A/B/C/D 的增益 */

    /* 选项 */
    double vref[4];         /* A/B/C/D 的参考电压 */

    /* 通道存在标志 */
    int have_load;
    int have_ldac;
} tlc5620_state;
```

#### 1.6.2 条件等待策略

```c
/* 主循环：等待 CLK 下降沿 OR LOAD 下降沿 OR LDAC 下降沿 */
srd_cond_builder *cb = c_cond_new();
c_cond_fall(cb, 0);   /* CLK 下降沿 */
if (s->have_load) {
    c_cond_or(cb);
    c_cond_fall(cb, 2); /* LOAD 下降沿 */
}
if (s->have_ldac) {
    c_cond_or(cb);
    c_cond_fall(cb, 3); /* LDAC 下降沿 */
}
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

匹配后通过 `matched` 位掩码判断：
- `matched & (1ULL << 0)` → CLK 下降沿
- `matched & (1ULL << cond_idx_load)` → LOAD 下降沿
- `matched & (1ULL << cond_idx_ldac)` → LDAC 下降沿

**注意**：条件索引取决于 `have_load` 和 `have_ldac` 的组合，需要在 start() 中预计算。

#### 1.6.3 Samplerate 守卫

```c
static void tlc5620_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    tlc5620_state *s = (tlc5620_state *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->samplerate = value;
    }
}

/* decode() 入口处 */
if (!s->samplerate) {
    s->samplerate = c_decoder_get_samplerate(di);
}
if (!s->samplerate) {
    /* 无 samplerate 时仍可解码，但 clock_width 估算可能不准确 */
    /* 不直接 return，允许继续 */
}
```

#### 1.6.4 关键 C 代码片段

**handle_11bits 等价实现：**

```c
static int tlc5620_handle_11bits(struct srd_decoder_inst *di, tlc5620_state *s)
{
    /* 截断到最后 11 位 */
    if (s->bits_count > 11) {
        int skip = s->bits_count - 11;
        /* 移动最后 11 位到数组开头 */
        for (int i = 0; i < 11; i++) {
            s->bits_value[i] = s->bits_value[i + skip];
            s->bits_ss[i] = s->bits_ss[i + skip];
            s->bits_es[i] = s->bits_es[i + skip];
        }
        s->bits_count = 11;
    }

    /* 不足 11 位报错 */
    if (s->bits_count < 11) {
        uint64_t ss = s->bits_ss[0];
        uint64_t es = s->bits_es[s->bits_count - 1];
        if (s->bits_count >= 2) {
            uint64_t cw = s->bits_es[1] - s->bits_ss[1];
            es = s->bits_es[s->bits_count - 1] + cw;
        }
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_INVALID_CMD, "Command too short");
        s->bits_count = 0;
        return 0;
    }

    /* 解析 DAC 选择 (bit 0-1) */
    s->ss_dac = s->bits_ss[0];
    s->es_dac = s->bits_ss[2];
    s->ss_gain = s->bits_ss[2];
    s->es_gain = s->bits_ss[3];
    s->ss_value = s->bits_ss[3];
    s->clock_width = s->es_gain - s->ss_gain;
    s->es_value = s->bits_ss[10] + s->clock_width;

    if (s->ss_dac_first == (uint64_t)-1)
        s->ss_dac_first = s->ss_dac;

    int dac_idx = (s->bits_value[0] << 1) | s->bits_value[1];
    const char *dac_names[] = {"DACA", "DACB", "DACC", "DACD"};
    s->dac_select = dac_idx;

    char dac_str[32];
    snprintf(dac_str, sizeof(dac_str), "DAC select: %s", dac_names[dac_idx]);
    C_ANN_PUT(di, s->ss_dac, s->es_dac, s->out_ann, ANN_DAC_SELECT,
              dac_str, dac_names[dac_idx], dac_names[dac_idx], dac_names[dac_idx] + 3);

    /* 解析增益 (bit 2) */
    s->gain = 1 + s->bits_value[2];
    char gain_str[16];
    snprintf(gain_str, sizeof(gain_str), "Gain: x%d", s->gain);
    char gain_short[8];
    snprintf(gain_short, sizeof(gain_short), "x%d", s->gain);
    C_ANN_PUT(di, s->ss_gain, s->es_gain, s->out_ann, ANN_GAIN,
              gain_str, gain_short, gain_short);

    /* 解析 DAC 值 (bit 3-10, MSB first) */
    int val = 0;
    for (int i = 3; i < 11; i++)
        val = (val << 1) | s->bits_value[i];
    s->dac_value = val;

    char val_str[32];
    snprintf(val_str, sizeof(val_str), "DAC value: %d", val);
    char val_short[16];
    snprintf(val_short, sizeof(val_short), "%d", val);
    C_ANN_PUT(di, s->ss_value, s->es_value, s->out_ann, ANN_VALUE,
              val_str, val_short, val_short, val_short);

    /* 输出每个 bit 的注解 */
    for (int i = 1; i < 11; i++) {
        char bstr[4];
        snprintf(bstr, sizeof(bstr), "%d", s->bits_value[i - 1]);
        C_ANN_PUT(di, s->bits_ss[i - 1], s->bits_ss[i], s->out_ann, ANN_BIT, bstr);
    }
    char bstr_last[4];
    snprintf(bstr_last, sizeof(bstr_last), "%d", s->bits_value[10]);
    C_ANN_PUT(di, s->bits_ss[10], s->bits_ss[10] + s->clock_width, s->out_ann, ANN_BIT, bstr_last);

    s->bits_count = 0;
    return 1;
}
```

**handle_falling_edge_load 等价实现：**

```c
static void tlc5620_handle_load_fall(struct srd_decoder_inst *di, tlc5620_state *s, uint64_t samplenum)
{
    if (!tlc5620_handle_11bits(di, s))
        return;

    const char *dac_names[] = {"DACA", "DACB", "DACC", "DACD"};
    C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_DATA_LATCH,
              "Falling edge on LOAD", "LOAD fall", "F");

    double vref = s->vref[s->dac_select];
    double voltage = vref * ((double)s->dac_value / 256.0) * s->gain;
    char v_str[32];
    snprintf(v_str, sizeof(v_str), "%.2fV", voltage);

    if (s->ldac == 0) {
        char ann_str[64];
        snprintf(ann_str, sizeof(ann_str), "Setting %s voltage to %s", dac_names[s->dac_select], v_str);
        char short_str[32];
        snprintf(short_str, sizeof(short_str), "%s=%s", dac_names[s->dac_select], v_str);
        C_ANN_PUT(di, s->ss_dac, s->es_value, s->out_ann, ANN_VOLTAGE_UPDATE,
                  ann_str, short_str);
    } else {
        char ann_str[64];
        snprintf(ann_str, sizeof(ann_str), "Setting %s register value to %s", dac_names[s->dac_select], v_str);
        char short_str[32];
        snprintf(short_str, sizeof(short_str), "%s=%s", dac_names[s->dac_select], v_str);
        C_ANN_PUT(di, s->ss_dac, s->es_value, s->out_ann, ANN_REG_WRITE,
                  ann_str, short_str);
    }

    s->dacval[s->dac_select] = s->dac_value;
    s->gains[s->dac_select] = s->gain;
}
```

**handle_falling_edge_ldac 等价实现：**

```c
static void tlc5620_handle_ldac_fall(struct srd_decoder_inst *di, tlc5620_state *s, uint64_t samplenum)
{
    C_ANN_PUT(di, samplenum, samplenum, s->out_ann, ANN_LDAC_FALL,
              "Falling edge on LDAC", "LDAC fall", "LDAC", "L");

    if (s->ss_dac_first == (uint64_t)-1)
        return;

    const char *dac_names[] = {"A", "B", "C", "D"};
    char full_str[128];
    int pos = 0;
    for (int i = 0; i < 4; i++) {
        if (s->dacval[i] < 0) {
            pos += snprintf(full_str + pos, sizeof(full_str) - pos, "DAC%s=? ", dac_names[i]);
        } else {
            double v = s->vref[i] * ((double)s->dacval[i] / 256.0) * s->gains[i];
            pos += snprintf(full_str + pos, sizeof(full_str) - pos, "DAC%s=%.2fV ", dac_names[i], v);
        }
    }
    /* 去掉末尾空格 */
    if (pos > 0 && full_str[pos - 1] == ' ')
        full_str[pos - 1] = '\0';

    char short_str[128];
    /* 去掉 DAC 前缀的短版本 */
    int spos = 0;
    for (int i = 0; i < 4; i++) {
        if (s->dacval[i] < 0) {
            spos += snprintf(short_str + spos, sizeof(short_str) - spos, "%s=? ", dac_names[i]);
        } else {
            double v = s->vref[i] * ((double)s->dacval[i] / 256.0) * s->gains[i];
            spos += snprintf(short_str + spos, sizeof(short_str) - spos, "%s=%.2fV ", dac_names[i], v);
        }
    }
    if (spos > 0 && short_str[spos - 1] == ' ')
        short_str[spos - 1] = '\0';

    char update_str[256];
    snprintf(update_str, sizeof(update_str), "Updating voltages: %s", full_str);
    C_ANN_PUT(di, s->ss_dac_first, samplenum, s->out_ann, ANN_VOLTAGE_UPDATE_ALL,
              update_str, full_str, short_str);

    s->ss_dac_first = (uint64_t)-1;
}
```

---

## 2. XY2-100 解码器详细规格

### 2.1 Python 元数据提取

| 属性 | Python 值 | C 映射 |
|------|----------|--------|
| `id` | `'xy2-100'` | `.id = "xy2_100_c"` | <!-- Updated: C解码器ID使用下划线替代连字符，与现有C解码器命名规范一致 -->
| `name` | `'XY2-100'` | `.name = "XY2-100(C)"` |
| `longname` | `'XY2-100(E) and XY-200(E) galvanometer protocol'` | `.longname = "XY2-100(E) and XY-200(E) galvanometer protocol (C)"` |
| `desc` | `'Serial protocol for galvanometer positioning in laser systems'` | `.desc = "Serial protocol for galvanometer positioning in laser systems (C implementation)"` |
| `license` | `'gplv2+'` | `.license = "gplv2+"` |
| `inputs` | `['logic']` | `.inputs = {"logic"}` |
| `outputs` | `[]` | `.outputs = NULL, .num_outputs = 0` |
| `tags` | `['Embedded/industrial']` | `.tags = {"Embedded/industrial"}` |

### 2.2 通道定义

**必需通道 (channels):**

| 索引 | id | name | desc | idn | type |
|------|----|------|------|-----|------|
| 0 | `clk` | `CLK` | `Clock` | `dec_xy2_100_chan_clk` | `SRD_CHANNEL_SCLK` | <!-- Updated: Python源码idn为dec_xy2-100_chan_clk(含连字符)，C标识符不允许连字符，已改为下划线 -->
| 1 | `sync` | `SYNC` | `Sync` | `dec_xy2_100_chan_sync` | `SRD_CHANNEL_COMMON` | <!-- Updated: 同上 -->
| 2 | `data` | `DATA` | `X, Y or Z axis data` | `dec_xy2_100_chan_data` | `SRD_CHANNEL_SDATA` | <!-- Updated: 同上 -->

**可选通道 (optional_channels):**

| 索引 | id | name | desc | idn | type |
|------|----|------|------|-----|------|
| 3 | `status` | `STAT` | `X, Y or Z axis status` | `dec_xy2_100_opt_chan_status` | `SRD_CHANNEL_SDATA` | <!-- Updated: Python源码idn含连字符，C标识符已改为下划线 -->

### 2.3 选项定义

无选项（Python 解码器没有定义 options）。

### 2.4 注解定义

**注解类 (annotations):**

| 索引 | id | 标签 | C enum |
|------|----|------|--------|
| 0 | `bit` | `Data Bit` | `ANN_BIT` |
| 1 | `stat_bit` | `Status Bit` | `ANN_STAT_BIT` |
| 2 | `type` | `Frame Type` | `ANN_TYPE` |
| 3 | `command` | `Command` | `ANN_COMMAND` |
| 4 | `parameter` | `Parameter` | `ANN_PARAMETER` |
| 5 | `parity` | `Parity` | `ANN_PARITY` |
| 6 | `position` | `Position` | `ANN_POS` |
| 7 | `status` | `Status` | `ANN_STATUS` |
| 8 | `warning` | `Human-readable warnings` | `ANN_WARNING` |

**注解行 (annotation_rows):**

| id | desc | 包含的 ann class 索引 |
|----|------|----------------------|
| `bits` | `Data Bits` | (0,) |
| `stat_bits` | `Status Bits` | (1,) |
| `data` | `Data` | (2, 3, 4, 5) |
| `positions` | `Positions` | (6,) |
| `statuses` | `Statuses` | (7,) |
| `warnings` | `Warnings` | (8,) |

### 2.5 解码逻辑分析

#### 2.5.1 协议概述

XY2-100 是振镜定位系统的串行通信协议，用于激光扫描系统。协议使用三线（CLK/SYNC/DATA）+ 可选 STAT 线：
- **CLK**：时钟信号
- **SYNC**：同步信号，低电平表示一帧结束
- **DATA**：数据线，在 CLK 上升沿时采样数据位
- **STAT**（可选）：状态线，在 CLK 下降沿时采样状态位

#### 2.5.2 帧格式

每帧 20 bits，SYNC=0 时表示帧结束：

```
Bit:  [0] [1] [2] [3] ... [18] [19]
      |TYPE----|<--- DATA ----->|PARITY|

三种帧类型：
1. 18-bit 位置帧：bit0=1, bits 1-18 为位置值, bit19 为奇偶校验(奇校验)
2. 16-bit 位置帧：bits 0-2 = 001, bits 3-18 为位置值, bit19 为偶校验
3. 命令帧：bits 0-2 = 111, bits 3-10 为命令, bits 11-18 为参数, bit19 为偶校验
```

**状态帧**（可选 STAT 通道）：19 bits，跳过第一个 bit，然后 19 个状态位。

#### 2.5.3 Python 解码流程

Python 解码器的 `decode()` 方法：

1. 等待 CLK 的**任意边沿**（`self.wait({0: 'e'})`）
2. 如果 CLK == 1（上升沿）：
   - 结束当前 data bit 区间（`bit_es = samplenum`）
   - 处理上一个 data bit（`process_bit(sync_value, bit_ss, bit_es, bit_value)`）
   - 开始新 bit 区间（`bit_ss = samplenum`）
3. 如果 CLK == 0（下降沿）：
   - 采样 DATA 值（`bit_value = data`）
   - 采样 SYNC 值（`sync_value = sync`）
   - 处理上一个 stat bit（如果有 STAT 通道）
   - 开始新 stat bit 区间

**关键**：DATA 在 CLK 下降沿被采样，但 bit 区间在 CLK 上升沿结束。SYNC 也在 CLK 下降沿采样。

#### 2.5.4 process_bit() 详解

当 `sync == 0` 时（帧结束标志），处理收集到的 20 bits：

1. 如果 bits < 20，输出警告 "Not enough data bits"，重置
2. 计算奇偶校验（XOR 所有 bit，不含 parity bit）
3. 判断帧类型：
   - `type_1_value = bits[0]`（仅 bit0）
   - `type_3_value = (bits[0] << 2) | (bits[1] << 1) | bits[2]`（bit0-2）
   - **18-bit 位置帧**：`type_1_value == 1` 且奇校验（parity_odd == 1）
   - **16-bit 位置帧**：`type_3_value == 1`（即 001）且偶校验
   - **命令帧**：`type_3_value == 7`（即 111）且偶校验
   - 其他：错误
4. 输出帧类型注解
5. 输出校验注解
6. 如果是位置帧，解析位置值（有符号数）：
   - 16-bit：bits[3:19]，补码，范围 [-32768, 32767]
   - 18-bit：bits[1:19]（实际代码用 bits[3:19]），补码，范围 [-131072, 131071]
7. 如果是命令帧，解析命令和参数

#### 2.5.5 process_stat_bit() 详解

1. 跳过第一个 stat bit（`stat_skip_bit = True`）
2. 收集 19 个 stat bits
3. 当 `sync == 0` 且收集满 19 bits 时，输出状态值（19-bit 无符号）

#### 2.5.6 边界情况

- 18-bit 位置帧和命令帧在奇偶校验错误时无法区分（代码中有警告）
- 16-bit 位置帧的奇偶校验错误会输出警告
- 未知帧类型输出错误并重置
- STAT 通道第一个 bit 被跳过
- `stat_skip_bit` 在 reset 时重置为 True，但**不在 process_bit 后重置**（Python 代码中 reset() 重置了 stat_skip_bit）

### 2.6 C 实现计划

#### 2.6.1 状态结构体

```c
#define XY2100_MAX_BITS 20
#define XY2100_MAX_STAT_BITS 19

enum xy2100_frame_type {
    FRAME_TYPE_NONE = 0,
    FRAME_TYPE_COMMAND = 1,
    FRAME_TYPE_16BIT_POS = 2,
    FRAME_TYPE_18BIT_POS = 3,
};

typedef struct {
    uint64_t samplerate;
    int out_ann;

    /* Data bits 采集 */
    int bits_count;
    uint64_t bits_ss[XY2100_MAX_BITS];
    uint64_t bits_es[XY2100_MAX_BITS];
    int bits_value[XY2100_MAX_BITS];

    /* Status bits 采集 */
    int stat_bits_count;
    uint64_t stat_bits_ss[XY2100_MAX_STAT_BITS];
    uint64_t stat_bits_es[XY2100_MAX_STAT_BITS];
    int stat_bits_value[XY2100_MAX_STAT_BITS];
    int stat_skip_bit;

    /* 当前 bit 追踪 */
    uint64_t bit_ss;
    int bit_value;
    uint64_t stat_ss;
    int stat_value;
    int sync_value;

    /* 通道存在标志 */
    int has_stat;
} xy2100_state;
```

#### 2.6.2 条件等待策略

```c
/* 主循环：等待 CLK 任意边沿 */
srd_cond_builder *cb = c_cond_new();
c_cond_edge(cb, 0);  /* CLK 任意边沿 */
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

然后读取 CLK 电平判断是上升沿还是下降沿：
```c
int clk = c_decoder_get_pin(di, 0, samplenum);
if (clk == 1) {
    /* 上升沿：结束 data bit，开始新 bit */
} else {
    /* 下降沿：采样 DATA 和 SYNC，处理 stat bit */
}
```

#### 2.6.3 Samplerate 守卫

```c
static void xy2100_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    xy2100_state *s = (xy2100_state *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->samplerate = value;
    }
}

/* decode() 入口处 */
if (!s->samplerate) {
    s->samplerate = c_decoder_get_samplerate(di);
}
/* XY2-100 不严格依赖 samplerate 进行解码，但可用于时间计算 */
```

#### 2.6.4 关键 C 代码片段

**process_bit 等价实现：**

```c
static void xy2100_process_bit(struct srd_decoder_inst *di, xy2100_state *s,
                                int sync, uint64_t bit_ss, uint64_t bit_es, int bit_value)
{
    char bstr[4];
    snprintf(bstr, sizeof(bstr), "%d", bit_value);
    C_ANN_PUT(di, bit_ss, bit_es, s->out_ann, ANN_BIT, bstr);

    if (s->bits_count < XY2100_MAX_BITS) {
        s->bits_ss[s->bits_count] = bit_ss;
        s->bits_es[s->bits_count] = bit_es;
        s->bits_value[s->bits_count] = bit_value;
    }
    s->bits_count++;

    if (sync == 0) {
        if (s->bits_count < 20) {
            C_ANN_PUT(di, s->bits_ss[0], bit_es, s->out_ann, ANN_WARNING,
                      "Not enough data bits");
            xy2100_reset_state(s);
            return;
        }

        /* 计算奇偶校验（不含 parity bit） */
        int parity = 0;
        for (int i = 0; i < 19; i++)
            parity ^= s->bits_value[i];

        int par_value = s->bits_value[19];
        int parity_even = (par_value == parity) ? 1 : 0;
        int parity_odd = (par_value != parity) ? 1 : 0;

        int type_1_value = s->bits_value[0];
        int type_3_value = (s->bits_value[0] << 2) | (s->bits_value[1] << 1) | s->bits_value[2];

        enum xy2100_frame_type frame_type = FRAME_TYPE_NONE;
        const char *parity_status = "X";
        uint64_t type_ss = s->bits_ss[0];
        uint64_t type_es = s->bits_es[2];

        /* 18-bit 位置帧 */
        if (type_1_value == 1 && parity_odd == 1) {
            frame_type = FRAME_TYPE_18BIT_POS;
            type_es = s->bits_es[0];
            C_ANN_PUT(di, s->bits_ss[0], bit_es, s->out_ann, ANN_WARNING,
                      "Careful: 18-bit position frames with wrong parity and command frames with wrong parity cannot be identified");
        }
        /* 16-bit 位置帧 */
        else if (type_3_value == 1) {
            frame_type = FRAME_TYPE_16BIT_POS;
            if (parity_even == 1)
                parity_status = "OK";
            else {
                parity_status = "NOK";
                C_ANN_PUT(di, s->bits_ss[0], bit_es, s->out_ann, ANN_WARNING,
                          "Parity error", "PE");
            }
        }
        /* 命令帧 */
        else if (type_3_value == 7 && parity_even == 1) {
            frame_type = FRAME_TYPE_COMMAND;
            C_ANN_PUT(di, s->bits_ss[0], bit_es, s->out_ann, ANN_WARNING,
                      "Careful: 18-bit position frames with wrong parity and command frames with wrong parity cannot be identified");
        }
        /* 未知 */
        else {
            C_ANN_PUT(di, s->bits_ss[0], bit_es, s->out_ann, ANN_WARNING,
                      "Error", "Unknown command or parity error");
            xy2100_reset_state(s);
            return;
        }

        /* 输出帧类型 */
        if (frame_type == FRAME_TYPE_16BIT_POS) {
            C_ANN_PUT(di, type_ss, type_es, s->out_ann, ANN_TYPE,
                      "16 bit Position Frame", "16 bit Pos", "Pos", "P");
        } else if (frame_type == FRAME_TYPE_18BIT_POS) {
            C_ANN_PUT(di, type_ss, type_es, s->out_ann, ANN_TYPE,
                      "18 bit Position Frame", "18 bit Pos", "Pos", "P");
        } else if (frame_type == FRAME_TYPE_COMMAND) {
            C_ANN_PUT(di, type_ss, type_es, s->out_ann, ANN_TYPE,
                      "Command Frame", "Command", "C");
        }

        /* 输出校验 */
        uint64_t par_ss = s->bits_ss[19];
        uint64_t par_es = s->bits_es[19];
        C_ANN_PUT(di, par_ss, par_es, s->out_ann, ANN_PARITY, parity_status);

        /* 输出位置值 */
        if (frame_type == FRAME_TYPE_16BIT_POS || frame_type == FRAME_TYPE_18BIT_POS) {
            int64_t pos = 0;
            if (frame_type == FRAME_TYPE_16BIT_POS) {
                int count = 15;
                for (int i = 3; i < 19; i++) {
                    pos |= (int64_t)s->bits_value[i] << count;
                    count--;
                }
                if (pos >= 32768) pos -= 65536;
            } else {
                int count = 17;
                for (int i = 3; i < 19; i++) {
                    pos |= (int64_t)s->bits_value[i] << count;
                    count--;
                }
                if (pos >= 131072) pos -= 262144;
            }
            char pos_str[32];
            snprintf(pos_str, sizeof(pos_str), "%lld", (long long)pos);
            C_ANN_PUT(di, type_es, par_ss, s->out_ann, ANN_POS, pos_str);
        }

        /* 输出命令和参数 */
        if (frame_type == FRAME_TYPE_COMMAND) {
            int cmd = 0;
            int count = 7;
            uint64_t cmd_es = 0;
            for (int i = 3; i < 11; i++) {
                cmd |= s->bits_value[i] << count;
                count--;
                cmd_es = s->bits_es[i];
            }
            char cmd_str[32];
            snprintf(cmd_str, sizeof(cmd_str), "Command 0x%X", cmd);
            char cmd_short[16];
            snprintf(cmd_short, sizeof(cmd_short), "0x%X", cmd);
            C_ANN_PUT(di, type_es, cmd_es, s->out_ann, ANN_COMMAND, cmd_str, cmd_short);

            int param = 0;
            count = 7;
            for (int i = 11; i < 19; i++) {
                param |= s->bits_value[i] << count;
                count--;
            }
            char param_str[64];
            snprintf(param_str, sizeof(param_str), "Parameter 0x%X / %d", param, param);
            char param_short[32];
            snprintf(param_short, sizeof(param_short), "0x%X / %d", param, param);
            char param_tiny[16];
            snprintf(param_tiny, sizeof(param_tiny), "0x%X", param);
            C_ANN_PUT(di, cmd_es, par_ss, s->out_ann, ANN_PARAMETER, param_str, param_short, param_tiny);
        }

        xy2100_reset_state(s);
    }
}
```

**process_stat_bit 等价实现：**

```c
static void xy2100_process_stat_bit(struct srd_decoder_inst *di, xy2100_state *s,
                                     int sync, uint64_t bit_ss, uint64_t bit_es, int bit_value)
{
    if (s->stat_skip_bit) {
        s->stat_skip_bit = 0;
        return;
    }

    char bstr[4];
    snprintf(bstr, sizeof(bstr), "%d", bit_value);
    C_ANN_PUT(di, bit_ss, bit_es, s->out_ann, ANN_STAT_BIT, bstr);

    if (s->stat_bits_count < XY2100_MAX_STAT_BITS) {
        s->stat_bits_ss[s->stat_bits_count] = bit_ss;
        s->stat_bits_es[s->stat_bits_count] = bit_es;
        s->stat_bits_value[s->stat_bits_count] = bit_value;
    }
    s->stat_bits_count++;

    if (sync == 0 && s->stat_bits_count == 19) {
        uint64_t stat_ss = s->stat_bits_ss[0];
        uint64_t stat_es = s->stat_bits_es[18];

        int status = 0;
        int count = 18;
        for (int i = 0; i < 19; i++) {
            status |= s->stat_bits_value[i] << count;
            count--;
        }
        char stat_str[32];
        snprintf(stat_str, sizeof(stat_str), "Status 0x%X", status);
        char stat_short[16];
        snprintf(stat_short, sizeof(stat_short), "0x%X", status);
        C_ANN_PUT(di, stat_ss, stat_es, s->out_ann, ANN_STATUS, stat_str, stat_short);
    }
}
```

**主 decode 循环：**

```c
static void xy2100_decode(struct srd_decoder_inst *di)
{
    xy2100_state *s = (xy2100_state *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    if (!s->samplerate) {
        s->samplerate = c_decoder_get_samplerate(di);
    }

    uint64_t bit_ss = (uint64_t)-1;
    int bit_value = 0;
    uint64_t stat_ss = (uint64_t)-1;
    int stat_value = 0;
    int sync_value = 0;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        c_cond_edge(cb, 0);  /* CLK 任意边沿 */
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK)
            return;

        int clk = c_decoder_get_pin(di, 0, samplenum);
        int sync = c_decoder_get_pin(di, 1, samplenum);
        int data = c_decoder_get_pin(di, 2, samplenum);
        int stat = s->has_stat ? c_decoder_get_pin(di, 3, samplenum) : 0;

        if (clk == 1) {
            /* 上升沿：结束 data bit，开始新 bit */
            stat_value = stat;
            uint64_t bit_es = samplenum;
            if (bit_ss != (uint64_t)-1) {
                xy2100_process_bit(di, s, sync_value, bit_ss, bit_es, bit_value);
            }
            bit_ss = samplenum;
        } else {
            /* 下降沿：采样 DATA 和 SYNC */
            bit_value = data;
            sync_value = sync;

            uint64_t stat_es = samplenum;
            if (stat_ss != (uint64_t)-1 && s->has_stat) {
                xy2100_process_stat_bit(di, s, sync_value, stat_ss, stat_es, stat_value);
            }
            stat_ss = samplenum;
        }
    }
}
```

---

## 3. 通用 C 解码器模板注意事项

### 3.1 ann_labels 格式

```c
static const char *tlc5620_ann_labels[][3] = {
    {"", "DAC select", "DAC select"},       /* ANN_DAC_SELECT */
    {"", "Gain", "Gain"},                   /* ANN_GAIN */
    {"", "DAC value", "DAC value"},         /* ANN_VALUE */
    {"", "Data latch", "Data latch point"}, /* ANN_DATA_LATCH */
    {"", "LDAC fall", "LDAC falling edge"}, /* ANN_LDAC_FALL */
    {"", "Bit", "Bit"},                     /* ANN_BIT */
    {"", "Register write", "Register write"}, /* ANN_REG_WRITE */
    {"", "Voltage update", "Voltage update"}, /* ANN_VOLTAGE_UPDATE */
    {"", "Voltage update all", "Voltage update (all DACs)"}, /* ANN_VOLTAGE_UPDATE_ALL */
    {"", "Invalid command", "Invalid command"}, /* ANN_INVALID_CMD */
};
```

第一列必须为 `""`（C 解码器约定）。

### 3.2 annotation_rows 格式

```c
static const int tlc5620_row_bits_classes[] = {ANN_BIT, -1};
static const int tlc5620_row_fields_classes[] = {ANN_DAC_SELECT, ANN_GAIN, ANN_VALUE, -1};
static const int tlc5620_row_registers_classes[] = {ANN_REG_WRITE, ANN_VOLTAGE_UPDATE, -1};
static const int tlc5620_row_voltage_updates_classes[] = {ANN_VOLTAGE_UPDATE_ALL, -1};
static const int tlc5620_row_events_classes[] = {ANN_DATA_LATCH, ANN_LDAC_FALL, -1};
static const int tlc5620_row_errors_classes[] = {ANN_INVALID_CMD, -1};

static const struct srd_c_ann_row tlc5620_ann_rows[] = {
    {"bits", "Bits", tlc5620_row_bits_classes, 1},
    {"fields", "Fields", tlc5620_row_fields_classes, 3},
    {"registers", "Registers", tlc5620_row_registers_classes, 2},
    {"voltage-updates", "Voltage updates", tlc5620_row_voltage_updates_classes, 1},
    {"events", "Events", tlc5620_row_events_classes, 2},
    {"errors", "Errors", tlc5620_row_errors_classes, 1},
};
```

### 3.3 srd_c_decoder_entry() 中选项初始化

```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    tlc5620_options[0].id = "vref_a";
    tlc5620_options[0].idn = "dec_tlc5620_opt_vref_a";
    tlc5620_options[0].desc = "Reference voltage DACA (V)";
    tlc5620_options[0].def = g_variant_new_double(3.3);
    tlc5620_options[0].values = NULL;

    tlc5620_options[1].id = "vref_b";
    tlc5620_options[1].idn = "dec_tlc5620_opt_vref_b";
    tlc5620_options[1].desc = "Reference voltage DACB (V)";
    tlc5620_options[1].def = g_variant_new_double(3.3);
    tlc5620_options[1].values = NULL;

    /* ... vref_c, vref_d 类似 ... */

    return &tlc5620_c_decoder;
}
```

### 3.4 CMakeLists.txt 修改

在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加：
```
tlc5620_c
xy2_100_c
```
<!-- Updated: xy2_100_c 使用下划线替代连字符，与现有C解码器CMake命名规范一致 -->

### 3.5 文件路径

- `libsigrokdecode/c_decoders/tlc5620_c.c`
- `libsigrokdecode/c_decoders/xy2_100_c.c` <!-- Updated: 文件名使用下划线替代连字符 -->
