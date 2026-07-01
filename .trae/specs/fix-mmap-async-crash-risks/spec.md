# 修复 mmap 异步写入链路闪退/清空卡顿 Spec

## Why

`LogicSnapshot` 的 mmap 异步写入链路存在多处缺陷：重新开始采集时 `first_payload` 在持有 `_mutex` 的情况下同步删除大磁盘缓存文件（默认 16GB），导致清空卡顿数秒~数十秒并阻塞 UI/解码线程；`free_data` 没有先停 async 写入线程就 reset mmap，存在竞态闪退；async 写入队列无反压，高速采集配慢盘时内存暴涨 OOM；`MmapAllocator::configure` 失败不回退留空 `base_ptr`，async 线程踩空指针；`capture_ended` drain 超时后未停 async 线程，紧接的下一次采集存在竞态。这些问题导致用户重新采集时卡顿、容易闪退。

## What Changes

- **修复 `LogicSnapshot::first_payload` 释放旧数据时的停线程顺序**：在 reset 旧 mmap 之前，先停 `_async_running` + join async 线程 + 清空 `_async_queue`，复用 `clear()` 的安全顺序，避免与仍在写盘的 async 线程竞态。
- **大文件删除移出持锁区**：`free_data()` 内 `_mmap_alloc.reset()` 触发 `MmapAllocator::clear()` 同步 `UnmapViewOfFile` + `CloseHandle` + `QFile::remove` 删除大磁盘缓存文件。将文件删除改为后台异步执行，并把 `_mmap_alloc.reset()` 移到 `_mutex` 释放之后，避免持锁删大文件阻塞 feed/UI/解码线程。
- **async 写入队列加反压**：`append_payload` 当前无限 push，无大小上限。加入水位阈值（按总字节数），超限时阻塞 feed 线程或丢弃最旧包，防止高速采集配慢盘时 OOM 闪退。
- **`MmapAllocator::configure` 失败回退**：`first_payload` 中 `configure` 失败时当前只打印 err 不回退，留下 `_base_ptr=null` 的 allocator 供 async 线程解引用空指针。失败时应 `reset()` 掉 `_mmap_alloc` 并降级为内存模式（或 `LeafBlockPool` 回退路径），不留给 async 线程踩空。
- **`capture_ended` drain 超时后强制停 async 线程**：当前 10s 超时 `break` 后 async 线程仍 running，紧接的 `first_payload` 存在竞态。超时后应设 `_async_running=false` 并 join 线程，丢弃残余队列，保证下一次采集干净启动。

## Impact

- Affected specs: `add-stream-disk-cache`（磁盘缓存初建，本次修复其运行期缺陷）、`redesign-buffer-management`（缓冲管理，本次修复其 mmap 落地实现）
- Affected code:
  - `PXView/pv/data/logicsnapshot.cpp` — `first_payload`、`free_data`、`clear`、`append_payload`、`capture_ended`、`async_write_worker`
  - `PXView/pv/data/logicsnapshot.h` — 反压阈值字段、反压相关原子量
  - `PXView/pv/data/mmap_allocator.cpp` — `clear()` 异步删除文件、`configure` 失败语义
  - `PXView/pv/data/mmap_allocator.h` — 异步删除线程/标志成员

## ADDED Requirements

### Requirement: mmap 释放顺序安全

系统在重新开始采集释放旧 mmap 数据时，SHALL 在 reset mmap allocator 之前先停止 async 写入线程并清空其写入队列，且 SHALL NOT 在持有 `_mutex` 时同步删除大磁盘缓存文件。

#### Scenario: 重新采集释放旧 mmap
- **WHEN** 用户在采集结束后再次点开始采集，`first_payload` 检测到 `total_sample_count`/`channel_num` 变化需重建
- **THEN** 系统先置 `_async_running=false` + notify + join async 线程 + 清空 `_async_queue`，再 reset 旧 `_mmap_alloc`，最后才创建新 mmap 并重启 async 线程
- **AND** 旧磁盘缓存文件的 `QFile::remove` 在后台线程异步执行，不阻塞 feed/UI/解码线程

#### Scenario: configure 失败回退
- **WHEN** `MmapAllocator::configure` 在新采集时失败（磁盘满/权限/映射失败）
- **THEN** 系统 `reset()` 掉失败的 `_mmap_alloc`，回退到 `LeafBlockPool` 内存分配路径，记录错误日志，不留下 `base_ptr=null` 的 allocator 供 async 线程踩空

### Requirement: async 写入队列反压

系统 SHALL 对 async 写入队列施加字节级反压，防止高速采集配慢盘时队列无限增长导致 OOM 闪退。

#### Scenario: 队列超水位阻塞
- **WHEN** `_async_queue_bytes_size` 超过阈值（默认 256MB，可配置）
- **THEN** `append_payload` 阻塞 feed 线程等待队列排空到低水位后再 push，避免内存无界增长
- **AND** 阻塞期间不影响 async 写入线程的正常消费

#### Scenario: capture_ended drain 超时强制停线程
- **WHEN** `capture_ended` 等待 async 队列排空超过 10s 仍未排空
- **THEN** 系统设 `_async_running=false` + join async 线程 + 清空残余队列，丢弃未写完数据并记录警告日志，保证下一次采集干净启动

## MODIFIED Requirements

### Requirement: LogicSnapshot 采集生命周期

`LogicSnapshot` 的 `first_payload` 在重建数据结构时，SHALL 采用「先停 async 线程 → 清队列 → free_data(reset mmap) → 重建结构 → 新建 mmap → 重启 async 线程」的顺序，与 `clear()` 的安全顺序保持一致。`free_data()` SHALL NOT 在 async 线程未停止时被外部直接调用 reset mmap。
