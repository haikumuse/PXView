# PXView Core/View 分离 v2：基于业务引擎的精简方案

## Why

现有 `decouple-core-from-view` spec（v1）引入了 5 个纯数据 Model 类（SignalModel、DecodeModel、SpectrumModel、LissajousModel、MathModel）作为 Core/View 之间的中间层。但 cppverdebug 分支的实施证明该方案存在根本性缺陷：

1. **DecodeModel/SpectrumModel/MathModel 信息不足** — SpectrumModel 不持有 SpectrumStack*、MathModel 不持有 MathStack*，导致 View 层无法从 Model 重建 trace（cppverdebug 中 `View::on_data_updated()` 是空 stub，SpectrumTrace/MathTrace/LissajousTrace 全部丢失）
2. **DecodeTrace 创建路径断裂** — `add_decoder()` 只创建 DecodeModel，依赖事件回调间接创建 DecodeTrace，但 `on_signals_changed` 每次删除所有旧 trace 再重建，时序不可靠
3. **冗余抽象** — DecoderStack/SpectrumStack/MathStack 本身就是 Core 层对象（继承 QObject，只依赖 Qt Core，不依赖 Qt Widgets），已包含 Model 应有的所有信息，再包一层 Model 是多余的

本方案（v2）保留 v1 的 Headless 和库分离目标，但采用更精简的架构：**只对 Signal 引入 Model（因 view::Signal 是 QObject，有线程限制），对 Decoder/Spectrum/Math/Lissajous 直接使用已有的 Stack 对象，不引入中间 Model 层**。

## 架构定位

```
AI Agent   ──→ MCP (HTTP :10110)      ──→ ISessionService ──→ SigSession
新 GUI     ──→ WS  (WebSocket :10430) ──→ ISessionService ──→ SigSession
原生 Qt GUI ──→ DirectTransport (进程内) ──→ ISessionService ──→ SigSession
```

与 v1 相同的三客户端架构，区别在于 Core 层内部设计：

| 对象 | v1 方案 | v2 方案（本方案） |
|------|---------|-----------------|
| 信号业务状态 | `SignalModel*` | `SignalModel*`（保留，QObject 线程限制） |
| 解码器 | `DecodeModel*`（持有 DecoderStack*） | 直接用 `DecoderStack*`（已是 Core 层对象） |
| 频谱 | `SpectrumModel*`（不持有 Stack，信息断裂） | 直接用 `SpectrumStack*`（已是 Core 层对象） |
| 数学运算 | `MathModel*`（不持有 Stack，信息断裂） | 直接用 `MathStack*`（已是 Core 层对象） |
| 李萨如 | `LissajousModel*` | `LissajousModel*`（保留，无对应 Stack） |

## What Changes

- **SignalModel 保留**：view::Signal 继承链为 `Signal → Trace → SelectableItem → QObject`，是 QObject，有线程亲和性，不能在非主线程创建。Core 层需要独立的信号业务状态载体
- **不引入 DecodeModel/SpectrumModel/MathModel**：DecoderStack（`pv::data::decode` 命名空间）、SpectrumStack、MathStack 继承 QObject 但只依赖 Qt Core（QObject/QString），不依赖 Qt Widgets，在 Headless 模式（QCoreApplication）下完全可用。它们已包含名称、通道映射、选项、进度、解码结果等全部信息
- **DataSource 接口重设计**：返回 `SignalModel*`（信号）和 `DecoderStack*`/`SpectrumStack*`/`MathStack*`（业务引擎），不返回 `view::*` 类型
- **SigSession 去视图化**：移除 `view::Signal*`、`view::SpectrumTrace*`、`view::LissajousTrace*`、`view::MathTrace*` 成员；DecoderStack 已由 SessionDocument 持有，无需改动
- **SessionDocument 去视图化**：移除 `view::DecodeTrace*`、`view::Signal*` 等成员，保留已有的 `_decoder_stacks`（Core 层对象）
- **View 层直接创建渲染对象**：View 持有 `view::Signal`、`view::DecodeTrace`、`view::SpectrumTrace` 等渲染对象，从 DataSource 获取 SignalModel 和 Stack 对象自行创建和管理，不依赖事件回调间接创建
- **add_decoder 创建责任拆分**：Core 层（SigSession/SessionDocument）只创建 DecoderStack；View 层自行创建 DecodeTrace 并关联到 DecoderStack
- **LissajousModel 保留**：Lissajous 当前只有 view::LissajousTrace，没有对应 Stack，需要一个轻量数据载体
- **Headless 模式**：`main.cpp` 支持 `--headless` 参数，使用 QCoreApplication
- **CMake 库分离**：Core 层编译为独立静态库 `pxview-core`，不依赖 Qt Widgets

