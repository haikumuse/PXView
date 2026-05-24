# Python解码器移植为C解码器 — 第三批规格说明书

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |

## 概述

本文档定义了将5个Python协议解码器移植为C解码器的详细规格。目标文件路径为 `libsigrokdecode/c_decoders/`，每个解码器生成一个独立的C源文件和对应的DLL。

### 移植清单

| # | Python解码器 | C文件名 | 复杂度 | 需要samplerate | 有binary输出 | 有python输出 |
|---|-------------|---------|--------|---------------|-------------|-------------|
| 1 | ac97 | ac97_c.c | ★★★★★ | 是 | 是 | 否 |
| 2 | sdcard_sd | sdcard_sd_c.c | ★★★★★ | 否 | 否 | 否 |
| 3 | emmc_sd | emmc_sd_c.c | ★★★★☆ | 否 | 否 | 否 |
| 4 | swim | swim_c.c | ★★★★☆ | 是 | 是 | 否 |
| 5 | rvswd | rvswd_c.c | ★★☆☆☆ | 否 | 否 | 是 |

### C解码器API参考

C解码器必须遵循 `libsigrokdecode/libsigrokdecode.h` 中定义的 `srd_c_decoder` 结构体：

```c
struct srd_c_decoder {
    const char *id;              // 解码器ID，必须以 "_c" 结尾
    const char *name;            // 短名称
    const char *longname;        // 长名称
    const char *desc;            // 描述
    const char *license;         // 许可证
    const struct srd_channel *channels;        // 必需通道
    int num_channels;
    const struct srd_channel *optional_channels; // 可选通道
    int num_optional_channels;
    const struct srd_decoder_option *options;  // 选项
    int num_options;
    int num_annotations;         // 注解数量
    const char *(*ann_labels)[3]; // 注解标签（每个最多3个文本）
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
                       const unsigned char *data, uint64_t data_len);  // <!-- Updated: 补全recv_proto完整签名 -->
};
```

### 关键API函数

```c
// 条件等待
srd_cond_builder *c_cond_new(void);
srd_cond_builder *c_cond_rise(srd_cond_builder *b, int ch);
srd_cond_builder *c_cond_fall(srd_cond_builder *b, int ch);
srd_cond_builder *c_cond_high(srd_cond_builder *b, int ch);
srd_cond_builder *c_cond_low(srd_cond_builder *b, int ch);
srd_cond_builder *c_cond_edge(srd_cond_builder *b, int ch);
srd_cond_builder *c_cond_noedge(srd_cond_builder *b, int ch);  // <!-- Updated: 已实现，无变化时匹配 -->
srd_cond_builder *c_cond_skip(srd_cond_builder *b, uint64_t count);
srd_cond_builder *c_cond_or(srd_cond_builder *b);  // <!-- Updated: 已实现，OR条件分隔符 -->
int c_cond_wait(srd_cond_builder *b, struct srd_decoder_inst *di,
    uint64_t *samplenum, uint64_t *matched);
int c_cond_wait_current(struct srd_decoder_inst *di, uint64_t *samplenum);  // <!-- Updated: 已实现，等效Python self.wait({}) -->
void c_cond_free(srd_cond_builder *b);

// 引脚读取
uint8_t c_decoder_get_pin(struct srd_decoder_inst *di, int ch, uint64_t samplenum);
uint8_t c_decoder_get_initial_pin(struct srd_decoder_inst *di, int ch);  // <!-- Updated: 已实现，读取di->old_pins_array，等效Python self.initial_pins -->
int c_decoder_has_channel(struct srd_decoder_inst *di, int ch);

// 输出注册
int c_decoder_register_output(struct srd_decoder_inst *di, int output_type, const char *proto_id);
int c_decoder_register_output_meta(struct srd_decoder_inst *di, int output_type,
    const char *proto_id, const char *meta_type, const char *meta_name, const char *meta_descr);  // <!-- Updated: 已实现，注册META输出 -->

// 注解输出宏
C_ANN_PUT(di, ss, es, out_id, cls, ...)         // 基本注解
C_ANN_PUT_TYPE(di, ss, es, out_id, cls, tp, ...) // 带类型注解
C_ANN_PUT_VAL(di, ss, es, out_id, cls, val, ...) // 带数值注解

// 二进制输出
int c_decoder_put_binary(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    int output_id, int bin_class, uint64_t size, const unsigned char *data);

// Logic输出（用于上层解码器堆叠）
int c_decoder_put_logic(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    int output_id, uint32_t channel_mask, const uint8_t *values, int num_channels);  // <!-- Updated: 已实现，用于SRD_OUTPUT_LOGIC输出 -->

// Python协议输出
int c_decoder_put_python(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    int output_id, const char *cmd, const unsigned char *data, uint64_t data_len);

// META输出
int c_decoder_put_meta_int(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample, int output_id, int64_t value);  // <!-- Updated: 已实现，输出int类型META -->
int c_decoder_put_meta_double(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample, int output_id, double value);  // <!-- Updated: 已实现，输出double类型META -->

// 元数据
uint64_t c_decoder_get_samplerate(struct srd_decoder_inst *di);
uint64_t c_decoder_get_last_samplenum(struct srd_decoder_inst *di);  // <!-- Updated: 已实现，获取最后一个采样号 -->

// 选项
int64_t c_decoder_get_option_int(struct srd_decoder_inst *di, const char *key, int64_t defval);
double c_decoder_get_option_double(struct srd_decoder_inst *di, const char *key, double defval);  // <!-- Updated: 已实现，读取double类型选项 -->
const char *c_decoder_get_option_string(struct srd_decoder_inst *di, const char *key, const char *defval);

// 私有数据
void *c_decoder_get_private(struct srd_decoder_inst *di);
void c_decoder_set_private(struct srd_decoder_inst *di, void *data);
```

---

## 1. AC97 解码器 (ac97_c.c)

### 1.1 Python解码器分析

#### 元数据
- **id**: `ac97`
- **name**: `AC '97`
- **longname**: `Audio Codec '97`
- **desc**: `Audio and modem control for PC systems.`
- **license**: `gplv2+`
- **inputs**: `['logic']`
- **outputs**: `[]`
- **tags**: `['Audio', 'PC']`

#### 通道定义

**必需通道 (2个)**:
| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | sync | SYNC | Frame synchronization | dec_ac97_chan_sync |
| 1 | clk | BIT_CLK | Data bits clock | dec_ac97_chan_clk |

**可选通道 (3个)**:
| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 2 | out | SDATA_OUT | Data output | dec_ac97_opt_chan_out |
| 3 | in | SDATA_IN | Data input | dec_ac97_opt_chan_in |
| 4 | rst | RESET# | Reset line | dec_ac97_opt_chan_rst |

#### 选项
无

