# Checklist

## 驱动源文件迁移验证

- [ ] atorch 三件套创建完成（protocol.h/protocol.c/api.c），driver_info 名称为 `atorch_driver_info`
- [ ] atorch 包含本地 static `feed_queue_analog` 实现（alloc/submit_one/flush/free），参考 rdtech-um 模板
- [ ] atorch 的 `sr_sw_limits` 在 protocol.h 内 static inline 定义（带 guard 宏防重复定义）
- [ ] atorch scan 函数手动遍历 options 解析 SR_CONF_CONN/SR_CONF_SERIALCOMM（不使用 sr_serial_extract_options）

- [ ] bkprecision-1856d 三件套创建完成，driver_info 名称为 `bkprecision_1856d_driver_info`
- [ ] bkprecision-1856d 的 `new_analog_struct` 已适配为 PXView 扁平 `struct sr_datafeed_analog`（analog.mq/analog.unit 直接赋值，无 sr_analog_init/encoding/meaning/spec）

- [ ] serial-lcr 三件套创建完成，driver_info 名称为 `serial_lcr_driver_info`
- [ ] serial-lcr 与已迁移 serial-dmm 框架一致，`new_analog_struct` 适配扁平 analog

- [ ] gwinstek-gpd 三件套创建完成，driver_info 名称为 `gwinstek_gpd_driver_info`
- [ ] gwinstek-gpd 的 `new_analog_struct` 已适配扁平 analog

- [ ] scpi-dmm 三件套创建完成，driver_info 名称为 `scpi_dmm_driver_info`
- [ ] scpi-dmm 的 `sr_scpi_scan` 调用为 `sr_scpi_scan((struct drv_context *)di->priv, ...)`（不是 di->context）
- [ ] scpi-dmm 的 `sr_scpi_source_add` 保持 session 参数（不受 5-arg session_source 规则约束）
- [ ] scpi-dmm 无 `SR_REGISTER_DEV_DRIVER` 残留（已移除，改为 extern + 手动 driver_info 赋值）
- [ ] scpi-dmm 的 `new_analog_struct` 已适配扁平 analog

## 转换规则合规性

- [ ] 所有 5 个驱动的 include 替换为 `#include "compat.h"`（不是 libsigrok-internal.h）
- [ ] 所有 5 个驱动的 `std_session_send_df_frame_begin` 调用 compat 层单一实现（不本地定义）
- [ ] 所有 5 个驱动的 `std_session_send_df_header(sdi, prefix, NULL)` 使用 3-arg（含 NULL terminator）
- [ ] 所有 5 个驱动使用 `std_scan_complete_compat(di, devices)`（不是 std_scan_complete）
- [ ] 所有 5 个驱动无 `SR_REGISTER_DEV_DRIVER` 宏
- [ ] 所有 5 个驱动包含 8 个 compat 包装函数（init/cleanup/scan/config_get/config_set/config_list/dev_acquisition_start/dev_acquisition_stop）
- [ ] 所有 5 个驱动的 `sr_session_source_add` 使用 5-arg（无 session 首参）

## CMake + hwdriver 注册验证

- [ ] CMakeLists.txt 包含 5 个 `option(ENABLE_DRIVER_ATORCH/.../SCPI_DMM)`
- [ ] CMakeLists.txt 包含 5 个 `add_definitions(-DHAVE_DRIVER_*)`
- [ ] CMakeLists.txt 包含 5 个源文件 list 条目
- [ ] hwdriver.c 包含 5 个 `extern SR_PRIV struct sr_dev_driver *_driver_info;`（在 HAVE 守卫内）
- [ ] hwdriver.c 的 drivers_list 数组包含 5 个 `&*_driver_info,`（在 HAVE 守卫内）

## 编译验证

- [ ] cmake 重新配置后 CMakeCache.txt 中 5 个 ENABLE_DRIVER_* 均为 ON
- [ ] `ninja -j 16` 编译成功，无错误
- [ ] PXView.exe 成功生成
- [ ] 无 multiple definition 错误（特别检查 std_session_send_df_frame_end 本地定义是否为 static）
- [ ] 无 undefined reference 错误（特别检查 feed_queue_analog、sr_sw_limits、SCPI 函数）
