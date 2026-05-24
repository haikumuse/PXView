# Python解码器移植为C解码器 — 第5批详细规范

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |

## 概述

本文档描述将5个Python协议解码器移植为C解码器的完整规范。每个解码器包含：
1. Python解码器完整分析（所有元数据字段）
2. C实现计划（结构体字段、枚举值、函数签名）
3. 关键实现说明（状态机细节、边界情况、格式字符串）
4. 与Python的差异及特殊处理

### C解码器API参考

- 文件位置：`libsigrokdecode/c_decoders/<name>_c.c`
- 注册到CMakeLists.txt的`C_DECODERS`列表
- 使用`#include "libsigrokdecode.h"`和`<glib.h>`
- 状态通过`c_decoder_get_private/set_private`管理
- 等待条件使用`srd_cond_builder`系列函数
- 注释输出使用`C_ANN_PUT`/`C_ANN_PUT_TYPE`/`C_ANN_PUT_VAL`宏
- Python输出使用`c_decoder_put_python`
- Logic输出使用`c_decoder_put_logic`（`SRD_OUTPUT_LOGIC`） <!-- Updated: c_decoder_put_logic已实现 -->
- 采样率通过`c_decoder_get_samplerate`获取
- 选项通过`c_decoder_get_option_string/int/double`获取
- 无条件等待使用`c_cond_wait_current(di, &samplenum)`（等效Python `self.wait({})`） <!-- Updated: c_cond_wait_current已实现 -->
- 初始引脚值使用`c_decoder_get_initial_pin(di, ch)`（等效Python `self.oldpin`） <!-- Updated: c_decoder_get_initial_pin已实现 -->

---

## 1. ADB (Apple Desktop Bus)

### 1.1 Python解码器元数据

| 字段 | 值 |
|------|-----|
| id | `'adb'` |
| name | `'ADB'` |
| longname | `'Apple Desktop Bus'` |
| desc | `'Decode command and data of Apple Desktop Bus protocol.'` |
| license | `'mit'` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['PC']` |

**通道：**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | `'data'` | `'Data'` | `'Data line'` | (无) |

**选项：**

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| `'format'` | `'Data format'` | `'hex'` | `('hex', 'dec', 'oct', 'bin')` | (无) |

**注释（annotations）：**

| 索引 | id | desc |
|------|-----|------|
| 0 | `'lo'` | `'Low'` |
| 1 | `'hi'` | `'High'` |
| 2 | `'attn'` | `'Attention'` |
| 3 | `'greset'` | `'Global Reset'` |
| 4 | `'bit'` | `'Bit'` |
| 5 | `'data'` | `'Data'` |
| 6 | `'start'` | `'Start'` |
| 7 | `'stop'` | `'Stop'` |
| 8 | `'srq'` | `'Service Request'` |
| 9 | `'reset'` | `'Reset'` |
| 10 | `'flush'` | `'Flush'` |
| 11 | `'listen'` | `'Listen'` |
| 12 | `'talk'` | `'Talk'` |
| 13 | `'unknown'` | `'Unknown'` |

**注释行（annotation_rows）：**

| id | label | class_tuple |
|----|-------|-------------|
| `'cells'` | `'Cells'` | (0,1,2,3,8) |
| `'bits'` | `'Bits'` | (4,6,7) |
| `'bytes'` | `'Bytes'` | (5,9,10,11,12,13) |

**binary：** 无

**是否需要samplerate：** 是（`to_us`函数依赖采样率将采样数转换为微秒）

**是否输出到其他解码器：** 否（outputs为空）

### 1.2 Python decode()逻辑完整分析

ADB协议是Apple Desktop Bus的单线异步协议。

**位单元时序：**
```
          ___
bit1: |__|   |
           __
bit0: |___|  |
      \   \  `--- cell_e
       \   +----- low_e
        +-------- cell_s
```

**状态机流程：**

1. **初始等待**：`wait({0: 'f'})` — 等待第一个下降沿，记录`cell_s`
2. **主循环**：
   - **低电平阶段**：`wait({0: 'r'})` — 等待上升沿，记录`low_e`
     - 计算`low = to_us(low_e - cell_s)`（低电平持续微秒数）
     - `low < 100`：正常cell低电平 → `putl(cell_s, low_e)`，若`bit_count % 8 == 0`记录`byte_s = cell_s`
     - `low > 1500`：全局复位 → `putr(cell_s, low_e)`
     - `low > 500`：注意力信号(560-1040us) → `puta(cell_s, low_e)`，设`attention = 1`
     - 否则(100-500us)：SRQ(140-260us) → `putQ(cell_s, low_e)`
   - **高电平阶段**：`wait({0: 'f'})` — 等待下一个下降沿，记录`cell_e`
     - 计算`high = to_us(cell_e - low_e)`，`cell = to_us(cell_e - cell_s)`
     - `high < 100`：正常cell高电平 → `puth(low_e, cell_e)`
       - `cell <= 130`：位单元
         - `bit_count == 0`：起始位(1) → `putS(cell_s, cell_e)`
         - 否则：比较low和high宽度判断bit值
           - `low > high`：bit0 → `putb(cell_s, cell_e, 0)`，`byte = ((byte << 1) & 0xff) | 0`
           - 否则：bit1 → `putb(cell_s, cell_e, 1)`，`byte = ((byte << 1) & 0xff) | 1`
         - `bit_count % 8 == 0`：完整字节
           - `attention == 1`：命令字节 → `putC(byte_s, cell_e, byte)`，`attention = 0`，`bit_count = -1`
           - 否则：数据字节 → `putD(byte_s, cell_e, byte)`
       - `cell > 130`：
         - `low < 100`：停止位(0) → `putT(cell_s, cell_e)`
         - 否则：attention后的起始位(1) → `putS(low_e, cell_e)`，`bit_count = 0`
     - `high >= 100`：
       - `low < 100`：停止位(0) → `putT(cell_s, low_e)`
3. 更新`cell_s = cell_e`

**命令字节解析（putC）：**
- `addr = (C >> 4) & 0x0f`
- `cmd = C & 0x0f`
- `reg = C & 0x03`
- `cmd == 0`：Reset(ANN 9)
- `cmd == 1`：Flush(ANN 10)
- `(cmd & 0x0c) == 0x08`：Listen(ANN 11)，格式`Listen($%X,r%d) %02X`
- `(cmd & 0x0c) == 0x0c`：Talk(ANN 12)，格式`Talk($%X,r%d) %02X`
- 其他：Unknown(ANN 13)

**数据字节格式（putD）：**
- hex：`%02X`
- dec：`%d`
- oct：`%03o`
- bin：`%08b`

**关键时间阈值（微秒）：**
- `< 100`：正常cell
- `100-500`：SRQ
- `500-1500`：Attention
- `> 1500`：Global Reset
- cell总长 `<= 130`：位单元
- cell总长 `> 130`：停止位或attention后起始位

### 1.3 C实现计划

**枚举定义：**
```c
enum {
    ANN_LO = 0,        // Low
    ANN_HI,            // High
    ANN_ATTN,          // Attention
    ANN_GRESET,        // Global Reset
    ANN_BIT,           // Bit
    ANN_DATA,          // Data
    ANN_START,         // Start
    ANN_STOP,          // Stop
    ANN_SRQ,           // Service Request
    ANN_RESET,         // Reset (command)
    ANN_FLUSH,         // Flush (command)
    ANN_LISTEN,        // Listen (command)
    ANN_TALK,          // Talk (command)
    ANN_UNKNOWN,       // Unknown (command)
    NUM_ANN,
};
```

**状态结构体：**
```c
typedef struct {
    uint64_t samplerate;
    int out_ann;
    int format;        // 0=hex, 1=dec, 2=oct, 3=bin
    uint64_t cell_s;   // 当前cell起始采样号
    int byte_val;      // 当前正在组装的字节值
    int bit_count;     // 已接收的位数
    int attention;     // 是否处于attention状态
    uint64_t byte_s;   // 当前字节起始采样号
} adb_state;
```

**通道定义：**
```c
static struct srd_channel adb_channels[] = {
    { "data", "Data", "Data line", 0, SRD_CHANNEL_SDATA, NULL },
};
```

**选项定义：**
```c
static struct srd_decoder_option adb_options[] = {
    { "format", NULL, "Data format", NULL, NULL },
};
```

**注释标签：**
```c
static const char* adb_ann_labels[][3] = {
    { "", "lo", "Low" },
    { "", "hi", "High" },
    { "", "attn", "Attention" },
    { "", "greset", "Global Reset" },
    { "", "bit", "Bit" },
    { "", "data", "Data" },
    { "", "start", "Start" },
    { "", "stop", "Stop" },
    { "", "srq", "Service Request" },
    { "", "reset", "Reset" },
    { "", "flush", "Flush" },
    { "", "listen", "Listen" },
    { "", "talk", "Talk" },
    { "", "unknown", "Unknown" },
};
```

**注释行：**
```c
static const int adb_row_cells_classes[] = {ANN_LO, ANN_HI, ANN_ATTN, ANN_GRESET, ANN_SRQ};
static const int adb_row_bits_classes[] = {ANN_BIT, ANN_START, ANN_STOP};
static const int adb_row_bytes_classes[] = {ANN_DATA, ANN_RESET, ANN_FLUSH, ANN_LISTEN, ANN_TALK, ANN_UNKNOWN};

static const struct srd_c_ann_row adb_ann_rows[] = {
    { "cells", "Cells", adb_row_cells_classes, 5 },
    { "bits", "Bits", adb_row_bits_classes, 3 },
    { "bytes", "Bytes", adb_row_bytes_classes, 6 },
};
```

**函数签名：**
```c
static void adb_reset(struct srd_decoder_inst *di);
static void adb_start(struct srd_decoder_inst *di);
static void adb_decode(struct srd_decoder_inst *di);
static void adb_destroy(struct srd_decoder_inst *di);
```

