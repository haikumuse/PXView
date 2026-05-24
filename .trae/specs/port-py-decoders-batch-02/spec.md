# Python 解码器移植为 C 解码器规格书 — Batch 02

## 概述

本文档详细描述将以下 5 个 Python 协议解码器移植为 C 解码器的完整规格：

1. **flexray** — FlexRay 汽车网络通信协议
2. **mipi_rffe** — MIPI RF 前端控制接口
3. **usb_power_delivery** — USB Power Delivery 协议
4. **iebus** — Inter-Equipment Bus 汽车通信总线
5. **spacewire** — SpaceWire 航天通信协议

每个解码器的 C 实现文件将放置于 `libsigrokdecode/c_decoders/` 目录，文件名格式为 `<name>_c.c`，并在 `CMakeLists.txt` 的 `C_DECODERS` 列表中注册。

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理、BITS v2格式、SPI DATA 17字节格式 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |
| uart_c.c | UART解码器范本 | IDLE/BREAK检测、双线独立状态机、samplerate计算、Python输出格式 <!-- Updated: 添加uart_c.c参考 --> |

## 通用 C 解码器架构参考

### 头文件与依赖

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"
```

### 关键 API 函数

| API 函数 | 用途 |
|----------|------|
| `c_decoder_get_private(di)` | 获取私有数据指针 |
| `c_decoder_set_private(di, ptr)` | 设置私有数据指针 |
| `c_decoder_register_output(di, type, id)` | 注册输出（SRD_OUTPUT_ANN / SRD_OUTPUT_PYTHON / SRD_OUTPUT_BINARY / SRD_OUTPUT_LOGIC / SRD_OUTPUT_META） |
| `c_decoder_register_output_meta(di, type, id, meta_type, meta_name, meta_descr)` | 注册META输出（含元数据描述） <!-- Updated: c_decoder_register_output_meta已实现 --> |
| `c_decoder_get_samplerate(di)` | 获取采样率 |
| `c_decoder_get_option_string(di, id, default)` | 获取字符串选项 |
| `c_decoder_get_option_int(di, id, default)` | 获取整数选项 |
| `c_decoder_put_python(di, ss, es, out_id, cmd, data, len)` | 输出 Python 协议数据 |
| `c_decoder_put_binary(di, ss, es, out_id, bin_cls, size, data)` | 输出二进制数据 |
| `c_decoder_put_logic(di, ss, es, out_id, channel_mask, values, num_channels)` | 输出逻辑通道数据 <!-- Updated: c_decoder_put_logic已实现 --> |
| `c_decoder_put_meta_int(di, ss, es, out_id, value)` | 输出整数元数据 <!-- Updated: c_decoder_put_meta_int已实现 --> |
| `c_decoder_put_meta_double(di, ss, es, out_id, value)` | 输出浮点元数据 <!-- Updated: c_decoder_put_meta_double已实现 --> |
| `c_cond_new()` / `c_cond_free(cb)` | 条件构建器创建/释放 |
| `c_cond_rise(cb, ch)` / `c_cond_fall(cb, ch)` / `c_cond_edge(cb, ch)` | 上升沿/下降沿/边沿条件 |
| `c_cond_high(cb, ch)` / `c_cond_low(cb, ch)` | 高/低电平条件 |
| `c_cond_skip(cb, count)` | 跳过采样点条件 |
| `c_cond_or(cb)` | 条件或 |
| `c_cond_wait(cb, di, &samplenum, &matched)` | 等待条件满足 |
| `c_cond_wait_current(di, &samplenum)` | 等效Python self.wait({})，获取当前采样点 <!-- Updated: c_cond_wait_current已实现 --> |
| `c_decoder_get_pin(di, ch, samplenum)` | 获取指定通道在指定采样点的值 |
| `c_decoder_get_initial_pin(di, ch)` | 获取初始引脚值 <!-- Updated: c_decoder_get_initial_pin已实现 --> |
| `c_decoder_has_channel(di, ch)` | 检查通道是否已连接 |
| `c_decoder_get_last_samplenum(di)` | 获取最后采样点号 |
| `C_ANN_PUT(di, ss, es, out, class, ...)` | 输出注释（变参字符串） |

### srd_c_decoder 结构体

```c
struct srd_c_decoder {
    const char *id;           // 解码器 ID，如 "flexray_c"
    const char *name;         // 显示名称
    const char *longname;     // 完整名称
    const char *desc;         // 描述
    const char *license;      // 许可证

    const struct srd_channel *channels;
    int num_channels;
    const struct srd_channel *optional_channels;
    int num_optional_channels;
    const struct srd_decoder_option *options;
    int num_options;

    int num_annotations;
    const char *(*ann_labels)[3];  // [long, short, shortest]
    int num_annotation_rows;
    const struct srd_c_ann_row *annotation_rows;

    const char **inputs;
    int num_inputs;
    const char **outputs;
    int num_outputs;
    const struct srd_decoder_binary *binary;
    int num_binary;
    const char **tags;
    int num_tags;

    void (*reset)(struct srd_decoder_inst *di);
    void (*start)(struct srd_decoder_inst *di);
    void (*decode)(struct srd_decoder_inst *di);
    void (*end)(struct srd_decoder_inst *di);
    void (*metadata)(struct srd_decoder_inst *di, int key, uint64_t value);
    void (*destroy)(struct srd_decoder_inst *di);
    void (*recv_proto)(struct srd_decoder_inst *di, uint64_t start_sample,
                       uint64_t end_sample, const char *cmd,
                       const unsigned char *data, uint64_t data_len);  <!-- Updated: 修正recv_proto签名 -->
};
```

### 导出宏

```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void) { ... }
```

### 通道类型常量

| 常量 | 值 | 含义 |
|------|-----|------|
| SRD_CHANNEL_SCLK | 8 | 时钟线 |
| SRD_CHANNEL_SDATA | 108 | 数据线 |
| SRD_CHANNEL_COMMON | -1 | 通用线（如CS#） <!-- Updated: 补充SRD_CHANNEL_COMMON常量 --> |

---

## 1. FlexRay 解码器 (flexray_c)

### 1.1 Python 解码器分析

#### 元数据

| 字段 | 值 |
|------|-----|
| id | `flexray` |
| name | `FlexRay` |
| longname | `FlexRay` |
| desc | `Automotive network communications protocol.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Automotive']` |

#### 通道定义

| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | `channel` | `Channel` | `FlexRay bus channel` | `dec_flexray_chan_channel` |

#### 选项定义

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| `channel_type` | `Channel type` | `A` | `('A', 'B')` | `dec_flexray_opt_channel_type` |
| `bitrate` | `Bitrate (bit/s)` | `10000000` | `(10000000, 5000000, 2500000)` | `dec_flexray_opt_bitrate` |

#### 注释定义 (annotations)

| 索引 | id | desc |
|------|-----|------|
| 0 | `data` | FlexRay payload data |
| 1 | `tss` | Transmission start sequence |
| 2 | `fss` | Frame start sequence |
| 3 | `reserved-bit` | Reserved bit |
| 4 | `ppi` | Payload preamble indicator |
| 5 | `null-frame` | Nullframe indicator |
| 6 | `sync-frame` | Full identifier |
| 7 | `startup-frame` | Startup frame indicator |
| 8 | `id` | Frame ID |
| 9 | `length` | Data length |
| 10 | `header-crc` | Header CRC |
| 11 | `cycle` | Cycle code |
| 12 | `data-byte` | Data byte |
| 13 | `frame-crc` | Frame CRC |
| 14 | `fes` | Frame end sequence |
| 15 | `bss` | Byte start sequence |
| 16 | `warning` | Warning |
| 17 | `bit` | Bit |
| 18 | `cid` | Channel idle delimiter |
| 19 | `dts` | Dynamic trailing sequence |
| 20 | `cas` | Collision avoidance symbol |

#### 注释行 (annotation_rows)

| id | label | class_tuple |
|----|-------|-------------|
| `bits` | `Bits` | `(15, 17)` |
| `fields` | `Fields` | `tuple(range(15)) + (18, 19, 20)` = `(0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,18,19,20)` |
| `warnings` | `Warnings` | `(16,)` |

#### 常量定义 (Const 类)

