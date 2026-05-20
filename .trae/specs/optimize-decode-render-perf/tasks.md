# Tasks

- [x] Task 1: RowData 每实例读写锁 + 二分查找
  - [x] SubTask 1.1: 在 `rowdata.h` 中将 `static std::mutex _global_visitor_mutex` 改为 `std::shared_mutex _visitor_mutex`（每实例），添加 `#include <shared_mutex>`
  - [x] SubTask 1.2: 在 `rowdata.cpp` 中删除 `std::mutex RowData::_global_visitor_mutex;` 静态定义
  - [x] SubTask 1.3: 将 `push_annotation()` 中的 `std::lock_guard<std::mutex>` 改为 `std::unique_lock<std::shared_mutex>`
  - [x] SubTask 1.4: 将 `get_annotation_subset()` 中的 `std::lock_guard<std::mutex>` 改为 `std::shared_lock<std::shared_mutex>`
  - [x] SubTask 1.5: 将 `get_max_sample()` 中的 `std::lock_guard<std::mutex>` 改为 `std::shared_lock<std::shared_mutex>`
  - [x] SubTask 1.6: 将 `get_annotation()` 中的 `std::lock_guard<std::mutex>` 改为 `std::shared_lock<std::shared_mutex>`
  - [x] SubTask 1.7: 将 `get_annotation_index()` 中的 `std::lock_guard<std::mutex>` 改为 `std::shared_lock<std::shared_mutex>`
  - [x] SubTask 1.8: 将 `clear()` 中的 `std::lock_guard<std::mutex>` 改为 `std::unique_lock<std::shared_mutex>`
  - [x] SubTask 1.9: 在 `get_annotation_subset()` 中实现二分查找：用 `std::lower_bound` 找到第一个 `end_sample() > start_sample` 的位置，从该位置向后遍历直到 `start_sample() > end_sample`
  - [x] SubTask 1.10: 编译验证

- [x] Task 2: Annotation 文本渲染缓存
  - [x] SubTask 2.1: 在 `annotation.h` 中添加 `QStaticText *_cached_static_text` 成员和 `_cached_font_key` 成员，添加 `get_cached_text()` 和 `invalidate_text_cache()` 方法声明
  - [x] SubTask 2.2: 在 `annotation.cpp` 中实现 `get_cached_text()`：如果缓存不存在或字体变化，创建新的 `QStaticText` 并调用 `prepare(QTransform(), font)`；返回缓存指针
  - [x] SubTask 2.3: 在 `annotation.cpp` 中实现 `invalidate_text_cache()`：删除缓存的 `QStaticText`，置空指针
  - [x] SubTask 2.4: 在析构函数中释放 `_cached_static_text`
  - [x] SubTask 2.5: 在 `decodetrace.cpp` 的 `draw_range()` 中，将 `p.drawText(rect, ...)` 替换为 `p.drawStaticText(pos, *a.get_cached_text(text, p.font()))`
  - [x] SubTask 2.6: 在 `decodetrace.cpp` 的 `draw_instant()` 中，将 `p.drawText(rect, ...)` 替换为 `p.drawStaticText(pos, *a.get_cached_text(text, p.font()))`
  - [x] SubTask 2.7: 编译验证

- [x] Task 3: View::data_updated() 去重
  - [x] SubTask 3.1: 在 `view.h` 中添加 `QElapsedTimer _data_updated_timer` 成员
  - [x] SubTask 3.2: 在 `view.cpp` 的 `data_updated()` 开头添加去重逻辑：如果 `_data_updated_timer.isValid() && _data_updated_timer.elapsed() < 16`，则只调用 `set_update(_time_viewport, true)` 和 `set_update(_fft_viewport, true)` 后 return
  - [x] SubTask 3.3: 在 `data_updated()` 末尾调用 `_data_updated_timer.start()`
  - [x] SubTask 3.4: 编译验证

- [x] Task 4: 增量编译 + 运行验证
  - [x] SubTask 4.1: 执行 `build_incremental.cmd` 确认编译通过
  - [ ] SubTask 4.2: 运行 PXView.exe，采集高频信号 + 添加协议解码器，验证解码过程中无明显卡顿

# Task Dependencies
- [Task 1] 和 [Task 2] 无依赖，可并行执行
- [Task 3] 无依赖，可与 Task 1/2 并行执行
- [Task 4] 依赖 [Task 1] + [Task 2] + [Task 3] 全部完成
