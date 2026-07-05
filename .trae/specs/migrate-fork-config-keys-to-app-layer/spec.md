# 清理 dsvdef.h Fork Stub 键 Spec

## Why

`dsvdef.h` 中定义了 **46 个 fork stub 配置键**（值 60040-60088），这些键：
- **不在上游 libsigrok.h 中**（libsigrok.h 已有 60001-60013/60020-60036 共 30 个 PXLogic 扩展键，pxlogic 驱动实现）
- **pxlogic 驱动不实现**任何 60040-60088 范围的键（驱动只处理上游键 + 60001-60036）
- 调用方盲目查询 → libsigrok `check_key()` 报 "Invalid key" → 日志噪音
- 其中 25 个是 DSO 残留死代码（DSO 硬件已弃用），1 个零调用，20 个是 Analog 模式相关但被误植到 sr_config 的应用层概念

**注意区分**：
- `SR_CONF_DISK_CACHE_ENABLE` (60011)、`SR_CONF_TEST` (60026)、`SR_CONF_OPERATION_MODE` (60027)、`SR_CONF_CHANNEL_MODE` (60029) 等 **不在本 spec 范围**——它们在 libsigrok.h 中已存在且 pxlogic 实现了。这些键只需调用方加 `is_dsl_device()` 守卫（已完成）。本 spec 只处理 dsvdef.h 中**无后端**的 60040-60088 键。

## What Changes

### A. 删除 25 个 DSO 残留键（B 类）

DSO 硬件已弃用，相关代码路径已无后端。删除 dsvdef.h 中以下键定义 + PXView 中所有调用点：

**DSO 探头/缩放**（5 个）：
- `SR_CONF_PROBE_VDIV` (60040)
- `SR_CONF_PROBE_COUPLING` (60041)
- `SR_CONF_TRIGGER_VALUE` (60048)
- `SR_CONF_MAX_DSO_SAMPLERATE` (60066)
- `SR_CONF_MAX_DSO_SAMPLELIMITS` (60067)

**DSO 时基**（2 个）：
- `SR_CONF_MAX_TIMEBASE` (60051)
- `SR_CONF_MIN_TIMEBASE` (60052)

**DSO 触发**（3 个）：
- `SR_CONF_TRIGGER_HOLDOFF` (60053)
- `SR_CONF_TRIGGER_MARGIN` (60054)
- `SR_CONF_TRIGGER_CHANNEL` (60055)

**DSO 校准模式**（3 个）：
- `SR_CONF_CALI` (60057)
- `SR_CONF_ZERO` (60063)
- `SR_CONF_HAVE_ZERO` (60064)

**DSO 零点校准操作**（5 个）：
- `SR_CONF_ZERO_SET` (60068)
- `SR_CONF_ZERO_LOAD` (60069)
- `SR_CONF_ZERO_COMB` (60070)
- `SR_CONF_ZERO_COMB_FGAIN` (60071)
- `SR_CONF_ZERO_DEFAULT` (60072)

**DSO 探头校准参数**（7 个）：
- `SR_CONF_PROBE_COMB_COMP` (60073)
- `SR_CONF_PROBE_COMB_COMP_EN` (60074)
- `SR_CONF_PROBE_VGAIN` (60075)
- `SR_CONF_PROBE_VGAIN_DEFAULT` (60076)
- `SR_CONF_PROBE_VGAIN_RANGE` (60077)
- `SR_CONF_PROBE_PREOFF` (60078)
- `SR_CONF_PROBE_PREOFF_MARGIN` (60079)

**BREAKING**: 删除这些键的 dsvdef.h 定义 + 所有调用点（包括 calibration.cpp / waitingdialog.cpp / dso_hardware_config.cpp / dsotriggerdock.cpp / fftoptions.cpp / session_service.cpp DSO 触发路径）。这些代码路径在 DSO 模式弃用后本就是死代码，删除是清理而非破坏功能。

### B. 删除 1 个零调用键（D 类）

- `SR_CONF_USB` (60088) — PXView 代码零调用，纯死代码

### C. 迁移 20 个 Analog/通用应用层键（A 类）

这些键代表仍然有效的 Analog 模式概念，但 pxlogic 不通过 sr_config 实现。迁移到 DeviceAgent 方法或上游等效机制：

