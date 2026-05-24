# Python 解码器移植到 C 的详细规格书 — Batch 08

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |

## 概述

本文档是将 5 个 Python 协议解码器移植为 C 解码器的详细规格。每个解码器包含完整的 Python 源码分析、C 实现计划、状态机细节和特殊处理说明。

目标文件路径：`libsigrokdecode/c_decoders/<name>_c.c`

参考 C 解码器模板：`spi_c.c`、`can_fd_c.c` <!-- Updated: 原引用ir_nec_c等非标准范本，已改为标准范本 -->

---

## 1. ieee488 — IEEE-488 GPIB/HPIB/IEC 总线解码器

### 1.1 Python 解码器元数据

| 字段 | 值 |
|------|-----|
| id | `ieee488` |
| name | `IEEE-488` |
| longname | `IEEE-488 GPIB/HPIB/IEC` |
| desc | `IEEE-488 General Purpose Interface Bus (GPIB/HPIB or IEC).` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['ieee488']` |
| tags | `['PC', 'Retro computing']` |

### 1.2 通道定义

**必需通道 (1个)：**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | dio1 | DIO1/DATA | Data I/O bit 1, or serial data | dec_ieee488_chan_dio1 |

**可选通道 (16个)：**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 1 | dio2 | DIO2 | Data I/O bit 2 | dec_ieee488_opt_chan_dio2 |
| 2 | dio3 | DIO3 | Data I/O bit 3 | dec_ieee488_opt_chan_dio3 |
| 3 | dio4 | DIO4 | Data I/O bit 4 | dec_ieee488_opt_chan_dio4 |
| 4 | dio5 | DIO5 | Data I/O bit 5 | dec_ieee488_opt_chan_dio5 |
| 5 | dio6 | DIO6 | Data I/O bit 6 | dec_ieee488_opt_chan_dio6 |
| 6 | dio7 | DIO7 | Data I/O bit 7 | dec_ieee488_opt_chan_dio7 |
| 7 | dio8 | DIO8 | Data I/O bit 8 | dec_ieee488_opt_chan_dio8 |
| 8 | eoi | EOI | End or identify | dec_ieee488_opt_chan_eoi |
| 9 | dav | DAV | Data valid | dec_ieee488_opt_chan_dav |
| 10 | nrfd | NRFD | Not ready for data | dec_ieee488_opt_chan_nrfd |
| 11 | ndac | NDAC | Not data accepted | dec_ieee488_opt_chan_ndac |
| 12 | ifc | IFC | Interface clear | dec_ieee488_opt_chan_ifc |
| 13 | srq | SRQ | Service request | dec_ieee488_opt_chan_srq |
| 14 | atn | ATN | Attention | dec_ieee488_opt_chan_atn |
| 15 | ren | REN | Remote enable | dec_ieee488_opt_chan_ren |
| 16 | clk | CLK | Serial clock | dec_ieee488_opt_chan_clk |

### 1.3 选项定义

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| iec_periph | Decode Commodore IEC bus peripherals details | 'no' | ('no', 'yes') | dec_ieee488_opt_iec_periph |
| delim | Payload data delimiter | 'eol' | ('none', 'eol') | dec_ieee488_opt_delim |

### 1.4 注解定义

**注解类 (11个)：**

| 索引 | id | 描述 |
|------|-----|------|
| 0 | bit | IEC bit |
| 1 | raw | Raw byte |
| 2 | cmd | Command |
| 3 | laddr | Listener address |
| 4 | taddr | Talker address |
| 5 | saddr | Secondary address |
| 6 | data | Data byte |
| 7 | eoi | EOI |
| 8 | text | Talker text |
| 9 | periph | IEC bus peripherals |
| 10 | warning | Warning |

**注解行 (7个)：**

| id | label | 包含的注解类索引 |
|----|-------|-----------------|
| bits | IEC bits | (0,) |
| raws | Raw bytes | (1,) |
| gpib | Commands/data | (2, 3, 4, 5, 6) |
| eois | EOI | (7,) |
| texts | Talker texts | (8,) |
| periphs | IEC peripherals | (9,) |
| warnings | Warnings | (10,) |

**二进制输出 (2个)：**

| 索引 | id | 描述 |
|------|-----|------|
| 0 | raw | Raw bytes |
| 1 | data | Talker bytes |

### 1.5 是否需要 samplerate

**否** — 此解码器不使用 samplerate，不需要 metadata 回调。

### 1.6 是否输出到其他解码器

**是** — 使用 `OUTPUT_PYTHON` 输出，需要 `c_decoder_put_python`。输出格式：

| ptype | addr | pdata |
|-------|------|-------|
| 'IEC_BIT' | 不适用 | 传输位值 (int) |
| 'GPIB_RAW' | 不适用 | 装饰后的原始字节值 (ATN时 b\|0x100) |
| 'COMMAND' | None | 命令字节值 |
| 'LISTEN' | 监听器地址 (0-30) | 原始字节值 (含0x20偏移) |
| 'TALK' | 发话器地址 (0-30) | 原始字节值 (含0x40偏移) |
| 'SECONDARY' | 副地址 (0-31) | 原始字节值 (含0x60偏移) |
| 'MSB_SET' | 原始字节值 | 原始字节值 (含0x80偏移) |
| 'DATA_BYTE' | 发话器地址 | 原始数据字节 |
| 'TALK_LISTEN' | 当前发话器 | 当前监听器列表 |
| 'TALKER_BYTES' | 发话器地址 | 累积字节序列 |
| 'TALKER_TEXT' | 发话器地址 | 累积文本序列 |

### 1.7 完整解码逻辑分析

#### 1.7.1 总体架构

此解码器支持两种传输模式，由是否存在 CLK 通道决定：
- **串行模式 (IEC)**: 需要 CLK + DIO1(DATA) + ATN
- **并行模式 (GPIB)**: 需要 DIO1-DIO8 + DAV + ATN

所有信号线都是低电平有效，需要取反后处理。

#### 1.7.2 命令表

```python
_cmd_table = {
    0x01: ['Go To Local', 'GTL'],
    0x04: ['Selected Device Clear', 'SDC'],
    0x05: ['Parallel Poll Configure', 'PPC'],
    0x08: ['Global Execute Trigger', 'GET'],
    0x09: ['Take Control', 'TCT'],
    0x11: ['Local Lock Out', 'LLO'],
    0x14: ['Device Clear', 'DCL'],
    0x15: ['Parallel Poll Unconfigure', 'PPU'],
    0x18: ['Serial Poll Enable', 'SPE'],
    0x19: ['Serial Poll Disable', 'SPD'],
    None: ['Unknown command 0x{cmd:02x}', 'command 0x{cmd:02x}', 'cmd {cmd:02x}', 'C{cmd_ord:c}'],
    0x3f: ['Unlisten', 'UNL'],
    0x5f: ['Untalk', 'UNT'],
}
```

#### 1.7.3 字节分类函数

- `_is_command(b)`: b 在 0x00-0x1f 范围 → 命令; b==0x3f → UNL; b==0x5f → UNT
- `_is_listen_addr(b)`: b 在 0x20-0x3f → 返回 b & 0x1f
- `_is_talk_addr(b)`: b 在 0x40-0x5f → 返回 b & 0x1f
- `_is_secondary_addr(b)`: b 在 0x60-0x7f → 返回 b & 0x1f
- `_is_msb_set(b)`: b & 0x80 → 返回 b

#### 1.7.4 串行解码状态机

4 个状态：

```
STEP_WAIT_READY_TO_SEND (0): 等待发送就绪
  条件: [{ATN: 'f'}, {DATA: 'l', CLK: 'h'}]
  DATA==0 且 CLK==1 → STEP_WAIT_READY_FOR_DATA

