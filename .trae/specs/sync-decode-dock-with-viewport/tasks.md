# Tasks

- [ ] Task 1: View 层新增 visible_range_changed 信号 + debounce 机制
  - [ ] SubTask 1.1: 在 `view.h` signals 段（line 629-638）新增 `void visible_range_changed();` 信号
  - [ ] SubTask 1.2: 在 `view.h` private 成员区新增 `QTimer *_viewport_change_timer;`；在 private 方法区新增 `void schedule_visible_range_notify();`
  - [ ] SubTask 1.3: 在 `view.cpp` 构造函数初始化 `_viewport_change_timer = new QTimer(this); _viewport_change_timer->setSingleShot(true); _viewport_change_timer->setInterval(100); connect(_viewport_change_timer, &QTimer::timeout, this, [this]{ emit visible_range_changed(); });`
  - [ ] SubTask 1.4: 在 `view.cpp` 实现 `schedule_visible_range_notify()`：`_viewport_change_timer->start();`
  - [ ] SubTask 1.5: 在 `view_layout.cpp` 的 `ViewLayout::set_scale_offset`（line 55）末尾调 `_view->schedule_visible_range_notify();`
  - [ ] SubTask 1.6: 在 `view_layout.cpp` 的 `ViewLayout::limit_scale_offset`（line 79）末尾调 `_view->schedule_visible_range_notify();`
  - [ ] SubTask 1.7: 在 `view_layout.cpp` 的 `ViewLayout::update_scale_offset`（line 104）末尾调 `_view->schedule_visible_range_notify();`
  - [ ] SubTask 1.8: 在 `view_layout.cpp` 的 `ViewLayout::set_scale`（line 128）末尾调 `_view->schedule_visible_range_notify();`
  - [ ] SubTask 1.9: 在 `view_layout.cpp` 的 `ViewLayout::zoom(double steps, int offset)`（line 143）末尾调 `_view->schedule_visible_range_notify();`
  - [ ] SubTask 1.10: 在 `view_layout.cpp` 的 `ViewLayout::h_scroll_value_changed`（line 191）末尾调 `_view->schedule_visible_range_notify();`
  - [ ] SubTask 1.11: 在 `view_data_sync.cpp` 的 `ViewDataSync::resizeEvent`（line 643）末尾调 `_view->schedule_visible_range_notify();`
  - [ ] SubTask 1.12: 编译验证 `cd build && ninja -j 16` 通过

- [ ] Task 2: DecoderStack 新增 get_visible_range 包装（Core 层）
  - [ ] SubTask 2.1: 在 `decoderstack.h` public 接口区新增 `std::pair<size_t, size_t> get_visible_range(const decode::Row &row, uint64_t start_sample, uint64_t end_sample);`
  - [ ] SubTask 2.2: 在 `decoderstack.cpp` 实现转发到对应 `RowData::get_visible_range`（参考现有 `get_annotation_index` 包装的实现模式）
  - [ ] SubTask 2.3: 编译验证 `cd build && ninja -j 16` 通过

- [ ] Task 3: DecoderModel 新增可视范围切片接口
  - [ ] SubTask 3.1: 在 `decodermodel.h` private 成员区新增 `int64_t _visible_start_row = -1;` `int64_t _visible_end_row = -1;`
  - [ ] SubTask 3.2: 在 `decodermodel.h` public 接口新增 `void set_visible_range(int64_t start_row, int64_t end_row);` `void clear_visible_range();`
  - [ ] SubTask 3.3: 在 `decodermodel.cpp` 实现 `set_visible_range`（保存参数 + `beginResetModel`/`endResetModel`）和 `clear_visible_range`（设 `_visible_start_row = -1` + reset）
  - [ ] SubTask 3.4: 修改 `DecoderModel::rowCount()`：若 `_visible_start_row >= 0`，返回 `max(0, min(_visible_end_row, list_annotation_size()) - _visible_start_row)`；否则保持原逻辑
  - [ ] SubTask 3.5: 修改 `DecoderModel::data()`：若 `_visible_start_row >= 0`，实际查询行号 = `_visible_start_row + index.row()`；否则保持原逻辑
  - [ ] SubTask 3.6: 修改 `DecoderModel::headerData()` 垂直表头：若 `_visible_start_row >= 0`，返回 `section + _visible_start_row`；否则保持原逻辑
  - [ ] SubTask 3.7: 编译验证 `cd build && ninja -j 16` 通过

