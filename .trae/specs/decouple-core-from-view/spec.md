# PXView 架构解耦：Core/View 分离与 Headless 模式 Spec

## Why

当前 SigSession 和 SessionDocument 直接持有 `view::Signal*`、`view::DecodeTrace*` 等 View 层对象，DataSource 接口也返回 View 类型，导致核心业务逻辑无法脱离 Qt Widgets 独立运行。这使得 Headless 模式、MCP/WS API 的无 GUI 运行、以及第三方客户端开发都受到根本性阻碍。

## 架构定位：三种客户端，统一服务层

```
AI Agent   ──→ MCP (HTTP :10110)      ──→ ISessionService ──→ SigSession
新 GUI     ──→ WS  (WebSocket :10430) ──→ ISessionService ──→ SigSession
原生 Qt GUI ──→ DirectTransport (进程内) ──→ ISessionService ──→ SigSession
```

三种客户端共享同一个 `ISessionService` 接口，仅传输层不同：

| 传输层 | 协议 | 目标客户端 | 核心能力 |
|--------|------|-----------|---------|
| **McpTransport** | HTTP + SSE | AI Agent | 采集控制、解码、导出；AI 工具链原生支持 HTTP |
| **WsTransport** | WebSocket (JSON-RPC 2.0) | 新 GUI 客户端 | 全功能：控制 + 波形数据读取 + 实时事件推送 + 解码注解 |
| **DirectTransport** | 进程内直连 | 原生 Qt GUI | 零开销函数调用，等同于直接调 ISessionService |

**WS 定位为新 GUI 客户端的通信通道**：WS API 已覆盖 ISessionService 的全部功能（21 个功能分组），包括波形数据读取（`get_logic_waveform`/`get_analog_waveform`/`get_dso_waveform`）、解码注解读取（`get_analyzer_results`）、实时事件推送（采集状态变更、解码进度、设备变更等），足以支撑一个完整的远程 GUI 客户端（Web/Qt/Flutter 等任意技术栈）。

## What Changes

- **重新设计 DataSource 接口**：返回纯数据模型而非 View 对象，View 层从数据模型自行创建渲染对象
- **引入 SignalModel 中间层**：不依赖 Qt Widgets 的信号业务模型，持有完整的信号状态（类型、参数、启用状态、触发配置等）
- **SigSession 去视图化**：移除 `view::Signal*`、`view::DecodeTrace*`、`view::SpectrumTrace*`、`view::LissajousTrace*`、`view::MathTrace*` 等成员，改为持有 SignalModel
- **SessionDocument 去视图化**：同步移除 View 依赖，与 SigSession 保持一致
- **SessionService 去视图化**：移除 `set_view(view::View*)` 和 `_view` 成员，View 操作通过独立接口提供
- **View 层 SignalFactory**：新增工厂类，从 SignalModel 创建 View 层 Signal/Trace 对象
- **Headless 模式**：`main.cpp` 支持 `--headless` 参数，使用 QCoreApplication 而非 QApplication
- **CMake 库分离**：将 Core 层编译为独立静态库 `pxview-core`，不依赖 Qt Widgets

### **BREAKING** Changes

- `DataSource` 接口签名变更：`get_signals()` 返回类型从 `vector<view::Signal*>&` 变为 `vector<SignalModel*>&`
- `SigSession::init_signals()` 不再创建 View 对象，改为创建 SignalModel
- `SessionService::set_view()` 移除，View 操作通过新的 `IViewService` 接口提供
- 所有调用 `_session->get_signals()` 并做 `static_cast<view::LogicSignal*>` 的代码需要适配

## Impact

- Affected specs: add-api-service-layer, add-multi-tab-sessions, add-mcp-sidebar-entry
- Affected code:
  - `PXView/pv/data/datasource.h` — 接口重设计
  - `PXView/pv/sigsession.h/cpp` — 移除 View 成员，改用 SignalModel
  - `PXView/pv/data/sessiondocument.h/cpp` — 同步移除 View 依赖
  - `PXView/pv/data/sessionsnapshot.h/cpp` — 同步移除 View 依赖（第三个 DataSource 实现者）
  - `PXView/pv/api/session_service.h/cpp` — 移除 View 依赖，消除 invokeMethod 死锁风险
  - `PXView/pv/storesession.cpp` — 10 处 get_signals() 调用适配（最大消费者之一）
  - `PXView/pv/view/` — 所有从 DataSource 读取数据的代码适配
  - `PXView/pv/dock/` — 所有 Dock 组件适配
  - `PXView/pv/dialogs/` — 所有对话框适配
  - `PXView/main.cpp` — Headless 模式支持
  - `CMakeLists.txt` — 库分离
  - `libsigrok/lib_main.c` — 新增带 void* 的回调注册 API

