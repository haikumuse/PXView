# Tasks

- [ ] Task 1: 新增 Dock 相关语义颜色令牌
  - [ ] SubTask 1.1: 在 dark.qss 的 Color Tokens 注释区新增 `@drawer-title-bg`、`@drawer-title-fg`、`@drawer-title-border`、`@drawer-edge-hover`、`@dock-gridline`、`@dock-groupbox-border`、`@dock-groupbox-title`、`@dock-status-ok`、`@dock-status-error` 令牌及对应暗色值
  - [ ] SubTask 1.2: 在 light.qss 的 Color Tokens 注释区新增相同的令牌名及对应亮色值

- [ ] Task 2: 补全 SlidingDrawer QSS 样式
  - [ ] SubTask 2.1: 在 dark.qss 的 SlidingDrawer 区块中添加 `#sliding_drawer_titlebar`、`#sliding_drawer_title`、`#sliding_drawer_edge_grip`、`#sliding_drawer_edge_grip:hover`、`#sliding_drawer_stack` 样式
  - [ ] SubTask 2.2: 在 light.qss 的 SlidingDrawer 区块中添加相同结构的样式

- [ ] Task 3: 添加 Dock 页面内容区域 QSS 样式
  - [ ] SubTask 3.1: 在 dark.qss 中添加各 Dock 页面 QScrollArea 无边框样式（通过 objectName 选择器）
  - [ ] SubTask 3.2: 在 dark.qss 中添加 Dock 页面 QGroupBox 统一样式（使用 `@dock-groupbox-border` / `@dock-groupbox-title` 令牌）
  - [ ] SubTask 3.3: 在 dark.qss 中添加 `#dock_search_result_view` 表格样式（使用 `@dock-gridline` 令牌替代 `#d0d0d0`）
  - [ ] SubTask 3.4: 在 dark.qss 中添加 `#dock_protocol_page QHeaderView` 字体样式
  - [ ] SubTask 3.5: 在 dark.qss 中添加 `[status="ok"]` / `[status="error"]` 动态属性选择器样式（使用 `@dock-status-ok` / `@dock-status-error` 令牌）
  - [ ] SubTask 3.6: 在 light.qss 中添加与 dark.qss 对应的所有 Dock 页面样式

- [ ] Task 4: 删除已无用的 QSS 样式
  - [ ] SubTask 4.1: 从 dark.qss 中删除 `QDockWidget` 及其子选择器样式块
  - [ ] SubTask 4.2: 从 light.qss 中删除 `QDockWidget` 及其子选择器样式块
  - [ ] SubTask 4.3: 从 stylesheet.qss 中删除 `QDockWidget` / `QDockWidget::title` / `QDockWidget > QWidget` 样式块
  - [ ] SubTask 4.4: 从 stylesheet.qss 中删除 `QScrollArea #measureWidget` / `#dsoTriggerWidget` / `#triggerWidget` / `#protocolWidget` 样式块

- [ ] Task 5: 消除 SearchDock 内联 setStyleSheet
  - [ ] SubTask 5.1: 为 `_result_view` 设置 objectName `"dock_search_result_view"`
  - [ ] SubTask 5.2: 删除 `searchdock.cpp` 中 `_result_view->setStyleSheet(...)` 调用
  - [ ] SubTask 5.3: 确认 QSS 中 `#dock_search_result_view` 选择器已覆盖原内联样式的所有属性

- [ ] Task 6: 消除 DeviceOptionsDock 内联 setStyleSheet
  - [ ] SubTask 6.1: 为 DeviceOptionsDock 的 QScrollArea 设置 objectName（如 `"dock_device_options_scroll"`）
  - [ ] SubTask 6.2: 删除 `deviceoptionsdock.cpp` 中 `this->setStyleSheet("QScrollArea{border:none;}")` 调用
  - [ ] SubTask 6.3: 确认 QSS 中对应选择器已覆盖无边框样式

