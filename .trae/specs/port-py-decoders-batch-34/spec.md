# Python→C 解码器移植规格书 — Batch 34

## 1. 概述

本规格书覆盖 5 个 Python 协议解码器到 C 解码器的移植工作。这些解码器均为**上层解码器**（upper-layer decoders），不直接读取 logic 信号，而是通过 `recv_proto()` 回调接收下层解码器输出的协议数据。

### 1.1 移植目标

| # | 解码器 | Python ID | C ID | 输入协议 | 输出协议 | 复杂度 |
|---|--------|-----------|------|----------|----------|--------|
| 1 | AVC-LAN | `avclan` | `avclan_c` | `iebus` | 无 | ★★★★★ |
| 2 | Ethernet | `ethernet` | `ethernet_c` | `4b5b` | `ethernet` | ★★★☆☆ |
| 3 | ARP | `arp` | `arp_c` | `ethernet` | 无 | ★★☆☆☆ |
| 4 | IPv4 | `ipv4` | `ipv4_c` | `ethernet` | `ipv4` | ★★★☆☆ |
| 5 | UDP | `udp` | `udp_c` | `ipv4` | `udp` | ★★☆☆☆ |

### 1.2 依赖链

<!-- Updated: 已验证依赖链中所有C解码器的存在状态。4b5b_c.c 已存在（C实现），ethernet_c/arp_c/ipv4_c/udp_c 为本次移植。avclan_c 依赖 iebus（仅有Python实现），被阻塞。 -->

```
logic → 4b5b_c → ethernet_c → arp_c
                            → ipv4_c → udp_c
logic → iebus(Python) → avclan_c  ⚠️ 阻塞：iebus无C实现
```

**关键约束**：`avclan_c` 依赖 `iebus` 输入，而 `iebus` 目前仅有 Python 实现。`avclan_c` 的 `recv_proto()` 必须能正确解析 Python `iebus` 解码器通过 `put()` 发送的 Python 对象。但 C 解码器的 `recv_proto` 只接收 `(cmd, data, data_len)` 格式——这意味着 `avclan_c` 的 `iebus` 上层解码器要么需要先移植 `iebus` 到 C，要么需要特殊处理 Python→C 跨语言协议传递。

<!-- Updated: 已验证 Python→C proto 桥接未实现。type_decoder.c 中 SRD_OUTPUT_PYTHON 分支（第557-578行）仅调用 PyObject_CallMethod(next_di->py_inst, "decode", ...)，不检查 next_di->is_c_inst，不会调用 recv_proto()。因此 avclan_c 被阻塞，除非先移植 iebus 为 C 解码器。建议将 avclan_c 标记为"阻塞"，或将其从本批次移除，待 iebus_c 移植后再处理。 -->

**决策**：`avclan_c` 的 `inputs` 声明为 `{"iebus", NULL}`，但实际运行时需要 `iebus` 的 C 版本或 Python→C 桥接。本规格书假设 `iebus` 的 Python 输出会通过 `c_decoder_put_python` 桥接机制传递到 `avclan_c` 的 `recv_proto()`。如果 `iebus` 未移植为 C，则 `avclan_c` 暂时无法与 Python `iebus` 直接对接（需要 libsigrokdecode 引擎层面的 Python→C proto 桥接支持）。

<!-- Updated: 上述"假设"不成立。经代码验证，libsigrokdecode 引擎目前不支持 Python→C proto 桥接。avclan_c 必须等待 iebus_c 移植完成后才能工作。 -->

---

## 参考实现

本批次解码器应参考以下标准范本实现：

<!-- Updated: 已验证所有参考实现文件均存在。4b5b_c.c 作为 ethernet_c 的直接下层解码器，其输出格式已验证与spec一致。lm75_c.c 和 ds1307_c.c 作为上层recv_proto范本已确认存在且可用。 -->

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| 4b5b_c.c | 底层编码解码范本 | 4B/5B编码解码、c_decoder_put_python()输出协议数据 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |

## 2. C 解码器架构规范

### 2.1 文件命名与结构

- 文件路径：`libsigrokdecode/c_decoders/{decoder_id}_c.c`
- 命名规则：`{python_id}_c.c`，如 `ethernet_c.c`、`arp_c.c`

### 2.2 上层解码器核心模式

上层解码器与底层解码器（如 i2c_c、can_fd_c）的根本区别：

| 特性 | 底层解码器 | 上层解码器 |
|------|-----------|-----------|
| 输入 | `logic`（原始采样数据） | 协议名（如 `i2c`、`ethernet`） |
| 主回调 | `decode()` — 循环等待条件 | `recv_proto()` — 被动接收协议数据 |
| 通道定义 | 必须定义 `channels` | `channels = NULL, num_channels = 0` |
| 条件构建 | `c_cond_*` 系列 | 不使用 |
| 数据获取 | `c_decoder_get_pin()` | 通过 `recv_proto()` 参数 |

### 2.3 recv_proto 签名

