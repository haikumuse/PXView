# Tasks

- [x] Task 1: View 层新增 visible_range_changed 信号 + debounce 机制
  - [ ] SubTask 1.1: 在 `view.h` 的 `Q_SIGNALS` 区新增 `void visible_range_changed()` 信号；在 private 成员区新增 `QTimer *_viewport_change_timer`（单触发，100ms）
  - [ ] SubTask 1.2: 在 `view.cpp` 构造函数初始化 `_viewport_change_timer = new QTimer(this); _viewport_change_timer->setSingleShot(true); _viewport_change_timer->setInterval(100); connect(_viewport_change_timer, &QTimer::timeout, this, [this]{ emit visible_range_changed(); });`
  - [ ] SubTask 1.3: 在 `View::set_scale_offset()` 的 `if (_scale != _preScale || _offset != _preOffset)` 块尾调用 `_viewport_change_timer->start();`
  - [ ] SubTask 1.4: 在 `View::h_scroll_value_changed()` 直接赋值 `_offset` 之后调用 `_viewport_change_timer->start();`
  - [ ] SubTask 1.5: 在 `View::update_scale_offset()` 末尾调用 `_viewport_change_timer->start();`
  - [ ] SubTask 1.6: 在 `View::limit_scale_offset()` 末尾调用 `_viewport_change_timer->start();`
  - [ ] SubTask 1.7: 编译验证 `cd build && ninja -j 16` 通过

- [x] Task 2: DecoderModel 新增可视范围切片接口
  - [ ] SubTask 2.1: 在 `decodermodel.h` private 成员区新增 `int64_t _visible_start_row = -1;` `int64_t _visible_end_row = -1;`
  - [ ] SubTask 2.2: 在 `decodermodel.h` public 接口新增 `void set_visible_range(int64_t start_row, int64_t end_row);` `void clear_visible_range();`
  - [ ] SubTask 2.3: 在 `decodermodel.cpp` 实现 `set_visible_range`（保存参数 + `beginResetModel`/`endResetModel`）和 `clear_visible_range`（设 `_visible_start_row = -1` + reset）
  - [ ] SubTask 2.4: 修改 `DecoderModel::rowCount()`：若 `_visible_start_row >= 0`，返回 `min(_visible_end_row - _visible_start_row, 全量行数 - _visible_start_row)`；否则保持原逻辑
  - [ ] SubTask 2.5: 修改 `DecoderModel::data()`：若 `_visible_start_row >= 0`，实际查询行号 = `_visible_start_row + index.row()`；否则保持原逻辑
  - [ ] SubTask 2.6: 编译验证 `cd build && ninja -j 16` 通过

- [x] Task 3: ProtocolDock 新增"列表跟随视口"开关按钮 + connect/disconnect
  - [ ] SubTask 3.1: 在 `protocoldock.h` private 成员区新增 `QToolButton *_follow_viewport_btn;` `bool _follow_viewport = true;` `bool _jumping_to_row = false;`
  - [ ] SubTask 3.2: 在 `protocoldock.h` private slots 新增 `void on_follow_viewport_toggled(bool checked);` `void on_visible_range_changed();`
  - [ ] SubTask 3.3: 在 `protocoldock.cpp` 工具栏构建处新增 `QToolButton`（checkable，默认 checked，tooltip "列表跟随波形可视范围过滤"），与现有按钮风格一致
  - [ ] SubTask 3.4: 在 `bind_context` 中 connect `_view->visible_range_changed` 到 `on_visible_range_changed`（仅当 `_follow_viewport` 为 true）；connect 按钮 toggled 到 `on_follow_viewport_toggled`
  - [ ] SubTask 3.5: 实现 `on_follow_viewport_toggled`：checked=true 时 connect 信号 + 立即调一次 `on_visible_range_changed()`；checked=false 时 disconnect 信号 + 调 `_decoder_model->clear_visible_range()`
  - [ ] SubTask 3.6: 编译验证 `cd build && ninja -j 16` 通过

- [x] Task 4: 实现可视范围过滤槽函数
  - [ ] SubTask 4.1: 实现 `on_visible_range_changed()`：
    - 取 `decoder_stack = _decoder_model->getDecoderStack()`；if(!decoder_stack) return;
    - 取 samplerate = `decoder_stack->samplerate()`；if(samplerate == 0) return;
    - 计算 `samples_per_pixel = samplerate * _view->scale()`
    - 计算 `start_sample = (uint64_t)max(_view->offset() * samples_per_pixel, 0.0)`
    - 计算 `end_sample = (uint64_t)max((_view->offset() + viewport_width) * samples_per_pixel, 0.0)`（viewport_width 取 `_view` 的 viewport 宽度）
    - 对 decoder_stack 的可见 row 调 `get_row_data(row)->get_visible_range(start_sample, end_sample)` 取 `[start_idx, end_idx)`
    - 调 `_decoder_model->set_visible_range(start_idx, end_idx)`
  - [ ] SubTask 4.2: 处理 mark_index 豁免：若 `decoder_stack->get_mark_index()` 不在切片范围，扩展 end_idx 包含 mark 行
  - [ ] SubTask 4.3: 编译验证 `cd build && ninja -j 16` 通过

- [x] Task 5: 处理 item_clicked 跳转过程的循环触发
  - [ ] SubTask 5.1: 在 `item_clicked` 调用 `show_region` 前设 `_jumping_to_row = true`，记录当前选中 QModelIndex
  - [ ] SubTask 5.2: 在 `on_visible_range_changed` 槽中检查 `_jumping_to_row`：若 true，调 `set_visible_range` 后用 `blockSignals` + `setCurrentIndex` 恢复选中行（若新范围包含该行）；并用 `QTimer::singleShot(150, ...)` 在跳转结束后清除 `_jumping_to_row`
  - [ ] SubTask 5.3: 编译验证 `cd build && ninja -j 16` 通过

- [x] Task 6: 端到端验证 + 安装
  - [x] SubTask 6.1: 执行 `cd build && ninja -j 16 && ninja install` 完整构建安装
  - [ ] SubTask 6.2: 启动 PXView.exe，加载一段解码数据，验证开关 ON 时拖动波形列表跟随、开关 OFF 时列表全量、点击列表项跳转不丢失选中行

# Task Dependencies
- Task 2 独立，可与 Task 1 并行
- Task 3 依赖 Task 1（需要 View 信号）和 Task 2（需要 DecoderModel 接口）
- Task 4 依赖 Task 3
- Task 5 依赖 Task 4
- Task 6 依赖所有前置 Task
