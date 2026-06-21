# Qt应用解耦与CLI/新客户端

> **Workspace:** c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb

---

**User**

调查一下现在这个qt应用的整体架构，如果要和C:\Users\admin\Downloads\old\Logic_source_new那样有一个明显的解耦方便做cli或者编写新客户端的话应该怎么做

**Assistant**

我来调查当前项目的架构和参考项目的架构，分析解耦方案。

Now I have a thorough understanding of the architecture. Let me compile the analysis.

---

## PXView 应用架构耦合性深度分析

### 一、应用生命周期与入口点

**入口文件**: `c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\main.cpp`

启动流程为线性串联：

```
main() → AppControl::Instance() → control->Init() → MainFrame w → control->Start() → w.ShowFormInit() → a.exec() → control->Stop() → control->UnInit()
```

关键观察：
- `AppControl` 是全局单例（非线程安全的懒汉模式，第62-69行），在 `main.cpp` 第211行创建
- `AppControl` 构造函数中直接 `new SigSession()`（appcontrol.cpp 第49行），SigSession 是其私有成员
- `AppControl::Init()` 负责 libsigrok 和 libsigrokdecode 的初始化，并调用 `_session->init()`
- `AppControl::Start()` 调用 `_session->Open()` 并初始化 API 服务层（WsTransport/McpTransport/DirectTransport）
- `MainFrame` 在 `main()` 中栈上创建，内部持有 `MainWindow`

**耦合点**: `AppControl` 是全局唯一枢纽，持有 SigSession 指针，所有需要 Session 的组件都通过 `AppControl::Instance()->GetSession()` 获取。这是一个典型的"上帝对象"反模式。

---

### 二、GUI 与后端的耦合方式

#### 2.1 SigSession — 核心耦合节点

**文件**: `c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\pv\sigsession.h`

SigSession 实现了三个接口：
- `IMessageListener` — 接收广播消息
- `IDeviceAgentCallback` — 接收设备配置变更回调
- `DataSource` — 为 View 层提供数据

SigSession 内部直接持有：
- `DeviceAgent _device_agent` — 直接内嵌，非指针
- `std::vector<ISessionCallback*> _callbacks` — 回调列表
- `std::vector<IMessageListener*> _msg_listeners` — 消息监听列表
- `SessionData *_view_data` / `*_capture_data` — 双缓冲数据
- `data::SessionDocument *_active_document` — 当前活动文档
- `std::vector<view::Signal*> _signals` — 直接持有 View 层信号对象
- `std::vector<view::DecodeTrace*>` — 直接持有 View 层解码轨迹
- `view::LissajousTrace *_lissajous_trace` / `view::MathTrace *_math_trace` — View 层对象

**严重耦合**: SigSession 直接持有 `view::Signal`、`view::DecodeTrace`、`view::LissajousTrace`、`view::MathTrace`、`view::SpectrumTrace` 等 View 层对象。这意味着业务逻辑层（Session）与渲染层（View）是紧耦合的，SigSession 无法在没有 Qt View 层的情况下独立存在或测试。

#### 2.2 数据流：硬件采集到 UI 显示

完整数据流路径：

```
libsigrok (C层)
  ↓ ds_set_datafeed_callback(data_feed_callback)  [sigsession.cpp:180]
  ↓ 静态回调 data_feed_callback()                  [sigsession.cpp:1420-1423]
  ↓ _session->data_feed_in(sdi, packet)             [sigsession.cpp:1332]
  ↓ 根据 packet->type 分发:
  │   SR_DF_LOGIC  → feed_in_logic()  → _capture_data->get_logic()->append_payload()
  │   SR_DF_DSO    → feed_in_dso()    → _capture_data->get_dso()->append_payload()
  │   SR_DF_ANALOG → feed_in_analog() → _capture_data->get_analog()->append_payload()
  │   SR_DF_END    → 通知回调 frame_ended() / trigger_message()
  ↓
  ↓ for (auto* cb : _callbacks) cb->data_updated() / cb->frame_ended()
  ↓
MainWindow (ISessionCallback实现)
  ↓ data_updated() → on_data_updated() [Qt信号槽]
  ↓
View 层
  ↓ effective_data_source() → 获取 DataSource (SigSession 或 SessionDocument)
  ↓ 从 DataSource 读取 Snapshot 数据进行渲染
```

**关键耦合点**:
1. libsigrok 的数据回调是 C 函数指针，通过静态变量 `SigSession::_session` 路由到实例方法（sigsession.cpp 第98行和第1420-1423行），这是典型的 C/C++ 桥接模式，但使用了全局静态变量
2. `feed_in_logic/dso/analog` 直接将数据写入 `_capture_data` 的 Snapshot，然后通过 `ISessionCallback` 通知 UI
3. View 层通过 `effective_data_source()` 获取数据源——如果当前文档有数据则从文档读取，否则从 SigSession（作为 DataSource）读取

#### 2.3 MainWindow — 多重继承的"上帝窗口"

**文件**: `c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\pv\mainwindow.h`

MainWindow 多重继承：
- `QMainWindow` — Qt 窗口基类
- `ISessionCallback` — 17个回调方法
- `IMainForm` — 语言切换
- `ISessionDataGetter` — 生成会话数据
- `IMessageListener` — 接收广播消息

MainWindow 构造函数中（mainwindow.cpp 第260-263行）：
```cpp
_session = ::AppControl::Instance()->GetSession();
_session->add_callback(this);
_device_agent = _session->get_device();
_session->add_msg_listener(this);
```

MainWindow 同时持有 `_session`（SigSession*）和 `_device_agent`（DeviceAgent*），后者是 `_session->get_device()` 返回的内部指针。MainWindow 直接调用 `_device_agent->` 的方法超过30处，直接调用 `_session->` 的方法超过20处。

**严重耦合**: MainWindow 既是 UI 窗口，又是业务逻辑的回调接收者，还直接操作 DeviceAgent 和 SigSession。没有业务逻辑与 UI 的分离。

---

### 三、SigSession、DeviceAgent、MainWindow 的交互关系

#### 3.1 SigSession 与 DeviceAgent

**文件**: `c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\pv\deviceagent.h`

DeviceAgent 是 SigSession 的内嵌成员（`DeviceAgent _device_agent`），SigSession 构造函数中设置回调：
```cpp
_device_agent.set_callback(this);  // sigsession.cpp:152
```

DeviceAgent 实现了 `IDeviceAgentCallback` 接口（仅一个方法 `DeviceConfigChanged()`），当设备配置变更时通知 SigSession。

DeviceAgent 是对 `libsigrok` C API 的 C++ 封装，提供类型化的 get/set config 方法（`get_config_uint64`、`set_config_bool` 等），内部调用 `ds_*` 系列函数。

**耦合程度**: 高度耦合。DeviceAgent 是 SigSession 的值成员，SigSession 是 DeviceAgent 回调的唯一消费者。