**decode函数伪代码：**
```c
static void adb_decode(struct srd_decoder_inst *di) {
    adb_state *s = (adb_state *)c_decoder_get_private(di);
    uint64_t samplenum, matched;

    // 等待第一个下降沿
    srd_cond_builder *cb = c_cond_new();
    c_cond_fall(cb, 0);
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;
    s->cell_s = samplenum;

    while (1) {
        // 等待上升沿（低电平结束）
        cb = c_cond_new();
        c_cond_rise(cb, 0);
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;
        uint64_t low_e = samplenum;
        double low_us = (double)(low_e - s->cell_s) * 1000000.0 / (double)s->samplerate;

        if (low_us < 100.0) {
            // 正常cell低电平
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%d", (int)((low_e - s->cell_s) * 1000000 / s->samplerate));
            C_ANN_PUT(di, s->cell_s, low_e, s->out_ann, ANN_LO, tmp);
            if (s->bit_count % 8 == 0) s->byte_s = s->cell_s;
        } else if (low_us > 1500.0) {
            // 全局复位
            C_ANN_PUT(di, s->cell_s, low_e, s->out_ann, ANN_GRESET,
                "Reset", "Rst", "R");
        } else if (low_us > 500.0) {
            // Attention
            char tmp[64];
            int dur = (int)((low_e - s->cell_s) * 1000000 / s->samplerate);
            snprintf(tmp, sizeof(tmp), "Attn:%d", dur);
            C_ANN_PUT(di, s->cell_s, low_e, s->out_ann, ANN_ATTN, tmp, "Attn", "A");
            s->attention = 1;
        } else {
            // SRQ (100-500us)
            char tmp[64];
            int dur = (int)((low_e - s->cell_s) * 1000000 / s->samplerate);
            snprintf(tmp, sizeof(tmp), "SRQ:%d", dur);
            C_ANN_PUT(di, s->cell_s, low_e, s->out_ann, ANN_SRQ, tmp, "SRQ", "Q");
        }

        // 等待下降沿（高电平结束/下一个cell开始）
        cb = c_cond_new();
        c_cond_fall(cb, 0);
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;
        uint64_t cell_e = samplenum;
        double high_us = (double)(cell_e - low_e) * 1000000.0 / (double)s->samplerate;
        double cell_us = (double)(cell_e - s->cell_s) * 1000000.0 / (double)s->samplerate;

        if (high_us < 100.0) {
            // 正常cell高电平
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%d", (int)((cell_e - low_e) * 1000000 / s->samplerate));
            C_ANN_PUT(di, low_e, cell_e, s->out_ann, ANN_HI, tmp);

            if (cell_us <= 130.0) {
                // 位单元
                s->bit_count++;
                if (s->bit_count == 0) {
                    // 起始位(1)
                    C_ANN_PUT(di, s->cell_s, cell_e, s->out_ann, ANN_START, "Start(1)", "S1", "S");
                } else {
                    if (low_us > high_us) {
                        // bit 0
                        C_ANN_PUT(di, s->cell_s, cell_e, s->out_ann, ANN_BIT, "0");
                        s->byte_val = ((s->byte_val << 1) & 0xff) | 0;
                    } else {
                        // bit 1
                        C_ANN_PUT(di, s->cell_s, cell_e, s->out_ann, ANN_BIT, "1");
                        s->byte_val = ((s->byte_val << 1) & 0xff) | 1;
                    }
                }

                if (s->bit_count && s->bit_count % 8 == 0) {
                    if (s->attention == 1) {
                        // 命令字节 - 调用putC逻辑
                        adb_put_command(di, s, s->byte_s, cell_e, s->byte_val);
                        s->attention = 0;
                        s->bit_count = -1;
                    } else {
                        // 数据字节 - 调用putD逻辑
                        adb_put_data(di, s, s->byte_s, cell_e, s->byte_val);
                    }
                }
            } else {
                // cell > 130us
                if (low_us < 100.0) {
                    // 停止位(0)
                    C_ANN_PUT(di, s->cell_s, cell_e, s->out_ann, ANN_STOP, "Stop(0)", "T0", "T");
                } else {
                    // attention后的起始位(1)
                    C_ANN_PUT(di, low_e, cell_e, s->out_ann, ANN_START, "Start(1)", "S1", "S");
                    s->bit_count = 0;
                }
            }
        } else {
            // high >= 100us
            if (low_us < 100.0) {
                // 停止位(0)
                C_ANN_PUT(di, s->cell_s, low_e, s->out_ann, ANN_STOP, "Stop(0)", "T0", "T");
            }
        }

        s->cell_s = cell_e;
    }
}
```

**辅助函数：**

```c
static void adb_put_command(struct srd_decoder_inst *di, adb_state *s,
    uint64_t ss, uint64_t es, int C) {
    int addr = (C >> 4) & 0x0f;
    int cmd = C & 0x0f;
    int reg = C & 0x03;
    char tmp[128];

    if (cmd == 0) {
        snprintf(tmp, sizeof(tmp), "Reset:%02X", C);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_RESET, tmp, "RST", "R");
    } else if (cmd == 1) {
        snprintf(tmp, sizeof(tmp), "Flush:%02X", C);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_FLUSH, tmp, "FLS", "F");
    } else if ((cmd & 0x0c) == 0x08) {
        snprintf(tmp, sizeof(tmp), "Listen($%X,r%d) %02X", addr, reg, C);
        char short_tmp[32];
        snprintf(short_tmp, sizeof(short_tmp), "L:%X:%d", addr, reg);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_LISTEN, tmp, short_tmp, "L");
    } else if ((cmd & 0x0c) == 0x0c) {
        snprintf(tmp, sizeof(tmp), "Talk($%X,r%d) %02X", addr, reg, C);
        char short_tmp[32];
        snprintf(short_tmp, sizeof(short_tmp), "T:%X:%d", addr, reg);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_TALK, tmp, short_tmp, "T");
    } else {
        snprintf(tmp, sizeof(tmp), "Unknown:%02X", C);
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_UNKNOWN, tmp, "Unk", "U");
    }
}

static void adb_put_data(struct srd_decoder_inst *di, adb_state *s,
    uint64_t ss, uint64_t es, int D) {
    char tmp[32];
    switch (s->format) {
        case 0: snprintf(tmp, sizeof(tmp), "%02X", D); break;
        case 1: snprintf(tmp, sizeof(tmp), "%d", D); break;
        case 2: snprintf(tmp, sizeof(tmp), "%03o", D); break;
        case 3: snprintf(tmp, sizeof(tmp), "%08b", D); break; // 需要自定义二进制格式化
    }
    C_ANN_PUT(di, ss, es, s->out_ann, ANN_DATA, tmp);
}
```

### 1.4 关键实现说明

1. **时间转换**：Python使用`to_us(sample) = sample / (samplerate / 1000000)`，C中应使用`(double)sample * 1000000.0 / (double)samplerate`
2. **二进制格式化**：C标准库没有`%b`格式说明符（C23才有），需要自定义`bin_format`函数
3. **bit_count初始值**：Python中`bit_count`从0开始但`bit_count += 1`后第一个位是1（因为初始wait已经跳过了起始位），需要注意`bit_count == 0`的判断在增量之前
4. **byte移位**：`byte = ((byte << 1) & 0xff) | bit`，保持8位
5. **attention标志**：收到attention信号后设1，命令字节处理后清0并设`bit_count = -1`
6. **putl/puth输出**：输出的是低/高电平持续时间的微秒整数值
7. **无metadata回调**：samplerate在start()中通过`c_decoder_get_samplerate`获取，也可在decode中延迟获取

---

## 2. AFSK (Audio Frequency Shift Keying)

### 2.1 Python解码器元数据

| 字段 | 值 |
|------|-----|
| id | `'afsk'` |
| name | `'AFSK'` |
| longname | `'Audio Frequency Shift Keying'` |
| desc | `'Audio Frequency Shift Keying'` |
| license | `'unknown'` |
| inputs | `['logic']` |
| outputs | `['afsk_bits']` |
| tags | `['Embedded/industrial']` |

**通道：**

| 序号 | id | name | desc |
|------|-----|------|------|
| 0 | `'afsk'` | `'afsk'` | `'AFSK stream'` |

**选项：**

| id | desc | default | values |
|----|------|---------|--------|
| `'markfreq'` | `'Mark(1) Frequency'` | 2000 | (整数) |
| `'spacefreq'` | `'Space(0) Frequency'` | 4000 | (整数) |
| `'marginpct'` | `'Error margin %'` | 40 | (整数) |

**注释（annotations）：**

| 索引 | id | desc |
|------|-----|------|
| 0 | `'bit-raw'` | `'Raw Bit'` |
| 1 | `'bit-error'` | `'Unknown half-cycle'` |
| 2 | `'bit-phase'` | `'Phase error'` |

**注释行（annotation_rows）：**

| id | label | class_tuple |
|----|-------|-------------|
| `'raw-bits'` | `'Raw Bits'` | (0,) |
| `'errors'` | `'Errors'` | (1, 2) |

**binary：** 无

**是否需要samplerate：** 是（计算半周期采样数）

**是否输出到其他解码器：** 是（outputs = `['afsk_bits']`，使用`OUTPUT_PYTHON`）

### 2.2 Python decode()逻辑完整分析

AFSK解码器通过检测音频信号中两个不同频率的半周期来解码位。

**初始化计算（在start()中）：**
- `markhalfcycle = int(samplerate * ((1 / markfreq) / 2)) - 1` — Mark频率的半周期采样数
- `markmargin = int(markhalfcycle * (marginpct * 0.01))` — Mark频率的容差
- `spacehalfcycle = int(samplerate * ((1 / spacefreq) / 2)) - 1` — Space频率的半周期采样数
- `spacemargin = int(spacehalfcycle * (marginpct * 0.01))` — Space频率的容差

**状态机流程：**

