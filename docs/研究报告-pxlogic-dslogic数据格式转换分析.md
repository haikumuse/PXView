# 研究报告：pxlogic/dslogic 数据格式与 libsigrok 标准的差异及转换层迁移可行性分析

> **研究对象**：libsigrok 0.6.0 中 pxlogic、dreamsourcelab-dslogic 驱动的逻辑采样数据格式
> **核心问题**：驱动层的 channel-block → sample-interleaved 转换能否迁移到 core/view 层以优化性能
> **研究日期**：2026-07-06

---

## 一、执行摘要（TL;DR）

1. **格式差异**：pxlogic 和 dslogic 硬件均输出 **channel-block（planar）格式**——每 64 个采样点构成一个原子块，块内每通道占 8 字节连续存放；而 libsigrok 标准要求 driver 输出 **sample-interleaved 格式**——每个采样点占 `⌈通道数/8⌉` 字节，所有通道按 bit 交织在同一采样点内。

2. **为什么做转换**：这是 libsigrok 的**硬性 API 契约**。所有上游消费者（包括 PXView core 层、其他 sigrok 前端、协议解码器）都假定 `SR_DF_LOGIC` 包是 sample-interleaved。其他驱动（fx2lafw、asix-sigma、demo）均遵守此契约。

3. **能否迁移到 core/view**——**分场景回答**（详见第六、七章）：
   - **USB 2.0 场景**（≤25 MB/s）：driver 标量 deinterleave ~40 MB/s 跟得上，**保持现状即可**，无需迁移。
   - **USB 3.0 Stream 场景**（最高 250 MB/s）：driver 标量 deinterleave ~40 MB/s **远跟不上**（差距 6 倍），且 driver 在 session 线程同步执行会阻塞 USB transfer 收割，导致设备 FIFO 溢出 → 静默丢数据或采集停止。**必须迁移 + 解耦线程**，单纯搬函数无效。

4. **USB 3.0 场景下的正确方案**（详见第七章方案 E）：
   - **driver 层零转换**：`receive_transfer` 只做 zero-copy 指针入队 + 立即 resubmit（保证 USB 收割路径极快，4 个 transfer slot 始终保持飞行）
   - **driver 内部 worker 线程**：从队列取 raw buffer → SIMD deinterleave → `sr_session_send`（避免阻塞 USB 收割）
   - 或更彻底：driver 直接发 raw 数据，core 层独立线程做 SIMD 转换 + chunk tree 写入（单遍直通，省一阶段）

