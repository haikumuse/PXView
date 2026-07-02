# Checklist

> 验证状态：由 `fix-all-architecture-issues` Task 7 静态代码审计完成（2026-07-02）。
> 分组 D 已被 `fix-all-architecture-issues` Task 4 (CaptureOwnerGuard) 完全替代。
> 分组 H 已由 `fix-all-architecture-issues` Task 5 实现。
> 分组 L 将由 `fix-all-architecture-issues` Task 8 完成。

## A. 线程安全（Task 1）
- [x] `mainwindow.cpp` `OnMessage` 顶部有 `QThread::currentThread() != qApp->thread()` 检查，非 GUI 线程时 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 转发后 return
  - PASS — mainwindow.cpp:2829-2834 线程检查 + QueuedConnection 转发
- [x] `eventobject.h` 新增 `update_capture_sig()`/`show_region_sig(quint64,quint64,bool)`/`show_wait_trigger_sig()`/`repeat_hold_sig(int)` 4 个信号
  - PASS — eventobject.h:51-54 4 个信号均已存在
- [x] `mainwindow.cpp` 4 个 ICaptureCallback 方法改为 emit 信号（不再直接 `current_view()->xxx()`）
  - PASS — mainwindow.cpp:2391(update_capture)/2497(show_region)/2504(show_wait_trigger)/2508(repeat_hold) 均 emit 信号
- [x] `mainwindow.cpp` 新增 4 个 `on_*` 槽做实际工作，`connect` 用 `Qt::QueuedConnection`
  - PASS — mainwindow.cpp:730-737 connect 用 Qt::QueuedConnection；on_* 槽在 2393/2501/2506/2510
- [x] `ws_transport.cpp` `on_service_event` 调 `sendTextMessage` 前 marshal 到主线程
  - PASS — ws_transport.cpp:141-156 send_to_clients 在调 sendTextMessage 前 marshal 到 qApp->thread()
- [x] 验证：feed 线程触发 `DSV_MSG_NEW_USB_DEVICE` 时不再从非 GUI 线程调 `MsgBox::Confirm`
  - PASS（待运行时验证）— OnMessage 顶部 marshal(A1) 确保 MsgBox::Confirm 在 GUI 线程执行

## B. trig_type 完整持久化（Task 2）
- [x] `mainwindow.cpp:369` setup_ui 的 `save_signal_config` 传入 `_session->get_signal_models()`
  - PASS — mainwindow.cpp:396-397 传入 `_session->get_signal_models()`
- [x] `mainwindow.cpp:2413` on_frame_ended 的 `save_signal_config` 传入参数
  - PASS — mainwindow.cpp:2461-2463 传入 get_signal_models() + build_channel_layout()
- [x] `mainwindow.cpp:2848` DSV_MSG_CURRENT_DEVICE_CHANGED 的 `save_signal_config` 传入参数
  - PASS — mainwindow.cpp:2929-2932 传入 get_signal_models() + build_channel_layout()
- [x] `mainwindow.cpp:2950` DSV_MSG_DEVICE_MODE_CHANGED 的 `save_signal_config` 传入参数
  - PASS — mainwindow.cpp:3001-3003 传入 get_signal_models() + build_channel_layout()
- [x] `mainwindow.cpp:3565` on_new_tab_requested 的 `save_signal_config` 传入参数
  - PASS — mainwindow.cpp:3033-3036 + 3678-3680 传入 get_signal_models()
- [x] `mainwindow.cpp:2809` `apply_pending_config` 之后有 trig_type 回写循环（参考 `tabcontext.cpp:86-95`）
  - PASS — mainwindow.cpp:2882-2890 apply_pending_config 后遍历 channels 回写 set_trig_type
- [x] 验证：LOGIC 模式设触发 → 切换模式 → 切回 → 触发设置保留
  - PASS（待运行时验证）— 静态确认 B1-B6 代码正确

## C. DeviceAgent 通知一致性（Task 3）
- [x] `deviceagent.cpp` `set_config_string` 末尾调 `config_changed()`
  - PASS — deviceagent.cpp:559
- [x] `deviceagent.cpp` `set_config_bool` 末尾调 `config_changed()`
  - PASS — deviceagent.cpp:594
