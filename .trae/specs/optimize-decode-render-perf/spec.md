# 解码渲染性能优化 Spec

## Why
Profile 采样报告证实：采集后解码期间 37.9% CPU 耗在 HarfBuzz 文本整形 + DWrite 字体渲染上。之前的优化（内存池、异步拷贝、50FPS 节流、draw_instant 小于4px跳过、draw_range 引用修复）已解决了数据层瓶颈和部分渲染层瓶颈，但仍有三个核心问题未解决：(1) `get_annotation_subset` 在全局互斥锁下线性遍历所有 annotation；(2) 每个 annotation 文本每次 paint 都重新走 HarfBuzz+DWrite 管线，无缓存；(3) `View::data_updated()` 无去重，多次调用触发多次完整重绘。

## What Changes
- 将 `RowData::_global_visitor_mutex` 从静态全局 `std::mutex` 改为每实例 `std::shared_mutex`，读操作用共享锁
- `get_annotation_subset` 使用二分查找定位可见区间，替代线性遍历
- 在 `Annotation` 类中添加 `QStaticText` 缓存，`draw_range`/`draw_instant` 中用 `drawStaticText` 替代 `drawText`
- `View::data_updated()` 添加去重机制，短时间内多次调用只触发一次 `viewport_update()`

## Impact
- Affected code: `rowdata.h`, `rowdata.cpp`, `annotation.h`, `annotation.cpp`, `decodetrace.cpp`, `view.cpp`
- Affected specs: `optimize-post-capture-perf`（互补，不冲突）
- 无 BREAKING 变更

## 审视已有优化：保留 vs 冗余

### 保留的已有优化（已验证有效，不回滚）
| 优化 | 位置 | 保留原因 |
|------|------|----------|
| 消除重复 `copy_data_to_document` | mainwindow.cpp | 消除了真实的双倍深拷贝浪费 |
| LeafBlockPool 内存池 | leaf_block_pool.h + 多文件 | 消除了 malloc/free 系统调用阻塞 |
| `copy_data_to_document` 异步化 | sigsession.cpp | UI 线程不再阻塞等待拷贝 |
| `DSV_MSG_COPY_TO_DOC_DONE` 流水线串行 | sigsession.cpp | 避免拷贝与解码并发争锁 |
| 50FPS 节流 `on_new_decode_data` | decodetrace.cpp | 限制了解码进度通知频率 |
| `draw_instant` 宽度<4px 跳过文本 | decodetrace.cpp | 避免了不可见文本的 DWrite 开销 |
| `draw_range` `const &` 引用修复 | decodetrace.cpp | 消除了每帧数万次字符串深拷贝 |
| `draw_range` 小矩形快速路径 | decodetrace.cpp | 极小标注只画竖线，跳过多边形和文本 |
| `draw_range` rect.width()<=4 跳过文本 | decodetrace.cpp | 与 draw_instant 一致的 LOD 策略 |
| `resize_table_view` 500行阈值 | protocoldock.cpp | 避免了解码完成瞬间的表格列宽计算卡顿 |
| 密集标注重叠跳过 `end <= last_x && end-start < 0.5` | decodetrace.cpp | 基本的重叠剔除，不引入视觉缝隙 |

### 不需要新增的优化（已有替代或收益不足）
| 原建议 | 不新增原因 |
|--------|-----------|
| `decode_data` 通知频率从 1/100 降至 1/1000 | **已实现**：当前代码已是 `notify_cnt = total / 1000`，且接收端有 50FPS 节流 |
| 分离解码区域重绘与波形重绘 | 收益不确定，实现复杂度高（需要重构 Viewport 的 paint 分层架构），且 pixmap 缓存已部分解决此问题 |
| `check_update()` 移出 paint 路径 | 仅影响 DSO/Analog 模式，当前问题在 LOGIC 模式解码，优先级低 |

## ADDED Requirements

### Requirement: RowData 每实例读写锁
系统 SHALL 将 `RowData::_global_visitor_mutex` 从静态全局 `std::mutex` 改为每实例 `std::shared_mutex`，读操作使用共享锁，写操作使用独占锁。

