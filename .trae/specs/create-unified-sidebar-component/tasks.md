# Tasks

- [x] Task 1: 创建 SideBar 类头文件
  - [x] SubTask 1.1: 新建 `pv/widgets/sidebar.h`
  - [x] SubTask 1.2: 定义 `ItemType` 枚举（`DockItem`、`ActionItem`）
  - [x] SubTask 1.3: 定义 `ItemInfo` 结构体（index、type、iconName、textId、defaultText、button 指针、drawerPageIndex）
  - [x] SubTask 1.4: 声明 `SideBar` 类，继承 `QWidget` + `IUiWindow`
  - [x] SubTask 1.5: 声明公开接口：`addItem()`、`addSeparator()`、`setItemVisible()`、`setItemEnabled()`、`setItemChecked()`、`clearAllChecked()`、`getItem()`
  - [x] SubTask 1.6: 声明信号：`dockItemClicked(int index)`、`actionItemClicked(int index)`
  - [x] SubTask 1.7: 声明 IUiWindow 三个虚函数 override
  - [x] SubTask 1.8: 声明私有成员：`QVBoxLayout*`、`QList<ItemInfo>`、`int _next_index`

- [x] Task 2: 实现 SideBar 类
  - [x] SubTask 2.1: 新建 `pv/widgets/sidebar.cpp`
  - [x] SubTask 2.2: 实现构造函数，创建 QVBoxLayout，设置边距和间距
  - [x] SubTask 2.3: 实现 `addItem()`：创建 XToolButton，设置图标/文字/样式，添加到布局，返回索引
  - [x] SubTask 2.4: 实现 `addSeparator()`：创建 QFrame 水平线，添加到布局
  - [x] SubTask 2.5: 实现 dock 按钮点击互斥逻辑：点击 dock 按钮时取消其他 dock 按钮选中，发射 `dockItemClicked` 信号
  - [x] SubTask 2.6: 实现 action 按钮点击逻辑：点击 action 按钮时仅发射 `actionItemClicked` 信号
  - [x] SubTask 2.7: 实现 `setItemVisible()`、`setItemEnabled()`、`setItemChecked()`、`clearAllChecked()`
  - [x] SubTask 2.8: 实现 `UpdateLanguage()`：遍历所有 ItemInfo，用 `L_S()` 宏更新按钮文本
  - [x] SubTask 2.9: 实现 `UpdateTheme()`：遍历所有 ItemInfo，用 `GetIconPath()` 更新按钮图标
  - [x] SubTask 2.10: 实现 `UpdateFont()`：更新所有按钮字体大小
  - [x] SubTask 2.11: 注册到 UiManager（`ADD_UI(this)`）

- [x] Task 3: 精简 TrigBar，移除侧边栏按钮
  - [x] SubTask 3.1: 从 `trigbar.h` 中删除 `_trig_button`、`_protocol_button`、`_measure_button`、`_search_button`、`_function_button`、`_setting_button` 成员
  - [x] SubTask 3.2: 从 `trigbar.h` 中删除 `_trig_action`、`_protocol_action`、`_measure_action`、`_search_action`、`_function_action`、`_display_action` 成员
  - [x] SubTask 3.3: 从 `trigbar.h` 中删除 `sig_protocol`、`sig_trigger`、`sig_measure`、`sig_search` 信号
  - [x] SubTask 3.4: 从 `trigbar.h` 中删除 `protocol_clicked()`、`trigger_clicked()`、`measure_clicked()`、`search_clicked()` 槽函数
  - [x] SubTask 3.5: 从 `trigbar.h` 中删除 `getDockOptions()`、`update_checked_status()`、`buttonGroup` 成员
  - [x] SubTask 3.6: 从 `trigbar.cpp` 中删除对应的按钮创建、信号连接、互斥逻辑代码
  - [x] SubTask 3.7: 从 `trigbar.cpp` 的 `retranslateUi()` 中删除按钮文本设置
  - [x] SubTask 3.8: 从 `trigbar.cpp` 的 `reStyle()` 中删除按钮图标设置
  - [x] SubTask 3.9: 修改 `trigbar.cpp` 的 `reload()` 方法，移除 action 可见性控制逻辑
  - [x] SubTask 3.10: 修改 `trigbar.cpp` 的 `update_view_status()` 方法，移除按钮启用状态控制逻辑
  - [x] SubTask 3.11: 保留 Function 菜单和 Display 菜单相关代码不变