1. **初始状态**：`state = 'IDLE'`
2. **主循环**：
   - 保存历史边沿采样号：`twoedgesagosample = oneedgeagosample`，`oneedgeagosample = currentedgesample`
   - 保存上一周期类型：`lastcycletype = cycletype`
   - 等待任意边沿：`wait({0: 'e'})`
   - 保存当前边沿采样号：`currentedgesample = samplenum`
   - 计算半周期长度：`length = currentedgesample - oneedgeagosample`
   - 判断半周期类型：
     - `length`在`[spacehalfcycle - spacemargin, spacehalfcycle + spacemargin]`范围内 → `'SPACE'`
     - `length`在`[markhalfcycle - markmargin, markhalfcycle + markmargin]`范围内 → `'MARK'`
     - 其他 → `'ERROR'`
   - 状态转换：
     - `SPACE + SPACE`（两个连续SPACE半周期）→ bit 0 → `putbitraw()`
     - `MARK + MARK`（两个连续MARK半周期）→ bit 1 → `putbitraw()`
     - `ERROR` → `puterror()`
     - `SPACE + MARK` 或 `MARK + SPACE`（相位错误）→ `putphaseerror()`
     - `PROCESSED + *` → 无操作（已处理的半周期跳过）

**putbitraw()：**
- 输出`OUTPUT_PYTHON`：`['BIT', self.lastbit]`
- 输出`OUTPUT_ANN`：`[0, ['%d' % self.lastbit]]`
- 范围：`twoedgesagosample`到`currentedgesample`

**puterror()：**
- 输出`OUTPUT_PYTHON`：`['ERROR', 'INVALID']`
- 输出`OUTPUT_ANN`：`[1, ['Error: Invalid cycle', 'Error', 'Err', 'E']]`
- 范围：`oneedgeagosample`到`currentedgesample`

**putphaseerror()：**
- 输出`OUTPUT_PYTHON`：`['ERROR', 'PHASE']`
- 输出`OUTPUT_ANN`：`[2, ['Phase error: Resyncing', 'Phase error', 'Phase', 'P']]`
- 范围：`oneedgeagosample`到`currentedgesample`

### 2.3 C实现计划

**枚举定义：**
```c
enum {
    ANN_BIT_RAW = 0,    // Raw Bit
    ANN_BIT_ERROR,      // Unknown half-cycle
    ANN_BIT_PHASE,      // Phase error
    NUM_ANN,
};

enum cycle_type {
    CYCLE_IDLE = 0,
    CYCLE_SPACE,
    CYCLE_MARK,
    CYCLE_ERROR,
    CYCLE_PROCESSED,
};
```

**状态结构体：**
```c
typedef struct {
    uint64_t samplerate;
    int out_ann;
    int out_python;
    int markfreq;
    int spacefreq;
    int marginpct;
    int64_t markhalfcycle;
    int64_t markmargin;
    int64_t spacehalfcycle;
    int64_t spacemargin;
    int lastbit;
    int cycletype;
    int lastcycletype;
    uint64_t twoedgesagosample;
    uint64_t oneedgeagosample;
    uint64_t currentedgesample;
} afsk_state;
```

**通道定义：**
```c
static struct srd_channel afsk_channels[] = {
    { "afsk", "afsk", "AFSK stream", 0, SRD_CHANNEL_SDATA, NULL },
};
```

**选项定义：**
```c
static struct srd_decoder_option afsk_options[] = {
    { "markfreq", NULL, "Mark(1) Frequency", NULL, NULL },
    { "spacefreq", NULL, "Space(0) Frequency", NULL, NULL },
    { "marginpct", NULL, "Error margin %", NULL, NULL },
};
```

**注释标签：**
```c
static const char* afsk_ann_labels[][3] = {
    { "", "bit-raw", "Raw Bit" },
    { "", "bit-error", "Unknown half-cycle" },
    { "", "bit-phase", "Phase error" },
};
```

**注释行：**
```c
static const int afsk_row_raw_classes[] = {ANN_BIT_RAW};
static const int afsk_row_errors_classes[] = {ANN_BIT_ERROR, ANN_BIT_PHASE};

static const struct srd_c_ann_row afsk_ann_rows[] = {
    { "raw-bits", "Raw Bits", afsk_row_raw_classes, 1 },
    { "errors", "Errors", afsk_row_errors_classes, 2 },
};
```

**输出定义：**
```c
static const char* afsk_outputs[] = { "afsk_bits", NULL };
```

**decode函数核心逻辑：**
```c
static void afsk_decode(struct srd_decoder_inst *di) {
    afsk_state *s = (afsk_state *)c_decoder_get_private(di);
    uint64_t samplenum, matched;

    while (1) {
        s->twoedgesagosample = s->oneedgeagosample;
        s->oneedgeagosample = s->currentedgesample;
        s->lastcycletype = s->cycletype;

        // 等待任意边沿
        srd_cond_builder *cb = c_cond_new();
        c_cond_edge(cb, 0);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;

        s->currentedgesample = samplenum;
        int64_t length = (int64_t)(s->currentedgesample - s->oneedgeagosample);

        // 判断半周期类型
        if (length >= (s->spacehalfcycle - s->spacemargin) &&
            length <= (s->spacehalfcycle + s->spacemargin)) {
            s->cycletype = CYCLE_SPACE;
        } else if (length >= (s->markhalfcycle - s->markmargin) &&
                   length <= (s->markhalfcycle + s->markmargin)) {
            s->cycletype = CYCLE_MARK;
        } else {
            s->cycletype = CYCLE_ERROR;
        }

        // 状态转换
        if (s->cycletype == CYCLE_SPACE && s->lastcycletype == CYCLE_SPACE) {
            s->lastbit = 0;
            // putbitraw
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "%d", s->lastbit);
            C_ANN_PUT(di, s->twoedgesagosample, s->currentedgesample, s->out_ann, ANN_BIT_RAW, tmp);
            // put python
            unsigned char bit_data = (unsigned char)s->lastbit;
            c_decoder_put_python(di, s->twoedgesagosample, s->currentedgesample,
                s->out_python, "BIT", &bit_data, 1);
            s->cycletype = CYCLE_PROCESSED;
        } else if (s->cycletype == CYCLE_MARK && s->lastcycletype == CYCLE_MARK) {
            s->lastbit = 1;
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "%d", s->lastbit);
            C_ANN_PUT(di, s->twoedgesagosample, s->currentedgesample, s->out_ann, ANN_BIT_RAW, tmp);
            unsigned char bit_data = (unsigned char)s->lastbit;
            c_decoder_put_python(di, s->twoedgesagosample, s->currentedgesample,
                s->out_python, "BIT", &bit_data, 1);
            s->cycletype = CYCLE_PROCESSED;
        } else if (s->cycletype == CYCLE_ERROR) {
            s->lastbit = 2;
            C_ANN_PUT(di, s->oneedgeagosample, s->currentedgesample, s->out_ann, ANN_BIT_ERROR,
                "Error: Invalid cycle", "Error", "Err", "E");
            c_decoder_put_python(di, s->oneedgeagosample, s->currentedgesample,
                s->out_python, "ERROR", (const unsigned char*)"INVALID", 7);
        } else if ((s->cycletype == CYCLE_SPACE && s->lastcycletype == CYCLE_MARK) ||
                   (s->cycletype == CYCLE_MARK && s->lastcycletype == CYCLE_SPACE)) {
            s->lastbit = 2;
            C_ANN_PUT(di, s->oneedgeagosample, s->currentedgesample, s->out_ann, ANN_BIT_PHASE,
                "Phase error: Resyncing", "Phase error", "Phase", "P");
            c_decoder_put_python(di, s->oneedgeagosample, s->currentedgesample,
                s->out_python, "ERROR", (const unsigned char*)"PHASE", 5);
        }
    }
}
```

### 2.4 关键实现说明

1. **半周期计算**：`markhalfcycle = (int64_t)(samplerate * (1.0 / markfreq) / 2.0) - 1`，注意Python的`int()`截断行为
2. **margin计算**：`markmargin = (int64_t)(markhalfcycle * marginpct / 100.0)`
3. **边沿历史**：需要维护3个边沿采样号（twoedgesagosample, oneedgeagosample, currentedgesample）
4. **PROCESSED状态**：当两个连续半周期匹配产生一个bit后，设cycletype为PROCESSED，下一个半周期不会与PROCESSED配对
5. **Python输出**：需要注册`SRD_OUTPUT_PYTHON`输出，使用`c_decoder_put_python`发送数据
6. **初始状态**：所有边沿采样号初始化为0，cycletype初始化为IDLE(0)
7. **debug输出**：Python代码中有print语句用于调试，C实现中应移除
8. **range检查**：Python的`range(a, b+1)`等价于C的`>= a && <= b`

---

## 3. AM230x (Aosong AM230x/DHTxx/RHTxx)

### 3.1 Python解码器元数据