| 常量 | 值 | 含义 |
|------|-----|------|
| cChannelIdleDelimiter | 11 | 通道空闲定界符长度 |
| cCrcInitA | 0xFEDCBA | 通道 A CRC 初始值 |
| cCrcInitB | 0xABCDEF | 通道 B CRC 初始值 |
| cCrcPolynomial | 0x5D6DCB | CRC 多项式 |
| cCrcSize | 24 | CRC 大小（位） |
| cCycleCountMax | 63 | 最大循环计数 |
| cdBSS | 2 | 字节开始序列长度 |
| cdCAS | 30 | 碰撞避免符号长度 |
| cdFES | 2 | 帧结束序列长度 |
| cdFSS | 1 | 帧开始序列长度 |
| cHCrcInit | 0x01A | 头 CRC 初始值 |
| cHCrcPolynomial | 0x385 | 头 CRC 多项式 |
| cHCrcSize | 11 | 头 CRC 大小（位） |
| cSamplesPerBit | 8 | 每位采样数 |
| cSlotIDMax | 2047 | 最大时隙 ID |
| cStaticSlotIDMax | 1023 | 最大静态时隙 ID |
| cVotingSamples | 5 | 投票采样数 |

#### 是否需要 samplerate

**是** — `metadata()` 回调中计算 `bit_width` 和 `sample_point`。decode() 开头检查 samplerate 是否存在。

#### 是否输出到其他解码器

**否** — outputs 为空列表，无 OUTPUT_PYTHON 注册。

### 1.2 状态机分析

#### 主状态

| 状态 | 描述 |
|------|------|
| `IDLE` | 等待总线显性状态（逻辑 0） |
| `GET BITS` | 按采样点逐位读取 |

#### IDLE 状态逻辑

1. 等待通道 0 变低 (`{0: 'l'}`) → 记录 `tss_start`
2. 等待通道 0 变高 (`{0: 'h'}`) → 记录 `tss_end`
3. 调用 `dom_edge_seen(force=True)` 记录边沿
4. 转入 `GET BITS`

#### GET BITS 状态逻辑

1. 计算下一个采样点位置：`pos = get_sample_point(curbit)`
2. 等待条件：`[{'skip': pos - samplenum}, {0: 'f'}]`
   - 如果匹配下降沿 (`matched & 0b10`)：调用 `dom_edge_seen()` 更新时钟同步
   - 如果匹配 skip (`matched & 0b01`)：调用 `handle_bit(fr_rx)` 处理位

#### handle_bit() 逻辑 — 按位号处理

1. 将位加入 `rawbits` 和 `bits` 数组
2. 检查是否为 BSS（字节开始序列）：
   - `is_bss_sequence()` 判断条件：`end_of_frame` 为 False 且 `(len(rawbits)-2)%10==0` 或 `(len(rawbits)-3)%10==0`
   - 如果是 BSS：从 `bits` 中弹出，输出 BSS 注释，`curbit++`，返回
   - 如果不是 BSS 且 bitnum > 1：输出 bit 注释
3. 按位号（bitnum = len(bits)-1）处理各字段：

| bitnum | 字段 | 注释类 | 格式 |
|--------|------|--------|------|
| 0 | FSS | 记录 ss_bit0 | — |
| 1 | 保留位 | ANN_RESERVED_BIT(3) | `Reserved bit: %d` / `RB: %d` / `RB` |
| 2 | PPI | ANN_PPI(4) | `Payload preamble indicator: %d` / `PPI: %d` |
| 3 | NF | ANN_NULL_FRAME(5) | `Null frame indicator: %s` / `NF: %d` / `NF` |
| 4 | Sync | ANN_SYNC_FRAME(6) | `Sync frame indicator: %d` / `Sync: %d` / `Sync` |
| 5 | Startup | ANN_STARTUP_FRAME(7) | `Startup frame indicator: %d` / `Startup: %d` / `Startup` |
| 6 | ID 开始 | 记录 ss_block | — |
| 16 | ID 结束 | ANN_ID(8) | `Frame ID: %d` / `ID: %d` / `%d` |
| 17 | Length 开始 | 记录 ss_block | — |
| 23 | Length 结束 | ANN_LENGTH(9) | `Payload length: %d` / `Length: %d` / `%d` |
| 24 | HCRC 开始 | 记录 ss_block | — |
| 34 | HCRC 结束 | ANN_HEADER_CRC(10) | `Header CRC: 0x%X (%s)` / `0x%X (%s)` / `0x%X` |
| 35 | Cycle 开始 | 记录 ss_block | — |
| 40 | Cycle 结束 | ANN_CYCLE(11) | `Cycle: %d` / `Cyc: %d` / `%d`，计算 last_databit |
| 41~last_databit-1 | 数据位 | 记录 ss_databytebits | — |
| last_databit | 数据结束 | ANN_DATA_BYTE(12) | `Data byte %d: 0x%02x` / `DB%d: 0x%02x` / `%02X` |
| last_databit+23 | Frame CRC | ANN_FRAME_CRC(13) | `Frame CRC: 0x%X (%s)` / `0x%X (%s)` / `0x%X` |
| last_databit+24 | FES 开始 | 记录 ss_block | — |
| last_databit+25 | FES | ANN_FES(14) | `Frame end sequence` / `FES` |
| last_databit+26 | DTS 检查 | — | 检查是否为动态帧 |
| last_xmit_bit | CID 开始 | 记录 ss_block | — |
| last_xmit_bit+10 | CID | ANN_CID(18) | `Channel idle delimiter` / `CID`，reset_variables() |

#### 特殊逻辑

- **bitnum == 1 时**：检查 rawbits[:3] 是否为 [1,1,0]（正常帧）或 [1,1,1]（CAS 碰撞避免符号）
  - [1,1,0]：输出 TSS 注释、FSS 注释、保留位注释
  - [1,1,1]：输出 CAS 注释，reset_variables()
- **CRC 算法**：通用 CRC，支持任意位数和多项式，用于 11 位头 CRC 和 24 位帧 CRC
- **时钟同步**：`dom_edge_seen()` 在检测到显性边沿时更新参考点，`get_sample_point()` 基于此计算采样点
- **putg() 辅助函数**：在注释范围两侧各扩展 sample_point 和 (bit_width - sample_point) 的宽度

### 1.3 C 实现计划

#### 私有数据结构

```c
typedef struct {
    int state;                  // STATE_IDLE / STATE_GET_BITS
    uint64_t samplerate;
    double bit_width;
    double sample_point;
    int sample_point_percent;   // 固定为 50

    uint8_t rawbits[4096];     // 所有位（含 BSS）
    int num_rawbits;
    uint8_t bits[4096];        // FlexRay 帧位（不含 BSS）
    int num_bits;
    int curbit;

    uint64_t tss_start, tss_end;
    uint64_t ss_block;
    uint64_t ss_bit0, ss_bit1, ss_bit2;
    uint64_t ss_databytebits[2048];
    int num_databytebits;

    int last_databit;
    int last_xmit_bit;
    int end_of_frame;
    int dynamic_frame;

    uint32_t frame_id;
    uint32_t payload_length;
    uint32_t header_crc;
    uint32_t frame_crc;
    uint32_t cycle;

    uint64_t dom_edge_snum;
    int dom_edge_bcount;

    int channel_type;           // 0=A, 1=B
    int bitrate;

    int out_ann;
} flexray_state;
```

#### 枚举定义

```c
enum {
    STATE_IDLE,
    STATE_GET_BITS,
};

enum {
    ANN_DATA = 0,
    ANN_TSS,
    ANN_FSS,
    ANN_RESERVED_BIT,
    ANN_PPI,
    ANN_NULL_FRAME,
    ANN_SYNC_FRAME,
    ANN_STARTUP_FRAME,
    ANN_ID,
    ANN_LENGTH,
    ANN_HEADER_CRC,
    ANN_CYCLE,
    ANN_DATA_BYTE,
    ANN_FRAME_CRC,
    ANN_FES,
    ANN_BSS,
    ANN_WARNING,
    ANN_BIT,
    ANN_CID,
    ANN_DTS,
    ANN_CAS,
    NUM_ANN,
};
```

#### 关键函数签名

