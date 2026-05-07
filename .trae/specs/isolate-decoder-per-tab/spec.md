# 解码器标签隔离 Spec

## Why
当前多标签页的 DecodeTrace 对象在 SigSession 和 SessionDocument 之间是浅拷贝（共享指针），导致在其他标签页启动新采集时，`clear_decode_result()` 清除所有共享 DecodeTrace 的注释数据，使非活跃标签页的解码器变为空行。需要让每个 SessionDocument 独占自己的 DecodeTrace 对象，实现标签间解码器完全隔离。

## 架构图

### 当前架构（有 Bug）

```
MainWindow
├── QTabWidget
│   ├── Tab[0]: View[0] + SessionDocument[0]
│   │              │           │
│   │              │           └── _decode_traces ──┐ (浅拷贝，共享指针)
│   │              │                                 │
│   │              └── _own_signals (深拷贝，独立)    │
│   │                  缩放参数来自 SessionDocument   │
│   │                  解码器显示来自 SessionDocument._decode_traces
│   │                                         │
│   └── Tab[1]: View[1] + SessionDocument[1]  │
│                  │           │               │
│                  │           └── _decode_traces ──┐ (浅拷贝，共享指针)
│                  │                                 │
│                  └── _own_signals (深拷贝，独立)    │
│                                                 │
│                                                 ▼
├── SigSession (全局唯一) ◄─────────────────────────┘
│   ├── _decode_traces = [Trace_A, Trace_B]  ◄── 共享同一批对象！
│   ├── clear_decode_result() → 清除所有 Trace 的注释 ← BUG根源
│   ├── add_decoder() → 加入 _decode_traces + _active_document
│   ├── DecoderStack::do_decode_work() → 从 _session->get_signals() 获取快照
│   ├── _active_document 仅在 on_frame_began() 更新
│   └── 帧结束: _active_document->_decode_traces = _decode_traces (浅拷贝覆盖)
│
└── ProtocolDock
    ├── ProtocolItemLayer 关联 DecoderStatus* (key_handel)
    ├── bind_context() 不重建 ProtocolItemLayer
    └── 切换标签后 ProtocolItemLayer 指向旧标签的解码器
```

**Bug 链**：
1. Tab A 添加解码器 → DecodeTrace 同时加入 SigSession._decode_traces 和 SessionDocument[0]._decode_traces（同一指针）
2. 切换到 Tab B → Tab A 的 SessionDocument._decode_traces 仍指向共享对象
3. Tab B 启动新采集 → `clear_decode_result()` 遍历 SigSession._decode_traces 调用 `decoder()->init()` → 清除所有共享 DecodeTrace 的注释
4. 切回 Tab A → View 从 SessionDocument[0]._decode_traces 获取解码器 → 注释已被清除 → 空行

### 目标架构（Document 独占所有权）

```
MainWindow
├── QTabWidget
│   ├── Tab[0]: View[0] + SessionDocument[0]
│   │              │           │
│   │              │           └── _decode_traces = [Trace_0A, Trace_0B]  ◄── 独占所有权
│   │              │               完全独立，不受其他标签影响
│   │              │               DecoderStack._owner_document → SessionDocument[0]
│   │              │
│   │              └── _own_signals (深拷贝，独立)
│   │                  缩放参数来自 SessionDocument
│   │                  解码器显示来自 SessionDocument[0]._decode_traces
│   │
│   └── Tab[1]: View[1] + SessionDocument[1]
│                  │           │
│                  │           └── _decode_traces = [Trace_1A]  ◄── 独占所有权
│                  │               完全独立，不受其他标签影响
│                  │               DecoderStack._owner_document → SessionDocument[1]
│                  │
│                  └── _own_signals (深拷贝，独立)
│
├── SigSession (全局唯一，解码调度器)
│   ├── _active_document → SessionDocument[0] 或 [1]  ◄── 标签切换时更新
│   ├── _decode_traces: 移除（通过 _active_document 代理访问）
│   ├── get_decode_signals() → _active_document->get_decode_signals()
│   ├── add_decoder() → 添加到 _active_document._decode_traces
│   ├── remove_decoder() → 从 _active_document 移除
│   ├── clear_decode_result() → 仅清除 _active_document 的解码结果
│   ├── DecoderStack::do_decode_work() → 从 _owner_document 获取快照
│   ├── _decode_tasks: 全局解码任务队列（持有任意 Document 的 DecodeTrace*）
│   └── 帧结束: 不再需要 _active_document->_decode_traces = _decode_traces
│
└── ProtocolDock
    ├── bind_context() → 重建 ProtocolItemLayer（从新活跃 Document 的 DecodeTrace）
    ├── ProtocolItemLayer 关联当前 Document 的 DecoderStatus*
    └── 切换标签后 ProtocolItemLayer 正确反映新标签的解码器
```

