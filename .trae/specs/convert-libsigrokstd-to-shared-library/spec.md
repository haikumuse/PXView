# 切换 libsigrokstd 为动态库方案 Spec

## Why

前序 spec `dual-libsigrok-coexist-restore-features` 声称完成"双 libsigrok 共存(方案 B 静态库 + 宏注入)",但实际验证发现:

1. **libsigrokstd 未接入 PXView**:顶层 [CMakeLists.txt](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/CMakeLists.txt) 缺少 `add_subdirectory(libsigrokstd)`、PXView target 未链接、[sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/sigsession.cpp) 0 处 `srstd_*` 调用、DeviceAgent 0 处 `LIB_SRSTD` 分支。**82 个迁移驱动实际未进入 PXView.exe**。
2. **静态库 + 宏注入方案的根本缺陷**:`srstd_rename.h` 通过 `#define sr_xxx srstd_xxx` 重命名 143 个 SR_API 函数 + 29 个 struct 标签,污染全局命名空间(数千个 `srstd_*` 符号),且每个迁移驱动需修改 `protocol.h`(加 `#include "srstd.h"`)+ `api.c`(去 `static`)共 2 处,破坏上游可追踪性。
3. **`__attribute__((visibility("hidden")))` 在静态库下无效**:静态库链接时所有全局符号进入同一符号表,visibility 属性被忽略,符号冲突无法用此机制规避。

**本 spec 切换为动态库方案**:libsigrokstd 编译为 `libsigrokstd.dll`(Windows)/ `libsigrokstd.so`(Linux)/ `libsigrokstd.dylib`(macOS),通过 `-fvisibility=hidden` 隐藏内部符号,仅导出 `srstd_*` 包装 API。运行期由 DLL/SO 边界物理隔离符号空间,**驱动源码零修改** + **全局命名空间干净** + **类型安全由 void* 接口保证**。

## What Changes

### A. 切换 libsigrokstd 为 SHARED 库

- 修改 [libsigrokstd/CMakeLists.txt](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokstd/CMakeLists.txt):
  - `add_library(libsigrokstd STATIC ...)` → `add_library(libsigrokstd SHARED ...)`
  - 输出从 `ARCHIVE_OUTPUT_DIRECTORY` 改为 `LIBRARY_OUTPUT_DIRECTORY`/`RUNTIME_OUTPUT_DIRECTORY`(Windows DLL 是 RUNTIME)
  - 增加 `-fvisibility=hidden` + `-fvisibility-inlines-hidden` 编译选项
  - 移除 `target_compile_options(... -include srstd_rename.h)`
  - 链接 libusb-1.0 为 SHARED(避免 libusb 静态符号也进入 libsigrokstd.dll)

### B. 引入跨平台导出宏 SRSTD_API

- 新增 [libsigrokstd/include/srstd_export.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokstd/include/srstd_export.h):
  ```c
  #if defined(_WIN32)
    #if defined(SRSTD_BUILDING_DLL)
      #define SRSTD_API __declspec(dllexport)
    #else
      #define SRSTD_API __declspec(dllimport)
    #endif
  #else
    #define SRSTD_API __attribute__((visibility("default")))
  #endif
  ```
- [libsigrokstd/bridge/srstd_pxview_glue.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokstd/bridge/srstd_pxview_glue.h) 所有公开函数加 `SRSTD_API` 前缀
- CMake 中 `target_compile_definitions(libsigrokstd PRIVATE SRSTD_BUILDING_DLL=1)`

### C. 移除宏注入基础设施

- 删除 `libsigrokstd/include/srstd_rename.h`(由 `tools/gen_srstd_rename.py` 生成,脚本保留作为历史参考)
- 删除 `libsigrokstd/include/srstd.h`(聚合头,改为直接 include 上游 `libsigrok/libsigrok.h`)
- 删除 `tools/migrate_drivers_to_srstd.py`(脚本生成的修改将被还原)

### D. 还原驱动源码修改(**零修改**)

