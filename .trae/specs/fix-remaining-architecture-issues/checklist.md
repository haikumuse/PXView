# Checklist

## A. 阶段 1（P0 紧急 bug）— 已完成
- [x] `View::signals_modified_refresh` 末尾调 `mark_derived_traces_dirty()` + `compute_signal_groups()`
- [x] `MainWindow::remove_tab` 逐 stack 调 `stop_decode_work()` 停 decoder 线程
- [x] `View::set_data_document(nullptr)` 修复 early-return bug
- [x] `MainWindow::remove_tab` deleteLater 前调 `view->set_data_document(nullptr)`
- [x] `SigSession::Close()` join glitch_filter_thread + signal_invert_thread
- [x] `~SigSession()` 兜底调 `Close()`（idempotent）
- [x] 5 个跨线程标志全改 `std::atomic`
- [ ] 验证：Math/Spectrum/Lissajous 对话框确认后波形立即显示（待运行时回归）
- [ ] 验证：关闭正在解码的 Tab 无崩溃（待运行时回归）
- [ ] 验证：app 关闭时毛刺任务在跑无 UAF（待运行时回归）
- [ ] 验证：删除解码器后 Header 绘制无 UAF（待运行时回归）

## B. 阶段 2（P1 分层与并发）— 已完成
- [x] `LogicSignal::commit_trig` 无 `ds_trigger_*` 调用
- [x] `view.cpp` `set_trig_cursor_posistion` 改查 Core
- [x] 5 个跨线程标志全 `std::atomic`
- [x] CaptureOwnerGuard 用锁统一更新三态
- [x] `clear_capture_owner_document` 用锁做原子快照
- [x] copy_to_doc_done 路径用锁保护组合更新
- [x] `join_copy_thread`/`broadcast_msg` 均在锁外（防死锁）
- [x] 验证：增量编译 0 error；Grep sigsession.h 无残留 volatile
- [ ] 验证：用户点 LogicSignal 触发 → 采集 → 触发正常工作（待运行时回归）

## C. 阶段 3（P2 半成品收口）— 已完成
- [x] AGENTS.md "Typed event bus" 条目措辞修正
- [x] project_memory.md 移除"必须用 IEventListener"硬约束
- [x] events.h 顶部 STATUS 注释
- [x] events.h 补全 41 个事件结构体
- [x] OnMessage 翻译表 38/43 覆盖
- [x] MainWindow 继承 IEventListener，override on_event(CaptureStateChanged)
- [x] 4 个死代码事件处理
- [x] Windows SDK `interface` 宏冲突修复
- [x] kingst-la2016 feed_queue_logic 符号重名修复
- [x] 验证：增量编译 0 error；翻译表覆盖率 38/43

## D. 阶段 4（P3 结构性重构）

### CMake 拆分（Task 8）— 已完成
- [x] 8 个 `CMake/*.cmake` 子模块已创建
- [x] 主 CMakeLists.txt 134 行（< 150 目标）
- [x] 验证：全量编译 0 error + install 成功

### MainWindow::OnMessage 拆分（Task 9）— 已完成
- [x] 39 个 case 按职责分组
- [x] 7 个独立处理器方法已创建
- [x] OnMessage 79 行路由 switch（< 80 目标）
- [x] on_data_updated(int,int) 用 QOverload 消歧
- [x] 验证：增量编译 0 error

### SigSession 拆分 + 单通道同步统一（Task 10）— 本次启动

#### SubTask 10.1: EventBus + C1+ 异步化
- [x] 新建 `PXView/pv/core/eventbus.h/.cpp`
- [x] 迁移 `_callbacks`/`_msg_listeners`/`_event_listeners`/`_broadcast_depth`
- [x] **`broadcast_msg(int)` 改为 `Qt::QueuedConnection` 异步派发**
- [x] 删除 `broadcast_msg_deferred` / `trigger_message_deferred`
- [x] `broadcast<T>()` 保持同步语义
- [x] 保留 `_broadcast_depth` 循环护栏
- [x] `SignalsChangedEvent` 新增 `rebuild_kind` 枚举 + `new_model_ptrs` 字段
- [x] 验证：增量编译 0 error
- [x] 验证：`grep broadcast_msg_deferred` 仅余注释说明（eventbus.h 0 命中）
- [x] 验证：`grep trigger_message_deferred` 仅余注释说明（eventbus.h 0 命中）
- [x] 审查：43 个 `broadcast_msg` 发射点无"广播后立即查状态"依赖
- [x] 审查：`load_config_from_json` 的 `SuppressConfigBroadcastGuard` 是否可删除
- [x] 审查：`switch_work_mode`/`set_device` 的 deferred 调用是否已统一

