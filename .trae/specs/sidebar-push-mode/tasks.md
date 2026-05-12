# Tasks

- [ ] Task 1: 修改 SlidingDrawer 类头文件
  - [ ] SubTask 1.1: 在 `slidingdrawer.h` 中添加 `drawerWidthChanged(int width)` 信号声明
  - [ ] SubTask 1.2: 在 `slidingdrawer.h` 中添加 `visibleWidth() const` 方法声明

- [ ] Task 2: 修改 SlidingDrawer 类实现
  - [ ] SubTask 2.1: 修改 `updatePanelGeometry()` 方法，计算可见宽度并发射信号
  - [ ] SubTask 2.2: 实现 `visibleWidth() const` 方法，返回当前可见宽度
  - [ ] SubTask 2.3: 在拖拽调整宽度时实时发射 `drawerWidthChanged` 信号

- [ ] Task 3: 修改 MainWindow 头文件
  - [ ] SubTask 3.1: 在 `mainwindow.h` 中添加 `on_drawer_width_changed(int width)` 槽函数声明

- [ ] Task 4: 修改 MainWindow 实现
  - [ ] SubTask 4.1: 在 `setup_ui()` 中连接 `SlidingDrawer::drawerWidthChanged` 信号到槽函数
  - [ ] SubTask 4.2: 实现 `on_drawer_width_changed(int width)` 槽函数，调整 `_tab_widget` 的右边距
  - [ ] SubTask 4.3: 确保动画过程中 `_tab_widget` 的右边距平滑过渡

- [ ] Task 5: 编译验证
  - [ ] SubTask 5.1: 确保项目可以正常编译，无语法错误
  - [ ] SubTask 5.2: 确保无未使用变量警告
  - [ ] SubTask 5.3: 确保抽屉打开时主视图宽度正确缩小
  - [ ] SubTask 5.4: 确保抽屉关闭时主视图宽度恢复全宽
  - [ ] SubTask 5.5: 确保拖拽调整抽屉宽度时主视图实时同步

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 4] depends on [Task 2] and [Task 3]
- [Task 5] depends on [Task 4]
