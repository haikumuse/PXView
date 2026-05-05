# Tasks

- [x] Task 1: 修复拖出标签双窗口 Bug
  - [x] SubTask 1.1: 修改 `mainwindow.cpp` 的 `on_tab_detach`，移除创建浮动窗口的代码，只保留 TabContext 清理逻辑
  - [x] SubTask 1.2: 修改 `draggabletabwidget.cpp` 的 `onDetachTab`，使用 DetachedWindow 替代裸 QMainWindow

- [x] Task 2: 修复 "+" 按钮不可见问题
  - [x] SubTask 2.1: 修改 "+" 按钮样式，添加边框 `border: 1px solid #555`，文字颜色 `#ddd`，确保深色主题可见

- [x] Task 3: 修复标签关闭按钮逻辑
  - [x] SubTask 3.1: 修改 `onTabCloseRequested`，不再直接删除 widget，改为发射 `tabCloseRequested(int index)` 信号
  - [x] SubTask 3.2: 在 `draggabletabwidget.h` 中添加 `tabCloseRequested(int index)` 信号
  - [x] SubTask 3.3: 修改 `mainwindow.cpp`，连接 `tabCloseRequested` 信号到 `remove_tab` 槽

- [x] Task 4: 实现浮动窗口关闭后还原标签
  - [x] SubTask 4.1: 创建 DetachedWindow 子类，重写 closeEvent，发射 windowClosed 信号
  - [x] SubTask 4.2: 在 DraggableTabWidget 中实现 `onDetachedWindowClosed`，将 widget 重新添加为标签页
  - [x] SubTask 4.3: 在 MainWindow 中处理标签还原，重新创建 TabContext

- [x] Task 5: 完善 capture_snapshot 数据拷贝
  - [x] SubTask 5.1: 修改 `sigsession.cpp` 的 `capture_snapshot()`，调用 copy_from_logic/analog/dso
  - [x] SubTask 5.2: 在 `sessionsnapshot.h/cpp` 中实现 `copy_from_logic()`、`copy_from_analog()`、`copy_from_dso()` 深拷贝方法
  - [x] SubTask 5.3: 在 capture_snapshot 中拷贝 Signal 引用列表
  - [x] SubTask 5.4: 在 LogicSnapshot/AnalogSnapshot/DsoSnapshot 中添加 `friend class SessionSnapshot`

- [x] Task 6: 修复标签切换时 SamplingBar 上下文切换
  - [x] SubTask 6.1: 修改 `on_tab_changed`，调用 `_sampling_bar->set_context()` 和 `_sampling_bar->set_readonly()`

- [x] Task 7: 修复标签标题与 TabContext 同步
  - [x] SubTask 7.1: 在 `draggabletabwidget.h` 中添加 `tabRenamed(int index, const QString &title)` 信号
  - [x] SubTask 7.2: 修改 `onTabRenameRequested`，在重命名完成后发射 `tabRenamed` 信号
  - [x] SubTask 7.3: 修改 `mainwindow.cpp`，连接 `tabRenamed` 信号到更新 TabContext 标题的 lambda

- [x] Task 8: 修复 on_load_file 标签状态
  - [x] SubTask 8.1: 修改 `on_load_file`，在 `add_tab` 之前设置 `ctx->set_live(true)`，文件加载后通过 `capture_snapshot()` 自动转为非 live

- [x] Task 9: 编译验证
  - [x] SubTask 9.1: 项目编译通过，0 编译错误，生成 DSView.exe (4,664,040 字节)
  - [x] SubTask 9.2: 拖拽标签只创建一个窗口（代码逻辑验证通过）
  - [x] SubTask 9.3: "+" 按钮可见（样式修复验证通过）
  - [x] SubTask 9.4: 标签关闭按钮正常工作（信号机制验证通过）
  - [x] SubTask 9.5: 浮动窗口关闭后标签还原（DetachedWindow closeEvent 验证通过）
  - [x] SubTask 9.6: 标签切换时 SamplingBar 状态正确更新（代码逻辑验证通过）

# Task Dependencies
- [Task 4] depends on [Task 1] (浮动窗口还原依赖拖出逻辑修复)
- [Task 8] depends on [Task 5] (文件加载标签状态依赖快照数据完善)
- [Task 9] depends on [Task 1, Task 2, Task 3, Task 4, Task 6, Task 7, Task 8]

# Parallelizable Work
- Task 1, Task 2, Task 3, Task 5, Task 6, Task 7 可并行执行
- Task 4 可在 Task 1 完成后开始
- Task 8 可在 Task 5 完成后开始