- 批量还原 82 个迁移驱动:
  - `protocol.h` 删除 `#include "srstd.h"`(动态库边界隔离,无需重命名)
  - `api.c` 恢复 `static`(动态库内部 static 符号不导出,无需手动去 static)
- `SR_REGISTER_DEV_DRIVER` 宏保留手动注册路径(Windows DLL 中 section 机制仍不可靠),`driver_registry.c` 不变
- **结果**:驱动源码回到上游原始状态,可任意 cherry-pick 上游更新

### E. 调整胶水层(不再依赖宏重命名)

- [libsigrokstd/bridge/srstd_pxview_glue.c](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokstd/bridge/srstd_pxview_glue.c):
  - 内部直接调用上游 `sr_config_get` / `sr_config_set` / `sr_dev_open` / `sr_session_start` 等真实符号(不再有 `srstd_*` 重命名)
  - 引用 `struct sr_dev_inst` / `struct sr_channel` 等真实类型(不再是 `struct srstd_dev_inst`)
  - 包含 `#include <libsigrok/libsigrok.h>`(上游版本,libsigrokstd 的 include 路径已 BEFORE)
- [libsigrokstd/bridge/srstd_init_shared.c](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokstd/bridge/srstd_init_shared.c):同上
- [libsigrokstd/bridge/srstd_bridge.c](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/libsigrokstd/bridge/srstd_bridge.c):struct 转换两端都用真实 `struct sr_*` 类型
- **关键约束**:胶水层头文件 `srstd_pxview_glue.h` 继续用 `void*` 接收所有 struct 指针(避免 PXView 调用方 include 上游 libsigrok.h 导致 struct 重定义)

### F. 接入 PXView(**首次接入,之前未做**)

- 顶层 [CMakeLists.txt](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/CMakeLists.txt):
  - 在 `include(${CMAKE_SOURCE_DIR}/cmake/libsigrok.cmake)` 后添加 `add_subdirectory(libsigrokstd)`
  - `target_link_libraries(${PROJECT_NAME} ...)` 增加 `libsigrokstd`
- [PXView/pv/sigsession.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/sigsession.h):
  - 新增成员 `void* _srstd_ctx = nullptr;`
  - 新增成员 `void* _libusb_ctx_cache = nullptr;`(供 srstd_pxview_init_shared 使用)
- [PXView/pv/sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/sigsession.cpp) `init()`:
  - 在 `ds_lib_init()` 成功后调用 `srstd_pxview_init_shared(&_srstd_ctx, ds_get_libusb_context())`
  - 注册数据回流:`srstd_glue_set_datafeed_callback(&DataFeedParser::data_feed_callback_ex, _data_feed_parser.get())`
- [PXView/pv/sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/sigsession.cpp) `uninit()`:
  - 在 `ds_lib_exit()` 前调用 `srstd_pxview_exit(_srstd_ctx)`;`_srstd_ctx = nullptr;`
- [PXView/pv/deviceagent.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/deviceagent.h) / `.cpp`:
  - 新增 `enum DeviceLib { LIB_PXVIEW, LIB_SRSTD };` + `DeviceLib device_lib() const;`
  - `get_config`/`set_config`/`get_config_list`/`start`/`stop`/`is_collecting` 方法按 `device_lib()` 分流:`LIB_SRSTD` 分支调用 `srstd_glue_*`
  - 设备扫描合并:`SigSession::get_device_list()` 调 `srstd_glue_scan_devices()` 合并两库结果,srstd 设备 handle 用 `SRSTD_MAKE_HANDLE(i)` 标记
  - `set_device()`:检测 `SRSTD_IS_HANDLE(handle)` 调 `srstd_glue_open_scanned_device(idx)`

### G. 部署配置

- Windows: `libsigrokstd.dll` 安装到 PXView.exe 同目录(install_packaging.cmake 增加 install TARGETS)
- Linux: `libsigrokstd.so` 安装到 `lib/` 并设置 RPATH `$ORIGIN`
- macOS: `libsigrokstd.dylib` 安装到 `Frameworks/`,RPATH `@loader_path/../Frameworks`

