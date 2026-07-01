# Tasks

- [x] Task 1: 修复 `first_payload` 释放旧数据时的停线程顺序
  - [x] SubTask 1.1: 在 `LogicSnapshot::first_payload`（logicsnapshot.cpp:246-249 附近）调用 `free_data()` 之前，插入「停 async 线程 + 清队列」逻辑：置 `_async_running=false`、`_async_cv.notify_one()`、`_async_thread.join()`、锁 `_async_mutex` 清空 `_async_queue` 并复位 `_async_queue_depth`/`_async_queue_bytes_size`。复用 `clear()`（logicsnapshot.cpp:125-138）前半段的安全顺序。
  - [x] SubTask 1.2: 验证后续 `if (!_async_running)` 重启 async 线程分支（logicsnapshot.cpp:332-336）能正确重新拉起线程，因为 1.1 已将其置 false 并 join。

- [x] Task 2: 大文件删除移出持锁区 + 异步删除
  - [x] SubTask 2.1: 在 `MmapAllocator`（mmap_allocator.h）新增后台删除能力：析构 `clear()` 时 `UnmapViewOfFile` + `CloseHandle` 同步执行（快），`QFile::remove(_file_path)` 改为 `std::thread` 后台执行后 detach；或新增 `_pending_delete_path` 成员与独立删除线程。
  - [x] SubTask 2.2: 在 `LogicSnapshot::free_data()`（logicsnapshot.cpp:87-90）确保 `_mmap_alloc.reset()` 不在 `first_payload` 持有的 `_mutex` 关键段内阻塞过久——由于 MmapAllocator 析构已异步删文件，`reset()` 本身仅同步 unmap+close，可接受持锁；确认 unmap 大映射不长时间阻塞。
  - [x] SubTask 2.3: 验证下一次 `configure()` 用 `CREATE_ALWAYS` 重建同名文件时，后台删除线程不会与之冲突（文件名含时间戳/唯一后缀，或等待删除完成再 configure）。

- [x] Task 3: async 写入队列加反压
  - [x] SubTask 3.1: 在 `LogicSnapshot`（logicsnapshot.h）新增反压阈值常量 `_async_queue_high_watermark`（默认 256MB）与 `_async_queue_low_watermark`（默认 64MB）。
  - [x] SubTask 3.2: 在 `append_payload`（logicsnapshot.cpp:343-356）push 前判断 `_async_queue_bytes_size`，超 high watermark 时 `_async_cv_drain.wait()` 阻塞 feed 线程，直到 async worker 消费到 low watermark 再 push；保留 `_async_cv` 通知。
  - [x] SubTask 3.3: 在 `async_write_worker`（logicsnapshot.cpp:358-418）pop 后队列降到 low watermark 时 `notify` drain 条件变量，解除 feed 阻塞。
  - [x] SubTask 3.4: 确保 `capture_ended` 与 `first_payload` 停线程路径不会因 feed 阻塞在 drain cv 上而死锁——停线程前 notify_all 解除 feed 阻塞。

- [x] Task 4: `configure` 失败回退
  - [x] SubTask 4.1: 在 `first_payload`（logicsnapshot.cpp:327-329）`configure` 失败分支中，调用 `_mmap_alloc.reset()` 清掉失败 allocator，使后续 `allocate_block`（logicsnapshot.cpp:420-438）走 `LeafBlockPool` 内存回退路径。
  - [x] SubTask 4.2: 记录明确错误日志并广播/标记内存模式降级，便于 UI 提示用户磁盘缓存不可用。

- [x] Task 5: `capture_ended` drain 超时后强制停 async 线程
  - [x] SubTask 5.1: 在 `capture_ended`（logicsnapshot.cpp:650-653）超时 `break` 分支后，加入「`_async_running=false` + `notify_one` + `_async_thread.join()` + 锁 `_async_mutex` 清空残余队列」逻辑，丢弃未写完数据。
  - [x] SubTask 5.2: 记录警告日志说明丢弃了多少字节，便于诊断硬盘写入瓶颈。
  - [x] SubTask 5.3: 确认超时停线程后，`first_payload` 的 `if (!_async_running)` 分支能重新拉起 async 线程（与 Task 1 协同）。

# Task Dependencies

- Task 1 与 Task 5 都改 async 线程停启路径，建议同一改动批次完成避免冲突。
- Task 3（反压）依赖 Task 1（正确停线程顺序），否则反压 cv 可能与错误停线程顺序死锁。
- Task 2（异步删文件）与 Task 4（configure 回退）相互独立，可并行。
- Task 4 独立，可最早完成。
