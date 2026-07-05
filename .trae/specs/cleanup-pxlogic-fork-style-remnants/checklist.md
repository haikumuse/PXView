# Checklist

## Phase 1 验证

- [ ] `pxlogic.h` 中 `safe_free` 宏已删除（grep `safe_free` 在 pxlogic 目录无匹配）
- [ ] `pxlogic.h` 中 `SR_AC_COUPLING` / `SR_PKT_OK` / `DS_CONF_DSO_VDIVS` 宏已删除
- [ ] `pxlogic.c` 中 `sr_dslogic_option_value_to_code2()` 函数已删除
- [ ] `pxlogic.c` 中 `dev_destroy()` 函数已删除
- [ ] `pxlogic.c` 中 `static struct pxlogic_trigger pxlogic_trigger_cfg` 已删除
- [ ] `pxlogic.c` 中 `sr_info("trigger_pos = %d", pxlogic_trigger_cfg.trigger_pos)` 死读取已删除
- [ ] `pxlogic.h` 中 `struct pxlogic_trigger` 定义已删除
- [ ] `pxlogic.h` 中 `PXLOGIC_TRIGGER_STAGES` / `PXLOGIC_MAX_TRIGGER_PROBES` 宏已删除
- [ ] `pxlogic.c` `config_set` 的 `SR_CONF_DEVICE_MODE` 中 DSO/ANALOG 分支已删除
- [ ] `pxlogic.c` 中 `DSLogic_dev_new` 已重命名为 `pxlogic_dev_new`
- [ ] `pxlogic.c` 中 `sr_info("DSLogic_dev_new")` 字符串已改为 `"pxlogic_dev_new"`
- [ ] `pxlogic.c` 中 `sci_adjust_probes` 已重命名为 `pxlogic_adjust_probes`
- [ ] `pxlogic.h` 中 `enum DSLOGIC_OPERATION_MODE2` 已合并入 `enum pxlogic_operation_mode`
- [ ] `pxlogic.h` 中 `DS_MAX_TRIG_PERCENT` 已重命名为 `PX_TRIG_MAX_PERCENT`
- [ ] `pxlogic.c` 中所有 `min(a,b)` / `max(a,b)` 调用已改为 `MIN(a,b)` / `MAX(a,b)`
- [ ] `pxlogic.h` 中 `min(a,b)` / `max(a,b)` 自定义宏已删除
- [ ] `pxlogic.c` 中 `set_trigger_pos()` 函数已删除
- [ ] `pxlogic.c` 中 `SR_DF_TRIGGER` payload 已改用 `std_session_send_df_trigger(sdi)`
- [ ] `pxlogic.c` `scan()` 末尾已改为 `return std_scan_complete(di, devices);`
- [ ] `pxlogic.c` `config_list` 已添加 `SR_CONF_TRIGGER_MATCH` 入口，返回 5 个 `SR_TRIGGER_*` 值
- [ ] `pxlogic.c` 中 `pxlogic_option_value_to_code()` 函数已删除，调用点改用 `std_str_idx`
- [ ] `cd build && ninja -j 16` 编译成功
- [ ] `ninja install` 安装成功
- [ ] PXView headless 模式启动正常，MCP 17 tools 响应正常

## Phase 2 验证