#### 注解定义 (32个)
| 枚举值 | id | desc |
|--------|-----|------|
| 0 | bit-out | Output bits |
| 1 | bit-in | Input bits |
| 2 | slot-out-raw | Output raw value |
| 3 | slot-out-tag | Output TAG |
| 4 | slot-out-cmd-addr | Output command address |
| 5 | slot-out-cmd-data | Output command data |
| 6 | slot-out-03 | Output slot 3 |
| 7 | slot-out-04 | Output slot 4 |
| 8 | slot-out-05 | Output slot 5 |
| 9 | slot-out-06 | Output slot 6 |
| 10 | slot-out-07 | Output slot 7 |
| 11 | slot-out-08 | Output slot 8 |
| 12 | slot-out-09 | Output slot 9 |
| 13 | slot-out-10 | Output slot 10 |
| 14 | slot-out-11 | Output slot 11 |
| 15 | slot-out-io-ctrl | Output I/O control |
| 16 | slot-in-raw | Input raw value |
| 17 | slot-in-tag | Input TAG |
| 18 | slot-in-sts-addr | Input status address |
| 19 | slot-in-sts-data | Input status data |
| 20 | slot-in-03 | Input slot 3 |
| 21 | slot-in-04 | Input slot 4 |
| 22 | slot-in-05 | Input slot 5 |
| 23 | slot-in-06 | Input slot 6 |
| 24 | slot-in-07 | Input slot 7 |
| 25 | slot-in-08 | Input slot 8 |
| 26 | slot-in-09 | Input slot 9 |
| 27 | slot-in-10 | Input slot 10 |
| 28 | slot-in-11 | Input slot 11 |
| 29 | slot-in-io-sts | Input I/O status |
| 30 | warning | Warning |
| 31 | error | Error |

#### 注解行 (8行)
| id | label | 注解类列表 |
|----|-------|-----------|
| bits-out | Output bits | (0,) |
| slots-out-raw | Output numbers | (2,) |
| slots-out | Output slots | (3,4,5,6,7,8,9,10,11,12,13,14,15) |
| bits-in | Input bits | (1,) |
| slots-in-raw | Input numbers | (16,) |
| slots-in | Input slots | (17,18,19,20,21,22,23,24,25,26,27,28,29) |
| warnings | Warnings | (30,) |
| errors | Errors | (31,) |

#### 二进制输出 (4个)
| 枚举值 | id | desc |
|--------|-----|------|
| 0 | frame-out | Frame bits, output data |
| 1 | frame-in | Frame bits, input data |
| 2 | slot-raw-out | Raw slot bits, output data |
| 3 | slot-raw-in | Raw slot bits, input data |

### 1.2 decode()逻辑分析

#### 时序关系
- 数据在BIT_CLK的**下降沿**采样
- 注解需要跨越**上升沿**之间的时间段
- SYNC在帧开始前**一个周期**上升

#### 帧结构
- 每帧256位
- Slot 0: 16位 (TAG字段)
- Slot 1-12: 各20位 (命令/状态/音频数据)
- `frame_slot_lens = [0, 16, 36, 56, 76, 96, 116, 136, 156, 176, 196, 216, 236, 256]`

#### 状态机
1. **初始化**: 等待BIT_CLK的任意边沿
2. **同步**: 如果CLK起始为低，等待上升沿
3. **主循环**:
   - 等待BIT_CLK下降沿 → 采样数据
   - 等待BIT_CLK上升沿 → 检测SYNC帧起始
   - 检测SYNC从0→1的跳变（prev_sync[0]==0 && prev_sync[1]==1）→ 调用start_frame()
   - 调用handle_bits()处理每个位对

#### handle_bits()逻辑
- 输出每个位的注解 (BITS_OUT / BITS_IN)
- 累积位到frame_bits_out/frame_bits_in
- 当累积位数达到slot边界时：
  - 将位转换为整数值
  - 输出SLOT_OUT_RAW/SLOT_IN_RAW注解
  - 调用handle_slot()处理slot

#### handle_slot_00() — TAG slot
- READY位 (1位): 值为1时输出"READY"/"RDY"/"R"，为0时输出"rdy"/"-"
- VALID位 (12位): 输出十六进制，设置have_slots数组标记哪些slot有效
- Reserved位 (1位): 非零时输出ERROR
- CODEC位 (2位): 输出CODEC ID

#### handle_slot_01() — 命令/状态地址
- 仅在have_slots标记有效时处理
- 输出方向 (is_out): READ/WRITE位 (1位)
- 寄存器地址 (7位): 输出ADDR，奇数地址时输出ERROR
- 数据请求/保留位 (10+2位)

#### handle_slot_02() — 命令/状态数据
- 数据 (16位): 输出DATA
- 保留位 (4位): 非零时输出ERROR

#### handle_slot_dummy() — 默认slot处理器 (slot 3-12)
- 仅在have_slots标记有效时处理
- 输出十六进制文本
- 输出二进制数据（取高16位）

### 1.3 C实现计划

#### 私有数据结构
```c
#define AC97_MAX_SLOTS 13
#define AC97_FRAME_TOTAL_BITS 256
#define AC97_MAX_FRAME_SS (AC97_FRAME_TOTAL_BITS + 2)

struct ac97_priv {
    int have_sdo;              // 是否有SDATA_OUT通道
    int have_sdi;              // 是否有SDATA_IN通道
    int have_reset;            // 是否有RESET通道

    uint64_t *frame_ss_list;   // 帧内每个位的采样号列表
    int frame_ss_count;        // frame_ss_list中的元素数

    uint8_t frame_bits_out[256]; // 输出位序列
    int frame_bits_out_len;
    uint8_t frame_bits_in[256];  // 输入位序列
    int frame_bits_in_len;

    uint32_t frame_slot_data_out[AC97_MAX_SLOTS]; // 每个slot的整数值
    int frame_slot_data_out_len;
    uint32_t frame_slot_data_in[AC97_MAX_SLOTS];
    int frame_slot_data_in_len;

    int have_slots_out[AC97_MAX_SLOTS]; // slot有效性标记
    int have_slots_in[AC97_MAX_SLOTS];

    int prev_sync[3];          // 前3个SYNC采样值

    int out_ann;
    int out_binary;
};
```

#### 帧slot长度表
```c
static const int frame_slot_lens[] = {0, 16, 36, 56, 76, 96, 116, 136, 156, 176, 196, 216, 236, 256};
```

#### 函数签名
```c
static void ac97_reset(struct srd_decoder_inst *di);
static void ac97_start(struct srd_decoder_inst *di);
static void ac97_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void ac97_decode(struct srd_decoder_inst *di);
static void ac97_destroy(struct srd_decoder_inst *di);

// 内部函数
static void start_frame(struct ac97_priv *s, uint64_t ss);
static void flush_frame_bits(struct srd_decoder_inst *di, struct ac97_priv *s);
static void handle_bits(struct srd_decoder_inst *di, struct ac97_priv *s,
    uint64_t ss, uint64_t es, int bit_out, int bit_in);
static void handle_slot(struct srd_decoder_inst *di, struct ac97_priv *s,
    int slotidx, int data_out_valid, uint32_t data_out,
    int data_in_valid, uint32_t data_in);
static void handle_slot_00(struct srd_decoder_inst *di, struct ac97_priv *s,
    int slotidx, int bitidx, int bitcount, int is_out, uint32_t data);
static void handle_slot_01(struct srd_decoder_inst *di, struct ac97_priv *s,
    int slotidx, int bitidx, int bitcount, int is_out, uint32_t data);
static void handle_slot_02(struct srd_decoder_inst *di, struct ac97_priv *s,
    int slotidx, int bitidx, int bitcount, int is_out, uint32_t data);
static void handle_slot_dummy(struct srd_decoder_inst *di, struct ac97_priv *s,
    int slotidx, int bitidx, int bitcount, int is_out, uint32_t data);
static uint32_t bits_to_int(const uint8_t *bits, int count);
static uint32_t get_bit_field(uint32_t data, int size, int off, int count);
```

