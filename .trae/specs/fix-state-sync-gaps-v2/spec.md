# fix-state-sync-gaps-v2 Spec

## Why
fix-state-sync-gaps v1 修复了表面同步问题（R1–R8 代码层均已落地），但审计发现 v1 自身引入了一个 P0 线程安全回归，且若干 v1 列出的 P0 项并未真正修干净；此外架构层仍有真正的状态分散（高级触发、DSO/Analog 硬件参数根本不在 Core 层，MCP transport 无事件推送能力）。本 spec 一次性修掉 v1 遗留的全部 P0 与 P1 项，并把 AGENTS.md 控制在 ~80 行。

具体待修问题：
1. **R5 回归（P0）**：`SigSession::trigger_message` 末尾同步调 `broadcast_msg`，使 `MainWindow::OnMessage` 被 feed/设备事件/复制/毛刺/反相线程同步调用，函数体内直接操作 QWidget（含 `MsgBox::Confirm` 模态弹窗），无任何 marshal。同样 `ICaptureCallback` 的 `update_capture`/`show_region`/`show_wait_trigger`/`repeat_hold` 也绕过 EventObject 信号直接操作 View QWidget。
2. **trig_type 持久化半修（P0）**：`save_signal_config` 已加 `signal_models` 参数，但 7 个调用点中只有 2 个传入；`mainwindow.cpp:2413/2848/2950/3565/369` 5 处丢失 trig_type，其中 `:2950` 模式切换会用全 0 覆盖当前 tab 的有效配置。`apply_pending_config` 路径也缺 trig_type 回写。
3. **DeviceAgent 9 个类型化 setter 静默（P0）**：`set_config_string/bool/uint64/uint16/uint32/int16/int32/byte/double` 全部不调 `config_changed()`，只有通用 `set_config` 调。
4. **`_capture_owner_document` 无生命周期管理（P0）**：5 个写入点全直接赋值无广播；`remove_tab` 完全不清理该指针；后台 copy 线程 detached 持有裸 `doc` 指针，关 owner Tab 即悬垂访问。
5. **on_frame_ended 跨 tab 双重启动解码器（P0）**：`mainwindow.cpp:2400-2409` 仍带 "TEST FIX FOR HYPOTHESIS 1" 调试代码，分支1（`active_document != owner_doc`）不检查 `copy_in_progress` 直接同步 copy + `start_all_decode_tasks`，与随后 `DSV_MSG_COPY_TO_DOC_DONE` 重复。
6. **Signal::set_enabled 不写回 Core（P1）**：只写 `_local_enabled` + `_probe->enabled`，不写 `SignalModel::_enabled`，Core/View 运行时不同步。
7. **DSO/Analog 硬件参数不在 Core（P1，架构断层）**：`DsoSignal`/`AnalogSignal` 的 setter（vdiv/coupling/offset/trig_value/factor 等）只写 libsigrok device config + View 成员，不写回 `SignalModel`，不广播。headless/MCP 永远读不到运行时变化。
8. **高级触发配置完全绕过 Core（P1，架构断层）**：`TriggerDock::commit_trigger` 高级分支只调 `ds_trigger_*` libsigrok API + 写 UI + 经 `dock::get_session` 写 session 文件，Core `SignalModel` 只存简单 `trig_type`。这是真正的状态分散。
9. **MCP transport 完全无事件推送（P1）**：`McpTransport` 不继承 `IServiceEventListener`；`AppService::_event_listeners` 始终为空（`add_event_listener` 无任何调用方），`AppService::notify_event` 发出的 `DeviceConfigChanged`/`DeviceDetached` 无人接收。
10. **其余漏广播（P1）**：`export_data`/`export_binary`/`export_decoder_table` 不广播 ExportComplete；`configure_and_start` 不广播 SampleConfigChanged；`enable_spectrum`/`enable_lissajous`/`enable_math` 不广播；`TriggerDock::try_commit_trigger` 不广播；`ProtocolDock::OnProtocolVisibilityChanged` 不广播；GUI 路径 decoder add/remove 只发通用 SignalsChanged；`SessionService::OnMessage` 缺 `ACTIVE_DOCUMENT_CHANGED`/`COPY_IN_PROGRESS_CHANGED` case；`broadcast_event` 在非主线程调 `WsTransport::sendTextMessage`（QWebSocket 非线程安全）。
11. **杂项（P2）**：`set_active_document` 无去重；`LogicSignal::set_trig` 无条件广播 SIMPLE_TRIGGER_CHANGED（rebuild 期间 N 次广播）；`on_frame_began` 无条件 `set_active_document` 绕过 R6 skip。

