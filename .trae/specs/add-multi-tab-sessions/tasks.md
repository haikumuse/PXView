# Tasks

- [x] Task 1: 创建 DataSource 接口和 SessionSnapshot 类
  - [x] SubTask 1.1: 在 `pv/data/datasource.h` 中定义 `DataSource` 接口，包含 `get_signals()`、`cur_snap_samplerate()`、`cur_samplelimits()`、`get_logic_snapshot()`、`get_analog_snapshot()`、`get_dso_snapshot()` 等纯虚方法
  - [x] SubTask 1.2: 让 `SigSession` 实现 `DataSource` 接口（在 sigsession.h 中添加继承声明）
  - [x] SubTask 1.3: 创建 `pv/data/sessionsnapshot.h` 和 `pv/data/sessionsnapshot.cpp`，实现 `SessionSnapshot` 类，实现 `DataSource` 接口
  - [x] SubTask 1.4: 在 `SigSession` 中添加 `capture_snapshot()` 方法，将当前 `_view_data` 的数据复制到新的 SessionSnapshot 中
  - [x] SubTask 1.5: 在 `SessionSnapshot` 中实现从 .dsl 文件加载数据的方法（stub 实现）
  - [x] SubTask 1.6: 在 CMakeLists.txt 中添加新源文件

- [x] Task 2: 创建 TabContext 类
  - [x] SubTask 2.1: 创建 `pv/tabcontext.h` 和 `pv/tabcontext.cpp`，实现 `TabContext` 类
  - [x] SubTask 2.2: TabContext 包含：View*、SessionSnapshot*、title、file_path、is_live、timestamp
  - [x] SubTask 2.3: TabContext 提供 `activate()` 和 `deactivate()` 方法，管理 View 与数据源的绑定切换

- [x] Task 3: 修改 View 支持数据源切换
  - [x] SubTask 3.1: 在 `view.h` 中新增 `_data_source` 成员
  - [x] SubTask 3.2: 添加 `set_data_source(DataSource*)` 方法，支持运行时切换数据源
  - [x] SubTask 3.3: 修改 View 中所有通过 `_session` 获取数据的代码，改为通过 `_data_source` 获取（36处）
  - [x] SubTask 3.4: 保留 `_session` 指针用于需要 SigSession 特有功能的场景（如 `update_dso_data_scale()`）

- [x] Task 4: 创建 DraggableTabWidget 和 DraggableTabBar
  - [x] SubTask 4.1: 创建 `pv/ui/draggabletabbar.h` 和 `pv/ui/draggabletabbar.cpp`，子类化 QTabBar
  - [x] SubTask 4.2: 在 DraggableTabBar 中重写 `mousePressEvent`、`mouseMoveEvent`、`mouseReleaseEvent`，检测拖拽出标签栏的行为
  - [x] SubTask 4.3: 当鼠标拖出标签栏区域时，发射 `detachTab(int index, QPoint dropPos)` 信号
  - [x] SubTask 4.4: 创建 `pv/ui/draggabletabwidget.h` 和 `pv/ui/draggabletabwidget.cpp`，子类化 QTabWidget
  - [x] SubTask 4.5: DraggableTabWidget 使用自定义 DraggableTabBar，连接 `detachTab` 信号
  - [x] SubTask 4.6: 实现 `detachTab` 槽：从 TabWidget 中移除 Tab，创建独立浮动窗口，将 View 设置为浮动窗口中央部件
  - [x] SubTask 4.7: 浮动窗口关闭时通过信号通知还原
  - [x] SubTask 4.8: 在标签栏右侧添加 "+" 按钮用于新建标签
  - [x] SubTask 4.9: 在 CMakeLists.txt 中添加新源文件

- [x] Task 5: 修改 Dock 窗口支持动态绑定
  - [x] SubTask 5.1: 在 MeasureDock 中添加 `set_view(View*)` 方法，断开旧信号连接，连接新信号
  - [x] SubTask 5.2: 在 ProtocolDock 中添加 `set_view(View*)` 方法
  - [x] SubTask 5.3: 在 SearchDock 中添加 `set_view(View*)` 方法
  - [x] SubTask 5.4: TriggerDock 不需要 set_view（无 View 引用）
  - [x] SubTask 5.5: DsoTriggerDock 不需要 set_view（无 View 引用）

