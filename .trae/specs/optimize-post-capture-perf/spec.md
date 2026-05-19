# LogicSnapshot 采集后卡顿优化 Spec

## Why
LOGIC 模式采集高频/大深度信号后，UI 冻结数秒才恢复。根因有三：(1) `copy_data_to_document()` 对同一 document 被调用两次，深拷贝翻倍；(2) `free_data()` 中大量 `free(2MB)` 调用阻塞 UI；(3) `copy_data_to_document()` 在 UI 线程同步执行 `malloc(2MB) + memcpy(2MB)`。本 spec 覆盖 review 文档推荐的前三个优先级修复。

## What Changes
- 消除 `on_frame_ended()` 中对同一 document 的重复 `copy_data_to_document()` 调用
- 新增 `LeafBlockPool` 内存池，将 `malloc/free(2MB)` 替换为池化复用，消除分配/释放卡顿
- 将 `DSV_MSG_REV_END_PACKET` 中的 `copy_data_to_document()` 移至后台线程，UI 线程不再阻塞

## Impact
- Affected code: `mainwindow.cpp`, `logicsnapshot.cpp`, `sessiondocument.cpp`, `sessionsnapshot.cpp`, `sigsession.cpp`, `sigsession.h`, `appcontrol.cpp`
- 新增文件: `leaf_block_pool.h`
- Affected specs: `optimize-main-thread-perf`（互补，不冲突）
- 无 BREAKING 变更

## ADDED Requirements

### Requirement: 消除重复 copy_data_to_document 调用
系统 SHALL 确保 LOGIC 模式下每次采集结束后，对同一个 `SessionDocument` 只执行一次 `copy_data_to_document()`。

#### Scenario: LOGIC 模式下 on_frame_ended 不重复拷贝
- **WHEN** LOGIC 模式采集结束，`DSV_MSG_REV_END_PACKET` 已调用 `copy_data_to_document(_active_document)`
- **AND** 随后 `on_frame_ended()` 被触发
- **THEN** `on_frame_ended()` 中不再对同一 document 执行 `copy_data_to_document()`
- **AND** `save_signal_config()` 和 `ctx->activate()` 仍然正常执行

#### Scenario: 非 LOGIC 模式下 on_frame_ended 仍执行拷贝
- **WHEN** DSO 或 ANALOG 模式采集结束
- **THEN** `on_frame_ended()` 中正常调用 `copy_data_to_document()`

#### Scenario: 切换标签页后 document 不同时仍执行拷贝
- **WHEN** `on_frame_ended()` 中 `ctx->document()` 与 `_active_document` 不是同一对象
- **THEN** 正常执行 `copy_data_to_document(ctx->document())`

### Requirement: LeafBlock 内存池
系统 SHALL 提供 `LeafBlockPool` 单例，将 LeafBlock 大小的内存块（约 2MB）的分配和释放改为池化复用，消除 `malloc/free` 的系统调用开销。

#### Scenario: acquire 从池中获取内存块
- **WHEN** 调用 `LeafBlockPool::instance().acquire(block_size)`
- **AND** 池中有空闲块
- **THEN** 直接返回空闲块，不调用系统 `malloc`
- **AND** 块内容可能包含旧数据，调用方必须自行 `memset` 初始化

#### Scenario: acquire 池空时回退到 malloc
- **WHEN** 调用 `acquire(block_size)` 且池中无空闲块
- **THEN** 调用系统 `malloc(block_size)` 分配新块

#### Scenario: release 归还内存块到池中
- **WHEN** 调用 `LeafBlockPool::instance().release(ptr)`
- **AND** 池中空闲块数量未达到上限
- **THEN** 将 ptr 放入空闲列表，不调用系统 `free`

#### Scenario: release 池满时真正释放
- **WHEN** 调用 `release(ptr)` 且池中空闲块数量已达到上限
- **THEN** 调用系统 `free(ptr)` 真正释放内存

