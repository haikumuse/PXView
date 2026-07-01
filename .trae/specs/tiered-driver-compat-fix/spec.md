# 分层修复 sigrok 驱动 compat 缺口 Spec

## Why

`audit-and-fix-migrated-drivers` 审计已完成 62 个驱动，发现 23 个 BROKEN、3 个 MAJOR、2 个 MINOR。根因调查确认：PXView libsigrok 核心基于 upstream 0.2.0（2014 年早期 fork），缺失 2014-2020 年间引入的 API（`sr_analog_meaning`、`sr_resource_*`、`sr_sw_limits`、`feed_queue_*`）及部分枚举值；叠加 DreamSourceLab 的故意自定义（`ds_data_forward`、正错误码、`packet.status`）。

当前修复呈"各驱动各自为战"的 ad-hoc 模式：6 个驱动各自复制 `sr_resource_*`，5 个驱动各自复制 `sr_sw_limits`，多个驱动各自定义 `read_u*_inc`/`write_u*_inc`。这导致代码膨胀、不一致、维护困难。需要一个系统化的三层修复架构，明确"哪些集中、哪些就地"，避免无序扩散。

## What Changes

### Layer 1：compat 层补全（通用，集中）

将**多驱动共用**的缺失定义集中到 `hardware/compat/`，消除重复：

- **compat_config.h** — 补全缺失常量（✅ 已完成）：
  - `SR_MQ_*` / `SR_UNIT_*` / `SR_MQFLAG_*` 缺失枚举值（✅ compat_config.h:356-421）
  - `SR_PACKET_INVALID/VALID/NEED_RX`（✅ compat_config.h:439-445）
  - `SR_CONF_SWAP`（✅ compat_config.h:154-156）
  - `SR_CONF_SIGNAL_GENERATOR` / `SR_CONF_ENABLED` / `SR_CONF_OUTPUT_FREQUENCY` / `SR_CONF_DUTY_CYCLE`（✅ kingst-la2016/protocol.h 本地定义已就位）
  - `read_u16le_inc` / `read_u8_inc` / `read_u32le_inc` / `write_u16le_inc` / `write_u32le_inc` / `write_u24le_inc` / `write_u40le_inc` / `write_u8_inc`（✅ compat_config.h:247-318，`#ifndef compat_*_defined` 守卫）
- **compat_helpers.h/.c** — 补全通用函数（✅ 已完成）：
  - `sr_hexdump_new` / `sr_hexdump_free`（✅ compat_helpers.h:707/714 + compat_helpers.c:880/908）
  - `sr_resource_open` / `sr_resource_read` / `sr_resource_close` + `struct sr_resource` + `SR_RESOURCE_FIRMWARE`（✅ compat_helpers.h:610-666 + compat_helpers.c:768/815/837）
  - `sr_log_loglevel_get` + `SR_LOG_SPEW`（✅ compat_helpers.h:593-594/680 + compat_helpers.c:866）

**明确不集中**（保持驱动本地 `static inline`）：

- `sr_sw_limits` — 5 个驱动已有本地副本（appa-55ii/asix-sigma/asix-omega-rtm-cli/arachnid-labs-re-load-pro/agilent-dmm），提取到 compat 层会与现有副本符号冲突，收益低。新驱动照搬 appa-55ii 模板。
- `feed_queue_logic` / `feed_queue_analog` — 仅 1-2 个驱动使用，且实现差异大，保持本地。

### Layer 2：驱动本地 shim（特定，按需）

每个驱动按需补充 PXView 缺失的 API shim，遵循已有成熟模板：

- **`sr_sw_limits` 本地副本**：照搬 `appa-55ii/protocol.h` 模板（`static inline` + `#ifndef` 守卫）。kingst-la2016、asix-sigma 已采用此模式。
- **`feed_queue_logic` 本地实现**：照搬 `asix-omega-rtm-cli/protocol.c` 模板。kingst-la2016 需要。
- **`feed_queue_analog` 本地实现**：照搬 `atorch/protocol.c` 模板。rdtech-tc/rdtech-um 已采用 `send_channel_value` 替代。
- **驱动特有包装器**：如 `ols_serial_timeout`、`hcs_serial_timeout`、`lps_serial_timeout` 等，保持驱动本地。
- **回调签名适配**：`receive_data(int fd, int revents, void *cb_data)` → `(int fd, int revents, const struct sr_dev_inst *sdi)`，移除 `sdi = cb_data` 解包。

### Layer 3：驱动业务逻辑修复（迁移错误）

修复迁移过程中丢失的业务逻辑，按设备类别：