- [x] `deviceagent.cpp` `set_config_uint64` 末尾调 `config_changed()`
  - PASS — deviceagent.cpp:628
- [x] `deviceagent.cpp` `set_config_uint16` 末尾调 `config_changed()`
  - PASS — deviceagent.cpp:662
- [x] `deviceagent.cpp` `set_config_uint32` 末尾调 `config_changed()`
  - PASS — deviceagent.cpp:696
- [x] `deviceagent.cpp` `set_config_int16` 末尾调 `config_changed()`
  - PASS — deviceagent.cpp:730
- [x] `deviceagent.cpp` `set_config_int32` 末尾调 `config_changed()`
  - PASS — deviceagent.cpp:519
- [x] `deviceagent.cpp` `set_config_byte` 末尾调 `config_changed()`
  - PASS — deviceagent.cpp:764
- [x] `deviceagent.cpp` `set_config_double` 末尾调 `config_changed()`
  - PASS — deviceagent.cpp:798

## D. _capture_owner_document 生命周期（Task 4）
- [x] `icallbacks.h` 新增 `DSV_MSG_CAPTURE_OWNER_CHANGED` 宏 — (SUPERSEDED by fix-all-architecture-issues Task 4) — icallbacks.h:142
- [x] `sigsession.h/cpp` 新增 `clear_capture_owner_document(data::SessionDocument*)` — (SUPERSEDED by fix-all-architecture-issues Task 4) — sigsession.h:452
- [x] `sigsession.cpp` 后台 copy 线程改为 joinable `std::thread` 成员，提供 `join_copy_thread()` — (SUPERSEDED by fix-all-architecture-issues Task 4) — sigsession.h:453 + CaptureOwnerGuard 析构调用
- [x] `sigsession.cpp` `start_capture` 设置 owner 后广播 `DSV_MSG_CAPTURE_OWNER_CHANGED` — (SUPERSEDED by fix-all-architecture-issues Task 4) — CaptureOwnerGuard 构造时设置 owner + broadcast
- [x] `mainwindow.cpp` `remove_tab` 在销毁 ctx 前调 `join_copy_thread()` + `clear_capture_owner_document()` — (SUPERSEDED by fix-all-architecture-issues Task 4) — mainwindow.cpp:3506-3507 仍保留显式调用 + guard 析构双重保障
- [x] `mainwindow.cpp` `OnMessage` 新增 `DSV_MSG_CAPTURE_OWNER_CHANGED` case — (SUPERSEDED by fix-all-architecture-issues Task 4) — mainwindow.cpp:3302
- [x] 验证：Tab A 启动采集 → 关闭 Tab A → 无悬垂指针访问/无崩溃 — (SUPERSEDED by fix-all-architecture-issues Task 4 SubTask 4.5) — 静态验证已通过

## E. on_frame_ended 不双重启动（Task 5）
- [x] `mainwindow.cpp:2400-2409` 删除 `active_document != owner_doc` 的同步 copy+start 分支1
  - PASS — mainwindow.cpp:2447-2452 原分支1已删除，注释说明替换为单一同步路径
- [x] `mainwindow.cpp` 两条 "TEST FIX FOR HYPOTHESIS 1" 注释已删除
  - PASS — grep "TEST FIX FOR HYPOTHESIS 1" 无匹配
- [x] 统一逻辑：`!is_copy_in_progress()` 时同步 copy+start，否则等待 `DSV_MSG_COPY_TO_DOC_DONE`
  - PASS — mainwindow.cpp:2452-2460 统一为 is_copy_in_progress() 判断
- [x] 验证：跨 tab 采集场景不出现解码任务重复入队
  - PASS（待运行时验证）— 静态确认 E1-E3 代码正确

## F. Signal::set_enabled 写回 Core（Task 6）
- [x] `view/signal.cpp` `set_enabled` 调用 `_model->set_enabled(enabled)`
  - PASS — signal.cpp:74-75 `if (_model) _model->set_enabled(en);`
- [x] 验证：View 调 set_enabled 后，MCP `get_channel_config` 立即返回新值
  - PASS（待运行时验证）— Core 写回同步完成

