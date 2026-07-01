# Tasks

## Phase 1：memset 跳过 + decommit + sparse 文件（低风险增量）

- [x] Task 1: 实现 mmap 槽位 written 位图
  - [x] 1.1: 在 `LogicSnapshot` 增加 `std::vector<bool> _mmap_slot_written`，大小为 `_channel_num * _max_blocks_per_channel`，在 `first_payload` 创建 mmap 后初始化
  - [x] 1.2: 增加辅助方法 `is_slot_fresh(channel, global_block_seq)` / `mark_slot_written(channel, global_block_seq)` / `clear_slot_written(channel, global_block_seq)`
- [x] Task 2: 改造 allocate_block 的 memset 策略
  - [x] 2.1: mmap 路径分支：fresh 槽位跳过 memset 并 mark written；复用槽位（written=1）执行 memset
  - [x] 2.2: LeafBlockPool 回退路径保留无条件 memset
  - [x] 2.3: 验证 loop 模式 wrap 复用走 memset 分支
- [x] Task 3: MmapAllocator 新增 decommit_block
  - [x] 3.1: Linux 分支 `madvise(ptr, size, MADV_DONTNEED)`
  - [x] 3.2: Windows 分支 `DiscardVirtualMemory(ptr, size)`（Vista+，失败回退 `VirtualFree(MEM_DECOMMIT)`）
  - [x] 3.3: 在头文件声明并导出
- [x] Task 4: push_to_free_list mmap 分支接入 decommit
  - [x] 4.1: mmap 地址分支调用 `_mmap_alloc->decommit_block(ptr, LeafBlockSpace)` 并 `clear_slot_written`
  - [x] 4.2: 检查所有 push_to_free_list 调用点（calc_mipmap 空块、move_first_node_to_last、free_head_blocks）语义一致
  - [x] 4.3: 确认 decommit 后 lbp 置 NULL 的时序不变，读路径不踩 decommitted 页
- [x] Task 5: 磁盘缓存文件开 sparse
  - [x] 5.1: Windows 分支 CreateFileA 成功后 `DeviceIoControl(FSCTL_SET_SPARSE)`，失败仅告警
  - [x] 5.2: Linux 分支确认 ftruncate 路径稀疏性，文档化
- [ ] Task 6: 编译与回归验证
  - [x] 6.1: `cd build && ninja -j 16 && ninja install` 通过（已验证 PXView.exe 产出）
  - [ ] 6.2: 一次完整捕获（多通道、混合空闲/活跃）后检查磁盘缓存文件实际大小 vs 满配（需硬件采集，待用户测试）
  - [ ] 6.3: loop 模式长时间采集验证无数据错乱（需硬件采集，待用户测试）
  - [ ] 6.4: 解码/渲染/搜索/毛刺滤波回归无崩溃（需硬件采集，待用户测试）

## Phase 2：per-block zstd 压缩磁盘后端（架构性）

- [ ] Task 7: 新增 CompressedBlockStore 基础设施
  - [ ] 7.1: 新建 `compressed_block_store.h/.cpp`，定义压缩归档文件格式（header + 索引区 + 压缩数据区）
  - [ ] 7.2: 实现 zstd 压缩/解压单块的工具函数
  - [ ] 7.3: 实现索引表的 mmap 读写
- [ ] Task 8: 实现 pinned LRU 热缓存与 BlockHandle
  - [ ] 8.1: `BlockHandle` RAII 类，持缓存项指针 + 引用计数 pin
  - [ ] 8.2: LRU 缓存，淘汰时跳过 pin 计数 > 0 的项
- [ ] Task 9: 接入 LogicSnapshot 磁盘压缩路径
  - [ ] 9.1: `disk_cache_config` 增加压缩开关 `compression_enabled`
  - [ ] 9.2: `allocate_block` 磁盘+压缩模式：从热缓存分配 uncompressed buffer，标记 dirty
  - [ ] 9.3: 后台压缩线程：dirty 块 → zstd → 归档 → 更新索引 → 标记 clean（可淘汰）
  - [ ] 9.4: `get_block_data` 压缩模式：缓存命中返回 BlockHandle；miss 则读索引→解压→入缓存→返回 BlockHandle
- [ ] Task 10: 调用点改造为 BlockHandle
  - [ ] 10.1: `get_samples` 改为返回 BlockHandle（或内部 pin 后返回裸指针并在出作用域 unpin）
  - [ ] 10.2: `get_sample_self` / `get_display_edges` / `get_nxt_edge` / `get_pre_edge` / `pattern_search` / `apply_glitch_filter` / `copy_from` / `clone_data` 逐个评估并改造
  - [ ] 10.3: RAM 模式保留裸指针快路径（编译期/运行期分支）
- [ ] Task 11: Phase 2 验证
  - [ ] 11.1: 压缩模式下捕获+解码+渲染+搜索全链路回归
  - [ ] 11.2: 压缩率与解压延迟基准（zstd level 1/3/9）
  - [ ] 11.3: 热缓存淘汰与 pin 安全性压测
  - [ ] 11.4: 与 `redesign-buffer-management` 的关系评估（是否合并）

# Task Dependencies

- Task 2 依赖 Task 1（位图）
- Task 4 依赖 Task 1 + Task 3
- Task 6 依赖 Task 1-5 全部完成
- Phase 2 整体依赖 Phase 1（Task 6）完成且回归通过
- Task 9 依赖 Task 7 + Task 8
- Task 10 依赖 Task 9
- Task 11 依赖 Task 10
- Phase 2 与 `redesign-buffer-management` 存在范围重叠，Task 11.4 决定是否合并
