# Tasks

## Phase 1: Schema 扩展

- [x] Task 1: 扩展 start_capture schema 增加所有缺失参数
  - [x] 1.1: 在 `logicDeviceConfiguration` 中添加 `rleEnabled`(boolean)、`streamBufferSizeGB`(number)、`streamMemBufferSizeGB`(number)、`diskCacheEnabled`(boolean)、`diskCachePath`(string)、`thresholdPreset`(string)、`operationMode`(string)、`bufferOptions`(string)、`digitalFilter`(string)
  - [x] 1.2: 补全 `glitchFilters` 的 items 定义为 `{channelIndex: integer, threshold: number}`
  - [x] 1.3: 在 `captureConfiguration` 中添加 `captureRatio`(integer, 0-100)、`repeatIntervalSeconds`(number)

## Phase 2: 接口和实现扩展

- [x] Task 2: 扩展 ISessionService 和 SessionService 接口
  - [x] 2.1: 在 `isession_service.h` 的 `configure_and_start()` 签名中添加新参数：`bool rle_enabled`、`double stream_buffer_size_gb`、`double stream_mem_buffer_size_gb`、`bool disk_cache_enabled`、`const std::string& disk_cache_path`、`const std::string& threshold_preset`、`const std::string& operation_mode`、`const std::string& buffer_options`、`const std::string& digital_filter`、`int capture_ratio`、`double repeat_interval_seconds`、`uint64_t sample_count`
  - [x] 2.2: 在 `session_service.h` 中同步更新签名
  - [x] 2.3: 在 `session_service.cpp` 的 `configure_and_start()` 实现中添加参数映射：
    - `rle_enabled` → `DeviceAgent::set_config_bool(SR_CONF_RLE, ...)`
    - `stream_buffer_size_gb` → `DeviceAgent::set_config_double(SR_CONF_STREAM_BUFF, ...)`（仅当 `disk_cache_enabled=true`）
    - `stream_mem_buffer_size_gb` → `DeviceAgent::set_config_double(SR_CONF_STREAM_MEM_BUFF, ...)`（仅当 `disk_cache_enabled=false`）
    - `disk_cache_enabled` → `DeviceAgent::set_config_bool(SR_CONF_DISK_CACHE_ENABLE, ...)`
    - `disk_cache_path` → `DeviceAgent::set_config_string(SR_CONF_DISK_CACHE_PATH, ...)`
    - `threshold_preset` → `DeviceAgent::set_config_string(SR_CONF_THRESHOLD, ...)`
    - `operation_mode` → `DeviceAgent::set_config_int16(SR_CONF_OPERATION_MODE, ...)`（需将字符串映射为 int16）
    - `buffer_options` → `DeviceAgent::set_config_string(SR_CONF_BUFFER_OPTIONS, ...)`
    - `digital_filter` → `DeviceAgent::set_config_string(SR_CONF_FILTER, ...)`
    - `capture_ratio` → `DeviceAgent::set_config_uint64(SR_CONF_CAPTURE_RATIO, ...)`
    - `repeat_interval_seconds` → `SigSession::set_repeat_intvl(...)`
    - `sample_count` → `DeviceAgent::set_config_uint64(SR_CONF_LIMIT_SAMPLES, ...)`（当 `duration_seconds==0` 且 `sample_count>0` 时使用）

## Phase 3: RPC dispatcher 参数解析

- [x] Task 3: 更新 on_start_capture 解析所有新参数
  - [x] 3.1: 在 `rpc_dispatcher.cpp` 的 `on_start_capture` 中解析 `logicDeviceConfiguration` 的所有新增字段
  - [x] 3.2: 解析 `captureConfiguration.captureRatio` 和 `captureConfiguration.repeatIntervalSeconds`
  - [x] 3.3: 解析 `manualCaptureMode.sampleCount` 并传递给 `configure_and_start`
  - [x] 3.4: 将所有新参数传递给 `configure_and_start()` 调用

## Phase 4: 构建验证

- [x] Task 4: 编译验证
  - [x] 4.1: 运行 `build_incremental.cmd` 确保无编译错误
  - [x] 4.2: 检查无新增编译警告

# Task Dependencies

- Task 1 → Task 2（schema 先定义，再实现接口）
- Task 2 → Task 3（接口先定义，再更新调用方）
- Task 3 → Task 4（代码完成后再编译）

# Parallelizable Work

- Task 1 的各子步骤可并行（schema 各字段独立）
- Task 2 的 2.3 中各参数映射逻辑独立，但需在同一个函数中实现