#### 3.2 SigSession 与 MainWindow

交互方式有三条通道：

1. **ISessionCallback 回调链**（观察者模式）：SigSession 维护 `_callbacks` 列表，在数据更新、帧开始/结束、触发等事件时遍历调用。MainWindow 是主要注册者。

2. **IMessageListener 广播消息**（发布-订阅模式）：SigSession 维护 `_msg_listeners` 列表，通过 `broadcast_msg(int msg)` 发送 `DSV_MSG_*` 消息码。MainWindow::OnMessage() 处理约20种消息类型，直接操作各种 UI 组件。

3. **DeviceEventObject Qt信号**：SigSession 持有 `DeviceEventObject _device_event`，MainWindow 通过 `connect(_session->device_event_object(), &DeviceEventObject::device_updated, ...)` 连接。

**耦合程度**: 极高。MainWindow 的 `OnMessage()` 方法（mainwindow.cpp 第2726行起）是一个超过200行的 switch-case，直接操作 `_sampling_bar`、`_trigger_widget`、`_device_options_widget`、`_signal_processing_widget`、`_protocol_widget` 等十几个 UI 组件。SigSession 的回调方法也直接触发 MainWindow 的 UI 更新。

#### 3.3 MainWindow 与 DeviceAgent

MainWindow 持有 DeviceAgent 的裸指针（`_device_agent = _session->get_device()`），直接调用其方法查询设备状态、工作模式、驱动名称等。这是 MainWindow 跨过 SigSession 直接访问底层设备信息的通道。

---

### 四、interface/ 目录的抽象层分析

**文件**: `c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\pv\interface\icallbacks.h`

定义了6个接口：

| 接口 | 方法数 | 实现者 | 用途 |
|------|--------|--------|------|
| `ISessionCallback` | 17 | MainWindow | Session事件通知 |
| `ISessionDataGetter` | 1 | MainWindow | 生成会话数据字符串 |
| `IDlgCallback` | 1 | 匿名对话框 | 对话框结果回调 |
| `IMainForm` | 1 | MainWindow | 语言切换 |
| `IMessageListener` | 1 | MainWindow, SigSession | 广播消息 |
| `IDecoderPannel` | 2 | ProtocolDock | 解码器UI更新 |
| `IParentNativeEventCallback` | 1 | MainFrame | 平台原生事件 |

**文件**: `c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\pv\interface\icontextaware.h`

`IContextAware` 接口提供 `bind_context(TabContext*)` 和 `unbind_context()` 方法，用于将组件绑定到 TabContext。

**文件**: `c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\pv\data\datasource.h`

`DataSource` 接口定义了 View 层从数据源读取信号、快照、采样率等信息的标准方法。SigSession 和 SessionDocument 都实现了此接口。

**文件**: `c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\pv\deviceagent.h`

`IDeviceAgentCallback` 接口仅一个方法 `DeviceConfigChanged()`。

**抽象层评估**:
- 接口定义存在，但实现者极少——`ISessionCallback` 几乎只有 MainWindow 一个实现者，`IMessageListener` 也只有 MainWindow 和 SigSession 自身
- 接口粒度不够——`ISessionCallback` 有17个方法，违反了接口隔离原则
- `DataSource` 是最成功的抽象——View 层通过它读取数据，不关心数据来自 SigSession 还是 SessionDocument
- `IContextAware` 是一个良好的设计模式，但使用范围有限

---

### 五、TabContext 与多标签架构

**文件**: `c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\pv\tabcontext.h`

TabContext 绑定三个核心对象：
- `view::View *_view` — 渲染视图
- `SigSession *_session` — 数据源（所有标签共享同一个 SigSession）
- `data::SessionDocument *_document` — 数据存储

TabContext 有两种状态：`LIVE`（正在采集）和 `HISTORICAL`（查看已保存数据）。

**关键问题**: 所有 TabContext 共享同一个 SigSession 实例（来自 AppControl 单例）。这意味着多标签并非真正的多会话——切换标签时需要切换 SigSession 的活动文档和视图数据。SigSession 的 `_active_document` 和 `_view_data` 是单一状态，不支持并行采集。

SessionManager（`c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\pv\sessionmanager.h`）是管理 TabContext 注册表的单例，但本质上是 MainWindow 内部管理的辅助类。

---

### 六、新增 API 服务层

**文件**: `c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\pv\api\iapp_service.h` 和 `isession_service.h`

AppControl::Start() 中初始化了 API 服务层：
- `AppService` — 实现 `IAppService` 接口
- `RpcDispatcher` — RPC 请求分发
- `WsTransport` — WebSocket 传输（端口10430）
- `McpTransport` — MCP 传输（端口10110）
- `DirectTransport` — 进程内直连传输

`ISessionService` 是一个设计良好的接口，有21个功能分组、超过80个方法，覆盖了采集控制、设备信息、通道管理、采样配置、触发配置、解码器管理、数据导出等所有功能。这是项目中**最彻底的抽象层**——通过此接口可以完全控制 PXView 而不依赖任何 Qt UI 组件。

**但问题在于**: ISessionService 的实现（AppService）内部仍然直接操作 SigSession 和 DeviceAgent，而 SigSession 本身与 View 层紧耦合。API 层绕过了 UI，但底层架构的耦合问题并未解决。

---

### 七、架构耦合总结

#### 7.1 核心耦合点

| 耦合点 | 严重程度 | 描述 |
|--------|----------|------|
| SigSession 持有 View 层对象 | **极高** | `_signals`、`_spectrum_traces`、`_lissajous_trace`、`_math_trace` 等都是 `pv::view::*` 类型，业务逻辑与渲染完全混合 |
| MainWindow 多重角色 | **极高** | 同时是 UI 窗口、Session 回调接收者、消息监听者、数据生成者，直接操作 Session 和 DeviceAgent |
| 全局静态 SigSession 指针 | **高** | `SigSession::_session` 静态成员用于 C 回调桥接，不支持多实例 |
| DeviceAgent 裸指针暴露 | **高** | MainWindow 直接持有并操作 DeviceAgent，绕过 SigSession |
| ISessionCallback 接口过大 | **中** | 17个方法的胖接口，几乎所有实现者都是 MainWindow |
| 单一 SigSession 实例 | **中** | 所有标签共享一个 Session，无法并行采集 |
| OnMessage 巨型 switch | **中** | 200+ 行的消息分发，直接操作十几个 UI 组件 |

#### 7.2 业务逻辑与 UI 分离评估

**结论：几乎没有分离。**

- SigSession 既是数据采集控制器，又是信号/轨迹对象的持有者，还直接创建和管理 View 层的 Signal、DecodeTrace 等对象
- MainWindow 既是 UI 窗口，又是业务逻辑的回调处理中心
- 没有独立的"业务逻辑层"或"服务层"——SigSession 承担了业务逻辑角色但与 View 层深度耦合
- 唯一的正向设计是 `DataSource` 接口——View 层通过它多态地读取数据

