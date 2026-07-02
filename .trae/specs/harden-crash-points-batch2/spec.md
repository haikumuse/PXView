# 加固剩余崩溃风险批次 2 Spec

## Why

`harden-remaining-crash-risks` 已修 8 类已知风险（assert(ptr) 守卫、DecoderStack 句柄、`effective_data_source` 移除、`sr_type()` 边界转换、`add_decode_task` 私有化、`RebuildGuard` 重入护栏、`CAPTURE_OWNER_CHANGED` 携带 `is_working`、`on_main_thread` 同步等待审计），代码改动已落地（仅链接阶段受阻于 `tiered-driver-compat-fix` 遗留）。`fix-broadcast-sync-async-race` 修了 `init_signals` 后广播延迟投递。`fix-mmap-async-crash-risks` 修了 mmap 异步写入链路。

最近一轮四维广度扫描（生命周期 / 线程安全 / Core-View 边界 / 容器迭代与重入）发现仍有 **12 类未覆盖的崩溃风险**：

1. **异步回调期 `current_view()->` 链式裸调 60+ 处**：`broadcast_msg` 已改全异步（`Qt::QueuedConnection`），ICaptureCallback / `on_signals_changed` 等回调到达时 Tab 可能已关闭，`current_view()` 返回 nullptr，但 60+ 处未做 null 检查。
2. **工作线程直碰 QWidget 3 处**：`calibration.cpp` / `protocoldock.cpp` / `protocolexp.cpp` 用 `QtConcurrent::run` 在工作线程直接调 `QSlider::setRange` / `QLineEdit::setText` / `QCheckBox::isChecked`，无视 Qt 跨线程访问 QWidget 的硬约束。
3. **触发器单一真相源运行时违规 4 处**：`SignalModel::commit_trig()` 在 Core 数据层直接调 `ds_trigger_probe_set`/`ds_trigger_set_en`，`TriggerDock::commit_trigger` 的 else 分支、`SessionService::set_logic_trigger_config`（MCP 路径）同样绕过 `SigSession::sync_trigger_to_libsigrok()` 单一入口。`get_logic_trigger_config` 从 `ds_trigger_get_*` 读而非 Core `TriggerConfig`。
4. **Broadcast 反向自删 2 处**：`DsoSignal::set_config_uint16` / `AnalogSignal::set_config_uint16` 同步触发 `config_changed` → `broadcast_msg(SAMPLE_COUNT_UPDATED)` → OnMessage → reload → View AllReplaced → `delete this`，仅靠 `auto model = _model;` 本地保活缓解，`this` 后续访问仍 UAF。
5. **Broadcast 语义注释错位 3 处**：`view.cpp:2468/2562/2618` 注释明确"broadcast 是同步直接调用"——与现状（全异步）矛盾，原依赖"先同步 reload、再 start_all_decode_tasks"的顺序假设已破坏。`tabcontext.cpp:87-105` reload → broadcast(DEVICE_OPTIONS_UPDATED) → rebuild 串联触发"二次 reload"，违反 memory 中"rebuild 路径不能广播 DEVICE_OPTIONS_UPDATED"约定。
6. **裸 `this` 跨线程 lambda 排队到 qApp 6 处**：`session_service.cpp` `QTimer::singleShot(0, qApp, [this, ...]{...})`、`ws_transport.cpp` `invokeMethod(qApp, [this, ...]{...})`、`winnativewidget.cpp`、`waitingdialog.cpp`、`calibration.cpp` `QtConcurrent::run([&]{ _device_agent->... })`、`mmap_allocator.cpp:253` fire-and-forget thread。对象在 qApp 派发 lambda 前析构 → UAF。
7. **GUI 线程同步 join 5 处**：`documentregistry.cpp` `CaptureOwnerGuard` 析构 `join_copy_thread()`、`sigsession.cpp:1587` `OnMessage(DSV_MSG_REV_END_PACKET)` 同步 `copy_thread().join()`、`session_service.cpp` `store.wait()`、`storeprogress.cpp` `~StoreProgress()` 调 `wait()`、`session_service.cpp:213` `invoke_or_call` 回退 `Qt::BlockingQueuedConnection`。轻则 UI 冻结，重则死锁。
8. **共享状态无锁访问 3 处**：`filterprocessor.cpp` glitch_filter_task / signal_invert_task 在 `std::thread` 内裸读 `_session->_view_data` / `_view_data->_logic_backup`，而 GUI 线程同时改写 `_view_data`、`clear_glitch_filter` 中 `delete + nullptr` `_logic_backup`。`restart_decoders()` 未检查 `is_copy_in_progress()` 同步调 `copy_data_to_document`。
9. **跨对象持 view 裸指针 / `sr_channel*` 裸指针 4 类**：`protocoldock.cpp:520/580` `layer->_trace = stack.get()`、`protocoldock.cpp:712` `decoder_model->setDecoderStack(... .get())`、`analogsignal.cpp`/`dsosignal.cpp` 函数局部持 `sr_channel*`、`storesession.cpp` `ChannelStateRestorer` RAII 遍历 GSList 保存 `sr_channel->enabled`，析构期访问。
10. **容器迭代中修改 / 析构链回连 4 处**：`eventbus.cpp:65-69` `dispatch_to` 同步遍历 `_callbacks`，listener 在 OnMessage 里同步 add/remove listener → vector 迭代器失效；`~View` 范围 for 中 delete 同一容器元素依赖析构链不回调；`eventbus.cpp:47-54` `broadcast_msg` lambda 捕获 `EventBus* this` 排队到 qApp，关停顺序中悬垂；`decodetaskmanager.cpp` 析构 join 期间访问 DecoderStack（unique_ptr 声明顺序敏感）。
11. **emit 默认 AutoConnection 4 处**：`searchdock.cpp:542/575`、`protocolexp.cpp:328`、`storesession.cpp` 多处、`mainwindow.cpp:732-768` `_event` connect。当前依赖 thread affinity 隐式 Queued，任何 `moveToThread` 改动 → 退化为 DirectConnection 在工作线程跑槽。
12. **解码任务静默失败 3 处**：`view.cpp:2476` `add_decoder` silent 路径漏调 `start_all_decode_tasks`、`session_service.cpp:2849` `add_analyzer` 嵌套 `QEventLoop::exec` 轮询、`storesession.cpp:820` `(int)m->type()` 与 `SR_CHANNEL_*` 混淆。