STEP_WAIT_READY_FOR_DATA (1): 等待数据就绪
  条件: [{ATN: 'f'}, {DATA: 'h', CLK: 'h'}, {CLK: 'l'}]
  DATA==1 且 CLK==1 → 记录 ss_byte, 处理 ATN/EOI 变化, 清空 bits → STEP_PREP_DATA_TEST_EOI
  CLK==0 → 传输中止 → STEP_WAIT_READY_TO_SEND

STEP_PREP_DATA_TEST_EOI (2): 准备数据/测试 EOI
  条件: [{ATN: 'f'}, {DATA: 'f'}, {CLK: 'l'}]
  DATA==0 且 CLK==1 → EOI 确认 → handle_eoi_change(True)
  CLK==0 → STEP_CLOCK_DATA_BITS, 记录 ss_bit

STEP_CLOCK_DATA_BITS (3): 时钟数据位
  条件: [{ATN: 'f'}, {CLK: 'e'}]
  CLK 上升沿 → 锁存 DATA 位
  CLK 下降沿 → 输出位注解, 8位完成后 → inject_dav_phase, 处理 EOI → STEP_WAIT_READY_TO_SEND
```

ATN 下降沿始终重置状态到 STEP_WAIT_READY_TO_SEND。

#### 1.7.5 并行解码逻辑

等待条件动态构建：DAV 边沿 + ATN 边沿 + EOI 边沿(如有) + IFC 边沿(如有)。

处理顺序（重要！低采样率时多个边沿可能在同一样本号）：
1. IFC 上升沿 → handle_ifc_change
2. EOI 上升沿 → handle_eoi_change
3. ATN 上升沿 → handle_atn_change
4. DAV 变化 → handle_dav_change
5. ATN 下降沿 → handle_atn_change
6. EOI 下降沿 → handle_eoi_change
7. IFC 下降沿 → handle_ifc_change

首次处理后，等待条件改为边沿触发 ('e')。

#### 1.7.6 数据字节处理 (handle_data_byte)

**ATN 激活时 (命令/地址)：**
1. 输出 RAW_BYTE 注解和 BIN_RAW 二进制
2. 输出 GPIB_RAW Python 数据
3. 分类字节：
   - 命令 (0x00-0x1f): 输出 CMD 注解, 更新 last_listener/last_talker
   - UNL (0x3f): 清空 last_listener
   - UNT (0x5f): 清空 last_talker
   - 监听地址 (0x20-0x3e): 输出 LADDR 注解, 添加到 last_listener
   - 发话地址 (0x40-0x5e): 输出 TADDR 注解, 设为 last_talker
   - 副地址 (0x60-0x7f): 输出 SADDR 注解
   - MSB 置位 (0x80+): 输出 SADDR 注解 (IEC 兼容)
4. 更新 IEC 外设信息
5. 输出 TALK_LISTEN Python 数据

**ATN 非激活时 (数据)：**
1. 累积字节到 accu_bytes 和 accu_text
2. 输出 DATA 注解 (带文本格式)
3. 处理 IEC 外设
4. 输出 DATA_BYTE Python 数据
5. 检查行终止符 (EOL 分隔选项)

#### 1.7.7 EOI 处理

- EOI 上升沿：记录 ss_eoi
- EOI 下降沿：如果之前 EOI 激活，输出 EOI 注解; 刷新文本累积器

#### 1.7.8 文本累积和刷新

- accu_bytes: 累积原始字节
- accu_text: 累积格式化文本
- 刷新条件：EOI 下降沿、ATN 上升沿、IFC 上升沿、行终止符后新数据
- 刷新时输出 BIN_DATA 二进制、TALKER_BYTES/TALKER_TEXT Python 数据、TEXT 注解

#### 1.7.9 IEC 外设处理 (可选)

仅在 `iec_periph` 选项为 'yes' 时激活。识别 Commodore IEC 总线外设：
- 地址 8: 'Disk 0'
- 地址 9: 'Disk 1'
- 副地址子命令: 0x60=Reopen, 0xe0=Close, 0xf0=Open

### 1.8 C 实现计划

#### 1.8.1 复杂度评估

**极高** — 这是 5 个解码器中最复杂的：
- 17 个通道 (1 必需 + 16 可选)
- 两种传输模式 (串行/并行)
- 11 个注解类 + 7 个注解行 + 2 个二进制输出
- Python 输出 (11 种 ptype)
- 文本累积和刷新逻辑
- IEC 外设子解码器
- 所有信号低电平有效需取反

#### 1.8.2 结构体设计

```c
#define PIN_DIO1  0
#define PIN_DIO2  1
#define PIN_DIO3  2
#define PIN_DIO4  3
#define PIN_DIO5  4
#define PIN_DIO6  5
#define PIN_DIO7  6
#define PIN_DIO8  7
#define PIN_EOI   8
#define PIN_DAV   9
#define PIN_NRFD  10
#define PIN_NDAC  11
#define PIN_IFC   12
#define PIN_SRQ   13
#define PIN_ATN   14
#define PIN_REN   15
#define PIN_CLK   16
#define PIN_DATA  PIN_DIO1

enum ieee488_state {
    STATE_WAIT_READY_TO_SEND = 0,
    STATE_WAIT_READY_FOR_DATA,
    STATE_PREP_DATA_TEST_EOI,
    STATE_CLOCK_DATA_BITS,
};

enum ieee488_ann {
    ANN_BIT = 0,
    ANN_RAW_BYTE,
    ANN_CMD,
    ANN_LADDR,
    ANN_TADDR,
    ANN_SADDR,
    ANN_DATA,
    ANN_EOI,
    ANN_TEXT,
    ANN_IEC_PERIPH,
    ANN_WARN,
    NUM_ANN,
};

enum ieee488_bin {
    BIN_RAW = 0,
    BIN_DATA,
    NUM_BIN,
};

#define MAX_ACCU_BYTES 4096
#define MAX_ACCU_TEXT  8192

