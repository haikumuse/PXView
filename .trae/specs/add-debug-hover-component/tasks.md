# Tasks

- [x] Task 1: 创建 DebugHelper 类核心实现
  - [x] SubTask 1.1: 创建 `pv/ui/debughelper.h`，声明 DebugHelper 类（继承 QObject），包含：开关状态、浮动信息面板 QLabel、高亮遮罩 QWidget、事件过滤器、快捷键绑定
  - [x] SubTask 1.2: 创建 `pv/ui/debughelper.cpp`，实现以下功能：
    - 构造函数：创建浮动 QLabel（设置 Qt::ToolTip 窗口标志、半透明深色背景、等宽字体）、创建高亮遮罩 QWidget（红色半透明边框、Qt::ToolTip 标志）
    - `toggle()`：切换调试模式开关，输出日志提示
    - `eventFilter()`：拦截 MouseMove 和 Leave 事件，调用 `updateInfo()`
    - `updateInfo()`：使用 `QApplication::widgetAt()` 获取鼠标下控件，收集类名（metaObject()->className()）、objectName、父级链路、几何信息、可见性，更新 QLabel 文本和位置，更新高亮遮罩位置和大小
    - `install()`：在 QApplication 上安装事件过滤器，注册快捷键 `Ctrl+Shift+D`
    - `uninstall()`：移除事件过滤器，隐藏面板和高亮
  - [x] SubTask 1.3: 确保信息面板不拦截鼠标事件（setAttribute Qt::WA_TransparentForMouseEvents），确保高亮遮罩同样透明

- [x] Task 2: 在 MainWindow 中集成 DebugHelper
  - [x] SubTask 2.1: 在 `mainwindow.h` 中添加前向声明 `class DebugHelper;` 和私有成员 `pv::ui::DebugHelper *_debug_helper;`
  - [x] SubTask 2.2: 在 `mainwindow.cpp` 构造函数中创建 DebugHelper 实例并调用 `install()`
  - [x] SubTask 2.3: 在 `mainwindow.cpp` 析构函数中调用 `uninstall()` 并删除 DebugHelper

- [x] Task 3: 更新 CMakeLists.txt 构建配置
  - [x] SubTask 3.1: 在 DSView_SOURCES 列表中添加 `DSView/pv/ui/debughelper.cpp`
  - [x] SubTask 3.2: 在 DSView_HEADERS 列表中添加 `DSView/pv/ui/debughelper.h`

# Task Dependencies
- Task 2 依赖 Task 1（需要 DebugHelper 类先存在）
- Task 3 与 Task 1 可并行（但需确保文件路径一致）
