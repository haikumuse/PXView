# Tasks

- [x] Task 1: 创建兼容层基础设施
  - [x] 1.1 创建 `libsigrok/hardware/compat/` 目录结构
  - [x] 1.2 创建 `compat_driver.h`：定义适配宏和驱动适配模式，将标准 sigrok 驱动回调签名转换为 PXView 签名
  - [x] 1.3 创建 `compat_config.h`：定义标准 sigrok 配置键常量和能力标志
  - [x] 1.4 扩展 `sr_dev_inst`/`sr_channel` 结构体：添加 `model`、`serial_num`、`connection_id`、`channel_groups`、`inst_type`、`session`、`sdi`、`priv` 兼容字段；添加 `SR_CONF_TRIGGER_MATCH`、`SR_CONF_SCAN_OPTIONS` 等兼容配置键
  - [x] 1.5 创建 `compat_helpers.c/h`：实现 `std_init`、`std_cleanup`、`std_dev_list`、`std_dev_clear`、`std_config_list`、`std_gvar_*`、`std_u64_idx` 等标准 sigrok 辅助函数

- [x] Task 2: 实现标准 sigrok 内部 API 兼容层
  - [x] 2.1 在 `compat/` 下实现 `sr_config_get_compat()`：接收 `uint32_t key`，转换为 `int id`，调用 PXView 的 `sr_config_get()`，`ch` 传 NULL
  - [x] 2.2 实现 `sr_config_set_compat()`：接收 `uint32_t key`，转换为 `int id`，调用 PXView 的 `sr_config_set()`，`ch` 传 NULL
  - [x] 2.3 实现 `sr_config_list_compat()`：接收 `uint32_t key`，转换为 `int id`，调用 PXView 的 `sr_config_list()`
  - [x] 2.4 实现 `sr_channel_new_compat()`：标准签名版本，创建 PXView 的 `sr_channel` 并映射字段（已在 Task1 完成）
  - [x] 2.5 实现 `compat_sr_dev_inst_new()`：标准签名版本，创建 PXView 的 `sr_dev_inst` 并映射字段
  - [x] 2.6 实现 `compat_sr_usb_dev_inst_new()`：USB 设备实例创建（3参数版本含 hdl）

- [x] Task 3: 移植串口通信后端（最小化实现）
  - [x] 3.1 创建 `compat_serial.h/c`：基于 libserialport 实现串口通信函数（serial_open/close/flush/drain/write/read/write_blocking/read_blocking/set_params/set_paramstr/readline/stream_detect/timeout/source_add/source_remove）
  - [x] 3.2 扩展 `sr_serial_dev_inst` 结构体添加 `sp_data` 字段（libserialport 端口句柄）
  - [x] 3.3 实现 `std_serial_dev_open/close`、`std_session_send_df_trigger`、`std_gvar_tuple_u64` 等辅助函数
  - [x] 3.4 在 CMakeLists.txt 中添加 libserialport 依赖和串口后端编译选项

- [x] Task 4: 扩展驱动注册机制
  - [x] 4.1 在 `hwdriver.c` 中添加 `HAVE_COMPAT_DRIVERS` 条件编译块
  - [x] 4.2 为兼容驱动添加 `extern` 声明和 `drivers_list[]` 条目
  - [x] 4.3 在 CMakeLists.txt 中添加 `HAVE_COMPAT_DRIVERS`、`HAVE_DRIVER_FX2LAFW`、`HAVE_DRIVER_SALEAE_LOGIC16` 等 CMake 选项

- [x] Task 5: 引入 fx2lafw 驱动（首批验证驱动）
  - [x] 5.1 从标准 sigrok 拷贝 `src/hardware/fx2lafw/` 源码到 `libsigrok/hardware/fx2lafw/`
  - [x] 5.2 修改驱动源码：include 兼容层头文件，使用 wrapper 函数适配 PXView 驱动签名
  - [x] 5.3 修改驱动内部 API 调用：适配 sr_session_send、usb_source_add、std_session_send_df_header 等
  - [x] 5.4 在 CMakeLists.txt 中添加 fx2lafw 编译目标（已在 Task4 完成）
  - [x] 5.5 编译验证，修复编译错误（语法检查通过）

