# Python→C 解码器移植规格 — Batch 12

本批次涵盖 5 个 Python 解码器到 C 的移植：**sda2506**, **signature**, **sony_md**, **st7735**, **st7789**。

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
- decoder_id 中 `-` 替换为 `_`

### struct srd_c_decoder 规范
- `.id` = `"xxx_c"`（带 `_c` 后缀）
- `.name` = `"XXX(C)"`（大写原名 + `(C)` 后缀）
- `.longname` / `.desc` 与 Python 版保持一致，末尾加 `(C implementation)`

### ann_labels 规范
- 每行第一列必须为 `""`（空字符串），API 内部处理 i+7 偏移
- 格式：`{"", "短标签", "长标签"}`

### annotation_rows 规范
- 所有 annotation class 必须映射到某个 annotation_row
- 使用 `-1` 结尾的 int 数组表示 class 列表

### samplerate 守卫
- 实现 `metadata` 回调获取 samplerate
- 在 `decode()` 入口处检查 samplerate，若为 0 则尝试 `c_decoder_get_samplerate()` 获取，仍为 0 则直接 return

### Condition Builder API 用法
```c
srd_cond_builder *cb = c_cond_new();
c_cond_rise(cb, CH_XXX);   // 上升沿
c_cond_fall(cb, CH_XXX);   // 下降沿
c_cond_edge(cb, CH_XXX);   // 任意边沿
c_cond_high(cb, CH_XXX);   // 高电平
c_cond_low(cb, CH_XXX);    // 低电平
c_cond_or(cb);             // 或条件
c_cond_skip(cb, count);    // 跳过采样数
c_cond_wait(cb, di, &samplenum, &matched);  // 等待
c_cond_free(cb);           // 释放
```

<!-- Updated: c_cond_wait_current() 已实现，等效于 Python self.wait({}) / self.wait(None)，用于获取当前采样位置的引脚值 -->
```c
int ret = c_cond_wait_current(di, &samplenum);  // 等效 Python self.wait(None)
```

<!-- Updated: c_decoder_get_initial_pin() 已实现，用于获取解码开始前的初始引脚状态 -->
```c
uint8_t initial_val = c_decoder_get_initial_pin(di, channel_index);  // 返回 0xFF 表示未连接
```

### 输出宏
- `C_ANN_PUT(di, ss, es, out_ann, ann_class, ...)` — 注解输出，可变参数为多级标签字符串
- `c_decoder_put_python(di, ss, es, out_python, name, data, len)` — Python 输出
- `c_decoder_put_binary(di, ss, es, out_binary, bin_class, data, len)` — 二进制输出
<!-- Updated: c_decoder_put_logic() 和 SRD_OUTPUT_LOGIC 已实现，用于输出逻辑信号数据给上层解码器 -->
- `c_decoder_put_logic(di, start_sample, end_sample, out_logic, channel_mask, values, num_channels)` — 逻辑信号输出（通过 `c_decoder_register_output(di, SRD_OUTPUT_LOGIC, ...)` 注册）

### Options 初始化
- 在 `srd_c_decoder_entry()` 函数中初始化
- 使用 `g_variant_new_string()` / `g_variant_new_int64()` 创建默认值
- 使用 `GSList` + `g_slist_append()` 构建可选值列表

### 构建集成
- 将 decoder id（不含 `_c`）添加到 `CMakeLists.txt` 的 `C_DECODERS` 列表

---

## 1. SDA2506 — Siemens SDA 2506-5 EEPROM

### 1.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `sda2506` |
| name | `SDA2506` |
| longname | `Siemens SDA 2506-5` |
| desc | `Serial nonvolatile 1-Kbit EEPROM.` |
| inputs | `['logic']` |
| outputs | `[]` |
| license | `gplv2+` |
| tags | `['IC', 'Memory']` |

### 1.2 通道定义

