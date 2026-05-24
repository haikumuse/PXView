# Python 解码器移植为 C 解码器 — Batch 06 详细规格

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |

## 概述

本文档描述将 5 个 Python 协议解码器移植为 C 解码器的完整规格。目标文件路径为 `libsigrokdecode/c_decoders/<name>_c.c`。

5 个解码器：
1. **dcc** — Digital Command Control（数字命令控制，模型铁路）
2. **delta-sigma** — Delta-Sigma 解码器（时钟同步数据流滤波）
3. **dsi** — Digital Serial Interface（数字串行接口，照明协议）
4. **em4100** — RFID EM4100 协议
5. **em4305** — RFID EM4205/EM4305 协议

---

## 通用 C 解码器模板结构

每个 C 解码器文件必须遵循以下结构：

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <glib.h>
#include "libsigrokdecode.h"

// 1. 枚举定义（状态机、注解类型）
// 2. 私有状态结构体
// 3. 通道定义数组
// 4. 选项定义数组
// 5. 注解标签数组
// 6. 注解行定义
// 7. 输入/输出/标签数组
// 8. reset() 函数
// 9. start() 函数
// 10. metadata() 函数（如需要）
// 11. decode() 函数
// 12. srd_c_decoder 结构体导出
// 13. SRD_C_DECODER_EXPORT 入口函数
```

### 关键 API 使用说明

- **条件等待**：使用 `srd_cond_builder` 系列 API（`c_cond_new`, `c_cond_rise`, `c_cond_fall`, `c_cond_edge`, `c_cond_skip`, `c_cond_or`, `c_cond_wait`, `c_cond_free`）
- **无条件等待**：`c_cond_wait_current(di, &samplenum)` — 等效 Python `self.wait({})`，获取当前采样位置 <!-- Updated: c_cond_wait_current已实现 -->
- **注解输出**：使用 `C_ANN_PUT(di, ss, es, out_id, cls, ...)` 宏
- **带数值注解**：使用 `C_ANN_PUT_VAL(di, ss, es, out_id, cls, val, ...)` 宏
- **获取采样率**：`c_decoder_get_samplerate(di)`
- **获取选项**：`c_decoder_get_option_int()`, `c_decoder_get_option_double()`, `c_decoder_get_option_string()`
- **通道检测**：`c_decoder_has_channel(di, ch)`
- **获取引脚值**：`c_decoder_get_pin(di, ch, samplenum)`
- **获取初始引脚值**：`c_decoder_get_initial_pin(di, ch)` — 等效 Python `self.oldpin`，读取数据开始前的引脚状态 <!-- Updated: c_decoder_get_initial_pin已实现 -->
- **私有数据**：`c_decoder_get_private(di)` / `c_decoder_set_private(di, ptr)`
- **注册输出**：`c_decoder_register_output(di, SRD_OUTPUT_ANN, "proto_id")`
- **Logic输出**：`c_decoder_put_logic(di, ss, es, out_id, data, num_samples)` — 用于 `SRD_OUTPUT_LOGIC` 类型输出 <!-- Updated: c_decoder_put_logic已实现 -->

---

## 1. DCC 解码器

### 1.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `'dcc'` |
| name | `'DCC'` |
| longname | `'Digital Command Control'` |
| desc | `'DCC protocol (operate model railways digitally)'` |
| license | `'gplv2+'` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Encoding']` |

### 1.2 通道

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | data | D0 | Data line | (无) |

无可选通道。

### 1.3 选项

| id | desc | default | values | C类型 |
|----|------|---------|--------|-------|
| CV_29_1 | CV29 Bit 1 | `'1: 28/128 speed mode'` | `('1: 28/128 speed mode', '0: 14 speed mode')` | string |
| Mode_112_127 | addr. 112-127 | `'operation mode'` | `('operation mode', 'service mode')` | string |
| Addr_offset | accessory addr. offset | `0` | — | int |
| Search_acc_addr | search acc. addr. [dec] | `''` | — | string→int |
| Search_dec_addr | search dec. addr. [dec] | `''` | — | string→int |
| Search_cv | search CV [dec] | `''` | — | string→int |
| Search_byte | search byte [dec/0b/0x] | `''` | — | string→int |
| Ignore_short_pulse | ignore pulse <= 4 µs | `'no'` | `('no', 'yes')` | string |

**选项解析说明**：
- `CV_29_1`：若为 `'0: 14 speed mode'` 则 `speed14 = true`
- `Mode_112_127`：若为 `'service mode'` 则 `serviceMode = true`
- `Addr_offset`：直接整数
- `Search_acc_addr`：解析为 int，范围 1-2047，否则 -2
- `Search_dec_addr`：解析为 int，范围 0-10239，否则 -2
- `Search_cv`：解析为 int，范围 1-16777216，否则 -2
- `Search_byte`：先尝试 base=10，再 base=2，再 base=16 解析，范围 0-255，否则 -2
- `Ignore_short_pulse`：若为 `'yes'` 则启用短脉冲过滤

