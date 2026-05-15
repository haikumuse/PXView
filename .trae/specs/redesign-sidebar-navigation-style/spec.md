# 侧边栏 NavigationBar 风格重构 Spec

## Why
当前侧边栏使用标准 `XToolButton`（QToolButton），缺乏选中状态的视觉反馈（无左侧指示条），图标风格陈旧（Visio 导出的复杂 SVG）。参考 Qt-Fluent-Widgets 的 `NavigationBar` 组件，重构为自定义绘制的侧边栏按钮，增加左侧选中指示条，替换为 Lucide 风格的简洁图标，并实现开始/立即按钮在采集运行时自动切换为停止状态。

## What Changes
- 新增 `SideBarButton` 自定义控件，替代 `XToolButton`，自绘背景、左侧指示条、图标、文字
- 替换所有侧边栏图标为 Lucide 风格 SVG（触发=闪电、解码=二进制、测量=直尺、搜索=搜索、选项=滑块）
- 开始按钮使用荧光绿 #00E676 的播放图标，按下后切换为红色 #e74c3c 的实心停止图标，采集完成后恢复
- 立即按钮使用暖黄 #FFC400 的步进图标，按下后切换为红色 #e74c3c 的实心停止图标，采集完成后恢复
- 侧边栏宽度收窄，按钮尺寸调整为 64×58（与 NavigationBar 一致）
- 更新 QSS 样式适配新控件

## Impact
- Affected specs: create-unified-sidebar-component（SideBar 类 API 不变，内部实现变更）
- Affected code:
  - `pv/widgets/sidebar.h` / `sidebar.cpp` — 核心重构
  - `pv/widgets/sidebarbutton.h` / `sidebarbutton.cpp` — 新增文件
  - `pv/mainwindow.h` / `mainwindow.cpp` — 图标切换逻辑
  - `PXView/icons/dark/*.svg` / `PXView/icons/light/*.svg` — 新图标文件
  - `PXView.qrc` — 注册新图标
  - `themes/dark.qss` / `themes/light.qss` — 样式更新

## ADDED Requirements

### Requirement: SideBarButton 自定义绘制控件
系统 SHALL 提供 `SideBarButton` 控件，继承自 `QWidget`，自绘以下元素：
- 背景：悬停时半透明矩形，选中时高亮背景
- 左侧指示条：选中时在左侧绘制 4px 宽圆角矩形（正常 `QRectF(0,16,4,24)`，按下 `QRectF(0,19,4,18)`）
- 图标：居中绘制在按钮上方区域 `QRectF(22,13,20,20)`
- 文字：居中绘制在按钮下方区域 `QRect(0,32,width(),26)`
- 按钮固定尺寸 64×58
- 支持 `setRunning(bool)` 方法：运行时切换为备用图标（alternateIcon），停止时恢复原始图标

#### Scenario: DockItem 选中状态
- **WHEN** 用户点击一个 DockItem 按钮
- **THEN** 该按钮显示左侧指示条和高亮背景，其他 DockItem 按钮取消选中

#### Scenario: ActionItem 点击
- **WHEN** 用户点击一个 ActionItem 按钮
- **THEN** 按钮触发 actionItemClicked 信号，无选中状态变化

#### Scenario: ActionItem 运行状态切换
- **WHEN** 调用 `setRunning(true)`
- **THEN** 按钮图标切换为 alternateIcon，文字保持不变
- **WHEN** 调用 `setRunning(false)`
- **THEN** 按钮图标恢复为原始 icon，文字保持不变

### Requirement: Lucide 风格 SVG 图标
系统 SHALL 为侧边栏提供以下 Lucide 风格 SVG 图标（dark/light 两套）：

| 项目 | 图标名 | SVG 路径描述 |
|------|--------|-------------|
| 触发 | zap | 闪电形 path |
| 解码 | binary | 两个圆角矩形 + 连接线 |
| 测量 | ruler | 直尺形 path + 刻度线 |
| 搜索 | search | 圆圈 + 斜线 |
| 选项 | sliders | 三条竖线 + 横向调节点 |
| 开始 | play | 三角形播放 path，填充 #00E676 |
| 立即 | step-forward | 三角形 + 竖线，填充 #FFC400 |
| 停止 | stop | 实心圆角矩形，填充 #e74c3c |

#### Scenario: 主题切换
- **WHEN** 用户切换深色/浅色主题
- **THEN** DockItem 图标颜色随主题变化（深色主题用浅色描边，浅色主题用深色描边），ActionItem 图标保持固定颜色

### Requirement: 开始/立即按钮运行时切换为停止状态
系统 SHALL 在开始或立即按钮被按下后，将该按钮的图标切换为停止图标（红色实心方块 #e74c3c），采集完成后自动恢复为原始图标。

#### Scenario: 点击开始按钮
- **WHEN** 用户点击开始按钮
- **THEN** 系统开始采集，开始按钮图标从 play（#00E676）切换为 stop（#e74c3c），文字不变
- **WHEN** 采集完成或用户再次点击该按钮
- **THEN** 系统停止采集，按钮图标恢复为 play（#00E676）

#### Scenario: 点击立即按钮
- **WHEN** 用户点击立即按钮
- **THEN** 系统开始单次采集，立即按钮图标从 step-forward（#FFC400）切换为 stop（#e74c3c），文字不变
- **WHEN** 采集完成或用户再次点击该按钮
- **THEN** 系统停止采集，按钮图标恢复为 step-forward（#FFC400）

#### Scenario: 运行中再次点击
- **WHEN** 开始/立即按钮处于运行状态（显示停止图标），用户点击该按钮
- **THEN** 系统停止采集，按钮恢复为原始图标

## MODIFIED Requirements

### Requirement: SideBar 使用 SideBarButton
SideBar 类 SHALL 使用 `SideBarButton` 替代 `XToolButton` 作为内部按钮控件。ItemInfo 结构中 `button` 字段类型从 `XToolButton*` 变为 `SideBarButton*`。公共 API（addItem、setItemVisible、setItemEnabled、setItemChecked、clearAllChecked、getItem）保持不变。新增 `setItemRunning(int index, bool running)` 方法用于切换按钮运行状态。

### Requirement: setupSideBar 图标更新
MainWindow::setupSideBar() SHALL 使用新图标名调用 addItem：
- 触发: "zap.svg"
- 解码: "binary.svg"
- 测量: "ruler.svg"
- 搜索: "search.svg"（替代 "search-bar.svg"）
- 选项: "sliders.svg"（替代 "params.svg"）
- 开始: "play.svg"（替代 "start.svg"），alternateIcon 为 "stop.svg"
- 立即: "step-forward.svg"（替代 "single.svg"），alternateIcon 为 "stop.svg"

### Requirement: MainWindow 采集状态联动
MainWindow SHALL 在采集状态变化时调用 SideBar::setItemRunning() 切换开始/立即按钮的图标状态：
- 采集开始时：对应按钮 setRunning(true)
- 采集结束时：对应按钮 setRunning(false)
- on_side_bar_action_clicked() 中：SIDEBAR_RUNSTOP 和 SIDEBAR_INSTANT 需判断当前是否运行中，若运行中则执行停止操作

## REMOVED Requirements

### Requirement: 独立停止按钮
**Reason**: 改为开始/立即按钮自身切换为停止状态，无需独立停止按钮
**Migration**: 不再需要 SIDEBAR_STOP 枚举值和独立的停止按钮 addItem 调用