| 序号 | id | name | desc | idn |
|------|----|------|------|-----|
| 0 | clk | CLK | Clock | dec_sda2506_chan_clk |
| 1 | d | DATA | Data | dec_sda2506_chan_d |
| 2 | ce | CE# | Chip-enable | dec_sda2506_chan_ce |

### 1.3 Annotations

| 索引 | id | 描述 |
|------|----|------|
| 0 | cmdbit | Command bit |
| 1 | databit | Data bit |
| 2 | cmd | Command |
| 3 | data | Data byte |
| 4 | warnings | Human-readable warnings |

### 1.4 Annotation Rows

| row id | name | 包含的 annotation 索引 |
|--------|------|------------------------|
| bits | Bits | (0, 1) |
| commands | Commands | (2,) |
| data | Data | (3,) |
| warnings | Warnings | (4,) |

### 1.5 Options

无

### 1.6 状态机分析

SDA2506 是一个 3 线（CLK/DATA/CE#）串行 EEPROM 解码器。核心逻辑：

1. **主循环**：等待 CLK 边沿或 CE 边沿
2. **命令模式**（CE=1 时 CLK 上升沿）：
   - 在 CLK 上升沿采样 DATA
   - 等待 CLK 下降沿完成一个 bit 周期
   - cmdbits 以 LSB-first 方式追加（新 bit 插入数组头部）
   - 最多保留 24 个 cmdbits
3. **数据模式**（CE=0 时 CLK 下降沿）：
   - 在 CLK 下降沿开始
   - 等待约 25μs（2.5 × 1e6/samplerate）数据就绪
   - 如果 CE 在等待期间变高，再等 CLK 上升沿或 CE 边沿
   - databits 以 LSB-first 方式追加（新 bit 插入数组头部）
   - 收集 8 个 databits 后输出一个数据字节
4. **CE 下降沿**（命令解析）：
   - 解析 addr（7 bit，从 cmdbits[1..7]）
   - 解析 CB（1 bit，从 cmdbits[0]）
   - 若 CB=0：Read 命令，解析 read 字段（7 bit）
   - 若 CB=1 且 d=0：Write 命令，解析 data 字段（8 bit），等待 CE 上升沿
   - 若 CB=1 且 d=1：Erase 命令，等待 CE 上升沿
5. **异常处理**：try/except 包裹 CE 解析，异常时 reset

### 1.7 C 实现方案

#### 状态结构体
```c
#define SDA_MAX_CMDBITS 24
#define SDA_MAX_DATABITS 8

typedef struct {
    // cmdbits: [value, ss, es] 存储
    int cmdbits_val[SDA_MAX_CMDBITS];
    uint64_t cmdbits_ss[SDA_MAX_CMDBITS];
    uint64_t cmdbits_es[SDA_MAX_CMDBITS];
    int cmdbits_count;

    int databits_val[SDA_MAX_DATABITS];
    uint64_t datastart;
    int databits_count;

    uint64_t samplerate;
    int out_ann;
} sda_state;
```

#### 通道映射
```c
#define CH_CLK 0
#define CH_DATA 1
#define CH_CE  2
```

#### 关键实现要点

1. **cmdbits 的 LSB-first 插入**：Python 用 `[(d, bitstart, self.samplenum)] + self.cmdbits` 实现头部插入。C 中用数组移位实现：
```c
// 在 cmdbits 数组头部插入新元素
for (int i = s->cmdbits_count; i > 0; i--) {
    s->cmdbits_val[i] = s->cmdbits_val[i-1];
    s->cmdbits_ss[i] = s->cmdbits_ss[i-1];
    s->cmdbits_es[i] = s->cmdbits_es[i-1];
}
s->cmdbits_val[0] = d;
s->cmdbits_ss[0] = bitstart;
s->cmdbits_es[0] = samplenum;
if (s->cmdbits_count < SDA_MAX_CMDBITS) s->cmdbits_count++;
```

