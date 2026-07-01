# Tasks

## 阶段 1：驱动源文件迁移（2 个驱动，可并行）

每个驱动迁移包含 4 步：① 创建 protocol.h（include compat.h，保留 struct/enum/声明；`sr_sw_limits` 内联定义如需）② 创建 protocol.c（套用转换规则；flat analog 适配如需）③ 创建 api.c（8 compat 包装 + driver_info + local helpers 如需）④ CMakeLists.txt + hwdriver.c 注册（option/源文件/HAVE 守卫/extern/list，统一在 Task 3 处理）

源驱动位置：`c:\Users\admin\Downloads\old\libsigrok\src\hardware\<name>\`
目标位置：`c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hardware\<name>\`

参考模板：
- `libsigrok/hardware/conrad-digi-35-cpu/`（Batch 1，std_dummy_dev_acquisition_start/stop 本地 no-op 实现）
- `libsigrok/hardware/hp-59306a/`（Batch 1，std_dummy + relay 设备）
- `libsigrok/hardware/colead-slm/`（Batch 1，本地 std_serial_dev_acquisition_stop 实现）
- `libsigrok/hardware/atorch/`（Batch 3，本地 dev_clear_with_callback 模式 + feed_queue_analog）
- `libsigrok/hardware/hp-3457a/`（Batch 2，SCPI + 扁平 sr_datafeed_analog 参考）
- `libsigrok/hardware/cem-dt-885x/`（Batch 2，local_std_str_idx + sr_sw_limits 内联）

- [ ] Task 1: 迁移 juntek-jds6600（1922 行，纯 Serial 信号发生器，1 driver_info）
  - [ ] 1.1 创建 protocol.h（include compat.h，保留 dev_context 含 device/waveforms/channel_config/quick_req；保留 jds6600_* 函数声明；无 sr_sw_limits）
  - [ ] 1.2 创建 protocol.c（套用转换规则；本地 `std_dummy_dev_acquisition_start` 返回 SR_OK no-op；本地 `std_dummy_dev_acquisition_stop` 返回 SR_OK no-op；本地 `dev_clear` 调用 `clear_helper(devc)` 释放 serial_number/names/fw_codes/quick_req 后调用 `std_dev_clear(driver)`）
  - [ ] 1.3 创建 api.c（8 compat 包装 + driver_info `juntek_jds6600_driver_info`；scan 手动遍历 options 解析 SR_CONF_CONN/SR_CONF_SERIALCOMM；`serial_write_blocking`/`serial_read_blocking` 直接调用 compat_serial.c 提供；无 sr_analog_init / feed_queue_analog）
  - [ ] 1.4 CMakeLists.txt 添加 option(ENABLE_DRIVER_JUNTEK_JDS6600) + 源文件 + add_definitions(-DHAVE_DRIVER_JUNTEK_JDS6600)；hwdriver.c 添加 extern + drivers_list 项（统一在 Task 3 处理）

- [ ] Task 2: 迁移 gmc-mh-1x-2x（1857 行，纯 Serial DMM，2 driver_info）
  - [ ] 2.1 创建 protocol.h（include compat.h，保留 dev_context；**枚举标签类型转换**：`enum sr_mq mq` → `int mq`、`enum sr_unit unit` → `int unit`、`enum sr_mqflag mqflags` → `uint64_t mqflags`（PXView 匿名枚举无标签类型，参考 atorch 验证）；保留 gmc_* 函数声明；`sr_sw_limits` static inline 定义带 `#ifndef SR_SW_LIMITS_H` guard 宏）
  - [ ] 2.2 创建 protocol.c（套用转换规则；`sr_analog_init(&analog, &encoding, &meaning, &spec, digits)` → `memset(&analog, 0, sizeof(analog))`；`analog.meaning->mq` → `analog.mq`；`analog.meaning->unit` → `analog.unit`；`analog.meaning->mqflags` → `analog.mqflags`；`analog.meaning->channels` → `analog.probes = g_slist_append(NULL, ch)`；`analog.encoding->digits` → `analog.unit_bits = 32`；移除 encoding/meaning/spec 局部变量；`serial_read_nonblocking`/`serial_write_blocking`/`serial_flush` 直接调用 compat_serial.c 提供）
  - [ ] 2.3 创建 api.c（2 套 8 compat 包装：`gmc_mh_1x_2x_rs232_*` + `gmc_mh_2x_bd232_*`（cleanup/dev_open/dev_close/dev_acquisition_start 可共享同名实现，config_get/config_set 共享，但 driver_info 结构体字段分别指向各自的 scan/config_list）；2 个 driver_info `gmc_mh_1x_2x_rs232_driver_info` + `gmc_mh_2x_bd232_driver_info`；本地 `std_serial_dev_acquisition_stop` 实现 serial_source_remove + serial_close + std_session_send_df_end 参考 colead-slm；scan 手动遍历 options；`serial_source_add(sdi->session, serial, G_IO_IN, 40, cb, sdi)` 5-arg 保持；`g_usleep` 直接使用）
  - [ ] 2.4 CMakeLists.txt 添加 option(ENABLE_DRIVER_GMC_MH_1X_2X) + 源文件 + add_definitions(-DHAVE_DRIVER_GMC_MH_1X_2X)；hwdriver.c 添加 2 个 extern + 2 个 drivers_list 项（统一在 Task 3 处理）

