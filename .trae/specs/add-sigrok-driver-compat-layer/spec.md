# Sigrok 驱动适配层 Spec

## Why
PXView 的 libsigrok 已与标准 sigrok 深度分叉，核心数据结构（`sr_dev_driver`、`sr_dev_inst`、`sr_channel`）和回调签名完全不同，导致标准 sigrok 的 87 个硬件驱动无法直接编译使用。需要引入适配层，使标准 sigrok 驱动能在 PXView 中运行，扩展对第三方硬件（如 Saleae、Rigol、Kingst 等设备）的支持。

## What Changes
- 在 `libsigrok/hardware/compat/` 下创建适配层基础设施（兼容头文件、辅助函数、适配宏）
- 提供标准 sigrok 驱动所需的内部 API 兼容实现（`sr_config_get/set/list` 的 `uint32_t key` 版本、`std_init/cleanup/dev_list/dev_clear` 等辅助函数）
- 提供标准 sigrok 驱动所需的 SCPI/Serial 通信后端（从标准 sigrok 移植 `src/scpi/` 和串口相关代码）
- 为每个要引入的标准驱动编写薄适配层，将标准 `sr_dev_driver` 回调签名转换为 PXView 签名
- 扩展 `hwdriver.c` 的驱动注册机制，支持条件编译引入兼容驱动
- 扩展 `ds_*` 公共 API，支持非 DSL 设备的基本操作（无 `dev_mode_list` 等的降级处理）
- 扩展 `DeviceAgent` 以支持标准 sigrok 设备类型（SCPI/Serial 设备）
- 更新 CMakeLists.txt 添加兼容驱动编译选项

## Impact
- Affected specs: 无直接影响现有 spec
- Affected code:
  - `libsigrok/libsigrok-internal.h` — 增加兼容类型定义
  - `libsigrok/hwdriver.c` — 扩展驱动注册
  - `libsigrok/lib_main.c` — 扩展 `ds_*` API 降级处理
  - `PXView/pv/deviceagent.cpp` — 支持非 DSL 设备
  - `PXView/pv/sigsession.cpp` — 支持标准 sigrok 数据流
  - `CMakeLists.txt` — 添加兼容驱动编译选项

## ADDED Requirements

### Requirement: 兼容层基础设施
系统 SHALL 在 `libsigrok/hardware/compat/` 下提供适配层，使标准 sigrok 驱动源码经最小修改后可在 PXView 中编译运行。

#### Scenario: 标准 sigrok 驱动编译
- **WHEN** 开发者将标准 sigrok 驱动源码放入 `libsigrok/hardware/` 并在 CMakeLists.txt 中启用
- **THEN** 该驱动应能通过 CMake 编译，链接到兼容层，并在运行时被 PXView 识别

#### Scenario: 标准 sigrok 驱动回调签名适配
- **WHEN** 标准 sigrok 驱动的 `config_get(key, data, sdi, cg)` 被调用
- **THEN** 适配层 SHALL 将其转换为 PXView 的 `config_get(id, data, sdi, ch, cg)`，其中 `ch` 传 NULL，`key` 从 `uint32_t` 转为 `int`

### Requirement: 标准 sigrok 内部 API 兼容
系统 SHALL 提供标准 sigrok 驱动内部调用的 API 兼容实现，包括但不限于：
- `sr_config_get/set/list` 的 `uint32_t key` 版本（内部转换到 PXView 的 `int id` 版本）
- `std_init`、`std_cleanup`、`std_dev_list`、`std_dev_clear` 辅助函数
- `STD_CONFIG_LIST` 宏
- `sr_channel_new`（标准签名版本）
- `sr_dev_inst_new`/`sr_dev_inst_free`
- `sr_usb_dev_inst_new`/`sr_serial_dev_inst_new`

#### Scenario: 标准 sigrok 驱动内部 API 调用
- **WHEN** 标准 sigrok 驱动内部调用 `sr_config_set(sdi, cg, SR_CONF_SAMPLERATE, data)`
- **THEN** 兼容层 SHALL 将其转换为 PXView 的 `sr_config_set(sdi, NULL, cg, (int)SR_CONF_SAMPLERATE, data)`

### Requirement: SCPI/Serial 通信后端
系统 SHALL 提供标准 sigrok 的 SCPI 和 Serial 通信后端，使 SCPI/Serial 设备驱动能正常通信。

#### Scenario: SCPI 设备通信
- **WHEN** 标准 sigrok 的 SCPI 驱动（如 rigol-ds）通过 `sr_scpi_send()` 发送命令
- **THEN** 系统 SHALL 通过兼容的 SCPI 后端将命令发送到实际设备