| 字段 | 值 |
|------|-----|
| id | `'am230x'` |
| name | `'AM230x'` |
| longname | `'Aosong AM230x/DHTxx/RHTxx'` |
| desc | `'Aosong AM230x/DHTxx/RHTxx humidity/temperature sensor.'` |
| license | `'gplv2+'` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['IC', 'Sensor']` |

**通道：**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | `'sda'` | `'SDA'` | `'Single wire serial data line'` | `'dec_am230x_chan_sda'` |

**选项：**

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| `'device'` | `'Device type'` | `'am230x'` | `('am230x/rht', 'dht11')` | `'dec_am230x_opt_device'` |

**注释（annotations）：**

| 索引 | id | desc |
|------|-----|------|
| 0 | `'start'` | `'Start'` |
| 1 | `'response'` | `'Response'` |
| 2 | `'bit'` | `'Bit'` |
| 3 | `'end'` | `'End'` |
| 4 | `'byte'` | `'Byte'` |
| 5 | `'humidity'` | `'Relative humidity in percent'` |
| 6 | `'temperature'` | `'Temperature in degrees Celsius'` |
| 7 | `'checksum'` | `'Checksum'` |

**注释行（annotation_rows）：**

| id | label | class_tuple |
|----|-------|-------------|
| `'bits'` | `'Bits'` | (0, 1, 2, 3) |
| `'bytes'` | `'Bytes'` | (4,) |
| `'results'` | `'Results'` | (5, 6, 7) |

**binary：** 无

**是否需要samplerate：** 是（将微秒时间转换为采样数进行时序验证）

**是否输出到其他解码器：** 否（outputs为空）

### 3.2 Python decode()逻辑完整分析

AM230x/DHTxx是单线温湿度传感器协议，传输40位数据（16位湿度 + 16位温度 + 8位校验和）。

**时序定义（微秒）：**

| 名称 | min | max |
|------|-----|-----|
| START LOW | 750 | 25000 |
| START HIGH | 10 | 10000 |
| RESPONSE LOW | 50 | 90 |
| RESPONSE HIGH | 50 | 90 |
| BIT LOW | 45 | 90 |
| BIT 0 HIGH | 20 | 35 |
| BIT 1 HIGH | 65 | 80 |

**状态机（8个状态）：**

1. **WAIT FOR START LOW**：等待下降沿 → 记录fall → 转到状态2
2. **WAIT FOR START HIGH**：等待上升沿 → 验证START LOW时间 → 记录rise → 转到状态3，否则reset
3. **WAIT FOR RESPONSE LOW**：等待下降沿 → 验证START HIGH时间 → 输出Start注释 → 记录fall → 转到状态4，否则reset
4. **WAIT FOR RESPONSE HIGH**：等待上升沿 → 验证RESPONSE LOW时间 → 记录rise → 转到状态5，否则reset
5. **WAIT FOR FIRST BIT**：等待下降沿 → 验证RESPONSE HIGH时间 → 输出Response注释 → 记录fall → bytepos追加 → 转到状态6，否则reset
6. **WAIT FOR BIT HIGH**：等待上升沿 → 验证BIT LOW时间 → 记录rise → 转到状态7，否则reset
7. **WAIT FOR BIT LOW**：等待下降沿 → 判断BIT 0 HIGH或BIT 1 HIGH → handle_byte(bit) → 否则reset
8. **WAIT FOR END**：等待上升沿 → 输出End注释 → reset

**is_valid函数：**
- LOW时序：`dt = samplenum - fall`，与cnt[name]['min']/['max']比较
- HIGH时序：`dt = samplenum - rise`，与cnt[name]['min']/['max']比较

**handle_byte函数：**
- 追加bit到bits列表
- 输出Bit注释：`[2, ['Bit: %d' % bit, '%d' % bit]]`
- 更新fall
- 转到WAIT FOR BIT HIGH状态
- 每8位：
  - 计算字节值：`bits2num(bits[-8:])`
  - 输出Byte注释：`[4, ['Byte: %#04x' % byte, '%#04x' % byte]]`
  - 16位时：计算湿度 → 输出Humidity注释
  - 32位时：计算温度 → 输出Temperature注释
  - 40位时：验证校验和 → 输出Checksum注释 → 转到WAIT FOR END状态
  - 追加bytepos

**湿度计算：**
- DHT11：`h = bits2num(bitlist[0:8])`（整数）
- AM230x/RHT：`h = bits2num(bitlist) / 10`（一位小数）

**温度计算：**
- DHT11：`t = bits2num(bitlist[0:8])`（整数）
- AM230x/RHT：`t = bits2num(bitlist[1:]) / 10`，若bitlist[0]==1则取负

**校验和计算：**
- `checksum = sum(bits2num(bits[i-8:i]) for i in range(8, len(bits)+1, 8)) % 256`

**bits2num函数：**
- 将位列表转为整数（LSB在最后）：`number += bitlist[-1-i] * 2^i`

**metadata回调：**
- 收到samplerate时，将timing表中的微秒值转换为采样数：`cnt[name][t] = timing[name][t] * samplerate / 1000000`

### 3.3 C实现计划

**枚举定义：**
```c
enum {
    ANN_START = 0,
    ANN_RESPONSE,
    ANN_BIT,
    ANN_END,
    ANN_BYTE,
    ANN_HUMIDITY,
    ANN_TEMPERATURE,
    ANN_CHECKSUM,
    NUM_ANN,
};

enum am230x_state {
    STATE_WAIT_START_LOW = 0,
    STATE_WAIT_START_HIGH,
    STATE_WAIT_RESPONSE_LOW,
    STATE_WAIT_RESPONSE_HIGH,
    STATE_WAIT_FIRST_BIT,
    STATE_WAIT_BIT_HIGH,
    STATE_WAIT_BIT_LOW,
    STATE_WAIT_END,
};
```

**时序结构体：**
```c
typedef struct {
    uint64_t min;
    uint64_t max;
} timing_range;
```

**状态结构体：**
```c
typedef struct {
    uint64_t samplerate;
    int out_ann;
    int device;         // 0=am230x/rht, 1=dht11
    int state;
    uint64_t fall;
    uint64_t rise;
    uint8_t bits[40];   // 最多40位
    int bit_count;
    uint64_t bytepos[5]; // 最多5个字节位置
    int bytepos_count;
    timing_range cnt[7]; // 7种时序的采样数范围
} am230x_state;
```

**时序索引枚举：**
```c
enum timing_index {
    TIMING_START_LOW = 0,
    TIMING_START_HIGH,
    TIMING_RESPONSE_LOW,
    TIMING_RESPONSE_HIGH,
    TIMING_BIT_LOW,
    TIMING_BIT_0_HIGH,
    TIMING_BIT_1_HIGH,
};
```

**时序常量（微秒）：**
```c
static const timing_range timing_us[7] = {
    { 750, 25000 },   // START LOW
    { 10,  10000 },   // START HIGH
    { 50,  90 },      // RESPONSE LOW
    { 50,  90 },      // RESPONSE HIGH
    { 45,  90 },      // BIT LOW
    { 20,  35 },      // BIT 0 HIGH
    { 65,  80 },      // BIT 1 HIGH
};
```

**通道定义：**
```c
static struct srd_channel am230x_channels[] = {
    { "sda", "SDA", "Single wire serial data line", 0, SRD_CHANNEL_SDATA, "dec_am230x_chan_sda" },
};
```

**选项定义：**
```c
static struct srd_decoder_option am230x_options[] = {
    { "device", NULL, "Device type", NULL, NULL },
};
```

**注释标签：**
```c
static const char* am230x_ann_labels[][3] = {
    { "", "start", "Start" },
    { "", "response", "Response" },
    { "", "bit", "Bit" },
    { "", "end", "End" },
    { "", "byte", "Byte" },
    { "", "humidity", "Relative humidity in percent" },
    { "", "temperature", "Temperature in degrees Celsius" },
    { "", "checksum", "Checksum" },
};
```

**注释行：**
```c
static const int am230x_row_bits_classes[] = {ANN_START, ANN_RESPONSE, ANN_BIT, ANN_END};
static const int am230x_row_bytes_classes[] = {ANN_BYTE};
static const int am230x_row_results_classes[] = {ANN_HUMIDITY, ANN_TEMPERATURE, ANN_CHECKSUM};

static const struct srd_c_ann_row am230x_ann_rows[] = {
    { "bits", "Bits", am230x_row_bits_classes, 4 },
    { "bytes", "Bytes", am230x_row_bytes_classes, 1 },
    { "results", "Results", am230x_row_results_classes, 3 },
};
```

**辅助函数：**

```c
static int is_valid(am230x_state *s, uint64_t samplenum, int timing_idx) {
    uint64_t dt;
    if (timing_idx == TIMING_START_LOW || timing_idx == TIMING_RESPONSE_LOW ||
        timing_idx == TIMING_BIT_LOW) {
        dt = samplenum - s->fall;
    } else {
        dt = samplenum - s->rise;
    }
    return (dt >= s->cnt[timing_idx].min && dt <= s->cnt[timing_idx].max);
}

static uint8_t bits2num(uint8_t *bits, int count) {
    uint8_t number = 0;
    for (int i = 0; i < count; i++) {
        number += bits[count - 1 - i] * (1 << i);
    }
    return number;
}

