# 解码注解磁盘化存储（方案 A：32B 固定记录 + mmap）Spec

## Why
长时间采集（如 4.6G PWM 波形）会产生数千万条解码注解（Annotation），当前全部驻留内存（2000 万条 ≈ 2.8GB）。当物理内存不足时，Windows 将 Annotation 对象 swap 到 pagefile.sys，UI 拖动时触发大量 page fault 导致界面假死。通过将 Annotation 数据从纯内存迁移到 mmap 文件，利用 OS Page Cache 自动管理冷热数据，可将内存占用从 GB 级降至仅当前视口的 MB 级，彻底消除 OOM 风险。

## What Changes
- 新增 `AnnotationRecord` 结构体：32B 固定长度二进制记录，替代当前 ~140B 的 C++ Annotation 对象
- 新增 `AnnotationStore` 类：管理 mmap 文件、Row Index、String Pool，提供按时间范围查询的 API
- 修改 `Annotation` 类：去掉 `mutable` 缓存字段（`_cached_text`, `_cached_font`, `_cached_best_annotation`, `_cached_rect_width`, `_cached_width_font`），改为轻量代理对象，从 Store 按需读取数据
- 修改 `RowData` 类：`push_annotation` 改为写入 AnnotationStore；查询方法改为从 Store 读取
- 修改 `DecoderStack::annotation_callback`：回调中将注解序列化为 AnnotationRecord 写入 Store
- 修改 `DecodeTrace` 渲染路径：从 Store 批量读取可见范围注解，去掉逐条 shared_mutex 加锁
- 修改 `AnnotationResTable`：适配 String Pool 写入，去重逻辑保留
- 修改 `AnnotationPool`：适配新的轻量 Annotation 尺寸
- **BREAKING** `Annotation` 类不再持有文本缓存和渲染缓存，所有缓存移至渲染层

## Impact
- Affected specs: `optimize-decode-render-perf`（Annotation 缓存机制变更），`optimize-decode-zoom-perf`（RowData 查询接口变更）
- Affected code:
  - `PXView/pv/data/decode/annotation.h` / `annotation.cpp` — 核心数据结构瘦身
  - `PXView/pv/data/decode/rowdata.h` / `rowdata.cpp` — 存储后端切换
  - `PXView/pv/data/decode/annotationrestable.h` / `annotationrestable.cpp` — String Pool 适配
  - `PXView/pv/data/decode/annotation_pool.h` — 池尺寸适配
  - `PXView/pv/data/decoderstack.h` / `decoderstack.cpp` — 回调写入 Store
  - `PXView/pv/view/decodetrace.h` / `decodetrace.cpp` — 渲染路径适配
  - 新增 `PXView/pv/data/decode/annotation_record.h` — AnnotationRecord 定义
  - 新增 `PXView/pv/data/decode/annotation_store.h` / `annotation_store.cpp` — Store 实现

## ADDED Requirements

### Requirement: AnnotationRecord 固定长度二进制记录
系统 SHALL 定义 32 字节固定长度的 `AnnotationRecord` 结构体，作为注解在磁盘上的存储格式。

#### Scenario: 结构体布局
- **WHEN** 系统定义 `AnnotationRecord`
- **THEN** 结构体包含以下字段：`start_sample`(uint64), `end_sample`(uint64), `str_pool_offset`(uint32), `str_count`(uint16), `format`(uint16), `type`(uint16), `is_numeric`(uint8), `hex_pool_offset`(uint32), `reserved`(uint8)
- **AND** `sizeof(AnnotationRecord) == 32`
- **AND** 使用 `#pragma pack(push, 1)` 保证无填充对齐

#### Scenario: 从 srd_proto_data 序列化
- **WHEN** `annotation_callback` 收到一条 `srd_proto_data`
- **THEN** 提取 `start_sample`, `end_sample`, `ann_class`, `ann_type` 填入 AnnotationRecord
- **AND** 将注解文本写入 String Pool，记录 `str_pool_offset` 和 `str_count`
- **AND** 如果含数值数据，将 hex 字符串写入 String Pool，记录 `hex_pool_offset` 和 `is_numeric=1`

### Requirement: AnnotationStore mmap 文件管理
系统 SHALL 提供 `AnnotationStore` 类，管理注解数据的 mmap 文件存储。

