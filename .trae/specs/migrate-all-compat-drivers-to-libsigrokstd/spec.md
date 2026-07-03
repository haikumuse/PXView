# 批量迁移 compat 驱动到 libsigrokstd 并删除 compat 层 Spec

## Why

方案B(双库共存)已验证胶水层(srstd_bridge + srstd_pxview_glue)完全通用——通过上游 API(`sr_driver_scan`/`sr_dev_open`/`sr_config_get/set`/`sr_dev_acquisition_start/stop`)调用驱动,不依赖任何特定驱动名。slogic 迁移已证明每个驱动只需机械的接口适配(拷源码+改 include+去 static+注册),不需要改业务逻辑。

当前 PXView libsigrok 的 compat 层(`ENABLE_COMPAT_DRIVERS=ON`)包含约 80 个驱动,是 2014 年 fork 后逐个修补的标准 sigrok 驱动,与上游 libsigrok 存在大量重复代码。维护两套驱动代码是技术债务。

将所有 compat 驱动批量迁移到 libsigrokstd(用上游原生源码),然后删除 PXView compat 层,可以:
1. 消除重复代码,降低维护负担
2. 让所有标准 sigrok 驱动使用上游最新实现(功能更完整)
3. PXView libsigrok 只保留 DSL/pxlogic 原生驱动,职责清晰

## What Changes

- **批量迁移**:将 PXView compat 层约 80 个驱动全部用上游 libsigrok 原生源码替代,拷入 `libsigrokstd/src/hardware/<driver>/`
- **自动化脚本**:编写 Python 脚本批量处理机械操作(拷贝+改 include+去 static+生成注册代码)
- **批量驱动注册**:重构 `srstd_init_shared.c`,从手动单驱动注册改为遍历所有迁移驱动的 `driver_info` 符号批量注册
- **DeviceAgent 驱动名识别**:扩展 `is_srstd_device()` 从硬编码 "sipeed-slogic-analyzer" 改为查询 `srstd_glue_get_driver_names()` 动态列表
- **删除 compat 层**:**BREAKING** 删除 `libsigrok/hardware/compat/` 整个目录、`libsigrok/hardware/` 下所有 compat 驱动目录、`CMake/options.cmake` 中所有 `ENABLE_DRIVER_*` option、`CMake/libsigrok.cmake` 中 compat 块
- **删除 `ENABLE_COMPAT_DRIVERS` 开关**:**BREAKING** 该开关不再需要,默认 OFF 且代码删除
- **保留 `sr_session_trigger_get` stub**:迁移到 libsigrokstd 内部(因为上游 libsigrok 本身有完整实现,compat 层的 stub 不再需要)
- **DSL/pxlogic 原生驱动不动**:DSLogic/DSCope/pxlogic 保留在 PXView libsigrok,不走 srstd 路径

## Impact

- Affected specs: `dual-libsigrok-coexist-restore-features`(方案B 的延伸)、`restructure-compat-and-migrate-slogic`(slogic 已完成,本 spec 是其扩展)、`add-sigrok-driver-compat-layer`(compat 层将被删除)
- Affected code:
  - `libsigrokstd/src/hardware/` 新增约 80 个驱动目录
  - `libsigrokstd/CMakeLists.txt` 批量注册源文件
  - `libsigrokstd/bridge/srstd_init_shared.c` 批量驱动注册
  - `libsigrokstd/bridge/srstd_pxview_glue.c` 无需改(已通用)
  - `PXView/pv/deviceagent.h/.cpp` `is_srstd_device()` 改为动态查询
  - `libsigrok/hardware/compat/` 整个删除
  - `libsigrok/hardware/` 下约 80 个 compat 驱动目录删除
  - `libsigrok/hwdriver.c` 删除 compat 驱动注册
  - `CMake/options.cmake` 删除 `ENABLE_COMPAT_DRIVERS` + 81 个 `ENABLE_DRIVER_*`
  - `CMake/libsigrok.cmake` 删除 compat 块

## ADDED Requirements

### Requirement: 批量驱动迁移自动化

系统 SHALL 提供 Python 脚本 `tools/migrate_drivers_to_srstd.py`,自动化执行以下机械操作:
1. 从上游 `c:\Users\admin\Downloads\libsigrok\src\hardware\<driver>\` 拷贝源码到 `libsigrokstd/src/hardware/<driver>/`
2. 修改每个驱动的 `protocol.h`:确保 `#include "srstd.h"` 在 `#include "libsigrok-internal.h"` 之前
3. 修改每个驱动的 `api.c`:去除 `<driver_name>_driver_info` 结构的 `static` 修饰符
4. 生成 `libsigrokstd/src/driver_registry.c`:包含所有驱动的 `extern` 声明 + `srstd_get_all_drivers()` 函数返回 `struct sr_dev_driver**` 数组

