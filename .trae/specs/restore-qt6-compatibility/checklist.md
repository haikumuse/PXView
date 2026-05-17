# Qt6 兼容性验证清单

## 构建系统
- [x] CMakeLists.txt 支持 Qt5/Qt6 双版本自动检测和强制切换
- [x] C++ 编译标准升级到 c++17
- [x] CMakeLists.txt Qt6 分支不链接 Qt5WinExtras
- [x] CMakeLists.txt 包含 wintaskbarprogress 源文件

## 兼容性辅助层
- [x] qtcompat.h 辅助头文件创建完成，定义 NativeEventResult 类型和事件坐标兼容宏

## nativeEvent 签名
- [x] 所有 nativeEvent 签名在 Qt6 下使用 qintptr*，Qt5 下使用 long*（mainframe, submainframe, winshadow）

## WinTaskbarProgress
- [x] WinTaskbarProgress 类实现 ITaskbarList3 COM 接口，替代 QWinTaskbarButton/QWinTaskbarProgress
- [x] mainframe.h/cpp 中所有 QWinTaskbarButton/QWinTaskbarProgress 引用已替换

## QMouseEvent 迁移
- [x] 所有 QMouseEvent::globalPos() 调用已替换为版本兼容写法（含 mainframe, submainframe, titlebar, slidingdrawer, draggabletabbar）
- [x] 所有 QMouseEvent::pos() 和 event->x()/y() 调用已替换为版本兼容写法（含 titlebar, viewport, header, ruler, view, draggabletabbar 等）

## QTextCodec → QStringConverter
- [x] encoding.cpp 中 QTextCodec 全部替换为 QStringConverter，无 Qt5Compat 依赖
- [x] path.cpp 中 QTextCodec::codecForName("System") 替换为 QStringEncoder
- [x] storesession.cpp 中残留 QTextCodec include 已移除
- [x] logdock.cpp 中无用 `#include <QTextCodec>` 已移除

## QSignalMapper → lambda
- [x] decodermenu.cpp 中 QSignalMapper 实现已替换为 lambda connect
- [x] decodetrace.h 中残留 QSignalMapper include 已移除
- [x] decodermenu.h 中无用 `#include <QSignalMapper>` 已移除

## QFontDatabase
- [x] main.cpp 中 QFontDatabase 静态方法在 Qt6 下使用实例方法

## High DPI
- [x] main.cpp 中 High DPI 属性调用受 QT_VERSION 守卫保护

## QDesktopWidget
- [x] winnativewidget.cpp 中残留 QDesktopWidget include 已移除
- [x] mainframe.cpp/mainwindow.cpp 中 QDesktopWidget 有版本守卫

## QPixmap::grabWidget
- [x] mainwindow.cpp 中 QPixmap::grabWidget 替换为 QWidget::grab()
- [x] mainwindow.cpp 中 Qt6 截图分支的 QApplication::desktop->winId() BUG 已修复

## SIGNAL/SLOT 迁移
- [x] 所有 SIGNAL()/SLOT() 旧式字符串语法已替换为新式仿函数语法（514+ 处）

## 风格统一
- [x] viewport.cpp 滚轮事件处理统一使用 QT_COMPAT_WHEEL_* 宏替代内联版本检查

## Qt6 特有 API 迁移
- [x] enterEvent(QEvent*) → enterEvent(QEnterEvent*) 签名变更（sidebarbutton.h/cpp, hoversplitter.h）
- [x] QButtonGroup::buttonClicked(int) → idClicked(int)（samplingbar.cpp）
- [x] QtConcurrent::run(this, &method) → QtConcurrent::run(lambda)（searchdock.cpp）
- [x] QKeyEvent 构造函数 0 → Qt::NoModifier（winnativewidget.cpp）
- [x] QContextMenuEvent 使用 pos()/globalPos() 而非 QT_COMPAT_POS 宏（draggabletabbar.cpp）

## 编译验证
- [x] Qt5 编译通过（MSYS2 MinGW64, Qt5 5.15.18）
- [x] Qt6 编译通过（MSYS2 MinGW64, Qt6 6.11.0）
