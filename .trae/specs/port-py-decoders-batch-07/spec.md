# Python→C 解码器移植规范 — Batch 07

本规范覆盖 5 个 Python 解码器到 C 的移植。每个解码器包含完整的 Python 源码分析、C 实现计划、关键实现注意事项和 Python→C 差异处理。

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |

## 目录 — ETH Auto Negotiation](#1-eth_an)
2. [fsi — Flexible Service Interface](#2-fsi)
3. [gpib — General Purpose Interface Bus](#3-gpib)
4. [guess_bitrate — Guess bitrate/baudrate](#4-guess_bitrate)
5. [iec — Commodore IEC bus](#5-iec)
6. [通用 C 解码器模式参考](#6-通用c解码器模式参考)

---

## 1. eth_an

### 1.1 Python 解码器元数据

| 字段 | 值 |
|------|-----|
| id | `eth_auto_negotiation` |
| name | `ETH_AN` |
| longname | `ETH Auto Negotiation` |
| desc | `ETH Auto Negotiation protocol.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['eth_an']` |
| tags | `['PC']` |

**channels (1个):**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | dp | TX+ | ETH TX+ signal | dec_eth_an_chan_dp |

**optional_channels:** 无

**options:** 无

**annotations (5个):**

| 索引 | id | desc |
|------|-----|------|
| 0 | data | FLP data |
| 1 | format | format describe |
| 2 | bitd | Bit desc |
| 3 | bit | Bit |
| 4 | NLP | Normal link pulses |

**annotation_rows (5个):**

| id | label | 包含的annotation class索引 |
|-----|-------|--------------------------|
| data | Data | (0,) |
| format | Format | (1,) |
| bitd | Bit desc | (2,) |
| bit | Bit | (3,) |
| NLP | NLP | (4,) |

**binary:** 无

**是否使用 samplerate:** 是 — `metadata()` 回调中获取，用于计算脉冲宽度时间

**是否输出到其他解码器:** 是 — `start()` 中注册了 `OUTPUT_PYTHON`、`OUTPUT_ANN`、`OUTPUT_BINARY`

### 1.2 Python 解码逻辑完整分析

#### 状态机

解码器有一个字符串状态机，初始状态为 `'base page'`，通过 `changeState()` 转换：

```
base page → base page ack → next page → next page ack → base page (循环)
```

转换条件：
- `base page → base page ack`: 当 `pre_hex != hex` 且 `(hex>>14)&0x3 == 0x3`（ACK和NP位都为1）
- `base page ack → next page`: 无条件（当 `pre_hex != hex`）
- `next page → next page ack`: 当 `(hex>>14)&0x3 == 0x1`（ACK=0, NP=1）
- `next page ack → base page`: 无条件

#### decodeTiming() — 时序解码

1. 等待通道0的上升沿 `{0: 'r'}`，记录 `ss`
2. 等待通道0的任意边沿 `{0: 'e'}`，记录 `es`
3. 计算脉冲宽度 `length = (es - ss) / samplerate`
4. 如果 `1us <= length <= 10us`：
   - 输出 NLP 注解 `[4, ['NLP']]`
   - 计算与上一个有效脉冲的间隔 `temp_length`
   - 如果 `60us <= 间隔 <= 70us`：设置 `hex` 的第 `index` 位为1，记录 data_list 条目，`last_vaild_ss = 0`
   - 如果 `120us <= 间隔 <= 140us`：记录 data_list 条目（调整位置），不设置位（保持0）
   - 更新 `pre_ss`、`pre_es`

**关键时序参数：**
- NLP 脉冲宽度：1μs ~ 10μs
- 逻辑1间隔：60μs ~ 70μs（两个NLP之间）
- 逻辑0间隔：120μs ~ 140μs（两个NLP之间，间隔翻倍）

#### data_list 结构

每个 bit 有一个字典：
```python
{'start': ss, 'end': es, 'Pre start': pre_ss, 'Pre end': pre_es}
```
- `start/end`: 当前NLP脉冲的采样范围
- `Pre start/Pre end`: 前一个NLP脉冲的采样范围

#### decodeBasePage() — 基页解码

当收集到16位数据且状态为 `base page` 或 `base page ack` 时调用。

**base_page_ta_dict (bit 5-15 的描述):**

| bit | 描述 |
|-----|------|
| 5 | 10BaseT-HD |
| 6 | 10BaseT-FD |
| 7 | 100BaseTX-HD |
| 8 | 100BaseTX-FD |
| 9 | 100BaseT4 |
| 10 | FC |
| 11 | AsyFC |
| 12 | Reserved |
| 13 | RF |
| 14 | ACK |
| 15 | NP |

**Selector field (bit 0-4):**
- `0x01` → `802.3`
- `0x02` → `802.9`
- 其他 → `unknow`

**输出注解：**
1. 对每个 bit i (0-15)：输出 `[3, [hex值]]` 范围为 `data_list[i]['Pre start']` 到 `data_list[i]['end']`
2. 对 bit 5-15 中的有效键：输出 `[2, [描述]]` 同范围
3. 整体：`[0, ["base page:"+hex(self.hex)]]` 范围 `data_list[0]['Pre start']` 到 `data_list[15]['end']`
4. Selector field：`[1, ["Selector field:"+hex(hex&0x1f)]]` 范围 `data_list[0]['Pre start']` 到 `data_list[4]['end']`
5. Selector type：`[2, [type_desc]]` 同范围
6. Technology ability：`[1, ["Technology ability field:"+hex((hex>>5)&0xff)]]` 范围 `data_list[5]['Pre start']` 到 `data_list[12]['end']`
7. Other fields：`[1, ["Other fields:"+hex((hex>>13)&0x7)]]` 范围 `data_list[13]['Pre start']` 到 `data_list[15]['end']`

#### decodeNextPage() — 下一页解码

**next_page_ta_dict (bit 0-10，当 MP=1 时):**

| bit | 描述 |
|-----|------|
| 0 | 1000BaseT M/S CFG EN |
| 1 | 1000BaseT M/S CFG Vale |
| 2 | Port type |
| 3 | 1000BaseT-FD |
| 4 | 1000BaseT-HD |
| 5 | 10GBaseT-FD |
| 6 | 10GBaseT M/S CFG EN |
| 7 | 10GBaseT M/S CFG Vale |
| 8-10 | Reserved |

**next_page_ot_dict (bit 11-15):**

| bit | 描述 |
|-----|------|
| 11 | T |
| 12 | ACK2 |
| 13 | MP |
| 14 | ACK |
| 15 | NP |

**MP位 (bit 13):**
- MP=1: "Technology ability field:" + hex(hex & 0x7ff)
- MP=0: "Unformatted code field:" + hex(hex & 0x7ff)，额外输出 "Master-Slave seed value (MSB)"

**输出注解：**
1. 对每个 bit i (0-15)：输出 `[3, [hex值]]`
2. 对 bit 11-15：输出 `[2, [描述]]`
3. 如果 MP=1，对 bit 0-10：输出 `[2, [描述]]`
4. 整体：`[0, ["next page:"+hex]]`
5. 字段：`[1, [mp描述+hex(hex&0x7ff)]]` 范围 bit 0-10
6. 如果 MP=0：`[2, ["Master-Slave seed value (MSB)"]]`
7. Other fields：`[1, ["Other fields:"+hex((hex>>11)&0x1f)]]` 范围 bit 11-15

#### decode() 主循环

```python
while True:
    decodeTiming()
    if index == 16:
        changeState()
        if state == 'base page' or state == 'base page ack':
            decodeBasePage()
        elif state == 'next page' or state == 'next page ack':
            decodeNextPage()
        updateOnceDecode()
```

`updateOnceDecode()` 保存 `pre_hex = hex`，然后清空 `data_list`、`index`、`hex`。

### 1.3 C 实现计划

#### 文件名
`libsigrokdecode/c_decoders/eth_an_c.c`

#### 注解枚举

```c
enum {
    ANN_DATA = 0,    // FLP data
    ANN_FORMAT,      // format describe
    ANN_BITD,        // Bit desc
    ANN_BIT,         // Bit
    ANN_NLP,         // Normal link pulses
    NUM_ANN,
};
```

#### 状态枚举

```c
enum eth_an_state {
    STATE_BASE_PAGE,
    STATE_BASE_PAGE_ACK,
    STATE_NEXT_PAGE,
    STATE_NEXT_PAGE_ACK,
};
```

#### 私有数据结构

```c
struct eth_an_priv {
    uint64_t samplerate;
    uint64_t ss, es;
    uint64_t pre_ss, pre_es;
    uint64_t last_valid_ss;
    uint64_t samplenum;
    uint16_t hex;
    uint16_t pre_hex;
    int index;
    int state;  // enum eth_an_state

    // data_list: 16个条目，每个条目4个uint64_t
    uint64_t dl_start[16];
    uint64_t dl_end[16];
    uint64_t dl_pre_start[16];
    uint64_t dl_pre_end[16];

    int out_ann;
    int out_python;
    int out_binary;
};
```

#### 函数签名

```c
static void eth_an_reset(struct srd_decoder_inst *di);
static void eth_an_start(struct srd_decoder_inst *di);
static void eth_an_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void eth_an_decode(struct srd_decoder_inst *di);
static void eth_an_destroy(struct srd_decoder_inst *di);
// 辅助函数
static void eth_an_change_state(struct eth_an_priv *s);
static void eth_an_decode_base_page(struct srd_decoder_inst *di, struct eth_an_priv *s);
static void eth_an_decode_next_page(struct srd_decoder_inst *di, struct eth_an_priv *s);
```

#### 通道定义

```c
static struct srd_channel eth_an_channels[] = {
    {"dp", "TX+", "ETH TX+ signal", 0, SRD_CHANNEL_ADATA, NULL},
};
```

#### 注解标签

```c
static const char *eth_an_ann_labels[][3] = {
    {"", "data", "FLP data"},
    {"", "format", "format describe"},
    {"", "bitd", "Bit desc"},
    {"", "bit", "Bit"},
    {"", "nlp", "Normal link pulses"},
};
```

#### 注解行

```c
static const int eth_an_row_data_classes[] = {ANN_DATA};
static const int eth_an_row_format_classes[] = {ANN_FORMAT};
static const int eth_an_row_bitd_classes[] = {ANN_BITD};
static const int eth_an_row_bit_classes[] = {ANN_BIT};
static const int eth_an_row_nlp_classes[] = {ANN_NLP};
static const struct srd_c_ann_row eth_an_ann_rows[] = {
    {"data", "Data", eth_an_row_data_classes, 1},
    {"format", "Format", eth_an_row_format_classes, 1},
    {"bitd", "Bit desc", eth_an_row_bitd_classes, 1},
    {"bit", "Bit", eth_an_row_bit_classes, 1},
    {"nlp", "NLP", eth_an_row_nlp_classes, 1},
};
```

### 1.4 关键实现注意事项

1. **samplerate 必需**：解码器依赖 samplerate 计算脉冲宽度时间。如果 samplerate 为 0，decode 函数应直接返回。

2. **浮点时间比较**：Python 使用 `float` 计算时间，C 中应使用 `double`：
   ```c
   double length = (double)(es - ss) / (double)samplerate;
   if (length <= 1.0e-5 && length >= 1.0e-6) { ... }
   ```

3. **data_list 用固定数组替代**：Python 用 list of dict，C 中用 4 个 `uint64_t[16]` 数组。

4. **wait 条件映射**：
   - `{0: 'r'}` → `c_cond_rise(cb, 0)`
   - `{0: 'e'}` → `c_cond_edge(cb, 0)`
   - 这两个 wait 是顺序执行的，不是并行的

5. **hex 格式化**：Python `str(hex(value))` 输出 `0x...` 格式，C 中用 `snprintf(buf, sizeof(buf), "0x%x", value)`。

6. **OUTPUT_PYTHON 输出**：Python 版本注册了 `out_python`，但实际 decode 中从未调用 `self.put(..., self.out_python, ...)`，因此 C 版本可以不注册 python 输出，或者注册但不使用。建议不注册以简化。

7. **OUTPUT_BINARY 输出**：同上，注册了但未使用，C 版本不需要注册。

8. **位操作**：`self.hex|0x1<<self.index` 需注意 C 中类型为 `uint16_t`，移位不会溢出。

9. **间隔计算中的 `temp_length >> 1`**：Python 中 `self.ss-(temp_length>>1)` 是整数运算，C 中保持一致用 `uint64_t`。

10. **状态转换逻辑**：`changeState()` 仅在 `pre_hex != hex` 时执行，`updateOnceDecode()` 先保存 `pre_hex` 再清空。

### 1.5 Python→C 差异处理

| Python 特性 | C 处理方式 |
|------------|-----------|
| `self.wait({0: 'r'})` | `c_cond_rise(cb, 0); c_cond_wait(cb, di, &samplenum, &matched);` |
| `self.wait({0: 'e'})` | `c_cond_edge(cb, 0); c_cond_wait(cb, di, &samplenum, &matched);` |
| `self.samplenum` | `c_cond_wait` 返回的 `samplenum` |
| `float` 时间计算 | `double` 类型 |
| `str(hex(value))` | `snprintf(buf, size, "0x%x", value)` |
| `list of dict` | 固定大小数组 |
| `self.put(ss, es, out, [cls, [text]])` | `C_ANN_PUT(di, ss, es, out, cls, text)` |

---

## 2. fsi

### 2.1 Python 解码器元数据

| 字段 | 值 |
|------|-----|
| id | `fsi` |
| name | `FSI` |
| longname | `Flexible Service Interface` |
| desc | `Protocol for FSI devices on Raptor OpenPOWER systems.` |
| license | `agplv3` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['PC']` |

**channels (2个):**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | data | DATA | Frame | dec_fsi_chan_data |
| 1 | clock | CLOCK | Clock | dec_fsi_chan_clock |

**optional_channels:** 无

**options:** 无

**annotations (9个):**

| 索引 | id | desc |
|------|-----|------|
| 0 | warnings | Warnings |
| 1 | start | Start |
| 2 | cycle-type | Cycle type |
| 3 | direction | Direction |
| 4 | addr | Address |
| 5 | data | Data |
| 6 | commands | Commands |
| 7 | crc | CRC |
| 8 | turn-around | TAR |

**annotation_rows (2个):**

| id | label | 包含的annotation class索引 |
|-----|-------|--------------------------|
| data | Data | (1, 2, 3, 4, 5, 6, 7, 8) |
| warnings | Warnings | (0,) |

**binary:** 无

**是否使用 samplerate:** 否 — 无 metadata 回调

**是否输出到其他解码器:** 否 — outputs 为空列表

### 2.2 Python 解码逻辑完整分析

#### 核心机制

FSI 是 IBM OpenPOWER 系统上的串行协议。数据在时钟边沿采样，但主从设备使用不同的时钟边沿：
- **主设备发送**：从设备在时钟上升沿采样 → 解码器在上升沿处理
- **从设备发送**：主设备在时钟下降沿采样 → 解码器在下降沿处理

**数据电性反转**：`fsi_data = not data`（物理信号是反相的）

#### 状态机（13个状态）

```
IDLE → TX_SLAVE_ID → COMMAND → DIRECTION → REL_ADDRESS_SIGN → ADDRESS → DATA_SIZE → TX_DATA → CRC → TAR → (RX_SLAVE_ID → RESPONSE → RX_DATA/RX_IPOLL_*) → CRC → TAR → ...
BREAK_TAR_QUEUED → BREAK_TAR → IDLE
```

完整状态列表：
1. `IDLE` — 等待 START 位（fsi_data_prev == 1）
2. `TX_SLAVE_ID` — 接收2位 slave ID
3. `COMMAND` — 接收2-3位命令码
4. `DIRECTION` — 接收1位方向（1=Read, 0=Write）
5. `REL_ADDRESS_SIGN` — 仅 REL_ADR 命令，1位符号
6. `ADDRESS` — 接收地址（2/8/21位）
7. `DATA_SIZE` — 确定数据大小或检测 TERM 命令
8. `TX_DATA` — 发送数据（8/16/32位）
9. `CRC` — 接收4位 CRC
10. `TAR` — Turn-around 周期
11. `RX_SLAVE_ID` — 接收响应 slave ID（2位）
12. `RESPONSE` — 接收响应码（1-2位）
13. `RX_DATA` — 接收读数据（8/16/32位）
14. `RX_IPOLL_INTERRUPT_FIELD` — I_POLL 响应中断字段（2位）
15. `RX_IPOLL_DMA_CONTROL_FIELD` — I_POLL 响应 DMA 控制字段（3位）
16. `BREAK_TAR_QUEUED` — BREAK 后的 TAR 入口
17. `BREAK_TAR` — BREAK 后的 TAR 周期

#### 命令码

| 命令 | 位数 | 码值 | 地址长度 |
|------|------|------|---------|
| ABS_ADR | 3 | 0b100 | 21 |
| REL_ADR | 3 | 0b101 | 8 |
| SAME_ADR | 2 | 0b11 | 2 |
| D_POLL | 3 | 0b010 | - |
| E_POLL | 3 | 0b011 | - |
| I_POLL | 3 | 0b001 | - |
| TERM | 6 | 0b111111 | - (在 DATA_SIZE 状态检测) |

#### 响应码

| 响应 | 位数 | 码值 | 含义 |
|------|------|------|------|
| I_POLL_RSP | 1 | 0b0 | I_POLL 响应 |
| ACK_D | 2 | 0b00 | 读确认 |
| ACK | 2 | 0b00 | 写确认 |
| BUSY | 2 | 0b01 | 忙 |
| ERR_A | 2 | 0b10 | 地址错误 |
| ERR_C | 2 | 0b11 | 命令错误 |

#### BREAK 检测

在时钟上升沿检测：连续256个采样点数据为1（反相后）视为 BREAK。

#### CRC 计算

Galois LFSR，多项式 0x7（MSB first）：
```python
crc_feedback = (((crc_prev >> 3) & 1) ^ fsi_data_prev) & 1
crc_internal bit0 = crc_feedback
crc_internal bit1 = (crc_prev & 1) ^ crc_feedback
crc_internal bit2 = ((crc_prev >> 1) & 1) ^ crc_feedback
crc_internal bit3 = ((crc_prev >> 2) & 1)
```

4位 CRC，在 `crc_count == 0` 时保存 `computed_crc_tx_end = crc_internal`，然后接收4位 CRC 与计算值比较。

#### TAR (Turn-Around) 周期

- `tar_cycles = 3`（固定）
- 在 TAR 状态中，每收到一个时钟边沿 `tar_timer++`
- 当 `tar_timer > tar_cycles` 时：
  - 如果已收到响应，处理响应逻辑
  - 检测新的 START 位（fsi_data_prev == 1）
  - 如果超时256个周期，输出超时警告

#### 主从采样边沿选择

```python
# 从设备发送时（TAR, RX_SLAVE_ID, RESPONSE, RX_DATA, RX_IPOLL_*, CRC且valid_response）
# 采样下降沿
if (fsi_clk): continue  # 跳过上升沿

# 主设备发送时（其他状态）
# 采样上升沿
if (not fsi_clk): continue  # 跳过下降沿
```

#### 地址处理

- `ABS_ADR`: 直接使用21位地址
- `SAME_ADR`: `(prev_address & ~0b11) | (address_raw & 0b11)`
- `REL_ADR`: 正数 `prev_address + address_raw`，负数 `prev_address - (0x100 - address_raw)`
- `prev_address` 按 `tx_slave_id` (0-3) 索引存储

#### DATA_SIZE 状态中的 TERM 检测

条件：`direction == 1 AND (address_raw & 3) == 3 AND fsi_data_prev == 1`
此时命令改为 TERM，方向改为 Write(0)。

#### 数据大小

- bit=0: BYTE (8位)
- bit=1 且 (address_raw & 3)==1: WORD (32位)，地址强制 `(address & ~3) | 1`
- bit=1 且 (address_raw & 1)==0: HALF_WORD (16位)，地址强制 `address & ~1`
- 其他: UNKNOWN → 回到 IDLE

### 2.3 C 实现计划

#### 文件名
`libsigrokdecode/c_decoders/fsi_c.c`

#### 注解枚举

```c
enum {
    ANN_WARNINGS = 0,
    ANN_START,
    ANN_CYCLE_TYPE,
    ANN_DIRECTION,
    ANN_ADDR,
    ANN_DATA,
    ANN_COMMANDS,
    ANN_CRC,
    ANN_TAR,
    NUM_ANN,
};
```

#### 状态枚举

```c
enum fsi_state {
    STATE_IDLE,
    STATE_TX_SLAVE_ID,
    STATE_COMMAND,
    STATE_DIRECTION,
    STATE_REL_ADDRESS_SIGN,
    STATE_ADDRESS,
    STATE_DATA_SIZE,
    STATE_TX_DATA,
    STATE_CRC,
    STATE_TAR,
    STATE_RX_SLAVE_ID,
    STATE_RESPONSE,
    STATE_RX_DATA,
    STATE_RX_IPOLL_INTERRUPT_FIELD,
    STATE_RX_IPOLL_DMA_CONTROL_FIELD,
    STATE_BREAK_TAR_QUEUED,
    STATE_BREAK_TAR,
};
```

#### 私有数据结构

```c
struct fsi_priv {
    int state;
    int tar_cycles;  // 固定为3

    // BREAK 检测
    uint64_t break_start_sample_number;
    int break_counter;
    int fsi_data_break_prev;

    // 采样追踪
    uint64_t samplenum_prev;
    int fsi_data_prev;

    // CRC
    int crc_internal;
    int crc_calculating;
    int computed_crc_tx_end;

    // 响应追踪
    int response_received;
    int valid_response;
    int busy_seq_count;

    // 地址追踪
    uint64_t prev_address[4];  // slave ID 0-3
    int prev_address_valid[4];

    // 当前事务
    int tx_slave_id;
    int rx_slave_id;
    int data_count;
    int command_count;
    int command_code;
    const char *command;
    int valid_command;
    int direction;
    int relative_address_negative;
    uint64_t address;
    uint64_t address_raw;
    int address_length;
    int address_count;
    const char *data_size;
    int data_length;
    uint64_t data;
    int crc;
    int crc_count;

    // 响应
    int response_count;
    int response_code;
    const char *response;

    // TAR
    int tar_timer;
    int timeout_counter;

    // 注解范围
    uint64_t ss_block;
    uint64_t es_block;

    int out_ann;
};
```

#### 函数签名

```c
static void fsi_reset(struct srd_decoder_inst *di);
static void fsi_start(struct srd_decoder_inst *di);
static void fsi_decode(struct srd_decoder_inst *di);
static void fsi_destroy(struct srd_decoder_inst *di);
```

### 2.4 关键实现注意事项

1. **数据反相**：`fsi_data = !data`，这是物理层特性，C 中用 `int fsi_data = !data_pin;`。

2. **fsi_data_prev 延迟**：所有状态机逻辑使用 `fsi_data_prev`（上一个采样点的数据），不是当前数据。这是因为在时钟边沿采样时，数据在边沿之前已经稳定。C 中必须在每次循环末尾保存 `fsi_data_prev = fsi_data`。

3. **wait 条件**：Python 中 `self.wait({1: 'e'})` 等待时钟任意边沿。C 中用 `c_cond_edge(cb, 1)`。

4. **matched 位掩码**：Python 中只有一个条件，所以 `matched` 总是匹配条件0。C 中也一样。

5. **putb 辅助函数**：Python 中 `putb` 使用 `ss_block`/`es_block` 作为范围。C 中直接用 `C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, cls, text)`。

6. **字符串命令/响应**：Python 用字符串变量如 `self.command = 'ABS_ADR'`，C 中用 `const char*` 指针或枚举+查找表。建议用枚举值+格式化字符串输出。

7. **prev_address 字典**：Python 用 dict 按 slave_id 索引，C 中用 `uint64_t prev_address[4]` 和 `int prev_address_valid[4]` 数组。

8. **BREAK 检测仅在上升沿**：Python 中 `if (fsi_clk):` 才处理 break 逻辑，C 中需要先获取时钟值。

9. **CRC LFSR 在每次循环末尾执行**：无论当前状态如何，只要 `crc_calculating` 为真就更新 CRC。C 中必须在主循环末尾复制此逻辑。

10. **复杂度最高**：这是5个解码器中最复杂的，有17个状态、主从边沿切换、CRC 计算、地址追踪。建议最后实现。

11. **TAR 状态中的条件分支**：TAR 状态中有复杂的逻辑——处理已收到的响应、检测新 START、超时检测。需要仔细按 Python 逻辑顺序实现。

12. **TERM 命令的特殊检测**：在 DATA_SIZE 状态中，特定条件下将命令改为 TERM。这是 SAME_ADR 和 TERM 命令冲突的解决方案。

### 2.5 Python→C 差异处理

| Python 特性 | C 处理方式 |
|------------|-----------|
| `self.wait({1: 'e'})` | `c_cond_edge(cb, 1); c_cond_wait(...)` |
| `not data` | `!data_pin` |
| `self.fsi_data_prev` | `s->fsi_data_prev`，循环末尾更新 |
| `self.putb([cls, [text]])` | `C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, cls, text)` |
| `self.command = 'ABS_ADR'` | 枚举值 + 格式化输出 |
| `self.prev_address = {}` | `uint64_t prev_address[4]` 固定数组 |
| `%` 格式化 | `snprintf(buf, size, format, args)` |
| `continue` | 在 C 的 while 循环中同样用 `continue` |

---

## 3. gpib

### 3.1 Python 解码器元数据

| 字段 | 值 |
|------|-----|
| id | `gpib` |
| name | `GPIB` |
| longname | `General Purpose Interface Bus` |
| desc | `IEEE-488 General Purpose Interface Bus (GPIB / HPIB).` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['PC']` |

**channels (16个):**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | dio1 | DIO1 | Data I/O bit 1 | dec_gpib_chan_dio1 |
| 1 | dio2 | DIO2 | Data I/O bit 2 | dec_gpib_chan_dio2 |
| 2 | dio3 | DIO3 | Data I/O bit 3 | dec_gpib_chan_dio3 |
| 3 | dio4 | DIO4 | Data I/O bit 4 | dec_gpib_chan_dio4 |
| 4 | dio5 | DIO5 | Data I/O bit 5 | dec_gpib_chan_dio5 |
| 5 | dio6 | DIO6 | Data I/O bit 6 | dec_gpib_chan_dio6 |
| 6 | dio7 | DIO7 | Data I/O bit 7 | dec_gpib_chan_dio7 |
| 7 | dio8 | DIO8 | Data I/O bit 8 | dec_gpib_chan_dio8 |
| 8 | eoi | EOI | End or identify | dec_gpib_chan_eoi |
| 9 | dav | DAV | Data valid | dec_gpib_chan_dav |
| 10 | nrfd | NRFD | Not ready for data | dec_gpib_chan_nrfd |
| 11 | ndac | NDAC | Not data accepted | dec_gpib_chan_ndac |
| 12 | ifc | IFC | Interface clear | dec_gpib_chan_ifc |
| 13 | srq | SRQ | Service request | dec_gpib_chan_srq |
| 14 | atn | ATN | Attention | dec_gpib_chan_atn |
| 15 | ren | REN | Remote enable | dec_gpib_chan_ren |

**optional_channels:** 无

**options (1个):**

| id | desc | default | values | idn |
|-----|------|---------|--------|-----|
| sample_total | Total number of samples | 0 | - | dec_gpib_opt_sample_total |

**annotations (3个):**

| 索引 | id | desc |
|------|-----|------|
| 0 | items | Items |
| 1 | gpib | DAT/CMD |
| 2 | eoi | EOI |

**annotation_rows (3个):**

| id | label | 包含的annotation class索引 |
|-----|-------|--------------------------|
| bytes | Bytes | (0,) |
| gpib | DAT/CMD | (1,) |
| eoi | EOI | (2,) |

**binary:** 无

**是否使用 samplerate:** 否

**是否输出到其他解码器:** 否

### 3.2 Python 解码逻辑完整分析

#### 核心机制

GPIB 使用16条信号线（8条数据 + 8条控制）。数据在 DAV 下降沿采样。所有信号都是低电平有效。

#### handle_bits() — 数据处理

1. 从8条 DIO 线读取数据字节：`item |= datapins[i] << i`（LSB first）
2. 反转数据字节：`item = item ^ 0xff`（因为 GPIB 低电平有效）
3. ATN 检测：`datapins[14] == 0`（ATN 低=命令模式）
4. EOI 检测：`datapins[8] == 0`（EOI 低=最后一个字节）

#### 延迟一拍输出

Python 实现中，第一个数据项不立即输出，而是保存到 `saved_item`/`saved_ATN`/`saved_EOI`。从第二个数据项开始，输出上一个保存的项，然后保存当前项。这确保了每个数据项有正确的结束采样号（下一个 DAV 下降沿的位置）。

#### GPIB 命令解码（ATN=1 时）

| 字节值 | 命令 |
|--------|------|
| 0x01 | GTL |
| 0x04 | SDC |
| 0x05 | PPC |
| 0x08 | GET |
| 0x09 | TCT |
| 0x11 | LLO |
| 0x14 | DCL |
| 0x15 | PPU |
| 0x18 | SPE |
| 0x19 | SPD |
| 0x3f | UNL |
| 0x5f | UNT |
| 0x20-0x3e | Address Listener: 'L' + chr(dbyte + 0x10) |
| 0x40-0x5e | Address Talker: 'T' + chr(dbyte - 0x10) |

#### 数据模式（ATN=0 时）

| 字节值 | 显示 |
|--------|------|
| 0x20-0x7e | chr(dbyte)（可打印ASCII） |
| 0x0a | LF |
| 0x0d | CR |

#### decode() 主循环

```python
waitcond = [{9: 'l'}]  # DAV 低电平
if lsn:  # 如果有 sample_total 选项
    waitcond.append({'skip': lsn})

while True:
    if lsn:
        waitcond[1]['skip'] = lsn - self.samplenum - 1
    (d1..d8, eoi, dav, nrfd, ndac, ifc, srq, atn, ren) = self.wait(waitcond)
    pins = (d1..d8, eoi, dav, nrfd, ndac, ifc, srq, atn, ren)
    handle_bits(pins)
    waitcond[0][9] = 'f'  # 后续等待 DAV 下降沿
```

**关键点**：第一次等待 DAV 低电平（可能已经是低的），之后等待 DAV 下降沿。

#### itemcount 限制

`handle_bits()` 中 `if self.itemcount < 16: return`，但每次调用 itemcount 加1，所以最多16次调用后重置。实际上由于 DAV 每次脉冲只调用一次，itemcount 通常不会达到16。

### 3.3 C 实现计划

#### 文件名
`libsigrokdecode/c_decoders/gpib_c.c`

#### 注解枚举

```c
enum {
    ANN_ITEMS = 0,
    ANN_GPIB,
    ANN_EOI,
    NUM_ANN,
};
```

#### 私有数据结构

```c
struct gpib_priv {
    int items[16];       // 缓冲的数据字节
    int itemcount;       // 当前计数
    int saved_item;      // 延迟输出的数据
    int saved_ATN;       // 延迟输出的 ATN 标志
    int saved_EOI;       // 延迟输出的 EOI 标志
    uint64_t ss_item;    // 当前项起始采样
    uint64_t es_item;    // 当前项结束采样
    int first;           // 是否是第一个数据项
    int64_t sample_total; // 选项值
    int out_ann;
};
```

#### 函数签名

```c
static void gpib_reset(struct srd_decoder_inst *di);
static void gpib_start(struct srd_decoder_inst *di);
static void gpib_decode(struct srd_decoder_inst *di);
static void gpib_destroy(struct srd_decoder_inst *di);
static void gpib_handle_bits(struct srd_decoder_inst *di, const uint8_t *pins, uint64_t samplenum);
```

#### 通道定义

```c
static struct srd_channel gpib_channels[] = {
    {"dio1", "DIO1", "Data I/O bit 1", 0, SRD_CHANNEL_SDATA, NULL},
    {"dio2", "DIO2", "Data I/O bit 2", 1, SRD_CHANNEL_SDATA, NULL},
    {"dio3", "DIO3", "Data I/O bit 3", 2, SRD_CHANNEL_SDATA, NULL},
    {"dio4", "DIO4", "Data I/O bit 4", 3, SRD_CHANNEL_SDATA, NULL},
    {"dio5", "DIO5", "Data I/O bit 5", 4, SRD_CHANNEL_SDATA, NULL},
    {"dio6", "DIO6", "Data I/O bit 6", 5, SRD_CHANNEL_SDATA, NULL},
    {"dio7", "DIO7", "Data I/O bit 7", 6, SRD_CHANNEL_SDATA, NULL},
    {"dio8", "DIO8", "Data I/O bit 8", 7, SRD_CHANNEL_SDATA, NULL},
    {"eoi", "EOI", "End or identify", 8, SRD_CHANNEL_SDATA, NULL},
    {"dav", "DAV", "Data valid", 9, SRD_CHANNEL_SDATA, NULL},
    {"nrfd", "NRFD", "Not ready for data", 10, SRD_CHANNEL_SDATA, NULL},
    {"ndac", "NDAC", "Not data accepted", 11, SRD_CHANNEL_SDATA, NULL},
    {"ifc", "IFC", "Interface clear", 12, SRD_CHANNEL_SDATA, NULL},
    {"srq", "SRQ", "Service request", 13, SRD_CHANNEL_SDATA, NULL},
    {"atn", "ATN", "Attention", 14, SRD_CHANNEL_SDATA, NULL},
    {"ren", "REN", "Remote enable", 15, SRD_CHANNEL_SDATA, NULL},
};
```

### 3.4 关键实现注意事项

1. **16通道解码器**：这是5个解码器中通道数最多的。所有16个通道都是必需的。

2. **wait 条件**：第一次 `{9: 'l'}`（DAV 低电平），之后 `{9: 'f'}`（DAV 下降沿）。可选地加上 `{'skip': N}` 条件。

3. **skip 条件动态更新**：Python 中 `waitcond[1]['skip'] = lsn - self.samplenum - 1`，C 中需要在每次循环重建条件构建器。

4. **get_pin 读取16个通道**：wait 返回后需要读取所有16个通道的值。C 中用 `c_decoder_get_pin(di, ch, samplenum)` 逐个读取。

5. **数据反转**：`item ^ 0xff`，GPIB 信号低电平有效。

6. **延迟输出模式**：第一个字节只保存不输出，从第二个字节开始输出上一个字节。C 中用 `saved_item`/`saved_ATN`/`saved_EOI` 变量实现。

7. **GPIB 命令解码表**：建议用 switch-case 或查找表实现，比 Python 的连续 if 更高效。

8. **Listener/Talker 地址格式化**：`'L' + chr(dbyte + 0x10)` 和 `'T' + chr(dbyte - 0x10)`，C 中用 `snprintf(buf, size, "L%c", (char)(dbyte + 0x10))`。

9. **sample_total 选项**：如果为0则不添加 skip 条件，非0则添加。C 中从 `c_decoder_get_option_int` 获取。

10. **itemcount 重置**：当 `itemcount >= 16` 时重置为0并清空 items 数组。

### 3.5 Python→C 差异处理

| Python 特性 | C 处理方式 |
|------------|-----------|
| `self.wait([{9: 'l'}, {'skip': N}])` | 两个条件用 `c_cond_or` 连接 |
| `(d1,..,d16) = self.wait(...)` | `c_decoder_get_pin(di, ch, samplenum)` 逐个读取 |
| `self.matched` | `matched` 位掩码 |
| `datapins[i]` | `c_decoder_get_pin(di, i, samplenum)` |
| `chr(value)` | `(char)value` |
| `'%02X' % value` | `snprintf(buf, size, "%02X", value)` |

---

## 4. guess_bitrate

### 4.1 Python 解码器元数据

| 字段 | 值 |
|------|-----|
| id | `guess_bitrate` |
| name | `Guess bitrate` |
| longname | `Guess bitrate/baudrate` |
| desc | `Guess the bitrate/baudrate of a UART (or other) protocol.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Clock/timing', 'Util']` |

**channels (1个):**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | data | Data | Data line | dec_guess_bitrate_chan_data |

**optional_channels:** 无

**options:** 无

**annotations (1个):**

| 索引 | id | desc |
|------|-----|------|
| 0 | bitrate | Bitrate / baudrate |

**annotation_rows:** 无（只有一个注解类，不需要行分组）

**binary:** 无

**是否使用 samplerate:** 是 — `metadata()` 回调中获取，用于计算比特率

**是否输出到其他解码器:** 否

### 4.2 Python 解码逻辑完整分析

#### 算法

1. 检查 samplerate 是否可用，不可用则抛出异常
2. 等待数据线第一个边沿 `{0: 'e'}`，记录 `ss_edge`
3. 进入主循环：
   - 等待数据线下一个边沿 `{0: 'e'}`
   - 计算边沿间距 `b = samplenum - ss_edge`
   - 如果 `bitwidth` 为 None 或 `b < bitwidth`：
     - 更新 `bitwidth = b`
     - 计算 `bitrate = int(float(samplerate) / float(b))`
     - 输出注解 `[0, ['%d' % bitrate]]`，范围为 `ss_edge` 到 `samplenum`
   - 更新 `ss_edge = samplenum`

#### 核心思想

通过找到任意两个连续边沿之间的最小距离，假设它对应一个比特时间，从而估算比特率。捕获越长，估算越准确。

### 4.3 C 实现计划

#### 文件名
`libsigrokdecode/c_decoders/guess_bitrate_c.c`

#### 注解枚举

```c
enum {
    ANN_BITRATE = 0,
    NUM_ANN,
};
```

#### 私有数据结构

```c
struct guess_bitrate_priv {
    uint64_t samplerate;
    uint64_t ss_edge;
    uint64_t bitwidth;    // 0 表示未设置
    int out_ann;
};
```

#### 函数签名

```c
static void guess_bitrate_reset(struct srd_decoder_inst *di);
static void guess_bitrate_start(struct srd_decoder_inst *di);
static void guess_bitrate_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void guess_bitrate_decode(struct srd_decoder_inst *di);
static void guess_bitrate_destroy(struct srd_decoder_inst *di);
```

#### 通道定义

```c
static struct srd_channel guess_bitrate_channels[] = {
    {"data", "Data", "Data line", 0, SRD_CHANNEL_SDATA, NULL},
};
```

#### 注解标签

```c
static const char *guess_bitrate_ann_labels[][3] = {
    {"", "bitrate", "Bitrate / baudrate"},
};
```

#### 注解行

```c
// 只有一个注解类，Python 中没有定义 annotation_rows
// C 中可以定义一个简单的行
static const int guess_bitrate_row_classes[] = {ANN_BITRATE};
static const struct srd_c_ann_row guess_bitrate_ann_rows[] = {
    {"bitrate", "Bitrate", guess_bitrate_row_classes, 1},
};
```

### 4.4 关键实现注意事项

1. **samplerate 必需**：如果 samplerate 为 0，decode 函数应直接返回（Python 抛出异常，C 中静默返回）。

2. **bitwidth 初始值**：Python 用 `None` 表示未设置，C 中用 `0` 表示（因为两个边沿间距不可能为0）。

3. **整数比特率**：Python 用 `int(float(samplerate) / float(b))`，C 中用 `(uint64_t)((double)samplerate / (double)b)` 或直接整数除法 `samplerate / b`。Python 用 float 转换是为了避免整数溢出，C 中 `uint64_t` 除法不会溢出。

4. **注解范围**：从 `ss_edge` 到当前 `samplenum`，表示检测到更短间距的区间。

5. **格式化**：Python `'%d' % bitrate`，C 中 `snprintf(buf, size, "%llu", (unsigned long long)bitrate)`。

6. **最简单的解码器**：这是5个中最简单的，只有1个通道、1个注解、无状态机。

### 4.5 Python→C 差异处理

| Python 特性 | C 处理方式 |
|------------|-----------|
| `raise SamplerateError(...)` | 直接 `return;`（C 中无法抛异常） |
| `self.wait({0: 'e'})` | `c_cond_edge(cb, 0); c_cond_wait(...)` |
| `bitwidth = None` | `s->bitwidth = 0`（0表示未设置） |
| `int(float(a) / float(b))` | `samplerate / b`（uint64_t整数除法） |
| `'%d' % bitrate` | `snprintf(buf, size, "%llu", bitrate)` |

---

## 5. iec

### 5.1 Python 解码器元数据

| 字段 | 值 |
|------|-----|
| id | `iec` |
| name | `IEC` |
| longname | `Commodore IEC bus` |
| desc | `Commodore serial IEEE-488 (IEC) bus protocol.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['PC', 'Retro computing']` |

**channels (3个):**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | data | DATA | Data I/O | dec_iec_chan_data |
| 1 | clk | CLK | Clock | dec_iec_chan_clk |
| 2 | atn | ATN | Attention | dec_iec_chan_atn |

**optional_channels (1个):**

| 序号 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | srq | SRQ | Service request | dec_iec_opt_chan_srq |

**options:** 无

**annotations (3个):**

| 索引 | id | desc |
|------|-----|------|
| 0 | items | Items |
| 1 | gpib | DAT/CMD |
| 2 | eoi | EOI |

**annotation_rows (3个):**

| id | label | 包含的annotation class索引 |
|-----|-------|--------------------------|
| bytes | Bytes | (0,) |
| gpib | DAT/CMD | (1,) |
| eoi | EOI | (2,) |

**binary:** 无

**是否使用 samplerate:** 否

**是否输出到其他解码器:** 否

### 5.2 Python 解码逻辑完整分析

#### step_wait_conds — 步进等待条件

```python
step_wait_conds = (
    [{2: 'f'}, {0: 'l', 1: 'h'}],          # step 0
    [{2: 'f'}, {0: 'h', 1: 'h'}, {1: 'l'}], # step 1
    [{2: 'f'}, {0: 'f'}, {1: 'l'}],          # step 2
    [{2: 'f'}, {1: 'e'}],                     # step 3
)
```

每个 step 的条件是 OR 关系（多个条件列表中任一匹配即可）。每个条件列表内部是 AND 关系。

**step 0 等待：**
- ATN 下降沿，或
- DATA 低 AND CLK 高

**step 1 等待：**
- ATN 下降沿，或
- DATA 高 AND CLK 高，或
- CLK 低

**step 2 等待：**
- ATN 下降沿，或
- DATA 下降沿，或
- CLK 低

**step 3 等待：**
- ATN 下降沿，或
- CLK 任意边沿

#### 状态机（4步 + ATN 重置）

**step 0 — 等待传输开始**
- 如果 ATN 下降沿匹配：重置 step=0
- 如果 DATA 低 AND CLK 高：准备发送，step=1

**step 1 — 等待数据准备**
- 如果 DATA 高 AND CLK 高：开始接收数据
  - 保存 `ss_item = samplenum`
  - `saved_ATN = !atn`（ATN 低=命令模式，取反）
  - `saved_EOI = False`
  - `bits = 0`, `numbits = 0`
  - step=2
- 如果 CLK 低：传输中止，step=0

**step 2 — EOI 检测**
- 如果 DATA 低 AND CLK 高：EOI 确认，`saved_EOI = True`
- 如果 CLK 低：开始位传输，step=3

**step 3 — 位传输**
- 如果 CLK 上升沿：锁存 DATA 位 `bits |= data << numbits`
- 如果 CLK 下降沿：位结束 `numbits++`
  - 如果 `numbits == 8`：调用 `handle_bits()`，step=0

**ATN 下降沿始终重置 step=0**：在任何步骤中，如果 ATN 下降沿匹配，立即重置。

#### handle_bits() — 字节处理

与 GPIB 类似但更简单：

1. 输出十六进制：`[0, ['%02X' % dbyte]]`
2. 解码命令/数据：
   - ATN 模式（与 GPIB 相同的命令集，加上 IEC 特有命令）：
     - 0x01: GTL, 0x04: SDC, 0x05: PPC, 0x08: GET, 0x09: TCT
     - 0x11: LLO, 0x14: DCL, 0x15: PPU, 0x18: SPE, 0x19: SPD
     - 0x3f: UNL, 0x5f: UNT
     - 0x20-0x3e: Listener 地址 `'L' + chr(dbyte + 0x10)`
     - 0x40-0x5e: Talker 地址 `'T' + chr(dbyte - 0x10)`
     - **IEC 特有（GPIB 没有）：**
     - 0x60-0x6f: Channel reopen `'R' + chr(dbyte - 0x30)`
     - 0xdf-0xef: Channel close `'C' + chr(dbyte - 0xb0)`
     - 0xf0-0xff: Channel open `'O' + chr(dbyte - 0xc0)`
   - 数据模式：
     - 0x20-0x7e: `chr(dbyte)`
     - 0x0a: LF
     - 0x0d: CR
3. EOI：如果 `saved_EOI` 为真，输出 `'EOI'`，否则输出 `' '`

#### matched 位掩码

Python 中 `self.matched & (0b1 << 0)` 检查条件0是否匹配。在 step_wait_conds 中，每个条件列表有一个索引：
- step 0: 条件0=ATN fall, 条件1=DATA low + CLK high
- step 1: 条件0=ATN fall, 条件1=DATA high + CLK high, 条件2=CLK low
- step 2: 条件0=ATN fall, 条件1=DATA fall, 条件2=CLK low
- step 3: 条件0=ATN fall, 条件1=CLK edge

### 5.3 C 实现计划

#### 文件名
`libsigrokdecode/c_decoders/iec_c.c`

#### 注解枚举

```c
enum {
    ANN_ITEMS = 0,
    ANN_GPIB,
    ANN_EOI,
    NUM_ANN,
};
```

#### 私有数据结构

```c
struct iec_priv {
    int step;
    int saved_ATN;
    int saved_EOI;
    uint64_t ss_item;
    uint64_t es_item;
    uint8_t bits;
    int numbits;
    int out_ann;
};
```

#### 函数签名

```c
static void iec_reset(struct srd_decoder_inst *di);
static void iec_start(struct srd_decoder_inst *di);
static void iec_decode(struct srd_decoder_inst *di);
static void iec_destroy(struct srd_decoder_inst *di);
static void iec_handle_bits(struct srd_decoder_inst *di, uint64_t samplenum);
```

#### 通道定义

```c
static struct srd_channel iec_channels[] = {
    {"data", "DATA", "Data I/O", 0, SRD_CHANNEL_SDATA, NULL},
    {"clk", "CLK", "Clock", 1, SRD_CHANNEL_SCLK, NULL},
    {"atn", "ATN", "Attention", 2, SRD_CHANNEL_SDATA, NULL},
};

static struct srd_channel iec_optional_channels[] = {
    {"srq", "SRQ", "Service request", 0, SRD_CHANNEL_SDATA, NULL},
};
```

### 5.4 关键实现注意事项

1. **多条件 OR 等待**：这是最复杂的部分。Python 的 `step_wait_conds` 是一个列表的列表，外层列表是 OR 关系，内层列表是 AND 关系。C 中需要用 `c_cond_or()` 连接多个条件组。

   例如 step 0：
   ```c
   srd_cond_builder *cb = c_cond_new();
   c_cond_fall(cb, 2);           // ATN fall
   c_cond_or(cb);
   c_cond_low(cb, 0);            // DATA low
   c_cond_high(cb, 1);           // CLK high
   ```

2. **matched 位掩码解析**：每个 OR 分支对应 matched 的一个位。需要根据 matched 值判断哪个条件匹配了。

3. **ATN 下降沿优先**：在任何 step 中，如果 ATN 下降沿匹配（`matched & 1`），立即重置 step=0，不处理其他条件。

4. **CLK 上升/下降沿区分**：step 3 中，CLK 上升沿锁存数据，CLK 下降沿结束位。C 中需要检查当前 CLK 值：
   ```c
   int clk_val = c_decoder_get_pin(di, 1, samplenum);
   if (clk_val == 1) {
       // 上升沿：锁存数据
   } else {
       // 下降沿：位结束
   }
   ```

5. **saved_ATN 取反**：`saved_ATN = !atn`，因为 ATN 低电平有效。当 ATN=0 时，saved_ATN=1（命令模式）。

6. **IEC 特有命令**：相比 GPIB，IEC 多了 Channel reopen (0x60-0x6f)、Channel close (0xdf-0xef)、Channel open (0xf0-0xff)。

7. **SRQ 可选通道**：虽然定义了 optional_channel srq，但 decode 逻辑中从未使用它。C 中需要声明但不需要在解码逻辑中处理。

8. **bits 累积**：`bits |= data << numbits`，最多8位，用 `uint8_t` 即可。

### 5.5 Python→C 差异处理

| Python 特性 | C 处理方式 |
|------------|-----------|
| `self.wait(step_wait_conds[step])` | 根据当前 step 构建 `srd_cond_builder` |
| `self.matched & (0b1 << 0)` | `matched & 1` |
| `not atn` | `!atn_val` |
| `chr(value)` | `(char)value` |
| `'%02X' % value` | `snprintf(buf, size, "%02X", value)` |
| `self.samplenum` | `c_cond_wait` 返回的 `samplenum` |

---

## 6. 通用C解码器模式参考

### 6.1 标准文件结构

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

// 1. 注解枚举
enum { ANN_... = 0, ..., NUM_ANN };

// 2. 状态枚举（如有状态机）
enum xxx_state { STATE_..., ... };

// 3. 私有数据结构
struct xxx_priv { ... };

// 4. 通道定义
static struct srd_channel xxx_channels[] = { ... };

// 5. 可选通道定义（如有）
static struct srd_channel xxx_optional_channels[] = { ... };

// 6. 选项定义（如有）
static struct srd_decoder_option xxx_options[] = { ... };

// 7. 注解标签
static const char *xxx_ann_labels[][3] = { ... };

// 8. 注解行
static const int xxx_row_xxx_classes[] = { ... };
static const struct srd_c_ann_row xxx_ann_rows[] = { ... };

// 9. 输入/输出/标签
static const char *xxx_inputs[] = { "logic", NULL };
static const char *xxx_outputs[] = { ..., NULL }; // 或 { NULL }
static const char *xxx_tags[] = { ..., NULL };

// 10. 回调函数
static void xxx_reset(struct srd_decoder_inst *di) { ... }
static void xxx_start(struct srd_decoder_inst *di) { ... }
static void xxx_metadata(struct srd_decoder_inst *di, int key, uint64_t value) { ... } // 如需要
static void xxx_decode(struct srd_decoder_inst *di) { ... }
static void xxx_destroy(struct srd_decoder_inst *di) { ... }

// 11. 解码器结构体
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    .longname = "... (C)",
    .desc = "... (C implementation)",
    .license = "gplv2+",
    .channels = xxx_channels,
    .num_channels = N,
    .optional_channels = xxx_optional_channels, // 或 NULL
    .num_optional_channels = N, // 或 0
    .options = xxx_options, // 或 NULL
    .num_options = N, // 或 0
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = N,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,
    .num_inputs = 1,
    .outputs = xxx_outputs,
    .num_outputs = N, // 0 如果无输出
    .binary = NULL,
    .num_binary = 0,
    .tags = xxx_tags,
    .num_tags = N,
    .metadata = xxx_metadata, // 或 NULL
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,
    .destroy = xxx_destroy,
};

// 12. 入口函数
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    // 初始化选项默认值
    return &xxx_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
```

### 6.2 命名规范

- C 解码器 id：`<python_id>_c`（如 `eth_an_c`, `fsi_c`, `gpib_c`, `guess_bitrate_c`, `iec_c`）
- C 解码器 name：`<Python Name>(C)`（如 `ETH_AN(C)`, `FSI(C)`, `GPIB(C)`, `Guess bitrate(C)`, `IEC(C)`）
- 私有结构体：`<decoder>_priv`
- 函数前缀：`<decoder>_`
- 通道 idn：保持与 Python 相同

### 6.3 CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加：
```
eth_an_c
fsi_c
gpib_c
guess_bitrate_c
iec_c
```

### 6.4 条件构建器使用模式

```c
// 简单边沿等待
srd_cond_builder *cb = c_cond_new();
c_cond_rise(cb, 0);  // 通道0上升沿
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);

// OR 条件
srd_cond_builder *cb = c_cond_new();
c_cond_fall(cb, 2);   // 条件0: ATN下降沿
c_cond_or(cb);
c_cond_low(cb, 0);    // 条件1: DATA低
c_cond_high(cb, 1);   //        AND CLK高
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
// matched & 1: 条件0匹配, matched & 2: 条件1匹配

// Skip 条件
srd_cond_builder *cb = c_cond_new();
c_cond_fall(cb, 9);   // DAV下降沿
c_cond_or(cb);
c_cond_skip(cb, N);   // 跳过N个采样
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);