#### 7.3 已有的解耦机制

1. **DataSource 接口** — View 层通过此接口读取数据，不关心数据来源（SigSession 或 SessionDocument），这是最成功的解耦点
2. **ISessionCallback / IMessageListener** — 提供了观察者模式的基础框架，但实现者单一，未发挥多态优势
3. **IDeviceAgentCallback** — DeviceAgent 通过此接口通知配置变更，SigSession 是唯一消费者
4. **IContextAware** — 组件与 TabContext 的绑定/解绑机制，设计良好但使用范围有限
5. **ISessionService / IAppService** — 新增的 API 层，提供了外部程序控制 PXView 的完整接口，是最彻底的抽象，但内部实现仍依赖紧耦合的底层

#### 7.4 数据流架构图

```
┌─────────────────────────────────────────────────────────────┐
│                    libsigrok (C11)                          │
│  USB设备 → ds_datafeed_callback → sr_datafeed_packet       │
└──────────────────────┬──────────────────────────────────────┘
                       │ C函数指针回调
                       ↓
┌─────────────────────────────────────────────────────────────┐
│              SigSession (核心耦合节点)                        │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐   │
│  │ DeviceAgent │  │ SessionData  │  │ view::Signal[]   │   │
│  │ (内嵌成员)   │  │ (双缓冲)      │  │ view::DecodeTr[] │   │
│  └──────┬──────┘  └──────┬───────┘  │ view::MathTrace  │   │
│         │                │          │ view::SpectrumTr[]│   │
│         │                │          └──────────────────┘   │
│  ISessionCallback ←──────┼──────── 数据更新通知             │
│  IMessageListener         │                                 │
│  IDeviceAgentCallback     │                                 │
│  DataSource (接口实现)     │                                 │
└─────────┬────────────────┬─────────────────────────────────┘
          │                │
    回调通知           DataSource接口
          │                │
          ↓                ↓
┌─────────────────────────────────────────────────────────────┐
│              MainWindow (UI + 业务混合)                      │
│  ISessionCallback: 17个回调 → 直接更新UI组件                  │
│  IMessageListener: OnMessage() → 200行switch操作UI          │
│  ISessionDataGetter: genSessionData()                       │
│  IMainForm: switchLanguage()                                │
│                                                             │
│  直接持有: _session, _device_agent, 十几个Dock/Toolbar       │
│  管理: TabContext列表, 标签页切换                             │
└─────────────────────────────────────────────────────────────┘
          │
          ↓
┌─────────────────────────────────────────────────────────────┐
│              View 层                                        │
│  View → Viewport → Signal/DecodeTrace/...                  │
│  通过 effective_data_source() → DataSource接口读取数据       │
│  (这是最成功的解耦点)                                        │
└─────────────────────────────────────────────────────────────┘
```

#### 7.5 改进方向建议

1. **将 View 层对象从 SigSession 中剥离**：SigSession 不应持有 `view::Signal*`、`view::DecodeTrace*` 等，应通过数据层（Snapshot/SessionDocument）间接提供数据，View 层自行创建和管理渲染对象
2. **拆分 ISessionCallback**：按职责拆分为 `IDataUpdateCallback`（数据更新）、`ICaptureStateCallback`（采集状态）、`ITriggerCallback`（触发事件）等小接口
3. **引入业务逻辑服务层**：在 SigSession 和 MainWindow 之间引入独立的 Service 层，封装采集控制、设备管理、解码器管理等业务逻辑，MainWindow 仅通过 Service 层交互
4. **消除 MainWindow 对 DeviceAgent 的直接访问**：所有设备操作应通过 SigSession 或 Service 层代理
5. **移除 SigSession 静态实例**：改用依赖注入或回调上下文传递，支持未来多实例场景

现在我对项目架构有了全面的了解。让我来整理分析结果。

---

## 项目架构深度分析：Saleae Logic 2 信号分析软件

这是 **Saleae Logic 2** 的源码——一款基于 Electron + React/TypeScript 的专业逻辑分析仪/示波器桌面应用。该项目在 GUI 与后端解耦方面采用了非常成熟的多层架构设计。以下从五个维度进行详细分析。

---

### 一、GUI 与后端/核心逻辑的分离

该项目实现了极其清晰的 **前后端分离**，核心机制是一个名为 **GraphClient-GraphServer** 的客户端-服务端架构。

**1. 后端核心：graph-io（C++ 原生库）**

后端是一个独立的 C++ 进程/库，称为 `graph_server_shared`（在 macOS 上是 `.dylib`，Windows 上是 `.dll`，Linux 上是 `.so`）。它负责：
- 硬件设备通信（USB 设备枚举、数据采集）
- 信号处理（数字/模拟信号存储、触发、毛刺过滤）
- 协议解码（低级分析器 Analyzer、高级分析器 HLA）
- 数据导出、序列化/反序列化

关键文件：`C:\Users\admin\Downloads\old\Logic_source_new\dist\main.js` 中的 `startGraphServer` 函数展示了后端启动逻辑——通过 `ffi-napi` 加载 C++ 动态库，调用 `CreateGraphServer` 创建服务端实例。

**2. 前端核心：GraphClient（TypeScript）**

前端通过 `GraphClient` 类与后端通信，该类定义在：
- `C:\Users\admin\Downloads\old\Logic_source_new\original_source\app\services\GraphClient\GraphClient.ts`

`GraphClient` 实现了 `GraphApi` 接口，提供两层 API：
- **低级 API**：直接通过 `GraphSocket` 发送/接收消息
- **高级 API**：通过 `operations` 成员（类型为 `GraphOperationApi`）提供业务操作

**3. 通信层：GraphSocket（双模式）**

`GraphSocket`（`C:\Users\admin\Downloads\old\Logic_source_new\original_source\app\services\GraphClient\GraphSocket.ts`）支持两种通信模式：

| 模式 | 场景 | 实现 |
|------|------|------|
| **WebSocket** | 生产环境（进程间通信） | 通过 `ws` 库连接 `ws://127.0.0.1:{port}/saleae` |
| **Direct（进程内）** | 开发/调试模式 | 通过 `@saleae/graph-interface` 原生模块直接调用 C++ 库 |

配置逻辑在 `C:\Users\admin\Downloads\old\Logic_source_new\original_source\app\services\GraphClient\utils.ts` 中的 `BackendConfig` 类型：
```typescript
export type BackendConfig =
  | { mode: 'websocket'; url: string }
  | { mode: 'direct'; graphServerSharedPath: string; pythonHomePath: string; logFileName: string }
  | { mode: 'mock' };
```

**4. 依赖注入：BackendProvider**

