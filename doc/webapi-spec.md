# PXView Web API 数据传输协议与缓存策略规格书

## 一、核心问题

PXView 支持最高 1GHz 采样率，16/32 通道逻辑分析，数据量可达 GB 级别。
浏览器无法缓存或处理这种量级的原始采样数据，因此：

1. **所有缓存逻辑必须在 PXView 服务端完成**，浏览器只接收"已加工"的显示数据
2. **数据传输量必须被压缩到可管理的级别**（KB~MB 级，而非 GB 级）
3. **流式采集模式下，数据持续产生，需要增量推送机制**

---

## 二、传输协议选型

### 2.1 双通道协议架构

```
┌─────────────────────────────────────────────────────────────┐
│  浏览器 (Web Frontend)                                      │
│    ├─ HTTP/REST  ← 查询类请求（状态、历史数据、解码结果）      │
│    └─ WebSocket  ← 实时推送（流式波形、采集事件）              │
└─────────────────────────────────────────────────────────────┘
         │ HTTP (请求-响应)          │ WebSocket (双向持久)
         ▼                          ▼
┌─────────────────────────────────────────────────────────────┐
│  PXView WebAPI Server (cpp-httplib, 端口 8900)              │
│    ├─ REST Controller  → 从 Snapshot 缓存中读取              │
│    └─ WS Handler       → 监听 ISessionCallback 增量推送      │
└─────────────────────────────────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────────────────────────────────┐
│  SigSession → SessionData(_view_data)                       │
│    ├─ LogicSnapshot   (4级树索引 + Mipmap 边缘缓存)          │
│    ├─ DsoSnapshot     (10级 Envelope 包络缓存)               │
│    └─ AnalogSnapshot  (10级 Envelope 包络缓存)               │
└─────────────────────────────────────────────────────────────┘
```

### 2.2 为什么用两种协议

| 场景 | 协议 | 理由 |
|------|------|------|
| 查询采集状态 | HTTP GET | 请求-响应模式，无状态，简单可靠 |
| 查询设备信息 | HTTP GET | 低频，无需持久连接 |
| 启动/停止采集 | HTTP POST | 控制指令，需确认响应 |
| 查询历史波形（停止后） | HTTP GET | 按需获取指定范围，支持分页 |
| 查询解码注解 | HTTP GET | 解码结果是静态的，按需查询 |
| **流式波形推送** | **WebSocket** | 高频增量数据，需低延迟持久通道 |
| **采集事件通知** | **WebSocket** | 状态变更需实时通知 |
| **解码进度推送** | **WebSocket** | 解码是异步过程，需进度通知 |

### 2.3 为什么不用 SSE (Server-Sent Events)

SSE 仅支持服务端→客户端单向文本流，而 WebSocket 具备：
- **双向通信**：浏览器可发送控制指令（缩放、平移）触发数据请求
- **二进制帧**：波形数据可用二进制编码传输，比 SSE 的文本编码效率高 2-4 倍
- **单一连接**：REST + SSE 需要两个连接，REST + WebSocket 只需一个 WebSocket + 短连接

### 2.4 数据编码格式

| 通道 | 编码格式 | 理由 |
|------|---------|------|
| HTTP REST 响应 | **JSON** | 可读性好，调试方便，数据量小（查询类） |
| WebSocket 控制消息 | **JSON** | 命令/事件消息，结构化，易解析 |
| WebSocket 波形数据 | **二进制 (自定义帧格式)** | 高效，避免 JSON 序列化开销 |

---

## 三、服务端缓存策略——复用 PXView 已有缓存

### 3.1 核心原则：PXView 是缓存主人，Web API 只是读取者

```
浏览器永远不缓存原始采样数据。
浏览器只缓存当前视口对应的"已加工显示数据"。
所有 Mipmap/Envelope 计算在 PXView 服务端完成。
```

PXView 已经实现了极其高效的缓存机制，Web API 层只需**读取这些缓存的结果**，不需要重新实现任何缓存逻辑：

### 3.2 逻辑信号——复用 Mipmap 边缘缓存

**已有缓存机制**：LogicSnapshot 的 4 级树形索引 + Mipmap

