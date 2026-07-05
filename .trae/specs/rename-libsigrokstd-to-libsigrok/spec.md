# 重命名 libsigrokstd → libsigrok Spec

## Why

`libsigrokstd` 这个名字是 PXView 区分 fork libsigrok 与上游 libsigrok 的历史产物。fork libsigrok 已在 `migrate-pxlogic-to-libsigrokstd` spec 中删除，`libsigrokstd` 现在是 PXView 唯一的 libsigrok 实现，名字中的 "std" 后缀已无意义，反而造成混淆（让人以为还有 "non-std" 版本）。

本 spec 将库整体重命名为 `libsigrok`，消除历史包袱，让 PXView 的 libsigrok 实现命名与上游一致。

## What Changes

### A. 目录与文件重命名

- **目录**：`libsigrokstd/` → `libsigrok/`
- **文件**：
  - `libsigrokstd/src/srstd_compat.h` → `libsigrok/src/sr_compat.h`
  - `libsigrokstd/src/srstd_compat.c` → `libsigrok/src/sr_compat.c`
  - `libsigrokstd/tests/test_srstd_init_shared.c` → `libsigrok/tests/test_sr_init_shared.c`
  - `libsigrokstd/tests/test_srstd_bridge.c` → `libsigrok/tests/test_sr_bridge.c`
  - `libsigrokstd/tests/test_srstd_trigger.c` → `libsigrok/tests/test_sr_trigger.c`

### B. 文件删除

- **删除** `libsigrokstd/include/srstd_export.h`
  - **原因**：bridge/ 目录已删除，无任何源代码 `#include` 此文件
  - **替代**：上游 `libsigrok.h` 已定义 `SR_API`/`SR_PRIV` 宏，行为与 `SRSTD_API` 完全相同（Windows 上均为空，Linux 上均为 `__attribute__((visibility("default")))`）

### C. CMake 配置改名

- target 名：`libsigrokstd` → `libsigrok`
- 变量名：
  - `libsigrokstd_SOURCES` → `libsigrok_SOURCES`
  - `SRSTD_UPSTREAM_SOURCES` → `SR_UPSTREAM_SOURCES`
  - `SRSTD_DRIVER_LIST_START` → `SR_DRIVER_LIST_START`
  - `SRSTD_DRIVER_LIST_STOP` → `SR_DRIVER_LIST_STOP`
- 路径：`libsigrokstd/include` → `libsigrok/include`
- `add_subdirectory(libsigrokstd)` → `add_subdirectory(libsigrok)`
- `target_link_libraries(... libsigrokstd)` → `target_link_libraries(... libsigrok)`
- `install(TARGETS libsigrokstd ...)` → `install(TARGETS libsigrok ...)`
- `set_source_files_properties` 中的 `srstd_compat.h` → `sr_compat.h`

### D. 宏与符号改名

- `SRSTD_API` → **删除**（改用上游 `SR_API`）
- `SRSTD_BUILDING_DLL` → **删除**（CMake 中本就未定义，仅注释提及）
- `SRSTD_COMPAT_H` include guard → `SR_COMPAT_H`
- `SRSTD_CONFIG_H` include guard → `SR_CONFIG_H`

### E. PACKAGE 字符串（config.h）

- `PACKAGE "libsigrokstd"` → `PACKAGE "libsigrok"`
- `PACKAGE_NAME "libsigrokstd"` → `PACKAGE_NAME "libsigrok"`

### F. 注释清理（约 38 处）

- PXView/pv/ 下所有注释中的 `libsigrokstd` → `libsigrok`
- PXView/pv/ 下所有注释中的 `srstd` → `sr`（仅在指代库名时）
- CMakeLists.txt / deps.cmake / install_packaging.cmake 注释同步更新
- AGENTS.md 中 `libsigrokstd/` → `libsigrok/`，`libsigrokstd 0.6.0` → `libsigrok 0.6.0`

### G. 输出文件名变化

- `libsigrokstd.dll` → `libsigrok.dll`
- `libsigrokstd.lib` → `libsigrok.lib`
- `libsigrokstd.so` → `libsigrok.so`（Linux）
- `libsigrokstd.dylib` → `libsigrok.dylib`（macOS）

## Impact

