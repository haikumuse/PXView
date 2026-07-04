# 弃用 PXView fork libsigrok + Core 层全量上游化 Spec

## Why

PXView fork libsigrok（2014 年从 upstream 0.2.0 fork）已深度偏离上游，导致 Core 层维护双库共存架构（fork STATIC + libsigrokstd SHARED + bridge 胶水层 + DeviceAgent 11+ 双分支）。DSL 4 驱动（DSLogic/DSCope/dsl/command）依赖 fork 私有扩展（sr_status 50 字段、sr_channel 25 扩展字段、Adv/Serial trigger、DSO 校准、LA_CROSS_DATA 格式），无法上游化。

本 spec 弃用 DSL 硬件支持，删除 fork libsigrok + bridge 层，Core 层全量迁移到上游 `sr_*` API，只保留 PXLogic（迁移到 libsigrokstd）+ 上游 80+ 驱动。彻底消除双库共存复杂度，Core 层只面对一套 API。

## What Changes

### A. 删除 PXView fork libsigrok（约 50 文件）

- 删除 `libsigrok/` 整个目录（hardware/DSL/、hardware/pxlogic/、hardware/common/、include/、src/、output/、input/、bindings/、contrib/）
- 删除 `CMakeLists.txt` 的 `include(${CMAKE_SOURCE_DIR}/cmake/libsigrok.cmake)`
- 删除 `libsigrok_SOURCES` 变量
- 删除 `cmake/libsigrok.cmake` 文件
- `pxview-core` 链接改为只链接 `libsigrokstd`

### B. 删除 libsigrokstd/bridge/ 双库桥接层（约 6 文件）

- 删除 `libsigrokstd/bridge/` 整个目录：
  - `srstd_bridge.h` / `srstd_bridge.c` — 影子结构 + 字段转换
  - `srstd_pxview_glue.h` / `srstd_pxview_glue.c` — void* 胶水层
  - `srstd_init_shared.c` — 共享 libusb_context
  - `srstd_compat.c` — compat shim
- 删除 `libsigrokstd/CMakeLists.txt` 的 bridge 源文件引用
- 删除 `SRSTD_MAKE_HANDLE`/`SRSTD_IS_HANDLE`/`LIB_SRSTD`/`LIB_PXVIEW` 相关宏和 enum

### C. PXLogic 迁移到 libsigrokstd（基于 pxlogic fork port + 上游 API 适配）

> Task 1 关键结论：scilogic 0.5.2 不能直接采用（缺失固件加载/触发位置读取/设备表/PWM/16 级触发/ch_num 寄存器/启动脉冲序列），但 USB 寄存器层（wr_reg/rd_reg/wr_data_update/rd_data_update）与 pxlogic fork 逐字节一致。方案改为以 **pxlogic fork 为基础** port 到 libsigrokstd，适配上游 API。

- 拷贝 `libsigrok/hardware/pxlogic/` 5 文件（api.c/protocol.c/protocol.h/usb_ctrl.c/usb_ctrl.h）到 `libsigrokstd/src/hardware/pxlogic/` 作为 port 起点
- **USB 寄存器层零修改**（Task 1 已验证 wr_reg/rd_reg/wr_data_update/rd_data_update 与 scilogic 0.5.2 逐字节一致）
- **驱动上层 API 适配**（fork API → 上游 API）：
  - `ds_data_forward(LA_CROSS_DATA, ...)` → 驱动内 deinterleave + `sr_session_send(sdi, packet)`
  - `ds_trigger` 全局对象（trig_mask0/1/trig_value0/1/...）→ `struct sr_trigger` + `sr_trigger_match`（保留 16 级 stage 数组到 dev_context，soft_trigger_logic 适配或保留 fork 触发逻辑）
  - `ds_log_init`/`ds_log_free` 等 fork 日志 API → `sr_dbg`/`sr_warn`/`sr_err`（上游日志）
  - `#include "../../libsigrok-internal.h"` → `#include "../libsigrok/libsigrok.h"` + `#include "libsigrok-internal.h"`（libsigrokstd 内部头）
  - `SR_CONF_*` key 全部保留（30 个 key 由 Task 2 在 libsigrokstd.h 中扩展）
  - 设备表 `supported_PX[]` + `logic_check_conf_profile` 保留（fork 独有，scilogic 缺失）
  - 固件加载 `firmware_config` + `hw_usb_open` 保留（fork 独有，scilogic 缺失）
  - PWM0 寄存器写入 + ctl_data 命令路径保留（fork 独有，scilogic 缺失）
  - 启动脉冲三次序列保留（fork 独有，scilogic 缺失）