### **BREAKING** Changes

- `DataSource` 接口签名变更：移除 `get_signals()`/`get_decode_signals()`/`get_spectrum_traces()`/`get_lissajous_trace()`/`get_math_trace()`，新增 `get_signal_models()`/`get_decoder_stacks()`/`get_spectrum_stacks()`/`get_lissajous_model()`/`get_math_stack()`
- `SigSession::add_decoder()` 输出参数从 `view::Trace*&` 改为 `data::DecoderStack*&`
- `SessionDocument::get_decode_traces()`/`add_decode_trace()` 移除，改为 `get_decoder_stacks()`（已存在）
- 所有调用 `_session->get_signals()` 并做 `static_cast<view::LogicSignal*>` 的代码需适配为从 SignalModel 读取数据

## Impact

- Affected specs: decouple-core-from-view（v1，本方案替代 v1）, add-api-service-layer, add-multi-tab-sessions, add-mcp-sidebar-entry
- Affected code:
  - `PXView/pv/data/datasource.h` — 接口重设计
  - `PXView/pv/data/signalmodel.h/cpp` — 新建（信号业务状态）
  - `PXView/pv/data/lissajousmodel.h/cpp` — 新建（李萨如配置）
  - `PXView/pv/sigsession.h/cpp` — 移除 View 成员，改用 SignalModel
  - `PXView/pv/data/sessiondocument.h/cpp` — 移除 View 依赖，保留 DecoderStack
  - `PXView/pv/data/sessionsnapshot.h/cpp` — 同步移除 View 依赖
  - `PXView/pv/api/session_service.h/cpp` — 移除 View 依赖，消除死锁
  - `PXView/pv/view/view.h/cpp` — 持有并管理渲染对象，从 DataSource 创建
  - `PXView/pv/view/signalfactory.h/cpp` — 新建，从 SignalModel 创建 Signal
  - `PXView/pv/storesession.cpp` — 适配 SignalModel
  - `PXView/pv/dock/` — 所有 Dock 组件适配
  - `PXView/pv/dialogs/` — 所有对话框适配
  - `PXView/main.cpp` — Headless 模式
  - `CMakeLists.txt` — 库分离
  - `libsigrok/lib_main.c` — 新增带 void* 的回调注册 API

## ADDED Requirements

### Requirement: SignalModel 纯数据模型

系统 SHALL 提供不依赖 Qt Widgets 的 SignalModel 类，持有信号的完整业务状态。

SignalModel SHALL 包含以下属性：
- 通道索引、名称、类型（使用现有 `api::ChannelType` 枚举）
- 启用/禁用状态
- 探头配置（vdiv、coupling、vfactor、map_default）
- 触发状态（可读写）：触发类型枚举，通过 `commit_trig()` 同步到 libsigrok
- DSO 特有参数（垂直偏移、零点偏移、hw_offset）
- 信号颜色（使用 `std::string` 而非 `QColor`）
- 毛刺过滤/信号反转状态
- 快照指针（void*，类型由 type 决定）

SignalModel SHALL NOT 依赖任何 `pv::view::*` 类型或 Qt Widgets 头文件。仅依赖 Qt Core（QString 等）和标准库。

SignalModel 是触发配置的**权威持有者**。View 层的 Signal 对象从 SignalModel 读取触发状态进行渲染，用户在 UI 中修改触发后通过 SignalModel 写入并调用 `commit_trig()` 同步到 libsigrok。

#### Scenario: SigSession 创建 SignalModel 而非 View 对象
- **WHEN** `SigSession::init_signals()` 被调用
- **THEN** 创建 `SignalModel` 对象并存入 `_signal_models` 向量
- **AND** 不创建任何 `view::Signal*` 对象

#### Scenario: View 层从 SignalModel 创建 Signal
- **WHEN** View 层需要渲染信号
- **THEN** 通过 `SignalFactory::create_signals(DataSource*)` 从 SignalModel 创建对应的 `view::Signal` 子类
- **AND** Signal 对象持有 SignalModel 指针以读取业务状态

#### Scenario: Headless 模式下配置信号参数
- **WHEN** Headless 模式下通过 MCP API 设置通道 vdiv
- **THEN** SessionService 直接操作 SignalModel，不创建 view::Signal
- **AND** SignalModel 的变更通过 signals_changed 事件通知 View 层（GUI 模式）

