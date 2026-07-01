# Tasks

## 阶段 1：驱动源文件迁移（5 个驱动，可并行）

每个驱动迁移包含 4 步：① 创建 protocol.h（include compat.h，保留 struct/enum/声明；`sr_sw_limits` 内联定义如需）② 创建 protocol.c（套用转换规则；flat analog 适配）③ 创建 api.c（8 compat 包装 + driver_info + local helpers 如需）④ CMakeLists.txt + hwdriver.c 注册（option/源文件/HAVE 守卫/extern/list）

源驱动位置：`c:\Users\admin\Downloads\libsigrok-slogic-dev\src\hardware\<name>\`
目标位置：`c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrok\hardware\<name>\`

参考模板：
- `libsigrok/hardware/rdtech-um/`（feed_queue_analog 本地实现 + 复杂 Serial）
- `libsigrok/hardware/hp-3457a/`（SCPI 适配 + 扁平 sr_datafeed_analog）
- `libsigrok/hardware/hp-3478a/`（SCPI + 扁平 analog 参考）
- `libsigrok/hardware/serial-dmm/`（Serial 框架）
- `libsigrok/hardware/colead-slm/`（Batch 1 已完成，sr_sw_limits 内联）
- `libsigrok/hardware/cem-dt-885x/`（Batch 2 已完成，local_std_str_idx）

- [ ] Task 1: 迁移 atorch（447 行，Serial，3 特殊API，DC 电源/负载计）
  - [ ] 1.1 创建 protocol.h（include compat.h，保留 dev_context/函数声明；`sr_sw_limits` static inline 定义带 guard 宏）
  - [ ] 1.2 创建 protocol.c（套用转换规则；本地 static `feed_queue_analog` 实现 alloc/submit_one/flush/free 参考 rdtech-um；flat analog 直接初始化 analog.mq/analog.unit 字段）
  - [ ] 1.3 创建 api.c（8 compat 包装 + driver_info `atorch_driver_info`；scan 手动遍历 options 解析 SR_CONF_CONN/SR_CONF_SERIALCOMM；`g_usleep` 直接使用）
  - [ ] 1.4 CMakeLists.txt 添加 option(ENABLE_DRIVER_ATORCH) + 源文件 + add_definitions(-DHAVE_DRIVER_ATORCH)；hwdriver.c 添加 extern + drivers_list 项（统一在 Task 6 处理）

- [ ] Task 2: 迁移 bkprecision-1856d（535 行，Serial，2 特殊API，频率计）
  - [ ] 2.1 创建 protocol.h（include compat.h，保留 dev_context/函数声明；`sr_sw_limits` static inline）
  - [ ] 2.2 创建 protocol.c（套用转换规则；`new_analog_struct` 适配为 PXView 扁平 `struct sr_datafeed_analog`：`analog.meaning->mq` → `analog.mq`，移除 `sr_analog_init()`）
  - [ ] 2.3 创建 api.c（8 compat 包装 + driver_info `bkprecision_1856d_driver_info`；scan 手动遍历 options；local_std_u64_idx helper 如需）
  - [ ] 2.4 CMakeLists.txt + hwdriver.c 注册（统一在 Task 6 处理）

- [ ] Task 3: 迁移 serial-lcr（621 行，Serial，2 特殊API，LCR 表）
  - [ ] 3.1 创建 protocol.h（include compat.h，保留 dev_context/函数声明；`sr_sw_limits` static inline）
  - [ ] 3.2 创建 protocol.c（套用转换规则；`new_analog_struct` 适配扁平 analog；与已迁移 serial-dmm 同框架）
  - [ ] 3.3 创建 api.c（8 compat 包装 + driver_info `serial_lcr_driver_info`；scan 手动遍历 options）
  - [ ] 3.4 CMakeLists.txt + hwdriver.c 注册（统一在 Task 6 处理）

- [ ] Task 4: 迁移 gwinstek-gpd（643 行，Serial，2 特殊API，可编程电源）
  - [ ] 4.1 创建 protocol.h（include compat.h，保留 dev_context/函数声明；`sr_sw_limits` static inline）
  - [ ] 4.2 创建 protocol.c（套用转换规则；`new_analog_struct` 适配扁平 analog）
  - [ ] 4.3 创建 api.c（8 compat 包装 + driver_info `gwinstek_gpd_driver_info`；scan 手动遍历 options）
  - [ ] 4.4 CMakeLists.txt + hwdriver.c 注册（统一在 Task 6 处理）

- [ ] Task 5: 迁移 scpi-dmm（1490 行，SCPI+Serial，2 特殊API，SCPI 万用表）
  - [ ] 5.1 创建 protocol.h（include compat.h，保留 dev_context/SCPI 函数声明；`sr_sw_limits` static inline；`SR_CONF_CONTINUOUS`/`SR_CONF_MEASURED_QUANTITY` 守卫定义如需）
  - [ ] 5.2 创建 protocol.c（套用转换规则；`sr_scpi_scan((struct drv_context *)di->priv, ...)`；`new_analog_struct` 适配扁平 analog；`sr_scpi_source_add` 保持 session 参数）
  - [ ] 5.3 创建 api.c（8 compat 包装 + driver_info `scpi_dmm_driver_info`；移除 `SR_REGISTER_DEV_DRIVER`；scan 用 `sr_scpi_scan((struct drv_context *)di->priv, ...)`）
  - [ ] 5.4 CMakeLists.txt + hwdriver.c 注册（统一在 Task 6 处理）

## 阶段 2：编译验证

- [ ] Task 6: CMake + hwdriver 注册 5 个驱动
  - [ ] 6.1 CMakeLists.txt 在 line 656 后插入 5 个 `option(ENABLE_DRIVER_ATORCH/.../SCPI_DMM)`
  - [ ] 6.2 CMakeLists.txt 在 line 863 后插入 5 个 `if(ENABLE_DRIVER_*) add_definitions(-DHAVE_DRIVER_*) endif()`
  - [ ] 6.3 CMakeLists.txt 在 line 1271 后插入 5 个 `if(ENABLE_DRIVER_*) list(APPEND libsigrok_SOURCES ...) endif()`
  - [ ] 6.4 hwdriver.c 在 line 286 后插入 5 个 `#ifdef HAVE_DRIVER_* extern SR_PRIV struct sr_dev_driver *_driver_info; #endif`
  - [ ] 6.5 hwdriver.c 在 line 480 后插入 5 个 `#ifdef HAVE_DRIVER_* &*_driver_info, #endif`

