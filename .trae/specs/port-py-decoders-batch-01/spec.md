# Python 解码器移植到 C — Batch 01 详细规格

> 版本：1.0  
> 日期：2026-05-23  
> 涉及解码器：qspi, sdio, spi_dual_quad, uart-fast, cjtag  
> 目标：将上述 5 个 Python 解码器移植为 C 解码器（`*_c.c`），编译为独立 DLL

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理、BITS v2格式、SPI DATA 17字节格式 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |
| uart_c.c | UART解码器范本 | IDLE/BREAK检测、双线独立状态机、samplerate计算、Python输出格式 <!-- Updated: 添加uart_c.c参考 --> |

## 目录

1. [总体架构与约定](#1-总体架构与约定)
2. [QSPI 解码器](#2-qspi-解码器)
3. [SDIO 解码器](#3-sdio-解码器)
4. [SPI Dual/Quad 解码器](#4-spi-dualquad-解码器)
5. [UART-fast 解码器](#5-uart-fast-解码器)
6. [cJTAG 解码器](#6-cjtag-解码器)
7. [CMakeLists.txt 修改](#7-cmakelists-修改)
8. [通用注意事项](#8-通用注意事项)

---

## 1. 总体架构与约定

### 1.1 C 解码器框架

每个 C 解码器需实现以下结构：

```c
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    // ... 元数据字段 ...
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,
    .destroy = xxx_destroy,
    .metadata = xxx_metadata,  // 如果需要 samplerate
};

SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void) {
    return &xxx_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void) {
    return SRD_C_DECODER_API_VERSION;
}
```

### 1.2 关键 API 映射（Python → C）

| Python | C |
|--------|---|
| `self.wait({0: 'r'})` | `c_cond_rise(cb, 0); c_cond_wait(cb, di, ...)` |
| `self.wait({0: 'f'})` | `c_cond_fall(cb, 0); c_cond_wait(cb, di, ...)` |
| `self.wait({0: 'e'})` | `c_cond_edge(cb, 0); c_cond_wait(cb, di, ...)` |
| `self.wait({'skip': N})` | `c_cond_skip(cb, N); c_cond_wait(cb, di, ...)` |
| `self.wait({})` | `c_cond_wait_current(di, &samplenum)` <!-- Updated: c_cond_wait_current已实现 --> |
| `self.samplenum` | `samplenum` (从 `c_cond_wait` 返回) |
| `self.matched` | `matched` (从 `c_cond_wait` 返回) |
| `self.put(ss, es, out_ann, [cls, ['text']])` | `C_ANN_PUT(di, ss, es, out_ann, cls, "text")` |
| `self.put(ss, es, out_ann, [cls, tp, ['text']])` | `C_ANN_PUT_TYPE(di, ss, es, out_ann, cls, tp, "text")` |
| `self.put(ss, es, out_binary, [bin_cls, data])` | `c_decoder_put_binary(di, ss, es, out_binary, bin_cls, size, data)` |
| `self.put(ss, es, out_python, ['CMD', data])` | `c_decoder_put_python(di, ss, es, out_python, "CMD", data, len)` |
| `self.has_channel(n)` | `c_decoder_has_channel(di, n)` |
| `self.options['key']` | `c_decoder_get_option_string(di, "key", default)` / `c_decoder_get_option_int(di, "key", default)` |
| `self.register(srd.OUTPUT_ANN)` | `c_decoder_register_output(di, SRD_OUTPUT_ANN, "proto_id")` |
| `self.register(srd.OUTPUT_LOGIC)` | `c_decoder_register_output(di, SRD_OUTPUT_LOGIC, "proto_id")` <!-- Updated: SRD_OUTPUT_LOGIC已实现 --> |
| `self.samplerate` | `s->samplerate` (从 metadata 回调获取) |
| `self.initial_pins` | `c_decoder_get_initial_pin(di, ch)` <!-- Updated: c_decoder_get_initial_pin已实现 --> |
| 输出逻辑数据 | `c_decoder_put_logic(di, ss, es, out_logic, channel_mask, values, num_channels)` <!-- Updated: c_decoder_put_logic已实现 --> |
| 输出元数据 | `c_decoder_put_meta_int(di, ss, es, out_meta, value)` / `c_decoder_put_meta_double(...)` <!-- Updated: META输出已实现 --> |

### 1.3 通道索引约定

C 解码器中通道索引按照 `channels` + `optional_channels` 的顺序排列，从 0 开始。

### 1.4 条件构建器使用模式

```c
srd_cond_builder *cb = c_cond_new();
// 添加条件...
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
if (ret != SRD_OK) return;
```

多条件 OR：使用 `c_cond_or()` 分隔。

### 1.5 私有状态管理

```c
typedef struct {
    // 所有解码器状态变量
} xxx_state;

static void xxx_reset(struct srd_decoder_inst *di) {
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(xxx_state)));
    }
    xxx_state *s = (xxx_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(xxx_state));
    // 初始化非零默认值...
}

static void xxx_destroy(struct srd_decoder_inst *di) {
    void *priv = c_decoder_get_private(di);
    if (priv) { g_free(priv); c_decoder_set_private(di, NULL); }
}
```

---

## 2. QSPI 解码器

### 2.1 Python 元数据提取

| 字段 | 值 |
|------|---|
| id | `smart_qspi` |
| name | `Smart QSPI` |
| longname | `Quad Serial Peripheral Interface` |
| desc | `Full-duplex, synchronous, serial bus.compatible with Dual SPI and Quad SPI interfaces.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['spi']` |
| tags | `['Embedded/industrial']` |

#### 通道定义

| 索引 | id | name | desc | type | idn | 必选 |
|------|----|------|------|------|-----|------|
| 0 | clk | CLK | Clock | 0 (SCLK) | dec_qspi_chan_clk | 是 |
| 1 | io0 | IO0 | Data i/o 0 | 109 (SDATA) | dec_qspi_chan_io0 | 是 |
| 2 | io1 | IO1 | Data i/o 1 | 107 (SDATA) | dec_qspi_opt_chan_io1 | 否 |
| 3 | io2 | IO2 | Data i/o 2 | 107 (SDATA) | dec_qspi_opt_chan_io2 | 否 |
| 4 | io3 | IO3 | Data i/o 3 | 107 (SDATA) | dec_qspi_opt_chan_io3 | 否 |
| 5 | cs | CS# | Chip-select | -1 (COMMON) | dec_qspi_opt_chan_cs | 否 |

#### 选项定义

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| cs_polarity | CS# polarity | active-low | (active-low, active-high) | dec_qspi_opt_cs_polarity |
| cpol | Clock polarity (CPOL) | 0 | (0, 1) | dec_qspi_opt_cpol |
| cpha | Clock phase (CPHA) | 0 | (0, 1) | dec_qspi_opt_cpha |
| bitorder | Bit order | msb-first | (msb-first, lsb-first) | dec_qspi_opt_bitorder |
| ads | Adress Mode | 24-Bit Address | (32-Bit Address, 24-Bit Address) | dec_qspi_adress_mode |
| frame | Frame Decoder | no | (yes, no) | dec_qspi_opt_frame |
| twolinesmode | TwoLinesMode | spi | (spi, dspi, qspi) | dec_qspi_two_lines_mode |
| invalidlevel | Keep high or low as invalid | both | (both, low, high) | dec_qspi_invalidlevel |

#### 注解定义

| 索引 | type | id | desc |
|------|------|----|------|
| 0 | 6 | data-transfer | data transfer |
| 1 | 108 | Quad data | Q-Data |
| 2 | 108 | Dual data | D-Data |
| 3 | 106 | d0 | IO0 data |
| 4 | 106 | d1 | IO1 data |
| 5 | 106 | d2 | IO2 data |
| 6 | 106 | d3 | IO3 data |
| 7 | 1000 | other | Human-readable warnings |

#### 注解行定义

| id | label | class_tuple |
|----|-------|-------------|
| data-transfer | data transfer | (0,) |
| Quad data | Q-Data | (1,) |
| Dual data | D-Data | (2,) |
| d0 | D0 | (3,) |
| d1 | D1 | (4,) |
| d2 | D2 | (5,) |
| d3 | D3 | (6,) |
| Other | Other | (7,) |

#### 二进制输出

无（Python 版本注册了 `OUTPUT_BINARY` 但未实际使用）

#### 是否需要 samplerate

**是** — `metadata()` 中获取 samplerate，用于计算 `bit_width` 和 bitrate 输出。

#### 是否输出到其他解码器

**是** — 注册了 `OUTPUT_PYTHON`，输出 `CS-CHANGE`、`TRANSFER` 等消息。

**注意**：QSPI 的 outputs 为 `['spi']`，上层解码器需为C解码器才能接收其输出。C解码器只能依赖已有C实现的底层解码器，不依赖Python解码器。 <!-- Updated: 添加C解码器依赖规则 -->

### 2.2 C 实现计划

#### 结构体设计

```c
enum qspi_ann {
    ANN_DATA_TRANSFER = 0,
    ANN_QUAD_DATA,
    ANN_DUAL_DATA,
    ANN_D0,
    ANN_D1,
    ANN_D2,
    ANN_D3,
    ANN_OTHER,
    NUM_ANN,
};

enum process_enum {
    PROCESS_COMMAND = 0,
    PROCESS_WRITE_BYTE,
    PROCESS_READ_BYTE,
    PROCESS_READ_BYTE_CONTINUOUS,
    PROCESS_CONTINUOUS_READ_MODE_BITS,
    PROCESS_ADDRESS_BY_MODE,
    PROCESS_ADDRESS_24BIT,
    PROCESS_ADDRESS_32BIT,
    PROCESS_DUMMY_BY_MODE,
    PROCESS_DUMMY_8BIT,
    PROCESS_DUMMY_32BIT,
    PROCESS_DUMMY_40BIT,
};

enum process_mode {
    MODE_SINGLE = 0,
    MODE_DUAL,
    MODE_QUAD,
};

// 命令后数据描述结构
typedef struct {
    int proc_enum;   // process_enum
    int mode;        // process_mode
} process_info;

// 命令表条目
typedef struct {
    const char *name;     // 命令全名
    const char *abbrev;   // 命令缩写
    const process_info *data_after; // 命令后数据序列
    int data_after_count; // 数据序列长度
} qspi_command_entry;

typedef struct {
    // 通道可用性
    int have_io1;
    int have_io3;  // io2+io3 同时存在
    int have_cs;
    int cs_cond_idx; // CS 在 wait 条件中的索引

    // 选项
    int cs_polarity;  // 0=active-low, 1=active-high
    int cpol;
    int cpha;
    int bit_order;    // 0=msb-first, 1=lsb-first
    int ads;          // 0=24bit, 1=32bit
    int frame;        // 0=no, 1=yes
    int spi_mode_set; // 0=spi, 1=dspi, 2=qspi
    int invalid_level;// 0=both, 1=low, 2=high

    // 采样率
    uint64_t samplerate;
    double bit_width;

    // 比特级状态
    int bitcount;
    uint64_t io0data, io1data, io2data, io3data;

    // 比特记录（每个 IO 线最多 8 个 bit）
    int io0bits_val[8]; uint64_t io0bits_ss[8]; uint64_t io0bits_es[8];
    int io1bits_val[8]; uint64_t io1bits_ss[8]; uint64_t io1bits_es[8];
    int io2bits_val[8]; uint64_t io2bits_ss[8]; uint64_t io2bits_es[8];
    int io3bits_val[8]; uint64_t io3bits_ss[8]; uint64_t io3bits_es[8];

    // 字节级记录（frame 模式用）
    // Data namedtuple: ss, es, val
    uint64_t io0bytes_ss[300]; uint64_t io0bytes_es[300]; uint64_t io0bytes_val[300];
    int io0bytes_cnt;
    uint64_t io1bytes_ss[300]; uint64_t io1bytes_es[300]; uint64_t io1bytes_val[300];
    int io1bytes_cnt;
    uint64_t io2bytes_ss[300]; uint64_t io2bytes_es[300]; uint64_t io2bytes_val[300];
    int io2bytes_cnt;
    uint64_t io3bytes_ss[300]; uint64_t io3bytes_es[300]; uint64_t io3bytes_val[300];
    int io3bytes_cnt;

    // 命令解析状态
    int command;         // 当前命令字节值，0=空闲
    int state_count;     // 当前在 diagram 中的位置
    int count;           // 地址/dummy 累计字节数
    uint64_t bits_data;  // 地址/dummy 累计数据
    uint64_t ss;         // 地址/dummy 起始样本

    // 传输级状态
    uint64_t ss_block;
    uint64_t ss_transfer;
    int cs_was_deasserted;

    // 输出 ID
    int out_ann;
    int out_python;
    int out_binary;
    int out_bitrate;
    int bw;
} qspi_state;
```

#### 命令表

Python 中 `command` 字典定义了约 40 个命令。C 实现需要用数组+查找函数替代：

```c
// 每个命令后的数据序列（静态定义）
static const process_info cmd_06_data[] = {}; // WREN: 无数据
static const process_info cmd_05_data[] = {{PROCESS_READ_BYTE, MODE_SINGLE}}; // RDSR
static const process_info cmd_03_data[] = {{PROCESS_ADDRESS_BY_MODE, MODE_SINGLE}, {PROCESS_READ_BYTE_CONTINUOUS, MODE_SINGLE}}; // READ
// ... 以此类推，为每个命令定义数据序列

// 命令查找表
typedef struct {
    uint8_t cmd_byte;
    const char *name;
    const char *abbrev;
    const process_info *data_after;
    int data_after_count;
} qspi_cmd_entry;

static const qspi_cmd_entry qspi_cmd_table[] = {
    {0x06, "Write Enable", "WREN", cmd_06_data, 0},
    {0x04, "Write Disable", "WRDI", cmd_04_data, 0},
    // ... 所有命令
};

// 查找函数
static const qspi_cmd_entry *find_command(uint8_t cmd_byte);
```

#### 函数签名

```c
static void qspi_reset(struct srd_decoder_inst *di);
static void qspi_start(struct srd_decoder_inst *di);
static void qspi_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void qspi_decode(struct srd_decoder_inst *di);
static void qspi_destroy(struct srd_decoder_inst *di);
```

### 2.3 解码逻辑详细分析

#### 主循环流程

1. **初始化**：检查通道可用性，确定 SPI 模式（CPOL/CPHA → 采样边沿）
2. **Wait 条件**：
   - 必选：CLK 边沿（上升或下降，取决于 SPI mode）
   - 可选：CS# 边沿（如果 CS 通道存在）
3. **首次采样**：`self.wait({})` 获取初始值 → C中使用 `c_cond_wait_current(di, &samplenum)` <!-- Updated: c_cond_wait_current已实现 -->
4. **主循环**：`self.wait(wait_cond)` 等待条件

#### 时钟边沿检测

- SPI Mode 0 (CPOL=0, CPHA=0)：上升沿采样 → `wait_cond = [{0: 'r'}]`
- SPI Mode 1 (CPOL=0, CPHA=1)：下降沿采样 → `wait_cond = [{0: 'f'}]`
- SPI Mode 2 (CPOL=1, CPHA=0)：下降沿采样 → `wait_cond = [{0: 'f'}]`
- SPI Mode 3 (CPOL=1, CPHA=1)：上升沿采样 → `wait_cond = [{0: 'r'}]`

#### CS# 处理

- CS# 变化时：
  - 输出 `CS-CHANGE` Python 消息
  - 如果 CS# 被断言（asserted）：记录传输起始，清空字节缓冲
  - 如果 CS# 被取消断言（deasserted）：输出 `TRANSFER` Python 消息（frame 模式下）
  - 重置所有解码状态
- CS# 未断言时忽略数据

#### 比特处理 (`handle_bit`)

1. 每个时钟有效边沿调用一次
2. `datapins` 顺序为 `(d3, d2, d1, d0)` — 注意 Python 中 `d = (d3, d2, d1, d0)` 的映射
3. 按 bitorder 将各 IO 线的比特累积到对应数据变量
4. 记录每个比特的 (val, ss, es)
5. 8 个比特后调用 `putdata()`

#### 数据输出 (`putdata`)

这是最复杂的函数，逻辑如下：

1. **确定 SPI 模式**：
   - 如果 IO2/IO3 数据无效（全0或全FF，取决于 invalidlevel 设置）→ 使用用户设置的 spiModeSet
   - 如果 IO2/IO3 有效 → `qspi`
   - 如果仅 IO1 有效 → `spi`（注意：Python 代码中此处逻辑可能不完整，dual 模式需确认）

2. **输出各 IO 线数据注解**：`@XX` 格式

3. **Quad/Dual 数据组合**：
   - **Quad**：每 2 个时钟周期（8 个 IO bit）组合为 4 个 Quad 字节
     - 每个 Quad 字节由 4 个 IO 线的 2 个 bit 组合：`io3[0], io2[0], io1[0], io0[0], io3[1], io2[1], io1[1], io0[1]`
     - MSB-first 时高位在前
   - **Dual**：每 4 个时钟周期（8 个 IO bit）组合为 2 个 Dual 字节
     - 每个 Dual 字节由 IO0+IO1 的 4 个 bit 组合

4. **命令解析状态机**：
   - `self.command == 0`：空闲态，检查当前字节是否为已知命令
     - 如果是命令字节：输出命令注解，更新地址模式（0xB7→32bit, 0xE9→24bit）
     - 如果命令有后继数据：设置 `self.command` 和 `self.diagram`
   - `self.command != 0`：处理命令后数据
     - 根据 `diagram[state_count]` 的 `enum` 和 `mode` 确定当前处理阶段
     - **地址阶段**（24bit/32bit）：累积 3/4 字节后输出地址注解
     - **Dummy 阶段**：累积指定数量后输出 Dummy 注解
     - **读/写数据阶段**：每个字节输出数据注解
     - READ_BYTE_CONTINUOUS 不递增 state_count（持续读取）

#### 格式字符串

- 命令：`"Command : Write Enable"`, `"CMD : WREN"`, `"WREN"`（三级缩写）
- 数据：`"Read Data : 0xAB"`, `"RD : 0xAB"`, `"0xAB"`
- 地址：`"24-Bit Address : 0x123456"`, `"AD : 0x123456"`, `"0x123456"`
- Dummy：`"Dummy Cycles"`, `"Dummy"`, `"D"`
- 模式位：同数据格式
- IO 线数据：`"@XX"` 格式
- Quad 数据：`"@XX"` 格式
- 模式标识：`"SPI"`, `"DSPI"`, `"QSPI"`

### 2.4 Python → C 差异与特殊处理

1. **命令表**：Python 用字典，C 用静态数组+查找函数
2. **process_info 比较**：Python 用 `__eq__`，C 用字段比较
3. **动态 diagram 列表**：Python 中 `command[cmd][2]` 是列表引用，C 中用指针+长度
4. **比特记录**：Python 用 `insert(0, ...)` 在列表头部插入，C 用固定数组+索引
5. **Data namedtuple**：C 中用三个并行数组 (ss[], es[], val[]) 替代
6. **Page Program 的 256 个 WRITE_BYTE_SINGLE**：Python 用列表乘法 `[WRITE_BYTE_SINGLE] * 256`，C 中需要特殊处理——当 `process_enum == WRITE_BYTE` 且不是最后一个元素时，不递增 state_count，直到 CS# 取消断言
7. **putg() 函数**：使用 `bit_width` 计算半比特宽度的注解范围，C 中需要 samplerate 计算
8. **invalidlevel 检查**：判断数据线是否保持恒定（全0/全FF）

---

## 3. SDIO 解码器

### 3.1 Python 元数据提取

| 字段 | 值 |
|------|---|
| id | `sdio` |
| name | `SDIO` |
| longname | `Secure Digital I/O` |
| desc | `Secure Digital I/O low-level protocol.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['sdio']` |
| tags | `['Memory']` |

#### 通道定义

| 索引 | id | name | desc | 必选 |
|------|----|------|------|------|
| 0 | cmd | CMD | Command | 是 |
| 1 | clk | CLK | Clock | 是 |
| 2 | dat0 | DAT0 | Data pin 0 | 否 |
| 3 | dat1 | DAT1 | Data pin 1 | 否 |
| 4 | dat2 | DAT2 | Data pin 2 | 否 |
| 5 | dat3 | DAT3 | Data pin 3 | 否 |

**注意**：Python 版本通道无 `type` 和 `idn` 字段，C 版本需补充。建议：
- cmd: type=SRD_CHANNEL_SDATA, idn="dec_sdio_chan_cmd"
- clk: type=SRD_CHANNEL_SCLK, idn="dec_sdio_chan_clk"
- dat0-3: type=SRD_CHANNEL_SDATA, idn="dec_sdio_opt_chan_dat0" 等

#### 选项定义

| id | desc | default | values |
|----|------|---------|--------|
| lines | Lines used | 1-line | (1-line, 4-line) |
| io_block_len | Block size of SDIO | 512 | (128, 256, 512, 1024) |
| polarity | Sample edge | risedge | (risedge, falledge) |

#### 注解定义

Python 中注解数量极多（128 + 多个字段类），具体如下：

| 索引范围 | id 模式 | 描述 |
|----------|---------|------|
| 0-63 | cmd0~cmd63 | CMD0~CMD63 |
| 64-127 | acmd0~acmd63 | ACMD0~ACMD63 |
| 128 | bits | Bits |
| 129 | field-start | Start bit |
| 130 | field-transmission | Transmission bit |
| 131 | field-cmd | Command |
| 132 | field-arg | Argument |
| 133 | field-crc | CRC |
| 134 | field-end | End bit |
| 135 | decoded-bit | Decoded bit |
| 136 | decoded-field | Decoded field |
| 137 | data | Data |
| 138 | data-field | Data fields |
| 139 | data-busy | Data busy |
| 140 | data-field-error | Data fields (Error) |
| 141 | message | Messages |

**总计**：142 个注解类

#### 注解行定义

| id | label | class_tuple |
|----|-------|-------------|
| datas | Datas Line | (137, 138, 139, 140) |
| raw-bits | Raw bits | (128,) |
| decoded-bits | Decoded bits | (135,) |
| decoded-fields | Decoded fields | (136,) |
| fields | Fields | (129, 130, 131, 132, 133, 134) |
| cmd | Commands | (0..127) |
| msg | Messages | (141,) |

#### 是否需要 samplerate

**否** — Python 版本没有 `metadata()` 方法。

#### 是否输出到其他解码器

**否** — 仅注册了 `OUTPUT_ANN`，没有 `OUTPUT_PYTHON`。

### 3.2 C 实现计划

#### 结构体设计

```c
enum sdio_ann {
    ANN_CMD0 = 0,
    // ... ANN_CMD0 到 ANN_CMD63 = 0~63
    // ... ANN_ACMD0 到 ANN_ACMD63 = 64~127
    ANN_BITS = 128,
    ANN_FIELD_START = 129,
    ANN_FIELD_TRANSMISSION = 130,
    ANN_FIELD_CMD = 131,
    ANN_FIELD_ARG = 132,
    ANN_FIELD_CRC = 133,
    ANN_FIELD_END = 134,
    ANN_DECODED_BIT = 135,
    ANN_DECODED_FIELD = 136,
    ANN_DATA = 137,
    ANN_DATA_FIELD = 138,
    ANN_DATA_BUSY = 139,
    ANN_DATA_FIELD_ERROR = 140,
    ANN_MESSAGE = 141,
    NUM_ANN = 142,
};

// SDIO 状态机状态
enum sdio_state {
    STATE_GET_COMMAND_TOKEN = 0,
    STATE_GET_RESPONSE_R1,
    STATE_GET_RESPONSE_R1b,
    STATE_GET_RESPONSE_R2,
    STATE_GET_RESPONSE_R3,
    STATE_GET_RESPONSE_R4,
    STATE_GET_RESPONSE_R5,
    STATE_GET_RESPONSE_R6,
    STATE_GET_RESPONSE_R7,
    STATE_HANDLE_CMD0,
    STATE_HANDLE_CMD2,
    // ... 其他命令处理状态
};

// 数据线状态
enum sdio_data_state {
    DATA_STATE_IDLE = 0,
    DATA_STATE_WAIT_FOR_START,
    DATA_STATE_DATA,
    DATA_STATE_CRC,
    DATA_STATE_CARD_BUSY,
};

typedef struct {
    // 选项
    int four_line;        // 0=1-line, 1=4-line
    int rise_sample;      // 1=risedge, 0=falledge
    int io_block_len;     // 128/256/512/1024

    // CMD 线状态机
    int state;            // sdio_state
    int is_acmd;          // CMD vs ACMD 标志
    int cmd;              // 当前命令号
    uint32_t arg;         // 当前参数
    int cmd_str_is_acmd;  // 用于格式化输出

    // Token 比特收集
    // token[i] = [ss, es, val]
    uint64_t token_ss[136];  // R2 最长 136 bit
    uint64_t token_es[136];
    int token_val[136];
    int token_count;

    // 数据线状态
    int data_state;       // sdio_data_state
    int data_bytes_required;
    int data_crc_resp;
    
    // 数据线接收缓冲
    // data_received[i] = [ss, pins[2:6]]
    uint64_t data_recv_ss[8192]; // 足够大
    int data_recv_pins[8192][4]; // dat0-3
    int data_recv_count;
    
    // CRC 计算
    uint16_t crc_value[4]; // 最多 4 条线的 CRC16

    // 输出
    int out_ann;
} sdio_state;
```

#### CRC 函数

需要从 `sd_crc.py` 移植 `crc7()` 和 `crc16()` 函数：

```c
static int crc7_bit(int data, int bit) {
    return (data & (1 << bit)) != 0 ? 1 : 0;
}

static uint8_t crc7(const int *bin_array, int len) {
    int data = 0;
    for (int i = 0; i < len; i++) {
        int di = bin_array[i] ^ crc7_bit(data, 6);
        data = (data & 0x07) | ((data & 0x38) << 1) | ((di ^ crc7_bit(data, 2)) << 3);
        data = (data & 0x78) | ((data & 0x03) << 1) | di;
    }
    return (uint8_t)data;
}

static uint16_t crc16(const int *bin_array, int len) {
    int data = 0;
    for (int i = 0; i < len; i++) {
        int di = bin_array[i] ^ crc7_bit(data, 15);
        data = (data & 0x0FFF) | ((data & 0x7000) << 1) | ((di ^ crc7_bit(data, 11)) << 12);
        data = (data & 0xF01F) | ((data & 0x07E0) << 1) | ((di ^ crc7_bit(data, 4)) << 5);
        data = (data & 0xFFE0) | ((data & 0x000F) << 1) | di;
    }
    return (uint16_t)data;
}
```

**注意**：`crc7_bit` 函数名有误，实际是通用位提取函数，用于 crc7 和 crc16。

#### 命令名称查找

从 `lists.py` 移植 `cmd_names` 和 `acmd_names` 字典为 C 静态数组：

```c
static const char *cmd_names[] = {
    "GO_IDLE_STATE",     // 0
    NULL,                // 1: Reserved
    "ALL_SEND_CID",      // 2
    // ...
};

static const char *acmd_names[] = {
    // ...
};

static const char *get_cmd_name(int cmd, int is_acmd) {
    const char **names = is_acmd ? acmd_names : cmd_names;
    if (cmd < 0 || cmd > 63) return "Unknown";
    return names[cmd] ? names[cmd] : "Unknown";
}
```

同样需要移植 `accepted_voltages` 和 `card_status` 查找表。

#### 支持的命令列表

```c
static const int cmd_list[] = {0, 2, 3, 5, 6, 7, 8, 9, 10, 13, 16, 52, 53, 55};
static const int acmd_list[] = {6, 13, 41, 51};
```

### 3.3 解码逻辑详细分析

#### 主循环

1. **Wait 条件**：CLK 上升沿或下降沿（取决于 polarity 选项）
2. **每次 wait 返回后**：
   - 先处理数据线状态（DATA/CRC/CARD_BUSY）
   - 再处理 CMD 线状态机

#### CMD 线状态机

**GET COMMAND TOKEN 状态**：
1. 等待 CMD 线起始位（CMD=0）
2. 收集 48 bit token
3. 处理公共字段（start bit, transmission, command index, argument, CRC, end bit）
4. 根据命令号分派到对应处理函数
5. 设置下一个状态（等待响应）

**GET RESPONSE 状态**：
1. 等待 CMD 线起始位（CMD=0）
2. 检查 transmission bit：如果为 1（host），说明这不是响应而是新命令，跳回 GET COMMAND TOKEN
3. 根据响应类型（R1/R1b/R2/R3/R4/R5/R6/R7）收集对应长度的 token
4. 处理响应字段

#### 响应格式

| 类型 | 长度 | 特点 |
|------|------|------|
| R1 | 48 bit | 正常响应，含 card status |
| R1b | 48 bit | 同 R1 + 可选 busy 信号 |
| R2 | 136 bit | CID/CSD 寄存器 |
| R3 | 48 bit | OCR 寄存器 |
| R4 | 48 bit | I/O OCR |
| R5 | 48 bit | IO_RW_DIRECT 响应 |
| R6 | 48 bit | Published RCA |
| R7 | 48 bit | Card interface condition |

#### 数据线处理

1. **IDLE/WAIT_FOR_START**：检测数据起始（所有 DAT 线低电平）
2. **DATA**：按 1-line 或 4-line 模式接收数据字节
3. **CRC**：接收 16 bit CRC 并校验
4. **CARD_BUSY**：检测 busy 信号

#### Token 比特收集

```c
// get_token_bits: 收集一个 bit 到 token 数组
// 返回 1 表示已收集够 n 个 bit
static int get_token_bits(sdio_state *s, uint64_t samplenum, int cmd_val, int n) {
    if (s->token_count > 0) {
        s->token_es[s->token_count - 1] = samplenum;
    }
    s->token_ss[s->token_count] = samplenum;
    s->token_es[s->token_count] = samplenum;
    s->token_val[s->token_count] = cmd_val;
    s->token_count++;
    if (s->token_count < n) return 0;
    // 最后一个 bit 的 es 估算
    if (s->token_count >= 2) {
        s->token_es[n - 1] += s->token_ss[n - 1] - s->token_ss[n - 2];
    }
    return 1;
}
```

#### Token 数据提取

```c
// get_token_data: 从 token[start..end] 提取整数值
static uint32_t get_token_data(sdio_state *s, int start, int end) {
    uint32_t val = 0;
    for (int i = start; i <= end; i++) {
        val = (val << 1) | s->token_val[i];
    }
    return val;
}
```

### 3.4 Python → C 差异与特殊处理

1. **注解数量巨大**（142 个）：C 中 `ann_labels` 数组需要 142 个条目，用代码生成或宏简化
2. **动态方法分派**：Python 用 `getattr(self, 'handle_cmd' + str(cmd))`，C 中用 switch-case 或函数指针数组
3. **异常处理**：Python 用 try/except 捕获所有异常并输出到 message 注解，C 中不需要（但应确保不会崩溃）
4. **lists.py 和 sd_crc.py**：需要内联到 C 文件中
5. **data_received 列表**：Python 用动态列表，C 用固定大小数组（需要足够大，最大 512*8*2=8192 个条目用于 4-line 模式 512 字节）
6. **puta() 函数**：参数地址映射 `token[47-8-e]` 到 `token[47-8-s]`，C 中直接计算索引
7. **CMD55 + ACMD 模式**：CMD55 设置 `is_acmd=True`，下一个非 55/63 命令后重置

---

## 4. SPI Dual/Quad 解码器

### 4.1 Python 元数据提取

| 字段 | 值 |
|------|---|
| id | `spi-dual-quad` |
| name | `SPI Dual/Quad` |
| longname | `Dual/Quad Serial Peripheral Interface` |
| desc | `Full-duplex, synchronous, serial bus.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['spi']` |
| tags | `['Embedded/industrial']` |

#### 通道定义

| 索引 | id | name | desc | 必选 |
|------|----|------|------|------|
| 0 | clk | CLK | Clock | 是 |
| 1 | sio0 | SIO0 | SPI Input/Output 0 | 是 |
| 2 | sio1 | SIO1 | SPI Input/Output 1 | 是 |
| 3 | sio2 | SIO2 | SPI Input/Output 2 | 否 |
| 4 | sio3 | SIO3 | SPI Input/Output 3 | 否 |
| 5 | cs | CS# | Chip-select | 否 |

**注意**：Python 版本通道无 `type` 和 `idn` 字段，C 版本需补充。建议：
- clk: type=SRD_CHANNEL_SCLK, idn="dec_spi_dual_quad_chan_clk"
- sio0: type=SRD_CHANNEL_SDATA, idn="dec_spi_dual_quad_chan_sio0"
- sio1: type=SRD_CHANNEL_SDATA, idn="dec_spi_dual_quad_chan_sio1"
- sio2: type=SRD_CHANNEL_SDATA, idn="dec_spi_dual_quad_opt_chan_sio2"
- sio3: type=SRD_CHANNEL_SDATA, idn="dec_spi_dual_quad_opt_chan_sio3"
- cs: type=SRD_CHANNEL_COMMON, idn="dec_spi_dual_quad_opt_chan_cs"

#### 选项定义

| id | desc | default | values |
|----|------|---------|--------|
| cs_polarity | CS# polarity | active-low | (active-low, active-high) |
| cpol | Clock polarity | 0 | (0, 1) |
| cpha | Clock phase | 0 | (0, 1) |
| bitorder | Bit order | msb-first | (msb-first, lsb-first) |
| wordsize | Word size | 8 | (整数) |
| twolnmd | Twolnmd | qspi | (spi, qspi, dspi) |
| protocol | protocol mode | spi | (spi, dual, quad, sqi) |

#### 注解定义

| 索引 | id | desc |
|------|----|------|
| 0 | spi-data | SPI data |
| 1 | sio0-bit | SIO0 bit |
| 2 | sio1-bit | SIO1 bit |
| 3 | sio2-bit | SIO2 bit |
| 4 | sio3-bit | SIO3 bit |
| 5 | warning | Warning |
| 6 | spi-transfer | SPI transfer |

#### 注解行定义

| id | label | class_tuple |
|----|-------|-------------|
| sio0-bits | SIO0 bits | (1,) |
| sio1-bits | SIO1 bits | (2,) |
| sio2-bits | SIO2 bits | (3,) |
| sio3-bits | SIO3 bits | (4,) |
| spi-data-vals | SPI data | (0,) |
| spi-transfers | SPI transfers | (6,) |
| other | Other | (5,) |

#### 二进制输出

| 索引 | id | desc |
|------|----|------|
| 0 | spi-data | SPI Data |

#### 是否需要 samplerate

**是** — `metadata()` 中获取 samplerate，用于 bitrate 计算。

#### 是否输出到其他解码器

**是** — 注册了 `OUTPUT_PYTHON`，输出 `CS-CHANGE`、`BITS`、`DATA`、`TRANSFER` 消息。

**BITS v2 格式**（C实现必须使用此格式，与spi_c.c/i2c_c.c一致）：
```
data[0] = have_mosi (bit0) | have_miso (bit1)
data[1] = mosi_bit_count (uint8_t)
data[2..2+count*17-1] = per bit: [value(1B)][ss(8B LE)][es(8B LE)]
data[2+count*17] = 0x00 (reserved/alignment)
data[2+count*17+1] = miso_bit_count (uint8_t)
data[2+count*17+2..] = per bit: [value(1B)][ss(8B LE)][es(8B LE)]
```
<!-- Updated: BITS v2格式已在spi_c.c/i2c_c.c中实现，c_decoder_utils.h有文档 -->

**SPI DATA 格式**（17字节，与spi_c.c一致）：
```
data[0] = (have_mosi ? 1 : 0) | (have_miso ? 2 : 0)
data[1..8] = mosi_val (LE uint64)
data[9..16] = miso_val (LE uint64)
```
<!-- Updated: SPI DATA 17字节格式已在spi_c.c中实现 -->

**注意**：SPI Dual/Quad 的 outputs 为 `['spi']`，上层解码器需为C解码器才能接收其输出。C解码器只能依赖已有C实现的底层解码器，不依赖Python解码器。 <!-- Updated: 添加C解码器依赖规则 -->

### 4.2 C 实现计划

#### 结构体设计

```c
enum spi_dq_ann {
    ANN_SPI_DATA = 0,
    ANN_SIO0_BIT,
    ANN_SIO1_BIT,
    ANN_SIO2_BIT,
    ANN_SIO3_BIT,
    ANN_WARNING,
    ANN_SPI_TRANSFER,
    NUM_ANN,
};

enum protocol_mode {
    PROTO_SPI = 0,
    PROTO_DUAL,
    PROTO_QUAD,
    PROTO_SQI,
};

enum current_mode {
    CUR_MODE_SPI = 0,
    CUR_MODE_DUAL,
    CUR_MODE_QUAD,
};

#define VAL 0
#define SS  1
#define ES  2

typedef struct {
    // 选项
    int cs_polarity;
    int cpol;
    int cpha;
    int bit_order;    // 0=msb-first, 1=lsb-first
    int wordsize;
    int twolnmd;      // 0=spi, 1=qspi, 2=dspi
    int protocol;     // PROTO_SPI/DUAL/QUAD/SQI

    // 通道
    int have_cs;
    int have_sio2;
    int have_sio3;
    int is_quad;

    // 运行时模式
    int current_mode; // CUR_MODE_SPI/DUAL/QUAD
    int command_phase;// SQI 模式下的命令阶段标志

    // 采样率
    uint64_t samplerate;

    // 比特级状态
    int bitcount;
    uint64_t spidata;
    int sio0bits_val[64]; uint64_t sio0bits_ss[64]; uint64_t sio0bits_es[64];
    int sio1bits_val[64]; uint64_t sio1bits_ss[64]; uint64_t sio1bits_es[64];
    int sio2bits_val[64]; uint64_t sio2bits_ss[64]; uint64_t sio2bits_es[64];
    int sio3bits_val[64]; uint64_t sio3bits_ss[64]; uint64_t sio3bits_es[64];

    // 字节级记录
    uint64_t spibytes_ss[256]; uint64_t spibytes_es[256]; uint64_t spibytes_val[256];
    int spibytes_cnt;

    // 传输级
    uint64_t ss_block;
    uint64_t ss_transfer;
    int cs_was_deasserted;

    // 输出
    int out_ann;
    int out_python;
    int out_binary;
    int out_bitrate;
    int bw;
} spi_dq_state;
```

### 4.3 解码逻辑详细分析

#### 主循环

1. **Wait 条件**：CLK 任意边沿 `{0: 'e'}` + 可选 CS# 边沿
2. **首次采样**：`self.wait({})` 获取初始值 → C中使用 `c_cond_wait_current(di, &samplenum)` <!-- Updated: c_cond_wait_current已实现 -->
3. **主循环**：`self.wait(wait_cond)` 等待条件

#### 时钟边沿检测（find_clk_edge）

与 QSPI 不同，此解码器在 `find_clk_edge` 中直接检查 CLK 电平：

- Mode 0: 上升沿采样 → `clk == 0` 时 return（等待上升）
- Mode 1: 下降沿采样 → `clk == 1` 时 return
- Mode 2: 下降沿采样 → `clk == 1` 时 return
- Mode 3: 上升沿采样 → `clk == 0` 时 return

**但注意**：Python 中 wait 条件是 `{0: 'e'}`（任意边沿），然后在 `find_clk_edge` 中过滤。C 实现可以直接用 `c_cond_rise` 或 `c_cond_fall` 来优化。

#### 协议模式处理

- **SPI 模式**：每个时钟周期 1 bit（仅 SIO0）
- **Dual 模式**：每个时钟周期 2 bit（SIO0 + SIO1）
- **Quad 模式**：每个时钟周期 4 bit（SIO0 + SIO1 + SIO2 + SIO3）
- **SQI 模式**：
  - 命令阶段（前 8 bit）：SPI 模式
  - 数据阶段：Quad 模式
  - CS# 拉高后重置为命令阶段

#### 比特累积

- **Quad**：`spidata |= sio3 << (shift+3) | sio2 << (shift+2) | sio1 << (shift+1) | sio0 << shift`
  - `bitcount += 4`
- **Dual**：`spidata |= sio1 << (ws-1-bitcount) | sio0 << (ws-1-bitcount-1)`
  - `bitcount += 2`
- **SPI**：`spidata |= sio0 << (ws-1-bitcount)`
  - `bitcount += 1`

当 `bitcount >= wordsize` 时输出数据。

#### 传输显示（decode_transfer）

CS# 取消断言时，根据协议模式显示不同格式的传输数据：
- SPI: 简单十六进制
- Dual: `"DUAL: XX XX ..."`
- Quad: `"QUAD: XX XX ..."`
- SQI: `"CMD: XX" + "DATA: XX XX ..."`

### 4.4 Python → C 差异与特殊处理

1. **SQI 模式命令阶段**：Python 中 `command_phase` 标志在 8 bit 后切换，C 中需要同样处理
2. **wordsize 必须是 2/4 的倍数**：Dual 模式下 wordsize 必须是 2 的倍数，Quad 模式下必须是 4 的倍数
3. **Data namedtuple**：C 中用三个并行数组替代
4. **TRANSFER 输出**：Python 中 SQI 模式输出 dict，C 中需要序列化为字节流
5. **比特记录 insert(0, ...)**：C 中用反向索引或正向存储+反向读取

---

## 5. UART-fast 解码器

### 5.1 Python 元数据提取

| 字段 | 值 |
|------|---|
| id | `uart-fast` |
| name | `UART-fast` |
| longname | `Universal Asynchronous Receiver/Transmitter` |
| desc | `Asynchronous, serial bus.(Ultra-fast version)' |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['uart']` |
| tags | `['Embedded/industrial']` |

#### 通道定义

| 索引 | id | name | desc | 必选 |
|------|----|------|------|------|
| 0 | rx | RX | UART receive line | 否 |
| 1 | tx | TX | UART transmit line | 否 |

**注意**：两个通道都是可选的，但至少需要一个。Python 版本通道无 `type` 和 `idn`，C 版本建议：
- rx: type=SRD_CHANNEL_SDATA, idn="dec_uart_fast_opt_chan_rx"
- tx: type=SRD_CHANNEL_SDATA, idn="dec_uart_fast_opt_chan_tx"

#### 选项定义

| id | desc | default | values |
|----|------|---------|--------|
| baudrate | Baud rate(波特率) | 115200 | (整数) |
| data_bits | Data bits(数据位数) | 8 | (5, 6, 7, 8, 9) |
| parity | Parity(校验位) | none | (none, odd, even, zero, one, ignore) |
| stop_bits | Stop bits(停止位) | 1.0 | (0.0, 0.5, 1.0, 1.5, 2.0) |
| bit_order | Bit order(位序) | lsb-first | (lsb-first, msb-first) |
| format | Data format(数据格式) | hex | (ascii, dec, hex, oct, bin) |
| invert | Invert RX/TX(反转信号) | no | (yes, no) |
| packet_idle_us | Packet delimit by idle time, us | -1 | (整数) |
| show_data_point | Show data point(数据点显示) | no | (yes, no) |

#### 注解定义

| 索引 | id | desc |
|------|----|------|
| 0 | rx-data | RX data |
| 1 | tx-data | TX data |
| 2 | rx-start | RX start bit |
| 3 | tx-start | TX start bit |
| 4 | rx-parity-ok | RX parity OK bit |
| 5 | tx-parity-ok | TX parity OK bit |
| 6 | rx-parity-err | RX parity error bit |
| 7 | tx-parity-err | TX parity error bit |
| 8 | rx-stop | RX stop bit |
| 9 | tx-stop | TX stop bit |
| 10 | rx-warning | RX warning |
| 11 | tx-warning | TX warning |
| 12 | atk-data-point | ATK Data point |

**注解行**：

| id | label | class_tuple |
|----|-------|-------------|
| rx-data-vals | RX data | (0, 2, 4, 6, 8) |
| rx-warnings | RX warnings | (10,) |
| tx-data-vals | TX data | (1, 3, 5, 7, 9) |
| tx-warnings | TX warnings | (11,) |
| atk-signs | ATK signs | (12,) |

#### 二进制输出

| 索引 | id | desc |
|------|----|------|
| 0 | rx | RX dump |
| 1 | tx | TX dump |
| 2 | rxtx | RX/TX dump |
| 3 | rx-ok | RX dump (no error) |
| 4 | tx-ok | TX dump (no error) |
| 5 | rxtx-ok | RX/TX dump (no error) |

#### 是否需要 samplerate

**是** — `metadata()` 中获取 samplerate，用于计算 `bit_width`、`half_bit_width`、`bit_samplenum`。

#### 是否输出到其他解码器

**是** — 注册了 `OUTPUT_PYTHON`，输出 `STARTBIT`、`DATA`、`PARITYBIT`、`STOPBIT`、`FRAME`、`PACKET`、`BREAK`、`IDLE` 等消息。

**注意**：`IDLE` 和 `BREAK` 输出已在现有 uart_c.c 中实现，uart-fast C实现应参考uart_c.c的IDLE/BREAK检测逻辑。 <!-- Updated: uart_c.c已实现IDLE/BREAK输出 -->

### 5.2 C 实现计划

#### 结构体设计

```c
#define UART_RX 0
#define UART_TX 1

enum uart_ann {
    ANN_RX_DATA = 0,
    ANN_TX_DATA,
    ANN_RX_START,
    ANN_TX_START,
    ANN_RX_PARITY_OK,
    ANN_TX_PARITY_OK,
    ANN_RX_PARITY_ERR,
    ANN_TX_PARITY_ERR,
    ANN_RX_STOP,
    ANN_TX_STOP,
    ANN_RX_WARN,
    ANN_TX_WARN,
    ANN_ATK_POINT,
    NUM_ANN,
};

enum uart_bin {
    BIN_RX = 0,
    BIN_TX,
    BIN_RXTX,
    BIN_RX_OK,
    BIN_TX_OK,
    BIN_RXTX_OK,
    NUM_BIN,
};

enum uart_state {
    STATE_WAIT_FOR_START_BIT = 0,
    STATE_GET_START_BIT,
    STATE_GET_DATA_BITS,
    STATE_GET_PARITY_BIT,
    STATE_GET_STOP_BITS,
};

// 状态机条目：(state, rel_ss, sample_point, rel_es)
typedef struct {
    int state;
    uint64_t rel_ss;
    uint64_t sample_point;
    uint64_t rel_es;
} uart_sm_entry;

typedef struct {
    // 选项
    uint64_t baudrate;
    int data_bits;       // 5-9
    int parity_type;     // 0=none, 1=odd, 2=even, 3=zero, 4=one, 5=ignore
    double stop_bits;    // 0.0, 0.5, 1.0, 1.5, 2.0
    int bit_order;       // 0=lsb-first, 1=msb-first
    int format;          // 0=hex, 1=dec, 2=oct, 3=bin, 4=ascii
    int invert;          // 0=no, 1=yes
    int64_t packet_idle_us;
    int show_data_point;

    // 采样率相关
    uint64_t samplerate;
    double bit_width;
    double half_bit_width;
    double bit_samplenum;  // 采样点偏移（半比特宽度）

    // 状态机
    uart_sm_entry *state_machine;
    int state_machine_len;

    // 每条线的独立状态
    int state[2];           // uart_state
    int state_num[2];       // 状态机索引
    uint64_t frame_start[2];
    int frame_valid[2];
    int packet_valid[2];
    uint64_t datavalue[2];
    int paritybit[2];

    // 数据位收集
    int databits_val[2][9];    // 最多 9 个数据位
    uint64_t databits_ss[2][9];
    uint64_t databits_es[2][9];
    int databits_cnt[2];

    // 停止位收集
    int stopbits_val[2][4];    // 最多 2 个完整停止位
    uint64_t stopbits_ss[2][4];
    uint64_t stopbits_es[2][4];
    int stopbits_cnt[2];

    // Break 检测
    uint64_t break_start[2];
    int break_start_valid[2];
    uint64_t break_min_sample_count;

    // 帧长度
    uint64_t frame_len_sample_count;

    // 数据边界
    uint64_t data_bounds_ss;
    uint64_t data_bounds_es;

    // Packet 处理
    uint64_t packet_data[2][4096]; // 足够大
    int packet_data_cnt[2];
    uint64_t ss_packet[2];
    uint64_t es_packet[2];
    uint64_t packet_idle_samples;
    int packet_idle_valid;

    // 通道
    int have_rx;
    int have_tx;
    int enabled_rxtx[2]; // 哪些线被启用
    int num_enabled;

    // 输出
    int out_ann;
    int out_python;
    int out_binary;
    int bw;
} uart_state;
```

#### 状态机构建

Python 中 `init_state_machine()` 动态构建状态机表。C 中需要在 `start()` 中动态分配：

```c
static void init_state_machine(uart_state *s) {
    int max_entries = 2 + s->data_bits + 1 + 2 + 1; // start + data + parity + stop + loop
    s->state_machine = g_malloc0(sizeof(uart_sm_entry) * max_entries);
    
    int idx = 0;
    // Start bit
    s->state_machine[idx++] = (uart_sm_entry){
        STATE_WAIT_FOR_START_BIT, 0, 0, 0};
    s->state_machine[idx++] = (uart_sm_entry){
        STATE_GET_START_BIT, 
        round(0), 
        round(s->bit_samplenum), 
        round(s->bit_width)};
    
    // Data bits
    s->data_bounds_ss = s->state_machine[idx-1].rel_es;
    for (int i = 0; i < s->data_bits; i++) {
        uint64_t ss = round((i+1) * s->bit_width);
        uint64_t sp = round((i+1) * s->bit_width + s->bit_samplenum);
        uint64_t es = round((i+2) * s->bit_width);
        s->state_machine[idx++] = (uart_sm_entry){
            STATE_GET_DATA_BITS, ss, sp, es};
    }
    s->data_bounds_es = s->state_machine[idx-1].rel_es;
    
    // Parity bit
    int frame_bit_num = 1 + s->data_bits;
    if (s->parity_type != 0) { // != none
        s->state_machine[idx++] = (uart_sm_entry){
            STATE_GET_PARITY_BIT,
            round(frame_bit_num * s->bit_width),
            round(frame_bit_num * s->bit_width + s->bit_samplenum),
            round((frame_bit_num + 1) * s->bit_width)};
        frame_bit_num++;
    }
    
    // Stop bits
    double sb = s->stop_bits;
    while (sb > 0.4) {
        if (sb > 0.9) {
            s->state_machine[idx++] = (uart_sm_entry){
                STATE_GET_STOP_BITS,
                round(frame_bit_num * s->bit_width),
                round(frame_bit_num * s->bit_width + s->bit_samplenum),
                round((frame_bit_num + 1) * s->bit_width)};
            sb -= 1.0;
            frame_bit_num++;
        } else if (sb > 0.4) {
            s->state_machine[idx++] = (uart_sm_entry){
                STATE_GET_STOP_BITS,
                round(frame_bit_num * s->bit_width),
                round(frame_bit_num * s->bit_width + s->bit_samplenum * 0.5),
                round((frame_bit_num + 0.5) * s->bit_width)};
            sb = 0;
        }
    }
    
    // 循环回第一个状态
    s->state_machine[idx] = s->state_machine[0];
    s->state_machine_len = idx + 1;
    
    for (int rxtx = 0; rxtx < 2; rxtx++) {
        s->state[rxtx] = STATE_WAIT_FOR_START_BIT;
        s->state_num[rxtx] = 0;
    }
}
```

### 5.3 解码逻辑详细分析

#### 主循环

1. **动态构建 wait 条件**：
   - 对每个启用的 RX/TX 线：
     - 如果在 WAIT_FOR_START_BIT 状态：等待下降沿（或上升沿，如果 invert）
     - 否则：等待 skip 到采样点
   - 同时等待 RX/TX 线的任意边沿（用于 break 检测和 idle 检测）

2. **wait 返回后**：
   - 对每个启用的线检查哪个条件匹配
   - 如果是数据条件匹配：调用 `inspect(rxtx, signal, False, 'sample')`
   - 如果是边沿条件匹配：调用 `inspect(rxtx, signal, False, 'edge')`

#### inspect 函数

```c
static void uart_inspect_sample(uart_state *s, struct srd_decoder_inst *di, 
                                 int rxtx, int signal) {
    switch (s->state[rxtx]) {
    case STATE_WAIT_FOR_START_BIT:
        handle_packet_idle(s, rxtx);
        wait_for_start_bit(s, di, rxtx, signal);
        break;
    case STATE_GET_START_BIT:
        get_start_bit(s, di, rxtx, signal);
        break;
    case STATE_GET_DATA_BITS:
        get_data_bits(s, di, rxtx, signal);
        break;
    case STATE_GET_PARITY_BIT:
        get_parity_bit(s, di, rxtx, signal);
        break;
    case STATE_GET_STOP_BITS:
        get_stop_bits(s, di, rxtx, signal);
        break;
    }
}
```

#### 状态推进

```c
static void advance_state_machine(uart_state *s, int rxtx, 
                                   int startbit_error, int stopbit_error) {
    if (startbit_error || stopbit_error) {
        s->state_num[rxtx] = 0;
        s->state[rxtx] = STATE_WAIT_FOR_START_BIT;
        if (startbit_error) return;
    } else {
        s->state_num[rxtx]++;
        s->state[rxtx] = s->state_machine[s->state_num[rxtx]].state;
    }
    
    if (s->state[rxtx] == STATE_WAIT_FOR_START_BIT) {
        s->state_num[rxtx] = 0;
        // handle_frame 和 get_packet_data
    }
}
```

#### Parity 检查

```c
static int parity_ok(int parity_type, int parity_bit, uint64_t data, int data_bits) {
    if (parity_type == 5) return 1; // ignore
    if (parity_type == 3) return parity_bit == 0; // zero
    if (parity_type == 4) return parity_bit == 1; // one
    
    int ones = 0;
    uint64_t d = data;
    for (int i = 0; i < data_bits; i++) {
        ones += d & 1;
        d >>= 1;
    }
    ones += parity_bit;
    
    if (parity_type == 1) return (ones % 2) == 1; // odd
    if (parity_type == 2) return (ones % 2) == 0; // even
    return 0;
}
```

#### 格式化输出

```c
static void format_value(uint64_t v, int data_bits, int format, char *out, int out_size) {
    switch (format) {
    case 4: // ascii
        if (v >= 32 && v <= 126) snprintf(out, out_size, "%c", (char)v);
        else if (data_bits <= 8) snprintf(out, out_size, "0x%02X", (unsigned)v);
        else snprintf(out, out_size, "0x%03X", (unsigned)v);
        break;
    case 1: // dec
        snprintf(out, out_size, "%llu", (unsigned long long)v);
        break;
    case 0: // hex
        snprintf(out, out_size, "%02X", (unsigned)v);
        break;
    case 2: // oct
        snprintf(out, out_size, "%03o", (unsigned)v);
        break;
    case 3: // bin
        for (int i = 0; i < data_bits; i++) {
            out[i] = ((v >> (data_bits - 1 - i)) & 1) ? '1' : '0';
        }
        out[data_bits] = '\0';
        break;
    }
}
```

### 5.4 Python → C 差异与特殊处理

1. **双线独立状态**：RX 和 TX 各有独立的状态机，C 中用数组索引 `[rxtx]` 管理
2. **skip 条件**：Python 用 `{'skip': N}` 等待到采样点，C 用 `c_cond_skip(cb, N)`
3. **动态 wait 条件**：每次循环都重新构建条件（因为 skip 值变化），C 中同样需要
4. **packet_idle_us**：需要 samplerate 才能计算，但 samplerate 在 metadata 中才获取。Python 在 `start()` 中调用 `init_packet_idle()`，但此时 samplerate 可能为 None。C 中需要在 `metadata()` 中重新计算
5. **bitpack 辅助函数**：Python 从 `common.srdhelper` 导入，C 中直接实现
6. **Break 检测**：在边沿条件中检测持续低电平
7. **stop_bits 浮点处理**：0.5 止位需要特殊处理（半比特宽度采样点）

---

## 6. cJTAG 解码器

### 6.1 Python 元数据提取

| 字段 | 值 |
|------|---|
| id | `cjtag` |
| name | `cJTAG` |
| longname | `Compact Joint Test Action Group (IEEE 1149.7)` |
| desc | `Protocol for testing, debugging, and flashing ICs.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['jtag']` |
| tags | `['Debug/trace']` |

#### 通道定义

| 索引 | id | name | desc | 必选 |
|------|----|------|------|------|
| 0 | tckc | TCKC | Test clock | 是 |
| 1 | tmsc | TMSC | Test mode select | 是 |

**注意**：Python 版本通道无 `type` 和 `idn`，C 版本建议：
- tckc: type=SRD_CHANNEL_SCLK, idn="dec_cjtag_chan_tckc"
- tmsc: type=SRD_CHANNEL_SDATA, idn="dec_cjtag_chan_tmsc"

#### 选项定义

无

#### 注解定义

Python 中注解由 JTAG 状态名 + cJTAG 状态名 + TDI/TDO/TMS 比特组成：

| 索引范围 | id 模式 | 描述 |
|----------|---------|------|
| 0-15 | jtag_states[i].lower() | JTAG 状态（16 个） |
| 16-26 | cjtag_states[i].lower() | cJTAG 状态（11 个） |
| 28 | bit-tdi | Bit (TDI) |
| 29 | bit-tdo | Bit (TDO) |
| 30 | bitstring-tdi | Bitstring (TDI) |
| 31 | bitstring-tdo | Bitstring (TDO) |
| 32 | bit-tms | Bit (TMS) |

**注意**：索引 27 未使用（Python 中 jtag_states 16 个 + cjtag_states 11 个 = 27，但 TDI 从 28 开始）

**总计**：33 个注解类

#### JTAG 状态列表（16 个）

```
TEST-LOGIC-RESET, RUN-TEST/IDLE,
SELECT-DR-SCAN, CAPTURE-DR, UPDATE-DR, PAUSE-DR, SHIFT-DR, EXIT1-DR, EXIT2-DR,
SELECT-IR-SCAN, CAPTURE-IR, UPDATE-IR, PAUSE-IR, SHIFT-IR, EXIT1-IR, EXIT2-IR
```

#### cJTAG 状态列表（11 个）

```
CJTAG_EC, CJTAG_SPARE, CJTAG_TPDEL, CJTAG_TPREV, CJTAG_TPST,
CJTAG_RDYC, CJTAG_DLYC, CJTAG_SCNFMT, CJTAG_CP, OSCAN1, FOUR_WIRE
```

#### 注解行定义

| id | label | class_tuple |
|----|-------|-------------|
| bits-tdi | Bits (TDI) | (28,) |
| bits-tdo | Bits (TDO) | (29,) |
| bitstrings-tdi | Bitstrings (TDI) | (30,) |
| bitstrings-tdo | Bitstrings (TDO) | (31,) |
| bits-tms | Bits (TMS) | (32,) |
| cjtag-states | CJTAG states | (16..26) |
| jtag-states | JTAG states | (0..15) |

#### 是否需要 samplerate

**否** — Python 版本没有 `metadata()` 方法。

#### 是否输出到其他解码器

**是** — 注册了 `OUTPUT_PYTHON`，输出 `NEW STATE`、`IR TDI`、`IR TDO`、`DR TDI`、`DR TDO` 消息。

**注意**：cJTAG 的 outputs 为 `['jtag']`，上层解码器需为C解码器才能接收其输出。已有 jtag_c.c 可作为参考。C解码器只能依赖已有C实现的底层解码器，不依赖Python解码器。 <!-- Updated: 添加C解码器依赖规则 -->

### 6.2 C 实现计划

#### 结构体设计

```c
enum jtag_state {
    ST_TEST_LOGIC_RESET = 0,
    ST_RUN_TEST_IDLE,
    ST_SELECT_DR_SCAN,
    ST_CAPTURE_DR,
    ST_UPDATE_DR,
    ST_PAUSE_DR,
    ST_SHIFT_DR,
    ST_EXIT1_DR,
    ST_EXIT2_DR,
    ST_SELECT_IR_SCAN,
    ST_CAPTURE_IR,
    ST_UPDATE_IR,
    ST_PAUSE_IR,
    ST_SHIFT_IR,
    ST_EXIT1_IR,
    ST_EXIT2_IR,
    NUM_JTAG_STATES,
};

enum cjtag_state {
    CST_CJTAG_EC = 0,
    CST_CJTAG_SPARE,
    CST_CJTAG_TPDEL,
    CST_CJTAG_TPREV,
    CST_CJTAG_TPST,
    CST_CJTAG_RDYC,
    CST_CJTAG_DLYC,
    CST_CJTAG_SCNFMT,
    CST_CJTAG_CP,
    CST_OSCAN1,
    CST_FOUR_WIRE,
    NUM_CJTAG_STATES,
};

enum cjtag_ann {
    // JTAG states: 0-15
    ANN_TEST_LOGIC_RESET = 0,
    ANN_RUN_TEST_IDLE,
    // ... 16 个 JTAG 状态注解
    // cJTAG states: 16-26
    ANN_CJTAG_EC = NUM_JTAG_STATES,
    // ... 11 个 cJTAG 状态注解
    ANN_BIT_TDI = 28,
    ANN_BIT_TDO = 29,
    ANN_BITSTRING_TDI = 30,
    ANN_BITSTRING_TDO = 31,
    ANN_BIT_TMS = 32,
    NUM_ANN = 33,
};

typedef struct {
    // JTAG 状态
    int state;
    int oldstate;
    
    // cJTAG 状态
    int cjtagstate;
    int oldcjtagstate;
    int escape_edges;
    int oaclen;
    int oldtms;
    int oacp;
    int oscan1cycle;
    
    // TDI/TDO 比特收集
    int bits_tdi[256];
    int bits_tdo[256];
    uint64_t bits_ss_tdi[256];
    uint64_t bits_es_tdi[256];
    uint64_t bits_ss_tdo[256];
    uint64_t bits_es_tdo[256];
    int bits_cnt;
    
    // 注解范围
    uint64_t ss_item;
    uint64_t es_item;
    uint64_t ss_bitstring;
    uint64_t es_bitstring;
    
    // 标志
    int first;
    int first_bit;
    
    // 输出
    int out_ann;
    int out_python;
} cjtag_state;
```

#### JTAG 状态转换表

```c
// next_state[current_state][tms]
static const int jtag_next_state[16][2] = {
    // TEST_LOGIC_RESET: tms=0→RUN_TEST_IDLE, tms=1→TEST_LOGIC_RESET
    {ST_RUN_TEST_IDLE, ST_TEST_LOGIC_RESET},
    // RUN_TEST_IDLE: tms=0→RUN_TEST_IDLE, tms=1→SELECT_DR_SCAN
    {ST_RUN_TEST_IDLE, ST_SELECT_DR_SCAN},
    // SELECT_DR_SCAN: tms=0→CAPTURE_DR, tms=1→SELECT_IR_SCAN
    {ST_CAPTURE_DR, ST_SELECT_IR_SCAN},
    // CAPTURE_DR: tms=0→SHIFT_DR, tms=1→EXIT1_DR
    {ST_SHIFT_DR, ST_EXIT1_DR},
    // UPDATE_DR: tms=0→RUN_TEST_IDLE, tms=1→SELECT_DR_SCAN
    {ST_RUN_TEST_IDLE, ST_SELECT_DR_SCAN},
    // PAUSE_DR: tms=0→PAUSE_DR, tms=1→EXIT2_DR
    {ST_PAUSE_DR, ST_EXIT2_DR},
    // SHIFT_DR: tms=0→SHIFT_DR, tms=1→EXIT1_DR
    {ST_SHIFT_DR, ST_EXIT1_DR},
    // EXIT1_DR: tms=0→PAUSE_DR, tms=1→UPDATE_DR
    {ST_PAUSE_DR, ST_UPDATE_DR},
    // EXIT2_DR: tms=0→SHIFT_DR, tms=1→UPDATE_DR
    {ST_SHIFT_DR, ST_UPDATE_DR},
    // SELECT_IR_SCAN: tms=0→CAPTURE_IR, tms=1→TEST_LOGIC_RESET
    {ST_CAPTURE_IR, ST_TEST_LOGIC_RESET},
    // CAPTURE_IR: tms=0→SHIFT_IR, tms=1→EXIT1_IR
    {ST_SHIFT_IR, ST_EXIT1_IR},
    // UPDATE_IR: tms=0→RUN_TEST_IDLE, tms=1→SELECT_DR_SCAN
    {ST_RUN_TEST_IDLE, ST_SELECT_DR_SCAN},
    // PAUSE_IR: tms=0→PAUSE_IR, tms=1→EXIT2_IR
    {ST_PAUSE_IR, ST_EXIT2_IR},
    // SHIFT_IR: tms=0→SHIFT_IR, tms=1→EXIT1_IR
    {ST_SHIFT_IR, ST_EXIT1_IR},
    // EXIT1_IR: tms=0→PAUSE_IR, tms=1→UPDATE_IR
    {ST_PAUSE_IR, ST_UPDATE_IR},
    // EXIT2_IR: tms=0→SHIFT_IR, tms=1→UPDATE_IR
    {ST_SHIFT_IR, ST_UPDATE_IR},
};
```

### 6.3 解码逻辑详细分析

#### 主循环

1. **Wait 条件**：TCKC 上升沿 `{0: 'r'}`
2. **每次上升沿**：
   - 调用 `handle_tapc_state()` 检查 cJTAG 状态
   - 如果在 OSCAN1 模式：
     - cycle 0: nTDI = (tmsc == 0) ? 1 : 0
     - cycle 1: TMS = tmsc
     - cycle 2: TDO = tmsc → 调用 `handle_rising_tckc_edge(tdi, tdo, tckc, tms)`
   - 否则：直接调用 `handle_rising_tckc_edge(None, None, tckc, tmsc)`
3. **TCKC 高电平期间**：等待 TCKC 下降沿或 TMSC 边沿
   - 如果 TMSC 变化：`handle_tmsc_edge()` 递增 `escape_edges`

#### handle_tapc_state

```c
static void handle_tapc_state(cjtag_state *s) {
    s->oldcjtagstate = s->cjtagstate;
    
    if (s->escape_edges >= 8) {
        s->cjtagstate = CST_FOUR_WIRE;
    }
    if (s->escape_edges == 6) {
        s->cjtagstate = CST_CJTAG_OAC;  // 注意：Python 中是 CJTAG_OAC
        s->oacp = 0;
        s->oaclen = 12;
    }
    
    s->escape_edges = 0;
}
```

**注意**：Python 中 `CSt.CJTAG_OAC` 不在 `cjtag_states` 列表中！`CSt` 枚举中定义了 `CJTAG_OAC`，但 `cjtag_states` 列表只包含 `['CJTAG_EC', 'CJTAG_SPARE', ...]`。`CJTAG_OAC` 实际上是 `CSt` 枚举中的第 9 个值（索引 8），对应 `CJTAG_CP`。需要仔细核对 Python 代码中 `CSt` 枚举的定义。

Python 中 `CSt` 枚举：
```python
s = 'EC SPARE TPDEL TPREV TPST RDYC DLYC SCNFMT CP OAC'.split()
s = ['CJTAG_' + x for x in s] + ['OSCAN1', 'FOUR_WIRE']
```
所以 `CSt.CJTAG_OAC` 是第 10 个值（索引 9），`CJTAG_CP` 是索引 8。

但 `cjtag_states` 列表是 `[s.value for s in CSt]`，包含所有 11 个值。

**等等**，重新审视：`handle_tapc_state` 中设置 `s->cjtagstate = CSt.CJTAG_OAC`，但 `CJTAG_OAC` 不在注解的 cJTAG 状态列表中。实际上 Python 代码中 `CJTAG_OAC` 的索引是 9（在 CSt 枚举中），对应注解索引 `16 + 9 = 25`。

让我重新核对 CSt 枚举值：
- 0: CJTAG_EC
- 1: CJTAG_SPARE
- 2: CJTAG_TPDEL
- 3: CJTAG_TPREV
- 4: CJTAG_TPST
- 5: CJTAG_RDYC
- 6: CJTAG_DLYC
- 7: CJTAG_SCNFMT
- 8: CJTAG_CP
- 9: CJTAG_OAC
- 10: OSCAN1
- 11: FOUR_WIRE

共 12 个值，不是 11 个！cjtag_states 有 12 个元素。注解索引 16-27 是 cJTAG 状态。

#### advance_state_machine

cJTAG 状态下的 OAC (Online Activation Command) 处理：

```c
static void advance_state_machine(cjtag_state *s, int tms) {
    s->oldstate = s->state;
    
    // cJTAG 状态处理
    if (s->cjtagstate >= 0 && s->cjtagstate != CST_OSCAN1 && s->cjtagstate != CST_FOUR_WIRE) {
        // 在 CJTAG_* 状态下
        s->oacp++;
        
        if (s->oacp > 4 && s->oaclen == 12)
            s->cjtagstate = CST_CJTAG_EC;
        if (s->oacp == 8 && tms == 0)
            s->oaclen = 36;
        if (s->oacp > 8 && s->oaclen == 36)
            s->cjtagstate = CST_CJTAG_SPARE;
        if (s->oacp > 13 && s->oaclen == 36)
            s->cjtagstate = CST_CJTAG_TPDEL;
        if (s->oacp > 16 && s->oaclen == 36)
            s->cjtagstate = CST_CJTAG_TPREV;
        if (s->oacp > 18 && s->oaclen == 36)
            s->cjtagstate = CST_CJTAG_TPST;
        if (s->oacp > 23 && s->oaclen == 36)
            s->cjtagstate = CST_CJTAG_RDYC;
        if (s->oacp > 25 && s->oaclen == 36)
            s->cjtagstate = CST_CJTAG_DLYC;
        if (s->oacp > 27 && s->oaclen == 36)
            s->cjtagstate = CST_CJTAG_SCNFMT;
        if (s->oacp > 8 && s->oaclen == 12)
            s->cjtagstate = CST_CJTAG_CP;
        if (s->oacp > 32 && s->oaclen == 36)
            s->cjtagstate = CST_CJTAG_CP;
        if (s->oacp > s->oaclen) {
            s->cjtagstate = CST_OSCAN1;
            s->oscan1cycle = 1;
            s->state = ST_TEST_LOGIC_RESET;
        }
        return;
    }
    
    // 正常 JTAG 状态转换
    s->state = jtag_next_state[s->state][tms ? 1 : 0];
}
```

#### handle_rising_tckc_edge

1. 调用 `advance_state_machine(tms)`
2. 如果是第一次：保存 ss_item
3. 否则：输出旧状态注解和 cJTAG 状态注解
4. 如果在 SHIFT-*/EXIT1-* 状态：收集 TDI/TDO 比特
5. 如果切换到 UPDATE-* 状态：输出完整的 TDI/TDO 比特串

#### TCKC 高电平期间的 TMSC 监控

```c
// 在 TCKC 上升沿处理完后，等待 TCKC 下降沿或 TMSC 边沿
while (tckc == 1) {
    // wait for TCKC falling edge or TMSC edge
    srd_cond_builder *cb = c_cond_new();
    c_cond_fall(cb, 0);  // TCKC falling
    c_cond_or(cb);
    c_cond_edge(cb, 1);  // TMSC edge
    
    uint64_t sub_samplenum, sub_matched;
    int ret = c_cond_wait(cb, di, &sub_samplenum, &sub_matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;
    
    int new_tmsc = c_decoder_get_pin(di, 1, sub_samplenum);
    if (new_tmsc != tmsc) {
        tmsc = new_tmsc;
        s->escape_edges++;
    }
    
    tckc = c_decoder_get_pin(di, 0, sub_samplenum);
}
```

### 6.4 Python → C 差异与特殊处理

1. **SrdStrEnum**：Python 用自定义枚举类，C 中用整数枚举 + 字符串数组
2. **cJTAG_OAC 状态**：Python 中 `CSt.CJTAG_OAC` 存在但不在注解的 cJTAG 状态名列表中。实际上 `cjtag_states` 包含所有 CSt 枚举值，包括 `CJTAG_OAC`。C 中注解索引需要包含此状态
3. **注解索引 27**：对应 `CJTAG_OAC`，需要确认是否在注解列表中
4. **内循环**：Python 中 `while (tckc == 1)` 内嵌 `self.wait()`，C 中需要同样的嵌套 wait 循环
5. **OSCAN1 模式**：3 个时钟周期组成一个 JTAG 时钟周期（nTDI, TMS, TDO），C 中需要正确处理
6. **比特串输出**：Python 用 `''.join(map(str, bits))`，C 中用循环构建字符串
7. **first/first_bit 标志**：用于跳过第一个样本的输出。C中可用 `c_cond_wait_current()` 获取初始值替代 <!-- Updated: c_cond_wait_current已实现 -->

---

## 7. CMakeLists.txt 修改

在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加：

```cmake
qspi_c
sdio_c
spi_dual_quad_c
uart_fast_c
cjtag_c
```

每个解码器会自动编译为 `build.dir/decoders/c_decoders/qspi_c.dll` 等。

---

## 8. 通用注意事项

### 8.1 内存管理

- 所有动态分配的内存在 `destroy()` 中释放
- 使用 `g_malloc0()` 分配（自动清零）
- 使用 `g_free()` 释放

### 8.2 字符串格式化

- 使用 `snprintf()` 避免缓冲区溢出
- 注解文本通常需要 3 个缩写级别（完整、中等、最短）
- 使用 `C_ANN_PUT` 宏简化注解输出

### 8.3 边界检查

- 数组访问需要边界检查（特别是比特收集数组）
- `token` 数组大小要足够（SDIO 的 R2 响应需要 136 个条目）
- 数据缓冲区要足够大

### 8.4 浮点处理

- UART-fast 中大量使用浮点计算（bit_width 等）
- C 中使用 `double` 类型保持精度
- `round()` 函数用于四舍五入

### 8.5 条件构建器使用

- 每次循环都需要重新创建 `srd_cond_builder`
- 使用后必须 `c_cond_free()`
- `c_cond_or()` 用于分隔 OR 条件组

### 8.6 Python 输出格式

- `c_decoder_put_python()` 需要序列化数据为字节流
- 需要确保与 Python 解码器的输出格式兼容，以便上层解码器能正确解析
- 对于复杂结构（如 TRANSFER 消息中的 Data namedtuple 列表），需要仔细设计序列化格式
- **BITS 输出必须使用 BITS v2 格式**（含 per-bit ss/es 时间戳），与 spi_c.c/i2c_c.c 保持一致。格式详见 `c_decoder_utils.h` <!-- Updated: BITS v2格式已实现 -->
- **SPI DATA 输出必须使用 17 字节格式**：`data[0]=flags, data[1..8]=mosi_val(LE), data[9..16]=miso_val(LE)`，与 spi_c.c 保持一致 <!-- Updated: SPI DATA 17字节格式已实现 -->
- **C解码器只能依赖已有C实现的底层解码器**，不依赖Python解码器。如果上层解码器仅有Python实现，则OUTPUT_PYTHON输出仅用于同类型C解码器间通信 <!-- Updated: 添加C解码器依赖规则 -->

### 8.7 id 命名约定

C 解码器的 id 必须以 `_c` 结尾，例如 `qspi_c`、`sdio_c` 等。这与现有 C 解码器（如 `spi_c`、`jtag_c`）保持一致。
