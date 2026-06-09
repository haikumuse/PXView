# Tasks

## Phase 1: start_capture 数字触发配置（P0 — 核心差距）

- [x] Task 1: 扩展 start_capture schema 增加 digitalCaptureMode
  - [x] 1.1: 在 `rpc_dispatcher.cpp` 的 `get_tool_schemas()` 中为 `captureConfiguration` 添加 `digitalCaptureMode` 子对象
  - [x] 1.2: `digitalCaptureMode` 包含字段：`triggerChannelIndex`(integer), `triggerType`(string: "rising"/"falling"/"pulse_high"/"pulse_low"), `afterTriggerSeconds`(number), `minPulseWidthSeconds`(number), `maxPulseWidthSeconds`(number), `linkedChannels`(array of {channelIndex, state})

- [x] Task 2: 实现 configure_and_start 触发配置
  - [x] 2.1: 在 `session_service.cpp` 的 `configure_and_start()` 中解析 `digitalCaptureMode` 参数
  - [x] 2.2: 通过 `ds_trigger_probe_set()` 设置触发通道和触发类型（R/F/1/0/C）
  - [x] 2.3: 通过 `ds_trigger_set_en(1)` 启用触发
  - [x] 2.4: 通过 `ds_trigger_set_mode(SIMPLE_TRIGGER)` 设置触发模式
  - [x] 2.5: 设置触发后采集时长（`ds_trigger_set_pos()`）和脉冲宽度条件（`ds_trigger_stage_set_count()`）
  - [x] 2.6: 设置链式通道条件（linkedChannels）
  - [x] 2.7: 在 `on_start_capture` 处理器中传递 digitalCaptureMode 参数到 configure_and_start

- [ ] Task 3: 验证触发采集
  - [ ] 3.1: 编写测试脚本，使用 digitalCaptureMode 启动触发采集
  - [ ] 3.2: 验证触发条件生效（对比手动触发采集结果）

## Phase 2: add_analyzer 堆叠解码器（P1 — 多层协议解码）

- [x] Task 4: 扩展 add_analyzer schema 增加 stackOnAnalyzerId
  - [x] 4.1: 在 `rpc_dispatcher.cpp` 的 `get_tool_schemas()` 中为 `add_analyzer` 添加 `stackOnAnalyzerId` 参数（string, 可选）

- [x] Task 5: 实现堆叠解码器逻辑
  - [x] 5.1: 在 `session_service.cpp` 的 `add_decoder()` 中解析 `stackOnAnalyzerId` 参数
  - [x] 5.2: 根据 `stackOnAnalyzerId` 找到父 DecodeTrace，获取其 DecoderStack
  - [x] 5.3: 通过 `decoder_stack->add_sub_decoder()` 将子解码器添加到父 DecoderStack
  - [x] 5.4: 堆叠解码器自动继承父解码器的通道映射（无需手动指定 channelMap）

- [ ] Task 6: 验证堆叠解码器
  - [ ] 6.1: 编写测试脚本：先添加 i2c_c，再堆叠 eeprom24c
  - [ ] 6.2: 验证堆叠解码器产生注解结果

## Phase 3: analyzerLabel 修复（P1 — 小修复）

- [x] Task 7: 修复 analyzerLabel 实现
  - [x] 7.1: 在 `rpc_dispatcher.cpp` 的 `on_add_analyzer` 中解析 `analyzerLabel` 参数
  - [x] 7.2: 将 `analyzerLabel` 传递到 `session->add_decoder()` 的 label 参数
  - [x] 7.3: 验证解码器实例显示自定义标签

## Phase 4: start_capture 通道模式配置（P2 — 便利功能）

- [x] Task 8: 扩展 start_capture schema 增加 channelMode
  - [x] 8.1: 在 `logicDeviceConfiguration` 中添加 `channelMode` 参数（string, 可选，如 "Buffer"/"Stream"）
  - [x] 8.2: 在 `configure_and_start()` 中通过 `DeviceAgent::set_config_string(SR_CONF_CHANNEL_MODE)` 设置通道模式

# Task Dependencies

- Task 1 → Task 2（schema 先定义，再实现）
- Task 2 → Task 3（实现后验证）
- Task 4 → Task 5（schema 先定义，再实现）
- Task 5 → Task 6（实现后验证）
- Task 7 无依赖（独立修复）
- Task 8 无依赖（独立功能）

# Parallelizable Work

- Task 1-3（触发配置）和 Task 4-6（堆叠解码器）和 Task 7（label修复）和 Task 8（通道模式）可并行
