# Tasks

## Phase 1: 动态库基础设施(并行)

- [ ] Task 1: 创建跨平台导出宏头文件 `libsigrokstd/include/srstd_export.h`
  - 定义 `SRSTD_API` 宏:Windows 用 `__declspec(dllexport/dllimport)`(由 `SRSTD_BUILDING_DLL` 切换),Linux/macOS 用 `__attribute__((visibility("default")))`
  - 包含 include guard 和 `extern "C"` 兼容性
- [ ] Task 2: 修改 [libsigrokstd/CMakeLists.txt](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokstd/CMakeLists.txt) 切换为 SHARED 库
  - `add_library(libsigrokstd STATIC ...)` → `add_library(libsigrokstd SHARED ...)`
  - 增加 `target_compile_options(libsigrokstd PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)`
  - 增加 `target_compile_definitions(libsigrokstd PRIVATE SRSTD_BUILDING_DLL=1)`
  - 移除 `target_compile_options(libsigrokstd PRIVATE -include .../srstd_rename.h)`
  - 输出目录从 `ARCHIVE_OUTPUT_DIRECTORY` 改为同时设置 `LIBRARY_OUTPUT_DIRECTORY` + `RUNTIME_OUTPUT_DIRECTORY`(Windows DLL 走 RUNTIME)
  - 链接 libusb 时确保用 SHARED 形式(`${LIBUSB_1_LIBRARIES}` 已经是 .dll/.so,无需改)

## Phase 2: 移除宏注入基础设施(并行,与 Phase 1 无依赖)

- [ ] Task 3: 删除宏注入相关文件
  - 删除 `libsigrokstd/include/srstd_rename.h`(由 gen_srstd_rename.py 生成)
  - 删除 `libsigrokstd/include/srstd.h`(聚合头,不再需要)
  - 保留 `tools/gen_srstd_rename.py`(标注 deprecated,留作历史参考)
  - 删除 `tools/migrate_drivers_to_srstd.py`(不再用于迁移新驱动)
- [ ] Task 4: 还原 82 个迁移驱动源码到上游状态
  - 写脚本 `tools/restore_drivers_from_srstd.py`:遍历 `libsigrokstd/src/hardware/*/`,对每个驱动:
    - `protocol.h` 删除 `#include "srstd.h"` 行
    - `api.c` 中 `<driver>_driver_info` 结构体前加回 `static`(被迁移脚本去掉的)
  - 或者更稳妥的做法:从上游 git 重新 checkout 这些驱动文件覆盖
  - 验证:每个驱动 `protocol.h` 中不再出现 `srstd.h`,`api.c` 中 `driver_info` 前 `static` 关键字存在

## Phase 3: 调整胶水层(依赖 Phase 1 + Phase 2)

- [ ] Task 5: 修改 [libsigrokstd/bridge/srstd_pxview_glue.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokstd/bridge/srstd_pxview_glue.h) 添加 SRSTD_API
  - `#include "srstd_export.h"`(在 glib.h 之后)
  - 所有公开函数声明前加 `SRSTD_API` 前缀(共约 25 个函数)
  - 保持函数签名不变(参数仍用 `void*`)
- [ ] Task 6: 修改 [libsigrokstd/bridge/srstd_pxview_glue.c](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokstd/bridge/srstd_pxview_glue.c) 内部调用真实 sr_*
  - 移除文件顶部对 `srstd_rename.h` 的依赖(此文件不再用 `-include` 注入)
  - 直接 `#include <libsigrok/libsigrok.h>`(上游版本,libsigrokstd include 路径已 BEFORE)
  - 把所有 `srstd_config_get` → `sr_config_get`、`srstd_dev_open` → `sr_dev_open`、`srstd_session_*` → `sr_session_*` 等真实符号
  - struct 引用从 `struct srstd_dev_inst` → `struct sr_dev_inst`(就是上游真实类型)
- [ ] Task 7: 修改 [libsigrokstd/bridge/srstd_init_shared.c](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokstd/bridge/srstd_init_shared.c) 同 Task 6
  - 内部调用真实 `sr_init` / `sr_exit` / `libusb_*`
  - struct `struct sr_context*`(上游真实类型)
- [ ] Task 8: 修改 [libsigrokstd/bridge/srstd_bridge.{h,c}](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokstd/bridge/srstd_bridge.h) 类型引用真实化
  - `srstd_bridge.c` 内部 struct 转换两端都用真实 `struct sr_*` 类型
  - 注意:此文件原 libsigrokstd_bridge OBJECT 库**没有** `-include srstd_rename.h`,本来就是用 PXView 的 struct;现在改为用上游的 struct(需要切 include 路径到 libsigrokstd 的 include)
  - 验证:编译通过,struct 字段访问正确