`BackendProvider`（`C:\Users\admin\Downloads\old\Logic_source_new\original_source\app\services\GraphClient\provider.tsx`）是 React Context 的 Provider，它：
- 创建 `GraphClient` 实例
- 管理连接状态（connecting / connected / socket-error / webgl-error）
- 通过 `BackendContext` 将 `GraphApi` 接口注入到整个 React 组件树
- 子组件只有在后端连接成功后才渲染

---

### 二、CLI 接口、API 层与客户端-服务端架构

该项目拥有 **三个独立的对外 API 接口**，形成了完整的客户端-服务端架构：

**1. gRPC Automation API（自动化接口）**

文件：`C:\Users\admin\Downloads\old\Logic_source_new\original_source\app\services\automation\server.ts`

这是一个完整的 **gRPC 服务端**，运行在 `127.0.0.1:10430`，提供以下 RPC 方法：

| 方法 | 功能 |
|------|------|
| `getDevices` | 获取设备列表 |
| `startCapture` / `stopCapture` / `waitCapture` | 采集控制 |
| `loadCapture` / `saveCapture` / `closeCapture` | 文件操作 |
| `addAnalyzer` / `removeAnalyzer` | 低级协议分析器管理 |
| `addHighLevelAnalyzer` / `removeHighLevelAnalyzer` | 高级协议分析器管理 |
| `exportRawDataCsv` / `exportRawDataBinary` | 原始数据导出 |
| `exportDataTableCsv` | 分析结果数据表导出 |
| `legacyExportAnalyzer` | 旧版分析器导出 |

启动参数 `--automation` 和 `--automationPort` 控制此服务的启用和端口。这是 Saleae 官方 Python 自动化库的底层通信协议。

**2. MCP (Model Context Protocol) Server（AI 工具接口）**

文件：`C:\Users\admin\Downloads\old\Logic_source_new\original_source\app\services\mcp\McpServer.ts`

这是一个 **JSON-RPC 2.0 over HTTP** 的 MCP 服务器，运行在 `127.0.0.1:10530`，遵循 MCP 协议规范 `2025-03-26`。它提供与 gRPC Automation API 功能等价的工具集：

- `start_capture` / `stop_capture` / `wait_capture`
- `load_capture` / `save_capture` / `close_capture`
- `add_analyzer` / `remove_analyzer`
- `add_high_level_analyzer` / `remove_high_level_analyzer`
- `export_raw_data_csv` / `export_raw_data_binary` / `export_data_table_csv`
- `get_devices`

协议实现在 `C:\Users\admin\Downloads\old\Logic_source_new\original_source\app\services\mcp\protocol.ts`，HTTP 传输层在 `C:\Users\admin\Downloads\old\Logic_source_new\original_source\app\services\mcp\transport\http.ts`。

启动参数 `--mcp` 和 `--mcpPort` 控制此服务。

**3. GraphServer WebSocket API（内部通信接口）**

这是 GUI 前端与 C++ 后端之间的内部 WebSocket 通信协议，使用自定义的 JSON 消息格式（`Saleae::*` 类型命名空间），包括：
- `RequestMeta` 路由元数据（sessionId, portName, portIndex, destination）
- `GraphActionData` 图操作指令
- `GraphStateDataV2` 状态订阅推送

---

### 三、目录结构与模块组织

项目采用 **Electron + React + MobX** 技术栈，目录组织遵循功能模块化原则：

```
original_source/
├── app/                          # 前端应用主目录
│   ├── apis/                     # API 接口定义层（抽象接口）
│   │   ├── DialogApi.ts          # 对话框 API 接口
│   │   ├── MsoDeviceApi.ts       # MSO 设备 API 接口
│   │   └── TrimApi.ts            # 裁剪 API 接口
│   ├── application/              # 应用入口与顶层组件
│   │   ├── App.tsx               # React 根组件
│   │   └── compositions/         # 顶层组合组件
│   ├── features/                 # 功能特性模块
│   ├── hooks/                    # 自定义 React Hooks（50+ 个）
│   ├── models/                   # 数据模型与状态定义
│   ├── services/                 # 核心服务层（最重要的目录）
│   │   ├── GraphClient/          # 后端通信客户端
│   │   ├── rapidDataStore/       # 中央状态管理（MobX）
│   │   ├── automation/           # gRPC 自动化服务
│   │   ├── mcp/                  # MCP AI 工具服务
│   │   ├── analyzers/            # 协议分析器管理
│   │   ├── channels/             # 通道管理
│   │   ├── device/               # 设备管理
│   │   ├── electron/             # Electron 集成
│   │   ├── extensions/           # 扩展系统
│   │   ├── graphModel/           # 图模型（节点/管道抽象）
│   │   ├── framedb/              # 帧数据库
│   │   ├── mso/                  # MSO 混合信号示波器
│   │   ├── sidebar/              # 侧边栏
│   │   ├── themes/               # 主题系统
│   │   └── ...                   # 其他服务
│   ├── text/                     # UI 文本/国际化
│   ├── thunks/                   # 异步操作
│   ├── utils/                    # 工具函数
│   └── notifications/            # 通知系统
├── shared/                       # 前端共享代码
│   ├── types/                    # 共享类型定义
│   ├── hooks/                    # 共享 Hooks
│   ├── utils/                    # 共享工具
│   └── components/               # 共享组件
└── dist/                         # Electron 主进程与构建产物
    ├── main.js                   # Electron 主进程入口
    ├── logic/                    # 渲染进程前端资源
    └── constants.json            # 编译时常量
```

**模块组织特点：**
- `apis/` 目录只定义接口（TypeScript type/interface），不包含实现
- `services/` 是核心业务逻辑层，每个子目录是一个独立的服务模块
- `hooks/` 是 React 组件与服务层之间的桥梁，通过自定义 Hook 将服务 API 暴露给组件
- `shared/` 是跨模块共享的类型和工具，不依赖具体业务

---

### 四、抽象层、接口与协议定义

**1. GraphApi 接口（核心抽象）**

`GraphApi` 是整个应用最关键的抽象接口，被 15 个文件引用。它定义了前端与后端通信的完整契约。`GraphClient` 是其唯一实现。通过这个接口：
- `RapidDataStore`（中央状态管理器）不直接依赖 `GraphClient`，而是依赖 `GraphApi` 接口
- 所有服务模块（AnalyzerManager, GraphManager, QueryNode 等）通过 `GraphApi` 与后端交互
- 测试时可以轻松替换为 mock 实现

**2. DialogApi 接口**

文件：`C:\Users\admin\Downloads\old\Logic_source_new\original_source\app\apis\DialogApi.ts`

对话框系统的抽象接口，被 `AutomationServer` 和 `McpServer` 作为依赖注入，使得自动化服务可以在无 GUI 的场景下操作对话框。

**3. GraphData 协议定义**

文件：`C:\Users\admin\Downloads\old\Logic_source_new\original_source\app\services\GraphClient\GraphData.ts`