```
Level 0: 原始位数据 (1 bit/采样)
Level 1: 翻转位图 (每64个Level0单元压缩为1 bit)
Level 2: 翻转位图 (每64个Level1单元压缩为1 bit)
Level 3: 翻转位图 (每64个Level2单元压缩为1 bit)

RootNode.tog: 标记哪些 LeafBlock 内有信号跳变
RootNode.first/last: 记录每个 LeafBlock 的首尾电平
```

**Web API 读取策略**：

| 场景 | 读取方法 | 数据量估算 |
|------|---------|-----------|
| 全局概览（缩放到全部数据） | `RootNode.first/last` 位图 | 16通道 × N个RootNode × 8字节 ≈ KB级 |
| 中等缩放 | `get_nxt_edge()` 逐通道提取边缘 | 边缘数远少于采样数，典型减少 99%+ |
| 深度缩放（每像素<1个采样） | `get_samples()` 读取原始位数据 | 仅当前视口范围，KB~百KB级 |
| 模式搜索 | `pattern_search()` | 返回匹配位置列表 |

**关键 API**：

```cpp
// 边缘提取——最高效的方式
// 返回值: 是否找到边沿; index: [in]起始位置 [out]边沿位置
bool get_nxt_edge(uint64_t &index, bool last_sample, 
                  uint64_t end, double min_length, int sig_index);

// 批量显示边缘——用于渲染
bool get_display_edges(
    std::vector<std::pair<bool, bool>> &edges,      // 每像素脉冲状态
    std::vector<std::pair<uint16_t, bool>> &togs,    // 边沿位置
    uint64_t start, uint64_t end, uint16_t width,
    uint16_t max_togs, double pixels_offset,
    double min_length, uint16_t sig_index);

// 单点值——用于深度缩放或光标测量
bool get_sample(uint64_t index, int sig_index);
```

**边缘序列 vs 原始数据的数据量对比**：

```
假设: 100MHz采样率, 10M采样点, 8通道, 信号平均每1000个采样跳变一次

原始数据: 10M × 1byte (8通道交织) = 10MB
边缘序列: 10K跳变 × (8字节位置 + 1字节值) × 8通道 = 720KB
压缩比:   ~14:1

如果信号更稳定(每10000采样跳变一次):
边缘序列: 1K跳变 × 9字节 × 8通道 = 72KB
压缩比:   ~139:1
```

### 3.3 DSO/模拟信号——复用 Envelope 包络缓存

**已有缓存机制**：10 级 Envelope 包络线

```
DsoSnapshot:
  EnvelopeScaleFactor = 256 (每256个样本聚合为1个min/max对)
  ScaleStepCount = 10 (10级)
  Level 0: 10M样本 → 39K个 EnvelopeSample
  Level 1: 39K → 152个
  Level 2: 152 → 1个
  总内存: ~40K × 2字节 × 10级 × 2通道 ≈ 1.6MB

AnalogSnapshot:
  EnvelopeScaleFactor = 16 (每16个样本聚合为1个min/max对)
  ScaleStepCount = 10
```

**Web API 读取策略**：

| 场景 | 读取方法 | 数据量估算 |
|------|---------|-----------|
| 全局概览 | `get_envelope_section()` 高层级 | 几十个 min/max 对，<1KB |
| 中等缩放 | `get_envelope_section()` 中层级 | 几百~几千个 min/max 对，KB级 |
| 深度缩放（每像素<256样本） | `get_samples()` 逐样本 | 仅当前视口，KB~百KB级 |

**关键 API**：

```cpp
// DsoSnapshot — 自动选择合适的 Envelope 层级
void get_envelope_section(EnvelopeSection &s,
    uint64_t start, uint64_t end, float min_length, int probe_index);

// EnvelopeSection 结构:
//   start:    起始样本偏移
//   scale:    每个EnvelopeSample代表的原始样本数
//   length:   EnvelopeSample 数量
//   samples:  min/max 对数组

// AnalogSnapshot — 类似但参数略有不同
void get_envelope_section(EnvelopeSection &s,
    uint64_t start, int64_t count, float min_length, int probe_index);
```