## ADDED Requirements

### Requirement: SignalModel 纯数据模型层

系统 SHALL 提供不依赖 Qt Widgets 的 SignalModel 类，持有信号的完整业务状态。

SignalModel SHALL 包含以下属性：
- 通道索引、名称、类型（使用现有 `api::ChannelType` 枚举）
- 启用/禁用状态
- 探头配置（vdiv、coupling、vfactor、map_default）
- 触发状态（可读写）：触发类型枚举（NONTRIG/POSTRIG/NEGTRIG 等），通过 `commit_trig()` 同步到 libsigrok
- DSO 特有参数（垂直偏移、零点偏移、hw_offset）
- 信号颜色（使用 `std::string` 而非 `QColor`）
- 毛刺过滤/信号反转状态

SignalModel 对触发状态的拥有权模型：SignalModel 是触发配置的**权威持有者**，View 层的 Signal 对象从 SignalModel 读取触发状态进行渲染，用户在 UI 中修改触发后通过 SignalModel 写入并调用 `commit_trig()` 同步到 libsigrok 全局 API。

SignalModel SHALL NOT 依赖任何 `pv::view::*` 类型或 Qt Widgets 头文件。

#### Scenario: SigSession 创建 SignalModel 而非 View 对象
- **WHEN** `SigSession::init_signals()` 被调用
- **THEN** 创建 `SignalModel` 对象并存入 `_signal_models` 向量
- **AND** 不创建任何 `view::Signal*` 对象

#### Scenario: View 层从 SignalModel 创建 Signal
- **WHEN** View 层需要渲染信号
- **THEN** 通过 `SignalFactory::create_signals(DataSource*)` 从 SignalModel 创建对应的 `view::Signal` 子类
- **AND** Signal 对象持有 SignalModel 的引用以读取业务状态

### Requirement: DataSource 接口去视图化

系统 SHALL 重新设计 `DataSource` 接口，不返回任何 `view::*` 类型。

新的 DataSource 接口 SHALL 提供：
- `get_signal_models()` → `vector<SignalModel*>&`
- `get_decode_models()` → `vector<DecodeModel*>&`（纯数据的解码器描述）
- `get_logic_snapshot()` / `get_analog_snapshot()` / `get_dso_snapshot()` — 保持不变
- `cur_snap_samplerate()` / `cur_samplelimits()` / `cur_sampletime()` — 保持不变
- `get_trigger_pos()` — 保持不变
- `get_decoder_model()` — 保持不变

DataSource 接口 SHALL NOT 包含任何 `view::Signal*`、`view::DecodeTrace*`、`view::SpectrumTrace*`、`view::LissajousTrace*`、`view::MathTrace*` 的返回值。

#### Scenario: View 层通过新接口获取数据
- **WHEN** View 层调用 `effective_data_source()->get_signal_models()`
- **THEN** 返回 `vector<SignalModel*>&`，View 层据此创建渲染对象
- **AND** 不再需要 `static_cast<view::LogicSignal*>` 等类型转换

### Requirement: DecodeModel 纯数据模型

系统 SHALL 提供 DecodeModel 类，作为 DecodeTrace 的纯数据对应物。

DecodeModel SHALL 包含：
- 解码器实例 ID、解码器名称、显示名称
- 通道映射（decoder_channel → signal_index）
- 解码器选项
- 解码进度
- 解码结果引用（通过 DecoderModel 访问）

DecodeModel SHALL NOT 依赖 `pv::view::*` 类型。

### Requirement: SigSession 去视图化

SigSession SHALL 移除所有 `view::*` 类型的成员变量和直接依赖。

具体移除：
- `std::vector<view::Signal*> _signals` → 替换为 `std::vector<SignalModel*> _signal_models`
- `std::vector<view::SpectrumTrace*> _spectrum_traces` → 替换为 `std::vector<SpectrumModel*> _spectrum_models`
- `view::LissajousTrace* _lissajous_trace` → 替换为 `LissajousModel* _lissajous_model`
- `view::MathTrace* _math_trace` → 替换为 `MathModel* _math_model`
- `#include "view/mathtrace.h"` → 移除

