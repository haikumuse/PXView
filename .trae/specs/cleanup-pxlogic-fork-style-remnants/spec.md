# 清理 PXLogic 驱动 fork-style 残留 Spec

## Why

PXLogic 驱动 (`libsigrok/src/hardware/pxlogic/`) 在 fork libsigrok 删除后仍保留大量 fork-style 代码：5 个 fork-only 结构体、17 个 fork 宏、5 个 fork 函数、16 个 fork-only config key、6 个全局可变状态。这些残留虽不阻塞功能（触发路径刚修复完），但与上游 `sr_*` API 规范存在三处偏离：

1. **API 表面不规范**：`config_list` 用 `g_variant_new_uint64((uintptr_t)&array)` 返回裸指针而非标准 GVariant 字符串数组，违反 libsigrok 0.6.0 ABI 契约
2. **死代码累积**：`safe_free` / `sr_dslogic_option_value_to_code2` / `dev_destroy` / `pxlogic_trigger_cfg` 等无调用者，维护成本高
3. **fork 命名残留**：`DSLogic_dev_new` / `sci_adjust_probes` / `DS_MAX_TRIG_PERCENT` 等 DS 前缀已无意义（DSL 硬件已弃用）

参考实现：`c:\Users\admin\Downloads\sigrok-git\libsigrok\src\hardware\scilogic\`（同硬件上游端口，已正确使用 `sr_session_trigger_get` / `std_str_idx` / `g_variant_new_strv` / `STD_CONFIG_LIST` 等标准 API）。

## What Changes

### Phase 1：低风险高回报（驱动单文件，零跨层）

#### 1.1 删除死代码

- 删除 `pxlogic.h` 中无调用宏：`safe_free`、`SR_AC_COUPLING`、`SR_PKT_OK`、`DS_CONF_DSO_VDIVS`
- 删除 `pxlogic.c` 中无调用函数：`sr_dslogic_option_value_to_code2()`、`dev_destroy()`
- 删除 `pxlogic.c` 中 `pxlogic_trigger_cfg` 静态全局 + 1497 行死读取 `sr_info("trigger_pos = %d", ...)`
- 删除 `pxlogic.h` 中 `struct pxlogic_trigger` 定义 + `PXLOGIC_TRIGGER_STAGES` / `PXLOGIC_MAX_TRIGGER_PROBES` 宏
- 删除 `pxlogic.c` 中 `SR_CONF_DEVICE_MODE` 的 DSO/ANALOG 死分支（仅 LOGIC 可达）

#### 1.2 重命名

- `DSLogic_dev_new` → `pxlogic_dev_new`（含 `sr_info("DSLogic_dev_new")` 字符串）
- `sci_adjust_probes` → `pxlogic_adjust_probes`
- `enum DSLOGIC_OPERATION_MODE2` 合并入 `enum pxlogic_operation_mode`
- `DS_MAX_TRIG_PERCENT` → `PX_TRIG_MAX_PERCENT`

#### 1.3 上游 API 替换

- `min/max` 自定义宏 → `MIN/MAX`（libsigrok-internal.h 已提供）
- `SR_DF_TRIGGER` 自定义 payload → `std_session_send_df_trigger(sdi)` 助手（删除整个 `set_trigger_pos()` 函数）
- `scan()` 末尾直接返回 → `std_scan_complete(di, devices)`
- `config_list` 添加 `SR_CONF_TRIGGER_MATCH` 入口（返回 `trigger_matches[]` i32 数组）
- `pxlogic_option_value_to_code()` → `std_str_idx(data, ARRAY_AND_SIZE(arr))`

### Phase 2：中风险（需应用层配合）

#### 2.1 fork-only config keys 清理

应用层 DeviceAgent 已有 `is_dsl_device()` 守卫拦截，pxlogic 驱动 case 删除：

- `SR_CONF_VLD_CH_NUM`（应用层迭代 `sdi->channels` 计算）
- `SR_CONF_INSTANT` / `SR_CONF_STREAM` / `SR_CONF_TEST` / `SR_CONF_LOOP_MODE`（应用层状态）
- `SR_CONF_MAX_HEIGHT` / `MAX_HEIGHT_VALUE`（应用层 UI 偏好）
- `SR_CONF_DISK_CACHE_ENABLE` / `DISK_CACHE_PATH` / `STREAM_BUFF` / `STREAM_MEM_BUFF`（应用层管理 — topic 已标记迁应用层）
- `SR_CONF_HW_DEPTH` / `USB_SPEED` / `USB30_SUPPORT`（由 `SR_CONF_LIMIT_SAMPLES` list 上限推导）
- `SR_CONF_PWM0_*` / `PWM1_*`（保留为 driver-private，移出 `hwoptions[]` / `sessions[]`）

#### 2.2 config_set 输入校验

- `OPERATION_MODE` / `CHANNEL_MODE` / `MAX_HEIGHT`：裸 `g_variant_get_int16` → `std_str_idx`
- `LIMIT_SAMPLES` / `SAMPLERATE`：裸 `g_variant_get_uint64` → `std_u64_idx`

#### 2.3 STD_CONFIG_LIST 宏化

- 定义 `scanopts[]` / `drvopts[]` / `devopts[]`（带 `SR_CONF_GET|SET|LIST` 能力位）
- `config_list` 公共 case 替换为 `STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts)`
- `config_list` 的 `SR_CONF_DEVICE_OPTIONS` / `SR_CONF_DEVICE_SESSIONS` 改为宏自动处理

#### 2.4 fork enum 清理

- `SR_TH_3V3` / `SR_TH_5V0` → 删除（`SR_CONF_VTH` 已用 double）
- `SR_FILTER_NONE` / `SR_FILTER_1T` → 字符串 `"none"` / `"1 clock"`
- `SR_TEST_*` → `SR_CONF_TEST_MODE` (boolean)
- `LOGIC` / `DSO` / `ANALOG` #define → 删除，直接用 `PXLOGIC_MODE_*`

### Phase 3：高风险 ABI 重构（单独排期，需 View 同步）

#### 3.1 `sr_list_item` → GVariant 字符串数组 **BREAKING**

- 删除 `struct sr_list_item` 定义
- `opmode_list[]` / `filter_list[]` / `extern_trigger_matches[]` / `channel_mode_list[]` 改为 `const char *[]`
- `config_list` 的 `SR_CONF_OPERATION_MODE` / `EX_TRIGGER_MATCH` / `CHANNEL_MODE` / `FILTER` 改用 `g_variant_new_strv`
- View 层 4 处 cast 指针同步改 `g_variant_get_strv`：
  - `PXView/pv/prop/binding/deviceoptions.cpp:348,355,386,393`
  - `PXView/pv/dock/deviceoptionsdock.cpp:360-361`
- 删除 `dsvdef.h:245` 中 `sr_list_item` stub 定义

#### 3.2 `lang_text_map` + `channel_mode_cn_map` 删除

- 删除 `struct lang_text_map_item`
- 删除 `lang_text_map[]` 静态表
- 删除 `channel_mode_cn_map[]` 静态表
- 中文映射交由 View 层 Qt `tr()` 系统处理

#### 3.3 `setup_probes` 引入 channel_groups

- 创建 `Logic` channel group 并 append 到 `sdi->channel_groups`
- 参考 scilogic/protocol.c:547-555 实现

## Impact

### 受影响代码

- `libsigrok/src/hardware/pxlogic/pxlogic.h`（~657 行，预计改动 ~150 行）
- `libsigrok/src/hardware/pxlogic/pxlogic.c`（~2118 行，预计改动 ~250 行）
- `PXView/pv/prop/binding/deviceoptions.cpp`（P3.1 影响 4 处）
- `PXView/pv/dock/deviceoptionsdock.cpp`（P3.1 影响 2 处）
- `PXView/pv/dsvdef.h`（P3.1 删除 `sr_list_item` stub）

### 受影响 spec

- `migrate-fork-config-keys-to-app-layer`（Phase 2.1 与之 Task 4-6 重叠）
- `dual-libsigrok-coexist-restore-features`（P3.1 删除 `sr_list_item` 影响 compat 层 stub）

### 硬件路径不变

Phase 1/2/3 全部不触动硬件操作代码路径：
- USB 传输：`receive_transfer` / `submit_transfers` 不变
- FPGA 寄存器：`usb_wr_reg` / `usb_rd_data_req` 不变
- 采集启动：`hw_dev_acquisition_start` 不变

## ADDED Requirements

### Requirement: PXLogic 驱动 API 表面对齐上游

PXLogic 驱动 SHALL 使用上游 libsigrok 0.6.0 标准 API，包括 `std_str_idx` / `std_u64_idx` / `STD_CONFIG_LIST` / `std_session_send_df_trigger` / `std_scan_complete` / `g_variant_new_strv` 等，禁止使用 fork-style 裸指针 cast 或自定义助手函数。

#### Scenario: config_list 返回标准 GVariant

- **WHEN** 应用层调用 `sr_config_list(SR_CONF_OPERATION_MODE, ...)`
- **THEN** 返回值 SHALL 为 `GVariant*` 字符串数组（`g_variant_new_strv`）
- **AND** SHALL NOT 为 `g_variant_new_uint64((uintptr_t)&array)` 裸指针

#### Scenario: config_set 输入校验

- **WHEN** 应用层调用 `sr_config_set(SR_CONF_OPERATION_MODE, ...)`
- **THEN** 驱动 SHALL 使用 `std_str_idx(data, ARRAY_AND_SIZE(arr))` 校验输入
- **AND** 越界值 SHALL 返回 `SR_ERR_ARG`

### Requirement: PXLogic 驱动命名规范化

PXLogic 驱动内所有公开 / 私有符号 SHALL 使用 `pxlogic_` 前缀，禁止 `DSLogic_` / `sci_` / `DS_` 等 fork 历史前缀。

#### Scenario: 函数命名

- **WHEN** 调用方引用 PXLogic 驱动的设备上下文构造函数
- **THEN** 函数名 SHALL 为 `pxlogic_dev_new()`
- **AND** SHALL NOT 为 `DSLogic_dev_new()`

## MODIFIED Requirements

### Requirement: PXLogic 硬件触发

PXLogic 硬件触发 SHALL 通过上游 `sr_session_trigger_get(sdi->session)` 读取触发配置（已于本次会话前置任务完成），后续 Phase 1.1 删除 `pxlogic_trigger_cfg` 静态全局 SHALL NOT 影响触发路径。

## REMOVED Requirements

### Requirement: fork-style `sr_list_item` API

**Reason**: 违反上游 libsigrok 0.6.0 ABI 契约（`config_list` 应返回 GVariant 字符串数组而非裸指针）
**Migration**: Phase 3.1 删除 `struct sr_list_item`，所有 `*_list[]` 改为 `const char *[]`，View 层 cast 指针路径改为 `g_variant_get_strv`

### Requirement: fork-style 中英双语映射表

**Reason**: 国际化应在 View 层 Qt `tr()` 系统处理，不应放在驱动层
**Migration**: Phase 3.2 删除 `lang_text_map[]` / `channel_mode_cn_map[]`，View 层补齐 Qt 翻译文件