2. **数据模式的 25μs 等待**：使用 `c_cond_skip` 实现：
```c
uint64_t skip_count = (uint64_t)(2.5 * (1e6 / s->samplerate));
srd_cond_builder *cb = c_cond_new();
c_cond_skip(cb, skip_count);
c_cond_or(cb);
c_cond_rise(cb, CH_CLK);
c_cond_or(cb);
c_cond_edge(cb, CH_CE);
```

3. **decode_bits / decode_field**：直接从 cmdbits 数组按 offset/width 提取值

4. **samplerate 守卫**：metadata 回调 + decode 入口检查

---

## 2. Signature — 签名分析

### 2.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `signature` |
| name | `Signature` |
| longname | `Signature analysis` |
| desc | `Annotate signature of logic patterns.` |
| inputs | `['logic']` |
| outputs | `[]` |
| license | `gplv2+` |
| tags | `['Debug/trace', 'Util', 'Encoding']` |

### 2.2 通道定义

| 序号 | id | name | desc | idn |
|------|----|------|------|-----|
| 0 | start | START | START channel | dec_signature_chan_start |
| 1 | stop | STOP | STOP channel | dec_signature_chan_stop |
| 2 | clk | CLOCK | CLOCK channel | dec_signature_chan_clk |
| 3 | data | DATA | DATA channel | dec_signature_chan_data |

### 2.3 Annotations

| 索引 | id | 描述 |
|------|----|------|
| 0 | bit0 | Bit0 |
| 1 | bit1 | Bit1 |
| 2 | start | START |
| 3 | stop | STOP |
| 4 | signature | Signature |

### 2.4 Annotation Rows

| row id | name | 包含的 annotation 索引 |
|--------|------|------------------------|
| bits | Bits | (0, 1, 2, 3) |
| signatures | Signatures | (4,) |

### 2.5 Options

| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| start_edge | START edge polarity | rising | (rising, falling) | dec_signature_opt_start_edge |
| stop_edge | STOP edge polarity | rising | (rising, falling) | dec_signature_opt_stop_edge |
| clk_edge | CLOCK edge polarity | falling | (rising, falling) | dec_signature_opt_clk_edge |
| annbits | Enable bit level annotations | no | (yes, no) | dec_signature_opt_annbits |

### 2.6 状态机分析

Signature 分析器模拟 HP 5004A 签名分析器：

1. **核心算法**：16-bit LFSR（线性反馈移位寄存器）
   - 反馈多项式：`shiftreg & 0x0291` 的 bit count + data bit，取最低位
   - 每个时钟有效沿：`incoming = (__builtin_popcount(shiftreg & 0x0291) + data) & 1`
   - 移位：`shiftreg = (incoming << 15) | (shiftreg >> 1)`

2. **门控逻辑**：
   - START 信号打开门（gate_is_open = True）
   - STOP 信号关闭门（gate_is_open = False）
   - 门打开时，每个 CLOCK 有效沿采样 DATA 并更新 shiftreg
   - 门关闭时输出签名

3. **签名显示**：16-bit 值分为 4 个 4-bit 组，每组映射到一个特殊字符：
   ```c
   // symbol_map
   0b0000→'0', 0b1000→'1', 0b0100→'2', 0b1100→'3',
   0b0010→'4', 0b1010→'5', 0b0110→'6', 0b1110→'7',
   0b0001→'8', 0b1001→'9', 0b0101→'A', 0b1101→'C',
   0b0011→'F', 0b1011→'H', 0b0111→'P', 0b1111→'U'
   ```

4. **边沿极性**：START/STOP/CLOCK 的有效沿由 options 决定

5. **位级注解**：当 `annbits=yes` 时，输出每个 bit 的注解和 START/STOP 标记

### 2.7 C 实现方案

#### 状态结构体
```c
typedef struct {
    int gate_is_open;
    uint64_t sample_start;
    int started;
    uint64_t last_samplenum;
    int prev_start;
    int prev_stop;
    uint16_t shiftreg;

    int start_edge_rising;  // 1=rising, 0=falling
    int stop_edge_rising;
    int annbits;

    int out_ann;
    uint64_t samplerate;
} sig_state;
```