SigSession SHALL NOT 直接 `#include` 任何 `pv/view/*.h` 头文件（前向声明除外）。

#### Scenario: Headless 模式下 SigSession 可独立运行
- **WHEN** 应用以 `--headless` 模式启动
- **THEN** SigSession 可以正常初始化、采集、解码、导出数据
- **AND** 不触发任何 QWidget 创建或 Qt Widgets 库依赖

### Requirement: SessionDocument 去视图化

SessionDocument SHALL 移除所有 `view::*` 类型的成员变量。

具体移除：
- `std::vector<view::DecodeTrace*> _decode_traces` → 替换为 `std::vector<DecodeModel*> _decode_models`
- `std::vector<view::Signal*> _signals` → 替换为 `std::vector<SignalModel*> _signal_models`
- `std::vector<view::SpectrumTrace*> _spectrum_traces` → 替换为 `std::vector<SpectrumModel*> _spectrum_models`

#### Scenario: SessionDocument 可在无 GUI 环境下加载和保存
- **WHEN** 通过 MCP API 调用 `load_file()` / `save_file()`
- **THEN** SessionDocument 正常工作，不依赖任何 View 对象

### Requirement: SessionSnapshot 去视图化

SessionSnapshot 也实现了 `DataSource` 接口，同样持有 `view::*` 类型成员，SHALL 同步去视图化。

具体移除：
- `std::vector<view::Signal*> _signals` → 替换为 `std::vector<SignalModel*> _signal_models`
- `std::vector<view::DecodeTrace*> _decode_traces` → 替换为 `std::vector<DecodeModel*> _decode_models`
- `std::vector<view::SpectrumTrace*> _spectrum_traces` → 替换为 `std::vector<SpectrumModel*> _spectrum_models`
- `view::LissajousTrace* _lissajous_trace` → 替换为 `LissajousModel* _lissajous_model`
- `view::MathTrace* _math_trace` → 替换为 `MathModel* _math_model`

#### Scenario: SessionSnapshot 可在无 GUI 环境下从文件加载数据
- **WHEN** 通过 MCP API 加载历史会话文件
- **THEN** SessionSnapshot 正常创建和填充，不依赖任何 View 对象

### Requirement: SessionService 去视图化

SessionService SHALL 移除对 `view::View` 的直接依赖。

具体移除：
- `void set_view(view::View *view)` → 移除
- `view::View *_view` → 移除
- `#include <QApplication>` → 替换为 `#include <QCoreApplication>`

View 相关操作（show_region、zoom_fit、zoom_in、zoom_out）SHALL 通过 `IServiceEventListener` 事件机制通知 View 层。具体新增以下 `ServiceEvent` 枚举值：
- `ViewShowRegion` — 携带 start_sample 和 end_sample 参数
- `ViewZoomFit`
- `ViewZoomIn`
- `ViewZoomOut`

MainWindow 注册为 `IServiceEventListener`，在 `on_service_event()` 中处理这些事件并调用 View 的对应方法。

#### Scenario: SessionService 在 Headless 模式下工作
- **WHEN** 应用以 `--headless` 模式启动
- **THEN** SessionService 的所有非 View 操作正常工作
- **AND** View 操作（zoom_fit 等）返回 `Result<void>::Success()` 但不执行实际操作

### Requirement: View 层 SignalFactory

系统 SHALL 提供 `SignalFactory` 类，负责从 SignalModel 创建 View 层 Signal 对象。

SignalFactory SHALL：
- 接收 `vector<SignalModel*>&` 输入
- 根据 `SignalModel::type` 创建对应的 `view::LogicSignal`、`view::AnalogSignal`、`view::DsoSignal`
- 将 SignalModel 指针传递给 Signal 对象，Signal 通过 Model 读取业务状态
- 提供 `update_signals()` 方法，当 SignalModel 变更时更新已有 Signal 对象

#### Scenario: 信号列表变更时 View 层自动更新
- **WHEN** SigSession 的 SignalModel 列表发生变更（如设备模式切换）
- **THEN** View 层通过 `signals_changed` 回调获知变更
- **AND** 调用 `SignalFactory::update_signals()` 更新 View 层的 Signal 对象

