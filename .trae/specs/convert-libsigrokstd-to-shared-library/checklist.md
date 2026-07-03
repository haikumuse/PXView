# Checklist

## Phase 1: 动态库基础设施

- [ ] `libsigrokstd/include/srstd_export.h` 已创建,定义 `SRSTD_API` 宏(Windows dllexport/dllimport + Linux/macOS visibility)
- [ ] `libsigrokstd/CMakeLists.txt` 中 `add_library(libsigrokstd SHARED ...)` 已切换
- [ ] CMake 已添加 `target_compile_options(libsigrokstd PRIVATE -fvisibility=hidden -fvisibility-inlines-hidden)`
- [ ] CMake 已添加 `target_compile_definitions(libsigrokstd PRIVATE SRSTD_BUILDING_DLL=1)`
- [ ] CMake 已移除 `-include .../srstd_rename.h` 强制注入
- [ ] CMake 输出目录已设置 `LIBRARY_OUTPUT_DIRECTORY` + `RUNTIME_OUTPUT_DIRECTORY`(Windows DLL 走 RUNTIME)
- [ ] `ninja libsigrokstd` 编译通过,生成 `libsigrokstd.dll`/`.so`/`.dylib`

## Phase 2: 移除宏注入基础设施

- [ ] `libsigrokstd/include/srstd_rename.h` 已删除
- [ ] `libsigrokstd/include/srstd.h` 已删除
- [ ] `tools/gen_srstd_rename.py` 标注 deprecated(保留作历史)
- [ ] `tools/migrate_drivers_to_srstd.py` 已删除或标注 deprecated
- [ ] 82 个迁移驱动 `protocol.h` 中无 `#include "srstd.h"` 残留(grep 验证 0 命中)
- [ ] 82 个迁移驱动 `api.c` 中 `<driver>_driver_info` 前 `static` 关键字已恢复(抽样验证 5+ 个)
- [ ] 还原脚本 `tools/restore_drivers_from_srstd.py` 已创建(或用 git checkout 替代)

## Phase 3: 调整胶水层

- [ ] `libsigrokstd/bridge/srstd_pxview_glue.h` 中所有公开函数声明带 `SRSTD_API` 前缀
- [ ] `libsigrokstd/bridge/srstd_pxview_glue.h` 顶部 `#include "srstd_export.h"`
- [ ] `libsigrokstd/bridge/srstd_pxview_glue.c` 内部调用真实 `sr_*` 符号(无 `srstd_*` 重命名残留)
- [ ] `libsigrokstd/bridge/srstd_pxview_glue.c` 直接 `#include <libsigrok/libsigrok.h>`(上游版本)
- [ ] `libsigrokstd/bridge/srstd_init_shared.c` 同上(真实 `sr_init`/`sr_exit`/`libusb_*`)
- [ ] `libsigrokstd/bridge/srstd_bridge.c` struct 转换两端用真实 `struct sr_*` 类型
- [ ] `libsigrokstd/src/driver_registry.c` 不再 `#include "srstd.h"`(改为 `#include <libsigrok/libsigrok.h>`)
- [ ] `ninja libsigrokstd` 编译通过,无 `srstd_*` 重命名未定义符号

## Phase 4: 接入 PXView

- [ ] 顶层 `CMakeLists.txt` 在 `include(${CMAKE_SOURCE_DIR}/cmake/libsigrok.cmake)` 后添加 `add_subdirectory(libsigrokstd)`
- [ ] 顶层 `CMakeLists.txt` 的 `target_link_libraries(${PROJECT_NAME} ...)` 中包含 `libsigrokstd`
- [ ] `PXView/pv/sigsession.h` 新增成员 `void* _srstd_ctx = nullptr;`
- [ ] `PXView/pv/sigsession.cpp` `init()` 中 `ds_lib_init()` 后调用 `srstd_pxview_init_shared()`
- [ ] `PXView/pv/sigsession.cpp` `init()` 中调用 `srstd_glue_set_datafeed_callback()`
- [ ] `PXView/pv/sigsession.cpp` `uninit()` 中在 `ds_lib_exit()` 前调用 `srstd_pxview_exit()`
- [ ] `PXView/pv/deviceagent.h` 新增 `enum DeviceLib { LIB_PXVIEW, LIB_SRSTD };` + `device_lib()` accessor
- [ ] `PXView/pv/deviceagent.cpp` 的 `get_config`/`set_config`/`get_config_list`/`start`/`stop`/`is_collecting` 添加 LIB_SRSTD 分流
- [ ] `PXView/pv/sigsession.cpp` `get_device_list()` 合并 `srstd_glue_scan_devices()` 结果
- [ ] `PXView/pv/sigsession.cpp` `set_device()` 检测 `SRSTD_IS_HANDLE()` 调 `srstd_glue_open_scanned_device()`
- [ ] `ninja PXView` 链接通过,无 multiple definition 错误
- [ ] `ninja PXView` 链接后 `nm PXView.exe | grep ' U sr_'` 显示 PXView fork 的 sr_* 是已定义符号,libsigrokstd 的 sr_* 不出现在 PXView.exe 符号表中

## Phase 5: 部署与验证

- [ ] `cmake/install_packaging.cmake` 添加 `install(TARGETS libsigrokstd RUNTIME DESTINATION bin LIBRARY DESTINATION lib)`
- [ ] Linux/macOS 已配置 RPATH(`$ORIGIN/../lib` 或 `@loader_path/../Frameworks`)
- [ ] `cd build && ninja -j 16 && ninja install` 成功
- [ ] `install.dir/bin/PXView.exe` 存在
- [ ] `install.dir/bin/libsigrokstd.dll` 存在(Windows)/ `install.dir/lib/libsigrokstd.so` 存在(Linux)
- [ ] 启动 PXView.exe(GUI 模式)不崩溃
- [ ] 启动日志包含 "libsigrokstd initialized" 字样
- [ ] `nm -D install.dir/bin/libsigrokstd.dll | grep ' T '` 仅显示 `srstd_*` 开头的导出函数(无 `sr_*` 内部符号泄露)
- [ ] PXView 原生驱动(DSL/pxlogic/demo/common)扫描正常,设备列表显示
- [ ] (如有硬件)至少 1 个 srstd 上游驱动(slogic 等)可扫描 + 打开 + 采集数据
- [ ] `.trae/specs/dual-libsigrok-coexist-restore-features/checklist.md` 已标注未接入条目 "[实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]"

## 跨平台验证(如适用)

- [ ] Windows 10/11: `libsigrokstd.dll` 加载成功,PXView.exe 启动无 DLL not found 错误
- [ ] Linux(Ubuntu 22.04+): `libsigrokstd.so` 加载成功,`ldd PXView` 显示 `libsigrokstd.so => ./lib/libsigrokstd.so`
- [ ] macOS(如支持): `libsigrokstd.dylib` 加载成功,`otool -L PXView` 显示 `@rpath/libsigrokstd.dylib`

## ABI 安全验证

- [ ] `PXView/pv/deviceagent.cpp` 中 `LIB_SRSTD` 分支调用 `srstd_glue_*` 时所有 struct 指针以 `void*` 传递(无 PXView struct 类型直接解引用上游 struct)
- [ ] `libsigrokstd/bridge/srstd_bridge.c` 是唯一允许同时解引用两库 struct 的代码(转换层)
- [ ] `PXView/pv/sigsession.cpp` 不 `#include <libsigrok/libsigrok.h>`(上游版本,仅 libsigrokstd 内部用)
- [ ] PXView 调用方仅 include `bridge/srstd_pxview_glue.h`(void* 接口)
