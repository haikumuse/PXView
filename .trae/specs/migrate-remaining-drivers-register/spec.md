# 迁移剩余串口驱动 + CMake 注册（避开 compat-fix 冲突）Spec

## Why

`tiered-driver-compat-fix` spec 正在修改 compat 层文件（`compat_config.h`、`compat_helpers.h/.c`）和 23 个 BROKEN 驱动，有多个后台代理运行中。同时，前期 batch3 的 serial-lcr 迁移失败（sub-agent 被停止，未创建文件），batch4 的 juntek-jds6600/gmc-mh-1x-2x spec 已创建但未实施，且 batch1-4 共 19 个驱动（20 个 driver_info）均未注册到 CMakeLists.txt/hwdriver.c，从未编译验证。

需要一个独立 spec 完成"新驱动迁移 + CMake 注册"，**严格避开** tiered-driver-compat-fix 正在修改的文件，编译验证推迟到 compat-fix 完成后。

## 冲突边界（关键约束）

### tiered-driver-compat-fix 正在修改的文件（本 spec 禁止触碰）

| 文件/驱动 | 修改内容 | 状态 |
|---|---|---|
| `compat/compat_config.h` | 添加 `read/write_u*_inc` 函数、`SR_CONF_*` 常量 | 🔄 进行中 |
| `compat/compat_helpers.h` | 添加 `sr_resource_*`、`sr_hexdump_*`、`sr_log_loglevel_get` | 🔄 进行中 |
| `compat/compat_helpers.c` | 实现 `sr_resource_open/read/close` | 🔄 进行中 |
| `openbench-logic-sniffer/` | 恢复 `convert_trigger`/`ols_metadata_quirks` | ✅ 已完成 |
| `kingst-la2016/` | 补全 `feed_queue_logic` + `sr_resource` + `_inc` + 回调签名 | 🔄 进行中 |
| `asix-sigma/` | 恢复 `sigma_fw_2_bitbang` + `sr_sw_limits` 本地副本 | ✅ 已完成（待编译） |
| `saleae-logic-pro/` | 补 `sr_resource_load` + `usb_source_remove` | ✅ 已完成 |
| `saleae-logic16/` | 迁移 `sr_resource_*` 到 compat 层 | ⏳ 待执行 |
| `sysclk-lwla/` | 迁移 `sr_resource_*` 到 compat 层 | ⏳ 待执行 |
| `lecroy-logicstudio/` | 补 `lls_setup_acquisition` 调用 | ✅ 已完成 |
| `pipistrello-ols/` | 恢复 `SR_CONF_SWAP` | ✅ 已完成 |
| 14 个 DMM 驱动 | `sr_analog_init` 展平 | ✅ 已完成（待编译） |
| 6 个示波器驱动 | 删除本地 `frame_begin/end` + SCPI 包装器 | ✅ 已完成（待编译） |

### 本 spec 允许修改的文件（无冲突）

| 文件 | 操作 | 理由 |
|---|---|---|
| `libsigrok/hardware/serial-lcr/` | **新建** 3 文件 | 目录不存在，纯新建 |
| `libsigrok/hardware/juntek-jds6600/` | **新建** 3 文件 | 目录不存在，纯新建 |
| `libsigrok/hardware/gmc-mh-1x-2x/` | **新建** 3 文件 | 目录不存在，纯新建 |
| `CMakeLists.txt` | 插入 option/add_definitions/list | tiered-driver-compat-fix 不触碰 |
| `libsigrok/hwdriver.c` | 插入 extern/drivers_list | tiered-driver-compat-fix 不触碰 |

### 编译推迟（关键约束）

**本 spec 不执行编译验证。** 原因：tiered-driver-compat-fix 正在修改 compat 层，编译会因 compat 层中间状态失败。编译验证推迟到 tiered-driver-compat-fix 全部完成后，由后续 spec 或手动执行。

