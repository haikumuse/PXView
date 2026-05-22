# Tasks

- [ ] Task 1: Annotation 类瘦身 — 去掉 mutable 缓存字段，每条从 ~140B 降至 ~32B
  - [ ] SubTask 1.1: 从 `annotation.h` 移除 `_cached_text`, `_cached_font`, `_cached_best_annotation`, `_cached_rect_width`, `_cached_width_font` 字段
  - [ ] SubTask 1.2: 从 `annotation.cpp` 移除 `get_cached_text()` 和 `get_cached_best_annotation()` 方法实现
  - [ ] SubTask 1.3: 在 `DecodeTrace` 中新增渲染级 LRU 缓存（key=annotation_ptr+rect_width, value=QStaticText），替代原 per-annotation 缓存
  - [ ] SubTask 1.4: 修改 `draw_annotation()` / `draw_range()` / `draw_instant()` 使用新的渲染级缓存
  - [ ] SubTask 1.5: 编译验证，确保渲染功能正常

- [ ] Task 2: 定义 AnnotationRecord 固定长度结构体
  - [ ] SubTask 2.1: 创建 `annotation_record.h`，定义 32B 的 `AnnotationRecord` 结构体，含 `static_assert(sizeof == 32)`
  - [ ] SubTask 2.2: 定义 Store 文件 Header 结构体（64B）和 Row Index Entry 结构体（16B）

- [ ] Task 3: 实现 AnnotationStore 核心类
  - [ ] SubTask 3.1: 创建 `annotation_store.h` / `annotation_store.cpp`，实现文件创建、mmap 映射、Header 初始化
  - [ ] SubTask 3.2: 实现 String Pool 写入：追加 null-terminated UTF-8 字符串，返回偏移量
  - [ ] SubTask 3.3: 实现 AnnotationRecord 写入：按 Row 分区顺序追加，更新 Row Index
  - [ ] SubTask 3.4: 实现按时间范围查询：Row Index 定位 → 二分查找 → 顺序扫描 → 返回记录指针数组
  - [ ] SubTask 3.5: 实现 String Pool 读取：通过偏移量读取字符串，转换为 QString
  - [ ] SubTask 3.6: 实现 `advise_dontneed()` 调用，释放已渲染页面的物理内存
  - [ ] SubTask 3.7: 实现 mmap 动态扩展：空间不足时扩展文件并重新映射
  - [ ] SubTask 3.8: 实现 finalize 和 close 方法

- [ ] Task 4: RowData 存储后端切换
  - [ ] SubTask 4.1: RowData 新增 `AnnotationStore*` 指针成员和 Row ID
  - [ ] SubTask 4.2: 修改 `push_annotation()`：序列化为 AnnotationRecord 写入 Store
  - [ ] SubTask 4.3: 修改 `get_visible_range()`：委托给 Store 的二分查找 API
  - [ ] SubTask 4.4: 修改 `annotation_at()`：从 Store 读取 AnnotationRecord，构造临时 Annotation 代理对象
  - [ ] SubTask 4.5: 修改 `get_first_annotation_ending_after()`：委托给 Store
  - [ ] SubTask 4.6: 修改 `clear()`：清理 Store 相关状态

- [ ] Task 5: DecoderStack 回调适配
  - [ ] SubTask 5.1: `DecoderStack` 新增 `AnnotationStore*` 成员
  - [ ] SubTask 5.2: 修改 `begin_decode_work()`：创建 AnnotationStore 实例，传递给所有 RowData
  - [ ] SubTask 5.3: 修改 `annotation_callback()`：构造 AnnotationRecord 并写入 Store，文本写入 String Pool
  - [ ] SubTask 5.4: 修改解码完成逻辑：调用 Store 的 finalize 方法
  - [ ] SubTask 5.5: 修改 `DecoderStack` 析构：关闭并删除 Store

- [ ] Task 6: DecodeTrace 渲染路径适配
  - [ ] SubTask 6.1: 修改 `paint_mid()` Path 2（放大模式）：批量从 Store 读取可见范围 AnnotationRecord，本地缓存后遍历渲染
  - [ ] SubTask 6.2: 修改 `paint_mid()` Path 1（缩小模式）：改为单次二分查找 + 顺序扫描，消除重复 `get_first_annotation_ending_after()` 调用
  - [ ] SubTask 6.3: 修改 `draw_annotation()` / `draw_range()` / `draw_instant()`：从 AnnotationRecord 读取文本，使用渲染级 LRU 缓存

- [ ] Task 7: AnnotationResTable 适配
  - [ ] SubTask 7.1: 修改 `AnnotationResTable` 支持 String Pool 模式：`MakeIndex` 时将文本写入 Store 的 String Pool
  - [ ] SubTask 7.2: 修改 `annotations()` 方法：从 Store 的 String Pool 读取文本而非内存中的 `src_lines`

- [ ] Task 8: 集成测试与编译验证
  - [ ] SubTask 8.1: 增量编译，修复所有编译错误
  - [ ] SubTask 8.2: 运行 PXView，加载一个 .pxl 文件，验证解码注解正常显示
  - [ ] SubTask 8.3: 测试缩放/滚动场景，验证渲染性能和正确性
  - [ ] SubTask 8.4: 测试大文件场景（长时间采集），验证内存占用显著降低

# Task Dependencies
- [Task 2] depends on nothing (可立即开始)
- [Task 1] depends on nothing (可立即开始，与 Task 2 并行)
- [Task 3] depends on [Task 2] (需要 AnnotationRecord 定义)
- [Task 4] depends on [Task 3] (需要 AnnotationStore API)
- [Task 5] depends on [Task 3] + [Task 4] (需要 Store 和 RowData 都就绪)
- [Task 6] depends on [Task 4] (需要 RowData 新接口)
- [Task 7] depends on [Task 3] (需要 String Pool API)
- [Task 8] depends on [Task 5] + [Task 6] + [Task 7] (所有模块完成后集成测试)