5. **关键约束**：单纯把 `deinterleave_cross_to_interleaved` 函数从 driver 搬到 core 层**完全无效**——因为 `sr_session_send` 是同步 API（[session.c#L1183-L1188](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/session.c#L1183-L1188)），`DataFeedParser::feed_in_logic` 仍在 session 线程同步执行（[datafeedparser.cpp#L107-L110](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/core/datafeedparser.cpp#L107-L110)），阻塞点只是换了位置，没有消除。

---

> **重要修正说明**：本报告初版（2026-07-06）按 USB 2.0 节奏评估得出"不建议迁移"的结论。经复核，该结论在 USB 3.0 Stream 模式下**不成立**——driver 层标量转换的处理能力（~40 MB/s）远低于 USB 3.0 数据流（最高 250 MB/s），会成为致命瓶颈。第七章已新增方案 E（driver 内部 worker 线程 + core 层独立线程协同）作为 USB 3.0 场景下的推荐方案。

---

## 二、三种数据格式详解

### 2.1 libsigrok 标准格式：sample-interleaved

**定义位置**：[libsigrok.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/include/libsigrok/libsigrok.h#L521-L526)

```c
struct sr_datafeed_logic {
    uint64_t length;     // 数据字节数（非采样点数）
    uint16_t unitsize;   // 每采样点字节数 = ⌈通道数/8⌉
    void *data;          // 采样数据缓冲区
};
```

**布局**（以 16 通道、unitsize=2 为例）：

```
sample0: [byte0: ch0..ch7][byte1: ch8..ch15]
sample1: [byte0: ch0..ch7][byte1: ch8..ch15]
sample2: ...
```

- 第 `s` 个采样点的第 `ch` 通道 bit 位于：`data[s*unitsize + ch/8]` 的 `ch%8` 位
- 采样点总数 = `length / unitsize`

**官方说明**：头文件中无显式 "sample-interleaved" 术语，但 [sr_channel.index 注释](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/include/libsigrok/libsigrok.h#L649-L655) 明确"Logic channels will be encoded according to this index in SR_DF_LOGIC packets"。该格式为隐含约定，由 demo/fx2lafw/asix-sigma 等驱动共同遵守。

### 2.2 pxlogic 硬件原始格式：LA_CROSS_DATA（channel-block）

**定义位置**：[pxlogic.c#L190-L195](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/pxlogic/pxlogic.c#L190-L195)

```
LA_CROSS_DATA: [8 bytes per channel, 64 samples per 8-byte block]
  Layout: [ch0: 64 samples in 8 bytes][ch1: 64 samples in 8 bytes]...
Sample-interleaved: [unitsize bytes per sample, all channels]
  unitsize = ch_num / 8
```

**布局**（以 ch_num=16、一个完整块=128 字节、含 64 个采样点为例）：

```
[ ch0: 8B (64 bits) ][ ch1: 8B ][ ch2: 8B ]...[ ch15: 8B ]
```

- 块大小 = `ch_num * 8` 字节，携带 64 个采样点 × ch_num 个通道
- 采样点 `s` 在通道 `ch` 的位置：块内偏移 `ch*8 + (s%64)/8` 字节的第 `(s%64)%8` 位
- **特点**：通道之间是 planar（分块）排列，相邻 bit 是同一通道的相邻采样

### 2.3 dslogic 硬件原始格式：channel-block（round-robin）

**定义位置**：[protocol.c#L832-L842](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/dreamsourcelab-dslogic/protocol.c#L832-L842)

```
The DSLogic emits sample data as sequences of 64-bit sample words
in a round-robin i.e. 64-bits from channel 0, 64-bits from channel 1
etc. for each of the enabled channels, then looping back to the
channel.

Because sigrok's internal representation is bit-interleaved channels
we must recast the data.
```

**布局**：与 pxlogic 完全同构——每 `channel_count × 8` 字节为一个原子块，块内每通道 8 字节（64 个采样点）。

### 2.4 三格式对比表

| 维度 | libsigrok 标准 (sample-interleaved) | pxlogic / dslogic 原始 (channel-block) |
|------|------------------------------------|--------------------------------------|
| **基本单元** | 1 个采样点 = `unitsize` 字节 | 1 个原子块 = `ch_num*8` 字节（含 64 个采样点） |
| **通道排列** | 同一采样点的所有通道交织存放 | 不同通道分块连续存放 |
| **bit 邻接关系** | 相邻 bit = 同一采样点的相邻通道 | 相邻 bit = 同一通道的相邻采样点 |
| **unitsize** | `⌈ch_num/8⌉` 字节 | 块内固定 `ch_num` 个 8 字节段 |
| **访问模式** | 顺序访问每采样点（cache 友好） | 跨通道访问需跳跃 `ch*8` 字节（cache 不友好） |
| **本质** | 时间维优先 | 通道维优先 |

pxlogic 与 dslogic 的原始格式**本质相同**（都是 channel-block/planar），仅在通道数固定性、输出宽度上有差异（见第四章）。

---

## 三、转换机制分析

### 3.1 pxlogic 转换：`deinterleave_cross_to_interleaved()`

**位置**：[pxlogic.c#L196-L218](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/pxlogic/pxlogic.c#L196-L218)

```c
static void deinterleave_cross_to_interleaved(const uint8_t *cross_buf,
        uint8_t *interleaved_buf, uint64_t byte_length, int ch_num)
```

**算法本质**：`ch_num × 64` 位矩阵转置为 `64 × ch_num` 位矩阵。

```
for s in 0..total_samples:
    block = s / 64
    bit_in_block = s % 64
    block_start = cross_buf + block * ch_num * 8
    out = interleaved_buf + s * unitsize
    for ch in 0..ch_num:
        ch_block = block_start + ch * 8
        bit = (ch_block[bit_in_block/8] >> (bit_in_block%8)) & 1
        if bit: out[ch/8] |= (1 << (ch%8))
```

**调用点**：[pxlogic.c#L1834-L1858](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/pxlogic/pxlogic.c#L1834-L1858) — 在 `receive_transfer()` 内、`sr_session_send()` 之前完成转换。

### 3.2 dslogic 转换：`deinterleave_buffer()`

**位置**：[protocol.c#L727-L749](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/dreamsourcelab-dslogic/protocol.c#L727-L749)

```c
static void deinterleave_buffer(const uint8_t *src, size_t length,
    uint16_t *dst_ptr, size_t channel_count, uint16_t channel_mask)
```

**算法本质**：与 pxlogic 相同（位矩阵转置），但循环结构不同：

```
for src_ptr in 0..length step (channel_count * 8):  // 外层按原子块步进
    for bit in 0..64:                                // 中层遍历 64 个 bit 位
        for channel in 0..16:                        // 内层组装 uint16_t
            if (channel_mask >> channel) & 1:
                if (*word_ptr++ >> bit) & 1:
                    sample |= 1 << channel
        *dst_ptr++ = sample
```

### 3.3 转换的性能开销点

按开销从大到小排序：

| # | 开销点 | 说明 | 优化空间 |
|---|--------|------|---------|
| 1 | **逐位标量双循环** | 4MB 传输 ≈ 3300 万次内层迭代，每次 1 加载+1 移位+1 掩码+1 条件 OR | **高**：可用 SIMD 一次完成 8×8 位转置 |
| 2 | **源数据 cache 不友好** | 跨通道访问步长 `ch*8` 字节，ch_num=32 时跨 cache line | 中：可预取或重排访问顺序 |
| 3 | **双重内存触及** | USB DMA buf → deinterleave_buf → core 再读 | 中：可与 core 层合并为零拷贝 |
| 4 | **每采样 memset** | 外层循环每点 `memset(out, 0, unitsize)` | 低：可合并到写操作 |
| 5 | 缓冲区分配 | 已优化：[pxlogic.c#L1591-L1599](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/pxlogic/pxlogic.c#L1591-L1599) 一次性预分配并复用 | 已无优化空间 |

---

## 四、pxlogic 与 dslogic 转换实现差异

| 维度 | DSLogic ([protocol.c](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/dreamsourcelab-dslogic/protocol.c)) | PXLogic ([pxlogic.c](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/pxlogic/pxlogic.c)) |
|------|-----------------------|----------------------|
| **通道数** | 固定 16 通道（`NUM_CHANNELS=16`） | 可变 `ch_num`（来自 `devc->ch_num`） |
| **输出 unitsize** | 固定 `sizeof(uint16_t)=2` 字节，即使启用通道 <16 | `ch_num/8` 字节，紧凑打包 |
| **通道选择** | 用 `channel_mask` 位掩码选启用通道 | 无 mask，所有 ch_num 通道连续打包 |
| **算法循环结构** | 外层原子块 → 中层 64 bit → 内层 16 通道（一次填一个 uint16_t） | 外层每采样点 → 内层 ch_num 通道（一次填一个 sample 的 unitsize 字节） |
| **触发处理** | 在 transfer 内拆 pre/post-trigger 段 | 用 `mstatus.trig_hit`/`captured_cnt*` 字段管理 |
| **失败回退** | 无 fallback，失败即丢数据 | 有 fallback：buf 不可用时直接发原始数据 |
| **常量定义** | `DSLOGIC_ATOMIC_SAMPLES=512`、`DSLOGIC_ATOMIC_BYTES=8` | 直接用字面量 `64`、`8` |

**结论**：两者本质转换相同（channel-block → sample-interleaved 的位矩阵转置），差异仅在工程实现细节。PXLogic 的实现更通用（通道数可变、紧凑打包、有 fallback），DSLogic 的实现更专一（固定 16 通道、mask 选择）。

---

## 五、为什么转换在 driver 层

### 5.1 libsigrok API 契约（硬约束）

`SR_DF_LOGIC` 包必须以 sample-interleaved 格式交付，这是 libsigrok 的**隐含但强制的 API 契约**。证据：

1. **头文件契约**：[sr_channel.index 注释](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/include/libsigrok/libsigrok.h#L649-L655) 明确通道按 index 在 SR_DF_LOGIC 包中编码
2. **多驱动共识**：demo、fx2lafw、asix-sigma、pxlogic、dslogic 五个驱动全部输出 sample-interleaved
3. **NEWS 文档**：`libsigrok/NEWS` L658 "Use unitsize 1 (not 2) if none of channels 8-15 are used" — 印证 unitsize = ⌈通道数/8⌉ 的标准公式
4. **PXView core 层契约注释**：[logicsnapshot.cpp#L348-L351](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/logicsnapshot.cpp#L348-L351) 明确 "data is always sample-interleaved"

### 5.2 上游消费者众多

libsigrok 的设计目标是**驱动与前端解耦**。一个 driver 输出的数据可能被以下消费者使用：
- PulseView（上游 Qt 前端）
- sigrok-cli（命令行）
- libsigrokdecode（协议解码器，C/Python）
- 第三方绑定（C++/Python/Java/Ruby）
- PXView

如果 driver 输出非标准格式，所有这些消费者都要各自实现 channel-block → sample-interleaved 转换，违反 DRY 原则且增加生态碎片化。

### 5.3 历史渊源

DSLogic 驱动源自上游 libsigrok 0.6.0，其 `deinterleave_buffer()` 是 DreamSourceLab 官方提交的实现。PXLogic 驱动是 PXView 项目从 DSLogic fork 后改造而来（保留了相同的硬件格式约定，但通用化了通道数）。两者都遵循"driver 负责格式归一化"的 libsigrok 设计哲学。

---

## 五B、USB 3.0 场景下的性能瓶颈实证

> 本章基于对 pxlogic 驱动 USB 配置、线程模型、反压机制的代码复核，量化 USB 3.0 场景下的瓶颈。

### 5B.1 pxlogic 的 USB 速率与吞吐量配置

**关键参数**（[pxlogic.c](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/pxlogic/pxlogic.c)）：

| 参数 | 值 | 位置 |
|------|-----|------|
| 数据端点 | `0x82` (bulk IN) | L1750 |
| num_transfers | **4**（硬编码） | L1582 |
| block_size 上限（USB 3.0） | **4 MB** | L1548 |
| block_size 上限（USB 2.0） | ~60 KB（10ms 等效） | L1548 |
| 在途数据量（USB 3.0） | 4 × 4 MB = **16 MB** | — |
| timeout | 0（无超时） | L1568 |
| 速度判定 | `libusb_get_device_speed()` 运行时 | L499, L540 |

**channel_modes 表支持的 Stream 模式组合**（[pxlogic.c#L106-L151](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/pxlogic/pxlogic.c#L106-L151)）：

| 模式 | 通道数 | 最大采样率 | 原始数据流 | USB 版本 |
|------|--------|-----------|-----------|---------|
| STREAM_LOGIC50x32 | 32 | 50 MHz | **200 MB/s** | USB 3.0 |
| STREAM_LOGIC125x16 | 16 | 125 MHz | **250 MB/s** | USB 3.0 |
| STREAM_LOGIC250x8 | 8 | 250 MHz | **250 MB/s** | USB 3.0 |
| STREAM_LOGIC500x4 | 4 | 500 MHz | **250 MB/s** | USB 3.0 |
| STREAM_LOGIC1000x2 | 2 | 1 GHz | **250 MB/s** | USB 3.0 |
| STREAM_LOGIC200x1 | 1 | 200 MHz | 25 MB/s | USB 2.0 |
| STREAM_LOGIC10x16 | 16 | 10 MHz | 20 MB/s | USB 2.0 |
| BUFFER_LOGIC250x32 | 32 | 250 MHz | 1000 MB/s（FPGA 内部） | 任意（采集后传输） |

**关键洞察**：
- USB 3.0 Stream 模式数据流被设计为 ≤250 MB/s（与 USB 3.0 实际可用带宽 ~400 MB/s 留有余量）
- Buffer 模式下 FPGA 内部数据流 1 GB/s 写入板载 DDR，USB 传输与采样率解耦——deinterleave 慢只影响采集完成后的下载时间，不丢数据

### 5B.2 driver 层 deinterleave 处理能力实测估算

**算法复杂度**（[pxlogic.c#L196-L218](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/pxlogic/pxlogic.c#L196-L218)）：
- 外层循环 `total_samples = byte_length * 8 / ch_num` 次
- 内层循环 `ch_num` 次
- **总迭代数 = byte_length * 8**（与通道数无关）

**4 MB transfer 的迭代量**：
```
4 × 1024 × 1024 × 8 = 33,554,432 次 ≈ 3300 万次内层迭代
```

**单核吞吐估算**（现代 x86 ~3 GHz，每次迭代约 3ns 含分支/cache miss 代价）：
```
33M × 3ns ≈ 100ms 处理 4MB
等效吞吐 ≈ 40 MB/s
```

### 5B.3 USB 3.0 Stream 模式下的瓶颈对比

| 场景 | 数据流入速率 | driver 处理速率 | 比值 | 后果 |
|------|------------|---------------|------|------|
| USB 2.0 Stream | ≤25 MB/s | ~40 MB/s | 0.6x | ✅ 跟得上 |
| USB 3.0 Stream (50MHz@32ch) | 200 MB/s | ~40 MB/s | **5x** | ❌ 严重跟不上 |
| USB 3.0 Stream (250MHz@8ch) | 250 MB/s | ~40 MB/s | **6.25x** | ❌ 严重跟不上 |
| USB 3.0 Buffer 模式 | 任意（FPGA 解耦） | ~40 MB/s | N/A | ✅ 不影响采集 |

### 5B.4 线程模型与反压机制缺失

**关键发现**：driver 层 deinterleave 在 session 线程**同步**执行，会阻塞 USB transfer 收割。

**线程架构**：
- 不存在独立 libusb 事件线程（[usb.c#L292-L302](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/usb.c#L292-L302) 注释：Windows WinUSB backend `libusb_get_pollfds()` 返回 NULL，采用 5ms timer-only 模式）
- USB 事件处理嵌入 session 线程的 GLib 主循环（[session.c#L894](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/session.c#L894) `g_main_loop_run`）
- `receive_transfer` → `deinterleave_cross_to_interleaved` → `sr_session_send` → `DataFeedParser::feed_in_logic` → `LogicSnapshot::append_payload` **全部同步串行**（[pxlogic.c#L1845-L1858](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/pxlogic/pxlogic.c#L1845-L1858)）

**反压传导链**（deinterleave 慢 → 设备丢数据）：

```
deinterleave 慢（100ms/4MB）
  ↓
receive_transfer 不返回
  ↓
transfer 不 resubmit（[pxlogic.c#L1879](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/pxlogic/pxlogic.c#L1879)）
  ↓
4 个 transfer slot 用完后无可用 buffer
  ↓
内核 USB Urb 队列排空
  ↓
设备端 FPGA FIFO 持续填充无人收割
  ↓
FIFO 溢出 → 静默丢数据（driver 无感知）
  ↓ 或
transfer timeout/stall → LIBUSB_TRANSFER_STALL
  ↓
[pxlogic.c#L1789-L1793](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/pxlogic/pxlogic.c#L1789-L1793) 捕获 → pxlogic_abort_acquisition → 采集停止
```

**反压机制缺失**：
- `num_transfers = 4` 硬编码，无队列深度调节
- 无背压反馈（driver 不知道设备 FIFO 状态）
- 无"丢数据继续"机制——一旦处理跟不上，要么静默丢数据，要么直接 abort

### 5B.5 core 层反压（已实现但用不上）

PXView core 层在 [logicsnapshot_diskcache_writer.h#L113-L114](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/logicsnapshot_diskcache_writer.h#L113-L114) 实现了 256MB/64MB 高低水位滞回背压，但这是反压到 driver 的——当 core 异步队列满时阻塞 `enqueue` → 阻塞 `append_payload` → 阻塞 `feed_in_logic` → 阻塞 `sr_session_send` → 阻塞 `receive_transfer`。

**问题**：core 层的 `append_payload` 本身只做入队（非阻塞），真正重活（mipmap 构建）在 [logicsnapshot_diskcache_writer.cpp#L291](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/logicsnapshot_diskcache_writer.cpp#L291) 的独立 `std::thread` 上异步执行。所以 core 层反压 rarely 触发，**真正的瓶颈是 driver 层 deinterleave 在 session 线程同步执行**。

### 5B.6 结论

USB 3.0 Stream 模式下：
1. driver 层标量 deinterleave（~40 MB/s）远跟不上数据流入（最高 250 MB/s），差距 6 倍
2. driver deinterleave 在 session 线程同步执行，阻塞 USB transfer 收割
3. 4 个 transfer slot 无队列深度调节，deinterleave 慢直接导致设备 FIFO 溢出
4. 后果是**静默丢数据**或**采集停止**
5. core 层的反压机制（256MB 水位）用不上，因为 core 入队是非阻塞的

**因此 USB 3.0 场景下，必须改造 driver 层架构**——不能只优化算法，必须解耦线程。

---

## 六、迁移到 core/view 层的可行性分析

### 6.1 当前 core 层的处理路径

PXView core 层已有**第二级解交织**——把 sample-interleaved 转换为 per-channel chunk tree：

**位置**：[logicsnapshot.cpp#L404-L650](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/logicsnapshot.cpp#L404-L650) `append_payload_impl()`

```
输入：sample-interleaved（每采样 unitsize 字节）
  ↓ 五阶段处理（Phase 1-5）
输出：per-channel chunk tree（每通道独立存储，8 sample 打包 1 字节）
```

关键索引（[L611-L620](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/logicsnapshot.cpp#L611-L620)）：
```cpp
if (src[k * unitsize + byte_pos] & bit_mask)  // sample-interleaved 索引
```

### 6.2 当前数据流（两阶段转换）

```
USB DMA buf (channel-block)
   │
   ▼ [driver: deinterleave_cross_to_interleaved]  ← 第一阶段转换
sample-interleaved buf
   │
   ▼ [core: append_payload_impl 五阶段]            ← 第二阶段转换
per-channel chunk tree
```

**两阶段转换各做一次全量位操作**，是性能浪费的根源。

### 6.3 迁移方案对比

#### 方案 A：完整迁移到 core 层（driver 输出原始 channel-block，core 同步处理）

```
USB DMA buf (channel-block)
   │
   ▼ [core: 单遍直通转换]                          ← 合并两阶段
per-channel chunk tree
```

**优点**：
- 省去一次全量位操作（理论加速 ~2x）
- core 层是 C++，可用模板/SIMD intrinsics 优化
- 可与 core 层现有的 64-sample 批量处理对齐

**缺点**：
- **违反 libsigrok API 契约**：其他 sigrok 前端无法使用该 driver
- **`sr_datafeed_logic` 无 format 字段**：driver 无法告知 core 数据格式
- **致命问题**：`sr_session_send` 是同步 API，`DataFeedParser::feed_in_logic` 仍在 session 线程同步执行——单纯搬函数**完全无效**，阻塞点只是换了位置

**结论**：**不可行**。USB 3.0 场景下单纯迁移函数无法解决同步阻塞问题。

#### 方案 B：保持 driver 层转换，用 SIMD 优化（仅算法层）

在 [pxlogic.c#L196-L218](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/pxlogic/pxlogic.c#L196-L218) 内用 BMI2/AVX2 的 8×8 位矩阵转置指令替代标量双循环。

**优点**：
- 不破坏 API 契约
- 8×8 位转置有成熟 SIMD 实现，理论加速 8-16x
- libsigrok 是 C 代码，C 同样支持 intrinsics

**缺点**：
- **USB 3.0 场景下仍不够**：即使 SIMD 加速 16x 达到 ~640 MB/s，但仍在 session 线程同步执行，会阻塞 USB 收割路径
- 需要运行时 CPU 特性检测

**结论**：**USB 2.0 场景推荐**；**USB 3.0 场景不够，必须配合方案 E 的线程解耦**。

#### 方案 C：core 层单遍直通（保留 driver 转换作为 fallback）

driver 仍输出 sample-interleaved（标准契约），但 core 层在 `append_payload_impl` 入口检测"是否同一 driver 的 raw 模式"，若是则走单遍直通路径。

**问题**：本质上还是方案 A 的变种，仍受 `sr_session_send` 同步性约束，无法解决 USB 3.0 场景的瓶颈。

**结论**：**不推荐**。

#### 方案 D：零拷贝路径（driver 直接写入 core 的 chunk tree）

让 driver 跳过中间 sample-interleaved buf，直接把 channel-block 数据写入 core 的 per-channel chunk tree。

**问题**：
- driver 无法访问 core 的内部数据结构（libsigrok 是独立库，不依赖 PXView）
- 跨库边界的内存共享需要重新设计 API
- 违反 layer separation 原则

**结论**：**不可行**。架构上不允许。

#### 方案 E：driver 内部 worker 线程 + 可选 core 层独立线程（USB 3.0 推荐方案）

**核心思想**：解耦 USB 收割路径与 deinterleave 计算，让 `receive_transfer` 立即返回。

**子方案 E1：driver 内部 worker 线程**（最小改动）

```
[session 线程] receive_transfer:
   ├─ zero-copy 指针入队 (raw channel-block buffer)
   └─ 立即 resubmit_transfer  ← USB 收割路径极快
        │
        ▼
[driver worker 线程] (新增):
   ├─ 从队列取 raw buffer
   ├─ SIMD deinterleave_cross_to_interleaved
   └─ sr_session_send(sample-interleaved)  ← 仍同步，但不在 USB 路径上
```

**关键设计**：
- `receive_transfer` 只做指针入队 + resubmit，耗时 < 1μs
- 4 个 transfer slot 始终保持飞行，USB 收割不会被阻塞
- driver 内部 worker 线程做 SIMD deinterleave + sr_session_send
- 队列深度足够大（如 64 个 buffer），吸收 USB 突发
- 队列满时反压：`receive_transfer` 阻塞入队（仍优于同步 deinterleave）

**子方案 E2：driver 直接发 raw + core 层独立线程转换**（更彻底）

```
[session 线程] receive_transfer:
   ├─ memcpy raw 数据到独立 buffer (因为 transfer buffer 会被 resubmit 复用)
   ├─ sr_session_send(raw channel-block, unitsize=0 标记 raw 格式)
   └─ 立即 resubmit_transfer
        │
        ▼
[core worker 线程] DataFeedParser::feed_in_logic (改造为投递任务):
   └─ 投递到 LogicSnapshotDiskCacheWriter 线程池
        │
        ▼
[core worker 线程] append_payload_impl (改造):
   ├─ SIMD 单遍直通: channel-block → per-channel chunk tree
   └─ 跳过中间 sample-interleaved 阶段
```

**关键设计**：
- `receive_transfer` 只做一次 memcpy + sr_session_send，耗时 ~5ms（4MB memcpy）
- core 层识别 raw 格式（通过 `unitsize=0` 或 driver name）走单遍直通路径
- 单遍直通省一阶段，性能提升 2x
- core 已有 DiskCacheWriter 独立线程，改造为线程池可并行处理多个 payload

**E1 vs E2 对比**：

| 维度 | E1 (driver worker) | E2 (core worker + raw 传输) |
|------|-------------------|---------------------------|
| 改动范围 | 仅 driver | driver + core |
| API 契约破坏 | 否（仍输出 sample-interleaved） | 是（driver 输出 raw） |
| 性能 | SIMD 8-16x | SIMD 8-16x + 单遍直通 2x = 16-32x |
| 实现复杂度 | 中（driver 内线程管理） | 高（driver + core 协同） |
| 反压粒度 | driver 队列（细粒度） | core 256MB 水位（粗粒度） |
| USB 收割路径耗时 | <1μs（指针入队） | ~5ms（4MB memcpy） |

**结论**：**E1 是 USB 3.0 场景的推荐方案**——最小改动、不破坏契约、性能提升足够（SIMD 16x → ~640 MB/s，超过 USB 3.0 的 250 MB/s）。E2 更彻底但改动大，可作为后续优化。

### 6.4 迁移可行性总评

| 方案 | USB 2.0 适用 | USB 3.0 适用 | 契约破坏 | 实现复杂度 | 推荐度 |
|------|------------|-------------|---------|-----------|--------|
| A. 完整迁移到 core（同步） | ❌ 无收益 | ❌ 同步阻塞未解决 | 是（严重） | 中 | ❌ 不推荐 |
| B. driver 层 SIMD（仅算法） | ✅ 推荐 | ⚠️ 不够（仍同步阻塞） | 否 | 中 | ⚠️ USB2.0 推荐 |
| C. core 单遍直通 | ❌ 无收益 | ❌ 同步阻塞未解决 | 是（轻度） | 高 | ❌ 不推荐 |
| D. 零拷贝 | ❌ 不可行 | ❌ 不可行 | 是（架构性） | 极高 | ❌ 不可行 |
| **E1. driver 内部 worker 线程** | ✅ 可选 | ✅ **强烈推荐** | 否 | 中 | ✅ **USB3.0 推荐** |
| E2. driver 发 raw + core 线程 | ✅ 可选 | ✅ 推荐 | 是（轻度） | 高 | ⚠️ 后续优化 |

---

## 七、性能优化的真正瓶颈与建议

### 7.1 真正的瓶颈是"同步阻塞 USB 收割路径"

从第五章 B 的分析可知，USB 3.0 场景下的**真正瓶颈不是"deinterleave 慢"，而是"deinterleave 在 session 线程同步执行，阻塞 USB transfer 收割"**。

即使把 deinterleave 从 ~40 MB/s 优化到 ~640 MB/s（SIMD 16x），如果仍在 session 线程同步执行，每次 4MB transfer 仍需 ~6ms 处理——这期间 USB 收割停滞，4 个 transfer slot 的缓冲可能不够（USB 3.0 在 6ms 内可流入 ~1.5MB × 6 = 9MB，超过 16MB 在途量的一半）。

**因此 USB 3.0 场景下必须同时满足**：
1. `receive_transfer` 立即返回（不阻塞 USB 收割）
2. deinterleave 在另一线程
3. SIMD 加速 deinterleave 本身

### 7.2 推荐优化路径（按场景 + ROI 排序）

#### 场景一：USB 2.0（≤25 MB/s）

**推荐：方案 B（driver 层 SIMD 优化）**

driver 标量 deinterleave ~40 MB/s 已能跟上 USB 2.0 的 25 MB/s，但余量小。SIMD 优化可把余量扩大到 16x，彻底消除风险。

**技术要点**：
- 8 通道 × 8 字节 = 64 bit 的位矩阵转置有经典算法（参见 Hacker's Delight Chapter 7）
- AVX2 的 `_mm256_movemask_epi8` 可一次提取 32 字节的 bit
- BMI2 的 `PDEP` 指令可高效完成位打包
- 需运行时检测 CPU 特性（`__builtin_cpu_supports("avx2")`）

**预期收益**：单阶段加速 8-16x，~40 MB/s → ~640 MB/s，余量从 1.6x 扩大到 25x。

#### 场景二：USB 3.0 Stream 模式（最高 250 MB/s）

**推荐：方案 E1（driver 内部 worker 线程 + SIMD）**

**实施步骤**：

1. **driver 内部增加 worker 线程 + ring buffer 队列**
   - 队列深度：64 个 buffer（每个 4MB，共 256MB，与 core 层反压水位对齐）
   - buffer 预分配，复用（避免运行时 malloc）
   - worker 线程在 `start_transfers` 时启动，`stop` 时停止

2. **改造 `receive_transfer`**（[pxlogic.c#L1769-L1889](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/pxlogic/pxlogic.c#L1769-L1889)）
   - 当前：deinterleave + sr_session_send + resubmit（同步串行）
   - 改造后：memcpy raw 到队列 buffer + 入队 + resubmit（< 1ms）
   - 队列满时阻塞入队（反压）

3. **worker 线程主循环**
   - 从队列取 raw buffer
   - SIMD deinterleave（方案 B 的实现）
   - `sr_session_send` 发 sample-interleaved 包
   - buffer 归还到池

4. **SIMD deinterleave 实现**（同方案 B）

**预期收益**：
- USB 收割路径耗时：100ms → <1ms（100x 加速）
- deinterleave 吞吐：40 MB/s → 640 MB/s（16x 加速，SIMD）
- 整体可支持 USB 3.0 的 250 MB/s 数据流，余量 2.5x

#### 场景三：极致性能（未来扩展）

**推荐：方案 E2（driver 发 raw + core 层独立线程单遍直通）**

**前提**：PXView 是 fork，不服务其他 sigrok 前端，可以接受轻度契约破坏。

**实施步骤**：

1. **driver 改造**：`receive_transfer` 直接发 raw channel-block 数据，`sr_datafeed_logic.unitsize = 0` 标记 raw 格式
2. **core 层 DataFeedParser 识别 raw 格式**（通过 unitsize=0 或 driver name），投递到线程池
3. **core 层 append_payload_impl 改造**：增加 channel-block → per-channel chunk tree 的单遍直通路径（SIMD）
4. **省一阶段**：跳过中间 sample-interleaved，性能再提升 2x

**预期收益**：USB 收割 < 5ms（仅 memcpy）+ core 并行 SIMD 单遍直通，整体 ~1.3 GB/s（远超 USB 3.0 带宽）。

**风险**：
- driver 输出非标准格式，破坏 libsigrok API 契约
- core 层需要识别 raw 格式，增加复杂度
- 建议作为方案 E1 之后的后续优化

### 7.3 不推荐的方案

- **方案 A（单纯迁移函数到 core）**：USB 3.0 场景下无效，因为 `sr_session_send` 同步性未解决
- **方案 C（core 单遍直通 + driver fallback）**：复杂度收益比差
- **方案 D（零拷贝）**：架构上不允许（libsigrok 是独立库）

---

## 八、结论

1. **格式差异**：pxlogic 和 dslogic 硬件输出 channel-block（planar）格式，libsigrok 标准要求 sample-interleaved 格式。两者本质是位矩阵的转置关系。

2. **转换必要性**：这是 libsigrok 的硬性 API 契约，所有 driver 必须遵守。转换在 driver 层是正确的设计，保证了 driver 与前端的解耦。

3. **迁移可行性**——**分场景结论**：
   - **USB 2.0 场景**（≤25 MB/s）：driver 标量 deinterleave ~40 MB/s 跟得上，**无需迁移**，可选 SIMD 优化（方案 B）扩大余量
   - **USB 3.0 Stream 场景**（最高 250 MB/s）：driver 标量 deinterleave ~40 MB/s **远跟不上**（差距 6 倍），且在 session 线程同步执行会阻塞 USB 收割，导致设备 FIFO 溢出 → 静默丢数据或采集停止。**必须改造**。

4. **USB 3.0 场景的关键洞察**：
   - **真正瓶颈不是"deinterleave 慢"，而是"deinterleave 在 session 线程同步执行，阻塞 USB transfer 收割"**
   - **单纯把 deinterleave 函数搬到 core 层完全无效**——因为 `sr_session_send` 是同步 API，`DataFeedParser::feed_in_logic` 仍在 session 线程同步执行，阻塞点只是换了位置
   - **必须同时满足三个条件**：`receive_transfer` 立即返回 + deinterleave 在另一线程 + SIMD 加速

5. **推荐方案**：
   - **USB 2.0**：方案 B（driver 层 SIMD 优化），~40 MB/s → ~640 MB/s，余量扩大 16x
   - **USB 3.0 Stream**：方案 E1（driver 内部 worker 线程 + SIMD），USB 收割路径 100ms → <1ms（100x），deinterleave 16x 加速，整体支持 250 MB/s 数据流
   - **未来极致性能**：方案 E2（driver 发 raw + core 层独立线程单遍直通），性能再提升 2x，但破坏 API 契约，建议作为 E1 之后的后续优化

6. **架构建议**：
   - 维持"driver 负责格式归一化、core 负责存储结构化"的分层职责
   - USB 3.0 场景下，driver 内部需引入 worker 线程解耦 USB 收割与计算
   - SIMD 优化应在 driver 层（方案 B/E1）和 core 层（`append_payload_impl`）同时进行
   - 避免方案 A/C/D 等单纯迁移函数的方案——不解决同步阻塞问题

---

## 附录：关键代码引用索引

| 主题 | 文件 | 行号 |
|------|------|------|
| SR_DF_LOGIC 结构体 | [libsigrok.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/include/libsigrok/libsigrok.h) | L521-L526 |
| sr_channel.index 编码注释 | 同上 | L649-L655 |
| pxlogic 格式说明注释 | [pxlogic.c](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/pxlogic/pxlogic.c) | L190-L195 |
| pxlogic deinterleave 函数 | 同上 | L196-L218 |
| pxlogic 转换调用点 | 同上 | L1834-L1858 |
| pxlogic deinterleave_buf 分配 | 同上 | L1591-L1599 |
| pxlogic fallback 路径 | 同上 | L1851-L1856 |
| dslogic 格式说明注释 | [protocol.c](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/dreamsourcelab-dslogic/protocol.c) | L832-L842 |
| dslogic deinterleave 函数 | 同上 | L727-L749 |
| dslogic send_data | 同上 | L751-L766 |
| core 层格式契约注释 | [logicsnapshot.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/logicsnapshot.cpp) | L348-L351 |
| core 层 append_payload_impl | 同上 | L404-L650 |
| core 层 unitsize fallback | 同上 | L417-L419 |
| core 层五阶段解交织 | 同上 | L531-L642 |
| demo 驱动 unitsize 计算 | [demo/api.c](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/demo/api.c) | L142 |
| fx2lafw 驱动逻辑包发送 | [fx2lafw/protocol.c](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrok/src/hardware/fx2lafw/protocol.c) | L368-L411 |
| DataFeedParser 入口 | [datafeedparser.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/core/datafeedparser.cpp) | L64-L112, L184-L186 |