#### 关键实现要点

1. **CLOCK 边沿选择**：根据 `clk_edge` option 决定等待上升沿还是下降沿
```c
srd_cond_builder *cb = c_cond_new();
if (s->clk_edge_rising)
    c_cond_rise(cb, CH_CLK);
else
    c_cond_fall(cb, CH_CLK);
```

2. **LFSR 反馈计算**：
```c
int incoming = (__builtin_popcount(s->shiftreg & 0x0291) + data_val) & 1;
s->shiftreg = (incoming << 15) | (s->shiftreg >> 1);
```
注意：MSVC 不支持 `__builtin_popcount`，需要用可移植实现：
```c
static int popcount16(uint16_t x) {
    int count = 0;
    while (x) { count += x & 1; x >>= 1; }
    return count;
}
```

3. **签名输出**：4 个 nibble 分别查 symbol_map 表
```c
static const char symbol_map[] = "0123456789ACFHP U"; // 按 4-bit 值索引
// 实际映射不是线性的，需要用查找表
static const char symbol_map[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'C', 'F', 'H', 'P', 'U'
};
// 注意：映射是 bit-reversed 的
// 0b0000→0, 0b1000→1, 0b0100→2, 0b1100→3, ...
// 即 symbol_map[bit_reverse_4(nibble)]
```
实际上 Python 的映射是直接用 4-bit 值作为 key 的 dict，C 中用数组：
```c
static const char symbol_map[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'C', 'F', 'H', 'P', 'U'
};
// 索引方式：symbol_map[(signature >> (n*4)) & 0x0f]
```

4. **Options 初始化**：4 个 string option，在 `srd_c_decoder_entry()` 中用 `g_variant_new_string()` + `GSList` 初始化

---

## 3. Sony MD Remote — Sony Minidisc LCD 远程控制

### 3.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `sony_md` |
| name | `Sony MD Remote` |
| longname | `Sony MD LCD Remote` |
| desc | `` (空) |
| inputs | `['logic']` |
| outputs | `['sony_md']` |
| license | `unknown` |
| tags | `['']` |

### 3.2 通道定义

| 序号 | id | name | desc |
|------|----|------|------|
| 0 | data | data | Data stream |

### 3.3 Annotations

| 索引 | id | 描述 |
|------|----|------|
| 0 | signals | Signals |
| 1 | bit-zero | 0 |
| 2 | bit-one | 1 |
| 3 | bit-error | Unknown half-cycle |
| 4 | state-error | State error |
| 5 | byte | Byte value |
| 6 | bit-count | Message bit count |
| 7 | bit-count-error | Expected multiple of 8 bits |

### 3.4 Annotation Rows

| row id | name | 包含的 annotation 索引 |
|--------|------|------------------------|
| signalling | Signalling | (0,) |
| raw-bits | Raw Bits | (1, 2) |
| byte-values | Byte Values | (5,) |
| Messages | Messages | (6,) |
| errors | Errors | (3, 4, 7) |

### 3.5 Options

| id | desc | default | idn |
|----|------|---------|-----|
| marginpct | Error margin % | 20 | dec_sony_md_opt_marginpct |

### 3.6 状态机分析

Sony MD Remote 是一个单线异步协议解码器，基于脉冲宽度编码：

**状态机**：
```
IDLE → PRESYNC → SYNC → DATA-BIT-HIGH → DATA-BIT-LOW → (循环或回到IDLE)
```

**时序参数**（基于 samplerate 计算）：
| 参数 | 时间 | 计算公式 |
|------|------|----------|
| Reset | 40ms | samplerate × 40/1000 |
| Presync | 1100μs | samplerate × 1100/1000000 |
| Presync Delay | 950μs (min 800μs, max 1500μs) | samplerate × 950/1000000 |
| Sync | 220μs (min 20μs) | samplerate × 220/1000000 |
| Bit Delay High | 32.5μs | samplerate × 32.5/1000000 |
| Short Data Long | 220μs (min 101μs, max 280μs) | samplerate × 220/1000000 |
| Short Data Short | 17μs (min 10μs, max 100μs) | samplerate × 17/1000000 |

