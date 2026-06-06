# 修复 DSO/Analog 信号配置丢失 hw_offset/offset Spec

## Why
多标签页架构引入 `SessionDocument` 时，`ChannelConfig` 结构体遗漏了 `hw_offset`、`offset`、`zero_offset` 等硬件偏移字段。当停止采集后 `rebuild_signals_from_config()` 用 `memset(0)` 创建假 probe 时，这些字段全部为 0，导致 DSO/Analog 波形在停止和再次采集时发生垂直偏移。

## What Changes
- 在 `ChannelConfig` 中增加 `hw_offset`、`offset`、`zero_offset` 字段
- 在 `save_signal_config()` 中保存这些字段
- 在 `apply_signal_config()` 中恢复这些字段到设备真实通道
- 在 `signal_config_to_json()` / `signal_config_from_json()` 中序列化/反序列化这些字段
- 在 `rebuild_signals_from_config()` 创建假 probe 时设置这些字段

## Impact
- Affected specs: add-multi-tab-sessions
- Affected code:
  - `PXView/pv/data/sessiondocument.h`（ChannelConfig 结构体增加字段）
  - `PXView/pv/data/sessiondocument.cpp`（save/apply/json 函数）
  - `PXView/pv/view/view.cpp`（rebuild_signals_from_config 创建假 probe 时赋值）

## ADDED Requirements

### Requirement: ChannelConfig 保存 DSO/Analog 硬件偏移字段
`ChannelConfig` 结构体 SHALL 包含 `hw_offset`、`offset`、`zero_offset` 字段，用于在多标签页切换和采集停止时保留硬件垂直偏移信息。

#### Scenario: 保存信号配置时包含偏移字段
- **WHEN** 调用 `SessionDocument::save_signal_config()` 且工作模式为 DSO 或 ANALOG
- **THEN** 每个通道的 `hw_offset`、`offset`、`zero_offset` 从设备 probe 中读取并保存到 `ChannelConfig`

#### Scenario: 应用信号配置时恢复偏移字段
- **WHEN** 调用 `SessionDocument::apply_signal_config()` 且工作模式为 DSO 或 ANALOG
- **THEN** 每个通道的 `hw_offset`、`offset`、`zero_offset` 从 `ChannelConfig` 恢复到设备真实通道

#### Scenario: JSON 序列化包含偏移字段
- **WHEN** 调用 `signal_config_to_json()`
- **THEN** 每个通道的 JSON 对象包含 `hw_offset`、`offset`、`zero_offset` 字段

#### Scenario: JSON 反序列化恢复偏移字段
- **WHEN** 调用 `signal_config_from_json()`
- **THEN** 每个通道的 `ChannelConfig` 从 JSON 中恢复 `hw_offset`、`offset`、`zero_offset`

### Requirement: 假 probe 包含正确的硬件偏移值
`rebuild_signals_from_config()` 创建的假 probe SHALL 包含从 `ChannelConfig` 恢复的 `hw_offset`、`offset`、`zero_offset` 值，而非 memset(0) 产生的默认值 0。

#### Scenario: 停止采集后假 probe 偏移正确
- **WHEN** DSO/Analog 采集停止，`rebuild_signals_from_config()` 创建假 probe
- **THEN** 假 probe 的 `hw_offset`、`offset`、`zero_offset` 与停止前的真实 probe 值一致

#### Scenario: 第二次采集启动时波形无偏移
- **WHEN** DSO/Analog 采集停止后再次启动采集
- **THEN** 波形垂直位置与停止前一致，不发生位移

## MODIFIED Requirements

### Requirement: ChannelConfig 结构体
`ChannelConfig` 新增三个字段：
```cpp
uint16_t hw_offset;    // 硬件零电平偏移
uint16_t offset;       // 通道偏移
uint16_t zero_offset;  // 零点偏移
```
构造函数中初始化为 0。

## REMOVED Requirements
无