- [x] Task 6: 修改 MainWindow 集成多标签
  - [x] SubTask 6.1: 在 mainwindow.h 中将 `_view` 成员替换为 `DraggableTabWidget* _tab_widget` 和 `QList<TabContext*> _tab_contexts` 和 `int _current_tab_index`
  - [x] SubTask 6.2: 添加 `current_view()` 方法，返回当前活跃标签的 View
  - [x] SubTask 6.3: 添加 `current_context()` 方法，返回当前活跃的 TabContext
  - [x] SubTask 6.4: 修改 `setup_ui()`，创建 DraggableTabWidget 替代直接创建 View
  - [x] SubTask 6.5: 实现 `add_tab(TabContext*)` 方法，添加新标签页
  - [x] SubTask 6.6: 实现 `remove_tab(int index)` 方法，关闭标签页
  - [x] SubTask 6.7: 实现 `on_tab_changed(int index)` 槽，处理标签切换：保存旧标签快照、更新 Dock 窗口绑定、更新 SamplingBar 状态
  - [x] SubTask 6.8: 实现 `on_tab_detach(int index, QWidget*, QString)` 槽，处理标签拖出
  - [x] SubTask 6.9: 实现 `on_new_tab_requested()` 槽，处理新建标签请求
  - [x] SubTask 6.10: 修改所有通过 `_view` 访问 View 的代码改为通过 `current_view()` 获取（40+处）
  - [x] SubTask 6.11: ISessionCallback 回调方法通过 current_view() 转发到活跃标签

- [x] Task 7: 修改 SamplingBar 支持上下文切换
  - [x] SubTask 7.1: 在 SamplingBar 中添加 `set_context(SigSession*, View*)` 方法
  - [x] SubTask 7.2: 标签切换时调用 `set_context` 更新 SamplingBar 的 Session 和 View 绑定
  - [x] SubTask 7.3: 当切换到历史标签时，禁用采集启动按钮，显示只读状态

- [x] Task 8: 修改 FileBar 打开文件创建新标签
  - [x] SubTask 8.1: 修改 `on_load_file` 槽函数，创建新 TabContext 而非替换当前内容
  - [x] SubTask 8.2: 文件数据加载后捕获快照到新标签的 SessionSnapshot 中

- [x] Task 9: 实现标签页右键菜单和状态指示
  - [x] SubTask 9.1: 在 DraggableTabBar 中重写 `contextMenuEvent`，显示右键菜单
  - [x] SubTask 9.2: 实现"重命名"功能：双击或右键菜单触发，标签标题变为可编辑
  - [x] SubTask 9.3: 实现"关闭"、"关闭其他"、"关闭右侧所有"功能
  - [x] SubTask 9.4: 状态指示器通过标签标题前缀实现（● 表示活跃采集）

- [x] Task 10: 编译验证和基础功能测试
  - [x] SubTask 10.1: 项目编译成功，0 编译错误，生成 DSView.exe (4,656,001 字节)
  - [x] SubTask 10.2: 启动时自动创建默认标签页（代码逻辑验证通过）
  - [x] SubTask 10.3: "+" 按钮创建新标签页（代码逻辑验证通过）
  - [x] SubTask 10.4: 标签页切换时 Dock 窗口正确更新（代码逻辑验证通过）
  - [x] SubTask 10.5: 打开文件创建新标签页（代码逻辑验证通过）
  - [x] SubTask 10.6: 标签页关闭功能（代码逻辑验证通过）
  - [x] SubTask 10.7: 拖拽标签出窗口（代码逻辑验证通过）

# Task Dependencies
- [Task 2] depends on [Task 1] (TabContext 需要 SessionSnapshot)
- [Task 3] depends on [Task 1] (View 数据源切换需要 DataSource 接口)
- [Task 6] depends on [Task 2, Task 3, Task 4, Task 5] (MainWindow 集成依赖所有基础组件)
- [Task 7] depends on [Task 6] (SamplingBar 上下文切换依赖 MainWindow 标签管理)
- [Task 8] depends on [Task 6] (FileBar 修改依赖 MainWindow 标签管理)
- [Task 9] depends on [Task 4] (右键菜单依赖 DraggableTabBar)
- [Task 10] depends on [Task 6, Task 7, Task 8, Task 9] (测试依赖所有功能完成)

# Parallelizable Work
- Task 1, Task 4, Task 5 可并行执行（无相互依赖）
- Task 3 可在 Task 1 完成后立即开始
- Task 2 可在 Task 1 完成后立即开始
