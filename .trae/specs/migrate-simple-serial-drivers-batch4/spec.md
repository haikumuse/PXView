# 迁移简单串口驱动 Batch 4 Spec

## Why
Batch 1-3 已迁移 17 个驱动（4+8+5）。剩余 8 个未迁移驱动中，4 个平台特定（baylibre-acme/beaglelogic/dcttech-usbrelay/mooshimeter-dmm）、1 个复杂 USB（greatfet）、1 个需扩展 TCP 后端（devantech-eth008）均不符合"不需要更改太多"标准。仅 **juntek-jds6600**（纯 Serial 信号发生器）和 **gmc-mh-1x-2x**（纯 Serial DMM，2 个 driver_info）为纯串口驱动，沿用 Batch 1-3 已验证的 14 条转换规则即可完成，本批次将迁移这两个驱动并完成全部 19 个驱动（20 个 driver_info）的 CMake 注册与编译验证。

## What Changes
- 迁移 `juntek-jds6600`（1922 行，纯 Serial 信号发生器，1 个 driver_info `juntek_jds6600_driver_info`）
- 迁移 `gmc-mh-1x-2x`（1857 行，纯 Serial DMM，2 个 driver_info `gmc_mh_1x_2x_rs232_driver_info` + `gmc_mh_2x_bd232_driver_info`）
- CMakeLists.txt 注册 19 个驱动 option / add_definitions / 源文件 list（batch1+2+3+4 合计）
- hwdriver.c 注册 20 个 driver_info 的 extern 声明 + drivers_list 项
- cmake 重新配置启用全部 19 个驱动 + ninja 编译验证

### 沿用 Batch 1-3 的 14 条转换规则
1. `#include <libsigrok/libsigrok.h>` + `"libsigrok-internal.h"` → `#include "compat.h"`
2. `std_session_send_df_header(sdi, LOG_PREFIX)` 保持 2-arg 调用（compat.h:220 提供）
3. `std_session_send_df_frame_begin(sdi)` 保持调用（compat_helpers.c 单一实现）
4. `std_session_send_df_end(sdi, LOG_PREFIX)` 保持 2-arg 调用（compat.h:224 提供）
5. `di->context` → `di->priv`（drv_context 私有数据访问）
6. `sr_dev_inst_new(inst_type, status, vendor, model, version)` → compat_sr_dev_inst_new（compat.h:194 宏）
7. 移除 `SR_REGISTER_DEV_DRIVER(name)` 宏调用（compat.h:135 仅声明 extern，PXView 不支持 constructor 注册）
8. 8 个 compat 包装函数：`<name>_compat_init` / `cleanup` / `scan` / `dev_open` / `dev_close` / `dev_acquisition_start` / `dev_acquisition_stop` / `config_list`（签名适配 PXView）
9. `config_channel_set` 合并到 `config_set`（PXView 无独立 channel_set 回调）
10. local `std_*_idx` helper（compat 层的 std_u64_idx/std_str_idx/std_bool_idx 签名为 `(sdi, key, data, vals, count)`）
11. `sr_config_set_compat(sdi, cg, key, data)` 替代 `sr_config_set`（compat.h:180）
12. driver_info 命名 `<name>_driver_info`（extern + static drv_ptr + compat_init 模式）
13. `serial_source_add(sdi->session, serial, G_IO_IN, 40, cb, sdi)` 保持 5-arg（session 参数保留）
14. `scan_complete_compat(sdi, LOG_PREFIX)` 替代 `std_scan_complete`（compat 提供）

### 各驱动特定适配点

