# 修复多标签页功能 Bug 和完善功能 Spec

## Why
多标签页功能的初始实现存在两个关键 Bug：拖拽标签出窗口时创建两个空白窗口、"+"按钮不可见。此外还有多项功能不完善：capture_snapshot 只复制元数据不复制波形数据、标签关闭按钮行为不一致、浮动窗口关闭后无法还原标签、SamplingBar 上下文切换未在标签切换时调用等。

## What Changes
- 修复 `on_tab_detach` 双窗口 Bug：移除 MainWindow 中重复创建浮动窗口的代码，DraggableTabWidget 已负责创建
- 修复 "+" 按钮不可见问题：改用 `setCornerWidget` 的正确方式，或改用 QTabBar 的 tabButton 机制
- 完善 `capture_snapshot()`：实现波形数据的深拷贝（Logic/Analog/Dso Snapshot）
- 修复 `onTabCloseRequested`：不应直接删除 widget，应通知 MainWindow 执行 remove_tab 逻辑
- 实现浮动窗口关闭后还原标签
- 修复 `on_tab_changed` 中 SamplingBar 上下文切换未调用 `set_context`
- 修复标签标题更新时 TabContext 标题不同步
- 修复 `on_load_file` 中新标签页未正确设置 live 状态

## Impact
- Affected specs: add-multi-tab-sessions
- Affected code:
  - `DSView/pv/ui/draggabletabwidget.cpp`（修复 detach 逻辑、关闭按钮逻辑、"+"按钮可见性）
  - `DSView/pv/ui/draggabletabwidget.h`（新增信号）
  - `DSView/pv/ui/draggabletabbar.cpp`（修复拖拽检测）
  - `DSView/pv/mainwindow.cpp`（修复 on_tab_detach、on_tab_changed、标签关闭逻辑）
  - `DSView/pv/mainwindow.h`（新增槽函数）
  - `DSView/pv/sigsession.cpp`（完善 capture_snapshot 数据拷贝）
  - `DSView/pv/data/sessionsnapshot.cpp`（完善数据拷贝方法）
  - `DSView/pv/data/sessionsnapshot.h`（新增深拷贝方法）
  - `DSView/pv/tabcontext.cpp`（修复 deactivate 逻辑）

## ADDED Requirements

### Requirement: 拖出标签只创建一个窗口
拖拽标签出标签栏时，只应创建一个包含 View 的浮动窗口，不应出现两个窗口。

#### Scenario: 拖拽标签出窗口
- **WHEN** 用户将标签拖出标签栏区域
- **THEN** 只创建一个浮动窗口
- **AND** 该窗口包含被拖出标签的 View
- **AND** MainWindow 的 on_tab_detach 只负责从 _tab_contexts 中移除该 TabContext 和更新索引
- **AND** 不再重复创建浮动窗口

### Requirement: "+" 按钮始终可见
标签栏右侧的 "+" 按钮应始终可见且可点击。

#### Scenario: "+" 按钮显示
- **WHEN** 应用启动后
- **THEN** 标签栏最右侧显示 "+" 按钮
- **AND** 点击 "+" 按钮创建新的空白采集标签页

#### Scenario: "+" 按钮样式
- **WHEN** 应用使用深色主题
- **THEN** "+" 按钮仍然可见，文字颜色与标签文字一致

### Requirement: 标签关闭按钮通知 MainWindow
点击标签上的关闭按钮（×）或右键菜单的"关闭"时，应通知 MainWindow 执行 remove_tab 逻辑，而非直接删除 widget。

#### Scenario: 点击标签关闭按钮
- **WHEN** 用户点击标签上的关闭按钮（×）
- **THEN** DraggableTabWidget 发射 tabCloseRequested(int index) 信号
- **AND** MainWindow 接收信号后执行 remove_tab(index)
- **AND** remove_tab 处理停止采集、创建新标签等逻辑

### Requirement: 浮动窗口关闭后还原标签
当浮动窗口被关闭时，应将标签还原回 DraggableTabWidget。

#### Scenario: 关闭浮动窗口
- **WHEN** 用户关闭浮动窗口
- **THEN** 浮动窗口中的 View 被重新添加为 DraggableTabWidget 的标签
- **AND** 标签标题保持不变
- **AND** 如果原 TabContext 仍存在，重新关联；否则创建新 TabContext

### Requirement: capture_snapshot 完整数据拷贝
capture_snapshot 应完整拷贝波形数据，而不仅仅是元数据。

#### Scenario: 拍摄快照包含波形数据
- **WHEN** 调用 SigSession::capture_snapshot()
- **THEN** 返回的 SessionSnapshot 包含当前 LogicSnapshot、AnalogSnapshot、DsoSnapshot 的完整数据副本
- **AND** 快照数据与原始数据完全独立，修改互不影响

### Requirement: 标签切换时 SamplingBar 完整上下文切换
标签切换时，SamplingBar 应完整更新上下文，包括 set_context 和 set_readonly。

#### Scenario: 切换到历史标签
- **WHEN** 用户切换到历史数据标签
- **THEN** SamplingBar 调用 set_context 更新 session 和 view
- **AND** SamplingBar 调用 set_readonly(true) 禁用采集控制

#### Scenario: 切换到活跃采集标签
- **WHEN** 用户切换到活跃采集标签
- **THEN** SamplingBar 调用 set_context 更新 session 和 view
- **AND** SamplingBar 调用 set_readonly(false) 启用采集控制

### Requirement: 标签标题与 TabContext 同步
当通过内联编辑器重命名标签时，TabContext 的标题应同步更新。

#### Scenario: 内联重命名标签
- **WHEN** 用户通过双击或右键菜单重命名标签
- **THEN** DraggableTabWidget 发射 tabRenamed(int index, QString title) 信号
- **AND** MainWindow 接收信号后更新对应 TabContext 的标题

## MODIFIED Requirements

### Requirement: MainWindow::on_tab_detach 不再创建浮动窗口
on_tab_detach 只负责从 _tab_contexts 中移除 TabContext 和更新索引，浮动窗口的创建由 DraggableTabWidget::onDetachTab 独立完成。

### Requirement: DraggableTabWidget::onTabCloseRequested 不再直接删除 widget
onTabCloseRequested 改为发射 tabCloseRequested(int index) 信号，由 MainWindow 的 remove_tab 处理完整逻辑。

## REMOVED Requirements

### Requirement: MainWindow::on_tab_detach 中创建浮动窗口
**Reason**: DraggableTabWidget::onDetachTab 已经创建了浮动窗口，on_tab_detach 中重复创建导致双窗口。
**Migration**: on_tab_detach 只执行 TabContext 清理逻辑。
