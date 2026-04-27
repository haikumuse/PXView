# 最小可行 PoC 计划：PXView Web 波形显示验证

## 目标

在 PXView 中嵌入一个 HTTP 服务器，使用 Demo Device 生成数据，在浏览器中显示逻辑波形和解码数据，验证整个数据提取→传输→渲染链路的可行性。

## 核心设计原则：复用 UI 层的数据访问路径

**不绕过 UI 层直接操作底层 Snapshot**，而是模拟 UI 渲染时的相同调用路径。

PXView 的 UI 层已经实现了完整的数据提取逻辑：
- `LogicSignal::paint_mid_align()` → 调用 `LogicSnapshot::get_display_edges()`
- `DsoSignal::paint_mid()` → 调用 `DsoSnapshot::get_samples()` / `get_envelope_section()`
- `DecodeTrace::paint_mid()` → 调用 `DecoderStack::get_annotation_subset()`

Web API 应该走**完全相同的路径**，只是把"画到 Canvas"换成"序列化为 JSON"。

### UI 层数据访问链（Web API 必须复用）

```
浏览器发送视口参数 (scale, offset, width)
    │
    ▼
WebApiServer 模拟 View 的参数计算
    │  samples_per_pixel = samplerate * scale
    │  start_index = floor(offset * samples_per_pixel)
    │  end_index = floor((offset + width) * samples_per_pixel)
    │
    ▼
调用与 UI 相同的数据提取方法
    │  Logic: get_display_edges(start, end, width, ...)
    │  DSO:   get_envelope_section(start, end, samples_per_pixel, ...)
    │  Decode: get_annotation_subset(row, start, end)
    │
    ▼
序列化为 JSON → 返回浏览器
```

## 前提约束

- **数据源**：Demo Device（随机模式，无需硬件）
- **浏览器端**：单个 HTML 文件，纯原生 JS + Canvas，无框架依赖
- **服务端**：最小 API 子集，仅验证数据通路
- **C++ 标准**：从 C++11 升级到 C++14（cpp-httplib 需要）

## 实施步骤

### Step 1: 引入 cpp-httplib 依赖

**操作**：
- 下载 `httplib.h` 到 `DSView/pv/webapi/` 目录
- 修改 `CMakeLists.txt`：C++ 标准从 `-std=c++11` 改为 `-std=c++14`
- 添加 `webapi/` 的 include 路径和源文件

**文件变更**：
- `CMakeLists.txt` — 修改 CMAKE_CXX_FLAGS，添加 webapi 源文件
- `DSView/pv/webapi/httplib.h` — 新增（第三方 header-only 库）

### Step 2: 创建 WebApiServer 类

**操作**：
- 创建 `webapi_server.h` / `.cpp`
- 在独立线程中启动 HTTP 服务器（端口 8900）
- 持有 `SigSession*` 引用，通过 `AppControl::Instance()->GetSession()` 获取
- 实现 `IMessageListener` 接口，监听数据更新事件

**核心逻辑**：
```
WebApiServer 启动 → 注册路由 → 等待请求
请求到达 → 通过 SigSession 的 Signal/DecodeTrace 对象提取数据 → JSON 响应
```

**文件变更**：
- `DSView/pv/webapi/webapi_server.h` — 新增
- `DSView/pv/webapi/webapi_server.cpp` — 新增

### Step 3: 在 main.cpp 中启动 WebApiServer

**操作**：
- 在 `main.cpp` 的 `control->Start()` 之后添加：
  ```cpp
  #include "pv/webapi/webapi_server.h"
  pv::webapi::WebApiServer::Instance()->Start(control->GetSession());
  ```
- 在退出前添加 `pv::webapi::WebApiServer::Instance()->Stop()`

**文件变更**：
- `DSView/main.cpp` — 添加 3 行代码

### Step 4: 实现最小 API 端点（复用 UI 层逻辑）

仅实现 4 个端点。关键：**所有数据提取走 UI 层相同的调用路径**。

| 端点 | 方法 | 说明 | UI 对应逻辑 |
|------|------|------|------------|
| `/api/v1/status` | GET | 采集状态、采样率、样本数 | `SigSession` 公共方法 |
| `/api/v1/channels` | GET | 通道列表 | `SigSession::get_signals()` |
| `/api/v1/waveform` | GET | 波形数据（视口驱动） | `LogicSignal::paint_mid_align()` 的数据提取部分 |
| `/api/v1/decode/annotations` | GET | 解码注解 | `DecodeTrace::paint_mid()` 的数据提取部分 |