### 1.4 关键实现注意事项

1. **帧起始检测**: Python使用3元素prev_sync队列，检测prev_sync[0]==0 && prev_sync[1]==1。C实现需要维护3个历史SYNC值。

2. **位采样时序**: 数据在CLK下降沿采样，但注解跨越上升沿之间。需要同时等待下降沿和上升沿。

3. **frame_ss_list动态分配**: 需要预分配足够大的数组（257个uint64_t），每帧开始时重置。

4. **二进制输出**: bits_to_bin_ann()将位数组转为字节数组，每8位一组，MSB优先。

5. **slot处理函数指针表**: Python用字典映射slot索引到处理函数。C实现用switch-case或函数指针数组。

6. **注解位置计算**: putf()使用frame_ss_list[frombit]和frame_ss_list[frombit+bitcount]作为ss和es。

7. **通道检查**: 必须在start()中检查SDATA_OUT和SDATA_IN至少有一个存在，否则报错。

8. **内存管理**: frame_ss_list需要在reset时分配/释放，或使用固定大小数组。

9. **int_to_nibble_text**: 将整数转为十六进制字符串，位数由bitcount决定：`digits = (bitcount + 3) / 4`。

10. **SYNC帧检测逻辑**: Python在CLK下降沿采样后，等待上升沿时检查prev_sync历史。C实现需要精确复现这个时序。

### 1.5 Python与C的差异处理

| Python特性 | C实现方案 |
|-----------|----------|
| `self.wait({Pins.BIT_CLK: 'e'})` | `c_cond_edge(cb, CLK)` |
| `self.wait({Pins.BIT_CLK: 'r'})` | `c_cond_rise(cb, CLK)` |
| `self.wait({Pins.BIT_CLK: 'f'})` | `c_cond_fall(cb, CLK)` |
| `self.samplenum` | `c_cond_wait()`返回的samplenum |
| `self.has_channel(Pins.SDATA_OUT)` | `c_decoder_has_channel(di, CH_SDATA_OUT)` |
| `self.put(ss, es, self.out_ann, [cls, data])` | `C_ANN_PUT(di, ss, es, out_ann, cls, ...)` |
| `self.put(ss, es, self.out_binary, [cls, data])` | `c_decoder_put_binary(di, ss, es, out_binary, cls, len, data)` |
| `bits_to_int(bits)` | 自定义函数，MSB优先位序列转整数 |
| `bits_to_bin_ann(bits)` | 自定义函数，位数组转字节数组 |
| `self.frame_ss_list` | 动态分配的uint64_t数组 |
| Python列表append | 预分配数组+计数器 |

---

## 2. SDCARD_SD 解码器 (sdcard_sd_c.c)

### 2.1 Python解码器分析

#### 元数据
- **id**: `sdcard_sd`
- **name**: `SD card (SD mode)`
- **longname**: `Secure Digital card (SD mode)`
- **desc**: `Secure Digital card (SD mode) low-level protocol.`
- **license**: `gplv2+`
- **inputs**: `['logic']`
- **outputs**: `[]`
- **tags**: `['Memory']`

#### 通道定义

**必需通道 (2个)**:
| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | cmd | CMD | Command | dec_sdcard_sd_chan_cmd |
| 1 | clk | CLK | Clock | dec_sdcard_sd_chan_clk |

**可选通道 (4个)**:
| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 2 | dat0 | DAT0 | Data pin 0 | dec_sdcard_sd_opt_chan_dat0 |
| 3 | dat1 | DAT1 | Data pin 1 | dec_sdcard_sd_opt_chan_dat1 |
| 4 | dat2 | DAT2 | Data pin 2 | dec_sdcard_sd_opt_chan_dat2 |
| 5 | dat3 | DAT3 | Data pin 3 | dec_sdcard_sd_opt_chan_dat3 |

#### 选项
无

#### 注解定义 (大量，约200+个)

注解由以下部分组成：
1. **CMD0-CMD63** (64个): 索引 0-63
2. **ACMD0-ACMD63** (64个): 索引 64-127
3. **响应类型** (6个): 索引 128-133
   - RESPONSE_R1 (128), RESPONSE_R1B (129), RESPONSE_R2 (130), RESPONSE_R3 (131), RESPONSE_R6 (132), RESPONSE_R7 (133)
4. **状态寄存器字段** (30个): 索引 134-163
5. **CID寄存器字段** (9个): 索引 164-172
6. **CSD寄存器字段** (34个): 索引 173-206
7. **位注解** (2个): 索引 207-208 (BIT_0, BIT_1)
8. **字段注解** (6个): 索引 209-214 (START, TRANSMISSION, CMD, ARG, CRC, END)
9. **解码注解** (2个): 索引 215-216 (DECODED_BIT, DECODED_F)

**总计**: 217个注解类

#### 注解行 (5行)
| id | label | 注解类范围 |
|----|-------|-----------|
| raw-bits | Raw bits | BIT_0, BIT_1 (207, 208) |
| decoded-bits | Decoded bits | DECODED_BIT + R_* (215, 128-133) |
| decoded-fields | Decoded fields | DECODED_F (216) |
| fields | Fields | START, TRANSMISSION, CMD, ARG, CRC, END (209-214) |
| commands | Commands | CMD0-63, ACMD0-63, RESPONSE_* (0-133) |

#### 二进制输出
无

### 2.2 decode()逻辑分析

#### 状态机
Python使用字符串枚举状态：

| 状态 | 说明 |
|------|------|
| GET_COMMAND_TOKEN | 等待命令token (48位) |
| HANDLE_CMD0 ~ HANDLE_CMD63 | 处理特定CMD命令 |
| HANDLE_ACMD0 ~ HANDLE_ACMD63 | 处理特定ACMD命令 |
| HANDLE_CMD999 | 处理未知CMD命令 |
| HANDLE_ACMD999 | 处理未知ACMD命令 |
| GET_RESPONSE_R1 | 等待R1响应 (48位) |
| GET_RESPONSE_R1B | 等待R1b响应 (48位) |
| GET_RESPONSE_R2 | 等待R2响应 (136位) |
| GET_RESPONSE_R3 | 等待R3响应 (48位) |
| GET_RESPONSE_R6 | 等待R6响应 (48位) |
| GET_RESPONSE_R7 | 等待R7响应 (48位) |

#### 主循环
1. 构建等待条件：始终等待CLK上升沿
2. 在GET_COMMAND_TOKEN或GET_RESPONSE状态且token为空时，额外等待CMD=低（起始位）
3. 读取CMD引脚值
4. 根据状态分发处理