```c
void (*recv_proto)(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

- `cmd`：下层解码器通过 `c_decoder_put_python()` 发送的命令字符串
- `data`：附带的数据字节（可为 NULL）
- `data_len`：data 的长度

<!-- Updated: 已验证 c_decoder_put_python() 在 c_decoder_api.c 第454-483行实现。该函数同步调用 next_di->recv_proto()，data 指针在 recv_proto 返回后可安全释放。BITS v2 格式文档见 c_decoders/c_decoder_utils.h。 -->

### 2.4 srd_c_decoder 结构体模板

```c
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    .longname = "Full Protocol Name (C)",
    .desc = "Protocol decoder (C implementation)",
    .license = "gplv2+",
    .channels = NULL,           // 上层解码器无通道
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = xxx_options,
    .num_options = N,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = N,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,       // 协议输入名
    .num_inputs = 1,
    .outputs = xxx_outputs,     // 协议输出名（如有）
    .num_outputs = N,
    .binary = NULL,             // 或 xxx_binary
    .num_binary = 0,
    .tags = xxx_tags,
    .num_tags = N,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,       // 上层解码器 decode() 通常为空
    .destroy = xxx_destroy,
    .recv_proto = xxx_recv_proto,  // 核心回调
};
```

### 2.5 ann_labels 规范

- 第一列必须为 `""`（空字符串）
- 第二列为短标签（用于 UI 显示）
- 第三列为描述文本

### 2.6 annotation_rows 规范

- 所有 annotation class 必须被映射到某个 row
- row 的 class 数组以 `-1` 结尾（仅底层解码器需要，上层解码器直接用数组长度参数）

### 2.7 decode() 函数

上层解码器的 `decode()` 函数通常为空实现：

```c
static void xxx_decode(struct srd_decoder_inst *di)
{
    (void)di;
    // 上层解码器通过 recv_proto 接收数据，不需要 decode 循环
}
```

---

## 3. 各解码器详细规格

---

### 3.1 avclan_c — AVC-LAN 协议解码器

> ⚠️ **阻塞状态**：avclan_c 依赖 iebus 输入，而 iebus 仅有 Python 实现。当前 libsigrokdecode 引擎不支持 Python→C proto 桥接，因此 avclan_c 无法工作。需先移植 iebus_c 后才能实施。

<!-- Updated: 添加阻塞警告。经代码验证，type_decoder.c 中 Python 解码器的 SRD_OUTPUT_PYTHON 输出不支持调用 C 解码器的 recv_proto()。 -->

#### 3.1.1 元数据

| 属性 | 值 |
|------|-----|
| id | `avclan_c` |
| name | `AVC-LAN(C)` |
| longname | `AVC-LAN Toyota Audio-Video Local Area Network (C)` |
| desc | `AVC-LAN Protocol Decoder (IEBus Mode 2 variant) (C implementation)` |
| license | `gplv3+` |
| inputs | `{"iebus", NULL}` |
| outputs | `{NULL}` (无输出) |
| tags | `{"Automotive", NULL}` |

#### 3.1.2 Annotations（31 个）

```c
enum {
    ANN_ADDRESS = 0,        // Device Address
    ANN_FUNCTION,           // Function (1)
    ANN_CTRL_OPCODE,        // Opcode (2)
    ANN_SEQUENCE_NO,        // Sequence No. (3)
    ANN_ADVERTISED_FUNC,    // Function (4)
    ANN_CMD_OPCODE,         // Opcode (5)
    ANN_CD_OPCODE,          // Opcode (6)
    ANN_CD_STATE,           // State (7)
    ANN_CD_FLAGS,           // Flags (8)
    ANN_DISC_NUMBER,        // Disc Number (9)
    ANN_TRACK_NUMBER,       // Track Number (10)
    ANN_TRACK_COUNT,        // Track Count (11)
    ANN_DISC_TITLE,         // Disc Name (12)
    ANN_TRACK_TITLE,        // Track Name (13)
    ANN_PLAYBACK_TIME,      // Playback time (14)
    ANN_DISC_SLOTS,         // Disc Slots (15)
    ANN_AUDIO_OPCODE,       // Opcode (16)
    ANN_AUDIO_FLAGS,        // Audio Flags (17)
    ANN_VOLUME,             // Volume (18)
    ANN_BASS,               // Bass (19)
    ANN_TREBLE,             // Treble (20)
    ANN_FADE,               // Fade (21)
    ANN_BALANCE,            // Balance (22)
    ANN_RADIO_OPCODE,       // Opcode (23)
    ANN_RADIO_STATE,        // State (24)
    ANN_RADIO_MODE,         // Mode (25)
    ANN_RADIO_FLAGS,        // Flags (26)
    ANN_BAND,               // Band (27)
    ANN_CHANNEL,            // Channel (28)
    ANN_FREQ,               // Frequency (29)
    ANN_WARNING,            // Warning (30)
    NUM_ANN,
};
```

#### 3.1.3 Annotation Rows（7 行）

```c
static const int row_devices_classes[] = {ANN_ADDRESS, ANN_FUNCTION};
static const int row_control_classes[] = {ANN_CTRL_OPCODE, ANN_SEQUENCE_NO, ANN_ADVERTISED_FUNC};
static const int row_cmd_classes[] = {ANN_CMD_OPCODE};
static const int row_cd_classes[] = {ANN_CD_OPCODE, ANN_CD_STATE, ANN_CD_FLAGS, ANN_DISC_NUMBER,
                                      ANN_TRACK_NUMBER, ANN_TRACK_COUNT, ANN_DISC_TITLE,
                                      ANN_TRACK_TITLE, ANN_PLAYBACK_TIME, ANN_DISC_SLOTS};
static const int row_audio_classes[] = {ANN_AUDIO_OPCODE, ANN_AUDIO_FLAGS, ANN_VOLUME,
                                         ANN_BASS, ANN_TREBLE, ANN_FADE, ANN_BALANCE};
static const int row_radio_classes[] = {ANN_RADIO_OPCODE, ANN_RADIO_STATE, ANN_RADIO_MODE,
                                         ANN_RADIO_FLAGS, ANN_BAND, ANN_CHANNEL, ANN_FREQ};
static const int row_warnings_classes[] = {ANN_WARNING};
```

#### 3.1.4 状态机

```
IDLE → (HEADER) → MASTER_ADDRESS → (MASTER ADDRESS) → SLAVE_ADDRESS → (SLAVE ADDRESS)
  → CONTROL → (CONTROL) → DATA_LENGTH → (DATA LENGTH) → DATA → (DATA) → IDLE
```

任何时刻收到 NAK → 重置为 IDLE。

#### 3.1.5 recv_proto 协议映射

Python `iebus` 解码器通过 `self.put(ss, es, self.out_python, (ptype, pdata))` 发送数据。C 版本需要定义对应的 cmd 字符串：

| Python ptype | C cmd 字符串 | data 格式 |
|-------------|-------------|----------|
| `'HEADER'` | `"HEADER"` | `data[0]` = broadcast_bit (uint8_t) |
| `'MASTER ADDRESS'` | `"MASTER ADDRESS"` | `data[0..1]` = address(LE uint16), `data[2]` = parity_bit |
| `'SLAVE ADDRESS'` | `"SLAVE ADDRESS"` | `data[0..1]` = address(LE uint16), `data[2]` = parity_bit, `data[3]` = ack_bit |
| `'CONTROL'` | `"CONTROL"` | `data[0]` = control, `data[1]` = parity_bit, `data[2]` = ack_bit |
| `'DATA LENGTH'` | `"DATA LENGTH"` | `data[0]` = data_length, `data[1]` = parity_bit, `data[2]` = ack_bit |
| `'DATA'` | `"DATA"` | 连续的 `(byte, parity, ack, ss_le, es_le)` 五元组 |
| `'NAK'` | `"NAK"` | 无 data |

**关键难点**：Python `DATA` 类型的 pdata 是 `[(b, parity_bit, ack_bit, ss, es), ...]` 列表。C 版本需要将此序列化为二进制格式。建议格式：

```
DATA cmd 的 data 布局:
  uint16_t count;           // 数据字节个数 N
  struct {                  // 重复 N 次
      uint8_t  byte_val;
      uint8_t  parity_bit;
      uint8_t  ack_bit;
      uint64_t ss;          // little-endian
      uint64_t es;          // little-endian
  } entries[N];