本 spec 收口这 12 类，与已存在的 `harden-remaining-crash-risks` / `fix-broadcast-sync-async-race` / `fix-mmap-async-crash-risks` 互补不重叠。

## What Changes

### 阶段 1（P0 必然崩溃 / 异步期批量闪退）

- **A1**: 抽 `MainWindow::safe_current_view()` 返回 `view::View*`（可能为 nullptr），把 `on_signals_changed` / `frame_began` / `receive_end` / `receive_data` / `receive_len` / `update_capture` 等 60+ 处裸调统一替换为 null 检查兜底
- **A2**: 修复 3 处工作线程直碰 QWidget——`calibration.cpp:312-315` / `protocoldock.cpp:1064` / `protocolexp.cpp:173-175`，参照 `deviceoptionsdock.cpp:553-582` 范式（worker 内通过 `QMetaObject::invokeMethod(this, [...], Qt::QueuedConnection)` marshal 回 GUI 线程）
- **A3**: 删除 `SignalModel::commit_trig()` 中 `ds_trigger_probe_set`/`ds_trigger_set_en` 调用，改为只更新 Core `_trig_type`；`TriggerDock::commit_trigger` else 分支清理；`SessionService::set_logic_trigger_config` 改写 Core `TriggerConfig`，由 `start_capture` 统一同步；`get_logic_trigger_config` 改读 Core `TriggerConfig` 不从 `ds_trigger_get_*` 读
- **A4**: Verify `harden-remaining-crash-risks` Task 1 对 14 处 `assert(false)` 后继续执行模式的覆盖率（与 harden Task 1 的 `assert(ptr)` 守卫模式互补），逐个 verify 是否已加显式 `if(!ptr) { pxv_err(...); return; }`，未覆盖的补齐

### 阶段 2（P1 异步期 UAF / 共享状态无锁）

