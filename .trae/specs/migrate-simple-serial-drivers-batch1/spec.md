# 简单 Serial/SCPI 驱动批量迁移 Spec（Batch 1）

## Why

`migrate-all-sigrok-drivers` spec 规划了 80 个驱动迁移任务，但实际执行分散在多轮对话中，进度难以追踪。当前 PXView 已有 60 个 compat 驱动目录，但仍有 22 个未迁移。评估显示其中 8 个驱动代码量小（<600 行）、通信类型单一（纯 Serial 或已可用 SCPI 后端）、特殊 API ≤1，是低风险批量迁移的最佳起点。本 spec 聚焦这 8 个简单驱动，机械套用已验证的 rdtech-um/serial-dmm/sipeed-slogic 迁移模板。

## What Changes

- 迁移 8 个简单驱动到 `libsigrok/hardware/<name>/`（创建 protocol.h/protocol.c/api.c 三件套）
- 在 `CMakeLists.txt` 添加 8 个 `option(ENABLE_DRIVER_*)` + 源文件条目 + `add_definitions(-DHAVE_DRIVER_*)`
- 在 `libsigrok/hwdriver.c` 添加 8 个 extern 声明 + drivers_list 注册项
- cmake 重新配置启用 8 个新驱动 + ninja 全量编译验证

### 8 个驱动清单（按难度从低到高）

| 序号 | 驱动 | 通信类型 | 代码行数 | 特殊API数 | 设备类型 |
|------|------|----------|----------|-----------|----------|
| 1 | conrad-digi-35-cpu | Serial | 196 | 0 | 电源控制器 |
| 2 | hp-59306a | SCPI | 217 | 1 (di->context) | 继电器 |
| 3 | colead-slm | Serial | 337 | 0 | 声级计 |
| 4 | icstation-usbrelay | Serial | 349 | 0 | USB继电器(走serial) |
| 5 | atorch | Serial | 390 | 0 | 电源 |
| 6 | bkprecision-1856d | Serial | 488 | 1 (std_u64_idx) | 频率计 |
| 7 | serial-lcr | Serial | 559 | 0 | LCR表(同serial-dmm框架) |
| 8 | gwinstek-gpd | Serial | 575 | 0 | 电源 |

## Impact

- Affected specs: `migrate-all-sigrok-drivers`（本 spec 是其子集，推进 Task 70/75/76/62/51/35/74/49 等）
- Affected code:
  - 新建 8 个驱动目录（`libsigrok/hardware/<name>/` 各 3 文件）
  - `CMakeLists.txt`（8 个 option + 源文件 + add_definitions 块）
  - `libsigrok/hwdriver.c`（8 个 extern + drivers_list 项）

## 迁移转换规则（每个驱动统一套用）

基于 sipeed-slogic-analyzer / rdtech-um / serial-dmm 已验证的迁移模板：

1. **include 替换**：`#include <libsigrok/libsigrok.h>` → `#include "hardware/compat/compat.h"`
2. **df_header/end**：`std_session_send_df_header(sdi)` → `(sdi, LOG_PREFIX)`（2-arg）
3. **frame_begin**：保持 `std_session_send_df_frame_begin(sdi)` 调用（compat 层已提供单一实现，不本地定义）
4. **scan_complete**：`std_scan_complete(di, sdi)` → `std_scan_complete_compat(di, sdi)`
5. **di->context**：`di->context` → `di->priv`
6. **dev_inst**：`sr_dev_inst_user_new(vendor, model, NULL)` → `sr_dev_inst_new(SR_INST_USER, SR_ST_INACTIVE, vendor, model, NULL)`
7. **SR_REGISTER_DEV_DRIVER**：移除
8. **结构体字段**：移除 `.config_channel_set`/`.dev_clear`/`.dev_list`；`.context = NULL` → `.priv = NULL`；添加 `.dev_mode_list`/`.dev_destroy`/`.dev_status_get` compat 默认；添加 `.driver_type = DRIVER_TYPE_HARDWARE`
9. **8 个 compat 包装函数**：init/cleanup/scan/config_get/config_set/config_list/acquisition_start/acquisition_stop
10. **config_channel_set**：per-channel 逻辑合并进 config_set 包装（`ch != NULL` 分支）
11. **std_*_idx**：若使用 3-arg `std_u64_idx`/`std_i32_idx`/`std_str_idx`，添加 local helper（仿 hantek-dso/api.c:49-86）
12. **sr_config_set**：`sr_config_set(sdi, NULL, key, data)` → `sr_config_set_compat(sdi, NULL, key, data)`
13. **driver_info**：命名 `<name>_driver_info`，添加 `static struct sr_dev_driver *<name>_drv_ptr;` + extern 前向声明
14. **session_source**：`sr_session_source_add(sdi->session, ...)` → 5-arg（移除 session 首参）；callback 签名改为 `const struct sr_dev_inst *sdi`

## ADDED Requirements

### Requirement: 批量迁移 8 个简单驱动
系统 SHALL 提供 8 个新驱动的 compat 迁移实现，每个驱动包含 protocol.h/protocol.c/api.c 三文件，套用统一转换规则。

#### Scenario: 编译验证
- **WHEN** 用户运行 `cmake -DENABLE_DRIVER_CONRAD_DIGI_35_CPU=ON -DENABLE_DRIVER_HP_59306A=ON ... -DENABLE_DRIVER_GWINSTEK_GPD=ON` 后 `ninja -j 16`
- **THEN** 8 个驱动全部编译通过，无 error，PXView.exe 链接成功

#### Scenario: 设备注册
- **WHEN** 8 个驱动启用并编译
- **THEN** hwdriver.c 的 drivers_list 包含 8 个新驱动项，每个由 `HAVE_DRIVER_*` 宏守卫

## REMOVED Requirements

### Requirement: 无
本 spec 不移除任何现有功能，仅新增驱动。