**Envelope 数据量估算**：

```
假设: 100MHz采样率, 10M采样点, 2通道DSO, 浏览器视口宽度1920像素

每像素对应样本数 = 10M / 1920 ≈ 5208 样本/像素
→ 使用 Envelope Level 0 (scale=256)
→ 需要 10M/256 = 39063 个 EnvelopeSample
→ 传输量: 39063 × 2字节(min+max) × 2通道 ≈ 156KB

如果进一步缩小视口(只看1/10范围):
→ 需要 3906 个 EnvelopeSample ≈ 15.6KB
```

### 3.4 解码数据——直接读取 Annotation 缓存

解码数据量本身很小（注解是稀疏的），直接读取即可：

```cpp
// 获取解码行列表
DecoderStack::list_rows_size()           // 行数
DecoderStack::list_row_title(row, title) // 行标题

// 获取注解
DecoderStack::get_annotation_subset(dest, row, start, end)  // 范围查询
DecoderStack::list_annotation(ann, row_idx, col_idx)        // 索引查询

// 注解内容
Annotation::start_sample() / end_sample()  // 采样范围
Annotation::annotations()                  // 文本内容 (vector<QString>)
Annotation::is_numberic()                  // 是否数值型
```

---

## 四、流式采集的数据传输策略

### 4.1 流式采集场景分析

PXView 有三种采集模式，流式场景各不相同：

| 模式 | 触发条件 | 数据特征 | 刷新频率 |
|------|---------|---------|---------|
| **单次采集 (SINGLE)** | 手动触发，采集固定长度后停止 | 数据量固定，一次性 | 采集结束后一次查询 |
| **重复采集 (REPEAT)** | 自动重复，每次采集完整波形 | 每次完整替换，双缓冲交换 | 每次采集结束后推送 |
| **循环/流式 (LOOP)** | 持续采集，环形缓冲区滚动 | 数据持续增长/滚动，无限 | **30fps 实时推送** |

**最复杂的场景是 LOOP 模式**：数据以硬件最高速率持续产生，环形缓冲区不断滚动，需要实时推送到浏览器。

### 4.2 LOOP 模式的数据流

```
硬件 (1GHz/100MHz)
    │ USB 批量传输
    ▼
libsigrok 采集线程
    │ sr_datafeed_packet 回调
    ▼
SigSession::feed_in_logic/dso/analog()
    │ 写入 _capture_data (环形缓冲区)
    │ _data_updated = true
    ▼
check_update() (50ms 定时器)
    │ 检测 _data_updated
    ▼
ISessionCallback::data_updated()
    │
    ├──→ Viewport 重绘 (桌面端，30fps)
    │
    └──→ WebAPI Server 推送 (新增)
         │ 读取 _view_data 缓存
         │ 提取增量边缘/包络数据
         ▼
         WebSocket → 浏览器
```

### 4.3 增量推送机制

**核心思想**：不推送原始采样数据，只推送"自上次推送以来新增的边缘/包络变化"。

#### 4.3.1 逻辑信号增量推送

```
服务端维护每个 WebSocket 连接的状态:
  _last_pushed_sample[sig_index]  // 上次推送到的采样位置

每次推送周期(33ms):
  1. 获取当前 ring_sample_count
  2. 对每个通道:
     a. 从 _last_pushed_sample 开始
     b. 调用 get_nxt_edge() 逐个提取新边缘
     c. 直到到达 ring_sample_count
  3. 将新边缘序列编码为二进制帧
  4. 通过 WebSocket 发送
  5. 更新 _last_pushed_sample
```

**增量推送数据量估算**：

```
假设: 100MHz采样率, 33ms推送周期, 8通道, 信号平均每1000采样跳变一次

每周期新增采样: 100M × 0.033 = 3.3M 采样
每周期新增边缘: 3.3M / 1000 = 3300 跳变/通道
8通道总边缘: 3300 × 8 = 26400
二进制编码: 26400 × (8字节位置 + 1字节值) = 237KB/周期

带宽需求: 237KB × 30fps ≈ 7.1 MB/s ≈ 57 Mbps

如果信号更稳定(每10000采样跳变一次):
带宽需求: 0.71 MB/s ≈ 5.7 Mbps  ← 完全可行
```

