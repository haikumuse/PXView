# fix-remaining-architecture-issues Spec

## Why

`fix-all-architecture-issues` 完成后，三路并行调研发现仍存在 10 项架构问题，分三层：
1. **3 项真实运行时 bug（P0）**：Math/Spectrum/Lissajous 波形不显示（dirty 标志短路）、Tab 关闭 View→Document UAF、glitch/invert 线程析构未 join
2. **2 项上一轮 spec 半成品（P1-P2）**：类型化事件总线是死代码（0 消费者、0 直接发射点）、View 层仍直调 `ds_trigger_*`（违反 AGENTS.md 明文约定）
3. **5 项结构性技术债（P2-P3）**：跨线程标志全为 `volatile bool` UB、SigSession 上帝类（3219 行/49 成员）、CMakeLists.txt 1889 行单文件、SessionDocument 6 角色混淆、MainWindow::OnMessage 504 行上帝方法

**二轮崩溃复盘追加（2026-07-02）**：DSO 模式 `set_zero_ratio` UAF + LOGIC 模式 `Header::paintEvent` UAF，根因均为 Core→View 状态同步的"同步/异步混合模式"——`init_signals` 同步重建 + `signals_changed` 同步回调 + `broadcast_msg` 同步派发 + `broadcast_msg_deferred` 异步派发四者混用，导致嵌套广播重入时对象被中途删除。补丁层（`SuppressConfigBroadcastGuard` + `compute_signal_groups` 重建）已堵住两个具体洞，但根治需统一为全异步单一通道。

本 spec 收口全部，按优先级分 4 阶段实施。阶段 1-3 已完成，阶段 4 解除延期，纳入单通道同步统一。

## What Changes

### 阶段 1（P0 紧急 bug 修复）— 已完成
- **A1**: `View::signals_modified_refresh` 末尾调 `mark_derived_traces_dirty()` + `compute_signal_groups()`
- **A2**: `MainWindow::remove_tab` 销毁顺序修复
- **A3**: `SigSession::Close()` + `~SigSession()` join 线程 + atomic 标志

### 阶段 2（P1 分层与并发加固）— 已完成
- **B2**: `LogicSignal::commit_trig` 移除 `ds_trigger_*` 直调
- **C4**: volatile→atomic + `_capture_state_mutex` 组合状态保护

### 阶段 3（P2 半成品收口）— 已完成
- **B1.1**: AGENTS.md 事件总线措辞修正
- **B1.2**: 事件结构体补全 + MainWindow IEventListener 注册

### 阶段 4（P3 结构性重构）— 已完成
- **C5**: `MainWindow::OnMessage` 拆分 — 已完成
- **C2**: `CMakeLists.txt` 拆分 — 已完成（主文件 134 行，8 个子模块）
- **C1**: `SigSession` 拆分为 CaptureManager/DecodeTaskManager/DataFeedParser/DocumentRegistry/EventBus/FilterProcessor，SigSession 退化为 facade — 已完成（sigsession.h 284 行）
- **C1+**: **Core→View 状态同步单通道统一** — 已完成。EventBus 提取时将 `broadcast_msg` 全部改为 `Qt::QueuedConnection` 异步派发，消除同步/异步混合导致的重入 UAF
- **C3**: `SessionDocument` 拆分为 SessionDocument（纯数据）+ SignalConfigStore（序列化），移除 DeviceAgent 耦合，移除 `friend class TabContext` — 已完成。UI 布局字段下沉 View 层 DockUiState **延后**（.pxc 序列化需要，迁移需扩展格式）

## Impact
- **Affected specs**: `fix-all-architecture-issues`（B1 揭示 Task 3 是死代码，需修正其 spec 描述）、`decouple-core-from-view-v2`（C1/C3 延续 Core/View 分离）
- **Affected code**:
  - `PXView/pv/view/view.cpp`（A1、B2 — 已完成）
  - `PXView/pv/mainwindow.cpp`（A2、C5 — 已完成）
  - `PXView/pv/sigsession.h/.cpp`（A3、C4 — 已完成；C1+C1+ — 本次）
  - `PXView/pv/view/logicsignal.cpp`（B2 — 已完成）
  - `PXView/pv/data/sessiondocument.h/.cpp`（C3 — 本次）
  - `CMakeLists.txt` + `CMake/*.cmake`（C2 — 已完成）
  - `PXView/pv/interface/events.h`、`icallbacks.h`（B1.2 — 已完成；C1+ — 本次扩展）
  - `AGENTS.md`、`project_memory.md`（B1.1 — 已完成；C1+/C3 完成后更新）

## ADDED Requirements

### Requirement: 派生 Trace 懒同步触发
The system SHALL ensure `View::signals_modified_refresh` marks derived traces dirty AND rebuilds `_signal_groups`, so Math/Spectrum/Lissajous traces are created immediately and Header::paintEvent never reads stale Trace pointers.

#### Scenario: Math 重建后波形立即显示
- **WHEN** 用户在 MathOptions 对话框确认启用 Math
- **THEN** MathTrace 立即创建并显示，无需切换 Tab

#### Scenario: 删除解码器后 Header 绘制无 UAF
- **WHEN** 用户删除一个 DecodeTrace 后 Header::paintEvent 触发
- **THEN** `_signal_groups` 已在 `signals_modified_refresh` 或 `sync_derived_traces` 中重建，无悬垂指针

### Requirement: Tab 关闭无悬垂指针
The system SHALL ensure Tab removal joins decoder threads, detaches View→Document pointer, and deletes document before View receives any paint event.

