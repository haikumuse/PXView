# Tasks

## 阶段 1：缺陷重构（`std_session_send_df_frame_begin` 收编）

- [x] Task 1: 验证 5 个本地实现一致性
  - [x] 1.1 读取 fx2lafw/gwinstek-gds-800/hameg-hmo/hantek-dso/hung-chang-dso-2100 各自 protocol.c 中 `std_session_send_df_frame_begin` 的实现体
  - [x] 1.2 对比 5 份实现，确认语义等价（均发送 SR_DF_FRAME_BEGIN packet + ds_data_forward）
  - [x] 1.3 若存在差异，记录差异并决定以哪份为规范实现（两变体均 ds_data_forward + SR_PKT_OK，等价）

- [x] Task 2: 在 compat 层添加单一规范实现
  - [x] 2.1 在 `libsigrok/hardware/compat/compat_helpers.h` 末尾添加 `std_session_send_df_frame_begin(const struct sr_dev_inst *sdi)` 声明
  - [x] 2.2 在 `libsigrok/hardware/compat/compat_helpers.c` 中 `std_session_send_df_end` 附近添加 `std_session_send_df_frame_begin` 定义（发送 SR_DF_FRAME_BEGIN packet，仿 std_session_send_df_end 模式）
  - [x] 2.3 确认 `SR_DF_FRAME_BEGIN` 常量与 `struct sr_datafeed_packet` 字段在 PXView libsigrok.h 中可用

- [x] Task 3: 删除 5 个驱动的本地定义
  - [x] 3.1 删除 `fx2lafw/protocol.h` 中的声明 + `fx2lafw/protocol.c` 中的定义
  - [x] 3.2 删除 `gwinstek-gds-800/protocol.h` 中的声明 + `gwinstek-gds-800/protocol.c` 中的定义
  - [x] 3.3 删除 `hameg-hmo/protocol.h` 中的声明 + `hameg-hmo/protocol.c` 中的定义
  - [x] 3.4 删除 `hantek-dso/protocol.h` 中的声明 + `hantek-dso/protocol.c` 中的定义
  - [x] 3.5 删除 `hung-chang-dso-2100/protocol.c` 中的定义（protocol.h 实际无声明）
  - [x] 3.6 grep 验证全局无残留 `std_session_send_df_frame_begin` 本地定义（5 个目标驱动仅剩调用点；发现另外 9 个驱动有本地定义，不在本 spec 范围内，若启用需后续处理）

## 阶段 2：迁移 sipeed-slogic-analyzer

- [x] Task 4: 创建迁移后的驱动源文件
  - [x] 4.1 创建 `libsigrok/hardware/sipeed-slogic-analyzer/protocol.h`：include 改为 `#include "hardware/compat/compat.h"`，保留 struct dev_context / slogic_model / 函数声明
  - [x] 4.2 创建 `libsigrok/hardware/sipeed-slogic-analyzer/protocol.c`：include 改为 compat.h；`std_session_send_df_header(sdi)`/`end(sdi)` → 2-arg `(sdi, LOG_PREFIX)`；`std_session_send_df_frame_begin(sdi)` 保持原调用（由 compat 层提供，**不本地定义**）；`sr_session_source_add(sdi->session, poll_obj, ...)` → 5-arg 移除 session；`sr_session_source_remove(sdi->session, poll_obj)` → 1-arg 移除 session；libusb 直接调用保留
  - [x] 4.3 创建 `libsigrok/hardware/sipeed-slogic-analyzer/api.c`：include 改为 compat.h；`std_scan_complete` → `std_scan_complete_compat`；`di->context` → `di->priv`；移除 `SR_REGISTER_DEV_DRIVER`；结构体初始化器移除 `.config_channel_set` / `.dev_clear`，`.context = NULL` → `.priv = NULL`，`.dev_mode_list`/`.dev_destroy`/`.dev_status_get` 指向 compat 默认实现；添加 8 个 compat 包装函数；`config_channel_set` per-channel 逻辑合并进 `config_set` 包装（`ch != NULL` 分支）

## 阶段 3：构建集成

- [x] Task 5: CMakeLists.txt + hwdriver.c 注册
  - [x] 5.1 在 `CMakeLists.txt` 的 `if(ENABLE_COMPAT_DRIVERS)` 块内添加 `option(ENABLE_DRIVER_SIPEED_SLOGIC_ANALYZER "Sipeed SLogic Analyzer" OFF)`
  - [x] 5.2 添加源文件条目（api.c + protocol.c，仿 pipistrello-ols 块）
  - [x] 5.3 添加 `add_definitions(-DHAVE_DRIVER_SIPEED_SLOGIC_ANALYZER)` 守卫
  - [x] 5.4 在 `libsigrok/hwdriver.c` 添加 `extern SR_PRIV struct sr_dev_driver sipeed_slogic_analyzer_driver_info;` 声明（由 `HAVE_DRIVER_SIPEED_SLOGIC_ANALYZER` 守卫）
  - [x] 5.5 在 hwdriver.c 驱动列表添加 `&sipeed_slogic_analyzer_driver_info` 注册项

## 阶段 4：编译验证

- [x] Task 6: 重新配置 cmake 并启用新驱动
  - [x] 6.1 运行 `cmake -DENABLE_DRIVER_SIPEED_SLOGIC_ANALYZER=ON`（在 build 目录重新配置）
  - [x] 6.2 确认 CMakeCache.txt 中 `ENABLE_DRIVER_SIPEED_SLOGIC_ANALYZER:BOOL=ON`

- [x] Task 7: 全量编译验证
  - [x] 7.1 运行 `cd build && ninja -j 16`
  - [x] 7.2 修复 sipeed-slogic 编译错误（protocol.c 5 个 + api.c 5 类：local_std_*_idx helpers、std_gvar_min_max_step_thresholds、sr_session_send 宏、soft_trigger_logic unitsize）
  - [x] 7.3 链接验证：启用 fx2lafw + sipeed-slogic 同时编译，968/968 成功，无 multiple definition 错误
  - [x] 7.4 PXView.exe 生成（2026-07-01 15:01:47）

# Task Dependencies

- [Task 3] depends on [Task 2]（删除本地定义前，compat 层单一实现必须先就位，否则调用点无符号）
- [Task 4] depends on [Task 2]（sipeed-slogic 调用 `std_session_send_df_frame_begin`，由 compat 层提供）
- [Task 5] depends on [Task 4]（注册前驱动文件必须存在）
- [Task 6] depends on [Task 5]
- [Task 7] depends on [Task 6]
- [Task 1] 可与 [Task 4] 并行（互不依赖）

# 并行策略

- **阶段 1 内部**：Task 1 → Task 2 → Task 3 串行（依赖链）
- **阶段 1 与阶段 2**：Task 4 可与 Task 1 并行启动（Task 4 只依赖 Task 2 完成后才能链接，但文件编写本身可先行）
- **阶段 3、4**：必须串行收尾
