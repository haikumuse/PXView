# Tasks

- [x] Task 1: 修改 DsoSignal::get_view_rect() 基于信号自身区域
  - [x] SubTask 1.1: 将 `get_view_rect()` 返回值从 `QRect(0, UpMargin, _viewport->width()-RightMargin, _viewport->height()-UpMargin-DownMargin)` 改为 `QRect(0, get_y()-get_totalHeight()/2, _viewport->width()-RightMargin, get_totalHeight())`

- [x] Task 2: 修改 View::signals_changed() 中 DSO 的 set_scale 调用
  - [x] SubTask 2.1: 将 `sig->set_scale(sig->get_view_rect().height())` 改为 `sig->set_scale(sig->get_totalHeight())`

- [x] Task 3: 修改 DsoSignal::paint_back() 中的 UpMargin 引用
  - [x] SubTask 3.1: 将 `p.drawRect(left, UpMargin, width, height)` 中的 `UpMargin` 替换为 `get_view_rect().top()`
  - [x] SubTask 3.2: 将缩放指示器中的 `UpMargin/2` 替换为信号区域顶部
  - [x] SubTask 3.3: 将网格线中的 `UpMargin` 替换为 `get_view_rect().top()`

- [x] Task 4: 验证 DSO 信号 go_vDialPre/go_vDialNext 中的 set_scale 调用
  - [x] SubTask 4.1: 确认 `set_scale(get_view_rect().height())` 在新坐标系统下仍然正确（因为 get_view_rect() 已改为基于信号自身区域）

- [x] Task 5: 编译验证
  - [x] SubTask 5.1: 执行 `build_incremental.cmd`，确认编译通过无错误

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 1]
- [Task 4] depends on [Task 1]

# Parallelizable Work
- Task 2, Task 3, Task 4 可在 Task 1 完成后并行执行