#### juntek-jds6600（1922 行，纯 Serial 信号发生器）
- **driver_info**：1 个 `juntek_jds6600_driver_info`
- **std_dummy_dev_acquisition_start/stop**：PXView compat 层不提供，本地实现 no-op 返回 SR_OK（参考 conrad-digi-35-cpu/hp-59306a/icstation-usbrelay 已验证模式）
- **std_dev_clear_with_callback**：PXView 不提供，本地实现：先调用 `clear_helper(devc)` 释放 dev_context 内部资源（g_free serial_number/names/fw_codes/quick_req），再调用 `std_dev_clear(driver)`
- **无 sr_analog_init**：信号发生器，无 datafeed analog 输出
- **无 feed_queue_analog**：使用 `std_session_send_df_header` + 直接 `sr_session_send`
- **scan**：手动遍历 options GSList 解析 SR_CONF_CONN/SR_CONF_SERIALCOMM（`sr_serial_extract_options` 不可用）
- **serial_write_blocking/serial_read_blocking**：compat_serial.c 已提供
- **CMake option**：`ENABLE_DRIVER_JUNTEK_JDS6600`，HAVE 宏 `HAVE_DRIVER_JUNTEK_JDS6600`

#### gmc-mh-1x-2x（1857 行，纯 Serial DMM，2 个 driver_info）
- **driver_info**：2 个 `gmc_mh_1x_2x_rs232_driver_info` + `gmc_mh_2x_bd232_driver_info`（同一文件两个通信协议变体）
- **8 compat 包装函数 × 2 套**：两个 driver_info 各需独立 compat_init/scan/config_list 等（cleanup/dev_open/dev_close/dev_acquisition_start 可共享同名实现，config_get/config_set 共享，但 driver_info 结构体字段分别指向）
- **枚举标签类型转换（atorch 验证规则）**：PXView 的 `libsigrok.h` 将 `SR_MQ_*`/`SR_UNIT_*`/`SR_MQFLAG_*` 定义为匿名枚举值，**不存在带标签的 `enum sr_mq`/`enum sr_unit`/`enum sr_mqflag` 类型**，使用枚举标签会导致 "incomplete type" 编译错误。dev_context 中 `enum sr_mq mq` → `int mq`；`enum sr_unit unit` → `int unit`；`enum sr_mqflag mqflags` → `uint64_t mqflags`（参考 atorch/fluke-dmm 模式）
- **sr_analog_init → flat analog 适配**：`memset(&analog, 0, sizeof(analog))`；`analog.meaning->mq` → `analog.mq`；`analog.meaning->unit` → `analog.unit`；`analog.meaning->mqflags` → `analog.mqflags`；`analog.meaning->channels` → `analog.probes`（g_slist_append(NULL, ch)）；`analog.encoding->digits` → `analog.unit_bits = 32`（sizeof(float)）；移除 encoding/meaning/spec 子结构
- **std_serial_dev_acquisition_stop**：PXView 不提供，本地实现 `serial_source_remove` + `serial_close` + `std_session_send_df_end`（参考 colead-slm 已验证模式）
- **serial_read_nonblocking**：compat_serial.c:745 已提供
- **serial_source_add(sdi->session, ...)**：5-arg 保持（session 参数保留）
- **g_usleep**：直接使用（glib 提供）
- **sr_sw_limits**：protocol.h 内 static inline 定义带 `#ifndef SR_SW_LIMITS_H` guard 宏防重复
- **scan**：手动遍历 options GSList 解析 SR_CONF_CONN/SR_CONF_SERIALCOMM
- **CMake option**：`ENABLE_DRIVER_GMC_MH_1X_2X`，HAVE 宏 `HAVE_DRIVER_GMC_MH_1X_2X`（一个 option 启用 2 个 driver_info）

## Impact
- Affected specs:
  - `migrate-all-sigrok-drivers`（主 spec，Batch 4 完成后剩余 6 个平台/后端依赖驱动留待后续）
  - `migrate-simple-serial-drivers-batch3`（Task 6-7 CMake 注册 + 编译验证由 Batch 4 Task 3-4 统一完成，覆盖 batch1+2+3+4 共 19 个驱动）
  - `audit-and-fix-migrated-drivers`（独立进行的已迁移驱动审计，不阻塞 Batch 4）
