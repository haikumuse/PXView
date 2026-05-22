# 统一块存储抽象层（BlockStore）重构 Spec

## Why

当前 LogicSnapshot 的磁盘缓存实现存在严重的架构缺陷：内存读写和磁盘读写走的是两条完全分离的代码路径，导致数据块的"在内存/在磁盘/传输中"三种状态散落在多个类中，缺乏统一的状态机管理。这引发了以下已确认的 Bug：

1. **Use-After-Free**：`DiskReadCache` LRU 淘汰时释放内存，但 `_ch_data[].lbp` 指针未同步置 NULL，UI/解码线程解引用野指针崩溃
2. **NULL 解引用**：`get_sample_self()` 在 `ensure_block_hot()` 失败后直接解引用 lbp，崩溃
3. **竞态条件**：淘汰回调在 `DiskReadCache::_mutex` 内修改 `_ch_data` 和 `_block_states`，但未持有 `LogicSnapshot::_mutex`
4. **内存泄漏**：写线程队列无上限、Ping-Pong 缓冲交换丢失配置、`free_data()` 重置 `_disk_cache_active`
5. **磁盘空间无限增长**：`DiskBufferManager` 采用只追加策略，被覆盖的块空间从不回收

根本原因是**缺少统一的数据访问抽象层**——上层代码（渲染、解码、搜索）需要自己判断数据在内存还是磁盘，手动调用 `ensure_block_hot()`，手动检查 NULL。这种"裸指针 + 散落状态检查"的模式注定无法安全地支持内存-磁盘分层存储。

## What Changes

- 新增 `BlockStore` 类：统一块存储抽象层，所有数据访问通过 `acquire_block()`/`release_block()` 进行，内部透明处理内存命中/磁盘加载/传输等待
- 新增 `BlockRegistry` 类：块位置状态机，统一管理块的 MEMORY_HOT / MEMORY_WARM / DISK / IN_TRANSIT / READ_CACHE 状态转换，替代散落的 `_block_states` map
- 新增 `SlidingWindow` 类：热数据窗口管理器，替代 `append_cross_payload()` 中硬编码的 `_hot_window_blocks` 逐出逻辑
- 重构 `DiskWriteThread` → `AsyncDiskWriter`：基于 `BlockRegistry` 状态驱动，块进入 IN_TRANSIT 时自动提交写入，写入完成后状态转为 DISK 并安全释放内存
- 重构 `DiskReadCache` → `DiskBlockReader`：O(1) 哈希查找 + 与 `BlockRegistry` 联动的淘汰回调，淘汰时通过状态机而非裸指针操作
- 重构 `DiskBufferManager` → `DiskStorage`：增加空间回收（GC），支持覆盖写入旧块
- 修改 `LogicSnapshot`：移除所有直接操作 `_ch_data[].lbp` 的磁盘缓存逻辑，改为通过 `BlockStore` 访问；`get_sample_self()` 等方法使用 `BlockStore::acquire_block()` 获取安全句柄
- 修改 `SigSession`：磁盘缓存配置通过 `BlockStore::configure()` 一次性下发，不再依赖 Ping-Pong 缓冲交换后的二次注入
- **BREAKING** `LogicSnapshot` 的 `_ch_data` 不再暴露给 `SessionDocument` / `SessionSnapshot`，改为通过 `BlockStore` 的迭代器接口访问

## Impact

- Affected specs: Stream 模式数据采集、协议解码、UI 渲染、数据保存、搜索
- Affected code:
  - `PXView/pv/data/logicsnapshot.h` / `logicsnapshot.cpp` — 核心重构
  - `PXView/pv/data/disk_write_thread.h` / `disk_write_thread.cpp` — 重构为 `AsyncDiskWriter`
  - `PXView/pv/data/disk_buffer_manager.h` / `disk_buffer_manager.cpp` — 重构为 `DiskStorage`
  - `PXView/pv/data/disk_read_cache.h` / `disk_read_cache.cpp` — 重构为 `DiskBlockReader`
  - `PXView/pv/data/disk_cache_config.h` — 扩展配置
  - `PXView/pv/sigsession.h` / `sigsession.cpp` — 配置下发改造
  - `PXView/pv/data/sessiondocument.cpp` — 改用 BlockStore 迭代器
  - `PXView/pv/data/sessionsnapshot.cpp` — 改用 BlockStore 迭代器
  - `PXView/pv/data/decoderstack.cpp` — 改用 BlockStore 安全句柄
  - `PXView/pv/data/storesession.cpp` — 改用 BlockStore 迭代器

## ADDED Requirements

### Requirement: 统一块存储抽象层（BlockStore）

系统 SHALL 提供 `BlockStore` 类作为 LogicSnapshot 数据访问的唯一入口，上层代码不再直接操作 `_ch_data[].lbp` 指针。