### 数据流对比

**当前（Bug）**：
```
添加解码器 → SigSession._decode_traces.push_back(trace)
           → _active_document._decode_traces.push_back(trace)  // 同一指针！

新采集启动 → clear_decode_result()
           → for (de : _decode_traces) de->decoder()->init()  // 清除所有！
           → Tab A 和 Tab B 的注释都被清除

帧结束 → _active_document->_decode_traces = _decode_traces  // 浅拷贝覆盖
```

**目标（修复后）**：
```
添加解码器 → _active_document->add_decode_trace(trace)  // 仅加入活跃 Document
           → trace->decoder()->set_owner_document(_active_document)

新采集启动 → clear_decode_result()
           → for (de : _active_document->get_decode_traces()) de->decoder()->init()
           → 仅清除活跃 Document 的注释，其他 Document 不受影响

帧结束 → 不再需要同步（Document 已拥有自己的 Trace）
```

## What Changes

### 核心改动
- **SigSession._decode_traces 移除**：所有访问通过 `_active_document->get_decode_traces()` 代理
- **DecoderStack 增加 _owner_document**：解码时从所属 Document 获取快照，而非全局 Session
- **TabContext::activate() 更新 _active_document**：标签切换时同步更新
- **clear_decode_result() 仅影响活跃 Document**：不再遍历全局共享列表
- **帧结束浅拷贝同步移除**：Document 已拥有自己的 Trace，无需同步
- **DecodeTrace::paint_back() 使用 Document 采样率**：不再依赖全局 Session
- **ProtocolDock 标签切换重建**：bind_context() 重建 ProtocolItemLayer

### 辅助改动
- **SessionDocument._decoder_stacks 同步维护**：add/remove decoder 时同步更新
- **clear_all_decoder_for_device() 新增**：设备切换/关闭时清除所有 Document 的解码器
- **capture_snapshot() 适配**：从活跃 Document 获取解码器列表

## Impact
- Affected specs: isolate-tab-data-snapshot（SessionDocument DataSource 已实现）
- Affected code:
  - `pv/sigsession.h/cpp` — _decode_traces 代理、_active_document 更新、clear_decode_result 隔离
  - `pv/data/decoderstack.h/cpp` — 新增 _owner_document、do_decode_work() 快照来源
  - `pv/data/sessiondocument.h/cpp` — _decoder_stacks 维护、_decoder_model 管理
  - `pv/tabcontext.h/cpp` — activate() 更新 _active_document
  - `pv/view/decodetrace.h/cpp` — paint_back() 采样率来源
  - `pv/dock/protocoldock.h/cpp` — bind_context() 重建 ProtocolItemLayer
  - `pv/mainwindow.cpp` — on_tab_changed() 适配、设备切换清除所有 Document

## ADDED Requirements

### Requirement 1: SessionDocument 独占 DecodeTrace 所有权
每个 SessionDocument SHALL 独占拥有其 _decode_traces 中的所有 DecodeTrace 对象。DecodeTrace 对象从创建起就属于某个 SessionDocument，不会在多个 Document 之间共享。SigSession 不再持有独立的 _decode_traces 列表。

#### Scenario: 添加解码器到活跃 Document
- **GIVEN** Tab A 是活跃标签，Tab A 的 SessionDocument 有 0 个解码器
- **WHEN** 用户在 Tab A 添加 SPI 解码器
- **THEN** 新 DecodeTrace 对象仅被添加到 Tab A 的 SessionDocument._decode_traces
- **AND** SigSession 不持有独立的 _decode_traces 副本
- **AND** Tab B 的 SessionDocument._decode_traces 不受影响

