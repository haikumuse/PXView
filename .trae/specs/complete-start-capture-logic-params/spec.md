# start_capture Logic 模式参数补全 Spec

## Why

PXView 的 MCP `start_capture` 当前只覆盖了 LOGIC 模式下约 30% 的设备参数。GUI 中可配置的 RLE、Stream 缓冲区、磁盘缓存、捕获比例、阈值预设、重复采集间隔等参数在 MCP 中均不可用。此外 `manualCaptureMode.sampleCount` 在 schema 中声明但未实现，`glitchFilters` 缺少 items 定义。这些缺失导致 AI Agent 无法完成 Stream 模式长采集、RLE 压缩采集、精确触发位置控制等常见场景。

## What Changes

- 扩展 `start_capture.logicDeviceConfiguration` 增加参数：`rleEnabled`、`streamBufferSizeGB`、`streamMemBufferSizeGB`、`diskCacheEnabled`、`diskCachePath`、`thresholdPreset`、`operationMode`、`bufferOptions`、`digitalFilter`
- 扩展 `start_capture.captureConfiguration` 增加参数：`captureRatio`、`repeatIntervalSeconds`
- 实现 `manualCaptureMode.sampleCount`（当前 schema 声明但代码忽略）
- 补全 `glitchFilters` 的 items 定义
- 在 `configure_and_start()` 中实现所有新增参数的映射

## Impact

- Affected specs: `close-mcp-gap-with-logic2`（在其基础上继续增强）
- Affected code: `pv/api/rpc_dispatcher.cpp`（schema 扩展 + on_start_capture 参数解析）, `pv/api/session_service.h/.cpp`（configure_and_start 签名扩展 + 参数实现）, `pv/api/isession_service.h`（接口签名扩展）

---

## ADDED Requirements

### Requirement: start_capture RLE 使能

系统 SHALL 在 `logicDeviceConfiguration` 中支持 `rleEnabled`（boolean），映射到 `SR_CONF_RLE`。

#### Scenario: 启用 RLE 压缩采集
- **WHEN** AI Agent 调用 `start_capture` 并提供 `logicDeviceConfiguration.rleEnabled=true`
- **THEN** 系统在启动采集前通过 `DeviceAgent::set_config_bool(SR_CONF_RLE, true)` 启用 RLE

### Requirement: start_capture Stream 缓冲区大小

系统 SHALL 在 `logicDeviceConfiguration` 中支持 `streamBufferSizeGB`（number, 1-1024）和 `streamMemBufferSizeGB`（number, 1-64），分别映射到 `SR_CONF_STREAM_BUFF` 和 `SR_CONF_STREAM_MEM_BUFF`。

#### Scenario: 设置磁盘 Stream 缓冲区
- **WHEN** AI Agent 调用 `start_capture` 并提供 `logicDeviceConfiguration.diskCacheEnabled=true`, `streamBufferSizeGB=10`
- **THEN** 系统设置 `SR_CONF_STREAM_BUFF` 为 10.0 (GB)

#### Scenario: 设置内存 Stream 缓冲区
- **WHEN** AI Agent 调用 `start_capture` 并提供 `logicDeviceConfiguration.diskCacheEnabled=false`, `streamMemBufferSizeGB=4`
- **THEN** 系统设置 `SR_CONF_STREAM_MEM_BUFF` 为 4.0 (GB)

### Requirement: start_capture 磁盘缓存

系统 SHALL 在 `logicDeviceConfiguration` 中支持 `diskCacheEnabled`（boolean）和 `diskCachePath`（string），映射到 `SR_CONF_DISK_CACHE_ENABLE` 和 `SR_CONF_DISK_CACHE_PATH`。

#### Scenario: 启用磁盘缓存
- **WHEN** AI Agent 调用 `start_capture` 并提供 `logicDeviceConfiguration.diskCacheEnabled=true`
- **THEN** 系统设置 `SR_CONF_DISK_CACHE_ENABLE` 为 true

### Requirement: start_capture 阈值预设

系统 SHALL 在 `logicDeviceConfiguration` 中支持 `thresholdPreset`（string），映射到 `SR_CONF_THRESHOLD`。与 `digitalThresholdVolts`（映射到 `SR_CONF_VTH`）不同，`thresholdPreset` 提供命名预设（如 "1.8V"/"3.3V"/"5V"/"Adjustable"），部分设备只支持预设不支持自定义电压。

#### Scenario: 使用阈值预设
- **WHEN** AI Agent 调用 `start_capture` 并提供 `logicDeviceConfiguration.thresholdPreset="3.3V"`
- **THEN** 系统通过 `DeviceAgent::set_config_string(SR_CONF_THRESHOLD, "3.3V")` 设置阈值预设

### Requirement: start_capture 操作模式

系统 SHALL 在 `logicDeviceConfiguration` 中支持 `operationMode`（string），映射到 `SR_CONF_OPERATION_MODE`。控制设备级操作模式（如 "Buffer"/"Stream"/"Internal test"）。

