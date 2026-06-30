# Tasks

- [x] Task 1: 修复 R5 线程安全回归（P0）
  - [x] SubTask 1.1: `mainwindow.cpp` `OnMessage` 顶部加线程检查，非 GUI 线程时 `QMetaObject::invokeMethod(this, [this,msg]{OnMessage(msg);}, Qt::QueuedConnection)` 转发后 return
  - [x] SubTask 1.2: `eventobject.h` 新增 4 个信号：`update_capture_sig()`、`show_region_sig(quint64,quint64,bool)`、`show_wait_trigger_sig()`、`repeat_hold_sig(int)`
  - [x] SubTask 1.3: `mainwindow.h/cpp` 4 个 ICaptureCallback 方法（`update_capture`/`show_region`/`show_wait_trigger`/`repeat_hold`）改为 emit 信号，新增 4 个 `on_*` 槽做实际工作，`connect` 用 `Qt::QueuedConnection`
  - [x] SubTask 1.4: `ws_transport.cpp` `on_service_event` 调 `sendTextMessage` 前 marshal 到主线程（`send_to_clients` 内 `QMetaObject::invokeMethod(qApp, ...)` 转发）

- [x] Task 2: trig_type 完整持久化（P0）
  - [x] SubTask 2.1: `mainwindow.cpp` setup_ui 的 `save_signal_config` 补 `_session->get_signal_models()`
  - [x] SubTask 2.2: `mainwindow.cpp` on_frame_ended 的 `save_signal_config` 补参数
  - [x] SubTask 2.3: `mainwindow.cpp` DSV_MSG_CURRENT_DEVICE_CHANGED 的 `save_signal_config` 补参数
  - [x] SubTask 2.4: `mainwindow.cpp` DSV_MSG_DEVICE_MODE_CHANGED 的 `save_signal_config` 补参数
  - [x] SubTask 2.5: `mainwindow.cpp` on_new_tab_requested 的 `save_signal_config` 补参数
  - [x] SubTask 2.6: `mainwindow.cpp` `apply_pending_config` 之后补 trig_type 回写逻辑（参考 `tabcontext.cpp:86-95`）
  - 注：DSV_MSG_DEVICE_OPTIONS_UPDATED 路径的 `save_signal_config` 也已补参数

- [x] Task 3: DeviceAgent 类型化 setter 补 config_changed()（P0）
  - [x] SubTask 3.1: `deviceagent.cpp` 9 个 setter（`set_config_string`/`set_config_bool`/`set_config_uint64`/`set_config_uint16`/`set_config_uint32`/`set_config_int16`/`set_config_int32`/`set_config_byte`/`set_config_double`）末尾补 `config_changed()` 调用

- [x] Task 4: _capture_owner_document 生命周期管理（P0）
  - [x] SubTask 4.1: `icallbacks.h` 新增 `DSV_MSG_CAPTURE_OWNER_CHANGED` 宏（编号 6033）
  - [x] SubTask 4.2: `sigsession.h/cpp` 新增 `clear_capture_owner_document(data::SessionDocument*)`：传入指针等于当前 owner 时置 nullptr 并广播 `DSV_MSG_CAPTURE_OWNER_CHANGED`
  - [x] SubTask 4.3: `sigsession.cpp` 后台 copy 线程从 detached 改为 joinable `std::thread` 成员（`_copy_thread`），提供 `join_copy_thread()`
  - [x] SubTask 4.4: `sigsession.cpp` `start_capture` 设置 owner 后广播 `DSV_MSG_CAPTURE_OWNER_CHANGED`
  - [x] SubTask 4.5: `mainwindow.cpp` `remove_tab` 在销毁 ctx 前调 `_session->join_copy_thread()` + `_session->clear_capture_owner_document(ctx->document())`
  - [x] SubTask 4.6: `mainwindow.cpp` `OnMessage` 新增 `DSV_MSG_CAPTURE_OWNER_CHANGED` case（is_working 时跳过 activate 避免重建波形轨道）

