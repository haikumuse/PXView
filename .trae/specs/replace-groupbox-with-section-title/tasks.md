# Tasks

- [x] Task 1: 在 dark.qss 和 light.qss 中新增子标题和分隔线样式，修改 QGroupBox 样式
  - [x] SubTask 1.1: 在 dark.qss Color Tokens 区新增 `@dock-section-title-fg` 和 `@dock-section-separator-color` 令牌，删除 `@dock-groupbox-border` 和 `@dock-groupbox-title` 令牌
  - [x] SubTask 1.2: 在 dark.qss 中新增 `#dock_section_title` 和 `#dock_section_separator` 样式块
  - [x] SubTask 1.3: 在 dark.qss 中修改 `QGroupBox`、`QGroupBox::title`、`QGroupBox:disabled` 样式为无边框
  - [x] SubTask 1.4: 在 light.qss 中执行与 dark.qss 相同的修改

- [x] Task 2: MeasureDock - 移除 QGroupBox，替换为子标题 + 分隔线
  - [x] SubTask 2.1: 在 measuredock.h 中将 4 个 `QGroupBox*` 成员变量替换为 `QLabel*` 子标题成员变量
  - [x] SubTask 2.2: 在 measuredock.cpp 构造函数中，将每个 QGroupBox 替换为：QLabel（子标题，objectName=dock_section_title）+ 原有内容布局 + QFrame（分隔线，objectName=dock_section_separator）
  - [x] SubTask 2.3: 调整主 QVBoxLayout：子标题 → 内容布局 → 分隔线 → 子标题 → 内容布局 → ...，移除 QGroupBox 的 margin-top 和边框占位

- [x] Task 3: DeviceOptionsDock - 移除 QGroupBox，替换为子标题 + 分隔线
  - [x] SubTask 3.1: 在 deviceoptionsdock.h 中将 `_dynamic_panel` 类型从 `QGroupBox*` 改为 `QWidget*`，将 `_glitch_filter_group` 类型从 `QGroupBox*` 改为 `QWidget*`
  - [x] SubTask 3.2: 在 deviceoptionsdock.cpp 的 `build_dynamic_panel()` 中，将 `new QGroupBox("group")` 替换为 `new QWidget` + 子标题 QLabel
  - [x] SubTask 3.3: 在 deviceoptionsdock.cpp 的构造函数和 `reload()` 中，将局部 `props_box` QGroupBox 替换为子标题 QLabel + 内容布局，设置 objectName 为 `dock_mode_section`
  - [x] SubTask 3.4: 在 deviceoptionsdock.cpp 的 `build_glitch_filter_panel()` 中，将 `_glitch_filter_group` 从 QGroupBox 替换为 QWidget + 子标题 QLabel
  - [x] SubTask 3.5: 修改 `rebuild_glitch_filter_panel()` 中查找 Mode 分组的逻辑，从 `qobject_cast<QGroupBox*>` + `title()` 改为通过 objectName 查找

- [x] Task 4: TriggerDock - 移除 QGroupBox，替换为子标题 + 分隔线
  - [x] SubTask 4.1: 在 triggerdock.h 中将 `_stage_groupBox_list` 类型从 `QVector<QGroupBox*>` 改为 `QVector<QWidget*>`，将 `_serial_groupBox` 类型从 `QGroupBox*` 改为 `QWidget*`
  - [x] SubTask 4.2: 在 triggerdock.cpp 中，将每个阶段的 QGroupBox 替换为 QWidget 容器 + 子标题 QLabel
  - [x] SubTask 4.3: 在 triggerdock.cpp 中，将 `_serial_groupBox` 从 QGroupBox 替换为 QWidget + 子标题 QLabel

# Task Dependencies
- [Task 2, Task 3, Task 4] depend on [Task 1]（QSS 样式需先就位）
- Task 2, Task 3, Task 4 之间无依赖，可并行执行