## What Changes

### A. 线程安全（P0）
- `MainWindow::OnMessage` 顶部加线程检查：非 GUI 线程时 `QMetaObject::invokeMethod(this, [this,msg]{OnMessage(msg);}, Qt::QueuedConnection)` 转发后 return。
- `EventObject` 新增 4 个信号：`update_capture_sig()`、`show_region_sig(quint64,quint64,bool)`、`show_wait_trigger_sig()`、`repeat_hold_sig(int)`。
- `MainWindow` 的 4 个 `ICaptureCallback` 方法改为 emit 信号（marshal），新增 4 个 `on_*` 槽做实际工作，connect 用 `Qt::QueuedConnection` 显式保证。
- `WsTransport::on_service_event` 调 `sendTextMessage` 前用 `QMetaObject::invokeMethod` 或 `QMetaObject::invokeMethod(qApp, ...)` marshal 到主线程。

### B. trig_type 完整持久化（P0）
- 5 处 `save_signal_config` 调用补 `_session->get_signal_models()`：`mainwindow.cpp:369/2413/2848/2950/3565`。
- `mainwindow.cpp:2809` `apply_pending_config` 之后补 trig_type 回写（与 `tabcontext.cpp:86-95` 对称）。

### C. DeviceAgent 通知一致性（P0）
- `deviceagent.cpp:418-667` 9 个类型化 setter（`set_config_string/bool/uint64/uint16/uint32/int16/int32/byte/double`）末尾补 `config_changed()`，与通用 `set_config` 行为对齐。

### D. _capture_owner_document 生命周期（P0）
- `SigSession` 新增 `clear_capture_owner_document(data::SessionDocument*)`：传入指针等于当前 owner 时置 nullptr 并广播 `DSV_MSG_CAPTURE_OWNER_CHANGED`（新增宏）。
- `MainWindow::remove_tab` 在销毁 ctx 前调 `_session->clear_capture_owner_document(ctx->document())`；若 owner 即将销毁且 `is_copy_in_progress()`，先 `join` copy 线程（改为可 join 的 `std::thread` 成员，detach 改 joinable）。
- `on_frame_ended` 移除 `mainwindow.cpp:2400-2409` 的 "TEST FIX FOR HYPOTHESIS 1" 分支1与分支2的同步 copy+start，统一改为：仅当 `!is_copy_in_progress()` 时同步 copy+start；否则等待 `DSV_MSG_COPY_TO_DOC_DONE`。删除两条 "TEST FIX FOR HYPOTHESIS 1" 注释。

### E. Signal::set_enabled 写回 Core（P1）
- `view::Signal::set_enabled(bool)` 增加 `if (_model) _model->set_enabled(enabled);`（SignalModel 已有 `set_enabled` 接口）。仍不广播（避免循环，由调用方广播）。

### F. DSO/Analog 硬件参数纳入 Core（P1，架构补齐）
- `SignalModel` 已有 `set_vdiv`/`set_coupling`/`set_vertical_offset`/`set_zero_offset`/`set_hw_offset`，补充 `set_trig_value`/`set_factor` 接口与字段。
- `DsoSignal::set_factor`/`set_acCoupling`/`set_trig_vpos`/`set_trig_ratio`/`set_zero_vpos`/`set_zero_ratio`/`go_vDialPre`/`go_vDialNext` 在写 libsigrok 后同步调 `SignalModel` 对应 setter。
- `AnalogSignal::set_zero_vpos`/`set_zero_ratio` 同步写 `SignalModel`。
- 这些 setter 末尾广播 `DSV_MSG_DEVICE_OPTIONS_UPDATED`（用户交互入口，非 rebuild 路径，不会循环）。

