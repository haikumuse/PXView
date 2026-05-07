# 数据快照隔离型多标签页完善 Spec

## Why
当前多标签页的容器层（QTabWidget、TabContext、Dock 切换）已实现，但数据层未真正隔离：所有 View 共享同一批 Signal 对象（浅拷贝指针），缩放参数依赖全局 SigSession，导致新建标签继承旧标签数据、旧标签缩放被锁死。需要完成方案二（数据快照隔离型）的核心数据隔离，使每个标签完全独立。

## What Changes

### 核心修复（影响标签隔离）
- **View 持有独立的 Signal 对象**：每个 View 的 `_own_signals` 从 SigSession 的 `_signals` 深拷贝创建，而非共享指针
- **View 缩放参数来自 SessionDocument**：`cur_sampletime()`、`cur_snap_samplerate()` 等参数从 `_document` 获取，而非全局 `_data_source`
- **新建标签显示空状态**：新标签的 View 不继承旧标签的数据和缩放状态
- **标签切换后旧标签可独立缩放**：旧标签的 `_maxscale`/`_minscale` 来自自己的 SessionDocument

### 架构完善
- **SessionDocument 实现 DataSource 接口**：使 View 可以从 SessionDocument 获取采样参数，不再依赖全局 SigSession
- **TabContext::activate() 切换 View 的数据源**：活跃标签绑定 SigSession，非活跃标签绑定自己的 SessionDocument
- **Signal 子类支持 clone()**：LogicSignal、AnalogSignal、DsoSignal 实现 clone() 方法，用于深拷贝

## Impact
- Affected specs: refactor-document-view-mvc（Task 2.5/2.6/4.4 暂缓项）、complete-viewport-data-fix（_own_signals 浅拷贝修正）
- Affected code:
  - `pv/data/sessiondocument.h/cpp` — 实现 DataSource 接口
  - `pv/view/view.h/cpp` — _own_signals 深拷贝、缩放参数来源切换
  - `pv/view/signal.h` — 新增 clone() 虚方法
  - `pv/view/logicsignal.h/cpp` — 实现 clone()
  - `pv/view/analogsignal.h/cpp` — 实现 clone()
  - `pv/view/dsosignal.h/cpp` — 实现 clone()
  - `pv/tabcontext.h/cpp` — activate() 切换数据源
  - `pv/mainwindow.cpp` — on_tab_changed() 适配

## ADDED Requirements

### Requirement 1: 每个 View 持有独立的 Signal 对象
每个 View 实例 SHALL 持有自己独立创建的 Signal 对象列表（`_own_signals`），通过 Signal::clone() 从 SigSession 的信号模板深拷贝创建。不同 View 的 Signal 对象完全独立，修改一个 View 的 Signal 不影响其他 View。

#### Scenario: 新建标签不继承旧标签数据
- **GIVEN** 标签 A 已完成采集，viewport 显示波形
- **WHEN** 用户点击 "+" 创建新标签 B
- **THEN** 标签 B 的 View 创建全新的 Signal 对象（clone 自 SigSession 模板）
- **AND** 标签 B 的 Signal 的 `_data` 指向空的 Snapshot
- **AND** 标签 B 的 viewport 显示空状态
- **AND** 标签 A 的 viewport 不受影响

#### Scenario: 标签切换后各自独立
- **GIVEN** 标签 A 有采集数据，标签 B 有采集数据
- **WHEN** 用户从 A 切换到 B
- **THEN** 标签 A 的 Signal 对象和 `_data` 指针保持不变
- **AND** 标签 B 的 Signal 对象和 `_data` 指针保持不变
- **AND** 两个标签的 Signal 对象完全独立

### Requirement 2: View 缩放参数来自 SessionDocument
View 的缩放参数（`_maxscale`、`_minscale`、滚动范围）SHALL 从当前绑定的 SessionDocument 获取，而非全局 SigSession。当 View 没有绑定有数据的 SessionDocument 时，使用 SigSession 的参数作为回退。

#### Scenario: 旧标签可独立缩放
- **GIVEN** 标签 A 有采集数据（samplerate=100MHz），标签 B 有采集数据（samplerate=1MHz）
- **WHEN** 用户从标签 B 切换到标签 A
- **THEN** 标签 A 的 `_maxscale` 基于标签 A 的 SessionDocument 的 samplerate 计算
- **AND** 标签 A 可以正常缩放浏览
- **AND** 标签 B 的缩放状态不受影响