### Requirement: LissajousModel 纯数据模型

系统 SHALL 提供 LissajousModel 类，作为李萨如图形的纯数据配置载体。

LissajousModel SHALL 包含：
- 通道1索引、通道2索引
- 启用状态

LissajousModel SHALL NOT 依赖 `pv::view::*` 类型。

LissajousModel 存在的原因：李萨如图形当前只有 `view::LissajousTrace`，没有对应的 Stack 处理引擎，因此需要一个轻量数据载体在 Core 层存储配置。

### Requirement: DataSource 接口去视图化（v2）

系统 SHALL 重新设计 `DataSource` 接口，不返回任何 `view::*` 类型。

新的 DataSource 接口 SHALL 提供：
- `get_signal_models()` → `vector<SignalModel*>&` — 信号业务状态
- `get_decoder_stacks()` → `vector<DecoderStack*>&` — 解码器处理引擎（Core 层对象，不是 View 对象）
- `get_spectrum_stacks()` → `vector<SpectrumStack*>&` — 频谱处理引擎
- `get_math_stack()` → `MathStack*` — 数学运算引擎
- `get_lissajous_model()` → `LissajousModel*` — 李萨如配置
- `get_logic_snapshot()` / `get_analog_snapshot()` / `get_dso_snapshot()` — 保持不变
- `cur_snap_samplerate()` / `cur_samplelimits()` / `cur_sampletime()` — 保持不变
- `get_trigger_pos()` — 保持不变
- `get_decoder_model()` — 保持不变（DecoderModel 是 UI 层用于显示解码器列表的模型，非业务数据）

DataSource 接口 SHALL NOT 包含任何 `view::Signal*`、`view::DecodeTrace*`、`view::SpectrumTrace*`、`view::LissajousTrace*`、`view::MathTrace*` 的返回值。

**关键设计决策**：DecoderStack/SpectrumStack/MathStack 是 `pv::data` 命名空间下的 QObject 子类，只依赖 Qt Core（QObject/QString），不依赖 Qt Widgets。它们在 Headless 模式（QCoreApplication）下完全可用。因此不需要为它们再引入 DecodeModel/SpectrumModel/MathModel 中间层。

#### Scenario: View 层通过新接口获取解码器列表
- **WHEN** View 层需要创建 DecodeTrace 渲染对象
- **THEN** 调用 `effective_data_source()->get_decoder_stacks()` 获取 `vector<DecoderStack*>&`
- **AND** 对每个 DecoderStack 创建对应的 DecodeTrace

#### Scenario: Headless 模式下 DataSource 不返回 View 对象
- **WHEN** Headless 模式下 SessionService 访问 DataSource
- **THEN** 所有返回值均为 Core 层对象（SignalModel、DecoderStack 等）
- **AND** 不触发任何 view::* 类型的实例化

### Requirement: SigSession 去视图化

SigSession SHALL 移除所有 `view::*` 类型的成员变量和直接依赖。

具体移除：
- `std::vector<view::Signal*> _signals` → 替换为 `std::vector<SignalModel*> _signal_models`
- `std::vector<view::SpectrumTrace*> _spectrum_traces` → 替换为 `std::vector<SpectrumStack*> _spectrum_stacks`
- `view::LissajousTrace* _lissajous_trace` → 替换为 `LissajousModel* _lissajous_model`
- `view::MathTrace* _math_trace` → 替换为 `MathStack* _math_stack`
- `#include "view/mathtrace.h"` 等所有 `pv/view/*.h` → 移除

SigSession SHALL NOT 直接 `#include` 任何 `pv/view/*.h` 头文件。

**DecoderStack 管理**：当前 SessionDocument 已持有 `_decoder_stacks`（Core 层对象），SigSession 通过 `_active_document->get_decoder_stacks()` 访问。此设计无需改动。

#### Scenario: Headless 模式下 SigSession 可独立运行
- **WHEN** 应用以 `--headless` 模式启动
- **THEN** SigSession 可以正常初始化、采集、解码、导出数据
- **AND** 不触发任何 QWidget 创建或 Qt Widgets 库依赖

### Requirement: SessionDocument 去视图化

SessionDocument SHALL 移除所有 `view::*` 类型的成员变量。

