# Tasks

- [x] Task 1: 目录重命名 `libsigrokstd/` → `libsigrok/`
  - [x] SubTask 1.1: 使用 `git mv libsigrokstd libsigrok` 重命名目录（保留 git 历史）
  - [x] SubTask 1.2: 验证目录重命名后文件结构完整

- [x] Task 2: 文件重命名（库内部）
  - [x] SubTask 2.1: `git mv libsigrok/src/srstd_compat.h libsigrok/src/sr_compat.h`
  - [x] SubTask 2.2: `git mv libsigrok/src/srstd_compat.c libsigrok/src/sr_compat.c`
  - [x] SubTask 2.3: `git mv libsigrok/tests/test_srstd_init_shared.c libsigrok/tests/test_sr_init_shared.c`（后改为删除整个 tests/ 目录，见 Task 3 备注）

> 备注：tests/ 目录下的 test_srstd_*.c 引用已删除的 srstd.h / srstd_bridge.h 头文件，是 EXCLUDE_FROM_ALL 的死代码。最终决定整个 tests/ 目录删除，并从 CMakeLists.txt 删除对应 test target 定义。Task 2.3-2.5 的重命名动作被 "整目录删除" 取代。

- [x] Task 3: 删除冗余文件
  - [x] SubTask 3.1: `git rm libsigrok/include/srstd_export.h`（无源代码 include，可安全删除）
  - [x] SubTask 3.2（新增）: 删除整个 `libsigrok/tests/` 目录（死代码，见上备注）

- [x] Task 4: 修改 `libsigrok/CMakeLists.txt`
  - [x] SubTask 4.1: 头部注释 `libsigrokstd` → `libsigrok`
  - [x] SubTask 4.2: 变量名 `SRSTD_UPSTREAM_SOURCES` → `SR_UPSTREAM_SOURCES`
  - [x] SubTask 4.3: 变量名 `SRSTD_DRIVER_LIST_START/STOP` → `SR_DRIVER_LIST_START/STOP`
  - [x] SubTask 4.4: 变量名 `libsigrokstd_SOURCES` → `libsigrok_SOURCES`
  - [x] SubTask 4.5: target 名 `add_library(libsigrokstd SHARED ...)` → `add_library(libsigrok SHARED ...)`
  - [x] SubTask 4.6: 所有 `target_compile_options/definitions/include_directories/link_libraries(libsigrokstd ...)` → `(libsigrok ...)`
  - [x] SubTask 4.7: `set_source_files_properties` 中 `srstd_compat.h` → `sr_compat.h`
  - [x] SubTask 4.8: 注释中 `srstd_export.h` / `SRSTD_API` / `SRSTD_BUILDING_DLL` 引用清理
  - [x] SubTask 4.9: 测试 target 定义全部删除（源文件已删，替换为说明注释）

- [x] Task 5: 修改 `libsigrok/src/config.h`
  - [x] SubTask 5.1: 头部注释 `libsigrokstd` → `libsigrok`
  - [x] SubTask 5.2: include guard `SRSTD_CONFIG_H` → `SR_CONFIG_H`
  - [x] SubTask 5.3: `PACKAGE "libsigrokstd"` → `PACKAGE "libsigrok"`
  - [x] SubTask 5.4: `PACKAGE_NAME "libsigrokstd"` → `PACKAGE_NAME "libsigrok"`

- [x] Task 6: 修改 `libsigrok/src/sr_compat.h`（原 srstd_compat.h）
  - [x] SubTask 6.1: 头部注释 `srstd_compat` → `sr_compat`
  - [x] SubTask 6.2: include guard `SRSTD_COMPAT_H` → `SR_COMPAT_H`

- [x] Task 7: 修改 `libsigrok/src/sr_compat.c`（原 srstd_compat.c）
  - [x] SubTask 7.1: 头部注释 `srstd_compat` → `sr_compat`

- [x] Task 8: 修改 `libsigrok/include/libsigrok/version.h`
  - [x] SubTask 8.1: 头部注释 `libsigrokstd` → `libsigrok`

- [x] Task 9: 修改顶层 `CMakeLists.txt`
  - [x] SubTask 9.1: 注释 `libsigrokstd` → `libsigrok`
  - [x] SubTask 9.2: `add_subdirectory(libsigrokstd)` → `add_subdirectory(libsigrok)`
  - [x] SubTask 9.3: include 路径 `${CMAKE_SOURCE_DIR}/libsigrokstd/include` → `${CMAKE_SOURCE_DIR}/libsigrok/include`
  - [x] SubTask 9.4: `target_link_libraries(... libsigrokstd)` → `target_link_libraries(... libsigrok)`

- [x] Task 10: 修改 `CMake/deps.cmake`
  - [x] SubTask 10.1: 注释 `./libsigrokstd/include` → `./libsigrok/include`

- [x] Task 11: 修改 `CMake/install_packaging.cmake`
  - [x] SubTask 11.1: 注释 `libsigrokstd.dll/.so/.dylib` → `libsigrok.dll/.so/.dylib`
  - [x] SubTask 11.2: `install(TARGETS libsigrokstd ...)` → `install(TARGETS libsigrok ...)`

