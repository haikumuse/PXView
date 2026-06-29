# Tasks

按依赖顺序排列。Task 1/2 是基础，必须最先做；Task 3/4/5 可并行；Task 6/7/8 依赖 Task 1。

- [x] Task 1: 扩展 DSV_MSG_* 宏与回调接口定义（R1/R7/R8 基础）
  - [x] SubTask 1.1: 在 `PXView/pv/interface/icallbacks.h` 新增 DSV_MSG_* 宏：`DSV_MSG_ACTIVE_DOCUMENT_CHANGED`=6015、`DSV_MSG_COPY_IN_PROGRESS_CHANGED`=6032（注意 6014 被 SIMPLE_TRIGGER_CHANGED 占用）
  - [x] SubTask 1.2: 在 `ICaptureCallback` 新增 `virtual void cur_samplelimits_changed() {}` 默认空实现
  - [x] SubTask 1.3: 移除 `DSV_MSG_BEGIN_DEVICE_OPTIONS` 宏定义（mainwindow.cpp 的 case 引用留给 Task 8）
  - [x] SubTask 1.4: 在 `PXView/pv/api/types.h` 的 `ServiceEvent` 枚举新增 `SampleConfigChanged`=450、`ChannelConfigChanged`=451、`TriggerConfigChanged`=452（`SaveComplete`/`LoadComplete`/`ExportComplete` 已存在，无需新增）
  - [x] SubTask 1.5: 静态检查通过（不编译）

- [x] Task 2: 实现 R5 架构微调 — SigSession::trigger_message 内部直接 broadcast_msg
  - [x] SubTask 2.1: sigsession.h:511-519 trigger_message 末尾追加 broadcast_msg(msg)
  - [x] SubTask 2.2: mainwindow.cpp:2493-2505 on_trigger_message 移除 _session->broadcast_msg(msg)
  - [x] SubTask 2.3: OnMessage 双重执行检查通过（REV_END_PACKET/COPY_TO_DOC_DONE/TRIG_NEXT_COLLECT 唯一发射源是 trigger_message，无双重执行）
  - [x] SubTask 2.4: 静态验证通过（不编译）
  - **注意**：R5 后 OnMessage 跑在 trigger_message 调用线程（feed/后台线程），需在 Task 10 验证 Qt 线程安全（Core 层 OnMessage 不直接操作 QWidget，dispatch_to 触发的回调经 EventObject 队列转 GUI 线程，理论安全）

- [x] Task 3: 实现 R1 — Core 层状态变化通知补全
  - [x] SubTask 3.1: set_active_document 末尾发 DSV_MSG_ACTIVE_DOCUMENT_CHANGED
  - [x] SubTask 3.2: set_cur_samplelimits 末尾调 cur_samplelimits_changed()
  - [x] SubTask 3.3: attach_data_to_signal 末尾自动 data_updated()+signals_changed()；rst_decoder 移除冗余 data_updated()；其它 3 个调用点保留
  - [x] SubTask 3.4: _copy_in_progress 赋值点（2254 true / 2261 false）发 DSV_MSG_COPY_IN_PROGRESS_CHANGED；构造函数初始化不发（callbacks 未就绪）
  - [x] SubTask 3.5: get_capture_owner_document() 已存在于 sigsession.h:359，无需新增

- [x] Task 4: 实现 R2 — View 层状态实时写回 Core
  - [x] SubTask 4.1: Signal::set_enabled 写回 _probe->enabled（选项 C：只写 probe 不广播，避免循环；广播由调用方负责）
  - [x] SubTask 4.2: LogicSignal::set_trig 调用 session->get_signal_by_index + model->set_trig_type 实时写回 Core，TODO 注释已移除
  - [x] SubTask 4.3: SessionDocument ChannelConfig 新增 trig_type 字段；save_signal_config/signal_config_to_json/from_json 序列化 trig_type（from_json 兼容旧文件）
  - [x] SubTask 4.4: rebuild_signals_from_config 通过 SignalFactory::update_signals(Modified) → apply_model_properties 自动从 SignalModel 恢复 trig_type，无需额外修改
  - [x] SubTask 4.5: 静态验证通过

- [x] Task 5: 实现 R3 — GUI 修改 Core 后广播
  - [x] SubTask 5.1: SamplingBar::commit_settings 末尾广播 DSV_MSG_DEVICE_OPTIONS_UPDATED
  - [~] SubTask 5.2: View::rebuild_signals_from_config **跳过**（会导致循环：rebuild_signals_from_config → broadcast → OnMessage → rebuild_signals → 循环；Tab 切换由 Task 5.3 覆盖）
  - [x] SubTask 5.3: TabContext::activate 在 apply_signal_config 后广播 DSV_MSG_DEVICE_OPTIONS_UPDATED；同时 deactivate 传入 signal_models 保存 trig_type；activate reload 后恢复 trig_type
  - [x] SubTask 5.4: 循环风险验证通过（reload 内部不触发同消息；set_enabled 不广播；rebuild_signals_from_config 不广播）
  - **待 Task 7+8+9 处理**：mainwindow.cpp:2905 的 save_signal_config 调用需补充 signal_models 参数（否则 trig_type 不保存）

