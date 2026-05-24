# Batch 36: Python 解码器移植为 C 解码器 — 详细规格

<!-- Updated: 本批次所有 5 个解码器均被阻塞 — 下层解码器均无 C 实现 -->

> ⚠️ **阻塞状态**：根据 C 解码器依赖规则（C 解码器只能依赖已有 C 实现的底层解码器），本批次所有 5 个解码器的下层依赖均只有 Python 实现，无 C 实现，**全部阻塞**。需要先完成下层解码器的 C 移植。

## 概述

本批次包含 5 个上层 Python 解码器，需要移植为 C 解码器。这些解码器均为上层协议解码器，通过 `recv_proto()` 回调接收下层解码器传递的协议数据，而非直接从 logic 信号采样。

### 解码器列表

| # | Python ID | C 文件名 | C ID | 输入协议 | 下层 C 实现 | 复杂度 | 状态 |
|---|-----------|----------|------|----------|------------|--------|------|
| 1 | `ook_oregon` | `ook_oregon_c.c` | `ook_oregon_c` | `ook` | ❌ 无（仅有 Python `ook`） | ★★★★ | 🔴 阻塞 |
| 2 | `ook_vis` | `ook_vis_c.c` | `ook_vis_c` | `ook` | ❌ 无（仅有 Python `ook`） | ★★★ | 🔴 阻塞 |
| 3 | `ltar_smartdevice` | `ltar_smartdevice_c.c` | `ltar_smartdevice_c` | `afsk_bits` | ❌ 无（仅有 Python `afsk`，输出 `afsk_bits`） | ★★ | 🔴 阻塞 |
| 4 | `ir_ltto_decode` | `ir_ltto_decode_c.c` | `ir_ltto_decode_c` | `ir_ltto` | ❌ 无（仅有 Python `ir_ltto`） | ★★★ | 🔴 阻塞 |
| 5 | `sony_md_decode` | `sony_md_decode_c.c` | `sony_md_decode_c` | `sony_md` | ❌ 无（仅有 Python `sony_md`） | ★★★★★ | 🔴 阻塞 |

<!-- Updated: 所有下层解码器均无 C 实现，根据 C 解码器依赖规则全部阻塞 -->

---
## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |

## 1. ook_oregon — Oregon Scientific 天气传感器协议

<!-- Updated: 🔴 阻塞 — 下层解码器 `ook` 仅有 Python 实现，无 C 实现（ook_c.c 不存在），需先移植 ook 为 C 解码器 -->

### 1.1 Python 元数据

```python
id = 'ook_oregon'
name = 'Oregon'
longname = 'Oregon Scientific'
desc = 'Oregon Scientific weather sensor protocol.'
license = 'gplv2+'
inputs = ['ook']
outputs = []
tags = ['Sensor']
```

### 1.2 Annotations (9 个)

| 索引 | Python ID | C 宏 | C 标签列 |
|------|-----------|------|----------|
| 0 | `bit` | `ANN_BIT` | `""`, `"Bit"` |
| 1 | `field` | `ANN_FIELD` | `""`, `"Field"` |
| 2 | `l2` | `ANN_L2` | `""`, `"L2"` |
| 3 | `pre` | `ANN_PREAMBLE` | `""`, `"Preamble"` |
| 4 | `syn` | `ANN_SYNC` | `""`, `"Sync"` |
| 5 | `id` | `ANN_SENSOR_ID` | `""`, `"SensorID"` |
| 6 | `ch` | `ANN_CHANNEL` | `""`, `"Channel"` |
| 7 | `roll` | `ANN_ROLLING_CODE` | `""`, `"Rolling code"` |
| 8 | `f1` | `ANN_FLAGS1` | `""`, `"Flags1"` |

### 1.3 Annotation Rows (3 行)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `bits` | Bits | (0,) |
| `fields` | Fields | (1, 3, 4) |
| `l2` | Level 2 | (2,) |

### 1.4 Binary Output

| 索引 | ID | 描述 |
|------|----|------|
| 0 | `data-hex` | Hex data |

### 1.5 Options (1 个)

| ID | 描述 | 默认值 | 可选值 |
|----|------|--------|--------|
| `unknown` | Unknown type is | `"Unknown"` | `"Unknown"`, `"Temp"`, `"Temp_Hum"`, `"Temp_Hum1"`, `"Temp_Hum_Baro"`, `"Temp_Hum_Baro1"`, `"UV"`, `"UV1"`, `"Wind"`, `"Rain"`, `"Rain1"` |

