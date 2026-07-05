# Tasks

按"低风险高回报优先"原则分三个 Phase。Phase 1 完成并验证后再进入 Phase 2，Phase 2 完成后再进入 Phase 3。

## Phase 1：低风险高回报（驱动单文件，零跨层）

- [ ] Task 1: 删除死代码
  - [ ] SubTask 1.1: 删除 `pxlogic.h` 中无调用宏：`safe_free`、`SR_AC_COUPLING`、`SR_PKT_OK`、`DS_CONF_DSO_VDIVS`
  - [ ] SubTask 1.2: 删除 `pxlogic.c` 中 `sr_dslogic_option_value_to_code2()` 函数（无调用者）
  - [ ] SubTask 1.3: 删除 `pxlogic.c` 中 `dev_destroy()` 函数（driver struct 未引用）
  - [ ] SubTask 1.4: 删除 `pxlogic.c` 中 `pxlogic_trigger_cfg` 静态全局 + 1497 行 `sr_info("trigger_pos = %d", ...)` 死读取
  - [ ] SubTask 1.5: 删除 `pxlogic.h` 中 `struct pxlogic_trigger` 定义 + `PXLOGIC_TRIGGER_STAGES` / `PXLOGIC_MAX_TRIGGER_PROBES` 宏
  - [ ] SubTask 1.6: 删除 `pxlogic.c` `config_set` 的 `SR_CONF_DEVICE_MODE` 中 DSO/ANALOG 死分支（仅保留 LOGIC 分支）

- [ ] Task 2: 重命名 fork 前缀
  - [ ] SubTask 2.1: `DSLogic_dev_new` → `pxlogic_dev_new`（含 `sr_info("DSLogic_dev_new")` 字符串改为 `"pxlogic_dev_new"`）
  - [ ] SubTask 2.2: `sci_adjust_probes` → `pxlogic_adjust_probes`
  - [ ] SubTask 2.3: `enum DSLOGIC_OPERATION_MODE2` 合并入 `enum pxlogic_operation_mode`（pxlogic.h:74 已存在）
  - [ ] SubTask 2.4: `DS_MAX_TRIG_PERCENT` → `PX_TRIG_MAX_PERCENT`（pxlogic.h:106 + pxlogic.c:1477 一处使用）

- [ ] Task 3: 上游 API 替换
  - [ ] SubTask 3.1: `min(a,b)` / `max(a,b)` 自定义宏 → `MIN(a,b)` / `MAX(a,b)`（libsigrok-internal.h 已提供，pxlogic.h:66-71 删除 + pxlogic.c 所有调用点替换）
  - [ ] SubTask 3.2: `SR_DF_TRIGGER` 自定义 payload → `std_session_send_df_trigger(sdi)`（删除整个 `set_trigger_pos()` 函数，调用点改用助手）
  - [ ] SubTask 3.3: `scan()` 末尾 `return devices;` → `return std_scan_complete(di, devices);`
  - [ ] SubTask 3.4: `config_list` 添加 `SR_CONF_TRIGGER_MATCH` 入口，返回 `trigger_matches[]` i32 数组：`{ SR_TRIGGER_ZERO, SR_TRIGGER_ONE, SR_TRIGGER_RISING, SR_TRIGGER_FALLING, SR_TRIGGER_EDGE }`
  - [ ] SubTask 3.5: `pxlogic_option_value_to_code()` → 调用点改用 `std_str_idx(data, ARRAY_AND_SIZE(arr))`，函数本身删除

- [ ] Task 4: Phase 1 编译验证
  - [ ] SubTask 4.1: `cd build && ninja -j 16` 编译成功
  - [ ] SubTask 4.2: `ninja install` 安装成功
  - [ ] SubTask 4.3: 启动 PXView headless 模式验证 MCP 17 tools 响应正常

## Phase 2：中风险（需应用层配合）