#### Token格式 (48位, MSB优先)
- Bit[47]: 起始位 (始终0)
- Bit[46]: 传输位 (1=主机, 0=卡)
- Bit[45:40]: 命令索引 (0-63)
- Bit[39:08]: 参数 (32位)
- Bit[07:01]: CRC7
- Bit[00]: 结束位 (始终1)

#### 命令处理
- **CMD0** (GO_IDLE_STATE): 无响应
- **CMD2** (ALL_SEND_CID): → R2
- **CMD3** (SEND_RELATIVE_ADDR): → R6
- **CMD6** (SWITCH_FUNC): → R1
- **CMD7** (SELECT/DESELECT_CARD): → R6
- **CMD8** (SEND_IF_COND): → R7
- **CMD9** (SEND_CSD): → R2
- **CMD10** (SEND_CID): → R2
- **CMD13** (SEND_STATUS): → R1
- **CMD16** (SET_BLOCKLEN): → R1
- **CMD55** (APP_CMD): → R1, 设置is_acmd=True
- **ACMD6** (SET_BUS_WIDTH): → R1
- **ACMD13** (SD_STATUS): → R1
- **ACMD41** (SD_SEND_OP_COND): → R3
- **ACMD51** (SEND_SCR): → R1

#### 响应处理
- **R1** (48位): 标准响应，包含card status
- **R1b** (48位): 同R1，带busy信号
- **R2** (136位): CID/CSD寄存器
- **R3** (48位): OCR寄存器
- **R6** (48位): 发布RCA响应
- **R7** (48位): 接口条件

#### 特殊逻辑
- **is_acmd标志**: CMD55后设置，下一个命令为ACMD，处理完后（非CMD55/63）清除
- **AssertionError处理**: 当响应token的传输位=1（主机）时，说明实际是命令而非响应，需要重新作为命令处理
- **puta()函数**: 参数位域注解，注意位序反转：`token[47-8-e]` 到 `token[47-8-s]`

### 2.3 C实现计划

#### 私有数据结构
```c
#define SDCARD_SD_MAX_TOKEN_BITS 136

enum sdcard_sd_state {
    STATE_GET_COMMAND_TOKEN = 0,
    STATE_HANDLE_CMD0,  // = 1
    // ... STATE_HANDLE_CMD63 = 64
    STATE_HANDLE_ACMD0, // = 65
    // ... STATE_HANDLE_ACMD63 = 128
    STATE_HANDLE_CMD999 = 129,
    STATE_HANDLE_ACMD999 = 130,
    STATE_GET_RESPONSE_R1 = 131,
    STATE_GET_RESPONSE_R1B = 132,
    STATE_GET_RESPONSE_R2 = 133,
    STATE_GET_RESPONSE_R3 = 134,
    STATE_GET_RESPONSE_R6 = 135,
    STATE_GET_RESPONSE_R7 = 136,
};

struct sd_bit {
    uint64_t ss;
    uint64_t es;
    int bit;
};

struct sdcard_sd_priv {
    int state;
    struct sd_bit token[SDCARD_SD_MAX_TOKEN_BITS];
    int token_len;
    int is_acmd;
    int cmd;
    int last_cmd;
    uint32_t arg;
    uint8_t crc;
    char cmd_str[64];
    int out_ann;
};
```

#### 注解枚举
```c
enum sdcard_sd_ann {
    ANN_CMD0 = 0,   // CMD0-CMD63: 0-63
    ANN_ACMD0 = 64, // ACMD0-ACMD63: 64-127
    ANN_RESPONSE_R1 = 128,
    ANN_RESPONSE_R1B = 129,
    ANN_RESPONSE_R2 = 130,
    ANN_RESPONSE_R3 = 131,
    ANN_RESPONSE_R6 = 132,
    ANN_RESPONSE_R7 = 133,
    // 状态寄存器字段: 134-163
    ANN_R_STATUS_OUT_OF_RANGE = 134,
    // ... (30个状态字段)
    ANN_R_STATUS_RSVD_TESTMODE = 163,
    // CID字段: 164-172
    ANN_R_CID_MID = 164,
    // ... (9个CID字段)
    ANN_R_CID_ONE = 172,
    // CSD字段: 173-206
    ANN_R_CSD_CSD_STRUCTURE = 173,
    // ... (34个CSD字段)
    ANN_R_CSD_ONE = 206,
    // 位注解
    ANN_BIT_0 = 207,
    ANN_BIT_1 = 208,
    // 字段注解
    ANN_F_START = 209,
    ANN_F_TRANSMISSION = 210,
    ANN_F_CMD = 211,
    ANN_F_ARG = 212,
    ANN_F_CRC = 213,
    ANN_F_END = 214,
    // 解码注解
    ANN_DECODED_BIT = 215,
    ANN_DECODED_F = 216,
    NUM_ANN = 217,
};
```

#### 函数签名
```c
static void sdcard_sd_reset(struct srd_decoder_inst *di);
static void sdcard_sd_start(struct srd_decoder_inst *di);
static void sdcard_sd_decode(struct srd_decoder_inst *di);
static void sdcard_sd_destroy(struct srd_decoder_inst *di);

// 内部函数
static int get_token_bits(struct sdcard_sd_priv *s, int cmd_pin, uint64_t samplenum, int n);
static int is_from_host(struct sdcard_sd_priv *s);
static int is_from_card(struct sdcard_sd_priv *s);
static void handle_common_token_fields(struct srd_decoder_inst *di, struct sdcard_sd_priv *s);
static void get_command_token(struct srd_decoder_inst *di, struct sdcard_sd_priv *s, int cmd_pin);
static void handle_cmd(struct srd_decoder_inst *di, struct sdcard_sd_priv *s);
static void handle_cmd0(struct srd_decoder_inst *di, struct sdcard_sd_priv *s);
// ... 各CMD处理函数
static void handle_response_r1(struct srd_decoder_inst *di, struct sdcard_sd_priv *s, int cmd_pin);
// ... 各响应处理函数
static void handle_reg_status(struct srd_decoder_inst *di, struct sdcard_sd_priv *s);
static void handle_reg_cid(struct srd_decoder_inst *di, struct sdcard_sd_priv *s);
static void handle_reg_csd(struct srd_decoder_inst *di, struct sdcard_sd_priv *s);
```

### 2.4 关键实现注意事项

1. **注解数量巨大**: 217个注解类，ann_labels数组需要217个条目，每个3个文本。这是所有解码器中最多的。

2. **puta()位序反转**: Python的puta()函数将参数位域的位序反转（`token[47-8-e]`到`token[47-8-s]`），C实现必须精确复现。

3. **状态机映射**: Python使用字符串状态名+getattr动态调用。C实现需要用枚举+switch-case或函数指针表。

4. **CMD55/ACMD切换**: CMD55设置is_acmd标志，下一个命令作为ACMD处理。ACMD处理完后（非CMD55/63）清除标志。

5. **AssertionError恢复**: Python在响应处理中捕获AssertionError（传输位=1），重新作为命令处理。C实现需要检查传输位，若为主机则切换到命令处理。

6. **R2响应136位**: 最长的token，需要token数组至少136个元素。