### 1.6 解码逻辑分析

**核心流程**：`decode()` 接收 `ook` 协议的 data（一个列表，每个元素为 `[ss, es, bit_char]`），然后：

1. **构建 ookstring**：将所有 bit 字符拼接成二进制字符串
2. **版本检测**：根据 preamble 模式判断协议版本：
   - v2.1：ookstring 前 40 位含 `"10011001"` → 分离 preamble + data
   - v1：ookstring 前 17 位含 `"E1100"` → preamble ≤ 12 位
   - v3：ookstring 前 28 位含 `"0101"` → preamble > 12 位
3. **Preamble/Sync 标注**：调用 `oregon_put_pre_and_sync()`
4. **v2.1 特殊处理**：丢弃奇数位，转为 v3 格式后调用 `oregon_v3()`
5. **v3 解码**：按 nibble 提取 SensorID(16bit)、Ch(4bit)、RollingCode(8bit)、Flags1(4bit)，然后剩余 nibble 逐个输出
6. **Level 2 解码**：根据 SensorID 查表确定传感器类型，解码温度/湿度/气压/风速/雨量等
7. **Checksum 验证**：累加反转 nibble，模 255

**关键数据结构**：
- `decoded_nibbles`：列表 `[ss, es, label, hex_result]`，用于 L2 解码
- `sensor` 字典（来自 `lists.py`）：SensorID → (model_list, type)
- `sensor_checksum` 字典：SensorID → (checksum_method, comment)
- `dir_table`：风向表

### 1.7 C 实现方案

**文件**：`ook_oregon_c.c`

**状态机**：
```c
enum oregon_state {
    ORE_IDLE,
    ORE_V1_DECODE,
    ORE_V2_DECODE,
    ORE_V3_DECODE,
};
```

**私有结构体**：
```c
typedef struct {
    int state;
    int out_ann;
    int out_binary;
    char ookstring[4096];  // 拼接的 bit 字符串
    int ook_len;
    // decoded entries from ook protocol
    uint64_t *ss_arr;  // 动态数组
    uint64_t *es_arr;
    char *bit_arr;     // '0'/'1'/'E'
    int num_bits;
    // nibble decode state
    int decode_pos;
    char ver[8];       // "v1", "v2.1", "v3"
    // L2 decode
    char sensor_id[8];
    int sensor_type;   // enum
    // options
    int unknown_type;
} oregon_state;
```

**recv_proto 回调**：
- `ook` 协议的 Python output 是 data 列表 `[[ss, es, bit_char], ...]`
- 在 C 中，`recv_proto` 接收 `cmd` 和 `data`/`data_len`
- 需要设计 `ook` → `ook_oregon_c` 的数据传递格式
- **关键**：`ook` 下层解码器通过 `c_decoder_put_python()` 发送数据，上层通过 `recv_proto()` 接收
- `cmd` 可以是 `"DATA"` 携带完整的 bit 列表，或者逐 bit 发送
- <!-- Updated: 🔴 阻塞 — `ook` 解码器无 C 实现，recv_proto 数据格式需在 ook_c.c 实现时确定 -->

**实现策略**：
- 由于 `ook` 协议一次性传递整个 bit 列表，`recv_proto` 应接收 `"DATA"` 命令，data 中包含所有 bit 信息
- 在 `recv_proto` 中完成全部解码逻辑（不需要 `decode()` 函数）
- `decode()` 函数体为空（与 `lm75_c.c` 模式一致）

**sensor 查找表**：将 `lists.py` 中的 `sensor` 和 `sensor_checksum` 字典转为 C 结构体数组

**关键 C 代码片段**：
```c
static void ook_oregon_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    oregon_state *s = (oregon_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") == 0) {
        // 解析 data 中的 bit 列表
        // 构建 ookstring
        // 版本检测
        // 调用 oregon_v1/v2/v3 解码
    }
}
```

### 1.8 难点与注意事项

<!-- Updated: 第 1 点已确认 — ook 无 C 实现，需先移植 ook_c -->

1. **ook 数据格式**：~~需要确认 `ook` C 解码器（如果存在）或 Python `ook` 解码器发送的 `recv_proto` 数据格式~~ → 已确认 `ook` 无 C 实现，需先移植 `ook_c`，数据格式在 `ook_c` 实现时定义
2. **v2.1 位丢弃**：需要正确实现奇数位丢弃和位置重对齐
3. **Nibble 反转**：Oregon 协议的 nibble 是从右到左读取的
4. **Checksum 算法**：累加反转 nibble，模 255，v2.1 需减 10
5. **L2 解码表**：sensor 查找表较大（~20 条），需完整移植
6. **温度解码**：支持负温度（sign bit），小数点处理