#### Scenario: 非活跃 Document 的解码器不受影响
- **GIVEN** Tab A 有 2 个解码器（有注释数据），Tab B 是活跃标签
- **WHEN** Tab B 启动新采集
- **THEN** Tab A 的 SessionDocument._decode_traces 中的注释数据保持不变
- **AND** 切回 Tab A 后解码器正常显示注释

### Requirement 2: SigSession 通过 _active_document 代理访问解码器
SigSession 的所有解码器操作（add/remove/clear/get）SHALL 通过 `_active_document->get_decode_traces()` 进行，不再使用独立的 `_decode_traces` 成员。`get_decode_signals()` 返回 `_active_document->get_decode_signals()`。

#### Scenario: get_decode_signals() 返回活跃 Document 的解码器
- **GIVEN** Tab A 是活跃标签，有 2 个解码器；Tab B 有 1 个解码器
- **WHEN** 调用 SigSession::get_decode_signals()
- **THEN** 返回 Tab A 的 SessionDocument 的 2 个 DecodeTrace

#### Scenario: _active_document 为 null 时的安全处理
- **GIVEN** _active_document 为 null
- **WHEN** 调用任何需要访问 _decode_traces 的方法
- **THEN** 方法安全返回（空列表/无操作），不崩溃

### Requirement 3: DecoderStack 从所属 Document 获取快照
DecoderStack SHALL 持有 _owner_document 指针，在 do_decode_work() 中优先从 _owner_document 获取 LogicSnapshot 进行解码，而非从 SigSession::_signals 获取。

#### Scenario: 从 Document 快照解码
- **GIVEN** Tab A 的 DecoderStack._owner_document 指向 Tab A 的 SessionDocument
- **AND** Tab A 的 SessionDocument 有 LogicSnapshot 数据
- **WHEN** DecoderStack::do_decode_work() 执行
- **THEN** _snapshot 从 _owner_document->get_active_logic() 获取
- **AND** 解码使用 Tab A 自己的快照数据

#### Scenario: Document 无数据时回退到 Session 信号
- **GIVEN** DecoderStack._owner_document 无数据（has_data() == false）
- **WHEN** DecoderStack::do_decode_work() 执行
- **THEN** _snapshot 从 _session->get_signals() 中的 LogicSignal::data() 获取（回退行为）

#### Scenario: 跨标签解码任务正确使用所属 Document
- **GIVEN** Tab A 的解码任务正在解码线程中运行
- **AND** 用户切换到 Tab B（_active_document 变为 Tab B 的 Document）
- **WHEN** 解码线程继续执行 do_decode_work()
- **THEN** Tab A 的 DecoderStack 仍使用 _owner_document（Tab A 的 Document）的快照
- **AND** 不受 _active_document 切换影响

### Requirement 4: TabContext::activate() 更新 _active_document
标签激活时 SHALL 调用 `_session->set_active_document(_document)`，确保 SigSession 的解码器操作指向正确的 Document。

#### Scenario: 切换到有数据的标签
- **WHEN** TabContext::activate() 被调用
- **THEN** _session->set_active_document() 被设置为该标签的 SessionDocument
- **AND** SigSession 的所有解码器操作作用于该 Document

#### Scenario: 切换到空标签
- **WHEN** TabContext::activate() 被调用，且 Document 无数据
- **THEN** _session->set_active_document() 仍被设置为该标签的 SessionDocument
- **AND** 添加解码器时加入该 Document

### Requirement 5: clear_decode_result() 仅影响活跃 Document
SigSession::clear_decode_result() SHALL 仅遍历 _active_document 的 _decode_traces 并调用 decoder()->init()，不影响其他 Document 的解码器。

#### Scenario: 新采集仅清除活跃标签的解码结果
- **GIVEN** Tab A 有 2 个解码器（有注释），Tab B 有 1 个解码器（有注释）
- **AND** Tab B 是活跃标签
- **WHEN** Tab B 启动新采集，调用 clear_decode_result()
- **THEN** 仅 Tab B 的 1 个解码器的注释被清除
- **AND** Tab A 的 2 个解码器的注释保持不变

### Requirement 6: 帧结束不再浅拷贝同步
帧结束时 SHALL 移除 `_active_document->get_decode_traces() = _decode_traces` 的浅拷贝赋值，因为 Document 已拥有自己的 DecodeTrace 对象。