#### Scenario: 读取内存中的块
- **WHEN** 上层代码通过 `acquire_block(channel, block_index)` 请求一个块
- **AND** 该块在内存中（状态为 MEMORY_HOT 或 MEMORY_WARM）
- **THEN** BlockStore 返回一个 `BlockHandle`，其中包含指向内存数据的指针
- **AND** BlockHandle 的引用计数 +1，确保数据在持有期间不会被释放

#### Scenario: 读取磁盘上的块
- **WHEN** 上层代码通过 `acquire_block(channel, block_index)` 请求一个块
- **AND** 该块仅在磁盘上（状态为 DISK）
- **THEN** BlockStore 自动从磁盘加载该块到读缓存
- **AND** 将块状态更新为 READ_CACHE
- **AND** 返回 `BlockHandle`，引用计数 +1

#### Scenario: 读取正在传输中的块
- **WHEN** 上层代码通过 `acquire_block(channel, block_index)` 请求一个块
- **AND** 该块正在被写入磁盘（状态为 IN_TRANSIT，数据仍在内存中）
- **THEN** BlockStore 直接返回内存中的数据指针（无需等待磁盘写入完成）
- **AND** 引用计数 +1，确保写线程不会在持有期间释放该块

#### Scenario: 释放块句柄
- **WHEN** 上层代码调用 `release_block(handle)`
- **THEN** BlockHandle 的引用计数 -1
- **AND** 当引用计数降为 0 且块状态为 IN_TRANSIT 时，将该块加入待释放队列
- **AND** 待释放队列在下次 `append_payload()` 时统一处理

#### Scenario: 块不存在
- **WHEN** 上层代码请求一个不存在的块（超出已采集范围）
- **THEN** BlockStore 返回空 `BlockHandle`（`data == nullptr`）
- **AND** 调用者可通过 `handle.is_valid()` 检查

### Requirement: 块位置状态机（BlockRegistry）

系统 SHALL 提供 `BlockRegistry` 类，统一管理所有数据块的位置状态，替代散落在多个类中的状态跟踪逻辑。

#### Scenario: 状态定义
- **WHEN** 系统运行时
- **THEN** 每个数据块处于以下状态之一：
  - `MEMORY_HOT`：在内存中，正在被写入或属于热数据窗口
  - `MEMORY_WARM`：在内存中，已完成 Mip-map 计算，等待写入磁盘
  - `IN_TRANSIT`：正在被异步写入磁盘，数据仍在内存中（引用计数保护）
  - `DISK`：仅在磁盘上，内存中无数据
  - `READ_CACHE`：从磁盘加载到读缓存中

#### Scenario: 状态转换规则
- **WHEN** 数据块状态变化时
- **THEN** 只允许以下转换路径：
  - `MEMORY_HOT → MEMORY_WARM`：块超出热数据窗口
  - `MEMORY_WARM → IN_TRANSIT`：块被提交给异步写线程
  - `IN_TRANSIT → DISK`：写线程完成写入且引用计数为 0
  - `IN_TRANSIT → MEMORY_HOT`：写线程完成前块被 acquire（读请求命中传输中的块）
  - `DISK → READ_CACHE`：块从磁盘加载到读缓存
  - `READ_CACHE → DISK`：读缓存 LRU 淘汰
- **AND** 所有状态转换通过 `BlockRegistry::transition(block_id, from, to)` 原子操作执行
- **AND** 非法状态转换（如 `DISK → MEMORY_HOT`）触发断言失败

#### Scenario: 线程安全
- **WHEN** 多个线程并发访问 BlockRegistry
- **THEN** 所有状态查询和转换操作通过内部读写锁（`std::shared_mutex`）保护
- **AND** 状态查询使用读锁，状态转换使用写锁
- **AND** 状态转换操作是原子的：查询当前状态 + 检查合法性 + 设置新状态在同一个写锁内完成

### Requirement: 热数据窗口管理器（SlidingWindow）

系统 SHALL 提供 `SlidingWindow` 类，管理内存中热数据块的保留和逐出策略。

#### Scenario: 窗口容量计算
- **WHEN** 磁盘缓存配置生效时
- **THEN** SlidingWindow 根据配置的 `memory_size_gb` 和通道数计算每个通道的热窗口块数
- **AND** 计算公式为：`per_channel_blocks = memory_size_gb * 1024^3 / (bytes_per_block * channel_count)`
- **AND** 总内存占用严格等于 `memory_size_gb`，不会因通道数增加而倍增

#### Scenario: 新块进入窗口
- **WHEN** 采集线程填充完一个新数据块
- **THEN** 新块标记为 MEMORY_HOT 并加入窗口尾部
- **AND** 如果窗口大小超过 `per_channel_blocks`，窗口头部的最旧块被逐出