7. **命令名称查找**: 需要内置cmd_names和acmd_names查找表（来自common/sdcard/mod.py）。

8. **accepted_voltages查找**: R7响应中需要查找电压表（来自common/sdcard/mod.py）。

9. **token最后一个bit的es计算**: `token[n-1].es += token[n-1].ss - token[n-2].ss`，即最后一个bit的持续时间等于前一个bit的持续时间。

10. **start位检测**: 在GET_COMMAND_TOKEN或GET_RESPONSE状态且token为空时，需要等待CMD=低（起始位=0）。

### 2.5 Python与C的差异处理

| Python特性 | C实现方案 |
|-----------|----------|
| `self.wait({Pin.CLK: 'r'})` + 可选CMD低 | 两次c_cond_wait: 先CLK上升沿，再检查CMD |
| `self.wait({Pin.CLK: 'r', Pin.CMD: 'l'})` | `c_cond_rise(cb, CLK); c_cond_low(cb, CMD); c_cond_wait(...)` |
| `getattr(self, 'handle_cmd%d' % cmd)()` | switch(cmd) case 0: handle_cmd0(); ... |
| `SrdIntEnum.from_list('Ann', a)` | 手动定义enum |
| `SrdStrEnum.from_list('St', s)` | 手动定义enum |
| `hasattr(self, 'handle_cmd%d')` | switch覆盖所有已知CMD号 |
| `AssertionError` 恢复 | 检查传输位，若为主机则切换处理 |
| `self.token.append(Bit(...))` | 数组+计数器 |

---

## 3. EMMC_SD 解码器 (emmc_sd_c.c)

### 3.1 Python解码器分析

#### 元数据
- **id**: `emmc_sd`
- **name**: `eMMC (SD mode)`
- **longname**: `Embedded Multimedia card (SD mode)`
- **desc**: `Embedded Multimedia card (SD mode) low-level protocol.`
- **license**: `gplv2+`
- **inputs**: `['logic']`
- **outputs**: `[]`
- **tags**: `['Memory']`

#### 通道定义

**必需通道 (2个)**:
| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | cmd | CMD | Command | (无idn) |
| 1 | clk | CLK | Clock | (无idn) |

**可选通道**: 无

#### 选项
无

#### 注解定义 (73个)
| 枚举值 | id | desc |
|--------|-----|------|
| 0-63 | cmd0-cmd63 | CMD0-CMD63 |
| 64 | bits | Bits |
| 65 | field-start | Start bit |
| 66 | field-transmission | Transmission bit |
| 67 | field-cmd | Command |
| 68 | field-arg | Argument |
| 69 | field-crc | CRC |
| 70 | field-end | End bit |
| 71 | decoded-bit | Decoded bits |
| 72 | decoded-field | Decoded fields |

#### 注解行 (5行)
| id | label | 注解类范围 |
|----|-------|-----------|
| raw-bits | Raw bits | (64,) |
| decoded-bits | Decoded bits | (71,) |
| decoded-fields | Decoded fields | (72,) |
| fields | Fields | (65,66,67,68,69,70) |
| cmds | Commands | (0-63) |

#### 二进制输出
无

### 3.2 decode()逻辑分析

#### 状态机
Python使用字符串状态：

| 状态 | 说明 |
|------|------|
| GET COMMAND TOKEN | 等待命令token (48位) |
| HANDLE CMD0 ~ HANDLE CMD56 | 处理特定CMD命令 |
| HANDLE CMD999 | 处理未知CMD命令 |
| GET RESPONSE R1 | 等待R1响应 (48位) |
| GET RESPONSE R1b | 等待R1b响应 (48位) |
| GET RESPONSE R2 | 等待R2响应 (136位) |
| GET RESPONSE R3 | 等待R3响应 (48位) |
| GET RESPONSE R4 | 等待R4响应 (39位) |
| GET RESPONSE R5 | 等待R5响应 (40位) |

#### 与sdcard_sd的主要区别
1. **无ACMD**: eMMC没有应用特定命令
2. **更多CMD处理**: CMD1,4,5,12,14,15,17,18,19,21,23,24,25,26,27,28,29,30,31,35,36,38,39,40,42,44,45,46,47,48,49,53,54,55,56
3. **额外响应类型**: R4 (39位), R5 (40位)
4. **device_status表**: 不同于sdcard_sd的card_status
5. **CMD6参数不同**: SWITCH_FUNC参数格式不同

#### 主循环
1. 等待CLK上升沿
2. 在GET COMMAND TOKEN或GET RESPONSE状态且token为空时，等待CMD=低（起始位）
3. 读取CMD引脚值
4. 根据状态分发处理

#### 特殊CMD处理
- **CMD23** (SET_BLOCK_COUNT): 参数解析复杂，bit30决定后续字段含义
- **CMD38** (ERASE): 多个标志位（Secure Request, Force Garbage Collect, Discard Enable, TRIM Enable）
- **CMD44** (QUEUED_TASK_PARAMS): 多个字段（Reliable Write, Data Direction, Tag request, Context ID等）

### 3.3 C实现计划

#### 私有数据结构
```c
#define EMMC_SD_MAX_TOKEN_BITS 136

enum emmc_sd_state {
    STATE_GET_COMMAND_TOKEN = 0,
    STATE_HANDLE_CMD0 = 1,
    // ... CMD号直接映射到状态
    STATE_HANDLE_CMD999 = 64,
    STATE_GET_RESPONSE_R1 = 65,
    STATE_GET_RESPONSE_R1B = 66,
    STATE_GET_RESPONSE_R2 = 67,
    STATE_GET_RESPONSE_R3 = 68,
    STATE_GET_RESPONSE_R4 = 69,
    STATE_GET_RESPONSE_R5 = 70,
};

struct emmc_bit {
    uint64_t ss;
    uint64_t es;
    int val;
};

struct emmc_sd_priv {
    int state;
    struct emmc_bit token[EMMC_SD_MAX_TOKEN_BITS];
    int token_len;
    int cmd;
    int last_cmd;
    uint32_t arg;
    uint8_t crc;
    char cmd_str[64];
    int out_ann;
};
```

#### 函数签名
```c
static void emmc_sd_reset(struct srd_decoder_inst *di);
static void emmc_sd_start(struct srd_decoder_inst *di);
static void emmc_sd_decode(struct srd_decoder_inst *di);
static void emmc_sd_destroy(struct srd_decoder_inst *di);

// 内部函数
static int get_token_bits(struct emmc_sd_priv *s, int cmd_pin, uint64_t samplenum, int n);
static void handle_common_token_fields(struct srd_decoder_inst *di, struct emmc_sd_priv *s);
static void get_command_token(struct srd_decoder_inst *di, struct emmc_sd_priv *s, int cmd_pin);
static void handle_cmd0(struct srd_decoder_inst *di, struct emmc_sd_priv *s);
// ... 各CMD处理函数 (约30个)
static void handle_response_r1(struct srd_decoder_inst *di, struct emmc_sd_priv *s, int cmd_pin);
// ... 各响应处理函数
```

### 3.4 关键实现注意事项

