# Tasks

- [x] Task 1: 重构SigSession解码任务调度为多线程并行
  - [x] SubTask 1.1: 修改`sigsession.h`，将`std::thread _decode_thread`改为`std::vector<std::thread> _decode_threads`，新增`std::mutex _running_tasks_mutex`和`std::vector<view::DecodeTrace*> _running_tasks`跟踪正在运行的任务
  - [x] SubTask 1.2: 重写`add_decode_task()`，为每个DecodeTrace创建独立线程执行`begin_decode_work()`，将线程和任务记录到`_running_tasks`
  - [x] SubTask 1.3: 重写`decode_task_proc()`为单任务执行函数`decode_single_task(DecodeTrace*)`，不再循环取下一个任务
  - [x] SubTask 1.4: 重写`remove_decode_task()`，在多线程环境下正确停止指定解码器线程
  - [x] SubTask 1.5: 重写`clear_all_decode_task()`，停止所有运行中的解码线程并join
  - [x] SubTask 1.6: 修改`get_top_decode_task()`逻辑或移除，适配新的多线程模型
  - [x] SubTask 1.7: 处理线程完成后的自动清理（从`_running_tasks`移除、join线程、触发`decode_end()`）

- [x] Task 2: 确保RowData写入线程安全
  - [x] SubTask 2.1: 检查`RowData::push_annotation()`是否已有`_global_visitor_mutex`保护，若无则添加
  - [x] SubTask 2.2: 检查`RowData::get_annotation_subset()`等读取方法的线程安全性

- [x] Task 3: 确保annotation_callback线程安全
  - [x] SubTask 3.1: 检查`DecoderStack::annotation_callback()`中对`_class_rows`查找和`RowData`写入的线程安全性
  - [x] SubTask 3.2: 确认`_output_mutex`是否足够保护`_samples_decoded`等字段的并发写入

- [x] Task 4: 确保LogicSnapshot读取线程安全
  - [x] SubTask 4.1: 检查`LogicSnapshot::get_samples()`是否线程安全（只读操作通常安全，但需确认内部无写入）
  - [x] SubTask 4.2: 确认实时采集模式下`get_ring_sample_count()`的线程安全性

- [x] Task 5: 验证Python GIL在多线程下的正确性
  - [x] SubTask 5.1: 确认`libsigrokdecode`中`PyGILState_Ensure/Release`在多线程调用场景下不会死锁
  - [x] SubTask 5.2: 确认`di_thread()`中的GIL管理与多线程并行解码兼容

- [ ] Task 6: 测试验证
  - [ ] SubTask 6.1: 测试单解码器解码功能正常
  - [ ] SubTask 6.2: 测试多解码器并行解码功能正常
  - [ ] SubTask 6.3: 测试解码器删除/停止在多线程下正常
  - [ ] SubTask 6.4: 测试清除所有解码器在多线程下正常
  - [ ] SubTask 6.5: 测试实时采集模式下的并行解码

# Task Dependencies
- [Task 2] depends on [Task 1] — 先建立多线程框架再加锁保护
- [Task 3] depends on [Task 1]
- [Task 4] depends on [Task 1]
- [Task 5] depends on [Task 1]
- [Task 6] depends on [Task 1, Task 2, Task 3, Task 4, Task 5]