### G. 高级触发配置纳入 Core（P1，架构补齐）
- 新建 `PXView/pv/data/triggerconfig.h/.cpp`（Core 层，无 QWidget 依赖）：`TriggerConfig` 类持有 `mode`（SIMPLE/ADV/SERIAL）、`trigger_pos`、`stages`（`std::vector<TriggerStage>`，每 stage 含 value/logic/inv/count）。
- `SigSession` 持有 `TriggerConfig _trigger_config` 成员，提供 getter/setter，setter 变化时广播 `DSV_MSG_TRIGGER_CONFIG_CHANGED`（新增宏）。
- `TriggerDock::commit_trigger` 高级分支改为：先写 `_session->trigger_config()`，再由 Core 写 libsigrok `ds_trigger_*`（保留驱动同步）。
- `TriggerDock::get_session`/`set_session` 改为从 Core `TriggerConfig` 序列化/反序列化（不再直接读 UI 控件）。
- `SessionDocument` 持久化 `TriggerConfig`（新增 `trigger_config` 字段）。
- MCP `get_trigger_config`/`set_dso_trigger_config`/`set_logic_trigger_config` 扩展支持高级触发字段。

### H. MCP transport 事件推送（P1，架构补齐）
- `McpTransport` 继承 `IServiceEventListener`，实现 `on_service_event`：将服务事件转 SSE 推送（复用现有 `send_sse_event` 机制）或 JSON-RPC notification。
- `appcontrol.cpp` 在创建 McpTransport 后调 `active_session->add_event_listener(_mcp_transport)`。
- `AppService::_event_listeners` 修复：在 `appcontrol.cpp` 同时调 `_app_service->add_event_listener(_ws_transport)` 和 `_app_service->add_event_listener(_mcp_transport)`，使 `DeviceConfigChanged`/`DeviceDetached` 可达。
- 或者：合并 `AppService::notify_event` 与 `SessionService::notify_event`，统一走 SessionService 路径（更彻底，但影响面大；本 spec 选前者最小改动）。

### I. 补全广播点（P1）
- `SessionService::export_data`/`export_binary`/`export_decoder_table` 成功后广播 `ExportComplete`。
- `SessionService::configure_and_start` 成功后广播 `SampleConfigChanged`。
- `SessionService::enable_spectrum`/`enable_lissajous`/`enable_math` 成功后广播 `ChannelConfigChanged`（或新增 `MathConfigChanged`，复用 ChannelConfigChanged 即可）。
- `TriggerDock::try_commit_trigger` 成功后广播 `DSV_MSG_TRIGGER_CONFIG_CHANGED`（G 项的广播）。
- `ProtocolDock::OnProtocolVisibilityChanged` 广播 `DSV_MSG_DEVICE_OPTIONS_UPDATED`（或新增 `DecoderVisibilityChanged` ServiceEvent）。
- GUI 路径 decoder add/remove：`View::add_decoder` 成功后广播 `DecoderAdded`，`View::remove_decoder` 广播 `DecoderRemoved`（通过 SessionService broadcast_event，需要 View 持有 session service 引用或经 `_session` 转发）。
- `SessionService::OnMessage` 补 `DSV_MSG_ACTIVE_DOCUMENT_CHANGED`/`DSV_MSG_COPY_IN_PROGRESS_CHANGED` case（映射为 `ActiveDocumentChanged`/`CopyInProgressChanged` ServiceEvent 推送）。

### J. 杂项（P2）
- `SigSession::set_active_document` 加去重：传入指针等于 `_active_document` 时直接 return（不广播）。
- `LogicSignal::set_trig`：仅当新 trig_type != 旧值时才广播 `DSV_MSG_SIMPLE_TRIGGER_CHANGED`。
- `MainWindow::on_frame_began`：`is_working()` 时跳过 `set_active_document`（与 R6 的 `TabContext::activate` 对称）。
- `set_collect_mode`/`set_repeat_interval`/`set_logic_trigger_config` 在 Core 实际修改后才广播（加返回值检查或前后对比）。

### K. AGENTS.md（约束：保持 ~80 行）
- 在 `## Conventions` 章节后新增简短 `## State Sync Conventions` 小节（约 6 行）：概述 (1) GUI 线程 marshal 规则、(2) 状态变化必须广播、(3) `_capture_owner_document` 生命周期、(4) 高级触发经 Core TriggerConfig。
- 若新增章节后超过 85 行，压缩 `## Key Files` 表格或合并 `## Conventions` 中冗余条目，确保总行数 ≤ 85。

