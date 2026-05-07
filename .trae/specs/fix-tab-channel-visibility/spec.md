# 方案 C：全局设备配置 + 按标签独立信号可见性 Spec

## Why
当前多标签页架构中，`Signal::enabled()` 直接返回 `_probe->enabled`，所有标签页的信号共享同一个 `sr_channel` 对象。这导致两个问题：
1. 活跃标签页修改设备通道后，当前活跃标签页的 View 不会实时更新信号显示
2. 已采集的历史标签页无法独立控制通道可见性——修改设备通道会影响所有标签页

需要将"硬件通道启用状态"（`enabled`）与"View 级别的信号可见性"（`visible`）分离，使活跃标签跟随设备配置，历史标签保持采集时的通道状态。

## What Changes
- **Trace 类添加 `_visible` 字段**：View 级别的独立可见性状态，默认为 `true`
- **Signal 类添加 `set_enabled()` 方法**：允许在 View 级别独立设置信号的启用状态
- **修改 `View::signals_changed()` 重建逻辑**：撤销之前的 `_own_signals.empty()` 条件，改为根据标签类型决定是否重建
- **修改 `TabContext::activate()`**：历史标签激活时恢复信号的可见性状态
- **修改 `MainWindow::OnMessage()` 中 `DSV_MSG_DEVICE_OPTIONS_UPDATED` 处理**：只对活跃标签重建信号，历史标签只刷新布局
- **修改 `View::set_data_source()`**：活跃标签切换数据源时同步 `enabled` 到 `visible`

## Impact
- Affected specs: isolate-tab-data-snapshot（Signal clone 机制）、fix-multi-tab-bugs（标签切换逻辑）
- Affected code:
  - `pv/view/trace.h` — 添加 `_visible` 字段和访问方法
  - `pv/view/signal.h/cpp` — 添加 `set_enabled()` 方法，修改 `enabled()` 优先使用本地状态
  - `pv/view/view.cpp` — 修改 `signals_changed()` 重建逻辑，修改 `set_data_source()`
  - `pv/tabcontext.cpp` — 修改 `activate()` 恢复历史标签可见性
  - `pv/mainwindow.cpp` — 修改 `DSV_MSG_DEVICE_OPTIONS_UPDATED` 处理逻辑

## ADDED Requirements

### Requirement: Trace 独立可见性
每个 Trace 对象 SHALL 挥有一个独立的 `_visible` 字段，用于控制该信号在当前 View 中是否可见。`_visible` 默认为 `true`。

#### Scenario: 新建信号的默认可见性
- **WHEN** 通过 `clone()` 创建新的 Signal 对象
- **THEN** 新 Signal 的 `_visible` 为 `true`

#### Scenario: 设置信号可见性
- **WHEN** 调用 `trace->set_visible(false)`
- **THEN** 该 Trace 在渲染时被跳过
- **AND** 该 Trace 不占用布局空间
- **AND** 其他标签页的同名 Trace 不受影响

### Requirement: Signal 独立启用状态
Signal 类 SHALL 支持 View 级别的独立启用状态，与 `_probe->enabled` 解耦。

#### Scenario: 活跃标签的 Signal 启用状态
- **WHEN** 活跃标签（无数据）的信号被克隆
- **THEN** `Signal::enabled()` 返回 `_probe->enabled`（跟随设备）
- **AND** `_visible` 同步为 `_probe->enabled`

#### Scenario: 历史标签的 Signal 启用状态
- **WHEN** 历史标签（有数据）的信号被克隆
- **THEN** `Signal::enabled()` 返回该信号在采集时的启用状态
- **AND** `_visible` 保持采集时的值
- **AND** 修改设备通道不影响历史标签的信号显示

### Requirement: 设备通道变更只影响活跃标签
当 `DeviceOptionsDock` 修改设备通道后，只有当前活跃标签（无数据）的 View 重建信号列表，历史标签只刷新布局。

#### Scenario: 修改设备通道后活跃标签更新
- **GIVEN** 当前活跃标签无数据，`_data_source` 为 `SigSession`
- **WHEN** 用户在 DeviceOptionsDock 中修改通道启用状态
- **THEN** `SigSession::reload()` 更新 `_signals`
- **AND** 当前活跃标签的 View 重建 `_own_signals`（从 `_session->get_signals()` 重新克隆）
- **AND** 活跃标签的 View 显示新的通道状态

#### Scenario: 修改设备通道后历史标签不受影响
- **GIVEN** 历史标签有数据，`_data_source` 为 `SessionDocument`
- **WHEN** 用户在 DeviceOptionsDock 中修改通道启用状态
- **THEN** 历史标签的 `_own_signals` 不被重建
- **AND** 历史标签的信号可见性保持不变
- **AND** 历史标签只重新计算布局位置

### Requirement: 历史标签切换时恢复信号可见性
当切换到历史标签时，该标签的信号可见性 SHALL 保持上次离开时的状态。

#### Scenario: 切换到历史标签
- **GIVEN** 标签 A 是历史标签，有采集数据
- **AND** 标签 A 上次显示时通道 0-3 可见，通道 4-7 不可见
- **WHEN** 用户从其他标签切换回标签 A
- **THEN** 标签 A 的 View 仍然显示通道 0-3，隐藏通道 4-7
- **AND** 信号可见性与上次离开时一致

### Requirement: 波形拖拽排序和高度拉伸不被覆盖
用户在 Header 中拖拽波形排序或拉伸高度后，`signals_changed()` 不应重建 `_own_signals`，只重新计算布局。

#### Scenario: 拖拽波形排序后保持顺序
- **WHEN** 用户在 Header 中拖拽波形改变顺序
- **THEN** 释放鼠标后波形顺序保持
- **AND** `_own_signals` 不被重建

#### Scenario: 拉伸波形高度后保持高度
- **WHEN** 用户拉伸波形高度
- **THEN** 拉伸后的高度保持
- **AND** `_own_signals` 不被重建

## MODIFIED Requirements

### Requirement: View::signals_changed() 重建逻辑
**Before**: `signals_changed()` 在 `_own_signals.empty()` 时才从数据源克隆信号
**After**: `signals_changed()` 根据调用来源决定是否重建：
- 由 `set_data_source()` 触发：始终重建（数据源变更）
- 由 `DSV_MSG_DEVICE_OPTIONS_UPDATED` 触发：仅活跃标签重建
- 由 Header 拖拽/拉伸触发：不重建，只重算布局
- 由标签切换触发：不重建，只重算布局

### Requirement: View::set_data_source() 信号克隆
**Before**: `set_data_source()` 始终从新数据源克隆信号
**After**: `set_data_source()` 始终从新数据源克隆信号（不变），但克隆后根据标签类型设置 `visible`：
- 活跃标签：`visible` = `_probe->enabled`
- 历史标签：`visible` 保持克隆值（来自数据源的信号状态）

## REMOVED Requirements

### Requirement: signals_changed() 中 _own_signals.empty() 条件
**Reason**: 该条件导致设备通道变更后活跃标签无法更新信号列表，需要更精细的重建控制
**Migration**: 改用显式的 `rebuild_signals()` 方法控制信号重建，`signals_changed()` 只负责布局计算