- **B1**: `DsoSignal::set_config_uint16` / `AnalogSignal::set_config_uint16` 把 `broadcast_msg` 改为 `QMetaObject::invokeMethod(qApp, [...], Qt::QueuedConnection)` 异步投递，函数尾部不再访问任何 `this` 成员；消除同步 broadcast → reload → delete this 链
- **B2**: 修正 `view.cpp:2468/2562/2618` 过时注释（"broadcast 是同步直接调用"已与现状矛盾）；显式同步等待 reload 完成或改异步路径
- **B3**: `tabcontext.cpp:87-105` 删除 reload 后的 `broadcast_msg(DEVICE_OPTIONS_UPDATED)`，避免触发"二次 reload"
- **B4**: 6 处裸 `this` 跨线程 lambda 加 `QPointer<T>` 守卫或 `weak_ptr`，lambda 内 `if (!guard) return;`——`session_service.cpp:2384/2819`、`ws_transport.cpp:147`、`winnativewidget.cpp:330`、`waitingdialog.cpp:111-113/139-142`、`calibration.cpp:285-287`、`mmap_allocator.cpp:253`（fire-and-forget thread 需 join 或显式 detach 到受控对象）
- **B5**: `documentregistry.cpp` `CaptureOwnerGuard` 析构 `join_copy_thread()` 改为加超时 + 失败时 detach + 警告日志；`sigsession.cpp:1587` `OnMessage(DSV_MSG_REV_END_PACKET)` 同步 join 改异步（投递到 worker 线程或用 QtConcurrent）
- **B6**: `session_service.cpp` `store.wait()` 改后台 `QtConcurrent::run` + `QFutureWatcher`；`storeprogress.cpp` `~StoreProgress()` `wait()` 加超时 + 取消机制；`session_service.cpp:213` `invoke_or_call` 在持锁路径上加断言禁止跨线程 BlockingQueued
- **B7**: `filterprocessor.cpp` 加 `_view_data_mutex`，`glitch_filter_task` / `signal_invert_task` 读 `_view_data` 时持锁，`clear_glitch_filter` 写 `_logic_backup` 时同样持锁；`sigsession.cpp` `restart_decoders()` 加 `is_copy_in_progress()` 守卫，copy 进行中改异步队列
- **B8**: `protocoldock.cpp:520/580` `layer->_trace = stack.get()` 改 `std::weak_ptr<DecoderStack>` 或 QPointer 守卫；`protocoldock.cpp:712/716/717/723` `decoder_model->setDecoderStack(... .get())`、`decoderoptionsdlg.cpp:483` `new DecoderGroupBox(_trace->decoder().get(), ...)` 同样改弱引用
- **B9**: `analogsignal.cpp` / `dsosignal.cpp` 函数局部持 `sr_channel*` 改短期持 + 复检 `_model` 存活（取指针前后立即用，避免跨函数调用）；`storesession.cpp` `ChannelStateRestorer` RAII 加设备存活守卫（设备销毁时拒绝 dtor 访问 GSList）

### 阶段 3（P2 防御性加固 / 类型强制）

- **C1**: `eventbus.cpp:65-69` `dispatch_to` 改拷贝快照遍历（`auto snapshot = _callbacks; for (auto *cb : snapshot) ...`），listener 在 OnMessage 里同步 add/remove 不影响当前遍历
- **C2**: `view.cpp:312-322` `~View` 范围 for 中 delete 改为先 `disconnect` 所有 signal/slot，再 delete；保证析构链不回调到 View
- **C3**: `eventbus.cpp:47-54` `broadcast_msg` lambda 捕获 `EventBus* this` 改为 `weak_ptr` 或在 EventBus 析构时取消 qApp 队列中的 pending lambda
- **C4**: `decodetaskmanager.cpp` 析构 join 期间访问 DecoderStack——核查 `SigSession` unique_ptr 成员声明顺序，确保 DecodeTaskManager 析构在 DecoderStack 之前；或改用 shared_ptr 注入
- **C5**: 4 处 emit 默认 AutoConnection 改显式 `Qt::QueuedConnection`——`searchdock.cpp:326`、`protocolexp.cpp:188`、`storesession.cpp` 信号订阅、`mainwindow.cpp:732-768` `_event` connect

