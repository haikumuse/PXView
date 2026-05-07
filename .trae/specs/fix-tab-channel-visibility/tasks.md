# Tasks

## Phase 1: Trace 添加独立可见性字段

- [x] Task 1.1: 在 Trace 类中添加 `_visible` 字段和访问方法
- [x] Task 1.2: 在 signals_changed() 布局计算中使用 `visible()` 替代 `enabled()` 判断可见性

## Phase 2: Signal 添加独立启用状态

- [x] Task 2.1: 在 Signal 类中添加 `_local_enabled` 字段和 `set_enabled()` 方法
- [x] Task 2.2: 修改 Signal::enabled() 使用 _local_enabled
- [x] Task 2.3: 修改各 Signal 子类的 clone() 方法拷贝 _local_enabled 和 _visible

## Phase 3: View 信号重建逻辑重构

- [x] Task 3.1: 添加 View::rebuild_signals() 方法
- [x] Task 3.2: 修改 View::signals_changed() 移除信号克隆逻辑
- [x] Task 3.3: 修改 View::set_data_source() 调用 rebuild_signals()

## Phase 4: 设备通道变更处理

- [x] Task 4.1: 修改 MainWindow::OnMessage() 中 DSV_MSG_DEVICE_OPTIONS_UPDATED 处理

## Phase 5: 标签切换时信号可见性保持

- [x] Task 5.1: 修改 TabContext::activate() 确保历史标签信号不被重建

## Phase 6: 编译验证

- [x] Task 6.1: 全量编译验证
  - 所有修改的 C++ 文件编译通过（0 编译错误）
  - srd.c 的编译错误是预已存在的 Python 3.14 API 兼容性问题，与本次修改无关