- [ ] Task 7: 消除 MeasureDock 按钮颜色内联 setStyleSheet
  - [ ] SubTask 7.1: 修改 `measuredock.cpp` 中 `set_cursor_btn_color()` 方法，将硬编码的 `rgb(240,240,240)` 等颜色替换为 `palette().color(QPalette::Window)` 等 QPalette 获取的主题色
  - [ ] SubTask 7.2: 确认按钮颜色在暗色/亮色主题下均正确显示

- [ ] Task 8: 消除 ProtocolDock 表头字体内联 setStyleSheet
  - [ ] SubTask 8.1: 为 ProtocolDock 的 `_table_view` 设置 objectName `"dock_protocol_table_view"`
  - [ ] SubTask 8.2: 删除 `protocoldock.cpp` 中 `_table_view->setStyleSheet(style)` 调用
  - [ ] SubTask 8.3: 确认 QSS 中 `#dock_protocol_table_view QHeaderView` 选择器已覆盖字体大小

- [ ] Task 9: 消除 ProtocolItemLayer 进度标签内联 setStyleSheet
  - [ ] SubTask 9.1: 修改 `protocolitemlayer.cpp`，将 `_progress_label->setStyleSheet("color:green;")` 改为 `_progress_label->setProperty("status", "ok")` + `style()->unpolish/polish`
  - [ ] SubTask 9.2: 修改 `protocolitemlayer.cpp`，将 `_progress_label->setStyleSheet("color:red;")` 改为 `_progress_label->setProperty("status", "error")` + `style()->unpolish/polish`
  - [ ] SubTask 9.3: 确认 QSS 中 `[status="ok"]` / `[status="error"]` 选择器已定义颜色

- [ ] Task 10: 消除 SearchComboBox 内联 setStyleSheet
  - [ ] SubTask 10.1: 为 SearchComboBox 的 `_scroll` 设置 objectName（如 `"dock_search_combo_scroll"`）
  - [ ] SubTask 10.2: 删除 `searchcombobox.cpp` 中 `_scroll->setStyleSheet("QScrollArea{border:none;}")` 调用
  - [ ] SubTask 10.3: 确认 QSS 中对应选择器已覆盖无边框样式

- [ ] Task 11: 统一各 Dock QScrollArea 边框和内边距
  - [ ] SubTask 11.1: 修改 `triggerdock.cpp`，为内部 QScrollArea 设置 `setFrameShape(QFrame::NoFrame)`，统一内容区域 margin 为 (12,8,12,8)
  - [ ] SubTask 11.2: 修改 `dsotriggerdock.cpp`，同上
  - [ ] SubTask 11.3: 修改 `measuredock.cpp`，为内部 QScrollArea 设置 `setFrameShape(QFrame::NoFrame)`，统一内容区域 margin
  - [ ] SubTask 11.4: 修改 `protocoldock.cpp`，统一内容区域 margin
  - [ ] SubTask 11.5: 修改 `searchdock.cpp`，统一内容区域 margin（SearchDock 继承 QWidget，需确保布局 margin 一致）
  - [ ] SubTask 11.6: 修改 `deviceoptionsdock.cpp`，统一内容区域 margin

- [ ] Task 12: 编译验证
  - [ ] SubTask 12.1: 确保项目正常编译，无语法错误
  - [ ] SubTask 12.2: 确保暗色主题下所有 Dock 页面样式正确
  - [ ] SubTask 12.3: 确保亮色主题下所有 Dock 页面样式正确
  - [ ] SubTask 12.4: 确保主题切换后所有 Dock 页面样式正确更新
  - [ ] SubTask 12.5: 确保无内联 setStyleSheet 硬编码颜色残留

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 1]
- [Task 5] depends on [Task 3]
- [Task 6] depends on [Task 3]
- [Task 8] depends on [Task 3]
- [Task 9] depends on [Task 3]
- [Task 10] depends on [Task 3]
- [Task 11] depends on [Task 3]
- [Task 12] depends on [Task 4] through [Task 11]
- [Task 5] [Task 6] [Task 7] [Task 8] [Task 9] [Task 10] [Task 11] can be done in parallel after Task 3