1. **device_status表**: 来自emmc_sd/mod.py，共32个条目，用于R1响应的状态位解码。

2. **CMD23条件解析**: bit30=1时Packed模式，bit30=0时Normal模式，字段含义不同。

3. **R4响应39位**: 非标准48位，需要特殊处理token长度。

4. **R5响应40位**: 同样非标准长度。

5. **putbit()函数**: 使用注解类135（但实际注解定义中最大索引为72），Python代码中使用了硬编码的注解索引号（128-136），这与ann_labels定义不一致。**C实现需要修正此不一致**，使用正确的注解枚举值。

6. **无ACMD**: 简化了状态机，无需is_acmd标志。

7. **cmd_names查找表**: 来自emmc_sd/mod.py，与sdcard_sd的cmd_names不同。

8. **注解索引不一致**: Python代码中handle_response_r1()使用了`[135, data]`，但注解定义中索引135不存在（最大72）。这可能是Python代码的bug。C实现应使用ANN_DECODED_BIT(71)或ANN_DECODED_F(72)。

### 3.5 Python与C的差异处理

| Python特性 | C实现方案 |
|-----------|----------|
| `from .mod import *` | 内联cmd_names和device_status表 |
| `self.state.startswith('HANDLE CMD')` | 枚举范围检查 |
| `getattr(self, 'handle_cmd%s' % cmdstr)` | switch(cmd) |
| 硬编码注解索引128-136 | 使用正确的枚举值 |
| `self.token[30]` 直接访问 | `s->token[30].val` |

---

## 4. SWIM 解码器 (swim_c.c)

### 4.1 Python解码器分析

#### 元数据
- **id**: `swim`
- **name**: `SWIM`
- **longname**: `STM8 SWIM bus`
- **desc**: `STM8 Single Wire Interface Module (SWIM) protocol.`
- **license**: `gplv2+`
- **inputs**: `['logic']`
- **outputs**: `[]`
- **tags**: `['Debug/trace']`

#### 通道定义

**必需通道 (1个)**:
| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | swim | SWIM | SWIM data line | dec_swim_chan_swim |

**可选通道**: 无

#### 选项 (1个)
| id | desc | default | values | idn |
|----|------|---------|--------|-----|
| debug | Debug | 'no' | ('yes', 'no') | dec_swim_opt_debug |

#### 注解定义 (16个)
| 枚举值 | id | desc |
|--------|-----|------|
| 0 | bit | Bit |
| 1 | enterseq | SWIM enter sequence |
| 2 | start-host | Start bit (host) |
| 3 | start-target | Start bit (target) |
| 4 | parity | Parity bit |
| 5 | ack | Acknowledgement |
| 6 | nack | Negative acknowledgement |
| 7 | byte-write | Byte write |
| 8 | byte-read | Byte read |
| 9 | cmd-unknown | Unknown SWIM command |
| 10 | cmd | SWIM command |
| 11 | bytes | Byte count |
| 12 | address | Address |
| 13 | data-write | Data write |
| 14 | data-read | Data read |
| 15 | debug | Debug |

#### 注解行 (4行)
| id | label | 注解类列表 |
|----|-------|-----------|
| bits | Bits | (0,) |
| framing | Framing | (2,3,4,5,6,7,8) |
| protocol | Protocol | (1,9,10,11,12,13,14) |
| debug | Debug | (15,) |

#### 二进制输出 (2个)
| 枚举值 | id | desc |
|--------|-----|------|
| 0 | tx | Dump of data written to target |
| 1 | rx | Dump of data read from target |

### 4.2 decode()逻辑分析

#### 时序参数
- HSI = 8MHz ±5% (目标内部RC振荡器)
- HSI_min = 7.2MHz, HSI_max = 8.8MHz
- swim_clock = HSI_min / 2 = 3.6MHz (初始值)
- 低速位 = 22个SWIM时钟周期
- 同步帧: 低电平持续64-128个SWIM时钟周期

#### 位解码
- 单线协议，位由高低电平持续时间比例决定
- 如果高电平时间 ≥ 低电平时间 → 位0
- 如果高电平时间 < 低电平时间 → 位1

#### 进入序列检测
- 4个2kHz脉冲 + 4个1kHz脉冲
- 不检查绝对频率，而是检查连续脉冲的相对长度
- 前4个脉冲长度相同，后4个脉冲长度为前4个的一半

#### 同步帧检测
- 低电平持续64-128个SWIM时钟周期
- 检测后重置协议状态机
- 用同步帧长度重新计算swim_clock: `swim_clock = 128 * (samplerate / low_duration)`

#### 协议状态机
| 状态 | 说明 |
|------|------|
| CMD | 等待命令字节 (4位 + parity + ACK) |
| N | 等待字节计数 (8位 + parity + ACK) |
| @E | 等待地址高字节 |
| @H | 等待地址中字节 |
| @L | 等待地址低字节 |
| D | 等待数据字节 |

#### 位序列处理 (bitseq函数)
- CMD状态: 4位命令 + 1位parity + 1位ACK/NACK
- 其他状态: 8位数据 + 1位parity + 1位ACK/NACK
- start位不作为数据，但参与parity计算
- 仅ACK时才传递数据到protocol()层

#### 命令定义
| 值 | 命令 | 说明 |
|----|------|------|
| 0x00 | SRST | 系统复位 |
| 0x01 | ROTF | 在线读取 |
| 0x02 | WOTF | 在线写入 |
| 其他 | UNK | 未知命令 |

#### 主循环
1. 如果bit_maxlen >= 0（正在位中间），等待任意采样
2. 否则等待SWIM线边沿
3. 检测边沿变化，维护eseq_edge和bit_edge历史
4. 检测同步帧和进入序列
5. 检测位边界并解码位值

### 4.3 C实现计划

#### 私有数据结构
```c
struct swim_priv {
    // 时序参数
    double HSI;
    double HSI_min;
    double HSI_max;
    double swim_clock;
    uint64_t bit_reflen;
    uint64_t sync_reflen_min;
    uint64_t sync_reflen_max;
    uint64_t eseq_reflen;
    int debug;

    // 边沿历史
    int eseq_edge_val[2];
    uint64_t eseq_edge_ss[2];
    int eseq_pairnum;
    uint64_t eseq_pairstart;

    int bit_edge_val[2];
    uint64_t bit_edge_ss[2];
    int bit_maxlen;

    // 位序列
    int bitseq_len;
    uint64_t bitseq_start;
    uint64_t bitseq_end;
    uint8_t bitseq_value;
    int bitseq_dir;

    // 协议状态
    int proto_state;
    int proto_byte_count;
    uint32_t proto_addr;
    uint64_t proto_addr_start;

    uint64_t samplerate;
    int out_ann;
    int out_binary;
};
```

#### 协议状态枚举
```c
enum swim_proto_state {
    PROTO_CMD = 0,
    PROTO_N,
    PROTO_ADDR_E,
    PROTO_ADDR_H,
    PROTO_ADDR_L,
    PROTO_DATA,
};
```