#### Scenario: 文件格式
- **WHEN** 创建 AnnotationStore
- **THEN** 文件格式为：Header(64B) + Row Index Table + Annotation Records(per-row, sorted by sample) + String Pool
- **AND** Header 包含 Magic("DANN"), Version, Row count, Annotation count, String Pool offset/size
- **AND** Row Index Table 每行 16B：`{ann_offset, ann_count, reserved}`
- **AND** Annotation Records 按 Row 分区，每区内按 `start_sample` 升序排列

#### Scenario: mmap 初始化
- **WHEN** 解码开始前，`DecoderStack` 创建 AnnotationStore
- **THEN** Store 在指定目录创建 `.dann` 文件
- **AND** 使用 `MmapAllocator` 或等价机制将文件 mmap 到地址空间
- **AND** 初始文件大小预留足够空间（基于采样率 × 通道数估算）

#### Scenario: 增量写入
- **WHEN** `annotation_callback` 被调用产生新注解
- **THEN** 将 AnnotationRecord 顺序追加到对应 Row 的记录区
- **AND** 将文本追加到 String Pool
- **AND** 更新 Row Index 中的 `ann_count`
- **AND** 如果 mmap 空间不足，扩展文件并重新映射

#### Scenario: 按时间范围查询
- **WHEN** 渲染线程请求 `[start_sample, end_sample]` 范围内的注解
- **THEN** Store 通过 Row Index 定位目标 Row 的记录区
- **AND** 使用二分查找定位第一条 `end_sample > start_sample` 的记录
- **AND** 顺序扫描直到 `start_sample > end_sample`
- **AND** 返回 AnnotationRecord 数组（指针直接指向 mmap 区域，零拷贝）

#### Scenario: String Pool 读取
- **WHEN** 渲染线程需要某条 AnnotationRecord 的文本
- **THEN** 通过 `str_pool_offset` 从 String Pool 读取 null-terminated UTF-8 字符串
- **AND** 连续读取 `str_count` 个字符串
- **AND** 转换为 `QString` 返回

#### Scenario: OS Page Cache 管理
- **WHEN** 渲染完成，当前视口的注解数据不再需要
- **THEN** 调用 `advise_dontneed()` 通知 OS 可回收这些页面
- **AND** OS 自动将冷数据页面换出，热数据保留在 RAM

#### Scenario: 解码完成
- **WHEN** 解码线程完成所有数据处理
- **THEN** Store 刷新 Row Index 和 Header
- **AND** 文件保持打开状态供后续查询使用

#### Scenario: 会话关闭
- **WHEN** TabContext 销毁或新采集开始
- **THEN** 关闭并删除 `.dann` 文件
- **AND** 释放 mmap 映射

### Requirement: Annotation 类瘦身
系统 SHALL 将 `Annotation` 类从 ~140B 缩减至 ~32B，去掉所有 `mutable` 渲染缓存字段。

#### Scenario: 移除的字段
- **WHEN** 重构 Annotation 类
- **THEN** 移除以下字段：`_cached_text`(QStaticText), `_cached_font`(QFont), `_cached_best_annotation`(QString), `_cached_rect_width`(double), `_cached_width_font`(QFont)
- **AND** 保留以下字段：`_start_sample`, `_end_sample`, `_format`, `_type`, `_resIndex`, `_status`

#### Scenario: 文本访问方式变更
- **WHEN** 渲染线程需要获取注解文本
- **THEN** 通过 `_resIndex` 从 `AnnotationResTable` 获取（与当前逻辑一致）
- **AND** `AnnotationResTable` 的数据源从内存中的 `AnnotationSourceItem` 改为从 AnnotationStore 的 String Pool 读取

#### Scenario: 渲染缓存外移
- **WHEN** 渲染线程需要 QStaticText 缓存
- **THEN** 在 `DecodeTrace` 渲染层维护一个 LRU 缓存（key = annotation地址 + rect_width，value = QStaticText）
- **AND** 缓存容量限制为最近 10000 条
- **AND** 渲染完成后缓存可丢弃，不影响数据正确性

### Requirement: RowData 存储后端切换
系统 SHALL 将 `RowData` 的存储后端从 `std::vector<Annotation*>` 切换为 `AnnotationStore`。

#### Scenario: push_annotation 写入 Store
- **WHEN** 解码线程调用 `RowData::push_annotation()`
- **THEN** 将 Annotation 数据序列化为 AnnotationRecord 写入 AnnotationStore
- **AND** 不再在内存中累积 Annotation 对象