**数据位解码**：
- 短脉冲（17μs）= 1
- 长脉冲（220μs）= 0

**消息结构**：
- 默认期望 16 bit
- 第 5 bit：Remote 有/无数据
- 第 9 bit：Player 有/无数据
- 第 13 bit：Player 是否让出总线
- 若 Player 让出总线且 Remote 无数据 → 错误
- 若 Player 让出总线 → 期望 115 bit
- 若 Player 有数据且不让出 → 期望 104 bit

**Python 输出**：输出 `OUTPUT_PYTHON` 格式的同步数据+位数据+结束标志

### 3.7 C 实现方案

#### 状态结构体
```c
enum sony_md_state {
    STATE_IDLE,
    STATE_PRESYNC,
    STATE_SYNC,
    STATE_DATA_BIT_HIGH,
    STATE_DATA_BIT_LOW,
};

typedef struct {
    enum sony_md_state state;

    uint64_t lastedgesample;
    int lastedgestate;
    uint64_t newedgesample;
    int newedgestate;

    int playerHasData;
    int remoteHasData;
    int playerCedesBus;

    uint64_t pulselength;

    int dataBitCount;
    int expectedBitCount;

    uint64_t databitstart;
    uint64_t databitend;

    uint64_t bytestartsample;
    uint64_t byteendsample;
    int bytevalue;

    uint64_t packetstartsample;
    uint64_t packetendsample;

    // 时序参数
    uint64_t resetCycles, resetMin, resetMax;
    uint64_t presyncCycles, presyncMin, presyncMax;
    uint64_t presyncDelayMin, presyncDelayMax;
    uint64_t syncMin, syncMax;
    uint64_t bitDelayHighMin;
    uint64_t dataLongMin, dataLongMax;
    uint64_t dataShortMin, dataShortMax;

    int marginpct;
    int out_ann;
    int out_python;
    uint64_t samplerate;
} sony_md_state;
```

#### 关键实现要点

1. **时序计算**：在 `start()` 中根据 samplerate 计算所有时序参数（与 Python 的 `start()` 一致）
```c
static void calc_timing(sony_md_state *s) {
    double margin = s->marginpct * 0.01;
    s->resetCycles = (uint64_t)(s->samplerate * 40.0 / 1000.0);
    s->resetMin = (uint64_t)(s->resetCycles * (1.0 - margin));
    s->resetMax = (uint64_t)(s->resetCycles * (1.0 + margin));
    s->presyncCycles = (uint64_t)(s->samplerate * 1100.0 / 1000000.0);
    s->presyncMin = (uint64_t)(s->presyncCycles * (1.0 - margin));
    s->presyncMax = (uint64_t)(s->presyncCycles * (1.0 + margin));
    s->presyncDelayMin = (uint64_t)(s->samplerate * 800.0 / 1000000.0);
    s->presyncDelayMax = (uint64_t)(s->samplerate * 1500.0 / 1000000.0);
    s->syncMin = (uint64_t)(s->samplerate * 20.0 / 1000000.0);
    s->syncMax = (uint64_t)(s->presyncCycles * (1.0 + margin));
    uint64_t bitDelayHighIdeal = (uint64_t)(s->samplerate * 32.5 / 1000000.0);
    s->bitDelayHighMin = (uint64_t)(bitDelayHighIdeal * (1.0 - margin));
    s->dataLongMin = (uint64_t)(s->samplerate * 101.0 / 1000000.0);
    s->dataLongMax = (uint64_t)(s->samplerate * 280.0 / 1000000.0);
    s->dataShortMin = (uint64_t)(s->samplerate * 10.0 / 1000000.0);
    s->dataShortMax = (uint64_t)(s->samplerate * 100.0 / 1000000.0);
}
```