static uint16_t bits2num16(uint8_t *bits, int count) {
    uint16_t number = 0;
    for (int i = 0; i < count && i < 16; i++) {
        number += bits[count - 1 - i] * (1 << i);
    }
    return number;
}
```

**decode函数核心逻辑：**
```c
static void am230x_decode(struct srd_decoder_inst *di) {
    am230x_state *s = (am230x_state *)c_decoder_get_private(di);
    uint64_t samplenum, matched;
    int ret;

    while (1) {
        switch (s->state) {
        case STATE_WAIT_START_LOW:
            {
                srd_cond_builder *cb = c_cond_new();
                c_cond_fall(cb, 0);
                ret = c_cond_wait(cb, di, &samplenum, &matched);
                c_cond_free(cb);
                if (ret != SRD_OK) return;
                s->fall = samplenum;
                s->state = STATE_WAIT_START_HIGH;
            }
            break;

        case STATE_WAIT_START_HIGH:
            {
                srd_cond_builder *cb = c_cond_new();
                c_cond_rise(cb, 0);
                ret = c_cond_wait(cb, di, &samplenum, &matched);
                c_cond_free(cb);
                if (ret != SRD_OK) return;
                if (is_valid(s, samplenum, TIMING_START_LOW)) {
                    s->rise = samplenum;
                    s->state = STATE_WAIT_RESPONSE_LOW;
                } else {
                    am230x_reset_state(s);
                }
            }
            break;

        // ... 类似处理其他状态 ...

        case STATE_WAIT_BIT_LOW:
            {
                srd_cond_builder *cb = c_cond_new();
                c_cond_fall(cb, 0);
                ret = c_cond_wait(cb, di, &samplenum, &matched);
                c_cond_free(cb);
                if (ret != SRD_OK) return;
                int bit;
                if (is_valid(s, samplenum, TIMING_BIT_0_HIGH)) {
                    bit = 0;
                } else if (is_valid(s, samplenum, TIMING_BIT_1_HIGH)) {
                    bit = 1;
                } else {
                    am230x_reset_state(s);
                    break;
                }
                // handle_byte逻辑
                s->bits[s->bit_count++] = bit;
                char tmp[16];
                snprintf(tmp, sizeof(tmp), "Bit: %d", bit);
                C_ANN_PUT(di, s->fall, samplenum, s->out_ann, ANN_BIT, tmp, bit ? "1" : "0");
                s->fall = samplenum;
                s->state = STATE_WAIT_BIT_HIGH;

                if (s->bit_count % 8 == 0) {
                    int byte_idx = s->bit_count / 8 - 1;
                    uint8_t byte_val = bits2num(&s->bits[byte_idx * 8], 8);
                    char tmp2[32];
                    snprintf(tmp2, sizeof(tmp2), "Byte: 0x%02x", byte_val);
                    C_ANN_PUT(di, s->bytepos[s->bytepos_count - 1], samplenum, s->out_ann, ANN_BYTE, tmp2, tmp2);

                    if (s->bit_count == 16) {
                        // 湿度
                        double h;
                        if (s->device == 1) { // DHT11
                            h = bits2num(&s->bits[0], 8);
                        } else {
                            h = bits2num16(&s->bits[0], 16) / 10.0;
                        }
                        char htmp[64];
                        snprintf(htmp, sizeof(htmp), "Humidity: %.1f %%", h);
                        char hshort[32];
                        snprintf(hshort, sizeof(hshort), "RH = %.1f %%", h);
                        C_ANN_PUT(di, s->bytepos[s->bytepos_count - 2], samplenum, s->out_ann, ANN_HUMIDITY, htmp, hshort);
                    } else if (s->bit_count == 32) {
                        // 温度
                        double t;
                        if (s->device == 1) { // DHT11
                            t = bits2num(&s->bits[16], 8);
                        } else {
                            t = bits2num16(&s->bits[17], 15) / 10.0;
                            if (s->bits[16] == 1) t = -t;
                        }
                        char ttmp[64];
                        snprintf(ttmp, sizeof(ttmp), "Temperature: %.1f °C", t);
                        char tshort[32];
                        snprintf(tshort, sizeof(tshort), "T = %.1f °C", t);
                        C_ANN_PUT(di, s->bytepos[s->bytepos_count - 2], samplenum, s->out_ann, ANN_TEMPERATURE, ttmp, tshort);
                    } else if (s->bit_count == 40) {
                        // 校验和
                        uint8_t parity = bits2num(&s->bits[32], 8);
                        uint8_t checksum = 0;
                        for (int i = 0; i < 4; i++) {
                            checksum += bits2num(&s->bits[i * 8], 8);
                        }
                        checksum %= 256;
                        if (parity == checksum) {
                            C_ANN_PUT(di, s->bytepos[s->bytepos_count - 1], samplenum, s->out_ann, ANN_CHECKSUM, "Checksum: OK", "OK");
                        } else {
                            C_ANN_PUT(di, s->bytepos[s->bytepos_count - 1], samplenum, s->out_ann, ANN_CHECKSUM, "Checksum: not OK", "NOK");
                        }
                        s->state = STATE_WAIT_END;
                    }
                    s->bytepos[s->bytepos_count++] = samplenum;
                }
            }
            break;

        case STATE_WAIT_END:
            {
                srd_cond_builder *cb = c_cond_new();
                c_cond_rise(cb, 0);
                ret = c_cond_wait(cb, di, &samplenum, &matched);
                c_cond_free(cb);
                if (ret != SRD_OK) return;
                C_ANN_PUT(di, s->fall, samplenum, s->out_ann, ANN_END, "End", "E");
                am230x_reset_state(s);
            }
            break;
        }
    }
}
```

### 3.4 关键实现说明

1. **时序转换**：在metadata回调或start()中将微秒值转换为采样数：`cnt[i].min = timing_us[i].min * samplerate / 1000000`
2. **bits2num**：Python版本使用`bitlist[-1-i] * 2**i`，即LSB在列表末尾。C中需要对应实现
3. **温度负值**：AM230x/RHT模式下，bitlist[0]（即bits[16]）为1表示负温度，实际温度值从bits[17]开始
4. **DHT11差异**：DHT11只取前8位作为整数湿度/温度，AM230x取16位除以10
5. **putfs/putb/putv**：
   - `putfs`：从fall到samplenum
   - `putb`：从bytepos[-1]到samplenum
   - `putv`：从bytepos[-2]到samplenum
6. **bytepos管理**：在WAIT FOR FIRST BIT状态成功时追加第一个bytepos，每8位追加一个
7. **°C字符**：C字符串中需要使用UTF-8编码的温度符号"\xc2\xb0C"
8. **校验和**：4个字节值相加模256，与第5个字节比较

---

## 4. Caliper (Digital Calipers)

### 4.1 Python解码器元数据

| 字段 | 值 |
|------|-----|
| id | `'caliper'` |
| name | `'Caliper'` |
| longname | `'Digital calipers'` |
| desc | `'Protocol of cheap generic digital calipers.'` |
| license | `'mit'` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Analog/digital', 'Sensor']` |

**通道：**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | `'clk'` | `'CLK'` | `'Serial clock line'` | `'dec_caliper_chan_clk'` |
| 1 | `'data'` | `'DATA'` | `'Serial data line'` | `'dec_caliper_chan_data'` |

**选项：**

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| `'timeout_ms'` | `'Packet timeout in ms, 0 to disable'` | 10 | (整数) | `'dec_caliper_opt_timeout_ms'` |
| `'unit'` | `'Convert units'` | `'keep'` | `('keep', 'mm', 'inch')` | `'dec_caliper_opt_unit'` |
| `'changes'` | `'Changes only'` | `'no'` | `('no', 'yes')` | `'dec_caliper_opt_changes'` |

**注释（annotations）：**

| 索引 | id | desc |
|------|-----|------|
| 0 | `'measurement'` | `'Measurement'` |
| 1 | `'warning'` | `'Warning'` |

**注释行（annotation_rows）：**

| id | label | class_tuple |
|----|-------|-------------|
| `'measurements'` | `'Measurements'` | (0,) |
| `'warnings'` | `'Warnings'` | (1,) |

**binary：** 无

**是否需要samplerate：** 是（计算超时采样数）

**是否输出到其他解码器：** 否（outputs为空）

### 4.2 Python decode()逻辑完整分析

Caliper解码器解析廉价数字游标卡尺的串行时钟数据协议。

**协议格式：**
- 16位数值数据 + 8位标志位 = 24位
- 时钟上升沿采样数据
- 可选超时检测

**状态机流程：**

1. **初始化**：`last_sent = None`，设置wait条件
2. **主循环**：
   - 等待条件：`[{0: 'r'}]`（CLK上升沿），可选加`{'skip': timeout_snum}`超时
   - **超时处理**：如果超时（`matched & 0b1 != 0b1`）且缓冲区有数据：
     - 输出Warning注释：`'timeout with N bits in buffer'`
     - reset
   - **正常数据**：
     - 记录ss（第一个bit位置）和es（最后活动位置）
     - 前16位：追加到`number_bits`
     - 接下来8位：追加到`flags_bits`，满8位后处理
   - **数据处理**（24位全部接收后）：
     - `negative = bool(flags_bits[4])` — 第4位为负号标志
     - `is_inch = bool(flags_bits[7])` — 第7位为英寸标志
     - `number = bitpack(number_bits)` — 将位列表打包为整数
     - 负数处理：`number = -number`
     - 英寸模式：`number /= 2000`
       - 若要转换为mm：`number *= 25.4`
     - 毫米模式：`number /= 100`
       - 若要转换为inch：`number = round(number / 25.4, 4)`
     - 输出Measurement注释：`'{number}{unit}'`
     - 若`changes == 'yes'`且值未变，则不输出
   - reset

**bitpack函数**：来自`common.srdhelper`，将位列表打包为整数（MSB优先）

**mm_per_inch常量**：25.4

### 4.3 C实现计划

**枚举定义：**
```c
enum {
    ANN_MEASUREMENT = 0,
    ANN_WARNING,
    NUM_ANN,
};
```

**状态结构体：**
```c
typedef struct {
    uint64_t samplerate;
    int out_ann;
    int timeout_ms;
    int unit;           // 0=keep, 1=mm, 2=inch
    int changes_only;   // 0=no, 1=yes
    uint64_t ss;        // 数据包起始
    uint64_t es;        // 数据包结束
    uint8_t number_bits[16];
    int number_count;
    uint8_t flags_bits[8];
    int flags_count;
    double last_number;
    int last_is_inch;
    int has_last;
} caliper_state;
```

**通道定义：**
```c
static struct srd_channel caliper_channels[] = {
    { "clk", "CLK", "Serial clock line", 0, SRD_CHANNEL_SCLK, "dec_caliper_chan_clk" },
    { "data", "DATA", "Serial data line", 1, SRD_CHANNEL_SDATA, "dec_caliper_chan_data" },
};
```

**选项定义：**
```c
static struct srd_decoder_option caliper_options[] = {
    { "timeout_ms", NULL, "Packet timeout in ms, 0 to disable", NULL, NULL },
    { "unit", NULL, "Convert units", NULL, NULL },
    { "changes", NULL, "Changes only", NULL, NULL },
};
```

**注释标签：**
```c
static const char* caliper_ann_labels[][3] = {
    { "", "measurement", "Measurement" },
    { "", "warning", "Warning" },
};
```

**注释行：**
```c
static const int caliper_row_measurements_classes[] = {ANN_MEASUREMENT};
static const int caliper_row_warnings_classes[] = {ANN_WARNING};

static const struct srd_c_ann_row caliper_ann_rows[] = {
    { "measurements", "Measurements", caliper_row_measurements_classes, 1 },
    { "warnings", "Warnings", caliper_row_warnings_classes, 1 },
};
```