#### Scenario: Serial 设备通信
- **WHEN** 标准 sigrok 的串口驱动（如 fluke-dmm）通过 `sr_serial_write()` 发送数据
- **THEN** 系统 SHALL 通过兼容的串口后端将数据发送到实际设备

### Requirement: 驱动适配器宏
系统 SHALL 提供一组适配宏，使标准 sigrok 驱动只需在驱动信息结构体定义处使用宏包装即可完成签名适配。

#### Scenario: 驱动适配宏使用
- **WHEN** 开发者使用 `COMPAT_DRIVER_INFO(dreamsourcelab_dslogic, ...)` 宏定义驱动信息
- **THEN** 宏 SHALL 自动生成 PXView 兼容的 `sr_dev_driver` 结构体，包含所有必要的适配回调

### Requirement: ds_* API 降级处理
系统 SHALL 在 `ds_*` 公共 API 中对非 DSL 设备进行降级处理，缺失的功能（如 `dev_mode_list`、`dev_status_get`）返回合理的默认值或错误码。

#### Scenario: 非 DSL 设备调用 dev_mode_list
- **WHEN** 前端通过 `ds_get_actived_device_mode_list()` 查询兼容驱动设备
- **THEN** 系统 SHALL 返回该设备支持的模式列表（如 LOGIC/ANALOG），若无 `dev_mode_list` 回调则返回仅包含当前模式的单元素列表

#### Scenario: 非 DSL 设备调用 dev_status_get
- **WHEN** 前端通过 `ds_get_actived_device_status()` 查询兼容驱动设备状态
- **THEN** 系统 SHALL 返回 SR_OK 和空状态结构，若无 `dev_status_get` 回调

### Requirement: DeviceAgent 兼容设备支持
系统 SHALL 扩展 DeviceAgent 以支持标准 sigrok 设备类型（SCPI/Serial 连接的设备），包括设备信息获取、配置读写、采集控制等基本操作。

#### Scenario: SCPI 设备信息获取
- **WHEN** 用户选择一个 SCPI 示波器设备
- **THEN** DeviceAgent SHALL 能获取设备厂商、型号、版本等信息（从 `sr_dev_inst` 的 `vendor`/`model`/`version` 字段映射）

#### Scenario: 兼容设备配置读写
- **WHEN** 前端对兼容设备进行配置读写
- **THEN** DeviceAgent SHALL 通过 `ds_get/set_actived_device_config()` 正常工作，`ch` 参数传 NULL

### Requirement: 首批兼容驱动引入
系统 SHALL 首先引入以下标准 sigrok 驱动作为验证：
1. `fx2lafw` — 通用 FX2 USB 逻辑分析仪驱动
2. `saleae-logic` — Saleae Logic 逻辑分析仪驱动

#### Scenario: fx2lafw 驱动扫描设备
- **WHEN** 用户连接 FX2 芯片的 USB 逻辑分析仪并启动 PXView
- **THEN** fx2lafw 驱动 SHALL 扫描到设备并在设备列表中显示

#### Scenario: saleae-logic 驱动采集数据
- **WHEN** 用户选择 Saleae Logic 设备并开始采集
- **THEN** 驱动 SHALL 正确采集逻辑数据并通过 datafeed 回调传回 PXView

### Requirement: 配置键映射
系统 SHALL 提供标准 sigrok 配置键（`SR_CONF_*`，`uint32_t` 枚举）到 PXView 配置键（`int` 枚举）的映射表，对于两者共有的配置键直接映射，对于 PXView 独有的配置键返回 `SR_ERR_NA`。

#### Scenario: 共有配置键映射
- **WHEN** 兼容驱动使用 `SR_CONF_SAMPLERATE`（标准 sigrok 枚举值）
- **THEN** 映射层 SHALL 将其转换为 PXView 的 `SR_CONF_SAMPLERATE`（int 枚举值）

#### Scenario: PXView 独有配置键
- **WHEN** 兼容驱动查询 `SR_CONF_ZERO`（PXView 独有）
- **THEN** 映射层 SHALL 返回 `SR_ERR_NA`

## MODIFIED Requirements

### Requirement: 驱动注册机制
现有 `hwdriver.c` 中的手动静态数组 `drivers_list[]` SHALL 扩展为支持条件编译引入兼容驱动，通过 `HAVE_COMPAT_DRIVERS` 宏控制。

### Requirement: ds_* API 设备类型判断
现有 `ds_*` API SHALL 增加设备类型判断逻辑，对兼容驱动设备跳过 DSL 专有操作（如零点校准、VGA 增益、PWM 输出等）。

