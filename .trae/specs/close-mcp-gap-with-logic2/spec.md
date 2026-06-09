# MCP API 对标 Logic 2 补全 Spec

## Why

PXView 当前 MCP API 有 17 个工具，数量上已超过 Logic 2 的 15 个，但存在三个关键差距：(1) `start_capture` 缺少数字触发配置（Logic 2 支持完整的触发条件：上升沿/下降沿/脉冲宽度/链式通道），(2) 缺少堆叠解码器支持（Logic 2 的 HLA，sigrok 的 stacked decoder），(3) `add_analyzer` 的 `analyzerLabel` 参数在 schema 中声明但实现中未使用。这些差距导致 AI Agent 无法完成触发采集和多层协议解码这两个核心场景。

## What Changes

- 扩展 `start_capture.captureConfiguration` 增加 `digitalCaptureMode`（数字触发采集模式），支持触发通道/触发类型/触发后时长/脉冲宽度/链式通道条件
- 扩展 `add_analyzer` 增加 `stackOnAnalyzerId` 参数，支持在已有解码器上堆叠新解码器
- 修复 `add_analyzer` 的 `analyzerLabel` 参数实现（当前 schema 声明但未生效）
- 扩展 `start_capture.logicDeviceConfiguration` 增加 `channelMode` 参数（通道分组模式）

## Impact

- Affected specs: `implement-full-mcp-protocol`（在其基础上增强）
- Affected code: `pv/api/rpc_dispatcher.cpp`（工具 schema + 处理器）, `pv/api/session_service.h/.cpp`（configure_and_start 参数扩展 + add_decoder 堆叠支持）, `pv/api/isession_service.h`（接口扩展）

---

## ADDED Requirements

### Requirement: start_capture 数字触发配置

系统 SHALL 在 `start_capture` 的 `captureConfiguration` 中支持 `digitalCaptureMode`，对标 Logic 2 的触发采集。

#### Scenario: 上升沿触发采集
- **WHEN** AI Agent 调用 `start_capture` 并提供 `captureConfiguration.digitalCaptureMode` 含 `triggerChannelIndex=0`, `triggerType="rising"`
- **THEN** 系统配置触发条件为通道0上升沿触发，启动采集，等待触发后采集指定时长数据

#### Scenario: 脉冲宽度触发
- **WHEN** AI Agent 调用 `start_capture` 并提供 `digitalCaptureMode` 含 `triggerType="pulse_high"`, `minPulseWidthSeconds=0.000001`, `maxPulseWidthSeconds=0.0001`
- **THEN** 系统配置脉冲宽度触发条件

#### Scenario: 链式通道条件
- **WHEN** AI Agent 调用 `start_capture` 并提供 `digitalCaptureMode` 含 `linkedChannels=[{"channelIndex":1,"state":"high"}]`
- **THEN** 系统配置附加通道状态条件，触发时通道1必须为高电平

#### Scenario: 触发后采集时长
- **WHEN** AI Agent 调用 `start_capture` 并提供 `digitalCaptureMode` 含 `afterTriggerSeconds=0.5`
- **THEN** 系统配置触发后采集0.5秒数据

### Requirement: add_analyzer 堆叠解码器

系统 SHALL 在 `add_analyzer` 中支持 `stackOnAnalyzerId` 参数，允许在已有解码器上堆叠新解码器（对标 Logic 2 的 HLA / sigrok 的 stacked decoder）。

#### Scenario: 堆叠 I2C EEPROM 解码器
- **WHEN** AI Agent 先调用 `add_analyzer` 添加 `i2c_c` 解码器（返回 `analyzerId="12345"`）
- **AND** 再调用 `add_analyzer` 添加 `eeprom24c` 解码器并指定 `stackOnAnalyzerId="12345"`
- **THEN** 系统将 `eeprom24c` 堆叠在 `i2c_c` 之上，`eeprom24c` 消费 `i2c_c` 的输出作为输入

#### Scenario: 堆叠解码器自动通道映射
- **WHEN** AI Agent 添加堆叠解码器且不提供 `settings.channelMap`
- **THEN** 系统自动将堆叠解码器的输入通道映射到父解码器的输出（无需手动指定通道）

### Requirement: add_analyzer analyzerLabel 实现

系统 SHALL 实现 `analyzerLabel` 参数，将自定义标签设置到解码器实例上。

#### Scenario: 设置解码器标签
- **WHEN** AI Agent 调用 `add_analyzer` 并提供 `analyzerLabel="My SPI Decoder"`
- **THEN** 解码器实例的显示名称被设置为 "My SPI Decoder"

### Requirement: start_capture 通道模式配置

系统 SHALL 在 `start_capture` 的 `logicDeviceConfiguration` 中支持 `channelMode` 参数。

#### Scenario: 设置通道模式
- **WHEN** AI Agent 调用 `start_capture` 并提供 `logicDeviceConfiguration.channelMode="Buffer"`
- **THEN** 系统在启动采集前设置 `SR_CONF_CHANNEL_MODE` 为 Buffer 模式

---

## MODIFIED Requirements

### Requirement: start_capture captureConfiguration 扩展

`start_capture` 的 `captureConfiguration` SHALL 支持三种采集模式：
- `manualCaptureMode` — 手动采集（已有）
- `timedCaptureMode` — 定时采集（已有）
- `digitalCaptureMode` — 数字触发采集（新增），含 `triggerChannelIndex`/`triggerType`/`afterTriggerSeconds`/`minPulseWidthSeconds`/`maxPulseWidthSeconds`/`linkedChannels`

### Requirement: add_analyzer 参数扩展

`add_analyzer` SHALL 支持新增参数：
- `stackOnAnalyzerId`（string, 可选）— 父解码器实例ID，用于堆叠解码
- `analyzerLabel`（string, 可选）— 自定义标签（已有 schema 声明，需修复实现）

---

## REMOVED Requirements

（无移除项）
