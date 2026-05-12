# 侧边栏推挤模式（SideBar Push Mode）Spec

## Why
当前侧边栏（SlidingDrawer）展开时采用覆盖模式（overlay），遮挡主视图内容。用户希望改为推挤模式（push mode），即侧边栏展开时主视图自动缩小相应宽度，类似 atk-logic-master 项目的 Session.qml 实现。这样可以：
1. 保持主视图内容始终可见
2. 提升用户体验，避免内容被遮挡
3. 与参考项目的交互模式保持一致

## What Changes
- 修改 `SlidingDrawer` 类，添加 `drawerWidthChanged(int width)` 信号，在抽屉宽度变化时发射
- 修改 `SlidingDrawer::updatePanelGeometry()`，使其定位在父容器右侧内部，不覆盖左侧内容
- 修改 `MainWindow`，连接 `SlidingDrawer::drawerWidthChanged` 信号，动态调整 `_tab_widget` 的右边距
- 修改 `MainWindow::setup_ui()`，为 `_tab_widget` 设置初始布局，预留侧边栏空间
- 确保动画过程中主视图宽度平滑过渡
- **BREAKING**: `SlidingDrawer` 的覆盖行为改变，从 overlay 变为 push 模式

## Impact
- Affected specs: `create-unified-sidebar-component`（SlidingDrawer 行为变更）
- Affected code:
  - `pv/widgets/slidingdrawer.h`（修改：添加信号声明）
  - `pv/widgets/slidingdrawer.cpp`（修改：updatePanelGeometry 逻辑）
  - `pv/mainwindow.h`（修改：添加槽函数声明）
  - `pv/mainwindow.cpp`（修改：连接信号，调整 tab_widget 布局）

## ADDED Requirements

### Requirement: SlidingDrawer 推挤模式支持
SlidingDrawer SHALL 支持推挤模式，展开时不覆盖主视图，而是将主视图向左推。

#### Scenario: 抽屉打开时推挤主视图
- **WHEN** 用户点击侧边栏按钮打开抽屉
- **THEN** SlidingDrawer 从右侧滑入
- **AND** 发射 `drawerWidthChanged(int width)` 信号，通知主视图调整宽度
- **AND** 主视图宽度自动缩小相应宽度

#### Scenario: 抽屉关闭时恢复主视图
- **WHEN** 用户点击已选中的按钮关闭抽屉
- **THEN** SlidingDrawer 向右侧滑出
- **AND** 发射 `drawerWidthChanged(0)` 信号
- **AND** 主视图宽度恢复为全宽

#### Scenario: 抽屉宽度调整时同步更新
- **WHEN** 用户拖拽抽屉左边缘调整宽度
- **THEN** SlidingDrawer 实时发射 `drawerWidthChanged(int width)` 信号
- **AND** 主视图宽度实时同步调整

### Requirement: MainWindow 响应抽屉宽度变化
MainWindow SHALL 响应 SlidingDrawer 的宽度变化信号，动态调整主视图区域大小。

#### Scenario: 连接抽屉宽度信号
- **WHEN** MainWindow 初始化时
- **THEN** 连接 `SlidingDrawer::drawerWidthChanged` 信号到槽函数
- **AND** 槽函数调整 `_tab_widget` 的 `contentsMargins` 或 `geometry`

#### Scenario: 动画过程中平滑过渡
- **WHEN** 抽屉打开/关闭动画过程中
- **THEN** `_tab_widget` 的右边距平滑变化
- **AND** 无闪烁或跳跃现象

### Requirement: 抽屉页面切换时保持宽度
SlidingDrawer SHALL 在同一抽屉内切换页面时保持当前宽度，不重复动画。

#### Scenario: 切换抽屉页面
- **WHEN** 抽屉已打开，用户点击另一个 dock 按钮
- **THEN** 抽屉内容切换到新页面
- **AND** 抽屉宽度保持不变
- **AND** 不发射额外的 `drawerWidthChanged` 信号

## MODIFIED Requirements

### Requirement: SlidingDrawer 定位逻辑
SlidingDrawer 的定位逻辑从覆盖模式改为推挤模式。

原有行为：
```cpp
// 覆盖模式：抽屉覆盖在父容器上
int panel_x = parent_w - qRound(_drawer_width * _slide_progress);
setGeometry(panel_x, 0, _drawer_width, parent_h);
```

新行为：
```cpp
// 推挤模式：抽屉定位在父容器右侧内部
int visible_width = qRound(_drawer_width * _slide_progress);
int panel_x = parent_w - visible_width;
setGeometry(panel_x, 0, visible_width, parent_h);
emit drawerWidthChanged(visible_width);
```

### Requirement: MainWindow 布局管理
MainWindow 的布局管理需要适配推挤模式。

原有行为：
```cpp
// 抽屉覆盖在主视图上，无需调整主视图大小
_sliding_drawer->raise();
```

新行为：
```cpp
// 抽屉推挤主视图，需要动态调整 tab_widget 的边距
connect(_sliding_drawer, &SlidingDrawer::drawerWidthChanged, [this](int width) {
    _tab_widget->setContentsMargins(0, 0, width, 0);
});
```

## REMOVED Requirements

### Requirement: SlidingDrawer 覆盖层模式
**Reason**: 推挤模式替代覆盖模式，主视图内容始终可见。
**Migration**: 如需恢复覆盖模式，可通过配置选项切换，但默认使用推挤模式。
