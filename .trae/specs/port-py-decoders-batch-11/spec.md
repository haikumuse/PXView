# Python Decoder 移植为 C Decoder 详细规格 — Batch 11

## 概述

本文档详细描述将 5 个 Python decoder 移植为 C decoder 的完整规格。目标 decoder 列表：

1. **parallel** — 通用并行同步总线 decoder
2. **pcfx-ctrlr** — PC-FX 游戏机控制器协议 decoder
3. **rinnai-control-panel** — Rinnai 燃气热水器控制面板脉冲长度编码协议 decoder
4. **rpm** — 转速（RPM）计算 decoder
5. **sae_j1850_vpw** — SAE J1850 VPW 汽车总线协议 decoder

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |


## 通用 C Decoder 实现规范

### 文件命名
- 文件路径：`libsigrokdecode/c_decoders/{decoder_id}_c.c`
- decoder id 中的 `-` 替换为 `_`
- 结构体命名：`{decoder_id}_c_decoder`

### 必须包含的头文件
```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"
```

### struct srd_c_decoder 字段规范
- `.id` = `"xxx_c"`（带 `_c` 后缀）
- `.name` = `"XXX(C)"`（带 `(C)` 后缀）
- `.longname` / `.desc` 保持与 Python 版本一致，末尾可加 `(C implementation)`

### ann_labels 规范
- 第一列必须为 `""`（空字符串），API 自动处理 i+7 偏移
- 格式：`static const char *xxx_ann_labels[][3] = { {"", "short", "Long"}, ... };`

### annotation_rows 规范
- 所有 annotation class 必须映射到 annotation_rows
- 使用 `static const int xxx_row_xxx_classes[] = {ANN_XX, ...};` 定义每行的 class 列表
- 使用 `static const struct srd_c_ann_row xxx_ann_rows[] = {...};` 定义行

### samplerate timing guard
- 必须实现 `metadata` callback 接收 `SRD_CONF_SAMPLERATE`
- 在 `decode()` 入口检查 samplerate 是否已设置，未设置时 fallback 或返回

### Condition Builder API 使用
```c
srd_cond_builder *cb = c_cond_new();
c_cond_rise(cb, channel_index);   // 上升沿
c_cond_fall(cb, channel_index);   // 下降沿
c_cond_edge(cb, channel_index);   // 任一边沿
c_cond_high(cb, channel_index);   // 高电平
c_cond_low(cb, channel_index);    // 低电平
c_cond_or(cb);                    // OR 分隔符
c_cond_skip(cb, sample_count);    // 跳过样本数
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
if (ret != SRD_OK) return;
```

<!-- Updated: c_cond_wait_current() 已实现，等效于 Python self.wait({}) / self.wait(None)，用于获取当前采样位置的引脚值 -->
```c
int ret = c_cond_wait_current(di, &samplenum);  // 等效 Python self.wait(None)
```

<!-- Updated: c_decoder_get_initial_pin() 已实现，用于获取解码开始前的初始引脚状态 -->
```c
uint8_t initial_val = c_decoder_get_initial_pin(di, channel_index);  // 返回 0xFF 表示未连接
```

### 输出 API
- `C_ANN_PUT(di, ss, es, out_ann, ann_class, ...)` — annotation 输出
- `C_ANN_PUT_VAL(di, ss, es, out_ann, ann_class, val, ...)` — 带数值的 annotation
- `c_decoder_put_python(di, ss, es, out_python, cmd, data, len)` — Python 层输出
- `c_decoder_put_binary(di, ss, es, out_binary, bin_class, size, data)` — 二进制输出
<!-- Updated: c_decoder_put_logic() 和 SRD_OUTPUT_LOGIC 已实现，用于输出逻辑信号数据给上层解码器 -->
- `c_decoder_put_logic(di, start_sample, end_sample, out_logic, channel_mask, values, num_channels)` — 逻辑信号输出（通过 `c_decoder_register_output(di, SRD_OUTPUT_LOGIC, ...)` 注册）

### Options 初始化
- 在 `srd_c_decoder_entry()` 中使用 `g_variant_new_*()` 初始化
- 字符串选项：`g_variant_new_string("default_value")`
- 整数选项：`g_variant_new_int64(default_value)`
- 枚举选项：使用 `GSList` + `g_slist_append()` 构建可选值列表

### Build 集成
- 将 decoder 名添加到 `CMakeLists.txt` 中的 `C_DECODERS` 列表

---

## 1. parallel — 通用并行同步总线

### 1.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `parallel` |
| name | `Parallel` |
| longname | `Parallel sync bus` |
| desc | `Generic parallel synchronous bus.` |
| inputs | `['logic']` |
| outputs | `['parallel']` |
| tags | `['Util']` |

**channels**: 无必需 channel

**optional_channels** (33个):
- `[0]`: `clk` / `CLK` / `Clock line`
- `[1-32]`: `d0`-`d31` / `D0`-`D31` / `Data line 0-31`

