# Tasks

- [ ] Task 1: 恢复 CMakeLists.txt Qt5/Qt6 双版本构建逻辑
  - [ ] SubTask 1.1: 取消注释 Qt6 find_package 块（行 212-229），更新 Qt6 组件列表（Core, Widgets, Gui, Svg, Concurrent，不含 WinExtras）
  - [ ] SubTask 1.2: 修改 Qt 版本检测逻辑：找不到 Qt5 时尝试 Qt6，而非直接 FATAL_ERROR
  - [ ] SubTask 1.3: 添加 Qt6 分支的 qt6_wrap_cpp/qt6_add_resources（行 665-674）
  - [ ] SubTask 1.4: C++ 标准从 c++11 升级到 c++17（行 685）
  - [ ] SubTask 1.5: Windows 下 Qt5 分支保留 Qt5WinExtras，Qt6 分支不链接
  - [ ] SubTask 1.6: 验证 Qt5 和 Qt6 分别能通过 CMake 配置阶段

- [ ] Task 2: 创建 Qt 版本兼容性辅助头文件
  - [ ] SubTask 2.1: 创建 `PXView/pv/ui/qtcompat.h`，定义 NativeEventResult 类型别名、QT_COMPAT_GLOBAL_POS/QT_COMPAT_POS/QT_COMPAT_X/QT_COMPAT_Y 宏
  - [ ] SubTask 2.2: 在需要 Qt 版本兼容的源文件中引入此头文件

- [ ] Task 3: 修复 nativeEvent 签名（4 个文件）
  - [ ] SubTask 3.1: mainframe.h — 声明使用 NativeEventResult 类型
  - [ ] SubTask 3.2: mainframe.cpp — 实现使用 NativeEventResult 类型，修复 *result 赋值和基类调用
  - [ ] SubTask 3.3: submainframe.h — 声明使用 NativeEventResult 类型
  - [ ] SubTask 3.4: submainframe.cpp — 实现使用 NativeEventResult 类型，修复基类调用
  - [ ] SubTask 3.5: winshadow.h — 声明使用 NativeEventResult 类型
  - [ ] SubTask 3.6: winshadow.cpp — 实现使用 NativeEventResult 类型

- [ ] Task 4: 替换 Qt5WinExtras 为 Win32 ITaskbarList3 原生实现
  - [ ] SubTask 4.1: 创建 `PXView/pv/wintaskbarprogress.h/cpp`，封装 ITaskbarList3 COM 接口
  - [ ] SubTask 4.2: mainframe.h — 移除 QWinTaskbarButton/QWinTaskbarProgress include 和成员，替换为 WinTaskbarProgress
  - [ ] SubTask 4.3: mainframe.cpp — 替换所有 _taskBtn/_taskPrg 调用为 WinTaskbarProgress API
  - [ ] SubTask 4.4: CMakeLists.txt — Qt6 分支不链接 Qt5WinExtras，Win32 条件编译新文件

- [ ] Task 5: 修复 QMouseEvent::globalPos() 迁移（5 个文件，11 处）
  - [ ] SubTask 5.1: mainframe.cpp — 2 处 globalPos() 替换
  - [ ] SubTask 5.2: submainframe.cpp — 2 处 globalPos() 替换
  - [ ] SubTask 5.3: titlebar.cpp — 4 处 globalPos() 替换
  - [ ] SubTask 5.4: slidingdrawer.cpp — 3 处 globalPos() 替换
  - [ ] SubTask 5.5: draggabletabbar.cpp — 1 处 globalPos() 替换