### 1.4 注解

| 枚举值 | id | desc |
|--------|-----|------|
| 0 | bits1 | Bits |
| 1 | bits2 | Other |
| 2 | frame1 | Frame |
| 3 | frame2 | Other |
| 4 | data1 | Data |
| 5 | data2 | Accessory address |
| 6 | data3 | Decoder address |
| 7 | data4 | CV |
| 8 | command | Command |
| 9 | error | Error |
| 10 | search1 | Accessory address |
| 11 | search2 | Decoder address |
| 12 | search3 | CV |
| 13 | search4 | Byte |

### 1.5 注解行

| id | label | 包含的注解类 |
|----|-------|-------------|
| bits_ | Bits | (0, 1) |
| frame_ | Frame | (2, 3) |
| data_ | Data | (5, 6, 7, 4) |
| command_ | Command | (8,) |
| error_ | Error | (9,) |
| search_ | Search | (10, 11, 12, 13) |

### 1.6 decode() 逻辑分析

#### 状态机

DCC 解码器使用字符串状态名：
- `WAITINGFORPREAMBLE` — 等待前导码
- `PREAMBLE` — 收集前导码位
- `ADDRESSDATABYTE` — 收集地址/数据字节

#### 核心流程

1. **初始化**：需要 samplerate，最低 25kHz。计算 accuracy = 1/samplerate * 1000000 (µs)
2. **边沿检测**：先等待上升沿(edge_1)，再等待下降沿(edge_2)，输出采样率信息
3. **主循环**：
   - 等待上升沿(edge_3)，再等待下降沿(edge_4)
   - 计算 total = (edge_3 - edge_1) / samplerate * 1000000 µs
   - 计算 part1 = (edge_2 - edge_1) / samplerate * 1000000 µs
   - 计算 part2 = (edge_3 - edge_2) / samplerate * 1000000 µs
4. **位值判定**（RCN-210 5）：
   - `'1'`：52-accuracy ≤ part1 ≤ 64+accuracy AND 52-accuracy ≤ part2 ≤ 64+accuracy AND |part1-part2| ≤ max(6, 2*accuracy)
   - `'0'`：(90-accuracy ≤ part1 ≤ 10000+accuracy AND 90-accuracy ≤ part2 ≤ 119+accuracy) OR 反转
   - 半'0'+半'1'：90+52-accuracy ≤ total ≤ 64+119+accuracy → 切换边沿检测方向
   - 其他：unknown timing
5. **短脉冲过滤**（可选）：若启用，忽略 ≤ 4µs 的脉冲
6. **Railcom cutout 检测**：454-accuracy ≤ total ≤ 488+119+6+accuracy
7. **collectDataBytes()**：根据位值收集字节
   - `WAITINGFORPREAMBLE`：等待第一个 '1'
   - `PREAMBLE`：收集 '1' 位，遇到 '0' 时检查是否 ≥10 位前导码
   - `ADDRESSDATABYTE`：收集 8 位数据字节 + 1 位分隔符/结束符
8. **handleDecodedBytes()**：解析完整数据包
   - 校验和验证（XOR 所有字节）
   - 根据 idPacket 分类处理（0-127/192-231 多功能解码器，128-191 附件解码器，255 空闲）
   - 大量子命令解析（速度、方向、功能组、CV 访问等）

#### 边沿检测方向切换

DCC 信号可能以相反极性出现。当检测到半'0'+半'1'时，需要切换 cond1/cond2：
- 正常：cond1='r'(上升沿), cond2='f'(下降沿)
- 反转：cond1='f'(下降沿), cond2='r'(上升沿)

#### 关键数据结构

```c
typedef struct {
    uint64_t dccStart;          // 当前位的起始样本号
    uint64_t dccLast;           // 最后前导码位样本号
    int dccBitCounter;          // 当前字节内位计数器
    uint64_t dccBitPos[9];      // 当前字节各位的位置（0-8）
    int dccValue;               // 当前字节值
    // decodedBytes: 动态数组，每个元素 [value, [start, ..., stop]]
    int decodedBytes[64];       // 解码字节值
    uint64_t decodedBytesStart[64]; // 每个字节起始
    uint64_t decodedBytesEnd[64];   // 每个字节结束
    int decodedBytesCount;      // 已解码字节数

    int dccStatus;              // 状态枚举
    int syncSignal;             // 是否正在同步
    int cond1;                  // 当前第一边沿条件（'r'或'f'）
    int cond2;                  // 当前第二边沿条件

    int64_t dec_addr_search;    // 搜索解码器地址
    int64_t acc_addr_search;    // 搜索附件地址
    int64_t cv_addr_search;     // 搜索CV
    int64_t byte_search;        // 搜索字节值
    int speed14;                // 14步速度模式
    int serviceMode;            // 服务模式
    int64_t AddrOffset;         // 附件地址偏移
    int ignoreInterferingPulse; // 忽略短脉冲

    uint64_t edge_1, edge_2, edge_3, edge_4; // 边沿位置
    uint64_t samplerate;
    double accuracy;            // µs精度
    int out_ann;
} dcc_state;
```