```

#### 3.1.6 查找表（lists.py 移植）

需要将 Python `lists.py` 中的所有 IntEnum/IntFlag 转换为 C 查找表：

1. **HWAddresses** — 硬件地址枚举（~30 项），用于将 12-bit 地址映射为设备名
2. **FunctionIDs** — 逻辑功能 ID（~30 项），用于将功能字节映射为功能名
3. **CommCtrlOpcodes** — 通信控制操作码（~15 项）
4. **CDOpcodes** — CD 播放器操作码（~10 项）
5. **CDSlots** — CD 插槽标志位
6. **CDStateCodes** — CD 状态码标志位
7. **CDFlags** — CD 标志位
8. **CmdSwOpcodes** — 命令开关操作码（~10 项）
9. **AudioAmpOpcodes** — 音频功放操作码
10. **AudioAmpFlags** — 音频功放标志位
11. **TunerOpcodes** — 调谐器操作码
12. **TunerFlags** — 调谐器标志位
13. **TunerState** — 调谐器状态
14. **TunerModes** — 调谐器模式

**C 实现策略**：使用结构体数组 + 线性搜索函数：

```c
typedef struct {
    int value;
    const char *name;
} name_entry;

static const name_entry hw_addresses[] = {
    {0x110, "EMV"},
    {0x120, "AVX"},
    // ...
    {0xFFF, "BROADCAST"},
};
#define HW_ADDR_COUNT (sizeof(hw_addresses) / sizeof(hw_addresses[0]))

static const char *find_name(const name_entry *table, int count, int value) {
    for (int i = 0; i < count; i++)
        if (table[i].value == value) return table[i].name;
    return NULL;
}
```

#### 3.1.7 核心解码逻辑

1. 收到 `HEADER` → 保存 broadcast_bit，状态转 MASTER_ADDRESS
2. 收到 `MASTER ADDRESS` → 保存 master_addr，查表输出地址注释
3. 收到 `SLAVE ADDRESS` → 保存 slave_addr，查表输出地址注释
4. 收到 `CONTROL` → 保存 control
5. 收到 `DATA LENGTH` → 保存 data_length
6. 收到 `DATA` → 解析数据字节序列，根据 broadcast_bit 区分单播/广播：
   - **单播** (broadcast_bit=1): data_bytes[1]=from_function, data_bytes[2]=to_function, data_bytes[3:]=payload
   - **广播** (broadcast_bit=0): data_bytes[0]=from_function, data_bytes[1]=to_function, data_bytes[2:]=payload
7. 根据 from_function 和 to_function 分派到具体处理函数
8. 收到 `NAK` → 重置状态机

#### 3.1.8 函数分派机制

Python 使用 `getattr(self, f'pkt_from_{from:02x}_to_{to:02x}', None)` 动态查找。C 版本使用优先级匹配表：

```c
typedef int (*pkt_handler_fn)(struct srd_decoder_inst *, avclan_state *);

typedef struct {
    int from_func;   // -1 = 通配
    int to_func;     // -1 = 通配
    pkt_handler_fn handler;
} pkt_dispatch_entry;

// 优先级顺序: from_to > to_only > from_only > both > default
```

需要实现的处理函数：
- `pkt_comm_ctrl()` — 处理 COMM_CTRL (0x01) 和 COMMUNICATION (0x12)
- `pkt_from_25()` — 处理 CMD_SW
- `pkt_from_60()` — 处理 TUNER
- `pkt_from_62()` / `pkt_to_62()` — 处理 CD
- `pkt_from_63()` / `pkt_from_43()` — 处理 CD_CHANGER / CD_CHANGER2
- `pkt_74()` — 处理 AUDIO_AMP

#### 3.1.9 辅助函数

- `bcd2dec(b)` — BCD 转十进制
- `map_left_right(value, center, neg_tag, pos_tag)` — 左右/前后映射

#### 3.1.10 预估代码量

~800-1000 行（含查找表 ~200 行，状态机 ~200 行，分派处理 ~400 行，结构定义 ~200 行）

---

### 3.2 ethernet_c — Ethernet II (IEEE 802.3) 解码器

<!-- Updated: ethernet_c 依赖 4b5b_c（已有C实现），无阻塞问题。4b5b_c 输出格式已验证与spec一致。 -->

#### 3.2.1 元数据

| 属性 | 值 |
|------|-----|
| id | `ethernet_c` |
| name | `Ethernet(C)` |
| longname | `Ethernet II (IEEE 802.3) (C)` |
| desc | `Ethernet networking protocol (C implementation)` |
| license | `gplv2+` |
| inputs | `{"4b5b", NULL}` |
| outputs | `{"ethernet", NULL}` |
| tags | `{"Networking", "PC", NULL}` |

#### 3.2.2 Annotations（2 个）

```c
enum {
    ANN_HEADER = 0,   // Decoded header
    ANN_DATA,         // Decoded data
    NUM_ANN,
};
```

#### 3.2.3 Annotation Rows（2 行）

```c
static const int row_headers_classes[] = {ANN_HEADER};
static const int row_datas_classes[] = {ANN_DATA};
```

#### 3.2.4 Binary Output

```c
static const struct srd_decoder_binary ethernet_binary[] = {
    {0, "pcapng", "Wireshark packet capture (.pcapng)"},
};
```

#### 3.2.5 状态机

```
IDLE → ("J" then "K" = SSD) → WAIT_SFD → (DATA 0xD5 = SFD) → DST_MAC → (6 DATA bytes)
  → SRC_MAC → (6 DATA bytes) → ETH_TYPE → (2 DATA bytes) → PAYLOAD → ("T" = ESD)
  → IDLE