#### 4.3.2 DSO/模拟信号增量推送

DSO 在 non-instant 模式下每帧覆盖整个缓冲区（不是增量的），因此推送策略不同：

```
DSO non-instant 模式(滚动):
  每帧数据是完整的一屏
  → 每帧推送完整 EnvelopeSection
  → 数据量: 视口宽度个 min/max 对 ≈ 1920×2字节×2通道 ≈ 7.7KB/帧
  → 带宽: 7.7KB × 30fps ≈ 231 KB/s ← 非常小

DSO instant 模式(单次触发):
  数据追加写入
  → 增量推送新 Envelope 数据
  → 类似逻辑信号的增量策略

Analog 流式:
  环形缓冲区滚动
  → 增量推送新 Envelope 数据
  → AnalogSnapshot 的 EnvelopeScaleFactor=16, 更精细
  → 数据量: 略大于 DSO, 但仍在 KB 级/帧
```

### 4.4 环形缓冲区回绕处理

当环形缓冲区写满一圈后，数据从头部覆盖旧数据。这需要特殊处理：

#### 4.4.1 LogicSnapshot 的回绕

LogicSnapshot 使用 `_loop_offset` + `move_first_node_to_last()` 机制：

```
_loop_offset: 逻辑地址0相对于物理地址0的偏移
外部读取使用逻辑索引(0 ~ ring_sample_count)
内部通过加 _loop_offset 转换为物理索引

当 _loop_offset >= RootNodeSamples 时:
  move_first_node_to_last() 将最旧的 RootNode 移到末尾
  _loop_offset -= RootNodeSamples
```

**Web API 处理策略**：

```
服务端维护:
  _last_pushed_sample: 逻辑索引

每次推送:
  current_count = snapshot->get_ring_sample_count()
  
  if current_count < _last_pushed_sample:
    // 环形缓冲区回绕了！
    // 方案1: 发送 "reset" 事件，浏览器清空并重新获取当前视口
    // 方案2: 从 ring_start 到 ring_end 发送完整数据
    
  else:
    // 正常增量推送
    从 _last_pushed_sample 到 current_count
```

**推荐方案1**：回绕时发送 reset 事件，浏览器重新请求当前视口数据。理由：
- 回绕发生时，旧数据已被覆盖，增量推送无意义
- 浏览器只需重新获取当前可见范围的数据
- 重新获取的数据量很小（已通过 Envelope/边缘压缩）

#### 4.4.2 AnalogSnapshot 的回绕

AnalogSnapshot 使用 `_ring_sample_count` 取模 `_total_sample_count`：

```
ring_start(): 缓冲区未满返回0，已满返回 _ring_sample_count
ring_end():   返回最新数据位置
```

**Web API 处理策略**：

```
每次推送:
  ring_start = snapshot->get_ring_start()
  ring_end   = snapshot->get_ring_end()
  
  if ring_start > _last_ring_start:
    // 数据向前滚动了
    // 推送 [ring_start, ring_end] 范围的 Envelope
    // 发送 "viewport_shift" 事件告知浏览器数据窗口移动
```

### 4.5 推送频率控制

PXView 桌面端使用 30fps 刷新率（33ms 间隔）。Web API 应采用相同策略：

```
推送频率: 30fps (33ms 间隔)
触发方式: 定时器驱动，而非数据驱动

理由:
1. 浏览器 requestAnimationFrame 也是 60fps 上限
2. 30fps 对人眼已足够流畅
3. 避免数据驱动导致的高频推送（1GHz采样时每微秒都有新数据）
4. 与 PXView 桌面端行为一致
```

**自适应降频**：当 WebSocket 发送缓冲区积压时，自动降低推送频率：

```
if (ws_send_buffer_size > threshold):
    skip_current_push = true  // 跳过本次推送
    // 下次推送时发送更大范围的增量数据
```

---

## 五、WebSocket 二进制帧格式

### 5.1 帧头格式（固定 8 字节）

