# Tasks

## 阶段 1（P0 紧急 bug 修复）

- [x] Task 1: 修复 Math/Spectrum/Lissajous 波形不显示（A1 dirty 标志短路）
  - [x] SubTask 1.1: `View::signals_modified_refresh`（view.cpp:2606）末尾调 `mark_derived_traces_dirty()`
  - [x] SubTask 1.2: 验证：增量编译 0 error（运行时 MathOptions 确认后 MathTrace 立即显示待 GUI 回归）

- [x] Task 2: 修复 Tab 关闭 View→Document UAF（A2 销毁时序）
  - [x] SubTask 2.1: `MainWindow::remove_tab` 在 destroy_context 前逐 stack 调 `stop_decode_work()` 停 decoder 线程（避免 clear_all_documents_decoders 影响其他 Tab）
  - [x] SubTask 2.2: 修复 `View::set_data_document(nullptr)` 原 early-return 不清 `_document` 的 bug，在 deleteLater 前调用解绑 View→Document 指针
  - [x] SubTask 2.3: 验证：增量编译 0 error（运行时关闭正在解码的 Tab 无崩溃待 GUI 回归）

- [x] Task 3: 修复 glitch/invert 线程析构未 join（A3 后台线程生命周期）
  - [x] SubTask 3.1: `SigSession::Close()` 加 glitch_filter_thread/signal_invert_thread 的 joinable+join+delete+nullptr
  - [x] SubTask 3.2: `~SigSession()` 兜底调 `Close()`（idempotent via _bClose guard）
  - [x] SubTask 3.3: `_is_working`/`_device_status`/`_glitch_filter_running`/`_signal_invert_running`/`_copy_in_progress` 全改 `std::atomic`
  - [x] SubTask 3.4: 验证：增量编译 0 error；Grep sigsession.h 无残留 volatile 跨线程标志

## 阶段 2（P1 分层与并发加固）

- [x] Task 4: 移除 View 层 ds_trigger_* 直调（B2 分层泄漏）
  - [x] SubTask 4.1: `LogicSignal::commit_trig` 移除全部 8 处 `ds_trigger_probe_set`/`ds_trigger_set_en`，简化为 `return _trig != NONTRIG`
  - [x] SubTask 4.2: `view.cpp` `set_trig_cursor_posistion` 中 `ds_trigger_get_en()` 改查 Core `trigger_config()` + SignalModel trig_type
  - [x] SubTask 4.3: 验证：增量编译 0 error（运行时触发功能待回归）

- [x] Task 5: 跨线程标志 atomic 化（C4 UB 修复）
  - [x] SubTask 5.1-5.3: `_is_working`/`_copy_in_progress`/`_device_status`/`_glitch_filter_running`/`_signal_invert_running` 全改 `std::atomic`（Task 3 一并完成）
  - [x] SubTask 5.4: 新增 `mutable std::mutex _capture_state_mutex`；CaptureOwnerGuard 构造/析构/move 用锁统一更新 `_is_working`/`_capture_owner_document`；`clear_capture_owner_document` 用锁做原子快照；copy_to_doc_done 路径用锁保护组合更新；join_copy_thread/broadcast_msg 均在锁外（防死锁）
  - [x] SubTask 5.5: 验证：增量编译 0 error；Grep sigsession.h 无残留 volatile 跨线程标志

## 阶段 3（P2 半成品收口）

- [x] Task 6: 更新 AGENTS.md 事件总线措辞（B1.1）
  - [x] SubTask 6.1: AGENTS.md "Typed event bus" 条目改为"RECOMMENDED interface, NOT a hard constraint"，注明 0 消费者/14 of 43 翻译表覆盖/待 Task 9 后迁移
  - [x] SubTask 6.2: project_memory.md Hard Constraints 中"新代码必须用 IEventListener"改为"推荐接口，非强制"
  - [x] SubTask 6.3: events.h 顶部加 STATUS 注释说明当前状态（0 消费者，翻译表 14/43 覆盖，4 个双重死代码事件）

- [x] Task 7: 事件总线真正迁移（B1.2，依赖 Task 9）
  - [x] SubTask 7.1: events.h 新增 23 个事件结构体（总计 41 个），跳过 5 个 "prev" 内部状态机消息
  - [x] SubTask 7.2: SigSession::OnMessage 翻译表新增 24 个 case（总计 38/43，剩余 5 个为 "prev" 内部消息不翻译）
  - [x] SubTask 7.3: MainWindow 继承 IEventListener，override on_event(CaptureStateChanged)，构造/析构中 register/unregister
  - [x] SubTask 7.4: SampleRateChanged 现有翻译；DecodeDone 在 decode_single_task 完成时发射；SignalsChanged 在 signals_changed() 内联发射；DataUpdated 保留供未来使用
  - [x] SubTask 7.5: 验证：增量编译 0 error；翻译表覆盖率 38/43（5 个 "prev" 消息不翻译）；MainWindow on_event override 已注册
  - [x] 附带修复：Windows SDK `interface` 宏冲突（#undef guard）；kingst-la2016 feed_queue_logic 符号重名链接错误

## 阶段 4（P3 结构性重构，长期演进）