### 阶段 4（P3 静默失败 / 一致性）

- **D1**: `view.cpp:2476-2477` `add_decoder` silent 路径在 Core 层加 `assert + log` 兜底，silent=true 时记警告日志便于排查漏调
- **D2**: `session_service.cpp:2849-2905` `add_analyzer` 嵌套 `QEventLoop::exec` 改 `QFutureWatcher` + 信号等待，避免外层 queued broadcast 重入
- **D3**: `storesession.cpp:820` `(int)m->type()` 改 `m->sr_type()`，与同文件 151/823 行风格一致

## Impact

- **Affected specs**:
  - `harden-remaining-crash-risks`（前置，已完成代码改动，编译受阻于 driver compat）——本 spec A4 验证其 Task 1 覆盖率，B-M 类是其未覆盖的补充
  - `fix-broadcast-sync-async-race`（前置，已完成）——本 spec B1/B2/B3 是其未覆盖的 set_config 触发同步 broadcast 与注释错位场景
  - `fix-mmap-async-crash-risks`（前置，已完成）——本 spec B4 mmap_allocator fire-and-forget thread 是其未覆盖的进程退出期 std::terminate 风险
  - `purify-architecture-concepts`（进行中，活跃）——本 spec A3 触发器违规与 spec Task 6 trigger 序列化走 Core 互补；B7 filterprocessor 加锁与 spec Task 19 数据下沉正交
- **Affected code**:
  - `PXView/pv/mainwindow.h/.cpp` — 抽 `safe_current_view()`、60+ 处 ICaptureCallback/on_signals_changed null 检查、`_event` connect Qt::QueuedConnection
  - `PXView/pv/dialogs/calibration.cpp` — `QtConcurrent::run` marshal 回 GUI、QPointer 守卫
  - `PXView/pv/dock/protocoldock.cpp` — `search_done` marshal、`layer->_trace` / `decoder_model->setDecoderStack` 弱引用
  - `PXView/pv/dialogs/protocolexp.cpp` — `save_proc` marshal、emit Qt::QueuedConnection
  - `PXView/pv/data/signalmodel.cpp` — `commit_trig` 改写
  - `PXView/pv/dock/triggerdock.cpp` — else 分支清理
  - `PXView/pv/api/session_service.cpp` — `set_logic_trigger_config` / `get_logic_trigger_config` 改走 Core、QTimer::singleShot QPointer 守卫、`store.wait` 异步化、`add_analyzer` QFutureWatcher、`invoke_or_call` 持锁断言
  - `PXView/pv/view/dsosignal.cpp` / `analogsignal.cpp` — `set_config_uint16` 异步化
  - `PXView/pv/view/view.cpp` — 注释修正、`~View` disconnect 顺序
  - `PXView/pv/tabcontext.cpp` — 删除冗余 broadcast
  - `PXView/pv/api/ws_transport.cpp` — QPointer 守卫
  - `PXView/pv/winnativewidget.cpp` — QPointer 守卫
  - `PXView/pv/dialogs/waitingdialog.cpp` — QPointer 守卫
  - `PXView/pv/data/mmap_allocator.cpp` — fire-and-forget thread join
  - `PXView/pv/core/eventbus.h/.cpp` — `dispatch_to` 拷贝快照、`broadcast_msg` weak_ptr
  - `PXView/pv/core/documentregistry.cpp` — `CaptureOwnerGuard` join 加超时
  - `PXView/pv/sigsession.cpp` — `OnMessage` join 异步、`restart_decoders` 守卫
  - `PXView/pv/core/filterprocessor.cpp` — `_view_data_mutex`
  - `PXView/pv/storesession.cpp` — `ChannelStateRestorer` 守卫、`type()→sr_type()`
  - `PXView/pv/dialogs/storeprogress.cpp` — `wait` 超时
  - `PXView/pv/dock/searchdock.cpp` — emit Qt::QueuedConnection
  - `PXView/pv/core/decodetaskmanager.cpp` — 析构顺序核查

