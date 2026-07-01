# 简单 Serial/SCPI/USB 驱动批量迁移 Spec（Batch 2）

## Why

`migrate-simple-serial-drivers-batch1` spec 已完成 4 个驱动（conrad-digi-35-cpu/hp-59306a/colead-slm/icstation-usbrelay），剩 4 个（atorch/bkprecision-1856d/serial-lcr/gwinstek-gpd）待迁移。基于 Explore agent 对全部未迁移驱动的复杂度评估，识别出 **8 个新驱动**（不在 Batch 1 中）行数 ≤1150、通信类型为已支持后端（纯 Serial / 已可用 SCPI / 已可用 libusb），无 Linux-only / Bluetooth / hidapi 依赖，是下一轮低风险批量迁移的最佳候选。本 spec 复用 Batch 1 已验证的 14 条统一转换规则。

## What Changes

- 迁移 8 个新驱动到 `libsigrok/hardware/<name>/`（创建 protocol.h/protocol.c/api.c 三件套）
- 在 `CMakeLists.txt` 添加 8 个 `option(ENABLE_DRIVER_*)` + 源文件条目 + `add_definitions(-DHAVE_DRIVER_*)`
- 在 `libsigrok/hwdriver.c` 添加 8 个 extern 声明 + drivers_list 注册项
- cmake 重新配置启用 8 个新驱动 + ninja 全量编译验证（与 Batch 1 剩余 4 个一并验证）

### 8 个驱动清单（按难度从低到高）

| 序号 | 驱动 | 通信类型 | 代码行数 | 特殊处理 | 设备类型 |
|------|------|----------|----------|----------|----------|
| 1 | zketech-ebd-usb | Serial | 696 | 调用 frame_begin（compat 头已提供） | 电池放电测试仪 |
| 2 | arachnid-labs-re-load-pro | Serial | 730 | 调用 frame_begin | 电子负载 |
| 3 | asix-omega-rtm-cli | Serial | 749 | 0 | 功率计 |
| 4 | kecheng-kc-330b | USB-libusb | 771 | 0 | USB 空气质量计 |
| 5 | hp-3457a | SCPI+Serial | 846 | SCPI 后端已可用 | 万用表 |
| 6 | microchip-pickit2 | USB-libusb | 855 | 0 | 编程器 |
| 7 | hp-3478a | SCPI+Serial | 950 | SCPI 后端已可用 | 万用表 |
| 8 | cem-dt-885x | Serial | 1142 | 0 | 声级计 |

### 排除清单（本轮不迁移，原因记录）

| 驱动 | 原因 |
|------|------|
| dcttech-usbrelay (593) | USB-HID 需 hidapi，PXView 无 hidapi 库（需改造为 libusb 直接控制，参考 uni-t-dmm，工作量较大） |
| devantech-eth008 (978) | 裸 TCP socket，需确认 compat 层 TCP 后端，留待后续批次 |
| scpi-dmm (1490) | 行数过大，通用 SCPI 框架驱动，需单独评估 |
| gmc-mh-1x-2x (1857) | 行数过大 |
| greatfet (1865) | 行数过大 |
| juntek-jds6600 (1922) | 行数过大 |
| mooshimeter-dmm (2314) | Bluetooth 依赖，需先确认 `sr_bt_*` 后端可用性 |
| beaglelogic (577) | Linux-only，依赖 `/dev/beaglelogic` 内核节点 |
| baylibre-acme (1043) | Linux-only，依赖 `/sys/class/gpio` sysfs + I2C |

## Impact

- Affected specs:
  - `migrate-simple-serial-drivers-batch1`（本 spec 与其并行，Batch 1 剩余 4 个驱动由其继续）
  - `migrate-all-sigrok-drivers`（本 spec 推进其 Task 64/65/66/67/68/74 等子集）
- Affected code:
  - 新建 8 个驱动目录（`libsigrok/hardware/<name>/` 各 3 文件）
  - `CMakeLists.txt`（8 个 option + 源文件 + add_definitions 块）
  - `libsigrok/hwdriver.c`（8 个 extern + drivers_list 项）

