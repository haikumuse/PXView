# Tasks

- [ ] Task 1: LogicSnapshot 数据修改基础设施
  - [ ] SubTask 1.1: 在 `logicsnapshot.h` 中声明 `set_sample_range(uint64_t start, uint64_t end, bool level, int sig_index)` 方法
  - [ ] SubTask 1.2: 在 `logicsnapshot.cpp` 中实现 `set_sample_range()`，处理跨 LeafBlock、已压缩块重新分配、位级写入
  - [ ] SubTask 1.3: 在 `logicsnapshot.h` 中声明 `recalc_mipmap(unsigned int order, uint64_t index0, uint64_t index1)` 方法
  - [ ] SubTask 1.4: 在 `logicsnapshot.cpp` 中实现 `recalc_mipmap()`，基于现有 `calc_mipmap()` 逻辑，对指定 LeafBlock 完整重建 4 级 mipmap 和 RootNode 元数据
  - [ ] SubTask 1.5: 在 `logicsnapshot.h` 中声明 `clone_data()` 方法，实现 LogicSnapshot 的深拷贝（复制所有通道数据、mipmap、RootNode 元数据）
  - [ ] SubTask 1.6: 编写单元测试验证 `set_sample_range()` 和 `recalc_mipmap()` 的正确性（修改数据后边沿搜索结果一致）

- [ ] Task 2: LogicSnapshot 毛刺滤波核心算法
  - [ ] SubTask 2.1: 在 `logicsnapshot.h` 中声明 `apply_glitch_filter(int sig_index, uint32_t threshold, std::function<void(int)> progress_callback)` 方法
  - [ ] SubTask 2.2: 在 `logicsnapshot.cpp` 中实现 `apply_glitch_filter()`：利用 `get_nxt_edge_self()` 逐段扫描，翻转短脉冲，回退检查级联效应，完成后对受影响 LeafBlock 调用 `recalc_mipmap()`
  - [ ] SubTask 2.3: 在 `logicsnapshot.h` 中声明 `apply_glitch_filter_all(const std::vector<uint32_t> &thresholds, std::function<void(int)> progress_callback)` 方法，对多个通道依次执行滤波
  - [ ] SubTask 2.4: 编写测试用例验证滤波算法：单脉冲滤除、级联滤除、全通（阈值=0）、边界条件

- [ ] Task 3: SigSession 滤波流程控制
  - [ ] SubTask 3.1: 在 `sigsession.h` 的 `SessionData` 中新增 `LogicSnapshot *_logic_backup` 指针和 `bool _glitch_filter_active` 标志
  - [ ] SubTask 3.2: 在 `sigsession.h` 中声明 `set_glitch_filter(const std::vector<uint32_t> &thresholds)` 和 `clear_glitch_filter()` 方法
  - [ ] SubTask 3.3: 在 `sigsession.cpp` 中实现 `set_glitch_filter()`：克隆 LogicSnapshot → 启动后台线程滤波 → 切换 view_data → 保留原始备份
  - [ ] SubTask 3.4: 在 `sigsession.cpp` 中实现 `clear_glitch_filter()`：恢复原始备份 → 释放克隆数据 → 重置标志
  - [ ] SubTask 3.5: 在采集开始时（`start_capture()` 相关流程中）清除滤波状态，释放备份数据
  - [ ] SubTask 3.6: 在 `icallbacks.h` 中新增消息常量 `DSV_MSG_GLITCH_FILTER_STARTED`、`DSV_MSG_GLITCH_FILTER_PROGRESS`、`DSV_MSG_GLITCH_FILTER_COMPLETED`、`DSV_MSG_GLITCH_FILTER_CLEARED`

- [ ] Task 4: DeviceOptionsDock 毛刺过滤 UI
  - [ ] SubTask 4.1: 在 `deviceoptionsdock.h` 中新增毛刺过滤相关成员变量：`QGroupBox *_glitch_filter_group`、`std::vector<QCheckBox*> _glitch_checkBox_list`、`std::vector<QSpinBox*> _glitch_spinbox_list`、`QPushButton *_apply_filter_btn`、`QPushButton *_restore_data_btn`、`QLabel *_filter_status_label`
  - [ ] SubTask 4.2: 在 `deviceoptionsdock.h` 中新增私有方法 `void build_glitch_filter_panel()` 和私有槽 `void on_apply_glitch_filter()`、`void on_restore_original_data()`、`void on_glitch_select_all()`、`void on_glitch_deselect_all()`
  - [ ] SubTask 4.3: 在 `deviceoptionsdock.cpp` 中实现 `build_glitch_filter_panel()`：创建"毛刺过滤"QGroupBox，包含通道列表（每行：复选框 + "≤" + SpinBox + "采样周期"）、全选/取消全选按钮、提示文字、"应用滤波"按钮、"恢复原始数据"按钮、滤波状态标签
  - [ ] SubTask 4.4: 在构造函数和 `update_view()` 中，Logic 模式下在 `_dynamic_panel`（Channel）和 `props_box`（Mode）之间插入 `_glitch_filter_group`
  - [ ] SubTask 4.5: 实现 `on_apply_glitch_filter()`：收集各通道阈值参数，调用 `SigSession::set_glitch_filter()`
  - [ ] SubTask 4.6: 实现 `on_restore_original_data()`：调用 `SigSession::clear_glitch_filter()`
  - [ ] SubTask 4.7: 实现按钮状态管理：根据采集状态、数据状态、滤波状态控制按钮启用/禁用
  - [ ] SubTask 4.8: 处理滤波消息：`DSV_MSG_GLITCH_FILTER_COMPLETED` 时更新状态标签为"已滤波"并刷新按钮状态；`DSV_MSG_GLITCH_FILTER_CLEARED` 时更新为"未滤波"
  - [ ] SubTask 4.9: 应用项目 QSS 样式，确保毛刺过滤分组框与现有 UI 风格一致

- [ ] Task 5: 滤波设置会话保存
  - [ ] SubTask 5.1: 在 `DeviceOptionsDock::get_session()` 中将毛刺滤波参数写入 JSON 的 `glitch_filter` 字段（每通道 enable + num）
  - [ ] SubTask 5.2: 在 `DeviceOptionsDock::set_session()` 中读取 `glitch_filter` 字段，恢复 UI 控件状态
  - [ ] SubTask 5.3: 加载会话时，如果滤波参数中有通道启用，自动调用 `SigSession::set_glitch_filter()` 执行滤波

# Task Dependencies
- [Task 2] depends on [Task 1] — 滤波算法依赖数据修改和 mipmap 重建方法
- [Task 3] depends on [Task 2] — 流程控制依赖滤波核心算法
- [Task 4] depends on [Task 3] — UI 依赖流程控制完成
- [Task 5] depends on [Task 4] — 持久化依赖 UI 完成
- [Task 1] 和 [Task 4 的 UI 布局部分] 可并行执行