2. **范围比较**：Python 用 `in range(min, max)`，C 中用：
```c
static int in_range(uint64_t val, uint64_t min, uint64_t max) {
    return val >= min && val < max;
}
```
注意 Python 的 `range` 是左闭右开 `[min, max)`。

3. **边沿检测**：主循环等待 data 通道的边沿
```c
srd_cond_builder *cb = c_cond_new();
c_cond_edge(cb, CH_DATA);
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

4. **Python 输出**：sony_md 有 `OUTPUT_PYTHON` 输出，需要定义输出结构体并使用 `c_decoder_put_python()`

5. **samplerate 守卫**：必须实现 metadata 回调 + decode 入口检查

---

## 4. ST7735 — Sitronix ST7735 TFT 控制器

### 4.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `st7735` |
| name | `ST7735` |
| longname | `Sitronix ST7735` |
| desc | `Sitronix ST7735 TFT controller protocol.` |
| inputs | `['logic']` |
| outputs | `[]` |
| license | `gplv2+` |
| tags | `['Display', 'IC']` |

### 4.2 通道定义

| 序号 | id | name | desc | idn |
|------|----|------|------|-----|
| 0 | cs | CS# | Chip-select | dec_st7735_chan_cs |
| 1 | clk | CLK | Clock | dec_st7735_chan_clk |
| 2 | mosi | MOSI | Master out, slave in | dec_st7735_chan_mosi |
| 3 | dc | DC | Data or command | dec_st7735_chan_dc |

### 4.3 Annotations

| 索引 | id | 描述 |
|------|----|------|
| 0 | bit | Bit |
| 1 | command | Command |
| 2 | data | Data |
| 3 | description | Description |

### 4.4 Annotation Rows

| row id | name | 包含的 annotation 索引 |
|--------|------|------------------------|
| bits | Bits | (0,) |
| fields | Fields | (1, 2) |
| description | Description | (3,) |

### 4.5 Options

无

### 4.6 状态机分析

ST7735 是一个 SPI 接口的 TFT 控制器解码器：

1. **主循环**：等待 CLK 边沿
2. **CS 高电平**：忽略，reset 状态
3. **CLK 上升沿**：采样 MOSI 位，记录 bit_ss
4. **CLK 下降沿**（且有有效 bit）：处理 bit
   - 累积 8 bit → 一个字节
   - DC=0 → Command，DC=1 → Data
5. **命令解析**：
   - 收到新 Command 时，先输出上一个 Command 的 description
   - 从 META 表查找命令名和描述
   - 未知命令输出 "Unknown command: XX. Data: ..."
6. **数据累积**：最多累积 MAX_DATA_LEN=128 字节

### 4.7 命令映射表（META）

```c
// 部分关键命令
{0x00, "NOP",     "No operation"},
{0x01, "SWRESET", "Software reset"},
{0x2A, "CASET",   "Column address set"},
{0x2B, "RASET",   "Row address set"},
{0x2C, "RAMWR",   "Memory write"},
{0x36, "MADCTL",  "Memory data address control"},
{0x3A, "COLMOD",  "Interface pixel format"},
{0xB1, "FRMCTR1", "Frame rate control (normal)"},
{0xC0, "PWCTR1",  "Power control 1"},
{0xE0, "GMCTRP1", "Gamma '+' polarity correction"},
{0xE1, "GMCTRN1", "Gamma '-' polarity correction"},
// ... 完整列表见 Python 源码
```

### 4.8 C 实现方案

#### 状态结构体
```c
#define ST7735_MAX_DATA_LEN 128