## What Changes

### 阶段 1：迁移 3 个纯 Serial 驱动（可并行）

#### serial-lcr（重做 — batch3 失败）
- **driver_info**：1 个 `serial_lcr_driver_info`
- **源**：`C:\Users\admin\Downloads\libsigrok\src\hardware\serial-lcr\`（1490 行）
- **适配点**：
  - 静态 `lcr_info` 数组（8 个模型：ES51919/VC4080 系列）替代 `SR_REGISTER_DEV_DRIVER_LIST`
  - `serial_stream_detect` 7-arg（compat_serial.c 已提供，baudrate 参数未实际使用）
  - 扁平 `sr_datafeed_analog`：`memset(&analog, 0, sizeof(analog))` + `analog.probes`/`analog.mq`/`analog.unit`/`analog.mqflags`/`analog.unit_bits = 32`
  - 本地 `sr_session_send_meta` + `send_frame_start`（参考 gwinstek-psp 模式）
  - 本地 `dev_acquisition_stop`（参考 colead-slm：serial_source_remove + serial_close + std_session_send_df_end）
  - scan 手动遍历 options GSList 解析 SR_CONF_CONN/SR_CONF_SERIALCOMM
  - **枚举标签类型转换**（atorch 验证规则）：dev_context 中 `enum sr_mq mq` → `int mq`、`enum sr_unit unit` → `int unit`、`enum sr_mqflag mqflags` → `uint64_t mqflags`

#### juntek-jds6600（batch4 — 新迁移）
- **driver_info**：1 个 `juntek_jds6600_driver_info`
- **源**：`C:\Users\admin\Downloads\libsigrok\src\hardware\juntek-jds6600\`（1922 行）
- **适配点**：
  - 本地 `std_dummy_dev_acquisition_start/stop` no-op（参考 conrad-digi-35-cpu/hp-59306a）
  - 本地 `dev_clear`：clear_helper + std_dev_clear（参考 atorch）
  - scan 手动遍历 options
  - 无 `sr_analog_init`（信号发生器，无 analog datafeed）
  - `serial_write_blocking`/`serial_read_blocking` 直接调用 compat_serial.c

#### gmc-mh-1x-2x（batch4 — 新迁移）
- **driver_info**：2 个 `gmc_mh_1x_2x_rs232_driver_info` + `gmc_mh_2x_bd232_driver_info`
- **源**：`C:\Users\admin\Downloads\libsigrok\src\hardware\gmc-mh-1x-2x\`（1857 行）
- **适配点**：
  - 2 套 8 compat 包装函数（共享 cleanup/dev_open/dev_close/dev_acquisition_start，独立 scan/config_list）
  - **枚举标签类型转换**：dev_context 中 `enum sr_mq mq` → `int mq`、`enum sr_unit unit` → `int unit`、`enum sr_mqflag mqflags` → `uint64_t mqflags`
  - 扁平 `sr_datafeed_analog`：`memset` + 直接字段赋值 + `analog.unit_bits = 32`
  - 本地 `std_serial_dev_acquisition_stop`（参考 colead-slm）
  - `sr_sw_limits` static inline 带 `#ifndef SR_SW_LIMITS_H` guard 宏
  - `serial_source_add(sdi->session, ...)` 5-arg 保持
  - `serial_read_nonblocking`/`serial_write_blocking`/`serial_flush` 直接调用 compat_serial.c

### 阶段 2：CMake + hwdriver 注册（19 个驱动 / 20 个 driver_info）

在 CMakeLists.txt 和 hwdriver.c 中注册 batch1(4) + batch2(8) + batch3(5) + 本 spec(3) = **20 个驱动目录**（21 个 driver_info，因 gmc-mh-1x-2x 有 2 个 driver_info）。

> 注：batch3 serial-lcr 本 spec 重新迁移后纳入，故总数为 20 个驱动目录 / 21 个 driver_info。

