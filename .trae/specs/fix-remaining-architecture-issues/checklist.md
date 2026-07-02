# Checklist

## A. 阶段 1（P0 紧急 bug）
- [x] `View::signals_modified_refresh`（view.cpp:2606）末尾调 `mark_derived_traces_dirty()`
- [x] `MainWindow::remove_tab` 在 destroy_context 前逐 stack 调 `stop_decode_work()` 停 decoder 线程
- [x] `View::set_data_document(nullptr)` 修复 early-return bug，实际清除 `_document` + signal data 指针
- [x] `MainWindow::remove_tab` 在 deleteLater 前调 `view->set_data_document(nullptr)`
- [x] `SigSession::Close()` join glitch_filter_thread + signal_invert_thread
- [x] `~SigSession()` 兜底调 `Close()`（idempotent）
- [x] `_glitch_filter_running`/`_signal_invert_running` 改 `std::atomic<bool>`
- [x] `_is_working`/`_copy_in_progress`/`_device_status` 改 `std::atomic`（Task 3 一并完成，原属 Task 5）
- [ ] 验证：Math 对话框确认后波形立即显示（无需切 Tab）（待运行时回归）
- [ ] 验证：Spectrum/Lissajous 对话框确认后波形立即显示（待运行时回归）
- [ ] 验证：关闭正在解码的 Tab 无崩溃（待运行时回归）
- [ ] 验证：关闭后 paint 事件不访问已释放 document（待运行时回归）
- [ ] 验证：app 关闭时毛刺任务在跑无 UAF（待运行时回归）

## B. 阶段 2（P1 分层与并发）
- [x] `LogicSignal::commit_trig` 无 `ds_trigger_*` 调用（8 处全部移除）
- [x] `view.cpp` `set_trig_cursor_posistion` 原 `ds_trigger_get_en()` 改查 Core `trigger_config()` + SignalModel trig_type
- [x] `_is_working` 改 `std::atomic<bool>`
- [x] `_copy_in_progress` 改 `std::atomic<bool>`
- [x] `_device_status` 改 `std::atomic<int>`
- [x] `_glitch_filter_running`/`_signal_invert_running` 改 `std::atomic<bool>`
- [x] CaptureOwnerGuard 用锁统一更新三态（`_capture_state_mutex`）
- [x] `clear_capture_owner_document` 用锁做原子快照（move-out-then-reset 模式）
- [x] copy_to_doc_done 路径用锁保护组合更新
- [x] `join_copy_thread`/`broadcast_msg` 均在锁外（防死锁）
- [x] 验证：增量编译 0 error
- [x] 验证：Grep sigsession.h 无残留 `volatile` 跨线程标志
- [ ] 验证：用户点 LogicSignal 触发 → 采集 → 触发正常工作（待运行时回归）
- [ ] 验证：触发游标显示正确（待运行时回归）

## C. 阶段 3（P2 半成品收口）
- [x] AGENTS.md "Typed event bus" 条目措辞修正为"RECOMMENDED, NOT hard constraint"
- [x] project_memory.md 移除"必须用 IEventListener"硬约束，改"推荐接口，非强制"
- [x] events.h 顶部加 STATUS 注释（0 消费者，翻译表 14/43 覆盖，4 个双重死代码事件）
- [x] events.h 补全 23 个事件结构体（总计 41 个），跳过 5 个 "prev" 内部消息
- [x] OnMessage 翻译表补全 24 个 case（总计 38/43 覆盖）
- [x] MainWindow 继承 IEventListener，override on_event(CaptureStateChanged) 作为 proof-of-concept
- [x] 4 个死代码事件处理：SampleRateChanged 已翻译；DecodeDone 在 decode 完成时发射；SignalsChanged 在 signals_changed() 发射；DataUpdated 保留供未来
- [x] Windows SDK `interface` 宏冲突修复（#undef guard）
- [x] kingst-la2016 feed_queue_logic 符号重名链接错误修复
- [x] 验证：增量编译 0 error；翻译表覆盖率 38/43

