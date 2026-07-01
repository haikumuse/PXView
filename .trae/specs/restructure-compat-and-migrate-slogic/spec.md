# 重构兼容层缺陷并迁移 sipeed-slogic-analyzer Spec

## Why

兼容层存在一处潜在链接缺陷：`std_session_send_df_frame_begin` 在 5 个驱动（fx2lafw、gwinstek-gds-800、hameg-hmo、hantek-dso、hung-chang-dso-2100）各自的 protocol.c 中本地定义，且 `SR_PRIV` 在 Windows 为空（`libsigrok.h:169`），导致这些"本地"定义实为全局符号。若同时启用其中两个驱动，会触发 `multiple definition` 链接冲突。

与此同时，`libsigrok-slogic-dev`（深圳市矽速科技 SLogic Combo 8 / 16U3 逻辑分析仪）相对标准 libsigrok 新增了 `sipeed-slogic-analyzer` 驱动，需要迁移到 PXView 兼容层。该驱动同样调用 `std_session_send_df_frame_begin`——若先完成缺陷重构，迁移时直接复用 compat 层的单一实现，无需再本地定义。

两件事共享兼容层基础设施，合并为一次 spec 推进最经济。

## What Changes

### A. 重构 `std_session_send_df_frame_begin` 缺陷（**BREAKING**：删除 5 个本地定义）

- 在 `libsigrok/hardware/compat/compat_helpers.h` 新增 `std_session_send_df_frame_begin` 声明
- 在 `libsigrok/hardware/compat/compat_helpers.c` 新增单一规范实现（仿现有 `std_session_send_df_end` 模式，发送 `SR_DF_FRAME_BEGIN` packet）
- 删除 5 个驱动的本地定义（protocol.h 声明 + protocol.c 定义）：
  - `fx2lafw`
  - `gwinstek-gds-800`
  - `hameg-hmo`
  - `hantek-dso`
  - `hung-chang-dso-2100`

**BREAKING 性质说明**：删除的是各驱动 `SR_PRIV` 本地副本，调用点不变（函数名与签名一致），由 compat 层单一实现接管。仅当多个相关驱动同时启用时才显现收益（消除链接冲突）；单驱动启用时行为等价。

### B. 迁移 sipeed-slogic-analyzer 驱动

源：`C:\Users\admin\Downloads\libsigrok-slogic-dev\src\hardware\sipeed-slogic-analyzer\`（api.c 33KB / protocol.c 14KB / protocol.h 3KB）

目标：`libsigrok/hardware/sipeed-slogic-analyzer/`，按 pipistrello-ols / chronovu-la 模板迁移：

- **protocol.h**：include 改为 `#include "hardware/compat/compat.h"`；保留 `struct dev_context`（libusb transfer 数组、GAsyncQueue、`soft_trigger_logic *stl`、`slogic_model` 函数指针表）
- **protocol.c**：include 改为 compat.h；`std_session_send_df_header(sdi)` / `end(sdi)` → 2-arg `(sdi, LOG_PREFIX)`；`sr_session_source_add(sdi->session, poll_obj, ...)` → 5-arg（移除 session）；`sr_session_source_remove(sdi->session, poll_obj)` → 1-arg；libusb 直接调用保留；**不本地定义** `std_session_send_df_frame_begin`（使用 A 部分新增的 compat 层单一实现）
- **api.c**：include 改为 compat.h；`std_scan_complete` → `std_scan_complete_compat`；`di->context` → `di->priv`；移除 `SR_REGISTER_DEV_DRIVER` 宏；结构体初始化器移除 `.config_channel_set` 和 `.dev_clear` 字段（PXView `struct sr_dev_driver` 无此二字段），`.context = NULL` → `.priv = NULL`，`.dev_mode_list`/`.dev_destroy`/`.dev_status_get` 指向 compat 默认实现；添加 8 个 compat 包装函数（init/cleanup/scan/config_get/config_set/config_list/acquisition_start/acquisition_stop），桥接 PXView 5-参签名（含 `ch`）与标准 sigrok 4-参签名；`config_channel_set` 的 per-channel 逻辑合并进 `config_set` 包装（`ch != NULL` 分支）

### C. 注册与构建集成

