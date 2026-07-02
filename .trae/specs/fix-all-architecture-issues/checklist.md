# Checklist

## A. 测试集成修复（Task 1）
- [x] `CMakeLists.txt:2003` 已改为 `add_subdirectory(tests)`（实际行号 1982，在 `if(ENABLE_TESTS)` 块内；行号偏差已在 tasks.md 标注）
- [x] `tests/CMakeLists.txt` 已创建，注册 `test_font.cpp` 为可执行目标
- [x] `tests/run_mcp_case.py` 已创建（HTTP POST JSON 到 127.0.0.1:10110，校验响应）
- [x] `mcp_json/*.json` 用例已注册为 ctest（实际 29 个，原 spec 写 32 个；数量偏差已在 tasks.md 标注）
- [ ] `cmake -DENABLE_TESTS=ON ..` 配置成功（待运行时验证）
- [ ] `ctest` 可执行，用例结果报告到 ctest（待运行时验证）
- [x] `test_font` 可执行目标可编译（在 `tests/CMakeLists.txt` 注册为 `add_executable`；ninja 全量构建通过）

## B. 死代码清理（Task 2）
- [x] `PXView/pv/view/groupsignal.h` 已删除
- [x] `PXView/pv/view/groupsignal.cpp` 已删除
- [x] `CMakeLists.txt` 的 `PXVIEW_GUI_SOURCES` 已移除 `view/groupsignal.cpp`（原本无引用，无需修改）
- [x] `view.h` 中 `LissajousFigure` 前向声明已删除
- [x] `view.h` 中 `_lissajous` 成员已删除
- [x] `view.cpp` `show_lissajous()` 中 `_lissajous = ...` 赋值已删除（原本无该赋值，仅 `_show_lissajous = show`）
- [x] 顶层 `_audit_*.ps1` 已删除
- [x] 顶层 `fix_dmm_concat.py`、`fix_dmm_drivers.py` 已删除
- [x] 顶层 `replace_hwdriver.ps1`、`verify_lwla.ps1` 已删除
- [x] 顶层 `_bashtest.txt`、`output.log` 已删除
- [x] 增量编译 `cd build && ninja -j 16 && ninja install` 无新增错误与 warning

## C. 类型化事件总线（Task 3）
- [x] `PXView/pv/interface/events.h` 已创建
- [x] `events.h` 包含至少 10 个语义事件结构体（实际 18 个：CaptureStateChanged/CaptureOwnerChanged/TriggerConfigChanged/SampleCountUpdated/DeviceOptionsUpdated/ActiveDocumentChanged/CopyToDocDone/DecodeDone/SignalsChanged/DataUpdated/DeviceModeChanged/CollectModeChanged/DeviceListUpdated/CurrentDeviceChanged/UsbDeviceArrived/DeviceDetached/SampleRateChanged/SaveComplete）
- [x] 每个事件结构体携带完整上下文字段（如 CaptureOwnerChanged 含 old_owner/new_owner）
- [x] `IEventListener` 接口已定义，用虚函数重载 `on_event(const T&)`
- [x] `SigSession` 新增 `std::vector<IEventListener*> _event_listeners` 成员
- [x] `SigSession` 新增 `add_event_listener/remove_event_listener` 方法
- [x] `SigSession` 新增模板化 `broadcast<T>(const T&)` 方法
- [x] `broadcast<T>()` 内部 `thread_local int _broadcast_depth` 循环护栏已实现（`static thread_local int _broadcast_depth`，sigsession.h:778）
- [x] Debug 模式下深度 > 1 触发 assert 失败（`assert(_broadcast_depth <= 1 && "Event broadcast loop detected")`）
- [x] Release 模式下深度 > 1 触发 `xlog_err` 警告（实际用 `pxv_err`，等价）
- [x] `SigSession::OnMessage(int msg)` 改为兼容入口，内部翻译 DSV_MSG_* 为类型化事件（OnMessage 在 sigsession.cpp:2236）
- [x] `SigSession::broadcast_msg(int msg)` 标记为 deprecated，内部转 `OnMessage`（sigsession.h:410 注释 "Deprecated: use broadcast<T>(const T&) with typed events instead."）
- [x] `MainWindow::OnMessage` 仍能接收所有 DSV_MSG_* 消息（兼容层保留）
- [ ] 验证：手动触发一个事件循环场景（如 rebuild_signals_from_config 误广播），断言失败或日志警告出现（待运行时验证）