- libsigrokstd.h 扩展 30 个 SR_CONF_* key（13 fork 60001-60013 保持原值 + 17 fork 30000-range 重新分配到 60020+）
- `hwdriver.c` 的 `sr_key_info_config[]` 新增 30 行映射
- 固件资源路径配置（`SCI_LOGIC.bin` / `hspi_ddr.bin` / `hspi_ddr_RST.bin`）
- **保留 LA_CROSS_DATA 内部表示**：驱动内部仍用 channel-block 处理 USB 数据，但在 `sr_session_send` 前做 deinterleave 转换为 sample-interleaved 输出，Core 层不再见 LA_CROSS_DATA

### D. Core 层 ds_* → sr_* 全量替换（12 文件）

| 旧 API（ds_*） | 新 API（sr_*） | 影响文件 |
|---|---|---|
| `ds_lib_init/exit` | `sr_init/exit(ctx)` | sigsession.cpp |
| `ds_get_device_list` | `sr_dev_list(driver)` 遍历 | sigsession.cpp |
| `ds_active_device` | `sr_dev_open(sdi)` | sigsession.cpp |
| `ds_get_actived_device_info` | `sr_dev_inst_vendor_get` 等 accessor | deviceagent.cpp |
| `ds_get_actived_device_config` | `sdi->driver->config_get` | deviceagent.cpp |
| `ds_set_actived_device_config` | `sdi->driver->config_set` | deviceagent.cpp |
| `ds_get_actived_device_config_list` | `sdi->driver->config_list` | deviceagent.cpp |
| `ds_start_collect` | `sr_session_start` | capturemanager.cpp |
| `ds_stop_collect` | `sr_session_stop` | capturemanager.cpp |
| `ds_is_collecting` | `sr_session->is_running`（需状态跟踪） | deviceagent.cpp |
| `ds_set_datafeed_callback_ex` | `sr_session_datafeed_callback_add` | sigsession.cpp |
| `ds_set_event_callback_ex` | `sr_session_datafeed_callback_add` + 私有事件 | sigsession.cpp |
| `ds_get_libusb_context` | `ctx->libusb_ctx` | sigsession.cpp |
| `ds_set_firmware_resource_dir` | `sr_resource_set_path` | sigsession.cpp |
| `ds_enable_device_channel` | `sr_dev_channel_enable` | deviceagent.cpp |
| `ds_get_actived_device_status` | **删除**（DSL 弃用，PXLogic 不需要 sr_status） | deviceagent.cpp |
| `ds_trigger_*` | `sr_trigger_*` subset（PXLogic 只用 simple trigger） | sigsession.cpp trigger 同步 |
| `ds_dsl_option_value_to_code` | **删除**（DSL 专属） | deviceagent.cpp |

**关键改动**：
- "全局活跃设备"模型 → session-based 模型：SigSession 持有 `struct sr_session*`，DeviceAgent 持有 `struct sr_dev_inst*`
- `DataFeedParser` 适配上游 `sr_datafeed_packet`（去掉 status/bExportOriginalData 字段）
- 事件总线：`DS_EV_*` 事件码改为基于 `sr_session_datafeed_callback` 的状态推断

### E. DeviceAgent 简化（4 文件）

