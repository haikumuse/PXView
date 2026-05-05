# Tasks

- [x] Task 1: 扩展 DockOptions 结构体，新增 deviceOptionsDock 字段
  - [x] SubTask 1.1: 在 `appconfig.h` 的 `DockOptions` 结构体中添加 `bool deviceOptionsDock;` 字段
  - [x] SubTask 1.2: 在 `appconfig.cpp` 中为 `deviceOptionsDock` 添加初始化（默认 false）

- [x] Task 2: 创建 DeviceOptionsDock 类头文件
  - [x] SubTask 2.1: 新建 `pv/dock/deviceoptionsdock.h`
  - [x] SubTask 2.2: 声明 `DeviceOptionsDock` 类，继承 `QScrollArea` 和 `IUiWindow`
  - [x] SubTask 2.3: 声明所有从 `dialogs::DeviceOptions` 迁移的成员变量（UI 组件 + 数据绑定）
  - [x] SubTask 2.4: 声明新增的 `_apply_button` 和 `settings_applied()` 信号
  - [x] SubTask 2.5: 声明 `update_view()`、`device_updated()` 公开方法
  - [x] SubTask 2.6: 声明 IUiWindow 三个虚函数 override
  - [x] SubTask 2.7: 声明所有从 `dialogs::DeviceOptions` 迁移的槽函数和私有方法

- [x] Task 3: 实现 DeviceOptionsDock 类
  - [x] SubTask 3.1: 新建 `pv/dock/deviceoptionsdock.cpp`
  - [x] SubTask 3.2: 实现构造函数，创建滚动面板 + Mode 分组框 + 动态面板 + 应用按钮（替代原 OK 按钮）
  - [x] SubTask 3.3: 迁移 `get_property_form()` 方法，构建设备属性表单
  - [x] SubTask 3.4: 迁移 `build_dynamic_panel()` 和 `dynamic_widget()` 方法
  - [x] SubTask 3.5: 迁移 `logic_probes()` 方法，构建 LOGIC 模式通道 UI
  - [x] SubTask 3.6: 迁移 `analog_probes()` 方法，构建 ANALOG 模式通道 UI
  - [x] SubTask 3.7: 迁移 `set_all_probes()`、`enable_max_probes()`、`enable_all_probes()`、`disable_all_probes()` 方法
  - [x] SubTask 3.8: 迁移 `channel_checkbox_clicked()`、`channel_check()`、`analog_channel_check()`、`on_analog_channel_enable()` 方法
  - [x] SubTask 3.9: 迁移 `zero_adj()`、`on_calibration()` 方法
  - [x] SubTask 3.10: 迁移 `mode_check_timeout()` 定时器回调，增加 Dock 不可见时停止定时器的逻辑
  - [x] SubTask 3.11: 实现 `apply_settings()` 方法，替代原 `accept()` 逻辑，提交后发射 `settings_applied()` 信号
  - [x] SubTask 3.12: 实现 `try_resize_scroll()` 方法，适配侧边栏宽度（移除固定宽度限制，改用自适应）
  - [x] SubTask 3.13: 实现 `update_view()` 方法，重建整个面板
  - [x] SubTask 3.14: 实现 `device_updated()` 方法，处理设备热插拔
  - [x] SubTask 3.15: 实现 IUiWindow 接口：`UpdateLanguage()`、`UpdateTheme()`、`UpdateFont()`
  - [x] SubTask 3.16: 迁移 `ChannelLabel` 内部类（或直接复用 `dialogs/deviceoptions.h` 中的定义）

- [x] Task 4: 修改 MainWindow，集成设备选项 Dock
  - [x] SubTask 4.1: 在 `mainwindow.h` 中添加 `_device_options_dock`（QDockWidget*）和 `_device_options_widget`（dock::DeviceOptionsDock*）成员
  - [x] SubTask 4.2: 在 `mainwindow.h` 中添加 `on_device_options(bool)` 槽函数声明
  - [x] SubTask 4.3: 在 `mainwindow.cpp` 构造函数中创建 QDockWidget 和 DeviceOptionsDock，添加到右侧区域
  - [x] SubTask 4.4: 在 `mainwindow.cpp` 中实现 `on_device_options(bool)` 槽函数，控制 Dock 显隐
  - [x] SubTask 4.5: 连接 `_device_options_widget` 的 `settings_applied()` 信号到 SamplingBar 的后续处理逻辑
  - [x] SubTask 4.6: 在 `DSV_MSG_DEVICE_OPTIONS_UPDATED` 消息处理中添加 `_device_options_widget->device_updated()` 调用
  - [x] SubTask 4.7: 安装事件过滤器 `_device_options_dock->installEventFilter(this)`

- [x] Task 5: 修改 SamplingBar，将 on_configure 改为信号发射
  - [x] SubTask 5.1: 在 `samplingbar.h` 中添加 `sig_device_options_toggle()` 信号声明
  - [x] SubTask 5.2: 重写 `samplingbar.cpp` 的 `on_configure()` 方法：检查设备存在后发射 `sig_device_options_toggle()` 信号
  - [x] SubTask 5.3: 修改 `config_device()` 方法，适配新逻辑
  - [x] SubTask 5.4: 在 MainWindow 中连接 `sig_device_options_toggle()` 信号到 `on_device_options()` 槽

- [x] Task 6: 扩展 TrigBar，新增设备选项切换按钮
  - [x] SubTask 6.1: 在 `trigbar.h` 中添加 `_device_options_button`（XToolButton）和 `_device_options_action`（QAction*）成员
  - [x] SubTask 6.2: 在 `trigbar.h` 中添加 `sig_device_options(bool)` 信号声明
  - [x] SubTask 6.3: 在 `trigbar.h` 中添加 `device_options_clicked()` 槽函数声明
  - [x] SubTask 6.4: 在 `trigbar.cpp` 构造函数中创建 `_device_options_button`，设置图标和文本
  - [x] SubTask 6.5: 实现 `device_options_clicked()` 槽函数，互斥逻辑与其他 4 个按钮一致
  - [x] SubTask 6.6: 修改 `update_checked_status()` 方法，同步 `_device_options_button` 的选中状态
  - [x] SubTask 6.7: 修改 `reload()` 方法，根据工作模式控制 `_device_options_action` 的可见性
  - [x] SubTask 6.8: 修改 `update_view_status()` 方法，控制 `_device_options_button` 的启用状态
  - [x] SubTask 6.9: 在 MainWindow 中连接 `sig_device_options(bool)` 信号到 `on_device_options()` 槽
  - [x] SubTask 6.10: 在 `setupRightToolBar()` 中添加 `_device_options_action` 到右侧工具栏

- [x] Task 7: 修改 CMakeLists.txt，添加新源文件
  - [x] SubTask 7.1: 在 `CMakeLists.txt` 中添加 `pv/dock/deviceoptionsdock.h` 和 `pv/dock/deviceoptionsdock.cpp`

- [x] Task 8: 编译验证
  - [x] SubTask 8.1: 确保项目可以正常编译，无语法错误（我们修改的所有文件编译通过）
  - [x] SubTask 8.2: 确保无未使用变量警告（无新增警告）
  - [x] SubTask 8.3: 确保原有模态对话框代码未被破坏（保留文件仍可编译）

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 2]
- [Task 4] depends on [Task 3]
- [Task 5] depends on [Task 4]
- [Task 6] depends on [Task 1]
- [Task 7] depends on [Task 2]
- [Task 8] depends on [Task 3] through [Task 7]
