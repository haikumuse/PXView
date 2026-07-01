# Tasks

## 阶段 1：驱动源文件迁移（8 个驱动，可并行）

每个驱动迁移包含 4 步：① 创建 protocol.h（include compat.h，保留 struct/enum/声明）② 创建 protocol.c（套用转换规则）③ 创建 api.c（8 个 compat 包装 + driver_info + local helpers 如需）④ CMakeLists.txt + hwdriver.c 注册（option/源文件/HAVE 守卫/extern/list）

源驱动位置：`c:\Users\admin\Downloads\libsigrok-slogic-dev\src\hardware\<name>\`
目标位置：`c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hardware\<name>\`

参考模板：
- `libsigrok/hardware/sipeed-slogic-analyzer/`（最新 USB-libusb 迁移）
- `libsigrok/hardware/serial-dmm/`（Serial 框架）
- `libsigrok/hardware/rdtech-um/`（复杂 Serial + feed_queue_analog）
- `libsigrok/hardware/rigol-ds/`（SCPI 适配参考）
- `libsigrok/hardware/hameg-hmo/`（SCPI 适配参考）
- `libsigrok/hardware/colead-slm/`（Batch 1 已完成，Serial + sr_sw_limits 内联）
- `libsigrok/hardware/conrad-digi-35-cpu/`（Batch 1 已完成，只写驱动 no-op acquisition）

- [x] Task 1: 迁移 zketech-ebd-usb（696 行，Serial，0 特殊API，电池放电测试仪）✅
  - [x] 1.1 创建 protocol.h（include compat.h，保留 dev_context/函数声明；`sr_sw_limits` 内联定义如需）
  - [x] 1.2 创建 protocol.c（套用转换规则；调用 frame_begin 用 compat 层单一实现，不本地定义；本地 SR_PRIV `std_session_send_df_frame_end`）
  - [x] 1.3 创建 api.c（8 compat 包装 + driver_info `zketech_ebd_usb_driver_info`；scan 手动遍历 options 解析 SR_CONF_CONN；13 个 SR_CONF_* 宏定义）
  - [ ] 1.4 CMakeLists.txt 添加 option(ENABLE_DRIVER_ZKETECH_EBD_USB) + 源文件 + add_definitions(-DHAVE_DRIVER_ZKETECH_EBD_USB)；hwdriver.c 添加 extern + drivers_list 项（统一在 Task 9 处理）

- [x] Task 2: 迁移 arachnid-labs-re-load-pro（730 行，Serial，0 特殊API，电子负载）✅
  - [x] 2.1-2.3 完成（driver_info `arachnid_labs_re_load_pro_driver_info`；static `sr_session_send_meta` + static `std_session_send_df_frame_end`；13 个 SR_CONF_* 宏）
  - [ ] 2.4 CMakeLists.txt + hwdriver.c 注册（统一在 Task 9 处理）

- [x] Task 3: 迁移 asix-omega-rtm-cli（749 行，Serial，0 特殊API，功率计）✅
  - [x] 3.1-3.3 完成（driver_info `asix_omega_rtm_cli_driver_info`；本地 static feed_queue_logic 实现 alloc/submit_one/flush/free，因 PXView 不提供此 API；local `asix_omega_rtm_cli_sr_hexdump_new/free`；5-arg sr_session_source_add；callback `const struct sr_dev_inst *sdi`）
  - [ ] 3.4 CMakeLists.txt + hwdriver.c 注册（统一在 Task 9 处理）

- [x] Task 4: 迁移 kecheng-kc-330b（771 行，USB-libusb，0 特殊API，空气质量计）✅
  - [x] 4.1 创建 protocol.h（include compat.h，保留 dev_context/USB 常量/函数声明）
  - [x] 4.2 创建 protocol.c（套用转换规则；`libusb_fill_bulk_transfer` 第 7 参 `(void *)sdi` 显式 cast；`sr_session_source_add` 5-arg；callback 签名 `const struct sr_dev_inst *sdi`；const-cast `sdi->status`；local_std_str_idx + local_std_u64_tuple_idx）
  - [x] 4.3 创建 api.c（8 compat 包装 + driver_info `kecheng_kc_330b_driver_info`；scan 用 `sr_usb_find`）
  - [ ] 4.4 CMakeLists.txt + hwdriver.c 注册（统一在 Task 9 处理）

- [x] Task 5: 迁移 hp-3457a（846 行，SCPI+Serial，万用表）✅
  - [x] 5.1 创建 protocol.h（include compat.h，保留 dev_context/SCPI 函数声明；`SR_CONF_MEASURED_QUANTITY`/`SR_CONF_ADC_POWERLINE_CYCLES`/`SR_MQFLAG_FOUR_WIRE` 守卫定义）
  - [x] 5.2 创建 protocol.c（套用转换规则；PXView 旧版扁平 `sr_datafeed_analog`，无 sr_analog_init/encoding/meaning/spec）
  - [x] 5.3 创建 api.c（8 compat 包装 + driver_info `hp_3457a_driver_info`；scan 用 `sr_scpi_scan((struct drv_context *)di->priv, ...)`）
  - [ ] 5.4 CMakeLists.txt + hwdriver.c 注册（统一在 Task 9 处理）

- [x] Task 6: 迁移 microchip-pickit2（855 行，USB-libusb，编程器）✅
  - [x] 6.1-6.3 完成（driver_info `microchip_pickit2_driver_info`；local `sr_hexdump_new/free`，local_std_u64_idx，`sr_parse_probe_names` 内联替代）
  - [ ] 6.4 CMakeLists.txt + hwdriver.c 注册（统一在 Task 9 处理）

- [x] Task 7: 迁移 hp-3478a（950 行，SCPI+Serial，万用表）✅
  - [x] 7.1-7.3 完成（driver_info `hp_3478a_driver_info`；适配 PXView 旧版扁平 sr_datafeed_analog 结构，参考 hp-3457a/fluke-dmm 模式）
  - [ ] 7.4 CMakeLists.txt + hwdriver.c 注册（统一在 Task 9 处理）

- [x] Task 8: 迁移 cem-dt-885x（1142 行，Serial，0 特殊API，声级计）✅
  - [x] 8.1-8.3 完成（driver_info `cem_dt_885x_driver_info`；local_std_gvar_tuple_array_u64 用 GVariantBuilder 构造 "a(tt)"；local_std_str_idx）
  - [ ] 8.4 CMakeLists.txt + hwdriver.c 注册（统一在 Task 9 处理）

## 阶段 2：编译验证

- [ ] Task 9: cmake 重新配置启用 Batch 2 全部 8 个驱动
  - [ ] 9.1 运行 cmake 启用 8 个新驱动选项（`-DENABLE_DRIVER_ZKETECH_EBD_USB=ON -DENABLE_DRIVER_ARACHNID_LABS_RE_LOAD_PRO=ON -DENABLE_DRIVER_ASIX_OMEGA_RTM_CLI=ON -DENABLE_DRIVER_KECHENG_KC_330B=ON -DENABLE_DRIVER_HP_3457A=ON -DENABLE_DRIVER_MICROCHIP_PICKIT2=ON -DENABLE_DRIVER_HP_3478A=ON -DENABLE_DRIVER_CEM_DT_885X=ON`）
  - [ ] 9.2 确认 CMakeCache.txt 中 8 个选项均为 ON

- [ ] Task 10: ninja 全量编译验证
  - [ ] 10.1 运行 `cd build && ninja -j 16`
  - [ ] 10.2 若有编译错误，逐个修复（重点关注 SCPI 适配、libusb callback 签名、std_*_idx 签名、sr_sw_limits 内联、frame_begin 调用）
  - [ ] 10.3 确认最终链接成功（PXView.exe 生成，无 multiple definition / undefined reference）

# Task Dependencies

- [Task 1-8] 互相独立，可并行执行（8 个驱动迁移）
- [Task 9] depends on [Task 1-8]（全部迁移完成后才能配置）
- [Task 10] depends on [Task 9]

# 并行策略

- **阶段 1**：Task 1-8 全部并行（最多 5 个 sub-agent 同时运行，分两批）
  - Batch 2-A（最简单 4 个，纯 Serial）：Task 1/2/3/8
  - Batch 2-B（USB/SCPI 4 个）：Task 4/5/6/7
- **阶段 2**：Task 9 → Task 10 串行收尾
- **与 Batch 1 关系**：本 spec 与 Batch 1 剩余 4 个驱动迁移可并行进行（不同驱动目录，无文件冲突）；最终编译验证可合并（12 个驱动同时启用）