typedef struct {
    /* 传输模式 */
    int is_serial;  /* 1=串行(IEC), 0=并行(GPIB) */

    /* 串行状态机 */
    enum ieee488_state serial_state;
    uint8_t serial_bits[8];
    int serial_bit_count;
    uint64_t ss_byte;
    uint64_t ss_bit;

    /* 当前原始字节和标志 */
    uint8_t curr_raw;
    int curr_atn;
    int curr_eoi;
    int latch_atn;
    int latch_eoi;

    /* 原始字节范围 */
    uint64_t ss_raw;
    uint64_t es_raw;

    /* EOI 范围 */
    uint64_t ss_eoi;
    uint64_t es_eoi;

    /* 文本累积 */
    uint8_t accu_bytes[MAX_ACCU_BYTES];
    int accu_bytes_len;
    char accu_text[MAX_ACCU_TEXT];
    int accu_text_len;
    uint64_t ss_text;
    uint64_t es_text;

    /* 发话器/监听器状态 */
    int last_talker;       /* -1 = None */
    int last_listener[31]; /* 排序的监听器地址列表 */
    int last_listener_count;

    /* IEC 外设状态 */
    int last_iec_addr;     /* -1 = None */
    int last_iec_sec;      /* -1 = None */

    /* 选项 */
    int iec_periph;        /* 0=no, 1=yes */
    int delim_eol;         /* 0=none, 1=eol */

    /* 输出 ID */
    int out_ann;
    int out_bin;
    int out_python;

    /* 通道可用性缓存 */
    int has_clk;
    int has_dio8;  /* 所有8个DIO线都可用 */
    int has_dav;
    int has_atn;
    int has_eoi;
    int has_ifc;
    int has_srq;

    /* 并行模式等待条件索引 */
    int idx_dav;
    int idx_atn;
    int idx_eoi;
    int idx_ifc;
    int parallel_first_pass;
} ieee488_priv;
```

#### 1.8.3 函数签名

```c
static void ieee488_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void ieee488_reset(struct srd_decoder_inst *di);
static void ieee488_start(struct srd_decoder_inst *di);
static void ieee488_decode(struct srd_decoder_inst *di);
static void ieee488_destroy(struct srd_decoder_inst *di);

/* 辅助函数 */
static uint8_t bitpack(uint8_t *bits, int count);
static int is_command(uint8_t b, int *is_unl, int *is_unt);
static int is_listen_addr(uint8_t b);
static int is_talk_addr(uint8_t b);
static int is_secondary_addr(uint8_t b);
static int is_msb_set(uint8_t b);
static void invert_pins(uint8_t *pins, int count);
static void emit_eoi_ann(struct srd_decoder_inst *di, ieee488_priv *s, uint64_t ss, uint64_t es);
static void flush_bytes_text_accu(struct srd_decoder_inst *di, ieee488_priv *s);
static void handle_ifc_change(struct srd_decoder_inst *di, ieee488_priv *s, int ifc);
static void handle_eoi_change(struct srd_decoder_inst *di, ieee488_priv *s, int eoi);
static void handle_atn_change(struct srd_decoder_inst *di, ieee488_priv *s, int atn);
static void handle_data_byte(struct srd_decoder_inst *di, ieee488_priv *s);
static void handle_dav_change(struct srd_decoder_inst *di, ieee488_priv *s, int dav, uint8_t *data);
static void inject_dav_phase(struct srd_decoder_inst *di, ieee488_priv *s, uint64_t ss, uint64_t es, uint8_t *bits);
static void decode_serial(struct srd_decoder_inst *di, ieee488_priv *s);
static void decode_parallel(struct srd_decoder_inst *di, ieee488_priv *s);
static void handle_iec_periph(struct srd_decoder_inst *di, ieee488_priv *s, uint64_t ss, uint64_t es, int addr, int sec, int data);
static const char *get_data_text(uint8_t b, char *buf, int buf_len);
```

#### 1.8.4 关键实现注意事项

1. **通道数量庞大**: 17 个通道是所有 C 解码器中最多的，需要仔细处理通道映射
2. **双模式**: decode() 入口需先判断 has_clk 决定走串行还是并行路径
3. **Python 输出**: 需要注册 SRD_OUTPUT_PYTHON 并使用 `c_decoder_put_python`
4. **二进制输出**: 需要注册 SRD_OUTPUT_BINARY 并使用 `c_decoder_put_binary`
5. **文本累积器**: 需要动态缓冲区管理，注意 MAX_ACCU_BYTES/MAX_ACCU_TEXT 的合理上限
6. **低电平有效**: 所有引脚值需要取反 (1-p)，但未连接的引脚 (值为 0xff/2) 不取反
7. **并行模式等待条件**: 首次用 'l' (低电平)，之后改为 'e' (边沿)
8. **命令表**: 需要完整的 _cmd_table 映射，未知命令用格式字符串
9. **ASCII 控制码表**: _get_data_text 中的 _control_codes 字典需要完整移植
10. **IEC 外设**: 仅在选项启用时工作，需要 Commodore 磁盘地址映射
11. **行终止符处理**: delim 选项为 'eol' 时需检测 CR(10)/LF(13) 后的非终止符字节来刷新

---

## 2. ir_irmp — IRMP 多协议红外遥控解码器

### 2.1 Python 解码器元数据

| 字段 | 值 |
|------|-----|
| id | `ir_irmp` |
| name | `IR IRMP` |
| longname | `IR IRMP` |
| desc | `IRMP infrared remote control multi protocol.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['IR']` |

### 2.2 通道定义

**必需通道 (1个)：**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | ir | IR | Data line | dec_ir_irmp_chan_ir |

**可选通道：无**

### 2.3 选项定义

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| polarity | Polarity | 'active-low' | ('active-low', 'active-high') | dec_ir_irmp_opt_polarity |

### 2.4 注解定义

**注解类 (1个)：**

| 索引 | id | 描述 |
|------|-----|------|
| 0 | packet | Packet |

**注解行 (1个)：**

| id | label | 包含的注解类索引 |
|----|-------|-----------------|
| packets | IR Packets | (0,) |

**二进制输出：无**

### 2.5 是否需要 samplerate

**是** — 需要 samplerate 来计算 rate_factor（采样率与 IRMP 库采样率的比值）。

### 2.6 是否输出到其他解码器

**否** — outputs 为空列表。

### 2.7 完整解码逻辑分析

#### 2.7.1 核心机制

此解码器**不是自行实现协议解析**，而是通过 ctypes 调用外部 `irmp.dll` / `libirmp.so` 共享库来检测红外协议。流程：

1. 加载 IRMP 共享库 (`irmp.dll` / `libirmp.so` / `libirmp.dylib`)
2. 获取库的固定采样率 (`irmp_get_sample_rate()`)
3. 验证捕获采样率必须是库采样率的整数倍
4. 计算 `rate_factor = samplerate / lib_rate`
5. 逐样本送入 IRMP 库 (`irmp_add_one_sample()`)
6. 当库返回检测到协议帧时，输出注解

#### 2.7.2 IRMP 库 API

```c
uint32_t irmp_get_sample_rate(void);           // 获取库的固定采样率
void*    irmp_instance_alloc(void);             // 分配实例
void     irmp_instance_free(void* inst);        // 释放实例
size_t   irmp_instance_id(void* inst);          // 获取实例 ID
int      irmp_instance_lock(void* inst, int);   // 锁定实例
void     irmp_instance_unlock(void* inst);      // 解锁实例
void     irmp_reset_state(void);                // 重置状态
int      irmp_add_one_sample(int level);        // 添加一个采样，返回非零表示检测到帧
int      irmp_get_result_data(ResultData*);     // 获取检测结果
char*    irmp_get_protocol_name(uint32_t proto); // 获取协议名称
```

ResultData 结构：
```c
struct ResultData {
    uint32_t protocol;
    char*    protocol_name;
    uint32_t address;
    uint32_t command;
    uint32_t flags;       // bit0=repeat, bit1=release
    uint32_t start_sample;
    uint32_t end_sample;
};
```

#### 2.7.3 解码循环

```python
ir, = self.wait()  # 获取初始 IR 值
with self.irmp:    # 锁定库实例
    self.irmp.reset_state()
    while True:
        if active == 1:
            ir = 1 - ir  # 极性取反
        if self.irmp.add_one_sample(ir):
            data = self.irmp.get_result_data()
            self.putframe(data)
        ir, = self.wait([{'skip': self.rate_factor}])  # 每 rate_factor 个样本读一次
