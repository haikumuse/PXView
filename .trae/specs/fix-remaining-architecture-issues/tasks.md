# Tasks

## 阶段 1（P0 紧急 bug 修复）— 已完成

- [x] Task 1: 修复 Math/Spectrum/Lissajous 波形不显示（A1 dirty 标志短路）
  - [x] SubTask 1.1: `View::signals_modified_refresh` 末尾调 `mark_derived_traces_dirty()` + `compute_signal_groups()`
  - [x] SubTask 1.2: 验证：增量编译 0 error

- [x] Task 2: 修复 Tab 关闭 View→Document UAF（A2 销毁时序）
  - [x] SubTask 2.1: `MainWindow::remove_tab` 逐 stack 调 `stop_decode_work()`
  - [x] SubTask 2.2: `View::set_data_document(nullptr)` 修复 early-return
  - [x] SubTask 2.3: 验证：增量编译 0 error

- [x] Task 3: 修复 glitch/invert 线程析构未 join（A3 后台线程生命周期）
  - [x] SubTask 3.1: `SigSession::Close()` join glitch_filter_thread/signal_invert_thread
  - [x] SubTask 3.2: `~SigSession()` 兜底调 `Close()`
  - [x] SubTask 3.3: 5 个跨线程标志全改 `std::atomic`
  - [x] SubTask 3.4: 验证：增量编译 0 error

## 阶段 2（P1 分层与并发加固）— 已完成

- [x] Task 4: 移除 View 层 ds_trigger_* 直调（B2 分层泄漏）
  - [x] SubTask 4.1: `LogicSignal::commit_trig` 移除全部 8 处 `ds_trigger_*`
  - [x] SubTask 4.2: `view.cpp` `set_trig_cursor_posistion` 改查 Core
  - [x] SubTask 4.3: 验证：增量编译 0 error

- [x] Task 5: 跨线程标志 atomic 化（C4 UB 修复）
  - [x] SubTask 5.1-5.5: atomic + `_capture_state_mutex` 组合状态保护
  - [x] SubTask 5.6: 验证：增量编译 0 error

## 阶段 3（P2 半成品收口）— 已完成

- [x] Task 6: 更新 AGENTS.md 事件总线措辞（B1.1）
  - [x] SubTask 6.1-6.3: AGENTS.md + project_memory.md + events.h STATUS 注释

- [x] Task 7: 事件总线真正迁移（B1.2）
  - [x] SubTask 7.1-7.5: 41 个事件结构体 + 38/43 翻译表 + MainWindow IEventListener 注册
  - [x] 附带修复：Windows SDK `interface` 宏冲突；kingst-la2016 符号重名

## 阶段 4（P3 结构性重构）

- [x] Task 8: CMakeLists.txt 拆分（C2）— 已完成
  - [x] SubTask 8.1-8.6: 8 个 `CMake/*.cmake` 子模块 + 主文件 134 行

- [x] Task 9: MainWindow::OnMessage 拆分（C5）— 已完成
  - [x] SubTask 9.1-9.4: 7 个处理器方法 + OnMessage 79 行路由

### 本次启动任务（解除延期）