```c
static void flexray_reset(struct srd_decoder_inst *di);
static void flexray_start(struct srd_decoder_inst *di);
static void flexray_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void flexray_decode(struct srd_decoder_inst *di);
static void flexray_destroy(struct srd_decoder_inst *di);

// 辅助函数
static uint32_t flexray_crc(uint32_t data, int data_len_bits, uint32_t polynom, int crc_len_bits, uint32_t iv, uint32_t xor_val);
static int is_bss_sequence(flexray_state *s);
static void handle_bit(flexray_state *s, struct srd_decoder_inst *di, uint8_t fr_rx, uint64_t samplenum);
static void dom_edge_seen(flexray_state *s, uint64_t samplenum);
static uint64_t get_sample_point(flexray_state *s, int bitnum);
static void putg(flexray_state *s, struct srd_decoder_inst *di, uint64_t ss, uint64_t es, int ann_class, ...);
static void putx(flexray_state *s, struct srd_decoder_inst *di, uint64_t samplenum, int ann_class, ...);
static void putb(flexray_state *s, struct srd_decoder_inst *di, uint64_t ss, uint64_t es, int ann_class, ...);
static void reset_variables(flexray_state *s);
```

### 1.4 关键实现注意事项

1. **CRC 算法**：Python 版本使用静态方法 `crc(data, data_len_bits, polynom, crc_len_bits, iv=0, xor=0)`，C 版本需实现完全相同的算法。注意 Python 版 iv 和 xor 先异或再处理。
2. **BSS 检测**：基于 rawbits 长度判断，`(len-2)%10==0` 或 `(len-3)%10==0`。end_of_frame 为 True 时不再检测 BSS。
3. **时钟同步**：`dom_edge_seen()` 更新参考点，`get_sample_point()` 基于参考点计算。Python 用浮点运算，C 版本也需用 double。
4. **putg() 扩展**：注释范围左右各扩展 sample_point 和 (bit_width - sample_point)，需用 int 截断。
5. **动态帧处理**：bitnum == last_databit+26 时检查 fr_rx，若为 0 则设 dynamic_frame=True。之后在 bitnum > last_databit+27 时检测 DTS。
6. **CAS 符号**：rawbits[:3] == [1,1,1] 时输出 CAS 注释并重置。
7. **Header CRC 验证**：使用 bits[4:24] 计算，多项式 0x385，初始值 0x01A，11 位。
8. **Frame CRC 验证**：使用 bits[1:-24] 计算，多项式 0x5D6DCB，初始值根据通道类型选择 0xFEDCBA(A) 或 0xABCDEF(B)，24 位。
9. **数据字节**：payload_length 是实际数据字节数的一半，数据字节数 = 2 * payload_length。
10. **位数组大小**：最大帧约 254 数据字节 + 头部 + BSS，rawbits 最大约 254*10 + 50 ≈ 2590，设 4096 足够。

### 1.5 与 Python 版本的差异处理

| 差异点 | Python | C 处理方式 |
|--------|--------|-----------|
| 动态列表 | `self.rawbits = []` / `self.bits = []` | 固定大小数组 + 计数器 |
| 字符串位拼接 | `''.join(str(d) for d in self.bits[6:])` | `bitpack_msb()` 函数将位数组打包为整数 |
| 列表切片 | `self.bits[4:24]` | 指针偏移 + 长度参数 |
| putx/putb 辅助 | 使用 self.samplenum / self.ss_block | 显式传递 samplenum 参数 |
| 异常处理 | `raise SamplerateError` | 直接 return |
| 选项访问 | `self.options['channel_type']` | `c_decoder_get_option_string()` |

---

## 2. MIPI RFFE 解码器 (mipi_rffe_c)

### 2.1 Python 解码器分析

#### 元数据

| 字段 | 值 |
|------|-----|
| id | `mipi_rffe` |
| name | `MIPI_RFFE` |
| longname | `RF Front-End Control Interface` |
| desc | `Two-wire, single-master, serial bus.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['mipi_rffe']` |
| tags | `['Embedded/industrial']` |

#### 通道定义

| 索引 | id | type | name | desc | idn |
|------|-----|------|------|------|-----|
| 0 | `sclk` | 8 (SCLK) | `SCLK` | `Serial clock line` | `dec_mipi_rffe_chan_sclk` |
| 1 | `sdata` | 108 (SDATA) | `SDATA` | `Serial data line` | `dec_mipi_rffe_chan_sdata` |

#### 选项定义

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| `error_display` | `Error display options` | `display` | `('display', 'not_display')` | `dec_mipi_rffe_opt_error_display` |

#### 注释定义 (annotations)

| 索引 | id | desc |
|------|-----|------|
| 0 | `ssc` | Sequence Start Condition |
| 1 | `sa` | Slave Address |
| 2 | `erw` | Extended register write |
| 3 | `err` | Extended register read |
| 4 | `erwl` | Extended register write long |
| 5 | `errl` | Extended register read long |
| 6 | `rw` | Register write |
| 7 | `rr` | Register read |
| 8 | `r0w` | Register 0 write |
| 9 | `bc` | Byte |
| 10 | `p` | Parity |
| 11 | `address` | Address |
| 12 | `bp` | Bus pack |
| 13 | `data` | DATA |
| 14 | `cmd_warnings` | Command warnings |
| 15 | `bic` | Bus Idle Condition |
| 16 | `bc_warnings` | BC warnings |
| 17 | `ije` | Illegal Jump Edge |
| 18 | `pw` | Parity warnings |

#### 注释行 (annotation_rows)

| id | label | class_tuple |
|----|-------|-------------|
| `command-data` | `Command/Data` | `(0,1,2,3,4,5,6,7,8,9,10,11,12,13,15)` |
| `warnings` | `Warnings` | `(14,16,17,18)` |

#### proto 字典（命令到注释映射）

```python
proto = {
    'SSC':    [0, 'Sequence Start Condition', 'SSC'],
    'SA':     [1, 'Slave Address', 'SA'],
    'ERW':    [2, 'Extended Register Write', 'ERW'],
    'ERR':    [3, 'Extended Register Read', 'ERR'],
    'ERWL':   [4, 'Extended Register Write Long', 'ERWL'],
    'ERRL':   [5, 'Extended Register Read Long', 'ERRL'],
    'RW':     [6, 'Register Write', 'RW'],
    'RR':     [7, 'Register Read', 'RR'],
    'R0W':    [8, 'Register 0 Write', 'R0W'],
    'BC':     [9, 'Byte', 'BC'],
    'P':      [10, 'Parity', 'P'],
    'ADDRESS':[11, 'Address', 'A'],
    'BP':     [12, 'Bus Pack', 'BP'],
    'DATA':   [13, 'Data', 'DATA'],
    'CMD_WARNINGS': [14, 'Command Warnings', 'CMD_WARN'],
    'BIC':    [15, 'Bus Idle Condition', 'BIC'],
    'BC_WARNINGS': [16, 'BC Warnings', 'BC_WARN'],
    'IJE':    [17, 'Illegal Jump Edge', 'IJE_WAEN'],
    'PW':     [18, 'Parity warnings', 'P_WAEN'],
}
```

#### 是否需要 samplerate

**是** — `metadata()` 回调中保存 samplerate，但解码逻辑本身不直接使用采样率进行计算（不像 flexray 那样计算采样点）。

#### 是否输出到其他解码器

**是** — outputs 为 `['mipi_rffe']`，注册了 OUTPUT_PYTHON。

**注意**：C解码器只能依赖已有C实现的底层解码器，不依赖Python解码器。如果上层解码器仅有Python实现，则OUTPUT_PYTHON输出仅用于同类型C解码器间通信。 <!-- Updated: 添加C解码器依赖规则 -->

### 2.2 状态机分析

#### 主状态

| 状态 | 描述 |
|------|------|
| `FIND SSC` | 寻找序列开始条件 |
| `FIND SLAVE ADDRESS` | 读取从机地址（4位） |
| `FIND COMMAND` | 解码命令类型 |
| `FIND BTEY_COUNT` | 读取字节计数 |
| `FIND ADDRESS` | 读取寄存器地址 |
| `FIND DATA` | 读取数据 |
| `FIND PARITY` | 校验奇偶位 |
| `FIND BUS_PARK` | 处理总线停靠 |

#### FIND SSC 状态逻辑

1. 等待 SCLK 低且 SDATA 上升沿：`{0: 'l', 1: 'r'}` → 记录 BPss
2. 等待 SCLK 高或 SCLK 低+SDATA 下降沿：`[{0: 'h'}, {0: 'l', 1: 'f'}]`
   - 如果 SCLK 高：continue（不是 SSC）
   - 如果 SCLK 低+SDATA 下降沿：继续