## ADDED Requirements

### Requirement: 异步回调期 current_view 空安全

系统 SHALL 在所有 `ICaptureCallback` / `ISessionStateCallback` 实现入口（`on_signals_changed` / `frame_began` / `frame_ended` / `receive_end` / `receive_data` / `receive_len` / `update_capture`）以及 60+ 处 `current_view()->` 裸调位置提供空指针防御。`MainWindow` SHALL 提供 `safe_current_view()` 返回 `view::View*`（可能为 nullptr）作为唯一访问入口。

#### Scenario: Tab 关闭时序竞态下回调到达

- **WHEN** `broadcast_msg` 改全异步后，ICaptureCallback 回调到达时 Tab 已被关闭，`current_view()` 返回 nullptr
- **THEN** 系统 SHALL 早期 return 并记录日志，不 SHALL 解引用 nullptr 导致崩溃

#### Scenario: 调用点统一使用 safe_current_view

- **WHEN** 检查 `mainwindow.cpp` 中所有 `current_view()->` 调用点
- **THEN** 全部经 `safe_current_view()` 或显式 `if (!view) return;` 守卫，无裸调

### Requirement: 工作线程不直接操作 QWidget

系统 SHALL NOT 在 `QtConcurrent::run` / `std::thread` 工作线程内直接调用 QWidget 的方法（`setRange` / `setValue` / `setText` / `isChecked` / `setFilterFixedString` 等）。工作线程需更新 QWidget 时 SHALL 通过 `QMetaObject::invokeMethod(this, [...], Qt::QueuedConnection)` marshal 回 GUI 线程。

#### Scenario: 校准对话框工作线程改 slider

- **WHEN** `calibration.cpp` `QtConcurrent::run([&]{ reload_value(); })` 在工作线程跑 `reload_value()`
- **THEN** `reload_value()` 内对 `QSlider` 的 `setRange` / `setValue` 调用 SHALL 经 `QMetaObject::invokeMethod` 投递回 GUI 线程
- **AND** 工作线程只读不写 QWidget 属性

#### Scenario: 协议 dock 搜索完成回写 UI

- **WHEN** `protocoldock.cpp` `QtConcurrent::run([&]{ search_done(); })` 在工作线程跑 `search_done()`
- **THEN** `search_done()` 内对 `_ann_search_edit->text()` / `_matchs_label->setText` / `setFilterFixedString` 的访问 SHALL marshal 回 GUI 线程

### Requirement: 触发器单一真相源运行时强制

系统 SHALL 让所有触发器写入路径（GUI `TriggerDock::commit_trigger`、MCP `SessionService::set_logic_trigger_config`、`SignalModel::commit_trig`）只更新 Core `TriggerConfig`，由 `SigSession::sync_trigger_to_libsigrok()` 在 `exec_capture` 内统一同步到 libsigrok。`SignalModel::commit_trig()` SHALL NOT 直接调用 `ds_trigger_*`。`SessionService::set_logic_trigger_config` SHALL NOT 直接调用 `ds_trigger_set_stage` / `set_en`。`get_logic_trigger_config` SHALL 从 Core `TriggerConfig` 读取，不从 `ds_trigger_get_*` 读。

#### Scenario: SignalModel::commit_trig 不写 libsigrok

- **WHEN** 检查 `SignalModel::commit_trig()` 实现
- **THEN** 仅更新 Core `_trig_type`，无 `ds_trigger_probe_set` / `ds_trigger_set_en` 调用
- **AND** libsigrok 同步在 `start_capture` 时由 `sync_trigger_to_libsigrok` 完成

#### Scenario: MCP 写触发器不直接调 ds_trigger

- **WHEN** MCP 客户端调 `set_logic_trigger_config`
- **THEN** 系统改写 Core `TriggerConfig`，下次 `start_capture` 时由 `sync_trigger_to_libsigrok` 同步
- **AND** MCP 与 GUI 路径不会互相覆盖触发器状态

### Requirement: set_config 不触发同步 broadcast 反向自删