任何时刻收到 "I"/"S"/"Q"/"H" → 重置为 IDLE
```

**状态说明**：
- **IDLE** — 等待 JK 序列（Start of Stream Delimiter）
- **WAIT_SFD** — 已收到 JK，等待 SFD 字节 0xD5
- **DST_MAC** — 累积 6 字节目的 MAC
- **SRC_MAC** — 累积 6 字节源 MAC
- **ETH_TYPE** — 累积 2 字节 EtherType
- **PAYLOAD** — 累积 payload 数据直到收到 "T"

#### 3.2.6 recv_proto 协议映射

Python `4b5b` 解码器发送 `(value, is_control_symbol)` 元组。C 版本 `4b5b_c` 通过 `c_decoder_put_python()` 发送控制符号短名称或 `"DATA"` 命令：

<!-- Updated: 已验证 4b5b_c.c 实际输出格式。4b5b_c 使用 ctrl_short[] 数组中的短名称作为 c_decoder_put_python() 的 cmd 参数（第241行），与spec描述一致。控制符号映射：symbol 24→"J", 17→"K", 13→"T", 7→"R", 0→"Q", 4→"H", 6→"L", 25→"S", 31→"I"。数据字节使用 cmd="DATA"，data[0]=字节值。 -->

| 4b5b_c cmd 字符串 | data 格式 | 说明 |
|-------------------|----------|------|
| `"J"` | 无 data | J 控制符号（Start of Stream 的一部分，JK 对） |
| `"K"` | 无 data | K 控制符号（Start of Stream 的一部分，JK 对） |
| `"T"` | 无 data | T 控制符号（End of Stream delimiter，终止帧） |
| `"R"` | 无 data | R 控制符号（End of Stream delimiter 的一部分，TR 对；也用于 RESET） |
| `"Q"` | 无 data | Q 控制符号（Quiet） |
| `"H"` | 无 data | H 控制符号（Halt） |
| `"L"` | 无 data | L 控制符号 |
| `"S"` | 无 data | SET 控制符号（空闲状态指示） |
| `"I"` | 无 data | IDLE 控制符号（空闲状态指示） |
| `"DATA"` | `data[0]` = 字节值 | 4b5b 解码的数据字节 |

**关键说明**：
- 4b5b_c 输出的控制符号 cmd 是 `ctrl_short[]` 数组中的短名称（"J", "K", "T", "R", "Q", "H", "L", "S", "I"），而非语义化的 "START"/"TERMINATE"/"RESET"
- 帧开始由 **JK 序列**（先收到 "J"，再收到 "K"）标识，即 100BASE-TX 的 Start of Stream Delimiter (SSD)
- 帧结束由 **T 符号** 标识（T 后通常跟 R，形成 End of Stream Delimiter / ESD）
- 空闲状态由 "I" (IDLE) 或 "S" (SET) 符号标识
- "DATA" 命令携带 1 字节数据（两个 4b5b nibble 合并后的完整字节）

#### 3.2.7 核心解码逻辑

1. 收到 `"J"` → 标记 jk_seen_j = true，记录 frame_start
2. 收到 `"K"` → 标记 jk_seen_k = true
   - 若 jk_seen_j == true：JK 序列完整，切换到 WAIT_SFD 状态
   - 否则：孤立 K 符号，重置状态
3. 收到 `"DATA"` → 追加字节到 buffer
   - IDLE 状态：忽略数据（未收到 JK 序列）
   - WAIT_SFD 状态：检测 SFD (0xD5)
     - 若 byte == 0xD5：输出 Preamble 注释，输出 SFD 注释，切换到 DST_MAC
     - 否则：非 SFD 字节，可能是 preamble 数据，继续等待
   - DST_MAC 状态：累积 6 字节
     - 输出 Destination MAC 注释（含广播检测）
     - 切换到 SRC_MAC
   - SRC_MAC 状态：累积 6 字节
     - 输出 Source MAC 注释
     - 切换到 ETH_TYPE
   - ETH_TYPE 状态：累积 2 字节
     - 查表输出 EtherType 注释
     - 切换到 PAYLOAD
   - PAYLOAD 状态：逐字节累积数据
4. 收到 `"T"` →
   - 验证 FCS (CRC32)
   - 输出 FCS 注释
   - 生成 pcapng 二进制输出
   - 向上层解码器发送 payload（不含 FCS）
   - 重置状态为 IDLE
5. 收到 `"R"` →
   - 若当前在帧接收中（非 IDLE）：作为 ESD 的一部分（T+R），已完成帧处理
   - 重置状态为 IDLE
6. 收到 `"I"` / `"S"` / `"Q"` / `"H"` / `"L"` →
   - 空闲/暂停/错误符号，重置状态为 IDLE

#### 3.2.8 EtherType 查找表

从 `ethernet/dicts.py` 移植，包含 ~40 个 EtherType 映射：

```c
typedef struct {
    uint16_t type;
    const char *long_name;
    const char *short_name;
} ethertype_entry;

static const ethertype_entry ethertype_table[] = {
    {0x0800, "Internet Protocol Version 4", "IPv4"},
    {0x0806, "Address Resolution Protocol", "ARP"},
    // ... ~40 项
};
```

#### 3.2.9 CRC32 校验

需要实现 CRC32 计算。Python 使用 `zlib.crc32(frame) == 0x2144DF1C` 验证 FCS。C 版本需要实现 CRC32 查表法：

```c
static uint32_t crc32_table[256];
static int crc32_initialized = 0;

static void crc32_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
        crc32_table[i] = crc;
    }
    crc32_initialized = 1;
}

