# Checklist

## R1: Core 内部状态变化通知完整性
- [x] `icallbacks.h` 新增 `DSV_MSG_ACTIVE_DOCUMENT_CHANGED`、`DSV_MSG_COPY_IN_PROGRESS_CHANGED` 宏定义
- [x] `ICaptureCallback` 新增 `cur_samplelimits_changed()` 虚方法（默认空实现）
- [x] `SigSession::set_active_document()` 发出 `DSV_MSG_ACTIVE_DOCUMENT_CHANGED`
- [x] `SigSession::set_cur_samplelimits()` 调用 `cur_samplelimits_changed()` 回调
- [x] `SigSession::attach_data_to_signal()` 内部自动发 `data_updated()` + `signals_changed()`
- [x] `SigSession::_copy_in_progress` 变化时发 `DSV_MSG_COPY_IN_PROGRESS_CHANGED`
- [x] 所有 `attach_data_to_signal` 调用点已扫描，无冗余的手动补发

## R2: View 层状态实时写回 Core
- [x] `view::Signal::set_enabled(bool)` 同时写 `_local_enabled` 和 `probe->enabled`
- [~] `view::Signal::set_enabled` 通过 Core 通知（DSV_MSG_DEVICE_OPTIONS_UPDATED） — 部分通过：实现只写 `probe->enabled` 不广播，由调用方广播（合理避免循环设计，signal.cpp:60-62 注释明确说明）
- [x] `LogicSignal::set_trig()` 实时调 `SignalModel::set_trig_type()`
- [x] logicsignal.cpp:98-105 的 TODO 注释已移除（已替换为 R2 注释）
- [x] `SignalModel` 的 trig_type 持久化接口完善
- [x] `SessionDocument::save_signal_config` / `apply_signal_config` 包含 trig_type
- [x] `View::rebuild_signals_from_config` 创建 LogicSignal 时从 SignalModel 恢复 trig_type（通过 `SignalFactory::apply_model_properties` → `set_trig(model->trig_type())`）
- [~] 验证：View header 点击通道开关 → rebuild_signals → 操作保留 — 待运行时验证
- [~] 验证：LogicSignal 设触发 → MCP `get_trigger_config` 立即返回新值 — 待运行时验证
- [~] 验证：Tab A 设上升沿触发 → 切 Tab B → 切回 Tab A → 触发设置保留 — 待运行时验证

## R3: GUI 修改 Core 状态后广播
- [x] `SamplingBar::commit_settings()` 发 `DSV_MSG_DEVICE_OPTIONS_UPDATED`
- [~] `View::rebuild_signals_from_config()` 发 `DSV_MSG_DEVICE_OPTIONS_UPDATED` — 设计决策跳过（避免 rebuild → broadcast → OnMessage → rebuild 循环；Tab 切换由 TabContext::activate 覆盖）
- [x] `TabContext::activate()` 在 apply_signal_config 后发 `DSV_MSG_DEVICE_OPTIONS_UPDATED`
- [x] 验证：无消息循环（reload 触发 DEVICE_OPTIONS_UPDATED 死循环已避免）— 通过代码分析确认

## R4: API 层修改状态后广播事件
- [x] `ServiceEvent::Type` 新增 SampleConfigChanged / ChannelConfigChanged / TriggerConfigChanged / LoadComplete / SaveComplete / ExportComplete
- [x] `clear_all_decoders` 改用 `clear_all_decoder(true)` 并广播 DecoderRemoved
- [x] `set_sample_rate` / `set_sample_limit` / `set_time_base` / `set_collect_mode` / `set_repeat_interval` 广播 SampleConfigChanged
- [x] `set_channel_enabled` / `set_channel_name` / `set_probe_config` 广播 ChannelConfigChanged
- [x] `set_logic_trigger_config` / `set_dso_trigger_config` 广播 TriggerConfigChanged
- [x] `load_file` / `save_file` / `export_raw_data_csv` 广播 LoadComplete / SaveComplete / ExportComplete
- [x] `WsTransport::on_service_event` 新增事件类型到 JSON 推送映射
- [~] 验证：MCP 调 `set_sample_rate` → WS 客户端收到 `SampleConfigChanged` — 待运行时验证
- [~] 验证：MCP 调 `clear_all_decoders` → WS 客户端收到 `DecoderRemoved` — 待运行时验证