## D. 阶段 4（P3 结构性重构）
### CMake 拆分（Task 8）
- [x] `CMake/options.cmake`、`CMake/deps.cmake`、`CMake/flags.cmake` 已创建
- [x] `CMake/core_sources.cmake` / `CMake/gui_sources.cmake` 已创建
- [x] `CMake/decoders.cmake` + `CMake/libsigrok.cmake` 已创建
- [x] `CMake/install_packaging.cmake` 已创建
- [x] 主 CMakeLists.txt 134 行（< 150 目标）
- [x] 验证：全量编译 1245 步 0 error + install 成功

### MainWindow::OnMessage 拆分（Task 9）
- [x] 39 个 case 按职责分组（设备切换5/采集状态7/设备选项7/UI选项4/数据更新7/滤波反相7/触发2）
- [x] 7 个独立处理器方法已创建并声明于 mainwindow.h
- [x] OnMessage 退化为 79 行路由 switch（< 80 目标）
- [x] on_data_updated(int,int) 与 Qt slot 重载用 QOverload<>::of 消歧
- [x] 验证：增量编译 778 步 0 error（GUI 回归待运行时）

### SigSession 拆分（Task 10）— **DEFERRED: 长期演进，分批执行**
- [ ] 抽取 EventBus（_callbacks/_msg_listeners/_event_listeners/_broadcast_depth）
- [ ] 抽取 DocumentRegistry（_all_documents/_active_document/_capture_owner_document/_capture_owner_guard）
- [ ] 抽取 DecodeTaskManager（_decode_threads/_running_tasks）
- [ ] 抽取 DataFeedParser（feed_in_*）
- [ ] 抽取 FilterProcessor（glitch/signal_invert 线程）
- [ ] 抽取 CaptureManager（采集生命周期 + 6 套 DsTimer）
- [ ] SigSession 退化为 facade
- [ ] 验证：增量编译 0 error；GUI + Headless 回归
- **延后原因**: SigSession 3377 行 God 类，6 个 manager 提取涉及大量调用方更新，单次会话风险极高；EventBus 无法完全自包含（翻译表需访问内部状态）。spec 标注"长期演进，可分批执行"。

### SessionDocument 拆分（Task 11）— **DEFERRED: 依赖 Task 10**
- [ ] 抽取 SignalConfigStore（ChannelConfig/SignalConfig + save/apply）
- [ ] SessionDocument 保留纯数据
- [ ] 移除 DeviceAgent* 依赖（改由 SigSession 协调）
- [ ] UI 布局字段（view_index/v_offset/own_height）下沉 DockUiState
- [ ] 移除 `friend class TabContext`
- [ ] 验证：序列化/反序列化 .pxc 正常；Tab 切换 UI 布局保留
- **延后原因**: 依赖 Task 10 完成后的新 SigSession facade 结构。

## E. 文档更新
- [x] project_memory.md 新增 Lessons Learned（A1 dirty 标志、A2 Tab UAF、A3 线程 join + atomic、B2 分层泄漏、C4 mutex 组合状态、B1.2 事件总线迁移 + Windows interface 宏、C2 CMake 拆分、C5 OnMessage 拆分 + QOverload、Task 10 延后决策）共 9 条
- [x] AGENTS.md 事件总线措辞修正（Task 6 已完成：MUST → RECOMMENDED）
- [x] AGENTS.md SigSession/SessionDocument 拆分后结构：**N/A**（Task 10/11 延后，结构未变化，无需更新）

## F. 最终验证
- [x] 阶段 1-2 完成后增量编译 0 error（验证：`ninja -j 16` → "no work to do"，EXIT_CODE=0）
- [x] 阶段 4 完成后全量构建成功（Task 8 已验证 1245 步 0 error + install 成功，PXView.exe 257MB）
- [ ] GUI 回归：启动/采集/停止/解码/切 tab/关 tab/Math/Spectrum/Lissajous 无崩溃（待运行时，需用户手动验证）
- [ ] Headless 回归：get_devices → add_analyzer → start_capture → wait_capture → get_analyzer_results → export_raw_data_csv（待运行时，需用户手动验证）
- [x] project_memory.md 与 AGENTS.md 已更新