## Impact
- Affected specs: fix-state-sync-gaps (v1)、refactor-document-view-mvc、refactor-trigger-state-management
- Affected code:
  - `PXView/pv/mainwindow.cpp/.h`（OnMessage marshal、ICaptureCallback 4 方法信号化、remove_tab 清理 owner、on_frame_ended 移除双启动、5 处 save_signal_config 补参数、on_frame_began skip、apply_pending_config 补 trig_type）
  - `PXView/pv/sigsession.cpp/.h`（clear_capture_owner_document、set_active_document 去重、copy 线程改 joinable、TriggerConfig 成员）
  - `PXView/pv/eventobject.h`（4 个新信号）
  - `PXView/pv/deviceagent.cpp`（9 个 setter 补 config_changed）
  - `PXView/pv/view/signal.cpp`（set_enabled 写回 model）
  - `PXView/pv/view/dsosignal.cpp`/`analogsignal.cpp`（setter 写回 model + 广播）
  - `PXView/pv/view/logicsignal.cpp`（set_trig 条件广播）
  - `PXView/pv/data/triggerconfig.h/.cpp`（新增）
  - `PXView/pv/data/signalmodel.h/.cpp`（补 trig_value/factor 字段）
  - `PXView/pv/data/sessiondocument.h/.cpp`（持久化 TriggerConfig）
  - `PXView/pv/dock/triggerdock.cpp`（commit_trigger 写 Core、get/set_session 从 Core 序列化）
  - `PXView/pv/dock/protocoldock.cpp`（OnProtocolVisibilityChanged 广播）
  - `PXView/pv/api/mcp_transport.h/.cpp`（继承 IServiceEventListener、on_service_event）
  - `PXView/pv/api/ws_transport.cpp`（sendTextMessage marshal）
  - `PXView/pv/api/app_service.h/.cpp`（无改动，仅修复 listener 注册）
  - `PXView/pv/api/appcontrol.cpp`（注册 McpTransport/WsTransport 到 AppService + SessionService）
  - `PXView/pv/api/session_service.cpp`（补广播、OnMessage 补 case）
  - `PXView/pv/interface/icallbacks.h`（新增 DSV_MSG_CAPTURE_OWNER_CHANGED / DSV_MSG_TRIGGER_CONFIG_CHANGED 宏）
  - `PXView/pv/view/view.cpp`（add_decoder/remove_decoder 广播 DecoderAdded/DecoderRemoved）
  - `AGENTS.md`（加 State Sync Conventions 小节）
  - `CMakeLists.txt`（新增 triggerconfig.cpp）

---

## ADDED Requirements

### Requirement: GUI 线程安全
所有 `IMessageListener::OnMessage` 与 `ICaptureCallback`/`ITriggerCallback` 实现方法在 GUI 模式下 SHALL 在 GUI 线程执行，即便被非 GUI 线程同步调用。

#### Scenario: feed 线程触发 trigger_message
- **WHEN** libsigrok feed 线程或设备事件线程调用 `trigger_message(msg)`，进而 `broadcast_msg` 同步调用 `MainWindow::OnMessage`
- **THEN** `MainWindow::OnMessage` 检测到当前线程非 GUI 线程
- **AND** 通过 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 将自身重新投递到 GUI 线程事件循环
- **AND** 原调用立即 return，不操作任何 QWidget

#### Scenario: ICaptureCallback 在非 GUI 线程触发
- **WHEN** `update_capture`/`show_region`/`show_wait_trigger`/`repeat_hold` 在非 GUI 线程被 dispatch_to 同步调用
- **THEN** 对应方法 emit EventObject 信号
- **AND** 信号经 Qt::QueuedConnection 投递到 GUI 线程槽
- **AND** 槽函数在 GUI 线程操作 View QWidget

#### Scenario: WsTransport 跨线程推送
- **WHEN** `broadcast_event` 在非主线程触发 `WsTransport::on_service_event`
- **THEN** `sendTextMessage` 调用前 marshal 到主线程
- **AND** 不违反 QWebSocket 线程安全约束

### Requirement: _capture_owner_document 生命周期管理
`_capture_owner_document` SHALL 在 owner Tab 关闭时被清理，SHALL 在变化时广播，SHALL 避免 copy 线程悬垂访问。