#### Scenario: 脚本执行成功
- **WHEN** 运行 `python tools/migrate_drivers_to_srstd.py --all`
- **THEN** 所有 compat 层驱动(除 demo/DSL/pxlogic)被拷贝到 `libsigrokstd/src/hardware/`,每个驱动的 protocol.h 含 `#include "srstd.h"`,api.c 的 driver_info 无 static 修饰符,`driver_registry.c` 生成完成

#### Scenario: 排除特殊依赖驱动
- **WHEN** 驱动依赖 VXI(rpc/rpc.h)/GPIB/VISA 等 Windows 不可用库
- **THEN** 脚本跳过这些驱动并在日志中记录,不阻断其他驱动迁移

### Requirement: 批量驱动注册

系统 SHALL 在 `srstd_init_shared()` 中通过 `driver_registry.c` 提供的 `srstd_get_all_drivers()` 函数批量注册所有迁移驱动到 `ctx->driver_list`,而非手动逐个注册。

#### Scenario: 所有迁移驱动自动注册
- **WHEN** `srstd_init_shared()` 被调用
- **THEN** 所有通过 `driver_registry.c` 注册的驱动自动添加到 `ctx->driver_list`,胶水层 `srstd_glue_scan_devices()` 遍历 driver_list 自动扫描所有驱动

### Requirement: DeviceAgent 动态驱动识别

`DeviceAgent::is_srstd_device()` SHALL 通过查询 `srstd_glue_get_driver_names()` 返回的驱动名列表判断,而非硬编码 "sipeed-slogic-analyzer"。

#### Scenario: 任意 srstd 驱动设备被正确分流
- **WHEN** 用户连接任意 srstd 库支持的设备(如 fx2lafw/saleae-logic16/chronovu-la)
- **THEN** `is_srstd_device()` 返回 true,设备通过 `srstd_glue_*` 路径处理,而非 PXView `ds_*` 路径

## REMOVED Requirements

### Requirement: compat 驱动兼容层
**Reason**: 所有 compat 驱动已迁移到 libsigrokstd,使用上游原生实现,PXView 不再需要维护 compat 层
**Migration**:
- `libsigrok/hardware/compat/` 整个删除
- `libsigrok/hardware/` 下约 80 个 compat 驱动目录删除
- `CMake/options.cmake` 删除 `ENABLE_COMPAT_DRIVERS` + 81 个 `ENABLE_DRIVER_*`
- `CMake/libsigrok.cmake` 删除 compat 块
- `libsigrok/hwdriver.c` 删除 compat 驱动注册
- DSL/pxlogic 原生驱动不受影响,继续走 PXView `ds_*` 路径

### Requirement: sr_session_trigger_get stub
**Reason**: 上游 libsigrok 有完整实现(srstd_session_trigger_get),compat 层的 stub 不再需要
**Migration**: 当前调用 `sr_session_trigger_get` 的 compat 驱动已全部迁移到 libsigrokstd,使用上游 `srstd_session_trigger_get` 实现

## 风险与决策点

### 风险1: 上游驱动与 PXView fork 版本差异
PXView 的 compat 驱动是 2014 年 fork 后逐个修补的,可能包含 PXView 特有的修改。上游版本可能行为不同。
**缓解**: 胶水层提供统一的接口适配,差异通过 `srstd_bridge.c` 转换层吸收。关键设备需硬件验证。

### 风险2: 特殊依赖驱动
某些驱动依赖 libserialport/hidapi/libftdi 等,PXView 当前可能未安装这些库。
**缓解**: 上游驱动有 `#ifdef` 守卫,缺失依赖时编译为 stub。脚本排除 VXI/GPIB/VISA 等不可用驱动。

### 决策点: demo 驱动
PXView 有自己的 `demo/demo.c`(用于无硬件测试),上游也有 `demo/`。两者实现可能不同。
**建议**: 保留 PXView 的 demo 驱动(走 `ds_*` 路径),不迁移上游 demo。因为 PXView demo 可能有特定测试模式。

### 决策点: SCPI 串口驱动
某些 SCPI/串口驱动(fluke-dmm/rigol-ds/siglent-sds 等)依赖 libserialport,PXView 可能未安装。
**建议**: 迁移这些驱动但保留 `#ifdef HAVE_LIBSERIALPORT` 守卫,缺失依赖时驱动不扫描设备(不阻断编译)。