static uint32_t crc32_calc(const uint8_t *buf, int len) {
    if (!crc32_initialized) crc32_init();
    uint32_t crc = 0xFFFFFFFF;
    for (int i = 0; i < len; i++)
        crc = (crc >> 8) ^ crc32_table[(crc ^ buf[i]) & 0xFF];
    return crc ^ 0xFFFFFFFF;
}
```

#### 3.2.10 pcapng 二进制输出

Python 版本生成 pcapng 格式的二进制输出。C 版本需要：
1. 在 `start()` 中输出 Section Header Block 和 Interface Description Block
2. 在收到 "T" 控制符号（ESD）时输出 Simple Packet Block

#### 3.2.11 向上层传递 payload

在收到 "T" 控制符号（ESD）时，通过 `c_decoder_put_python()` 将 payload（不含 FCS 的 4 字节）和 blocks 信息传递给上层：

```c
// 发送 payload 给上层解码器
// cmd = "PAYLOAD"
// data 布局:
//   uint16_t payload_len;
//   uint8_t  payload[payload_len];
//   uint16_t block_count;
//   struct { uint64_t ss; uint64_t es; } blocks[block_count];
```

#### 3.2.12 预估代码量

~500-600 行（含 EtherType 表 ~80 行，CRC32 ~40 行，状态机 ~200 行，pcapng ~60 行，结构定义 ~120 行）

---

### 3.3 arp_c — ARP 协议解码器

#### 3.3.1 元数据

| 属性 | 值 |
|------|-----|
| id | `arp_c` |
| name | `ARP(C)` |
| longname | `Address Resolution Protocol (C)` |
| desc | `ARP (C implementation)` |
| license | `gplv2+` |
| inputs | `{"ethernet", NULL}` |
| outputs | `{NULL}` (无输出) 或 `{"", NULL}` |
| tags | `{"Networking", "PC", NULL}` |

#### 3.3.2 Annotations（2 个）

```c
enum {
    ANN_DATA = 0,   // Decoded data
    ANN_MSG,        // Message
    NUM_ANN,
};
```

#### 3.3.3 Annotation Rows（2 行）

```c
static const int row_datas_classes[] = {ANN_DATA};
static const int row_msgs_classes[] = {ANN_MSG};
```

#### 3.3.4 recv_proto 协议映射

ARP 解码器接收 Ethernet 解码器发送的 payload 数据：

| C cmd 字符串 | data 格式 |
|-------------|----------|
| `"PAYLOAD"` | 见下方结构 |

```
PAYLOAD data 布局:
  uint16_t payload_len;       // payload 字节数
  uint8_t  payload[payload_len];
  uint16_t block_count;       // block 条目数
  struct { uint64_t ss; uint64_t es; } blocks[block_count];
```

#### 3.3.5 核心解码逻辑

ARP 解码器是一次性解析——收到完整 payload 后立即解析所有字段：

1. 解析 ARP 包头：`struct.unpack('>2H2BH6s4s6s4s', payload[:28])`
   - htype (2 bytes) — Hardware Type
   - ptype (2 bytes) — Protocol Type
   - hlen (1 byte) — Hardware Address Length
   - plen (1 byte) — Protocol Address Length
   - oper (2 bytes) — Operation
   - sha (6 bytes) — Sender Hardware Address (MAC)
   - spa (4 bytes) — Sender Protocol Address (IP)
   - tha (6 bytes) — Target Hardware Address (MAC)
   - tpa (4 bytes) — Target Protocol Address (IP)

2. 逐字段输出注释，使用 blocks 数组中的 ss/es 定位

3. 输出消息注释（ANN_MSG）：
   - Request + spa==tpa → "ARP Announcement for {spa} ({sha})"
   - Request + spa=="0.0.0.0" → "ARP Probe for {tpa} ({sha})"
   - Request → "Who has {tpa}? Tell {spa} ({sha})"
   - Reply → "{spa} is at {sha}"

#### 3.3.6 EtherType 查找表

ARP 解码器也使用 EtherType 查找表（用于 ptype 字段），可复用 ethernet_c 中的表，或在 arp_c.c 中内联一份精简版。

#### 3.3.7 预估代码量

~300-400 行（含 EtherType 表 ~80 行，解析逻辑 ~150 行，结构定义 ~70 行）

---

### 3.4 ipv4_c — IPv4 协议解码器

#### 3.4.1 元数据

| 属性 | 值 |
|------|-----|
| id | `ipv4_c` |
| name | `IPv4(C)` |
| longname | `Internet Protocol Version 4 (C)` |
| desc | `IPv4 (C implementation)` |
| license | `gplv2+` |
| inputs | `{"ethernet", NULL}` |
| outputs | `{"ipv4", NULL}` |
| tags | `{"Networking", "PC", NULL}` |

#### 3.4.2 Annotations（2 个）

```c
enum {
    ANN_HEADER = 0,   // Decoded header
    ANN_DATA,         // Decoded data
    NUM_ANN,
};
```

#### 3.4.3 Annotation Rows（2 行）

```c
static const int row_headers_classes[] = {ANN_HEADER};
static const int row_datas_classes[] = {ANN_DATA};
```

#### 3.4.4 recv_proto 协议映射

与 ARP 相同，接收 Ethernet 的 `"PAYLOAD"` cmd。

#### 3.4.5 核心解码逻辑

1. 获取 IHL：`(payload[0] & 0x0F) * 4`，若 != 20 则跳过（暂不支持可选字段）

<!-- Updated: "暂不支持可选字段"不是C框架限制，而是实现简化。Python版本也不处理IP可选字段。如需支持，可在后续版本中添加IHL>20时的选项解析逻辑。 -->
2. 解析 IP 包头字段：
   - Version + Header Length (byte 0)
   - DSCP + ECN (byte 1)
   - Total Length (bytes 2-3)
   - Identification (bytes 4-5)
   - Flags + Fragment Offset (bytes 6-7)
   - TTL (byte 8)
   - Protocol (byte 9)
   - Header Checksum (bytes 10-11)
   - Source IP (bytes 12-15)
   - Destination IP (bytes 16-19)

3. 校验 Header Checksum：
```c
uint16_t ip_checksum(const uint8_t *header, int len) {
    uint32_t sum = 0;
    for (int i = 0; i < len; i += 2)
        sum += (header[i] << 8) | header[i + 1];
    sum = (sum + (sum >> 16)) & 0xFFFF;
    return (sum == 0xFFFF) ? 1 : 0;
}
```

4. 输出 IP Payload 数据注释（bytes 20+）

5. 向上层解码器发送 payload：
```c
// cmd = "IP_PAYLOAD"
// data 布局:
//   uint16_t payload_len;
//   uint8_t  payload[payload_len];
//   uint16_t block_count;
//   struct { uint64_t ss; uint64_t es; } blocks[block_count];
//   uint8_t  src_ip[4];
//   uint8_t  dst_ip[4];
```

#### 3.4.6 IP Protocol 查找表

从 `ipv4/dicts.py` 移植，包含 ~100 个 IP 协议号映射：

```c
typedef struct {
    uint8_t protocol;
    const char *short_name;
    const char *long_name;
} ip_protocol_entry;
```

#### 3.4.7 预估代码量

~450-550 行（含 IP Protocol 表 ~200 行，解析逻辑 ~150 行，结构定义 ~100 行）

---

### 3.5 udp_c — UDP 协议解码器

#### 3.5.1 元数据

| 属性 | 值 |
|------|-----|
| id | `udp_c` |
| name | `UDP(C)` |
| longname | `User Datagram Protocol (C)` |
| desc | `UDP (C implementation)` |
| license | `gplv2+` |
| inputs | `{"ipv4", NULL}` |
| outputs | `{"udp", NULL}` |
| tags | `{"Networking", "PC", NULL}` |

#### 3.5.2 Options（1 个）

```c
static struct srd_decoder_option udp_options[] = {
    {"format", NULL, "Data format", NULL, NULL},
};
```

可选值：`"ascii"`, `"dec"`, `"hex"` (默认), `"oct"`, `"bin"`

#### 3.5.3 Annotations（2 个）

```c
enum {
    ANN_HEADER = 0,   // Decoded header
    ANN_DATA,         // Decoded data
    NUM_ANN,
};
```

#### 3.5.4 Annotation Rows（2 行）

```c
static const int row_headers_classes[] = {ANN_HEADER};
static const int row_datas_classes[] = {ANN_DATA};
```

#### 3.5.5 Binary Output

```c
static const struct srd_decoder_binary udp_binary[] = {
    {0, "raw", "Raw UDP payload"},
};
```

#### 3.5.6 recv_proto 协议映射

接收 IPv4 的 `"IP_PAYLOAD"` cmd：

```
IP_PAYLOAD data 布局:
  uint16_t payload_len;
  uint8_t  payload[payload_len];
  uint16_t block_count;
  struct { uint64_t ss; uint64_t es; } blocks[block_count];
  uint8_t  src_ip[4];
  uint8_t  dst_ip[4];