```
字节 0:    帧类型
  0x01 = 逻辑边缘数据
  0x02 = DSO/Analog 包络数据
  0x03 = 解码注解更新
  0x04 = 状态事件
  0x05 = 错误事件
  0x06 = 视口重置

字节 1:    通道掩码 (bit0=CH0, bit1=CH1, ...)
字节 2-3:  保留 (0x0000)
字节 4-7:  时间戳 (uint32, 毫秒精度, 相对于连接建立时间)
```

### 5.2 逻辑边缘帧 (type=0x01)

```
帧头 (8字节)
---
通道0边缘数 (uint16, 2字节)
通道0边缘数据:
  位置0 (uint64, 8字节) — 相对于推送起始的偏移
  值0   (uint8,  1字节) — 0或1
  位置1 (uint64, 8字节)
  值1   (uint8,  1字节)
  ...
通道1边缘数 (uint16, 2字节)
通道1边缘数据:
  ...
---
总长度: 8 + Σ(2 + edge_count × 9) × enabled_channels
```

**优化**：位置使用变长编码（varint），因为连续边缘位置差值通常很小：

```
如果 |pos[i] - pos[i-1]| < 128:
  编码为 1 字节 (varint)
否则:
  编码为 8 字节 (uint64)

典型压缩效果: 边缘密集时 50%+ 压缩
```

### 5.3 DSO/Analog 包络帧 (type=0x02)

```
帧头 (8字节)
---
通道0:
  start_sample (uint64, 8字节) — 起始样本位置
  scale        (uint32, 4字节) — 每个EnvelopeSample代表的原始样本数
  length       (uint32, 4字节) — EnvelopeSample 数量
  samples      (length × 2字节) — min/max 对, 每个各1字节
通道1:
  ...
---
总长度: 8 + Σ(16 + length × 2) × enabled_channels
```

### 5.4 解码注解更新帧 (type=0x03)

解码注解数据量小，使用 JSON 编码：

```
帧头 (8字节)
---
JSON payload (UTF-8):
{
  "stack_id": 0,
  "progress": 75,
  "new_annotations": [
    {"row": 0, "start": 1000, "end": 1010, "texts": ["0x1A"]},
    {"row": 0, "start": 1010, "end": 1020, "texts": ["0x2B"]}
  ]
}
```

### 5.5 状态事件帧 (type=0x04)

```
帧头 (8字节)
---
JSON payload (UTF-8):
{
  "event": "capture_started" | "capture_stopped" | "data_updated" | "device_changed",
  "samplerate": 100000000,
  "sample_count": 5000000,
  "trigger_pos": 2500000
}
```

### 5.6 视口重置帧 (type=0x06)

环形缓冲区回绕时发送：

```
帧头 (8字节)
---
JSON payload (UTF-8):
{
  "event": "ring_reset",
  "new_sample_count": 10000000,
  "ring_start": 5000000,
  "ring_end": 4999999
}
```

---

## 六、HTTP REST API 详细设计

### 6.1 状态与设备

```
GET /api/v1/status
→ {
    "capture_state": "idle" | "capturing" | "stopped",
    "collect_mode": "single" | "repeat" | "loop",
    "samplerate": 100000000,
    "sample_count": 10000000,
    "total_sample_count": 10000000,
    "ring_sample_count": 10000000,
    "ring_start": 0,
    "ring_end": 9999999,
    "trigger_pos": 5000000,
    "device_mode": "logic" | "dso" | "analog",
    "is_stream": false,
    "is_single_buffer": true
  }

GET /api/v1/device/info
→ {
    "name": "PX-Logic U3 channel 16 Pro",
    "driver": "pxlogic",
    "mode": "logic",
    "channel_num": 16,
    "is_hardware": true,
    "is_demo": false,
    "is_file": false
  }

GET /api/v1/device/channels
→ {
    "channels": [
      {"index": 0, "name": "CH0", "type": "logic", "enabled": true, "color": "#FF0000"},
      {"index": 1, "name": "CH1", "type": "logic", "enabled": true, "color": "#00FF00"}
    ]
  }

POST /api/v1/capture/start
  Body: {"instant": true}
→ {"status": "ok"}

POST /api/v1/capture/stop
→ {"status": "ok"}
```