3. 等待 SCLK 低+SDATA 边沿或 SCLK 高：`[{0: 'l', 1: 'e'}, {0: 'r'}]`
   - 如果 SCLK 低+SDATA 边沿：continue
   - 如果 SCLK 高：输出 SSC 注释，转入 FIND SLAVE ADDRESS

#### handle() 函数 — 通用数据读取

参数：`cmd`（命令名），`state`（下一状态），`key`（高位位数），`key0`（低位位数）

1. 如果 bitcount == 0：记录 DATAss
2. 循环读取 key 位数据：
   - 如果 `_display` 模式：等待 SCLK 上升沿或 SCLK 低+SDATA 边沿
     - SCLK 上升沿：左移 databyte，加入 sdata
     - SCLK 低+SDATA 边沿：输出 IJE 警告，继续
   - 否则：只等 SCLK 上升沿
3. 继续循环读取剩余位（同上逻辑）
4. 等待 SCLK 下降沿
5. 处理读取的数据：
   - 如果 cmd == 'BC'：设置 BC = d+1，检查范围
   - 如果 cmd == 'P'：校验奇偶位，输出结果
   - 否则：输出数据注释
6. 设置下一状态

#### handle_CMD() 函数 — 命令解码

按 bitcount 逐步解码命令类型：

| bitcount | 条件 | 结果 |
|----------|------|------|
| 0 | sdata=1 | cmdset('R0W', 'FIND DATA') |
| 1 | sdata=1 | extended=0; sdata=0 → extended=1 |
| 2 | !extended | sdata=1 → cmdset('RR','FIND ADDRESS'); sdata=0 → cmdset('RW','FIND ADDRESS') |
| 2 | extended | isWrite = !sdata |
| 3 | extended, !isWrite, !sdata | cmdset('ERR','FIND BTEY_COUNT') |
| 3 | extended, isWrite, !sdata | cmdset('ERW','FIND BTEY_COUNT') |
| 3 | !extended | 输出 CMD_WARNINGS，init() |
| 4 | sdata=1 | cmdset('ERRL','FIND BTEY_COUNT') |
| 4 | sdata=0 | cmdset('ERWL','FIND BTEY_COUNT') |

#### Parity() 函数

奇偶校验计算：
1. 根据 cmdkey 和 Pcount 调整 Pdata
2. 计算 Pdata 中 1 的个数（Popcount），得到奇偶性
3. 与实际奇偶位比较

#### FIND PARITY 状态逻辑

根据 cmdkey 和 Pcount 决定下一状态：

| cmdkey | Pcount | 下一状态 |
|--------|--------|----------|
| R0W | 1 | FIND BUS_PARK |
| ERW | 1 | FIND ADDRESS (ADDcount=1) |
| ERW | 2 | FIND DATA |
| ERW | BC+2 | FIND BUS_PARK |
| ERW | >2 | FIND DATA |
| ERR | 1 | FIND ADDRESS (ADDcount=1) |
| ERR | 2 | FIND BUS_PARK (BPcount=1) |
| ERR | BC+2 | FIND BUS_PARK (BPcount=2) |
| ERR | >2 | FIND DATA |
| ERWL | 1 | FIND ADDRESS (ADDcount=2) |
| ERWL | 2 | FIND ADDRESS (ADDcount=1) |
| ERWL | 3 | FIND DATA |
| ERWL | BC+3 | FIND BUS_PARK |
| ERWL | >3 | FIND DATA |
| ERRL | 1 | FIND ADDRESS (ADDcount=2) |
| ERRL | 2 | FIND ADDRESS (ADDcount=1) |
| ERRL | 3 | FIND BUS_PARK (BPcount=1) |
| ERRL | BC+3 | FIND BUS_PARK (BPcount=2) |
| ERRL | >3 | FIND DATA |
| RW | 1 | FIND DATA |
| RW | 2 | FIND BUS_PARK (BPcount=1) |
| RR | 1 | FIND BUS_PARK (BPcount=1) |
| RR | 2 | FIND BUS_PARK (BPcount=2) |

#### FIND BUS_PARK 状态逻辑

1. 等待 SCLK 低+SDATA 低：`{0: 'l', 1: 'l'}`
2. 如果命令是读操作（ERR/ERRL/RR）：
   - BPcount==1 → key=0，转入 FIND DATA
   - BPcount==2 → key=1，转入 FIND DATA 后 init()
3. 否则：转入 FIND SSC，init()

### 2.3 C 实现计划

#### 私有数据结构

```c
typedef struct {
    uint64_t samplerate;
    uint64_t ss, es;
    int bitcount;
    uint32_t databyte;
    int state;
    int extended;           // -1=未定, 0=基本, 1=扩展
    char cmdkey[8];         // "NULL"/"ERW"/"ERR"/"ERWL"/"ERRL"/"RW"/"RR"/"R0W"
    int BC;
    int bits;               // 剩余数据位数
    int Pcount;
    int BPcount;
    int ADDcount;
    uint64_t BPss;
    uint64_t SSCs;
    uint64_t Pes;
    int sdata;
    int Pdata;
    int Pkey;
    int parity;
    int isWrite;
    int isLong;
    int _display;           // 1=display errors, 0=not
    uint64_t DATAss;
    int out_ann;
    int out_python;
} mipi_rffe_state;
```

#### 枚举定义

```c
enum {
    STATE_FIND_SSC,
    STATE_FIND_SLAVE_ADDRESS,
    STATE_FIND_COMMAND,
    STATE_FIND_BYTE_COUNT,
    STATE_FIND_ADDRESS,
    STATE_FIND_DATA,
    STATE_FIND_PARITY,
    STATE_FIND_BUS_PARK,
};

enum {
    ANN_SSC = 0,
    ANN_SA,
    ANN_ERW,
    ANN_ERR,
    ANN_ERWL,
    ANN_ERRL,
    ANN_RW,
    ANN_RR,
    ANN_R0W,
    ANN_BC,
    ANN_P,
    ANN_ADDRESS,
    ANN_BP,
    ANN_DATA,
    ANN_CMD_WARNINGS,
    ANN_BIC,
    ANN_BC_WARNINGS,
    ANN_IJE,
    ANN_PW,
    NUM_ANN,
};
```

### 2.4 关键实现注意事项

1. **cmdkey 字符串比较**：Python 用字符串比较，C 版本可用枚举或 strncmp。建议用枚举 `CMD_NONE, CMD_ERW, CMD_ERR, CMD_ERWL, CMD_ERRL, CMD_RW, CMD_RR, CMD_R0W`。
2. **奇偶校验调整**：Parity() 函数中根据 cmdkey 和 Pcount 调整 Pdata，逻辑复杂需仔细实现。
3. **IJE 检测**：在 `_display` 模式下，SCLK 低电平期间 SDATA 发生变化时输出 IJE 警告。
4. **BC 范围检查**：ERW/ERR 命令 BC 范围 1-16，其他命令 BC 范围 1-8。
5. **handle() 中循环等待**：Python 版本在 handle() 中有 while True 循环等待 SCLK 上升沿，C 版本需用 c_cond_wait 实现。
6. **Bus Park 处理**：读操作有两个 Bus Park（中间和末尾），写操作只有一个（末尾）。
7. **OUTPUT_PYTHON 格式**：需实现 Python 输出，格式为 `[SSC, <Command Frame>, <Data Frame>]`。

### 2.5 与 Python 版本的差异处理

| 差异点 | Python | C 处理方式 |
|--------|--------|-----------|
| 字符串 cmdkey | `self.cmdkey = 'ERW'` | 枚举值 |
| proto 字典查找 | `proto['SSC'][0]` | 常量数组 |
| 动态状态转换 | `self.state = 'FIND DATA'` | 枚举赋值 |
| while True 循环 | 在 handle() 中循环等待 | c_cond_wait 循环 |
| self.matched 位检查 | `self.matched & (0b1 << 0)` | `matched & 1` / `matched & 2` |

---

## 3. USB Power Delivery 解码器 (usb_power_delivery_c)

### 3.1 Python 解码器分析

#### 元数据

| 字段 | 值 |
|------|-----|
| id | `usb_power_delivery` |
| name | `USB PD` |
| longname | `USB Power Delivery` |
| desc | `USB Power Delivery protocol.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['usb_pd']` |
| tags | `['PC']` |