**options**:
| id | desc | default | values |
|----|------|---------|--------|
| `clock_edge` | Clock edge to sample on | `rising` | `('rising', 'falling')` |
| `wordsize` | Data wordsize (# bus cycles) | `0` | (integer) |
| `endianness` | Data endianness | `little` | `('little', 'big')` |

**annotations**:
| index | id | name |
|-------|----|------|
| 0 | items | Items |
| 1 | words | Words |

**annotation_rows**:
| id | name | classes |
|----|------|---------|
| items | Items | (0,) |
| words | Words | (1,) |

### 1.2 解码逻辑分析

#### 核心算法
1. **通道检测**：遍历 33 个 optional_channels，确定哪些已连接。至少需要 1 个 channel。`max_connected` = 已连接 channel 的最大索引。
2. **Clock 模式**：
   - 有 CLK 时：在 clock 的配置边沿（rising/falling）采样数据
   - 无 CLK 时：在任意数据线的任一边沿采样数据
3. **bitpack**：将连接的数据线（D0-Dn）打包为一个整数值 item
4. **Word 组装**：按 wordsize 选项收集 items，按 endianness 组装为 word
5. **延迟输出**：item/word 的 annotation 延迟到下一个采样点输出（因为需要 end samplenum）

#### 状态机
- 无显式状态机，使用 `while True` + `self.wait(conds)` 循环
- `is_first` 标志处理无 clock 模式下的首次采样
- `self.first` 标志控制延迟输出逻辑

#### 关键数据流
```
wait(conds) → 采样 pins → bitpack → item → handle_bits() → handle_word()
                                                      ↓
                                              延迟输出 item annotation
                                              延迟输出 word annotation
```

#### 边界情况
- 无 clock 模式：首次 `wait(None)` 获取初始值，之后 `wait([{idx: 'e'} for idx in has_channels])`
  <!-- Updated: Python self.wait(None) 在 C 中应使用 c_cond_wait_current(di, &samplenum)，该函数已实现 -->
- `end()` 回调：输出最后一个保存的 item 和 word
- `wordsize == 0`：不输出 word annotation
- 未连接的数据线视为 0

### 1.3 C 实现规划

#### 状态结构体
```c
#define PARALLEL_MAX_CHANNELS 33

struct parallel_priv {
    uint64_t samplerate;
    int have_clock;
    int clock_edge;       // 0=rising, 1=falling
    int wordsize;
    int endianness;       // 0=little, 1=big
    int num_item_bits;
    int max_connected;    // 最高连接 channel 索引

    int idx_channels[PARALLEL_MAX_CHANNELS]; // channel 索引映射, -1=未连接
    int has_channels[PARALLEL_MAX_CHANNELS]; // 已连接 channel 列表
    int num_has_channels;

    uint64_t prv_dex;     // 上一个采样点
    uint64_t saved_item;  // 保存的 item 值
    int has_saved_item;   // 是否有保存的 item

    uint64_t items[256];  // word 组装缓冲区
    int item_count;
    uint64_t saved_word;
    int has_saved_word;
    uint64_t ss_word;     // word 起始 sample
    uint64_t es_word;     // word 结束 sample

    int first;            // 首次标志
    int is_first_wait;    // 无 clock 首次 wait 标志

    int out_ann;
    int out_python;
};
```

#### Channel 定义
```c
// 33 个 optional channels: CLK + D0-D31
static struct srd_channel parallel_optional_channels[33];
// 在 srd_c_decoder_entry() 中动态初始化
```

**注意**：由于 C decoder 的 optional_channels 是静态数组，需要在 `srd_c_decoder_entry()` 中初始化：
```c
parallel_optional_channels[0].id = "clk";
parallel_optional_channels[0].name = "CLK";
parallel_optional_channels[0].desc = "Clock line";
parallel_optional_channels[0].order = 0;
parallel_optional_channels[0].type = SRD_CHANNEL_SCLK;
parallel_optional_channels[0].idn = NULL;
for (int i = 0; i < 32; i++) {
    char *id_buf = g_malloc_printf("d%d", i);
    char *name_buf = g_malloc_printf("D%d", i);
    char *desc_buf = g_malloc_printf("Data line %d", i);
    parallel_optional_channels[i+1].id = id_buf;
    parallel_optional_channels[i+1].name = name_buf;
    parallel_optional_channels[i+1].desc = desc_buf;
    parallel_optional_channels[i+1].order = i + 1;
    parallel_optional_channels[i+1].type = SRD_CHANNEL_SDATA;
    parallel_optional_channels[i+1].idn = NULL;
}
```

#### 关键实现 — bitpack
```c
static uint64_t parallel_bitpack(struct srd_decoder_inst *di,
                                  struct parallel_priv *s,
                                  uint64_t samplenum)
{
    uint64_t item = 0;
    int idx_strip = s->max_connected + 1;
    for (int i = 1; i < idx_strip; i++) {
        if (s->idx_channels[i] != -1) {
            int pin = c_decoder_get_pin(di, s->idx_channels[i], samplenum);
            item |= ((uint64_t)pin << (i - 1));
        }
        // 未连接的 channel 视为 0，无需操作
    }
    return item;
}
```

#### 关键实现 — decode 主循环
```c
static void parallel_decode(struct srd_decoder_inst *di)
{
    struct parallel_priv *s = (struct parallel_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched;
    int ret;

    // samplerate guard
    if (s->samplerate == 0) {
        s->samplerate = c_decoder_get_samplerate(di);
        if (s->samplerate == 0) return;
    }

    while (1) {
        srd_cond_builder *cb = c_cond_new();

        if (s->have_clock) {
            if (s->clock_edge == 0)
                c_cond_rise(cb, 0);  // CLK is channel 0
            else
                c_cond_fall(cb, 0);
        } else {
            if (s->is_first_wait) {
                // 首次无 clock：Python 用 self.wait(None) 获取初始值
                // C 中应使用 c_cond_wait_current() 等效实现
                // <!-- Updated: c_cond_wait_current() 已实现，等效于 Python self.wait({}) / self.wait(None) -->
                c_cond_wait_current(di, &samplenum);
                // 直接读取当前采样位置的引脚值，无需等待边沿
            } else {
                // 等待任意已连接数据线的边沿
                for (int i = 0; i < s->num_has_channels; i++) {
                    if (i > 0) c_cond_or(cb);
                    c_cond_edge(cb, s->has_channels[i]);
                }
            }
        }

        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;

        uint64_t item = parallel_bitpack(di, s, samplenum);

        if (!s->have_clock && s->is_first_wait) {
            s->is_first_wait = 0;
            s->saved_item = item;
            s->has_saved_item = 1;
            s->prv_dex = samplenum;
            continue;
        }

        // 输出保存的 item
        if (s->has_saved_item) {
            int num_digits = (s->num_item_bits + 3) / 4;
            char fmt_item[32];
            snprintf(fmt_item, sizeof(fmt_item), "@%%0%dX", num_digits);
            char item_str[32];
            snprintf(item_str, sizeof(item_str), fmt_item, s->saved_item);
            C_ANN_PUT(di, s->prv_dex, samplenum, s->out_ann, 0, item_str);
            // Python output
            unsigned char item_bytes[8];
            // ... pack item to bytes
            c_decoder_put_python(di, s->prv_dex, samplenum, s->out_python, "ITEM", item_bytes, sizeof(item_bytes));
        }

        s->saved_item = item;
        s->has_saved_item = 1;
        s->prv_dex = samplenum;

        // Word 组装
        parallel_handle_word(di, s, item, samplenum);
    }
}
```

#### metadata callback
```c
static void parallel_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    struct parallel_priv *s = (struct parallel_priv *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->samplerate = value;
    }
}
```

### 1.4 难度评估
- **复杂度：高** — 33 个 optional channels、动态 channel 映射、有/无 clock 两种模式、延迟输出逻辑、word 组装
- **主要挑战**：optional_channels 数组的动态初始化、无 clock 模式下的多条件 OR wait、bitpack 实现

---

## 2. pcfx-ctrlr — PC-FX 控制器协议

### 2.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `pcfx_cntrlr` |
| name | `PCFX Cntrlr` |
| longname | `PCFX Controller` |
| desc | `Controller protocol for NEC PC-FX videogame console` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Retro computing']` |

**channels** (3个):
| index | id | name | desc |
|-------|----|------|------|
| 0 | trigger | TRG | Trigger |
| 1 | clk | CLK | Clock |
| 2 | data | DATA | Data |

**optional_channels** (1个):
| index | id | name | desc |
|-------|----|------|------|
| 3 | dir | DIR | Data Direction |

**options**:
| id | desc | default | values |
|----|------|---------|--------|
| `bitvals` | Show bit values | `electrical` | `('electrical', 'internal')` |

**annotations** (12个):
| index | id | name |
|-------|----|------|
| 0 | start | Start |
| 1 | reset | Reset |
| 2 | bit | Bit |
| 3 | outbits | Outbound Bits |
| 4 | byte | Byte |
| 5 | word | Word |
| 6 | ctrldata | Controller Data |
| 7 | ctrlpad | Joypad Controller |
| 8 | ctrltap | Multitap Controller |
| 9 | ctrlmouse | Mouse Controller |
| 10 | ctrlunkn | Unknown Controller |
| 11 | warning | Warnings |

**annotation_rows** (6个):
| id | name | classes |
|----|------|---------|
| starts | Start | (0,1) |
| bits | Bits | (2,3) |
| bytes | Bytes | (4,) |
| words | Words | (5,) |
| controller | Controller | (6,7,8,9,10) |
| warnings | Warnings | (11,) |

### 2.2 解码逻辑分析

#### 状态机
```
FIND START → CHECK RESET → START BIT → END BIT → (循环32次) → FIND START
```

**FIND START**: 等待 TRG 下降沿 → 记录 startsamplenum，进入 CHECK RESET

**CHECK RESET**: 等待以下条件之一：
- `{0: 'l', 1: 'f'}` — TRG 低且 CLK 下降沿 → 这是 RESET joypad counter 事件
- `{0: 'r'}` — TRG 上升沿 → 正常触发完成

matched bit 0 触发时设置 `triggertype = 1`（Reset），否则 `triggertype = 0`（Trigger）。
输出 Start/Reset annotation，初始化 bit 计数器和数组，进入 START BIT。

**START BIT**: 等待 `{1: 'f'}` (CLK 下降沿) 或 `{0: 'f'}` (TRG 下降沿)
- CLK 下降沿匹配：读取 DATA 值，存入 bits_value（取反），进入 END BIT

**END BIT**: 等待 `{1: 'r'}` (CLK 上升沿) 或 `{0: 'f'}` (TRG 下降沿)
- CLK 上升沿匹配：记录 bit 结束位置，输出 bit annotation
- bitcount++ 后检查是否达到 32

**32 bits 完成后**：
1. 输出 4 个 byte annotation（每 8 bit 一组）
2. 输出 1 个 word annotation（32 bit）
3. 解析控制器类型（bits[28:31]）：
   - 15 → Joypad：输出按钮状态 (I, II, III, IV, V, VI, Sel, Run, Up, Right, Down, Left, Mode1, Mode2)
   - 14 → Multitap
   - 13 → Mouse：解析 X/Y 坐标和左右键
   - 其他 → Unknown

#### 关键函数
- `get_bitfield(start_bit, field_size)` — 从 bits_value 数组提取位域值
- `putbit(annot_type, value, bitnum, dispval)` — 输出单个按钮 annotation

#### 边界情况
- DIR channel 可选，影响 bit annotation 使用 class 2 还是 3
- bitvals 选项影响 bit 显示值（electrical vs internal/inverted）
- Mouse 的 Y/X 坐标使用补码：`value & 0x80` 时 `value = 0 - value`

### 2.3 C 实现规划

#### 状态枚举和结构体
```c
enum pcfx_state {
    STATE_FIND_START,
    STATE_CHECK_RESET,
    STATE_START_BIT,
    STATE_END_BIT,
};

struct pcfx_priv {
    uint64_t samplerate;
    int state;
    uint64_t startsamplenum;
    int triggertype;      // 0=normal, 1=reset
    uint64_t startbit;    // 当前 bit 的起始 sample
    int bitvalue;         // 实际电平值
    int dispbit;          // 显示值
    int have_direction;   // DIR channel 是否连接
    int dir;              // 方向值
    int bitcount;
    uint32_t bits_value[32]; // 32 bits 的内部值（取反后）
    uint64_t bits_start[32]; // 每个 bit 的起始 sample
    uint64_t bits_end[32];   // 每个 bit 的结束 sample
    int bitvals;          // 0=electrical, 1=internal
    int out_ann;
};
```

#### Channel 定义
```c
#define CH_TRG  0
#define CH_CLK  1
#define CH_DATA 2
#define CH_DIR  3

static struct srd_channel pcfx_channels[] = {
    {"trigger", "TRG", "Trigger", 0, SRD_CHANNEL_SDATA, NULL},
    {"clk", "CLK", "Clock", 1, SRD_CHANNEL_SCLK, NULL},
    {"data", "DATA", "Data", 2, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_channel pcfx_optional_channels[] = {
    {"dir", "DIR", "Data Direction", 3, SRD_CHANNEL_SDATA, NULL},
};
```

#### 关键实现 — get_bitfield
```c
static uint32_t pcfx_get_bitfield(struct pcfx_priv *s, int start_bit, int field_size)
{
    uint32_t value = 0;
    for (int i = 0; i < field_size; i++) {
        value |= (s->bits_value[start_bit + i] << i);
    }
    return value;
}
```

#### 关键实现 — 控制器解析（32 bits 完成后）
```c
static void pcfx_handle_complete(struct srd_decoder_inst *di, struct pcfx_priv *s, uint64_t samplenum)
{
    // 输出 4 个 byte
    for (int byteseq = 0; byteseq < 4; byteseq++) {
        int startbit = byteseq * 8;
        uint32_t value = pcfx_get_bitfield(s, startbit, 8);
        char buf[16];
        snprintf(buf, sizeof(buf), "0x%2.2X", value);
        C_ANN_PUT(di, s->bits_start[startbit], s->bits_end[startbit + 7], s->out_ann, 4, buf);
    }

    // 输出 word
    uint32_t word_val = pcfx_get_bitfield(s, 0, 32);
    char word_buf[16];
    snprintf(word_buf, sizeof(word_buf), "0x%8.8X", word_val);
    C_ANN_PUT(di, s->bits_start[0], s->bits_end[31], s->out_ann, 5, word_buf);

    // 控制器类型检测
    uint32_t ctrl_type = pcfx_get_bitfield(s, 28, 4);
    if (ctrl_type == 15) {
        // Joypad
        C_ANN_PUT(di, s->bits_start[28], s->bits_end[31], s->out_ann, 7, "Joypad");
        uint32_t btns = pcfx_get_bitfield(s, 0, 16);
        // 输出各按钮...
        pcfx_put_button(di, s, 6, btns, 0, "I");
        pcfx_put_button(di, s, 6, btns, 1, "II");
        // ... 其他按钮
    } else if (ctrl_type == 14) {
        C_ANN_PUT(di, s->bits_start[28], s->bits_end[31], s->out_ann, 8, "Multitap");
    } else if (ctrl_type == 13) {
        // Mouse
        C_ANN_PUT(di, s->bits_start[28], s->bits_end[31], s->out_ann, 9, "Mouse");
        // 解析 X/Y 坐标...
    } else {
        char buf[16];
        snprintf(buf, sizeof(buf), "(%d)", ctrl_type);
        C_ANN_PUT(di, s->bits_start[28], s->bits_end[31], s->out_ann, 10, buf);
    }
}
```

#### 关键实现 — decode 主循环
```c
static void pcfx_decode(struct srd_decoder_inst *di)
{
    struct pcfx_priv *s = (struct pcfx_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched;
    int ret;

    while (1) {
        switch (s->state) {

        case STATE_FIND_START: {
            srd_cond_builder *cb = c_cond_new();
            c_cond_fall(cb, CH_TRG);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;
            s->startsamplenum = samplenum;
            s->state = STATE_CHECK_RESET;
            s->triggertype = 0;
            break;
        }

        case STATE_CHECK_RESET: {
            srd_cond_builder *cb = c_cond_new();
            c_cond_low(cb, CH_TRG);
            c_cond_fall(cb, CH_CLK);
            c_cond_or(cb);
            c_cond_rise(cb, CH_TRG);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;

            if (matched & 0x1) {
                s->triggertype = 1;
            }
            // 两个条件都进入 START BIT
            if (s->triggertype == 0) {
                C_ANN_PUT(di, s->startsamplenum, samplenum, s->out_ann, 0,
                          "Trigger", "Trig", "T");
            } else {
                C_ANN_PUT(di, s->startsamplenum, samplenum, s->out_ann, 1,
                          "Reset Joy Count", "Reset", "R");
            }
            s->bitcount = 0;
            s->startbit = samplenum;
            s->state = STATE_START_BIT;
            break;
        }

        case STATE_START_BIT: {
            srd_cond_builder *cb = c_cond_new();
            c_cond_fall(cb, CH_CLK);
            c_cond_or(cb);
            c_cond_fall(cb, CH_TRG);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;

            if (matched & 0x1) {  // CLK fall
                s->bitvalue = c_decoder_get_pin(di, CH_DATA, samplenum);
                s->bits_value[s->bitcount] = 1 - s->bitvalue;  // 内部值取反
                s->bits_start[s->bitcount] = s->startbit;
                s->state = STATE_END_BIT;
            }
            // TRG fall = framing error, 忽略
            break;
        }

        case STATE_END_BIT: {
            srd_cond_builder *cb = c_cond_new();
            c_cond_rise(cb, CH_CLK);
            c_cond_or(cb);
            c_cond_fall(cb, CH_TRG);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;

            if (matched & 0x1) {  // CLK rise
                s->bits_end[s->bitcount] = samplenum;
                int disp = (s->bitvals == 0) ? s->bitvalue : (1 - s->bitvalue);
                int ann_class = (s->have_direction && s->dir == 1) ? 3 : 2;
                char bit_str[2] = {disp ? '1' : '0', '\0'};
                C_ANN_PUT(di, s->startbit, samplenum, s->out_ann, ann_class, bit_str);
            }

            s->startbit = samplenum;
            s->bitcount++;

            if (s->bitcount == 32) {
                pcfx_handle_complete(di, s, samplenum);
                s->state = STATE_FIND_START;
            } else {
                s->state = STATE_START_BIT;
            }
            break;
        }

        }
    }
}
```

### 2.4 难度评估
- **复杂度：中高** — 多状态机、32-bit 数据收集、控制器类型解析（Joypad/Mouse/Multitap）
- **主要挑战**：多条件 OR wait、DIR optional channel 处理、控制器类型分支逻辑

---

## 3. rinnai-control-panel — Rinnai 控制面板脉冲编码

### 3.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `rinnai-control-panel` |
| name | `Rinnai Control Panel` |
| longname | `Rinnai control panel internal pulse length encoding protocol` |
| desc | `Bidirectional, half-duplex, asynchronous serial bus.` |
| inputs | `['logic']` |
| outputs | `['rinnai']` |
| tags | `['Embedded/industrial']` |

**channels** (1个):
| index | id | name | desc |
|-------|----|------|------|
| 0 | data | Data | Pulse length signal line |

**options**:
| id | desc | default | values |
|----|------|---------|--------|
| `invert` | Invert bits | `no` | `('yes', 'no')` |
| `bit_numbering` | Bit numbering, first | `lsb` | `('lsb', 'msb')` |

**annotations** (5个):
| index | id | name |
|-------|----|------|
| 0 | bit | Bit |
| 1 | warning | Warning |
| 2 | reset | Reset |
| 3 | byte | Byte |
| 4 | packet | Packet |

**annotation_rows** (4个):
| id | name | classes |
|----|------|---------|
| bits | Bits | (0, 2) |
| warnings | Warnings | (1,) |
| bytes | Bytes | (3,) |
| packets | Packets | (4,) |

### 3.2 解码逻辑分析

#### 核心常量
```python
SYMBOL_DURATION_US = 600
SYMBOL_SHORT_PERIOD_RATIO_MIN = 0.15  # 90us
SYMBOL_SHORT_PERIOD_RATIO_MAX = 0.35  # 210us
SYMBOL_LONG_PERIOD_RATIO_MIN = 0.65   # 390us
SYMBOL_LONG_PERIOD_RATIO_MAX = 0.85   # 510us
RESET_RATIO_MIN = 1                    # 600us
RESET_RATIO_MAX = 2                    # 1200us
```

#### 状态机
```
INITIAL → IDLE → PRE → SYMBOL → (循环) → PRE (reset时)
                ↑         |
                +---------+ (bad bit → IDLE)
```

**INITIAL**: 等待 data 低电平 → 记录 fall，进入 IDLE

**IDLE**: 等待 data 上升沿 → 记录 rise，计算低电平时间，进入 PRE

**PRE**: 等待 data 下降沿 → 记录 fall，计算高电平时间：
- 时间在 RESET_RATIO_MIN*600 ~ RESET_RATIO_MAX*600 us → Reset pulse，输出 Reset annotation，进入 SYMBOL，flush bytes
- 其他 → Bad pre warning，回到 IDLE，flush bytes

**SYMBOL**: 等待 data 上升沿 → 记录 rise，再等待 data 下降沿 → 计算两个时间片：
- `timeA` = rise - fall（低电平时间）
- `timeB` = fall_new - rise（高电平时间）

判断逻辑：
- `timeA` 短 + `timeB` 长 → bit = 1（或 0 如果 invert）
- `timeA` 长 + `timeB` 短 → bit = 0（或 1 如果 invert）
- `timeB` 在 reset 范围 → 新的 reset，flush
- 其他 → Bad bit warning，回到 IDLE，flush

#### Bit/Byte/Packet 组装
- `bit_append()`: 输出 bit annotation，按 bit_numbering 组装 byte
- 8 bits 完成 → `byte_append()`: 输出 byte annotation + Python output
- `bytes_flush()`: 输出 packet annotation（所有 bytes 的 hex 逗号分隔）

#### 边界情况
- invert 选项反转 bit 值
- lsb/msb 选项影响 byte 组装顺序
- Reset pulse 既是同步信号也是 packet 分隔符

### 3.3 C 实现规划

#### 状态枚举和结构体
```c
enum rinnai_state {
    STATE_INITIAL,
    STATE_IDLE,
    STATE_PRE,
    STATE_SYMBOL,
};

#define SYMBOL_DURATION_US       600
#define SHORT_RATIO_MIN          0.15
#define SHORT_RATIO_MAX          0.35
#define LONG_RATIO_MIN           0.65
#define LONG_RATIO_MAX           0.85
#define RESET_RATIO_MIN          1.0
#define RESET_RATIO_MAX          2.0

#define MAX_PACKET_BYTES         64

struct rinnai_priv {
    uint64_t samplerate;
    int state;
    uint64_t fall;
    uint64_t rise;
    int invert;
    int lsb_first;

    int bit_count;
    uint8_t byte_val;
    uint64_t byte_start;

    uint8_t bytes[MAX_PACKET_BYTES];
    int byte_count;
    uint64_t packet_start;

    int out_ann;
    int out_python;
};
```

#### 关键实现 — 时间计算
```c
static double rinnai_samples_to_us(struct rinnai_priv *s, uint64_t samples)
{
    return (samples * 1000000.0) / s->samplerate;
}
```

#### 关键实现 — decode 主循环
```c
static void rinnai_decode(struct srd_decoder_inst *di)
{
    struct rinnai_priv *s = (struct rinnai_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched;
    int ret;

    if (s->samplerate == 0) {
        s->samplerate = c_decoder_get_samplerate(di);
        if (s->samplerate == 0) return;
    }

    while (1) {
        srd_cond_builder *cb;
        switch (s->state) {

        case STATE_INITIAL: {
            cb = c_cond_new();
            c_cond_low(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;
            s->fall = samplenum;
            s->state = STATE_IDLE;
            break;
        }

        case STATE_IDLE: {
            cb = c_cond_new();
            c_cond_rise(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;
            s->rise = samplenum;
            s->state = STATE_PRE;
            break;
        }

        case STATE_PRE: {
            cb = c_cond_new();
            c_cond_fall(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;
            double time_us = rinnai_samples_to_us(s, samplenum - s->rise);
            if (time_us > RESET_RATIO_MIN * SYMBOL_DURATION_US &&
                time_us < RESET_RATIO_MAX * SYMBOL_DURATION_US) {
                char buf[32];
                snprintf(buf, sizeof(buf), "Reset: %d", (int)time_us);
                C_ANN_PUT(di, s->rise, samplenum, s->out_ann, 2, buf);
                s->state = STATE_SYMBOL;
                rinnai_bytes_flush(di, s, samplenum);
            } else {
                char buf[32];
                snprintf(buf, sizeof(buf), "Bad pre: %d", (int)time_us);
                C_ANN_PUT(di, s->rise, samplenum, s->out_ann, 1, buf);
                s->state = STATE_IDLE;
                rinnai_bytes_flush(di, s, samplenum);
            }
            s->fall = samplenum;
            break;
        }

        case STATE_SYMBOL: {
            cb = c_cond_new();
            c_cond_rise(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;
            s->rise = samplenum;

            cb = c_cond_new();
            c_cond_fall(cb, 0);
            ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;

            double timeA = rinnai_samples_to_us(s, s->rise - s->fall);
            double timeB = rinnai_samples_to_us(s, samplenum - s->rise);

            if (timeA > SHORT_RATIO_MIN * SYMBOL_DURATION_US &&
                timeA < SHORT_RATIO_MAX * SYMBOL_DURATION_US &&
                timeB > LONG_RATIO_MIN * SYMBOL_DURATION_US &&
                timeB < LONG_RATIO_MAX * SYMBOL_DURATION_US) {
                int bit = s->invert ? 0 : 1;
                rinnai_bit_append(di, s, s->fall, samplenum, bit);
            } else if (timeB > SHORT_RATIO_MIN * SYMBOL_DURATION_US &&
                       timeB < SHORT_RATIO_MAX * SYMBOL_DURATION_US &&
                       timeA > LONG_RATIO_MIN * SYMBOL_DURATION_US &&
                       timeA < LONG_RATIO_MAX * SYMBOL_DURATION_US) {
                int bit = s->invert ? 1 : 0;
                rinnai_bit_append(di, s, s->fall, samplenum, bit);
            } else if (timeB > RESET_RATIO_MIN * SYMBOL_DURATION_US &&
                       timeB < RESET_RATIO_MAX * SYMBOL_DURATION_US) {
                rinnai_bits_reset(s);
                char buf[32];
                snprintf(buf, sizeof(buf), "Reset: %d", (int)timeB);
                C_ANN_PUT(di, s->rise, samplenum, s->out_ann, 2, buf);
                rinnai_bytes_flush(di, s, s->fall);
            } else {
                rinnai_bits_reset(s);
                char buf[64];
                snprintf(buf, sizeof(buf), "Bad Bit: %d,%d", (int)timeA, (int)timeB);
                C_ANN_PUT(di, s->fall, samplenum, s->out_ann, 1, buf);
                s->state = STATE_IDLE;
                rinnai_bytes_flush(di, s, s->fall);
            }
            s->fall = samplenum;
            break;
        }

        }
    }
}
```

### 3.4 难度评估
- **复杂度：中** — 类似 onewire 的脉冲长度编码，4 状态 FSM
- **主要挑战**：精确的时间比例计算、bit/byte/packet 三级组装、invert 和 bit_numbering 选项

---

## 4. rpm — 转速计算

### 4.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `rpm` |
| name | `RPM` |
| longname | `Revolutions per minute` |
| desc | `Calculate the number of turns in one minute.` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Util']` |

**channels** (1个):
| index | id | name | desc |
|-------|----|------|------|
| 0 | data | Data | Data line |

**options**:
| id | desc | default | values |
|----|------|---------|--------|
| `num_pulses` | Number of pulses per revolution | `2` | (integer) |
| `edge` | Edges to check | `falling` | `('rising', 'falling')` |

**annotations** (1个):
| index | id | name |
|-------|----|------|
| 0 | rpm | RPM |

**annotation_rows** (1个):
| id | name | classes |
|----|------|---------|
| rpms | RPM | (0,) |

### 4.2 解码逻辑分析

#### 核心算法
```
RPM = 1000 / (t * 1000) * 60 = 60000 / (t * 1000) = 60 / t
```
其中 `t` 是相邻 N 个 edge 之间的时间（秒），N = num_pulses。

#### 流程
1. 等待配置的 edge（rising/falling）
2. 记录首次 edge 的 samplenum
3. 后续 edge：
   - 计算与上次 edge 的时间差 `t = samples / samplerate`
   - 如果 `t >= 0.5` 秒：重置计数器（太长的间隔视为无效）
   - 否则递增 edge_num
   - 当 `edge_num == num_pulses` 时：计算 RPM 并输出 annotation，重置计数器

#### 边界情况
- 首次 edge 只记录位置，不计算
- 间隔 ≥ 0.5 秒的 edge 重置计数器
- samplerate 必须已知

### 4.3 C 实现规划

#### 状态结构体
```c
struct rpm_priv {
    uint64_t samplerate;
    uint64_t last_samplenum;
    int edge_num;
    int edge_type;       // 0=rising, 1=falling
    int num_pulses;
    int out_ann;
};
```

#### 关键实现 — decode
```c
static void rpm_decode(struct srd_decoder_inst *di)
{
    struct rpm_priv *s = (struct rpm_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched;
    int ret;

    if (s->samplerate == 0) {
        s->samplerate = c_decoder_get_samplerate(di);
        if (s->samplerate == 0) return;
    }

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        if (s->edge_type == 0)
            c_cond_rise(cb, 0);
        else
            c_cond_fall(cb, 0);
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;

        if (s->last_samplenum == 0) {
            s->last_samplenum = samplenum;
            continue;
        }

        s->edge_num++;
        double t = (double)(samplenum - s->last_samplenum) / s->samplerate;

        if (t >= 0.5) {
            s->edge_num = 0;
            s->last_samplenum = samplenum;
            continue;
        }

        if (s->edge_num == s->num_pulses) {
            s->edge_num = 0;
            int rpm = (int)(60.0 / t);
            char buf[32];
            snprintf(buf, sizeof(buf), "%d", rpm);
            C_ANN_PUT_VAL(di, s->last_samplenum, samplenum, s->out_ann, 0, rpm, buf);
            s->last_samplenum = samplenum;
        }
    }
}
```

### 4.4 难度评估
- **复杂度：低** — 最简单的 decoder 之一，单 channel、单 annotation、简单计算
- **主要挑战**：几乎无，注意 samplerate guard 和边界条件

---

## 5. sae_j1850_vpw — SAE J1850 VPW 汽车总线

### 5.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `sae_j1850_vpw` |
| name | `SAE J1850 VPW` |
| longname | `SAE J1850 VPW.` |
| desc | `SAE J1850 Variable Pulse Width 1x and 4x.` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Automotive']` |

**channels** (1个):
| index | id | name | desc |
|-------|----|------|------|
| 0 | data | Data | Data line |

**annotations** (5个):
| index | id | name |
|-------|----|------|
| 0 | raw | Raw |
| 1 | sof | SOF |
| 2 | ifs | EOF/IFS |
| 3 | data | Data |
| 4 | packet | Packet |

**annotation_rows** (3个):
| id | name | classes |
|----|------|---------|
| raws | Raws | (0, 1, 2) |
| bytes | Bytes | (3,) |
| packets | Packets | (4,) |

### 5.2 解码逻辑分析

#### VPW 时序参数（微秒）
```
sof   = 200,  sof_l = 164,  sof_h = 245
long  = 128,  long_l = 97,  long_h = 170
short = 64,   short_l = 24, short_h = 97
ifs   = 240
spd   = 1 (1x) 或 4 (4x)
```

#### 状态机
```
IDLE → (检测 SOF) → DATA → (检测 EOF/IFS) → IDLE
```

**IDLE**:
- 等待 data 边沿
- 检测脉冲宽度：
  - active 电平，宽度在 sof_l~sof_h → 1X SOF，spd=1，进入 DATA
  - active 电平，宽度在 sof_l/4~sof_h/4 → 4X SOF，spd=4，进入 DATA

**DATA**:
- 等待 data 边沿
- 检测脉冲宽度（除以 spd）：
  - ≥ ifs/spd → EOF/IFS，输出 Checksum annotation，回到 IDLE
  - short_l/spd ~ short_h/spd：
    - active 电平 → bit=1
    - 非active 电平 → bit=0
  - long_l/spd ~ long_h/spd：
    - active 电平 → bit=0
    - 非active 电平 → bit=1

#### Bit/Byte/Packet 组装
- `handle_bit()`: MSB-first 组装 byte，8 bits 完成后输出 Data annotation
- Byte 0 = Priority, Byte 1 = Destination, Byte 2 = Source, Byte 3 = Mode
- Mode 1 时 Byte 4 = PID
- 最后一个 byte = Checksum（回溯标记）

#### 关键：active 电平
- `self.active = 0` — 初始值
- SOF 脉冲在 active 电平上检测
- bit 解码依赖 pin 是否等于 active

#### 边界情况
- 4x 模式下所有时序参数除以 4
- `sof_h = 245`（spec 是 240，但放宽以兼容 60us 4x 采样）
- `short_l = 24`（spec 是 35，但放宽以兼容实际 4x@1MHz 采样）
- `long_h = 170`（spec 是 164，但放宽以兼容低采样率）
- Checksum 是回溯标记（retrospective annotation）

### 5.3 C 实现规划

#### 状态枚举和结构体
```c
enum vpw_state {
    STATE_IDLE,
    STATE_DATA,
};

struct vpw_priv {
    uint64_t samplerate;
    int state;
    int active;           // active logic level
    int spd;              // 1 or 4
    uint8_t byte_val;
    int bit_count;
    uint64_t datastart;
    int byte_count;       // byte offset in packet
    int mode;             // mode byte value
    uint64_t csa;         // checksum byte start sample
    uint64_t csb;         // checksum byte end sample
    int out_ann;
};
```

#### VPW 时序常量（微秒）
```c
#define VPW_SOF_US         200
#define VPW_SOF_L_US       164
#define VPW_SOF_H_US       245
#define VPW_LONG_US        128
#define VPW_LONG_L_US      97
#define VPW_LONG_H_US      170
#define VPW_SHORT_US       64
#define VPW_SHORT_L_US     24
#define VPW_SHORT_H_US     97
#define VPW_IFS_US         240
```

#### 辅助函数 — us 转换
```c
static uint64_t vpw_us_to_samples(struct vpw_priv *s, int us)
{
    return (uint64_t)us * s->samplerate / 1000000;
}

static int vpw_samples_to_us(struct vpw_priv *s, uint64_t samples)
{
    return (int)(samples * 1000000 / s->samplerate);
}
```

#### 关键实现 — handle_bit
```c
static void vpw_handle_bit(struct srd_decoder_inst *di, struct vpw_priv *s,
                            uint64_t ss, uint64_t es, int bit)
{
    s->byte_val |= (bit << (7 - s->bit_count));  // MSB-first
    char bit_str[2] = {bit ? '1' : '0', '\0'};
    C_ANN_PUT(di, ss, es, s->out_ann, 0, bit_str);

    if (s->bit_count == 0)
        s->datastart = ss;

    if (s->bit_count == 7) {
        s->csa = s->datastart;
        s->csb = es;
        char byte_str[8];
        snprintf(byte_str, sizeof(byte_str), "%02X", s->byte_val);
        C_ANN_PUT(di, s->datastart, es, s->out_ann, 3, byte_str);

        // Packet field annotation
        if (s->byte_count == 0) {
            C_ANN_PUT(di, s->datastart, es, s->out_ann, 4, "Priority", "Prio", "P");
        } else if (s->byte_count == 1) {
            C_ANN_PUT(di, s->datastart, es, s->out_ann, 4, "Destination", "Dest", "D");
        } else if (s->byte_count == 2) {
            C_ANN_PUT(di, s->datastart, es, s->out_ann, 4, "Source", "Src", "S");
        } else if (s->byte_count == 3) {
            C_ANN_PUT(di, s->datastart, es, s->out_ann, 4, "Mode", "M");
            s->mode = s->byte_val;
        } else if (s->mode == 1 && s->byte_count == 4) {
            C_ANN_PUT(di, s->datastart, es, s->out_ann, 4, "Pid", "P");
        }

        s->bit_count = -1;
        s->byte_val = 0;
        s->byte_count++;
    }
    s->bit_count++;
}
```

#### 关键实现 — decode 主循环
```c
static void vpw_decode(struct srd_decoder_inst *di)
{
    struct vpw_priv *s = (struct vpw_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0;
    uint64_t matched;
    int ret;

    if (s->samplerate == 0) {
        s->samplerate = c_decoder_get_samplerate(di);
        if (s->samplerate == 0) return;
    }

    // 等待第一个边沿
    srd_cond_builder *cb = c_cond_new();
    c_cond_edge(cb, 0);
    ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;

    uint64_t es = samplenum;

    while (1) {
        uint64_t ss = es;
        cb = c_cond_new();
        c_cond_edge(cb, 0);
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;
        es = samplenum;

        uint64_t samples = es - ss;
        int t = vpw_samples_to_us(s, samples);
        int pin = c_decoder_get_pin(di, 0, ss);  // 脉冲起始时的电平

        if (s->state == STATE_IDLE) {
            if (pin == s->active && t >= VPW_SOF_L_US && t <= VPW_SOF_H_US) {
                C_ANN_PUT(di, ss, es, s->out_ann, 0, "1X SOF", "S1", "S");
                s->spd = 1;
                s->byte_val = 0;
                s->bit_count = 0;
                s->byte_count = 0;
                s->state = STATE_DATA;
            } else if (pin == s->active &&
                       t >= VPW_SOF_L_US / 4 && t <= VPW_SOF_H_US / 4) {
                C_ANN_PUT(di, ss, es, s->out_ann, 0, "4X SOF", "S4", "4");
                s->spd = 4;
                s->byte_val = 0;
                s->bit_count = 0;
                s->byte_count = 0;
                s->state = STATE_DATA;
            }
        } else if (s->state == STATE_DATA) {
            int t_scaled = t / s->spd;
            if (t_scaled >= VPW_IFS_US / s->spd) {
                s->state = STATE_IDLE;
                C_ANN_PUT(di, ss, es, s->out_ann, 0, "EOF/IFS", "E");
                C_ANN_PUT(di, s->csa, s->csb, s->out_ann, 4, "Checksum", "CS", "C");
                s->byte_count = 0;
            } else if (t_scaled >= VPW_SHORT_L_US / s->spd &&
                       t_scaled <= VPW_SHORT_H_US / s->spd) {
                int bit = (pin == s->active) ? 1 : 0;
                vpw_handle_bit(di, s, ss, es, bit);
            } else if (t_scaled >= VPW_LONG_L_US / s->spd &&
                       t_scaled <= VPW_LONG_H_US / s->spd) {
                int bit = (pin == s->active) ? 0 : 1;
                vpw_handle_bit(di, s, ss, es, bit);
            }
        }
    }
}
```

**注意**：VPW 的 `t` 比较逻辑需要仔细处理。Python 代码使用 `t in range(low, high)`，即 `[low, high)` 区间。C 实现应使用 `t >= low && t < high` 或 `t >= low && t <= high`（与 Python range 语义一致用 `<`，但 Python 代码中的 range 实际上对整数有效，而 t 是整数微秒值）。

### 5.4 难度评估
- **复杂度：中** — 单 channel、2 状态 FSM、VPW 时序参数、1x/4x 双速模式
- **主要挑战**：VPW 时序范围判断、active 电平概念、Checksum 回溯标记、1x/4x 速度切换

---

## 附录 A：C Decoder 模板

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

enum xxx_ann {
    ANN_XXX = 0,
    NUM_ANN,
};

struct xxx_priv {
    uint64_t samplerate;
    int state;
    int out_ann;
};

static struct srd_channel xxx_channels[] = {
    {"data", "Data", "Data line", 0, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_decoder_option xxx_options[] = {
    {"option_id", NULL, "Option description", NULL, NULL},
};

static const char *xxx_ann_labels[][3] = {
    {"", "short", "Long"},
};

static const int xxx_row_xxx_classes[] = {ANN_XXX};
static const struct srd_c_ann_row xxx_ann_rows[] = {
    {"row_id", "Row name", xxx_row_xxx_classes, 1},
};

static const char *xxx_inputs[] = {"logic", NULL};
static const char *xxx_outputs[] = {NULL};
static const char *xxx_tags[] = {"Tag", NULL};

static void xxx_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(struct xxx_priv)));
    }
    struct xxx_priv *s = (struct xxx_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(struct xxx_priv));
}