定义了前后端通信的所有消息类型：
- `RequestMeta`：请求路由元数据（id, type, meta.sessionId, meta.portName, meta.destination）
- `GraphActionData`：图操作指令（addNode, removeNode, addPipe, connectApi 等）
- 各种具体请求/响应类型（触发设置、分析器配置、帧搜索等）

**4. GraphState 状态定义**

文件：`C:\Users\admin\Downloads\old\Logic_source_new\original_source\app\services\GraphClient\GraphState.ts`

定义了后端所有节点状态的 TypeScript 类型，与 C++ 后端的状态类型一一对应：
- `DisaggregatorState`、`LegacyAnalyzerState`、`HlaNodeState` 等
- `AllKernelTypes`：所有可创建的内核节点类型（30+ 种）

**5. DeviceState 设备类型系统**

文件：`C:\Users\admin\Downloads\old\Logic_source_new\original_source\app\services\GraphClient\DeviceState.ts`

定义了设备相关的类型抽象：
- `KnownConnectedDevice`：已知设备联合类型
- `ConnectedLegacyDeviceV2`：传统逻辑分析仪设备
- `ConnectedDeviceV2WithMSO`：MSO 混合信号设备
- `LogicDeviceCommandLookup`：设备命令类型查找表（请求-响应类型映射）

**6. MCP 协议定义**

文件：`C:\Users\admin\Downloads\old\Logic_source_new\original_source\app\services\mcp\protocol.ts`

实现了完整的 MCP JSON-RPC 2.0 协议处理，包括：
- `initialize` / `notifications/initialized` 握手
- `ping` 心跳
- `tools/list` 工具列表
- `tools/call` 工具调用（含 AJV schema 校验）

---

### 五、数据采集与显示的分离

该项目在数据采集与显示之间实现了 **四层分离**：

**第一层：C++ 后端（graph-io）负责数据采集**

C++ 后端通过 `LogicDeviceNode` 直接与 USB 硬件通信，采集原始数字/模拟信号数据，存入 `DigitalStore` / `AnalogStore` 节点。前端完全不参与数据采集过程。

**第二层：GraphClient 负责通信桥接**

`GraphClient` 通过 WebSocket/直接调用将采集控制指令（如 `StartCapture`, `SetSampleRate`, `EnableChannels`）发送到后端，并订阅后端推送的状态更新（`GraphStateDataV2`）。

**第三层：RapidDataStore 负责状态管理**

文件：`C:\Users\admin\Downloads\old\Logic_source_new\original_source\app\services\rapidDataStore\RapidDataStore.ts`

`RapidDataStore` 是基于 MobX 的中央状态管理器，它：
- 持有 `GraphApi`（后端接口）的引用
- 管理 `SessionStore`（采集会话）的创建/切换/销毁
- 管理 `DeviceListManager`（设备列表）
- 管理 `AutomationServer` 和 `McpServer`
- 通过 `SessionDeviceManager` 管理设备配置
- 不包含任何渲染逻辑

**第四层：React 组件负责显示**

React 组件通过以下路径获取数据：
1. `BackendProvider` 提供 `GraphApi` 上下文
2. `RapidDataStoreProvider` 消费 `GraphApi`，创建 `RapidDataStore` 并提供上下文
3. 组件通过 `useRapidDataStore`、`useActiveSession` 等 Hook 获取状态
4. MobX 的 `observer` HOC 自动追踪依赖并响应状态变化

渲染相关的代码完全在 `features/`、`services/overlay/`、`services/statelessRender/` 等目录中，与数据采集逻辑零耦合。

---

### 总结：解耦架构全景图

```
┌──────────────────────────────────────────────────────────────┐
│                     外部客户端                                │
│  ┌─────────────┐  ┌──────────────┐                          │
│  │ Python 脚本  │  │  AI Agent    │                          │
│  │ (gRPC)      │  │  (MCP/HTTP)  │                          │
│  └──────┬──────┘  └──────┬───────┘                          │
└─────────┼────────────────┼──────────────────────────────────┘
          │                │
          ▼                ▼
┌──────────────────────────────────────────────────────────────┐
│                   Electron 主进程                             │
│  ┌──────────────────┐  ┌──────────────────┐                 │
│  │ AutomationServer │  │   McpServer      │                 │
│  │ (gRPC :10430)    │  │ (HTTP :10530)    │                 │
│  └────────┬─────────┘  └────────┬─────────┘                 │
│           │     RapidDataStore   │                           │
│           └──────────┬──────────┘                            │
│                      │                                       │
│           ┌──────────▼──────────┐                            │
│           │     GraphApi        │  ← 抽象接口                │
│           └──────────┬──────────┘                            │
│                      │                                       │
│           ┌──────────▼──────────┐                            │
│           │    GraphClient      │                            │
│           │  ┌───────────────┐  │                            │
│           │  │  GraphSocket  │  │                            │
│           │  │ (WS / Direct) │  │                            │
│           │  └───────┬───────┘  │                            │
│           └──────────┼──────────┘                            │
└──────────────────────┼───────────────────────────────────────┘
                       │ WebSocket / FFI
┌──────────────────────▼───────────────────────────────────────┐
│                C++ 后端 (graph-io)                            │
│  ┌────────────┐ ┌─────────────┐ ┌──────────────┐           │
│  │ DeviceNode │ │ AnalyzerNode│ │  Store Nodes  │           │
│  │ (USB采集)  │ │ (协议解码)   │ │ (数据存储)    │           │
│  └────────────┘ └─────────────┘ └──────────────┘           │
└──────────────────────────────────────────────────────────────┘
```

**核心解耦手段总结：**

| 解耦维度 | 实现方式 | 关键文件 |
|----------|---------|---------|
| GUI 与后端 | GraphClient/GraphSocket 双模式通信 + GraphApi 接口抽象 | `GraphClient.ts`, `GraphSocket.ts`, `provider.tsx` |
| 数据采集与显示 | C++ 后端采集 → GraphClient 桥接 → MobX 状态管理 → React 渲染 | `RapidDataStore.ts`, `Provider.tsx` |
| 自动化与 GUI | gRPC Server + MCP Server 独立运行，共享 RapidDataStore | `automation/server.ts`, `mcp/McpServer.ts` |
| 接口与实现 | `GraphApi`/`DialogApi`/`SidebarApi` 等纯接口定义 | `apis/` 目录 |
| 通信协议 | 自定义 JSON 消息协议（Saleae::* 命名空间）+ RequestMeta 路由 | `GraphData.ts`, `GraphActions.ts` |
| 进程隔离 | Electron 主进程管理 C++ 后端生命周期，渲染进程通过 WebSocket 通信 | `dist/main.js` |