- **Affected specs**：
  - `convert-libsigrokstd-to-shared-library`（历史，不改）
  - `migrate-pxlogic-to-libsigrokstd`（历史，不改）
  - `migrate-all-compat-drivers-to-libsigrokstd`（历史，不改）
  - `dual-libsigrok-coexist-restore-features`（历史，不改）
  - `.trae/specs/` 下所有历史 spec 文档**保持原貌**，不改动（历史记录）

- **Affected code**：
  - `libsigrokstd/` → `libsigrok/`（整个目录）
  - `CMakeLists.txt`（顶层）
  - `CMake/deps.cmake`
  - `CMake/install_packaging.cmake`
  - `PXView/pv/*.cpp` / `*.h`（注释清理约 38 处）
  - `AGENTS.md`
  - `devdoc/Debugging DSView Event Loop.md`

- **不影响**：
  - 上游 libsigrok 源代码（100% unmodified 原则）
  - `tools/` 下的 Python 脚本（一次性工具，不在构建路径，保留原貌）
  - libsigrokdecode / common / PXView 的功能代码

## 不改动项

1. `.trae/specs/` 下的历史 spec 文档（保留原貌，作为历史记录）
2. `tools/` 下的 Python 脚本（`restore_drivers_from_srstd.py` 等，一次性工具，不在构建路径）
3. 上游 libsigrok 源代码（100% unmodified 原则，只改 config.h 的 PACKAGE 字符串）
4. 固件打包（属于后续独立 spec，本 spec 只做改名）

## ADDED Requirements

### Requirement: 库名统一为 libsigrok

系统 SHALL 将 PXView 的 libsigrok 实现命名为 `libsigrok`，不再使用 `libsigrokstd` 名字。

#### Scenario: 编译输出

- **WHEN** 执行 `ninja -j 16`
- **THEN** 生成 `libsigrok.dll`（Windows）/ `libsigrok.so`（Linux）/ `libsigrok.dylib`（macOS）
- **AND** 不存在 `libsigrokstd.*` 文件

#### Scenario: CMake target 引用

- **WHEN** 顶层 CMakeLists.txt 引用 libsigrok 库
- **THEN** 使用 `target_link_libraries(... libsigrok)` 而非 `libsigrokstd`
- **AND** 使用 `add_subdirectory(libsigrok)` 而非 `libsigrokstd`

### Requirement: 删除冗余导出宏头文件

系统 SHALL 删除 `srstd_export.h`，改用上游 `libsigrok.h` 中已定义的 `SR_API`/`SR_PRIV` 宏。

#### Scenario: 编译时无 SRSTD_API 残留

- **WHEN** 编译 libsigrok 库
- **THEN** 不存在 `SRSTD_API` 宏定义
- **AND** 不存在 `srstd_export.h` 文件
- **AND** `WINDOWS_EXPORT_ALL_SYMBOLS=ON` 仍然正常工作

## MODIFIED Requirements

### Requirement: libsigrok 共享库构建

[原] libsigrokstd 构建为 SHARED 库，输出 `libsigrokstd.dll`，通过 `WINDOWS_EXPORT_ALL_SYMBOLS=ON` 导出所有符号，`SRSTD_API` 在 Windows 上为空以配合此机制。

[改] libsigrok 构建为 SHARED 库，输出 `libsigrok.dll`，通过 `WINDOWS_EXPORT_ALL_SYMBOLS=ON` 导出所有符号，上游 `SR_API` 在 Windows 上为空以配合此机制。

## REMOVED Requirements

### Requirement: SRSTD_API 导出宏

**Reason**：`srstd_export.h` 定义的 `SRSTD_API` 宏与上游 `libsigrok.h` 的 `SR_API` 宏行为完全相同（Windows 均为空，Linux 均为 `__attribute__((visibility("default")))`）。bridge/ 目录已删除，无任何源代码 `#include "srstd_export.h"`，此宏已成为死代码。

**Migration**：直接删除 `srstd_export.h` 文件和 `SRSTD_API` 宏定义。需要导出宏的代码改用上游 `SR_API`/`SR_PRIV`（已存在于 `libsigrok.h` 中）。

### Requirement: SRSTD_BUILDING_DLL 编译宏

**Reason**：CMake 中本就未定义此宏（仅注释提及），是历史遗留的死代码引用。

**Migration**：删除所有注释中的 `SRSTD_BUILDING_DLL` 引用，无需替代。