### 1.7 C 实现要点

1. **状态机用枚举**：
   ```c
   enum dcc_status { DCC_WAITINGFORPREAMBLE, DCC_PREAMBLE, DCC_ADDRESSDATABYTE };
   ```

2. **边沿条件切换**：在 C 中用 `c_cond_rise`/`c_cond_fall` 动态构建条件，每次循环根据当前 cond1/cond2 创建新的条件构建器

3. **decodedBytes 存储**：Python 用列表存储 `[value, [pos0, ..., pos8]]`，C 中用固定大小数组（DCC 包最大约 10 字节），每个字节存储值和起始/结束样本号

4. **handleDecodedBytes() 翻译**：这是最复杂的部分（约 900 行 Python），包含大量条件分支。C 实现需要：
   - 使用 `snprintf` 构建输出字符串
   - 用辅助函数 `put_packetbyte()` 和 `put_packetbytes()` 封装注解输出
   - 仔细处理 `incPos()` 的越界检查

5. **需要 metadata 回调**：是，需要 samplerate

6. **不需要 c_decoder_put_python**：outputs 为空

7. **weekday/month 查找表**：需要静态字符串数组

8. **格式字符串**：大量使用 `str()` 和 `hex()` 格式化，C 中用 `snprintf(buf, sizeof(buf), "%d", val)` 和 `snprintf(buf, sizeof(buf), "%X", val)`

9. **Python 的 `self.wait({0: 'e'})`**：C 中用 `c_cond_edge(b, 0)` + `c_cond_wait()`

10. **复杂度评估**：**极高**。DCC 是本次批次中最复杂的解码器，handleDecodedBytes 函数有约 900 行逻辑，包含数十种命令类型解析。建议最后实现。

---

## 2. Delta-Sigma 解码器

### 2.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `'delta-sigma'` |
| name | `'Delta-Sigma'` |
| longname | `'Delta-Sigma Decoder'` |
| desc | `'Clocked.'` |
| license | `'gplv2+'` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Util']` |

### 2.2 通道

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | dat | DAT | Data | (无) |
| 1 | clk | CLK | Clock | (无) |

无可选通道。

### 2.3 选项

| id | desc | default | values | C类型 |
|----|------|---------|--------|-------|
| clock_mode | Clock Mode | `'normal'` | `('normal', 'manchester')` | string |
| filter_type | Filter type | `'sinc3'` | `('sinc_fast', 'sinc1', 'sinc2', 'sinc3')` | string |
| osr | Oversampling Factor | `4` | — | int |
| shift | Right shift the result by | `0` | — | int |
| scale | Code-Actual scaler | `1.0` | — | double |

### 2.4 注解

| 枚举值 | id | desc |
|--------|-----|------|
| 0 | bit-stream | Bit Stream |
| 1 | filtered | Filtered |
| 2 | converted | Converted |

### 2.5 注解行

| id | label | 包含的注解类 |
|----|-------|-------------|
| bit-streams | Bit Stream | (0,) |
| filtereds | Filtered | (1,) |
| converteds | Converted | (2,) |

### 2.6 decode() 逻辑分析

#### 核心流程

1. **验证通道**：DAT(0) 和 CLK(1) 都必须存在
2. **等待条件**：CLK 上升沿 `{1: 'r'}`
3. **每次时钟上升沿**：
   - 读取 DAT 值
   - 输出当前位注解（从上一个时钟沿到当前）
   - 根据选择的滤波器类型运行滤波算法
   - 当计数器达到 OSR 时输出滤波结果

#### Sinc 滤波器算法

**Sinc1**：
```
if dat > 0: DELTA1 += 1 else: DELTA1 -= 1
CNTR += 1
if CNTR == osr:
    CNTR = 0
    DN0 = DELTA1
    DN1 = DN0_prev
    CN3 = DN0 - DN1
    output CN3
```

**Sinc2**：
```
if dat > 0: DELTA1 += 1 else: DELTA1 -= 1
CN1 += DELTA1
CNTR += 1
if CNTR == osr:
    CNTR = 0
    DN0 = CN1
    DN1 = DN0_prev
    CN3 = DN0 - DN1
    CN4 = CN3 - DN3_prev
    output CN4
```

**Sinc3**：
```
if dat > 0: DELTA1 += 1 else: DELTA1 -= 1
CN1 += DELTA1
CN2 += CN1
CNTR += 1
if CNTR == osr:
    CNTR = 0
    DN0 = CN2
    DN1 = DN0_prev
    CN3 = DN0 - DN1
    CN4 = CN3 - DN3_prev
    CN5 = CN4 - DN5_prev
    output CN5
```

**Sinc Fast**：Python 代码中未实现（选项存在但无对应处理逻辑），C 实现中应回退到 sinc3 或输出错误。

#### 输出格式