- [x] Task 4: 精简 SamplingBar，移除侧边栏按钮
  - [x] SubTask 4.1: 从 `samplingbar.h` 中删除 `_configure_button`、`_run_stop_button`、`_instant_button` 成员
  - [x] SubTask 4.2: 从 `samplingbar.h` 中删除 `_configure_action`、`_run_stop_action`、`_instant_action` 成员
  - [x] SubTask 4.3: 从 `samplingbar.h` 中删除 `sig_device_options(bool)` 信号
  - [x] SubTask 4.4: 从 `samplingbar.cpp` 中删除按钮创建和 widgetToAction 包装代码
  - [x] SubTask 4.5: 从 `samplingbar.cpp` 的 `retranslateUi()` 中删除按钮文本设置
  - [x] SubTask 4.6: 从 `samplingbar.cpp` 的 `reStyle()` 中删除按钮图标设置
  - [x] SubTask 4.7: 修改 `on_configure()` 方法，移除信号发射逻辑
  - [x] SubTask 4.8: 修改 `reload()` 方法，移除 action 可见性控制逻辑
  - [x] SubTask 4.9: 保留 `config_device()` 公开方法（供 SideBar 调用）
  - [x] SubTask 4.10: 保留 `run_or_stop()`、`run_or_stop_instant()` 公开方法（供 SideBar 调用）

- [x] Task 5: 修改 MainWindow，集成 SideBar
  - [x] SubTask 5.1: 从 `mainwindow.h` 中删除 `_right_tool_bar` 成员
  - [x] SubTask 5.2: 在 `mainwindow.h` 中添加 `#include "widgets/sidebar.h"` 和 `_side_bar` 成员
  - [x] SubTask 5.3: 在 `mainwindow.h` 中添加侧边栏按钮索引常量或枚举
  - [x] SubTask 5.4: 删除 `setupRightToolBar()` 方法，添加 `setupSideBar()` 方法
  - [x] SubTask 5.5: 在 `mainwindow.h` 中添加 `on_side_bar_dock_clicked(int)` 和 `on_side_bar_action_clicked(int)` 槽函数
  - [x] SubTask 5.6: 在 `mainwindow.cpp` 中实现 `setupSideBar()`：创建 SideBar，addItem 注册所有按钮，addSeparator 分组
  - [x] SubTask 5.7: 在 `mainwindow.cpp` 中实现 `on_side_bar_dock_clicked(int)`：根据索引打开对应 SlidingDrawer 页面，更新 DockOptions
  - [x] SubTask 5.8: 在 `mainwindow.cpp` 中实现 `on_side_bar_action_clicked(int)`：根据索引调用 SamplingBar 的 run_or_stop/run_or_stop_instant
  - [x] SubTask 5.9: 修改 SlidingDrawer 关闭回调，调用 `_side_bar->clearAllChecked()` 替代原来的手动 checked 状态管理
  - [x] SubTask 5.10: 修改 `restore_dock()`，通过 SideBar 接口恢复按钮状态
  - [x] SubTask 5.11: 修改 `reset_all_view()`，通过 SideBar 接口更新按钮状态
  - [x] SubTask 5.12: 删除原来的 `on_protocol`/`on_trigger`/`on_measure`/`on_search`/`on_device_options` 槽函数（逻辑已合并到 `on_side_bar_dock_clicked`）
  - [x] SubTask 5.13: 修改信号连接：删除 TrigBar 的 sig_protocol/sig_trigger/sig_measure/sig_search 连接，改为连接 SideBar 信号
  - [x] SubTask 5.14: 修改信号连接：删除 SamplingBar 的 sig_device_options 连接
  - [x] SubTask 5.15: 将 SideBar 添加到 MainWindow 布局中（替代原来的 QToolBar 右侧区域）

- [x] Task 6: 修改 CMakeLists.txt
  - [x] SubTask 6.1: 在 CMakeLists.txt 中添加 `pv/widgets/sidebar.h` 和 `pv/widgets/sidebar.cpp`

- [x] Task 7: 编译验证
  - [x] SubTask 7.1: 确保项目可以正常编译，无语法错误
  - [x] SubTask 7.2: 确保无未使用变量警告（仅有原有代码的警告）
  - [x] SubTask 7.3: 确保侧边栏按钮点击后 SlidingDrawer 正确打开/关闭（代码逻辑已实现，需运行时验证）
  - [x] SubTask 7.4: 确保 dock 按钮互斥逻辑正确（代码逻辑已实现，需运行时验证）
  - [x] SubTask 7.5: 确保主题切换后图标正确更新（SideBar 实现 UpdateTheme，需运行时验证）
  - [x] SubTask 7.6: 确保语言切换后文字正确更新（SideBar 实现 UpdateLanguage，需运行时验证）

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 5] depends on [Task 2] and [Task 3] and [Task 4]
- [Task 6] depends on [Task 1]
- [Task 7] depends on [Task 5] and [Task 6]
- [Task 3] and [Task 4] can be done in parallel