```

#### 3.5.7 核心解码逻辑

1. 解析 UDP 包头：`struct.unpack('>4H', payload[:8])`
   - Source Port (2 bytes)
   - Destination Port (2 bytes)
   - Length (2 bytes)
   - Checksum (2 bytes)

2. 校验 Checksum（使用 IPv4 伪头部）：
```c
// 伪头部: src_ip(4) + dst_ip(4) + 0x00 + 0x11(UDP) + udp_length(2)
uint32_t sum = 0;
// 加入伪头部
for (int i = 0; i < 4; i++) sum += (src_ip[i] << 8) | (i+1 < 4 ? src_ip[i+1] : 0);
// ... 类似处理 dst_ip, protocol, length
// 加入 UDP 数据
for (int i = 0; i < udp_len; i += 2) sum += (payload[i] << 8) | payload[i+1];
sum = (sum + (sum >> 16)) & 0xFFFF;
int checksum_ok = (sum == 0xFFFF);
```

3. 输出 Payload 数据注释，格式由 `format` 选项控制

4. 向上层发送 payload：
```c
// cmd = "UDP_PAYLOAD"
// data 布局:
//   uint16_t payload_len;
//   uint8_t  payload[payload_len];
//   uint16_t block_count;
//   struct { uint64_t ss; uint64_t es; } blocks[block_count];
```

5. 输出 binary 数据（raw UDP payload）

#### 3.5.8 预估代码量

~300-400 行（含校验和 ~60 行，格式化输出 ~80 行，解析逻辑 ~100 行，结构定义 ~60 行）

---

## 4. 跨解码器协议约定

### 4.1 协议数据传输格式

上层解码器之间通过 `c_decoder_put_python()` / `recv_proto()` 传递数据。统一约定如下：

#### 4.1.1 4b5b → Ethernet

<!-- Updated: 已验证 4b5b_c.c 的 c_decoder_put_python() 输出格式与下表一致。控制符号使用 ctrl_short[] 短名称（"J","K","T","R","Q","H","L","S","I"），数据使用 cmd="DATA" + data[0]=字节值。 -->

| cmd | data | 说明 |
|-----|------|------|
| `"J"` | 无 | J 控制符号（SSD 第一部分） |
| `"K"` | 无 | K 控制符号（SSD 第二部分，JK=Start of Stream） |
| `"T"` | 无 | T 控制符号（ESD 第一部分，终止帧） |
| `"R"` | 无 | R 控制符号（ESD 第二部分，TR=End of Stream） |
| `"Q"` | 无 | Q 控制符号（Quiet） |
| `"H"` | 无 | H 控制符号（Halt） |
| `"L"` | 无 | L 控制符号 |
| `"S"` | 无 | SET 控制符号（空闲） |
| `"I"` | 无 | IDLE 控制符号（空闲） |
| `"DATA"` | `data[0]` = 字节值 | 4b5b 解码的数据字节 |

#### 4.1.2 Ethernet → ARP / IPv4

| cmd | data | 说明 |
|-----|------|------|
| `"PAYLOAD"` | 见下方结构 | Ethernet payload（不含 FCS）+ blocks + 可选的 src/dst IP |

PAYLOAD data 布局：
```
Offset  Size      Field
0       2         payload_len (uint16_t, little-endian)
2       N         payload bytes (N = payload_len)
2+N     2         block_count (uint16_t, little-endian)
4+N     M*16      blocks (M = block_count, 每个 block: uint64_t ss + uint64_t es, LE)
```

#### 4.1.3 IPv4 → UDP

| cmd | data | 说明 |
|-----|------|------|
| `"IP_PAYLOAD"` | 见下方结构 | IPv4 payload + blocks + src_ip + dst_ip |

IP_PAYLOAD data 布局：
```
Offset  Size      Field
0       2         payload_len (uint16_t, LE)
2       N         payload bytes
2+N     2         block_count (uint16_t, LE)
4+N     M*16      blocks
4+N+M*16  4       src_ip[4]
8+N+M*16  4       dst_ip[4]
```

#### 4.1.4 UDP → 上层

| cmd | data | 说明 |
|-----|------|------|
| `"UDP_PAYLOAD"` | 见下方结构 | UDP payload + blocks |

UDP_PAYLOAD data 布局：
```
Offset  Size      Field
0       2         payload_len (uint16_t, LE)
2       N         payload bytes
2+N     2         block_count (uint16_t, LE)
4+N     M*16      blocks
```

### 4.2 iebus → avclan 协议约定

由于 iebus 目前仅有 Python 实现，avclan_c 的 recv_proto 需要定义与 Python iebus 输出对应的 cmd 映射：

| Python ptype | C cmd | data 格式 |
|-------------|-------|----------|
| `'HEADER'` | `"HEADER"` | `data[0]` = broadcast_bit |
| `'MASTER ADDRESS'` | `"MASTER ADDRESS"` | `data[0..1]` = addr (LE uint16), `data[2]` = parity |
| `'SLAVE ADDRESS'` | `"SLAVE ADDRESS"` | `data[0..1]` = addr (LE uint16), `data[2]` = parity, `data[3]` = ack |
| `'CONTROL'` | `"CONTROL"` | `data[0]` = control, `data[1]` = parity, `data[2]` = ack |
| `'DATA LENGTH'` | `"DATA LENGTH"` | `data[0]` = length, `data[1]` = parity, `data[2]` = ack |
| `'DATA'` | `"DATA"` | 见 3.1.5 节 |
| `'NAK'` | `"NAK"` | 无 data |

**重要提示**：Python iebus 解码器通过 `self.put(ss, es, self.out_python, (ptype, pdata))` 发送数据。当上层是 C 解码器时，libsigrokdecode 引擎需要将 Python tuple 转换为 `recv_proto()` 的 `(cmd, data, data_len)` 格式。这需要引擎层面的桥接支持。如果该桥接尚未实现，avclan_c 将无法与 Python iebus 配合工作。

<!-- Updated: 已确认该桥接未实现。avclan_c 被 iebus 依赖阻塞，除非先移植 iebus_c。本节定义的 cmd/data 格式仅在 iebus_c 移植后有效。 -->

---

## 5. CMakeLists.txt 修改

在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加：

```cmake
avclan_c
ethernet_c
arp_c
ipv4_c
udp_c
```

---

## 6. 关键代码片段

### 6.1 ethernet_c recv_proto 骨架

```c
static void ethernet_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ethernet_state *s = (ethernet_state *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    // 处理 J 控制符号（SSD 第一部分）
    if (strcmp(cmd, "J") == 0) {
        s->jk_seen_j = 1;
        s->frame_start = start_sample;
        return;
    }

    // 处理 K 控制符号（SSD 第二部分，JK=Start of Stream）
    if (strcmp(cmd, "K") == 0) {
        if (s->jk_seen_j) {
            // JK 序列完整，进入 WAIT_SFD 状态
            s->state = ETH_WAIT_SFD;
        } else {
            // 孤立 K 符号，重置
            ethernet_reset_vars(s);
        }
        s->jk_seen_k = 1;
        return;
    }

    // 处理 T 控制符号（ESD 第一部分，终止帧）
    if (strcmp(cmd, "T") == 0) {
        if (s->state != ETH_IDLE) {
            // 验证 FCS
            uint32_t crc = crc32_calc(s->frame_data, s->frame_len);
            int fcs_ok = (crc == 0x2144DF1C);

            // 输出 FCS 注释
            // ...

            // 输出 pcapng
            // ...

            // 发送 payload 给上层
            if (s->out_python >= 0) {
                // 构造 PAYLOAD data
                uint16_t plen = (uint16_t)(s->payload_len - 4); // 去掉 FCS
                int data_size = 2 + plen + 2 + s->block_count * 16;
                uint8_t *buf = g_malloc(data_size);
                memcpy(buf, &plen, 2);
                memcpy(buf + 2, s->payload, plen);
                uint16_t bc = s->block_count;
                memcpy(buf + 2 + plen, &bc, 2);
                memcpy(buf + 4 + plen, s->blocks, bc * 16);
                c_decoder_put_python(di, s->payload_start_ss, s->fcs_start_ss,
                                     s->out_python, "PAYLOAD", buf, data_size);
                g_free(buf);
            }
        }
        ethernet_reset_vars(s);
        return;
    }

    // 处理 R 控制符号（ESD 第二部分，TR=End of Stream）
    if (strcmp(cmd, "R") == 0) {
        // T 已处理帧结束，R 作为 ESD 的后续符号，确保状态重置
        ethernet_reset_vars(s);
        return;
    }

    // 处理空闲/暂停/错误控制符号
    if (strcmp(cmd, "I") == 0 || strcmp(cmd, "S") == 0 ||
        strcmp(cmd, "Q") == 0 || strcmp(cmd, "H") == 0 ||
        strcmp(cmd, "L") == 0) {
        ethernet_reset_vars(s);
        return;
    }

    // 处理数据字节
    if (strcmp(cmd, "DATA") == 0) {
        if (s->state == ETH_IDLE) return; // 未收到 JK 序列，忽略数据
        uint8_t byte_val = (data && data_len > 0) ? data[0] : 0;
        ethernet_handle_data_byte(di, s, byte_val, start_sample, end_sample);
    }
}
```

### 6.2 ipv4_c recv_proto 骨架

```c
static void ipv4_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ipv4_state *s = (ipv4_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "PAYLOAD") != 0 || !data || data_len < 4)
        return;

    // 解析 PAYLOAD data
    uint16_t payload_len;
    memcpy(&payload_len, data, 2);
    const uint8_t *payload = data + 2;
    uint16_t block_count;
    memcpy(&block_count, data + 2 + payload_len, 2);
    const uint8_t *blocks_data = data + 4 + payload_len;

    // 解析 block 的辅助宏
    #define BLOCK_SS(idx) ({ uint64_t v; memcpy(&v, blocks_data + (idx)*16, 8); v; })
    #define BLOCK_ES(idx) ({ uint64_t v; memcpy(&v, blocks_data + (idx)*16 + 8, 8); v; })

    // 检查 IHL
    int ihl = (payload[0] & 0x0F) * 4;
    if (ihl != 20) return; // 暂不支持可选字段

    if ((int)payload_len < 20 || (int)block_count < 20) return;

    // Version + Header Length
    s->ss_block = BLOCK_SS(0);
    s->es_block = BLOCK_ES(0);
    char t[64];
    snprintf(t, sizeof(t), "Version: 4 Header Length: %d bytes", ihl);
    C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_HEADER, t);

    // ... 解析其余字段 ...

    // 向上层发送 IP_PAYLOAD
    if (s->out_python >= 0 && payload_len > 20) {
        uint16_t ip_plen = payload_len - 20;
        int buf_size = 2 + ip_plen + 2 + (block_count - 20) * 16 + 4 + 4;
        uint8_t *buf = g_malloc(buf_size);
        int pos = 0;
        memcpy(buf + pos, &ip_plen, 2); pos += 2;
        memcpy(buf + pos, payload + 20, ip_plen); pos += ip_plen;
        uint16_t bc = block_count - 20;
        memcpy(buf + pos, &bc, 2); pos += 2;
        memcpy(buf + pos, blocks_data + 20 * 16, bc * 16); pos += bc * 16;
        memcpy(buf + pos, payload + 12, 4); pos += 4;  // src_ip
        memcpy(buf + pos, payload + 16, 4); pos += 4;  // dst_ip
        c_decoder_put_python(di, BLOCK_SS(20), BLOCK_ES(block_count-1),
                             s->out_python, "IP_PAYLOAD", buf, pos);
        g_free(buf);
    }
}
```

### 6.3 avclan_c 查找表片段

```c
typedef struct {
    int value;
    const char *name;
} name_entry;