#### Scenario: 关闭 owner Tab
- **GIVEN** Tab A 是 capture owner，正在后台 copy
- **WHEN** 用户关闭 Tab A
- **THEN** `remove_tab` 先等待 copy 线程 join（若进行中）
- **AND** 调用 `clear_capture_owner_document(ctx->document())` 置空 owner
- **AND** 广播 `DSV_MSG_CAPTURE_OWNER_CHANGED`
- **AND** 销毁 ctx/document，无悬垂指针

#### Scenario: owner 变化通知
- **WHEN** `start_capture` 设置 `_capture_owner_document`
- **THEN** 广播 `DSV_MSG_CAPTURE_OWNER_CHANGED`
- **AND** 下游（TabContext）被动响应，无需轮询 getter

### Requirement: 高级触发配置纳入 Core
高级触发配置（ADV/SERIAL 模式）SHALL 存储在 Core 层 `TriggerConfig` 对象，SHALL 经 SigSession 访问，SHALL 由 SessionDocument 持久化。

#### Scenario: TriggerDock 提交高级触发
- **WHEN** 用户在 TriggerDock 配置高级触发并点击 commit
- **THEN** `TriggerDock::commit_trigger` 写入 `_session->trigger_config()`
- **AND** Core 写 libsigrok `ds_trigger_*` 同步驱动
- **AND** 广播 `DSV_MSG_TRIGGER_CONFIG_CHANGED`
- **AND** MCP `get_trigger_config` 能读到完整高级触发配置

#### Scenario: session 文件持久化高级触发
- **WHEN** 保存 session 文件
- **THEN** `SessionDocument` 序列化 `TriggerConfig`（mode/stages/trigger_pos）
- **AND** 加载 session 文件时反序列化回 Core `TriggerConfig`
- **AND** TriggerDock 从 Core 读取配置填充 UI

### Requirement: DSO/Analog 硬件参数写回 Core
`DsoSignal`/`AnalogSignal` 的硬件参数 setter SHALL 同步写回 `SignalModel`，SHALL 在用户交互入口广播。

#### Scenario: 用户调整 vdiv
- **WHEN** 用户通过 DsoSignal 的 go_vDialNext 调整 vdiv
- **THEN** 写 libsigrok `SR_CONF_PROBE_VDIV`
- **AND** 同步调用 `SignalModel::set_vdiv`
- **AND** 广播 `DSV_MSG_DEVICE_OPTIONS_UPDATED`
- **AND** MCP `get_channel_config` 能读到新 vdiv

### Requirement: MCP transport 事件推送
`McpTransport` SHALL 订阅服务事件，SHALL 将事件推送给 MCP 客户端。

#### Scenario: MCP 客户端订阅事件
- **WHEN** MCP 客户端通过 SSE 长连接或 JSON-RPC notification 订阅
- **AND** 服务端发生 `CaptureStateChanged`/`SampleConfigChanged`/`DecoderAdded` 等事件
- **THEN** McpTransport 通过 `on_service_event` 接收事件
- **AND** 推送给 MCP 客户端

#### Scenario: AppService 事件可达
- **WHEN** `AppService::notify_event(DeviceConfigChanged)` 被调用（如 connect_device）
- **THEN** WsTransport 与 McpTransport 均收到事件
- **AND** 推送给各自客户端

### Requirement: AGENTS.md State Sync Conventions 章节
AGENTS.md SHALL 包含简短 State Sync Conventions 小节，总行数 SHALL ≤ 85 行。

#### Scenario: 文档行数约束
- **WHEN** 本 spec 实施完成
- **THEN** AGENTS.md 总行数在 78–85 之间
- **AND** 包含 State Sync Conventions 小节（约 6 行）
- **AND** 涵盖 GUI 线程 marshal、状态广播、owner 生命周期、TriggerConfig 四条规则

---

## MODIFIED Requirements

### Requirement: trig_type 完整持久化
v1 已加 `signal_models` 参数但 5 处调用未传入。本 spec 要求所有 `save_signal_config` 调用 SHALL 传入 `signal_models`，`apply_pending_config` 路径 SHALL 回写 trig_type。

