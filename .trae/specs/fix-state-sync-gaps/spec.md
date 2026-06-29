# 状态同步漏洞修复 Spec

## Why

PXView 三层架构（Core/View/API）在状态通知完整性上存在多处缺口，导致 MCP（端口 10110）/ WS（端口 10430）/ Qt GUI 三端可能不同步。已确认的故障模式包括：用户在 View header 点击通道开关后操作丢失、MCP 在捕获前查询触发配置拿到陈旧值、Tab 间切换触发设置丢失、headless 模式下 Core 自己发的消息绕一圈经 GUI 才能回到 Core（GUI 不存在则桥接断裂）、`DSV_MSG_DEVICE_CONFIG_UPDATED` 是已定义但全工程无人发布的死消息。本 spec 系统性补全状态通知链路。

**架构判断**：不需要大改。三层 + 单 SigSession + 4 回调子接口 + DSV_MSG 双通道骨架保留。仅 1 处最小架构微调（R5），其余全部为补全式修复。

## What Changes

### Core 层通知补全（R1）
- `SigSession::set_active_document()` 发 `DSV_MSG_ACTIVE_DOCUMENT_CHANGED` 新消息
- `SigSession::set_cur_samplelimits()` 发 `cur_samplelimits_changed()` 新回调（与 `set_cur_snap_samplerate` 对称）
- `SigSession::attach_data_to_signal()` 内部不再要求调用者手动补 `data_updated()`/`signals_changed()`，由函数自己发
- `SigSession::_copy_in_progress` 变化时发 `DSV_MSG_COPY_IN_PROGRESS_CHANGED` 新消息

### View 层状态写回 Core（R2）
- `view::Signal::set_enabled()` 写回 `sr_channel::enabled`（消除 `_local_enabled` 与 `probe->enabled` 双向不同步）
- `view::LogicSignal::set_trig()` 实时写回 `SignalModel::set_trig_type()` 并通过 Core 通知，不再只在捕获开始时由 `TriggerDock::try_commit_trigger()` 一次性提交

### GUI 修改 Core 后广播（R3）
- `SamplingBar::commit_settings()` 改采样率/采样点后发 `DSV_MSG_DEVICE_OPTIONS_UPDATED`
- `View::rebuild_signals_from_config()` 改 `probe->enabled` 后发 `DSV_MSG_DEVICE_OPTIONS_UPDATED`
- `TabContext::activate()` 调 `apply_signal_config` 后发 `DSV_MSG_DEVICE_OPTIONS_UPDATED`

### API 层修改后广播事件（R4）
- `SessionService::clear_all_decoders()` 不再传 `bUpdateView=false`，发 `DecoderRemoved` 事件
- `set_sample_rate` / `set_sample_limit` / `set_time_base` / `set_collect_mode` / `set_repeat_interval` 发 `SampleConfigChanged` 事件
- `set_channel_enabled` / `set_channel_name` / `set_probe_config` 发 `ChannelConfigChanged` 事件
- `set_logic_trigger_config` / `set_dso_trigger_config` 发 `TriggerConfigChanged` 事件
- `load_file` / `save_file` / `export_*` 发 `SaveComplete` / `LoadComplete` / `ExportComplete` 事件
- 上述事件类型在 `ServiceEvent` 枚举中新增

### Headless 消息桥接（R5，架构微调）
- `SigSession::trigger_message()` 内部分发完 `ITriggerCallback` 后，**直接调用** `broadcast_msg(msg)`，不再依赖 MainWindow 回灌
- 从 `MainWindow::on_trigger_message()` 移除 `_session->broadcast_msg(msg)` 调用（保留 UI 更新逻辑）
- 影响：headless 模式下 Core 的 `OnMessage` 处理路径（REV_END_PACKET / COPY_TO_DOC_DONE / TRIG_NEXT_COLLECT）自动工作，无需 headless 专用 workaround

### Tab 归属与隔离修复（R6）
- `MainWindow::on_frame_ended()` 用 `_session->get_capture_owner_document()` 取目标 document，不再用 `current_context()->document()`
- `TabContext::activate()` 在 session working 时跳过 `set_active_document()`（避免 frame_ended 落到错误 tab），改为在 END_COLLECT_WORK 时再设
- `LogicSignal::_trig` 持久化到 `SessionDocument`（通过 SignalModel 的 trig_type），切 tab 后恢复