- 删除 `DeviceLib` enum（`LIB_PXVIEW`/`LIB_SRSTD`）
- 删除 `set_device_lib`/`device_lib` 方法
- 删除 `srstd_glue_*` 调用（bridge 已删）
- 删除 11+ 个 `if (LIB_SRSTD)` 双分支
- 所有方法直接调 `sdi->driver->config_get/set` 或 `sr_session_*`
- `DeviceAgent` 退化为 thin wrapper：持有 `struct sr_dev_inst*`，方法纯转发到 `sdi->driver->*` 或 `sr_session_*`

### F. 删除 LA_CROSS_DATA 路径（4 文件）

- 删除 `LogicSnapshot::append_cross_payload`（logicsnapshot.cpp/logicsnapshot.h）
- 删除 `LA_CROSS_DATA`/`LA_SPLIT_DATA` enum 引用
- 删除 `logicsnapshot_diskcache_writer.cpp` 中的 cross data 处理路径
- LogicSnapshot 只保留 sample-interleaved 路径（`append_payload`，标准 `sr_datafeed_logic`）
- PXLogic（pxlogic 驱动 port 后内 deinterleave）+ 上游驱动都输出 sample-interleaved，统一走 `append_payload`

### G. 删除 fork 扩展功能（16 文件）

#### G1. sr_status 50 字段（DSO 测量 dock）
- 删除 `deviceagent.cpp`/`deviceagent.h` 的 `get_device_status`/`sr_status` 相关方法
- 删除 `dso_measure.cpp`/`dso_measure.h` 的 sr_status 字段读取（频率/周期/Vrms 等测量值）
- DSO 测量 dock 适配为"无硬件测量"模式（纯软件计算）或整体移除
- 删除 `capturemanager.cpp` 的 sr_status 轮询逻辑
- 删除 `datafeedparser.cpp`/`datafeedparser.h` 的 status 字段处理

#### G2. sr_channel 25 扩展字段（DSO 校准）
- 删除 `deviceagent.cpp` 的 `get_probe_vdiv`/`get_probe_offset`/`get_probe_vgain`/`get_probe_preoff` 等 fork 扩展 accessor
- 删除 `datasource.cpp`/`datasource.h` 的 sr_channel 扩展字段访问
- DSO 校准链路整体删除（`dsl_probe_cali_fgain`/`dsl_config_adc` 等）

#### G3. Adv trigger + Serial trigger（UI 保留，调用删除）
- **保留** `triggerdock.cpp`/`triggerdock.h` 的 Adv trigger UI（count/inv/logic 16 级 stage）——后续 PXLogic 可扩展支持
- **保留** `triggerdock.cpp` 的 Serial trigger UI——后续 PXLogic 可扩展支持
- **保留** `TriggerConfig` Core 结构（含 Adv/Serial trigger 字段）——单一真相源不破坏
- 删除 `sigsession.cpp` 的 `ds_trigger_*` 调用（fork API 随 libsigrok 删除）
- `SigSession::sync_trigger_to_libsigrok()` 只同步 simple trigger 部分（通过 `sr_trigger_*` 或驱动 config_set）
- Adv/Serial trigger 字段保留在 TriggerConfig 中，但 sync 时暂不传递给驱动（stub）——待 PXLogic 驱动未来扩展
- pxlogic 驱动 port 后保留 16 级触发 stage 到 dev_context，当前 sync 只下发 simple trigger（5 种 match：ZERO/ONE/RISING/FALLING/EDGE），Adv/Serial stage 数组保留供未来扩展

#### G4. DSO 模式
- DSCope 硬件弃用，DSO 模式不再支持
- `DsoSignal`/`DsoDock`/`DsoMeasure` 适配为"无硬件"状态或整体移除
- `View` 的 DSO 模式切换路径删除

### H. View 层适配

- `TriggerDock`：**保留** Adv/Serial trigger UI（后续 PXLogic 可扩展），只删除对 `ds_trigger_*` 的调用
- `DsoDock`：整体移除或适配为 stub（无 DSCope 硬件）
- `ChannelDock`：适配上游 channel 模型（无 sr_channel 扩展字段）
- `Viewport`：删除 sr_status 相关显示（测量值 dock）
- `SamplingBar`：适配上游采样率/模式切换

### I. MCP API 适配