#### SubTask 10.2: DocumentRegistry
- [x] 新建 `PXView/pv/core/documentregistry.h/.cpp`
- [x] 迁移 `_all_documents`/`_active_document`/`_capture_owner_document`/`_capture_owner_guard`
- [x] `CaptureOwnerGuard` 内嵌类迁移
- [x] SigSession 持有 `unique_ptr<DocumentRegistry>`
- [x] 验证：增量编译 0 error

#### SubTask 10.3: DecodeTaskManager
- [x] 新建 `PXView/pv/core/decodetaskmanager.h/.cpp`
- [x] 迁移 `_decode_threads`/`_running_tasks` + `add_decode_task`/`start_all_decode_tasks`/`rst_decoder`
- [x] `attach_data_to_signal` 逻辑迁移
- [x] 验证：增量编译 0 error

#### SubTask 10.4: DataFeedParser
- [x] 新建 `PXView/pv/core/datafeedparser.h/.cpp`
- [x] 迁移 `feed_in_header`/`feed_in_logic`/`feed_in_dso`/`feed_in_analog`/`feed_in_trigger`
- [x] `data_feed_callback_ex` 静态 trampoline 迁移
- [x] 验证：增量编译 0 error

#### SubTask 10.5: FilterProcessor
- [x] 新建 `PXView/pv/core/filterprocessor.h/.cpp`
- [x] 迁移 `glitch_filter_thread`/`signal_invert_thread` + start/stop 方法
- [x] 验证：增量编译 0 error

#### SubTask 10.6: CaptureManager
- [x] 新建 `PXView/pv/core/capturemanager.h/.cpp`
- [x] 迁移采集生命周期 + 6 套 DsTimer
- [x] `exec_capture`/`start_capture`/`stop_capture`/`capture_init` 迁移
- [x] 验证：增量编译 0 error

#### SubTask 10.7: SigSession facade
- [x] SigSession 持有 6 个 manager 的 `unique_ptr`
- [x] public 方法退化为转发
- [x] `sigsession.h` 行数 < 300（实际 284 行）
- [x] 验证：增量编译 0 error
- [ ] 验证：GUI 回归（启动/采集/停止/解码/切 tab/关 tab 无崩溃）— 待运行时回归
- [ ] 验证：Headless 回归（MCP 流程完整）— 待运行时回归

### SessionDocument 拆分（Task 11）— 依赖 Task 10
- [x] 新建 `PXView/pv/data/signalconfigstore.h/.cpp`
- [x] `ChannelConfig`/`SignalConfig` + save/apply 方法迁移
- [x] 移除 `DeviceAgent*` 参数（改由 SigSession 注入）
- [x] SessionDocument 仅保留纯数据字段
- [ ] ~~UI 布局字段（view_index/v_offset/own_height）下沉 `view::DockUiState`~~ — **延后**：.pxc 序列化需要，迁移需扩展格式
- [ ] ~~`ChannelLayoutState` 移至 `pv::view` 命名空间~~ — **延后**：会造成 Core→View 依赖
- [x] 移除 `friend class TabContext`
- [x] 验证：序列化/反序列化 .pxc 格式不变
- [ ] 验证：Tab 切换 UI 布局保留 — 待运行时回归
- [ ] 验证：旧 .pxc 文件兼容性 — 待运行时回归

## E. 文档更新
- [x] project_memory.md Lessons Learned 9 条（阶段 1-3）
- [x] AGENTS.md 事件总线措辞修正
- [x] AGENTS.md 更新：C1+ 完成后恢复"必须用 IEventListener"硬约束
- [x] AGENTS.md 更新：SigSession 拆分后结构 + broadcast_msg 异步化说明（Key Files 表 + State Sync Conventions）
- [x] AGENTS.md 更新：SessionDocument 拆分后结构（Key Files 表含 signalconfigstore.h）
- [x] project_memory.md 新增 Lessons Learned（C1+ 异步化、C1 拆分、C3 拆分）

## F. 最终验证
- [x] 阶段 1-3 完成后增量编译 0 error
- [x] 阶段 4 Task 8/9 完成后全量构建成功
- [x] Task 10.1 完成后：grep 无 deferred 残留 + 增量编译 0 error
- [x] Task 10.7 完成后：sigsession.h < 300 行（284 行）+ 增量编译 0 error
- [x] Task 11 完成后：.pxc 序列化格式不变 + 增量编译 0 error
- [ ] GUI + Headless 运行时回归（待用户验证）
- [x] project_memory.md 与 AGENTS.md 已更新
