# 修复 broadcast_msg 同步/异步竞态 Spec

## Why

`switch_work_mode` 切换 DSO 模式时崩溃（`SIGSEGV`，`this=0xfeeefeeefeeefeee`），根因是 Core→View 状态同步存在**同步/异步时序错配 + 增量更新未重绑 `_model`** 的复合问题：

1. `SigSession::switch_work_mode` → `init_signals()` 同步全量重建 Core `SignalModel`（旧 `shared_ptr` 释放，内存被 Windows debug heap 填 `0xfeeefeee`）。
2. `init_signals()` 末尾调 `signals_changed()`。该回调链在 GUI 线程内是同步的（`dispatch_to` → `MainWindow::signals_changed()` → Qt 信号 AutoConnection=DirectConnection → `View::on_signals_changed()`），但 `View::on_signals_changed()` 走 `compute_change_event` **增量更新**——当新 SignalModel 与旧的 index/type 相同时，可能被判为 `Modified`，**保留旧 `view::Signal` 对象及其悬垂的 `_model` 裸指针**，而非 `AllReplaced` 全量重建。
3. `switch_work_mode` 紧接着**同步** `broadcast_msg(DSV_MSG_DEVICE_MODE_CHANGED)`，handler `on_device_options` → `load_device_config` → `load_config_from_json` → `DsoSignal::set_zero_ratio` → `_model->set_zero_offset(this=0xfeeefeeefeeefeee)` → **UAF 崩溃**。

现有补丁（`mainwindow.cpp:3254` 在 handler 顶部手动 `current_view()->rebuild_signals()` 全量重建）只是单点治标：只修了 `DEVICE_MODE_CHANGED` 一个 handler，其它 `broadcast_msg` 调用点同样存在竞态风险，且根因（同步广播抢在 View 完整重建前 + 增量更新可能漏绑）未消除。这正是项目反复踩坑的"手动广播网"技术债的又一次发作。

本 spec 从机制层根治：让紧随 Core 状态重建的 `broadcast_msg` **延迟到下一事件循环**（保证 View 同步重建已完成），并确保 Core 全量重建 SignalModel 时 View 一定走 `AllReplaced` 全量重绑，从而彻底消除该竞态窗口。

## What Changes

- **P0 延迟状态重建类广播**：`SigSession` 新增 `broadcast_msg_deferred(int msg, int param)`，通过 `QMetaObject::invokeMethod(this, [...]{ broadcast_msg(msg, param); }, Qt::QueuedConnection)` 把广播投递到下一事件循环。所有紧随 Core SignalModel 全量重建（`init_signals`/`reload`/`switch_work_mode`）的 `broadcast_msg` 改用延迟版本，保证 handler 在 View 同步重建完成之后执行。
- **P0 全量重建时 View 强制 AllReplaced**：当 Core 全量重建 SignalModel（`init_signals`/`reload`，旧 `shared_ptr` 全部销毁、新对象指针全变）时，View SHALL 走 `AllReplaced` 全量重建并重绑所有 `view::Signal::_model`，SHALL NOT 走 `Modified` 增量更新保留旧 Signal 对象。实现方式（二选一，由实施者定）：
  - (a) 修复 `SignalFactory::compute_change_event`：以 SignalModel 指针身份（而非仅 index/type）判断是否全量替换，旧指针不在新列表中即判 `AllReplaced`；或
  - (b) 新增 `ISessionStateCallback::signals_replaced()`（全量重建语义，区别于增量 `signals_changed()`），`init_signals`/`reload` 调 `signals_replaced()`，`View` 收到后直接 `rebuild_signals()` 全量重建。
- **P1 移除单点补丁**：删除 `mainwindow.cpp:3254` 的手动 `current_view()->rebuild_signals()` 及其上方 3244–3253 的注释（已由 P0 两项根治，该补丁冗余）。
- **P1 审计其余 broadcast_msg 调用点**：审计 `sigsession.cpp` 全部 `broadcast_msg`/`trigger_message` 调用，凡紧随 Core SignalModel 重建或 `_signal_models` 变更的，改用 `broadcast_msg_deferred`；纯属性变更（不涉及 SignalModel 对象生命周期）的保持同步。
- **P2 回归测试**：新增压力测试，循环切换工作模式（LOGIC↔DSO↔ANALOG）N 次，验证无 UAF/崩溃；可复用 headless MCP 路径或单元测试框架。

## Impact

- Affected specs:
  - `fix-state-sync-gaps-v2`（前置，已完成）—— 该 spec 修的是跨线程 marshal（非 GUI→GUI），本 spec 修的是 GUI 线程内同步广播与 View 异步/增量重建的竞态，二者正交
  - `harden-remaining-crash-risks`（进行中）—— 该 spec 的 `rebuild_signals_from_config` 重入护栏与本 spec 的全量重建语义互补，不冲突