- [x] Task 12: 清理 PXView 源代码注释（约 38 处）
  - [x] SubTask 12.1: `PXView/pv/sigsession.cpp` / `sigsession.h` 中 `libsigrokstd` → `libsigrok`（`_srstd_ctx` 变量名保留不改）
  - [x] SubTask 12.2: `PXView/pv/deviceagent.cpp` 中 `libsigrokstd` → `libsigrok`
  - [x] SubTask 12.3: `PXView/pv/dsvdef.h` 中 `libsigrokstd` → `libsigrok`
  - [x] SubTask 12.4: `PXView/pv/data/signalmodel.cpp` 中 `libsigrokstd` → `libsigrok`
  - [x] SubTask 12.5: `PXView/pv/data/signalconfigstore.cpp` 中 `libsigrokstd` → `libsigrok`
  - [x] SubTask 12.6: `PXView/pv/data/analogsnapshot.cpp` 中 `libsigrokstd` / `srstd` → `libsigrok` / `sr`
  - [x] SubTask 12.7: `PXView/pv/storesession.cpp` 中 `libsigrokstd` → `libsigrok`
  - [x] SubTask 12.8: `PXView/pv/toolbars/samplingbar.cpp` 中 `srstd` → `sr`

- [x] Task 13: 更新 `AGENTS.md`
  - [x] SubTask 13.1: `libsigrokstd/` → `libsigrok/`
  - [x] SubTask 13.2: `libsigrokstd 0.6.0` → `libsigrok 0.6.0`

- [x] Task 14: 更新 `devdoc/Debugging DSView Event Loop.md`
  - [x] SubTask 14.1: `libsigrokstd` → `libsigrok`

- [x] Task 15: 编译验证
  - [x] SubTask 15.1: 执行 `cd build && ninja -j 16`，确认 0 error（1222/1223 编译目标完成，exit code 0）
  - [x] SubTask 15.2: 执行 `ninja install`，确认安装成功
  - [x] SubTask 15.3: 验证 `install.dir/bin/` 中 libsigrok 库文件存在（实际为 `liblibsigrok.dll`，CMake 在 Windows 上对 target 名 `libsigrok` 自动加 `lib` 前缀导致双前缀；这是历史问题，原 `libsigrokstd` target 也生成 `liblibsigrokstd.dll`，非本次改名引入。已清理旧的 `liblibsigrokstd.dll` 残留）
  - [x] SubTask 15.4: 验证 `install.dir/bin/PXView.exe` GUI 启动正常（5 秒存活测试通过）
  - [x] SubTask 15.5: 验证 headless 模式 `PXView.exe --headless` 启动正常（PID 正常分配，HTTP 10110 监听正常）
  - [x] SubTask 15.6: 验证 MCP API 17 tools 全部响应正常（tools/list 返回 17 个工具，HTTP 200）

- [x] Task 16: grep 验证无残留
  - [x] SubTask 16.1: `grep -r "libsigrokstd"` 在 `PXView/` / `libsigrok/` / `CMake/` / `CMakeLists.txt` / `AGENTS.md` / `devdoc/` 中无残留（仅 `.trae/specs/` 历史文档除外）
  - [x] SubTask 16.2: `grep -r "SRSTD_API|SRSTD_BUILDING_DLL|srstd_export|srstd_compat"` 在 `PXView/` / `libsigrok/` / `CMake/` / `CMakeLists.txt` 中无残留
  - [x] SubTask 16.3（新增）: `grep -r "SRSTD_UPSTREAM_SOURCES|SRSTD_DRIVER_LIST|libsigrokstd_SOURCES"` 在 `libsigrok/CMakeLists.txt` 中无残留

# Task Dependencies

- Task 1 (目录重命名) 必须先完成，Task 2-8 依赖目录已重命名
- Task 2 (文件重命名) 必须先完成，Task 4-8 依赖文件已重命名
- Task 3 (删除文件) 可与 Task 2 并行
- Task 4-8 (库内部修改) 可并行
- Task 9-11 (CMake 配置) 可并行，但必须在 Task 1 后
- Task 12 (PXView 注释清理) 可与 Task 4-11 并行
- Task 13-14 (文档更新) 可与 Task 4-12 并行
- Task 15 (编译验证) 必须在 Task 1-14 全部完成后
- Task 16 (grep 验证) 必须在 Task 1-14 全部完成后

# 已知历史问题（已修复）

- [x] **`liblibsigrok.dll` 双前缀**：CMake 在 Windows 上对 `add_library(libsigrok SHARED ...)` 自动生成 `liblibsigrok.dll`（`lib` 前缀 + target 名 `libsigrok`）。这是历史问题（原 `libsigrokstd` target 也生成 `liblibsigrokstd.dll`）。**已修复**：在 `libsigrok/CMakeLists.txt` 的 output directory set_target_properties 中添加 `OUTPUT_NAME "sigrok"`，CMake 现在生成 `libsigrok.dll`（lib 前缀 + sigrok），与 Linux/macOS 输出名一致。target 名仍为 `libsigrok`（语义清晰），仅 artifact 文件名归一化。install(TARGETS libsigrok ...) 自动按 OUTPUT_NAME 安装正确文件，无需改 install_packaging.cmake。验证：`ninja -j 16` 成功生成 `libsigrok.dll`（6.96MB），PXView headless 启动正常，MCP 17 tools 响应正常，旧 `liblibsigrok.dll` / `liblibsigrokstd.dll` 残留已清理。