```

#### 2.7.4 注解格式

5 个缩放级别：
```
'Protocol: {name} ({nr}), Address 0x{addr:04x}, Command: 0x{cmd:04x}, Flags: {flg}'
'P: {name} ({nr}), Addr: 0x{addr:x}, Cmd: 0x{cmd:x}, Flg: {flg}'
'P: {nr} A: 0x{addr:x} C: 0x{cmd:x} F: {flg}'
'C:{cmd:x} A:{addr:x} {flg}'
'C:{cmd:x}'
```

Flags: repeat='rep'/'r', release='rel'/'R', 无标志='-'

### 2.8 C 实现计划

#### 2.8.1 复杂度评估

**特殊** — 此解码器依赖外部 IRMP 共享库。在 C 实现中有两种路径：

**路径 A (推荐): 直接链接 IRMP 库**
- C 解码器 DLL 可以直接调用 irmp.dll 的 C API，无需 ctypes
- 更高效，无 Python 中间层
- 需要确保 irmp.dll 在运行时可用

**路径 B: 内联实现**
- 将 IRMP 核心逻辑直接编译进 C 解码器
- 工作量极大，IRMP 支持数十种协议
- 不推荐

#### 2.8.2 结构体设计

```c
enum irmp_ann {
    ANN_PACKET = 0,
    NUM_ANN,
};

typedef struct {
    uint64_t samplerate;
    int active;          /* 0=active-low, 1=active-high */
    uint64_t rate_factor;
    int out_ann;

    /* IRMP 库句柄 */
    void *irmp_lib;      /* LoadLibrary 句柄 */
    void *irmp_inst;     /* irmp_instance_alloc 返回值 */

    /* 函数指针 */
    uint32_t (*fn_get_sample_rate)(void);
    void*    (*fn_instance_alloc)(void);
    void     (*fn_instance_free)(void*);
    size_t   (*fn_instance_id)(void*);
    int      (*fn_instance_lock)(void*, int);
    void     (*fn_instance_unlock)(void*);
    void     (*fn_reset_state)(void);
    int      (*fn_add_one_sample)(int);
    int      (*fn_get_result_data)(void*);  /* ResultData* */
    char*    (*fn_get_protocol_name)(uint32_t);

    /* ResultData 缓冲 */
    uint32_t result_protocol;
    char     result_proto_name[256];
    uint32_t result_address;
    uint32_t result_command;
    uint32_t result_flags;
    uint32_t result_start;
    uint32_t result_end;
} irmp_priv;
```

#### 2.8.3 函数签名

```c
static void irmp_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void irmp_reset(struct srd_decoder_inst *di);
static void irmp_start(struct srd_decoder_inst *di);
static void irmp_decode(struct srd_decoder_inst *di);
static void irmp_destroy(struct srd_decoder_inst *di);
static int irmp_load_library(irmp_priv *s);
static void irmp_putframe(struct srd_decoder_inst *di, irmp_priv *s);
```

#### 2.8.4 关键实现注意事项

1. **外部库依赖**: 必须在运行时动态加载 `irmp.dll`（Windows）或 `libirmp.so`（Linux）
2. **采样率验证**: 捕获采样率必须是 IRMP 库采样率的整数倍，否则报错
3. **rate_factor 计算**: `rate_factor = samplerate / lib_rate`，用于 skip 等待
4. **极性处理**: active-low 时 IR 值取反后送入库
5. **逐样本送入**: 使用 `c_cond_skip(rate_factor)` 每隔 rate_factor 个样本读一次
6. **实例管理**: 需要在 start 中分配实例，destroy 中释放
7. **线程安全**: Python 版本使用 lock/unlock，C 版本可能需要类似机制
8. **ResultData 结构**: 需要定义与 IRMP 库兼容的结构体
9. **库不可用处理**: 如果 irmp.dll 不存在，解码器应优雅失败
10. **start_sample/end_sample 缩放**: 库返回的样本号需要乘以 rate_factor 转换为实际样本号
11. **初始 IR 值获取**: 使用 `c_decoder_get_initial_pin(di, 0)` 获取初始引脚状态，等效于 Python 的 `ir, = self.wait()` <!-- Updated: c_decoder_get_initial_pin已实现 -->

---

## 3. ir_ltto — LTTO 激光标签红外协议解码器

### 3.1 Python 解码器元数据

| 字段 | 值 |
|------|-----|
| id | `ir_ltto` |
| name | `IR LTTO` |
| longname | `LTTO laser tag IR` |
| desc | `A decoder for the LTTO laser tag IR protocol` |
| license | `unknown` |
| inputs | `['logic']` |
| outputs | `['ir_ltto']` |
| tags | `['Embedded/industrial']` |

### 3.2 通道定义

**必需通道 (1个)：**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | ir | IR | Demodulated IR | (无 idn) |

**可选通道：无**

### 3.3 选项定义

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| polarity | Polarity | 'active-low' | ('active-low', 'active-high') | (无 idn) |

### 3.4 注解定义

**注解类 (9个)：**

| 索引 | id | 描述 |
|------|-----|------|
| 0 | pre-sync | PRE-SYNC |
| 1 | pre-sync-pause | PRE-SYNC PAUSE |
| 2 | sync | SYNC |
| 3 | long-sync | LONG-SYNC |
| 4 | bit-pause | Bit Pause |
| 5 | bit | Bit |
| 6 | signature | Signature |
| 7 | long-sync-signature | Long SYNC Signature |
| 8 | signature-error | Error |

**注解行 (2个)：**

| id | label | 包含的注解类索引 |
|----|-------|-----------------|
| bits | Bits | (0, 1, 2, 3, 4, 5) |
| signatures | Signatures | (6, 7, 8) |

**二进制输出：无**

### 3.5 是否需要 samplerate

**是** — 所有时间参数都依赖 samplerate 计算。

### 3.6 是否输出到其他解码器

**是** — 使用 `OUTPUT_PYTHON` 输出签名为 `['SHORT'/'LONG', count, data]`。

### 3.7 完整解码逻辑分析

#### 3.7.1 时间参数（基于 samplerate）

| 参数 | 时间 | 计算公式 |
|------|------|---------|
| margin | 0.5ms | `samplerate * 0.0005 - 1` |
| presync | 3ms | `samplerate * 0.003 - 1` |
| presyncpause | 6ms | `samplerate * 0.006 - 1` |
| sync | 3ms | `samplerate * 0.003 - 1` |
| longsync | 6ms | `samplerate * 0.006 - 1` |
| bitpause | 2ms | `samplerate * 0.002 - 1` |
| dazero | 1ms | `samplerate * 0.001 - 1` |
| daone | 2ms | `samplerate * 0.002 - 1` |

#### 3.7.2 状态机

5 个状态：

```
IDLE → PSP → SYNC → BITPAUSE → BIT → BITPAUSE → BIT → ... → IDLE
                                    ↘ (bitpause 超时) → 输出签名 → IDLE
              ↘ (presyncpause 超时) → 错误 → IDLE
                        ↘ (sync 超时) → 错误 → IDLE