#### 通道定义

| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | `cc1` | `CC1` | `Configuration Channel 1` | `dec_usb_power_delivery_chan_cc1` |

#### 可选通道定义

| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | `cc2` | `CC2` | `Configuration Channel 2` | `dec_usb_power_delivery_opt_chan_cc2` |

#### 选项定义

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| `fulltext` | `Full text decoding of packets` | `no` | `('yes', 'no')` | `dec_usb_power_delivery_opt_fulltext` |

#### 注释定义 (annotations)

| 索引 | id | desc |
|------|-----|------|
| 0 | `type` | Packet Type |
| 1 | `preamble` | Preamble |
| 2 | `sop` | Start of Packet |
| 3 | `header` | Header |
| 4 | `data` | Data |
| 5 | `crc` | Checksum |
| 6 | `eop` | End Of Packet |
| 7 | `sym` | 4b5b symbols |
| 8 | `warnings` | Warnings |
| 9 | `src` | Source Message |
| 10 | `snk` | Sink Message |
| 11 | `payload` | Payload |
| 12 | `text` | Plain text |

#### 注释行 (annotation_rows)

| id | label | class_tuple |
|----|-------|-------------|
| `4b5b` | `Symbols` | `(7,)` |
| `phase` | `Parts` | `(1,2,3,4,5,6)` |
| `payload` | `Payload` | `(11,)` |
| `type` | `Type` | `(0,9,10)` |
| `warnings` | `Warnings` | `(8,)` |
| `text` | `Full text` | `(12,)` |

#### 二进制输出 (binary)

| 索引 | id | desc |
|------|-----|------|
| 0 | `raw-data` | RAW binary data |

#### 是否需要 samplerate

**是** — `metadata()` 回调中计算 `maxbit` 和 `threshold`。decode() 开头检查 samplerate。

#### 是否输出到其他解码器

**是** — outputs 为 `['usb_pd']`，注册了 OUTPUT_PYTHON、OUTPUT_BINARY 和 OUTPUT_META。

**OUTPUT_META 注册方式**：使用 `c_decoder_register_output_meta(di, SRD_OUTPUT_META, "usb_pd", "bitrate", "Bitrate", "Bitrate of USB PD communication")` 注册，然后使用 `c_decoder_put_meta_int(di, ss, es, out_bitrate, bitrate_value)` 输出。 <!-- Updated: META输出API已实现 -->

**注意**：C解码器只能依赖已有C实现的底层解码器，不依赖Python解码器。 <!-- Updated: 添加C解码器依赖规则 -->

### 3.2 常量定义

#### BMC 编码常量

| 常量 | 值 | 含义 |
|------|-----|------|
| UI_US | 1000000/600000.0 ≈ 1.6667 | 单位间隔（微秒） |
| THRESHOLD_US | (UI_US + 2*UI_US)/2 ≈ 2.5 | half-1 与 0 的阈值 |

#### 4b5b 解码表 (DEC4B5B)

32 项查找表，将 5 位编码映射为 4 位数据或控制符号：

```c
static const int DEC4B5B[32] = {
    0x10, 0x10, 0x10, 0x10,  // Error
    0x10, 0x10, 0x13, 0x14,  // Sync-3, RST-1
    0x10, 0x01, 0x04, 0x05,  // 1, 4, 5
    0x10, 0x16, 0x06, 0x07,  // EOP, 6, 7
    0x10, 0x12, 0x08, 0x09,  // Sync-2, 8, 9
    0x02, 0x03, 0x0A, 0x0B,  // 2, 3, A, B
    0x11, 0x15, 0x0C, 0x0D,  // Sync-1, RST-2, C, D
    0x0E, 0x0F, 0x00, 0x10,  // E, F, 0, Error
};
```

#### 符号常量

| 常量 | 值 | 含义 |
|------|-----|------|
| SYM_ERR | 0x10 | 错误符号 |
| SYNC1 | 0x11 | 同步符号 1 |
| SYNC2 | 0x12 | 同步符号 2 |
| SYNC3 | 0x13 | 同步符号 3 |
| RST1 | 0x14 | 复位符号 1 |
| RST2 | 0x15 | 复位符号 2 |
| EOP | 0x16 | 包结束 |

#### SOP 序列与映射

7 个 SOP 序列映射到 START_OF_PACKETS 字典：

| 序列 | 名称 |
|------|------|
| (SYNC1, SYNC1, SYNC1, SYNC2) | SOP |
| (SYNC1, SYNC1, SYNC3, SYNC3) | SOP' |
| (SYNC1, SYNC3, SYNC1, SYNC3) | SOP" |
| (SYNC1, RST2, RST2, SYNC3) | SOP' Debug |
| (SYNC1, RST2, SYNC3, SYNC2) | SOP" Debug |
| (RST1, SYNC1, RST1, SYNC3) | Cable Reset |
| (RST1, RST1, RST1, RST2) | Hard Reset |

#### 控制消息类型 (CTRL_TYPES)

24 种控制消息类型（0-24），包括 reserved, GOOD CRC, GOTO MIN, ACCEPT, REJECT 等。

#### 数据消息类型 (DATA_TYPES)

12 种数据消息类型，包括 SOURCE CAP, REQUEST, BIST, SINK CAP, VDM 等。

#### 扩展消息类型 (EXTENDED_TYPES)

18 种扩展消息类型（1-30），包括 Source_Cap_Extended, Status, EPR_Mode 等。

#### RDO 标志位 (RDO_FLAGS)

5 种 RDO 标志位：unchunked, no_suspend, comm_cap, cap_mismatch, give_back。

#### BIST 模式 (BIST_MODES)

8 种 BIST 模式（0-7）。

#### VDM 命令 (VDM_CMDS)

8 种 VDM 命令（1-6, 16-17）。

#### VDM ACK 类型

4 种：REQ, ACK, NAK, BSY。

#### EPR 模式 (EPR_MODE_ACTION / EPR_MODE_DATA)

5 种 EPR 动作和 6 种 EPR 失败原因。

#### 扩展控制消息类型 (EXT_CONTROL_MSG_TYPES)

4 种扩展控制消息。

#### 符号名称表 (SYM_NAME)

23 项，每项 [long, short] 格式。

### 3.3 状态机分析

此解码器没有传统意义上的显式状态机。它采用两阶段处理：

**阶段 1：BMC 解码（decode() 函数）**

1. 等待 CC1 或 CC2 边沿，或超时（samplerate/1000 采样点）
2. 首次采样：记录 startsample 和 previous
3. 计算与上次边沿的时间差 diff
4. 如果 diff > maxbit（3*UI_US 对应的采样数）：
   - 将 previous 加入 edges
   - 调用 decode_packet() 处理已收集的位
   - 重置为下一包
5. 否则根据 diff 和 threshold 判断 BMC 编码：
   - diff > threshold 且 !half_one → bit=0，记录 edge
   - diff <= threshold 且 half_one → bit=1，记录 edge，half_one=False
   - diff <= threshold 且 !half_one → half_one=True，记录 start_one
   - 其他 → 无效 BMC，记录 bad，bit=0

**阶段 2：包解码（decode_packet() 函数）**

1. 检查 edges 数量 >= 50
2. scan_eop()：扫描 4b5b 符号寻找 SOP 序列
   - 遍历 bits 数组，每 5 位解码一个 4b5b 符号
   - 检查连续 4 个符号是否匹配 SOP 序列
   - 支持 3/4 匹配的容错检测
   - 找到后输出 Preamble 注释和 SOP 注释
   - Hard Reset / Cable Reset 返回 -1
3. 读取包头（16 位 = 4 个 4b5b 符号）
4. 解码包头字段：
   - head_ext() = (head>>15)&1
   - head_count() = (head>>12)&7
   - head_id() = (head>>9)&7
   - head_power_role() = (head>>8)&1
   - head_rev() = ((head>>6)&3)+1
   - head_data_role() = (head>>5)&1
   - head_type() = head&0x1F
5. 输出 Source/Sink Message 注释
6. 如果是扩展消息：读取扩展头，处理分块数据
7. 如果是控制/数据消息：读取数据字
8. CRC32 校验
9. 检查 EOP
10. 输出元数据（bitrate）和二进制数据

### 3.4 C 实现计划

#### 私有数据结构