- [x] Task 5: 移除 on_frame_ended 双重启动（P0）
  - [x] SubTask 5.1: `mainwindow.cpp` 重构 on_frame_ended 分支：删除 `active_document != owner_doc` 的同步 copy+start 分支1；统一为 `!is_copy_in_progress()` 时同步 copy+start，否则等待 `DSV_MSG_COPY_TO_DOC_DONE`
  - [x] SubTask 5.2: 删除两条 "TEST FIX FOR HYPOTHESIS 1" 注释
  - [x] SubTask 5.3: 验证跨 tab 场景：copy 进行中 → 跳过同步 start → COPY_TO_DOC_DONE 单次 start

- [x] Task 6: Signal::set_enabled 写回 Core（P1）
  - [x] SubTask 6.1: `view/signal.cpp` `set_enabled(bool)` 增加 `if (_model) _model->set_enabled(enabled);`

- [x] Task 7: DSO/Analog 硬件参数写回 Core（P1）
  - [x] SubTask 7.1: `signalmodel.h/cpp` 有 `set_trig_value`/`set_vfactor` 接口与字段（注：vfactor 命名与 getter `vfactor()` 一致）
  - [x] SubTask 7.2: `dsosignal.cpp` `set_factor`/`set_acCoupling`/`set_trig_vpos`/`set_trig_ratio`/`set_zero_vpos`/`set_zero_ratio`/`go_vDialPre`/`go_vDialNext` 在写 libsigrok 后同步调 SignalModel 对应 setter
  - [x] SubTask 7.3: `analogsignal.cpp` `set_zero_vpos`/`set_zero_ratio` 同步写 SignalModel
  - [x] SubTask 7.4: 用户交互入口（set_acCoupling/set_factor/go_vDialPre/go_vDialNext）末尾广播 `DSV_MSG_DEVICE_OPTIONS_UPDATED`；restore 路径（set_trig_ratio/set_zero_ratio）不广播避免 rebuild 循环

- [x] Task 8: 高级触发配置纳入 Core（P1，架构补齐）
  - [x] SubTask 8.1: 新建 `PXView/pv/data/triggerconfig.h/.cpp`：`TriggerConfig` 类，持有 `mode`/`trigger_pos`/`stages`，提供 to_json/from_json
  - [x] SubTask 8.2: `CMakeLists.txt` 把 `triggerconfig.cpp` 加入 `PXVIEW_CORE_SOURCES`
  - [x] SubTask 8.3: `sigsession.h/cpp` 持有 `TriggerConfig _trigger_config` 成员，提供 getter/setter，setter 广播 `DSV_MSG_TRIGGER_CONFIG_CHANGED`；`icallbacks.h` 新增该宏（编号 6034）
  - [x] SubTask 8.4: `triggerdock.cpp` `commit_trigger` 高级分支先写 `_session->trigger_config()`
  - [x] SubTask 8.5: `triggerdock.cpp` `get_session`/`set_session` 从 Core `TriggerConfig` 序列化
  - [x] SubTask 8.6: `sessiondocument.h/cpp` 持久化 `TriggerConfig`
  - [x] SubTask 8.7: `session_service.cpp` `get_logic_trigger_config` 嵌入 Core `TriggerConfig.to_json()` 到 config_json，返回高级触发字段
  - [x] SubTask 8.8: `mainwindow.cpp` `OnMessage` 新增 `DSV_MSG_TRIGGER_CONFIG_CHANGED` case（刷新 TriggerDock UI）

- [x] Task 9: MCP transport 事件推送（P1，架构补齐）
  - [x] SubTask 9.1: `mcp_transport.h` 继承 `IServiceEventListener`，声明 override `on_service_event`
  - [x] SubTask 9.2: `mcp_transport.cpp` 实现 `on_service_event`：将服务事件转 SSE/JSON-RPC notification
  - [x] SubTask 9.3: `appcontrol.cpp` 注册 `active_session->add_event_listener(_ws_transport/_mcp_transport)` + `_app_service->add_event_listener(_ws_transport/_mcp_transport)`
  - [x] SubTask 9.4: 验证 MCP 客户端能收到 `CaptureStateChanged`/`SampleConfigChanged`/`DeviceConfigChanged` 推送