---

## 2. ook_vis — OOK 可视化

<!-- Updated: 🔴 阻塞 — 下层解码器 `ook` 仅有 Python 实现，无 C 实现（ook_c.c 不存在），需先移植 ook 为 C 解码器 -->

### 2.1 Python 元数据

```python
id = 'ook_vis'
name = 'OOK visualisation'
longname = 'On-off keying visualisation'
desc = 'OOK visualisation in various formats.'
license = 'gplv2+'
inputs = ['ook']
outputs = ['ook']
tags = ['Encoding']
```

### 2.2 Annotations (6 个)

| 索引 | Python ID | C 宏 | C 标签列 |
|------|-----------|------|----------|
| 0 | `bit` | `ANN_BIT` | `""`, `"Bit"` |
| 1 | `ref` | `ANN_REF` | `""`, `"Reference"` |
| 2 | `field` | `ANN_FIELD` | `""`, `"Field"` |
| 3 | `ref_field` | `ANN_REF_FIELD` | `""`, `"Ref field"` |
| 4 | `level2` | `ANN_L2` | `""`, `"L2"` |
| 5 | `ref_level2` | `ANN_REF_L2` | `""`, `"Ref L2"` |

### 2.3 Annotation Rows (6 行)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `bits` | Bits | (0,) |
| `compare` | Compare | (1,) |
| `fields` | Fields | (2,) |
| `ref_fields` | Ref fields | (3,) |
| `level2` | L2 | (4,) |
| `ref_level2` | Ref L2 | (5,) |

### 2.4 Options (4 个)

| ID | 描述 | 默认值 | 可选值 |
|----|------|--------|--------|
| `displayas` | Display as | `"Nibble - Hex"` | `"Byte - Hex"`, `"Byte - Hex rev"`, `"Byte - BCD"`, `"Byte - BCD rev"`, `"Nibble - Hex"`, `"Nibble - Hex rev"`, `"Nibble - BCD"`, `"Nibble - BCD rev"` |
| `synclen` | Sync length | `"4"` | `"0"` ~ `"10"` |
| `syncoffset` | Sync offset | `"0"` | `"-4"` ~ `"4"` |
| `refsample` | Compare | `"off"` | `"off"`, `"show numbers"`, `"1"` ~ `"30"` |

### 2.5 解码逻辑分析

**核心流程**：

1. **缓存 OOK 数据**：`add_to_cache()` 将每次接收的 decoded 数据缓存
2. **display_all()**：
   - 构建 ookstring
   - 根据 `displayas` 选项确定 bits 宽度（4=Nibble, 8=Byte）
   - 按 bits 宽度逐段输出 field annotation
   - 调用 `display_level2()` 进行 L2 解码
3. **display_level2()**：
   - 自动检测 preamble 模式（`1111` 或 `1010`）
   - 标注 Preamble、Sync
   - 剩余部分按 bits 宽度输出
4. **参考比较**：如果 `refsample` 选项开启，与缓存的参考 trace 比较
5. **透传数据**：通过 `putp(data)` 将 OOK 数据继续向上层传递

**关键特性**：
- **outputs = ['ook']**：此解码器是透传的，输出与输入相同的 `ook` 协议
- **缓存机制**：`ookcache` 保存历史 trace 用于参考比较
- **BCD 转换**：使用 `bcd2int()` 辅助函数

### 2.6 C 实现方案

**文件**：`ook_vis_c.c`

**私有结构体**：
```c
#define OOK_VIS_MAX_CACHE_TRACES 30
#define OOK_VIS_MAX_BITS 4096

typedef struct {
    int out_ann;
    int out_python;
    int displayas;    // enum
    int sync_length;
    int sync_offset;
    int ref_sample;   // 0=off, -1=show numbers, 1~30=ref index
    int trace_num;
    // Current trace
    char ookstring[OOK_VIS_MAX_BITS];
    int ook_len;
    // Cache
    int cache_count;
    // ... cache storage for reference comparison
} ookvis_state;
```

**recv_proto 回调**：
- 接收 `"DATA"` 命令，携带 bit 列表
- 执行 display_all 逻辑
- 通过 `c_decoder_put_python()` 透传数据