- [x] Task 8: CMakeLists.txt 拆分（C2）
  - [x] SubTask 8.1: 新建 `CMake/deps.cmake`（find_package 集中）+ `CMake/options.cmake`（option 声明）+ `CMake/flags.cmake`（编译选项）
  - [x] SubTask 8.2: 新建 `CMake/core_sources.cmake`、`CMake/gui_sources.cmake`（set PXVIEW_CORE/GUI_SOURCES）
  - [x] SubTask 8.3: 新建 `CMake/decoders.cmake`（C_DECODERS）+ `CMake/libsigrok.cmake`（libsigrok/libsigrokdecode/common 构建规则）
  - [x] SubTask 8.4: 新建 `CMake/install_packaging.cmake`（install + CPack）
  - [x] SubTask 8.5: 主 CMakeLists.txt 用 `include()` 组装，行数 134（< 150 目标）
  - [x] SubTask 8.6: 验证：全量编译 1245 步 0 error + install 成功

- [x] Task 9: MainWindow::OnMessage 拆分（C5，Task 7 前置）
  - [x] SubTask 9.1: 按职责分组 39 个 case：设备切换(5)/采集状态(7)/设备选项(7)/UI选项(4)/数据更新(7)/滤波反相(7)/触发(2)
  - [x] SubTask 9.2: 每组抽取为独立处理器方法（on_device_changed/on_capture_state/on_device_options/on_ui_options/on_data_updated/on_filter_completed/on_trigger_changed）
  - [x] SubTask 9.3: OnMessage 退化为 79 行路由 switch（< 80 目标），保留线程 marshal 前置逻辑
  - [x] SubTask 9.4: 验证：增量编译 778 步 0 error（GUI 回归待运行时）；on_data_updated(int,int) 与 Qt slot on_data_updated() 重载用 QOverload<>::of 消歧

- [ ] Task 10: SigSession 拆分（C1，高风险大重构）— **DEFERRED: 长期演进，建议分批执行**
  - [ ] SubTask 10.1: 抽取 EventBus（_callbacks/_msg_listeners/_event_listeners/_broadcast_depth + broadcast_msg/broadcast<T>/add/remove_listener）
  - [ ] SubTask 10.2: 抽取 DocumentRegistry（_all_documents/_active_document/_capture_owner_document/_capture_owner_guard + register/unregister/set_active_document/clear_capture_owner_document）
  - [ ] SubTask 10.3: 抽取 DecodeTaskManager（_decode_threads/_running_tasks + add_decode_task/clear_all_decode_task/start_all_decode_tasks/rst_decoder）
  - [ ] SubTask 10.4: 抽取 DataFeedParser（feed_in_header/logic/dso/analog/trigger）
  - [ ] SubTask 10.5: 抽取 FilterProcessor（glitch_filter_thread/signal_invert_thread + 相关方法）
  - [ ] SubTask 10.6: 抽取 CaptureManager（采集生命周期 + 6 套 DsTimer + exec_capture/start/stop_capture）
  - [ ] SubTask 10.7: SigSession 退化为 facade，持有上述 manager 的 unique_ptr
  - [ ] SubTask 10.8: 验证：增量编译 0 error；GUI + Headless 回归
  - **延后原因**: SigSession 是数据流的心脏（2715 行 .cpp + 662 行 .h = 3377 行），OnMessage 翻译表需要访问 SigSession 内部状态（is_working/_device_status/_trigger_config 等），EventBus 无法完全自包含。6 个 manager 的提取涉及大量调用方更新，单次会话完成风险极高。spec 本身标注"长期演进，可分批执行"。

- [ ] Task 11: SessionDocument 拆分（C3，依赖 Task 10）— **DEFERRED: 依赖 Task 10**
  - [ ] SubTask 11.1: 抽取 SignalConfigStore（ChannelConfig/SignalConfig + save/apply_signal_config/apply_pending_config），移除 DeviceAgent* 参数（改由 SigSession 协调）
  - [ ] SubTask 11.2: SessionDocument 保留纯数据（_logic/_analog/_dso/_decoder_stacks/_spectrum_stacks/_math_stack/_lissajous_model/_signal_models/_trigger_config）
  - [ ] SubTask 11.3: UI 布局字段（view_index/v_offset/own_height）下沉 View 层 DockUiState
  - [ ] SubTask 11.4: 移除 `friend class TabContext`
  - [ ] SubTask 11.5: 验证：序列化/反序列化 .pxc 正常；Tab 切换 UI 布局保留
  - **延后原因**: 依赖 Task 10 完成后的新 SigSession facade 结构。SessionDocument 本身较小（354+147=501 行），但 friend TabContext 和 DeviceAgent 耦合的解耦需要 SigSession 先拆分。

## Task Dependencies
- Task 1/2/3 独立，可并行（阶段 1）
- Task 4/5 独立，可并行（阶段 2，依赖阶段 1 完成）
- Task 6 独立（阶段 3）；Task 7 依赖 Task 9（C5 拆分后才能迁移 IEventListener）
- Task 8 独立（阶段 4）
- Task 9 独立（阶段 4），是 Task 7 的前置
- Task 10 依赖阶段 1-3 完成（避免合并冲突）
- Task 11 依赖 Task 10（SessionDocument 拆分需 SigSession 拆分后的新结构）

## Parallelizable Work
- 阶段 1：Task 1/2/3 可并行
- 阶段 2：Task 4/5 可并行
- 阶段 4：Task 8/9 可并行；Task 10/11 顺序执行
