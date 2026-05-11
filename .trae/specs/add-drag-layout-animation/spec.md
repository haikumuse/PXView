# 拖拽交互体验优化 Spec

## Why
当前 Header 中拖拽通道重排后，所有 trace 瞬间跳到新位置，视觉上生硬突兀。参考 ATK 项目中 QML ListView 的 `moveDisplaced` 过渡动画（150ms 线性滑动）和拖拽释放后的 `backAnim` 回弹动画（100ms OutCubic），为 PXView 的拖拽释放和通道增删场景添加平滑过渡动画。

## What Changes
- 在 `View` 中新增基于 QTimer 的布局动画系统，当 trace 的 `v_offset` 发生变化时，以插值动画平滑过渡到目标位置
- 修改 `Header::mouseReleaseEvent`，拖拽释放后启动布局动画而非直接调用 `signals_changed` 重算
- 修改 `Viewport::paintEvent` 和 `Header::paintEvent`，在动画期间读取 trace 的动画中间 `v_offset` 值进行绘制
- 在 `Trace` 中新增动画状态属性（`_anim_v_offset`、`_animating`），用于存储动画中间值
- 动画期间禁用 DSO/ANALOG 模式的 QPixmap 缓存，确保每帧都能正确绘制

## Impact
- Affected code: `view.h/cpp`、`header.h/cpp`、`viewport.h/cpp`、`trace.h/cpp`

## ADDED Requirements

### Requirement: 拖拽释放后的布局过渡动画
当用户在 Header 中拖拽通道并释放后，系统 SHALL 以 150ms 的线性插值动画将所有 trace 从旧位置平滑滑动到新位置，而非瞬间跳转。

#### Scenario: 拖拽释放后通道平滑滑动
- **WHEN** 用户在 Header 中拖拽一个通道到新位置并释放鼠标
- **THEN** 所有受影响的 trace 以 150ms 线性动画从旧 v_offset 滑动到新 v_offset
- **AND** 动画期间 Header 和 Viewport 同步更新绘制

#### Scenario: 动画期间用户再次拖拽
- **WHEN** 布局动画正在播放时用户按下鼠标开始新的拖拽
- **THEN** 当前动画立即停止，trace 停在当前中间位置，从该位置开始响应新的拖拽

### Requirement: 通道增删后的布局过渡动画
当通道被添加或删除导致其他通道位置变化时，系统 SHALL 以 150ms 线性插值动画平滑过渡。

#### Scenario: 协议解码通道添加
- **WHEN** 新的协议解码通道被添加，导致其他通道位置调整
- **THEN** 受影响的 trace 以 150ms 线性动画滑动到新位置

#### Scenario: 通道被禁用/隐藏
- **WHEN** 通道被禁用或隐藏，其他通道位置上移
- **THEN** 受影响的 trace 以 150ms 线性动画滑动到新位置

### Requirement: 动画参数
- 动画时长：150ms
- 缓动曲线：线性（与 ATK 的 moveDisplaced 一致）
- 帧率：约 60fps（QTimer 间隔 ~16ms）
- 动画总帧数：约 9-10 帧

### Requirement: 动画与绘制系统的协调
- 动画期间，DSO/ANALOG 模式的 QPixmap 缓存 SHALL 被禁用（标记 `_need_update = true`），确保每帧都重新绘制
- 动画结束后，Pixmap 缓存恢复正常工作
- 动画期间，Header 和 Viewport 的 `paintEvent` SHALL 读取 trace 的动画中间 `v_offset` 值（`get_anim_v_offset()`），而非最终目标值

### Requirement: 动画与滚动条同步
- 动画期间，View 的滚动条位置 SHALL 保持不变
- 动画结束后，调用 `normalize_layout()` 确保所有 v_offset 为正值