#### Scenario: 模式切换不丢 trig_type
- **WHEN** 用户在 LOGIC 模式设置上升沿触发，切换到其他模式再切回
- **THEN** `DSV_MSG_DEVICE_MODE_CHANGED` 处理时 `save_signal_config` 传入 `signal_models`
- **AND** trig_type 不被全 0 覆盖
- **AND** 切回 LOGIC 后触发设置保留

### Requirement: DeviceAgent 类型化 setter 通知
所有 DeviceAgent setter（通用版与类型化版）SHALL 在配置变化后调用 `config_changed()`。

#### Scenario: 类型化 setter 触发通知
- **WHEN** 调用 `set_config_bool`/`set_config_uint64` 等任一类型化 setter
- **THEN** 内部调用 `config_changed()`
- **AND** `IDeviceAgentCallback::DeviceConfigChanged` 被触发
- **AND** 上层（SamplingBar/SessionService）收到通知

### Requirement: Signal::set_enabled 写回 Core
`view::Signal::set_enabled` SHALL 同步写 `SignalModel::_enabled`，仍不广播（由调用方广播）。

#### Scenario: set_enabled 后 Core 同步
- **WHEN** View 调用 `Signal::set_enabled(false)`
- **THEN** `_local_enabled` 和 `_probe->enabled` 写入
- **AND** `_model->set_enabled(false)` 调用
- **AND** MCP `get_channel_config` 立即返回 enabled=false

### Requirement: on_frame_ended 不双重启动解码器
`on_frame_ended` SHALL 统一走 `DSV_MSG_COPY_TO_DOC_DONE` 路径，SHALL 移除 "TEST FIX FOR HYPOTHESIS 1" 调试代码。

#### Scenario: 跨 tab 采集不双重启动
- **GIVEN** capture owner 与当前 active tab 不同
- **WHEN** 采集结束触发 `on_frame_ended`
- **AND** 后台 copy 正在进行
- **THEN** `on_frame_ended` 跳过同步 copy + start
- **AND** 等待 `DSV_MSG_COPY_TO_DOC_DONE` 单次 `start_all_decode_tasks`
- **AND** 不出现解码任务重复入队

### Requirement: 补全状态广播点
以下操作 SHALL 在状态变化后广播对应 ServiceEvent 或 DSV_MSG_*：export_data/export_binary/export_decoder_table（ExportComplete）、configure_and_start（SampleConfigChanged）、enable_spectrum/enable_lissajous/enable_math（ChannelConfigChanged）、TriggerDock::try_commit_trigger（TRIGGER_CONFIG_CHANGED）、ProtocolDock::OnProtocolVisibilityChanged、GUI decoder add/remove（DecoderAdded/DecoderRemoved）、SessionService::OnMessage 补 ACTIVE_DOCUMENT_CHANGED/COPY_IN_PROGRESS_CHANGED case。

### Requirement: set_active_document 去重
`set_active_document` SHALL 在传入指针等于当前值时直接 return，不触发广播。

### Requirement: LogicSignal::set_trig 条件广播
`LogicSignal::set_trig` SHALL 仅在 trig_type 实际变化时广播 `DSV_MSG_SIMPLE_TRIGGER_CHANGED`。

### Requirement: on_frame_began 跳过 set_active_document
`MainWindow::on_frame_began` SHALL 在 `is_working()` 时跳过 `set_active_document`，与 R6 的 TabContext::activate 对称。

---

## REMOVED Requirements

### Requirement: "TEST FIX FOR HYPOTHESIS 1" 调试代码
**Reason**: `mainwindow.cpp:2400-2409` 的同步 copy+start 分支与 `DSV_MSG_COPY_TO_DOC_DONE` 路径重复启动解码器，是假设性调试代码未清理。
**Migration**: 统一走 `DSV_MSG_COPY_TO_DOC_DONE` 单次启动路径；`on_frame_ended` 仅当 `!is_copy_in_progress()` 时同步 copy+start（同 tab 场景）。

### Requirement: AppService._event_listeners 空转
**Reason**: `AppService::add_event_listener` 无任何调用方，`notify_event` 发出的事件无人接收，是 dead code path。
**Migration**: 在 `appcontrol.cpp` 注册 WsTransport/McpTransport 到 AppService，使 `DeviceConfigChanged`/`DeviceDetached` 可达。
