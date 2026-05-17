# Tasks

## 前一轮会话已完成任务

- [x] Task 1: 恢复 CMakeLists.txt Qt5/Qt6 双版本构建逻辑
  - [x] SubTask 1.1: 取消注释 Qt6 find_package 块，更新 Qt6 组件列表（Core, Widgets, Gui, Svg, Concurrent，不含 WinExtras）
  - [x] SubTask 1.2: 修改 Qt 版本检测逻辑：找不到 Qt5 时尝试 Qt6，而非直接 FATAL_ERROR
  - [x] SubTask 1.3: 添加 Qt6 分支的 qt6_wrap_cpp/qt6_add_resources
  - [x] SubTask 1.4: C++ 标准从 c++11 升级到 c++17
  - [x] SubTask 1.5: Windows 下 Qt5 分支保留 Qt5WinExtras，Qt6 分支不链接
  - [x] SubTask 1.6: 验证 Qt5 能通过 CMake 配置阶段

- [x] Task 2: 创建 Qt 版本兼容性辅助头文件
  - [x] SubTask 2.1: 创建 `PXView/pv/ui/qtcompat.h`，定义 NativeEventResult 类型别名和事件坐标兼容宏
  - [x] SubTask 2.2: 在需要 Qt 版本兼容的源文件中引入此头文件

- [x] Task 3: 修复 nativeEvent 签名（6 个文件）
  - [x] SubTask 3.1: mainframe.h — 声明使用 NativeEventResult 类型
  - [x] SubTask 3.2: mainframe.cpp — 实现使用 NativeEventResult 类型，修复 *result 赋值和基类调用
  - [x] SubTask 3.3: submainframe.h — 声明使用 NativeEventResult 类型
  - [x] SubTask 3.4: submainframe.cpp — 实现使用 NativeEventResult 类型，修复基类调用
  - [x] SubTask 3.5: winshadow.h — 声明使用 NativeEventResult 类型
  - [x] SubTask 3.6: winshadow.cpp — 实现使用 NativeEventResult 类型

- [x] Task 4: 替换 Qt5WinExtras 为 Win32 ITaskbarList3 原生实现
  - [x] SubTask 4.1: 创建 `PXView/pv/wintaskbarprogress.h/cpp`，封装 ITaskbarList3 COM 接口
  - [x] SubTask 4.2: mainframe.h — 移除 QWinTaskbarButton/QWinTaskbarProgress include 和成员，替换为 WinTaskbarProgress
  - [x] SubTask 4.3: mainframe.cpp — 替换所有 _taskBtn/_taskPrg 调用为 WinTaskbarProgress API
  - [x] SubTask 4.4: CMakeLists.txt — 添加 wintaskbarprogress 源文件

- [x] Task 5: 修复 QMouseEvent::globalPos() 迁移（5 个文件，11 处）
  - [x] SubTask 5.1: mainframe.cpp — 2 处 globalPos() 替换
  - [x] SubTask 5.2: submainframe.cpp — 2 处 globalPos() 替换
  - [x] SubTask 5.3: titlebar.cpp — 4 处 globalPos() 替换
  - [x] SubTask 5.4: slidingdrawer.cpp — 2 处 globalPos() 替换
  - [x] SubTask 5.5: draggabletabbar.cpp — 1 处 globalPos() 替换

- [x] Task 6: 修复 QMouseEvent::pos() 和 event->x()/y() 迁移（10 个文件，77+ 处）
  - [x] SubTask 6.1: viewport.cpp — 38 处 event->pos() 替换
  - [x] SubTask 6.2: header.cpp — 11 处 event->pos() 替换
  - [x] SubTask 6.3: ruler.cpp — 7 处替换
  - [x] SubTask 6.4: view.cpp — 5 处替换
  - [x] SubTask 6.5: draggabletabbar.cpp — 9 处替换
  - [x] SubTask 6.6: 其他文件（viewstatus, sidebarbutton, devmode, debughelper, searchpatterninput）7 处替换

- [x] Task 7: 替换 QTextCodec 为 QStringConverter（3 个文件）
  - [x] SubTask 7.1: encoding.cpp — init() 函数的 Win32 分支加版本守卫；移除无守卫的 QTextCodec include
  - [x] SubTask 7.2: path.cpp — ToUnicodePath() 函数 Qt6 下改用 QStringEncoder
  - [x] SubTask 7.3: storesession.cpp — 移除残留的 QTextCodec include

- [x] Task 8: 替换 QSignalMapper 为 lambda connect（3 个文件）
  - [x] SubTask 8.1: decodermenu.h/cpp — 移除 QSignalMapper 成员，改用 lambda connect
  - [x] SubTask 8.2: decodetrace.h — 移除残留的 QSignalMapper include

- [x] Task 9: 修复 QFontDatabase 静态方法（1 个文件）
  - [x] SubTask 9.1: main.cpp — Qt6 下使用 QFontDatabase 实例方法替代静态方法