typedef struct {
    int accum_byte;
    int accum_bits_num;
    uint64_t bit_ss;
    uint64_t byte_ss;
    int current_bit;

    int current_cmd;
    uint8_t current_data[ST7735_MAX_DATA_LEN];
    int current_data_len;
    uint64_t desc_ss;
    uint64_t desc_es;

    int out_ann;
} st7735_state;
```

#### 通道映射
```c
#define CH_CS  0
#define CH_CLK 1
#define CH_MOSI 2
#define CH_DC  3
```

#### 关键实现要点

1. **CLK 双沿处理**：Python 用 `self.wait({1: 'e'})` 等待 CLK 边沿，然后在两个分支分别处理上升沿（采样）和下降沿（处理）
```c
srd_cond_builder *cb = c_cond_new();
c_cond_edge(cb, CH_CLK);
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);

int cs = c_decoder_get_pin(di, CH_CS, samplenum);
int clk = c_decoder_get_pin(di, CH_CLK, samplenum);
int mosi = c_decoder_get_pin(di, CH_MOSI, samplenum);
int dc = c_decoder_get_pin(di, CH_DC, samplenum);

if (cs == 1) { /* reset */ continue; }
if (clk == 1) { /* 上升沿：采样 */ }
if (clk == 0 && s->current_bit >= 0) { /* 下降沿：处理 */ }
```

2. **命令查找表**：用结构体数组 + 线性搜索
```c
typedef struct {
    uint8_t cmd;
    const char *name;
    const char *desc;
} st7735_cmd_entry;