**关键 C 代码片段**：
```c
static void ookvis_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ookvis_state *s = (ookvis_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") == 0) {
        // 解析 bit 列表
        // 构建 ookstring
        // display_all()
        // 透传 c_decoder_put_python()
        s->trace_num++;
    }
}
```

### 2.7 难点与注意事项

<!-- Updated: 补充 ook 下层阻塞标注 -->

1. **缓存机制**：C 中需要手动管理 trace 缓存，可能需要动态内存
2. **参考比较**：`display_ref()` 需要访问历史 trace 的 bit 数据
3. **Preamble 检测**：自动检测 `1111` 或 `1010` 模式
4. **BCD 转换**：需实现 `bcd2int()` 辅助函数
5. **透传输出**：需要 `out_python` 输出端口，将数据传递给上层
6. **🔴 下层阻塞**：`ook` 解码器无 C 实现，需先移植 `ook_c`

---

## 3. ltar_smartdevice — LTAR SmartDevice 协议

<!-- Updated: 🔴 阻塞 — 下层解码器 `afsk_bits` 仅有 Python 实现（Python `afsk` 解码器输出 `afsk_bits`），无 C 实现（afsk_c.c 不存在），需先移植 afsk 为 C 解码器 -->

### 3.1 Python 元数据

```python
id = 'ltar_smartdevice'
name = 'LTAR SmartDevice'
longname = 'LTAR SmartDevice'
desc = "A decoder for the LTAR laser tag blaster's Smart Device protocol"
license = 'unknown'
inputs = ['afsk_bits']
outputs = ['ltar_smartdevice']
tags = ['Embedded/industrial']
```

### 3.2 Annotations (9 个)

| 索引 | Python ID | C 宏 | C 标签列 |
|------|-----------|------|----------|
| 0 | `bit-start` | `ANN_BIT_START` | `""`, `"Start Bit"` |
| 1 | `bit-data` | `ANN_BIT_DATA` | `""`, `"Data Bit"` |
| 2 | `bit-stop` | `ANN_BIT_STOP` | `""`, `"Stop Bit"` |
| 3 | `bit-spacer` | `ANN_BIT_SPACER` | `""`, `"Spacer Bit"` |
| 4 | `bit-blockend` | `ANN_BIT_BLOCKEND` | `""`, `"Block Stop Bit"` |
| 5 | `frame` | `ANN_FRAME` | `""`, `"Data frame"` |
| 6 | `frame-error` | `ANN_FRAME_ERROR` | `""`, `"Framing error"` |
| 7 | `block` | `ANN_BLOCK` | `""`, `"Data block"` |
| 8 | `block-error` | `ANN_BLOCK_ERROR` | `""`, `"Block error"` |

### 3.3 Annotation Rows (3 行)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `bits` | Bits | (0, 1, 2, 3, 4) |
| `frames` | Frames | (5, 6) |
| `blocks` | Blocks | (7, 8) |

### 3.4 解码逻辑分析

**核心流程**：状态机处理 AFSK bits 数据

**状态**：
- `IDLE` → 等待 start bit (0)
- `DATA` → 收集 8 个数据 bit
- `FRAMESTOP` → 等待 stop bit (1)
- `WAITINGFORBLOCKEND` → 等待 spacer bits 或新 frame start

**帧格式**：10-bit 帧 = start(0) + 8 data bits + stop(1)
- 数据 bit 顺序：LSB first，输出时 bit-swap 为 MSB first
- Block 结束：15+ spacer bits (1)
- Frame 间隔：≤10 spacer bits

**错误处理**：
- `ERROR/PHASE`：重新同步，中止当前解码
- `ERROR/INVALID`：无效周期，中止当前解码
- Framing error：stop bit 不为 1

**Python output 格式**：
```python
['BLOCK', currentblockdata]
# currentblockdata = [[frame_data, frame_value], ...]
# frame_data = [[ss, es, bit_value], ...] × 10
# frame_value = int (MSB first)
```

### 3.5 C 实现方案

**文件**：`ltar_smartdevice_c.c`

**状态机**：
```c
enum ltar_sd_state {
    LTAR_SD_IDLE,
    LTAR_SD_DATA,
    LTAR_SD_FRAMESTOP,
    LTAR_SD_WAITING_BLOCKEND,
};
```

