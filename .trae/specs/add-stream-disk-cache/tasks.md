# Tasks

- [x] Task 1: 新增磁盘缓存基础设施类
  - [x] SubTask 1.1: 创建 `DiskBufferManager` 类（disk_buffer_manager.h/cpp），实现缓存文件的创建、打开、关闭、删除，以及每通道文件的顺序写入和随机读取接口
  - [x] SubTask 1.2: 创建 `DiskWriteThread` 类（disk_write_thread.h/cpp），实现后台异步写入线程，包含写入队列、启动/停止、刷写等待、队列深度监控
  - [x] SubTask 1.3: 创建 `DiskReadCache` 类（disk_read_cache.h/cpp），实现 LRU 读缓存，包含叶块加载、缓存命中查找、淘汰策略、容量管理
  - [x] SubTask 1.4: 创建 `DiskCacheConfig` 结构体，封装磁盘缓存配置（启用标志、缓存路径、总缓存深度、内存部分大小、磁盘部分大小）

- [x] Task 2: 改造 LogicSnapshot 支持分层存储
  - [x] SubTask 2.1: 在 `logicsnapshot.h` 中定义叶块状态枚举 `BlockState { HOT, WARM, COLD }`，新增 `std::map<void*, BlockState> _block_states` 成员跟踪每个叶块的状态
  - [x] SubTask 2.2: 在 `logicsnapshot.h` 中新增 `DiskBufferManager*`、`DiskWriteThread*`、`DiskReadCache*` 指针成员，以及热数据窗口大小 `_hot_window_blocks` 成员
  - [x] SubTask 2.3: 修改 `append_cross_payload()` 中叶块分配逻辑：分配新叶块时设置状态为 HOT；当叶块数超过热数据窗口时，将最旧叶块标记为 WARM 并加入异步写入队列
  - [x] SubTask 2.4: 修改 `calc_mipmap()` 完成后的逻辑：如果叶块状态为 WARM 且不在当前查看窗口，将其加入异步写入队列
  - [x] SubTask 2.5: 修改 `get_samples()` / `get_sample_self()` / `get_display_edges()` 等读取方法：当叶块为 COLD 状态时，通过 DiskReadCache 从磁盘加载
  - [x] SubTask 2.6: 修改 `move_first_node_to_last()` 和 `free_head_blocks()`：对 COLD 状态的叶块跳过内存释放（内存已不在），改为标记磁盘区域可覆盖
  - [x] SubTask 2.7: 修改 `free_data()`：停止写入线程、刷写待写入数据、关闭磁盘文件、清理读缓存
  - [x] SubTask 2.8: 修改 `first_payload()`：根据磁盘缓存配置初始化 DiskBufferManager、DiskWriteThread、DiskReadCache

- [x] Task 3: 新增 libsigrok 配置项
  - [x] SubTask 3.1: 在 `libsigrok.h` 中新增 `SR_CONF_DISK_CACHE_PATH` 和 `SR_CONF_DISK_CACHE_ENABLE` 配置项常量
  - [x] SubTask 3.2: 在 `hwdriver.c` 中注册新配置项的名称和类型
  - [x] SubTask 3.3: 在 `pxlogic.h` 的 `PX_context` 中新增 `disk_cache_enable` 和 `disk_cache_path` 成员
  - [x] SubTask 3.4: 在 `pxlogic.c` 的 `config_get()` / `config_set()` 中处理新配置项
  - [x] SubTask 3.5: 修改 `SR_CONF_STREAM_BUFF` 的语义：当磁盘缓存启用时，该值表示总缓存深度（内存+磁盘）

- [x] Task 4: 修改 SigSession 集成磁盘缓存
  - [x] SubTask 4.1: 在 `SigSession::start_capture()` 中，根据设备配置初始化磁盘缓存参数并传递给 LogicSnapshot
  - [x] SubTask 4.2: 在 `SigSession::stop_capture()` 中，确保磁盘写入线程完成刷写后再停止
  - [x] SubTask 4.3: 在 `SigSession::feed_in_logic()` 中，检查磁盘写入队列积压情况，必要时触发警告回调

- [x] Task 5: UI 支持磁盘缓存配置
  - [x] SubTask 5.1: 在 `deviceoptions.cpp` 中添加 `SR_CONF_DISK_CACHE_ENABLE` 的布尔绑定（开关控件）
  - [x] SubTask 5.2: 在 `deviceoptions.cpp` 中添加 `SR_CONF_DISK_CACHE_PATH` 的路径选择控件
  - [x] SubTask 5.3: 修改 `SR_CONF_STREAM_BUFF` 的范围提示，当磁盘缓存启用时显示更大的范围（1-1024GB）
  - [x] SubTask 5.4: 在状态栏添加磁盘缓存状态指示器（写入速度、队列深度、磁盘使用量）

- [x] Task 6: 磁盘缓存索引文件格式
  - [x] SubTask 6.1: 定义索引文件格式（二进制），包含：魔数、版本号、通道数、每通道叶块数量、每个叶块的磁盘偏移量和状态
  - [x] SubTask 6.2: 实现 `DiskBufferManager` 的索引文件写入和读取
  - [x] SubTask 6.3: 实现环形覆盖时索引文件的更新

- [x] Task 7: 性能监控与降级策略
  - [x] SubTask 7.1: 在 `DiskWriteThread` 中实现写入速度统计，每秒计算平均写入带宽
  - [x] SubTask 7.2: 采集启动时执行磁盘写入速度测试（写入 64MB 数据测量速度），低于 200MB/s 时警告用户
  - [x] SubTask 7.3: 实现写入队列积压监控：超过 64 个叶块显示警告，超过 256 个叶块暂停采集
  - [x] SubTask 7.4: 实现磁盘空间监控：可用空间低于 10% 时停止写入并通知 UI

- [x] Task 8: 数据保存兼容性
  - [x] SubTask 8.1: 修改 `StoreSession::save_logic()`，当磁盘缓存启用时，从磁盘读取 COLD 状态叶块数据后写入 .pxl 文件
  - [x] SubTask 8.2: 修改 `LogicSnapshot::copy_from()` 和 `clone_data()`，处理 COLD 状态叶块的拷贝（需从磁盘加载）
  - [x] SubTask 8.3: 确保采集结束后缓存文件正确清理，不留残余临时文件

# Task Dependencies
- [Task 2] depends on [Task 1] — LogicSnapshot 改造依赖磁盘缓存基础设施类
- [Task 4] depends on [Task 2] and [Task 3] — SigSession 集成依赖 LogicSnapshot 改造和配置项
- [Task 5] depends on [Task 3] — UI 绑定依赖配置项定义
- [Task 6] depends on [Task 1] — 索引文件格式依赖 DiskBufferManager
- [Task 7] depends on [Task 1] and [Task 2] — 性能监控依赖写入线程和 LogicSnapshot
- [Task 8] depends on [Task 2] — 数据保存兼容性依赖 LogicSnapshot 改造
- [Task 1] has no dependencies — 可独立开始
- [Task 3] has no dependencies — 可独立开始
- [Task 1] and [Task 3] can be parallelized