### 死消息接到发布点（R7）
- `DSV_MSG_DEVICE_CONFIG_UPDATED` 在 `DeviceAgent` 配置变化时发布（SamplingBar / DeviceOptionsDock / API set_*_config 后）
- 移除 `DSV_MSG_BEGIN_DEVICE_OPTIONS` 死代码（仅 MainWindow::OnMessage 用作合并 case，重构合并逻辑）

### GUI 订阅缺失消息（R8）
- `MainWindow::OnMessage` 新增 case：`DSV_MSG_COLLECT_START`、`DSV_MSG_TRIG_NEXT_COLLECT`、`DSV_MSG_GLITCH_FILTER_STARTED`、`DSV_MSG_GLITCH_FILTER_PROGRESS`、`DSV_MSG_SIGNAL_INVERT_STARTED`、`DSV_MSG_COPY_IN_PROGRESS_CHANGED`、`DSV_MSG_ACTIVE_DOCUMENT_CHANGED`
- 这些 case 主要更新状态栏指示器/进度条，不影响核心数据流

### **BREAKING** 改动
- `MainWindow::on_trigger_message()` 不再调用 `_session->broadcast_msg()`（行为已迁移到 Core）
- `SessionService::clear_all_decoders()` 不再接受 `bUpdateView=false` 静默模式（始终广播）
- `view::Signal::set_enabled()` 行为变化：现在会修改 `sr_channel::enabled`，调用方需注意副作用

## Impact

- Affected specs:
  - `refactor-document-view-mvc`（R6 修复其未完成的 Tab 归属问题）
  - `decouple-core-from-view-v2`（R2 补全 View→Core 写回）
  - `add-multi-tab-sessions`（R6 改善多 tab 隔离）
  - `fix-multi-tab-bugs`（R6 修复 capture_snapshot 之外的归属错位）

- Affected code:
  - `PXView/pv/sigsession.h` / `.cpp`（R1, R5, R6 — 新增消息、新回调、trigger_message 内部 broadcast_msg、capture_owner_document 暴露）
  - `PXView/pv/interface/icallbacks.h`（R1, R7 — 新增 DSV_MSG_* 宏、新增 cur_samplelimits_changed 回调）
  - `PXView/pv/view/signal.h` / `.cpp`（R2 — set_enabled 写回 probe）
  - `PXView/pv/view/logicsignal.h` / `.cpp`（R2 — set_trig 实时写回 Core）
  - `PXView/pv/view/view.cpp`（R3 — rebuild_signals_from_config 广播）
  - `PXView/pv/toolbars/samplingbar.cpp`（R3 — commit_settings 广播）
  - `PXView/pv/tabcontext.cpp`（R3, R6 — activate 广播 + working 时延迟 set_active_document）
  - `PXView/pv/mainwindow.cpp`（R5, R6, R8 — on_trigger_message 移除回灌、on_frame_ended 用 capture_owner、OnMessage 新增 case）
  - `PXView/pv/api/session_service.h` / `.cpp`（R4 — 各 set_* 方法广播事件）
  - `PXView/pv/api/types.h`（R4 — ServiceEvent 新增类型）
  - `PXView/pv/api/ws_transport.cpp`（R4 — 新事件类型映射到推送）
  - `PXView/pv/data/signalmodel.h`（R2 — trig_type 持久化接口完善）
  - `PXView/pv/data/sessiondocument.h` / `.cpp`（R6 — LogicSignal trig 持久化）

---

## ADDED Requirements

### Requirement: Core 内部状态变化通知完整性

Core 层 SHALL 在所有改变外部可见状态的字段修改后，通过 DSV_MSG_* 消息或回调子接口通知外部。不允许存在"修改了字段但无任何通知"的盲区。

#### Scenario: 切换活动文档通知
- **GIVEN** 当前活动文档为 A
- **WHEN** 调用 `SigSession::set_active_document(B)`
- **THEN** 发出 `DSV_MSG_ACTIVE_DOCUMENT_CHANGED`
- **AND** MCP/WS 客户端通过订阅收到通知
- **AND** GUI 的 TabContext 能感知到 active document 变化（用于跨 tab 操作的同步）

#### Scenario: 采样点数变化通知
- **GIVEN** 当前采样点数为 N1
- **WHEN** 调用 `SigSession::set_cur_samplelimits(N2)`
- **THEN** 调用 `ICaptureCallback::cur_samplelimits_changed()` 回调
- **AND** MainWindow 收到回调后更新采样周期标签

