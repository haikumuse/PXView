# Checklist

## A. 线程安全（Task 1）
- [ ] `mainwindow.cpp` `OnMessage` 顶部有 `QThread::currentThread() != qApp->thread()` 检查，非 GUI 线程时 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 转发后 return
- [ ] `eventobject.h` 新增 `update_capture_sig()`/`show_region_sig(quint64,quint64,bool)`/`show_wait_trigger_sig()`/`repeat_hold_sig(int)` 4 个信号
- [ ] `mainwindow.cpp` 4 个 ICaptureCallback 方法改为 emit 信号（不再直接 `current_view()->xxx()`）
- [ ] `mainwindow.cpp` 新增 4 个 `on_*` 槽做实际工作，`connect` 用 `Qt::QueuedConnection`
- [ ] `ws_transport.cpp` `on_service_event` 调 `sendTextMessage` 前 marshal 到主线程
- [ ] 验证：feed 线程触发 `DSV_MSG_NEW_USB_DEVICE` 时不再从非 GUI 线程调 `MsgBox::Confirm`

## B. trig_type 完整持久化（Task 2）
- [ ] `mainwindow.cpp:369` setup_ui 的 `save_signal_config` 传入 `_session->get_signal_models()`
- [ ] `mainwindow.cpp:2413` on_frame_ended 的 `save_signal_config` 传入参数
- [ ] `mainwindow.cpp:2848` DSV_MSG_CURRENT_DEVICE_CHANGED 的 `save_signal_config` 传入参数
- [ ] `mainwindow.cpp:2950` DSV_MSG_DEVICE_MODE_CHANGED 的 `save_signal_config` 传入参数
- [ ] `mainwindow.cpp:3565` on_new_tab_requested 的 `save_signal_config` 传入参数
- [ ] `mainwindow.cpp:2809` `apply_pending_config` 之后有 trig_type 回写循环（参考 `tabcontext.cpp:86-95`）
- [ ] 验证：LOGIC 模式设触发 → 切换模式 → 切回 → 触发设置保留

## C. DeviceAgent 通知一致性（Task 3）
- [ ] `deviceagent.cpp` `set_config_string` 末尾调 `config_changed()`
- [ ] `deviceagent.cpp` `set_config_bool` 末尾调 `config_changed()`
- [ ] `deviceagent.cpp` `set_config_uint64` 末尾调 `config_changed()`
- [ ] `deviceagent.cpp` `set_config_uint16` 末尾调 `config_changed()`
- [ ] `deviceagent.cpp` `set_config_uint32` 末尾调 `config_changed()`
- [ ] `deviceagent.cpp` `set_config_int16` 末尾调 `config_changed()`
- [ ] `deviceagent.cpp` `set_config_int32` 末尾调 `config_changed()`
- [ ] `deviceagent.cpp` `set_config_byte` 末尾调 `config_changed()`
- [ ] `deviceagent.cpp` `set_config_double` 末尾调 `config_changed()`

## D. _capture_owner_document 生命周期（Task 4）
- [ ] `icallbacks.h` 新增 `DSV_MSG_CAPTURE_OWNER_CHANGED` 宏
- [ ] `sigsession.h/cpp` 新增 `clear_capture_owner_document(data::SessionDocument*)`
- [ ] `sigsession.cpp` 后台 copy 线程改为 joinable `std::thread` 成员，提供 `join_copy_thread()`
- [ ] `sigsession.cpp` `start_capture` 设置 owner 后广播 `DSV_MSG_CAPTURE_OWNER_CHANGED`
- [ ] `mainwindow.cpp` `remove_tab` 在销毁 ctx 前调 `join_copy_thread()` + `clear_capture_owner_document()`
- [ ] `mainwindow.cpp` `OnMessage` 新增 `DSV_MSG_CAPTURE_OWNER_CHANGED` case
- [ ] 验证：Tab A 启动采集 → 关闭 Tab A → 无悬垂指针访问/无崩溃

## E. on_frame_ended 不双重启动（Task 5）
- [ ] `mainwindow.cpp:2400-2409` 删除 `active_document != owner_doc` 的同步 copy+start 分支1
- [ ] `mainwindow.cpp` 两条 "TEST FIX FOR HYPOTHESIS 1" 注释已删除
- [ ] 统一逻辑：`!is_copy_in_progress()` 时同步 copy+start，否则等待 `DSV_MSG_COPY_TO_DOC_DONE`
- [ ] 验证：跨 tab 采集场景不出现解码任务重复入队

## F. Signal::set_enabled 写回 Core（Task 6）
- [ ] `view/signal.cpp` `set_enabled` 调用 `_model->set_enabled(enabled)`
- [ ] 验证：View 调 set_enabled 后，MCP `get_channel_config` 立即返回新值

## G. DSO/Analog 硬件参数写回 Core（Task 7）
- [ ] `signalmodel.h/cpp` 有 `set_trig_value`/`set_factor` 接口与字段
- [ ] `dsosignal.cpp` `set_factor` 写 libsigrok 后调 `SignalModel::set_factor`
- [ ] `dsosignal.cpp` `set_acCoupling` 写后调 `SignalModel::set_coupling`
- [ ] `dsosignal.cpp` `set_trig_vpos`/`set_trig_ratio` 写后调 `SignalModel::set_trig_value`
- [ ] `dsosignal.cpp` `set_zero_vpos`/`set_zero_ratio` 写后调 `SignalModel::set_zero_offset`
- [ ] `dsosignal.cpp` `go_vDialPre`/`go_vDialNext` 写后调 `SignalModel::set_vdiv`
- [ ] `analogsignal.cpp` `set_zero_vpos`/`set_zero_ratio` 写后调 `SignalModel::set_zero_offset`
- [ ] 这些 setter 末尾广播 `DSV_MSG_DEVICE_OPTIONS_UPDATED`
- [ ] 验证：用户调 vdiv → MCP `get_channel_config` 立即返回新值

