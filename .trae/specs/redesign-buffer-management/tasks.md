# Tasks

- [x] Task 1: 实现 BlockRegistry 块位置状态机
  - [x] SubTask 1.1: 定义 BlockId 结构体（channel + block_index）、BlockLocation 枚举、状态转换规则
  - [x] SubTask 1.2: 实现 BlockRegistry 类：`register_block()`、`transition()`、`query()`、`get_blocks_by_state()` 方法
  - [x] SubTask 1.3: 实现引用计数：`acquire_ref()`、`release_ref()`，引用计数为 0 时触发回调
  - [x] SubTask 1.4: 线程安全：使用 `std::shared_mutex`，读操作用共享锁，写操作用独占锁
  - [x] SubTask 1.5: 编写单元测试验证状态转换合法性、引用计数正确性、并发安全性

- [x] Task 2: 重构 DiskBufferManager → DiskStorage
  - [x] SubTask 2.1: 在 DiskStorage 中增加可复用空间管理：`free_list` 追踪被覆盖/删除块的磁盘偏移
  - [x] SubTask 2.2: 修改 `write_block()` 逻辑：优先从 free_list 分配空间，free_list 为空时追加写入
  - [x] SubTask 2.3: 增加 `disk_full` 状态恢复机制：`reset_disk_full()` 方法
  - [x] SubTask 2.4: 保留原有接口兼容性：`open()`、`read_block()`、`save_index()`、`load_index()`、`destroy()`

- [x] Task 3: 重构 DiskReadCache → DiskBlockReader
  - [x] SubTask 3.1: 将 LRU 链表从 `std::list` 改为 `std::unordered_map` + 双向链表，实现 O(1) 查找
  - [x] SubTask 3.2: 移除淘汰回调直接操作 `_ch_data` 的逻辑，改为通过 BlockRegistry 状态转换
  - [x] SubTask 3.3: 实现 `load_batch()` 批量预加载接口
  - [x] SubTask 3.4: 修复 `load()` 中双重检查的竞态条件

- [x] Task 4: 重构 DiskWriteThread → AsyncDiskWriter
  - [x] SubTask 4.1: 使用 DiskCacheConfig 中的 `write_queue_threshold_warn` 和 `write_queue_threshold_stop` 替代硬编码值
  - [x] SubTask 4.2: 写入完成后通过 BlockRegistry 状态转换（IN_TRANSIT → DISK），而非裸回调
  - [x] SubTask 4.3: 引用计数为 0 时自动释放内存到 LeafBlockPool，引用计数 > 0 时保留内存
  - [x] SubTask 4.4: 修复 `start()` 清空残留队列时错误调用 `on_complete` 的问题
  - [x] SubTask 4.5: 实现磁盘满恢复机制

- [x] Task 5: 实现 SlidingWindow 热数据窗口管理器
  - [x] SubTask 5.1: 实现窗口容量计算：`per_channel_blocks = memory_size_gb * 1024^3 / (bytes_per_block * channel_count)`
  - [x] SubTask 5.2: 实现新块入窗和旧块逐出逻辑
  - [x] SubTask 5.3: 逐出时通过 BlockRegistry 将块从 MEMORY_HOT → MEMORY_WARM → IN_TRANSIT
  - [x] SubTask 5.4: 磁盘缓存禁用时退化为无限制窗口

- [x] Task 6: 实现 BlockStore 统一块存储抽象层
  - [x] SubTask 6.1: 定义 BlockHandle 结构体：数据指针、BlockId、引用计数句柄
  - [x] SubTask 6.2: 实现 `acquire_block()`：查询 BlockRegistry → 内存命中直接返回 / DISK 状态自动加载 / IN_TRANSIT 状态直接返回内存指针
  - [x] SubTask 6.3: 实现 `release_block()`：引用计数 -1，降为 0 时检查是否需要释放内存
  - [x] SubTask 6.4: 实现块迭代器接口：供 StoreSession / SessionDocument / SessionSnapshot 按序遍历所有块
  - [x] SubTask 6.5: 实现 `submit_new_block()`：采集线程写入新块时调用，注册到 BlockRegistry 并加入 SlidingWindow
  - [x] SubTask 6.6: 实现 `configure()`、`start()`、`stop()`、`flush()` 生命周期方法