- 删除 DSL 专属 MCP 工具中直接调用 `ds_trigger_*` 的实现代码（如 `set_adv_trigger`/`set_serial_trigger` 工具内部实现），但**保留** TriggerConfig 写入路径（用户可通过 MCP 配置 Adv/Serial trigger 字段，sync 时 stub 不下发驱动）
- 适配 `sr_*` 返回值
- `SessionService` 的 `sr_channel_type_to_api()` 保留（上游 SR_CHANNEL_* 值与 fork 一致）

## Impact

- **删除约 100+ 文件**：`libsigrok/` 整个目录 + `libsigrokstd/bridge/` 整个目录
- **Core 层 ds_* 调用约 50+ 处替换为 sr_***：12 文件
- **DeviceAgent 11+ 双分支消除**：4 文件
- **View 层 fork 扩展引用清除**：16 文件
- **DSL 硬件（DSLogic/DSCope）不再支持**
- **DSO 模式整体移除**（无 DSCope 硬件）
- **Adv/Serial trigger 调用路径移除**（UI 和 TriggerConfig Core 结构保留供 PXLogic 未来扩展）

### 影响的 spec
- `dual-libsigrok-coexist-restore-features` — 双库共存架构**完全废弃**
- `migrate-all-sigrok-drivers` — 不再需要"迁移"，fork 直接删除
- `modernize-core-layer-radical` — Core 层简化为单库单 API
- `modernize-view-layer-v3` — View 层移除 DSO UI，**保留** Adv/Serial trigger UI 供 PXLogic 未来扩展

### 影响的代码
- `libsigrok/` — 整个目录删除
- `libsigrokstd/bridge/` — 整个目录删除
- `libsigrokstd/include/libsigrok/libsigrok.h` — 新增 30 个 SR_CONF_* key
- `libsigrokstd/src/hwdriver.c` — `sr_key_info_config[]` 新增 30 行
- `libsigrokstd/src/hardware/pxlogic/` — 新增 5 个驱动源文件（port from fork）
- `PXView/pv/sigsession.cpp` / `sigsession.h` — ds_* → sr_* 替换
- `PXView/pv/deviceagent.cpp` / `deviceagent.h` — 删除双分支 + fork 扩展 accessor
- `PXView/pv/core/capturemanager.cpp` — ds_start/stop → sr_session_*
- `PXView/pv/core/datafeedparser.cpp` / `.h` — 适配上游 packet + 删 status
- `PXView/pv/core/sessionstatecontext.cpp` / `.h` — ds_* 清理
- `PXView/pv/data/logicsnapshot.cpp` / `.h` — 删 append_cross_payload
- `PXView/pv/data/logicsnapshot_diskcache_writer.cpp` — 删 cross data 路径
- `PXView/pv/data/datasource.cpp` / `.h` — 删 sr_channel 扩展访问
- `PXView/pv/data/signalmodel.cpp` — ds_* 清理
- `PXView/pv/dock/triggerdock.cpp` / `.h` — 删 Adv/Serial trigger UI
- `PXView/pv/view/dso_measure.cpp` / `.h` — 删 sr_status 测量
- `PXView/pv/view/logicsignal.cpp` — ds_trigger_* 清理
- `PXView/pv/view/view_cursors.cpp` — ds_* 清理
- `PXView/pv/storesession.cpp` — sr_output API 验证（100% 兼容，零改动）
- `PXView/pv/api/session_service.cpp` — ds_* → sr_* 替换
- `CMakeLists.txt` — 删除 libsigrok.cmake include + libsigrok_SOURCES

## ADDED Requirements

### Requirement: libsigrokstd 0.6.0 扩展 30 个 SR_CONF_* key

系统 SHALL 在 `libsigrokstd/include/libsigrok/libsigrok.h` 的 `enum sr_configkey` 中新增 30 个 key，覆盖 PXLogic 驱动所需的全部配置场景。fork 60001-60013 保持原值（无冲突），fork 30000-range key 重新分配到 60020+（避免与上游 30000-range key 冲突）。