### Requirement: Headless 模式

系统 SHALL 支持 `--headless` 启动参数，在无 GUI 环境下运行。

Headless 模式 SHALL：
- 使用 `QCoreApplication` 而非 `QApplication`
- 不创建 MainFrame、MainWindow 或任何 QWidget
- 正常初始化 SigSession、DeviceAgent、SessionService
- 正常启动 WsTransport(:10430) 和 McpTransport(:10110)
- 支持完整的采集、解码、导出流程

Headless 模式 SHALL 支持 `--ws-port` 和 `--mcp-port` 参数自定义端口。

#### Scenario: Headless 模式启动
- **WHEN** 用户执行 `PXView.exe --headless --ws-port 10430 --mcp-port 10110`
- **THEN** 应用启动后不创建任何窗口
- **AND** MCP 和 WS API 正常可用
- **AND** 可以通过 API 完成采集、解码、导出操作

#### Scenario: Headless 模式下采集并导出
- **WHEN** 通过 MCP API 发送 `start_capture` → `wait_capture` → `export_raw_data_csv`
- **THEN** 数据正常采集、解码、导出
- **AND** 不触发任何 QWidget 相关代码

#### Scenario: Headless 模式 + WS 新 GUI 客户端
- **WHEN** PXView 以 `--headless` 模式运行，同时一个 Web GUI 客户端通过 WS 连接
- **THEN** Web GUI 可通过 WS API 完成完整的交互流程：设备选择 → 通道配置 → 采集 → 波形渲染 → 解码 → 注解显示
- **AND** WS 客户端可接收实时事件推送（采集状态、解码进度等）
- **AND** WS 客户端可读取波形原始数据进行本地渲染
- **AND** Core 进程不创建任何 QWidget

### Requirement: CMake 库分离

构建系统 SHALL 将 Core 层编译为独立静态库 `pxview-core`。

`pxview-core` 库 SHALL 包含：
- SigSession、DeviceAgent、SessionData
- SignalModel、DecodeModel、SpectrumModel、LissajousModel、MathModel
- DataSource 接口（去视图化后）
- SessionDocument（去视图化后）
- SessionService、RpcDispatcher、Transport 层
- libsigrok、libsigrokdecode、common 的链接

`pxview-core` SHALL NOT 依赖 Qt Widgets 模块。

最终可执行文件 SHALL 链接 `pxview-core` + Qt Widgets 相关代码。

#### Scenario: pxview-core 可独立编译
- **WHEN** 执行 CMake 配置仅包含 `pxview-core` 目标
- **THEN** 编译成功，不出现 Qt Widgets 相关的链接错误

### Requirement: 线程安全与死锁防护

系统 SHALL 明确各层的线程模型，并消除 Core 模式下的死锁风险。

**线程模型**：
- **libsigrok 数据回调**：在采集线程中触发，通过 `std::mutex` 保护共享数据
- **ISessionCallback 回调**：在采集线程中触发，实现者必须自行处理线程安全（MainWindow 通过 Qt 信号槽转到主线程）
- **SessionService API 调用**：在调用者线程中执行，内部通过 mutex 保护 SigSession 访问
- **IServiceEventListener 事件广播**：通过 mutex 保护 listener 列表，回调在触发线程中执行

**死锁风险分析与消除**：

当前 SessionService 中存在 3 处 `QMetaObject::invokeMethod(qApp, ..., Qt::BlockingQueuedConnection)` 调用（add_decoder、stack_decoder、remove_decoder），它们从非主线程向主线程同步派发操作。这种模式在以下场景会产生死锁：

1. **主线程正在 QEventLoop 中等待**（如 `wait_capture_complete()`），同时 MCP 线程调用 `add_decoder()` → `invokeMethod(BlockingQueuedConnection)` → 主线程事件循环被 QEventLoop 占据，无法处理 invokeMethod 的队列消息 → 死锁
2. **嵌套 QEventLoop**：`add_decoder` 的 `wait_for_completion` 分支内部又启动 QEventLoop 等待解码完成，如果此时另一个 API 调用触发了 `invokeMethod` → 死锁
3. **Headless 模式**：`QCoreApplication` 的事件循环行为与 `QApplication` 不同，`invokeMethod(BlockingQueuedConnection)` 在 Headless 模式下可能无法正常工作（因为 `qApp` 对象类型不同）

