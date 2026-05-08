# Tasks

- [x] Task 1: 在TitleBar中实现内嵌Ribbon功能
  - [x] SubTask 1.1: 在TitleBar中添加QTabBar成员和工具面板容器（QWidget + QHBoxLayout），替换原有setTabBar逻辑
  - [x] SubTask 1.2: 实现addCategory(const QString &title)方法，创建Tab页并返回对应的QHBoxLayout供添加工具按钮
  - [x] SubTask 1.3: 实现addAction(int categoryIndex, QAction *action)方法，将Action转为QToolButton添加到对应分类面板
  - [x] SubTask 1.4: 实现展开/折叠动画（QPropertyAnimation控制工具面板的maximumHeight）
  - [x] SubTask 1.5: 实现Tab点击切换展开/折叠逻辑（clickTab/onTabChanged）
  - [x] SubTask 1.6: 设置TitleBar和TabBar的样式表，确保明暗主题下hover/selected样式正确
  - [x] SubTask 1.7: 实现retranslateUi()方法更新Tab文本

- [x] Task 2: 修改MainWindow适配新TitleBar Ribbon
  - [x] SubTask 2.1: 移除QRibbon相关成员变量（_QRibbon、_category_file、_category_display、_category_help、_menu_bar）和方法（MainWindowRibbonHelper、Ribbon_setupUi、Ribbon_retranslateUi、setupFileCategory、setupDisplayCategory、setupHelpCategory、setupRightToolBar）
  - [x] SubTask 2.2: 在MainWindow构造函数中，改为通过TitleBar的接口创建Ribbon分类和添加Action
  - [x] SubTask 2.3: 修改switchTheme中的Ribbon_retranslateUi调用为TitleBar的retranslateUi
  - [x] SubTask 2.4: 移除构造函数中QRibbon创建、install、setMenuBar(nullptr)、insertWidget等逻辑

- [x] Task 3: 删除QRibbon文件和编译配置
  - [x] SubTask 3.1: 删除DSView/pv/QRibbon/QRibbon.h、QRibbon.cpp、qribbon.ui、QRibbon.qrc文件
  - [x] SubTask 3.2: 从CMakeLists.txt中移除QRibbon相关源文件和UI文件引用
  - [x] SubTask 3.3: 从mainwindow.h中移除#include "QRibbon/QRibbon.h"

- [x] Task 4: 验证和修复
  - [x] SubTask 4.1: 编译验证无编译错误
  - [x] SubTask 4.2: 验证Ribbon展开/折叠功能正常
  - [x] SubTask 4.3: 验证主题切换不崩溃且样式正确
  - [x] SubTask 4.4: 验证选中Tab hover无深色效果
  - [x] SubTask 4.5: 验证工具按钮点击触发正确的Action

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 2]
- [Task 4] depends on [Task 3]
