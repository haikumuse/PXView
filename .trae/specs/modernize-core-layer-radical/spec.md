# modernize-core-layer-radical Spec

## Why

前序 `modernize-core-layer-final` spec 在 Task 6/7 选择了"评估后保留 + 注释"的保守方案，把技术债从代码异味变成**文档化的技术债**而非根治：

- **循环依赖未实际解除**：5 个 manager 仍持 `SigSession*` 双向裸指针。审计统计：5 个 manager 的 .cpp 中存在 **263 处** `_session->_xxx` 深链式直访 SigSession 私有字段（capturemanager.cpp:104 处、datafeedparser.cpp:81 处、filterprocessor.cpp:64 处、decodetaskmanager.cpp:11 处、documentregistry.cpp:3 处）。封装边界**实质完全穿透**，"friend 已消除"只是字面意义上的胜利。
- **SigSession 仍是 facade God class**：284 行持有 6 个 `unique_ptr<manager>` + 19+ private 字段（`_view_data`/`_capture_data`/`_device_agent`/`_is_working`/`_device_status`/`_is_triged`/`_trigger_flag`/`_trigger_ch`/`_hw_replied`/`_bClose`/`_is_saving`/`_dso_status`/`_dso_status_valid`/`_error`/`_error_pattern`/`_save_start`/`_save_end`/`_session_time`/`_map_zoom` 等）。manager 仍通过 `_session->_view_data->_logic_backup` 这种**三层深链式访问**操纵状态（filterprocessor.cpp:48/76/80/82/88/93/104 等共 64 处）。
- **DocumentRegistry 持非拥有裸指针**：`_active_document`/`_capture_owner_document`/`_all_documents` 仍是非拥有裸指针，所有者在外层 TabContext（View）和 SessionService（API）。这意味着 **Core 边界还没真正独立**，文档生命周期由 View/API 层管理，Core 只是被动观察。
- **legacy IMessageListener 双通道并存**：OnMessage 仍保留 5 个 pre/post ordering case（`CURRENT_DEVICE_CHANGE_PREV`/`START_COLLECT_WORK_PREV`/`STORE_CONF_PREV`/`CAPTURE_OWNER_CHANGED` + default log），与 `broadcast<T>()` 形成"同步 pre-broadcast + 异步 typed event"双通道。spec 自己承认"legacy IMessageListener paths remain"——这意味着状态同步路径并未真正单一化。
- **DataUpdated 事件是死代码**：events.h STATUS 块自己承认"the only struct without a direct emitter...its on_event override is currently a no-op"。41 个事件中唯一一个完全没接线。
- **未编译验证**：modernize-core-layer-final 所有 `cd build && ninja -j 16 && ninja install` 验证项都是 `[ ]`，静态 grep 通过 ≠ 编译通过 ≠ 运行通过。

本 spec 采用**激进重构策略**，目标是让 Core 层真正达到"现代化"标准：单一真相源、单一状态同步通道、零循环依赖、零封装穿透、零非拥有裸指针。**全部层均可修改**（Core / View / API），需编译验证，遵循最理想的架构不考虑成本。

## What Changes

### 阶段 1：SessionStateContext 中间层（彻底解除循环依赖）

- **A1**: 新建 `PXView/pv/core/sessionstatecontext.h/.cpp`，定义 `core::SessionStateContext` 类，持有当前所有 manager 共享的可变状态：
  - `SessionData *_view_data` / `SessionData *_capture_data` / `std::vector<SessionData*> _data_list`
  - `DeviceAgent _device_agent`（从 SigSession 迁入）
  - `std::atomic<bool> _is_working` / `std::atomic<int> _device_status`
  - `bool _is_triged` / `_trigger_flag` / `_hw_replied` / `_bClose` / `_is_saving` / `_dso_status_valid`
  - `uint8_t _trigger_ch` / `SESSION_ERROR_STATUS _error`
  - `uint64_t _error_pattern` / `_save_start` / `_save_end`
  - `sr_status _dso_status` / `QDateTime _session_time` / `_trig_time`
  - `int _map_zoom`
  - `std::vector<std::shared_ptr<SignalModel>> _signal_models`
  - `std::vector<std::shared_ptr<SpectrumStack>> _spectrum_stacks`
  - `std::shared_ptr<MathStack> _math_stack` / `LissajousModel *_lissajous_model`
  - `data::TriggerConfig _trigger_config`
  - `std::mutex _data_mutex` / `_sampling_mutex`
  - 所有字段通过 `inline T& ref()` / `const T& get()` / `set_xxx(T)` 三段式 accessor 暴露，**禁止外部直访私有字段**