**Analog 通道缩放**（迁移到 DeviceAgent 方法，内部用 sr_channel / 上游 API）：
- `SR_CONF_PROBE_OFFSET` (60042) → `DeviceAgent::get_probe_offset(ch)`
- `SR_CONF_PROBE_HW_OFFSET` (60043) → `DeviceAgent::get_probe_hw_offset(ch)`
- `SR_CONF_REF_MIN` (60045) → `DeviceAgent::get_ref_min()`
- `SR_CONF_REF_MAX` (60046) → `DeviceAgent::get_ref_max()`
- `SR_CONF_UNIT_BITS` (60047) → `DeviceAgent::get_unit_bits()` （已存在）
- `SR_CONF_PROBE_MAP_UNIT` (60059) → `DeviceAgent::get_probe_map_unit(ch)`
- `SR_CONF_PROBE_MAP_MIN` (60060) → `DeviceAgent::get_probe_map_min(ch)`
- `SR_CONF_PROBE_MAP_MAX` (60061) → `DeviceAgent::get_probe_map_max(ch)`
- `SR_CONF_PROBE_MAP_DEFAULT` (60044) → `DeviceAgent::get_probe_map_default(ch)`

**通道使能**（迁移到上游 `sr_channel->enabled` 字段，`signalmodel.cpp:322` 已直接设置）：
- `SR_CONF_PROBE_EN` (60049) → 删除 `signalmodel.cpp:338` 的冗余 `set_config_bool` 调用

**通用设备能力**（迁移到 DeviceAgent 方法）：
- `SR_CONF_WAIT_UPLOAD` (60050) → `DeviceAgent::wait_upload()` 或 pxlogic 实现
- `SR_CONF_TOTAL_CH_NUM` (60062) → 改用 pxlogic 已实现的 `SR_CONF_VLD_CH_NUM` (60023)
- `SR_CONF_ACTUAL_SAMPLES` (60056) → `DeviceAgent::get_actual_samples()`
- `SR_CONF_FILE_VERSION` (60058) → `DeviceAgent::get_file_version()`
- `SR_CONF_PROBE_CONFIGS` (60080) → `DeviceAgent::get_probe_configs()`
- `SR_CONF_STATUS` (60081) → `DeviceAgent::get_status()`
- `SR_CONF_CLOCK_TYPE` (60082) → 改用 pxlogic 已实现的 `SR_CONF_CLOCK_EDGE`
- `SR_CONF_BANDWIDTH_LIMIT` (60083) → `DeviceAgent::get_bandwidth_limit(ch)`
- `SR_CONF_BANDWIDTH` (60084) → `DeviceAgent::get_bandwidth(ch)`
- `SR_CONF_RLE_SUPPORT` (60065) → `DeviceAgent::is_rle_supported()`

**BREAKING**: 删除这些键的 dsvdef.h 定义。DeviceAgent 新增方法内部对支持该键的设备仍可调 `sr_config_get`（如果未来 pxlogic 实现），对不支持的设备返回默认值。

### D. demo 采样率列表离散化（独立问题）

- demo 驱动保持上游纯净（不改 demo 源码）
- `SamplingBar::update_sample_rate_selector` 检测 `std_gvar_samplerates_steps` 返回的 `{min, max, step}` 格式时，在应用层生成 1-2-5 序列的离散列表
- combo box 显示离散列表

### E. limit_samples 应用层 fallback

- `DeviceAgent::get_sample_limit()` 在驱动返回 0 时返回应用层默认值 `SR_MHZ(1)`（1M 采样点）
- 默认值在 `AppConfig::default_sample_limit` 中可配置

## Impact

- **Affected specs**: `fix-view-work-mode-query`（已完成的相关守卫）、`decouple-core-from-view-v2`
- **Affected code**:
  - `PXView/pv/dsvdef.h` — 删除 26 个键定义
  - `PXView/pv/deviceagent.cpp/.h` — 新增 ~15 个 Analog/通用方法
  - `PXView/pv/data/signalmodel.cpp` — 删除 `commit_settings` 中 DSO 键提交路径（338-351）
  - `PXView/pv/view/analogsignal.cpp` — 改用 DeviceAgent 方法替代 `get_config_*(SR_CONF_PROBE_OFFSET, ...)` 等
  - `PXView/pv/dock/triggerdock.cpp` — `SR_CONF_TOTAL_CH_NUM` 改为 `SR_CONF_VLD_CH_NUM`
  - `PXView/pv/dialogs/deviceoptions.cpp` — prop binding 中 `SR_CONF_CLOCK_TYPE` 改为 `SR_CONF_CLOCK_EDGE`
  - `PXView/pv/dock/calibration.cpp` — 删除（DSO 校准专用，整体死代码）
  - `PXView/pv/dialogs/waitingdialog.cpp` — 删除（DSO 零点校准专用）
  - `PXView/pv/dock/dso_hardware_config.cpp` — 删除（DSO 硬件配置专用）
  - `PXView/pv/dock/dsotriggerdock.cpp` — 删除（DSO 触发 dock，DSO 模式弃用）
  - `PXView/pv/api/session_service.cpp` — 删除 DSO 触发 get/set 路径
  - `PXView/pv/toolbars/samplingbar.cpp` — 删除 DSO 时基查询 + 采样率列表离散化
  - `PXView/pv/dialogs/fftoptions.cpp` — 删除 `SR_CONF_MAX_DSO_SAMPLELIMITS` 查询
  - `PXView/pv/data/view_data_sync.cpp` — `SR_CONF_ACTUAL_SAMPLES` 改用 DeviceAgent
  - `PXView/pv/data/storesession.cpp` — 删除 DSO 校准/状态导出路径