#### 4.1 `/api/v1/waveform` — 复用 LogicSignal 的渲染逻辑

浏览器发送与 UI 相同的视口参数，服务端用相同的公式计算采样范围，调用相同的方法：

```
GET /api/v1/waveform?scale=0.000001&offset=0&width=1920

参数（与 View 类一致）:
  scale:  秒/像素（与 _view->scale() 相同含义）
  offset: 像素偏移（与 _view->offset() 相同含义）
  width:  视口像素宽度

服务端计算（与 LogicSignal::paint_mid_align 完全一致）:
  samplerate = session->cur_snap_samplerate()
  samples_per_pixel = samplerate * scale
  start_index = floor(offset * samples_per_pixel)
  end_index = floor((offset + width + 1) * samples_per_pixel)
  max_togs = width / 10  (TogMaxScale=10)

数据提取（与 UI 调用相同方法）:
  LogicSnapshot* snap = session->get_snapshot(SR_CHANNEL_LOGIC)
  snap->get_display_edges(edges, togs, start_index, end_index, width, max_togs,
                          offset, samples_per_pixel, channel_index)

响应 JSON:
{
  "samplerate": 1000000,
  "scale": 0.000001,
  "offset": 0,
  "samples_per_pixel": 1.0,
  "channels": {
    "0": {
      "first_value": true,
      "edges": [[150, false], [320, true], ...]   // [像素位置, 电平值]
    },
    "1": { ... }
  }
}
```

**为什么这样设计**：
- `get_display_edges()` 是 PXView UI 自己用的渲染接口，PXView 无论怎么重构，这个接口必须保持稳定（否则自己的 UI 就画不了）
- 传入的参数（scale, offset, width）与 UI 传入的完全一致，保证数据一致性
- 返回的 edges 已经是像素级别的渲染数据，浏览器只需画线，无需任何计算

#### 4.2 `/api/v1/decode/annotations` — 复用 DecodeTrace 的逻辑

```
GET /api/v1/decode/annotations?scale=0.000001&offset=0&width=1920

参数（与 DecodeTrace::paint_mid 一致）:
  scale:  秒/像素
  offset: 像素偏移
  width:  视口像素宽度

服务端计算（与 DecodeTrace::paint_mid 完全一致）:
  samplerate = decoder_stack->samplerate()
  samples_per_pixel = samplerate * scale
  start_sample = (0 + offset) * samples_per_pixel
  end_sample = (width + offset) * samples_per_pixel

数据提取（与 UI 调用相同方法）:
  decoder_stack->get_rows_gshow()  → 行列表及可见性
  decoder_stack->has_annotations(row)
  decoder_stack->get_annotation_subset(annotations, row, start_sample, end_sample)

响应 JSON:
{
  "decoders": [
    {
      "id": "spi",
      "rows": [
        {
          "title": "MOSI",
          "visible": true,
          "annotations": [
            {"start": 100, "end": 108, "texts": ["0x1A"]},
            {"start": 108, "end": 116, "texts": ["0x2B"]}
          ]
        }
      ]
    }
  ]
}
```

#### 4.3 `/api/v1/status` — 复用 SigSession 的公共接口

```
GET /api/v1/status

数据来源（全部是 SigSession 的公共方法）:
  session->is_working()           → 采集状态
  session->cur_snap_samplerate()  → 采样率
  session->get_snapshot(type)->get_sample_count()  → 样本数
  session->get_snapshot(type)->get_ring_sample_count()  → 环形缓冲区样本数
  session->get_device()->get_work_mode()  → 工作模式
  session->get_device()->is_demo()  → 是否Demo设备

响应 JSON:
{
  "capture_state": "stopped",
  "samplerate": 1000000,
  "sample_count": 1000000,
  "ring_sample_count": 1000000,
  "work_mode": "logic",
  "is_demo": true
}
```

#### 4.4 `/api/v1/channels` — 复用 SigSession::get_signals()

```
GET /api/v1/channels

数据来源:
  session->get_signals()  → vector<view::Signal*>
  signal->enabled()       → 是否使能
  signal->get_index()     → 通道索引
  signal->signal_type()   → 信号类型 (SR_CHANNEL_LOGIC/DSO/ANALOG)

响应 JSON:
{
  "channels": [
    {"index": 0, "name": "CH0", "type": "logic", "enabled": true},
    {"index": 1, "name": "CH1", "type": "logic", "enabled": true}
  ]
}
```

### Step 5: 解决 SigSession 访问权限问题（最小改动）

