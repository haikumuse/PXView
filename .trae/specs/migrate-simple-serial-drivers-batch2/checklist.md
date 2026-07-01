# 验证清单

## Task 1: zketech-ebd-usb

- [ ] `libsigrok/hardware/zketech-ebd-usb/protocol.h` 创建（include compat.h，dev_context + 声明）
- [ ] `libsigrok/hardware/zketech-ebd-usb/protocol.c` 创建（套用转换规则，frame_begin 调用 compat 实现）
- [ ] `libsigrok/hardware/zketech-ebd-usb/api.c` 创建（8 compat 包装 + `zketech_ebd_usb_driver_info`）
- [ ] CMakeLists.txt 添加 `option(ENABLE_DRIVER_ZKETECH_EBD_USB)` + 源文件 + `add_definitions(-DHAVE_DRIVER_ZKETECH_EBD_USB)`
- [ ] hwdriver.c 添加 `extern SR_PRIV struct sr_dev_driver zketech_ebd_usb_driver_info;` + drivers_list 项（HAVE 守卫）

## Task 2: arachnid-labs-re-load-pro

- [ ] `libsigrok/hardware/arachnid-labs-re-load-pro/protocol.h` 创建
- [ ] `libsigrok/hardware/arachnid-labs-re-load-pro/protocol.c` 创建（frame_begin 调用 compat 实现）
- [ ] `libsigrok/hardware/arachnid-labs-re-load-pro/api.c` 创建（8 compat 包装 + `arachnid_labs_re_load_pro_driver_info`）
- [ ] CMakeLists.txt 添加 option + 源文件 + add_definitions
- [ ] hwdriver.c 添加 extern + drivers_list 项

## Task 3: asix-omega-rtm-cli

- [ ] `libsigrok/hardware/asix-omega-rtm-cli/protocol.h` 创建
- [ ] `libsigrok/hardware/asix-omega-rtm-cli/protocol.c` 创建
- [ ] `libsigrok/hardware/asix-omega-rtm-cli/api.c` 创建（8 compat 包装 + `asix_omega_rtm_cli_driver_info`）
- [ ] CMakeLists.txt 添加 option + 源文件 + add_definitions
- [ ] hwdriver.c 添加 extern + drivers_list 项

## Task 4: kecheng-kc-330b（USB-libusb）

- [ ] `libsigrok/hardware/kecheng-kc-330b/protocol.h` 创建（dev_context + USB 常量）
- [ ] `libsigrok/hardware/kecheng-kc-330b/protocol.c` 创建（`libusb_fill_bulk_transfer` 第 7 参 `(void *)sdi`；`sr_session_source_add` 5-arg；callback 签名 `const struct sr_dev_inst *sdi`）
- [ ] `libsigrok/hardware/kecheng-kc-330b/api.c` 创建（8 compat 包装 + `kecheng_kc_330b_driver_info`；scan 用 `sr_usb_find`）
- [ ] CMakeLists.txt 添加 option + 源文件 + add_definitions
- [ ] hwdriver.c 添加 extern + drivers_list 项

## Task 5: hp-3457a（SCPI）

- [ ] `libsigrok/hardware/hp-3457a/protocol.h` 创建（dev_context + SCPI 函数声明）
- [ ] `libsigrok/hardware/hp-3457a/protocol.c` 创建（SCPI 调用直接可用）
- [ ] `libsigrok/hardware/hp-3457a/api.c` 创建（8 compat 包装 + `hp_3457a_driver_info`；scan 用 `sr_scpi_scan((struct drv_context *)di->priv, ...)`）
- [ ] CMakeLists.txt 添加 option + 源文件 + add_definitions
- [ ] hwdriver.c 添加 extern + drivers_list 项

## Task 6: microchip-pickit2（USB-libusb）

- [ ] `libsigrok/hardware/microchip-pickit2/protocol.h` 创建
- [ ] `libsigrok/hardware/microchip-pickit2/protocol.c` 创建（USB-libusb 适配，同 Task 4 模式）
- [ ] `libsigrok/hardware/microchip-pickit2/api.c` 创建（8 compat 包装 + `microchip_pickit2_driver_info`）
- [ ] CMakeLists.txt 添加 option + 源文件 + add_definitions
- [ ] hwdriver.c 添加 extern + drivers_list 项

## Task 7: hp-3478a（SCPI）

- [ ] `libsigrok/hardware/hp-3478a/protocol.h` 创建
- [ ] `libsigrok/hardware/hp-3478a/protocol.c` 创建
- [ ] `libsigrok/hardware/hp-3478a/api.c` 创建（8 compat 包装 + `hp_3478a_driver_info`；SCPI 适配同 Task 5 模式）
- [ ] CMakeLists.txt 添加 option + 源文件 + add_definitions
- [ ] hwdriver.c 添加 extern + drivers_list 项

## Task 8: cem-dt-885x

- [ ] `libsigrok/hardware/cem-dt-885x/protocol.h` 创建
- [ ] `libsigrok/hardware/cem-dt-885x/protocol.c` 创建
- [ ] `libsigrok/hardware/cem-dt-885x/api.c` 创建（8 compat 包装 + `cem_dt_885x_driver_info`）
- [ ] CMakeLists.txt 添加 option + 源文件 + add_definitions
- [ ] hwdriver.c 添加 extern + drivers_list 项

## 构建集成验证

- [ ] CMakeCache.txt 中 8 个新驱动选项均为 ON
- [ ] hwdriver.c 中 8 个新驱动均由对应 `HAVE_DRIVER_*` 宏守卫
- [ ] CMakeLists.txt 中 8 个驱动的源文件条目（api.c + protocol.c）正确添加

## 编译验证

- [ ] `cd build && ninja -j 16` 编译成功，无 error
- [ ] 无 `multiple definition of 'std_session_send_df_frame_begin'` 错误（compat 层单一实现保证）
- [ ] 无 `undefined reference to 'sr_serial_extract_options'` 错误（手动遍历 options 替代）
- [ ] 无 `undefined reference to 'std_dummy_dev_acquisition_start/stop'` 错误（no-op 替代）
- [ ] PXView.exe 链接成功生成
- [ ] 启用 Batch 1 + Batch 2 共 12 个新驱动同时编译链接成功（共存验证）

## 设备注册验证

- [ ] PXView 启动后设备列表包含 8 个新驱动
- [ ] 每个新驱动的厂商/型号信息正确显示
