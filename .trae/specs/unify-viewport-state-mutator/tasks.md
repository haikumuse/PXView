# Tasks

- [x] Task 1: 新增 3 个 private 统一 mutator 声明与实现
  - [ ] SubTask 1.1: 在 `PXView/pv/view/view.h` private 方法区新增 3 个声明：`void apply_scale_offset(double scale, int64_t offset);` `void apply_scale(double scale);` `void apply_offset(int64_t offset);`
  - [ ] SubTask 1.2: 在 `PXView/pv/view/view.cpp` 实现这 3 个 mutator：
    - `apply_scale_offset`：clamp scale 到 [_minscale, _maxscale] → clamp offset 到 [get_min_offset(), get_max_offset()] → 比较 _scale/_offset 是否真的改变 → 若改变则写入并 `_viewport_change_timer->start()`
    - `apply_scale`：委托 `apply_scale_offset(scale, _offset)`
    - `apply_offset`：委托 `apply_scale_offset(_scale, offset)`
  - [ ] SubTask 1.3: 确认 mutator 不调用 viewport_update/update_scroll/_header->update()/_ruler->update()（这些副作用由外层 public 函数负责，mutator 只管状态 + timer）

- [x] Task 2: 改造 set_scale_offset 和 set_scale（最常用入口）
  - [ ] SubTask 2.1: 改造 `View::set_scale_offset(double scale, int64_t offset)`（约第 914-932 行）：
    - 保留 _preScale/_preOffset 保存
    - 将 `_scale = max(scale, _minscale)` 和 `_offset = floor(max(offset, get_min_offset()))` 替换为 `apply_scale_offset(scale, offset)`
    - 保留 viewport_update/update_scroll/_header->update()/_ruler->update()
    - 删除末尾的 `_viewport_change_timer->start()`（mutator 内部已启动）
  - [ ] SubTask 2.2: 改造 `View::set_scale(double scale)`（约第 2543-2557 行）：
    - 保留 _preScale 保存
    - 将 `_scale = scale` 替换为 `apply_scale(scale)`
    - 保留 viewport_update/update_scroll/_header->update()/_ruler->update()
    - 删除末尾的 `_viewport_change_timer->start()`

- [x] Task 3: 改造 zoom 函数（4 处赋值点）
  - [ ] SubTask 3.1: 改造 `View::zoom(double anchor, double factor)`（约第 627-688 行）：
    - 第 657 行 `_scale = max(min(_scale, _maxscale), _minscale)` → `apply_scale(_scale)`（先 clamp 再调，或直接 apply_scale 让 mutator clamp）
    - 第 671 行 `_scale = max(min(scale, _maxscale), _minscale)` → `apply_scale(scale)`
    - 第 677-678 行 `_offset = floor(...)` + `_offset = max(min(...))` → `apply_offset(computed_offset)`（注意：offset 计算依赖 _scale 的新值，需先用 apply_scale 更新 _scale，再算 offset，再 apply_offset）
    - 删除第 685 行的 `_viewport_change_timer->start()`（mutator 内部已启动）
  - [ ] SubTask 3.2: 验证 zoom 的 offset 计算顺序：原代码 `_offset = floor((_offset + offset) * (_preScale / _scale) - offset)` 中 _scale 是新值，_preScale 是旧值。改造后需保证 apply_scale 先执行（写入新 _scale），再计算 offset，再 apply_offset

- [x] Task 4: 改造 limit_scale_offset 和 update_scale_offset
  - [ ] SubTask 4.1: 改造 `View::limit_scale_offset()`（约第 929-951 行）：
    - 第 958-959 行 `_scale = max(min(...))` / `_offset = max(min(...))` → 直接 `apply_scale_offset(_scale, _offset)`（mutator 内部 clamp，比手动 clamp 更简洁）
    - 删除末尾的 `_viewport_change_timer->start()`
  - [ ] SubTask 4.2: 改造 `View::update_scale_offset()`（约第 1226-1265 行）：
    - 第 1263/1265/1268 行 `_scale = max(_scale, _minscale)` / `_scale = _session->cur_view_time() / width` / `_scale = max(_scale, _minscale)` → 用 `apply_scale(...)` 替换
    - 第 1271 行 `_offset = max(_offset, get_min_offset())` → `apply_offset(_offset)`
    - 删除末尾的 `_viewport_change_timer->start()`
  - [ ] SubTask 4.3: 注意 update_scale_offset 中多次 _scale 赋值的语义：每次 apply_scale 都会启动 timer，但 QTimer::start 幂等，最后一次生效，行为正确

- [x] Task 5: 改造 mode_changed 和 resizeEvent 和 h_scroll_value_changed
  - [ ] SubTask 5.1: 改造 `View::mode_changed()`（约第 1281-1288 行）：
    - 第 1285 行 `_scale = WellSamplesPerPixel * 1.0 / samplerate` → `apply_scale(WellSamplesPerPixel * 1.0 / samplerate)`
    - 第 1287 行 `_scale = max(min(_scale, _maxscale), _minscale)` → `apply_scale(_scale)`（或合并为一行 apply_scale 让 mutator clamp）
    - 修复原漏 timer 的 bug（mutator 内部自动启动）
  - [ ] SubTask 5.2: 改造 `View::resizeEvent(QResizeEvent *)`（约第 1591-1640 行）：
    - 第 1620 行 `_scale = _session->cur_view_time() / width` → `apply_scale(_session->cur_view_time() / width)`
    - 第 1627 行 `_scale = _maxscale` → `apply_scale(_maxscale)`
    - 删除末尾的 `_viewport_change_timer->start()`
  - [ ] SubTask 5.3: 改造 `View::h_scroll_value_changed(int value)`（约第 1642-1660 行）：
    - 第 1650 行 `_offset = value` → `apply_offset(value)`
    - 第 1655 行 `_offset = floor(value * 1.0 / MaxScrollValue * length)` → `apply_offset(floor(value * 1.0 / MaxScrollValue * length))`
    - 第 1658 行 `_offset = max(min(_offset, get_max_offset()), get_min_offset())` → 删除（mutator 内部已 clamp）或保留为 `apply_offset(_offset)`
    - 删除末尾的 `_viewport_change_timer->start()`

- [x] Task 6: 验证无残留直接赋值 + 编译检查
  - [ ] SubTask 6.1: 执行 `grep -nE "_scale\s*=|_offset\s*=" PXView/pv/view/view.cpp` 确认仅返回注释和局部变量（如 `next_v_offset`、`new_scale`、`ideal_scale`），无成员赋值
  - [ ] SubTask 6.2: 执行 `cd build && ninja -j 16` 编译验证通过
  - [ ] SubTask 6.3: 执行 `ninja install` 安装成功

# Task Dependencies
- Task 1 是基础，Task 2-5 都依赖 Task 1（需要 mutator 可用）
- Task 2-5 之间相互独立，可并行（都修改 view.cpp 不同函数，但同一文件需串行编辑以避免冲突）
- Task 6 依赖所有前置 Task
- **重要：Task 1-5 全部完成后才统一编译（Task 6.2），不每完成一个 Task 就编译**（用户明确要求）
