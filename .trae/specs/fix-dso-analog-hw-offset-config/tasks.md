# Tasks

- [x] Task 1: 在 ChannelConfig 中增加 hw_offset/offset/zero_offset 字段
  - [x] SubTask 1.1: 修改 `sessiondocument.h` 的 `ChannelConfig` 结构体，添加 `uint16_t hw_offset`、`uint16_t offset`、`uint16_t zero_offset` 字段及构造函数初始化

- [x] Task 2: 在 save_signal_config 中保存偏移字段
  - [x] SubTask 2.1: 修改 `sessiondocument.cpp` 的 `save_signal_config()`，在 DSO/ANALOG 模式下从 probe 读取 `hw_offset`、`offset`、`zero_offset` 并存入 ChannelConfig

- [x] Task 3: 在 apply_signal_config 中恢复偏移字段
  - [x] SubTask 3.1: 修改 `sessiondocument.cpp` 的 `apply_signal_config()`，在 DSO/ANALOG 模式下将 ChannelConfig 中的 `hw_offset`、`offset`、`zero_offset` 写回设备真实通道

- [x] Task 4: 在 JSON 序列化/反序列化中支持偏移字段
  - [x] SubTask 4.1: 修改 `signal_config_to_json()`，在每个通道 JSON 对象中添加 `hw_offset`、`offset`、`zero_offset` 字段
  - [x] SubTask 4.2: 修改 `signal_config_from_json()`，从 JSON 中恢复 `hw_offset`、`offset`、`zero_offset`

- [x] Task 5: 在 rebuild_signals_from_config 中为假 probe 赋值偏移字段
  - [x] SubTask 5.1: 修改 `view.cpp` 的 `rebuild_signals_from_config()`，创建假 probe 后从 ChannelConfig 设置 `hw_offset`、`offset`、`zero_offset`

- [x] Task 6: 编译验证
  - [x] SubTask 6.1: 执行 `build_incremental.cmd`，确认编译通过无错误

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 1]
- [Task 4] depends on [Task 1]
- [Task 5] depends on [Task 1]
- [Task 6] depends on [Task 1, Task 2, Task 3, Task 4, Task 5]

# Parallelizable Work
- Task 2, Task 3, Task 4, Task 5 可在 Task 1 完成后并行执行
