# 拖拽交互体验优化 Spec

## Why
PXView 的 Header 拖拽重排通道时，通道位置瞬间跳变，没有视觉过渡，用户无法预判松手后通道的最终排列位置。ATK 项目通过 QML ListView 内置的 move/moveDisplaced 过渡动画和拖拽回弹动画，提供了流畅的拖拽体验。PXView 需要在纯 C++ Qt Widgets 架构下实现类似效果。

## What Changes
- 在 Header 拖拽过程中绘制**目标位置占位指示线**，让用户预判松手后通道排列位置
- 在拖拽释放后，对被移动的 trace 和被挤开的 trace 播放 **v_offset 平滑过渡动画**（~120ms OutCubic 缓动）
- 在 Header 的 paintEvent 中增加拖拽状态的视觉反馈（拖拽中的 trace 半透明、目标位置高亮线）

## Impact
- Affected code: `pv/view/header.h`, `pv/view/header.cpp`, `pv/view/trace.h`, `pv/view/trace.cpp`, `pv/view/view.h`, `pv/view/view.cpp`, `pv/view/viewport.h`, `pv/view/viewport.cpp`

## ADDED Requirements

### Requirement: 拖拽占位指示线
系统 SHALL 在 Header 拖拽过程中，在目标插入位置绘制一条水平高亮指示线，提示用户松手后通道将排列到该位置。

#### Scenario: 拖拽 Logic 通道时显示占位线
- **WHEN** 用户在 Header 中按住一个 Logic 通道标签并上下拖动
- **THEN** 在该通道即将插入的位置显示一条水平高亮线，指示目标位置

#### Scenario: 拖拽 DSO/Analog 通道时不显示占位线
- **WHEN** 用户拖拽 DSO 或 Analog 通道（这些通道是调整零点位置而非重排）
- **THEN** 不显示占位指示线，行为与当前一致

### Requirement: 拖拽释放后平滑过渡动画
系统 SHALL 在拖拽释放后，对所有受影响的 trace 的 v_offset 播放平滑过渡动画，而非瞬间跳变。

#### Scenario: 通道重排后的平滑过渡
- **WHEN** 用户拖拽释放一个 Logic 通道，导致多个通道位置变化
- **THEN** 被拖拽的通道和被挤开的通道都以 ~120ms OutCubic 缓动平滑滑动到新位置

#### Scenario: 动画期间的用户操作
- **WHEN** 过渡动画正在播放时用户发起新的拖拽操作
- **THEN** 立即停止当前动画，将所有 trace 设为动画目标位置，然后响应新的拖拽操作

#### Scenario: DSO/Analog 通道拖拽不触发过渡动画
- **WHEN** 用户拖拽释放 DSO 或 Analog 通道（仅调整零点位置）
- **THEN** 不触发过渡动画，行为与当前一致

### Requirement: 拖拽中的视觉反馈
系统 SHALL 在拖拽过程中对被拖拽的 trace 提供视觉区分。

#### Scenario: 拖拽中的 trace 半透明显示
- **WHEN** 用户正在拖拽一个 trace
- **THEN** 该 trace 在 Header 和 Viewport 中以降低的透明度（~60%）绘制，与静止通道形成视觉区分

## MODIFIED Requirements

### Requirement: Header::mouseMoveEvent 拖拽逻辑
当前实现中，拖拽时直接修改 trace 的 v_offset（snap 到网格），释放时调用 signals_changed() 重算布局。修改为：拖拽时记录目标插入位置但不立即修改其他 trace 的 v_offset，仅在 Header 中绘制占位指示线；释放时计算最终布局并启动过渡动画。

### Requirement: Header::mouseReleaseEvent 释放逻辑
当前实现中，释放时直接调用 signals_changed() + normalize_layout() 导致瞬间跳变。修改为：释放时计算所有 trace 的目标 v_offset，然后启动 QTimer 驱动的插值动画，在 ~120ms 内平滑过渡到目标位置。