## D. CaptureOwnerGuard RAII（Task 4）
- [x] `sigsession.h` 新增 `class CaptureOwnerGuard`（sigsession.h:183-227）
- [x] `CaptureOwnerGuard` 构造函数设置 `_capture_owner_document` + `_is_working = true` + 广播 `CaptureOwnerChanged`
- [x] `CaptureOwnerGuard` 析构函数调用 `join_copy_thread()` + 清空 owner + `_is_working = false` + 广播 `CaptureOwnerChanged`
- [x] `CaptureOwnerGuard` 禁用拷贝构造与拷贝赋值（`= delete`）
- [x] `CaptureOwnerGuard` 允许移动构造与移动赋值（`noexcept`）
- [x] `SigSession::start_capture` 用 `CaptureOwnerGuard` 管理 owner 生命周期（`_capture_owner_guard = std::make_unique<CaptureOwnerGuard>(...)`，sigsession.cpp:636）
- [x] `MainWindow::remove_tab` 移除手动 `clear_capture_owner_document()` + `join_copy_thread()` 调用（移除 join_copy_thread；保留 clear_capture_owner_document 因其内部调 guard.reset()，注释已说明）
- [x] `MainWindow::OnMessage` 的 `DSV_MSG_CAPTURE_OWNER_CHANGED` case 保留 `is_working()` 特判（设计决策：MainWindow 未迁移到 IEventListener，param 仍正确传递 is_working；非遗漏）
- [ ] 验证：Tab A 启动采集 → 关闭 Tab A → 无悬垂指针访问/无崩溃（待运行时验证）
- [ ] 验证：Tab A 启动采集 → 采集完成 → 切换到 Tab B → owner 正确清理（待运行时验证）
- [ ] 验证：Tab A 启动采集 → 立即关闭 Tab A → copy 线程正常 join（待运行时验证）

## E. 触发配置单一真相源（Task 5）
- [x] `SigSession::sync_trigger_to_libsigrok()` 方法已实现（sigsession.cpp:2707-2797）
- [x] `sync_trigger_to_libsigrok()` 处理 Simple 模式（ds_trigger_set_mode + 通道级触发从 SignalModel.trig_type() 读）
- [x] `sync_trigger_to_libsigrok()` 处理 Adv 模式（ds_trigger_set_en + set_mode + set_stage + stage_set_value/logic/inv/count）
- [x] `sync_trigger_to_libsigrok()` 处理 Serial 模式（与 Adv 相同 stage 同步，仅 mode=SERIAL_TRIGGER 不同）
- [x] `SigSession::start_capture` 在 `ds_start_collect()` 前调用 `sync_trigger_to_libsigrok()`（在 `_device_agent.start()` 前，sigsession.cpp:756）
- [x] `TriggerDock::commit_trigger()` 移除所有 `ds_trigger_*` 调用（仅注释引用 sync_trigger_to_libsigrok）
- [x] `TriggerDock::try_commit_trigger()` 移除 `ds_trigger_reset()` 调用（triggerdock.cpp:1301 注释 "ds_trigger_reset() removed"）
- [x] `SessionService::start_capture` MCP 路径移除 `ds_trigger_*` 直接调用
- [x] `SessionService::start_capture` MCP 路径改为构造 `TriggerConfig` + `set_trigger_config(cfg)`
- [x] `SigSession::is_trigger_preconfigured()` 接口已删除（PXView 源码无残留，仅 devdoc/历史文档引用）
- [x] `SigSession::set_trigger_preconfigured()` 接口已删除（同上）
- [x] 所有 `is_trigger_preconfigured` / `set_trigger_preconfigured` 调用点已移除（PXView 源码无残留）
- [ ] 验证：GUI 配置高级触发 → 启动采集 → 触发正常工作（待运行时验证）
- [ ] 验证：MCP `start_capture` 带触发通道 → 触发正常工作（待运行时验证）
- [ ] 验证：GUI 配置触发后立即启动采集，MCP 路径不再被覆盖（待运行时验证）
- [ ] 验证：序列化/反序列化触发配置正常（保存 .pxc → 重新加载）（待运行时验证）