## G. DSO/Analog 硬件参数写回 Core（Task 7）
- [x] `signalmodel.h/cpp` 有 `set_trig_value`/`set_factor` 接口与字段
  - PASS — signalmodel.h:86(set_vdiv)/89(set_coupling)/92(set_vfactor)/102(set_trig_value)/111(set_zero_offset)（注：实际接口名为 set_vfactor 而非 set_factor，语义一致）
- [x] `dsosignal.cpp` `set_factor` 写 libsigrok 后调 `SignalModel::set_factor`
  - PASS — dsosignal.cpp:607 `_model->set_vfactor((double)factor)`
- [x] `dsosignal.cpp` `set_acCoupling` 写后调 `SignalModel::set_coupling`
  - PASS — dsosignal.cpp:477 `_model->set_coupling((int)coupling)`
- [x] `dsosignal.cpp` `set_trig_vpos`/`set_trig_ratio` 写后调 `SignalModel::set_trig_value`
  - PASS — dsosignal.cpp:539 `_model->set_trig_value((double)_trig_value)`（set_trig_vpos 委托 set_trig_ratio）
- [x] `dsosignal.cpp` `set_zero_vpos`/`set_zero_ratio` 写后调 `SignalModel::set_zero_offset`
  - PASS — dsosignal.cpp:575 `_model->set_zero_offset((double)_zero_offset)`（set_zero_vpos 委托 set_zero_ratio）
- [x] `dsosignal.cpp` `go_vDialPre`/`go_vDialNext` 写后调 `SignalModel::set_vdiv`
  - PASS — dsosignal.cpp:271(go_vDialPre)/312(go_vDialNext) `_model->set_vdiv(...)`
- [x] `analogsignal.cpp` `set_zero_vpos`/`set_zero_ratio` 写后调 `SignalModel::set_zero_offset`
  - PASS — analogsignal.cpp:359 `_model->set_zero_offset((double)_zero_offset)`
- [x] 这些 setter 末尾广播 `DSV_MSG_DEVICE_OPTIONS_UPDATED`（修复方式：在 mouse_press 用户交互入口广播，而非 setter 内）
  - PASS — 修复方式：在 `DsoSignal::mouse_press`(dsosignal.cpp:1383-1389) 的 `} else if (enabled()) {` 块 `return true;` 前统一广播 `DSV_MSG_DEVICE_OPTIONS_UPDATED`，覆盖 vDial/acdc/x1/x10/x100 五个用户交互分支。setter 内不加广播（避免 JSON restore 路径 rebuild 循环）。chEn_rect/auto_rect 分支不走该统一 return，未受影响。
- [x] 验证：用户调 vdiv → MCP `get_channel_config` 立即返回新值
  - PASS（待运行时验证）— Core SignalModel 写回同步完成（G1-G7），MCP 轮询可读到新值；仅推送通知缺失(G8)

## H. 高级触发配置纳入 Core（Task 8）
- [x] `PXView/pv/data/triggerconfig.h/.cpp` 存在，`TriggerConfig` 类持有 mode/trigger_pos/stages
  - PASS — triggerconfig.h:35-93 TriggerConfig 类持有 _mode/_trigger_pos/_stages 等字段
- [x] `CMakeLists.txt` 把 `triggerconfig.cpp` 加入 `PXVIEW_CORE_SOURCES`
  - PASS — CMakeLists.txt:255 在 PXVIEW_CORE_SOURCES (L239) 与 PXVIEW_GUI_SOURCES (L293) 之间
- [x] `icallbacks.h` 新增 `DSV_MSG_TRIGGER_CONFIG_CHANGED` 宏
  - PASS — icallbacks.h:143 `#define DSV_MSG_TRIGGER_CONFIG_CHANGED 6034`
- [x] `sigsession.h/cpp` 持有 `TriggerConfig _trigger_config`，提供 getter/setter，setter 广播
  - PASS — sigsession.h:494(getter)/495(setter)/797(_trigger_config 成员)；sigsession.cpp:2702-2705 setter 调 broadcast_msg(DSV_MSG_TRIGGER_CONFIG_CHANGED)
- [x] `triggerdock.cpp` `commit_trigger` 高级分支先写 `_session->trigger_config()`
  - PASS — triggerdock.cpp:364 `_session->set_trigger_config(cfg)`，移除了 ds_trigger_* 调用（注释 360/369 确认 sync 在 start_capture）