**decode函数核心逻辑：**
```c
static void caliper_decode(struct srd_decoder_inst *di) {
    caliper_state *s = (caliper_state *)c_decoder_get_private(di);
    uint64_t samplenum, matched;
    int ret;
    int has_timeout = (s->timeout_ms > 0);
    uint64_t timeout_snum = 0;
    if (has_timeout) {
        timeout_snum = (uint64_t)s->timeout_ms * s->samplerate / 1000;
    }

    while (1) {
        // 等待CLK上升沿，可选超时
        srd_cond_builder *cb = c_cond_new();
        c_cond_rise(cb, 0);
        if (has_timeout) {
            c_cond_or(cb);
            c_cond_skip(cb, timeout_snum);
        }
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;

        // 超时检查
        if (has_timeout && !(matched & 0x1)) {
            // 超时
            if (s->number_count > 0 || s->flags_count > 0) {
                int count = s->number_count + s->flags_count;
                char tmp[64];
                snprintf(tmp, sizeof(tmp), "timeout with %d bits in buffer", count);
                char tmp2[32];
                snprintf(tmp2, sizeof(tmp2), "timeout (%d bits)", count);
                C_ANN_PUT(di, s->ss, samplenum, s->out_ann, ANN_WARNING, tmp, tmp2, "timeout");
            }
            caliper_reset_state(s);
            continue;
        }

        // 采样DATA引脚
        uint8_t data = c_decoder_get_pin(di, 1, samplenum);

        // 记录位置
        if (!s->ss) s->ss = samplenum;
        s->es = samplenum;

        // 收集位数
        if (s->number_count < 16) {
            s->number_bits[s->number_count++] = data;
            continue;
        }
        if (s->flags_count < 8) {
            s->flags_bits[s->flags_count++] = data;
            if (s->flags_count < 8) continue;
        }

        // 24位全部接收，处理数据
        int negative = s->flags_bits[4] ? 1 : 0;
        int is_inch = s->flags_bits[7] ? 1 : 0;

        // bitpack: MSB first
        uint32_t number = 0;
        for (int i = 0; i < 16; i++) {
            number = (number << 1) | s->number_bits[i];
        }

        if (negative) number = -number; // 注意：这里需要用int32_t
        int32_t signed_number = (int32_t)number;
        if (negative) signed_number = -signed_number;

        double value;
        if (is_inch) {
            value = (double)signed_number / 2000.0;
            if (s->unit == 1) { // 转mm
                value *= 25.4;
                is_inch = 0;
            }
        } else {
            value = (double)signed_number / 100.0;
            if (s->unit == 2) { // 转inch
                value = round(value / 25.4 * 10000.0) / 10000.0;
                is_inch = 1;
            }
        }

        const char *unit_str = is_inch ? "in" : "mm";

        // 变化检测
        int should_output = 1;
        if (s->changes_only && s->has_last) {
            // 比较当前值和上次值（简化比较）
            if (value == s->last_number && is_inch == s->last_is_inch) {
                should_output = 0;
            }
        }

        if (should_output) {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), "%g%s", value, unit_str);
            char tmp2[32];
            snprintf(tmp2, sizeof(tmp2), "%g", value);
            C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_MEASUREMENT, tmp, tmp2);
            s->last_number = value;
            s->last_is_inch = is_inch;
            s->has_last = 1;
        }

        caliper_reset_state(s);
    }
}
```

### 4.4 关键实现说明

1. **bitpack**：Python的`bitpack`从`common.srdhelper`导入，MSB优先打包。C中手动实现：`number = (number << 1) | bit`
2. **负数处理**：Python中`number = -number`对无符号数取负后变为负数。C中需要用`int32_t`处理
3. **超时条件**：使用`c_cond_or` + `c_cond_skip`实现超时。matched的bit 0对应第一个条件（CLK上升沿），bit 1对应第二个条件（超时）
4. **ss初始值**：Python中`if not self.ss: self.ss = self.samplenum`，ss初始为0。C中同样用0判断
5. **round函数**：Python的`round(number / mm_per_inch, 4)`，C中可用`round(x * 10000) / 10000`
6. **%g格式**：Python的`{number}`使用默认格式，C中`%g`会自动选择`%f`或`%e`
7. **timeout_snum计算**：`timeout_ms * samplerate / 1000`，注意整数溢出，使用uint64_t
8. **DATA引脚读取**：使用`c_decoder_get_pin(di, 1, samplenum)`在CLK上升沿读取DATA值

---

## 5. Carrera (Carrera Digital)

### 5.1 Python解码器元数据

| 字段 | 值 |
|------|-----|
| id | `'Carrera'` |
| name | `'Carrera Digital Decoder'` |
| longname | `'longname'` |
| desc | `'was macht der wohl?'` |
| license | `'gplv2+'` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['C Digital']` |

**通道：**

| 序号 | id | name | desc |
|------|-----|------|------|
| 0 | `'data'` | `'Data'` | `'Data line'` |

**可选通道：** 无（`optional_channels = ()`）

**选项：**

| id | desc | default | values |
|----|------|---------|--------|
| `'invert'` | `'Signal ist invertiert'` | `'nein'` | `('ja', 'nein')` |
| `'format'` | `'Data format'` | `'hex'` | `('hex', 'dec', 'oct', 'bin')` |

**注释（annotations）：**

| 索引 | id | desc |
|------|-----|------|
| 0 | `'controller_0'` | `'Reglerwort ID 0'` |
| 1 | `'controller_1'` | `'Reglerwort ID 1'` |
| 2 | `'controller_2'` | `'Reglerwort ID 2'` |
| 3 | `'controller_3'` | `'Reglerwort ID 3'` |
| 4 | `'controller_4'` | `'Reglerwort ID 4'` |
| 5 | `'controller_5'` | `'Reglerwort ID 5'` |
| 6 | `'controller_sc'` | `'Reglerwort SC/Ghost'` |
| 7 | `'controller_prog'` | `'Programmierwort'` |
| 8 | `'controller_active'` | `'Aktivdatenwort'` |
| 9 | `'bit'` | `'Bit'` |
| 10 | `'quittierung'` | `'Quittierungswort'` |
| 11 | `'prog_gas'` | `''` |
| 12 | `'prog_general'` | `'Programmierdatenwort'` |
| 13 | `'prog_bremse'` | `''` |
| 14 | `'prog_tank'` | `''` |
| 15 | `'prog_werte'` | `''` |
| 16 | `'prog_tanken'` | `''` |
| 17 | `'prog_position'` | `''` |
| 18 | `'prog_finish'` | `''` |
| 19 | `'prog_finishline'` | `''` |
| 20 | `'prog_fuel'` | `''` |
| 21 | `'prog_jumpstart'` | `''` |
| 22 | `'prog_traffic_light'` | `''` |
| 23 | `'prog_lapcount'` | `''` |
| 24 | `'prog_reset'` | `''` |
| 25 | `'prog_pitlaneadapter'` | `''` |
| 26 | `'prog_performance'` | `''` |

**注释行（annotation_rows）：**

| id | label | class_tuple |
|----|-------|-------------|
| `'word_bit_value'` | `'Bits'` | (9,) |
| `'word_controller'` | `'Reglerwort'` | (0,1,2,3,4,5,6) |
| `'active_quit'` | `'Aktiv-/Quittierungswort'` | (8, 10) |
| `'prog_word'` | `'Programmierdatenwort'` | (11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26) |

**binary：** 无

**是否需要samplerate：** 是（计算微秒时间间隔）

**是否输出到其他解码器：** 否（outputs为空）

### 5.2 Python decode()逻辑完整分析

Carrera Digital是数字赛车控制系统协议。

**位时序：**
- 位周期：75-125微秒
- 字间隔：>6000微秒
- 位判定：在边沿变化时，根据间隔时间判断是否为新位

**数据字类型：**
1. **Reglerdatenwort（控制器字）**：dataWord < 1024
   - ID = (dataWord >> 6) & 0x07（3位）
   - ID=7时为SC/Ghost控制器
   - 格式：`ID:X G: Y WT:Z TA:W` 或 `KFR:X TK:Y FR:Z NH:W PC:V TA:U`
2. **Aktivdatenwort（活动数据字）**：127 < dataWord < 256（在next_could_be_active_data_word为True时）
3. **Quittierungswort（确认字）**：dataWord < 512（在next_could_be_active_data_word为True时）
4. **Programmierdatenwort（编程数据字）**：dataWord >= 1024
   - regler = flip_bits(dataWord >> 0, 3)（3位，翻转）
   - befehl = flip_bits(dataWord >> 3, 5)（5位，翻转）
   - wert = flip_bits(dataWord >> 8, 4)（4位，翻转）
   - 注释索引 = 11 + befehl

**decode()主循环：**

1. 等待任意边沿：`wait({0: 'e'})`
2. 计算时间间隔：`intervalMicros = currentMicros - previousMicros`
3. `intervalMicros < 200`：更新`endDataWord = samplenum`
4. `75 <= intervalMicros <= 125`：有效位周期
   - 更新`previousMicros = currentMicros`
   - `dataWord <<= 1`
   - 如果pin[0] == bit（考虑反转）：`dataWord |= 1`，输出bit 1
   - 否则：输出bit 0
   - 更新`bitStart = samplenum`
5. `intervalMicros > 6000`：字间隔
   - 如果`next_could_be_active_data_word`：
     - `127 < dataWord < 256`：Aktivdatenwort
     - `dataWord < 512`：Quittierungswort
     - 重置`next_could_be_active_data_word = False`
   - 否则如果`dataWord < 1024`：Reglerdatenwort
   - 否则：Programmierdatenwort
   - 重置`dataWord = 1`，更新位置

**flip_bits函数**：反转位顺序
```python
def flip_bits(self, value, bitCount):
    bitCount -= 1
    result = 0
    while bitCount >= 0 and value:
        if value & 1:
            result |= (1 << bitCount)
        value >>= 1
        bitCount -= 1
    return result
```

**get_value_from_dataword函数**：提取指定位域
```python
def get_value_from_dataword(self, bitsToShift=0, bitWidth=1):
    compare_val = (1 << bitWidth) - 1
    return (self.dataWord >> bitsToShift) & compare_val