```c
typedef struct {
    uint64_t samplerate;
    uint64_t maxbit;
    uint64_t threshold;

    // BMC 解码状态
    uint64_t previous;
    uint64_t startsample;
    uint8_t bits[4096];
    int num_bits;
    uint64_t edges[4096];
    int num_edges;
    int half_one;
    uint64_t start_one;

    // 包解码状态
    int idx;
    int packet_seq;
    uint16_t head;
    uint16_t ext_head;
    uint32_t data[16];
    uint32_t ext_data[16];
    int num_data;
    int chunked;
    int chunk_num;
    int req_chunk;
    int data_size;

    // 存储的 PDO 信息
    char stored_pdos[17][64];
    int cap_mark[17];

    // 文本追踪
    char text[2048];

    int out_ann;
    int out_python;
    int out_binary;
    int out_bitrate;
} usb_pd_state;
```

#### 枚举定义

```c
enum {
    ANN_TYPE = 0,
    ANN_PREAMBLE,
    ANN_SOP,
    ANN_HEADER,
    ANN_DATA,
    ANN_CRC,
    ANN_EOP,
    ANN_SYM,
    ANN_WARNINGS,
    ANN_SRC,
    ANN_SNK,
    ANN_PAYLOAD,
    ANN_TEXT,
    NUM_ANN,
};
```

#### 关键函数签名

```c
static void usb_pd_reset(struct srd_decoder_inst *di);
static void usb_pd_start(struct srd_decoder_inst *di);
static void usb_pd_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void usb_pd_decode(struct srd_decoder_inst *di);
static void usb_pd_destroy(struct srd_decoder_inst *di);

static void decode_packet(usb_pd_state *s, struct srd_decoder_inst *di);
static int scan_eop(usb_pd_state *s, struct srd_decoder_inst *di);
static uint16_t get_short(usb_pd_state *s, struct srd_decoder_inst *di);
static uint32_t get_word(usb_pd_state *s, struct srd_decoder_inst *di);
static int get_sym(usb_pd_state *s, struct srd_decoder_inst *di, int i, int rec);
static uint32_t compute_crc32(usb_pd_state *s);
static void puthead(usb_pd_state *s, struct srd_decoder_inst *di);
static void putpayload(usb_pd_state *s, struct srd_decoder_inst *di, int s0, int s1, int idx);
static const char *find_corrupted_sop(int k[4]);
```

### 3.5 关键实现注意事项

1. **BMC 解码**：这是最复杂的部分。Python 版本在 decode() 的 while True 循环中逐边沿处理，C 版本需要用 c_cond_wait 等待边沿。
2. **4b5b 解码**：5 位到 4 位映射，使用查找表 DEC4B5B[32]。
3. **SOP 检测**：需要检查 7 种 SOP 序列，支持 3/4 匹配容错。
4. **CRC32**：使用 zlib.crc32，C 版本需实现或使用系统 CRC32。Python 版本将 head 和 data 打包为小端字节序列后计算。
5. **PDO 存储**：stored_pdos 字典存储已解码的 PDO 信息，用于后续 REQUEST 消息的解码。C 版本用固定大小数组。
6. **扩展消息**：支持分块传输（chunked），需处理 chunk_num==0（首块）和 chunk_num>0（后续块）。
7. **OUTPUT_META**：输出 bitrate 元数据，使用 `c_decoder_register_output_meta()` 注册，使用 `c_decoder_put_meta_int()` 输出。 <!-- Updated: META输出API已实现 -->
8. **OUTPUT_BINARY**：输出原始 BMC 解码后的位数据。
9. **可选通道 CC2**：Python 版本等待 `{0: 'e'}` 或 `{1: 'e'}`，C 版本需处理可选通道。
10. **超时检测**：`{'skip': int(self.samplerate/1e3)}` 用于检测包间空闲。

### 3.6 与 Python 版本的差异处理

| 差异点 | Python | C 处理方式 |
|--------|--------|-----------|
| zlib.crc32 | `zlib.crc32(bdata)` | 自实现 CRC32 或使用系统库 |
| struct.pack | `struct.pack('<H'+'I'*n, ...)` | 手动小端打包 |
| 动态列表 | `self.bits = []` / `self.edges = []` | 固定大小数组 + 计数器 |
| 字典查找 | `CTRL_TYPES[t]` | 常量字符串数组 |
| stored_pdos 字典 | `self.stored_pdos = {}` | 固定大小字符串数组 |
| text 拼接 | `self.text += '...'` | snprintf 追加 |
| 可选通道 | `optional_channels` | 检查通道是否已连接 |

### 3.7 复杂度评估

**此解码器是 5 个中最复杂的**，原因：
- BMC 编码解码
- 4b5b 符号解码
- 多种 SOP 序列检测（含容错）
- 完整的 PD 协议栈（控制/数据/扩展消息）
- CRC32 校验
- PDO 存储和引用
- 多种输出类型（ANN/PYTHON/BINARY/META）
- 可选通道支持

---

## 4. IEBus 解码器 (iebus_c)

### 4.1 Python 解码器分析

#### 元数据

| 字段 | 值 |
|------|-----|
| id | `iebus` |
| name | `IEBus` |
| longname | `Inter-Equipment Bus` |
| desc | `Inter-Equipment Bus is an automotive communication bus used in Toyota and Honda vehicles` |
| license | `gplv3+` |
| inputs | `['logic']` |
| outputs | `['iebus']` |
| tags | `['Automotive']` |

#### 通道定义

| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | `bus` | `BUS` | `Bus input` | `dec_iebus_chan_bus` <!-- Updated: 补充idn --> |

#### 选项定义

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| `mode` | `Mode` | `Mode 2` | `('Mode 2',)` | `dec_iebus_opt_mode` <!-- Updated: 补充idn --> |
| `bus_polarity` | `Bus polarity` | `idle-low` | `('idle-low', 'idle-high')` | `dec_iebus_opt_bus_polarity` <!-- Updated: 补充idn --> |
| `ignore_nak` | `Ignore NAK condition` | `Disabled` | `('Disabled', 'Enabled')` | `dec_iebus_opt_ignore_nak` <!-- Updated: 补充idn --> |

#### 注释定义 (annotations)

| 索引 | id | desc |
|------|-----|------|
| 0 | `start-bit` | Start bit |
| 1 | `bit` | Bit |
| 2 | `parity` | Parity |
| 3 | `ack` | Acknowledge |
| 4 | `broadcast` | Broadcast flag |
| 5 | `maddr` | Master address |
| 6 | `saddr` | Slave address |
| 7 | `control` | Control |
| 8 | `datalen` | Data Length |
| 9 | `byte` | Data Byte |
| 10 | `warning` | Warning |

#### 注释行 (annotation_rows)

| id | label | class_tuple |
|----|-------|-------------|
| `bits` | `Bits` | `(0,1,2,3)` |
| `fields` | `Raw Fields` | `(4,5,6,7,8,9)` |
| `warnings` | `Warnings` | `(10,)` |

#### 是否需要 samplerate

**是** — `metadata()` 回调中保存 samplerate。bits() 方法中使用 `27e-6 * samplerate` 和 `33e-6 * samplerate` 计算时间。

#### 是否输出到其他解码器

**是** — outputs 为 `['iebus']`，注册了 OUTPUT_PYTHON。

**注意**：C解码器只能依赖已有C实现的底层解码器，不依赖Python解码器。 <!-- Updated: 添加C解码器依赖规则 -->

### 4.2 状态机分析

此解码器没有显式状态变量，而是通过函数调用链实现隐式状态机。

#### 主解码流程 (decode())

1. **header()**：读取帧头
   - 等待总线上升沿（idle-low 模式）或下降沿（idle-high 模式）→ 记录 ss
   - 等待总线下降沿（idle-low）或上升沿（idle-high）→ 记录 es
   - 检查 (es-ss)/samplerate >= 100e-6（开始位宽度）
   - 如果太短：输出警告，返回 (None, None, ss, es)
   - 输出 Start bit 注释
   - 读取广播位：`read_broadcast_bit()`
   - 返回 (1, broadcast_bit, ss, es)

2. **主地址**：`value(12)` 读取 12 位
   - 输出 Master address 注释
   - 读取奇偶校验位：`parity_bit(master_addr)`
   - 输出 PYTHON: `['MASTER ADDRESS', (master_addr, parity_bit)]`