#### 函数签名
```c
static void swim_reset(struct srd_decoder_inst *di);
static void swim_start(struct srd_decoder_inst *di);
static void swim_metadata(struct srd_decoder_inst *di, int key, uint64_t value);
static void swim_decode(struct srd_decoder_inst *di);
static void swim_destroy(struct srd_decoder_inst *di);

// 内部函数
static void adjust_timings(struct swim_priv *s);
static void protocol(struct srd_decoder_inst *di, struct swim_priv *s);
static void bitseq(struct srd_decoder_inst *di, struct swim_priv *s,
    uint64_t bitstart, uint64_t bitend, int bit);
static void decode_bit(struct srd_decoder_inst *di, struct swim_priv *s,
    uint64_t start, uint64_t mid, uint64_t end);
static void detect_synchronize_frame(struct srd_decoder_inst *di, struct swim_priv *s);
static void detect_enter_sequence(struct swim_priv *s, uint64_t start, uint64_t end);
```

### 4.4 关键实现注意事项

1. **需要samplerate**: SWIM解码器需要采样率来计算时序参数。必须在metadata回调中获取，并在start()中验证。

2. **浮点运算**: 大量使用浮点运算计算时序（HSI频率、swim_clock、reflen等）。C实现需要使用double类型。

3. **ceil/floor函数**: 需要包含`<math.h>`，链接时可能需要`-lm`。

4. **边沿检测逻辑复杂**: Python代码维护2元素边沿历史队列（eseq_edge, bit_edge），C实现需要精确复现。

5. **bit_maxlen机制**: 当检测到位开始时，设置bit_maxlen=bit_reflen，然后逐采样递减，用于限制位的最大长度。

6. **swim = -1特殊值**: 当bit_maxlen==0且当前为高电平时，设置swim=-1表示超时。

7. **同步帧重计算swim_clock**: `swim_clock = 128 * (samplerate / low_duration)`，这会动态调整时序。

8. **进入序列检测**: 不检查绝对频率，只检查相对脉冲长度。前4个脉冲等长，后4个脉冲为前4个的一半。

9. **位解码**: 通过高低电平持续时间比例判断0/1。高≥低→0，高<低→1。

10. **parity计算**: start位参与parity，但最终`bitseq_value &= 0xff`清除start位的影响。

11. **binary输出**: ACK时输出数据字节到binary（tx=0, rx=1）。

### 4.5 Python与C的差异处理

| Python特性 | C实现方案 |
|-----------|----------|
| `self.wait()` (无条件) | `c_cond_wait_current(di, &samplenum)` <!-- Updated: 已有c_cond_wait_current()API，等效Python self.wait({}) --> |
| `self.wait({0: 'e'})` | `c_cond_edge(cb, 0)` |
| `math.ceil()`, `math.floor()` | `<math.h>` 的 `ceil()`, `floor()` |
| Python浮点除法 | C的double除法 |
| `self.samplenum` | `c_cond_wait()` 返回的samplenum |
| `bytes([self.bitseq_value])` | `uint8_t buf = value; c_decoder_put_binary(...)` |
| `self.options['debug']` | `c_decoder_get_option_string(di, "debug", "no")` |

---

## 5. RVSWD 解码器 (rvswd_c.c)

### 5.1 Python解码器分析

#### 元数据
- **id**: `rvswd`
- **name**: `RVSWD`
- **longname**: `RISC-V Serial Wire Debug (WCH)`
- **desc**: `WCH RISC-V Serial Wire Debug protocol.`
- **license**: `gplv2+`
- **inputs**: `['logic']`
- **outputs**: `[]`
- **tags**: `['Debug/trace']`

#### 通道定义

**必需通道 (2个)**:
| 索引 | id | name | desc | idn |
|------|-----|------|------|-----|
| 0 | clk | CLK | Serial clock line | (无idn) |
| 1 | dio | DIO | Serial data line | (无idn) |

**可选通道**: 无

#### 选项
无

#### 注解定义 (11个)
| 枚举值 | id | desc | names |
|--------|-----|------|-------|
| 0 | start | Start condition | ["START", "S"] |
| 1 | stop | Stop condition | ["STOP", "P"] |
| 2 | address-host | Address host | ["ADDR HOST 0x{data:02x}", "AH 0x{data:02x}", "{data:02x}"] |
| 3 | address-target | Address target | ["ADDR TARGET 0x{data:02x}", "AT 0x{data:02x}", "{data:02x}"] |
| 4 | data-host | Data host | ["DATA HOST 0x{data:08x}", "DH 0x{data:08x}", "{data:08x}"] |
| 5 | data-target | Data target | ["DATA TARGET 0x{data:08x}", "DT 0x{data:08x}", "{data:08x}"] |
| 6 | parity-host | Parity host | ["PARITY HOST 0x{data:01x}", "PH 0x{data:01x}", "{data:01x}"] |
| 7 | parity-target | Parity target | ["PARITY TARGET 0x{data:01x}", "PT 0x{data:01x}", "{data:01x}"] |
| 8 | operation | Operation | ["OPERATION 0x{data:01x}", "OP 0x{data:01x}", "{data:01x}"] |
| 9 | status | Status | ["STATUS 0x{data:01x}", "ST 0x{data:01x}", "{data:01x}"] |
| 10 | bit | Bit | ["BIT {data[0]}: {data[1]:b}", "{data[1]:b}"] |

#### 注解行 (2行)
| id | label | 注解类列表 |
|----|-------|-----------|
| addr-data | Address/data | (0,1,2,3,4,5,6,7,8,9) |
| bits | Bits | (10,) |

#### 二进制输出
无

#### Python输出
注册了SRD_OUTPUT_PYTHON输出（虽然outputs列表为空）

### 5.2 decode()逻辑分析

#### 协议概述
WCH RISC-V串行线调试协议，双线（CLK+DIO），类似I2C的起始/停止条件。

#### 起始条件
CLK=高时DIO下降沿

#### 停止条件
CLK=高时DIO上升沿

#### 位采样
- CLK下降沿：终止当前位（设置end sample）
- CLK上升沿：采样DIO值（push bit value）

#### 数据包格式

**短包 (52位)**:
| 位范围 | 字段 | 注解 |
|--------|------|------|
| 0-6 | address-host | 7位主机地址 |
| 7 | operation | 操作位 |
| 8 | parity-host | 主机校验 |
| 9-13 | (turnaround?) | 无注解 |
| 14-45 | data-target | 32位目标数据 |
| 46 | parity-target | 目标校验 |
| 47-51 | (turnaround?) | 无注解 |

**长包 (84位)**:
| 位范围 | 字段 | 注解 |
|--------|------|------|
| 0-6 | address-host | 7位主机地址 |
| 7-38 | data-host | 32位主机数据 |
| 39-40 | operation | 2位操作 |
| 41 | parity-host | 主机校验 |
| 42-48 | address-target | 7位目标地址 |
| 49-80 | data-target | 32位目标数据 |
| 81-82 | status | 2位状态 |
| 83 | parity-target | 目标校验 |

#### 主循环
1. 如果不在包中：等待START条件（CLK=高, DIO=下降沿）
2. 如果在包中：等待CLK上升沿/下降沿 或 STOP条件（CLK=高, DIO=上升沿）
3. 检测STOP条件时处理完整包
4. CLK上升沿采样DIO，CLK下降沿终止位