#### Scenario: pxlogic 驱动 config_get/set 全覆盖
- **WHEN** PXView 上层调用 `DeviceAgent::get_config_*(key, ...)` 或 `set_config_*(key, ...)` 访问 PXLogic 设备
- **THEN** pxlogic 驱动的 `config_get`/`config_set` 函数 SHALL 返回 `SR_OK` 并正确读写 devc 字段
- **AND** 30 个 key 全部有对应的 case 分支，无 `SR_ERR_NA` 默认返回
- **AND** fork 原有的所有 SR_CONF_* case 分支全部保留（port 不删功能）

#### Scenario: enum 值在 libsigrokstd 范围内唯一
- **WHEN** 编译 libsigrokstd 时
- **THEN** 13 个 fork 60001-60013 key 的 enum 值 SHALL 与 PXView fork 完全一致（无冲突）
- **AND** 17 个 fork 30000-range key SHALL 重新分配到 60020+ 范围（避免与上游 SR_CONF_PATTERN_MODE=30002/SR_CONF_RLE=30003 等冲突）
- **AND** `sr_key_info_config[]` 数组 SHALL 包含全部 30 个 key 的字符串映射
- **AND** 编译零 "duplicate enum value" 错误

### Requirement: PXLogic 驱动 port 到 libsigrokstd

系统 SHALL 在 `libsigrokstd/src/hardware/pxlogic/` 目录提供完整的 PXLogic 驱动实现，基于 PXView fork pxlogic 源码 port，适配上游 libsigrok 0.6.0 API，保留 fork 的全部硬件功能（固件加载/PWM/16 级触发/ctl_data/启动脉冲序列/设备表）。

#### Scenario: 驱动注册成功
- **WHEN** libsigrokstd.dll 加载时
- **THEN** `SR_REGISTER_DEV_DRIVER(pxlogic_driver_info)` SHALL 通过 section 机制自动注册
- **AND** `sr_driver_list(ctx)` SHALL 包含 "PX Logic" 驱动

#### Scenario: USB 寄存器层零修改
- **WHEN** port pxlogic fork 的 `usb_ctrl.c` 到 libsigrokstd
- **THEN** `wr_reg`/`rd_reg`/`wr_data_update`/`rd_data_update` 函数 SHALL 与 fork 逐字节一致（Task 1 已验证）
- **AND** 固件加载 `firmware_config` + `hw_usb_open` 路径保留
- **AND** PWM0 寄存器写入 + ctl_data 命令路径保留
- **AND** 启动脉冲三次序列保留

#### Scenario: 数据采集输出 sample-interleaved 格式
- **WHEN** PXLogic 硬件通过 USB 发送 channel-block（LA_CROSS_DATA）数据
- **THEN** pxlogic 驱动 SHALL 在 `sr_session_send` 前做 deinterleave 转换为 sample-interleaved
- **AND** 输出的 `sr_datafeed_logic.unitsize` SHALL 等于 `channel_count / 8`
- **AND** 驱动内部仍可用 channel-block 处理，但 Core 层不再见 LA_CROSS_DATA
- **AND** PXView `LogicSnapshot::append_payload` SHALL 直接处理

### Requirement: Core 层只支持上游 sr_* API

系统 SHALL 删除所有 `ds_*` API 调用，Core 层只通过 `sr_*` API 访问设备。

#### Scenario: DeviceAgent 无双分支
- **WHEN** PXView 打开任意设备（PXLogic 或上游驱动）
- **THEN** `DeviceAgent` SHALL 直接调用 `sdi->driver->config_get/set` 或 `sr_session_*`
- **AND** 不存在 `if (device_lib == LIB_SRSTD)` 或 `if (device_lib == LIB_PXVIEW)` 分支
- **AND** `DeviceLib` enum 已删除

#### Scenario: SigSession 使用 session-based 模型
- **WHEN** PXView 启动采集
- **THEN** `SigSession` SHALL 调用 `sr_session_new()` + `sr_session_datafeed_callback_add()` + `sr_session_start()`
- **AND** 不调用 `ds_start_collect` 或 `ds_set_datafeed_callback_ex`