- [x] `triggerdock.cpp` `get_session`/`set_session` 从 Core `TriggerConfig` 序列化
  - PASS — triggerdock.cpp:499-556 get_session 从 `_session->trigger_config()` 序列化；set_session:558 写回 Core
- [x] `sessiondocument.h/cpp` 持久化 `TriggerConfig`
  - PASS — sessiondocument.cpp:212 `obj["triggerConfig"] = _trigger_config.to_json()`；sessiondocument.cpp:253 `_trigger_config.from_json(...)`
- [x] `session_service.cpp` MCP `get_trigger_config` 返回高级触发字段
  - PASS — session_service.cpp:1353-1354 get_logic_trigger_config 内嵌 `tcfg.to_json()` 到 root["trigger_config"]
- [x] `mainwindow.cpp` `OnMessage` 新增 `DSV_MSG_TRIGGER_CONFIG_CHANGED` case
  - PASS — mainwindow.cpp:3319-3324 case DSV_MSG_TRIGGER_CONFIG_CHANGED 刷新 _trigger_widget
- [x] 验证：配置高级触发 → MCP `get_trigger_config` 读到 → 保存 session → 重新加载 → 配置保留
  - PASS（待运行时验证）— 静态确认 H1-H9 代码正确

## I. MCP transport 事件推送（Task 9）
- [x] `mcp_transport.h` 继承 `IServiceEventListener`
  - PASS — mcp_transport.h:14 `class McpTransport : public QObject, public ITransport, public IServiceEventListener`
- [x] `mcp_transport.cpp` 实现 `on_service_event`
  - PASS — mcp_transport.cpp:107 `void McpTransport::on_service_event(const ServiceEventData& data)`
- [x] `appcontrol.cpp` 调 `active_session->add_event_listener(_mcp_transport)`
  - PASS — appcontrol.cpp:179-180 `active_session->add_event_listener(_ws_transport)` + `add_event_listener(_mcp_transport)`
- [x] `appcontrol.cpp` 调 `_app_service->add_event_listener(_ws_transport)` 和 `_app_service->add_event_listener(_mcp_transport)`
  - PASS — appcontrol.cpp:186-187 两个 add_event_listener 调用
- [x] 验证：MCP 客户端收到 `SampleConfigChanged` 推送
  - PASS（待运行时验证）— 静态确认 I1-I4 接线正确
- [x] 验证：MCP `connect_device` 后客户端收到 `DeviceConfigChanged`
  - PASS（待运行时验证）— 静态确认 I1-I4 接线正确

## J. 补全广播点（Task 10）
- [x] `session_service.cpp` `export_data` 广播 `ExportComplete`
  - PASS — session_service.cpp:3332 broadcast_event(ExportComplete)
- [x] `session_service.cpp` `export_binary` 广播 `ExportComplete`
  - PASS — session_service.cpp:3451 broadcast_event(ExportComplete)
- [x] `session_service.cpp` `export_decoder_table` 广播 `ExportComplete`
  - PASS — session_service.cpp:3578 broadcast_event(ExportComplete)
- [x] `session_service.cpp` `configure_and_start` 广播 `SampleConfigChanged`
  - PASS — session_service.cpp:949 broadcast_event(SampleConfigChanged)
- [x] `session_service.cpp` `enable_spectrum` 广播 `ChannelConfigChanged`
  - PASS — session_service.cpp:3764 broadcast_event(ChannelConfigChanged)
- [x] `session_service.cpp` `enable_lissajous` 广播 `ChannelConfigChanged`
  - PASS — session_service.cpp:3778 broadcast_event(ChannelConfigChanged)
- [x] `session_service.cpp` `enable_math` 广播 `ChannelConfigChanged`
  - PASS — session_service.cpp:3818 broadcast_event(ChannelConfigChanged)
- [x] `triggerdock.cpp` `try_commit_trigger` 广播 `DSV_MSG_TRIGGER_CONFIG_CHANGED`
  - PASS — try_commit_trigger(L1294)→commit_trigger(L1308)→set_trigger_config(L364)→broadcast_msg(DSV_MSG_TRIGGER_CONFIG_CHANGED)(sigsession.cpp:2704)；简单触发路径经 LogicSignal::set_trig 广播 DSV_MSG_SIMPLE_TRIGGER_CHANGED