- [x] Task 7: 改造 LogicSnapshot 数据访问
  - [x] SubTask 7.1: 移除 `_disk_cache_active`、`_block_states`、`_hot_window_blocks`、`_total_blocks_written`、`_pending_releases`、`_release_mutex` 等散落状态变量
  - [x] SubTask 7.2: 新增 `BlockStore* _block_store` 成员，替代上述所有散落状态
  - [x] SubTask 7.3: 改造 `first_payload()`：通过 `_block_store->configure()` 和 `_block_store->start()` 初始化
  - [x] SubTask 7.4: 改造 `append_cross_payload()`：块填满后调用 `_block_store->submit_new_block()`，移除手动磁盘缓存调度逻辑
  - [x] SubTask 7.5: 改造 `get_sample_self()`：使用 `_block_store->get_block_data()` 获取安全句柄，移除 `ensure_block_hot()` 调用和 NULL 检查
  - [x] SubTask 7.6: 改造 `get_samples()`：返回 BlockHandle 保护的数据指针
  - [x] SubTask 7.7: 改造 `get_display_edges()`：批量 acquire 所需块，遍历完成后批量 release
  - [x] SubTask 7.8: 改造 `get_nxt_edge_self()` / `get_pre_edge_self()`：使用 BlockStore 安全访问
  - [x] SubTask 7.9: 改造 `free_data()`：调用 `_block_store->stop()` 和 `_block_store->flush()`
  - [x] SubTask 7.10: 改造 `move_first_node_to_last()`：通过 SlidingWindow 感知环形回绕

- [x] Task 8: 改造 SigSession 配置下发
  - [x] SubTask 8.1: 确保 `_disk_cache_config` 在 `capture_init()` 中自动注入 BlockStore
  - [x] SubTask 8.2: 移除 `start_capture()` 中对 LogicSnapshot 的直接 `set_disk_cache_config()` 调用

- [x] Task 9: 改造数据消费者
  - [x] SubTask 9.1: 改造 `DecoderStack::decode_data()`：使用 BlockHandle 保护的数据指针，在切换块时 release 旧块
  - [x] SubTask 9.2: 改造 `StoreSession::store_logic_data()`：使用 BlockStore 迭代器接口
  - [x] SubTask 9.3: 改造 `SessionDocument::copy_from_logic()`：使用 BlockStore 迭代器接口，移除 friend 类直接访问 `_ch_data`
  - [x] SubTask 9.4: 改造 `SessionSnapshot::copy_from_logic()`：同上

- [ ] Task 10: 集成测试与验证
  - [x] SubTask 10.1: 编译通过，无编译错误和警告
  - [ ] SubTask 10.2: 磁盘缓存禁用时回归测试：所有行为与重构前一致
  - [ ] SubTask 10.3: 磁盘缓存启用时功能测试：数据正确写入磁盘、内存占用不超过配置值、UI 正常渲染
  - [ ] SubTask 10.4: 压力测试：16 通道 250MB/s 持续采集 10 分钟，无崩溃、无内存泄漏
  - [ ] SubTask 10.5: 解码器并发测试：采集期间运行协议解码器，无崩溃
  - [ ] SubTask 10.6: 磁盘满测试：模拟磁盘空间不足，验证优雅降级

# Task Dependencies
- [Task 1] 无依赖，可首先实现
- [Task 2] 无依赖，可首先实现（与 Task 1 并行）
- [Task 3] 依赖 [Task 1]（需要 BlockRegistry 状态转换）
- [Task 4] 依赖 [Task 1]（需要 BlockRegistry 状态转换）和 [Task 2]（使用 DiskStorage）
- [Task 5] 依赖 [Task 1]（需要 BlockRegistry 状态转换）
- [Task 6] 依赖 [Task 1] [Task 3] [Task 4] [Task 5]（组合所有组件）
- [Task 7] 依赖 [Task 6]（使用 BlockStore）
- [Task 8] 依赖 [Task 6] [Task 7]（配置注入到 BlockStore）
- [Task 9] 依赖 [Task 7]（LogicSnapshot 改造完成后才能改造消费者）
- [Task 10] 依赖 [Task 7] [Task 8] [Task 9]（所有改造完成后才能集成测试）