3. **从地址**：`value(12)` 读取 12 位
   - 输出 Slave address 注释
   - 读取奇偶校验位和 ACK 位
   - 如果 NAK：输出 PYTHON: `['NAK', None]`，continue 重新搜索

4. **控制位**：`value(4)` 读取 4 位
   - 如果匹配 Commands 枚举：输出命令名称
   - 否则：输出十六进制值
   - 读取奇偶校验位和 ACK 位
   - NAK 检查

5. **数据长度**：`value(8)` 读取 8 位
   - data_len==0 时设为 256
   - 如果 data_len > 128：输出警告
   - 读取奇偶校验位和 ACK 位
   - NAK 检查

6. **数据字节**：`handle_data_bytes(data_len)` 读取 data_len 个字节
   - 每字节 8 位 + 奇偶位 + ACK 位
   - NAK 时中断

#### bits() 方法 — 核心位读取

1. 等待同步边沿（上升沿或下降沿，取决于极性）
2. 记录 bit_start
3. 跳过 27µs（采样点在数据阶段中间）
4. 读取位值（取反，因为总线空闲低时高电平=0）
5. 如果 idle-high 极性，再次取反
6. 计算位结束时间：bit_start + 33µs * samplerate
7. 输出 Bit 注释

#### Commands 枚举

| 值 | 名称 |
|----|------|
| 0x00 | READ_STATUS |
| 0x03 | READ_DATA_LOCK |
| 0x04 | READ_LOCK_ADDR_LO |
| 0x05 | READ_LOCK_ADDR_HI |
| 0x06 | READ_STATUS_UNLOCK |
| 0x07 | READ_DATA |
| 0x0a | WRITE_CMD_LOCK |
| 0x0b | WRITE_DATA_LOCK |
| 0x0e | WRITE_CMD |
| 0x0f | WRITE_DATA |

### 4.3 C 实现计划

#### 私有数据结构

```c
typedef struct {
    uint64_t samplerate;
    int bus_polarity;    // 0=idle-low, 1=idle-high
    int ignore_nak;      // 0=Disabled, 1=Enabled
    int broadcast_bit;

    uint64_t bits_begin;
    uint64_t bits_end;

    int out_ann;
    int out_python;
} iebus_state;
```

#### 枚举定义

```c
enum {
    ANN_START_BIT = 0,
    ANN_BIT,
    ANN_PARITY,
    ANN_ACK,
    ANN_BROADCAST,
    ANN_MADDR,
    ANN_SADDR,
    ANN_CONTROL,
    ANN_DATALEN,
    ANN_BYTE,
    ANN_WARNING,
    NUM_ANN,
};

enum {
    CMD_READ_STATUS = 0x00,
    CMD_READ_DATA_LOCK = 0x03,
    CMD_READ_LOCK_ADDR_LO = 0x04,
    CMD_READ_LOCK_ADDR_HI = 0x05,
    CMD_READ_STATUS_UNLOCK = 0x06,
    CMD_READ_DATA = 0x07,
    CMD_WRITE_CMD_LOCK = 0x0a,
    CMD_WRITE_DATA_LOCK = 0x0b,
    CMD_WRITE_CMD = 0x0e,
    CMD_WRITE_DATA = 0x0f,
};
```

#### 关键函数签名

```c
static void iebus_reset(struct srd_decoder_inst *di);
static void iebus_start(struct srd_decoder_inst *di);
static void iebus_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void iebus_decode(struct srd_decoder_inst *di);
static void iebus_destroy(struct srd_decoder_inst *di);

static int read_bits(iebus_state *s, struct srd_decoder_inst *di, int n, uint8_t *out_bits);
static int read_bit(iebus_state *s, struct srd_decoder_inst *di);
static int read_value(iebus_state *s, struct srd_decoder_inst *di, int num_bits, uint16_t *value, uint64_t *ss, uint64_t *es);
static int read_header(iebus_state *s, struct srd_decoder_inst *di, int *start_bit, int *broadcast_bit, uint64_t *ss, uint64_t *es);
static int read_broadcast_bit(iebus_state *s, struct srd_decoder_inst *di);
static int read_ack_bit(iebus_state *s, struct srd_decoder_inst *di);
static int read_parity_bit(iebus_state *s, struct srd_decoder_inst *di, int value);
static int handle_data_bytes(iebus_state *s, struct srd_decoder_inst *di, int data_len);
```

### 4.4 关键实现注意事项

1. **时序依赖**：bits() 方法使用 `27e-6 * samplerate` 作为采样偏移和 `33e-6 * samplerate` 作为位长度，C 版本需精确计算。
2. **总线极性**：支持 idle-low 和 idle-high 两种极性，影响边沿等待方向和位值取反。
3. **开始位检测**：通过测量高低电平持续时间判断是否为有效开始位（>= 100µs）。
4. **奇偶校验**：使用 `bin(value).count('1') % 2` 计算期望奇偶位，C 版本用 popcount。
5. **NAK 处理**：非广播帧中 ACK=1 表示 NAK，可选项忽略 NAK。
6. **数据长度 0**：IEBus 中 data_len==0 表示 256 字节。
7. **OUTPUT_PYTHON 格式**：多种帧类型（HEADER, MASTER ADDRESS, SLAVE ADDRESS, CONTROL, DATA LENGTH, DATA, NAK）。

### 4.5 与 Python 版本的差异处理

| 差异点 | Python | C 处理方式 |
|--------|--------|-----------|
| reduce() | `reduce(lambda a,b: (a<<1)|b, bus)` | 循环移位 |
| bin().count('1') | `bin(value).count('1') % 2` | popcount 内置函数或查表 |
| 动态位列表 | `bits = []` | 固定数组 |
| 函数返回多值 | `return (v, ss, es)` | 通过指针参数返回 |
| first_true() | `first_true(iterable, ...)` | 循环查找 |
| Commands 枚举 | Python IntEnum | C 枚举 + 名称数组 |

---

## 5. SpaceWire 解码器 (spacewire_c)

### 5.1 Python 解码器分析

#### 元数据