- [ ] Task 9: 修改 [libsigrokstd/src/driver_registry.c](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokstd/src/driver_registry.c)(如有 #include "srstd.h" 改为 #include <libsigrok/libsigrok.h>)
  - 验证:82 个驱动的 `<driver>_driver_info` 符号仍被手动注册

## Phase 4: 接入 PXView(依赖 Phase 3)

- [ ] Task 10: 修改顶层 [CMakeLists.txt](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/CMakeLists.txt) 添加 libsigrokstd 子项目
  - 在 `include(${CMAKE_SOURCE_DIR}/cmake/libsigrok.cmake)`(第 68 行)后添加 `add_subdirectory(libsigrokstd)`
  - 在 `target_link_libraries(${PROJECT_NAME} ...)`(第 103 行)的 `pxview-core` 后增加 `libsigrokstd`
  - 验证:`ninja libsigrokstd` 单独编译通过,`ninja PXView` 链接时无符号冲突
- [ ] Task 11: 修改 [PXView/pv/sigsession.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/sigsession.h) 添加 srstd 上下文成员
  - 在 private 区新增 `void* _srstd_ctx = nullptr;`
  - 新增 `void* _libusb_ctx_cache = nullptr;`(供 init_shared 使用,避免重复查询)
- [ ] Task 12: 修改 [PXView/pv/sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/sigsession.cpp) init/uninit 调用 srstd
  - 顶部 `#include "bridge/srstd_pxview_glue.h"`(在 libsigrok.h 之后,不冲突因为 srstd_pxview_glue.h 不 include libsigrok.h)
  - `init()` 在 `ds_lib_init()` 成功 return 前:
    ```cpp
    extern "C" void* ds_get_libusb_context(void);  // 已存在的 PXView API,或新加
    if (srstd_pxview_init_shared(&_srstd_ctx, ds_get_libusb_context()) == SR_OK) {
        srstd_glue_set_datafeed_callback(&core::DataFeedParser::data_feed_callback_ex,
                                          _data_feed_parser.get());
        pxv_info("libsigrokstd initialized");
    } else {
        pxv_err("libsigrokstd init failed, upstream drivers unavailable");
        _srstd_ctx = nullptr;
    }
    ```
  - `uninit()` 在 `ds_lib_exit()` 前:
    ```cpp
    if (_srstd_ctx) {
        srstd_glue_close_active_device();
        srstd_pxview_exit(_srstd_ctx);
        _srstd_ctx = nullptr;
    }
    ```
  - 注意:如果 `ds_get_libusb_context()` 不存在,需要在 libsigrok 暴露一个 accessor(或从 sr_context 中读取)
- [ ] Task 13: 修改 [PXView/pv/deviceagent.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/deviceagent.h) + `.cpp` 添加 LIB_SRSTD 分流
  - 新增 `enum DeviceLib { LIB_PXVIEW, LIB_SRSTD };` + `DeviceLib _device_lib = LIB_PXVIEW;`
  - `device_lib()` accessor
  - `get_config`/`set_config`/`get_config_list`/`start`/`stop`/`is_collecting` 方法添加 if 分流:
    ```cpp
    if (_device_lib == LIB_SRSTD) {
        return srstd_glue_dev_config_get(_active_srstd_sdi, ch, cg, key, data);
    }
    // 原 PXView 路径
    ```
- [ ] Task 14: 修改 [PXView/pv/sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/sigsession.cpp) `get_device_list()` 合并扫描结果
  - 调 `ds_get_device_list()` 获取 PXView fork 设备列表
  - 调 `srstd_glue_scan_devices()` 获取 srstd 设备数 N
  - 对每个 i in 0..N:调 `srstd_glue_get_scanned_device_name()` 填名字,handle 用 `SRSTD_MAKE_HANDLE(i)`
  - 合并到返回列表
- [ ] Task 15: 修改 [PXView/pv/sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/sigsession.cpp) `set_device()` 分流
  - 检测 `SRSTD_IS_HANDLE(handle)` → `srstd_glue_open_scanned_device(idx)` + DeviceAgent 切到 LIB_SRSTD
  - 否则走原 PXView 路径 + DeviceAgent 切到 LIB_PXVIEW

## Phase 5: 部署与验证(依赖 Phase 4)

- [ ] Task 16: 修改 [cmake/install_packaging.cmake](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/cmake/install_packaging.cmake) 安装动态库
  - `install(TARGETS libsigrokstd RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)`
  - Windows:`libsigrokstd.dll` → `bin/`(与 PXView.exe 同目录)
  - Linux:`libsigrokstd.so` → `lib/`,PXView 设置 RPATH `$ORIGIN/../lib`
  - macOS:`libsigrokstd.dylib` → `Frameworks/`,RPATH `@loader_path/../Frameworks`
- [ ] Task 17: 修正前序 spec 勾选状态
  - 修改 `.trae/specs/dual-libsigrok-coexist-restore-features/checklist.md`:在所有未实际接入的条目旁添加标注 "[实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]"
  - 不删除该 spec 文档
- [ ] Task 18: 编译验证
  - `cd build && ninja -j 16 && ninja install`
  - 验证 `install.dir/bin/PXView.exe` 存在
  - 验证 `install.dir/bin/libsigrokstd.dll` 存在(Windows)
  - 启动 PXView.exe 不崩溃,启动日志含 "libsigrokstd initialized"
- [ ] Task 19: 运行时验证(至少 1 个上游驱动)
  - 启动 PXView.exe --headless
  - MCP 调 `get_devices`,验证设备列表中包含至少 1 个 srstd 驱动(如 demo-srstd 模拟设备)
  - 如果有真实硬件(slogic),尝试扫描 + 打开 + 配置 + 采集
  - 验证 PXView 原生驱动(DSL/pxlogic)不受影响

# Task Dependencies

- Phase 1(Task 1, 2)+ Phase 2(Task 3, 4)可并行,无相互依赖
- Phase 3(Task 5-9)依赖 Phase 1(Task 2 完成 SHARED 切换)+ Phase 2(Task 3 完成 srstd_rename.h 删除)
- Phase 4(Task 10-15)依赖 Phase 3 全部完成
- Phase 5(Task 16-19)依赖 Phase 4 完成
  - Task 16 可与 Task 14/15 并行(部署文件不依赖运行时逻辑)
  - Task 17 可与任何任务并行(纯文档修正)
  - Task 18 依赖 Task 16 完成(需要 install 才有 .dll)
  - Task 19 依赖 Task 18 完成