具体移除：
- `std::vector<view::DecodeTrace*> _decode_traces` → 移除（已有 `_decoder_stacks`）
- `std::vector<view::Signal*> _signals` → 替换为 `std::vector<SignalModel*> _signal_models`
- `std::vector<view::SpectrumTrace*> _spectrum_traces` → 替换为 `std::vector<SpectrumStack*> _spectrum_stacks`
- `view::LissajousTrace* _lissajous_trace` → 替换为 `LissajousModel* _lissajous_model`
- `view::MathTrace* _math_trace` → 替换为 `MathStack* _math_stack`

SessionDocument SHALL 保留 `_decoder_stacks`（已是 Core 层对象）。

移除的方法：
- `get_decode_traces()` / `add_decode_trace()` / `remove_decode_trace()` — 移除
- `get_signals()` / `get_decode_signals()` / `get_spectrum_traces()` / `get_lissajous_trace()` / `get_math_trace()` — 替换为新接口

新增/保留的方法：
- `get_decoder_stacks()` / `add_decoder_stack()` / `remove_decoder_stack()` — 保留（已存在）
- `get_signal_models()` — 新增
- `get_spectrum_stacks()` — 新增
- `get_math_stack()` — 新增
- `get_lissajous_model()` — 新增

#### Scenario: SessionDocument 可在无 GUI 环境下加载和保存
- **WHEN** 通过 MCP API 调用 `load_file()` / `save_file()`
- **THEN** SessionDocument 正常工作，不依赖任何 View 对象

### Requirement: SessionSnapshot 去视图化

SessionSnapshot 也实现了 `DataSource` 接口，SHALL 同步去视图化，与 SessionDocument 保持一致。

### Requirement: View 层管理渲染对象生命周期

View 层 SHALL 自行持有和管理所有渲染对象，不依赖 Core 层事件回调间接创建。

View SHALL 持有：
- `std::vector<view::Signal*> _signals` — 从 SignalModel 创建
- `std::vector<view::DecodeTrace*> _decode_traces` — 从 DecoderStack 创建
- `std::vector<view::SpectrumTrace*> _spectrum_traces` — 从 SpectrumStack 创建
- `view::MathTrace* _math_trace` — 从 MathStack 创建
- `view::LissajousTrace* _lissajous_trace` — 从 LissajousModel 创建

View 层 SHALL 通过以下方式管理渲染对象：
- **信号变更**：监听 `signals_changed` 事件，通过 `SignalFactory::update_signals()` 增量更新 Signal 列表
- **解码器添加**：View 层主动调用 Core 层创建 DecoderStack，然后自行创建 DecodeTrace
- **解码器删除**：View 层删除 DecodeTrace，通知 Core 层删除对应的 DecoderStack
- **数据更新**：View 层的渲染对象长期存在，数据更新时只需刷新数据指针，不需要重建对象

#### Scenario: 添加解码器时 View 层直接创建 DecodeTrace
- **WHEN** 用户在 GUI 中点击"添加解码器"
- **THEN** View 调用 Core 层创建 DecoderStack
- **AND** View 自行创建 DecodeTrace 并关联到 DecoderStack
- **AND** 不依赖 signals_changed 事件回调间接创建

#### Scenario: 数据更新时不重建渲染对象
- **WHEN** 采集数据更新触发 `data_updated()`
- **THEN** View 层的 DecodeTrace/SpectrumTrace/MathTrace 对象保持不变
- **AND** 只刷新关联的数据指针（快照引用）
- **AND** 不删除并重建渲染对象

#### Scenario: 信号列表变更时只更新 Signal
- **WHEN** 设备通道变化触发 `signals_changed`
- **THEN** View 层通过 SignalFactory 增量更新 Signal 列表
- **AND** DecodeTrace/SpectrumTrace/MathTrace 不受影响（它们关联的是 Stack，不是 Signal）

### Requirement: SignalFactory 工厂类

系统 SHALL 提供 `SignalFactory` 类，负责从 SignalModel 创建 View 层 Signal 对象。

SignalFactory SHALL：
- 接收 `vector<SignalModel*>&` 输入
- 根据 `SignalModel::type` 创建对应的 `view::LogicSignal`、`view::AnalogSignal`、`view::DsoSignal`
- 将 SignalModel 指针传递给 Signal 对象，Signal 通过 Model 读取业务状态
- 提供 `update_signals()` 方法，根据 SignalChangeEvent 增量更新已有 Signal 对象
- 保存/恢复 UI 状态（selected、visible、view_index、v_offset、own_height）以避免全量重建时丢失 UI 状态

