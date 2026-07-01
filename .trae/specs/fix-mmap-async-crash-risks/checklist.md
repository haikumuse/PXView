# Checklist

- [x] `LogicSnapshot::first_payload` 在 `free_data()` 之前先停 async 线程并清空队列，与 `clear()` 安全顺序一致（logicsnapshot.cpp:246-249）
- [x] `free_data()` / `first_payload` 不再在持有 `_mutex` 时同步删除大磁盘缓存文件，文件删除改为后台异步
- [x] `MmapAllocator::clear()` 的 `QFile::remove` 在后台线程执行，不阻塞调用线程
- [x] 后台删除文件与下一次 `configure()` 的 `CREATE_ALWAYS` 重建同名文件不冲突
- [x] `append_payload` 在 `_async_queue_bytes_size` 超 high watermark 时阻塞 feed 线程，降到 low watermark 后恢复 push
- [x] `async_write_worker` 消费到 low watermark 时 notify feed 线程解除阻塞
- [x] 反压阈值可配置（默认 high=256MB / low=64MB），且不引入死锁（停线程路径先 notify_all 解除 feed 阻塞）
- [x] `first_payload` 中 `MmapAllocator::configure` 失败时 `reset()` 掉失败 allocator，回退到 `LeafBlockPool` 内存路径，不留空 `base_ptr`
- [x] `capture_ended` drain 超时 10s 后强制停 async 线程 + 清残余队列 + 记录警告日志
- [x] 超时停线程后 `first_payload` 能正确重新拉起 async 线程
- [x] 重新采集（含磁盘缓存 16GB 场景）不再出现数秒卡顿与 UI 冻结
- [x] 高速采集配慢盘场景不再 OOM 闪退，feed 线程反压可见
- [x] 编译通过（`cd build && ninja -j 16 && ninja install`），PXView.exe 启动正常