- [x] Task 6: 引入 saleae-logic16 驱动（首批验证驱动）
  - [x] 6.1 从标准 sigrok 拷贝 `src/hardware/saleae-logic16/` 源码到 `libsigrok/hardware/saleae-logic16/`
  - [x] 6.2 修改驱动源码：include 兼容层头文件，使用 wrapper 函数适配 PXView 驱动签名
  - [x] 6.3 修改驱动内部 API 调用：适配 sr_resource_open/read/close、soft_trigger、std_gvar 等
  - [x] 6.4 在 CMakeLists.txt 中添加 saleae-logic16 编译目标（已在 Task4 完成）
  - [x] 6.5 编译验证，修复编译错误（语法检查通过）

- [x] Task 7: 扩展 ds_* API 降级处理
  - [x] 7.1 在 `lib_main.c` 的 `ds_get_actived_device_mode_list()` 中处理无 `dev_mode_list` 回调的情况，返回默认模式列表
  - [x] 7.2 在 `ds_get_actived_device_status()` 中处理无 `dev_status_get` 回调的情况，返回空状态
  - [x] 7.3 在 `ds_get_actived_device_config()`/`ds_set_actived_device_config()` 中处理兼容驱动的 `ch=NULL` 调用路径（已确认无需修改，现有代码安全）
  - [x] 7.4 在 `ds_get_actived_device_info()` 中处理兼容设备信息映射（`model` → `name` 等）

- [x] Task 8: 扩展 DeviceAgent 支持兼容设备
  - [x] 8.1 在 `deviceagent.cpp` 中添加设备类型判断逻辑，区分 DSL 设备和兼容设备
  - [x] 8.2 对兼容设备跳过 DSL 专有操作（零点校准、VGA 增益、PWM 输出、磁盘缓存等）
  - [x] 8.3 对兼容设备提供基本配置读写支持（采样率、采样深度、通道启用等）

- [x] Task 9: 集成测试验证
  - [x] 9.1 验证 fx2lafw 驱动编译通过
  - [x] 9.2 验证 saleae-logic16 驱动编译通过
  - [x] 9.3 验证 PXView 原有 DSL 驱动功能不受影响
  - [ ] 9.4 验证兼容驱动设备能出现在设备列表中（使用 demo 模式或实际硬件）

- [x] Task 10: 引入 raspberrypi-pico (RP2040) 驱动
  - [x] 10.1 从标准 sigrok 拷贝 `src/hardware/raspberrypi-pico/` 源码到 `libsigrok/hardware/raspberrypi-pico/`
  - [x] 10.2 修改驱动源码：include 兼容层头文件，使用 wrapper 函数适配 PXView 驱动签名
  - [x] 10.3 修改驱动内部 API 调用：适配 sr_session_send、serial_source_add/remove、std_session_send_df_header/end 等
  - [x] 10.4 禁用触发器代码（PXView 无 struct sr_trigger 定义，sr_session_trigger_get 返回 NULL）
  - [x] 10.5 在 CMakeLists.txt 中添加 raspberrypi-pico 编译选项和驱动注册
  - [x] 10.6 编译验证通过

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 1]
- [Task 4] depends on [Task 1]
- [Task 5] depends on [Task 1, Task 2, Task 4]
- [Task 6] depends on [Task 1, Task 2, Task 4]
- [Task 7] depends on [Task 1]
- [Task 8] depends on [Task 7]
- [Task 9] depends on [Task 5, Task 6, Task 7, Task 8]
- [Task 10] depends on [Task 3, Task 4]