系统 SHALL 让 `DsoSignal::set_config_uint16` / `AnalogSignal::set_config_uint16` 触发的 `config_changed` → `broadcast_msg` 异步投递（`Qt::QueuedConnection`），函数尾部 SHALL NOT 继续访问任何 `this` 成员。这避免同步 broadcast → OnMessage → reload → View AllReplaced → `delete this` 链。

#### Scenario: DsoSignal 写配置不 UAF

- **WHEN** `DsoSignal::set_config_uint16(SR_CONF_PROBE_OFFSET, ...)` 触发 `config_changed`
- **THEN** `broadcast_msg(SAMPLE_COUNT_UPDATED)` SHALL 异步投递到下一事件循环
- **AND** `set_config_uint16` 函数返回前不再访问 `this->_model` 等成员
- **AND** reload 触发的 View AllReplaced 在 `set_config_uint16` 栈帧展开后执行

### Requirement: Broadcast 语义注释与代码一致

`view.cpp` 中 `add_decoder` / `remove_decoder` / `clear_all_decoders` 的注释 SHALL 与当前 `broadcast_msg` 全异步语义一致。原"broadcast 是同步直接调用"的注释 SHALL 删除或改写。`tabcontext.cpp` reload 后 SHALL NOT 紧接 `broadcast_msg(DEVICE_OPTIONS_UPDATED)` 触发二次 reload。

#### Scenario: 注释与机制一致

- **WHEN** 检查 `view.cpp:2468/2562/2618` 注释
- **THEN** 注释明确 broadcast 为 `Qt::QueuedConnection` 异步投递
- **AND** 不再出现"broadcast 是同步直接调用（非 Qt queued）"等过时表述

#### Scenario: tab 切换不触发二次 reload

- **WHEN** `tabcontext.cpp:87-105` reload 后
- **THEN** 不再 `broadcast_msg(DEVICE_OPTIONS_UPDATED)`，避免 OnMessage 二次 reload

### Requirement: 裸 this 跨线程 lambda 加守卫

系统 SHALL 在所有 `QMetaObject::invokeMethod(qApp, [this, ...]{...}, Qt::QueuedConnection)` / `QTimer::singleShot(0, qApp, [this, ...]{...})` / `QtConcurrent::run([&]{ this->... })` / `std::thread([this]{...})` 模式中加 `QPointer<T>` 或 `weak_ptr` 守卫，lambda 内 `if (!guard) return;`。`mmap_allocator.cpp:253` fire-and-forget thread SHALL join 或显式 detach 到受控对象。

#### Scenario: SessionService 析构后排队 lambda 不 UAF

- **WHEN** `session_service.cpp:2384` `QTimer::singleShot(0, qApp, [this, decoder_stack]{...})` 投递后，SessionService 在 lambda 派发前析构
- **THEN** lambda 内 `QPointer<SessionService> guard(this); if (!guard) return;` 守卫生效
- **AND** 不访问悬垂 `this`

#### Scenario: mmap_allocator fire-and-forget thread 安全退出

- **WHEN** `mmap_allocator.cpp:253` `std::thread([file_to_delete]{...})` 创建后
- **THEN** 进程退出时线程已 join 或显式 detach，不 `std::terminate`

### Requirement: GUI 线程不阻塞 join

系统 SHALL NOT 在 GUI 线程同步 `join()` 后台线程超过 100ms（copy_thread / store_thread / filter_thread）。`CaptureOwnerGuard` 析构、`OnMessage(DSV_MSG_REV_END_PACKET)` 同步 join、`store.wait()`、`~StoreProgress()` wait SHALL 改异步或加超时 + 失败 detach。

#### Scenario: CaptureOwnerGuard 析构不冻结 UI

- **WHEN** `documentregistry.cpp` `CaptureOwnerGuard` 析构调 `join_copy_thread()`
- **THEN** 加超时（如 5s），超时后 detach + 警告日志
- **AND** 不阻塞 GUI 事件循环

#### Scenario: store.wait 不阻塞主线程

- **WHEN** `session_service.cpp` `store.wait()` 在主线程同步等待
- **THEN** 改 `QtConcurrent::run` + `QFutureWatcher` 异步等待
- **AND** UI 期间可响应