SignalFactory SHALL NOT 处理 DecodeTrace/SpectrumTrace/MathTrace 的创建——这些由 View 层直接管理。

#### Scenario: 信号列表变更时 View 层自动更新
- **WHEN** SigSession 的 SignalModel 列表发生变更
- **THEN** View 层通过 `signals_changed` 回调获知变更
- **AND** 调用 `SignalFactory::update_signals()` 增量更新 Signal 对象
- **AND** UI 状态（选中、可见性、偏移等）通过 channel_index 映射保留

### Requirement: add_decoder 创建责任拆分

`SigSession::add_decoder()` SHALL 只创建 DecoderStack（Core 层对象），不创建 DecodeTrace（View 层对象）。

新签名：
```cpp
bool SigSession::add_decoder(
    srd_decoder *dec, bool silent, DecoderStatus *dstatus,
    std::list<pv::data::decode::Decoder *> &sub_decoders,
    data::DecoderStack *&out_stack);
```

行为：
1. 创建 DecoderStack（包含解码器栈）
2. 设置 probes（空映射）
3. 添加子解码器
4. 将 DecoderStack 存入 SessionDocument 的 `_decoder_stacks`
5. 如果非 silent 且有 view 数据，添加解码任务
6. `out_stack = decoder_stack`

**不执行**：
- 创建 DecodeTrace（View 层职责）
- 调用 `create_popup()`（View 层职责）
- 调用 `signals_changed()`（View 层会自行刷新）

GUI 模式下 View 层在调用 `add_decoder()` 后：
1. 从 `out_stack` 获取 DecoderStack
2. 创建 DecodeTrace 并关联
3. 调用 `trace->create_popup()` 弹窗配置
4. 触发布局更新

Headless 模式下 SessionService：
1. 调用 `add_decoder()` 获取 DecoderStack
2. 通过 API 参数设置 channel_map（不弹窗）

#### Scenario: GUI 模式添加解码器
- **WHEN** 用户在 ProtocolDock 点击"添加解码器"
- **THEN** View 调用 `_session->add_decoder()` 获取 DecoderStack
- **AND** View 创建 DecodeTrace 并关联到 DecoderStack
- **AND** DecodeTrace 弹出配置窗口
- **AND** 解码器轨道正常显示

#### Scenario: Headless 模式添加解码器
- **WHEN** MCP API 调用 `add_decoder(decoder_id, channel_map, options)`
- **THEN** SessionService 调用 SigSession 创建 DecoderStack
- **AND** 不创建 DecodeTrace
- **AND** 不弹窗
- **AND** channel_map 通过 API 参数设置

### Requirement: SessionService 去视图化

SessionService SHALL 移除对 `view::View` 的直接依赖。

具体移除：
- `void set_view(view::View *view)` → 移除
- `view::View *_view` → 移除

View 相关操作（show_region、zoom_fit、zoom_in、zoom_out）SHALL 通过 `IServiceEventListener` 事件机制通知 View 层。新增 `ServiceEvent` 枚举值：ViewShowRegion、ViewZoomFit、ViewZoomIn、ViewZoomOut。

#### Scenario: SessionService 在 Headless 模式下工作
- **WHEN** 应用以 `--headless` 模式启动
- **THEN** SessionService 的所有非 View 操作正常工作
- **AND** View 操作返回成功但不执行实际操作

### Requirement: 线程安全与死锁防护

去视图化后，SigSession 的 `add_decoder()` 不再需要创建 `view::DecodeTrace`（QObject），因此不再强制要求在主线程执行。SessionService SHALL 移除所有 `QMetaObject::invokeMethod(qApp, ..., Qt::BlockingQueuedConnection)` 调用。

对于 `wait_capture_complete()` 和解码等待中的 `QEventLoop`：
- Headless 模式下 SHALL 改用 `std::condition_variable` + `std::mutex`
- GUI 模式下可保留 `QEventLoop`

#### Scenario: MCP 线程调用 add_decoder 不死锁
- **WHEN** 主线程正在 `wait_capture_complete()` 中，同时 MCP 线程调用 `add_decoder()`
- **THEN** `add_decoder()` 通过 mutex 保护直接操作 SigSession，不使用 invokeMethod
- **AND** 不产生死锁

### Requirement: libsigrok 回调 API 扩展

当前 `ds_set_datafeed_callback` 的回调签名没有 `void*` 用户数据参数，导致 SigSession 需要静态实例 `_session`。SHALL 在 libsigrok 中新增带上下文的回调注册 API：