**消除方案**：

去视图化后，SigSession 的 `add_decoder()` 不再需要创建 `view::DecodeTrace`（QObject），因此不再强制要求在主线程执行。SessionService SHALL 移除所有 `invokeMethod(BlockingQueuedConnection)` 调用，改为：
- 所有 SessionService 方法直接在调用者线程执行
- SigSession 的操作通过 `std::mutex` 保护线程安全
- 不再需要 `QThread::currentThread() == qApp->thread()` 的线程检查

对于 `wait_capture_complete()` 和解码等待中的 `QEventLoop`：
- Headless 模式下 SHALL 改用 `std::condition_variable` + `std::mutex` 替代 `QEventLoop`，避免对 Qt 事件循环的依赖
- GUI 模式下可保留 `QEventLoop`（因为需要处理 UI 事件），但 SHALL 确保不会在 QEventLoop 内部调用 `invokeMethod(BlockingQueuedConnection)`

对于 `QCoreApplication::processEvents()` 调用（当前 7 处）：
- `configure_and_start()` 中的 `processEvents()` 用于等待设备模式切换完成，SHALL 改为条件变量等待
- 其他 `processEvents()` 调用 SHALL 逐一评估是否可以替换为条件变量或信号量

#### Scenario: MCP 线程调用 add_decoder 不死锁
- **WHEN** 主线程正在 `wait_capture_complete()` 的 QEventLoop 中，同时 MCP 线程调用 `add_decoder()`
- **THEN** `add_decoder()` 通过 mutex 保护直接操作 SigSession，不使用 `invokeMethod`
- **AND** 不产生死锁

#### Scenario: Headless 模式下 wait_capture_complete 不依赖 QEventLoop
- **WHEN** Headless 模式下调用 `wait_capture_complete()`
- **THEN** 使用 `std::condition_variable` 等待采集完成
- **AND** 不依赖 `QEventLoop` 或 `processEvents()`

#### Scenario: 多客户端并发调用 API
- **WHEN** MCP 客户端和 WS 客户端同时调用 SessionService 方法
- **THEN** 通过 mutex 保证 SigSession 状态的一致性
- **AND** 不出现数据竞争

## MODIFIED Requirements

### Requirement: ISessionCallback 接口拆分

现有 `ISessionCallback`（17 个方法）SHALL 拆分为更小的接口：

- `IDataCallback`：`data_updated()`、`receive_data_len()`、`receive_header()`
- `ICaptureCallback`：`frame_began()`、`frame_ended()`、`update_capture()`
- `ITriggerCallback`：`receive_trigger()`、`show_wait_trigger()`
- `ISessionStateCallback`：`session_error()`、`session_save()`、`signals_changed()`、`decode_done()`

为保持向后兼容，`ISessionCallback` SHALL 继承所有子接口并提供默认空实现。

### Requirement: SigSession 静态实例移除

`SigSession::_session` 静态成员 SHALL 被移除。

当前 `ds_set_datafeed_callback` 的回调签名没有 `void*` 用户数据参数（`typedef void (*ds_datafeed_callback_t)(const struct sr_dev_inst *sdi, const struct sr_datafeed_packet *packet)`），因此需要在 libsigrok 中新增带上下文的回调注册 API：

- 在 `libsigrok/lib_main.c` 中新增 `ds_set_datafeed_callback_ex(ds_datafeed_callback_ex_t cb, void *user_data)`，其中 `ds_datafeed_callback_ex_t` 签名为 `void (*)(const struct sr_dev_inst*, const struct sr_datafeed_packet*, void*)`
- 同样新增 `ds_set_event_callback_ex(dslib_event_callback_ex_t cb, void *user_data)`
- SigSession 使用新 API 注册回调，传入 `this` 指针作为 `user_data`
- 静态回调函数从 `user_data` 恢复 SigSession 指针，调用实例方法

如果修改 libsigrok API 的成本过高，可暂时保留 `_session` 静态成员（当前单实例场景风险可控），将其降级为 P2 优先级。

## REMOVED Requirements

### Requirement: 独立 CLI 可执行文件
**Reason**: 优先级低于 MCP/WS API 和 Headless 模式。Headless 模式 + Python SDK 可以覆盖 CLI 的所有场景。
**Migration**: 如需脚本化，通过 WS API 编写 Python 脚本即可。