static const st7735_cmd_entry st7735_cmd_table[] = {
    {0x00, "NOP",     "No operation"},
    {0x01, "SWRESET", "Software reset"},
    // ... 完整列表
    {0xFF, NULL, NULL} // 哨兵
};
```

3. **description 输出**：在收到新 Command 时输出上一个 Command+Data 的 description

---

## 5. ST7789 — Sitronix ST7789 TFT 控制器

### 5.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `st7789` |
| name | `ST7789` |
| longname | `Sitronix ST7789` |
| desc | `Sitronix ST7789 TFT controller protocol.` |
| inputs | `['logic']` |
| outputs | `[]` |
| license | `gplv2+` |
| tags | `['Display', 'SPI']` |

### 5.2 通道定义

| 序号 | id | name | desc | idn |
|------|----|------|------|-----|
| 0 | csx | CSX | Chip selection signal | dec_st7789_chan_csx |
| 1 | dcx | DCX | Clock signal | dec_st7789_chan_dcx |
| 2 | sdo | SDO | Serial output data | dec_st7789_chan_sdo |
| 3 | wrx | WRX | Command / data | dec_st7789_chan_wrx |

**注意**：Python 源码中通道描述有误（dcx 描述为 "Clock signal"，wrx 描述为 "Command / data"），实际语义是 dcx=数据/命令选择，wrx=写选通。C 实现应保持与 Python 一致的 id/name/desc。

### 5.3 Annotations

| 索引 | id | 描述 |
|------|----|------|
| 0 | bit | Bit |
| 1 | command | Command |
| 2 | data | Data |
| 3 | cmd_data | Command + Data |
| 4 | asserted | Assertion |

### 5.4 Annotation Rows

| row id | name | 包含的 annotation 索引 |
|--------|------|------------------------|
| bits | Bits | (0,) |
| bytes | Bytes | (1, 2) |
| cmd_data | Command + Data | (3,) |
| asserted | Assertion | (4,) |

### 5.5 Options

无

### 5.6 状态机分析

ST7789 与 ST7735 类似，但协议细节不同：

1. **外层循环**：等待 CSX 下降沿（片选有效）
2. **内层循环**：等待 CSX 上升沿或 DCX 边沿
   - CSX=1（片选释放）：输出 "Asserted" 注解，输出上一个 cmd+data 的组合注解，跳出内层循环
   - DCX=1 且 bit 未设置：采样 SDO 位（数据位开始）
   - DCX=0 且 bit 已设置：完成一个 bit
3. **字节完成**（8 bit）：
   - WRX=1 → Data 字节
   - WRX=0 → Command 字节
4. **命令+数据组合输出**：每个 Command 及其后续 Data 一起输出

**与 ST7735 的关键区别**：
- ST7789 用 DCX（数据/命令）和 WRX（写信号）两个信号
- ST7789 的时钟由 DCX 边沿提供（不是独立的 CLK）
- ST7789 有 "Command + Data" 组合注解行
- ST7789 有 "Asserted" 注解（CSX 有效期间）

### 5.7 命令映射表（COMMAND_MAP）

ST7789 有比 ST7735 更多的命令（约 60 个），完整列表见 Python 源码。关键命令包括：
- 0x00 NOP, 0x01 SWRESET, 0x2A CASET, 0x2B RASET, 0x2C RAMWR
- 0x36 MADCTL, 0x3A COLMOD
- 0xB0 RAMCTRL, 0xB2 PORCTRL, 0xB7 GCTRL
- 0xE0 PVGAMCTRL, 0xE1 NVGAMCTRL
- 等等

### 5.8 C 实现方案

#### 状态结构体
```c
typedef struct {
    int bit;           // 当前采样的位值，-1 表示未采样
    int bit_count;     // 当前字节已采样的位数
    uint8_t byte_val;  // 当前累积的字节值
    uint64_t bit_start_samplenum;
    uint64_t byte_sample_startnum;

    int last_cmd;      // 上一个命令字节值
    uint64_t last_cmd_data_ss;  // 上一个命令数据起始
    uint64_t last_cmd_data_es;  // 上一个命令数据结束
    uint8_t last_cmd_data[256]; // 上一个命令的数据
    int last_cmd_data_len;

    uint64_t csx_start_samplenum;

    int out_ann;
} st7789_state;
```

#### 通道映射
```c
#define CH_CSX 0
#define CH_DCX 1
#define CH_SDO 2
#define CH_WRX 3
```

#### 关键实现要点

1. **双层循环结构**：
```c
while (1) {
    // 等待 CSX 下降沿
    srd_cond_builder *cb = c_cond_new();
    c_cond_fall(cb, CH_CSX);
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;

    uint64_t csx_start = samplenum;
    // 初始化内层状态
    s->bit = -1;
    s->bit_count = 0;
    s->byte_val = 0;
    s->byte_sample_startnum = 0;

    while (1) {
        cb = c_cond_new();
        c_cond_rise(cb, CH_CSX);
        c_cond_or(cb);
        c_cond_edge(cb, CH_DCX);
        ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;

        int csx = c_decoder_get_pin(di, CH_CSX, samplenum);
        int dcx = c_decoder_get_pin(di, CH_DCX, samplenum);
        int sdo = c_decoder_get_pin(di, CH_SDO, samplenum);
        int wrx = c_decoder_get_pin(di, CH_WRX, samplenum);

        if (csx == 1) {
            // 片选释放，输出 Asserted 和 cmd_data
            break;
        }
        // ... 处理位采样和字节完成
    }
}
```

2. **命令查找表**：与 ST7735 类似但更完整

3. **cmd_data 组合输出**：在 CSX 释放或新 Command 开始时，输出上一个 Command+Data 的组合字符串

---

## 复杂度评估

| 解码器 | 复杂度 | 预估代码行数 | 关键难点 |
|--------|--------|-------------|----------|
| sda2506 | ★★★☆☆ | ~350 | cmdbits 头部插入、25μs skip 等待、CE 边沿解析 |
| signature | ★★☆☆☆ | ~250 | LFSR 反馈、symbol_map 查表、4个 string options |
| sony_md | ★★★★☆ | ~500 | 多个时序参数、范围比较、Python 输出、状态机复杂 |
| st7735 | ★★★☆☆ | ~400 | 命令表、双沿处理、description 延迟输出 |
| st7789 | ★★★★☆ | ~500 | 双层循环、DCX/WRX 协议、命令表大、cmd_data 组合 |

---

## CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加：
```
sda2506
signature
sony_md
st7735
st7789
```

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