- **A2**: 5 个 manager（CaptureManager/DocumentRegistry/DecodeTaskManager/DataFeedParser/FilterProcessor）构造函数签名从 `(EventBus*, SigSession*)` 改为 `(EventBus*, SessionStateContext*)`，移除 `SigSession *_session` 成员，新增 `SessionStateContext *_state` 成员
- **A3**: 263 处 `_session->_xxx` 直访全部改为 `_state->xxx()` accessor 调用：
  - `_session->_view_data` → `_state->view_data()` 或 `_state->set_view_data()`
  - `_session->_device_agent` → `_state->device_agent()`（返回引用，因 DeviceAgent 内部状态需修改）
  - `_session->_is_working` → `_state->is_working()` / `_state->set_is_working()`
  - 三层深链式 `_session->_view_data->_logic_backup` → `_state->view_data()->logic_backup()` 或新增 `SessionStateContext::view_logic_backup()` 短路 accessor
- **A4**: SigSession 退化为 **thin facade**：
  - 移除 19+ private 字段，全部下沉到 `SessionStateContext`
  - 持有 `std::unique_ptr<core::SessionStateContext> _state` + 6 个 manager `unique_ptr`
  - 保留 public 方法作为 forward stub（`is_working()` → `_state->is_working()` 等），保持 API 兼容
  - 移除 `friend class core::XxxManager` 全部 5 处（已是 dead code 因 manager 不再访问 SigSession 私有）
- **A5**: SigSession::init() 调整初始化顺序：先构造 `SessionStateContext`，再构造 6 个 manager 注入 `_state.get()`，最后注入 `_event_bus`

### 阶段 2：DocumentRegistry 所有权上移（消除非拥有裸指针）

- **B1** **BREAKING**: `SessionDocument` 所有权从 TabContext/SessionService 迁移到 DocumentRegistry
  - DocumentRegistry 持有 `std::vector<std::unique_ptr<SessionDocument>> _owned_documents`
  - `register_document(SessionDocument*)` 改为 `take_document(std::unique_ptr<SessionDocument>)`，DocumentRegistry 接管所有权
  - TabContext 改持 `SessionDocument*`（弱引用，从 DocumentRegistry::get_document_by_index 获取）
  - SessionService 改用 `DocumentRegistry::create_api_document()` 工厂方法，不再 `new SessionDocument`
- **B2**: `_active_document` 改为 `size_t _active_document_index`（指向 `_owned_documents` 的索引），避免裸指针
- **B3**: `_capture_owner_document` 改为 `size_t _capture_owner_index`，CaptureOwnerGuard 内嵌类持索引而非指针
- **B4**: TabContext::close_tab 不再 `delete _document`，改为 `DocumentRegistry::release_document(index)`，由 DocumentRegistry 在文档引用计数归零时 unique_ptr 自动销毁
- **B5**: SessionService::~SessionService 不再 `delete _api_document`，改为 `DocumentRegistry::release_api_document(handle)`

### 阶段 3：legacy IMessageListener 完全移除（单一状态同步通道）

- **C1** **BREAKING**: 移除 `interface/icallbacks.h` 中 `IMessageListener` 接口和 `DSV_MSG_*_PREV`/`DSV_MSG_STORE_CONF_PREV` 等 5 个 pre/post ordering 代码
- **C2**: EventBus 新增 `broadcast_sync<T>(const T&)` 方法——同步直发 typed event，不通过 Qt::QueuedConnection 队列，专供需要 pre-broadcast 同步语义的场景
- **C3**: SigSession 内 5 处 pre/post ordering broadcast 改用 `broadcast_sync<T>()`：
  - `DSV_MSG_CURRENT_DEVICE_CHANGE_PREV` → `broadcast_sync<CurrentDeviceChanged>({})`
  - `DSV_MSG_START_COLLECT_WORK_PREV` → `broadcast_sync<StartCollectWork>({})`
  - `DSV_MSG_STORE_CONF_PREV` → `broadcast_sync<StoreConfPrev>({})`（新增事件结构体）
  - `DSV_MSG_CAPTURE_OWNER_CHANGED` 的同步路径 → `broadcast_sync<CaptureOwnerChanged>({})`
- **C4**: MainWindow::OnMessage 方法**完全删除**（5 case 全部迁移到 on_event override，pre-broadcast 改用 broadcast_sync 在 SigSession 内同步派发）
- **C5**: 移除 `interface/icallbacks.h` 中 `DSV_MSG_*` 全部 43 个消息码（已全部迁移到 typed event，无消费者）
- **C6**: `add_msg_listener`/`remove_msg_listener` API 移除（IMessageListener 已无实现类）

### 阶段 4：DataUpdated 事件接线（消除死代码）

- **D1**: 在 `DataFeedParser::feed_in_logic` / `feed_in_dso` / `feed_in_analog` 末尾添加 `_event_bus->broadcast<DataUpdated>({})`，作为底层采样数据更新的真实发射点
- **D2**: 移除 events.h 中 DataUpdated 注释的"dead-code"声明
- **D3**: MainWindow::on_event(DataUpdated) 实现具体逻辑（调用 `on_data_updated` 处理器，替代原 DSV_MSG_DATA_UPDATED 路径）