### 6.2 波形数据——视口驱动查询

**核心设计**：浏览器发送"我想看什么"（视口参数），服务端返回"加工好的显示数据"。

```
GET /api/v1/waveform/logic/edges?start=0&end=10000000&width=1920&channels=0,1,2,3
  参数:
    start:    起始样本索引 (逻辑索引)
    end:      结束样本索引 (逻辑索引)
    width:    视口像素宽度 (用于计算 min_length 和 max_togs)
    channels: 通道列表 (逗号分隔)
→ {
    "samplerate": 100000000,
    "start_sample": 0,
    "end_sample": 10000000,
    "width": 1920,
    "channels": {
      "0": {
        "edges": [
          {"sample": 0, "value": 1},
          {"sample": 150, "value": 0},
          {"sample": 320, "value": 1}
        ],
        "first_value": 1  // 起始电平
      },
      "1": { ... }
    }
  }

GET /api/v1/waveform/dso/envelope?channel=0&start=0&end=10000000&width=1920
  参数:
    channel:  通道索引
    start:    起始样本索引
    end:      结束样本索引
    width:    视口像素宽度 (用于选择 Envelope 层级)
→ {
    "samplerate": 100000000,
    "channel": 0,
    "start_sample": 0,
    "scale": 256,
    "envelope": [
      {"min": 45, "max": 52},
      {"min": 48, "max": 55},
      ...
    ]
  }

GET /api/v1/waveform/dso/raw?channel=0&start=0&count=1000
  参数:
    channel:  通道索引
    start:    起始样本索引
    count:    采样点数量 (限制最大10000)
→ {
    "samplerate": 100000000,
    "channel": 0,
    "start_sample": 0,
    "data": [128, 130, 125, ...]  // uint8 原始值数组
  }

GET /api/v1/waveform/analog/envelope?channel=0&start=0&end=10000000&width=1920
  (同 DSO envelope 格式)

GET /api/v1/waveform/analog/raw?channel=0&start=0&count=1000
  (同 DSO raw 格式)
```

### 6.3 解码数据

```
GET /api/v1/decode/list
→ {
    "decoders": [
      {
        "stack_id": 0,
        "decoder_id": "spi",
        "label": "SPI",
        "progress": 100,
        "is_running": false,
        "rows_count": 4
      }
    ]
  }

GET /api/v1/decode/{stack_id}/rows
→ {
    "stack_id": 0,
    "rows": [
      {"index": 0, "title": "SPI: MOSI", "has_annotations": true, "visible": true},
      {"index": 1, "title": "SPI: MISO", "has_annotations": true, "visible": true},
      {"index": 2, "title": "SPI: CLK",  "has_annotations": true, "visible": false},
      {"index": 3, "title": "SPI: CS",   "has_annotations": true, "visible": true}
    ]
  }

GET /api/v1/decode/{stack_id}/annotations?row=0&start=0&end=10000000
→ {
    "stack_id": 0,
    "row": 0,
    "samplerate": 100000000,
    "annotations": [
      {"start": 0, "end": 8, "format": 0, "type": 0, "texts": ["0x1A"]},
      {"start": 8, "end": 16, "format": 0, "type": 0, "texts": ["0x2B"]}
    ]
  }

GET /api/v1/decode/{stack_id}/annotations/all?page=1&per_page=100
→ {
    "stack_id": 0,
    "page": 1,
    "per_page": 100,
    "total": 1500,
    "annotations": [ ... ]
  }

PUT /api/v1/decode/{stack_id}/format
  Body: {"format": "hex"}  // hex|dec|oct|bin|ascii
→ {"status": "ok"}
```

---

## 七、WebSocket 通信协议

### 7.1 连接建立

```
ws://localhost:8900/api/v1/ws

连接后服务端发送初始状态:
{
  "type": "init",
  "capture_state": "stopped",
  "samplerate": 100000000,
  "sample_count": 10000000,
  "channels": [...]
}
```

### 7.2 客户端→服务端消息 (JSON)