```

**IDLE**: 等待 PRE-SYNC 脉冲
- 条件: length 在 [presync-margin, presync+margin] 且 oldpinstate == activeState
- 动作: 输出 PRE-SYNC 注解, 清空 data/count/waslongsync, 记录 packetstartsample
- 转移: → PSP

**PSP**: 等待 PRE-SYNC 暂停
- 条件: length 在 [presyncpause-margin, presyncpause+margin]
- 动作: 输出 PRE-SYNC PAUSE 注解
- 转移: → SYNC
- 超时: 输出错误注解 → IDLE

**SYNC**: 等待 SYNC 或 LONG-SYNC 脉冲
- length 在 [sync-margin, sync+margin]: 输出 SYNC 注解 → BITPAUSE
- length 在 [longsync-margin, longsync+margin]: 输出 LONG-SYNC 注解, waslongsync=1 → BITPAUSE
- 超时: 输出错误注解 → IDLE

**BITPAUSE**: 等待位间暂停
- 条件: length 在 [bitpause-margin, bitpause+margin]
- 动作: 输出 Bit Pause 注解
- 转移: → BIT
- 超时:
  - count==0: 输出错误注解 → IDLE
  - count>0 且 waslongsync==0: 输出 signature 注解 → IDLE
  - count>0 且 waslongsync==1: 输出 long-sync-signature 注解 → IDLE

**BIT**: 读取数据位
- handle_bit(length): 判断 0 或 1
  - length 在 [dazero-margin, dazero+margin]: bit=0
  - length 在 [daone-margin, daone+margin]: bit=1
  - 其他: bit=None (错误)
- 动作: 输出位注解, data = (data << 1) | bit, count++
- 转移: → BITPAUSE
- bit==None: 输出错误注解 → IDLE

#### 3.7.3 等待条件

- BIT/BITPAUSE 状态: `[{0: 'e'}, {'skip': bitpause + margin + margin}]`
- 其他状态: `{0: 'e'}`

#### 3.7.4 签名输出格式

**SHORT 签名:**
- Python: `['SHORT', count, data]`
- 注解: `['Signature, %d bits: 0x%03X' % (count, data), 'Sig, %d: 0x%03X' % (count, data), 'S %d: 0x%03X' % (count, data)]`

**LONG 签名:**
- Python: `['LONG', count, data]`
- 注解: `['Signature, long SYNC, %d bits: 0x%03X' % (count, data), 'Sig, LS, %d: 0x%03X' % (count, data), 'S LS %d: 0x%03X' % (count, data)]`

**错误:**
- 注解: `['Error', 'Err', 'E']`

### 3.8 C 实现计划

#### 3.8.1 结构体设计

```c
#define IR_CH 0

enum ltto_state {
    STATE_IDLE = 0,
    STATE_PSP,
    STATE_SYNC,
    STATE_BITPAUSE,
    STATE_BIT,
};

enum ltto_ann {
    ANN_PRE_SYNC = 0,
    ANN_PRE_SYNC_PAUSE,
    ANN_SYNC,
    ANN_LONG_SYNC,
    ANN_BIT_PAUSE,
    ANN_BIT,
    ANN_SIGNATURE,
    ANN_LONG_SYNC_SIGNATURE,
    ANN_ERROR,
    NUM_ANN,
};

typedef struct {
    enum ltto_state state;
    uint64_t samplerate;
    int active;           /* 0=active-low, 1=active-high */
    int out_ann;
    int out_python;

    /* 时间参数 */
    uint64_t margin;
    uint64_t presync;
    uint64_t presyncpause;
    uint64_t sync;
    uint64_t longsync;
    uint64_t bitpause;
    uint64_t dazero;
    uint64_t daone;

    /* 边沿跟踪 */
    uint64_t oldedgesample;
    uint64_t newedgesample;
    int oldpinstate;
    int ir;

    /* 数据累积 */
    uint32_t data;
    int count;
    int waslongsync;
    int lastbit;
    uint64_t packetstartsample;
} ltto_priv;
```

#### 3.8.2 函数签名

```c
static void ltto_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void ltto_reset(struct srd_decoder_inst *di);
static void ltto_start(struct srd_decoder_inst *di);
static void ltto_decode(struct srd_decoder_inst *di);
static void ltto_destroy(struct srd_decoder_inst *di);
static void handle_bit(ltto_priv *s, uint64_t tick);
static void calc_rate(ltto_priv *s);
```

#### 3.8.3 关键实现注意事项

1. **时间参数用 range 检查**: Python 用 `in range(a, b)`，C 中用 `tick >= a && tick < b`
2. **位累积**: `data = (data << 1) | bit`，注意 data 用 uint32_t
3. **签名格式**: `%d bits: 0x%03X` — 3位十六进制
4. **Python 输出**: 需要 `c_decoder_put_python` 输出 SHORT/LONG 签名
5. **BIT/BITPAUSE 状态的 skip**: `bitpause + margin + margin` 作为超时
6. **极性**: activeState 在 start 中根据 polarity 选项设置
7. **初始 ir 值**: 可通过 `c_decoder_get_initial_pin(di, 0)` 获取初始引脚状态，或使用 `c_cond_wait_current()` 等效于 Python 的 `self.wait({})` <!-- Updated: c_decoder_get_initial_pin和c_cond_wait_current已实现 -->
8. **注解无 idn**: Python 源码中通道和选项都没有 idn 字段

---

## 4. ir_rc6 — RC-6 红外遥控协议解码器

### 4.1 Python 解码器元数据

| 字段 | 值 |
|------|-----|
| id | `ir_rc6` |
| name | `IR RC-6` |
| longname | `IR RC-6` |
| desc | `RC-6 infrared remote control protocol.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['IR']` |

### 4.2 通道定义

**必需通道 (1个)：**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | ir | IR | IR data line | dec_ir_rc6_chan_ir |

**可选通道：无**

### 4.3 选项定义

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| polarity | Polarity | 'auto' | ('auto', 'active-low', 'active-high') | dec_ir_rc6_opt_polarity |

### 4.4 注解定义

**注解类 (7个)：**

| 索引 | id | 描述 |
|------|-----|------|
| 0 | bit | Bit |
| 1 | sync | Sync |
| 2 | startbit | Startbit |
| 3 | field | Field |
| 4 | togglebit | Togglebit |
| 5 | address | Address |
| 6 | command | Command |

**注解行 (2个)：**

| id | label | 包含的注解类索引 |
|----|-------|-----------------|
| bits | Bits | (0,) |
| fields | Fields | (1, 2, 3, 4, 5, 6) |

**二进制输出：无**

### 4.5 是否需要 samplerate

**是** — halfbit 参数依赖 samplerate：`halfbit = int((samplerate * 0.000889) / 2.0)`

### 4.6 是否输出到其他解码器

**否** — outputs 为空列表。

### 4.7 完整解码逻辑分析

#### 4.7.1 RC-6 协议基础

- 一个位周期 = 0.889ms (半位低 + 半位高)
- halfbit = samplerate * 0.000889 / 2
- 使用曼彻斯特编码
- 同步模式: 6个半位的低电平 + 2个半位的高电平 (deltas [6, 2])

#### 4.7.2 状态机

3 个状态：
```
IDLE → SYNC → DATA → IDLE
                 ↘ (超时 6*halfbit) → IDLE