- 注解 0 (Bit Stream)：`'%d' % current_dat`
- 注解 1 (Filtered)：`'%d' % (code >> shift)`
- 注解 2 (Converted)：`'%d' % (code * scale)`

### 2.7 C 实现要点

1. **私有状态结构体**：
   ```c
   typedef struct {
       uint64_t samplerate;
       uint64_t last_samplenum;
       int current_dat;
       uint64_t last_filternum;

       int64_t sinc_DELTA1;
       int64_t sinc_CN1;
       int64_t sinc_CN2;
       int64_t sinc_DN0;
       int64_t sinc_DN1;
       int64_t sinc_DN3;
       int64_t sinc_DN5;
       int64_t sinc_CNTR;

       int clock_mode;     // 0=normal, 1=manchester
       int filter_type;    // 0=sinc_fast, 1=sinc1, 2=sinc2, 3=sinc3
       int osr;
       int shift;
       double scale;
       int out_ann;
   } delta_sigma_state;
   ```

2. **需要 metadata 回调**：是，需要 samplerate

3. **不需要 c_decoder_put_python**：outputs 为空

4. **等待条件**：始终 `{1: 'r'}`（CLK 上升沿），用 `c_cond_rise(b, 1)` 构建

5. **第一个样本特殊处理**：`find_clk_edge()` 中，`last_samplenum` 为 None 时输出 'X'，否则输出前一个 dat 值

6. **滤波器状态变量更新顺序**：必须先计算输出，再更新 prev 值（DN1=DN0_prev, DN3=CN3_prev, DN5=CN4_prev）

7. **复杂度评估**：**低**。逻辑简单，约 200 行 Python，无复杂状态机。

---

## 3. DSI 解码器

### 3.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `'dsi'` |
| name | `'DSI'` |
| longname | `'Digital Serial Interface'` |
| desc | `'Digital Serial Interface (DSI) lighting protocol.'` |
| license | `'gplv2+'` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Embedded/industrial', 'Lighting']` |

### 3.2 通道

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | dsi | DSI | DSI data line | `'dec_dsi_chan_dsi'` |

无可选通道。

### 3.3 选项

| id | desc | default | values | idn | C类型 |
|----|------|---------|--------|-----|-------|
| polarity | Polarity | `'active-high'` | `('active-low', 'active-high')` | `'dec_dsi_opt_polarity'` | string |

### 3.4 注解

| 枚举值 | id | desc |
|--------|-----|------|
| 0 | bit | Bit |
| 1 | startbit | Start bit |
| 2 | level | Dimmer level |
| 3 | raw | Raw data |

### 3.5 注解行

| id | label | 包含的注解类 |
|----|-------|-------------|
| bits | Bits | (0,) |
| raw | Raw data | (3,) |
| fields | Fields | (1, 2) |

### 3.6 decode() 逻辑分析

#### 状态机

- `IDLE` — 等待起始位
- `PHASE0` — 第一相位
- `PHASE1` — 第二相位

#### 核心流程

1. **需要 samplerate**：计算 halfbit = int((samplerate * 0.0016667) / 2.0)
2. **极性处理**：若 `active-high`，则反转输入（`dsi ^= 1`），使内部逻辑统一为 active-low
3. **IDLE 状态**：
   - 等待任意边沿
   - 记录第一个半位边沿（`samplenum - halfbit`）
   - 设置 `phase0 = dsi ^ 1`
   - 转到 PHASE1
4. **PHASE0/PHASE1 交替**：
   - 检测边沿或半位超时
   - PHASE0 → PHASE1：记录 phase0
   - PHASE1 → PHASE0：如果 bit==1 且 phase0==1，则为停止位，处理收集的位；否则记录数据位
5. **帧类型**：
   - 17 位 = Forward 帧（1 起始位 + 8 数据位 + 8 数据位 + 停止位）
   - 9 位 = Backward 帧（1 起始位 + 8 数据位 + 停止位）
6. **handle_bits()**：
   - 输出每个位的注解
   - 起始位注解
   - Backward 帧：计算 level = f / 2.55，输出百分比
   - Forward 帧：暂未实现完整解析（Python 代码中只处理了 Backward）

#### 关键时序参数

- DSI 位周期：1666.7 µs（1TE）
- halfbit = samplerate * 0.0016667 / 2

#### 边沿检测细节

在 PHASE0/PHASE1 中：
- 如果检测到实际边沿（`old_dsi != dsi`），记录边沿位置
- 如果在 `edges[-1] + halfbit * 1.5` 处未检测到边沿，插入虚拟边沿
- 否则跳过

### 3.7 C 实现要点

1. **私有状态结构体**：
   ```c
   typedef struct {
       uint64_t samplerate;
       int halfbit;
       int old_dsi;
       int state;          // DSI_IDLE, DSI_PHASE0, DSI_PHASE1
       int phase0;

       // 动态数组替代
       uint64_t edges[64]; // 边沿位置
       int edges_count;
       struct { uint64_t pos; int val; } bits[32]; // 位数据
       int bits_count;
       uint64_t ss_es_bits[32][2]; // 每位的 [ss, es]
       int ss_es_bits_count;

       int out_ann;
   } dsi_state;
   ```

