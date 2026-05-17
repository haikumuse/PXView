# 拖动卡顿全面修复 Spec

## Why
拖动 SlidingDrawer 边框/整体窗口时出现 300ms 级别的间歇性卡顿帧。调查报告（`doc/drag_stutter_investigation.md`）识别了 8 个根因，从每帧级联 Viewport 全量重绘到 500ms 定时器 USB 轮询竞争 UI 线程。现有优化（异步 `removePushMargin`、`mouseGrabber()` 保护等）仅缓解表面症状，未触及核心架构问题。本 spec 追求最优解，从根本上消除卡顿路径。

## What Changes
- **重构 SlidingDrawer 拖动模式**：拖动期间完全不触碰 layout margin，改为纯 overlay 模式调整 drawer 位置/大小，仅在拖动结束时一次性 apply margin
- **将 `check_update()` 从 `doPaint()` 中移出**：改为独立低频定时器驱动，避免 paint 路径中的同步阻塞
- **Viewport QPixmap 智能缓存**：仅在 size 变化超过阈值或内容真正变化时才重建 QPixmap，resize 时复用已有缓冲区
- **拖动/动画期间暂停后台定时器**：`mode_check_timer` 和 `disk_cache_status_timer` 在拖动/动画期间自动暂停
- **消除 `setSlideOffset()` 的 `pw->update(dirtyRect)` 级联重绘**：动画期间用 `QWidget::scroll()` 替代 `update(dirtyRect)`，或直接取消父 widget 的脏区域标记
- **优化 `resizeEvent` 中的冗余 `raise()` 调用**：仅在首次布局时 raise，后续 resize 跳过
- **消除开/关动画的双重 margin 变更**：统一为 overlay→push 单次切换
- **消除拖动开始时的异步 `removePushMargin`**：拖动全程使用 overlay 模式，不再需要 removePushMargin

## Impact
- Affected code: `slidingdrawer.cpp/h`, `viewport.cpp/h`, `deviceoptionsdock.cpp/h`, `mainwindow.cpp/h`, `sigsession.cpp/h`
- Affected specs: `optimize-main-thread-perf`（部分重叠，本 spec 是更彻底的修复）
- **BREAKING**: `SlidingDrawer` 拖动行为从"拖动时 remove margin + overlay"改为"全程 overlay + 结束时 apply margin"

## ADDED Requirements

### Requirement: SlidingDrawer 拖动全程 Overlay 模式
系统 SHALL 在拖动期间完全不触碰 `_push_layout` 的 margin，仅通过 `setGeometry()` + `setFixedSize()` 调整 drawer 自身位置和大小。

#### Scenario: 拖动期间不触发 layout reflow
- **WHEN** 用户拖动 SlidingDrawer 边缘调整宽度
- **THEN** `_push_layout->setContentsMargins()` 在整个拖动过程中不被调用
- **AND** drawer 始终作为 overlay 定位在父 widget 右侧
- **AND** `_tab_widget` 的尺寸在拖动期间不变

#### Scenario: 拖动结束时一次性 apply margin
- **WHEN** 用户释放鼠标结束拖动
- **THEN** `applyPushMargin()` 被调用一次，设置 right margin = 新的 `_drawer_width`
- **AND** drawer 位置调整为 margin 区域内（`_slide_offset = 0`）
- **AND** `_tab_widget` 一次性 resize 到最终尺寸

#### Scenario: 拖动期间 drawer 实时调整大小
- **WHEN** 用户拖动边缘
- **THEN** `setFixedSize(new_width, parentHeight)` 和 `positionOverlay()` 每帧被调用
- **AND** drawer 视觉上跟随鼠标实时移动
- **AND** 不触发 Viewport 的 resize/paint

### Requirement: `check_update()` 从 paint 路径中移出
系统 SHALL 将 `SigSession::check_update()` 从 `Viewport::doPaint()` 中移出，改为独立定时器驱动。

#### Scenario: paint 期间不执行 check_update
- **WHEN** `Viewport::doPaint()` 被调用
- **THEN** 不再调用 `_view.session().check_update()`
- **AND** paint 路径仅包含渲染逻辑

#### Scenario: check_update 由独立定时器驱动
- **WHEN** Viewport 可见且设备正在采集
- **THEN** 一个独立的 `QTimer`（间隔 50-100ms）定期调用 `check_update()`
- **AND** `check_update()` 的结果通过信号通知 Viewport 更新

#### Scenario: Viewport 不可见时停止定时器
- **WHEN** Viewport 被隐藏或其所在 tab 被切换走
- **THEN** check_update 定时器停止
- **AND** Viewport 重新可见时定时器恢复

### Requirement: Viewport QPixmap 智能缓存
系统 SHALL 优化 Viewport 的 QPixmap 分配策略，避免每次 resize 都重建全尺寸缓冲区。

