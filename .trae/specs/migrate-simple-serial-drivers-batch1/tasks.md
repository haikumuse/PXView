# Tasks

## 阶段 1：驱动源文件迁移（8 个驱动，可并行）

每个驱动迁移包含 4 步：① 创建 protocol.h（include compat.h，保留 struct/enum/声明）② 创建 protocol.c（套用转换规则）③ 创建 api.c（8 个 compat 包装 + driver_info + local helpers 如需）④ CMakeLists.txt + hwdriver.c 注册（option/源文件/HAVE 守卫/extern/list）

源驱动位置：`c:\Users\admin\Downloads\libsigrok-slogic-dev\src\hardware\<name>\`
目标位置：`c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hardware\<name>\`

参考模板：`libsigrok/hardware/sipeed-slogic-analyzer/`（最新迁移）、`libsigrok/hardware/serial-dmm/`（Serial 框架）、`libsigrok/hardware/rdtech-um/`（复杂 Serial）

- [ ] Task 1: 迁移 conrad-digi-35-cpu（196 行，Serial，0 特殊API，最简单）
  - [ ] 1.1 创建 protocol.h（include compat.h，保留 dev_context/函数声明）
  - [ ] 1.2 创建 protocol.c（套用转换规则，无特殊 API）
  - [ ] 1.3 创建 api.c（8 compat 包装 + driver_info `conrad_digi_35_cpu_driver_info`）
  - [ ] 1.4 CMakeLists.txt 添加 option(ENABLE_DRIVER_CONRAD_DIGI_35_CPU) + 源文件 + add_definitions(-DHAVE_DRIVER_CONRAD_DIGI_35_CPU)；hwdriver.c 添加 extern + drivers_list 项

- [ ] Task 2: 迁移 hp-59306a（217 行，SCPI，1 di->context，继电器）
  - [ ] 2.1 创建 protocol.h
  - [ ] 2.2 创建 protocol.c（di->context → di->priv）
  - [ ] 2.3 创建 api.c（8 compat 包装 + driver_info `hp_59306a_driver_info`；SCPI 后端已可用，参考 hameg-hmo/rigol-ds 的 SCPI 适配）
  - [ ] 2.4 CMakeLists.txt + hwdriver.c 注册

- [ ] Task 3: 迁移 colead-slm（337 行，Serial，0 特殊API，声级计）
  - [ ] 3.1-3.4 同上模式，driver_info `colead_slm_driver_info`

- [ ] Task 4: 迁移 icstation-usbrelay（349 行，Serial，0 特殊API，USB继电器走serial）
  - [ ] 4.1-4.4 同上模式，driver_info `icstation_usbrelay_driver_info`

- [ ] Task 5: 迁移 atorch（390 行，Serial，0 特殊API，电源，用 feed_queue_analog）
  - [ ] 5.1-5.4 同上模式，driver_info `atorch_driver_info`；参考 rdtech-um 的 feed_queue_analog 用法

- [ ] Task 6: 迁移 bkprecision-1856d（488 行，Serial，1 std_u64_idx，频率计）
  - [ ] 6.1-6.4 同上模式，driver_info `bkprecision_1856d_driver_info`；添加 local_std_u64_idx helper（仿 hantek-dso/api.c:62-73）

- [ ] Task 7: 迁移 serial-lcr（559 行，Serial，0 特殊API，LCR表）
  - [ ] 7.1-7.4 同上模式，driver_info `serial_lcr_driver_info`；与已迁移 serial-dmm 同框架，可直接参照其包装

- [ ] Task 8: 迁移 gwinstek-gpd（575 行，Serial，0 特殊API，电源）
  - [ ] 8.1-8.4 同上模式，driver_info `gwinstek_gpd_driver_info`

## 阶段 2：编译验证

- [ ] Task 9: cmake 重新配置启用全部 8 个驱动
  - [ ] 9.1 运行 cmake -DENABLE_DRIVER_CONRAD_DIGI_35_CPU=ON -DENABLE_DRIVER_HP_59306A=ON -DENABLE_DRIVER_COLEAD_SLM=ON -DENABLE_DRIVER_ICSTATION_USBRELAY=ON -DENABLE_DRIVER_ATORCH=ON -DENABLE_DRIVER_BKPRECISION_1856D=ON -DENABLE_DRIVER_SERIAL_LCR=ON -DENABLE_DRIVER_GWINSTEK_GPD=ON
  - [ ] 9.2 确认 CMakeCache.txt 中 8 个选项均为 ON

- [ ] Task 10: ninja 全量编译验证
  - [ ] 10.1 运行 `cd build && ninja -j 16`
  - [ ] 10.2 若有编译错误，逐个修复（重点关注 std_*_idx 签名、SCPI 适配、feed_queue_analog 等）
  - [ ] 10.3 确认最终链接成功（PXView.exe 生成，无 multiple definition / undefined reference）

# Task Dependencies

- [Task 1-8] 互相独立，可并行执行（8 个驱动迁移）
- [Task 9] depends on [Task 1-8]（全部迁移完成后才能配置）
- [Task 10] depends on [Task 9]

# 并行策略

- **阶段 1**：Task 1-8 全部并行（最多 5 个 sub-agent 同时运行，分两批）
  - Batch 1（最简单 4 个）：Task 1/2/3/4
  - Batch 2（稍复杂 4 个）：Task 5/6/7/8
- **阶段 2**：Task 9 → Task 10 串行收尾