| 字段 | 值 |
|------|-----|
| id | `spacewire` |
| name | `Spacewire` |
| longname | `Spacewire` |
| desc | `High speed data transfer protocol used for communication between spacecraft subsystems` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['spacewire']` |
| tags | `['Aerospace']` |

#### 通道定义

| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | `data` | `Data` | `Data line` | `dec_spacewire_chan_data` <!-- Updated: 补充idn --> |
| 1 | `strobe` | `Strobe` | `Strobe line` | `dec_spacewire_chan_strobe` <!-- Updated: 补充idn --> |

#### 选项定义

无选项。

#### 注释定义 (annotations)

| 索引 | id | desc |
|------|-----|------|
| 0 | `D` | Data |
| 1 | `P` | Parity |
| 2 | `DCF` | Data Control Flag |
| 3 | `ctrl-char` | Control Character |
| 4 | `data-char` | Data Character |
| 5 | `code` | Control Code |
| 6 | `time` | Control Code |
| 7 | `warning` | Warning |

#### 注释行 (annotation_rows)

| id | label | class_tuple |
|----|-------|-------------|
| `bits` | `Bits` | `(0,1,2)` |
| `characters` | `Characters` | `(3,4)` |
| `codes` | `Control Codes` | `(5,6)` |
| `warnings` | `Warnings` | `(7,)` |

#### 常量定义

| 常量 | 值 | 含义 |
|------|-----|------|
| CHAR_LEN_CONTROL | 3 | 控制字符长度（DCF + Parity + 2 data bits） |
| CHAR_LEN_DATA | 9 | 数据字符长度（DCF + Parity + 8 data bits） |
| PACKET_MASK_CONTROL | 0b111 | 控制字符数据掩码 |
| FCT | 0x1 | Flow Control Token |
| EOP | 0x5 | End of Packet |
| EEP | 0x3 | End of Packet (Error) |
| ESC | 0x7 | Escape |

#### 是否需要 samplerate

**否** — 不使用 samplerate，纯基于 Data/Strobe 信号边沿检测。

#### 是否输出到其他解码器

**是** — outputs 为 `['spacewire']`，但 Python 版本未注册 OUTPUT_PYTHON。C 版本可选择是否添加。

### 5.2 状态机分析

#### 主状态

| 状态 | 描述 |
|------|------|
| `IDLE` | 搜索 NULL 控制码以同步 |
| `SYNC` | 已同步，解码字符 |

#### IDLE 状态逻辑

1. 等待 Data 或 Strobe 边沿：`[{0: 'e'}, {1: 'e'}]`
2. 将 data 位移入 data_val：`data_val = (data_val << 1) | data`
3. 检查 data_val 低 7 位是否为 NULL 码模式 `0b1110100`
4. 如果匹配：
   - 输出 Parity 注释（位 7）
   - 输出 Data 位注释（位 0-6）
   - 输出 NULL 控制码注释
   - 转入 SYNC 状态
   - 设置 last_len=3, index=0

#### SYNC 状态逻辑

使用 index 计数器跟踪当前字符内的位位置。

**index == 1**（第 2 位，即 DCF 位之后）：
1. 检查 DCF 位（data_val & 1）：1=控制字符，0=数据字符
2. 设置 char_len
3. 输出 DCF 注释
4. 计算奇偶校验：
   - 对上一个字符（last_data_val）的所有位（除 parity 和 DCF）异或
   - 再异或当前 DCF 位
   - 再取反（奇校验）
5. 输出 Parity 注释
6. 如果奇偶校验失败：输出 Warning

**index == char_len**（字符结束）：
1. 输出数据位注释
2. 如果是控制字符（char_len == 3）：
   - 提取控制字符值（3 位反转）
   - FCT/ESC/EEP/EOP 输出对应注释
   - 其他值输出数值
   - 检测 NULL 控制码：上一个字符是 ESC + 当前是 FCT
3. 如果是数据字符（char_len == 9）：
   - 提取数据值（8 位反转）
   - 输出十六进制数据注释
   - 检测 Time 控制码：上一个字符是 ESC
4. 更新 last_len, last_data_val, index=0

**其他 index**：
- index++

#### Data/Strobe 编码原理

SpaceWire 使用 Data-Strobe 编码：
- 时钟 = Data XOR Strobe
- 每当 Data 或 Strobe 发生变化时，就是一个时钟边沿
- 解码器等待 Data 或 Strobe 的任何边沿来获取位

#### 控制字符位反转

Python 代码：`int('{:03b}'.format(data_val & PACKET_MASK_CONTROL)[::-1], 2)`

这是将 3 位二进制字符串反转后再转为整数。例如：
- 0b100 → "100" → "001" → 1 (FCT)
- 0b111 → "111" → "111" → 7 (ESC)
- 0b011 → "011" → "110" → 6 → 但实际 EEP=0x3, EOP=0x5

C 版本位反转函数：
```c
static int reverse_bits(int val, int n) {
    int result = 0;
    for (int i = 0; i < n; i++)
        result |= ((val >> i) & 1) << (n - 1 - i);
    return result;
}
```

### 5.3 C 实现计划

#### 私有数据结构

```c
typedef struct {
    int state;              // STATE_IDLE / STATE_SYNC
    int index;              // 当前字符内的位索引
    int char_len;           // 当前字符长度
    int last_len;           // 上一个字符长度
    int data_val;           // 移位寄存器
    int last_data_val;      // 上一个字符值

    uint64_t last_samplenums[15]; // 最近采样点记录（CHAR_LEN_CONTROL + CHAR_LEN_DATA + 3 = 15）
    int num_samplenums;

    int out_ann;
} spacewire_state;
```

#### 枚举定义

```c
enum {
    STATE_IDLE,
    STATE_SYNC,
};

enum {
    ANN_DATA = 0,
    ANN_PARITY,
    ANN_DCF,
    ANN_CTRL_CHAR,
    ANN_DATA_CHAR,
    ANN_CODE,
    ANN_TIME,
    ANN_WARNING,
    NUM_ANN,
};
```

#### 关键函数签名

```c
static void spacewire_reset(struct srd_decoder_inst *di);
static void spacewire_start(struct srd_decoder_inst *di);
static void spacewire_decode(struct srd_decoder_inst *di);
static void spacewire_destroy(struct srd_decoder_inst *di);

static int reverse_bits(int val, int n);
```

### 5.4 关键实现注意事项

1. **Data-Strobe 编码**：等待 Data 或 Strobe 的边沿，每个边沿代表一个位。
2. **NULL 码检测**：在 IDLE 状态搜索 7 位模式 0b1110100（即 ESC 字符后跟 FCT 字符的位模式）。
3. **位反转**：控制字符和数据字符都需要位反转。3 位反转用查表更快：
   ```c
   static const int reverse3[] = {0, 4, 2, 6, 1, 5, 3, 7};
   ```
4. **奇偶校验**：奇校验，对上一个字符所有位（除 parity 和 DCF）+ 当前 DCF 位异或后取反。
5. **last_samplenums 数组**：Python 版本用 insert(0, samplenum) + pop() 维护，C 版本用环形缓冲区更高效。
6. **控制码检测**：NULL = ESC + FCT，Time = ESC + 数据字符。
7. **注释位置**：Python 使用 last_samplenums[i+1] 到 last_samplenums[i] 的范围（注意顺序是反的，因为 insert(0,...) 使最新在前）。

### 5.5 与 Python 版本的差异处理

| 差异点 | Python | C 处理方式 |
|--------|--------|-----------|
| list.insert(0, x) | O(n) 插入 | 环形缓冲区或移位数组 |
| 字符串反转 | `'{:03b}'.format(v)[::-1]` | reverse_bits() 或查表 |
| 动态 last_samplenums | `last_samplenums = [0]*12` | 固定大小数组 + 环形索引 |
| 无 samplerate 依赖 | 不需要 metadata 回调 | 可省略 metadata 函数 |

---

## 通用实现指南

### CMakeLists.txt 修改

在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加：

```
flexray_c
mipi_rffe_c
usb_power_delivery_c
iebus_c
spacewire_c
```

### 文件命名

| 解码器 | C 文件 | 导出变量名 |
|--------|--------|-----------|
| flexray | `flexray_c.c` | `flexray_c_decoder` |
| mipi_rffe | `mipi_rffe_c.c` | `mipi_rffe_c_decoder` |
| usb_power_delivery | `usb_power_delivery_c.c` | `usb_power_delivery_c_decoder` |
| iebus | `iebus_c.c` | `iebus_c_decoder` |
| spacewire | `spacewire_c.c` | `spacewire_c_decoder` |

### 通用代码模式

#### 重置函数

```c
static void xxx_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di))
        c_decoder_set_private(di, g_malloc0(sizeof(xxx_state)));
    xxx_state *s = (xxx_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(xxx_state));
    // 设置初始状态值
}
```

#### 启动函数

```c
static void xxx_start(struct srd_decoder_inst *di)
{
    xxx_state *s = (xxx_state *)c_decoder_get_private(di);
    s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "xxx");
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "xxx");
    // 如需META输出（如bitrate）：
    // s->out_bitrate = c_decoder_register_output_meta(di, SRD_OUTPUT_META, "xxx", "bitrate", "Bitrate", "Bitrate of xxx communication"); <!-- Updated: c_decoder_register_output_meta已实现 -->
    s->samplerate = c_decoder_get_samplerate(di);
    // 读取选项
}
```

#### 销毁函数

```c
static void xxx_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}
```

#### 导出函数

```c
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    .longname = "XXX Protocol (C)",
    .desc = "XXX protocol decoder. (C implementation)",
    .license = "gplv2+",
    .channels = channels,
    .num_channels = N,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = options,
    .num_options = M,
    .num_annotations = NUM_ANN,
    .ann_labels = ann_labels,
    .num_annotation_rows = R,
    .annotation_rows = ann_rows,
    .inputs = inputs,
    .num_inputs = 1,
    .outputs = outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = tags,
    .num_tags = 1,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,
    .metadata = xxx_metadata,
    .destroy = xxx_destroy,
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &xxx_c_decoder;
}
```

### 复杂度排序

从简单到复杂：

1. **spacewire** — 最简单，无 samplerate 依赖，状态机简单，2 通道
2. **iebus** — 中等，samplerate 依赖简单，1 通道，时序逻辑
3. **flexray** — 中等偏上，复杂 CRC，BSS 检测，时钟同步
4. **mipi_rffe** — 复杂，多状态，命令解码复杂，奇偶校验调整
5. **usb_power_delivery** — 最复杂，BMC 编码，4b5b，多协议层，CRC32

### 建议实现顺序

1. spacewire → 2. iebus → 3. flexray → 4. mipi_rffe → 5. usb_power_delivery