```

**print_reglerdatenwort**：
- `regler_id = (dataWord >> 6) & 0x07`
- `ta = dataWord & 0x01`
- 若regler_id == 2 或 7：`next_could_be_active_data_word = True`
- 若regler_id == 7：
  - 改为SC，提取pc(bit1), nh(bit2), fr(bit3), tk(bit4), kfr(bit5)
  - 格式：`KFR:{} TK:{} FR:{} NH:{} PC:{} TA:{}`
- 否则：
  - `gas = (dataWord >> 1) & 0x0f`
  - `wt = (dataWord >> 5) & 0x01`
  - 格式：`ID:{} G: {} WT:{} TA:{}`
- 注释索引 = regler_id（0-6）

**print_aktivdatenwort**：
- `ie = dataWord & 0x01`
- `r5 = (dataWord >> 1) & 0x01` ... `r0 = (dataWord >> 6) & 0x01`
- 格式：`R0:{} R1:{} R2:{} R3:{} R4:{} R5:{} IE:{}`
- 注释索引 = 8

**print_quittierungswort**：
- `s7 = dataWord & 0x01` ... `s0 = (dataWord >> 7) & 0x01`
- 格式：`S0:{} S1:{} S2:{} S3:{} S4:{} S5:{} S6:{} S7:{}`
- 注释索引 = 10

**print_programmierdatenwort**：
- `wert = flip_bits((dataWord >> 8) & 0x0f, 4)`
- `befehl = flip_bits((dataWord >> 3) & 0x1f, 5)`
- `regler = flip_bits((dataWord >> 0) & 0x07, 3)`
- 格式：`Befehl: {}, Regler: {}, Wert: {}`（使用format_data格式化）
- 注释索引 = 11 + befehl

**format_data函数**：
- hex：零填充到合适宽度
- oct：零填充到合适宽度
- bin：零填充到bit_width
- dec：普通十进制

### 5.3 C实现计划

**枚举定义：**
```c
enum {
    ANN_CONTROLLER_0 = 0,
    ANN_CONTROLLER_1,
    ANN_CONTROLLER_2,
    ANN_CONTROLLER_3,
    ANN_CONTROLLER_4,
    ANN_CONTROLLER_5,
    ANN_CONTROLLER_SC,     // 6
    ANN_CONTROLLER_PROG,   // 7
    ANN_CONTROLLER_ACTIVE, // 8
    ANN_BIT,               // 9
    ANN_QUITTIERUNG,       // 10
    ANN_PROG_GAS,          // 11
    ANN_PROG_GENERAL,      // 12
    ANN_PROG_BREMSE,       // 13
    ANN_PROG_TANK,         // 14
    ANN_PROG_WERTE,        // 15
    ANN_PROG_TANKEN,       // 16
    ANN_PROG_POSITION,     // 17
    ANN_PROG_FINISH,       // 18
    ANN_PROG_FINISHLINE,   // 19
    ANN_PROG_FUEL,         // 20
    ANN_PROG_JUMPSTART,    // 21
    ANN_PROG_TRAFFIC_LIGHT,// 22
    ANN_PROG_LAPCOUNT,     // 23
    ANN_PROG_RESET,        // 24
    ANN_PROG_PITLANE,      // 25
    ANN_PROG_PERFORMANCE,  // 26
    NUM_ANN,
};
```

**状态结构体：**
```c
typedef struct {
    uint64_t samplerate;
    int out_ann;
    int invert;         // 0=nein, 1=ja
    int format;         // 0=hex, 1=dec, 2=oct, 3=bin
    double currentMicros;
    double previousMicros;
    double intervalMicros;
    uint64_t wordStart;
    uint64_t wordEnd;
    uint64_t bitStart;
    uint32_t dataWord;
    uint64_t beginDataWord;
    uint64_t endDataWord;
    int next_could_be_active_data_word;
} carrera_state;
```

**通道定义：**
```c
static struct srd_channel carrera_channels[] = {
    { "data", "Data", "Data line", 0, SRD_CHANNEL_SDATA, NULL },
};
```

**选项定义：**
```c
static struct srd_decoder_option carrera_options[] = {
    { "invert", NULL, "Signal ist invertiert", NULL, NULL },
    { "format", NULL, "Data format", NULL, NULL },
};
```

**注释标签：**
```c
static const char* carrera_ann_labels[][3] = {
    { "", "controller_0", "Reglerwort ID 0" },
    { "", "controller_1", "Reglerwort ID 1" },
    { "", "controller_2", "Reglerwort ID 2" },
    { "", "controller_3", "Reglerwort ID 3" },
    { "", "controller_4", "Reglerwort ID 4" },
    { "", "controller_5", "Reglerwort ID 5" },
    { "", "controller_sc", "Reglerwort SC/Ghost" },
    { "", "controller_prog", "Programmierwort" },
    { "", "controller_active", "Aktivdatenwort" },
    { "", "bit", "Bit" },
    { "", "quittierung", "Quittierungswort" },
    { "", "prog_gas", "prog_gas" },
    { "", "prog_general", "Programmierdatenwort" },
    { "", "prog_bremse", "prog_bremse" },
    { "", "prog_tank", "prog_tank" },
    { "", "prog_werte", "prog_werte" },
    { "", "prog_tanken", "prog_tanken" },
    { "", "prog_position", "prog_position" },
    { "", "prog_finish", "prog_finish" },
    { "", "prog_finishline", "prog_finishline" },
    { "", "prog_fuel", "prog_fuel" },
    { "", "prog_jumpstart", "prog_jumpstart" },
    { "", "prog_traffic_light", "prog_traffic_light" },
    { "", "prog_lapcount", "prog_lapcount" },
    { "", "prog_reset", "prog_reset" },
    { "", "prog_pitlaneadapter", "prog_pitlaneadapter" },
    { "", "prog_performance", "prog_performance" },
};
```

**注释行：**
```c
static const int carrera_row_bits_classes[] = {ANN_BIT};
static const int carrera_row_controller_classes[] = {ANN_CONTROLLER_0, ANN_CONTROLLER_1, ANN_CONTROLLER_2, ANN_CONTROLLER_3, ANN_CONTROLLER_4, ANN_CONTROLLER_5, ANN_CONTROLLER_SC};
static const int carrera_row_active_quit_classes[] = {ANN_CONTROLLER_ACTIVE, ANN_QUITTIERUNG};
static const int carrera_row_prog_classes[] = {ANN_PROG_GAS, ANN_PROG_GENERAL, ANN_PROG_BREMSE, ANN_PROG_TANK, ANN_PROG_WERTE, ANN_PROG_TANKEN, ANN_PROG_POSITION, ANN_PROG_FINISH, ANN_PROG_FINISHLINE, ANN_PROG_FUEL, ANN_PROG_JUMPSTART, ANN_PROG_TRAFFIC_LIGHT, ANN_PROG_LAPCOUNT, ANN_PROG_RESET, ANN_PROG_PITLANE, ANN_PROG_PERFORMANCE};

static const struct srd_c_ann_row carrera_ann_rows[] = {
    { "word_bit_value", "Bits", carrera_row_bits_classes, 1 },
    { "word_controller", "Reglerwort", carrera_row_controller_classes, 7 },
    { "active_quit", "Aktiv-/Quittierungswort", carrera_row_active_quit_classes, 2 },
    { "prog_word", "Programmierdatenwort", carrera_row_prog_classes, 16 },
};
```

**辅助函数：**

```c
static uint32_t carrera_flip_bits(uint32_t value, int bitCount) {
    bitCount--;
    uint32_t result = 0;
    while (bitCount >= 0 && value) {
        if (value & 1)
            result |= (1 << bitCount);
        value >>= 1;
        bitCount--;
    }
    return result;
}

static uint32_t carrera_get_value(uint32_t dataWord, int bitsToShift, int bitWidth) {
    uint32_t mask = (1 << bitWidth) - 1;
    return (dataWord >> bitsToShift) & mask;
}

static double carrera_get_usec(uint64_t samplenum, uint64_t samplerate) {
    return (double)samplenum * 1000000.0 / (double)samplerate;
}

