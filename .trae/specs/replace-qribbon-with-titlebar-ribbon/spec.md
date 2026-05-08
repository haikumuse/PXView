# 删除QRibbon并在标题栏重写Ribbon Spec

## Why
当前QRibbon继承自QMenuBar，通过.ui文件定义布局，存在大量样式表冲突、间距异常、hover效果错误、主题切换崩溃等bug。其架构将TabBar从QTabWidget中提取到TitleBar，导致样式表优先级混乱。需要在TitleBar中直接重写一个简洁的Ribbon实现，彻底消除这些bug。

## What Changes
- **删除** QRibbon类（QRibbon.h、QRibbon.cpp、qribbon.ui、QRibbon.qrc）
- **删除** CMakeLists.txt中QRibbon相关的源文件和UI文件引用
- **删除** MainWindow中所有QRibbon相关的成员变量和方法（`_QRibbon`、`MainWindowRibbonHelper()`、`Ribbon_setupUi()`、`Ribbon_retranslateUi()`、`setupFileCategory()`、`setupDisplayCategory()`、`setupHelpCategory()`、`setupRightToolBar()`）
- **重写** TitleBar，使其内嵌QTabBar + 可折叠的工具按钮面板，实现Ribbon功能
- **修改** MainWindow构造函数，移除QRibbon创建和安装逻辑，改为使用TitleBar内置的Ribbon
- **保留** 原有的菜单Action创建逻辑（File/Settings/Help分类），但改为由TitleBar管理

## Impact
- Affected code: QRibbon目录全部文件、titlebar.h/cpp、mainwindow.h/cpp、CMakeLists.txt
- Affected specs: 无其他spec依赖QRibbon

## ADDED Requirements

### Requirement: TitleBar内嵌Ribbon
系统SHALL在TitleBar中直接集成Ribbon功能，包括TabBar标签页和可折叠的工具按钮面板。

#### Scenario: 正常显示
- **WHEN** 应用启动完成
- **THEN** TitleBar中显示Tab标签（File/Settings/Help），点击Tab展开对应工具按钮面板，再次点击折叠

#### Scenario: Tab切换
- **WHEN** 用户点击不同的Tab标签
- **THEN** 工具按钮面板切换到对应分类，面板自动展开

#### Scenario: 主题切换
- **WHEN** 用户切换明暗主题
- **THEN** Ribbon的样式正确跟随主题变化，不崩溃、不出现灰色hover

### Requirement: TitleBar Ribbon样式
系统SHALL确保Ribbon的样式在明暗主题下均正确显示。

#### Scenario: 选中Tab hover
- **WHEN** 鼠标悬停在已选中的Tab上
- **THEN** Tab保持选中状态的背景色，不出现深色/灰色hover效果

#### Scenario: 未选中Tab hover
- **WHEN** 鼠标悬停在未选中的Tab上
- **THEN** Tab显示轻微高亮效果

#### Scenario: 间距
- **WHEN** Ribbon显示时
- **THEN** TabBar与工具按钮面板之间无多余间距，TabBar与标题栏左侧无过大间距

### Requirement: 工具按钮面板
系统SHALL将原有菜单Action转换为QToolButton显示在Ribbon面板中。

#### Scenario: Action显示
- **WHEN** Ribbon面板展开
- **THEN** 对应分类的所有Action以图标+文字的QToolButton形式显示，无图标的Action使用默认齿轮图标

#### Scenario: Action交互
- **WHEN** 用户点击Ribbon中的工具按钮
- **THEN** 对应的Action被触发，效果与原菜单一致

## REMOVED Requirements

### Requirement: QRibbon类
**Reason**: QRibbon继承QMenuBar的架构导致样式冲突和bug，被TitleBar内置Ribbon替代
**Migration**: 所有QRibbon功能迁移到TitleBar中实现