### Requirement: 删除 LA_CROSS_DATA 路径

系统 SHALL 删除 `LogicSnapshot::append_cross_payload` 和所有 `LA_CROSS_DATA` 引用，只保留 sample-interleaved 路径。

#### Scenario: LogicSnapshot 只处理 sample-interleaved
- **WHEN** datafeed packet 到达 LogicSnapshot
- **THEN** `append_payload` SHALL 处理标准 `sr_datafeed_logic`（length/unitsize/data）
- **AND** 不存在 `append_cross_payload` 方法
- **AND** 不存在 `LA_CROSS_DATA`/`LA_SPLIT_DATA` enum 引用

### Requirement: 删除 fork 扩展功能

系统 SHALL 删除所有 PXView fork libsigrok 的私有扩展功能，包括 sr_status 50 字段、sr_channel 25 扩展字段、Adv/Serial trigger、DSO 校准链路。

#### Scenario: 无 sr_status 调用
- **WHEN** 编译 PXView 时
- **THEN** 不存在 `sr_status` struct 引用
- **AND** 不存在 `ds_get_actived_device_status` 调用
- **AND** DSO 测量 dock（dso_measure.cpp）已删除或改为纯软件计算

#### Scenario: Adv/Serial trigger UI 保留但 sync 暂不传递
- **WHEN** 用户在 TriggerDock 配置 Adv/Serial trigger
- **THEN** UI SHALL 正常显示和编辑 Adv/Serial trigger 字段（count/inv/logic/serial 参数）
- **AND** `TriggerConfig` Core 结构 SHALL 保留 Adv/Serial trigger 字段
- **AND** `SigSession::sync_trigger_to_libsigrok()` SHALL 只同步 simple trigger 部分
- **AND** 不存在 `ds_trigger_set_mask`/`ds_trigger_set_count` 等 fork API 调用
- **AND** pxlogic 驱动 port 后保留 16 级 stage 数组到 dev_context，sync 时通过 `sr_trigger_match` 适配或保留 fork 触发逻辑（待未来扩展 Adv/Serial 硬件触发）

#### Scenario: 无 DSO 模式
- **WHEN** PXView 启动
- **THEN** 不存在 DSCope 设备识别
- **AND** DsoDock 已移除或 stub
- **AND** View 不提供 DSO 模式切换

## REMOVED Requirements

### Requirement: 双库共存架构

**Reason**: DSL 硬件弃用，不再需要 fork libsigrok + bridge 桥接层
**Migration**:
- 删除 `libsigrok/` 整个目录
- 删除 `libsigrokstd/bridge/` 整个目录
- 删除 `DeviceAgent::DeviceLib` enum + 双分支
- Core 层只链接 libsigrokstd

### Requirement: DSL 4 驱动支持

**Reason**: DSL 4 驱动深度依赖 fork 扩展，无法上游化；用户决定弃用 DSL 硬件
**Migration**:
- DSLogic/DSCope/dsl/command 4 驱动随 fork libsigrok 一起删除
- PXView 不再识别 DSLogic/DSCope 硬件
- 用户需改用 PXLogic 或上游支持的 LA 硬件

### Requirement: DSO 模式

**Reason**: DSCope 硬件弃用，DSO 模式依赖 sr_status 50 字段 + sr_channel 25 扩展字段 + DSO 校准链路
**Migration**:
- DsoSignal/DsoDock/DsoMeasure 整体移除或 stub
- View 不提供 DSO 模式切换

### Requirement: Adv trigger + Serial trigger 调用路径

**Reason**: fork libsigrok 删除后 `ds_trigger_*` API 不复存在，但 UI 和 Core 结构保留供 PXLogic 未来扩展
**Migration**:
- TriggerDock UI 保留（Adv/Serial trigger 控件不删）
- `TriggerConfig` Core 结构保留（Adv/Serial 字段不删）
- `SigSession::sync_trigger_to_libsigrok()` 只同步 simple trigger，Adv/Serial 部分 stub
- 删除所有 `ds_trigger_*` 调用