### Requirement: compat 层扩展覆盖范围

> **更新（2026-07-02，由 `tiered-driver-compat-fix` spec 完成）**：compat 层（`hardware/compat/`）的覆盖范围在原有基础上大幅扩展，集中了多驱动（≥3）共用的缺失 API 与常量定义，消除驱动本地重复。

compat 层 SHALL 在 `compat_config.h` / `compat_helpers.h` / `compat_helpers.c` 中提供以下扩展实现：

**常量与枚举（compat_config.h）**：
- 缺失 `SR_MQ_*` 值（`SR_MQ_TIME`=10100 等，使用 PXView 保留值避开冲突）
- 缺失 `SR_UNIT_*` 值（`SR_UNIT_REVOLUTIONS_PER_MINUTE` 等）
- 缺失 `SR_MQFLAG_*` 值（`SR_MQFLAG_DURATION`=0x20000 / `SR_MQFLAG_AVG`=0x40000 / `REFERENCE` / `UNSTABLE` / `FOUR_WIRE`）
- `SR_PACKET_INVALID`(-1) / `SR_PACKET_VALID`(0) / `SR_PACKET_NEED_RX`(1)
- `SR_CONF_SWAP`=30159 / `SR_CONF_MEASURED_QUANTITY`=30160 / `SR_CONF_RANGE`=30161
- `SR_ERR_CHANNEL_GROUP`（映射到 `SR_ERR_ARG`）

**增量读写函数（compat_config.h，`#ifndef compat_*_defined` 守卫）**：
- `read_u16le_inc` / `read_u8_inc` / `read_u32le_inc`
- `write_u16le_inc` / `write_u32le_inc` / `write_u24le_inc` / `write_u40le_inc` / `write_u8_inc`

**资源加载（compat_helpers.h/.c，`#ifndef SR_RESOURCE_STRUCT_DEFINED` / `COMPAT_SR_RESOURCE_DECLARED` 守卫）**：
- `struct sr_resource` + `#define SR_RESOURCE_FIRMWARE 1`
- `sr_resource_open` / `sr_resource_read` / `sr_resource_close`（用 `DS_RES_PATH` + fopen/fread/fclose）

**日志与调试（compat_helpers.h/.c）**：
- `#define SR_LOG_SPEW 5`（`#ifndef SR_LOG_SPEW` 守卫）
- `sr_log_loglevel_get`（返回 `SR_LOG_DBG`）
- `sr_hexdump_new` / `sr_hexdump_free`（桩实现，调试用）

**字符串与错误处理（compat_helpers.h/.c）**：
- `sr_strerror`（`#ifndef COMPAT_SR_STRERROR_DECLARED` 守卫）
- `sr_atoi` / `sr_atof_ascii` / `sr_atol`

**std_* 辅助函数（compat_helpers.h/.c，3 参规范签名）**：
- `std_str_idx` / `std_u64_idx`（3 参版本：`(data, strs, count)`，非 5 参）
- `std_u64_tuple_idx` / `std_cg_idx`
- `std_dev_clear_callback` typedef + `std_dev_clear_with_callback`
- `std_gvar_tuple_array`（`uint64[][2]` 规范签名）+ `std_gvar_array_str`（字符串数组，匹配 upstream 两个独立函数）

#### Scenario: 驱动使用 compat 层扩展 API

- **WHEN** 驱动需要固件加载（`sr_resource_*`）、增量读写（`*_inc`）、错误字符串（`sr_strerror`）或 std_* 辅助函数
- **THEN** SHALL `#include "hardware/compat/compat.h"` 并调用 compat 层提供的规范版本
- **AND** SHALL NOT 在驱动本地重复定义这些符号（除非是驱动特有包装器，如 `sigma_sr_resource_load`）

#### Scenario: compat 层常量值安全

- **WHEN** compat 层补全 `SR_MQ_*` / `SR_UNIT_*` / `SR_MQFLAG_*` / `SR_CONF_*` 缺失值
- **THEN** 该值 SHALL 使用 PXView 保留区，不与 `libsigrok.h` 已有枚举冲突
- **AND** SHALL 在注释中标注 "canonical sigrok value = X, PXView reserved value = Y"

#### Scenario: 保持驱动本地的 shim

- **WHEN** 仅 1-2 个驱动需要某 API（如 `sr_sw_limits`、`feed_queue_logic`）
- **THEN** 该 shim SHALL 保持驱动 `protocol.h` 的 `static inline` 本地副本
- **AND** SHALL NOT 提取到 compat 层（避免与现有 `static inline` 副本符号冲突）
