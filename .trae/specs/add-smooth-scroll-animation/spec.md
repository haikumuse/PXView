# 流畅滚动动画 Spec

## Why
DSView 项目所有滚动条（View 主视图水平/垂直滚动条、7 个 Dock 面板的 QScrollArea 滚动条、2 个 QTableView 滚动条）均为即时跳变，没有任何缓动动画，体验生硬。ATK 项目通过 `Easing.OutCubic` 缓动 + 分层动画 + Timer 节流实现了非常流畅的滚动体验。本变更将 ATK 的流畅滚动技术移植到 DSView 的 Qt C++ 代码中。

## What Changes
- 新增 `SmoothScrollBar` 自定义控件，封装 `QPropertyAnimation` + `QEasingCurve::OutCubic` 缓动
- 新增 `SmoothScrollArea` 自定义控件，为 Dock 面板提供滚轮平滑滚动
- 修改 `View` 类，将水平/垂直滚动条替换为 `SmoothScrollBar`，并适配现有 `_updating_scroll` 防递归逻辑
- 修改 7 个 Dock 类的基类从 `QScrollArea` 改为 `SmoothScrollArea`
- 为 SearchDock 和 ProtocolDock 的 QTableView 添加 `QScroller` 惯性滚动（等效于 QML `Flickable.AutoFlickDirection`）
- 修改全局 QSS 样式，统一滚动条视觉风格

## Impact
- Affected code: `view.h/cpp`, `viewport.cpp`, `header.cpp`, 所有 Dock 类, `searchdock.cpp`, `protocoldock.cpp`, `dark.qss`, `light.qss`
- 新增文件: `smoothscrollbar.h/cpp`, `smoothscrollarea.h/cpp`

## ADDED Requirements

### Requirement: SmoothScrollBar 自定义控件
系统 SHALL 提供 `SmoothScrollBar` 类（继承 `QScrollBar`），具备以下能力：

#### Scenario: 滚轮/点击轨道触发平滑滚动
- **WHEN** 滚轮事件或点击滚动条轨道导致 value 变化
- **THEN** value 通过 `QPropertyAnimation` 以 `QEasingCurve::OutCubic` 缓动从当前值过渡到目标值
- **AND** 动画时长默认 300ms，可通过 `animationDuration` 属性配置
- **AND** 动画期间 `valueChanged` 信号持续发射，驱动内容跟随

#### Scenario: 拖拽滑块直接滚动
- **WHEN** 用户拖拽滚动条滑块
- **THEN** value 立即跟随鼠标位置变化，不触发动画（保证拖拽的即时响应感）

#### Scenario: 程序化设置 value
- **WHEN** 代码调用 `setSliderPosition()` 或 `setValue()` 且 `_updating_scroll` 为 true
- **THEN** value 立即设置，不触发动画（避免程序更新滚动条位置时产生不必要的动画）

#### Scenario: 连续滚轮事件加速
- **WHEN** 100ms 内连续收到同方向滚轮事件
- **THEN** 累计计数递增，当 count > 3 时步幅加倍，count > 6 时步幅再加倍
- **AND** 动画时长随加速级别适当延长（模拟 ATK 的惯性滚动加速）

### Requirement: SmoothScrollArea 自定义控件
系统 SHALL 提供 `SmoothScrollArea` 类（继承 `QScrollArea`），具备以下能力：

#### Scenario: Dock 面板滚轮平滑滚动
- **WHEN** 用户在 Dock 面板中滚动鼠标滚轮
- **THEN** 滚动位置通过 `QPropertyAnimation` 以 `OutCubic` 缓动平滑过渡
- **AND** 动画时长 300ms
- **AND** 连续滚轮事件自动合并（新事件到来时从当前位置重新计算目标位置）

#### Scenario: 保持现有 QScrollArea 兼容性
- **WHEN** `SmoothScrollArea` 替换 `QScrollArea` 作为 Dock 基类
- **THEN** 所有现有功能（`setWidgetResizable`, `setHorizontalScrollBarPolicy`, `setWidget` 等）保持不变

### Requirement: View 主视图平滑滚动
系统 SHALL 修改 `View` 类以支持平滑滚动：

#### Scenario: 水平滚动条平滑滚动
- **WHEN** 用户通过滚轮或点击轨道触发水平滚动
- **THEN** `_offset` 通过 `SmoothScrollBar` 的动画逐步变化
- **AND** `h_scroll_value_changed` 在动画期间持续被调用
- **AND** `_ruler->update()` 和 `viewport_update()` 在动画期间持续被调用

#### Scenario: 垂直滚动条平滑滚动
- **WHEN** 用户通过滚轮或点击轨道触发垂直滚动
- **THEN** `_vOffset` 通过 `SmoothScrollBar` 的动画逐步变化
- **AND** `v_scroll_value_changed` 在动画期间持续被调用

#### Scenario: View 的 update_scroll 程序化更新
- **WHEN** `update_scroll()` 被程序调用（如缩放、resize、数据更新后）
- **THEN** 滚动条位置立即设置，不触发动画（使用 `_updating_scroll` 标志）

#### Scenario: View 垂直滚动条 margin-top 样式保留
- **WHEN** View 的垂直滚动条应用了 `margin-top: RulerHeight` 样式
- **THEN** 替换为 SmoothScrollBar 后该样式仍然生效

### Requirement: QTableView 惯性滚动（Flickable）
系统 SHALL 为 SearchDock 和 ProtocolDock 的 QTableView 添加惯性滚动：

#### Scenario: QTableView 触控/滚轮惯性滚动
- **WHEN** 用户在 QTableView 上滚动鼠标滚轮
- **THEN** 通过 `QScroller` 实现平滑减速的惯性滚动效果
- **AND** 滚动方向自动根据内容超出方向决定（等效 `Flickable.AutoFlickDirection`）
- **AND** 边界行为为 `StopAtBounds`（不超出内容范围）

#### Scenario: QScroller 不影响 QTableView 现有交互
- **WHEN** 用户点击/选择表格单元格
- **THEN** QScroller 不干扰正常的鼠标点击和选择操作

### Requirement: 全局滚动条 QSS 样式统一
系统 SHALL 确保所有滚动条视觉风格一致：

#### Scenario: Dark 主题滚动条样式
- **WHEN** 使用 Dark 主题
- **THEN** 所有滚动条（包括 SmoothScrollBar）使用 dark.qss 中定义的样式
- **AND** 滚动条 handle 在 hover 时高亮

#### Scenario: Light 主题滚动条样式
- **WHEN** 使用 Light 主题
- **THEN** 所有滚动条使用 light.qss 中定义的样式
