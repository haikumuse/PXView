# Checklist

## 目录与文件重命名

- [x] `libsigrokstd/` 目录已重命名为 `libsigrok/`
- [x] `srstd_compat.h` 已重命名为 `sr_compat.h`
- [x] `srstd_compat.c` 已重命名为 `sr_compat.c`
- [x] 3 个测试文件 `test_srstd_*.c` 已删除（整个 tests/ 目录删除，死代码清理）
- [x] `srstd_export.h` 已删除（无源代码 include）

## CMake 配置

- [x] `libsigrok/CMakeLists.txt` 中 target 名为 `libsigrok`
- [x] 所有变量名使用 `SR_*` 前缀（非 `SRSTD_*`）
- [x] 顶层 `CMakeLists.txt` 使用 `add_subdirectory(libsigrok)`
- [x] 顶层 `CMakeLists.txt` include 路径为 `${CMAKE_SOURCE_DIR}/libsigrok/include`
- [x] 顶层 `CMakeLists.txt` 链接 `libsigrok`（非 `libsigrokstd`）
- [x] `CMake/deps.cmake` 注释路径为 `./libsigrok/include`
- [x] `CMake/install_packaging.cmake` 使用 `install(TARGETS libsigrok ...)`

## 源代码修改

- [x] `libsigrok/src/config.h` 中 `PACKAGE` 和 `PACKAGE_NAME` 为 `"libsigrok"`
- [x] `libsigrok/src/config.h` include guard 为 `SR_CONFIG_H`
- [x] `libsigrok/src/sr_compat.h` include guard 为 `SR_COMPAT_H`
- [x] `libsigrok/src/sr_compat.c` 注释中无 `srstd_compat` 残留
- [x] `libsigrok/include/libsigrok/version.h` 注释中无 `libsigrokstd` 残留

## 注释清理

- [x] `PXView/pv/` 下所有 `.cpp` / `.h` 文件中 `libsigrokstd` 已改为 `libsigrok`
- [x] `PXView/pv/` 下所有 `.cpp` / `.h` 文件中 `srstd`（指代库名时）已改为 `sr`
- [x] `AGENTS.md` 中 `libsigrokstd/` 已改为 `libsigrok/`
- [x] `AGENTS.md` 中 `libsigrokstd 0.6.0` 已改为 `libsigrok 0.6.0`
- [x] `devdoc/Debugging DSView Event Loop.md` 中 `libsigrokstd` 已改为 `libsigrok`

## 编译验证

- [x] `cd build && ninja -j 16` 编译通过，0 error
- [x] `ninja install` 安装成功
- [x] `install.dir/bin/` 中存在 libsigrok 库文件（实际为 `liblibsigrok.dll`，历史双前缀问题，旧 `liblibsigrokstd.dll` 已清理）
- [x] `install.dir/bin/PXView.exe` GUI 启动正常（5 秒存活测试通过）
- [x] `PXView.exe --headless` 启动正常
- [x] MCP API 17 tools 全部响应正常（tools/list 返回 17 个工具，HTTP 200）

## grep 验证无残留

- [x] `grep -r "libsigrokstd" PXView/ libsigrok/ CMake/ CMakeLists.txt AGENTS.md devdoc/` 无残留（仅 `.trae/specs/` 历史文档除外）
- [x] `grep -r "SRSTD_API|SRSTD_BUILDING_DLL|srstd_export|srstd_compat"` 在源代码中无残留
- [x] `grep -r "SRSTD_UPSTREAM_SOURCES|SRSTD_DRIVER_LIST|libsigrokstd_SOURCES" libsigrok/CMakeLists.txt` 无残留

## 不改动项验证

- [x] `.trae/specs/` 下历史 spec 文档未被修改（grep 残留仅出现在历史文档中，符合 spec 要求）
- [x] `tools/` 下的 Python 脚本未被修改（不在构建路径，保留原貌）
- [x] 上游 libsigrok 源代码未被修改（除 `config.h` 的 PACKAGE 字符串）

## 已知历史问题（已修复）

- [x] `liblibsigrok.dll` 双前缀问题：已在 `libsigrok/CMakeLists.txt` 中添加 `OUTPUT_NAME "sigrok"`，CMake 现在生成 `libsigrok.dll`（lib 前缀 + sigrok），与 Linux/macOS 输出名一致。target 名仍为 `libsigrok`（语义清晰），仅 artifact 文件名归一化。验证：`ninja -j 16` 成功，`libsigrok.dll` 6.96MB，PXView headless 启动正常，MCP 17 tools 响应正常，旧 `liblibsigrok.dll` / `liblibsigrokstd.dll` 残留已清理。
