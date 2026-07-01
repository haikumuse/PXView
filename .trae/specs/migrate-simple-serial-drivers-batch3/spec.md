# 简单 Serial/SCPI 驱动批量迁移 Spec（Batch 3）

## Why

`migrate-simple-serial-drivers-batch1` spec 规划了 8 个简单驱动迁移，仅完成 Task 1-4（conrad-digi-35-cpu/hp-59306a/colead-slm/icstation-usbrelay），Task 5-8（atorch/bkprecision-1856d/serial-lcr/gwinstek-gpd）尚未执行。同时评估剩余 14 个未迁移驱动后发现：4 个被平台特定原因排除（baylibre-acme/beaglelogic Linux 专用、dcttech-usbrelay 需 hidapi、mooshimeter-dmm 蓝牙），5 个代码量过大（>1800 行）留待后续批次。本批次聚焦 5 个低风险驱动（4 个 batch1 遗留 + scpi-dmm），全部基于已验证的 rdtech-um/serial-dmm/hp-3457a 迁移模板机械套用。

## What Changes

- 迁移 5 个驱动到 `libsigrok/hardware/<name>/`（创建 protocol.h/protocol.c/api.c 三件套）
- 在 `CMakeLists.txt` 添加 5 个 `option(ENABLE_DRIVER_*)` + 源文件条目 + `add_definitions(-DHAVE_DRIVER_*)`
- 在 `libsigrok/hwdriver.c` 添加 5 个 extern 声明 + drivers_list 注册项
- cmake 重新配置启用 5 个新驱动 + ninja 全量编译验证（与 batch1/batch2 共 17 个驱动统一编译）

### 5 个驱动清单（按难度从低到高）

| 序号 | 驱动 | 通信类型 | 代码行数 | 特殊API数 | 设备类型 |
|------|------|----------|----------|-----------|----------|
| 1 | atorch | Serial | 447 | 3 (feed_queue_analog, sr_sw_limits, sr_serial_extract) | DC 电源/负载计 |
| 2 | bkprecision-1856d | Serial | 535 | 2 (sr_sw_limits, new_analog_struct) | 频率计 |
| 3 | serial-lcr | Serial | 621 | 2 (sr_sw_limits, new_analog_struct) | LCR 表 |
| 4 | gwinstek-gpd | Serial | 643 | 2 (sr_sw_limits, new_analog_struct) | 可编程电源 |
| 5 | scpi-dmm | SCPI+Serial | 1490 | 2 (sr_sw_limits, new_analog_struct) | SCPI 万用表 |

### 14 条统一迁移转换规则（Batch 1/2 已验证，Batch 3 复用）

1. include 替换：`#include "libsigrok-internal.h"` → `#include "compat.h"`
2. df_header/end 2-arg：`std_session_send_df_header(sdi, prefix)` → `std_session_send_df_header(sdi, prefix, NULL)`
3. frame_begin 保持调用：compat 层 `compat_helpers.c` 提供单一实现，直接调用不本地定义
4. scan_complete_compat：`std_scan_complete(di, devices)` → `std_scan_complete_compat(di, devices)`
5. di->context → di->priv：`struct drv_context *drvc = di->context` → `di->priv`
6. sr_dev_inst_new：保持 5-arg（compat 层支持）
7. 移除 SR_REGISTER_DEV_DRIVER：用 `extern struct sr_dev_driver xxx_driver_info;` + 手动赋值
8. 结构体字段清理：移除 `SR_PRIV struct sr_dev_driver xxx_driver_info = {}` 内的 `dev_mode_list` 等不存在字段（用 compat 默认值）
9. 8 个 compat 包装函数：init/cleanup/scan/config_get/config_set/config_list/dev_acquisition_start/dev_acquisition_stop
10. config_channel_set 合并：多 channel config 合并为单一 config_set
11. local std_*_idx helper：如需 `std_u64_idx`/`std_str_idx` 在 api.c 内 static 定义
12. sr_config_set_compat：替代 `sr_config_set`
13. driver_info 命名：`xxx_driver_info`，8 个 compat wrapper 引用 `xxx_drv_ptr`
14. session_source 5-arg：`sr_session_source_add(sdi->session, fd, events, timeout, cb, cb_data)` 保持 5-arg（无 session 首参）