#### CMakeLists.txt 插入点（3 处）
- line 657 后：20 个 `option(ENABLE_DRIVER_*)`
- line 867 后：20 个 `if(ENABLE_DRIVER_*) add_definitions(-DHAVE_DRIVER_*) endif()`
- line 1281 后：20 个 `if(ENABLE_DRIVER_*) list(APPEND libsigrok_SOURCES ...) endif()`（gmc-mh-1x-2x 一个 list 含 2 个 driver_info）

#### hwdriver.c 插入点（2 处）
- line 289 后：21 个 `#ifdef HAVE_DRIVER_* extern SR_PRIV struct sr_dev_driver *_driver_info; #endif`
- line 486 后：21 个 `#ifdef HAVE_DRIVER_* &*_driver_info, #endif`

### 阶段 3：编译验证 — 推迟（不在本 spec 范围）

**明确不执行编译。** 等 tiered-driver-compat-fix 完成后，由后续 spec 或手动执行编译验证。

## 20 个驱动注册清单（batch1+2+3+本 spec 合计）

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
| 15 | gwinstek-gpd | gwinstek_gpd_driver_info | ENABLE_DRIVER_GWINSTEK_GPD | B3 |
| 16 | scpi-dmm | scpi_dmm_driver_info | ENABLE_DRIVER_SCPI_DMM | B3 |
| 17 | serial-lcr | serial_lcr_driver_info | ENABLE_DRIVER_SERIAL_LCR | 本 spec |
| 18 | juntek-jds6600 | juntek_jds6600_driver_info | ENABLE_DRIVER_JUNTEK_JDS6600 | 本 spec |
| 19 | gmc-mh-1x-2x (rs232) | gmc_mh_1x_2x_rs232_driver_info | ENABLE_DRIVER_GMC_MH_1X_2X | 本 spec |
| 20 | gmc-mh-1x-2x (bd232) | gmc_mh_2x_bd232_driver_info | ENABLE_DRIVER_GMC_MH_1X_2X | 本 spec |

注：20 个驱动目录，21 个 driver_info 条目（gmc-mh-1x-2x 一个目录含 2 个 driver_info，共用一个 CMake option）。

## Impact

- **Affected specs**:
  - `migrate-simple-serial-drivers-batch3`（serial-lcr 重做纳入本 spec）
  - `migrate-simple-serial-drivers-batch4`（juntek-jds6600 + gmc-mh-1x-2x 纳入本 spec，batch4 spec 视为被本 spec 取代）
  - `migrate-all-sigrok-drivers`（主 spec 进度更新：已迁移 20 个驱动）
  - `tiered-driver-compat-fix`（本 spec 不触碰其修改范围，编译推迟到其完成后）
- **Affected code**:
  - `libsigrok/hardware/serial-lcr/`（新建：protocol.h, protocol.c, api.c）
  - `libsigrok/hardware/juntek-jds6600/`（新建：protocol.h, protocol.c, api.c）
  - `libsigrok/hardware/gmc-mh-1x-2x/`（新建：protocol.h, protocol.c, api.c）
  - `CMakeLists.txt`（3 处插入点：option / add_definitions / list）
  - `libsigrok/hwdriver.c`（2 处插入点：extern / drivers_list）

## ADDED Requirements

### Requirement: 3 个驱动迁移完成
系统 SHALL 完成以下 3 个纯 Serial 驱动的迁移，每个驱动创建 protocol.h / protocol.c / api.c 三个文件，套用 14 条转换规则并适配各驱动特定点：

#### Scenario: serial-lcr 迁移完成
- **WHEN** 检查 `libsigrok/hardware/serial-lcr/` 目录
- **THEN** 存在 protocol.h（include compat.h，静态 lcr_info 数组 8 模型，dev_context 枚举标签类型已转换为 int/int/uint64_t）、protocol.c（扁平 analog + 本地 sr_session_send_meta + 本地 dev_acquisition_stop）、api.c（8 compat 包装 + driver_info `serial_lcr_driver_info`）
- **AND** 无 `SR_REGISTER_DEV_DRIVER` 宏调用
- **AND** 无 `#include <libsigrok/libsigrok.h>` 或 `#include "libsigrok-internal.h"`