### Requirement: FilterProcessor 共享状态加锁

系统 SHALL 让 `FilterProcessor` 的 `glitch_filter_task` / `signal_invert_task` 读 `_session->_view_data` 时持 `_view_data_mutex`，`clear_glitch_filter` 写 `_logic_backup` 时同样持锁。`restart_decoders()` SHALL 检查 `is_copy_in_progress()`，copy 进行中改异步队列。

#### Scenario: glitch_filter 与 clear_glitch_filter 互斥

- **WHEN** GUI 线程调 `clear_glitch_filter` `delete + nullptr` `_logic_backup` 时，FilterProcessor 后台线程正在读 `_logic_backup`
- **THEN** mutex 保护下互斥执行，不出现 UAF

#### Scenario: restart_decoders 不与 copy 线程争用

- **WHEN** `restart_decoders()` 同步调 `copy_data_to_document(doc)` 时后台 copy 线程仍在跑
- **THEN** `is_copy_in_progress()` 守卫返回 true，改异步队列等待 copy 完成后再执行

### Requirement: 跨对象不持 view / sr_channel 裸指针

`ProtocolItemLayer::_trace`、`DecoderGroupBox` 持有的 `DecoderStack*` SHALL 改为 `weak_ptr<DecoderStack>` 或 QPointer 守卫。`analogsignal.cpp` / `dsosignal.cpp` 函数局部持 `sr_channel*` SHALL 短期持 + 复检 `_model` 存活。`ChannelStateRestorer` RAII SHALL 加设备存活守卫。

#### Scenario: DecoderStack 销毁后 ProtocolItemLayer 不悬垂

- **WHEN** DecoderStack 从 SessionDocument 移除（shared_ptr 引用归零）而 ProtocolItemLayer 仍存活
- **THEN** `layer->_trace` 是 `weak_ptr`，`lock()` 返回 nullptr，UI 安全跳过
- **AND** 不访问已释放的 DecoderStack

#### Scenario: 设备切换时 ChannelStateRestorer dtor 不 UAF

- **WHEN** 导出期间 `set_device()` 或设备拔出导致 `sdi->channels` 被释放，`ChannelStateRestorer` dtor 再次遍历 GSList
- **THEN** 设备存活守卫拦截，dtor 拒绝访问 GSList

### Requirement: EventBus 遍历快照稳定性

`EventBus::dispatch_to<Iface>()` SHALL 拷贝 `_callbacks` 快照后遍历，listener 在回调中同步 add/remove listener 不影响当前遍历。`broadcast_msg` lambda 捕获的 `EventBus* this` SHALL 改为 `weak_ptr` 或在 EventBus 析构时取消 qApp 队列中的 pending lambda。

#### Scenario: listener 在 OnMessage 里同步注册新 listener

- **WHEN** 某 listener 的 OnMessage 中同步调 `add_callback(new_listener)`
- **THEN** 当前 dispatch_to 遍历不受影响，新 listener 在下一次 dispatch 才生效
- **AND** 不出现 vector 迭代器失效 UAF

### Requirement: 跨线程 emit 显式 Qt::QueuedConnection

系统 SHALL 在所有跨工作线程 emit 的信号连接显式声明 `Qt::QueuedConnection`，SHALL NOT 依赖 thread affinity 隐式 Queued。涉及 `searchdock.cpp` / `protocolexp.cpp` / `storesession.cpp` 信号 / `mainwindow.cpp` `_event` connect。

#### Scenario: moveToThread 改动不破坏连接

- **WHEN** 任何 QObject 的 thread affinity 改变（如未来重构把 dock moveToThread）
- **THEN** 跨线程信号连接因显式 `Qt::QueuedConnection` 不退化为 DirectConnection
- **AND** 不在工作线程跑槽

### Requirement: 解码任务静默失败可观测

`view.cpp` `add_decoder` silent 路径 SHALL 在 Core 层加 `assert + log` 兜底，silent=true 时记警告日志便于排查漏调。`session_service.cpp` `add_analyzer` 嵌套 `QEventLoop::exec` SHALL 改 `QFutureWatcher` + 信号等待，避免外层 queued broadcast 重入。`storesession.cpp:820` `(int)m->type()` SHALL 改 `m->sr_type()` 与同文件风格一致。