#### Scenario: attach_data_to_signal 自动通知
- **WHEN** 调用 `SigSession::attach_data_to_signal(view_data)`
- **THEN** 函数内部发出 `data_updated()` 和 `signals_changed()` 回调
- **AND** 调用者无需手动补发通知

#### Scenario: 后台 copy 进度通知
- **GIVEN** `_copy_in_progress == false`
- **WHEN** `OnMessage(DSV_MSG_REV_END_PACKET)` 启动后台 copy，置 `_copy_in_progress = true`
- **THEN** 发出 `DSV_MSG_COPY_IN_PROGRESS_CHANGED`（detail=started）
- **AND** copy 完成后发出 `DSV_MSG_COPY_IN_PROGRESS_CHANGED`（detail=completed）

### Requirement: View 层状态实时写回 Core

View 层修改任何 Core 持有的语义状态时 SHALL 实时写回 Core，不允许仅在 View 本地缓存。Core 是唯一可信源。

#### Scenario: View header 点击通道开关
- **GIVEN** 通道 0 当前 `probe->enabled == false`，View 显示为关闭
- **WHEN** 用户在 View header 点击通道 0 的开关图标
- **THEN** `view::Signal::set_enabled(true)` 内部写 `probe->enabled = true`
- **AND** 发出 `DSV_MSG_DEVICE_OPTIONS_UPDATED` 通知其它 GUI 组件
- **AND** 下一次 `View::rebuild_signals()` 不会丢失用户的开关操作

#### Scenario: LogicSignal 触发设置实时写回
- **GIVEN** 通道 0 当前触发类型为 NONTRIG
- **WHEN** 用户在 View 上把通道 0 设为上升沿触发
- **THEN** `LogicSignal::set_trig()` 实时调用 `SignalModel::set_trig_type(RISING)`
- **AND** 通过 Core 通知 TriggerDock UI 同步显示
- **AND** MCP/API 调用 `get_trigger_config` 立即返回最新的上升沿配置（无需等捕获开始）

#### Scenario: 切换 Tab 后触发设置保留
- **GIVEN** Tab A 中通道 0 设为上升沿触发
- **WHEN** 用户切换到 Tab B 再切回 Tab A
- **THEN** Tab A 的通道 0 仍显示上升沿触发
- **AND** LogicSignal 重建时从 SignalModel/SessionDocument 读取 trig_type 恢复

### Requirement: GUI 修改 Core 状态后广播

GUI 任何组件修改 Core 状态后 SHALL 发出对应 DSV_MSG_* 消息，确保其它 GUI 组件和 API 客户端能感知。

#### Scenario: SamplingBar 改采样率
- **WHEN** 用户在 SamplingBar 修改采样率并提交
- **THEN** `SamplingBar::commit_settings()` 写 `SR_CONF_SAMPLERATE` 后发 `DSV_MSG_DEVICE_OPTIONS_UPDATED`
- **AND** SigSession::OnMessage 收到后调 `reload()` 重建 SignalModel（如果不在采集中）
- **AND** 其它 GUI 组件（MeasureDock 等）收到通知刷新

#### Scenario: View rebuild_signals_from_config 改通道
- **WHEN** `View::rebuild_signals_from_config()` 修改 `probe->enabled`
- **THEN** 函数末尾发 `DSV_MSG_DEVICE_OPTIONS_UPDATED`
- **AND** DeviceOptionsDock 的通道勾选 UI 同步刷新

#### Scenario: TabContext::activate 应用 signal_config
- **WHEN** `TabContext::activate()` 调用 `apply_signal_config()` 修改 `probe->enabled`
- **THEN** activate 末尾发 `DSV_MSG_DEVICE_OPTIONS_UPDATED`
- **AND** 切 tab 后 DeviceOptionsDock UI 反映新 tab 的通道状态

### Requirement: API 层修改状态后广播事件

SessionService 任何修改 Core 状态的方法 SHALL 通过 `broadcast_event()` 推送 ServiceEvent 给所有 `IServiceEventListener`（包括 WsTransport），确保 WS 客户端能实时感知 MCP 调用引起的状态变化。

#### Scenario: MCP 调用 clear_all_decoders 后 WS 收到通知
- **GIVEN** 一个 WS 客户端已订阅事件
- **WHEN** MCP 客户端调用 `clear_all_decoders`
- **THEN** SessionService 内部调 `_session->clear_all_decoder(true)`（bUpdateView=true）
- **AND** 对每个被移除的 decoder 发 `ServiceEvent::DecoderRemoved`
- **AND** WS 客户端收到推送并刷新本地 UI