- **libsigrok 不改**：上游驱动保持纯净
- **PXLogic 驱动不受影响**：pxlogic 不实现任何 60040-60088 键，删除定义对它无影响

## ADDED Requirements

### Requirement: DeviceAgent Analog 通道缩放接口
系统 SHALL 在 DeviceAgent 中提供 Analog 通道缩放相关方法，不通过 sr_config 查询 dsvdef.h fork stub 键。

#### Scenario: Analog 信号读取探头偏移
- **WHEN** AnalogSignal 绘制需要探头偏移
- **THEN** 调用 `device->get_probe_offset(ch)` 而非 `device->get_config_int16(SR_CONF_PROBE_OFFSET, ...)`

#### Scenario: demo 设备读取 ADC 参考
- **WHEN** AnalogSignal 缩放需要 ADC 参考值
- **THEN** 调用 `device->get_ref_min()` / `device->get_ref_max()`，demo 设备返回默认值（不报错）

### Requirement: demo 采样率离散列表生成
系统 SHALL 在 `SamplingBar::update_sample_rate_selector` 中检测 `{min, max, step}` 格式的采样率返回值，并在应用层生成 1-2-5 序列的离散列表。

#### Scenario: demo 设备采样率列表填充
- **WHEN** demo 驱动返回 `{1Hz, 1GHz, 1Hz}` 的 step 格式
- **THEN** PXView 在应用层生成 `[1Hz, 2Hz, 5Hz, 10Hz, ..., 1GHz]` 的离散列表（约 30 个条目），combo box 显示可选采样率

### Requirement: limit_samples 应用层 fallback
系统 SHALL 在 `DeviceAgent::get_sample_limit()` 中，当驱动返回 0 时使用 `AppConfig::default_sample_limit`（默认 `SR_MHZ(1)`）作为 fallback。

#### Scenario: demo 设备初次启动
- **WHEN** demo 驱动 `devc->limit_samples == 0`（上游约定"不限制"）
- **THEN** `DeviceAgent::get_sample_limit()` 返回 `SR_MHZ(1)`，PXView 采集 1M 采样点

### Requirement: 通道使能单一真相源
系统 SHALL 通过上游 `sr_channel->enabled` 字段管理通道使能，不通过 `SR_CONF_PROBE_EN` fork stub 键。

#### Scenario: 用户启用通道
- **WHEN** 用户在通道 dock 中勾选通道
- **THEN** `SignalModel::commit_settings` 仅设置 `sr_channel->enabled = enabled`，不再调用 `set_config_bool(SR_CONF_PROBE_EN, ...)`

## REMOVED Requirements

### Requirement: DSO 模式相关代码路径
**Reason**: DSO 硬件已弃用（DSCope 硬件移除），DSO 模式代码路径成为死代码。dsvdef.h 中 25 个 DSO 相关 fork stub 键无任何驱动后端实现。
**Migration**: 删除以下文件 + dsvdef.h 中的键定义：
- `calibration.cpp` / `waitingdialog.cpp` / `dso_hardware_config.cpp` / `dsotriggerdock.cpp`（整体死代码）
- `session_service.cpp` 中 DSO 触发 get/set 路径
- `signalmodel.cpp:338-351` DSO 键提交路径
- `samplingbar.cpp` 中 DSO 时基查询路径

### Requirement: SR_CONF_USB fork stub 键
**Reason**: PXView 代码零调用，纯死代码。
**Migration**: 直接删除 dsvdef.h 定义。
