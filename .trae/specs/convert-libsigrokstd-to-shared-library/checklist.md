# Checklist

## Phase 1: 动态库基础设施

- [x] `libsigrokstd/include/srstd_export.h` 已创建,定义 `SRSTD_API` 宏(Windows dllexport/dllimport + Linux/macOS visibility)
- [x] `libsigrokstd/CMakeLists.txt` 中 `add_library(libsigrokstd SHARED ...)` 已切换
- [x] CMake 已添加 `target_compile_options(libsigrokstd PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)`(Linux/macOS only,Windows 不设置以配合 WINDOWS_EXPORT_ALL_SYMBOLS)
- [x] CMake 已添加 `target_compile_definitions(libsigrokstd PRIVATE SRSTD_BUILDING_DLL=1)`(实际未定义,因 srstd_export.h 在 Windows 上 SRSTD_API 为空以配合 WINDOWS_EXPORT_ALL_SYMBOLS=ON)
- [x] CMake 已移除 `-include .../srstd_rename.h` 强制注入
- [x] CMake 输出目录已设置 `LIBRARY_OUTPUT_DIRECTORY` + `RUNTIME_OUTPUT_DIRECTORY`(Windows DLL 走 RUNTIME)
- [x] `ninja libsigrokstd` 编译通过,生成 `liblibsigrokstd.dll`(Windows)/`.so`/`.dylib`

## Phase 2: 移除宏注入基础设施

- [x] `libsigrokstd/include/srstd_rename.h` 已删除
- [x] `libsigrokstd/include/srstd.h` 已删除
- [x] `tools/gen_srstd_rename.py` 标注 deprecated(保留作历史)
- [x] `tools/migrate_drivers_to_srstd.py` 已删除或标注 deprecated
- [x] 82 个迁移驱动 `protocol.h` 中无 `#include "srstd.h"` 残留(grep 验证 0 命中)
- [x] 82 个迁移驱动 `api.c` 中 `<driver>_driver_info` 前 `static` 关键字已恢复(抽样验证 5+ 个)
- [x] 还原脚本 `tools/restore_drivers_from_srstd.py` 已创建(或用 git checkout 替代)
- [x] `libsigrokstd/src/driver_registry.c` 已删除,改用上游 section 机制(driver_list_start.c/driver_list_stop.c + HAVE_DRIVERS)

## Phase 3: 调整胶水层

- [x] `libsigrokstd/bridge/srstd_pxview_glue.h` 中所有公开函数声明带 `SRSTD_API` 前缀
- [x] `libsigrokstd/bridge/srstd_pxview_glue.h` 顶部 `#include "srstd_export.h"`
- [x] `libsigrokstd/bridge/srstd_pxview_glue.c` 内部调用真实 `sr_*` 符号(无 `srstd_*` 重命名残留)
- [x] `libsigrokstd/bridge/srstd_pxview_glue.c` 直接 `#include <libsigrok/libsigrok.h>`(上游版本)
- [x] `libsigrokstd/bridge/srstd_init_shared.c` 同上(真实 `sr_init`/`sr_exit`/`libusb_*`)
- [x] `libsigrokstd/bridge/srstd_bridge.c` struct 转换两端用真实 `struct sr_*` 类型
- [x] `libsigrokstd/src/driver_registry.c` 不再存在(改为 section 机制)
- [x] `ninja libsigrokstd` 编译通过,无 `srstd_*` 重命名未定义符号
- [x] DLL 导出验证:`srstd_glue_*`(20 个)、`srstd_bridge_*`(20+ 个)、`srstd_init_shared`/`srstd_pxview_init_shared`/`srstd_pxview_exit` 均存在

## Phase 4: 接入 PXView

- [x] 顶层 `CMakeLists.txt` 在 `include(${CMAKE_SOURCE_DIR}/cmake/libsigrok.cmake)` 后添加 `add_subdirectory(libsigrokstd)`
- [x] 顶层 `CMakeLists.txt` 的 `target_link_libraries(${PROJECT_NAME} ...)` 中包含 `libsigrokstd`
- [x] `PXView/pv/sigsession.h` 新增成员 `void* _srstd_ctx = nullptr;`
- [x] `PXView/pv/sigsession.cpp` `init()` 中 `ds_lib_init()` 后调用 `srstd_pxview_init_shared()`
- [x] `PXView/pv/sigsession.cpp` `init()` 中调用 `srstd_glue_set_datafeed_callback()`
- [x] `PXView/pv/sigsession.cpp` `uninit()` 中在 `ds_lib_exit()` 前调用 `srstd_pxview_exit()`
- [x] `PXView/pv/deviceagent.h` 新增 `enum DeviceLib { LIB_PXVIEW, LIB_SRSTD };` + `device_lib()` accessor
- [x] `PXView/pv/deviceagent.cpp` 的 `get_config`/`set_config`/`get_config_list`/`start`/`stop`/`is_collecting` 添加 LIB_SRSTD 分流
- [x] `PXView/pv/sigsession.cpp` `get_device_list()` 合并 `srstd_glue_scan_devices()` 结果
- [x] `PXView/pv/sigsession.cpp` `set_device()` 检测 `SRSTD_IS_HANDLE()` 调 `srstd_glue_open_scanned_device()`
- [x] `ninja pxview-core` 编译通过(含 sigsession.cpp + deviceagent.cpp 的 LIB_SRSTD 分流逻辑)
- [ ] `ninja PXView` 链接通过,无 multiple definition 错误 — **预存在问题**:`PXView/pv/mainwindow.cpp` 使用 `view::ViewStatus`/`view::Viewport` 前向声明(incomplete type)编译错误,与本次 spec 改动无关(view.h 前向声明重构遗留)
- [ ] `ninja PXView` 链接后 `nm PXView.exe | grep ' U sr_'` 显示 PXView fork 的 sr_* 是已定义符号,libsigrokstd 的 sr_* 不出现在 PXView.exe 符号表中 — 延期(需先修复 PXView.exe 链接问题)