- Affected code:
  - `libsigrok/hardware/juntek-jds6600/`（新建：protocol.h, protocol.c, api.c）
  - `libsigrok/hardware/gmc-mh-1x-2x/`（新建：protocol.h, protocol.c, api.c）
  - `CMakeLists.txt`（line 657 后插入 2 个 option，line 867 后插入 2 个 add_definitions，line 1281 后插入 2 个源文件 list）
  - `libsigrok/hwdriver.c`（line 289 后插入 3 个 extern，line 486 后插入 3 个 drivers_list 项；gmc-mh-1x-2x 占 2 项）

## 19 个驱动注册清单（batch1+2+3+4 合计）

| # | 驱动 | driver_info 名 | CMake option | 批次 |
|---|---|---|---|---|
| 1 | conrad-digi-35-cpu | conrad_digi_35_cpu_driver_info | ENABLE_DRIVER_CONRAD_DIGI_35_CPU | B1 |
| 2 | hp-59306a | hp_59306a_driver_info | ENABLE_DRIVER_HP_59306A | B1 |
| 3 | colead-slm | colead_slm_driver_info | ENABLE_DRIVER_COLEAD_SLM | B1 |
| 4 | icstation-usbrelay | icstation_usbrelay_driver_info | ENABLE_DRIVER_ICSTATION_USBRELAY | B1 |
| 5 | zketech-ebd-usb | zketech_ebd_usb_driver_info | ENABLE_DRIVER_ZKETECH_EBD_USB | B2 |
| 6 | arachnid-labs-re-load-pro | arachnid_labs_re_load_pro_driver_info | ENABLE_DRIVER_ARACHNID_LABS_RE_LOAD_PRO | B2 |
| 7 | asix-omega-rtm-cli | asix_omega_rtm_cli_driver_info | ENABLE_DRIVER_ASIX_OMEGA_RTM_CLI | B2 |
| 8 | kecheng-kc-330b | kecheng_kc_330b_driver_info | ENABLE_DRIVER_KECHENG_KC_330B | B2 |
| 9 | hp-3457a | hp_3457a_driver_info | ENABLE_DRIVER_HP_3457A | B2 |
| 10 | microchip-pickit2 | microchip_pickit2_driver_info | ENABLE_DRIVER_MICROCHIP_PICKIT2 | B2 |
| 11 | hp-3478a | hp_3478a_driver_info | ENABLE_DRIVER_HP_3478A | B2 |
| 12 | cem-dt-885x | cem_dt_885x_driver_info | ENABLE_DRIVER_CEM_DT_885X | B2 |
| 13 | atorch | atorch_driver_info | ENABLE_DRIVER_ATORCH | B3 |
| 14 | bkprecision-1856d | bkprecision_1856d_driver_info | ENABLE_DRIVER_BKPRECISION_1856D | B3 |
| 15 | serial-lcr | serial_lcr_driver_info | ENABLE_DRIVER_SERIAL_LCR | B3 |
| 16 | gwinstek-gpd | gwinstek_gpd_driver_info | ENABLE_DRIVER_GWINSTEK_GPD | B3 |
| 17 | scpi-dmm | scpi_dmm_driver_info | ENABLE_DRIVER_SCPI_DMM | B3 |
| 18 | juntek-jds6600 | juntek_jds6600_driver_info | ENABLE_DRIVER_JUNTEK_JDS6600 | B4 |
| 19 | gmc-mh-1x-2x (rs232) | gmc_mh_1x_2x_rs232_driver_info | ENABLE_DRIVER_GMC_MH_1X_2X | B4 |
| 20 | gmc-mh-1x-2x (bd232) | gmc_mh_2x_bd232_driver_info | ENABLE_DRIVER_GMC_MH_1X_2X | B4 |

注：19 个驱动目录，20 个 driver_info 条目（gmc-mh-1x-2x 一个目录含 2 个 driver_info）。

## ADDED Requirements

### Requirement: Batch 4 驱动迁移完成
系统 SHALL 完成以下 2 个驱动的迁移，每个驱动创建 protocol.h / protocol.c / api.c 三个文件，套用 14 条转换规则并适配各驱动特定点：