## 阶段 2：CMake + hwdriver 注册（19 个驱动 / 20 个 driver_info）

- [ ] Task 3: CMake + hwdriver 注册全部 19 个驱动（batch1+2+3+4 合计，覆盖 batch3 Task 6）
  - [ ] 3.1 CMakeLists.txt 在 line 657 后插入 19 个 `option(ENABLE_DRIVER_*)`（batch1: 4 + batch2: 8 + batch3: 5 + batch4: 2）
  - [ ] 3.2 CMakeLists.txt 在 line 867 后插入 19 个 `if(ENABLE_DRIVER_*) add_definitions(-DHAVE_DRIVER_*) endif()`
  - [ ] 3.3 CMakeLists.txt 在 line 1281 后插入 19 个 `if(ENABLE_DRIVER_*) list(APPEND libsigrok_SOURCES libsigrok/hardware/<name>/api.c libsigrok/hardware/<name>/protocol.c) endif()`（gmc-mh-1x-2x 的 2 个 driver_info 共用同一源文件 list）
  - [ ] 3.4 hwdriver.c 在 line 289 后插入 20 个 `#ifdef HAVE_DRIVER_* extern SR_PRIV struct sr_dev_driver *_driver_info; #endif`（gmc-mh-1x-2x 占 2 个：rs232 + bd232）
  - [ ] 3.5 hwdriver.c 在 line 486 后插入 20 个 `#ifdef HAVE_DRIVER_* &*_driver_info, #endif`（gmc-mh-1x-2x 占 2 个）

## 阶段 3：编译验证

- [ ] Task 4: cmake 重新配置启用 19 个驱动 + ninja 编译验证（覆盖 batch3 Task 7）
  - [ ] 4.1 运行 cmake 启用 19 个驱动选项（`-DENABLE_DRIVER_CONRAD_DIGI_35_CPU=ON -DENABLE_DRIVER_HP_59306A=ON -DENABLE_DRIVER_COLEAD_SLM=ON -DENABLE_DRIVER_ICSTATION_USBRELAY=ON -DENABLE_DRIVER_ZKETECH_EBD_USB=ON -DENABLE_DRIVER_ARACHNID_LABS_RE_LOAD_PRO=ON -DENABLE_DRIVER_ASIX_OMEGA_RTM_CLI=ON -DENABLE_DRIVER_KECHENG_KC_330B=ON -DENABLE_DRIVER_HP_3457A=ON -DENABLE_DRIVER_MICROCHIP_PICKIT2=ON -DENABLE_DRIVER_HP_3478A=ON -DENABLE_DRIVER_CEM_DT_885X=ON -DENABLE_DRIVER_ATORCH=ON -DENABLE_DRIVER_BKPRECISION_1856D=ON -DENABLE_DRIVER_SERIAL_LCR=ON -DENABLE_DRIVER_GWINSTEK_GPD=ON -DENABLE_DRIVER_SCPI_DMM=ON -DENABLE_DRIVER_JUNTEK_JDS6600=ON -DENABLE_DRIVER_GMC_MH_1X_2X=ON`）
  - [ ] 4.2 确认 CMakeCache.txt 中 19 个选项均为 ON
  - [ ] 4.3 运行 `cd build && ninja -j 16`
  - [ ] 4.4 若有编译错误，逐个修复（重点关注：juntek 的 std_dummy 本地实现、gmc 的 2 套 compat 包装、flat analog 适配、std_serial_dev_acquisition_stop 本地实现、sr_sw_limits 内联 guard 宏、serial_source_add 5-arg 签名）
  - [ ] 4.5 确认最终链接成功（PXView.exe 生成，无 multiple definition / undefined reference）

# Task Dependencies

- [Task 1-2] 互相独立，可并行执行（2 个驱动迁移）
- [Task 3] depends on [Task 1-2]（全部迁移完成后才能注册）
- [Task 4] depends on [Task 3]

# 并行策略

- **阶段 1**：Task 1-2 全部并行（最多 2 个 sub-agent 同时运行）
  - 优先级 1（最简单，无 sr_analog_init，仅 std_dummy no-op）：Task 1（juntek-jds6600）
  - 优先级 2（2 driver_info + flat analog + std_serial_dev_acquisition_stop）：Task 2（gmc-mh-1x-2x）
- **阶段 2**：Task 3 串行（CMake + hwdriver 注册 19 个驱动）
- **阶段 3**：Task 4 串行（cmake 配置 + ninja 编译，逐个修复错误）
- **与 Batch 1/2/3 关系**：本 spec 的 Task 3-4 统一处理 batch1+2+3+4 共 19 个驱动的注册与编译，覆盖 batch2 Task 9-10 和 batch3 Task 6-7（这两个 spec 的尾部任务由本 spec 完成后即视为完成）
- **与运行中 batch3 sub-agent 关系**：batch3 的 atorch/scpi-dmm/serial-lcr 3 个 sub-agent 仍在运行，不与本 spec 冲突（本 spec 处理不同驱动 juntek-jds6600/gmc-mh-1x-2x）；但 Task 3-4 的编译验证需等待 batch3 sub-agent 完成后再执行，否则编译会因 batch3 驱动文件不完整而失败