- **DMM（14 个 BROKEN）**：`sr_analog_init(&analog, num, meaning->channels, unit, mq, mqflags)` → 展平为直接赋值 `analog.probes` / `analog.num_samples` / `analog.unit` / `analog.mq` / `analog.mqflags` + `packet.status = SR_PKT_OK` + `ds_data_forward`。
- **示波器（6 个 frame_begin + 2 个 MAJOR）**：
  - 删除 6 个驱动的本地 `std_session_send_df_frame_begin/end`，改用 compat 层版本
  - siglent-sds：补回 `dev_buffer_usage_printf` 等 SCPI 缓冲区管理包装器
  - hung-chang-dso-2100：补回 `config_channel_set` 通道合并逻辑
- **逻辑分析仪（5 个 BROKEN）**：
  - openbench-logic-sniffer：恢复 `convert_trigger`/`ols_metadata_quirks`/`ols_get_metadata` 10-token/通道组扩展/前后触发分割（✅ 已完成）
  - kingst-la2016：补全 `feed_queue_logic` + `sr_resource`（直接采用 compat 层版本）+ `_inc` 函数 + 回调签名（✅ 已完成，api.c/protocol.c 编译通过）
  - asix-sigma：恢复 `sigma_fw_2_bitbang` + 修复 compat 不匹配（✅ 已完成，编译通过）
  - saleae-logic-pro：补 `sr_resource_load` + `usb_source_remove`（✅ 已完成）
  - pipistrello-ols：恢复 `SR_CONF_SWAP`（✅ 已完成）
  - lecroy-logicstudio：补 `lls_setup_acquisition` 调用（✅ 已完成）

## Reference Paths

- **PXView（目标）库**：`c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hardware\`
- **原版（参考）库**：`C:\Users\admin\Downloads\libsigrok\src\hardware\`（原版 libsigrok 0.6.0，已从 `C:\Users\admin\Downloads\old\libsigrok\` 迁移至此）
- **compat 层**：`c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hardware\compat\`
- **构建目录**：`c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\build\`

> 实施时所有子代理 SHALL 使用上述新路径引用原版（参考）库，不得使用旧路径 `C:\Users\admin\Downloads\old\libsigrok\`。

## Impact

- **Affected specs**：
  - `audit-and-fix-migrated-drivers`（本 spec 细化其 Task 11 的修复策略）
  - `add-sigrok-driver-compat-layer`（本 spec 扩展其 compat 层覆盖范围）
  - `migrate-all-sigrok-drivers`（本 spec 修正其已迁移驱动的 compat 缺口）
- **Affected code**：
  - `libsigrok/hardware/compat/compat_config.h`（补 _inc 函数 + 确认 SR_CONF 常量）
  - `libsigrok/hardware/compat/compat_helpers.h` + `compat_helpers.c`（补 sr_resource_* + sr_hexdump_* + sr_log_loglevel_get）
  - 23 个 BROKEN 驱动 + 3 个 MAJOR 驱动（Layer 3 业务逻辑修复）
  - kingst-la2016 / asix-sigma（Layer 2 本地 shim）

## ADDED Requirements

### Requirement: 三层修复架构

所有 compat 缺口修复 SHALL 按三层架构组织，禁止跨层混用：

1. **Layer 1（compat 层）**：多驱动（≥3）共用的缺失定义，集中到 `hardware/compat/`
2. **Layer 2（驱动本地 shim）**：单驱动或少数驱动需要的 API shim，放驱动 `protocol.h` 的 `static inline`
3. **Layer 3（业务逻辑）**：迁移丢失的业务逻辑，按原版库原版补回

#### Scenario: 多驱动共用缺失项集中

- **WHEN** 3 个以上驱动需要同一缺失定义（如 `sr_resource_*`、`read_u*_inc`）
- **THEN** 该定义 SHALL 放入 `hardware/compat/` 对应头文件
- **AND** 驱动 SHALL 移除本地重复定义，改用 compat 层版本

#### Scenario: 单驱动特有 shim

- **WHEN** 仅 1-2 个驱动需要某 API（如 `feed_queue_logic`）
- **THEN** 该 shim SHALL 放驱动 `protocol.h` 的 `static inline`
- **AND** 使用 `#ifndef` 守卫避免与 compat 层未来添加的同名定义冲突

### Requirement: compat 层常量值安全

compat 层补全的枚举常量 SHALL 使用 PXView 保留值，不与 PXView 已有枚举冲突：