```

**IDLE**: 等待同步模式
- 检测 deltas[-2:] == [6, 2] (6个半位 + 2个半位)
- 确认后: state=SYNC, num_edges=0, bits=[]
- 极性确定:
  - auto 模式: value=1, invert=(ir==0)
  - active-high: value=ir
  - active-low: value=1-ir
- 记录同步位: bits.append((edges[-3], edges[-1], 8, value))

**SYNC → DATA**: 处理前6位
- handle_bit(): 当 len(bits)==6 时
  - bits[0]: 同步位 (宽度8, 值必须为1)
  - bits[1]: 起始位 (值必须为1)
  - bits[2-4]: 模式字段 (3位, mode = sum)
  - bits[5]: 切换位

**DATA**: 数据位处理
- 等待条件: [{0: 'e'}, {'skip': halfbit * 6}] (超时6个半位)
- 超时 → state=IDLE
- 边沿处理:
  - 每2个边沿计数一次 (num_edges % 2 == 0)
  - deltas[-2] 在 [1,2,3] 且 deltas[-1] 在 [1,2,3,6] → DATA 状态
  - 如果 deltas[-2] != deltas[-1]: 插入边界，拆分为两个位
  - 否则: 一个位跨越两个半位周期

#### 4.7.3 位表示

每个位是一个元组: (start_sample, end_sample, width_in_halfbits, value)

#### 4.7.4 包处理 (handle_package)

**Mode 0 (标准):** 22位
- bits[6-13]: 8位地址
- bits[14-21]: 8位命令
- 格式: Address: %0.2X, Data: %0.2X

**Mode 6A (短地址):** >= 15位
- bits[6] == 0: 短地址标志
- bits[6-13]: 8位地址
- bits[14+]: 可变长度数据
- 格式: Address: %0.2X, Data: %X

**Mode 6B (长地址):** >= 23位
- bits[6-21]: 16位地址
- bits[22+]: 可变长度数据
- 格式: Address: %0.2X (16位值), Data: %X

#### 4.7.5 极性处理

```python
# auto 模式下，同步检测后:
invert = (ir == 0)
value = ir if invert else 1 - ir

# 数据阶段:
value = ir if invert else 1 - ir
```

非 auto 模式:
```python
value = ir if polarity == 'active-low' else 1 - ir
```

### 4.8 C 实现计划

#### 4.8.1 结构体设计

```c
#define IR_CH 0
#define MAX_BITS 64
#define MAX_EDGES 128
#define MAX_DELTAS 128

enum rc6_state {
    STATE_IDLE = 0,
    STATE_SYNC,
    STATE_DATA,
};

enum rc6_ann {
    ANN_BIT = 0,
    ANN_SYNC,
    ANN_STARTBIT,
    ANN_FIELD,
    ANN_TOGGLEBIT,
    ANN_ADDRESS,
    ANN_COMMAND,
    NUM_ANN,
};

typedef struct {
    enum rc6_state state;
    uint64_t samplerate;
    uint64_t halfbit;
    int out_ann;

    /* 极性 */
    int invert;
    int polarity_auto;  /* 1=auto */

    /* 边沿和增量跟踪 */
    uint64_t edges[MAX_EDGES];
    int num_edges;
    uint64_t deltas[MAX_DELTAS];
    int num_deltas;

    /* 位跟踪 */
    struct {
        uint64_t ss;
        uint64_t es;
        int width;    /* 半位宽度 */
        int value;
    } bits[MAX_BITS];
    int num_bits;

    /* 模式 */
    int mode;

    /* 边沿计数 */
    int num_edges_counted;
} rc6_priv;
```

#### 4.8.2 函数签名

```c
static void rc6_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void rc6_reset(struct srd_decoder_inst *di);
static void rc6_start(struct srd_decoder_inst *di);
static void rc6_decode(struct srd_decoder_inst *di);
static void rc6_destroy(struct srd_decoder_inst *di);
static void handle_bit(struct srd_decoder_inst *di, rc6_priv *s);
static void handle_package(struct srd_decoder_inst *di, rc6_priv *s);
```

#### 4.8.3 关键实现注意事项

1. **边沿/增量缓冲区**: 需要足够大的数组，Python 版本使用动态列表
2. **半位计算**: `halfbit = (uint64_t)((double)samplerate * 0.000889 / 2.0)`
3. **delta 四舍五入**: `delta = int(delta + 0.5)` — C 中用 `(uint64_t)(delta_double + 0.5)`
4. **同步检测**: `deltas[-2:] == [6, 2]` — 检查最后两个 delta
5. **位边界插入**: 当 `deltas[-2] != deltas[-1]` 时，需要在 edges 列表中插入一个边界点
6. **auto 极性**: 同步检测时根据当前 IR 值确定极性
7. **DATA 状态超时**: skip = halfbit * 6，超时后回到 IDLE
8. **Mode 0**: 固定 22 位，8位地址 + 8位命令
9. **Mode 6**: 可变长度，区分短地址(6A)和长地址(6B)
10. **位值计算**: `sum([bits[N+i][3] << (M-i) for i in range(M)])` — MSB 优先
11. **注解格式**: Address 用 `%0.2X`，Data 用 `%0.2X` (mode 0) 或 `%X` (mode 6)

---

## 5. ir_recoil — Recoil 激光标签红外协议解码器

### 5.1 Python 解码器元数据

| 字段 | 值 |
|------|-----|
| id | `ir_recoil` |
| name | `IR Recoil` |
| longname | `Recoil laser tag IR` |
| desc | `A decoder for the Recoil laser tag IR protocol` |
| license | `unknown` |
| inputs | `['logic']` |
| outputs | `['ir_recoil']` |
| tags | `['Embedded/industrial']` |

### 5.2 通道定义

**必需通道 (1个)：**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | ir | IR | Demodulated IR | (无 idn) |

**可选通道：无**

### 5.3 选项定义

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| polarity | Polarity | 'active-low' | ('active-low', 'active-high') | (无 idn) |

### 5.4 注解定义

**注解类 (4个)：**

| 索引 | id | 描述 |
|------|-----|------|
| 0 | sync | SYNC |
| 1 | sync-pause | SYNC PAUSE |
| 2 | bit | Bit |
| 3 | packet | Packet |

**注解行 (2个)：**

| id | label | 包含的注解类索引 |
|----|-------|-----------------|
| bits | Bits | (0, 1, 2) |
| packets | Packet | (3,) |

**二进制输出：无**

### 5.5 是否需要 samplerate

**是** — 所有时间参数都依赖 samplerate 计算。

### 5.6 是否输出到其他解码器

**否** — outputs 为 `['ir_recoil']` 但 Python 代码中只注册了 OUTPUT_ANN，没有 OUTPUT_PYTHON。

### 5.7 完整解码逻辑分析

#### 5.7.1 时间参数（基于 samplerate）

| 参数 | 时间 | 计算公式 |
|------|------|---------|
| margin | 0.2ms | `samplerate * 0.0002 - 1` |
| sync | 3.3ms | `samplerate * 0.0033 - 1` |
| syncpause | 1.5ms | `samplerate * 0.0015 - 1` |
| dazero | 0.4ms | `samplerate * 0.0004 - 1` |
| daone | 0.8ms | `samplerate * 0.0008 - 1` |
| dathreshold | 0.6ms | `samplerate * 0.00059 - 1` |
| daminimum | 0.2ms | `samplerate * 0.0002 - 1` |
| damaximum | 1.2ms | `samplerate * 0.0012 - 1` |

#### 5.7.2 状态机

3 个状态：

```
IDLE → SYNCING → DATA → DATA → ... → IDLE
         ↘ (syncpause 超时) → 输出包 → IDLE
                         ↘ (位错误) → 输出包 → IDLE