#### Scenario: 逐出旧块
- **WHEN** 窗口头部最旧块被逐出
- **THEN** 该块状态从 MEMORY_HOT 转为 MEMORY_WARM
- **AND** 该块被提交给 AsyncDiskWriter 进行异步写入
- **AND** 该块状态转为 IN_TRANSIT

#### Scenario: 磁盘缓存禁用
- **WHEN** 磁盘缓存未启用
- **THEN** SlidingWindow 不限制窗口大小，所有块保持 MEMORY_HOT 状态
- **AND** 行为与重构前完全一致

### Requirement: 异步磁盘写入器（AsyncDiskWriter）

系统 SHALL 提供 `AsyncDiskWriter` 类替代现有 `DiskWriteThread`，基于 BlockRegistry 状态驱动。

#### Scenario: 提交写入任务
- **WHEN** SlidingWindow 将块状态转为 IN_TRANSIT 并提交写入任务
- **THEN** AsyncDiskWriter 将任务加入内部队列
- **AND** 队列深度上限由 `DiskCacheConfig.write_queue_threshold_stop` 配置（默认 256 个块）
- **AND** 队列满时 `submit()` 阻塞等待（反压机制），而非无限增长

#### Scenario: 写入完成回调
- **WHEN** 异步线程完成一个块的磁盘写入
- **THEN** 通过 BlockRegistry 将块状态从 IN_TRANSIT 转为 DISK
- **AND** 如果该块引用计数为 0，将内存释放回 LeafBlockPool
- **AND** 如果该块引用计数 > 0（有读者持有），保留内存直到引用计数降为 0

#### Scenario: 采集停止刷写
- **WHEN** 采集停止调用 `flush()`
- **THEN** 等待所有队列中的写入任务完成
- **AND** 所有 IN_TRANSIT 状态的块转为 DISK 状态

#### Scenario: 磁盘满处理
- **WHEN** 磁盘空间不足导致写入失败
- **THEN** 标记磁盘满状态，停止接受新写入任务
- **AND** 已提交的任务继续尝试写入
- **AND** 当磁盘空间恢复（用户清理空间后），可通过 `reset_disk_full()` 恢复写入

### Requirement: 磁盘块读取器（DiskBlockReader）

系统 SHALL 提供 `DiskBlockReader` 类替代现有 `DiskReadCache`，提供 O(1) 查找和与 BlockRegistry 联动的淘汰机制。

#### Scenario: 查找缓存块
- **WHEN** BlockStore 请求加载一个 DISK 状态的块
- **THEN** DiskBlockReader 首先在 `std::unordered_map` 中查找（O(1)）
- **AND** 命中则返回数据指针，更新 LRU 顺序
- **AND** 未命中则从磁盘读取，加入缓存

#### Scenario: LRU 淘汰
- **WHEN** 缓存总大小超过 `read_cache_bytes` 限制
- **THEN** 淘汰最久未访问的块
- **AND** 通过 BlockRegistry 将块状态从 READ_CACHE 转为 DISK
- **AND** 释放该块的内存
- **AND** **不再**直接操作 `_ch_data[].lbp` 指针（由 BlockRegistry 状态驱动）

#### Scenario: 批量预加载
- **WHEN** `get_display_edges()` 需要访问多个连续块
- **THEN** DiskBlockReader 支持批量预加载，减少磁盘 I/O 次数

### Requirement: 磁盘存储（DiskStorage）

系统 SHALL 提供 `DiskStorage` 类替代现有 `DiskBufferManager`，增加空间回收能力。

#### Scenario: 覆盖写入
- **WHEN** 循环模式下旧块需要被新数据覆盖
- **THEN** DiskStorage 将旧块的磁盘空间标记为可复用
- **AND** 后续写入优先使用可复用空间，而非无限追加
- **AND** 磁盘文件大小不超过配置的 `disk_size_gb`

#### Scenario: 空间整理
- **WHEN** 磁盘文件中碎片空间超过 50%
- **THEN** DiskStorage 在采集停止时执行空间整理（可选）
- **AND** 整理期间不影响正在进行的读取操作

#### Scenario: 索引持久化
- **WHEN** 采集停止或应用退出
- **THEN** DiskStorage 将块索引写入磁盘文件
- **AND** 索引包含每个块的通道号、逻辑块索引、磁盘偏移、状态
- **AND** 下次启动时可从索引恢复

### Requirement: LogicSnapshot 数据访问改造

系统 SHALL 改造 `LogicSnapshot` 类，使其通过 BlockStore 进行所有数据访问。