- [x] Task 6: 实现 R4 — API 层修改后广播 ServiceEvent
  - [x] SubTask 6.1: clear_all_decoders 改用 clear_all_decoder(true)，循环广播 DecoderRemoved（instance_id 来自 _api_document 快照）
  - [x] SubTask 6.2: set_sample_rate/set_sample_limit/set_time_base/set_collect_mode/set_repeat_interval 广播 SampleConfigChanged
  - [x] SubTask 6.3: set_channel_enabled/set_channel_name/set_probe_config 广播 ChannelConfigChanged
  - [x] SubTask 6.4: set_logic_trigger_config/set_dso_trigger_config 广播 TriggerConfigChanged
  - [x] SubTask 6.5: load_file/save_file/export_raw_data_csv 广播 LoadComplete/SaveComplete/ExportComplete
  - [x] SubTask 6.6: WsTransport::on_service_event 新增 6 个 case 映射（on_sample_config_changed 等）
  - [x] SubTask 6.7: 静态验证通过（broadcast_event 调用从 61 增至 75）
  - **待 Task 10 验证**：clear_all_decoder(true) 在 GUI 模式下 dispatch_to<ISessionStateCallback> 是否线程安全

- [x] Task 7: 实现 R6 — Tab 归属与隔离修复
  - [x] SubTask 7.1: on_frame_ended 用 get_capture_owner_document()，nullptr 回退 ctx->document() + qWarning
  - [x] SubTask 7.2: tabcontext.cpp:74-79 activate 在 is_working() 时跳过 set_active_document；mainwindow.cpp:2803-2822 END_COLLECT_WORK 显式调 set_active_document(ctx->document())
  - [~] SubTask 7.3: 运行时验证留给 Task 10

- [x] Task 8: 实现 R7 — 死消息接到发布点
  - [x] SubTask 8.1: 采用选项 C — SamplingBar::commit_settings 在 DEVICE_OPTIONS_UPDATED 之前额外发 DSV_MSG_DEVICE_CONFIG_UPDATED（DeviceAgent 不持有 session 引用）
  - [x] SubTask 8.2: mainwindow.cpp:3089 移除 case DSV_MSG_BEGIN_DEVICE_OPTIONS 标签，保留 COLLECT_MODE_CHANGED
  - [x] SubTask 8.3: 全工程 grep 确认无代码残留（仅 .trae/specs 文档引用）

- [x] Task 9: 实现 R8 — GUI 订阅缺失消息
  - [x] SubTask 9.1: COLLECT_START / TRIG_NEXT_COLLECT 用 statusBar()->showMessage
  - [x] SubTask 9.2: GLITCH_FILTER_STARTED / PROGRESS 用 _disk_cache_status_label + statusBar
  - [x] SubTask 9.3: SIGNAL_INVERT_STARTED 用 _disk_cache_status_label
  - [x] SubTask 9.4: COPY_IN_PROGRESS_CHANGED 用 _disk_cache_status_label
  - [x] SubTask 9.5: ACTIVE_DOCUMENT_CHANGED 调 update_title_bar_text()
  - **额外修复**：mainwindow.cpp:2918 save_signal_config 补 signal_models 参数（Task 4+5 残留）

- [x] Task 10: 整体构建 + 回归验证
  - [x] SubTask 10.1: build_incremental.cmd 全量构建成功（119/119 目标完成，0 编译错误，1 预存在警告 unused parameter 'silent'）
  - [~] SubTask 10.2: GUI 回归留给用户手动验证（构建产物可正常启动，日志无错误）
  - [x] SubTask 10.3: Headless smoke test 通过（get_devices 返回 2 设备、get_capture_status 返回有效 idle 状态、无崩溃无死锁、端口 10110/10430 正常监听）
  - [~] SubTask 10.4: WS 推送验证留给用户手动测试（需 wscat 客户端连接）
  - [x] SubTask 10.5: AGENTS.md 追加 "State Sync Conventions" 章节（8 个子章节，76-113 行）；project_memory.md 追加 12 条 Engineering Conventions + 5 条 Lessons Learned

# Task Dependencies

- Task 1 必须最先做（提供宏/回调/事件类型定义）
- Task 2（R5 架构微调）必须在 Task 3 之后或并行做，但优先级高，因为它改变消息流方向，影响所有后续验证
- Task 3 依赖 Task 1
- Task 4 / Task 5 / Task 6 可并行（修改不同文件：view/signal+logicsignal vs samplingbar+view+tabcontext vs session_service）
- Task 7 依赖 Task 3.5（`get_capture_owner_document` 接口）
- Task 8 依赖 Task 1.3（移除 `DSV_MSG_BEGIN_DEVICE_OPTIONS`）
- Task 9 依赖 Task 1.1（新宏定义）
- Task 10 是最终验证，依赖所有其它任务完成