#### Scenario: MCP 修改采样率后 WS 收到通知
- **WHEN** MCP 客户端调用 `set_sample_rate`
- **THEN** SessionService 写入 Core 后发 `ServiceEvent::SampleConfigChanged`
- **AND** WS 客户端收到推送，本地缓存的 SampleConfig 失效

#### Scenario: MCP 修改通道使能后 WS 收到通知
- **WHEN** MCP 客户端调用 `set_channel_enabled`
- **THEN** SessionService 发 `ServiceEvent::ChannelConfigChanged`
- **AND** WS 客户端收到推送

#### Scenario: MCP 修改触发配置后 WS 收到通知
- **WHEN** MCP 客户端调用 `set_logic_trigger_config` 或 `set_dso_trigger_config`
- **THEN** SessionService 发 `ServiceEvent::TriggerConfigChanged`
- **AND** GUI 的 TriggerDock 收到通知刷新 UI

#### Scenario: MCP 加载/保存文件后广播
- **WHEN** MCP 客户端调用 `load_file` / `save_file` / `export_raw_data_csv`
- **THEN** 完成后分别发 `ServiceEvent::LoadComplete` / `SaveComplete` / `ExportComplete`
- **AND** WS 客户端收到推送

### Requirement: Headless 消息桥接自洽

Core 自己发出的 DSV_MSG_* 消息 SHALL 在 Core 内部直接广播给所有 IMessageListener，不依赖 GUI 中转。GUI 仍通过 ITriggerCallback 接收消息用于 UI 更新，但不再承担"回灌"职责。

#### Scenario: Headless 模式 REV_END_PACKET 处理
- **GIVEN** 应用以 `--headless` 模式运行，无 MainWindow
- **WHEN** 采集结束触发 `SigSession::data_feed_in(SR_DF_END)` 发出 `DSV_MSG_REV_END_PACKET`
- **THEN** SigSession::trigger_message 内部直接调 `broadcast_msg(DSV_MSG_REV_END_PACKET)`
- **AND** SigSession::OnMessage 收到并执行 buffer swap + add_decode_task + frame_ended
- **AND** 无需 headless 专用 workaround 分支

#### Scenario: GUI 模式行为不变
- **GIVEN** GUI 模式运行
- **WHEN** SigSession 发出 `DSV_MSG_START_COLLECT_WORK`
- **THEN** MainWindow 通过 ITriggerCallback 收到消息并更新 UI
- **AND** SigSession::OnMessage 也收到消息（如果该消息有 Core 处理逻辑）
- **AND** 顺序：Core OnMessage 先执行，GUI UI 更新后执行（与原顺序可能不同，需验证无回归）

#### Scenario: MainWindow 不再回灌
- **WHEN** MainWindow::on_trigger_message 被调用
- **THEN** 函数只执行 UI 更新逻辑
- **AND** 不再调用 `_session->broadcast_msg(msg)`

### Requirement: 捕获结果归属发起 Tab

采集完成后的数据 SHALL 复制到发起采集的 Tab 的 SessionDocument，而非当前活动 Tab。

#### Scenario: 跨 Tab 采集归属正确
- **GIVEN** 用户在 Tab A 启动采集，`_capture_owner_document == docA`
- **WHEN** 采集进行中用户切到 Tab B，采集结束触发 `on_frame_ended`
- **THEN** 数据复制到 `docA`（capture owner），不是 `docB`（current）
- **AND** Tab A 的 View 在用户切回时显示采集数据

#### Scenario: 切 Tab 时不改变 capture owner
- **GIVEN** 采集进行中，`_capture_owner_document == docA`
- **WHEN** 用户切到 Tab B，`TabContext::activate(docB)` 被调用
- **THEN** `set_active_document(docB)` 被跳过（因为 session 在 working）
- **AND** `_active_document` 仍为 docA
- **AND** END_COLLECT_WORK 后才执行 `set_active_document(docB)`

### Requirement: 死消息清理与发布点补全

所有已定义的 DSV_MSG_* SHALL 有至少一个发布点，或被显式移除。不允许存在"定义了但无人发布"的死消息。

#### Scenario: DSV_MSG_DEVICE_CONFIG_UPDATED 有发布点
- **WHEN** `DeviceAgent` 的 sample_rate / sample_limit / time_base / channel_mode 等配置变化
- **THEN** DeviceAgent 通过 `trigger_message(DSV_MSG_DEVICE_CONFIG_UPDATED)` 发布
- **AND** SessionService 的 OnMessage case 实际能被触发