2. **需要 metadata 回调**：是，需要 samplerate 计算 halfbit

3. **不需要 c_decoder_put_python**：outputs 为空

4. **等待条件**：`self.wait()` 无条件（每次采样），C 中使用 `c_cond_wait_current(di, &samplenum)` 获取当前位置（等效于 Python 的 `self.wait({})`），或用 `c_cond_edge(b, 0)` 等待边沿 <!-- Updated: c_cond_wait_current()已实现，是self.wait({})的正确等价API -->

5. **Python 的 `self.wait()` 无参数**：C 中应使用 `c_cond_wait_current(di, &samplenum)` 获取当前采样位置（等效于 Python 的 `self.wait({})`，不前进采样位置）。若需前进到下一采样点再等待边沿，可用 `c_cond_edge(b, 0)`。`c_cond_skip(b, 1)` 逐样本等待效率极低，不推荐。 <!-- Updated: c_cond_wait_current()已实现，替代原来建议的c_cond_skip(b,1)方案 -->

6. **极性反转**：在 start() 中根据选项设置 `old_dsi` 初始值（active-low → 1, active-high → 0），在 decode 中对 active-high 输入做 `dsi ^= 1`

7. **半位超时检测**：Python 中通过 `self.samplenum == (self.edges[-1] + int(self.halfbit * 1.5))` 精确匹配，C 中需要用 `c_cond_skip()` 实现定时等待，或者用边沿等待+采样点检查

8. **复杂度评估**：**中等**。状态机较简单，但边沿检测和半位超时逻辑需要仔细翻译。

---

## 4. EM4100 解码器

### 4.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `'em4100'` |
| name | `'EM4100'` |
| longname | `'RFID EM4100'` |
| desc | `'EM4100 100-150kHz RFID protocol.'` |
| license | `'gplv2+'` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['IC', 'RFID']` |

### 4.2 通道

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | data | Data | Data line | `'dec_em4100_chan_data'` |

无可选通道。

### 4.3 选项

| id | desc | default | values | idn | C类型 |
|----|------|---------|--------|-----|-------|
| polarity | Polarity | `'active-high'` | `('active-low', 'active-high')` | `'dec_em4100_opt_polarity'` | string |
| datarate | Data rate | `64` | `(64, 32, 16)` | `'dec_em4100_opt_datarate'` | int |
| coilfreq | Coil frequency | `125000` | — | `'dec_em4100_opt_coilfreq'` | int |

### 4.4 注解

| 枚举值 | id | desc |
|--------|-----|------|
| 0 | bit | Bit |
| 1 | header | Header |
| 2 | version-customer | Version/customer |
| 3 | data | Data |
| 4 | rowparity-ok | Row parity OK |
| 5 | rowparity-err | Row parity error |
| 6 | colparity-ok | Column parity OK |
| 7 | colparity-err | Column parity error |
| 8 | stopbit | Stop bit |
| 9 | tag | Tag |

### 4.5 注解行

| id | label | 包含的注解类 |
|----|-------|-------------|
| bits | Bits | (0,) |
| fields | Fields | (1, 2, 3, 4, 5, 6, 7, 8) |
| tags | Tags | (9,) |

### 4.6 decode() 逻辑分析

#### 状态机

- `HEADER` — 搜索 9 个连续的 '1' 位
- `PAYLOAD` — 收集 50 位有效载荷（10 组 × 5 位 = 4 数据位 + 1 行校验位）
- `TRAILER` — 收集 5 位尾部（4 列校验位 + 1 停止位）

#### Manchester 解码

EM4100 使用 Manchester 编码。解码逻辑在 `manchester_decode()` 中：

1. 等待边沿 `{0: 'e'}`
2. 计算脉冲长度 `pl = samplenum - oldsamplenum`
3. 根据 `pl` 和 `oldpl` 与 `halfbit_limit` 的比较确定位值和位置

**位判定规则**：
- `pl > halfbit_limit`：长脉冲，位在脉冲中间
  - 若 `oldpl > halfbit_limit`：前一个也是长脉冲，位从前一个脉冲中间开始
  - 否则：位从前一个脉冲开始处开始
- `pl <= halfbit_limit`：短脉冲
  - 若 `oldpl > halfbit_limit`：前一个是长脉冲，位在当前脉冲结束处
  - 否则：两个连续短脉冲，需要检查是否漏位

**关键参数计算**（在 metadata 中）：
```
bit_width = (samplerate / coilfreq) * datarate
halfbit_limit = bit_width/2 + bit_width/4
polarity = 0 if active-low else 1
```

#### putbit() 逻辑

1. **HEADER 状态**：
   - 连续 9 个 '1' → 输出 Header 注解，转到 PAYLOAD
   - 遇到 '0' → 重置计数

2. **PAYLOAD 状态**：
   - 每 5 位一组（4 数据位 + 1 行校验位）
   - 前 10 组：Version/customer（注解类 2）
   - 后 40 组：Data（注解类 3）
   - 行校验检查（XOR 4 数据位 == 校验位）
   - 累积 tag 值：`tag = (tag << 4) | data`
   - 50 位后转到 TRAILER

3. **TRAILER 状态**：
   - 4 列校验位 + 1 停止位
   - 列校验：每列 10 位的 XOR 应等于列校验位
   - 停止位应为 0
   - 全部校验通过 → 输出 Tag 注解（`'Tag: %010X' % tag`）
   - 5 位后重置回 HEADER

### 4.7 C 实现要点

1. **私有状态结构体**：
   ```c
   typedef struct {
       uint64_t samplerate;
       int oldpin;
       uint64_t last_samplenum;
       uint64_t lastlast_samplenum;
       uint64_t last_edge;
       double bit_width;
       double halfbit_limit;
       int oldpp;
       uint64_t oldpl;
       uint64_t oldsamplenum;
       uint64_t last_bit_pos;
       uint64_t ss_first;
       int first_one;
       int state;              // EM_HEADER, EM_PAYLOAD, EM_TRAILER
       int data;
       int data_bits;
       uint64_t ss_data;
       int data_parity;
       int payload_cnt;
       int data_col_parity[6];
       int col_parity[6];
       uint64_t col_parity_pos[6][2]; // [ss, es] for each column parity bit
       int col_parity_pos_count;
       uint64_t tag;
       int all_row_parity_ok;
       int polarity;
       int out_ann;
   } em4100_state;
   ```

2. **需要 metadata 回调**：是，需要 samplerate 计算 bit_width 和 halfbit_limit

3. **不需要 c_decoder_put_python**：outputs 为空

4. **等待条件**：`{0: 'e'}`（任意边沿），用 `c_cond_edge(b, 0)`

5. **Manchester 解码中的浮点**：`bit_width` 和 `halfbit_limit` 是浮点数，C 中用 `double`

6. **tag 类型**：Python 中 tag 是整数，最大 40 位（10 个十六进制位），C 中用 `uint64_t`

7. **col_parity_pos 数组**：Python 中是动态列表，C 中用固定大小数组 [6][2]

8. **复杂度评估**：**中等**。Manchester 解码逻辑需要仔细翻译，但整体结构清晰。

---

## 5. EM4305 解码器

### 5.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `'em4305'` |
| name | `'EM4305'` |
| longname | `'RFID EM4205/EM4305'` |
| desc | `'EM4205/EM4305 100-150kHz RFID protocol.'` |
| license | `'gplv2+'` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['IC', 'RFID']` |