- Affected code:
  - `PXView/pv/sigsession.h/.cpp` —— 新增 `broadcast_msg_deferred`，`switch_work_mode`/`init_signals`/`reload` 改用延迟广播；若选方案(b) 新增 `signals_replaced()` 回调
  - `PXView/pv/interface/icallbacks.h` —— 若选方案(b) 在 `ISessionStateCallback` 加 `signals_replaced()` 虚函数
  - `PXView/pv/view/signalfactory.h/.cpp` —— 若选方案(a) 修 `compute_change_event` 指针身份判定
  - `PXView/pv/view/view.cpp` —— `on_signals_changed` 全量重建分支；若选方案(b) 新增 `on_signals_replaced` 处理
  - `PXView/pv/mainwindow.cpp` —— 删除 3254 行补丁及 3244–3253 注释；若选方案(b) 增加 `signals_replaced` 转发
  - `PXView/pv/api/session_service.cpp` —— 若选方案(b) 同步增加 `signals_replaced` 实现
  - 测试目录 —— 新增模式切换压力测试

---

## ADDED Requirements

### Requirement: 状态重建类广播延迟投递
紧随 Core SignalModel 全量重建（`init_signals`/`reload`/`switch_work_mode`）的 `broadcast_msg` SHALL 通过 `broadcast_msg_deferred` 以 `Qt::QueuedConnection` 投递到下一事件循环，SHALL NOT 同步直达 handler。这保证 handler 在 View 同步重建（`signals_changed` → `on_signals_changed`）完成之后执行。

#### Scenario: 切换 DSO 模式不再 UAF
- **WHEN** 用户点击 DevMode 切换到 DSO 模式
- **THEN** `switch_work_mode` 调 `init_signals()` 全量重建 SignalModel
- **AND** `init_signals` 末尾 `signals_changed()` 同步触发 View 全量重绑 `_model`（见下一 Requirement）
- **AND** `switch_work_mode` 通过 `broadcast_msg_deferred(DSV_MSG_DEVICE_MODE_CHANGED)` 把广播投递到下一事件循环
- **AND** `on_device_options` handler 在 View 重建完成后执行，`DsoSignal::_model` 指向有效新 SignalModel
- **AND** `set_zero_ratio` → `set_zero_offset` 不解引用 `0xfeeefeeefeeefeee`，无崩溃

#### Scenario: 纯属性变更广播保持同步
- **WHEN** 某操作仅修改 SignalModel 属性（vdiv/coupling/enabled），不重建 SignalModel 对象
- **THEN** `broadcast_msg` 保持同步直达（响应即时）
- **AND** 不误用延迟版本引入不必要的事件循环延迟

### Requirement: Core 全量重建触发 View AllReplaced
当 Core 全量重建 SignalModel（`init_signals`/`reload`，旧 `shared_ptr` 全部销毁）时，View SHALL 走 `AllReplaced` 全量重建，重新创建/重绑所有 `view::Signal::_model`，SHALL NOT 走 `Modified` 增量更新保留指向已释放 SignalModel 的旧 Signal 对象。

#### Scenario: init_signals 后 View 无悬垂 _model
- **GIVEN** `_own_signals` 中存在 DsoSignal，其 `_model` 指向旧 SignalModel
- **WHEN** `init_signals()` 销毁旧 SignalModel 并创建新 SignalModel（同 index/type 但不同指针）
- **THEN** View 重建判定为 `AllReplaced`（通过指针身份判定或显式 `signals_replaced()` 回调）
- **AND** 重建后所有 `view::Signal::_model` 指向新 SignalModel
- **AND** 无任何 `_model` 指向已释放内存

### Requirement: 模式切换回归测试
系统 SHALL 提供压力测试，循环切换工作模式多次，验证无 UAF/崩溃。

#### Scenario: 连续模式切换不崩溃
- **WHEN** 测试循环切换 LOGIC→DSO→ANALOG→LOGIC 共 N 次（N≥20）
- **THEN** 每次切换后 View 信号有效，无 SIGSEGV
- **AND** 无 ASan/UBSan 报告 use-after-free

---

## MODIFIED Requirements

### Requirement: broadcast_msg 调用点语义分类
`sigsession.cpp` 中所有 `broadcast_msg`/`trigger_message` 调用 SHALL 按语义分类：紧随 SignalModel 全量重建的改用 `broadcast_msg_deferred`；纯属性变更的保持同步。分类结果 SHALL 在代码注释或 PR 描述中记录。

#### Scenario: 审计后无遗留竞态调用点
- **WHEN** 审计完成
- **THEN** 每个紧随 `init_signals`/`reload` 的 `broadcast_msg` 均改为 `broadcast_msg_deferred`
- **AND** 纯属性变更广播保持同步并注释说明

---

## REMOVED Requirements

### Requirement: DEVICE_MODE_CHANGED handler 手动 rebuild_signals 补丁
**Reason**: `mainwindow.cpp:3254` 的 `current_view()->rebuild_signals()` 是针对该崩溃的单点补丁，现已由延迟广播 + 全量重建语义根治，保留会造成双重重建（View 已全量重建一次，handler 再重建一次）的冗余开销。
**Migration**: 删除 3254 行 `rebuild_signals()` 调用及 3244–3253 注释；`on_device_options` handler 后续逻辑（`mode_changed`/`load_device_config` 等）依赖 View 已完成的全量重建，无需再手动触发。