### 阶段 5：编译验证与回归测试

- **E1**: `cd build && ninja -j 16 && ninja install` 必须通过 0 error
- **E2**: GUI 启动 + Headless 启动双模式回归
- **E3**: ctest 全部测试用例通过
- **E4**: MCP API 端到端测试（get_devices → add_analyzer → start_capture → wait_capture → get_analyzer_results → export_raw_data_csv）

## Impact

- **Affected specs**:
  - `modernize-core-layer-final`（本 spec 是其激进版本，Task 6/7 的"仅注释"决策被推翻，真正实施）
  - `decouple-core-from-view-v2`（Core/View 分离 spec，B1 所有权上移与之一致）
  - `fix-remaining-architecture-issues`（阶段 3 C1 manager friend 消除的最终形态——manager 不再需要任何 friend）
- **Affected code**:
  - 新增：`PXView/pv/core/sessionstatecontext.h/.cpp`（阶段 1 A1）
  - 重写：`PXView/pv/core/capturemanager.h/.cpp`（A2/A3）
  - 重写：`PXView/pv/core/documentregistry.h/.cpp`（A2/A3 + B1-B5）
  - 重写：`PXView/pv/core/decodetaskmanager.h/.cpp`（A2/A3）
  - 重写：`PXView/pv/core/datafeedparser.h/.cpp`（A2/A3 + D1）
  - 重写：`PXView/pv/core/filterprocessor.h/.cpp`（A2/A3）
  - 重写：`PXView/pv/core/eventbus.h/.cpp`（C2 broadcast_sync）
  - 大幅瘦身：`PXView/pv/sigsession.h/.cpp`（A4 thin facade，移除 19+ 字段）
  - 修改：`PXView/pv/interface/events.h`（C3 StoreConfPrev 新结构体 + D2 移除死代码注释）
  - 大幅瘦身：`PXView/pv/interface/icallbacks.h`（C1/C5/C6 移除 IMessageListener + 43 DSV_MSG_*）
  - 修改：`PXView/pv/mainwindow.h/.cpp`（C4 移除 OnMessage，D3 实现 on_event(DataUpdated)）
  - 修改：`PXView/pv/tabcontext.h/.cpp`（B1/B4 改持弱引用）
  - 修改：`PXView/pv/api/session_service.h/.cpp`（B1/B5 改用 DocumentRegistry 工厂）
  - 修改：`PXView/pv/api/app_service.cpp`（B1 改用 DocumentRegistry 工厂）
  - 更新：`AGENTS.md`、`project_memory.md`（架构变更记录）

## ADDED Requirements

### Requirement: SessionStateContext 中间层

The system SHALL provide a `core::SessionStateContext` class that owns all shared mutable session state (SessionData pointers, DeviceAgent, atomic flags, error state, signal models, trigger config, mutexes). All managers SHALL hold `SessionStateContext*` instead of `SigSession*`. State access SHALL be via typed accessor methods (`xxx()` / `set_xxx(T)`), NOT direct private field access.

#### Scenario: manager 不再持 SigSession*
- **WHEN** 检查 5 个 manager 头文件
- **THEN** 无 `SigSession *_session` 成员，改为 `SessionStateContext *_state`
- **AND** 构造函数签名是 `(EventBus*, SessionStateContext*)`

#### Scenario: 无 _session->_ 深链式直访
- **WHEN** grep `pv/core/*.cpp` `_session->_`
- **THEN** 0 命中（全部改为 `_state->xxx()` accessor）

#### Scenario: SigSession 是 thin facade
- **WHEN** 检查 `sigsession.h` private 成员
- **THEN** 仅余 `unique_ptr<SessionStateContext> _state` + 6 个 `unique_ptr<manager>` + `unique_ptr<EventBus> _event_bus`
- **AND** 无 19+ private 状态字段（已下沉到 SessionStateContext）

### Requirement: DocumentRegistry 真正拥有 SessionDocument

The system SHALL make DocumentRegistry the sole owner of SessionDocument instances via `std::vector<std::unique_ptr<SessionDocument>>`. TabContext and SessionService SHALL hold weak references (raw `SessionDocument*` obtained via accessor), NOT own documents. Document creation/destruction SHALL be centralized in DocumentRegistry factory methods.

#### Scenario: DocumentRegistry 持 unique_ptr
- **WHEN** 检查 `documentregistry.h` _owned_documents 成员
- **THEN** 类型是 `std::vector<std::unique_ptr<SessionDocument>>`
- **AND** `_active_document`/`_capture_owner_document` 改为 `size_t` 索引

