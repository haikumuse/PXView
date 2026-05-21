# Tasks

- [x] Task 1: RowData 新增迭代 API
  - [x] SubTask 1.1: 在 `rowdata.h` 中声明 `get_visible_range(start_sample, end_sample)` 返回 `std::pair<size_t, size_t>`，使用二分查找返回可见范围 `[start_idx, end_idx)`
  - [x] SubTask 1.2: 在 `rowdata.h` 中声明 `find_index_after_sample(sample)` 返回 `size_t`，使用二分查找返回第一个 `start_sample > sample` 的注解索引
  - [x] SubTask 1.3: 在 `rowdata.h` 中声明 `annotation_at(index)` 返回 `const Annotation*`，带共享锁保护
  - [x] SubTask 1.4: 在 `rowdata.cpp` 中实现 `get_visible_range()`：使用 `lower_bound` 找起始位置，线性扫描找结束位置，返回索引对
  - [x] SubTask 1.5: 在 `rowdata.cpp` 中实现 `find_index_after_sample()`：使用 `upper_bound` 查找
  - [x] SubTask 1.6: 在 `rowdata.cpp` 中实现 `annotation_at()`：共享锁 + 边界检查 + 返回指针
  - [x] SubTask 1.7: 编译验证

- [x] Task 2: DecodeTrace 新增 draw_merge_block() 方法
  - [x] SubTask 2.1: 在 `decodetrace.h` 中声明 `draw_merge_block(QPainter&, double start_x, double end_x, int y, int h, QColor fill)`
  - [x] SubTask 2.2: 在 `decodetrace.cpp` 中实现 `draw_merge_block()`：绘制实心矩形 + 斜线纹理
  - [x] SubTask 2.3: 编译验证

- [x] Task 3: 重写 paint_mid() 遍历逻辑
  - [x] SubTask 3.1: 将 `paint_mid()` 中 `get_annotation_subset()` + 线性遍历替换为新的迭代逻辑：
    - 使用 `get_visible_range()` 获取索引范围
    - 使用 `annotation_at()` 按索引访问注解
    - 当 `min_annWidth < 4px` 时：过小注解累加到合并块，合并块结束后调用 `draw_merge_block()` 绘制
    - 合并块结束后调用 `find_index_after_sample()` 跳跃到下一个可见注解
    - 当 `min_annWidth >= 4px` 时：保持原有逐条绘制逻辑（使用 `draw_annotation()`）
  - [x] SubTask 3.2: 处理混合场景：合并过程中遇到足够大的注解时，先绘制合并块再正常绘制该注解
  - [x] SubTask 3.3: 处理边界情况：合并块超出屏幕右边界时立即绘制并终止遍历
  - [x] SubTask 3.4: 编译验证

- [x] Task 4: 增量编译 + 运行验证
  - [x] SubTask 4.1: 执行 `build_incremental.cmd` 确认编译通过
  - [ ] SubTask 4.2: 运行 PXView.exe，采集高频信号 + 添加协议解码器，缩放到最小验证无明显卡顿
  - [ ] SubTask 4.3: 验证正常缩放级别下解码通道显示无异常（六边形+文本正常渲染）
  - [ ] SubTask 4.4: 验证合并色块与正常注解视觉上有区分

# Task Dependencies
- [Task 2] 无依赖，可与 Task 1 并行
- [Task 3] 依赖 [Task 1] + [Task 2]
- [Task 4] 依赖 [Task 1] + [Task 2] + [Task 3]