#### Scenario: drain 释放池中所有闲置内存
- **WHEN** 调用 `LeafBlockPool::instance().drain()`
- **THEN** 对池中所有空闲块调用系统 `free`
- **AND** 清空空闲列表

#### Scenario: 所有 malloc(LeafBlockSpace) 替换为池化调用
- **WHEN** 代码中存在 `malloc(LeafBlockSpace)` 或 `malloc(LogicSnapshot::LeafBlockSpace)`
- **THEN** 替换为 `LeafBlockPool::instance().acquire(LeafBlockSpace)` 或对应变体
- **AND** 覆盖 `logicsnapshot.cpp`（9处）、`sessiondocument.cpp`（1处）、`sessionsnapshot.cpp`（1处）

#### Scenario: 所有 free(lbp) 替换为池化调用
- **WHEN** 代码中存在对 LeafBlock 指针的 `free()` 调用
- **THEN** 替换为 `LeafBlockPool::instance().release(ptr)`
- **AND** 覆盖 `logicsnapshot.cpp` 中 `free_data()`、`decode_end()`、`free_decode_lpb()` 的 free 调用

#### Scenario: 应用退出时释放内存池
- **WHEN** `AppControl::Destroy()` 被调用
- **THEN** 调用 `LeafBlockPool::instance().drain()` 释放所有池化内存

### Requirement: copy_data_to_document 异步化
系统 SHALL 将 `DSV_MSG_REV_END_PACKET` 处理中的 `copy_data_to_document()` 移至后台线程执行，UI 线程立即返回。

#### Scenario: 采集结束后 UI 立即可交互
- **WHEN** LOGIC 模式采集结束，`DSV_MSG_REV_END_PACKET` 被处理
- **THEN** `frame_ended()` 信号立即发出，UI 线程不等待拷贝完成
- **AND** 用户可以立即拖拽、缩放波形

#### Scenario: 后台线程执行深拷贝
- **WHEN** `bAddDecoder` 为 true 且 `_active_document` 非空
- **THEN** 在后台线程中执行 `copy_data_to_document(doc)`
- **AND** 拷贝完成后在 UI 线程启动解码器

#### Scenario: 后台拷贝期间源数据不被破坏
- **WHEN** 后台拷贝线程正在读取 `_view_data`
- **THEN** `_view_data` 不会被 `clear()` 或修改
- **AND** 如果用户在此期间启动新采集，等待拷贝完成后再执行

#### Scenario: 后台拷贝完成前禁止新采集破坏源数据
- **WHEN** 后台拷贝正在进行（`_copy_in_progress == true`）
- **AND** 用户点击新采集或 Repeat 模式自动触发下一轮
- **THEN** 等待后台拷贝完成后再开始新采集
- **AND** 等待期间保持 UI 响应

#### Scenario: 解码器在拷贝完成后正确启动
- **WHEN** 后台 `copy_data_to_document()` 完成
- **THEN** 在 UI 线程中对所有 decode traces 执行 `set_capture_end_flag(true)`、`frame_ended()`、`add_decode_task()`
- **AND** 解码器能正确读取 document 中的数据

## MODIFIED Requirements

### Requirement: MainWindow::on_frame_ended()
原实现无条件调用 `_session->copy_data_to_document(ctx->document())`。新实现 SHALL 检查 `_active_document` 是否已与 `ctx->document()` 相同，若相同则跳过拷贝。

### Requirement: SigSession DSV_MSG_REV_END_PACKET 处理
原实现在 UI 线程同步执行 `copy_data_to_document()` + `add_decode_task()`。新实现 SHALL 将拷贝移至后台线程，拷贝完成后再在 UI 线程启动解码器。

### Requirement: LogicSnapshot 内存管理
原实现使用系统 `malloc/free` 管理 LeafBlock 内存。新实现 SHALL 使用 `LeafBlockPool` 池化管理。
