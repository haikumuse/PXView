# 修复测试框架decoder_test.c的stack通道映射 Spec

## Why

测试框架decoder_test.c在创建stack中的上游解码器（如uart_c）时，未调用`srd_inst_channel_set_all()`设置通道映射，导致上游解码器的所有通道（包括未连接的可选通道）被默认channelmap激活，输出大量无意义注解。这是16个UART堆叠解码器测试FAIL的根因。

## What Changes

- **decoder_test.c**: 修改stack解码器创建逻辑，当stack条目没有`channels`字段时，自动将输入数据的前N个通道映射到stack解码器的前N个必需通道
- **decoder_test.c**: 为stack解码器也调用`srd_inst_channel_set_all()`，确保通道映射正确
- **test_factory.py**: 修改测试数据生成逻辑，为stack中的解码器自动生成正确的`channels`字段

## Impact

- Affected code: `libsigrokdecode/tests/decoder_test.c`, `libsigrokdecode/tests/test_factory.py`
- Affected tests: 16个UART堆叠解码器（modbus_c, lin_c, midi_c, bluetooth_h4_c, j1708_c, sbus_futaba_c, amulet_ascii_c, ufcs_c, arm_etmv3_c, arm_itm_c, arm_tpiu_c, boost_c, crsf_c, pan1321_c, pn532_c, scs_c, streletz_c）
- 预期效果: 16个UART堆叠解码器从FAIL变为PASS

## ADDED Requirements

### Requirement: stack解码器自动通道映射

当config.json的stack条目没有`channels`字段时，decoder_test SHALL 自动将输入数据的前N个通道映射到stack解码器的前N个通道（按解码器定义顺序），并调用`srd_inst_channel_set_all()`。

#### Scenario: UART堆叠解码器自动映射
- **GIVEN** config.json包含 `"stack": [{"id": "uart_c"}]` 且无 `channels` 字段
- **AND** 输入数据有2个通道
- **WHEN** decoder_test创建uart_c实例
- **THEN** uart_c的通道0(RX)映射到输入通道0，通道1(TX)映射到输入通道1
- **AND** uart_c的可选通道（如果有）映射为-1（未连接）
- **AND** `c_decoder_has_channel(di, CH_RX)` 返回 TRUE
- **AND** `c_decoder_has_channel(di, CH_TX)` 返回 TRUE

#### Scenario: stack条目有显式channels字段时优先使用
- **GIVEN** config.json包含 `"stack": [{"id": "uart_c", "channels": {"rx": 0}}]`
- **WHEN** decoder_test创建uart_c实例
- **THEN** 使用显式指定的channels映射，不自动映射

### Requirement: test_factory.py为stack解码器生成channels字段

test_factory.py在生成测试数据时，SHALL 为stack中的解码器自动生成正确的`channels`字段，将输入数据通道映射到stack解码器的必需通道。

#### Scenario: 生成UART堆叠测试数据
- **WHEN** test_factory为modbus_c生成测试数据
- **THEN** 生成的config.json中stack条目包含 `"channels": {"rx": 0, "tx": 1}`

## MODIFIED Requirements

（无修改的已有需求）

## REMOVED Requirements

（无移除的需求）

## 技术细节

### decoder_test.c 当前stack处理逻辑（需修改）

当前代码（约第640-671行）：
```c
// 读取stack数组
cJSON *j_stack = cJSON_GetObjectItem(config, "stack");
if (j_stack && cJSON_IsArray(j_stack)) {
    for (int si = 0; si < cJSON_GetArraySize(j_stack); si++) {
        cJSON *stack_entry = cJSON_GetArrayItem(j_stack, si);
        char *stack_id = cJSON_GetObjectItem(stack_entry, "id")->valuestring;
        // 创建解码器实例
        // 只有当stack_entry有"channels"字段时才调用srd_inst_channel_set_all()
        cJSON *j_channels = cJSON_GetObjectItem(stack_entry, "channels");
        if (j_channels && cJSON_IsObject(j_channels)) {
            // 设置通道映射
        }
        // 否则不设置！← 这是BUG
    }
}
```

### 修复方案

当stack条目没有`channels`字段时，自动生成默认映射：
1. 获取stack解码器的通道列表（从`srd_decoder->channels`）
2. 按顺序将前N个通道映射到输入数据的通道0, 1, 2, ...
3. 调用`srd_inst_channel_set_all()`设置映射

### Python uart通道定义

Python uart有2个可选通道：RX(索引0)和TX(索引1)。C uart_c也有相同的2个可选通道。当输入数据有2个通道时，自动映射为 rx→0, tx→1。

### parallel_c问题

parallel_c的测试数据只有1个通道（CLK），没有数据通道。这不是框架问题，而是测试数据不充分。parallel_c需要至少CLK+1个数据通道才能工作。需要为parallel_c生成包含更多通道的测试数据。
