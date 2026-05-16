# DeviceOptionsDock 滚动性能优化 Spec

## Why
基于火焰图分析，DeviceOptionsDock 滚动时 `paintSiblingsRecursive` 递归绘制占 35.3% 主线程时间，`setStyle_helper` 级联占 100.5% 总采样。根因是 Widget 层级过深（10-11 层嵌套）+ `update_view()` 全量销毁重建 + 滚动动画期间布局计算未禁用，导致每帧都触发递归重绘和级联布局更新。

## What Changes
- 为 DeviceOptionsDock 及其子 Widget 设置 `Qt::WA_OpaquePaintEvent` + `Qt::WA_NoSystemBackground`，跳过背景绘制
- 重构 `update_view()` 为增量更新模式，避免全量 delete/new 导致的 `setStyle_helper` 级联
- 扁平化毛刺过滤面板，移除 `row_container` 和 `content_widget` 中间容器，改用 `QGridLayout`
- 优化 `try_resize_scroll()` 为批量操作，先禁用布局再逐个设置固定尺寸
- SmoothScrollArea 动画期间禁用内容布局计算，动画结束后恢复

## Impact
- Affected code: `deviceoptionsdock.h/cpp`, `smoothscrollarea.cpp`
- Affected specs: `optimize-main-thread-perf`（互补），`add-smooth-scroll-animation`（动画行为不变）

## ADDED Requirements

### Requirement: DeviceOptionsDock 不透明绘制属性
系统 SHALL 为 DeviceOptionsDock 及其不透明子 Widget 设置 `Qt::WA_OpaquePaintEvent` 和 `Qt::WA_NoSystemBackground` 属性，跳过系统背景填充。

#### Scenario: 构造时设置不透明属性
- **WHEN** DeviceOptionsDock 构造完成
- **THEN** `_container_panel`、`_dynamic_panel`、`mode_section`、`_glitch_filter_group` 均设置了 `WA_OpaquePaintEvent` + `WA_NoSystemBackground`
- **AND** 滚动时这些 Widget 不触发背景填充绘制

#### Scenario: update_view 重建后重新设置属性
- **WHEN** `update_view()` 重建了子 Widget
- **THEN** 新创建的 Widget 同样设置了 `WA_OpaquePaintEvent` + `WA_NoSystemBackground`

### Requirement: update_view 增量更新
系统 SHALL 将 `update_view()` 从全量销毁重建改为增量更新模式，仅更新变化的控件文本/状态。

#### Scenario: 语言/字体更新时仅更新文本和字体
- **WHEN** `UpdateLanguage()` 或 `UpdateFont()` 被调用
- **THEN** 仅遍历现有 QLabel/QPushButton 更新文本和字体
- **AND** 不执行任何 `delete` 或 `new` 操作
- **AND** 不触发 `setStyle_helper` 级联

#### Scenario: 主题更新时仅更新样式属性
- **WHEN** `UpdateTheme()` 被调用
- **THEN** 仅调用 `update()` 触发样式重绘
- **AND** 不销毁和重建 Widget 树

#### Scenario: 设备变更时仍允许全量重建
- **WHEN** `device_updated()` 检测到设备变更需要完全重建
- **THEN** 执行全量 `update_view()` 逻辑（保留现有行为）

### Requirement: 毛刺过滤面板扁平化
系统 SHALL 移除毛刺过滤面板中的中间容器 Widget，改用 `QGridLayout` 直接排列通道行。

#### Scenario: 每个通道行不再有中间容器
- **WHEN** `build_glitch_filter_panel()` 构建毛刺过滤面板
- **THEN** 每个通道行直接通过 `QGridLayout::addWidget()` 添加到 `ch_container` 的网格中
- **AND** 不再创建 `row_container` 和 `content_widget` 中间 Widget
- **AND** 16 通道场景下减少 32 个 Widget 实例

### Requirement: try_resize_scroll 批量操作
系统 SHALL 优化 `try_resize_scroll()` 为批量操作模式，减少布局重算次数。

#### Scenario: 批量设置固定尺寸时禁用布局
- **WHEN** `try_resize_scroll()` 执行
- **THEN** 先调用 `setUpdatesEnabled(false)` 和 `_container_lay->setEnabled(false)`
- **AND** 批量设置所有 QLabel 的 `setFixedSize`
- **AND** 最后恢复 `setEnabled(true)` 和 `setUpdatesEnabled(true)`

### Requirement: SmoothScrollArea 动画期间禁用布局
系统 SHALL 在 SmoothScrollArea 滚动动画期间禁用内容 Widget 的布局计算。

#### Scenario: 动画开始时禁用布局
- **WHEN** SmoothScrollArea 的滚动动画开始
- **THEN** 对 `widget()->layout()` 调用 `setEnabled(false)` 禁用布局计算
- **AND** 滚动帧中不触发布局重算

#### Scenario: 动画结束时恢复布局
- **WHEN** 滚动动画结束（finished 信号）
- **THEN** 对 `widget()->layout()` 调用 `setEnabled(true)` 恢复布局
- **AND** 触发一次 `activate()` 确保布局正确