## 迁移转换规则（与 Batch 1 一致，复用已验证模板）

基于 sipeed-slogic-analyzer / rdtech-um / serial-dmm / Batch 1 已迁移驱动验证的模板：

1. **include 替换**：`#include <libsigrok/libsigrok.h>` → `#include "hardware/compat/compat.h"`
2. **df_header/end**：`std_session_send_df_header(sdi)` → `(sdi, LOG_PREFIX)`（2-arg）
3. **frame_begin**：保持 `std_session_send_df_frame_begin(sdi)` 调用（compat 层 `compat_helpers.c:236` 已提供单一实现，**不本地定义**）
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

### Batch 2 特定注意事项

- **SCPI 驱动（hp-3457a / hp-3478a）**：参考 hameg-hmo / rigol-ds / siglent-sds 的 SCPI 适配模式，`sr_scpi_scan` 第 1 参改为 `(struct drv_context *)di->priv`，`sr_scpi_open/close/send` 直接可用
- **USB-libusb 驱动（kecheng-kc-330b / microchip-pickit2）**：参考 sipeed-slogic-analyzer / fx2lafw 的 libusb 适配模式，`sr_session_source_add` 用 5-arg，`libusb_fill_bulk_transfer` 第 7 参 `(void *)sdi` 显式 cast
- **frame_begin 调用驱动（zketech-ebd-usb / arachnid-labs-re-load-pro）**：compat 层 `compat_helpers.c:236` 已提供 `std_session_send_df_frame_begin`，直接调用即可，无需本地定义
- **`sr_serial_extract_options` 不可用**：所有 Serial 驱动 scan 函数需手动遍历 options GSList 解析 `SR_CONF_CONN` / `SR_CONF_SERIALCOMM`（参考 Batch 1 conrad-digi-35-cpu / hp-59306a 模式）
- **`sr_sw_limits_*` 不可用**：需在 protocol.h 内以 `static inline` 定义（参考 fluke-dmm / colead-slm 模式）
- **`std_dummy_dev_acquisition_start/stop` 不可用**：纯写/无采集流驱动用 no-op `return SR_OK`（参考 rohde-schwarz-sme-0x 模式）

## ADDED Requirements

### Requirement: 批量迁移 8 个简单驱动（Batch 2）
系统 SHALL 提供 8 个新驱动的 compat 迁移实现，每个驱动包含 protocol.h/protocol.c/api.c 三文件，套用统一 14 条转换规则。

#### Scenario: 编译验证
- **WHEN** 用户运行 `cmake -DENABLE_DRIVER_ZKETECH_EBD_USB=ON -DENABLE_DRIVER_ARACHNID_LABS_RE_LOAD_PRO=ON -DENABLE_DRIVER_ASIX_OMEGA_RTM_CLI=ON -DENABLE_DRIVER_KECHENG_KC_330B=ON -DENABLE_DRIVER_HP_3457A=ON -DENABLE_DRIVER_MICROCHIP_PICKIT2=ON -DENABLE_DRIVER_HP_3478A=ON -DENABLE_DRIVER_CEM_DT_885X=ON` 后 `ninja -j 16`
- **THEN** 8 个驱动全部编译通过，无 error，PXView.exe 链接成功，无 multiple definition / undefined reference

#### Scenario: 设备注册
- **WHEN** 8 个驱动启用并编译
- **THEN** hwdriver.c 的 drivers_list 包含 8 个新驱动项，每个由 `HAVE_DRIVER_*` 宏守卫

#### Scenario: 与 Batch 1 共存
- **WHEN** Batch 1 剩余 4 个驱动（atorch/bkprecision-1856d/serial-lcr/gwinstek-gpd）与本 Batch 2 的 8 个驱动同时启用
- **THEN** 12 个驱动全部编译链接成功，无符号冲突（`std_session_send_df_frame_begin` 单一实现保证）

## REMOVED Requirements

### Requirement: 无
本 spec 不移除任何现有功能，仅新增驱动。
