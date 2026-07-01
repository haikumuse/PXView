# Tasks

> 进度图例：✅ 已完成 / 🔄 进行中 / ⏳ 待执行
> 
> **关键约束**：本 spec 不触碰 `tiered-driver-compat-fix` 正在修改的文件（compat 层 + 23 个 BROKEN 驱动），不执行编译。

## 阶段 1：驱动迁移（3 个，可并行）

源驱动位置：`C:\Users\admin\Downloads\libsigrok\src\hardware\<name>\`
目标位置：`c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hardware\<name>\`

参考模板：
- `libsigrok/hardware/conrad-digi-35-cpu/`（std_dummy_dev_acquisition_start/stop 本地 no-op）
- `libsigrok/hardware/colead-slm/`（本地 std_serial_dev_acquisition_stop）
- `libsigrok/hardware/atorch/`（本地 dev_clear_with_callback + 枚举标签类型转换）
- `libsigrok/hardware/gwinstek-gpd/`（扁平 analog + sr_sw_limits 内联）
- `libsigrok/hardware/hp-3457a/`（SCPI + 扁平 analog 参考）

- [x] Task 1: 迁移 serial-lcr（1490 行，纯 Serial LCR 表，1 driver_info）— 重做（batch3 失败）✅ 已完成（1674 行）
  - [x] 1.1 创建 protocol.h（266 行，含 lcr_info[] 变体表 6 型号 + 本地宏定义 + dev_context）
  - [x] 1.2 创建 protocol.c（917 行，扁平 analog + 本地 sr_session_send_meta + std_session_send_df_frame_end + serial_lcr_sr_atof_ascii + serial_lcr_dev_acquisition_stop + 内联 es51919/vc4080 解析器）
  - [x] 1.3 创建 api.c（491 行，8 compat 包装 + driver_info `serial_lcr_driver_info` + scan 手动遍历 options + serial_source_add 5-arg）

- [x] Task 2: 迁移 juntek-jds6600（1922 行，纯 Serial 信号发生器，1 driver_info）✅ 已完成（2390 行）
  - [x] 2.1 创建 protocol.h（124 行，含 dev_context + jds6600_* 函数声明 + 本地 ATTR_FMT_PRINTF 宏）
  - [x] 2.2 创建 protocol.c（1668 行，本地 std_dummy_dev_acquisition_start/stop no-op + clear_helper + jds6600_dev_clear + 完整协议实现）
  - [x] 2.3 创建 api.c（598 行，8 compat 包装 + driver_info `juntek_jds6600_driver_info` + 本地 jds6600_str_idx）

- [x] Task 3: 迁移 gmc-mh-1x-2x（1857 行，纯 Serial DMM，2 driver_info）✅ 已完成（2177 行）
  - [x] 3.1 创建 protocol.h（206 行，dev_context 枚举标签类型转换 int/int/uint64_t + sr_sw_limits static inline 带 #ifndef SR_SW_LIMITS_H guard 宏含 6 helper）
  - [x] 3.2 创建 protocol.c（1429 行，扁平 analog + 本地 gmc_mh_serial_timeout wrapper + gmc_mh_1x_2x_dev_acquisition_stop + const-sdi receive 回调）
  - [x] 3.3 创建 api.c（542 行，2 套 8 compat 包装 = 16 个 + 2 个 driver_info `gmc_mh_1x_2x_rs232_driver_info` + `gmc_mh_2x_bd232_driver_info` + scan 手动遍历 options + serial_source_add 5-arg）

## 阶段 2：CMake + hwdriver 注册（20 个驱动 / 21 个 driver_info）

- [x] Task 4: CMake + hwdriver 注册全部 20 个驱动 ✅ 已完成（311 行插入）
  - [x] 4.1 CMakeLists.txt 插入 19 个 `option(ENABLE_DRIVER_*)`（gmc-mh-1x-2x 共用 1 个 option）
  - [x] 4.2 CMakeLists.txt 插入 19 个 add_definitions 块（gmc-mh-1x-2x 块含 2 个 add_definitions，共 20 个 -DHAVE_DRIVER 宏）
  - [x] 4.3 CMakeLists.txt 插入 19 个 list 块（gmc-mh-1x-2x 为 1 个块含 api.c + protocol.c）
  - [x] 4.4 hwdriver.c 插入 20 个 extern 声明（gmc-mh-1x-2x 占 2 个 #ifdef 块）
  - [x] 4.5 hwdriver.c 插入 20 个 drivers_list 项（gmc-mh-1x-2x 占 2 个 #ifdef 块）
  - [x] 4.6 Grep 验证：19 option + 20 add_definitions + 19 list + 20 extern + 20 drivers_list 全部存在；gmc-mh-1x-2x 2 个 driver_info 均已注册

## 阶段 3：编译验证 — 推迟（不在本 spec 范围）

- [ ] Task 5: 编译验证 ⏳ 推迟到 tiered-driver-compat-fix 完成后
  - [ ] 5.1 确认 tiered-driver-compat-fix 全部 Task 完成（compat 层稳定）
  - [ ] 5.2 运行 cmake 启用 20 个驱动选项
  - [ ] 5.3 运行 `cd build && ninja -j 16`
  - [ ] 5.4 修复编译错误
  - [ ] 5.5 确认 PXView.exe 生成成功

# Task Dependencies

- [Task 1, 2, 3] 互相独立，可并行执行（3 个驱动迁移）
- [Task 4] depends on [Task 1, 2, 3]（全部迁移完成后才能注册）
- [Task 5] depends on [tiered-driver-compat-fix 全部完成] + [Task 4]（不在本 spec 范围）

# 并行策略

- **阶段 1**：Task 1-3 全部并行（最多 3 个 sub-agent 同时运行）
- **阶段 2**：Task 4 串行（CMake + hwdriver 注册 20 个驱动）
- **阶段 3**：Task 5 推迟（等 tiered-driver-compat-fix 完成）

# 与其他 spec 的关系

| spec | 关系 |
|---|---|
| `migrate-simple-serial-drivers-batch3` | serial-lcr 重做纳入本 spec Task 1，batch3 其他 4 个驱动已迁移完成 |
| `migrate-simple-serial-drivers-batch4` | juntek-jds6600 + gmc-mh-1x-2x 纳入本 spec Task 2-3，batch4 spec 视为被本 spec 取代 |
| `tiered-driver-compat-fix` | 本 spec 不触碰其修改范围（compat 层 + 23 个 BROKEN 驱动），编译推迟到其完成后 |
| `migrate-all-sigrok-drivers` | 主 spec，本 spec 完成后进度更新为 20 个驱动已迁移 |
