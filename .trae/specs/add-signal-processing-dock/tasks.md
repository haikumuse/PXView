# Tasks

- [x] Task 1: 新增 SVG 图标文件并注册到资源系统
  - [x] SubTask 1.1: 创建 `PXView/icons/dark/audio-waveform.svg`（波形图标，stroke 颜色适配 dark 主题）
  - [x] SubTask 1.2: 创建 `PXView/icons/light/audio-waveform.svg`（波形图标，stroke 颜色适配 light 主题）
  - [x] SubTask 1.3: 在 `PXView/PXView.qrc` 中注册两个新图标文件

- [x] Task 2: LogicSnapshot 新增信号取反核心算法
  - [x] SubTask 2.1: 在 `logicsnapshot.h` 中声明 `invert_channel(int sig_index)` 方法
  - [x] SubTask 2.2: 在 `logicsnapshot.cpp` 中实现 `invert_channel`：遍历所有 LeafBlock，对指定通道的每个采样位取反，然后重建 mipmap

- [x] Task 3: SigSession 新增信号取反流程控制
  - [x] SubTask 3.1: 在 `SessionData` 中新增 `_signal_invert_active` 和 `_signal_invert_channels` 字段
  - [x] SubTask 3.2: 在 `sigsession.h` 中声明 `set_signal_invert()`、`clear_signal_invert()`、`is_signal_invert_active()` 方法
  - [x] SubTask 3.3: 在 `sigsession.cpp` 中实现取反流程：备份原始数据 → 从备份恢复 → 应用取反 → 如果滤波激活则应用滤波
  - [x] SubTask 3.4: 修改 `glitch_filter_task`：滤波前先检查取反状态，从原始备份恢复后先应用取反再滤波
  - [x] SubTask 3.5: 在 `icallbacks.h` 中新增 `DSV_MSG_SIGNAL_INVERT_STARTED`、`DSV_MSG_SIGNAL_INVERT_COMPLETED`、`DSV_MSG_SIGNAL_INVERT_CLEARED` 消息常量

- [x] Task 4: 创建 SignalProcessingDock
  - [x] SubTask 4.1: 创建 `pv/dock/signalprocessingdock.h`，声明 `SignalProcessingDock` 类（继承 QWidget + IUiWindow + IContextAware）
  - [x] SubTask 4.2: 创建 `pv/dock/signalprocessingdock.cpp`，实现：
    - 信号取反区域：通道复选框列表、全选/取消全选、应用取反/恢复原始数据按钮、状态标签
    - 毛刺过滤区域：通道复选框 + SpinBox 列表、全选/取消全选、应用滤波/恢复原始数据按钮、状态标签
    - 非 Logic 模式提示
    - IUiWindow 接口实现（UpdateLanguage/UpdateTheme/UpdateFont）
    - IContextAware 接口实现（bind_context/unbind_context）
    - 会话保存/恢复（get_session/set_session）
    - 消息监听（DSV_MSG_GLITCH_FILTER_*、DSV_MSG_SIGNAL_INVERT_*）更新 UI 状态

- [x] Task 5: 从 DeviceOptionsDock 移除毛刺过滤代码
  - [x] SubTask 5.1: 从 `deviceoptionsdock.h` 移除毛刺过滤相关成员变量和方法声明
  - [x] SubTask 5.2: 从 `deviceoptionsdock.cpp` 移除毛刺过滤相关代码（build_glitch_filter_panel、on_apply_glitch_filter、on_restore_original_data 等）
  - [x] SubTask 5.3: 从 `deviceoptionsdock.cpp` 的构造函数和 update_view 中移除毛刺过滤面板的创建和插入
  - [x] SubTask 5.4: 从 `deviceoptionsdock.cpp` 的 get_session/set_session 中移除 glitch_filter 相关的 JSON 序列化/反序列化
  - [x] SubTask 5.5: 从 `deviceoptionsdock.cpp` 的 device_updated 中移除 rebuild_glitch_filter_panel 调用

- [x] Task 6: MainWindow 注册新侧边栏 Dock
  - [x] SubTask 6.1: 在 `mainwindow.h` 中新增 `SIDEBAR_SIGNAL_PROCESSING` 枚举值，调整后续枚举值
  - [x] SubTask 6.2: 在 `mainwindow.h` 中新增 `_signal_processing_dock`、`_signal_processing_widget`、`_drawer_page_signal_processing` 成员
  - [x] SubTask 6.3: 在 `mainwindow.cpp` 的构造函数中创建 `SignalProcessingDock` 实例和 QDockWidget
  - [x] SubTask 6.4: 在 `mainwindow.cpp` 中将 SignalProcessingDock 添加为 SlidingDrawer 页面
  - [x] SubTask 6.5: 在 `setupSideBar()` 中注册新的侧边栏项（audio-waveform 图标，位于 Options 和 Log 之间）
  - [x] SubTask 6.6: 在 `on_side_bar_dock_clicked()` 中处理 `SIDEBAR_SIGNAL_PROCESSING` 分支
  - [x] SubTask 6.7: 在 `DockOptions` 中新增 `signalProcessingDock` 字段并初始化
  - [x] SubTask 6.8: 在 `SessionDocument` 中新增 `_dock_signal_processing_session` 字段
  - [x] SubTask 6.9: 在 `CMakeLists.txt` 中新增 `signalprocessingdock.cpp` 源文件

- [x] Task 7: 编译验证
  - [x] SubTask 7.1: 运行增量构建，确保无编译错误

# Task Dependencies
- [Task 2] depends on nothing (can start immediately)
- [Task 3] depends on [Task 2] (取反流程需要 invert_channel 方法)
- [Task 4] depends on [Task 3] (Dock UI 需要调用 SigSession 的取反/滤波方法)
- [Task 5] depends on [Task 4] (先创建新 Dock 再移除旧代码，避免功能中断)
- [Task 6] depends on [Task 4] (MainWindow 需要引用 SignalProcessingDock 类)
- [Task 1] depends on nothing (can start immediately, parallel with Task 2)
- [Task 7] depends on [Task 1, Task 2, Task 3, Task 4, Task 5, Task 6]