- `ds_set_datafeed_callback_ex(ds_datafeed_callback_ex_t cb, void *user_data)`
- `ds_set_event_callback_ex(dslib_event_callback_ex_t cb, void *user_data)`

SigSession 使用新 API 注册回调，传入 `this` 指针，移除 `_session` 静态成员。

### Requirement: Headless 模式

系统 SHALL 支持 `--headless` 启动参数，在无 GUI 环境下运行。

Headless 模式 SHALL：
- 使用 `QCoreApplication` 而非 `QApplication`
- 不创建 MainFrame、MainWindow 或任何 QWidget
- 正常初始化 SigSession、DeviceAgent、SessionService
- 正常启动 WsTransport(:10430) 和 McpTransport(:10110)
- 支持完整的采集、解码、导出流程
- 支持 `--ws-port` 和 `--mcp-port` 参数自定义端口

#### Scenario: Headless 模式启动
- **WHEN** 用户执行 `PXView.exe --headless --ws-port 10430 --mcp-port 10110`
- **THEN** 应用启动后不创建任何窗口
- **AND** MCP 和 WS API 正常可用

#### Scenario: Headless 模式下采集并导出
- **WHEN** 通过 MCP API 发送 `start_capture` → `wait_capture` → `export_raw_data_csv`
- **THEN** 数据正常采集、解码、导出
- **AND** 不触发任何 QWidget 相关代码

#### Scenario: Headless + WS 新 GUI 客户端
- **WHEN** PXView 以 `--headless` 模式运行，Web GUI 客户端通过 WS 连接
- **THEN** Web GUI 可通过 WS API 完成完整交互流程
- **AND** WS 客户端可读取波形原始数据并接收实时事件推送

### Requirement: CMake 库分离

构建系统 SHALL 将 Core 层编译为独立静态库 `pxview-core`。

`pxview-core` SHALL 包含：
- SigSession、DeviceAgent、SessionData
- SignalModel、LissajousModel
- DecoderStack、SpectrumStack、MathStack（已是 Core 层对象）
- DataSource 接口（去视图化后）
- SessionDocument、SessionSnapshot（去视图化后）
- SessionService、RpcDispatcher、Transport 层

`pxview-core` SHALL NOT 依赖 Qt Widgets 模块，仅依赖 Qt Core（QObject、QString 等）。

### Requirement: MathStack 去视图化

实施 Task 4 时发现：`MathStack` 构造函数当前接收 `view::DsoSignal*` 参数用于读取 vdiv/hw_offset/vfactor 等 DSO 参数。这阻断了 `SigSession::math_rebuild()` 在 Core 层创建 MathStack 的路径（当前已加 TODO 注释跳过）。

MathStack SHALL 修改构造函数，不再接收 `view::DsoSignal*`，改为接收通道索引 + 通过 SignalModel 获取 DSO 参数：
- 构造签名变更为 `MathStack(SigSession*, int channel_index, ...)` 或类似
- 通过 `_session->get_signal_models()` 查找匹配的 SignalModel
- 从 SignalModel 读取 vdiv()、vfactor()、hw_offset()（依赖 Task 18 的属性同步验证）

完成此要求后，`SigSession::math_rebuild()` SHALL 移除 TODO 注释，完整实现 MathStack 创建。

#### Scenario: math_rebuild 在 Core 层创建 MathStack
- **WHEN** 用户或 API 触发数学运算重建
- **THEN** `SigSession::math_rebuild()` 直接创建 MathStack
- **AND** 不依赖任何 view::DsoSignal
- **AND** MathTrace 由 View 层后续创建并关联到 MathStack

### Requirement: SignalModel 属性同步验证

实施过程中发现：`SignalModel` 暴露的 `vdiv()`、`vfactor()`、`hw_offset()` 等方法需要与原 `view::DsoSignal` 的 `get_vDialValue()`、`get_factor()`、`get_hw_offset()` 返回值保持一致。当前未经验证，存在隐性数据不一致风险。

SignalModel 的 DSO 相关属性 SHALL 在以下时机从 DeviceAgent 正确填充：
- `init_signals()` 创建 SignalModel 时
- 设备配置变更触发 `device_options_updated` 后
- DSO 垂直档位/耦合/偏移变化时

验证要点：
- `SignalModel::vdiv()` ↔ `DsoSignal::get_vDialValue()` 一致
- `SignalModel::vfactor()` ↔ `DsoSignal::get_factor()` 一致
- `SignalModel::hw_offset()` ↔ `DsoSignal::get_hw_offset()` 一致