## R5: Headless 消息桥接自洽
- [x] `SigSession::trigger_message(int)` 末尾调 `broadcast_msg(msg)`
- [x] `MainWindow::on_trigger_message` 移除 `_session->broadcast_msg(msg)` 调用
- [x] GUI 模式下 `SigSession::OnMessage` 不会双重执行（无 GUI 回灌 + Core broadcast 一次）— 通过代码分析确认
- [x] Headless 模式下 `DSV_MSG_REV_END_PACKET` 处理路径自动工作 — smoke test 间接验证
- [x] Headless 专用 workaround 分支降级为防御性 assert 或移除 — 保留为正常路径，无特殊 workaround 分支
- [~] 验证：GUI 模式 START_COLLECT_WORK / END_COLLECT_WORK / REV_END_PACKET 流程无回归 — 待运行时验证
- [~] 验证：Headless 模式 MCP 完整流程通过 — smoke test 部分通过

## R6: 捕获结果归属发起 Tab
- [x] `SigSession` 暴露 `get_capture_owner_document()` getter
- [x] `MainWindow::on_frame_ended` 用 `get_capture_owner_document()` 取目标，nullptr 时回退 current_context
- [x] `TabContext::activate` 在 `is_working()` 时跳过 `set_active_document`
- [x] END_COLLECT_WORK 处理时显式调用 `set_active_document`
- [~] 验证：Tab A 启动采集 → 切 Tab B → 采集结束 → 数据落到 Tab A — 待运行时验证

## R7: 死消息清理与发布点补全
- [x] `DSV_MSG_DEVICE_CONFIG_UPDATED` 在 DeviceAgent 配置变化时发布（简化方案：由 `SamplingBar::commit_settings` 发布）
- [x] `DSV_MSG_BEGIN_DEVICE_OPTIONS` 宏从 icallbacks.h 移除
- [x] `MainWindow::OnMessage` 重构 case 分组，直接处理 `DSV_MSG_COLLECT_MODE_CHANGED`
- [x] 全工程 grep 确认 `DSV_MSG_BEGIN_DEVICE_OPTIONS` 无残留引用（仅 .trae/specs 文档引用）
- [x] SessionService 的 `DSV_MSG_DEVICE_CONFIG_UPDATED` case 实际能被触发（通过 SamplingBar 发布点）

## R8: GUI 订阅处理中状态消息
- [x] `MainWindow::OnMessage` 新增 `DSV_MSG_COLLECT_START` case
- [x] `MainWindow::OnMessage` 新增 `DSV_MSG_TRIG_NEXT_COLLECT` case
- [x] `MainWindow::OnMessage` 新增 `DSV_MSG_GLITCH_FILTER_STARTED` case
- [x] `MainWindow::OnMessage` 新增 `DSV_MSG_GLITCH_FILTER_PROGRESS` case
- [x] `MainWindow::OnMessage` 新增 `DSV_MSG_SIGNAL_INVERT_STARTED` case
- [x] `MainWindow::OnMessage` 新增 `DSV_MSG_COPY_IN_PROGRESS_CHANGED` case
- [x] `MainWindow::OnMessage` 新增 `DSV_MSG_ACTIVE_DOCUMENT_CHANGED` case

## 最终验证
- [x] `./build_incremental.cmd` 全量构建成功，无新增 warning（PXView.exe 构建于 2026/6/30，119/119 目标完成）
- [~] GUI 模式回归测试通过（启动/采集/停止/解码/切 tab/关 tab） — 待用户手动验证
- [~] Headless 模式 MCP 完整流程通过（get_devices → add_analyzer → start_capture → wait_capture → get_capture_status → get_analyzer_results → export_raw_data_csv） — smoke test 部分通过
- [~] WS 推送验证通过（MCP 调 set_sample_rate，WS 客户端收到 SampleConfigChanged） — 待用户手动测试
- [x] `AGENTS.md` 更新 Lessons Learned 和 Engineering Conventions（追加 "State Sync Conventions" 章节，AGENTS.md:76-113）
- [x] `project_memory.md` 更新（R5 桥接改动、broadcast_msg 不再由 GUI 调用）— 文件在 c:\Users\admin\.trae-cn\memory\projects\-c-Users-admin-Downloads-DSView-main-2026-4-27cppnb\project_memory.md，行 31-35 新增 5 条 Lessons Learned（注：验证 sub-agent 在项目目录下搜索找不到，实际在 memory 目录下，已确认更新）