#### Scenario: DSV_MSG_BEGIN_DEVICE_OPTIONS 被移除
- **WHEN** 重构 MainWindow::OnMessage 的合并 case
- **THEN** `DSV_MSG_BEGIN_DEVICE_OPTIONS` 宏从 icallbacks.h 移除
- **AND** 原 case 改用 `DSV_MSG_COLLECT_MODE_CHANGED` 单独处理

### Requirement: GUI 订阅处理中状态消息

GUI SHALL 订阅所有"处理中"和"进度"类 DSV_MSG_* 消息，确保用户能看到后台操作的进度反馈。

#### Scenario: 毛刺滤波显示处理中指示器
- **GIVEN** 用户启动毛刺滤波
- **WHEN** SigSession 发出 `DSV_MSG_GLITCH_FILTER_STARTED`
- **THEN** MainWindow::OnMessage 收到并显示"毛刺滤波处理中"状态栏指示器
- **AND** 后续 `DSV_MSG_GLITCH_FILTER_PROGRESS` 更新进度条
- **AND** `DSV_MSG_GLITCH_FILTER_COMPLETED` 隐藏指示器

#### Scenario: 信号反相显示处理中
- **GIVEN** 用户启动信号反相
- **WHEN** SigSession 发出 `DSV_MSG_SIGNAL_INVERT_STARTED`
- **THEN** MainWindow::OnMessage 显示"信号反相处理中"指示器
- **AND** 完成后隐藏

#### Scenario: 重复采集下一次启动可见
- **GIVEN** 重复采集模式，一次采集结束
- **WHEN** SigSession 发出 `DSV_MSG_TRIG_NEXT_COLLECT`
- **THEN** MainWindow::OnMessage 更新状态栏显示"等待下一次采集..."

---

## MODIFIED Requirements

### Requirement: SigSession::trigger_message 行为

原行为：`trigger_message(msg)` 只分发 ITriggerCallback，由 MainWindow 负责调 `broadcast_msg` 回灌给 Core。

修改后：`trigger_message(msg)` 分发 ITriggerCallback 后**立即**调用 `broadcast_msg(msg)`。MainWindow 不再回灌。

### Requirement: SessionService::clear_all_decoders 行为

原行为：`clear_all_decoders()` 调 `_session->clear_all_decoder(false)` 抑制 signals_changed。

修改后：`clear_all_decoders()` 调 `_session->clear_all_decoder(true)`，并对每个被移除的 decoder 显式 broadcast `DecoderRemoved` 事件。

### Requirement: view::Signal::set_enabled 行为

原行为：`set_enabled(bool)` 只设 `_local_enabled`，不写 `probe->enabled`。

修改后：`set_enabled(bool)` 同时设 `_local_enabled` 和 `probe->enabled`，并发出 `DSV_MSG_DEVICE_OPTIONS_UPDATED`。

### Requirement: MainWindow::on_frame_ended 归属逻辑

原行为：用 `current_context()->document()` 作为 copy 目标。

修改后：用 `_session->get_capture_owner_document()` 作为 copy 目标；若为空（异常情况）回退到 current_context。

---

## REMOVED Requirements

### Requirement: MainWindow::on_trigger_message 中调用 _session->broadcast_msg
**Reason**: 回灌职责迁移到 SigSession::trigger_message 内部，避免 headless 桥接断裂。
**Migration**: MainWindow::on_trigger_message 只保留 UI 更新逻辑。headless 专用 workaround（DSV_MSG_REV_END_PACKET else 分支显式调 frame_ended/add_decode_task）保留作为防御性代码，但不再是主路径。

### Requirement: DSV_MSG_BEGIN_DEVICE_OPTIONS 死消息
**Reason**: 全工程无发布者，仅 MainWindow::OnMessage 用作合并 case 的占位符。
**Migration**: 重构 MainWindow::OnMessage 的 case 分组，直接处理 `DSV_MSG_COLLECT_MODE_CHANGED` 等实际消息。

### Requirement: headless 模式 DSV_MSG_REV_END_PACKET 专用 workaround
**Reason**: R5 架构微调后，Core 直接 broadcast_msg，headless 下 OnMessage 自动被调用。
**Migration**: 移除 `sigsession.cpp` 中 headless 检测分支（保留作为防御性 assert，但主路径走 broadcast_msg）。注意：需验证 GUI 模式下不会双重处理（OnMessage 既被 Core broadcast 调用，又被 GUI 触发的链路调用）。