#### Scenario: juntek-jds6600 迁移完成
- **WHEN** 检查 `libsigrok/hardware/juntek-jds6600/` 目录
- **THEN** 存在 protocol.h（include compat.h，dev_context 含 device/waveforms/channel_config/quick_req）、protocol.c（本地 std_dummy_dev_acquisition_start/stop no-op + 本地 dev_clear）、api.c（8 compat 包装 + driver_info `juntek_jds6600_driver_info`）
- **AND** 无 `SR_REGISTER_DEV_DRIVER` 宏调用

#### Scenario: gmc-mh-1x-2x 迁移完成
- **WHEN** 检查 `libsigrok/hardware/gmc-mh-1x-2x/` 目录
- **THEN** 存在 protocol.h（include compat.h，dev_context 枚举标签类型已转换为 int/int/uint64_t，sr_sw_limits static inline 带 guard 宏）、protocol.c（扁平 analog + 本地 std_serial_dev_acquisition_stop）、api.c（2 套 8 compat 包装 + 2 个 driver_info）
- **AND** 无 `SR_REGISTER_DEV_DRIVER` 宏调用

### Requirement: 全部 20 个驱动 CMake 注册
系统 SHALL 在 CMakeLists.txt 和 hwdriver.c 中注册全部 20 个驱动（21 个 driver_info），使所有 batch1-本 spec 驱动可通过 cmake option 启用：

#### Scenario: CMakeLists.txt 注册完成
- **WHEN** 检查 CMakeLists.txt
- **THEN** 存在 20 个 `option(ENABLE_DRIVER_*)` 声明
- **AND** 存在 20 个 `if(ENABLE_DRIVER_*) add_definitions(-DHAVE_DRIVER_*) endif()` 块
- **AND** 存在 20 个 `if(ENABLE_DRIVER_*) list(APPEND libsigrok_SOURCES ...) endif()` 块

#### Scenario: hwdriver.c 注册完成
- **WHEN** 检查 libsigrok/hwdriver.c
- **THEN** 存在 21 个 `#ifdef HAVE_DRIVER_* extern SR_PRIV struct sr_dev_driver *_driver_info; #endif` 声明
- **AND** 存在 21 个 `#ifdef HAVE_DRIVER_* &*_driver_info, #endif` drivers_list 项

### Requirement: 不触碰 tiered-driver-compat-fix 修改范围
本 spec 的所有操作 SHALL 严格避开 tiered-driver-compat-fix 正在修改的文件和驱动：

#### Scenario: 无冲突
- **WHEN** 本 spec 实施完成
- **THEN** `compat/compat_config.h`、`compat/compat_helpers.h`、`compat/compat_helpers.c` 未被本 spec 修改
- **AND** 23 个 BROKEN 驱动（openbench-logic-sniffer/kingst-la2016/asix-sigma/saleae-logic-pro/saleae-logic16/sysclk-lwla/lecroy-logicstudio/pipistrello-ols/14 个 DMM/6 个示波器）未被本 spec 修改
- **AND** 未执行编译（编译推迟到 tiered-driver-compat-fix 完成后）

## MODIFIED Requirements

### Requirement: migrate-all-sigrok-drivers 主 spec 进度
主 spec `migrate-all-sigrok-drivers` 的迁移进度 SHALL 更新为：已迁移 20 个驱动（batch1: 4 + batch2: 8 + batch3: 4 + 本 spec: 3 + serial-lcr 重做），剩余 6 个驱动因平台/后端依赖暂缓。

## REMOVED Requirements
无删除项。