**私有结构体**：
```c
#define LTAR_SD_MAX_FRAMES 32
#define LTAR_SD_MAX_BITS 10

typedef struct {
    int state;
    int out_ann;
    int out_python;
    // Current frame
    uint64_t frame_bits_ss[10];
    uint64_t frame_bits_es[10];
    int frame_bits_val[10];
    int frame_bit_count;
    // Current block
    int block_frame_count;
    uint64_t block_start_ss;
    // Spacer count
    int spacer_count;
} ltar_sd_state;
```

**recv_proto 回调**：
- `cmd = "BIT"`：携带单个 bit 数据
- `cmd = "ERROR"`：携带错误类型

**关键 C 代码片段**：
```c
static void ltar_sd_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ltar_sd_state *s = (ltar_sd_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "BIT") == 0) {
        int bit_val = (data_len > 0) ? data[0] : 0;
        // 状态机处理
        switch (s->state) {
        case LTAR_SD_IDLE:
            if (bit_val == 0) { /* start bit */ }
            break;
        case LTAR_SD_DATA:
            /* 收集 8 data bits */
            break;
        case LTAR_SD_FRAMESTOP:
            if (bit_val == 1) { /* frame complete */ }
            else { /* framing error */ }
            break;
        case LTAR_SD_WAITING_BLOCKEND:
            /* spacer counting */
            break;
        }
    } else if (strcmp(cmd, "ERROR") == 0) {
        /* 中止当前解码 */
    }
}
```

### 3.6 难点与注意事项

<!-- Updated: 第 1 点已确认 — afsk_bits 无 C 实现，需先移植 afsk_c -->

1. **数据格式**：~~需确认 `afsk_bits` 下层解码器的 `recv_proto` 数据格式~~ → 已确认 `afsk_bits` 无 C 实现（Python `afsk` 解码器输出 `afsk_bits`），需先移植 `afsk_c`，数据格式在 `afsk_c` 实现时定义
2. **Block 输出**：Python 版本通过 `OUTPUT_PYTHON` 发送复杂的嵌套结构，C 版本需要简化
3. **Bit swap**：8 data bits 从 LSB first 转为 MSB first
4. **Spacer 计数**：15+ spacer bits 标记 block 结束，10+ spacer bits 后不接受新 frame

---

## 4. ir_ltto_decode — LTTO 激光标签 IR 解码

<!-- Updated: 🔴 阻塞 — 下层解码器 `ir_ltto` 仅有 Python 实现，无 C 实现（ir_ltto_c.c 不存在），需先移植 ir_ltto 为 C 解码器 -->

### 4.1 Python 元数据

```python
id = 'ir_ltto_decode'
name = 'IR LTTO Decode'
longname = 'LTTO laser tag IR Decode'
desc = 'A decoder for the LTTO laser tag IR protocol'
license = 'unknown'
inputs = ['ir_ltto']
outputs = ['ir_ltto_decode']
tags = ['Embedded/industrial']
```

### 4.2 Annotations (6 个)

| 索引 | Python ID | C 宏 | C 标签列 |
|------|-----------|------|----------|
| 0 | `signature-type` | `ANN_SIG_TYPE` | `""`, `"Signature Type"` |
| 1 | `signature-error` | `ANN_SIG_ERROR` | `""`, `"Error"` |
| 2 | `signature-data` | `ANN_SIG_DATA` | `""`, `"Signature Data"` |
| 3 | `packet-type` | `ANN_PKT_TYPE` | `""`, `"Packet Type"` |
| 4 | `packet-error` | `ANN_PKT_ERROR` | `""`, `"Packet Error"` |
| 5 | `packet-data` | `ANN_PKT_DATA` | `""`, `"Packet Data"` |

### 4.3 Annotation Rows (4 行)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `signature-types` | Signature type | (0, 1) |
| `signature-datas` | Signature data | (2,) |
| `packet-types` | Packet type | (3, 4) |
| `packet-datas` | Packet data | (5,) |

### 4.4 解码逻辑分析

**输入格式**：`(synclength, bitcount, bitdata)`
- `synclength`：`"SHORT"` 或 `"LONG"`
- `bitcount`：bit 数量
- `bitdata`：整数值

**解码规则**：

| synclength | bitcount | 类型 | 处理 |
|------------|----------|------|------|
| SHORT | 7 | Tag | `putTagSignature()` |
| SHORT | 9 | Multibyte start/end | bit8=0 → start, bit8=1 → end |
| SHORT | 8 | Multibyte data | `putMultibyteDataSignature()` |
| LONG | 5 | LTTO Beacon | `putLTTOBeaconSignature()` |
| LONG | 9 | LTAR Beacon | `putLTARBeaconSignature()` |