- `CMakeLists.txt`：在 `if(ENABLE_COMPAT_DRIVERS)` 块内添加 `option(ENABLE_DRIVER_SIPEED_SLOGIC_ANALYZER ...)` + 源文件条目（api.c + protocol.c）+ `add_definitions(-DHAVE_DRIVER_SIPEED_SLOGIC_ANALYZER)`
- `libsigrok/hardware/hwdriver.c`：添加 `extern struct sr_dev_driver sipeed_slogic_analyzer_driver_info;` 声明 + `&sipeed_slogic_analyzer_driver_info` 注册项（由 `HAVE_DRIVER_SIPEED_SLOGIC_ANALYZER` 守卫）

## Impact

- **Affected specs**: 无直接关联 spec（独立工作）
- **Affected code**:
  - `libsigrok/hardware/compat/compat_helpers.h` / `.c`（新增 `std_session_send_df_frame_begin`）
  - `libsigrok/hardware/fx2lafw/protocol.h` / `protocol.c`（删除本地定义）
  - `libsigrok/hardware/gwinstek-gds-800/protocol.h` / `protocol.c`（删除本地定义）
  - `libsigrok/hardware/hameg-hmo/protocol.h` / `protocol.c`（删除本地定义）
  - `libsigrok/hardware/hantek-dso/protocol.h` / `protocol.c`（删除本地定义）
  - `libsigrok/hardware/hung-chang-dso-2100/protocol.h` / `protocol.c`（删除本地定义）
  - `libsigrok/hardware/sipeed-slogic-analyzer/api.c` / `protocol.c` / `protocol.h`（新建）
  - `CMakeLists.txt`（新增 option + 源条目 + add_definitions）
  - `libsigrok/hardware/hwdriver.c`（新增 extern + 注册）

## ADDED Requirements

### Requirement: 兼容层提供 `std_session_send_df_frame_begin` 单一实现

compat 层 SHALL 在 `compat_helpers.h` 声明、`compat_helpers.c` 定义 `std_session_send_df_frame_begin(const struct sr_dev_inst *sdi)`，发送 `SR_DF_FRAME_BEGIN` 类型 packet。该实现 SHALL 与现有 5 个驱动本地实现语义等价（发送 frame begin packet 并 `ds_data_forward`）。

#### Scenario: 多驱动同时启用无链接冲突
- **WHEN** 用户同时启用 fx2lafw 与 sipeed-slogic-analyzer（两者均调用 `std_session_send_df_frame_begin`）
- **THEN** 链接成功，无 `multiple definition` 错误

#### Scenario: 单驱动行为不变
- **WHEN** 仅启用 hantek-dso（已删除本地定义，改用 compat 层实现）
- **THEN** 运行时 frame begin packet 行为与重构前等价

### Requirement: sipeed-slogic-analyzer 驱动接入 PXView 兼容层

系统 SHALL 在 `libsigrok/hardware/sipeed-slogic-analyzer/` 提供迁移后的驱动源码（api.c / protocol.c / protocol.h），遵循 pipistrello-ols / chronovu-la 既定 compat 包装模式。

#### Scenario: 驱动编译通过
- **WHEN** `ENABLE_DRIVER_SIPEED_SLOGIC_ANALYZER=ON` 且执行 `ninja -j 16`
- **THEN** sipeed-slogic-analyzer 的 api.c / protocol.c 编译通过，仅允许警告（如 LOG_PREFIX 重定义）

#### Scenario: 驱动注册可见
- **WHEN** `HAVE_DRIVER_SIPEED_SLOGIC_ANALYZER` 宏定义且 PXView 启动
- **THEN** `sipeed_slogic_analyzer_driver_info` 出现在 hwdriver.c 驱动列表

#### Scenario: 兼容层包装正确
- **WHEN** PXView 调用 `config_set(id, data, sdi, ch, cg)`（5-参）
- **THEN** 包装函数正确转发至标准 sigrok 4-参 `config_set(key, data, sdi, cg)`，并在 `ch != NULL` 时执行原 `config_channel_set` 的 per-channel 逻辑

## MODIFIED Requirements

### Requirement: 5 个驱动不再本地定义 `std_session_send_df_frame_begin`

fx2lafw / gwinstek-gds-800 / hameg-hmo / hantek-dso / hung-chang-dso-2100 的 protocol.h SHALL 移除 `std_session_send_df_frame_begin` 声明，protocol.c SHALL 移除其本地定义。调用点不变，由 compat 层单一实现接管。

## REMOVED Requirements

### Requirement: 各驱动本地定义 `std_session_send_df_frame_begin`

**Reason**: `SR_PRIV` 在 Windows 为空导致全局符号冲突隐患；5 份重复实现维护负担高
**Migration**: 统一收编进 `compat_helpers.c`，调用点无需修改（函数名与签名一致）