### 5.2 通道

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | data | Data | Data line | `'dec_em4305_chan_data'` |

无可选通道。

### 5.3 选项

| id | desc | default | idn | C类型 |
|----|------|---------|-----|-------|
| coilfreq | Coil frequency | `125000` | `'dec_em4305_opt_coilfreq'` | int |
| first_field_stop | First field stop min | `40` | `'dec_em4305_opt_first_field_stop'` | int |
| w_gap | Write gap min | `12` | `'dec_em4305_opt_w_gap'` | int |
| w_one_max | Write one max | `32` | `'dec_em4305_opt_w_one_max'` | int |
| w_zero_on_min | Write zero on min | `15` | `'dec_em4305_opt_w_zero_on_min'` | int |
| w_zero_off_max | Write zero off max | `27` | `'dec_em4305_opt_w_zero_off_max'` | int |
| em4100_decode | EM4100 decode | `'on'` | `'dec_em4305_opt_em4100_decode'` | string |

### 5.4 注解

| 枚举值 | id | desc |
|--------|-----|------|
| 0 | bit_value | Bit value |
| 1 | first_field_stop | First field stop |
| 2 | write_gap | Write gap |
| 3 | write_mode_exit | Write mode exit |
| 4 | bit | Bit |
| 5 | opcode | Opcode |
| 6 | lock | Lock |
| 7 | data | Data |
| 8 | password | Password |
| 9 | address | Address |
| 10 | bitrate | Bitrate |

**注意**：Python 代码中 annotation_rows 的 fields 行包含 (5, 6, 7, 8, 9)，但注解 6 (lock) 在 Python 代码中实际未被使用。C 实现中仍需定义。

### 5.5 注解行

| id | label | 包含的注解类 |
|----|-------|-------------|
| bits | Bits | (0,) |
| structure | Structure | (1, 2, 3, 4) |
| fields | Fields | (5, 6, 7, 8, 9) |
| decode | Decode | (10,) |

### 5.6 decode() 逻辑分析

#### 状态机

- `FFS_SEARCH` — 搜索 First Field Stop
- `FFS_DETECTED` — 已检测到 FFS，进入写模式
- `SKIP` — 跳过状态（仅一拍后回到 FFS_SEARCH）