- [ ] Task 5: fork-only config keys 清理
  - [ ] SubTask 5.1: 删除 `SR_CONF_VLD_CH_NUM` case（应用层 deviceoptionsdock.cpp / deviceoptions.cpp 已用 `sdi->channels` 长度 fallback）
  - [ ] SubTask 5.2: 删除 `SR_CONF_INSTANT` / `SR_CONF_STREAM` / `SR_CONF_TEST` / `SR_CONF_LOOP_MODE` case（应用层 DeviceAgent 守卫已拦截）
  - [ ] SubTask 5.3: 删除 `SR_CONF_MAX_HEIGHT` / `MAX_HEIGHT_VALUE` case（应用层 UI 偏好）
  - [ ] SubTask 5.4: 删除 `SR_CONF_DISK_CACHE_ENABLE` / `DISK_CACHE_PATH` / `STREAM_BUFF` / `STREAM_MEM_BUFF` case（应用层管理）
  - [ ] SubTask 5.5: 删除 `SR_CONF_HW_DEPTH` / `USB_SPEED` / `USB30_SUPPORT` case（由 `SR_CONF_LIMIT_SAMPLES` list 上限推导）
  - [ ] SubTask 5.6: `SR_CONF_PWM0_*` / `PWM1_*` 保留 case 但从 `hwoptions[]` / `sessions[]` 数组移除（不进公开 devopts）

- [ ] Task 6: config_set 输入校验
  - [ ] SubTask 6.1: `SR_CONF_OPERATION_MODE` 裸 `g_variant_get_int16` → `std_str_idx(data, ARRAY_AND_SIZE(logic_mode_str))`
  - [ ] SubTask 6.2: `SR_CONF_CHANNEL_MODE` 裸 `g_variant_get_int16` → `std_str_idx`
  - [ ] SubTask 6.3: `SR_CONF_MAX_HEIGHT` 裸 `g_variant_get_string` → `std_str_idx`
  - [ ] SubTask 6.4: `SR_CONF_LIMIT_SAMPLES` / `SAMPLERATE` 裸 `g_variant_get_uint64` → `std_u64_idx`

- [ ] Task 7: STD_CONFIG_LIST 宏化
  - [ ] SubTask 7.1: 定义 `scanopts[] = { SR_CONF_CONN }` 数组
  - [ ] SubTask 7.2: 定义 `drvopts[]` 数组（含 `SR_CONF_CONN` / `SR_CONF_USB` 等 scan 阶段选项）
  - [ ] SubTask 7.3: 定义 `devopts[]` 数组（带 `SR_CONF_GET|SET|LIST` 能力位，覆盖所有保留 key）
  - [ ] SubTask 7.4: `config_list` 公共 case（`SR_CONF_DEVICE_OPTIONS` / `DEVICE_SESSIONS` / `SCAN_OPTIONS`）替换为 `STD_CONFIG_LIST(key, data, sdi, cg, scanopts, drvopts, devopts)`

- [ ] Task 8: fork enum 清理
  - [ ] SubTask 8.1: 删除 `SR_TH_3V3` / `SR_TH_5V0`（`SR_CONF_VTH` 已用 double）
  - [ ] SubTask 8.2: `SR_FILTER_NONE` / `SR_FILTER_1T` → 字符串 `"none"` / `"1 clock"`，`config_set` 的 `SR_CONF_FILTER` 改用 `std_str_idx`
  - [ ] SubTask 8.3: `SR_TEST_*` → `SR_CONF_TEST_MODE` (boolean)
  - [ ] SubTask 8.4: 删除 `LOGIC` / `DSO` / `ANALOG` #define，直接用 `PXLOGIC_MODE_*`

- [ ] Task 9: Phase 2 编译验证
  - [ ] SubTask 9.1: `cd build && ninja -j 16` 编译成功
  - [ ] SubTask 9.2: `ninja install` 安装成功
  - [ ] SubTask 9.3: PXView headless 模式 MCP 工具响应正常
  - [ ] SubTask 9.4: 验证 demo 设备采集正常（无 fork key 错误日志）

## Phase 3：高风险 ABI 重构（需 View 同步编译）