#### Scenario: resize 时复用已有 QPixmap
- **WHEN** Viewport 的 size 发生变化但变化量小于 4 像素
- **THEN** 复用已有的 `_pixmap`，不重新分配
- **AND** 绘制时使用 `QPainter::setClipRect()` 裁剪到实际区域

#### Scenario: size 变化超过阈值时才重建
- **WHEN** Viewport 的 size 变化超过 4 像素
- **THEN** 重建 `_pixmap = QPixmap(size())`
- **AND** 正常执行全量重绘

#### Scenario: 内容未变化时跳过重绘
- **WHEN** `paintSignals()` 被调用但 scale/offset/signalHeight/vOffset/need_update 均未变化
- **THEN** 直接绘制缓存的 `_pixmap`，不遍历 trace 重绘
- **AND** 此路径已在现有代码中实现（`_curScale` 等缓存变量），仅需确保 resize 时不强制清空

### Requirement: 拖动/动画期间暂停后台定时器
系统 SHALL 在 SlidingDrawer 拖动或动画期间暂停 `mode_check_timer` 和 `disk_cache_status_timer`。

#### Scenario: 拖动开始时暂停后台定时器
- **WHEN** SlidingDrawer 的 `_drag_active` 变为 true
- **THEN** `DeviceOptionsDock::_mode_check_timer` 停止
- **AND** `MainWindow::_disk_cache_status_timer` 停止

#### Scenario: 拖动结束后恢复后台定时器
- **WHEN** SlidingDrawer 的 `_drag_active` 变为 false（finishDrag）
- **THEN** `DeviceOptionsDock::_mode_check_timer` 恢复（如果 dock 可见）
- **AND** `MainWindow::_disk_cache_status_timer` 恢复

#### Scenario: 动画期间暂停后台定时器
- **WHEN** SlidingDrawer 的 `_is_animating` 变为 true
- **THEN** 同样暂停后台定时器
- **AND** 动画结束后恢复

### Requirement: 消除 `setSlideOffset()` 的父 widget 脏区域更新
系统 SHALL 在动画期间消除 `setSlideOffset()` 中对父 widget 的 `update(dirtyRect)` 调用。

#### Scenario: 动画期间不标记父 widget 脏区域
- **WHEN** `setSlideOffset()` 在动画期间被调用
- **THEN** 不对 `parentWidget()` 调用 `update(dirtyRect)`
- **AND** 仅通过 `positionOverlay()` 移动 drawer 自身位置
- **AND** Qt 的 WA_OpaquePaintEvent 属性确保 drawer 移动后旧区域被正确重绘

### Requirement: 优化 `resizeEvent` 中的冗余 raise() 调用
系统 SHALL 在 SlidingDrawer 的 `resizeEvent` 中减少冗余的 `raise()` 调用。

#### Scenario: 仅首次 resize 时 raise
- **WHEN** SlidingDrawer 的 `resizeEvent` 被调用
- **THEN** `_edge_grip` 和 `_left_separator` 的 `setGeometry()` 正常执行
- **AND** `raise()` 仅在 widget 首次被显示时调用（通过 bool 标志控制）
- **AND** 后续 resize 不再调用 `raise()`

### Requirement: 统一开/关动画的 margin 切换
系统 SHALL 消除开/关动画期间的双重 margin 变更。

#### Scenario: 打开动画全程 overlay
- **WHEN** `SlidingDrawer::open()` 被调用
- **THEN** 不调用 `removePushMargin()`（因为关闭时已经是 margin=0 或 overlay 状态）
- **AND** 动画结束后调用 `applyPushMargin()` 一次
- **AND** 仅触发一次 layout reflow

#### Scenario: 关闭动画先 apply 再 overlay
- **WHEN** `SlidingDrawer::close()` 被调用且当前处于 push 模式
- **THEN** 先 `removePushMargin()` 一次（tab 扩展到全宽）
- **AND** 然后开始 overlay 动画
- **AND** 总共只触发一次 layout reflow

### Requirement: 拖动开始时不再需要异步 removePushMargin
系统 SHALL 在拖动开始时直接进入 overlay 模式，无需异步 margin 操作。

#### Scenario: 拖动开始时直接 overlay
- **WHEN** 用户开始拖动 drawer 边缘
- **THEN** 不调用 `removePushMargin()`（因为 drawer 已在 overlay 位置）
- **AND** 不使用 `QTimer::singleShot(0, ...)` 异步操作
- **AND** 拖动开始无卡顿

## MODIFIED Requirements

### Requirement: SlidingDrawer 打开/关闭流程
原流程：open → removePushMargin → 动画 → applyPushMargin；close → removePushMargin → 动画 → hide
新流程：open → 动画(overlay) → applyPushMargin；close → removePushMargin → 动画(overlay) → hide

## REMOVED Requirements

### Requirement: 拖动期间异步 removePushMargin
**Reason**: 拖动全程使用 overlay 模式，不再需要 removePushMargin
**Migration**: 删除 `_drag_margin_removed` 标志和 `QTimer::singleShot(0, ...)` 逻辑