// 无条件等待（等效 Python self.wait({})）
// c_cond_wait_current() 获取当前采样位置，不前进采样指针
uint64_t cur_sample;
int ret = c_cond_wait_current(di, &cur_sample);
// 用于在 recv_proto() 回调中获取当前采样位置，或在 decode() 开头获取初始引脚状态
``` <!-- Updated: 添加c_cond_wait_current()使用模式 -->

### 6.5 注解输出模式

```c
// 简单文本
C_ANN_PUT(di, ss, es, out_ann, ANN_CLASS, "text");

// 多级文本（长/中/短）
C_ANN_PUT(di, ss, es, out_ann, ANN_CLASS, "long text", "mid text", "short");

// 带数值
char buf[64];
snprintf(buf, sizeof(buf), "Value: 0x%x", val);
C_ANN_PUT(di, ss, es, out_ann, ANN_CLASS, buf);

// 带数值和十六进制
C_ANN_PUT_VAL(di, ss, es, out_ann, ANN_CLASS, val, "description");
```

### 6.6 实现优先级建议

1. **guess_bitrate** — 最简单，1通道1注解无状态机
2. **iec** — 中等复杂度，3通道4步状态机
3. **eth_an** — 中等复杂度，1通道但有时序计算
4. **gpib** — 较复杂，16通道延迟输出
5. **fsi** — 最复杂，17个状态+CRC+主从切换

### 6.7 通用注意事项

1. 所有 `c_cond_wait` 返回值必须检查，`ret != SRD_OK` 时立即 `return`。
2. `c_cond_builder` 使用后必须 `c_cond_free` 释放。
3. 私有数据在 `reset` 中用 `g_malloc0` 分配，在 `destroy` 中用 `g_free` 释放。
4. `samplerate` 通过 `c_decoder_get_samplerate(di)` 在 `start` 中获取，也可通过 `metadata` 回调获取。建议两者都实现。
5. 通道类型：时钟线用 `SRD_CHANNEL_SCLK`，数据线用 `SRD_CHANNEL_SDATA`，模拟/特殊用 `SRD_CHANNEL_ADATA`。
6. 注解标签是 `[N][3]` 的字符串数组，每行3个字符串（空/短id/长描述）。
7. **`c_cond_wait_current(di, &samplenum)`**：等效于 Python 的 `self.wait({})`，获取当前采样位置但不前进采样指针。常用于 `decode()` 开头获取初始引脚状态（配合 `c_decoder_get_pin()`），或在 `recv_proto()` 回调中获取当前采样位置。 <!-- Updated: 添加c_cond_wait_current()说明 -->
8. **`c_decoder_get_initial_pin(di, ch)`**：获取通道 `ch` 在数据开始前的初始引脚值（读取 `di->old_pins_array`）。等效于 Python 中在第一次 `self.wait()` 之前读取 `self.oldpin` 或初始引脚状态。 <!-- Updated: 添加c_decoder_get_initial_pin()说明 -->
9. **C解码器依赖规则**：C解码器的 `inputs` 只能引用已有C实现的底层解码器（如 `spi`、`i2c`、`uart`、`can`、`jtag` 等），不得依赖仅有Python实现的解码器。若底层仅有Python实现，该C解码器标记为"阻塞"。 <!-- Updated: 添加C解码器依赖规则 -->