**Tag 解码**：从 bitdata 提取 team(2bit)、player(5bit)、megatag(2bit)

**LTTO Beacon 解码**：team(2bit)、hitflag(1bit)、extra(2bit)，特殊处理 Area Beacon

**LTAR Beacon 解码**：hitflag(1bit)、shields(1bit)、health(2bit)、team(2bit)、player(3bit)

**Multibyte Packet**：start frame + data frames + end frame → 输出完整 packet

**ptype 查找表**：0x00~0x90 范围的 packet type 名称映射（~50 条）

### 4.5 C 实现方案

**文件**：`ir_ltto_decode_c.c`

**私有结构体**：
```c
#define LTTO_MAX_MULTIBYTE 16

typedef struct {
    int out_ann;
    int out_python;
    // Multibyte state
    int in_multibyte;
    uint64_t multibyte_start_ss;
    uint64_t multibyte_end_es;
    int multibyte_data_count;
    uint64_t multibyte_data_ss[LTTO_MAX_MULTIBYTE];
    uint64_t multibyte_data_es[LTTO_MAX_MULTIBYTE];
    int multibyte_data_val[LTTO_MAX_MULTIBYTE];
} ltto_decode_state;
```

**recv_proto 回调**：
- `cmd = "SHORT"` 或 `"LONG"`：data 包含 `[bitcount, bitdata]`

**关键 C 代码片段**：
```c
static void ltto_decode_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ltto_decode_state *s = (ltto_decode_state *)c_decoder_get_private(di);
    if (!s) return;

    int is_short = (strcmp(cmd, "SHORT") == 0);
    int is_long = (strcmp(cmd, "LONG") == 0);

    if (data_len < 3) return;  // 需要 bitcount(1) + bitdata(2)
    int bitcount = data[0];
    int bitdata = (data[1] << 8) | data[2];

    if (is_short) {
        if (bitcount == 7) {
            ltto_put_tag_signature(di, s, start_sample, end_sample, bitdata);
        } else if (bitcount == 9) {
            if (!(bitdata & 0x100))
                ltto_put_multibyte_start(di, s, start_sample, end_sample, bitdata);
            else
                ltto_put_multibyte_end(di, s, start_sample, end_sample, bitdata);
        } else if (bitcount == 8) {
            ltto_put_multibyte_data(di, s, start_sample, end_sample, bitdata);
        }
    } else if (is_long) {
        if (bitcount == 5)
            ltto_put_ltto_beacon(di, s, start_sample, end_sample, bitdata);
        else if (bitcount == 9)
            ltto_put_ltar_beacon(di, s, start_sample, end_sample, bitdata);
    }
}
```

### 4.6 难点与注意事项

<!-- Updated: 补充 ir_ltto 下层阻塞标注 -->

1. **ptype 查找表**：需要将 ~50 条 packet type 映射转为 C 数组
2. **Tag 解码**：team/player/megatag 的 bit 位提取需要精确
3. **Multibyte 状态**：需要跟踪 start/data/end 序列
4. **Beacon 特殊处理**：Area Beacon 有特殊逻辑（hitflag=0 且 extra≠0）
5. **healthtext 数组**：4 个健康等级文本
6. **🔴 下层阻塞**：`ir_ltto` 解码器无 C 实现，需先移植 `ir_ltto_c`

---

## 5. sony_md_decode — Sony MD LCD Remote 解码

<!-- Updated: 🔴 阻塞 — 下层解码器 `sony_md` 仅有 Python 实现，无 C 实现（sony_md_c.c 不存在），需先移植 sony_md 为 C 解码器 -->

### 5.1 Python 元数据

```python
id = 'sony_md_decode'
name = 'Sony MD Remote Decode'
longname = 'Sony MD LCD Remote Decoder'
desc = ''
license = 'unknown'
inputs = ['sony_md']
outputs = ['sony_md_decode']
tags = ['']
```

### 5.2 Annotations (16 个)