该项目的架构设计代表了桌面信号分析软件的一种先进模式：**将 C++ 高性能数据采集引擎与 Web 技术栈的灵活 UI 完全解耦**，同时通过 gRPC 和 MCP 双协议暴露自动化接口，使得同一套核心逻辑可以同时服务于人工操作、脚本自动化和 AI Agent 三种使用场景。

## 架构对比分析

### 当前 PXView 的架构问题

当前 PXView 的核心问题是 **业务逻辑与 UI 几乎没有分离**：

| 耦合点 | 严重程度 | 说明 |
|--------|----------|------|
| SigSession 持有 View 层对象 | **极高** | `_signals`、`_decode_traces`、`_math_trace` 等都是 `pv::view::*` 类型，Session 无法脱离 View 独立存在 |
| MainWindow 多重角色 | **极高** | 同时是 UI 窗口 + Session 回调 + 消息监听者，直接操作 Session 和 DeviceAgent |
| 全局静态 `_session` 指针 | **高** | 用于 C 回调桥接，不支持多实例 |
| DeviceAgent 裸指针暴露 | **高** | MainWindow 跨过 Session 直接操作底层设备 |
| ISessionCallback 胖接口 | **中** | 17 个方法，几乎只有 MainWindow 一个实现者 |
| OnMessage 200+ 行 switch | **中** | 直接操作十几个 UI 组件 |

当前数据流：
```
libsigrok(C) → SigSession(持有View对象!) → MainWindow(UI+业务混合) → View层
```

### 参考 Saleae Logic 2 的架构

Saleae Logic 2 实现了 **四层解耦**：

```
外部客户端(Python/AI) → gRPC/MCP API → GraphClient(GraphApi接口) → C++后端(graph-io)
                                              ↑
                                         React UI 通过同一接口
```

关键设计：
1. **GraphApi 抽象接口** — UI 和自动化服务都依赖接口而非实现
2. **双模式通信** — WebSocket（生产）和 Direct FFI（开发），后端可独立运行
3. **gRPC Automation API** (:10430) — Python 脚本自动化
4. **MCP Server** (:10530) — AI Agent 工具调用
5. **RapidDataStore** — 纯状态管理层，不含渲染逻辑

---

### PXView 解耦改造方案

PXView 已经有了 `ISessionService`/`IAppService` API 层和 WsTransport/McpTransport/DirectTransport 传输层，这是很好的基础。但底层 SigSession 与 View 层的耦合是根本障碍。

#### 第一阶段：剥离 SigSession 中的 View 依赖

**目标**：让 SigSession 可以在没有 Qt View 的情况下独立运行

1. **将 `view::Signal*` 等从 SigSession 移出**：SigSession 只管理数据（Snapshot、SessionData），不再持有 Signal/DecodeTrace/MathTrace 等 View 对象。由 View 层根据 Session 的数据自行创建和管理渲染对象。

2. **拆分 ISessionCallback**：17 个方法的胖接口拆分为：
   - `ICaptureCallback` — 采集开始/结束/状态变更
   - `IDataCallback` — 数据更新/帧结束
   - `ITriggerCallback` — 触发事件
   - `IDeviceCallback` — 设备变更

3. **移除 SigSession 静态实例**：C 回调桥接改用上下文指针传递（libsigrok 的 `ds_set_datafeed_callback` 已支持 `void*` 用户数据），消除全局静态变量。

#### 第二阶段：建立独立的核心服务层

**目标**：核心业务逻辑可独立于 GUI 运行

```
┌─────────────────────────────────────────┐
│          Core Service Layer (新增)        │
│  ┌──────────────┐  ┌─────────────────┐  │
│  │ CaptureService│  │ DeviceService   │  │
│  │ (采集控制)    │  │ (设备管理)      │  │
│  └──────────────┘  └─────────────────┘  │
│  ┌──────────────┐  ┌─────────────────┐  │
│  │ DecodeService │  │ ExportService   │  │
│  │ (协议解码)    │  │ (数据导出)      │  │
│  └──────────────┘  └─────────────────┘  │
│          ↓ 实现 ISessionService 接口      │
└─────────────────────────────────────────┘
         ↑                    ↑
    GUI Client           CLI / 外部客户端
  (MainWindow)         (WsTransport/gRPC)
```

4. **Core Service 独立项目**：将 SigSession + DeviceAgent + 数据层（Snapshot/SessionDocument/DiskCache）抽取为独立的 `pxview-core` 库，不依赖 Qt Widgets，仅依赖 Qt Core（信号槽）和 libsigrok/libsigrokdecode。

5. **ISessionService 成为 Core 的正式接口**：当前 `ISessionService` 已有 80+ 方法覆盖全部功能，让它成为 Core Service Layer 的正式对外接口，GUI 和 CLI 都是它的消费者。

#### 第三阶段：CLI 客户端

**目标**：无需 GUI 即可完成采集、解码、导出

6. **pxview-cli 可执行文件**：新建 `cli/` 目录，链接 `pxview-core` 库，通过 `ISessionService` 接口操作：
   ```
   pxview-cli capture --device DSLogic --channels 0,1,2 --samplerate 10M --duration 1s -o capture.sr
   pxview-cli decode --input capture.sr --decoder spi_c --channel MOSI=0,CLK=1,CS=2
   pxview-cli export --input capture.sr --format csv -o output.csv
   pxview-cli devices --list
   ```

7. **Headless 模式**：GUI 应用增加 `--headless` 启动参数，跳过 Qt Widgets 初始化，只启动 Core Service + API Server，供远程控制。

#### 第四阶段：多客户端支持

8. **统一 API Gateway**：当前已有 WsTransport(:10430) 和 McpTransport(:10110)，可以统一为：
   - **WebSocket API** (:10430) — 供自定义客户端/脚本使用
   - **MCP API** (:10110) — 供 AI Agent 使用
   - **gRPC API** (新增) — 供 Python/高性能自动化使用（参考 Saleae 的做法）

9. **协议标准化**：当前 API 消息格式是内部定义，建议定义清晰的 JSON-RPC 或 gRPC 协议，方便第三方客户端对接。

---

### 改造优先级建议

| 优先级 | 改造项 | 原因 |
|--------|--------|------|
| P0 | 剥离 SigSession 中的 View 对象 | 这是所有后续工作的前提，当前 SigSession 无法脱离 Qt View 独立运行 |
| P0 | Core Service 独立编译 | 只有 Core 能独立编译，CLI 才能实现 |
| P1 | CLI 客户端 | 验证解耦是否成功，同时提供实用价值 |
| P1 | Headless 模式 | 远程控制/CI 场景需要 |
| P2 | gRPC API | Python 自动化生态更完善 |
| P2 | 协议文档化 | 第三方客户端开发需要 |

### 与 Saleae 的差距