### H. 修正前序 spec 状态

- 修改 `.trae/specs/dual-libsigrok-coexist-restore-features/checklist.md`:把"Task 7/8/9/10 已完成 [x]"改为"[未接入 PXView — 由 convert-libsigrokstd-to-shared-library 接管]"
- 不删除该 spec 文档(保留作为方案 B 静态库路径的历史记录)

## Impact

- **Affected specs**:
  - `dual-libsigrok-coexist-restore-features`(方案 B 静态库 — 标注未接入部分)
  - `migrate-all-compat-drivers-to-libsigrokstd`(迁移脚本 — 脚本将弃用)
  - `migrate-all-sigrok-drivers` / `migrate-remaining-drivers-register`(批量注册 — driver_registry.c 保留)
  - `audit-and-fix-migrated-drivers`(驱动审计 — 静态库特定问题不再适用)
- **Affected code**:
  - `libsigrokstd/CMakeLists.txt`(STATIC → SHARED + visibility)
  - `libsigrokstd/include/srstd_rename.h`(删除)
  - `libsigrokstd/include/srstd.h`(删除)
  - `libsigrokstd/include/srstd_export.h`(新增)
  - `libsigrokstd/bridge/srstd_pxview_glue.{h,c}`(API 加 SRSTD_API + 内部调用真实 sr_*)
  - `libsigrokstd/bridge/srstd_init_shared.c`(同上)
  - `libsigrokstd/bridge/srstd_bridge.{h,c}`(同上)
  - `libsigrokstd/src/driver_registry.c`(保持手动注册,不变)
  - 82 个迁移驱动的 `protocol.h` + `api.c`(批量还原上游状态)
  - `CMakeLists.txt`(顶层 — add_subdirectory + 链接)
  - `PXView/pv/sigsession.{h,cpp}`(init/uninit 接入)
  - `PXView/pv/deviceagent.{h,cpp}`(LIB_SRSTD 分流)
  - `cmake/install_packaging.cmake`(安装 .dll/.so/.dylib)
- **ABI 风险**:`struct sr_dev_inst`/`struct sr_channel` 在 PXView fork 与上游版本字段布局不同,跨 DLL 边界必须用 `void*` + accessor 函数,禁止直接解引用。胶水层 `srstd_bridge.c` 内部是唯一允许解引用两库 struct 的代码。

## ADDED Requirements

### Requirement: libsigrokstd 作为动态库构建

系统 SHALL 将 libsigrokstd 构建为 SHARED 库(`.dll`/`.so`/`.dylib`),通过 `-fvisibility=hidden` 隐藏内部符号,仅导出标记为 `SRSTD_API` 的函数。

#### Scenario: 编译期符号隔离
- **WHEN** libsigrokstd 编译完成
- **THEN** 内部 `sr_*` 符号不进入动态符号表(`nm -D libsigrokstd.so` 仅显示 `srstd_*` 开头的导出函数)
- **AND** PXView.exe 链接后无符号冲突(`nm PXView.exe | grep ' T sr_'` 仅显示 PXView fork 的符号)

#### Scenario: 跨平台导出宏
- **WHEN** 在 Windows 编译 libsigrokstd 时(`SRSTD_BUILDING_DLL` 定义)
- **THEN** `SRSTD_API` 展开为 `__declspec(dllexport)`
- **WHEN** PXView 链接 libsigrokstd.dll 时
- **THEN** `SRSTD_API` 展开为 `__declspec(dllimport)`
- **WHEN** 在 Linux/macOS 编译时
- **THEN** `SRSTD_API` 展开为 `__attribute__((visibility("default")))`

### Requirement: libsigrokstd 驱动源码零修改

系统 SHALL 保持 82 个迁移驱动源码与上游完全一致(无 `#include "srstd.h"`、无 `static` 移除)。

#### Scenario: 上游同步
- **WHEN** 上游 libsigrok 发布新版本驱动
- **THEN** 可直接覆盖 `libsigrokstd/src/hardware/<driver>/` 目录文件并重新构建,无需任何手工合并