#### Scenario: 帧结束后 Document 的解码器不变
- **WHEN** 采集帧结束
- **THEN** 活跃 Document 的 _decode_traces 不被覆盖
- **AND** 已有的 DecodeTrace 对象的 decoder()->set_capture_end_flag(true) 和 frame_ended() 仍正常调用
- **AND** 解码任务仍正常入队

### Requirement 7: DecodeTrace::paint_back() 使用 Document 采样率
DecodeTrace::paint_back() 中计算解码区域标记位置时 SHALL 使用所属 Document 的采样率，而非全局 SigSession 的采样率。

#### Scenario: 非活跃标签的解码区域标记位置正确
- **GIVEN** Tab A 的采样率为 100MHz，Tab B 的采样率为 1MHz
- **WHEN** 在 Tab A 查看 Tab A 的解码器
- **THEN** 解码区域标记（蓝色竖线和三角箭头）位置基于 Tab A 的 100MHz 采样率计算

### Requirement 8: ProtocolDock 标签切换时重建 ProtocolItemLayer
ProtocolDock::bind_context() SHALL 重建 ProtocolItemLayer 列表，从新活跃 Document 的 DecodeTrace 对象创建对应的 UI 控件。

#### Scenario: 切换标签后 ProtocolDock 显示正确的解码器
- **GIVEN** Tab A 有 SPI 解码器，Tab B 有 I2C 解码器
- **WHEN** 从 Tab A 切换到 Tab B
- **THEN** ProtocolDock 的 ProtocolItemLayer 列表清除并重建
- **AND** 显示 Tab B 的 I2C 解码器控件（进度条、设置按钮、删除按钮）
- **AND** 不显示 Tab A 的 SPI 解码器控件

#### Scenario: 切换标签后解码进度信号正确连接
- **GIVEN** Tab B 的解码器正在解码
- **WHEN** 切换到 Tab B
- **THEN** Tab B 的 DecodeTrace::decoded_progress 信号连接到 ProtocolDock::decoded_progress 槽
- **AND** 进度条正常更新

### Requirement 9: 设备切换/关闭时清除所有 Document 的解码器
新增 SigSession::clear_all_documents_decoders() 方法，在设备切换或 Session 关闭时，清除所有 TabContext 的 SessionDocument 中的解码器。

#### Scenario: 设备切换清除所有标签的解码器
- **GIVEN** Tab A 有 2 个解码器，Tab B 有 1 个解码器
- **WHEN** 用户切换设备
- **THEN** Tab A 和 Tab B 的所有解码器都被清除
- **AND** 两个标签的 ProtocolItemLayer 都被清空

## MODIFIED Requirements

### Requirement: SigSession::_decode_traces 访问方式
**Before**: SigSession 持有独立的 `_decode_traces` 成员，所有方法直接访问
**After**: SigSession 通过 `_active_document->get_decode_traces()` 代理访问，`_decode_traces` 成员移除

### Requirement: DecoderStack::do_decode_work() 快照来源
**Before**: 从 `_session->get_signals()` 中的 LogicSignal::data() 获取快照
**After**: 优先从 `_owner_document->get_active_logic()` 获取，无数据时回退到 `_session->get_signals()`

### Requirement: SigSession::add_decoder() 添加目标
**Before**: 同时添加到 `_decode_traces` 和 `_active_document->_decode_traces`（同一指针）
**After**: 仅添加到 `_active_document->_decode_traces`

### Requirement: TabContext::activate() 操作
**Before**: 不更新 `_active_document`
**After**: 调用 `_session->set_active_document(_document)`

### Requirement: ProtocolDock::bind_context() 操作
**Before**: 仅更新 `_session`、`_view`、table model，不重建 ProtocolItemLayer
**After**: 额外重建 ProtocolItemLayer 列表

## REMOVED Requirements

### Requirement: 帧结束时浅拷贝同步 _decode_traces
**Reason**: Document 独占所有权后，不再需要从 SigSession 同步到 Document
**Migration**: 移除 `_active_document->get_decode_traces() = _decode_traces` 赋值

### Requirement: SigSession 持有独立的 _decode_traces 成员
**Reason**: 解码器列表由各 Document 独占持有，SigSession 仅代理访问活跃 Document
**Migration**: 所有 `_decode_traces` 访问改为 `_active_document->get_decode_traces()`
