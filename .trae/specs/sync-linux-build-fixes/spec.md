# 同步 Linux 构建修复与消除跨平台警告 Spec

## Why
WSL Ubuntu (GCC 11.4) 构建中发现 3 个编译错误和多个编译警告，这些在 Windows MSVC 构建中未暴露。需要将修复同步回本地代码库，并消除所有跨平台可见的警告，确保 Linux/Windows 双平台零警告构建。

## What Changes
- 修复 `app_service.cpp` 中 `reinterpret_cast<uintptr_t>` 类型转换错误（3处），改为 `static_cast<uint64_t>`
- 修复 `session_service.cpp` 中 `QDateTime::fromMSecsSinceEpoch` deprecated 重载调用
- 修复 `session_service.cpp` 中未使用参数 `keep`
- 修复 `rpc_dispatcher.cpp` 中 `QFile::open` 返回值被忽略（nodiscard）
- 修复 `rpc_dispatcher.cpp` 中未使用参数 `params`
- 修复 `ook_oregon_c.c` 中 `nib_strs` 可能未初始化警告
- 修复 `instance.c` 中 `wanted_term` 变量 set-but-not-used 警告
- 改进 `CMakeLists.txt` 中 nlohmann_json 的 FetchContent 为 find_package 优先 + FetchContent fallback
- 移除 `CMakeLists.txt` 中无条件的 test_font 目标

## Impact
- Affected code: PXView/pv/api/app_service.cpp, session_service.cpp, rpc_dispatcher.cpp, libsigrokdecode/c_decoders/ook_oregon_c.c, libsigrokdecode/instance.c, CMakeLists.txt
- 所有修改不改变运行时行为，仅消除编译错误和警告
- CMakeLists.txt 的 nlohmann_json 改动影响构建系统，需确保 Windows (MSVC) 和 Linux (GCC) 均能正常构建

## ADDED Requirements

### Requirement: 消除 reinterpret_cast 跨平台类型错误
`app_service.cpp` 中 `ds_device_handle` 到字符串的转换 SHALL 使用 `static_cast<uint64_t>` 而非 `reinterpret_cast<uintptr_t>`。

#### Scenario: Linux GCC 编译无类型转换错误
- **WHEN** 使用 GCC 在 Linux 上编译 app_service.cpp
- **THEN** 不产生 "invalid cast from type 'ds_device_handle' {aka 'long long unsigned int'} to type 'uintptr_t' {aka 'long unsigned int'}" 错误

### Requirement: 消除 QDateTime deprecated API 警告
`session_service.cpp` 中 `QDateTime::fromMSecsSinceEpoch` 调用 SHALL 使用非 deprecated 重载。

#### Scenario: Qt 6.9+ 编译无 deprecated 警告
- **WHEN** 使用 Qt 6.9+ 编译 session_service.cpp
- **THEN** 不产生 `QDateTime::fromMSecsSinceEpoch(qint64, Qt::TimeSpec)` deprecated 警告

### Requirement: 消除未使用参数警告
所有 API 层 C++ 代码中未使用的函数参数 SHALL 通过 `(void)param` 或移除参数名来消除警告。

#### Scenario: GCC 编译无 unused-parameter 警告
- **WHEN** 使用 GCC -Wunused-parameter 编译
- **THEN** session_service.cpp 的 show_region 和 rpc_dispatcher.cpp 的 on_list_analyzers 不产生 unused-parameter 警告

### Requirement: 消除 QFile::open nodiscard 警告
`rpc_dispatcher.cpp` 中 `QFile::open` 的返回值 SHALL 被检查或显式忽略。

#### Scenario: GCC 编译无 unused-result 警告
- **WHEN** 使用 GCC 编译 rpc_dispatcher.cpp
- **THEN** 不产生 ignoring return value of 'QFile::open' 警告

### Requirement: 消除 C decoder 未初始化变量警告
`ook_oregon_c.c` 中 `nib_strs` 数组 SHALL 在声明时初始化，消除可能未初始化警告。

#### Scenario: GCC 编译 ook_oregon_c.c 无 maybe-uninitialized 警告
- **WHEN** 使用 GCC -O2 编译 ook_oregon_c.c
- **THEN** 不产生 'nib_strs' may be used uninitialized 警告

### Requirement: 消除 libsigrokdecode wanted_term 警告
`instance.c` 中 `wanted_term` 变量 SHALL 消除 set-but-not-used 警告。

#### Scenario: GCC 编译 instance.c 无 set-but-not-used 警告
- **WHEN** 使用 GCC 编译 instance.c
- **THEN** 不产生 variable 'wanted_term' set but not used 警告

### Requirement: nlohmann_json 构建兼容性
CMakeLists.txt SHALL 优先使用系统安装的 nlohmann_json（find_package），仅在未找到时 fallback 到 FetchContent。

#### Scenario: 系统已安装 nlohmann_json 时直接使用
- **WHEN** 系统已安装 nlohmann-json3-dev 包
- **THEN** CMake 配置直接使用系统包，不尝试 git clone

#### Scenario: 系统未安装 nlohmann_json 时 FetchContent fallback
- **WHEN** 系统未安装 nlohmann_json
- **THEN** CMake 通过 FetchContent 下载并构建

### Requirement: 移除无条件 test_font 目标
CMakeLists.txt 中 test_font 目标 SHALL 被移除或置于 BUILD_TESTING 条件守卫下。

#### Scenario: Release 构建不包含 test_font
- **WHEN** 执行默认 CMake 配置（无 BUILD_TESTING）
- **THEN** 不编译 test_font 可执行文件

## MODIFIED Requirements
（无修改的已有需求）

## REMOVED Requirements
（无移除的已有需求）
