# Checklist

## Task 1: View 层新增 visible_range_changed 信号 + debounce 机制
- [ ] view.h signals 段新增 `void visible_range_changed();`
- [ ] view.h private 成员区新增 `QTimer *_viewport_change_timer;`
- [ ] view.h private 方法区新增 `void schedule_visible_range_notify();`
- [ ] view.cpp 构造函数初始化 timer（`new QTimer(this)`, setSingleShot(true), setInterval(100)）并 connect timeout → emit `visible_range_changed()`
- [ ] view.cpp 实现 `schedule_visible_range_notify()`（`_viewport_change_timer->start();`）
- [ ] view_layout.cpp `ViewLayout::set_scale_offset`（line 55）末尾调 `_view->schedule_visible_range_notify()`
- [ ] view_layout.cpp `ViewLayout::limit_scale_offset`（line 79）末尾调 `_view->schedule_visible_range_notify()`
- [ ] view_layout.cpp `ViewLayout::update_scale_offset`（line 104）末尾调 `_view->schedule_visible_range_notify()`
- [ ] view_layout.cpp `ViewLayout::set_scale`（line 128）末尾调 `_view->schedule_visible_range_notify()`
- [ ] view_layout.cpp `ViewLayout::zoom(double steps, int offset)`（line 143）末尾调 `_view->schedule_visible_range_notify()`
- [ ] view_layout.cpp `ViewLayout::h_scroll_value_changed`（line 191）末尾调 `_view->schedule_visible_range_notify()`
- [ ] view_data_sync.cpp `ViewDataSync::resizeEvent`（line 643）末尾调 `_view->schedule_visible_range_notify()`
- [ ] 连续 drag/zoom 期间 debounce timer 反复重启，仅最后一次触发 emit
- [ ] cd build && ninja -j 16 编译通过

## Task 2: DecoderStack 新增 get_visible_range 包装（Core 层）
- [ ] decoderstack.h public 接口新增 `std::pair<size_t, size_t> get_visible_range(const decode::Row &row, uint64_t start_sample, uint64_t end_sample);`
- [ ] decoderstack.cpp 实现转发到对应 `RowData::get_visible_range`
- [ ] cd build && ninja -j 16 编译通过

## Task 3: DecoderModel 新增可视范围切片接口
- [ ] decodermodel.h private 成员区新增 `int64_t _visible_start_row = -1;` `int64_t _visible_end_row = -1;`
- [ ] decodermodel.h public 接口新增 `void set_visible_range(int64_t start_row, int64_t end_row);` `void clear_visible_range();`
- [ ] decodermodel.cpp 实现 `set_visible_range`（保存参数 + beginResetModel/endResetModel）
- [ ] decodermodel.cpp 实现 `clear_visible_range`（设 `_visible_start_row = -1` + reset）
- [ ] decodermodel.cpp `rowCount` 在 `_visible_start_row >= 0` 时返回切片行数
- [ ] decodermodel.cpp `data` 在 `_visible_start_row >= 0` 时按 `_visible_start_row + index.row()` 查询
- [ ] decodermodel.cpp `headerData` 垂直表头在 `_visible_start_row >= 0` 时返回 `section + _visible_start_row`
- [ ] decodermodel 在 `_visible_start_row = -1` 时行为与改动前完全一致（向后兼容）
- [ ] cd build && ninja -j 16 编译通过

## Task 4: ProtocolDock 新增"列表跟随视口"开关按钮
- [ ] protocoldock.h private 成员区新增 `QToolButton *_follow_viewport_btn;` `bool _follow_viewport = true;` `bool _jumping_to_row = false;`
- [ ] protocoldock.h private slots 新增 `void on_follow_viewport_toggled(bool checked);` `void on_visible_range_changed();`
- [ ] protocoldock.cpp 底部面板 `bot_title_layout`（line 174-182）新增 QToolButton（checkable，默认 checked，tooltip "列表跟随波形可视范围过滤"）
- [ ] protocoldock.cpp `bind_context` 中 connect `_view->visible_range_changed` → `on_visible_range_changed`（仅当 `_follow_viewport` 为 true）
- [ ] protocoldock.cpp `bind_context` 中 connect `_follow_viewport_btn->toggled` → `on_follow_viewport_toggled`
- [ ] 实现 `on_follow_viewport_toggled`：checked=true 时 connect 信号 + 立即调一次 `on_visible_range_changed()`；checked=false 时 disconnect 信号 + 调 `_decoder_model->clear_visible_range()`
- [ ] cd build && ninja -j 16 编译通过