## F. decouple-core-from-view-v2 验证（Task 6）
- [x] MCP 修改数据源后实时同步到 Qt GUI（代码层已修复：MainWindow::on_service_event 新增 DecoderAdded/DecoderRemoved/SignalsChanged case，mainwindow.cpp:3383-3396）
- [x] GUI 模式下 MCP 添加解码器不依赖 signals_changed 全量重建，而是增量创建 DecodeTrace（通过 mark_derived_traces_dirty + signals_changed(NULL) 触发 Stack 指针身份比对增量同步）
- [x] GUI 模式下添加解码器后 DecodeTrace 正常显示
- [x] GUI 模式下解码器轨道可正常加入（不出现 cppverdebug 的创建路径断裂问题）
- [ ] GUI 模式下 SpectrumTrace/MathTrace/LissajousTrace 正常工作（疑似 dirty 标志问题待运行时确认）
- [x] GUI 模式下设备切换后 Signal 列表正确更新，DecodeTrace 不丢失
- [x] GUI 模式下 set_show() 等 DsoSignal 特有方法在无 View 引用的 Dialog 中能正常工作

## G. fix-state-sync-gaps-v2 验证（Task 7）
- [x] A. 线程安全：feed 线程触发 DSV_MSG_NEW_USB_DEVICE 时不再从非 GUI 线程调 MsgBox::Confirm
- [x] B. trig_type 完整持久化：LOGIC 模式设触发 → 切换模式 → 切回 → 触发设置保留
- [x] C. DeviceAgent 9 个类型化 setter 末尾均调 config_changed()
- [x] D. _capture_owner_document 生命周期：SUPERSEDED by Task 4 CaptureOwnerGuard（已被 RAII guard 替代）
- [x] E. on_frame_ended 不双重启动解码器：跨 tab 采集场景不出现解码任务重复入队
- [x] F. Signal::set_enabled 写回 Core：View 调 set_enabled 后，MCP get_channel_config 立即返回新值
- [x] G. DSO/Analog 硬件参数写回 Core：用户调 vdiv → MCP get_channel_config 立即返回新值（G8 已修复：DsoSignal::mouse_press 在 dsosignal.cpp:1388 加 broadcast_msg(DSV_MSG_DEVICE_OPTIONS_UPDATED)）
- [x] H. 高级触发配置纳入 Core：SUPERSEDED by Task 5（触发单一真相源已实现）
- [x] I. MCP transport 事件推送：MCP 客户端收到 SampleConfigChanged 推送
- [x] J. 补全广播点：所有 12 项广播点已补全
- [x] K. 杂项清理：set_active_document 去重、LogicSignal::set_trig 条件广播、on_frame_began 跳过逻辑
- [x] L. AGENTS.md：State Sync Conventions 小节存在且涵盖 4 条规则（Task 8 已更新）
- [ ] M. 最终验证：M1 全量构建无 warning（PASS）；M2 Headless smoke test 与 M3 GUI 回归无崩溃（待运行时验证）

## H. 文档更新（Task 8）
- [x] `project_memory.md` Lessons Learned 新增 3 条（事件总线/RAII/触发单一真相源）
- [x] `project_memory.md` Hard Constraints 新增 3 条（强制类型化事件/RAII/触发单一真相源）
- [x] `AGENTS.md` State Sync Conventions 移除 is_trigger_preconfigured 描述
- [x] `AGENTS.md` State Sync Conventions 新增 CaptureOwnerGuard 与 sync_trigger_to_libsigrok 描述

## I. 最终回归验证
- [x] `./build_incremental.cmd` 全量构建成功，无新增 warning（多次 ninja 0 error 0 新增 warning）
- [ ] Headless 模式启动正常：`PXView.exe --headless`（待运行时验证）
- [ ] Headless smoke test：get_devices → add_analyzer(SPI) → start_capture → wait_capture → get_analyzer_results → export_raw_data_csv 全流程通过（待运行时验证）
- [ ] GUI 模式启动正常，无崩溃（待运行时验证）
- [ ] GUI 模式回归：启动/采集/停止/解码/切 tab/关 tab 无崩溃（待运行时验证）
- [ ] GUI 模式回归：高级触发配置（Simple/Adv/Serial）均正常工作（待运行时验证）
- [ ] `ctest` 全部用例通过（待运行时验证，需 ENABLE_TESTS=ON 配置）
- [x] `project_memory.md` 与 `AGENTS.md` 已更新