```json
// 订阅视口数据
{
  "type": "subscribe_viewport",
  "viewport": {
    "start_sample": 0,
    "end_sample": 10000000,
    "width": 1920,
    "channels": [0, 1, 2, 3]
  }
}

// 更新视口范围（缩放/平移时）
{
  "type": "update_viewport",
  "viewport": {
    "start_sample": 5000000,
    "end_sample": 6000000,
    "width": 1920,
    "channels": [0, 1, 2, 3]
  }
}

// 取消订阅
{
  "type": "unsubscribe_viewport"
}

// 请求完整视口快照
{
  "type": "request_snapshot",
  "viewport": {
    "start_sample": 0,
    "end_sample": 10000000,
    "width": 1920,
    "channels": [0, 1, 2, 3]
  }
}
```

### 7.3 服务端→客户端消息

- **二进制帧**：波形数据（边缘序列、包络数据），见第五章
- **JSON 帧**：控制消息、状态事件、解码更新

```json
// 采集状态变更
{"type": "capture_state", "state": "capturing", "samplerate": 100000000}

// 数据更新通知（非流式模式）
{"type": "data_updated", "sample_count": 10000000}

// 解码进度
{"type": "decode_progress", "stack_id": 0, "progress": 75}

// 解码完成
{"type": "decode_done", "stack_id": 0}

// 环形缓冲区重置
{"type": "ring_reset", "sample_count": 10000000, "ring_start": 5000000}

// 错误
{"type": "error", "message": "Device disconnected"}
```

### 7.4 流式推送流程

```
浏览器                          PXView Server
  │                                │
  │── subscribe_viewport ─────────→│  记录视口参数
  │                                │
  │                                │← 定时器触发 (33ms)
  │                                │   读取 _view_data 缓存
  │                                │   提取增量边缘/包络
  │←── 二进制帧(边缘数据) ─────────│  
  │                                │
  │                                │← 定时器触发 (33ms)
  │←── 二进制帧(边缘数据) ─────────│
  │                                │
  │── update_viewport ────────────→│  用户缩放/平移
  │                                │   清除增量状态
  │←── 二进制帧(新视口完整数据) ───│  发送新视口范围的完整数据
  │                                │
  │                                │← 环形缓冲区回绕
  │←── JSON(ring_reset) ──────────│  通知浏览器
  │── request_snapshot ───────────→│  浏览器请求新数据
  │←── 二进制帧(当前视口数据) ─────│
```

---

## 八、浏览器端缓存策略

### 8.1 浏览器缓存什么

| 数据类型 | 缓存策略 | 理由 |
|---------|---------|------|
| 逻辑边缘序列 | **缓存当前视口** | 边缘数据量小，视口内通常 <100KB |
| DSO/Analog 包络 | **缓存当前视口** | 包络数据量小，视口内通常 <50KB |
| 解码注解 | **全部缓存** | 注解数据量极小，且不频繁变化 |
| 原始采样数据 | **不缓存** | 数据量太大，按需从服务端获取 |
| 设备/状态信息 | **缓存 + 事件更新** | 低频变化，通过 WebSocket 事件同步 |

### 8.2 浏览器渲染策略

```
1. 初始加载:
   → 请求当前视口的边缘/包络数据
   → 渲染波形

2. 用户缩放/平移:
   → 发送 update_viewport
   → 等待服务端返回新视口数据
   → 替换缓存并重新渲染

3. 流式采集:
   → 接收增量边缘/包络数据
   → 追加到现有缓存
   → 增量渲染（只绘制新增部分）

4. 环形缓冲区回绕:
   → 收到 ring_reset 事件
   → 清空缓存
   → 请求当前视口完整数据
   → 重新渲染
```

### 8.3 Canvas 渲染优化

```
- 使用双 Canvas: 一个显示已完成数据，一个绘制增量数据
- 逻辑信号: 用 Path2D 绘制边缘序列，效率远高于逐像素绘制
- DSO/Analog: 用 fillRect 绘制包络区域，与桌面端策略一致
- 增量渲染: 只清除和重绘新增区域，不全屏重绘
- requestAnimationFrame: 与浏览器刷新率同步，避免过度绘制
```

---

## 九、对 SigSession 的最小改动

### 9.1 新增公共访问方法