#### Scenario: get_sample_self 改造
- **WHEN** `get_sample_self()` 需要读取某个采样点
- **THEN** 通过 `BlockStore::acquire_block()` 获取块句柄
- **AND** 从块句柄中读取数据
- **AND** 读取完成后调用 `BlockStore::release_block()`
- **AND** 不再存在 `if (lbp == NULL && _disk_cache_active)` 的散落检查

#### Scenario: get_display_edges 改造
- **WHEN** `get_display_edges()` 需要遍历多个块
- **THEN** 批量 acquire 所需块，遍历完成后批量 release
- **AND** 块在遍历期间保证不被释放

#### Scenario: DecoderStack 数据访问改造
- **WHEN** 解码线程通过 `get_samples()` 获取数据指针
- **THEN** 返回的指针由 BlockHandle 的引用计数保护
- **AND** 解码线程在切换到下一个块时 release 当前块
- **AND** 不再存在跨锁使用裸指针的 Use-After-Free 风险

#### Scenario: 数据保存改造
- **WHEN** StoreSession 需要读取块数据
- **THEN** 通过 BlockStore 的迭代器接口按序访问所有块
- **AND** 不再直接访问 `_ch_data` 内部结构

#### Scenario: 数据复制改造
- **WHEN** SessionDocument / SessionSnapshot 需要复制 LogicSnapshot 数据
- **THEN** 通过 BlockStore 的迭代器接口访问所有块
- **AND** 不再通过 friend 类直接操作 `_ch_data`

### Requirement: 配置下发改造

系统 SHALL 改造 `SigSession` 的磁盘缓存配置下发流程，消除 Ping-Pong 缓冲交换导致的配置丢失。

#### Scenario: 配置持久化
- **WHEN** 用户在 UI 中配置磁盘缓存参数
- **THEN** 配置存储在 `SigSession::_disk_cache_config` 成员中
- **AND** 每次 `capture_init()` 创建新的缓冲对象时，自动将配置注入 BlockStore
- **AND** 不再依赖 `start_capture()` 到 `first_payload()` 之间的时序正确性

#### Scenario: 配置变更
- **WHEN** 用户在采集前修改磁盘缓存配置
- **THEN** 新配置在下次采集启动时生效
- **AND** 当前正在进行的采集不受影响

## MODIFIED Requirements

### Requirement: LogicSnapshot 环形缓冲区
原要求：环形缓冲区通过 `move_first_node_to_last()` 在内存中移动根节点实现循环，磁盘上的旧数据区域通过索引标记为可覆盖。

修改为：环形缓冲区通过 `move_first_node_to_last()` 在内存中移动根节点实现循环。SlidingWindow 感知环形回绕，被移除的块通过 BlockRegistry 状态机正确处理（MEMORY_HOT → MEMORY_WARM → IN_TRANSIT → DISK）。DiskStorage 支持覆盖写入旧块的磁盘空间。

### Requirement: LogicSnapshot 叶块分配
原要求：叶块分配优先从 WARM/COLD 状态的叶块中复用内存；如果无可用叶块则 `malloc`；`malloc` 失败时尝试释放读缓存中的叶块后重试；全部失败才设置 `_memory_failed`。

修改为：叶块通过 `LeafBlockPool::acquire()` 分配。当内存池耗尽时，BlockStore 尝试释放引用计数为 0 的 DISK 状态块的内存（通过 BlockRegistry 查询）。如果仍无可用内存，尝试缩小 DiskBlockReader 的缓存。全部失败才设置 `_memory_failed`。

### Requirement: DiskCacheConfig 配置项
原要求：`write_queue_threshold_warn` 和 `write_queue_threshold_stop` 定义了但未使用。

修改为：`write_queue_threshold_warn` 和 `write_queue_threshold_stop` 被 AsyncDiskWriter 实际使用。`write_queue_threshold_warn` 触发 UI 警告，`write_queue_threshold_stop` 为 `submit()` 阻塞的队列深度上限。

## REMOVED Requirements

### Requirement: LogicSnapshot 直接管理磁盘缓存状态
**Reason**: 磁盘缓存状态管理由 BlockRegistry 统一负责，LogicSnapshot 不再直接操作 `_disk_cache_active`、`_block_states`、`_hot_window_blocks`、`_total_blocks_written` 等散落的状态变量。
**Migration**: 所有通过 `_block_states` 查询块状态的代码改为通过 BlockRegistry 查询；所有 `if (lbp == NULL && _disk_cache_active)` 的检查改为通过 BlockStore::acquire_block() 获取安全句柄。

### Requirement: DiskReadCache 淘汰回调直接操作 _ch_data
**Reason**: 淘汰回调跨锁域操作 `_ch_data[].lbp` 和 `_block_states` 构成竞态条件。
**Migration**: 淘汰通过 BlockRegistry 状态转换完成，不再直接操作 LogicSnapshot 内部数据。