static void xxx_start(struct srd_decoder_inst *di)
{
    struct xxx_priv *s = (struct xxx_priv *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "xxx");
}

static void xxx_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    struct xxx_priv *s = (struct xxx_priv *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE)
        s->samplerate = value;
}

static void xxx_decode(struct srd_decoder_inst *di)
{
    struct xxx_priv *s = (struct xxx_priv *)c_decoder_get_private(di);
    // ... decode logic
}

static void xxx_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}

struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    .longname = "XXX (C)",
    .desc = "XXX decoder (C implementation)",
    .license = "gplv2+",
    .channels = xxx_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = xxx_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = 1,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,
    .num_inputs = 1,
    .outputs = xxx_outputs,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = xxx_tags,
    .num_tags = 1,
    .reset = xxx_reset,
    .start = xxx_start,
    .metadata = xxx_metadata,
    .decode = xxx_decode,
    .destroy = xxx_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    xxx_options[0].idn = "dec_xxx_opt_option_id";
    xxx_options[0].def = g_variant_new_string("default");
    GSList *vals = NULL;
    vals = g_slist_append(vals, g_variant_new_string("val1"));
    vals = g_slist_append(vals, g_variant_new_string("val2"));
    xxx_options[0].values = vals;
    return &xxx_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
```

## 附录 B：CMakeLists.txt 修改

在 `CMakeLists.txt` 第 837 行的 `C_DECODERS` 列表末尾添加 5 个新 decoder：

```
set(C_DECODERS ... parallel_c pcfx_ctrlr_c rinnai_control_panel_c rpm_c sae_j1850_vpw_c)
```

**注意**：文件名中 `-` 替换为 `_`，即：
- `parallel_c` → `libsigrokdecode/c_decoders/parallel_c.c`
- `pcfx_ctrlr_c` → `libsigrokdecode/c_decoders/pcfx_ctrlr_c.c`
- `rinnai_control_panel_c` → `libsigrokdecode/c_decoders/rinnai_control_panel_c.c`
- `rpm_c` → `libsigrokdecode/c_decoders/rpm_c.c`
- `sae_j1850_vpw_c` → `libsigrokdecode/c_decoders/sae_j1850_vpw_c.c`

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
