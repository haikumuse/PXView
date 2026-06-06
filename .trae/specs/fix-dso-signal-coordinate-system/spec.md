# 修复 DSO 信号坐标系统与布局不一致 Spec

## Why
DSO 模式下信号显示偏小。根本原因是 `DsoSignal` 的坐标系统基于整个 viewport 高度（`get_view_rect()` 返回 `_viewport->height()`），而布局系统只给信号分配了 viewport 的一部分高度（`_signalHeight`）。两者不一致导致波形被压缩。AnalogSignal 不受影响是因为其坐标系统基于信号自身区域（`get_totalHeight()` + `get_y()`）。

## What Changes
- 将 `DsoSignal::get_view_rect()` 从基于整个 viewport 改为基于信号自身分配区域（与 AnalogSignal 一致）
- 将 `view.cpp` 中 DSO 信号的 `set_scale()` 参数从 `get_view_rect().height()` 改为 `get_totalHeight()`
- 修正 `DsoSignal::paint_back()` 中使用 `UpMargin` 常量的地方改为使用 `get_view_rect().top()`

## Impact
- Affected specs: fix-view-work-mode-query
- Affected code:
  - `PXView/pv/view/dsosignal.cpp`（`get_view_rect()`、`paint_back()`）
  - `PXView/pv/view/view.cpp`（`set_scale` 调用）

## ADDED Requirements

### Requirement: DsoSignal 坐标系统基于信号自身区域
`DsoSignal::get_view_rect()` SHALL 返回基于信号自身分配区域的矩形（`get_y()` 和 `get_totalHeight()`），而非整个 viewport。

#### Scenario: 2通道 DSO 模式下信号填满分配区域
- **WHEN** DSO 模式有2个通道，viewport 高度为367px，每个信号分配259px
- **THEN** `get_view_rect().height()` 返回259（信号自身高度），而非337（viewport高度减margin）
- **AND** 波形正确填满信号的分配区域

#### Scenario: 零线位于信号区域中心
- **WHEN** DSO 信号的零偏移为中间值
- **THEN** `get_zero_vpos()` 返回信号区域中心位置（基于 `get_y()` 和 `get_totalHeight()`）

### Requirement: DSO 信号 set_scale 使用信号自身高度
`View::signals_changed()` 中 DSO 信号的 `set_scale()` 调用 SHALL 传入 `get_totalHeight()` 而非 `get_view_rect().height()`，与 AnalogSignal 保持一致。

#### Scenario: scale 与信号高度匹配
- **WHEN** DSO 信号的 `totalHeight` 为259px
- **THEN** `_scale = 259 / (_ref_max - _ref_min) * _stop_scale`

### Requirement: DSO paint_back 网格基于信号区域
`DsoSignal::paint_back()` 中的网格线和缩放指示器 SHALL 基于信号自身区域绘制，而非 viewport 绝对坐标。

#### Scenario: 网格线覆盖信号区域
- **WHEN** DSO 信号区域从 y=130 到 y=389
- **THEN** 网格线从信号区域顶部到底部均匀分布

## MODIFIED Requirements
无

## REMOVED Requirements
无