- [x] Task 10: SigSession 拆分 + 单通道同步统一（C1 + C1+）
  **执行策略**：按风险分 3 批，每批独立可编译可验证。EventBus 优先（含 C1+ 同步统一），其余 manager 次之。

  - [x] SubTask 10.1: 抽取 EventBus + **broadcast_msg 全异步化（C1+ 核心）**
    - 新建 `PXView/pv/core/eventbus.h/.cpp`，迁移 `_callbacks`/`_msg_listeners`/`_event_listeners`/`_broadcast_depth`
    - **C1+ 关键改动**：`broadcast_msg(int)` 内部改为 `QMetaObject::invokeMethod(this, ..., Qt::QueuedConnection)` 异步派发
    - 删除 `broadcast_msg_deferred` / `trigger_message_deferred`（统一为异步，不再需要双轨制）
    - `broadcast<T>()` 保持同步语义（类型化事件在异步队列处理时调用，不会再嵌套）
    - 保留 `_broadcast_depth` 循环护栏作为兜底
    - `SignalsChangedEvent` 结构体新增 `rebuild_kind` 枚举 + `new_model_ptrs` 字段
    - 验证：增量编译 0 error；`grep broadcast_msg_deferred` 0 命中

  - [x] SubTask 10.2: 抽取 DocumentRegistry
    - 新建 `PXView/pv/core/documentregistry.h/.cpp`，迁移 `_all_documents`/`_active_document`/`_capture_owner_document`/`_capture_owner_guard`
    - `CaptureOwnerGuard` 内嵌类迁移至 DocumentRegistry
    - `register`/`unregister`/`set_active_document`/`clear_capture_owner_document` 方法迁移
    - SigSession 持有 `unique_ptr<DocumentRegistry>`，转发调用
    - 验证：增量编译 0 error

  - [x] SubTask 10.3: 抽取 DecodeTaskManager
    - 新建 `PXView/pv/core/decodetaskmanager.h/.cpp`，迁移 `_decode_threads`/`_running_tasks`
    - `add_decode_task`（私有）/`clear_all_decode_task`/`start_all_decode_tasks`/`rst_decoder` 方法迁移
    - `attach_data_to_signal` 逻辑迁移
    - 验证：增量编译 0 error

  - [x] SubTask 10.4: 抽取 DataFeedParser
    - 新建 `PXView/pv/core/datafeedparser.h/.cpp`，迁移 `feed_in_header`/`feed_in_logic`/`feed_in_dso`/`feed_in_analog`/`feed_in_trigger`
    - `data_feed_callback_ex` 静态 trampoline + `data_feed_callback` 实例方法迁移
    - 验证：增量编译 0 error

  - [x] SubTask 10.5: 抽取 FilterProcessor
    - 新建 `PXView/pv/core/filterprocessor.h/.cpp`，迁移 `glitch_filter_thread`/`signal_invert_thread` + 相关方法
    - `start_glitch_filter`/`stop_glitch_filter`/`start_signal_invert`/`stop_signal_invert` 方法迁移
    - 验证：增量编译 0 error

  - [x] SubTask 10.6: 抽取 CaptureManager
    - 新建 `PXView/pv/core/capturemanager.h/.cpp`，迁移采集生命周期 + 6 套 DsTimer
    - `exec_capture`/`start_capture`/`stop_capture`/`capture_init` 方法迁移
    - 验证：增量编译 0 error

  - [x] SubTask 10.7: SigSession 退化为 facade
    - SigSession 持有 6 个 manager 的 `unique_ptr`
    - 原有 public 方法退化为转发（保持 API 兼容）
    - `sigsession.h` 行数从 545 行降至 284 行（< 300 目标）
    - 验证：增量编译 0 error；GUI + Headless 回归待运行时验证

- [x] Task 11: SessionDocument 拆分（C3，依赖 Task 10）
  - [x] SubTask 11.1: 抽取 SignalConfigStore
    - 新建 `PXView/pv/data/signalconfigstore.h/.cpp`
    - 迁移 `ChannelConfig`/`SignalConfig` 结构体 + `save_signal_config`/`apply_signal_config`/`apply_pending_config`
    - 移除 `DeviceAgent*` 参数（改由 SigSession 注入，SignalConfigStore 持有 `SigSession*`）
  - [x] SubTask 11.2: SessionDocument 保留纯数据
    - 仅保留 `_logic`/`_analog`/`_dso`/`_decoder_stacks`/`_spectrum_stacks`/`_math_stack`/`_lissajous_model`/`_signal_models`/`_trigger_config`
  - [ ] ~~SubTask 11.3: UI 布局字段下沉 View 层 DockUiState~~ — **延后**
    - `view_index`/`v_offset`/`own_height` 保留在 `ChannelConfig`（.pxc 序列化需要）
    - `ChannelLayoutState` 保留在 `pv::data` 命名空间（移至 `pv::view` 会造成 Core→View 依赖）
    - 完整迁移需扩展 .pxc 格式，风险较高，留待后续 session
  - [x] SubTask 11.4: 移除 `friend class TabContext`
    - TabContext 改用 public 接口访问（`get_channels()`/`set_pending_config()`）
  - [x] SubTask 11.5: 验证：.pxc 序列化格式不变；增量编译 0 error

## Task Dependencies
- Task 1-9 已完成
- Task 10 内部子任务顺序执行（10.1 → 10.2 → ... → 10.7）
- Task 10.1（EventBus + C1+）是本次最高优先级，独立可交付
- Task 11 依赖 Task 10.7（SigSession facade 完成后才能拆 SessionDocument）

## Parallelizable Work
- Task 10 各子任务严格顺序（每步依赖前一步的编译验证）
- Task 11 必须在 Task 10 完成后启动

## 风险控制
- **Task 10.1（C1+ 异步化）**：最高风险点。异步化后所有"广播后立即查状态"的调用方会行为变化。需逐个审查 `broadcast_msg` 的 43 个发射点。建议先跑一次 GUI 回归测试再继续。
- **Task 10.2-10.6**：机械提取，风险中等。每步独立编译验证，失败可回退。
- **Task 10.7**：API 转发层，风险低。纯机械工作。
- **Task 11**：中等风险。`.pxc` 文件兼容性需测试（UI 布局字段下沉后，旧 .pxc 文件的 view_index/v_offset/own_height 需迁移读取）。