| 维度 | Saleae Logic 2 | PXView 现状 | 改造后 |
|------|---------------|-------------|--------|
| GUI 与后端分离 | 完全分离（C++ 后端 + Electron 前端） | 几乎未分离 | Core 库独立 + GUI 作为客户端 |
| CLI 支持 | 无（但有 gRPC 自动化） | 无 | 有（pxview-cli） |
| 自动化 API | gRPC + MCP 双协议 | WsTransport + McpTransport（已有基础） | 增加 gRPC |
| 状态管理 | MobX 独立状态层 | MainWindow 混合管理 | Core Service 层管理 |
| 通信协议 | 自定义 JSON + gRPC | 内部 JSON | 标准化 JSON-RPC/gRPC |

PXView 已有的 `ISessionService`/`IAppService` 接口和 WsTransport/McpTransport 传输层是很好的解耦基础，最关键的一步是 **让 SigSession 不再持有 View 层对象**，使核心逻辑可以脱离 GUI 独立运行。

---

**User**

你认为需要cli吗，逻辑分析仪的话，我们经常比较cli和mcp谁更好

**Assistant**

这是个好问题。我的看法：**对逻辑分析仪来说，CLI 的价值有限，MCP 更值得优先投入。**

### CLI 的实际场景

CLI 在逻辑分析仪上能做的事基本就是：

```bash
pxview-cli capture --device DSLogic --channels 0,1,2 --rate 10M -o data.sr
pxview-cli decode --input data.sr --decoder spi_c --channel MOSI=0,CLK=1
pxview-cli export --input data.sr --format csv -o out.csv
pxview-cli devices
```

这些场景的核心特征是：**不需要看波形，只需要批量跑任务**。比如 CI 里自动采集+解码+导出。但说实话，逻辑分析仪 90% 的使用场景都需要看波形——你抓信号就是为了看时序、看协议交互、看毛刺，这些 CLI 做不了。

### MCP 的优势

MCP 本质上是 **AI 可调用的 CLI**，但比 CLI 强在：

1. **组合能力** — AI 可以根据上一步结果决定下一步，CLI 脚本也能做但更笨拙
2. **自然语言交互** — "帮我抓一下 SPI 通信看看有没有丢包" 比 `--decoder spi_c --channel MOSI=0,CLK=1,CS=2` 直觉得多
3. **覆盖 CLI 的场景** — MCP 能做的自动化，CLI 都能做；反过来不行
4. **你已经有了** — McpTransport 已经实现，ISessionService 接口已覆盖 80+ 方法

### 我的建议

| 方案 | 投入 | 回报 | 建议 |
|------|------|------|------|
| 纯 MCP | 低（已有基础） | 高 | **优先做** |
| CLI | 中（需要新项目） | 低 | 不值得单独做 |
| MCP + 薄 CLI wrapper | 低 | 中 | 如果需要脚本化，CLI 只是 MCP/WS API 的薄包装 |

**具体来说**：与其做独立 CLI，不如把精力放在：

1. **完善 MCP/WS API** — 让它覆盖采集、解码、导出的完整流程
2. **Headless 模式** — `PXView.exe --headless` 只启动 Core + API Server，不创建窗口
3. 如果确实需要脚本化，写一个 **Python SDK**（类似 Saleae 的 `saleae/automation`），底层调 WS API，比 CLI 更灵活

CLI 可以作为最后一步，用 100 行代码包装 WS API 调用即可，不需要作为独立架构目标。

---

**User**

所以最后你要怎么去构造最终的架构

**Assistant**

基于当前代码现状和实际需求（MCP/WS API 优先，非 CLI），最终架构如下：

## 最终架构设计

### 核心思路：三层分离

```
┌─────────────────────────────────────────────────────────┐
│                    Client Layer                          │
│  ┌──────────┐  ┌──────────┐  ┌──────────────────────┐  │
│  │ Qt GUI   │  │ MCP      │  │ WebSocket Client      │  │
│  │(MainWindow│  │ Server   │  │ (Python/自定义/脚本)   │  │
│  │ + View)  │  │(:10110)  │  │ (:10430)              │  │
│  └────┬─────┘  └────┬─────┘  └──────────┬───────────┘  │
└───────┼──────────────┼──────────────────┼───────────────┘
        │              │                  │
        ▼              ▼                  ▼
┌─────────────────────────────────────────────────────────┐
│                 Service Layer (新增)                      │
│                                                         │
│  ISessionService (已有，80+ 方法，作为统一接口)            │
│  IAppService    (已有)                                   │
│                                                         │
│  ┌─────────────────────────────────────────────────┐    │
│  │              SessionService                     │    │
│  │  - 采集控制 (start/stop/wait)                    │    │
│  │  - 设备管理 (list/config/get)                    │    │
│  │  - 通道/采样率/触发配置                           │    │
│  │  - 解码器管理                                    │    │
│  │  - 数据导出                                      │    │
│  │  - 会话文件加载/保存                              │    │
│  └──────────────────────┬──────────────────────────┘    │
│                         │                               │
│  ┌──────────────────────┴──────────────────────────┐    │
│  │            RpcDispatcher (已有)                   │    │
│  │  WsTransport / McpTransport / DirectTransport    │    │
│  └──────────────────────────────────────────────────┘    │
└─────────────────────────┬───────────────────────────────┘
                          │
                          ▼
┌─────────────────────────────────────────────────────────┐
│                   Core Layer                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ SigSession   │  │ DeviceAgent  │  │ Data Layer   │  │
│  │ (纯数据+采集  │  │ (设备封装)    │  │ Snapshot     │  │
│  │  控制，无View)│  │              │  │ SessionDoc   │  │
│  └──────┬───────┘  └──────────────┘  │ DiskCache    │  │
│         │                             └──────────────┘  │
│         ▼                                               │
│  ┌──────────────────────────────────────────────────┐   │
│  │  libsigrok (C11)  │  libsigrokdecode (C11+Py)   │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### 具体改造步骤

#### Step 1：SigSession 去视图化

这是最关键的一步。当前 SigSession 持有的 View 对象需要全部移出：

```cpp
// sigsession.h 当前持有（需要移除）:
std::vector<view::Signal*> _signals;          // → 移到 View 层
std::vector<view::DecodeTrace*> _decode_traces; // → 移到 View 层
view::LissajousTrace* _lissajous_trace;       // → 移到 View 层
view::MathTrace* _math_trace;                 // → 移到 View 层
view::SpectrumTrace* _spectrum_trace;         // → 移到 View 层
```

改造后 SigSession 只负责：

```cpp
// sigsession.h 改造后:
class SigSession : public IMessageListener,
                   public IDeviceAgentCallback,
                   public DataSource
{
    // 数据采集控制
    DeviceAgent _device_agent;
    SessionData* _view_data;
    SessionData* _capture_data;
    
    // 回调通知（保持接口不变）
    std::vector<ISessionCallback*> _callbacks;
    std::vector<IMessageListener*> _msg_listeners;
    
    // 信号元数据（非View对象，纯数据描述）
    struct ChannelInfo {
        int index;
        std::string name;
        std::string type; // "logic" / "analog" / "dso"
    };
    std::vector<ChannelInfo> _channels;  // 替代 view::Signal*
    
    // 解码器元数据（非View对象）
    struct DecoderInfo {
        std::string id;
        std::string decoder_name;
        std::map<int, int> channel_map;  // decoder_channel -> signal_index
    };
    std::vector<DecoderInfo> _decoders;  // 替代 view::DecodeTrace*
};
```

View 层自行根据 `ChannelInfo`/`DecoderInfo` 创建 Signal/DecodeTrace：

```cpp
// View 层（新增 SignalFactory）
class SignalFactory {
public:
    static std::vector<view::Signal*> create_signals(SigSession* session) {
        auto channels = session->get_channels();
        std::vector<view::Signal*> signals;
        for (auto& ch : channels) {
            if (ch.type == "logic")
                signals.push_back(new view::LogicSignal(ch, session));
            else if (ch.type == "analog")
                signals.push_back(new view::AnalogSignal(ch, session));
            // ...
        }
        return signals;
    }
};
```

#### Step 2：SessionService 实现

当前 `ISessionService` 已有接口定义，但实现（AppService）直接操作 SigSession。改造为独立的 Service 层：

```cpp
// pv/service/session_service.h (新增)
class SessionService : public ISessionService {
public:
    explicit SessionService(SigSession* session);
    