#### 核心流程

1. **需要 samplerate**：计算各种时序参数
2. **等待条件**：`{0: 'e'}`（任意边沿）
3. **时序参数计算**（metadata 中）：
   ```
   field_clock = samplerate / coilfreq
   wzmax = w_zero_off_max * field_clock
   wzmin = w_zero_on_min * field_clock
   womax = w_one_max * field_clock
   ffs = first_field_stop * field_clock
   writegap = w_gap * field_clock
   nogap = 300 * field_clock
   ```

4. **FFS_SEARCH 状态**：
   - 如果脉冲长度 `pl > ffs`，检测到 First Field Stop
   - 输出 FFS 注解，转到 FFS_DETECTED

5. **FFS_DETECTED 状态**：
   - 如果 `pl > writegap`，检测到写间隙
   - 如果距离上次间隙超过 `nogap`，退出写模式

6. **间隙检测后的位解码**：
   - 0 位：`(last_samplenum - old_gap_end) > wzmin && < wzmax`
   - 1 位：`(last_samplenum - old_gap_end) > womax && < nogap`
   - 多个 1 位：计算 `one_bits = (last_samplenum - old_gap_end) / womax`
   - 1 位后可能跟 0 位

7. **put_fields()**：
   - 50 位帧：Login 命令（3 位命令 + 32 位密码 + 校验）
   - 57 位帧：Write/Read 命令（3 位命令 + 6 位地址 + 32 位数据 + 校验）
   - 解码配置字（地址 4）：比特率、编码器、延迟等
   - EM4100 兼容解码（地址 5/6）

#### 辅助函数

- `get_8_bits(idx)`：从 bits_pos 获取 8 位值
- `get_32_bits(idx)`：从 bits_pos 获取 32 位值（4×8 位，间隔 9 位含校验位）
- `get_3_bits(idx)`：获取 3 位值
- `get_4_bits(idx)`：获取 4 位值
- `print_row_parity(idx, length)`：行校验检查
- `print_col_parity(idx)`：列校验检查
- `decode_config(idx)`：解码配置字
- `em4100_decode1(idx)` / `em4100_decode2(idx)`：EM4100 兼容解码

#### bits_pos 数据结构

```python
bits_pos = [[0 for col in range(3)] for row in range(70)]
# bits_pos[i][0] = bit value
# bits_pos[i][1] = start sample
# bits_pos[i][2] = end sample
```

### 5.7 C 实现要点

1. **私有状态结构体**：
   ```c
   typedef struct {
       uint64_t samplerate;
       uint64_t last_samplenum;
       uint64_t oldsamplenum;
       uint64_t old_gap_end;
       int gap_detected;
       int bit_nr;
       int state;          // EM_FFS_SEARCH, EM_FFS_DETECTED, EM_SKIP

       double field_clock;
       double wzmax;
       double wzmin;
       double womax;
       double ffs;
       double writegap;
       double nogap;

       // bits_pos: [70][3] — [bit_value, ss, es]
       int bits_pos_val[70];
       uint64_t bits_pos_ss[70];
       uint64_t bits_pos_es[70];

       int em4100_decode1_partial;
       int out_ann;
   } em4305_state;
   ```

2. **需要 metadata 回调**：是，需要 samplerate 计算各种时序参数

3. **不需要 c_decoder_put_python**：outputs 为空

4. **等待条件**：`{0: 'e'}`（任意边沿），用 `c_cond_edge(b, 0)`

5. **bits_pos 数组**：Python 中是 70×3 的二维列表，C 中用三个并行数组更高效

6. **em4100_decode 选项**：字符串 `'on'`/`'off'`，C 中用 `c_decoder_get_option_string()` 获取

7. **命令表**：`cmds = ['Invalid', 'Login', 'Write word', 'Invalid', 'Read word', 'Disable', 'Protect', 'Invalid']`

8. **比特率字符串表**：`br_string = ['RF/8', 'RF/16', 'Unused', 'RF/32', 'RF/40', 'Unused', 'Unused', 'RF/64']`

9. **编码器字符串表**：`encoder = ['not used', 'Manchester', 'Bi-phase', 'not used']`

10. **延迟字符串表**：`delayed_on = ['No delay', 'Delayed on - BP/8', 'Delayed on - BP/4', 'No delay']`

11. **put_fields() 中的帧长度判断**：
    - 50 位：Login 帧
    - 57 位：Write/Read 帧
    - 其他：忽略

12. **decode_config() 详细字段**：
    - 位 2-4：比特率（3 位索引到 br_string）
    - 位 6-7：编码器（2 位索引到 encoder）
    - 位 11-12：零位
    - 位 13-14：延迟开启（2 位索引到 delayed_on）
    - 位 15-19：最后默认读字（LWR）
    - 位 20：读登录
    - 位 21：零位
    - 位 22：写登录
    - 位 23-24：零位
    - 位 25：禁用
    - 位 27：Reader talk first
    - 位 28：零位
    - 位 29：Pigeon 模式
    - 位 30-34：保留