#### Scenario: get_visible_range 从 Store 读取
- **WHEN** 渲染线程调用 `RowData::get_visible_range()`
- **THEN** 通过 AnnotationStore 的二分查找 API 获取可见范围的记录索引
- **AND** 返回的索引可直接用于从 Store 读取 AnnotationRecord

#### Scenario: annotation_at 从 Store 读取
- **WHEN** 渲染线程调用 `RowData::annotation_at(index)`
- **THEN** 从 AnnotationStore 读取对应位置的 AnnotationRecord
- **AND** 构造临时 Annotation 代理对象返回（或直接返回 AnnotationRecord 指针）

#### Scenario: 并发读写安全
- **WHEN** 解码线程正在写入新注解，同时渲染线程正在读取已有注解
- **THEN** 写入操作追加到 Row 记录区末尾，不影响已有数据的读取
- **AND** Row Index 的 `ann_count` 更新使用原子操作
- **AND** 渲染线程读取的 `ann_count` 可能略滞后于实际写入数量（可接受，下一帧会读到新数据）

### Requirement: DecodeTrace 渲染路径适配
系统 SHALL 修改 `DecodeTrace` 的渲染路径，从 AnnotationStore 批量读取注解，消除逐条加锁。

#### Scenario: 批量读取可见注解
- **WHEN** `paint_mid()` 需要渲染某行的注解
- **THEN** 一次性从 AnnotationStore 读取可见范围内的所有 AnnotationRecord
- **AND** 将结果缓存在本地数组中，渲染循环中不再调用 `annotation_at()`
- **AND** 渲染循环中不再获取 `shared_mutex`

#### Scenario: 文本渲染缓存
- **WHEN** 绘制某条注解的文本
- **THEN** 先查找渲染层 LRU 缓存
- **AND** 缓存命中则直接使用 `QStaticText`
- **AND** 缓存未命中则从 AnnotationRecord 的 String Pool 读取文本，构建 QStaticText 并加入缓存

#### Scenario: Path 1（缩小模式）优化
- **WHEN** `min_annWidth < 2.0` 像素
- **THEN** 使用单次二分查找 + 顺序扫描（与 Path 2 统一），不再重复调用 `get_first_annotation_ending_after()`

### Requirement: DecoderStack 回调适配
系统 SHALL 修改 `DecoderStack::annotation_callback`，将注解数据写入 AnnotationStore。

#### Scenario: 回调写入流程
- **WHEN** `annotation_callback` 收到 `srd_proto_data`
- **THEN** 构造 AnnotationRecord
- **AND** 将文本写入 String Pool（利用 AnnotationResTable 去重）
- **AND** 将 AnnotationRecord 写入对应 Row 的记录区
- **AND** 更新 Row Index

#### Scenario: 解码开始时创建 Store
- **WHEN** `DecoderStack::begin_decode_work()` 启动解码
- **THEN** 创建 AnnotationStore 实例，关联到当前解码会话
- **AND** 将 Store 实例传递给所有 RowData

#### Scenario: 解码结束时完成 Store
- **WHEN** 解码完成
- **THEN** 调用 AnnotationStore 的 finalize 方法，刷新索引
- **AND** Store 保持打开供渲染查询

## MODIFIED Requirements

### Requirement: Annotation 文本渲染缓存
原实现（`optimize-decode-render-perf` spec）：每个 Annotation 对象内嵌 `mutable QStaticText _cached_text` 和 `mutable QString _cached_best_annotation`，按 (text, font, rect_width) 缓存。

修改为：渲染缓存从 Annotation 对象中移除，改为在 `DecodeTrace` 渲染层维护独立的 LRU 缓存。key 为 (annotation_record_offset, rect_width)，value 为 QStaticText。缓存容量上限 10000 条，LRU 淘汰。Annotation 对象不再持有任何 mutable 缓存字段。

### Requirement: RowData 查询接口
原实现（`optimize-decode-zoom-perf` spec）：`get_visible_range()`, `find_index_after_sample()`, `annotation_at()` 操作内存中的 `std::vector<Annotation*>`，每次 `annotation_at()` 获取 `shared_mutex`。

修改为：查询接口语义不变，但内部实现委托给 AnnotationStore。`annotation_at()` 不再逐条加锁，改为批量读取后本地缓存。`get_visible_range()` 使用 Store 的二分查找 API。

## REMOVED Requirements

### Requirement: 无
本次变更不移除任何现有功能。Annotation 的所有查询接口语义保持不变，仅内部存储后端从纯内存切换为 mmap 文件。