#### Scenario: juntek-jds6600 迁移完成
- **WHEN** 检查 `libsigrok/hardware/juntek-jds6600/` 目录
- **THEN** 存在 protocol.h（include compat.h，保留 dev_context 含 device/waveforms/channel_config/quick_req；保留 jds6600_* 函数声明）、protocol.c（套用转换规则；本地 std_dummy_dev_acquisition_start/stop no-op；本地 dev_clear 调用 clear_helper + std_dev_clear）、api.c（8 compat 包装 + driver_info `juntek_jds6600_driver_info`；scan 手动遍历 options；无 sr_analog_init）
- **AND** 无 `SR_REGISTER_DEV_DRIVER` 宏调用
- **AND** 无 `#include <libsigrok/libsigrok.h>` 或 `#include "libsigrok-internal.h"`（已替换为 `#include "compat.h"`）

#### Scenario: gmc-mh-1x-2x 迁移完成
- **WHEN** 检查 `libsigrok/hardware/gmc-mh-1x-2x/` 目录
- **THEN** 存在 protocol.h（include compat.h，保留 dev_context 含 model/limits/mq/unit/mqflags/value/scale/buf；保留 gmc_* 函数声明；sr_sw_limits static inline 带 guard 宏）、protocol.c（套用转换规则；sr_analog_init → flat analog 适配 memset+直接字段赋值；analog.meaning->channels → analog.probes）、api.c（2 套 8 compat 包装 + 2 个 driver_info `gmc_mh_1x_2x_rs232_driver_info` + `gmc_mh_2x_bd232_driver_info`；本地 std_serial_dev_acquisition_stop；scan 手动遍历 options；serial_source_add 5-arg）
- **AND** 无 `SR_REGISTER_DEV_DRIVER` 宏调用
- **AND** 无 `#include <libsigrok/libsigrok.h>` 或 `#include "libsigrok-internal.h"`

### Requirement: 全部 19 个驱动 CMake 注册
系统 SHALL 在 CMakeLists.txt 和 hwdriver.c 中注册全部 19 个驱动（20 个 driver_info），使所有 batch1-4 驱动可通过 cmake option 启用编译：

#### Scenario: CMakeLists.txt 注册完成
- **WHEN** 检查 CMakeLists.txt
- **THEN** 存在 19 个 `option(ENABLE_DRIVER_*)` 声明
- **AND** 存在 19 个 `if(ENABLE_DRIVER_*) add_definitions(-DHAVE_DRIVER_*) endif()` 块
- **AND** 存在 19 个 `if(ENABLE_DRIVER_*) list(APPEND libsigrok_SOURCES ...) endif()` 块（gmc-mh-1x-2x 的 2 个 driver_info 共用一个源文件 list）

#### Scenario: hwdriver.c 注册完成
- **WHEN** 检查 libsigrok/hwdriver.c
- **THEN** 存在 20 个 `#ifdef HAVE_DRIVER_* extern SR_PRIV struct sr_dev_driver *_driver_info; #endif` 声明（gmc-mh-1x-2x 占 2 个）
- **AND** 存在 20 个 `#ifdef HAVE_DRIVER_* &*_driver_info, #endif` drivers_list 项

### Requirement: 编译验证通过
系统 SHALL 通过 cmake 配置启用全部 19 个驱动并成功编译：

#### Scenario: cmake 配置 + ninja 编译成功
- **WHEN** 运行 `cmake -DENABLE_DRIVER_*=ON ...`（19 个驱动全部 ON）+ `cd build && ninja -j 16`
- **THEN** 编译 SHALL 成功完成，无 multiple definition / undefined reference 错误
- **AND** PXView.exe 生成成功

## MODIFIED Requirements

### Requirement: migrate-all-sigrok-drivers 主 spec 进度
主 spec `migrate-all-sigrok-drivers` 的迁移进度 SHALL 更新为：已迁移 19 个驱动（batch1: 4 + batch2: 8 + batch3: 5 + batch4: 2），剩余 6 个驱动因平台/后端依赖暂缓（baylibre-acme/beaglelogic/dcttech-usbrelay/mooshimeter-dmm/greatfet/devantech-eth008）。

## REMOVED Requirements
无删除项。