- [ ] Task 7: cmake 重新配置启用 5 个驱动 + ninja 编译验证
  - [ ] 7.1 运行 cmake 启用 5 个新驱动选项（`-DENABLE_DRIVER_ATORCH=ON -DENABLE_DRIVER_BKPRECISION_1856D=ON -DENABLE_DRIVER_SERIAL_LCR=ON -DENABLE_DRIVER_GWINSTEK_GPD=ON -DENABLE_DRIVER_SCPI_DMM=ON`）
  - [ ] 7.2 确认 CMakeCache.txt 中 5 个选项均为 ON
  - [ ] 7.3 运行 `cd build && ninja -j 16`
  - [ ] 7.4 若有编译错误，逐个修复（重点关注 SCPI 适配、flat analog 适配、feed_queue_analog、std_*_idx 签名、sr_sw_limits 内联、frame_begin 调用）
  - [ ] 7.5 确认最终链接成功（PXView.exe 生成，无 multiple definition / undefined reference）

# Task Dependencies

- [Task 1-5] 互相独立，可并行执行（5 个驱动迁移）
- [Task 6] depends on [Task 1-5]（全部迁移完成后才能注册）
- [Task 7] depends on [Task 6]

# 并行策略

- **阶段 1**：Task 1-5 全部并行（最多 5 个 sub-agent 同时运行）
  - 优先级 1（最简单 3 个，纯 Serial + new_analog_struct）：Task 2/3/4
  - 优先级 2（Serial + feed_queue_analog）：Task 1
  - 优先级 3（SCPI + 大代码量）：Task 5
- **阶段 2**：Task 6 → Task 7 串行收尾
- **与 Batch 1/2 关系**：本 spec 独立处理 5 个驱动；最终编译验证（Task 7）可与 batch1/batch2 共 17 个驱动统一启用编译，避免分批编译开销
