# 解码通道缩放性能优化 Spec

## Why
当缩放到最小时，可见范围内可能有数十万甚至百万条注解。当前实现虽然通过 `last_x` 跳过了重复像素的绘制，但仍需：(1) 将所有可见注解指针拷贝到临时 `std::vector`（大量内存分配+拷贝）；(2) 线性遍历每一条注解做坐标计算和判断。atk-logic-master 通过阈值合并+跳跃式遍历+无临时容器，在此场景下不卡顿。

## What Changes
- 在 `DecodeTrace::paint_mid()` 中实现阈值合并机制：当 `min_annWidth < 4px` 时，将相邻过小注解合并为一个色块绘制，而非逐条画竖线
- 实现跳跃式遍历：合并块结束后，利用二分查找直接跳到合并块结束位置之后的下一个注解，跳过中间所有注解
- 在 `RowData` 中新增迭代式 API，避免 `get_annotation_subset()` 构建临时 `std::vector<Annotation*>`
- 新增 `draw_merge_block()` 方法绘制合并色块（实心矩形+斜线纹理，视觉上区分于正常注解）

## Impact
- Affected code: `rowdata.h`, `rowdata.cpp`, `decodetrace.h`, `decodetrace.cpp`
- Affected specs: `optimize-decode-render-perf`（前置依赖，已完成）
- 无 BREAKING 变更，`get_annotation_subset()` 保留但不再被 `paint_mid` 调用

## ADDED Requirements

### Requirement: 阈值合并机制
系统 SHALL 在 `DecodeTrace::paint_mid()` 中，当 `min_annWidth < 4px` 时，将相邻的过小注解合并为一个色块绘制，而非逐条绘制竖线。

#### Scenario: 过小注解合并为色块
- **WHEN** `min_annWidth < 4px`（即最小注解在屏幕上不足 4 像素宽）
- **AND** 遍历到一条注解，其像素宽度小于 4px
- **THEN** 将该注解累加到当前合并块（记录合并块的起始像素和累计像素宽度）
- **AND** 不对该注解调用 `draw_annotation()`/`draw_range()`/`draw_instant()`

#### Scenario: 合并块绘制
- **WHEN** 合并块结束（遇到足够大的注解、超出屏幕右边界、或注解遍历结束）
- **THEN** 调用 `draw_merge_block()` 绘制合并色块
- **AND** 合并色块为实心矩形 + 斜线纹理，视觉上与正常注解的六边形有明显区分

#### Scenario: 正常注解照常绘制
- **WHEN** `min_annWidth >= 4px`
- **THEN** 按现有逻辑逐条绘制注解（六边形+文本），不触发合并机制

#### Scenario: 混合场景
- **WHEN** `min_annWidth < 4px` 但某条注解的像素宽度 >= 4px
- **THEN** 先绘制当前合并块（如有），然后正常绘制该注解，之后继续合并逻辑

### Requirement: 跳跃式遍历
系统 SHALL 在合并块结束后，利用二分查找直接跳到合并块结束位置之后的下一个注解，跳过中间所有注解。

#### Scenario: 合并块结束后的跳跃
- **WHEN** 一个合并块绘制完成
- **THEN** 计算合并块结束位置对应的样本索引
- **AND** 使用二分查找定位到第一个 `start_sample > 合并块结束样本` 的注解
- **AND** 直接跳到该注解继续遍历，跳过中间所有注解

#### Scenario: 无合并时的正常遍历
- **WHEN** `min_annWidth >= 4px`，不需要合并
- **THEN** 按现有逻辑线性遍历可见范围内的注解

### Requirement: 避免临时 vector 构建
系统 SHALL 在 `DecodeTrace::paint_mid()` 中直接在 `RowData` 上迭代，不再构建临时 `std::vector<Annotation*>`。

#### Scenario: 直接迭代 RowData
- **WHEN** `paint_mid()` 需要遍历某行的注解
- **THEN** 通过 `RowData` 的新迭代 API 获取可见范围的起始索引
- **AND** 使用索引直接访问 `RowData` 内部的注解数组
- **AND** 不构建任何临时 `std::vector<Annotation*>`

#### Scenario: RowData 新增迭代 API
- **WHEN** 需要遍历注解
- **THEN** `RowData` 提供 `get_visible_range(start_sample, end_sample)` 返回可见范围的 `[start_idx, end_idx)` 索引对
- **AND** `RowData` 提供 `find_index_after_sample(sample)` 返回第一个 `start_sample > sample` 的注解索引（用于跳跃遍历）
- **AND** `RowData` 提供 `annotation_at(index)` 返回指定索引的注解指针（共享锁保护）

#### Scenario: 向后兼容
- **WHEN** 其他代码仍调用 `get_annotation_subset()`
- **THEN** 该方法继续正常工作，不受影响

### Requirement: 合并色块绘制方法
系统 SHALL 在 `DecodeTrace` 中新增 `draw_merge_block()` 方法，绘制合并色块。

#### Scenario: 合并色块样式
- **WHEN** 调用 `draw_merge_block(painter, start_x, end_x, y, height, color)`
- **THEN** 绘制一个实心矩形，填充颜色使用该行注解的主色调
- **AND** 在矩形上叠加斜线纹理（每隔 1 像素画一条 45 度斜线），使合并块与正常注解视觉区分

## MODIFIED Requirements

### Requirement: DecodeTrace::paint_mid() 遍历逻辑
原实现使用 `get_annotation_subset()` 构建临时 vector 后线性遍历。新实现 SHALL 使用 RowData 迭代 API + 阈值合并 + 跳跃遍历。

### Requirement: RowData 查询接口
原实现仅提供 `get_annotation_subset()` 一种查询方式。新实现 SHALL 额外提供 `get_visible_range()`、`find_index_after_sample()`、`annotation_at()` 三个方法。