### Requirement: libsigrokstd 接入 PXView 主程序

系统 SHALL 在 PXView 启动时初始化 libsigrokstd,共享 libusb_context,合并上游驱动扫描结果到设备列表。

#### Scenario: 启动接入
- **WHEN** PXView 启动调用 `SigSession::init()`
- **THEN** `ds_lib_init()` 成功后调用 `srstd_pxview_init_shared(&_srstd_ctx, libusb_ctx)`
- **AND** 调用 `srstd_glue_set_datafeed_callback()` 注册数据回流
- **AND** 启动日志包含 "libsigrokstd initialized" 字样

#### Scenario: 设备列表合并
- **WHEN** 用户打开设备选择下拉框
- **THEN** 列表包含 PXView fork 的 4 个原生驱动(DSL/pxlogic/demo/common)扫描结果
- **AND** 包含上游 82 个迁移驱动扫描结果(以 `SRSTD_MAKE_HANDLE(i)` 标记 handle 高位)

#### Scenario: 设备分流
- **WHEN** 用户选中一个 srstd 设备(`SRSTD_IS_HANDLE(handle)` 为真)
- **THEN** DeviceAgent 切换到 `LIB_SRSTD` 模式
- **AND** 后续 `get_config`/`set_config`/`start`/`stop` 调用走 `srstd_glue_*` 分支

### Requirement: libusb_context 跨 DLL 共享

系统 SHALL 在 PXView 与 libsigrokstd 之间共享同一个 libusb_context,避免 USB 设备枚举冲突。

#### Scenario: 共享 libusb_context
- **WHEN** `srstd_pxview_init_shared()` 被调用
- **THEN** 上游 `sr_init()` 创建的 libusb_context 被释放
- **AND** 替换为 PXView 的 libusb_context 指针
- **WHEN** `srstd_pxview_exit()` 被调用
- **THEN** 上游调用 `sr_exit()` 前先置 libusb_ctx=NULL,避免释放共享 context

## MODIFIED Requirements

### Requirement: libsigrokstd 符号隔离机制(原方案 B 静态库 + 宏注入)

[原 spec `dual-libsigrok-coexist-restore-features` 的方案 B 静态库 + 宏注入路径已废弃。改为动态库 + visibility 路径。胶水层 API 保持不变(`srstd_glue_*` / `srstd_pxview_*` 函数签名一致),仅内部实现从调用 `srstd_xxx`(被 `srstd_rename.h` 重命名)改为直接调用上游真实 `sr_xxx`。]

## REMOVED Requirements

### Requirement: srstd_rename.h 宏注入符号隔离

**Reason**: 静态库架构下必需的编译期符号重命名,在动态库方案下由 `-fvisibility=hidden` + DLL 边界替代。宏注入污染全局命名空间(数千 `srstd_*` 符号),且强制每个迁移驱动改 2 处源码。

**Migration**:
- 删除 `libsigrokstd/include/srstd_rename.h`
- 删除 `libsigrokstd/include/srstd.h`
- 删除 `tools/migrate_drivers_to_srstd.py`(脚本已无用)
- 还原 82 个迁移驱动 `protocol.h`(去 `#include "srstd.h"`)和 `api.c`(恢复 `static`)— 用一个新脚本 `tools/restore_drivers_from_srstd.py` 批量还原,或直接从上游 git 重新 cherry-pick
- 胶水层内部 `srstd_xxx` 调用改为真实 `sr_xxx`(由 `srstd_rename.h` 提供的 `#define` 全部失效)

### Requirement: 静态库 + 全局符号污染的链接方式

**Reason**: 静态库链接到 PXView.exe 时所有符号进入同一全局表,82 个驱动的 `sr_*` 内部符号与 PXView fork 的 `sr_*` 冲突,只能靠宏注入规避。

**Migration**: 改为 SHARED 库后,libsigrokstd.dll 有独立符号表,内部 `sr_*` 不进入 PXView.exe 全局符号空间,无需任何重命名。