```

**IDLE**: 等待 SYNC 脉冲
- 条件: length 在 [sync-margin, sync+margin] 且 oldpinstate == activeState
- 动作: 输出 SYNC 注解, data='', count=0, 记录 packetstartsample
- 转移: → SYNCING

**SYNCING**: 等待 SYNC 暂停
- 条件: length 在 [syncpause-margin, syncpause+margin]
- 动作: 输出 SYNC PAUSE 注解
- 转移: → DATA
- 超时: 输出包注解 → IDLE

**DATA**: 读取数据位
- 等待条件: `[{0: 'e'}, {'skip': damaximum + margin}]`
- handle_bit(length):
  - length 在 [daminimum, dathreshold]: bit=0
  - length 在 [dathreshold, damaximum]: bit=1
  - 其他: bit=None
- 动作: 输出位注解, data += str(bit), count++
- 超时或位错误: 输出包注解 → IDLE

#### 5.7.3 包输出格式

3 个缩放级别：
```
'Packet, %d bits: 0b%s' % (count, data)
'Pack, %d: 0b%s' % (count, data)
'P %d: 0b%s' % (count, data)
```

其中 data 是二进制字符串 (如 "10110010")。

#### 5.7.4 位判断逻辑

与 ir_ltto 不同，ir_recoil 使用**阈值区间**而非精确匹配：
- 0: [daminimum, dathreshold) = [0.2ms, 0.6ms)
- 1: [dathreshold, damaximum) = [0.6ms, 1.2ms)

#### 5.7.5 数据累积方式

ir_recoil 使用**字符串拼接**: `data = data + str(self.lastbit)`
而 ir_ltto 使用**位移**: `data = (data << 1) | self.lastbit`

这意味着 ir_recoil 的 data 是二进制字符串表示，用于输出 "0b10110010" 格式。

### 5.8 C 实现计划

#### 5.8.1 结构体设计

```c
#define IR_CH 0
#define MAX_DATA_BITS 256

enum recoil_state {
    STATE_IDLE = 0,
    STATE_SYNCING,
    STATE_DATA,
};

enum recoil_ann {
    ANN_SYNC = 0,
    ANN_SYNC_PAUSE,
    ANN_BIT,
    ANN_PACKET,
    NUM_ANN,
};

typedef struct {
    enum recoil_state state;
    uint64_t samplerate;
    int active;           /* 0=active-low, 1=active-high */
    int out_ann;

    /* 时间参数 */
    uint64_t margin;
    uint64_t sync;
    uint64_t syncpause;
    uint64_t dazero;
    uint64_t daone;
    uint64_t dathreshold;
    uint64_t daminimum;
    uint64_t damaximum;

    /* 边沿跟踪 */
    uint64_t oldedgesample;
    uint64_t newedgesample;
    int oldpinstate;
    int ir;

    /* 数据累积 */
    char data[MAX_DATA_BITS + 1];  /* 二进制字符串, 如 "10110010" */
    int count;
    int lastbit;
    uint64_t packetstartsample;
} recoil_priv;
```

#### 5.8.2 函数签名

```c
static void recoil_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void recoil_reset(struct srd_decoder_inst *di);
static void recoil_start(struct srd_decoder_inst *di);
static void recoil_decode(struct srd_decoder_inst *di);
static void recoil_destroy(struct srd_decoder_inst *di);
static void handle_bit(recoil_priv *s, uint64_t tick);
static void calc_rate(recoil_priv *s);
```

#### 5.8.3 关键实现注意事项

1. **数据格式**: 使用字符串拼接而非位移，输出 "0b10110010" 格式
2. **阈值判断**: 使用 [daminimum, dathreshold) 和 [dathreshold, damaximum) 区间
3. **DATA 状态超时**: skip = damaximum + margin
4. **包注解范围**: packetstartsample 到 oldedgesample + 1
5. **SYNC 脉冲条件**: 需要同时检查长度和 oldpinstate == activeState
6. **注解无 idn**: Python 源码中通道和选项都没有 idn 字段
7. **license 为 unknown**: C 解码器中也使用 "unknown"
8. **极性**: 只有 active-low 和 active-high，没有 auto
9. **初始引脚状态**: 使用 `c_decoder_get_initial_pin(di, 0)` 获取初始 IR 值，等效于 Python 的 `self.ir` 初始值 <!-- Updated: c_decoder_get_initial_pin已实现 -->

---

## 6. 通用实现模式

### 6.1 C 解码器文件模板

每个 C 解码器文件需要包含以下部分：

```c
#include "libsigrokdecode.h"
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 1. 状态枚举 */
/* 2. 注解枚举 */
/* 3. 通道常量 */
/* 4. 私有结构体 */
/* 5. 通道定义数组 */
/* 6. 选项数组声明 */
/* 7. 注解标签数组 */
/* 8. 注解行定义 */
/* 9. 输入/输出/标签数组 */
/* 10. 辅助函数 */
/* 11. metadata 回调 */
/* 12. reset 回调 */
/* 13. start 回调 */
/* 14. decode 回调 */
/* 15. destroy 回调 */
/* 16. srd_c_decoder 结构体 */
/* 17. srd_c_decoder_entry() 入口函数 */
/* 18. srd_c_decoder_api_version() 函数 */
```

### 6.2 通道定义格式

```c
static struct srd_channel xxx_channels[] = {
    { "id", "name", "desc", order, SRD_CHANNEL_SDATA, "idn" },
};
```

### 6.3 可选通道定义格式

```c
static struct srd_channel xxx_optional_channels[] = {
    { "id", "name", "desc", order, SRD_CHANNEL_COMMON, "idn" },
};
```

### 6.4 选项初始化 (在 srd_c_decoder_entry 中)

```c
GVariant* vals[] = { g_variant_new_string("val1"), g_variant_new_string("val2") };
GSList* list = NULL;
list = g_slist_append(list, vals[0]);
list = g_slist_append(list, vals[1]);
xxx_options_arr[N].id = "option_id";
xxx_options_arr[N].idn = "idn";
xxx_options_arr[N].desc = "Description";
xxx_options_arr[N].def = g_variant_new_string("default");
xxx_options_arr[N].values = list;
```

### 6.5 注解标签格式

```c
static const char* xxx_ann_labels[][3] = {
    { "", "id", "Description" },  /* 索引 0 */
    { "", "id2", "Description2" }, /* 索引 1 */
};
```

### 6.6 注解行格式

```c
static const int xxx_row_xxx_classes[] = { ANN_XX, ANN_YY, -1 };
static const struct srd_c_ann_row xxx_ann_rows[] = {
    { "row_id", "Row Label", xxx_row_xxx_classes, count },
};
```

### 6.7 CMakeLists.txt 修改

在 CMakeLists.txt 的 `C_DECODERS` 列表中添加新的解码器名称：
```
ieee488_c
ir_irmp_c
ir_ltto_c
ir_rc6_c
ir_recoil_c
```

### 6.8 条件构建器使用模式

```c
srd_cond_builder* cb = c_cond_new();
c_cond_edge(cb, IR_CH);        /* 等待任意边沿 */
c_cond_or(cb);                  /* 或 */
c_cond_skip(cb, count);         /* 跳过 N 个样本 */
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
if (ret != SRD_OK) return;