- [ ] `pxlogic.c` `config_get` / `config_set` 中 `SR_CONF_VLD_CH_NUM` case 已删除
- [ ] `pxlogic.c` 中 `SR_CONF_INSTANT` / `SR_CONF_STREAM` / `SR_CONF_TEST` / `SR_CONF_LOOP_MODE` case 已删除
- [ ] `pxlogic.c` 中 `SR_CONF_MAX_HEIGHT` / `MAX_HEIGHT_VALUE` case 已删除
- [ ] `pxlogic.c` 中 `SR_CONF_DISK_CACHE_ENABLE` / `DISK_CACHE_PATH` / `STREAM_BUFF` / `STREAM_MEM_BUFF` case 已删除
- [ ] `pxlogic.c` 中 `SR_CONF_HW_DEPTH` / `USB_SPEED` / `USB30_SUPPORT` case 已删除
- [ ] `pxlogic.h` `hwoptions[]` / `sessions[]` 数组中已移除 `SR_CONF_PWM0_*` / `PWM1_*`（保留 case 实现但不公开）
- [ ] `pxlogic.c` `config_set` 的 `SR_CONF_OPERATION_MODE` 已用 `std_str_idx` 校验
- [ ] `pxlogic.c` `config_set` 的 `SR_CONF_CHANNEL_MODE` 已用 `std_str_idx` 校验
- [ ] `pxlogic.c` `config_set` 的 `SR_CONF_MAX_HEIGHT` 已用 `std_str_idx` 校验
- [ ] `pxlogic.c` `config_set` 的 `SR_CONF_LIMIT_SAMPLES` / `SAMPLERATE` 已用 `std_u64_idx` 校验
- [ ] `pxlogic.h` 已定义 `scanopts[]` / `drvopts[]` / `devopts[]` 数组（带 GET|SET|LIST 能力位）
- [ ] `pxlogic.c` `config_list` 公共 case 已替换为 `STD_CONFIG_LIST` 宏
- [ ] `pxlogic.h` 中 `SR_TH_3V3` / `SR_TH_5V0` 已删除
- [ ] `pxlogic.h` 中 `SR_FILTER_NONE` / `SR_FILTER_1T` 已删除，`SR_CONF_FILTER` 改用字符串
- [ ] `pxlogic.h` 中 `SR_TEST_*` 已删除，改用 `SR_CONF_TEST_MODE` (boolean)
- [ ] `pxlogic.h` 中 `LOGIC` / `DSO` / `ANALOG` #define 已删除，代码改用 `PXLOGIC_MODE_*`
- [ ] `cd build && ninja -j 16` 编译成功
- [ ] `ninja install` 安装成功
- [ ] PXView headless 模式 MCP 工具响应正常
- [ ] demo 设备采集正常（无 fork key 错误日志）

## Phase 3 验证

- [ ] `pxlogic.h` 中 `struct sr_list_item` 定义已删除
- [ ] `pxlogic.c` 中 `opmode_list[]` / `filter_list[]` / `extern_trigger_matches[]` / `channel_mode_list[]` 已改为 `const char *[]`
- [ ] `pxlogic.c` `config_list` 的 `SR_CONF_OPERATION_MODE` / `EX_TRIGGER_MATCH` / `CHANNEL_MODE` / `FILTER` 已改用 `g_variant_new_strv`
- [ ] `pxlogic.c` 中 `static struct sr_list_item channel_mode_list[CHANNEL_MODE_LIST_LEN]` 静态可变缓冲已删除
- [ ] `PXView/pv/prop/binding/deviceoptions.cpp:348,355,386,393` cast uint64 指针路径已改为 `g_variant_get_strv`
- [ ] `PXView/pv/dock/deviceoptionsdock.cpp:360-361` 同上
- [ ] `PXView/pv/dsvdef.h` 中 `struct sr_list_item` stub 定义已删除
- [ ] `pxlogic.h` 中 `struct lang_text_map_item` 定义已删除
- [ ] `pxlogic.c` 中 `lang_text_map[]` 静态表已删除
- [ ] `pxlogic.c` 中 `channel_mode_cn_map[]` 静态表已删除
- [ ] View 层 Qt 翻译文件（lang/*.json）已补齐 operation mode / filter / channel mode 中文显示
- [ ] `pxlogic.c` `setup_probes()` 已创建 "Logic" channel group 并 append 到 `sdi->channel_groups`
- [ ] View 层渲染逻辑已验证不依赖 channel_groups 分组（或保留兼容代码）
- [ ] `cd build && ninja -j 16` 编译成功（驱动 + View 同步）
- [ ] `ninja install` 安装成功
- [ ] PXView GUI 模式启动，deviceoptions sidebar 显示 operation mode / filter / channel mode 列表正常
- [ ] PXView headless 模式 MCP 工具响应正常
- [ ] demo 设备采集 + 触发功能正常
- [ ] PXLogic 硬件设备采集 + 触发功能正常（如有硬件可测）

## 硬件路径不变验证

- [ ] `pxlogic.c` 中 `receive_transfer()` / `submit_transfers()` USB 传输代码未修改
- [ ] `pxlogic.c` 中 `usb_wr_reg()` / `usb_rd_data_req()` FPGA 寄存器访问未修改
- [ ] `pxlogic.c` 中 `hw_dev_acquisition_start()` 采集启动流程未修改
- [ ] `set_trigger()` 中 `sr_session_trigger_get()` + `devc->capture_ratio` 路径未修改（本次会话前置任务已修复）
