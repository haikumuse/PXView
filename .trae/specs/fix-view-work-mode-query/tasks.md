# Tasks

- [x] Task 1: 在 View 中添加 get_work_mode() 方法
  - [x] SubTask 1.1: 在 `view.h` 中声明 `int get_work_mode() const;`
  - [x] SubTask 1.2: 在 `view.cpp` 中实现：优先从 `_document->get_signal_config().work_mode` 获取，否则回退到 `_device_agent->get_work_mode()`

- [x] Task 2: 替换 viewport.cpp 中的模式查询
  - [x] SubTask 2.1: 将所有 `_view.session().get_device()->get_work_mode()` 替换为 `_view.get_work_mode()`

- [x] Task 3: 替换 header.cpp 中的模式查询
  - [x] SubTask 3.1: 将所有 `_view.session().get_device()->get_work_mode()` 替换为 `_view.get_work_mode()`

- [x] Task 4: 编译验证
  - [x] SubTask 4.1: 执行 `build_incremental.cmd`，确认编译通过无错误

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 1]
- [Task 4] depends on [Task 1, Task 2, Task 3]

# Parallelizable Work
- Task 2 和 Task 3 可在 Task 1 完成后并行执行