13. **复杂度评估**：**中高**。位解码逻辑较简单，但 put_fields() 和 decode_config() 有较多字段解析。

---

## 通用实现注意事项

### 1. 采样率检查

所有 5 个解码器都需要 samplerate。在 decode() 开始时检查：
```c
uint64_t samplerate = c_decoder_get_samplerate(di);
if (samplerate == 0) return; // 或报错
```

### 2. 条件等待模式

C 解码器的主循环模式：
```c
while (1) {
    srd_cond_builder *b = c_cond_new();
    c_cond_edge(b, 0);  // 或其他条件
    uint64_t samplenum, matched;
    int ret = c_cond_wait(b, di, &samplenum, &matched);
    c_cond_free(b);
    if (ret != SRD_OK) break;
    // 处理逻辑...
}
```

### 3. 注解输出模式

```c
C_ANN_PUT(di, start_sample, end_sample, out_ann, annotation_class, "text1", "text2", "text3");
```

### 4. 选项读取模式

```c
const char *str_val = c_decoder_get_option_string(di, "key", "default");
int64_t int_val = c_decoder_get_option_int(di, "key", default_val);
double dbl_val = c_decoder_get_option_double(di, "key", default_val);
```

### 5. 导出符号

每个 C 解码器文件末尾必须有：
```c
static struct srd_c_decoder decoder = {
    .id = "name",
    .name = "Name",
    // ... 所有字段 ...
    .reset = name_reset,
    .start = name_start,
    .decode = name_decode,
    .metadata = name_metadata,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void) {
    return &decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void) {
    return SRD_C_DECODER_API_VERSION;
}
```

### 6. CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加：
```
dcc_c
delta-sigma_c
dsi_c
em4100_c
em4305_c
```

### 7. Python 与 C 的关键差异

| Python | C |
|--------|---|
| `self.wait({ch: 'r'})` | `c_cond_rise(b, ch)` + `c_cond_wait()` |
| `self.wait({ch: 'f'})` | `c_cond_fall(b, ch)` + `c_cond_wait()` |
| `self.wait({ch: 'e'})` | `c_cond_edge(b, ch)` + `c_cond_wait()` |
| `self.wait()` (无条件) | `c_cond_wait_current(di, &samplenum)` <!-- Updated: c_cond_wait_current()已实现 --> |
| `self.samplenum` | `c_cond_wait()` 返回的 `samplenum` |
| `self.put(ss, es, out, [cls, [texts]])` | `C_ANN_PUT(di, ss, es, out, cls, texts...)` |
| `self.has_channel(ch)` | `c_decoder_has_channel(di, ch)` |
| 动态列表 | 固定大小数组 |
| Python 异常 | 静默返回或日志 |
| `int(data)` | 直接类型转换 |
| `str(val)` | `snprintf(buf, sizeof(buf), "%d", val)` |
| `hex(val)` | `snprintf(buf, sizeof(buf), "%X", val)` |
| `'%010X' % val` | `snprintf(buf, sizeof(buf), "%010llX", (unsigned long long)val)` |
| `'%02X' % val` | `snprintf(buf, sizeof(buf), "%02X", val)` |
| `'{:.0f}'.format(val)` | `snprintf(buf, sizeof(buf), "%.0f", val)` |
| `'{:.1f}'.format(val)` | `snprintf(buf, sizeof(buf), "%.1f", val)` |

---

## 实现优先级建议

| 优先级 | 解码器 | 原因 |
|--------|--------|------|
| 1 | delta-sigma | 最简单，约 200 行，无复杂状态机 |
| 2 | em4100 | 中等复杂度，Manchester 解码有参考 |
| 3 | dsi | 中等复杂度，半位超时需注意 |
| 4 | em4305 | 中高复杂度，多字段解析 |
| 5 | dcc | 极高复杂度，约 1400 行 Python，数十种命令 |

---

## 各解码器差异汇总

| 特性 | dcc | delta-sigma | dsi | em4100 | em4305 |
|------|-----|-------------|-----|--------|--------|
| 通道数 | 1 | 2 | 1 | 1 | 1 |
| 需要samplerate | ✓ | ✓ | ✓ | ✓ | ✓ |
| 需要metadata | ✓ | ✓ | ✓ | ✓ | ✓ |
| 输出到其他解码器 | ✗ | ✗ | ✗ | ✗ | ✗ |
| 状态机复杂度 | 高 | 低 | 中 | 中 | 中 |
| 注解类数 | 14 | 3 | 4 | 10 | 11 |
| 选项数 | 8 | 5 | 1 | 3 | 7 |
| Python行数 | ~1400 | ~220 | ~160 | ~240 | ~400 |
| 边沿检测 | 双边沿+方向切换 | CLK上升沿 | 任意边沿+半位超时 | 任意边沿 | 任意边沿 |
| 编码方式 | DCC双极性 | 直接采样 | DSI Manchester | Manchester | 脉冲宽度 |