```cpp
// sigsession.h 中新增:
public:
    SessionData* get_view_data() const { return _view_data; }
    SessionData* get_capture_data() const { return _capture_data; }
```

### 9.2 新增数据监听器接口

```cpp
// sigsession.h 中新增:
public:
    class IWebDataListener {
    public:
        virtual ~IWebDataListener() {}
        virtual void on_data_updated() = 0;
        virtual void on_frame_ended() = 0;
        virtual void on_frame_began() = 0;
        virtual void on_decode_done(int stack_id) = 0;
        virtual void on_capture_state_changed(int state) = 0;
    };

    void add_web_data_listener(IWebDataListener* listener);
    void remove_web_data_listener(IWebDataListener* listener);

private:
    std::vector<IWebDataListener*> _web_data_listeners;
```

### 9.3 在现有回调中触发监听器

```cpp
// MainWindow::data_updated() 中新增:
for (auto* listener : _session->_web_data_listeners)
    listener->on_data_updated();

// MainWindow::frame_ended() 中新增:
for (auto* listener : _session->_web_data_listeners)
    listener->on_frame_ended();

// 类似处理 frame_began, decode_done, capture_state_changed
```

### 9.4 解决 get_ch_order 访问权限

LogicSnapshot 和 DsoSnapshot 的 `get_ch_order()` 是 private，Web API 层需要通道映射。

**方案**：在 LogicSnapshot/DsoSnapshot 中新增公共包装方法：

```cpp
// logicsnapshot.h 新增:
public:
    int get_channel_order(int sig_index) { return get_ch_order(sig_index); }

// dsosnapshot.h 新增:
public:
    int get_channel_order(int sig_index) { return get_ch_order(sig_index); }
```

### 9.5 解决 _mutex 访问权限

`Snapshot::_mutex` 是 protected，Web API 层需要加锁保护数据读取。

**方案**：在 Snapshot 基类中新增公共锁访问方法：

```cpp
// snapshot.h 新增:
public:
    std::mutex& mutex() const { return _mutex; }
```

---

## 十、性能预算

### 10.1 单次查询延迟

| API | 目标延迟 | 数据量 |
|-----|---------|--------|
| GET /status | <5ms | <1KB |
| GET /device/info | <5ms | <1KB |
| GET /waveform/logic/edges (全量) | <50ms | <500KB |
| GET /waveform/dso/envelope (全量) | <30ms | <200KB |
| GET /decode/annotations | <20ms | <100KB |

### 10.2 流式推送带宽

| 场景 | 推送频率 | 每帧数据量 | 带宽需求 |
|------|---------|-----------|---------|
| 逻辑信号(稳定, 8ch) | 30fps | ~10KB | ~300KB/s |
| 逻辑信号(活跃, 8ch) | 30fps | ~240KB | ~7.2MB/s |
| DSO(2ch, envelope) | 30fps | ~8KB | ~240KB/s |
| Analog(2ch, envelope) | 30fps | ~16KB | ~480KB/s |
| 解码注解 | 事件驱动 | ~1KB | 忽略不计 |

### 10.3 内存开销

Web API 层本身的内存开销极小（<10MB），因为：
- 不复制原始采样数据
- 只读取 Snapshot 缓存的结果
- WebSocket 连接状态每连接 <1KB
- 增量推送的临时缓冲区 <1MB

---

## 十一、实施优先级

| Phase | 内容 | 依赖 |
|-------|------|------|
| **P1** | 引入 cpp-httplib，实现 HTTP 服务器 + `/status` + `/device/info` | SigSession 公共方法 |
| **P2** | 实现 `DataExtractor`，逻辑/DSO/Analog 波形 REST API | Snapshot mutex 访问 |
| **P3** | 实现解码数据 REST API | 无额外依赖 |
| **P4** | 实现 WebSocket 服务 + 流式推送 | IWebDataListener |
| **P5** | 实现增量推送 + 环形缓冲区回绕处理 | P4 |
| **P6** | 前端网页（波形渲染 + 解码列表） | P1-P5 |
| **P7** | 性能优化（二进制帧、varint编码、自适应降频） | P5 |
