# Tasks

## Phase 1: 自动化迁移脚本

- [ ] Task 1: 编写批量迁移 Python 脚本
  - [ ] SubTask 1.1: 创建 `tools/migrate_drivers_to_srstd.py`,扫描 `c:\Users\admin\Downloads\libsigrok\src\hardware\` 与 PXView `libsigrok/hardware/` 的交集驱动目录(排除 DSL/pxlogic/demo/compat/sipeed-slogic-analyzer)
  - [ ] SubTask 1.2: 脚本对每个驱动执行:拷贝上游源码到 `libsigrokstd/src/hardware/<driver>/`
  - [ ] SubTask 1.3: 脚本修改每个驱动的 `protocol.h`:在 `#include "libsigrok-internal.h"` 前插入 `#include "srstd.h"`(若已存在则跳过)
  - [ ] SubTask 1.4: 脚本修改每个驱动的 `api.c`:用正则去除 `struct sr_dev_driver <name>_driver_info` 前的 `static` 修饰符
  - [ ] SubTask 1.5: 脚本生成 `libsigrokstd/src/driver_registry.c`:遍历所有迁移驱动,提取 `<name>_driver_info` 符号名,生成 `extern` 声明 + `srstd_get_all_drivers()` 函数(返回 NULL 终止的 `struct sr_dev_driver**` 数组)
  - [ ] SubTask 1.6: 脚本支持 `--exclude` 参数排除特殊依赖驱动(VXI/GPIB/VISA 等),支持 `--dry-run` 预览
- [ ] Task 2: 识别并排除特殊依赖驱动
  - [ ] SubTask 2.1: 检查所有 compat 驱动是否依赖 `rpc/rpc.h`(VXI)/`<gpib.h>`(GPIB)/`<visa.h>`(VISA),列出需排除的驱动
  - [ ] SubTask 2.2: 检查依赖 libserialport/hidapi/libftdi 的驱动,确认 `#ifdef` 守卫存在(可迁移,缺失依赖时编译为 stub)

## Phase 2: libsigrokstd 批量集成

- [ ] Task 3: 修改 libsigrokstd CMake 批量注册驱动源文件
  - [ ] SubTask 3.1: 在 `libsigrokstd/CMakeLists.txt` 用 `file(GLOB ...)` 或显式列表注册所有迁移驱动目录的 `.c` 文件
  - [ ] SubTask 3.2: 将 `driver_registry.c` 加入源文件列表(注意:不带 `srstd_rename.h` 编译,因为它需要被 PXView 侧引用?实际上带 `srstd_rename.h` 编译,因为内部 `extern struct sr_dev_driver` 会被重命名为 `srstd_dev_driver`)
  - [ ] SubTask 3.3: 验证 `ninja libsigrokstd` 编译所有迁移驱动通过,无符号冲突
- [ ] Task 4: 重构 srstd_init_shared.c 批量驱动注册
  - [ ] SubTask 4.1: 删除 srstd_init_shared.c 中 slogic 单驱动手动注册代码(line 88-101)
  - [ ] SubTask 4.2: 调用 `srstd_get_all_drivers()` 获取所有迁移驱动数组,遍历追加到 `ctx->driver_list`
  - [ ] SubTask 4.3: 验证 `srstd_init_shared` 后 `ctx->driver_list` 包含所有迁移驱动

## Phase 3: DeviceAgent 动态驱动识别

- [ ] Task 5: 扩展胶水层提供驱动名列表
  - [ ] SubTask 5.1: 在 `srstd_pxview_glue.h` 声明 `srstd_glue_get_driver_names(char*** names, int* count)`,返回所有注册驱动的 `.name` 字段
  - [ ] SubTask 5.2: 在 `srstd_pxview_glue.c` 实现:遍历 `g_ctx->driver_list`,收集每个驱动的 `drv->name`