Web API 需要通过 Signal 对象访问数据，但 Signal 的 `_data` 指针是 protected。需要添加公共访问方法。

**操作**：
- 在 `sigsession.h` 中添加：
  ```cpp
  SessionData* get_view_data() const { return _view_data; }
  ```
- 在 `snapshot.h` 中添加：
  ```cpp
  std::mutex& mutex() const { return _mutex; }
  ```
- 在 `logicsnapshot.h` 中添加：
  ```cpp
  int get_channel_order(int sig_index) { return get_ch_order(sig_index); }
  ```

**文件变更**：
- `DSView/pv/sigsession.h` — 添加 1 行
- `DSView/pv/data/snapshot.h` — 添加 1 行
- `DSView/pv/data/logicsnapshot.h` — 添加 1 行

### Step 6: 创建前端 HTML 页面

**操作**：
- 创建 `DSView/pv/webapi/static/index.html`
- 由 cpp-httplib 的文件服务器直接提供
- 纯原生 HTML + JS + Canvas，无任何框架依赖

**页面功能**：
1. 连接状态显示（轮询 `/api/v1/status`）
2. 通道列表显示（从 `/api/v1/channels` 获取）
3. 逻辑波形 Canvas 渲染（从 `/api/v1/waveform` 获取边缘数据）
4. 解码注解列表显示（从 `/api/v1/decode/annotations` 获取）
5. 手动刷新按钮 + 自动刷新（1秒间隔轮询）

**波形渲染逻辑**（与 LogicSignal::paint_mid_align 完全对应）：
```
获取 edges 数据 → 对每个通道:
  1. 设定起始电平 (first_value)
  2. 遍历 edges: [pixel_position, level]
  3. 画水平线到 pixel_position，然后画垂直跳变线
  4. 继续到下一个 edge
  （这与 LogicSignal 中 wave_lines 的绘制逻辑完全一致）
```

**文件变更**：
- `DSView/pv/webapi/static/index.html` — 新增

### Step 7: 集成测试

**操作**：
1. 编译 PXView（确保 Demo Device 可用）
2. 启动 PXView，等待 Demo Device 加载
3. 在 PXView 中手动启动一次采集
4. 打开浏览器访问 `http://localhost:8900`
5. 验证：状态显示正确、波形可渲染、解码数据可展示

## 文件变更汇总

| 文件 | 操作 | 改动量 |
|------|------|--------|
| `CMakeLists.txt` | 修改 | ~10行 |
| `DSView/main.cpp` | 修改 | ~3行 |
| `DSView/pv/sigsession.h` | 修改 | ~1行 |
| `DSView/pv/data/snapshot.h` | 修改 | ~1行 |
| `DSView/pv/data/logicsnapshot.h` | 修改 | ~1行 |
| `DSView/pv/webapi/httplib.h` | 新增 | 第三方库 |
| `DSView/pv/webapi/webapi_server.h` | 新增 | ~50行 |
| `DSView/pv/webapi/webapi_server.cpp` | 新增 | ~350行 |
| `DSView/pv/webapi/static/index.html` | 新增 | ~400行 |

**总改动量**：对现有代码仅修改 ~16 行，新增 ~800 行

## 为什么"靠近 UI 层"是正确的

1. **版本兼容**：PXView 无论怎么重构内部存储，必须保证自己的 UI 能画图。只要我们模拟 UI 层的调用方式，API 就和 UI 一样长久有效
2. **缓存复用**：`get_display_edges()` 和 `get_envelope_section()` 内部已经使用了 Mipmap/Envelope 缓存，Web API 自动享受这些优化
3. **数据一致性**：浏览器看到的数据与 PXView 桌面端显示的完全一致
4. **最小侵入**：不需要了解 Snapshot 内部的树形索引、环形缓冲区等复杂实现

## 风险与应对

| 风险 | 应对 |
|------|------|
| C++14 升级导致编译错误 | C++14 向后兼容 C++11，风险极低 |
| cpp-httplib 在 Windows 上编译问题 | 使用 v0.14 稳定版，已验证 Win32 支持 |
| Demo Device 未自动加载 | 在 PXView UI 中手动选择 Demo Device |
| 线程安全问题 | 所有 Snapshot 读取前加锁（通过 `mutex()`） |
| 解码器未配置 | 在 PXView UI 中手动添加 SPI 解码器验证 |
| `get_display_edges` 的参数理解偏差 | 严格对照 `LogicSignal::paint_mid_align()` 的调用方式 |