- [ ] Task 4: ProtocolDock 新增"列表跟随视口"开关按钮 + connect/disconnect
  - [ ] SubTask 4.1: 在 `protocoldock.h` private 成员区新增 `QToolButton *_follow_viewport_btn;` `bool _follow_viewport = true;` `bool _jumping_to_row = false;`
  - [ ] SubTask 4.2: 在 `protocoldock.h` private slots 新增 `void on_follow_viewport_toggled(bool checked);` `void on_visible_range_changed();`
  - [ ] SubTask 4.3: 在 `protocoldock.cpp` 底部面板 `bot_title_layout`（line 174-182）新增 `QToolButton`（checkable，默认 checked，tooltip "列表跟随波形可视范围过滤"），与现有按钮风格一致
  - [ ] SubTask 4.4: 在 `bind_context`（line 315-342）中 connect `_view->visible_range_changed` 到 `on_visible_range_changed`（仅当 `_follow_viewport` 为 true）；connect `_follow_viewport_btn->toggled` 到 `on_follow_viewport_toggled`
  - [ ] SubTask 4.5: 实现 `on_follow_viewport_toggled`：checked=true 时 connect 信号 + 立即调一次 `on_visible_range_changed()`；checked=false 时 disconnect 信号 + 调 `_decoder_model->clear_visible_range()`
  - [ ] SubTask 4.6: 编译验证 `cd build && ninja -j 16` 通过

- [ ] Task 5: 实现可视范围过滤槽函数
  - [ ] SubTask 5.1: 实现 `on_visible_range_changed()`：
    - 取 `decoder_stack = _decoder_model->getDecoderStack()`；if(!decoder_stack) return;
    - 取 `samplerate = decoder_stack->samplerate()`；if(samplerate == 0) return;
    - 计算 `samples_per_pixel = samplerate * _view->scale()`
    - 计算 `start_sample = (uint64_t)max(_view->offset() * samples_per_pixel, 0.0)`
    - 取 `viewport_width = _view->viewport()->width()`；if(viewport_width <= 0) return;
    - 计算 `end_sample = (uint64_t)max((_view->offset() + viewport_width) * samples_per_pixel, 0.0)`
    - 遍历 `decoder_stack->get_rows_lshow()` 取当前 `filterKeyColumn` 对应的协议行（复用 `nav_table_view` 算法）
    - 调 `decoder_stack->get_visible_range(row, start_sample, end_sample)` 取 `[start_idx, end_idx)`
    - 调 `_decoder_model->set_visible_range(start_idx, end_idx)`
  - [ ] SubTask 5.2: 处理 mark_index 豁免：`mark = decoder_stack->get_mark_index()`；若 `mark >= 0`，`mark_row = decoder_stack->get_annotation_index(row, mark)`；若 `mark_row >= end_idx`，扩展 `end_idx = mark_row + 1`；若 `mark_row < start_idx`，扩展 `start_idx = mark_row`
  - [ ] SubTask 5.3: 编译验证 `cd build && ninja -j 16` 通过

- [ ] Task 6: 处理 item_clicked 跳转过程的循环触发
  - [ ] SubTask 6.1: 在 `item_clicked`（protocoldock.cpp:774）调用 `_session->show_region(...)` 之前设 `_jumping_to_row = true`
  - [ ] SubTask 6.2: 在 `on_visible_range_changed` 中检查 `_jumping_to_row`：若为 true，在 `set_visible_range` 之后用 `blockSignals` + `setCurrentIndex` 恢复当前选中行
  - [ ] SubTask 6.3: 用 `QTimer::singleShot(150, this, [this]{ _jumping_to_row = false; });` 清除标志
  - [ ] SubTask 6.4: 编译验证 `cd build && ninja -j 16` 通过

- [ ] Task 7: 编译安装 + 回归验证
  - [ ] SubTask 7.1: `cd build && ninja -j 16 && ninja install`（0 error，install.dir/bin/PXView.exe 生成）
  - [ ] SubTask 7.2: Headless 启动 + MCP API tools/list 返回 17 tools（确认无回归）
  - [ ] SubTask 7.3: grep 验证 `visible_range_changed` 在 view.h/view.cpp 各 1 处定义
  - [ ] SubTask 7.4: grep 验证 `schedule_visible_range_notify` 在 view.h 1 处声明 + view.cpp 1 处实现 + view_layout.cpp 6 处调用 + view_data_sync.cpp 1 处调用
  - [ ] SubTask 7.5: grep 验证 `_visible_start_row` 在 decodermodel.h 1 处成员 + decodermodel.cpp 多处使用
  - [ ] SubTask 7.6: grep 验证 `_follow_viewport` 在 protocoldock.h 成员 + protocoldock.cpp 使用

- [ ] Task 8: GUI 手动验证（用户执行）
  - [ ] SubTask 8.1: 开关 ON 时拖动波形 → 列表跟随过滤
  - [ ] SubTask 8.2: 开关 ON 时滚轮缩放波形 → 列表跟随过滤
  - [ ] SubTask 8.3: 开关 ON 时窗口缩放 → 列表跟随过滤
  - [ ] SubTask 8.4: 开关 OFF 时 → 列表恢复全量展示
  - [ ] SubTask 8.5: 点击列表项跳转波形 → 选中行不丢失
  - [ ] SubTask 8.6: 多 decoder stack 切换 → 过滤逻辑正常