## H. 高级触发配置纳入 Core（Task 8）
- [ ] `PXView/pv/data/triggerconfig.h/.cpp` 存在，`TriggerConfig` 类持有 mode/trigger_pos/stages
- [ ] `CMakeLists.txt` 把 `triggerconfig.cpp` 加入 `PXVIEW_CORE_SOURCES`
- [ ] `icallbacks.h` 新增 `DSV_MSG_TRIGGER_CONFIG_CHANGED` 宏
- [ ] `sigsession.h/cpp` 持有 `TriggerConfig _trigger_config`，提供 getter/setter，setter 广播
- [ ] `triggerdock.cpp` `commit_trigger` 高级分支先写 `_session->trigger_config()`
- [ ] `triggerdock.cpp` `get_session`/`set_session` 从 Core `TriggerConfig` 序列化
- [ ] `sessiondocument.h/cpp` 持久化 `TriggerConfig`
- [ ] `session_service.cpp` MCP `get_trigger_config` 返回高级触发字段
- [ ] `mainwindow.cpp` `OnMessage` 新增 `DSV_MSG_TRIGGER_CONFIG_CHANGED` case
- [ ] 验证：配置高级触发 → MCP `get_trigger_config` 读到 → 保存 session → 重新加载 → 配置保留

## I. MCP transport 事件推送（Task 9）
- [ ] `mcp_transport.h` 继承 `IServiceEventListener`
- [ ] `mcp_transport.cpp` 实现 `on_service_event`
- [ ] `appcontrol.cpp` 调 `active_session->add_event_listener(_mcp_transport)`
- [ ] `appcontrol.cpp` 调 `_app_service->add_event_listener(_ws_transport)` 和 `_app_service->add_event_listener(_mcp_transport)`
- [ ] 验证：MCP 客户端收到 `SampleConfigChanged` 推送
- [ ] 验证：MCP `connect_device` 后客户端收到 `DeviceConfigChanged`

## J. 补全广播点（Task 10）
- [ ] `session_service.cpp` `export_data` 广播 `ExportComplete`
- [ ] `session_service.cpp` `export_binary` 广播 `ExportComplete`
- [ ] `session_service.cpp` `export_decoder_table` 广播 `ExportComplete`
- [ ] `session_service.cpp` `configure_and_start` 广播 `SampleConfigChanged`
- [ ] `session_service.cpp` `enable_spectrum` 广播 `ChannelConfigChanged`
- [ ] `session_service.cpp` `enable_lissajous` 广播 `ChannelConfigChanged`
- [ ] `session_service.cpp` `enable_math` 广播 `ChannelConfigChanged`
- [ ] `triggerdock.cpp` `try_commit_trigger` 广播 `DSV_MSG_TRIGGER_CONFIG_CHANGED`
- [ ] `protocoldock.cpp` `OnProtocolVisibilityChanged` 广播 `DSV_MSG_DEVICE_OPTIONS_UPDATED`
- [ ] `view.cpp` `add_decoder` 经 `_session` 广播 `DecoderAdded`
- [ ] `view.cpp` `remove_decoder` 广播 `DecoderRemoved`
- [ ] `session_service.cpp` `OnMessage` 有 `DSV_MSG_ACTIVE_DOCUMENT_CHANGED` case
- [ ] `session_service.cpp` `OnMessage` 有 `DSV_MSG_COPY_IN_PROGRESS_CHANGED` case
- [ ] `session_service.cpp` `OnMessage` 有 `DSV_MSG_CAPTURE_OWNER_CHANGED` case

## K. 杂项清理（Task 11）
- [ ] `sigsession.cpp` `set_active_document` 有去重（指针相等时 return）
- [ ] `logicsignal.cpp` `set_trig` 仅当新值 != 旧值时才广播
- [ ] `mainwindow.cpp` `on_frame_began` 在 `is_working()` 时跳过 `set_active_document`
- [ ] `session_service.cpp` `set_collect_mode`/`set_repeat_interval`/`set_logic_trigger_config` 加 Core 修改检查

## L. AGENTS.md（Task 12）
- [ ] AGENTS.md 包含 `## State Sync Conventions` 小节
- [ ] AGENTS.md 总行数在 78–85 之间
- [ ] State Sync Conventions 涵盖 4 条规则（GUI 线程 marshal / 状态广播 / owner 生命周期 / TriggerConfig）

## M. 最终验证（Task 13）
- [ ] `./build_incremental.cmd` 全量构建成功，无新增 warning
- [ ] Headless smoke test：`get_devices` + `get_trigger_config`（含高级触发）+ `set_sample_rate`（验证推送）
- [ ] GUI 模式回归：启动/采集/停止/解码/切 tab/关 tab 无崩溃
- [ ] `project_memory.md` 更新（R5 回归修复、TriggerConfig 架构、MCP 推送）
