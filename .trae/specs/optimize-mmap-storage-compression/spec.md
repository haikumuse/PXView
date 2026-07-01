# mmap 存储压缩与稀疏化优化 Spec

## Why

当前 `LogicSnapshot` 的 mmap 落盘链路按最坏情况一次性预留全量空间（`max_blocks_per_channel × LeafBlockSpace × channel_num`），且 `allocate_block` 对每个 2 MiB leaf block 强制 `memset` 清零，把 OS 本应懒加载的零页全部 fault-in 并写零到磁盘；空块释放时 mmap 地址分支直接 return（[logicsnapshot.cpp:1804-1810](file:///c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\pv\data\logicsnapshot.cpp#L1804-L1810)），既不 decommit 页、也不让磁盘稀疏化。结果是：磁盘后端即使信号 90% 空闲，缓存文件仍接近满配（16 通道 × 1G 采样 ≈ 2.5 GiB 几乎全零）。本 spec 通过"跳过冗余 memset + 空块 decommit + 稀疏文件 + 可选的 per-block zstd 压缩"四步，把磁盘占用与物理内存占用降到与"有效数据量"相当。

## What Changes

### Phase 1（低风险增量，落盘/内存占用 ↓50~90%）
- **跳过 mmap 首次分配的冗余 memset**：`allocate_block`（[logicsnapshot.cpp:420-438](file:///c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\pv\data\logicsnapshot.cpp#L420-L438)）仅对"回收复用"的块 `memset`；mmap 首次分配的块由 OS 懒加载零填充。引入 per-slot `written` 位图跟踪槽位是否已写过，loop 模式 wrap 复用时按位图判断是否需清零。
- **空块/淘汰块 decommit**：`push_to_free_list` 的 mmap 分支不再 no-op，改为对该块调用 `MmapAllocator::decommit_block(ptr, LeafBlockSpace)`（Linux `madvise(MADV_DONTNEED)` / Windows `DiscardVirtualMemory`），并清除对应 `written` 位图位。decommit 后该槽位回归 fresh，下次分配跳过 memset。
- **磁盘缓存文件开 sparse**：`MmapAllocator::configure`（[mmap_allocator.cpp:42-74](file:///c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\PXView\pv\data\mmap_allocator.cpp#L42-L74)）的 Windows 分支在 `CreateFileA` 后调用 `FSCTL_SET_SPARSE` 将文件标记为稀疏；Linux 的 `ftruncate` 本身已稀疏。配合 Phase 1 前两步，零区间不落盘。

### Phase 2（架构性，有效数据 10~100× 压缩）
- **per-leaf-block zstd 压缩磁盘后端**：磁盘模式下，落盘的 leaf block 经 zstd 压缩后写入压缩归档文件，配一张 mmap'd 索引表（block_id → {压缩偏移, 压缩长度, 原始长度}）。读取时按需解压进一个 pinned LRU 热缓存。
- **块句柄 pin/unpin 契约**：因压缩块不再有稳定的裸 mmap 指针，磁盘压缩模式下 `get_block_data` 的返回值改为 `BlockHandle`（RAII，引用计数 pin 住缓存项，析构 unpin）。`get_samples` 等长期持指针的调用点改为持 `BlockHandle`。
- **BREAKING**：`MmapAllocator::get_block_data` 的裸 `void*` 返回在磁盘压缩模式下不再保证跨调用的指针稳定性；调用方必须改用 `BlockHandle`。RAM（匿名 mmap）模式保留裸指针快路径。

## Impact

- Affected specs:
  - `add-stream-disk-cache`（磁盘缓存初建，本次优化其落盘体积）
  - `redesign-buffer-management`（BlockStore 重构；Phase 2 的 `BlockHandle`/pinned 缓存与其目标一致，若该 spec 推进，Phase 2 应并入其中，不在本 spec 重复实现）
  - `fix-mmap-async-crash-risks`（异步写入链路；Phase 1 的 memset 跳过需保证不破坏其 drain/回退语义）
- Affected code:
  - `PXView/pv/data/mmap_allocator.h` / `.cpp` — `decommit_block`、`configure` 开 sparse、位图查询接口
  - `PXView/pv/data/logicsnapshot.h` / `.cpp` — `allocate_block` memset 策略、`push_to_free_list` decommit、`_mmap_slot_written` 位图、Phase 2 的 `BlockHandle` 与调用点改造
  - 新增 `PXView/pv/data/compressed_block_store.h` / `.cpp`（Phase 2）
  - `PXView/pv/data/disk_cache_config.h` — 压缩开关配置项

## ADDED Requirements

### Requirement: mmap 首次分配跳过冗余 memset

系统在 mmap 后端首次分配一个 leaf block 时，SHALL NOT 对该块执行 `memset` 清零；该块的页 SHALL 由 OS 懒加载零填充。系统 SHALL 维护一个 per-slot 位图记录每个 mmap 槽位是否已被写入，仅在槽位被复用（位图标记已写）时才执行 `memset`。

#### Scenario: 首次分配 mmap 块
- **WHEN** `allocate_block` 从 `_mmap_alloc` 取得一个此前未写过的槽位（位图为 0）
- **THEN** 系统跳过 `memset`，直接返回指针；该块首次被 `append_cross_payload` 写入时，未写区域由 OS 零填充
- **AND** 位图对应位置 1

#### Scenario: loop 模式 wrap 复用槽位
- **WHEN** loop 模式下环形缓冲 wrap，`allocate_block` 取得一个位图标记为已写的槽位
- **THEN** 系统执行 `memset` 清除残留数据后再返回

#### Scenario: LeafBlockPool 回退路径
- **WHEN** mmap 不可用，`allocate_block` 从 `LeafBlockPool::acquire()` 取得回收块
- **THEN** 系统执行 `memset`（回收块可能含脏数据），与原行为一致

### Requirement: 空块/淘汰块 decommit

系统在通过 `push_to_free_list` 释放一个 mmap 地址块时，SHALL 对该块的页区间执行 decommit（Linux `madvise(MADV_DONTNEED)` / Windows `DiscardVirtualMemory`），归还物理页给 OS；并 SHALL 清除该槽位的 `written` 位图位，使其在下次分配时被视为 fresh。

#### Scenario: calc_mipmap 判定空块释放
- **WHEN** `calc_mipmap` 在 `isEnd` 时检测到 `level3==0`（整块无跳变），调用 `push_to_free_list` 释放该 mmap 块
- **THEN** 该块的物理页被 decommit，磁盘文件对应区间回归稀疏零（配合 sparse 文件）
- **AND** 位图位清除，下次分配该槽位时跳过 memset

#### Scenario: loop 模式 move_first_node_to_last
- **WHEN** loop 模式环形推进，`move_first_node_to_last` 释放旧 root node 的 mmap lbp
- **THEN** 这些块被 decommit，位图位清除

#### Scenario: decommit 后的读访问安全性
- **WHEN** 一个块被 decommit 后，其 `_ch_data[].lbp[]` 已置 NULL
- **THEN** 任何读路径（`get_sample_self`/`get_samples`）SHALL NOT 解引用该 NULL 指针；`get_sample_self` 通过 root node 的 `first`/`tog` 位重建常量电平，`get_samples` 返回 NULL 由调用方处理

### Requirement: 磁盘缓存文件稀疏化

系统在 Windows 上创建磁盘缓存文件后，SHALL 通过 `FSCTL_SET_SPARSE` 将其标记为稀疏文件，使零区间不占用磁盘空间。Linux 上 `ftruncate` 创建的文件本即为稀疏。配合 memset 跳过与 decommit，磁盘实际占用 SHALL 仅反映非零有效数据。

#### Scenario: 磁盘缓存文件创建
- **WHEN** `MmapAllocator::configure` 以 `use_disk_file=true` 创建缓存文件
- **THEN** Windows 分支在 `CreateFileA` 成功后立即调用 `DeviceIoControl(FSCTL_SET_SPARSE)`；失败时记录警告但不中止（退化为非稀疏）

#### Scenario: 空闲捕获的磁盘占用
- **WHEN** 一个 16 通道 × 1G 采样、信号 90% 空闲的捕获完成
- **THEN** 磁盘缓存文件实际占用 SHALL 显著低于满配 2.5 GiB（预期 < 500 MiB，取决于有效跳变密度）

### Requirement: per-block zstd 压缩磁盘后端（Phase 2）

系统在磁盘模式且启用压缩时，SHALL 对每个落盘的 leaf block 用 zstd 压缩后写入压缩归档，并维护一张 mmap'd 索引表支持 O(1) 定位。读取时 SHALL 按需解压进 pinned LRU 热缓存，缓存项在被持有时（`BlockHandle` 引用计数 > 0）SHALL NOT 被淘汰。

#### Scenario: 落盘压缩
- **WHEN** 一个 leaf block 完成写入与 mipmap 计算，被提交到磁盘
- **THEN** 系统用 zstd 压缩该块（含 raw + 3 级 mipmap），追加写入压缩归档，更新索引表项 `{压缩偏移, 压缩长度, 原始长度}`

#### Scenario: 按需解压读取
- **WHEN** `get_block_data` 请求一个不在热缓存的块
- **THEN** 系统按索引读取压缩区间，zstd 解压进热缓存，返回 pin 住该缓存项的 `BlockHandle`
- **AND** 调用方析构 `BlockHandle` 时引用计数减 1，归零后该缓存项可被 LRU 淘汰

#### Scenario: RAM 模式快路径不受影响
- **WHEN** 使用匿名 mmap（非磁盘、非压缩）模式
- **THEN** `get_block_data` 走原有裸指针快路径，不经过压缩/解压与 `BlockHandle` 开销

## MODIFIED Requirements

### Requirement: allocate_block 的清零策略

`allocate_block` 在取得块后，SHALL 按来源决定是否清零：
- mmap 槽位且 `written` 位图为 0（fresh）：不清零，置位图
- mmap 槽位且 `written` 位图为 1（复用）：清零
- `LeafBlockPool` 回收块：清零

原行为是无条件 `memset`，新行为按位图与来源区分。

### Requirement: MmapAllocator 的块生命周期管理

`MmapAllocator` SHALL 新增 `decommit_block(void* ptr, uint64_t size)` 方法，用于归还指定块的物理页（不释放虚拟映射）。`configure` 在磁盘模式 SHALL 创建稀疏文件。位图查询/置位/清位接口 SHALL 由 `LogicSnapshot` 自维护（位图索引 = `channel * _max_blocks_per_channel + global_block_seq`）。