#### Scenario: TabContext 不再 delete document
- **WHEN** 检查 `tabcontext.cpp`
- **THEN** 无 `delete _document`，改为 `DocumentRegistry::release_document(index)`

#### Scenario: SessionService 不再 new/delete document
- **WHEN** 检查 `session_service.cpp`
- **THEN** 无 `new SessionDocument` / `delete _api_document`
- **AND** 改用 `DocumentRegistry::create_api_document()` 工厂方法

### Requirement: 单一状态同步通道

The system SHALL use a single event dispatch channel. Legacy `IMessageListener` / `DSV_MSG_*` int-message codes SHALL be REMOVED. The 5 pre/post ordering codes SHALL be migrated to `broadcast_sync<T>()` (synchronous direct dispatch, no Qt::QueuedConnection queue). `MainWindow::OnMessage` method SHALL be DELETED.

#### Scenario: IMessageListener 接口移除
- **WHEN** 检查 `interface/icallbacks.h`
- **THEN** 无 `IMessageListener` 类定义
- **AND** 无 `DSV_MSG_*` 宏定义（43 个全部移除）

#### Scenario: MainWindow 无 OnMessage 方法
- **WHEN** 检查 `mainwindow.h` / `mainwindow.cpp`
- **THEN** 无 `OnMessage` 方法声明和定义

#### Scenario: pre-broadcast 用 broadcast_sync
- **WHEN** SigSession 需要同步 pre-broadcast 通知
- **THEN** 调用 `_event_bus->broadcast_sync<T>(event)`，不走 Qt::QueuedConnection 队列

### Requirement: DataUpdated 事件实际发射

The system SHALL emit `DataUpdated` typed event from `DataFeedParser::feed_in_logic`/`feed_in_dso`/`feed_in_analog` after each successful feed-in. The event SHALL NOT be dead code.

#### Scenario: feed_in_* 发射 DataUpdated
- **WHEN** DataFeedParser 接收到 logic/dso/analog 数据包并完成写入
- **THEN** 调用 `_event_bus->broadcast<DataUpdated>({})`

#### Scenario: events.h 无 dead-code 标注
- **WHEN** 检查 `events.h` DataUpdated 注释
- **THEN** 无 "dead-code" / "no emitter" 字样

## MODIFIED Requirements

### Requirement: 类型化事件总线（modernize-core-layer-final 阶段 3）
[原：41 typed event structs，MainWindow 41/41 override，OnMessage 收缩到 5 case]
修改为：41+1 typed event structs（新增 StoreConfPrev），MainWindow 41/41 override，OnMessage **完全删除**，pre/post ordering 改用 `broadcast_sync<T>()`，`IMessageListener` 接口与 `DSV_MSG_*` 宏全部移除。事件总线真正成为**单一通道**。

### Requirement: manager friend 消除（modernize-core-layer-final 阶段 1）
[原：5 个 manager 头文件无 `friend class SigSession`]
修改为：5 个 manager 头文件无 `friend class SigSession` 且**无 `SigSession*` 成员**，manager 与 SigSession 完全解耦——manager 通过 `SessionStateContext*` 访问共享状态，无任何反向引用。

### Requirement: DocumentRegistry 文档裸指针（modernize-core-layer-final Task 6）
[原：评估结论为"风险>收益，仅加注释"]
修改为：**实施改造**。DocumentRegistry 持 `unique_ptr<SessionDocument>`，所有者从 TabContext/SessionService 上移到 DocumentRegistry。TabContext/SessionService 改持弱引用。

### Requirement: 循环依赖解除（modernize-core-layer-final Task 7）
[原：评估结论为"保留 SigSession* + 注释"]
修改为：**实施改造**。5 个 manager 改持 `SessionStateContext*`，SigSession 与 manager 之间无双向裸指针引用。`SessionStateContext` 是单向依赖（manager 持有它，它不持有 manager）。

## REMOVED Requirements

### Requirement: events.h "0 consumers / 0 emission points" 注释
**Reason**: 已在 modernize-core-layer-final Task 1 修正，本 spec 不再涉及。

### Requirement: OnMessage 5-case fallback
**Reason**: pre/post ordering 代码已迁移到 `broadcast_sync<T>()`，OnMessage 无任何残留 case，整个方法删除。
**Migration**: C3/C4 改用 broadcast_sync<T>() + on_event override。

### Requirement: IMessageListener 接口
**Reason**: 41+1 typed event 已覆盖所有通知场景，IMessageListener 是历史包袱，双通道并存是状态不一致的根源。
**Migration**: C1/C5/C6 完全移除，所有消费者改用 IEventListener。

### Requirement: DSV_MSG_* 43 个消息码宏
**Reason**: 已全部由 typed event struct 替代，宏编码是 type-unsafe 的历史包袱。
**Migration**: C5 移除全部宏定义，调用点改用 `broadcast<T>()` / `broadcast_sync<T>()`。