| 索引 | Python ID | C 宏 | C 标签列 |
|------|-----------|------|----------|
| 0 | `info` | `ANN_INFO` | `""`, `"Info"` |
| 1 | `transfer-block` | `ANN_TRANSFER_BLOCK` | `""`, `"Transfer block"` |
| 2 | `raw-value` | `ANN_RAW_VALUE` | `""`, `"Raw Value"` |
| 3 | `data-field-value-positive` | `ANN_DATA_VAL_POS` | `""`, `"Data Field Value (Positive)"` |
| 4 | `debug` | `ANN_DEBUG` | `""`, `"Debug"` |
| 5 | `debug-two` | `ANN_DEBUG2` | `""`, `"Debug2"` |
| 6 | `data-field-value-negative` | `ANN_DATA_VAL_NEG` | `""`, `"Data Field Value (Negative)"` |
| 7 | `sender-player` | `ANN_SENDER_PLAYER` | `""`, `"Transfer Block From Player"` |
| 8 | `sender-remote` | `ANN_SENDER_REMOTE` | `""`, `"Transfer Block From Remote"` |
| 9 | `data-field-name` | `ANN_DATA_FIELD_NAME` | `""`, `"Data Field Name"` |
| 10 | `error` | `ANN_ERROR` | `""`, `"Error"` |
| 11 | `warning` | `ANN_WARNING` | `""`, `"Warning"` |
| 12 | `data-field-unused` | `ANN_DATA_UNUSED` | `""`, `"Data Field (Unused)"` |
| 13 | `data-field-unknown` | `ANN_DATA_UNKNOWN` | `""`, `"Data Field (Unknown)"` |
| 14 | `data-field-static` | `ANN_DATA_STATIC` | `""`, `"Data Field (Static)"` |
| 15 | `command` | `ANN_COMMAND` | `""`, `"Command"` |

### 5.3 Annotation Rows (11 行)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `informational` | Informational | (0,) |
| `transfer-blocks` | Data Transfer Blocks | (1,) |
| `senders` | Block Sender | (7, 8) |
| `commands` | Commands | (15,) |
| `raw-values` | Raw Values | (2,) |
| `data-field-names` | Data Field Names | (9,) |
| `data-field-values` | Data Field Values | (3, 6, 12, 13, 14) |
| `debugs` | Debugs | (4,) |
| `debugs-two` | Debugs 2 | (5,) |
| `errors` | Errors | (10,) |
| `warnings` | Warnings | (11,) |

### 5.4 解码逻辑分析

**输入格式**：`(syncData, bitData, cleanEnd)`
- `bitData` = `[startOfBits, endOfBits, numberOfBits, [bit_list]]`
- `bit_list` = `[[ss, es, bit_val], ...]` — 每个 bit 的详细信息

**核心流程**：

1. **Message Start/End**：标注消息起止
2. **Remote Header** (8 bits)：解析 remote 状态（ready for text, data to send, Kanji capable, present）
3. **Player Header** (8 bits)：解析 player 状态（data to send, cede bus, present）
4. **Player Data Block** (88 bits = 11 bytes)：
   - 10 bytes data + 1 byte checksum
   - `expandPlayerDataBlock()` 根据 command byte 解码各种子协议
5. **Remote Data Block** (99 bits = 11 × 9-bit frames)：
   - 10 × 9-bit data frames + 1 × 9-bit checksum
   - `expandRemoteDataBlock()` 解码 remote capabilities 和 text

**子协议解码**（`expandPlayerDataBlock`）：
- 0x01: Request Remote Capabilities
- 0x02: Unknown (initialization)
- 0x03: Scroll Control
- 0x05: LCD Backlight Control
- 0x06: LCD Remote Service Mode
- 0x08: Pre-text update
- 0x40: Volume Level
- 0x41: Playback Mode
- 0x42: Recording Indicator
- 0x43: Battery Level Indicator
- 0x46: EQ/Sound Indicator
- 0x47: Alarm Indicator
- 0xA0: Track number
- 0xA1: LCD Disc Icon Control
- 0xC0: Player capabilities
- 0xC8: LCD Text (含 Shift-JIS 解码)

**Shift-JIS 字符解码**：`putLCDCharacter()` 支持单字节 ASCII、半角片假名、双字节 SJIS

### 5.5 C 实现方案

**文件**：`sony_md_decode_c.c`

**私有结构体**：
```c
#define SONY_MD_MAX_VALUES 16
#define SONY_MD_MAX_BITS 256

typedef struct {
    int out_ann;
    // Message state
    uint64_t msg_start_ss;
    uint64_t msg_end_es;
    // Values extracted from bit data
    uint8_t values[SONY_MD_MAX_VALUES];
    int value_count;
    // Checksum
    uint8_t checksum;
    // Shift-JIS carryover
    uint8_t sjis_carryover;
    // Debug output
    char debug_hex[256];
    // Bit data cache
    uint64_t bit_ss[SONY_MD_MAX_BITS];
    uint64_t bit_es[SONY_MD_MAX_BITS];
    int bit_val[SONY_MD_MAX_BITS];
    int num_bits;
} sony_md_state;
```

