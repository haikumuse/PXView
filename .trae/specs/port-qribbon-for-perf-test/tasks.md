# Tasks

- [ ] Task 1: 复制 QRibbon 源文件到当前项目
  - [ ] SubTask 1.1: 创建 PXView/pv/QRibbon/ 目录
  - [ ] SubTask 1.2: 复制 QRibbon.h、QRibbon.cpp、qribbon.ui 到该目录
  - [ ] SubTask 1.3: 修改 QRibbon.cpp 中的 include 路径，适配当前项目的 log.h（dsv_info）

- [ ] Task 2: 修改 CMakeLists.txt 添加 QRibbon 源文件
  - [ ] SubTask 2.1: 在 CMakeLists.txt 中添加 QRibbon.cpp、qribbon.ui

- [ ] Task 3: 修改 MainWindow 集成 QRibbon
  - [ ] SubTask 3.1: 在 mainwindow.h 中添加 QRibbon 前向声明和成员变量 _QRibbon
  - [ ] SubTask 3.2: 在 mainwindow.cpp 中恢复 MainWindowRibbonHelper()（创建 QMenuBar + QMenu）
  - [ ] SubTask 3.3: 在构造函数中调用 _QRibbon->install(this)
  - [ ] SubTask 3.4: 移除 setMenuBar(nullptr) 调用

- [ ] Task 4: 编译测试
  - [ ] SubTask 4.1: 编译并安装
  - [ ] SubTask 4.2: 运行程序，点击 QRibbon Tab，观察 Viewport paintEvent 日志帧率

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 1]
- [Task 4] depends on [Task 2, Task 3]