#### Scenario: SignalModel 属性与 DsoSignal 一致
- **WHEN** 用户在 UI 中修改 DSO 通道 vdiv
- **THEN** SignalModel.vdiv() 返回新值
- **AND** View 层通过 SignalModel 读取，无需直接访问 DsoSignal

### Requirement: Dialog/Dock View 访问路径规范化

实施 Task 11 时发现：Dialog 和 Dock 组件对 View 的访问路径不统一，部分组件无 View 引用（如 mathoptions、TrigBar），但需要 DSO 特有的方法（如 `set_show()`、`math_rebuild()`）。

规范化策略：
- **简单情况（仅需 name/index/color）**：Dialog/Dock 通过 `_session->get_signal_models()` 读取 SignalModel
- **复杂情况（需 DsoSignal 特有方法）**：
  - 若 Dialog 已有 View 引用（如 dsomeasure.cpp 有 `View&`），使用 `_view->get_own_signals()` 获取 view::Signal
  - 若 Dialog 无 View 引用（如 mathoptions、TrigBar），暂时加 TODO 注释跳过相关功能，待 Task 17 完成后或重构 Dialog 签名后再启用
- **Trace 派生类的 paint()**：通过 `_view->get_own_signals()` 获取已渲染的 Signal（_view 在构造后已设置）

新增 View 访问方法（已在 Task 9.12 完成）：
- `get_own_signals()` — View 自己持有的 Signal 列表
- `get_own_decode_traces()` — View 自己持有的 DecodeTrace 列表
- `get_own_spectrum_traces()` — View 自己持有的 SpectrumTrace 列表
- `get_own_math_trace()` / `get_own_lissajous_trace()` — 单一对象访问

#### Scenario: 无 View 引用的 Dialog 优雅降级
- **WHEN** mathoptions 对话框需要触发 math_rebuild
- **THEN** 当前先加 TODO 注释跳过该调用
- **AND** 其他简单配置（如 radio button 列表）通过 SignalModel 仍可正常工作

### Requirement: View 派生对象懒同步机制

实施 Task 9 时发现：Core 层的 Stack 对象（DecoderStack/SpectrumStack/MathStack）由 Core 持有，但对应的 Trace 渲染对象由 View 持有。当 Core 层变更 Stack 列表后，View 层需要一种机制将新 Stack 同步为 Trace。

View SHALL 提供 `mark_derived_traces_dirty()` / `sync_derived_traces()` 懒同步机制：
- Core 层变更 Stack 后调用 `mark_derived_traces_dirty()` 标记需同步
- View 在下次 `paint` 或 `get_traces()` 前调用 `sync_derived_traces()` 完成增量同步
- 按指针身份比对，保留已存在的 Trace，只增量创建/删除变更部分

此机制避免全量重建 Trace 导致的 UI 状态丢失和性能开销。

#### Scenario: 增量同步 Trace
- **WHEN** Core 层添加新 DecoderStack
- **THEN** View 调用 sync_derived_traces() 检测到新指针
- **AND** 只为新 Stack 创建 DecodeTrace
- **AND** 现有 DecodeTrace 保持不变（保留 UI 状态）

## MODIFIED Requirements

### Requirement: ISessionCallback 接口拆分

现有 `ISessionCallback`（17 个方法）SHALL 拆分为更小的独立接口：
- `IDataCallback`：`data_updated()`、`receive_data_len()`、`receive_header()`
- `ICaptureCallback`：`frame_began()`、`frame_ended()`、`update_capture()`
- `ITriggerCallback`：`receive_trigger()`、`show_wait_trigger()`
- `ISessionStateCallback`：`session_error()`、`session_save()`、`signals_changed()`、`decode_done()`

**不保留向后兼容**：`ISessionCallback` 不再继承所有子接口。`MainWindow` 直接实现需要的子接口。当前唯一实现者是 `MainWindow`，没有第三方实现者，向后兼容是冗余的。

## REMOVED Requirements

### Requirement: DecodeModel 纯数据模型
**Reason**: DecoderStack 已是 Core 层对象（pv::data::decode 命名空间，继承 QObject 只依赖 Qt Core），已包含实例 ID、名称、通道映射、选项、进度、解码结果等全部信息。引入 DecodeModel 会导致信息冗余和潜在的同步问题（cppverdebug 中 DecodeModel 持有 DecoderStack* 是反向引用，且 View 层无法从 DecodeModel 完整重建 DecodeTrace）。
**Migration**: DataSource 接口使用 `get_decoder_stacks()` 返回 `vector<DecoderStack*>&`，View 层直接从 DecoderStack 创建 DecodeTrace。