/* 检查哪个条件匹配 */
if (matched & (1ULL << 0)) { /* 第一个条件匹配 */ }
if (matched & (1ULL << 1)) { /* 第二个条件匹配 (skip) */ }
```

### 6.8.1 获取初始引脚状态 <!-- Updated: 新增c_cond_wait_current和c_decoder_get_initial_pin说明 -->

等效于 Python 的 `self.wait({})`（无条件等待，获取当前样本位置和引脚值）：

```c
/* 方法1: c_cond_wait_current — 等效于 self.wait({})，获取当前样本位置 */
uint64_t cur_sample;
if (c_cond_wait_current(di, &cur_sample) != SRD_OK)
    return;
int ir = c_decoder_get_pin(di, 0, cur_sample);

/* 方法2: c_decoder_get_initial_pin — 直接获取初始引脚状态（不推进样本位置） */
uint8_t init_ir = c_decoder_get_initial_pin(di, 0);
```

### 6.9 Python 输出使用模式

```c
/* 注册 Python 输出 */
int out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "proto_id");

/* 输出 Python 数据 */
unsigned char py_data[...];
/* 填充 py_data */
c_decoder_put_python(di, ss, es, out_python, "PTYPE", py_data, data_len);
```

### 6.10 二进制输出使用模式

```c
/* 注册二进制输出 */
int out_bin = c_decoder_register_output(di, SRD_OUTPUT_BINARY, "proto_id");

/* 输出二进制数据 */
c_decoder_put_binary(di, ss, es, out_bin, bin_class, size, data_ptr);
```

### 6.11 Logic 输出使用模式 <!-- Updated: 新增c_decoder_put_logic说明，SRD_OUTPUT_LOGIC已实现 -->

```c
/* 注册 Logic 输出 */
int out_logic = c_decoder_register_output(di, SRD_OUTPUT_LOGIC, "proto_id");

/* 输出 Logic 数据 */
uint8_t values[NUM_CHANNELS];
/* 填充 values 数组，每个元素为 0 或 1 */
uint32_t channel_mask = (1 << 0) | (1 << 1); /* 使用的通道掩码 */
c_decoder_put_logic(di, ss, es, out_logic, channel_mask, values, NUM_CHANNELS);
```

---

## 7. 各解码器差异对比

| 特性 | ieee488 | ir_irmp | ir_ltto | ir_rc6 | ir_recoil |
|------|---------|---------|---------|--------|-----------|
| 通道数 | 1+16 | 1 | 1 | 1 | 1 |
| 需要samplerate | 否 | 是 | 是 | 是 | 是 |
| Python输出 | 是(11种) | 否 | 是 | 否 | 否 |
| 二进制输出 | 是(2种) | 否 | 否 | 否 | 否 |
| 状态机复杂度 | 极高 | N/A | 中 | 高 | 低 |
| 外部依赖 | 无 | irmp.dll | 无 | 无 | 无 |
| 极性auto | 否 | 否 | 否 | 是 | 否 |
| 实现难度 | 极高 | 特殊 | 中 | 高 | 低 |

---

## 8. 实现优先级建议

1. **ir_recoil** — 最简单，3状态，无Python输出，可作为热身
2. **ir_ltto** — 中等复杂度，5状态，有Python输出
3. **ir_rc6** — 较复杂，曼彻斯特编码，auto极性
4. **ir_irmp** — 特殊，依赖外部库
5. **ieee488** — 最复杂，双模式，17通道，大量Python输出

---

## 9. 移植中的通用注意事项

### 9.1 Python `range()` 到 C 的转换

Python `x in range(a, b)` → C `x >= a && x < b`

注意 Python range 是左闭右开 `[a, b)`。

### 9.2 Python `self.samplenum` 到 C

C 解码器中通过 `c_cond_wait()` 返回的 `samplenum` 获取当前样本号。

### 9.3 Python `self.matched` 到 C

C 中通过 `c_cond_wait()` 返回的 `matched` 位掩码判断哪个条件匹配。

### 9.4 Python `self.wait()` 到 C

使用 `srd_cond_builder` + `c_cond_wait()` 组合替代。

### 9.5 Python `self.put()` 到 C

使用 `C_ANN_PUT` 宏替代。

### 9.6 Python 动态列表到 C

使用固定大小数组 + 计数器替代，需要设定合理的最大值。

### 9.7 Python 字符串格式化到 C

使用 `snprintf()` 替代 Python 的 `.format()` 和 f-string。

### 9.8 Python `True`/`False` 到 C

使用 `1`/`0` 或 `TRUE`/`FALSE` 宏。

### 9.9 Python `None` 到 C

使用 `-1` 或特定哨兵值表示"无"状态。

### 9.10 内存管理

- `reset()` 中使用 `g_malloc0()` 分配私有结构体
- `destroy()` 中使用 `g_free()` 释放
- 选项的 GVariant 和 GSList 在 `srd_c_decoder_entry()` 中创建，不需要手动释放