## 风险与缓解

### 风险 1：DSL 硬件用户流失（高影响）

弃用 DSL 4 驱动后，DSLogic/DSCope 用户无法继续使用 PXView。

**缓解**：
1. 在 README/AGENTS.md 明确标注"DSL 硬件不支持，请使用原版 DSView"
2. 保留最后一个支持 DSL 的 PXView 版本 tag，供用户回退
3. 提供迁移指南：DSLogic 用户可改用上游 fx2lafw 驱动（基础功能）

### 风险 2：fork API → 上游 API 适配（中风险）

pxlogic fork 深度依赖 fork 全局状态（`ds_trigger` 全局对象、`ds_data_forward` LA_CROSS_DATA 通道、`ds_log_*` 日志 API），port 到 libsigrokstd 需要替换为 session-based 上游 API。

**缓解**（Task 1 已完成 USB 协议层验证）：
1. USB 寄存器层（wr_reg/rd_reg/wr_data_update/rd_data_update）已验证与 scilogic 0.5.2 逐字节一致，零修改 port
2. fork 全局 `ds_trigger` → dev_context 内 16 级 stage 数组 + `sr_trigger_match` 适配
3. fork `ds_data_forward(LA_CROSS_DATA)` → 驱动内 deinterleave + `sr_session_send`（sample-interleaved 输出）
4. fork `ds_log_*` → 上游 `sr_dbg`/`sr_warn`/`sr_err`
5. 实测验证（Task 12 硬件回归）

### 风险 3：Core 层 ds_* → sr_* 语义转换（中风险）

"全局活跃设备"到 session-based 的语义转换不是 1:1 映射。PXView 大量代码假设"当前设备"是全局的。

**缓解**：
1. SigSession 持有 `struct sr_session*`，DeviceAgent 持有 `struct sr_dev_inst*`
2. 主动约束为单 session（不支持多 session 并发）
3. 状态管理保持单设备模型

### 风险 4：DSO 测量 dock 退化（低风险）

删除 sr_status 后，DSO 测量 dock（频率/周期/Vrms）无法从硬件获取实时值。

**缓解**：
1. DSCope 硬件弃用，DSO 测量 dock 整体移除（无硬件可用）
2. PXLogic 是纯 LA，不需要 DSO 测量

### 风险 5：View 层 UI 残留（低风险）

删除 DSO/Adv/Serial trigger UI 后，可能有残留引用导致编译错误。

**缓解**：
1. 编译驱动修复，逐一清除残留
2. grep 验证无 ds_trigger_*/sr_status/LA_CROSS_DATA 引用

## 范围与非目标

### 范围内
- 删除 `libsigrok/` 整个目录（fork）
- 删除 `libsigrokstd/bridge/` 整个目录（双库桥接层）
- PXLogic 迁移到 libsigrokstd（基于 pxlogic fork port + 上游 API 适配 + 17 key 扩展）
- Core 层 `ds_*` → `sr_*` 全量替换（12 文件）
- DeviceAgent 简化（删除双分支 + LIB_PXVIEW/LIB_SRSTD enum）
- 删除 LA_CROSS_DATA 路径
- 删除 sr_status 50 字段使用
- 删除 sr_channel 25 扩展字段使用
- 删除 Adv/Serial trigger 的 `ds_trigger_*` 调用路径（UI 和 TriggerConfig Core 结构保留）
- 删除 DSO 模式（DsoSignal/DsoDock/DsoMeasure 移除或 stub）
- View 层适配（TriggerDock/ChannelDock/Viewport）
- 编译 + 功能 + 回归验证

### 非目标
- **不**实现 Backend 策略模式（单库不需要抽象）
- **不**实现 sr_status / dev_status_get 回调（DSL 弃用，PXLogic 不需要）
- **不**修改数据导出层（sr_output API 100% 兼容，零改动）
- **不**支持多 session 并发（保持单设备模型）
- **不**保留 DSL 4 驱动的任何代码（彻底删除）
- **不**保留 DSO 模式的任何 UI（彻底删除）