#### Scenario: 关闭正在解码的 Tab
- **WHEN** 用户关闭持有捕获 owner 的 Tab
- **THEN** copy 线程 + decoder 线程均 join，View 解绑 document 指针，document 安全销毁，无 UAF

### Requirement: 后台线程生命周期管理
The system SHALL join all background threads (decode/copy/glitch_filter/signal_invert) in `SigSession::Close()` and destructor, and use `std::atomic` for cross-thread running flags.

#### Scenario: 应用关闭时毛刺滤波任务仍在运行
- **WHEN** app 关闭且 glitch_filter_task 仍在跑
- **THEN** Close() join 该线程后才销毁 _view_data，无 UAF

### Requirement: View 层不直调 libsigrok trigger API
The system SHALL route all trigger state changes through Core `SignalModel::set_trig_type()` / `set_trigger_config()`, with `ds_trigger_*` called only by `sync_trigger_to_libsigrok()`.

#### Scenario: 用户点 LogicSignal 触发边沿
- **WHEN** 用户在 LogicSignal 上设置边沿触发
- **THEN** 仅 SignalModel::set_trig_type 被调用，ds_trigger_probe_set 不被 View 层直接调用

### Requirement: 跨线程标志原子性
The system SHALL use `std::atomic` for all cross-thread state flags (_is_working/_copy_in_progress/_device_status/_glitch_filter_running/_signal_invert_running).

#### Scenario: 并发读取 is_working
- **WHEN** GUI 线程读取 is_working() 而 worker 线程写入
- **THEN** 无数据竞争 UB，弱内存架构上可见性正确

### Requirement: Core→View 状态同步单一异步通道
The system SHALL dispatch all Core→View state-change notifications through a single asynchronous channel. Synchronous `broadcast_msg` and asynchronous `broadcast_msg_deferred` MUST NOT coexist for the same notification path. All notifications MUST be queued via `Qt::QueuedConnection` and processed in FIFO order on the GUI thread, eliminating re-entrant broadcast UAF.

**Rationale**: The DSO `set_zero_ratio` UAF and LOGIC `Header::paintEvent` UAF both stem from synchronous broadcasts triggering nested `reload()` → View AllReplaced → deleting `this` mid-method. A single async channel ensures the caller's stack frame completes before any consumer mutates state.

#### Scenario: set_config_* 不再触发同步嵌套广播
- **WHEN** `DsoSignal::set_zero_ratio` 调用 `set_config_uint16(SR_CONF_PROBE_OFFSET)`
- **THEN** `DeviceConfigChanged` 广播被异步排队，`set_zero_ratio` 方法栈完整结束后消费者才处理

#### Scenario: init_signals 重建不删除正在执行的调用方
- **WHEN** `load_config_from_json` 触发 `reload()` → `signals_changed()` → View AllReplaced
- **THEN** AllReplaced 重建被异步排队，`load_config_from_json` 方法栈完整结束后 View 才重建

#### Scenario: 类型化事件携带重建完成上下文
- **WHEN** Core 完成一次 SignalModel wholesale rebuild
- **THEN** `SignalsChangedEvent` 携带 `rebuild_kind` 枚举（AllReplaced/Modified/Added/Removed）+ `new_model_ptrs` 列表，消费者无需回查 session 状态

## MODIFIED Requirements

### Requirement: 类型化事件总线（fix-all-architecture-issues Task 3）
[原：18 事件结构体 + IEventListener + broadcast<T>，新代码强制用]
修改为：B1.2 已完成 41 个事件结构体 + 38/43 翻译表 + MainWindow IEventListener 注册。C1+ 完成后，`broadcast_msg` 全异步化，`broadcast<T>` 成为唯一派发通道，恢复硬约束。

### Requirement: MainWindow::OnMessage
[原：504 行/39 case 上帝方法，直接操控 10+ widget]
修改为：已完成拆分，OnMessage 79 行路由 switch + 7 个处理器方法。

### Requirement: SigSession
[原：3219 行/49 成员/20+ 职责上帝类]
修改为：拆分为 CaptureManager/DecodeTaskManager/DataFeedParser/DocumentRegistry/EventBus/FilterProcessor，SigSession 退化为协调 facade，持有各 manager 的 unique_ptr。**EventBus 提取时必须将 broadcast_msg 改为 Qt::QueuedConnection 异步派发**（C1+ 要求）。

### Requirement: SessionDocument
[原：6 角色混淆 + DeviceAgent 耦合 + UI 布局字段 + friend TabContext]
修改为：纯数据模型，序列化下沉 SignalConfigStore，UI 布局字段下沉 View 层 DockUiState，移除 DeviceAgent 依赖与 friend 声明。

### Requirement: CMakeLists.txt
[原：1889 行单文件混合依赖/源清单/安装/打包/测试]
修改为：已完成拆分，主文件 134 行 + 8 个 `CMake/*.cmake` 子模块。spec 原 < 100 行目标放宽至 < 150 行（GPL 头 21 行 + 目标定义 50 行属必须留在主文件）。

## REMOVED Requirements

### Requirement: AGENTS.md "新代码必须用 IEventListener" 硬约束
**Reason**: 调研发现 0 消费者、0 直接发射点，硬约束误导开发者认为已生效。
**Migration**: B1.1 改为"推荐接口，待 C1+ 完成后恢复硬约束"。

### Requirement: broadcast_msg_deferred / trigger_message_deferred 双轨制
**Reason**: 同步 `broadcast_msg` 与异步 `broadcast_msg_deferred` 并存导致开发者需记忆每个消息该用哪个，混合使用导致 UAF。
**Migration**: C1+ 完成后，所有 `broadcast_msg` 统一为 `Qt::QueuedConnection` 异步，`*_deferred` 变体删除。