#### Scenario: 空标签使用全局参数
- **GIVEN** 标签 B 是新建的空标签，SessionDocument 无数据
- **WHEN** 用户切换到标签 B
- **THEN** 标签 B 的缩放参数使用 SigSession 的全局参数作为回退
- **AND** viewport 显示空状态

### Requirement 3: SessionDocument 实现 DataSource 接口
SessionDocument SHALL 实现 DataSource 接口，提供 `cur_sampletime()`、`cur_snap_samplerate()`、`cur_samplelimits()` 等方法，使 View 可以从 SessionDocument 获取采样参数。

#### Scenario: 从 SessionDocument 获取采样参数
- **WHEN** View 绑定到一个有数据的 SessionDocument
- **THEN** `SessionDocument::cur_snap_samplerate()` 返回文档记录的采样率
- **AND** `SessionDocument::cur_sampletime()` 返回文档记录的采样时间
- **AND** `SessionDocument::cur_samplelimits()` 返回文档记录的采样深度

#### Scenario: SessionDocument 无数据时的回退
- **WHEN** SessionDocument 没有数据（has_data() == false）
- **THEN** `cur_snap_samplerate()` 返回 0
- **AND** `cur_sampletime()` 返回 0
- **AND** View 使用 SigSession 的参数作为回退

### Requirement 4: Signal 子类支持 clone()
LogicSignal、AnalogSignal、DsoSignal SHALL 实现 clone() 虚方法，创建与原始 Signal 相同配置但数据独立的副本。

#### Scenario: clone() 创建独立 Signal
- **WHEN** 调用 `signal->clone()`
- **THEN** 返回一个新的 Signal 对象
- **AND** 新 Signal 的通道索引、名称、颜色、使能状态与原始相同
- **AND** 新 Signal 的 `_data` 指向 nullptr（由 set_data_document 设置）
- **AND** 新 Signal 与原始 Signal 完全独立

### Requirement 5: TabContext::activate() 切换 View 数据源
标签激活时 SHALL 将 View 的数据源切换到该标签的 SessionDocument。

#### Scenario: 激活有数据的标签
- **WHEN** `TabContext::activate()` 被调用
- **AND** SessionDocument 有数据
- **THEN** View 的 `_data_source` 切换到 SessionDocument
- **AND** View 的 `_own_signals` 的 `_data` 指向 SessionDocument 的 Snapshot
- **AND** View 的缩放参数从 SessionDocument 获取

#### Scenario: 激活空标签
- **WHEN** `TabContext::activate()` 被调用
- **AND** SessionDocument 无数据
- **THEN** View 的 `_data_source` 保持为 SigSession（全局回退）
- **AND** View 的 `_own_signals` 保持不变
- **AND** View 的缩放参数从 SigSession 获取

### Requirement 6: get_traces() 完全使用 _own_signals
View::get_traces() SHALL 完全使用 `_own_signals` 获取信号列表，不再回退到 `_data_source->get_signals()`。DecodeTrace 和 SpectrumTrace 仍从 `_data_source` 获取。

## MODIFIED Requirements

### Requirement: View::_own_signals 从浅拷贝改为深拷贝
**Before**: `_own_signals` 存储指向 SigSession 共享 Signal 的指针（浅拷贝）
**After**: `_own_signals` 存储通过 clone() 创建的独立 Signal 对象（深拷贝），View 负责在析构时释放

### Requirement: View 缩放参数来源
**Before**: 所有缩放参数从 `_data_source`（全局 SigSession）获取
**After**: 优先从 `_document`（SessionDocument）获取，无数据时回退到 `_data_source`

### Requirement: clone_signals_for_document() 实现
**Before**: 从 `_data_source->get_signals()` 浅拷贝指针
**After**: 通过 `signal->clone()` 深拷贝创建独立对象

## REMOVED Requirements

### Requirement: _own_signals 为空时回退到 _data_source->get_signals()
**Reason**: 每个 View 必须始终持有自己的 Signal 对象，不再需要回退到共享信号。
**Migration**: View 构造后立即通过 clone_signals_for_document() 创建独立 Signal。