    // === 采集控制 ===
    bool start_capture() override;
    bool stop_capture() override;
    bool wait_capture(int timeout_ms) override;
    CaptureState get_capture_state() override;
    
    // === 设备管理 ===
    std::vector<DeviceInfo> get_devices() override;
    bool select_device(const std::string& id) override;
    DeviceInfo get_current_device() override;
    
    // === 通道/采样率/触发 ===
    std::vector<ChannelInfo> get_channels() override;
    bool set_channel_enabled(int ch, bool enabled) override;
    std::vector<uint64_t> get_sample_rates() override;
    bool set_sample_rate(uint64_t rate) override;
    // ... 触发配置等
    
    // === 解码器 ===
    std::vector<DecoderInfo> get_decoders() override;
    std::string add_decoder(const std::string& name, 
                           const std::map<int,int>& ch_map) override;
    bool remove_decoder(const std::string& id) override;
    
    // === 数据导出 ===
    bool export_csv(const std::string& path, 
                   const ExportOptions& opts) override;
    bool export_binary(const std::string& path,
                      const ExportOptions& opts) override;
    
    // === 会话文件 ===
    bool load_session(const std::string& path) override;
    bool save_session(const std::string& path) override;
    
private:
    SigSession* _session;  // 核心依赖，但不依赖任何 View
};
```

#### Step 3：Headless 模式

修改 `main.cpp` 入口，支持无 GUI 启动：

```cpp
// main.cpp 改造
int main(int argc, char* argv[]) {
    // 解析参数
    bool headless = args.contains("--headless");
    int ws_port = args.value("--ws-port", "10430").toInt();
    int mcp_port = args.value("--mcp-port", "10110").toInt();
    
    // 初始化 Core（无论是否有 GUI 都需要）
    auto* control = AppControl::Instance();
    control->Init();  // 初始化 libsigrok + libsigrokdecode
    control->GetSession()->init();
    
    if (headless) {
        // Headless 模式：只启动 Service + API Server
        QCoreApplication app(argc, argv);  // 不需要 QApplication
        
        SessionService service(control->GetSession());
        RpcDispatcher dispatcher(&service);
        
        WsTransport ws(&dispatcher, ws_port);
        McpTransport mcp(&dispatcher, mcp_port);
        
        control->Start();  // 启动 Session（打开设备等）
        ws.start();
        mcp.start();
        
        return app.exec();
    } else {
        // GUI 模式：原有逻辑
        QApplication app(argc, argv);
        // ... 创建 MainFrame, MainWindow 等
    }
}
```

#### Step 4：MainWindow 瘦身

MainWindow 当前直接操作 SigSession 和 DeviceAgent，改为通过 SessionService 交互：

```cpp
// mainwindow.h 改造后
class MainWindow : public QMainWindow,
                   public ISessionCallback,
                   public IMessageListener
{
    // 移除：SigSession* _session;
    // 移除：DeviceAgent* _device_agent;
    
    // 新增：
    ISessionService* _service;  // 只依赖接口
    
    // View 层自行管理 Signal 对象
    std::vector<view::Signal*> _signals;  // 从 SigSession 移到这里
    std::vector<view::DecodeTrace*> _decode_traces;  // 从 SigSession 移到这里
};
```

### 最终文件结构

```
PXView/
├── main.cpp                    # 入口，支持 --headless
├── pv/
│   ├── appcontrol.cpp/h        # 单例，管理 Core 初始化
│   ├── sigsession.cpp/h        # 纯数据+采集控制，无 View 依赖
│   ├── deviceagent.cpp/h       # 设备封装（不变）
│   ├── tabcontext.cpp/h        # 标签上下文（不变）
│   │
│   ├── service/                # 【新增】Service Layer
│   │   ├── session_service.h   # ISessionService 实现
│   │   ├── session_service.cpp
│   │   ├── app_service.h       # IAppService 实现
│   │   └── app_service.cpp
│   │
│   ├── api/                    # 【已有】API Transport Layer
│   │   ├── iapp_service.h      # 接口定义
│   │   ├── isession_service.h
│   │   ├── rpc_dispatcher.h/cpp
│   │   ├── ws_transport.h/cpp
│   │   ├── mcp_transport.h/cpp
│   │   └── direct_transport.h/cpp
│   │
│   ├── mainwindow.cpp/h        # 瘦身，只依赖 ISessionService
│   ├── view/                   # View 层，自行管理 Signal/Trace
│   │   ├── signal_factory.h    # 【新增】从 ChannelInfo 创建 Signal
│   │   ├── signal.cpp/h
│   │   ├── decode_trace.cpp/h
│   │   └── ...
│   ├── data/                   # 数据层（不变）
│   ├── interface/              # 回调接口（拆分 ISessionCallback）
│   └── ...
```

### 改造优先级

| 顺序 | 改造项 | 依赖 | 效果 |
|------|--------|------|------|
| 1 | SigSession 去视图化 | 无 | Core 可脱离 GUI 编译运行 |
| 2 | SessionService 实现 | Step 1 | 统一业务逻辑入口 |
| 3 | Headless 模式 | Step 2 | `--headless` 启动，MCP/WS 可用 |
| 4 | MainWindow 瘦身 | Step 2 | GUI 通过 Service 交互 |
| 5 | 拆分 ISessionCallback | Step 4 | 接口更清晰 |

Step 1-3 完成后，就可以实现 Headless + MCP 的核心场景了。Step 4-5 是后续优化。

---

