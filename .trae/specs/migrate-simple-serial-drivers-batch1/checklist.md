# 验证清单

## 驱动源文件创建

### conrad-digi-35-cpu
- [ ] `libsigrok/hardware/conrad-digi-35-cpu/protocol.h` 已创建（include compat.h）
- [ ] `libsigrok/hardware/conrad-digi-35-cpu/protocol.c` 已创建（套用转换规则）
- [ ] `libsigrok/hardware/conrad-digi-35-cpu/api.c` 已创建（8 compat 包装 + driver_info `conrad_digi_35_cpu_driver_info`）

### hp-59306a
- [ ] `libsigrok/hardware/hp-59306a/protocol.h` 已创建
- [ ] `libsigrok/hardware/hp-59306a/protocol.c` 已创建（di->context → di->priv）
- [ ] `libsigrok/hardware/hp-59306a/api.c` 已创建（SCPI 适配 + driver_info `hp_59306a_driver_info`）

### colead-slm
- [ ] `libsigrok/hardware/colead-slm/protocol.h` 已创建
- [ ] `libsigrok/hardware/colead-slm/protocol.c` 已创建
- [ ] `libsigrok/hardware/colead-slm/api.c` 已创建（driver_info `colead_slm_driver_info`）

### icstation-usbrelay
- [ ] `libsigrok/hardware/icstation-usbrelay/protocol.h` 已创建
- [ ] `libsigrok/hardware/icstation-usbrelay/protocol.c` 已创建
- [ ] `libsigrok/hardware/icstation-usbrelay/api.c` 已创建（driver_info `icstation_usbrelay_driver_info`）

### atorch
- [ ] `libsigrok/hardware/atorch/protocol.h` 已创建
- [ ] `libsigrok/hardware/atorch/protocol.c` 已创建（feed_queue_analog 适配）
- [ ] `libsigrok/hardware/atorch/api.c` 已创建（driver_info `atorch_driver_info`）

### bkprecision-1856d
- [ ] `libsigrok/hardware/bkprecision-1856d/protocol.h` 已创建
- [ ] `libsigrok/hardware/bkprecision-1856d/protocol.c` 已创建
- [ ] `libsigrok/hardware/bkprecision-1856d/api.c` 已创建（local_std_u64_idx helper + driver_info `bkprecision_1856d_driver_info`）

### serial-lcr
- [ ] `libsigrok/hardware/serial-lcr/protocol.h` 已创建
- [ ] `libsigrok/hardware/serial-lcr/protocol.c` 已创建
- [ ] `libsigrok/hardware/serial-lcr/api.c` 已创建（参照 serial-dmm 框架 + driver_info `serial_lcr_driver_info`）

### gwinstek-gpd
- [ ] `libsigrok/hardware/gwinstek-gpd/protocol.h` 已创建
- [ ] `libsigrok/hardware/gwinstek-gpd/protocol.c` 已创建
- [ ] `libsigrok/hardware/gwinstek-gpd/api.c` 已创建（driver_info `gwinstek_gpd_driver_info`）

## 构建集成

- [ ] `CMakeLists.txt` 已添加 8 个 option(ENABLE_DRIVER_*)
- [ ] `CMakeLists.txt` 已添加 8 个源文件条目（api.c + protocol.c）
- [ ] `CMakeLists.txt` 已添加 8 个 add_definitions(-DHAVE_DRIVER_*)
- [ ] `libsigrok/hwdriver.c` 已添加 8 个 extern 声明（由 HAVE_DRIVER_* 守卫）
- [ ] `libsigrok/hwdriver.c` 已在 drivers_list 添加 8 个驱动项

## 编译验证

- [ ] cmake 重新配置成功（8 个 ENABLE_DRIVER_* = ON）
- [ ] `ninja -j 16` 编译通过，无 error（仅允许 warning）
- [ ] 8 个驱动各自的 api.c / protocol.c 编译通过（.obj 产物生成）
- [ ] 最终链接成功（PXView.exe 生成，无 multiple definition / undefined reference 错误）
- [ ] 已有驱动（fx2lafw / sipeed-slogic / ipdbg-la 等）编译不受影响