- [x] Task 10: 修复 High DPI 属性和 QDesktopWidget 残留（2 个文件）
  - [x] SubTask 10.1: main.cpp — 用 QT_VERSION 守卫包裹 AA_EnableHighDpiScaling/AA_DisableHighDpiScaling/AA_UseHighDpiPixmaps
  - [x] SubTask 10.2: winnativewidget.cpp — 移除无守卫的 QDesktopWidget include

- [x] Task 11: 修复 QPixmap::grabWidget 和截图 API（1 个文件）
  - [x] SubTask 11.1: mainwindow.cpp — Qt6 分支修复 QApplication::desktop->winId() BUG，替换 QPixmap::grabWidget 为 QWidget::grab()

- [x] Task 12: 迁移 SIGNAL/SLOT 旧式语法到新式仿函数语法（46 个文件，514+ 处）
  - [x] SubTask 12.1: 主要文件迁移（mainwindow, protocoldock, measuredock, triggerdock, samplingbar, dsotriggerdock 等 33 个文件）
  - [x] SubTask 12.2: dialog 文件迁移（storeprogress, search, regionoptions, protocollist, protocolexp, mathoptions, lissajousoptions, interval, fftoptions, dsomeasure, deviceoptions, decoderoptionsdlg, calibration 等 13 个文件）
  - [x] SubTask 12.3: 修复迁移引起的编译错误（类型不匹配、访问权限等）

- [x] Task 13: 编译验证
  - [x] SubTask 13.1: 使用 Qt5 编译通过 ✅
  - [x] SubTask 13.2: 使用 Qt6 编译通过 ✅
  - [x] SubTask 13.3: 修复编译过程中发现的遗漏问题（IntProp→Int, protected继承→public, QButtonGroup重载, QWidget*→QPushButton*, DSDialog*→this, view.h private→public slots）

## 本轮新增任务（遗漏修复）

- [x] Task 14: 修复残留的无用 Qt5-only 头文件 include
  - [x] SubTask 14.1: logdock.cpp:37 — 移除无用的 `#include <QTextCodec>`
  - [x] SubTask 14.2: decodermenu.h:27 — 移除无用的 `#include <QSignalMapper>`

- [x] Task 15: 修复 titlebar.cpp 中遗漏的鼠标事件 API 迁移
  - [x] SubTask 15.1: titlebar.cpp:649 — `event->pos()` → `QT_COMPAT_POS(event)`
  - [x] SubTask 15.2: titlebar.cpp:657-658 — `event->pos().x()/y()` → `QT_COMPAT_X(event)`/`QT_COMPAT_Y(event)`
  - [x] SubTask 15.3: titlebar.cpp:666 — `event->globalPos()` → `QT_COMPAT_GLOBAL_POS(event)`

- [x] Task 16: 修复其他文件中遗漏的 globalPos() 迁移
  - [x] SubTask 16.1: mainframe.cpp:475 — `mouse_event->globalPos()` → `QT_COMPAT_GLOBAL_POS(mouse_event)`
  - [x] SubTask 16.2: submainframe.cpp:557 — `mouse_event->globalPos()` → `QT_COMPAT_GLOBAL_POS(mouse_event)`
  - [x] SubTask 16.3: slidingdrawer.cpp:513 — `event->globalPos()` → `QT_COMPAT_GLOBAL_POS(event)`

- [x] Task 17: 统一 viewport.cpp 滚轮事件处理风格（可选优化）
  - [x] SubTask 17.1: viewport.cpp:1699-1715 — 将内联 `#if QT_VERSION_CHECK` 替换为 `QT_COMPAT_WHEEL_*` 宏

- [x] Task 18: Qt6 编译验证
  - [x] SubTask 18.1: 安装 Qt6 开发环境（MSYS2 mingw-w64-x86_64-qt6 6.11.0）
  - [x] SubTask 18.2: 使用 Qt6 执行 CMake 配置和编译 ✅
  - [x] SubTask 18.3: 修复 Qt6 编译过程中发现的 5 个新问题
    - [x] enterEvent(QEvent*) → enterEvent(QEnterEvent*) 签名变更（sidebarbutton.h/cpp, hoversplitter.h）
    - [x] QButtonGroup::buttonClicked(int) → idClicked(int)（samplingbar.cpp）
    - [x] QtConcurrent::run(this, &method) → QtConcurrent::run(lambda)（searchdock.cpp）
    - [x] QKeyEvent 构造函数 0 → Qt::NoModifier（winnativewidget.cpp）
    - [x] QContextMenuEvent 不兼容 QT_COMPAT_POS 宏（draggabletabbar.cpp）

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3-11] depends on [Task 2]
- [Task 4] depends on [Task 1]
- [Task 12] 可与 Task 3-11 并行执行
- [Task 13] depends on [Task 1-12]
- [Task 14-16] 无依赖，可立即执行
- [Task 17] 无依赖，可立即执行（低优先级）
- [Task 18] depends on [Task 14-16]