- PXView `SR_MQ_HARMONIC_RATIO = 10015`原版 = 10032）→ 新增 `SR_MQ_TIME` 用保留值 10100
- PXView `SR_MQFLAG_AVG = 0x20000`（原版 = 0x40000）→ compat 层 `SR_MQFLAG_AVG` 用 0x40000
- `SR_CONF_*` 新增项从 30159 起递增（避开 PXView 已用 30150-30158）

#### Scenario: 常量值不冲突

- **WHEN** compat 层添加新 `SR_MQ_*` / `SR_UNIT_*` / `SR_MQFLAG_*` 值
- **THEN** 该值 SHALL 不与 PXView `libsigrok.h` 已有枚举值重复
- **AND** SHALL 在 `compat_config.h` 注释中标注"canonical sigrok value = X, PXView reserved value = Y"

### Requirement: sr_resource_* 集中化

`sr_resource_open` / `sr_resource_read` / `sr_resource_close` + `struct sr_resource` + `SR_RESOURCE_FIRMWARE` SHALL 集中到 `compat_helpers.h/.c`，消除 6 个驱动的重复本地实现。

#### Scenario: 驱动改用 compat 层 sr_resource

- **WHEN** 驱动需要固件文件加载
- **THEN** SHALL `#include "hardware/compat/compat.h"` 并调用 `sr_resource_open/read/close`
- **AND** SHALL NOT 在驱动本地定义 `struct sr_resource` 或 `SR_RESOURCE_FIRMWARE`
- **AND** 6 个现有本地实现（asix-sigma/saleae-logic-pro/saleae-logic16/sysclk-lwla/lecroy-logicstudio/kingst-la2016）SHALL 迁移到 compat 层版本

### Requirement: read/write _inc 函数集中化

`read_u16le_inc` / `read_u8_inc` / `read_u32le_inc` / `write_u16le_inc` / `write_u32le_inc` / `write_u24le_inc` / `write_u40le_inc` / `write_u8_inc` SHALL 集中到 `compat_config.h`，与已有的 `read_u16le`/`write_u16le` 等非 _inc 版本同列。

#### Scenario: 驱动改用 compat 层 _inc 函数

- **WHEN** 驱动需要增量读写整数
- **THEN** SHALL 使用 `compat_config.h` 提供的 `*_inc` 函数
- **AND** SHALL NOT 在驱动本地重新定义

### Requirement: DMM sr_analog_init 展平模式

14 个 BROKEN DMM 驱动 SHALL 将 `sr_analog_init` + `meaning->channels` 嵌套模式展平为 PXView 扁平结构：

```c
/* 原版（编译失败） */
sr_analog_init(&analog, num, meaning->channels, unit, mq, mqflags);

/* NEW（PXView 扁平） */
analog.probes = probes;
analog.num_samples = num;
analog.unit = unit;
analog.mq = mq;
analog.mqflags = mqflags;
analog.data = data;
packet.type = SR_DF_ANALOG;
packet.status = SR_PKT_OK;
packet.payload = &analog;
ds_data_forward(sdi, &packet);
```

#### Scenario: DMM 驱动编译通过

- **WHEN** 启用任一 DMM 驱动（`ENABLE_DRIVER_<NAME>=ON`）
- **THEN** 该驱动 SHALL 编译无 `sr_analog_init` / `sr_analog_meaning` 未定义错误
- **AND** 运行时 SHALL 正确上送模拟量数据帧

### Requirement: sr_sw_limits 保持驱动本地

`sr_sw_limits` 结构及函数 SHALL 保持各驱动 `protocol.h` 的 `static inline` 本地副本，不提取到 compat 层。

**理由**：5 个驱动已有本地副本，提取到 compat 层会与 `static inline` 符号冲突；各副本用 `#ifndef` 守卫已避免跨驱动冲突；收益低于风险。

#### Scenario: 新驱动需要 sr_sw_limits

- **WHEN** 新迁移驱动需要 `sr_sw_limits`
- **THEN** SHALL 照搬 `appa-55ii/protocol.h` 模板（含 `init/config_get/config_set/acquisition_start/update_samples_read/check`）
- **AND** 如需 `get_remain`，SHALL 照搬 `asix-omega-rtm-cli/protocol.h` 模板
- **AND** SHALL NOT 修改 `hardware/compat/` 头文件

## MODIFIED Requirements

### Requirement: compat 层覆盖范围

`add-sigrok-driver-compat-layer` spec 定义的 compat 层覆盖范围扩展为：

- 原有：`sr_session_send`→`ds_data_forward`、错误码映射、`std_*` 函数、USB/serial/SCPI 包装
- 新增：`sr_resource_*`、`sr_hexdump_*`、`sr_log_loglevel_get`、`read/write_u*_inc`、缺失枚举常量

## REMOVED Requirements

无删除项。