- [ ] Task 10: `sr_list_item` → GVariant 字符串数组
  - [ ] SubTask 10.1: `opmode_list[]` 改为 `const char *[]`，`config_list` 返回 `g_variant_new_strv`
  - [ ] SubTask 10.2: `filter_list[]` 同上
  - [ ] SubTask 10.3: `extern_trigger_matches[]` 同上
  - [ ] SubTask 10.4: `channel_mode_list[]` 同上（移除 `static struct sr_list_item channel_mode_list[CHANNEL_MODE_LIST_LEN]` 静态可变缓冲）
  - [ ] SubTask 10.5: View 层 `deviceoptions.cpp:348,355,386,393` cast uint64 指针 → `g_variant_get_strv` 字符串数组
  - [ ] SubTask 10.6: View 层 `deviceoptionsdock.cpp:360-361` 同上
  - [ ] SubTask 10.7: 删除 `pxlogic.h` 中 `struct sr_list_item` 定义
  - [ ] SubTask 10.8: 删除 `PXView/pv/dsvdef.h:245` 中 `sr_list_item` stub 定义

- [ ] Task 11: `lang_text_map` + `channel_mode_cn_map` 删除
  - [ ] SubTask 11.1: 删除 `pxlogic.h` 中 `struct lang_text_map_item` 定义
  - [ ] SubTask 11.2: 删除 `pxlogic.c` 中 `lang_text_map[]` 静态表
  - [ ] SubTask 11.3: 删除 `pxlogic.c` 中 `channel_mode_cn_map[]` 静态表
  - [ ] SubTask 11.4: View 层补齐 Qt 翻译文件（lang/*.json）覆盖 operation mode / filter / channel mode 中文显示

- [ ] Task 12: `setup_probes` 引入 channel_groups
  - [ ] SubTask 12.1: 创建 `struct sr_channel_group`，命名为 "Logic"
  - [ ] SubTask 12.2: 将所有 LOGIC 通道 append 到 channel_group->channels
  - [ ] SubTask 12.3: append channel_group 到 `sdi->channel_groups`
  - [ ] SubTask 12.4: 验证 View 层不依赖 channel_groups 分组渲染（如依赖则保留兼容代码）

- [ ] Task 13: Phase 3 编译验证
  - [ ] SubTask 13.1: `cd build && ninja -j 16` 编译成功（驱动 + View 同步）
  - [ ] SubTask 13.2: `ninja install` 安装成功
  - [ ] SubTask 13.3: PXView GUI 模式启动，验证 deviceoptions sidebar 显示 operation mode / filter / channel mode 列表正常
  - [ ] SubTask 13.4: PXView headless 模式 MCP 工具响应正常
  - [ ] SubTask 13.5: 验证 demo 设备采集 + 触发功能正常

# Task Dependencies

- Task 2 (重命名) 依赖 Task 1 (删除死代码) 完成，避免重命名后立即删除造成混淆
- Task 3 (上游 API 替换) 依赖 Task 1、Task 2 完成，因为 `set_trigger_pos` 删除涉及 `min/max` 替换
- Task 4 (Phase 1 验证) 依赖 Task 1、2、3 全部完成
- Task 5-8 (Phase 2) 依赖 Task 4 通过，且与 `migrate-fork-config-keys-to-app-layer` spec 的 Task 4-6 协调（应用层 DeviceAgent 守卫已就位）
- Task 9 (Phase 2 验证) 依赖 Task 5、6、7、8 完成
- Task 10-12 (Phase 3) 依赖 Task 9 通过，且需要一次性同步编译驱动 + View
- Task 13 (Phase 3 验证) 依赖 Task 10、11、12 全部完成

# 并行机会

- Task 1 的 6 个 SubTask 互相独立，可并行
- Task 2 的 4 个重命名互相独立，可并行
- Task 3 的 5 个上游 API 替换互相独立，可并行
- Phase 2 中 Task 5、6、7、8 互相独立，可并行（但 Task 7 STD_CONFIG_LIST 依赖 Task 5 删除 fork key 后才能确定 devopts[] 内容）