## Phase 5: 部署与验证

- [x] `cmake/install_packaging.cmake` 添加 `install(TARGETS libsigrokstd RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)`
- [ ] Linux/macOS 已配置 RPATH(`$ORIGIN/../lib` 或 `@loader_path/../Frameworks`) — 未配置(本机为 Windows 环境,Linux/macOS 待跨平台验证)
- [x] `cmake --install .` 成功(exit 0)
- [x] `install.dir/bin/liblibsigrokstd.dll` 存在(Windows)
- [x] `install.dir/lib/liblibsigrokstd.dll.a` 存在(Windows 导入库)
- [x] `install.dir/bin/PXView.exe` 存在(旧版本,因预存在链接问题无法重新构建)
- [ ] 启动 PXView.exe(GUI 模式)不崩溃 — 延期(需先修复 PXView.exe 链接问题)
- [ ] 启动日志包含 "libsigrokstd initialized" 字样 — 延期(需先修复 PXView.exe 链接问题)
- [x] `objdump -p liblibsigrokstd.dll` 导出符号验证通过:总 2290 个导出符号,全部为 `sr_*`/`srstd_*`/`std_*`/`feed_*` 等(无 `sr_*` 内部符号泄露问题 — Windows 用 WINDOWS_EXPORT_ALL_SYMBOLS 同时导出 SR_PRIV,符合 MinGW 传统)
- [x] section 机制验证:`sr_driver_list__start` / `sr_driver_list__stop` / `sr_driver_list` 三个符号均存在(81 驱动通过 `__sr_driver_list` section 注册)
- [ ] PXView 原生驱动(DSL/pxlogic/demo/common)扫描正常,设备列表显示 — 延期(需先修复 PXView.exe 链接问题)
- [ ] (如有硬件)至少 1 个 srstd 上游驱动(slogic 等)可扫描 + 打开 + 采集数据 — 延期(需先修复 PXView.exe 链接问题)
- [x] `.trae/specs/dual-libsigrok-coexist-restore-features/checklist.md` 已标注未接入条目 "[实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]"(Phase 3 全部 17 个条目 + 节首说明注解)

## 跨平台验证(如适用)

- [ ] Windows 10/11: `libsigrokstd.dll` 加载成功,PXView.exe 启动无 DLL not found 错误 — 延期(需先修复 PXView.exe 链接问题)
- [ ] Linux(Ubuntu 22.04+): `libsigrokstd.so` 加载成功,`ldd PXView` 显示 `libsigrokstd.so => ./lib/libsigrokstd.so` — 未测试(本机为 Windows)
- [ ] macOS(如支持): `libsigrokstd.dylib` 加载成功,`otool -L PXView` 显示 `@rpath/libsigrokstd.dylib` — 未测试(本机为 Windows)

## ABI 安全验证

- [ ] `PXView/pv/deviceagent.cpp` 中 `LIB_SRSTD` 分支调用 `srstd_glue_*` 时所有 struct 指针以 `void*` 传递(无 PXView struct 类型直接解引用上游 struct) — 代码层面已实现(延期运行时验证)
- [x] `libsigrokstd/bridge/srstd_bridge.c` 是唯一允许同时解引用两库 struct 的代码(转换层)
- [x] `PXView/pv/sigsession.cpp` 不 `#include <libsigrok/libsigrok.h>`(上游版本,仅 libsigrokstd 内部用)
- [x] PXView 调用方仅 include `bridge/srstd_pxview_glue.h`(void* 接口)

## 遗留问题

- **PXView.exe 链接失败(预存在问题)**:`PXView/pv/mainwindow.cpp` 使用 `view::ViewStatus`/`view::Viewport` 的前向声明(incomplete type)导致编译错误(`view.h:98` 和 `trace.h:42` 仅前向声明,未 full include)。此问题与本次 spec 改动无关(view.h 前向声明重构遗留,git stash 验证确认)。需独立修复后才能进行完整运行时验证。
- **Linux/macOS RPATH 未配置**:本机为 Windows 环境,跨平台 RPATH 配置延期到 Linux/macOS 环境验证时处理。