#### put_annotation_bits()函数
- 将位列表转为整数值（MSB优先）
- 输出注解，使用格式化名称

### 5.3 C实现计划

#### 私有数据结构
```c
#define RVSWD_MAX_BITS 128

struct rvswd_bit {
    int val;
    uint64_t start;
    uint64_t end;
};

struct rvswd_priv {
    struct rvswd_bit bits[RVSWD_MAX_BITS];
    int bits_len;
    int curr_bit_val;
    uint64_t curr_bit_start;
    int in_packet;
    int out_ann;
    int out_python;
    int out_binary;
};
```

#### 函数签名
```c
static void rvswd_reset(struct srd_decoder_inst *di);
static void rvswd_start(struct srd_decoder_inst *di);
static void rvswd_decode(struct srd_decoder_inst *di);
static void rvswd_destroy(struct srd_decoder_inst *di);

// 内部函数
static void handle_start_condition(struct rvswd_priv *s, uint64_t samplenum);
static void handle_stop_condition(struct srd_decoder_inst *di, struct rvswd_priv *s, uint64_t samplenum);
static void handle_bit(struct rvswd_priv *s, int clk, int dio, uint64_t samplenum);
static void process_packet(struct srd_decoder_inst *di, struct rvswd_priv *s);
static void process_short_packet(struct srd_decoder_inst *di, struct rvswd_priv *s);
static void process_long_packet(struct srd_decoder_inst *di, struct rvswd_priv *s);
static void put_annotation_bits(struct srd_decoder_inst *di, struct rvswd_priv *s,
    int start_idx, int count, int ann_id);
```

### 5.4 关键实现注意事项

1. **注解名称格式化**: Python使用`"{data:02x}"`等格式化字符串。C实现需要在运行时用snprintf生成格式化文本。

2. **bit注解特殊格式**: `"BIT {data[0]}: {data[1]:b}"` — data是列表[data[0], data[1]]。C实现需要特殊处理，将位索引和位值格式化为字符串。

3. **matched检查**: Python使用`self.matched == (False, False, True)`检查哪个条件匹配。C实现需要通过`c_cond_wait()`返回的matched位掩码来判断。

4. **位列表管理**: Python的bit_list类有push/terminate/reset方法。C实现用数组+计数器替代。

5. **短包/长包区分**: 52位=短包，84位=长包，其他长度=无效。

6. **put_annotation_bits()**: 将位范围转为整数（MSB优先），然后格式化输出注解。

7. **SRD_OUTPUT_PYTHON**: Python代码注册了python输出但outputs列表为空。C实现应注册python输出以保持与Python版本兼容，上层解码器可能依赖此输出。 <!-- Updated: 原文说"不需要实际输出python数据"，但为兼容性应注册输出 -->

8. **SRD_OUTPUT_BINARY**: Python代码注册了binary输出但未使用。C实现可以省略。

9. **位采样逻辑**: CLK下降沿终止位（设置end），CLK上升沿采样DIO（设置bit value）。这与大多数协议相反。

### 5.5 Python与C的差异处理

| Python特性 | C实现方案 |
|-----------|----------|
| `self.wait({clk: "h", dio: "f"})` | `c_cond_high(cb, CLK); c_cond_fall(cb, DIO); c_cond_wait(...)` |
| `self.wait([{clk: "r"}, {clk: "f"}, {clk: "h", dio: "r"}])` | `c_cond_rise(cb, CLK); c_cond_or(cb); c_cond_fall(cb, CLK); c_cond_or(cb); c_cond_high(cb, CLK); c_cond_rise(cb, DIO); c_cond_wait(...)` |
| `self.matched == (False, False, True)` | 检查matched位掩码 |
| `ANNOTATIONS.fnames(id, data)` | snprintf格式化 |
| `bit_list` 类 | 结构体数组+计数器 |
| `self.register(srd.OUTPUT_PYTHON)` | `c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "rvswd")` <!-- Updated: 应注册python输出以保持兼容性 --> |

### 5.6 多条件等待的实现方案

Python的`self.wait([{clk: "r"}, {clk: "f"}, {clk: "h", dio: "r"}])`需要同时等待3种OR条件。C API通过`c_cond_or()`函数支持OR条件列表，每个OR分支之间用`c_cond_or()`分隔。

**标准c_cond_or模式**：在同一个`srd_cond_builder`中，先添加第一组条件，调用`c_cond_or(b)`开始新的OR分支，再添加第二组条件，依此类推。`c_cond_wait()`返回时，通过`matched`位掩码判断匹配了哪个OR分支。

```c
// Python: self.wait([{clk: "r"}, {clk: "f"}, {clk: "h", dio: "r"}])
// 等价C实现：3个OR分支
srd_cond_builder *cb = c_cond_new();

// OR分支1: CLK上升沿
c_cond_rise(cb, CLK);
c_cond_or(cb);

// OR分支2: CLK下降沿
c_cond_fall(cb, CLK);
c_cond_or(cb);

// OR分支3: CLK=高 且 DIO上升沿（STOP条件）
c_cond_high(cb, CLK);
c_cond_rise(cb, DIO);

uint64_t samplenum, matched;
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);

// 通过matched判断匹配了哪个OR分支
// matched的bit位对应OR分支编号（从0开始）
int clk = c_decoder_get_pin(di, CLK, samplenum);
int dio = c_decoder_get_pin(di, DIO, samplenum);

if (matched & (1 << 2)) {
    // OR分支3匹配: STOP条件（CLK=高且DIO上升沿）
} else if (matched & (1 << 0)) {
    // OR分支1匹配: CLK上升沿 → 采样DIO
} else if (matched & (1 << 1)) {
    // OR分支2匹配: CLK下降沿 → 终止当前位
}
```

**c_cond_or使用参考**：多个现有C解码器已使用此模式，如`ps2_c.c`、`i2c_c.c`、`spi_c.c`、`uart_c.c`、`seven_segment_c.c`等。

---

## 通用实现指南

### C解码器文件模板

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

// 1. 状态枚举
// 2. 注解枚举
// 3. 私有数据结构
// 4. 通道定义数组
// 5. 注解标签数组
// 6. 注解行定义
// 7. 二进制输出定义
// 8. 输入/输出/标签定义
// 9. 选项定义
// 10. 内部函数实现
// 11. reset/start/decode/destroy函数
// 12. srd_c_decoder结构体
// 13. srd_c_decoder_entry()导出函数
// 14. srd_c_decoder_api_version()导出函数
```

### CMakeLists.txt修改

在CMakeLists.txt的C_DECODERS列表中添加新解码器名称：
```
ac97_c
sdcard_sd_c
emmc_sd_c
swim_c
rvswd_c
```

### 测试验证

每个C解码器完成后需要验证：
1. 编译无错误无警告
2. DLL正确加载
3. 通道配置正确显示
4. 与Python解码器在相同输入数据下产生相同的注解输出
5. 二进制输出（如有）格式正确
6. Python输出（如有）格式正确
7. 边界条件处理（无数据、部分帧、错误帧）
