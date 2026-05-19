# Tasks

- [x] Task 1: 消除 on_frame_ended 中重复的 copy_data_to_document 调用
  - [x] SubTask 1.1: 在 `mainwindow.cpp` 的 `on_frame_ended()` 中，将 `_session->copy_data_to_document(ctx->document())` 改为条件调用：仅当 `_session->get_active_document() != ctx->document()` 时才执行
  - [x] SubTask 1.2: 确认 `SigSession::get_active_document()` 是公开方法（若不是则添加）

- [x] Task 2: 创建 LeafBlockPool 内存池
  - [x] SubTask 2.1: 新建 `PXView/pv/data/leaf_block_pool.h`，实现 `LeafBlockPool` 单例类（acquire/release/drain/idle_count/set_max_pool_size）
  - [x] SubTask 2.2: 在 `appcontrol.cpp` 的 `Destroy()` 中调用 `LeafBlockPool::instance().drain()`

- [x] Task 3: 替换 logicsnapshot.cpp 中的 malloc/free 为内存池调用
  - [x] SubTask 3.1: 添加 `#include "leaf_block_pool.h"`
  - [x] SubTask 3.2: 替换 `free_data()` 中 2 处 `free` 为 `LeafBlockPool::instance().release()`（L101 的 lbp free 和 L110-112 的 _free_block_list free）
  - [x] SubTask 3.3: 替换 `append_cross_payload()` 中 5 处 `malloc(LeafBlockSpace)` 为 `LeafBlockPool::instance().acquire(LeafBlockSpace)`（L398, L471, L533, L563, L590）
  - [x] SubTask 3.4: 替换 `copy_from()` 中 1 处 `malloc(LeafBlockSpace)` 为 `LeafBlockPool::instance().acquire(LeafBlockSpace)`（L694）
  - [x] SubTask 3.5: 替换 `decode_end()` 中 1 处 `free` 为 `LeafBlockPool::instance().release()`（L1728）
  - [x] SubTask 3.6: 替换 `free_decode_lpb()` 中 1 处 `free` 为 `LeafBlockPool::instance().release()`（L1741）
  - [x] SubTask 3.7: 替换 `set_sample_range()` 中 1 处 `malloc(LeafBlockSpace)` 为 `LeafBlockPool::instance().acquire(LeafBlockSpace)`（L1813）
  - [x] SubTask 3.8: 替换 `clone_data()` 中 1 处 `malloc(LeafBlockSpace)` 为 `LeafBlockPool::instance().acquire(LeafBlockSpace)`（L1971）
  - [x] SubTask 3.9: 替换 `apply_glitch_filter()` 中 1 处 `malloc(LeafBlockSpace)` 为 `LeafBlockPool::instance().acquire(LeafBlockSpace)`（L2057）

- [x] Task 4: 替换 sessiondocument.cpp 和 sessionsnapshot.cpp 中的 malloc 为内存池调用
  - [x] SubTask 4.1: 在 `sessiondocument.cpp` 中添加 `#include "leaf_block_pool.h"`，替换 `copy_from_logic()` 中 1 处 `malloc(LogicSnapshot::LeafBlockSpace)` 为 `LeafBlockPool::instance().acquire(...)`（L112）
  - [x] SubTask 4.2: 在 `sessionsnapshot.cpp` 中添加 `#include "leaf_block_pool.h"`，替换 `copy_from_logic()` 中 1 处 `malloc(LogicSnapshot::LeafBlockSpace)` 为 `LeafBlockPool::instance().acquire(...)`（L210）

- [x] Task 5: copy_data_to_document 异步化
  - [x] SubTask 5.1: 在 `sigsession.h` 中添加 `volatile bool _copy_in_progress` 成员
  - [x] SubTask 5.2: 修改 `sigsession.cpp` 中 `DSV_MSG_REV_END_PACKET` 处理逻辑：将 `copy_data_to_document()` 移至后台线程，`frame_ended()` 信号提前发出
  - [x] SubTask 5.3: 后台线程拷贝完成后，通过 `trigger_message(DSV_MSG_COPY_TO_DOC_DONE)` 通知 UI 线程
  - [x] SubTask 5.4: 在 `exec_capture()` 中添加 `_copy_in_progress` 等待保护

# Task Dependencies
- [Task 2] depends on nothing (可独立开始)
- [Task 3] depends on [Task 2] (需要 LeafBlockPool 类)
- [Task 4] depends on [Task 2] (需要 LeafBlockPool 类)
- [Task 5] depends on [Task 1] (异步化后 on_frame_ended 中的重复调用逻辑需先确定)
- [Task 1] depends on nothing (可独立开始)
- [Task 3] 和 [Task 4] 可并行执行
