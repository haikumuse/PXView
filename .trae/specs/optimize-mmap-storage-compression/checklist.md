# Checklist

## Phase 1
- [x] `_mmap_slot_written` 位图在 `first_payload` 创建 mmap 后正确初始化，大小 = channel_num × max_blocks_per_channel
- [x] `allocate_block` 对 mmap fresh 槽位跳过 memset，对复用槽位与 LeafBlockPool 回收块执行 memset
- [x] `MmapAllocator::decommit_block` 在 Linux/Windows 分支正确实现并经验证
- [x] `push_to_free_list` 的 mmap 分支调用 decommit 并清除位图位，不再 no-op
- [x] decommit 后 lbp 置 NULL 的时序保持，`get_sample_self`/`get_samples` 不解引用 decommitted 页
- [x] Windows 磁盘缓存文件经 `FSCTL_SET_SPARSE` 标记为稀疏；失败仅告警不中止
- [ ] 一次混合空闲捕获后，磁盘缓存文件实际大小显著低于满配（需硬件采集，待用户测试）
- [ ] loop 模式长时间采集无数据错乱、无崩溃（需硬件采集，待用户测试）
- [x] `cd build && ninja -j 16 && ninja install` 通过
- [ ] 解码/渲染/搜索/毛刺滤波回归无回归（需硬件采集，待用户测试）

## Phase 2
- [ ] `CompressedBlockStore` 压缩归档格式（header+索引+数据）定义清晰并实现
- [ ] zstd 压缩/解压单块工具函数实现并单测
- [ ] `BlockHandle` RAII 引用计数 pin 正确，析构 unpin
- [ ] LRU 缓存淘汰跳过 pin>0 项
- [ ] `disk_cache_config.compression_enabled` 开关生效
- [ ] `allocate_block` 磁盘+压缩模式从热缓存分配 dirty buffer
- [ ] 后台压缩线程正确压缩 dirty 块、更新索引、标记 clean
- [ ] `get_block_data` 压缩模式缓存 miss 时按需解压并返回 BlockHandle
- [ ] `get_samples` 等长期持指针调用点改造为 BlockHandle/pin 语义，无悬垂
- [ ] RAM 模式保留裸指针快路径，无压缩开销
- [ ] 压缩模式全链路（捕获+解码+渲染+搜索）回归无崩溃
- [ ] 压缩率与解压延迟基准记录
- [ ] 评估与 `redesign-buffer-management` 是否合并并记录结论