static const name_entry hw_addresses[] = {
    {0x110, "EMV"},       {0x120, "AVX"},       {0x128, "DIN1_TV"},
    {0x140, "AVN"},       {0x144, "G_BOOK"},    {0x160, "AUDIO_HU1"},
    {0x178, "NAVI"},      {0x17C, "MONET"},     {0x17D, "TEL"},
    {0x180, "Rr_TV"},     {0x190, "AUDIO_HU2"}, {0x1A0, "DVD_P"},
    {0x1D6, "CLOCK"},     {0x1AC, "CAMERA_C"},  {0x1C0, "Rr_CONT"},
    {0x1C2, "TV_TUNER2"}, {0x1C4, "PANEL"},     {0x1C6, "GW"},
    {0x1C8, "FM_M_LCD"},  {0x1CC, "ST_WHEEL_CTRL"}, {0x1D8, "GW_TRIP"},
    {0x1EC, "BODY"},      {0x1F0, "RADIO_TUNER"}, {0x1F1, "XM"},
    {0x1F2, "SIRIUS"},    {0x1F4, "RSA"},       {0x1F6, "RSE"},
    {0x1FF, "GROUP_AUDIO"}, {0x230, "TV_TUNER"}, {0x240, "CD_CH2"},
    {0x250, "DVD_CH"},    {0x280, "CAMERA"},    {0x360, "CD_CH1"},
    {0x3A0, "MD_CH"},     {0x440, "DSP_AMP"},   {0x480, "AMP"},
    {0x530, "ETC"},       {0x5C8, "MAYDAY"},    {0xFFF, "BROADCAST"},
};
#define HW_ADDR_COUNT (sizeof(hw_addresses) / sizeof(hw_addresses[0]))