- [x] Task 10: 补全广播点（P1）
  - [x] SubTask 10.1: `session_service.cpp` `export_data`/`export_binary`/`export_decoder_table` 成功后广播 `ExportComplete`
  - [x] SubTask 10.2: `session_service.cpp` `configure_and_start` 成功后广播 `SampleConfigChanged`
  - [x] SubTask 10.3: `session_service.cpp` `enable_spectrum`/`enable_lissajous`/`enable_math` 成功后广播 `ChannelConfigChanged`
  - [x] SubTask 10.4: `triggerdock.cpp` `try_commit_trigger` 经 `commit_trigger` → `set_trigger_config` 广播 `DSV_MSG_TRIGGER_CONFIG_CHANGED`
  - [x] SubTask 10.5: `protocoldock.cpp` `OnProtocolVisibilityChanged` 广播 `DSV_MSG_DEVICE_OPTIONS_UPDATED`
  - [x] SubTask 10.6: `view.cpp` `add_decoder`/`remove_decoder` 经 `_session->broadcast_msg(DSV_MSG_DEVICE_OPTIONS_UPDATED)` 通知（View 不能直接调 SessionService；MCP 路径已在 session_service 广播 DecoderAdded/DecoderRemoved）
  - [x] SubTask 10.7: `session_service.cpp` `OnMessage` 补 `DSV_MSG_ACTIVE_DOCUMENT_CHANGED`/`DSV_MSG_COPY_IN_PROGRESS_CHANGED`/`DSV_MSG_CAPTURE_OWNER_CHANGED` case

- [x] Task 11: 杂项清理（P2）
  - [x] SubTask 11.1: `sigsession.cpp` `set_active_document` 加去重：传入指针等于 `_active_document` 时直接 return
  - [x] SubTask 11.2: `logicsignal.cpp` `set_trig` 仅当新值 != 旧值时才广播 `DSV_MSG_SIMPLE_TRIGGER_CHANGED`
  - [x] SubTask 11.3: `mainwindow.cpp` `on_frame_began` 在 `is_working()` 时跳过 `set_active_document`
  - [x] SubTask 11.4: `session_service.cpp` `set_collect_mode`/`set_repeat_interval` 加 Core 修改检查（old==new 时 return 不广播）

- [x] Task 12: AGENTS.md 更新（约束：总行数 ≤ 85）
  - [x] SubTask 12.1: 在 `## Conventions` 章节后新增 `## State Sync Conventions` 小节，涵盖：GUI 线程 marshal、状态变化必须广播、_capture_owner_document 生命周期、高级触发经 Core TriggerConfig
  - [x] SubTask 12.2: 总行数 82（在 78–85 范围内）
  - [x] SubTask 12.3: Build 章节补注：`build_incremental.cmd`/`.sh` 默认启动应用，仅编译用 `cd build && ninja -j 16 && ninja install`

- [x] Task 13: 构建验证 + checklist 核对
  - [x] SubTask 13.1: `ninja -j 16 && ninja install` 构建成功，无新增 warning
  - [x] SubTask 13.2: 逐项核对 `checklist.md`
  - [ ] SubTask 13.3: GUI 回归：启动/采集/停止/解码/切 tab/关 tab 无崩溃（用户验证）
  - [x] SubTask 13.4: 修复 start_capture 后波形轨道消失 bug（DSV_MSG_CAPTURE_OWNER_CHANGED handler 在 is_working 时跳过 activate）

# Task Dependencies
- Task 1（线程安全）独立，可并行
- Task 2（trig_type）独立，可并行
- Task 3（DeviceAgent）独立，可并行
- Task 4（owner 生命周期）独立，可并行
- Task 5（双启动移除）依赖 Task 4（owner 广播机制）
- Task 6（set_enabled 写回）独立，可并行
- Task 7（DSO/Analog 写回）依赖 Task 6（同样的写回模式）
- Task 8（TriggerConfig）独立，可并行，但工作量大
- Task 9（MCP 推送）独立，可并行
- Task 10（补全广播）依赖 Task 8（TriggerConfig 广播宏）、Task 4（owner 广播宏）
- Task 11（杂项）独立，可并行
- Task 12（AGENTS.md）应在 Task 1–11 完成后更新
- Task 13（验证）依赖所有 Task 完成

# 并行执行建议
- 第一批（独立）：Task 1、Task 2、Task 3、Task 6、Task 11
- 第二批（独立或弱依赖）：Task 4、Task 7、Task 8、Task 9
- 第三批（依赖前两批）：Task 5、Task 10
- 第四批：Task 12、Task 13
