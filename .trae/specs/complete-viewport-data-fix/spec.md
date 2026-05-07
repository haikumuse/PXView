# 完成 Viewport 数据显示修复与 MVC 架构收尾 Spec

## Why
Phase 1-4 的 Document-View MVC 重构已完成核心框架和编译，但存在 7 个具体问题导致"点击开始后 viewport 上没有数据"，同时有未完成的 Signal 隔离和解码器迁移任务。本条 spec 覆盖全部剩余工作。

## What Changes

### 必须修复（影响数据显示）
- **修复 has_data()**：当前仅检查 ref 指针非空，不检查 ref 指向的 Snapshot 是否真有数据
- **修复 set_data_document()**：当前操作共享信号，应改为每个 View 有自己独立的信号副本
- **修复 Timings**：确保 `copy_data_to_document()` 调用时 `_view_data` 已包含完整数据
- **清理死代码**：SessionDocument::set_samplerate() 写到的自有 `_logic/_analog/_dso` 永远为空

### 架构完善（影响标签隔离）
- **实现 _own_signals 填充**：View 通过 clone_signals_for_document() 获得独立的信号副本
- **切换所有 paint/update 路径**：View 方法优先使用 `_own_signals`
- **完成解码器迁移**：DecodeTask 从 SigSession 迁移到 SessionDocument
- **清理 `capture_snapshot()` 的所有残留引用**（声明已移除，确保无遗漏）

### 消除冗余
- 移除 `attach_data_to_signal()` + `set_data_document()` 的双重 `set_data()` 调用

## Impact
- Affected specs: refactor-document-view-mvc（所有未完成项）
- Affected code:
  - `pv/data/sessiondocument.h/cpp` — has_data(), set_samplerate(), 自有 Snapshot
  - `pv/view/view.h/cpp` — _own_signals, set_data_document(), clone_signals_for_document()
  - `pv/view/signal.h/cpp` + 所有子类 — set_data()
  - `pv/sigsession.h/cpp` — copy_data_to_document(), attach_data_to_signal(), 解码器管线
  - `pv/mainwindow.cpp` — on_frame_ended(), on_frame_began(), on_tab_changed()
  - `pv/tabcontext.cpp` — activate()

## ADDED Requirements

### Requirement 1: View 持有独立的信号副本
每个 View 实例 SHALL 持有自己独立的 `_own_signals` 向量，通过 `clone_signals_for_document()` 从 SessionDocument 的 DecoderStack 配置创建信号副本。

#### Scenario: 首次采集
- **GIVEN** 一个 LIVE 标签采集完成
- **WHEN** `on_frame_ended()` → `ctx->activate()` → `View::set_data_document()`
- **THEN** View 的 `_own_signals` 被填充（如果为空则首次创建）
- **AND** 每个 `_own_signal` 的 `_data` 指向 document 的 `get_active_*()`

#### Scenario: 二次采集
- **GIVEN** 标签已完成一次采集
- **WHEN** 再次采集完成
- **THEN** `_own_signals` 已存在，只更新 `_data` 指针，不重建信号对象

#### Scenario: 标签切换
- **GIVEN** 标签 A 有数据，标签 B 为空
- **WHEN** 从 A 切换到 B
- **THEN** 标签 A 的 `_own_signals` 保持不变（`_data` 不变）
- **AND** 标签 B 的 `_own_signals` 为空，viewport 显示空状态

### Requirement 2: has_data() 检查实际数据
SessionDocument::has_data() SHALL 不仅检查 ref 指针非空，同时检查 ref 指向的 Snapshot 内部 `get_sample_count() > 0`。

### Requirement 3: 解码器栈归属 SessionDocument
每个标签的解码器栈 SHALL 完全归属其 SessionDocument，SigSession 不再持有 `_decode_traces` 列表。解码任务调度从 SessionDocument 发起。

### Requirement 4: 清理死代码
以下代码 SHALL 被移除或改为不操作自有空 Snapshot：
- `SessionDocument::set_samplerate()` 中设置 `_logic/_analog/_dso` 采样率的代码
- `attach_data_to_signal()` 中操作共享信号_data 的代码（由 set_data_document 统一管理）

## MODIFIED Requirements

### Requirement: set_data_document() 操作范围
**Before**: 操作 `_data_source->get_signals()`（全局共享信号）
**After**: 操作 `_own_signals`（本 View 独立信号副本）

### Requirement: on_frame_ended() 调用顺序
**Before**: `copy_data_to_document()` → `ctx->activate()` → `receive_end()`
**After**: `copy_data_to_document()` → `ctx->activate()`（内部完成信号设置+viewport更新）

## REMOVED Requirements

### Requirement: attach_data_to_signal() 
**Reason**: 已被 `set_data_document()` 替代，双重设置造成冗余。
**Migration**: 所有信号数据绑定统一通过 `View::set_data_document()` 完成。