static const name_entry function_ids[] = {
    {0x01, "COMM_CTRL"},       {0x12, "COMMUNICATION"},   {0x21, "SW"},
    {0x23, "SW_NAME"},         {0x24, "SW_CONVERTING"},   {0x25, "CMD_SW"},
    {0x28, "BEEP_HU"},         {0x29, "BEEP_SPEAKERS"},   {0x34, "FRONT_PSNG_MONITOR"},
    {0x43, "CD_CHANGER2"},     {0x55, "BLUETOOTH_TEL"},   {0x56, "INFO_DRAWING"},
    {0x58, "NAV_ECU"},         {0x5C, "CAMERA"},          {0x5D, "CLIMATE_DRAWING"},
    {0x5E, "AUDIO_DRAWING"},   {0x5F, "TRIP_INFO_DRAWING"}, {0x60, "TUNER"},
    {0x61, "TAPE_DECK"},       {0x62, "CD"},              {0x63, "CD_CHANGER"},
    {0x74, "AUDIO_AMP"},       {0x80, "GPS"},             {0x85, "VOICE_CTRL"},
    {0xE0, "CLIMATE_CTRL_DEV"}, {0xE5, "TRIP_INFO"},
};
#define FUNC_ID_COUNT (sizeof(function_ids) / sizeof(function_ids[0]))

static const char *find_name(const name_entry *table, int count, int value) {
    for (int i = 0; i < count; i++)
        if (table[i].value == value) return table[i].name;
    return NULL;
}
```

---

## 7. 风险与注意事项

### 7.1 Python→C 跨语言协议桥接

**最大风险**：当前 `c_decoder_put_python()` 仅在 C→C 解码器之间传递数据。如果下层是 Python 解码器，其 `put()` 发送的 Python 对象（tuple/list）不会自动转换为 `recv_proto()` 的 `(cmd, data, data_len)` 格式。

<!-- Updated: 已通过代码验证确认此风险。type_decoder.c 第557-578行中，Python解码器的 SRD_OUTPUT_PYTHON 输出仅调用 PyObject_CallMethod(next_di->py_inst, "decode", ...)，不检查 next_di->is_c_inst，不会调用 C 解码器的 recv_proto()。此为引擎层面限制，非本spec可解决。 -->

- `ethernet_c` 依赖 `4b5b` 输入 → `4b5b_c` 已存在（C 实现），✅ 无问题
- `arp_c` / `ipv4_c` 依赖 `ethernet` 输入 → `ethernet_c` 将是 C 实现，✅ 无问题
- `udp_c` 依赖 `ipv4` 输入 → `ipv4_c` 将是 C 实现，✅ 无问题
- `avclan_c` 依赖 `iebus` 输入 → `iebus` 仅有 Python 实现，❌ **阻塞 — 需先移植 iebus_c**

<!-- Updated: avclan_c 状态从"需要桥接"升级为"阻塞"。C解码器依赖规则：C解码器只能依赖已有C实现的底层解码器。iebus 无C实现，因此 avclan_c 无法在本批次中完成。 -->

### 7.2 blocks 数据精度

Python 版本中每个 payload 字节都有精确的 `{ss, es}` 采样位置。C 版本通过序列化传递这些位置信息，需要确保：
- `uint64_t` 的字节序一致（使用 little-endian）
- block_count 与实际 payload 长度匹配

### 7.3 内存管理

- `recv_proto()` 中动态分配的缓冲区（用于构造 `c_decoder_put_python` 的 data）必须在调用后释放
- 状态结构体中的动态数组（如 ethernet 的 payload/blocks）需要在 `destroy()` 中释放

### 7.4 CRC32 性能

ethernet_c 的 CRC32 校验使用查表法，初始化开销可忽略，运行时每字节仅需一次查表+异或操作。