- [ ] Task 6: 修改 DeviceAgent 动态识别
  - [ ] SubTask 6.1: 在 `deviceagent.h` 删除硬编码 `is_srstd_device()` 实现,改为声明(需访问 srstd 驱动名列表)
  - [ ] SubTask 6.2: 在 `deviceagent.cpp` 实现 `is_srstd_device()`:调用 `srstd_glue_get_driver_names()`,遍历比对 `_driver_name`
  - [ ] SubTask 6.3: 验证任意 srstd 驱动设备(如 fx2lafw)能被 `is_srstd_device()` 识别为 true

## Phase 4: 删除 PXView compat 层

- [ ] Task 7: 删除 compat 基础设施
  - [ ] SubTask 7.1: 删除 `libsigrok/hardware/compat/` 整个目录(compat.h/compat_config.h/compat_driver.h/compat_helpers.h/compat_helpers.c/compat_serial.h/compat_serial.c/compat_scpi.h/compat_scpi.c)
  - [ ] SubTask 7.2: 删除 `libsigrok/hardware/` 下所有 compat 驱动目录(约 80 个,保留 DSL/pxlogic/demo/common/sipeed-slogic-analyzer 空目录已删)
- [ ] Task 8: 删除 CMake compat 配置
  - [ ] SubTask 8.1: 删除 `CMake/options.cmake` 中 `ENABLE_COMPAT_DRIVERS` option + 所有 81 个 `ENABLE_DRIVER_*` option
  - [ ] SubTask 8.2: 删除 `CMake/libsigrok.cmake` 中两个 `if(ENABLE_COMPAT_DRIVERS)` 块及其内容
- [ ] Task 9: 删除 hwdriver.c compat 驱动注册
  - [ ] SubTask 9.1: 删除 `libsigrok/hwdriver.c` 中所有 compat 驱动的 `extern` 声明与 `&<name>_driver_info` 注册项(保留 DSLogic/DSCope/pxlogic/demo)
  - [ ] SubTask 9.2: 删除 `HAVE_DRIVER_*` 宏相关的 `#ifdef` 块

## Phase 5: 编译验证与清理

- [ ] Task 10: 编译验证
  - [ ] SubTask 10.1: 验证 `ninja -j 16 && ninja install` 编译通过
  - [ ] SubTask 10.2: 验证 PXView `--headless` 启动无崩溃
  - [ ] SubTask 10.3: 验证 `nm libsigrokstd.a | grep ' T sr_'` 返回空(符号隔离未被破坏)
  - [ ] SubTask 10.4: 验证 `nm libsigrokstd.a | grep ' T srstd_'` 返回 500+ 符号(原 408 + 新增驱动函数)
- [ ] Task 11: 清理残留
  - [ ] SubTask 11.1: grep `ENABLE_COMPAT_DRIVERS` 在项目中无残留(除历史 spec/devdoc)
  - [ ] SubTask 11.2: grep `compat_helpers|compat_serial|compat_scpi` 在 `libsigrok/` 无残留
  - [ ] SubTask 11.3: 删除空的 `libsigrok/hardware/sipeed-slogic-analyzer/` 目录(Task 11 of dual-libsigrok spec 已删文件,目录残留)

# Task Dependencies

- Task 2 依赖 Task 1(脚本需知道排除列表)
- Task 3 依赖 Task 1(脚本生成驱动源码后才能注册)
- Task 4 依赖 Task 3(driver_registry.c 需在 CMake 注册)
- Task 5 依赖 Task 4(胶水层需 driver_list 已批量注册)
- Task 6 依赖 Task 5(DeviceAgent 需胶水层提供驱动名列表)
- Task 7/8/9 可并行(删除不同部分)
- Task 10 依赖 Task 6/7/8/9(全部修改完成后编译验证)
- Task 11 依赖 Task 10

# Parallelizable Work

- Task 1 内部 SubTask 1.2/1.3/1.4/1.5 可顺序执行(同一脚本内)
- Task 7/8/9 在 Phase 4 内部可并行(删除不同部分:compat 目录/CMake/hwdriver.c)
- Task 5 与 Task 7/8/9 可并行(胶水层扩展与 compat 删除独立)