static void carrera_format_data(char *buf, int bufsize, uint32_t value, int bit_width, int format) {
    switch (format) {
    case 0: // hex
        {
            int width = (bit_width + 3) / 4;
            if (width < 2) width = 2;
            snprintf(buf, bufsize, "%0*x", width, value & ((1 << bit_width) - 1));
        }
        break;
    case 1: // dec
        snprintf(buf, bufsize, "%u", value);
        break;
    case 2: // oct
        {
            int width = (bit_width + 2) / 3;
            if (width < 3) width = 3;
            snprintf(buf, bufsize, "%0*o", width, value & ((1 << bit_width) - 1));
        }
        break;
    case 3: // bin
        {
            // 手动生成二进制字符串
            uint32_t mask = 1 << (bit_width - 1);
            int pos = 0;
            for (int i = 0; i < bit_width && pos < bufsize - 1; i++) {
                buf[pos++] = (value & mask) ? '1' : '0';
                mask >>= 1;
            }
            buf[pos] = '\0';
        }
        break;
    }
}
```

**decode函数核心逻辑：**
```c
static void carrera_decode(struct srd_decoder_inst *di) {
    carrera_state *s = (carrera_state *)c_decoder_get_private(di);
    uint64_t samplenum, matched;
    int ret;
    int bit_val = s->invert ? 1 : 0;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        c_cond_edge(cb, 0);
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;

        s->currentMicros = carrera_get_usec(samplenum, s->samplerate);
        s->intervalMicros = s->currentMicros - s->previousMicros;

        if (s->intervalMicros < 200.0) {
            s->endDataWord = samplenum;
        }

        if (s->intervalMicros >= 75.0 && s->intervalMicros <= 125.0) {
            // 有效位
            s->previousMicros = s->currentMicros;
            s->dataWord <<= 1;

            uint8_t pin = c_decoder_get_pin(di, 0, samplenum);
            if (pin == bit_val) {
                s->dataWord |= 1;
                C_ANN_PUT(di, s->bitStart, samplenum, s->out_ann, ANN_BIT, "1");
            } else {
                C_ANN_PUT(di, s->bitStart, samplenum, s->out_ann, ANN_BIT, "0");
            }
            s->bitStart = samplenum;
        } else if (s->intervalMicros > 6000.0) {
            // 字间隔
            if (s->next_could_be_active_data_word) {
                if (s->dataWord > 127 && s->dataWord < 256) {
                    carrera_print_aktivdatenwort(di, s);
                } else if (s->dataWord < 512) {
                    carrera_print_quittierungswort(di, s);
                }
                s->next_could_be_active_data_word = 0;
            } else if (s->dataWord < 1024) {
                carrera_print_reglerdatenwort(di, s);
            } else {
                carrera_print_programmierdatenwort(di, s);
            }
            s->dataWord = 1;
            s->previousMicros = s->currentMicros;
            s->beginDataWord = samplenum;
            s->bitStart = samplenum;
        }
    }
}
```

**print_reglerdatenwort：**
```c
static void carrera_print_reglerdatenwort(struct srd_decoder_inst *di, carrera_state *s) {
    int regler_id = carrera_get_value(s->dataWord, 6, 3);
    int ta = carrera_get_value(s->dataWord, 0, 1);

    if (regler_id == 2 || regler_id == 7)
        s->next_could_be_active_data_word = 1;

    char desc_long[128];
    char desc_short[16];
    char desc[32];
    int ann_idx;

    if (regler_id == 7) {
        ann_idx = 6; // ANN_CONTROLLER_SC
        int pc = carrera_get_value(s->dataWord, 1, 1);
        int nh = carrera_get_value(s->dataWord, 2, 1);
        int fr = carrera_get_value(s->dataWord, 3, 1);
        int tk = carrera_get_value(s->dataWord, 4, 1);
        int kfr = carrera_get_value(s->dataWord, 5, 1);
        snprintf(desc_long, sizeof(desc_long), "KFR:%d TK:%d FR:%d NH:%d PC:%d TA:%d", kfr, tk, fr, nh, pc, ta);
        snprintf(desc_short, sizeof(desc_short), "R SC");
        snprintf(desc, sizeof(desc), "Regler SC");
    } else {
        ann_idx = regler_id; // 0-5
        int gas = (s->dataWord >> 1) & 0x0f;
        int wt = carrera_get_value(s->dataWord, 5, 1);
        snprintf(desc_long, sizeof(desc_long), "ID:%d G: %d WT:%d TA:%d", regler_id, gas, wt, ta);
        snprintf(desc_short, sizeof(desc_short), "R %d", regler_id);
        snprintf(desc, sizeof(desc), "Regler %d", regler_id);
    }

    C_ANN_PUT(di, s->beginDataWord, s->endDataWord, s->out_ann, ann_idx, desc_short, desc, desc_long);
}
```

**print_aktivdatenwort：**
```c
static void carrera_print_aktivdatenwort(struct srd_decoder_inst *di, carrera_state *s) {
    int ie = carrera_get_value(s->dataWord, 0, 1);
    int r5 = carrera_get_value(s->dataWord, 1, 1);
    int r4 = carrera_get_value(s->dataWord, 2, 1);
    int r3 = carrera_get_value(s->dataWord, 3, 1);
    int r2 = carrera_get_value(s->dataWord, 4, 1);
    int r1 = carrera_get_value(s->dataWord, 5, 1);
    int r0 = carrera_get_value(s->dataWord, 6, 1);

    char desc_short[16];
    snprintf(desc_short, sizeof(desc_short), "IE:%d", ie);
    char desc_long[128];
    snprintf(desc_long, sizeof(desc_long), "R0:%d R1:%d R2:%d R3:%d R4:%d R5:%d IE:%d", r0, r1, r2, r3, r4, r5, ie);

    C_ANN_PUT(di, s->beginDataWord, s->endDataWord, s->out_ann, ANN_CONTROLLER_ACTIVE, desc_short, desc_long);
}
```

**print_quittierungswort：**
```c
static void carrera_print_quittierungswort(struct srd_decoder_inst *di, carrera_state *s) {
    int s0 = carrera_get_value(s->dataWord, 7, 1);
    int s1 = carrera_get_value(s->dataWord, 6, 1);
    int s2 = carrera_get_value(s->dataWord, 5, 1);
    int s3 = carrera_get_value(s->dataWord, 4, 1);
    int s4 = carrera_get_value(s->dataWord, 3, 1);
    int s5 = carrera_get_value(s->dataWord, 2, 1);
    int s6 = carrera_get_value(s->dataWord, 1, 1);
    int s7 = carrera_get_value(s->dataWord, 0, 1);

    char desc_long[128];
    snprintf(desc_long, sizeof(desc_long), "S0:%d S1:%d S2:%d S3:%d S4:%d S5:%d S6:%d S7:%d", s0, s1, s2, s3, s4, s5, s6, s7);

    C_ANN_PUT(di, s->beginDataWord, s->endDataWord, s->out_ann, ANN_QUITTIERUNG, "Q", "Quitt.", desc_long);
}
```

**print_programmierdatenwort：**
```c
static void carrera_print_programmierdatenwort(struct srd_decoder_inst *di, carrera_state *s) {
    uint32_t wert_raw = carrera_get_value(s->dataWord, 8, 4);
    uint32_t befehl_raw = carrera_get_value(s->dataWord, 3, 5);
    uint32_t regler_raw = carrera_get_value(s->dataWord, 0, 3);

    uint32_t wert = carrera_flip_bits(wert_raw, 4);
    uint32_t befehl = carrera_flip_bits(befehl_raw, 5);
    uint32_t regler = carrera_flip_bits(regler_raw, 3);

    char befehl_str[32], regler_str[32], wert_str[32];
    carrera_format_data(befehl_str, sizeof(befehl_str), befehl, 5, s->format);
    carrera_format_data(regler_str, sizeof(regler_str), regler, 3, s->format);
    carrera_format_data(wert_str, sizeof(wert_str), wert, 4, s->format);

    char desc[128];
    snprintf(desc, sizeof(desc), "Befehl: %s, Regler: %s, Wert: %s", befehl_str, regler_str, wert_str);

    int ann_idx = 11 + (int)befehl;
    if (ann_idx >= NUM_ANN) ann_idx = ANN_PROG_GENERAL; // 安全保护

    C_ANN_PUT(di, s->beginDataWord, s->endDataWord, s->out_ann, ann_idx, desc);
}
```

### 5.4 关键实现说明

1. **时间计算**：所有时间间隔以微秒计算，`currentMicros = samplenum * 1000000.0 / samplerate`
2. **dataWord初始值**：Python中初始化为1（不是0），因为位移操作`dataWord <<= 1`后OR 1
3. **位判定**：`pin[0] == bit`时为1，其中`bit`根据invert选项决定（invert时bit=1，否则bit=0）
4. **字间隔判定**：`intervalMicros > 6000`表示新字开始
5. **有效位判定**：`75 <= intervalMicros <= 125`表示有效位
6. **next_could_be_active_data_word**：当控制器ID为2或7时设为True，下一个字可能是Aktivdatenwort或Quittierungswort
7. **flip_bits**：反转位顺序，用于Programmierdatenwort中的字段解码
8. **注释索引**：Programmierdatenwort的注释索引为`11 + befehl`，需要确保不越界
9. **format_data**：需要支持hex/dec/oct/bin四种格式，bin需要手动实现
10. **prog_performance注释**：注意Python源码中id为`'prog_pe rformance'`（有空格），这可能是笔误，C实现中应修正为`'prog_performance'`
11. **previousMicros初始化**：初始为0，第一个边沿的intervalMicros会很大，触发字间隔处理
12. **dataWord溢出**：Python中整数无溢出，C中用uint32_t足够（协议最长约13位+8位=21位）

---

## 通用实现模式

### C解码器文件模板

```c
#include "libsigrokdecode.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 枚举定义
enum { ... NUM_ANN };

// 状态结构体
typedef struct { ... } xxx_state;

// 通道、选项、注释等静态数据
static struct srd_channel xxx_channels[] = { ... };
static struct srd_decoder_option xxx_options[] = { ... };
static const char* xxx_ann_labels[][3] = { ... };
static const struct srd_c_ann_row xxx_ann_rows[] = { ... };

// 回调函数
static void xxx_reset(struct srd_decoder_inst *di) { ... }
static void xxx_start(struct srd_decoder_inst *di) { ... }
static void xxx_decode(struct srd_decoder_inst *di) { ... }
static void xxx_destroy(struct srd_decoder_inst *di) { ... }

// 解码器结构体
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    .longname = "...",
    .desc = "...",
    .license = "...",
    .channels = xxx_channels,
    .num_channels = N,
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
    .outputs = xxx_outputs,
    .num_outputs = N,
    .binary = NULL,
    .num_binary = 0,
    .tags = xxx_tags,
    .num_tags = N,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,
    .destroy = xxx_destroy,
};

// 入口函数
SRD_C_DECODER_EXPORT struct srd_c_decoder* srd_c_decoder_entry(void) {
    // 初始化选项的id、idn、desc、def、values
    return &xxx_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void) {
    return SRD_C_DECODER_API_VERSION;
}
```

### 二进制格式化辅助函数

由于C标准库不普遍支持`%b`格式，需要自定义：

```c
static int format_bin(char *buf, int bufsize, unsigned int value, int width) {
    int pos = 0;
    for (int i = width - 1; i >= 0 && pos < bufsize - 1; i--) {
        buf[pos++] = (value & (1 << i)) ? '1' : '0';
    }
    buf[pos] = '\0';
    return pos;
}
```

### 采样率检查模式

```c
if (!s->samplerate)
    s->samplerate = c_decoder_get_samplerate(di);
if (!s->samplerate)
    return;
```

### CMakeLists.txt修改

在`C_DECODERS`列表中添加5个新解码器名称：
```
adb_c
afsk_c
am230x_c
caliper_c
carrera_c
```