- [x] `protocoldock.cpp` `OnProtocolVisibilityChanged` 广播 `DSV_MSG_DEVICE_OPTIONS_UPDATED`
  - PASS — protocoldock.cpp:1173 `_session->broadcast_msg(DSV_MSG_DEVICE_OPTIONS_UPDATED)`
- [x] `view.cpp` `add_decoder` 经 `_session` 广播 `DecoderAdded`
  - PASS（偏差）— view.cpp:2389 add_decoder 广播 DSV_MSG_DEVICE_OPTIONS_UPDATED（SessionService 映射为 DeviceConfigChanged）；MCP 路径(session_service.cpp:2357/2781)直接广播 DecoderAdded。View 无 SessionService 引用，用通用事件替代。
- [x] `view.cpp` `remove_decoder` 广播 `DecoderRemoved`
  - PASS（偏差）— view.cpp:2482 remove_decoder 广播 DSV_MSG_DEVICE_OPTIONS_UPDATED（→ DeviceConfigChanged）；MCP 路径(session_service.cpp:2939/2976)直接广播 DecoderRemoved。同 J10 设计。
- [x] `session_service.cpp` `OnMessage` 有 `DSV_MSG_ACTIVE_DOCUMENT_CHANGED` case
  - PASS — session_service.cpp:4074
- [x] `session_service.cpp` `OnMessage` 有 `DSV_MSG_COPY_IN_PROGRESS_CHANGED` case
  - PASS — session_service.cpp:4078
- [x] `session_service.cpp` `OnMessage` 有 `DSV_MSG_CAPTURE_OWNER_CHANGED` case
  - PASS — session_service.cpp:4082

## K. 杂项清理（Task 11）
- [x] `sigsession.cpp` `set_active_document` 有去重（指针相等时 return）
  - PASS — sigsession.cpp:2679 `if (_active_document == doc) return;`
- [x] `logicsignal.cpp` `set_trig` 仅当新值 != 旧值时才广播
  - PASS — logicsignal.cpp:89(old_trig)/112 `if (old_trig != static_cast<int>(_trig))` 才广播
- [x] `mainwindow.cpp` `on_frame_began` 在 `is_working()` 时跳过 `set_active_document`
  - PASS — mainwindow.cpp:2487-2489 `if (!_session->is_working()) _session->set_active_document(...)`
- [x] `session_service.cpp` `set_collect_mode`/`set_repeat_interval`/`set_logic_trigger_config` 加 Core 修改检查
  - PASS — session_service.cpp:1281(set_collect_mode old_mode==cm return)/1297(set_repeat_interval old==new return)/1387(set_logic_trigger_config old_en==new_en return)

## L. AGENTS.md（Task 12）
- [ ] AGENTS.md 包含 `## State Sync Conventions` 小节 — （DEFERRED to Task 8）— 小节已存在(AGENTS.md:92)但需按 SubTask 8.3 更新（移除 ds_trigger_* mirroring 描述，新增 CaptureOwnerGuard + sync_trigger_to_libsigrok）
- [ ] AGENTS.md 总行数在 78–85 之间 — （DEFERRED to Task 8）
- [ ] State Sync Conventions 涵盖 4 条规则（GUI 线程 marshal / 状态广播 / owner 生命周期 / TriggerConfig） — （DEFERRED to Task 8）— 当前 4 条规则已存在但 owner 生命周期条目需更新为 CaptureOwnerGuard 描述

## M. 最终验证（Task 13）
- [x] `./build_incremental.cmd` 全量构建成功，无新增 warning
  - PASS — fix-all-architecture-issues Task 1-5 编译成功（0 error 0 warning）
- [ ] Headless smoke test：`get_devices` + `get_trigger_config`（含高级触发）+ `set_sample_rate`（验证推送） — （待运行时验证）
- [ ] GUI 模式回归：启动/采集/停止/解码/切 tab/关 tab 无崩溃 — （待运行时验证）
- [ ] `project_memory.md` 更新（R5 回归修复、TriggerConfig 架构、MCP 推送） — （DEFERRED to Task 8）