#### Scenario: 设置操作模式
- **WHEN** AI Agent 调用 `start_capture` 并提供 `logicDeviceConfiguration.operationMode="Stream"`
- **THEN** 系统通过 `DeviceAgent::set_config_int16(SR_CONF_OPERATION_MODE, ...)` 设置操作模式

### Requirement: start_capture 缓冲区选项

系统 SHALL 在 `logicDeviceConfiguration` 中支持 `bufferOptions`（string），映射到 `SR_CONF_BUFFER_OPTIONS`。

#### Scenario: 设置缓冲区选项
- **WHEN** AI Agent 调用 `start_capture` 并提供 `logicDeviceConfiguration.bufferOptions="..."`
- **THEN** 系统通过 `DeviceAgent::set_config_string(SR_CONF_BUFFER_OPTIONS, ...)` 设置缓冲区选项

### Requirement: start_capture 数字滤波器

系统 SHALL 在 `logicDeviceConfiguration` 中支持 `digitalFilter`（string），映射到 `SR_CONF_FILTER`。

#### Scenario: 启用数字滤波器
- **WHEN** AI Agent 调用 `start_capture` 并提供 `logicDeviceConfiguration.digitalFilter="..."`
- **THEN** 系统通过 `DeviceAgent::set_config_string(SR_CONF_FILTER, ...)` 设置滤波器

### Requirement: start_capture 捕获比例

系统 SHALL 在 `captureConfiguration` 中支持 `captureRatio`（integer, 0-100），映射到 `SR_CONF_CAPTURE_RATIO`。这是 GUI 中设置触发位置的标准方式（百分比），比 `afterTriggerSeconds` 更直接。

#### Scenario: 设置触发位置比例
- **WHEN** AI Agent 调用 `start_capture` 并提供 `captureConfiguration.captureRatio=50`
- **THEN** 系统设置 `SR_CONF_CAPTURE_RATIO` 为 50，触发点在采集数据的 50% 位置

### Requirement: start_capture 重复采集间隔

系统 SHALL 在 `captureConfiguration` 中支持 `repeatIntervalSeconds`（number），映射到 `SigSession::set_repeat_intvl()`。当前 MCP 硬编码为 0.1 秒。

#### Scenario: 设置重复采集间隔
- **WHEN** AI Agent 调用 `start_capture` 并提供 `captureMode="repeat"`, `captureConfiguration.repeatIntervalSeconds=0.5`
- **THEN** 系统设置重复采集间隔为 0.5 秒

### Requirement: manualCaptureMode.sampleCount 实现

系统 SHALL 实现 `manualCaptureMode.sampleCount` 参数，当前 schema 声明但代码忽略。映射到 `SR_CONF_LIMIT_SAMPLES`。

#### Scenario: 按样本数采集
- **WHEN** AI Agent 调用 `start_capture` 并提供 `captureConfiguration.manualCaptureMode.sampleCount=1000000`
- **THEN** 系统设置 `SR_CONF_LIMIT_SAMPLES` 为 1000000，采集 100 万个样本

### Requirement: glitchFilters schema 补全

系统 SHALL 补全 `glitchFilters` 的 items 定义，使 MCP 客户端知道期望的结构。

#### Scenario: 设置毛刺滤波器
- **WHEN** AI Agent 调用 `start_capture` 并提供 `logicDeviceConfiguration.glitchFilters=[{"channelIndex":0,"threshold":5}]`
- **THEN** 系统为通道 0 设置宽度阈值为 5 的毛刺滤波器

---

## MODIFIED Requirements

### Requirement: start_capture logicDeviceConfiguration 扩展

`logicDeviceConfiguration` SHALL 支持新增参数：
- `rleEnabled`（boolean, 可选）— RLE 压缩使能
- `streamBufferSizeGB`（number, 可选, 1-1024）— 磁盘 Stream 缓冲区大小
- `streamMemBufferSizeGB`（number, 可选, 1-64）— 内存 Stream 缓冲区大小
- `diskCacheEnabled`（boolean, 可选）— 磁盘缓存使能
- `diskCachePath`（string, 可选）— 磁盘缓存路径
- `thresholdPreset`（string, 可选）— 阈值预设名称
- `operationMode`（string, 可选）— 操作模式
- `bufferOptions`（string, 可选）— 缓冲区选项
- `digitalFilter`（string, 可选）— 数字滤波器
- `glitchFilters` items 定义补全为 `{channelIndex: integer, threshold: number}`

### Requirement: start_capture captureConfiguration 扩展

`captureConfiguration` SHALL 支持新增参数：
- `captureRatio`（integer, 可选, 0-100）— 触发位置百分比
- `repeatIntervalSeconds`（number, 可选）— 重复采集间隔

### Requirement: configure_and_start 签名扩展

`configure_and_start()` SHALL 接受所有新增参数并正确映射到对应的 `SR_CONF_*` 或 `SigSession` API。

---

## REMOVED Requirements

（无移除项）