#### Scenario: silent 路径漏调 start_all_decode_tasks 可观测

- **WHEN** 未来新增 silent=true 调用点漏调 `rst_decoder` / `start_all_decode_tasks`
- **THEN** Core 层 assert + log 兜底输出警告
- **AND** 不静默失败导致 DecoderStack 永不解码

#### Scenario: add_analyzer 不嵌套事件循环

- **WHEN** MCP `add_analyzer` 等待 decoder 启动完成
- **THEN** 用 `QFutureWatcher` 而非 `QEventLoop::exec`
- **AND** 外层 queued broadcast 不会重入

## MODIFIED Requirements

### Requirement: MainWindow ICaptureCallback 实现

**原行为**：`MainWindow::frame_began` / `receive_end` / `receive_data` / `receive_len` / `update_capture` / `on_signals_changed` 直接 `current_view()->on_*()` 裸调，依赖 Tab 未关闭的隐含约定。

**新行为**：所有 ICaptureCallback / ISessionStateCallback 实现入口 SHALL 经 `safe_current_view()` 取 view，`if (!view) return;` 守卫，Tab 关闭时序竞态下安全 return + 日志。

### Requirement: SignalModel::commit_trig 职责

**原行为**：`commit_trig()` 直接调用 `ds_trigger_probe_set` / `ds_trigger_set_en` 同步 libsigrok，绕过 Core 单一入口。

**新行为**：`commit_trig()` 仅更新 Core `_trig_type`，libsigrok 同步由 `SigSession::sync_trigger_to_libsigrok()` 在 `exec_capture` 内统一完成。

### Requirement: DsoSignal / AnalogSignal set_config_uint16

**原行为**：`set_config_uint16` 同步触发 `config_changed` → `broadcast_msg` → OnMessage → reload → AllReplaced → `delete this`，函数尾部继续访问 `this->_model` 等 UAF。

**新行为**：`broadcast_msg` 改 `Qt::QueuedConnection` 异步投递，函数尾部不访问 `this` 成员。

### Requirement: tabcontext reload 路径

**原行为**：`reload()` → 设置 trig_type → `broadcast_msg(DEVICE_OPTIONS_UPDATED)` → `rebuild_signals_from_config()`，触发二次 reload。

**新行为**：`reload()` 后不再 `broadcast_msg(DEVICE_OPTIONS_UPDATED)`，避免二次 reload。

## REMOVED Requirements

### Requirement: `current_view()->` 链式裸调作为 Tab 存活的隐含约定

**Reason**：`broadcast_msg` 改全异步后 Tab 关闭时序竞态显著，60+ 处裸调是异步回调期最大批量闪退源。`TabContext` 已支持空 view 状态，但 MainWindow 未做对应防御。

**Migration**：抽 `safe_current_view()` 返回 `view::View*`（可能为 nullptr），60+ 处统一替换为 null 检查兜底。

### Requirement: `SignalModel::commit_trig()` 直接调 `ds_trigger_*`

**Reason**：违反"触发配置只能写 Core TriggerConfig，禁止直接调用 ds_trigger_*"硬约束。GUI / MCP / commit_trig 三条路径并存导致下次 `exec_capture` 时驱动读到不一致配置 → 越界/崩溃。

**Migration**：`commit_trig()` 仅更新 Core `_trig_type`；`TriggerDock` else 分支删除对 `commit_trig` 的调用，统一走 Core `TriggerConfig` 写入；`SessionService::set_logic_trigger_config` 同样改写 Core，由 `sync_trigger_to_libsigrok` 统一同步。

### Requirement: `view.cpp:2468/2562/2618` "broadcast 是同步直接调用" 注释

**Reason**：`broadcast_msg` 已改全异步，注释与现状矛盾，原依赖"先同步 reload、再 start_all_decode_tasks"的顺序假设已破坏。

**Migration**：删除过时注释，改写为"broadcast 经 `Qt::QueuedConnection` 异步投递"，需要同步等待 reload 完成的路径改为显式同步等待或异步路径。
