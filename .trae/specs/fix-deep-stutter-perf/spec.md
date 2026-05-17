# 深层卡顿根因修复 Spec

## Why
之前的 8 个优化（`fix-drag-stutter-perf`）修复了 SlidingDrawer 路径上的问题，但窗口拖动/resize 走的是 `WinNativeWidget → ResizeChild → Qt resize → doPaint` 路径，与 SlidingDrawer 完全无关。PXView 的 `Viewport::doPaint()` 比原版 DSView 重很多——LOGIC 模式每帧无条件重建 QPixmap、每帧排序 signal groups 并绘制圆角卡片、每帧做 theme 颜色字符串查找——这些才是 300ms 卡顿帧的真正根因。

## What Changes
- **LOGIC 模式 QPixmap 智能缓存**：将 4px 阈值缓存逻辑从非 LOGIC 分支扩展到 LOGIC 分支，消除 resize 时每帧 ~4MB 无条件分配
- **Signal Group 卡片预计算缓存**：将每帧的 group 排序 + 边界计算 + drawRoundedRect 改为仅在 signals 变化时预计算，doPaint 中直接绘制预计算结果
- **Theme 颜色缓存**：将 `GetThemeColor("@trace-divider-color")` 从每帧调用改为在 `UpdateTheme()` 时缓存到成员变量
- **Resize 节流**：在 `resizeEvent` 中使用定时器节流，连续 resize 期间最多 16ms 刷新一次，避免 WM_SIZE 风暴

## Impact
- Affected code: `viewport.cpp/h`, `view.cpp/h`
- Affected specs: `fix-drag-stutter-perf`（补充修复，不冲突）
- 无 BREAKING 变更

## ADDED Requirements

### Requirement: LOGIC 模式 QPixmap 智能缓存
系统 SHALL 在 LOGIC 模式的 `paintSignals()` 中应用与 DSO/Analog 分支相同的 4px 阈值 QPixmap 缓存策略。

#### Scenario: LOGIC 模式小幅 resize 不重建 QPixmap
- **WHEN** Viewport 处于 LOGIC 模式且 size 变化小于 4 像素
- **THEN** 复用已有 `_pixmap`，不调用 `QPixmap(size())` 重新分配
- **AND** `_pixmap.fill(Qt::transparent)` 仍然执行以确保内容正确

#### Scenario: LOGIC 模式大幅 resize 正常重建
- **WHEN** Viewport 处于 LOGIC 模式且 size 变化超过 4 像素
- **THEN** 正常执行 `_pixmap = QPixmap(curSize)` 和 `_pixmap_size = curSize`

### Requirement: Signal Group 卡片预计算缓存
系统 SHALL 将 signal group 的排序和卡片边界计算从每帧执行改为仅在 signals 变化时预计算。

#### Scenario: signals 未变化时跳过 group 排序和边界计算
- **WHEN** `doPaint()` 被调用且 signal groups 未发生变化
- **THEN** 直接使用预计算的 `_cached_group_cards` 绘制圆角矩形
- **AND** 不执行 `std::sort(group_indices, ...)` 排序
- **AND** 不执行遍历每个 group 的每个 trace 计算边界的循环

#### Scenario: signals 变化时重新计算 group cards
- **WHEN** `signals_changed()` 被调用（signals 增加/删除/重排）
- **THEN** 重新计算所有 group 的排序和卡片边界
- **AND** 将结果存储到 `_cached_group_cards` 中

#### Scenario: vOffset 变化时卡片位置跟随
- **WHEN** 用户垂直滚动（vOffset 变化）但 signals 未变
- **THEN** 卡片绘制使用预计算的相对位置，加上当前 vOffset 偏移
- **AND** 不需要重新排序 groups

### Requirement: Theme 颜色缓存
系统 SHALL 将 `doPaint()` 中的 theme 颜色查找改为使用缓存的成员变量。

#### Scenario: doPaint 中不调用 GetThemeColor
- **WHEN** `doPaint()` 被调用
- **THEN** divider 颜色从成员变量 `_cached_divider_color` 读取
- **AND** 不调用 `AppConfig::Instance().GetThemeColor("@trace-divider-color")`

#### Scenario: 主题切换时更新缓存
- **WHEN** `UpdateTheme()` 被调用
- **THEN** 重新查询 `GetThemeColor("@trace-divider-color")` 并更新 `_cached_divider_color`

### Requirement: Resize 节流
系统 SHALL 在 Viewport 的 resize 期间限制重绘频率，避免 WM_SIZE 风暴导致的连续全量重绘。

#### Scenario: 连续 resize 期间节流重绘
- **WHEN** 连续的 `resizeEvent` 在 16ms 内多次触发
- **THEN** 仅在最后一次 resize 后 16ms 执行一次完整重绘
- **AND** 中间的 resize 事件仅更新 ViewStatus 几何，不触发 paintEvent

#### Scenario: 单次 resize 正常处理
- **WHEN** 单次 `resizeEvent` 触发且距上次超过 16ms
- **THEN** 正常执行 resize + paint 流程

## MODIFIED Requirements

### Requirement: Viewport::UpdateTheme()
原实现为空函数 `{}`。新实现 SHALL 缓存 theme 颜色到成员变量。

### Requirement: Viewport::paintSignals() LOGIC 分支
原 LOGIC 分支无条件执行 `_pixmap = QPixmap(size())`。新实现 SHALL 使用 4px 阈值判断是否需要重建。