- [ ] Task 6: 修复 QMouseEvent::pos() 和 event->x()/y() 迁移（13 个文件，85+ 处）
  - [ ] SubTask 6.1: viewport.cpp — 约 38 处 event->pos() 替换
  - [ ] SubTask 6.2: header.cpp — 约 12 处 event->pos() 和 event->x()/y() 替换
  - [ ] SubTask 6.3: ruler.cpp — 约 7 处替换
  - [ ] SubTask 6.4: view.cpp — 约 4 处替换
  - [ ] SubTask 6.5: draggabletabbar.cpp — 约 9 处替换
  - [ ] SubTask 6.6: titlebar.cpp — 约 4 处替换
  - [ ] SubTask 6.7: 其他文件（viewstatus, keywordlineedit, sidebarbutton, searchcombobox, devmode, debughelper, mainwindow）少量替换

- [ ] Task 7: 替换 QTextCodec 为 QStringConverter（3 个文件）
  - [ ] SubTask 7.1: encoding.cpp — init() 函数的 Win32 分支改用 QStringConverter；移除无守卫的 QTextCodec include
  - [ ] SubTask 7.2: path.cpp — ToUnicodePath() 函数改用 QStringEncoder 替代 QTextCodec::codecForName("System")
  - [ ] SubTask 7.3: storesession.cpp — 移除残留的 QTextCodec include

- [ ] Task 8: 替换 QSignalMapper 为 lambda connect（2 个文件）
  - [ ] SubTask 8.1: decodermenu.h/cpp — 移除 QSignalMapper 成员，改用 lambda connect(action, &QAction::triggered, [this, action]() { on_action(action); })
  - [ ] SubTask 8.2: decodetrace.h — 移除残留的 QSignalMapper include

- [ ] Task 9: 修复 QFontDatabase 静态方法（1 个文件）
  - [ ] SubTask 9.1: main.cpp — Qt6 下使用 QFontDatabase 实例方法替代静态方法

- [ ] Task 10: 修复 High DPI 属性和 QDesktopWidget 残留（2 个文件）
  - [ ] SubTask 10.1: main.cpp — 用 QT_VERSION 守卫包裹 AA_EnableHighDpiScaling/AA_DisableHighDpiScaling/AA_UseHighDpiPixmaps
  - [ ] SubTask 10.2: winnativewidget.cpp — 移除无守卫的 QDesktopWidget include

- [ ] Task 11: 修复 QPixmap::grabWidget 和截图 API（1 个文件）
  - [ ] SubTask 11.1: mainwindow.cpp — Qt6 分支修复 QApplication::desktop->winId() BUG，替换 QPixmap::grabWidget 为 QWidget::grab()

- [ ] Task 12: 迁移 SIGNAL/SLOT 旧式语法到新式仿函数语法（50 个文件，602 处）
  - [ ] SubTask 12.1: mainwindow.cpp — 54 处迁移
  - [ ] SubTask 12.2: protocoldock.cpp — 34 处迁移
  - [ ] SubTask 12.3: measuredock.cpp — 56 处迁移
  - [ ] SubTask 12.4: triggerdock.cpp — 38 处迁移
  - [ ] SubTask 12.5: samplingbar.cpp — 26 处迁移
  - [ ] SubTask 12.6: dsotriggerdock.cpp — 46 处迁移
  - [ ] SubTask 12.7: 其他文件（logdock, logobar, viewport, view, deviceoptionsdock, filebar, trigbar, titlebar 等 43 个文件）剩余迁移

- [ ] Task 13: 编译验证
  - [ ] SubTask 13.1: 使用 Qt5 编译通过
  - [ ] SubTask 13.2: 使用 Qt6 编译通过
  - [ ] SubTask 13.3: 修复编译过程中发现的遗漏问题

# Task Dependencies
- [Task 2] depends on [Task 1] (需要先确定 CMake 配置)
- [Task 3-11] depends on [Task 2] (使用 qtcompat.h 辅助宏)
- [Task 4] depends on [Task 1] (CMake 需先配置好 Qt6 分支)
- [Task 12] 可与 Task 3-11 并行执行（无文件交叉依赖）
- [Task 13] depends on [Task 1-12] (所有修改完成后验证)