**recv_proto 回调**：
- `cmd = "MESSAGE"`：data 包含完整的 bit 数据

**关键 C 代码片段**：
```c
static void sony_md_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    sony_md_state *s = (sony_md_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "MESSAGE") == 0) {
        // 解析 bitData
        // putMessageStart()
        // expandMessage()
        // putMessageEnd()
    }
}
```

### 5.6 难点与注意事项

<!-- Updated: 补充 sony_md 下层阻塞标注 -->

1. **极其复杂**：这是本批次最复杂的解码器，代码量约 1200 行 Python
2. **Bit 级操作**：大量 bit 级别的位置计算和值提取
3. **Shift-JIS 解码**：需要实现 SJIS 双字节字符解码
4. **子协议分支**：`expandPlayerDataBlock` 有 ~20 种子协议
5. **Checksum**：XOR checksum 验证
6. **9-bit Remote Data Block**：remote 数据帧是 9-bit 格式
7. **characters 查找表**：特殊字符映射
8. **建议**：优先实现核心框架和主要子协议（0x40/0x41/0xC8），其余子协议可后续补充
9. **🔴 下层阻塞**：`sony_md` 解码器无 C 实现，需先移植 `sony_md_c`

---

## 通用 C 解码器模板（recv_proto 模式）

所有上层解码器遵循以下模板：

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

// 1. Annotation 枚举
enum { ANN_XXX = 0, ..., NUM_ANN };

// 2. 私有状态结构体
typedef struct {
    int out_ann;
    // ... 其他状态变量
} xxx_state;

// 3. Annotation 标签
static const char *xxx_ann_labels[][3] = {
    {"", "label1", "description1"},
    // ...
};

// 4. Annotation Rows
static const int xxx_row_xxx_classes[] = { ANN_XXX, -1 };
static const struct srd_c_ann_row xxx_ann_rows[] = {
    {"row_id", "Row Name", xxx_row_xxx_classes, N},
};

// 5. Inputs/Outputs/Tags
static const char *xxx_inputs[] = {"proto_name", NULL};
static const char *xxx_outputs[] = {"output_proto", NULL};  // 或 NULL
static const char *xxx_tags[] = {"Tag", NULL};

// 6. Options (如果有)
static struct srd_decoder_option xxx_options[] = { ... };

// 7. reset
static void xxx_reset(struct srd_decoder_inst *di) {
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(xxx_state)));
    xxx_state *s = (xxx_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(xxx_state));
    // 初始化特定字段
}

// 8. start
static void xxx_start(struct srd_decoder_inst *di) {
    xxx_state *s = (xxx_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "proto_name");
    // 读取 options
}

// 9. decode (空函数，recv_proto 模式)
static void xxx_decode(struct srd_decoder_inst *di) {
    (void)di;
}

// 10. recv_proto (核心逻辑)
static void xxx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    xxx_state *s = (xxx_state *)c_decoder_get_private(di);
    if (!s) return;
    // 解码逻辑
}

// 11. destroy
static void xxx_destroy(struct srd_decoder_inst *di) {
    void *priv = c_decoder_get_private(di);
    if (priv) { g_free(priv); c_decoder_set_private(di, NULL); }
}

// 12. Decoder 结构体
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    .longname = "Full Name (C)",
    .desc = "Description (C implementation)",
    .license = "gplv2+",
    .channels = NULL, .num_channels = 0,
    .optional_channels = NULL, .num_optional_channels = 0,
    .options = xxx_options, .num_options = N,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = N,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs, .num_inputs = 1,
    .outputs = xxx_outputs, .num_outputs = N,
    .binary = NULL, .num_binary = 0,
    .tags = xxx_tags, .num_tags = 1,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,
    .destroy = xxx_destroy,
    .recv_proto = xxx_recv_proto,
};

// 13. Entry point
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void) {
    // 初始化 option 默认值
    return &xxx_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void) {
    return SRD_C_DECODER_API_VERSION;
}
```

---

## CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加：
```
ook_oregon_c
ook_vis_c
ltar_smartdevice_c
ir_ltto_decode_c
sony_md_decode_c
```