## Task 5: 实现可视范围过滤槽函数
- [ ] 实现 `on_visible_range_changed()`：
  - 取 `decoder_stack = _decoder_model->getDecoderStack()`；if(!decoder_stack) return;
  - 取 `samplerate = decoder_stack->samplerate()`；if(samplerate == 0) return;
  - 计算 `samples_per_pixel = samplerate * _view->scale()`
  - 计算 `start_sample = (uint64_t)max(_view->offset() * samples_per_pixel, 0.0)`
  - 取 `viewport_width = _view->viewport()->width()`；if(viewport_width <= 0) return;
  - 计算 `end_sample = (uint64_t)max((_view->offset() + viewport_width) * samples_per_pixel, 0.0)`
  - 遍历 `decoder_stack->get_rows_lshow()` 取当前 `filterKeyColumn` 对应的协议行（复用 nav_table_view 算法）
  - 调 `decoder_stack->get_visible_range(row, start_sample, end_sample)` 取 `[start_idx, end_idx)`
  - mark_index 豁免：用 `decoder_stack->get_annotation_index(row, mark)` 映射为行号，若不在范围则扩展
  - 调 `_decoder_model->set_visible_range(start_idx, end_idx)`
- [ ] 无 DecoderStack 绑定时槽函数直接 return
- [ ] samplerate 为 0 时槽函数直接 return
- [ ] viewport_width <= 0 时槽函数直接 return
- [ ] cd build && ninja -j 16 编译通过

## Task 6: 处理 item_clicked 跳转过程的循环触发
- [ ] `item_clicked`（protocoldock.cpp:774）在调用 `_session->show_region(...)` 之前设 `_jumping_to_row = true`
- [ ] `on_visible_range_changed` 检查 `_jumping_to_row`：若为 true，在 `set_visible_range` 之后用 `blockSignals` + `setCurrentIndex` 恢复当前选中行
- [ ] 用 `QTimer::singleShot(150ms)` 清除 `_jumping_to_row` 标志
- [ ] cd build && ninja -j 16 编译通过

## Task 7: 编译安装 + 回归验证
- [ ] cd build && ninja -j 16 编译通过（0 error）
- [ ] ninja install 安装成功（install.dir/bin/PXView.exe 生成）
- [ ] Headless 启动 + MCP API tools/list 返回 17 tools（确认无回归）
- [ ] grep 验证：`visible_range_changed` 在 view.h/view.cpp 各 1 处定义
- [ ] grep 验证：`schedule_visible_range_notify` 在 view.h 1 处声明 + view.cpp 1 处实现 + view_layout.cpp 6 处调用 + view_data_sync.cpp 1 处调用
- [ ] grep 验证：`_visible_start_row` 在 decodermodel.h 1 处成员 + decodermodel.cpp 多处使用
- [ ] grep 验证：`_follow_viewport` 在 protocoldock.h 成员 + protocoldock.cpp 使用

## Task 8: GUI 手动验证（用户执行）
- [ ] 开关 ON 时拖动波形 → 列表跟随过滤
- [ ] 开关 ON 时滚轮缩放波形 → 列表跟随过滤
- [ ] 开关 ON 时窗口缩放 → 列表跟随过滤
- [ ] 开关 OFF 时 → 列表恢复全量展示
- [ ] 点击列表项跳转波形 → 选中行不丢失
- [ ] 多 decoder stack 切换 → 过滤逻辑正常