### 各驱动特殊适配点

**atorch**（参考 rdtech-um 的 feed_queue_analog 用法）：
- `feed_queue_analog_alloc/submit_one/flush/free` 本地 static 实现（PXView 不提供此 API）
- `sr_sw_limits` 在 protocol.h 内 static inline 定义（带 guard 宏防重复）
- `sr_serial_extract_options` 不可用 → scan 手动遍历 options 解析 SR_CONF_CONN/SR_CONF_SERIALCOMM
- `g_usleep` 可直接使用

**bkprecision-1856d / serial-lcr / gwinstek-gpd**（参考 hp-3457a 的扁平 analog 适配）：
- `new_analog_struct`（sr_analog_init/encoding/meaning/spec）→ PXView 旧版扁平 `struct sr_datafeed_analog`
- 适配：`analog.meaning->mq` → `analog.mq`，`analog.meaning->unit` → `analog.unit`，`analog.encoding->unit_pitch` → `analog.unit_pitch`，移除 `sr_analog_init()` 调用
- `sr_sw_limits` protocol.h 内 static inline
- `SR_CONF_CONTINUOUS`/`SR_CONF_MEASURED_QUANTITY` 等 PXView 缺失宏需 guard 定义

**scpi-dmm**（参考 hp-3457a/hp-3478a 的 SCPI 适配）：
- `sr_scpi_scan(di->context, ...)` → `sr_scpi_scan((struct drv_context *)di->priv, ...)`
- `sr_scpi_open/close/send/read` 直接可用
- `sr_scpi_source_add` 保持 session 参数（不受 5-arg 规则约束）
- `new_analog_struct` 适配扁平 analog 结构（同上）
- `SR_REGISTER_DEV_DRIVER` 移除

## Impact

- Affected specs:
  - `migrate-simple-serial-drivers-batch1`：本 spec 完成其剩余 Task 5-8（4 个驱动）
  - `migrate-simple-serial-drivers-batch2`：与 batch2 共同编译验证（17 个驱动统一启用）
  - `migrate-all-sigrok-drivers`：总进度推进，剩余 9 个驱动（greatfet/gmc-mh-1x-2x/juntek-jds6600 等）
- Affected code:
  - `CMakeLists.txt`：新增 5 个 option + add_definitions + 源文件 list（line 656/863/1271 后插入）
  - `libsigrok/hwdriver.c`：新增 5 个 extern + drivers_list 项（line 286/480 后插入）
  - `libsigrok/hardware/<name>/`：5 个新目录，每个含 protocol.h/protocol.c/api.c

## ADDED Requirements

### Requirement: Batch 3 驱动迁移
系统 SHALL 提供 5 个新 compat 驱动：atorch、bkprecision-1856d、serial-lcr、gwinstek-gpd、scpi-dmm，每个驱动创建 protocol.h/protocol.c/api.c 三件套，套用 14 条转换规则。

#### Scenario: 迁移完成
- **WHEN** 5 个驱动的三件套文件创建完成
- **THEN** 每个驱动的 driver_info 名称正确（atorch_driver_info/bkprecision_1856d_driver_info/serial_lcr_driver_info/gwinstek_gpd_driver_info/scpi_dmm_driver_info）
- **AND** 每个驱动包含 8 个 compat 包装函数 + driver_info 结构体
- **AND** 无 SR_REGISTER_DEV_DRIVER 残留
- **AND** 无本地 std_session_send_df_frame_begin 定义（使用 compat 层单一实现）

#### Scenario: 编译验证通过
- **WHEN** CMakeLists.txt 和 hwdriver.c 注册 5 个新驱动
- **AND** cmake 重新配置启用 5 个驱动
- **THEN** `ninja -j 16` 编译成功
- **AND** PXView.exe 生成
- **AND** 无 multiple definition / undefined reference 错误