### Requirement: SpectrumModel 纯数据模型
**Reason**: SpectrumStack 已是 Core 层对象，包含通道索引、启用状态、频谱数据等全部信息。cppverdebug 中 SpectrumModel 不持有 SpectrumStack*，导致 View 层无法重建 SpectrumTrace（TODO stub）。
**Migration**: DataSource 接口使用 `get_spectrum_stacks()` 返回 `vector<SpectrumStack*>&`。

### Requirement: MathModel 纯数据模型
**Reason**: MathStack 已是 Core 层对象，包含运算类型、通道索引、数学数据等全部信息。cppverdebug 中 MathModel 不持有 MathStack*，导致 View 层无法重建 MathTrace（TODO stub）。
**Migration**: DataSource 接口使用 `get_math_stack()` 返回 `MathStack*`。

### Requirement: 独立 CLI 可执行文件
**Reason**: 优先级低于 MCP/WS API 和 Headless 模式。
**Migration**: 通过 WS API 编写 Python 脚本即可。

## 实施进度与发现（Implementation Notes）

本节记录实施过程中已完成的进度和发现的关键问题，对应 `tasks.md` 中的状态标记。

### 已完成（Phase 1-3 主体）

- ✅ SignalModel 纯数据模型（Task 1）
- ✅ LissajousModel 纯数据模型（Task 2）
- ✅ DataSource 接口 v2（Task 3）
- ✅ SigSession 去视图化（Task 4，除 4.22 math_rebuild TODO）
- ✅ libsigrok 回调 API 扩展（Task 5，5.4 待迁移）
- ✅ SessionDocument 去视图化（Task 6）
- ✅ SessionSnapshot 去视图化（Task 7）
- ✅ SignalFactory（Task 8，含 Qt `signals` 关键字冲突修复）
- ✅ View 派生对象持有与懒同步机制（Task 9 部分：9.1-9.4, 9.9-9.12）

### 进行中

- ⏳ Task 9 剩余（9.5-9.8）：View::add_decoder/remove_decoder/on_signals_changed/data_updated 完整实现
- ⏳ Task 11：Dock/Dialog 适配（11.1 已完成，11.2-11.9 待实现）

### 关键发现

1. **Qt `signals` 宏冲突**（已修复，Task 8.7）
   - 问题：Qt 定义 `signals` 宏为 `protected`，导致 `std::vector<Signal*> signals;` 在包含 QObject 头文件后被预处理为 `std::vector<Signal*> protected;`，触发 `expected unqualified-id before 'public'` 编译错误
   - 教训：在涉及 Qt 的代码中，避免使用 `signals`、`slots`、`emit`、`foreach` 作为变量/参数名
   - 修复：将 `signals` 变量改名为 `result`（create_signals）和 `sig_list`（save_ui_state/restore_ui_state）

2. **MathStack 构造依赖 view::DsoSignal***（待解决，Task 17）
   - 问题：`SigSession::math_rebuild()` 无法在 Core 层创建 MathStack，因 MathStack 构造需要 DSO 参数（vdiv/hw_offset/vfactor），当前只能从 view::DsoSignal 获取
   - 临时方案：加 TODO 注释跳过 math_rebuild 调用
   - 永久方案：MathStack 改为接收通道索引 + 从 SignalModel 读取 DSO 参数

3. **SignalModel 属性同步未验证**（待验证，Task 18）
   - 风险：SignalModel.vdiv()/vfactor()/hw_offset() 与原 DsoSignal.get_vDialValue()/get_factor()/get_hw_offset() 的返回值是否一致，未做对比验证
   - 需在 init_signals() 和设备配置变更时确保 SignalModel 属性从 DeviceAgent 正确填充

4. **Dialog View 访问路径不统一**（待规范化，Task 19）
   - 现象：mathoptions、TrigBar 等组件无 View 引用但需调用 DsoSignal 方法
   - 务实策略：简单情况用 SignalModel，复杂情况加 TODO 跳过；后续按 Task 17 完成情况再启用

5. **View 懒同步机制**（已实施，Task 9.11）
   - 解决：Core 层 Stack 变更后 View 层按需增量同步 Trace，避免全量重建丢失 UI 状态
   - 实现：`mark_derived_traces_dirty()` + `sync_derived_traces()` 按指针身份增量比对