#### Scenario: push_annotation 使用独占锁
- **WHEN** 解码线程调用 `RowData::push_annotation()`
- **THEN** 使用 `std::unique_lock<std::shared_mutex>` 获取独占锁
- **AND** 仅阻塞同一 RowData 实例的其他写操作和读操作
- **AND** 不阻塞其他 RowData 实例的任何操作

#### Scenario: get_annotation_subset 使用共享锁
- **WHEN** UI 线程调用 `RowData::get_annotation_subset()`
- **THEN** 使用 `std::shared_lock<std::shared_mutex>` 获取共享锁
- **AND** 允许同一 RowData 实例的并发读操作
- **AND** 仅阻塞同一 RowData 实例的写操作

#### Scenario: 不同 RowData 实例完全并行
- **WHEN** 解码线程在 RowData A 上执行 `push_annotation()`
- **AND** UI 线程同时在 RowData B 上执行 `get_annotation_subset()`
- **THEN** 两个操作完全并行，无锁竞争

### Requirement: get_annotation_subset 二分查找
系统 SHALL 在 `get_annotation_subset` 中使用二分查找定位可见区间，替代线性遍历。

#### Scenario: 利用 annotation 有序性快速定位
- **WHEN** 调用 `get_annotation_subset(dest, start_sample, end_sample)`
- **THEN** 使用 `std::lower_bound` 在 `_annotations` 中找到第一个 `start_sample > start_sample参数` 的 annotation
- **AND** 从该位置向后遍历直到 annotation 的 `start_sample > end_sample参数`
- **AND** 仅对可见区间内的 annotation 执行 `dest.push_back()`

#### Scenario: 空数据集快速返回
- **WHEN** `_annotations` 为空
- **THEN** 立即返回，不执行任何查找

### Requirement: Annotation 文本渲染缓存
系统 SHALL 在 `Annotation` 类中缓存 `QStaticText`，避免每次 paint 重复走 HarfBuzz+DWrite 管线。

#### Scenario: 首次绘制时创建缓存
- **WHEN** `draw_range` 或 `draw_instant` 需要绘制 annotation 文本
- **AND** 该 annotation 的 `QStaticText` 缓存尚未创建
- **THEN** 创建 `QStaticText` 对象并调用 `prepare(font)` 预完成文本整形
- **AND** 将缓存存储在 annotation 对象中

#### Scenario: 后续绘制使用缓存
- **WHEN** 同一 annotation 再次被绘制（如滚动/缩放后重绘）
- **AND** `QStaticText` 缓存已存在且字体未变
- **THEN** 使用 `QPainter::drawStaticText()` 绘制缓存的预整形文本
- **AND** 不再调用 HarfBuzz 文本整形和 DWrite 字体渲染

#### Scenario: 字体变化时重建缓存
- **WHEN** 应用字体设置发生变化
- **THEN** 清除所有 annotation 的文本缓存
- **AND** 下次绘制时使用新字体重建缓存

### Requirement: View::data_updated() 去重
系统 SHALL 对 `View::data_updated()` 添加短时间去重机制，避免多次快速调用触发多次完整重绘。

#### Scenario: 短时间内多次调用只触发一次重绘
- **WHEN** `data_updated()` 在 16ms 内被多次调用（如多个解码器同时通知更新）
- **THEN** 只执行一次 `viewport_update()` 和 `_ruler->update()`
- **AND** `update_scroll()` 和 `update_scale_offset()` 也只执行一次

#### Scenario: 超过去重窗口后正常触发
- **WHEN** 距离上次 `data_updated()` 实际执行已超过 16ms
- **THEN** 正常执行所有更新操作

## MODIFIED Requirements

### Requirement: RowData 锁机制
原实现使用静态全局 `std::mutex _global_visitor_mutex`，所有 RowData 实例共享。新实现 SHALL 使用每实例 `std::shared_mutex`，支持读写锁语义。

### Requirement: Annotation 文本绘制
原实现使用 `QPainter::drawText()` 每次重新走文本渲染管线。新实现 SHALL 使用 `QPainter::drawStaticText()` 配合缓存的 `QStaticText` 对象。
