# 闪退堆栈捕获 — 构建验证收尾计划

## 摘要

用户需求："能不能在闪退的时候直接打印闪退堆栈在某个窗口上方便报告问题"。
采用两段式方案：崩溃瞬间写日志 + MessageBoxA 弹窗；下次启动用 addr2line 符号化后展示完整堆栈窗口。

上一会话已完成全部代码实现（5 个崩溃文件 + main.cpp + CMakeLists.txt 改动），但构建在 `ninja -t restat` 阶段因 permission denied 中断，且后续未重试。本计划只负责**完成构建验证**——代码层面无需再改。

## 当前状态分析（已通过 Phase 1 探索验证）

### 已就位的实现文件
| 文件 | 状态 | 说明 |
|------|------|------|
| `PXView/pv/crash/crash_log.h` | ✅ 完整 | 格式契约头，无 Qt/windows.h 依赖；`CRASH_LOG_FILENAME`/`MAGIC`/`MAX_FRAMES`/`CrashLogContext` |
| `PXView/pv/crash/crash_handler.h` | ✅ 完整 | 公开 `install_crash_handler()` |
| `PXView/pv/crash/crash_handler.cpp` | ✅ 完整 | `SetUnhandledExceptionFilter`+`_set_purecall_handler`+`_set_invalid_parameter_handler`+`signal(SIGABRT)`；`RtlCaptureStackBackTrace` 抓栈；`_open`/`_write` 写日志；`MessageBoxA` 弹窗；`_exit` 终止 |
| `PXView/pv/crash/crash_reporter.h` | ✅ 完整 | 公开 `show_crash_report_if_exists(QWidget*)` |
| `PXView/pv/crash/crash_reporter.cpp` | ✅ 完整 | 解析日志 → 定位 addr2line → QProcess 符号化（含 PIE 偏移回退）→ `DSDialog`+`QPlainTextEdit` 展示，附复制/打开日志文件夹/关闭按钮 |

### 已就位的集成点
| 位置 | 改动 | 状态 |
|------|------|------|
| `main.cpp:49` | `#include <QTimer>` | ✅ |
| `main.cpp:52-53` | `#include "pv/crash/crash_handler.h"` + `crash_reporter.h`（`#ifdef _WIN32` 块内） | ✅ |
| `main.cpp:281-286` | `pxv_log_init()` 后调 `pv::crash::install_crash_handler()`（在 `AppControl::Init()` 之前，避免 Python `signal()` 干扰） | ✅ |
| `main.cpp:366-373` | `w.ShowHelpDocAsync()` 后 `QTimer::singleShot(800, &w, ...)` 调 `show_crash_report_if_exists(&w)` | ✅ |
| `CMakeLists.txt:422-423` | WIN32 块 `list(APPEND PXVIEW_GUI_SOURCES)` 加 `crash_handler.cpp`/`crash_reporter.cpp` | ✅ |
| `CMakeLists.txt:555-557` | 顶层 `set(PXView_HEADERS)` 加三个头文件（避免被 WIN32 块覆盖式 set 吃掉） | ✅ |
| `CMakeLists.txt:1437-1439` | Release 块加 `-g`（`CMAKE_CXX_FLAGS_RELEASE`/`CMAKE_C_FLAGS_RELEASE`/`add_compile_options`） | ✅ |
| `CMakeLists.txt:1710-1721` | `find_program(ADDR2LINE_EXE)` + `install(PROGRAMS ... RENAME addr2line.exe)` | ✅ |

### 构建状态
- `build/CMakeCache.txt`：`CMAKE_BUILD_TYPE=Release`、`ENABLE_DRIVER_HUNG_CHANG_DSO_2100=OFF`（上次会话修复 libieee1284 缺失问题，已持久化）
- `build/build.ninja`：已含 `-g` 标志（`FLAGS = -Wall -Wextra -O3 -DNDEBUG -g -O3 -g`），且已注册 `crash_handler.cpp.obj`/`crash_reporter.cpp.obj` 构建目标
- `build/build_log.txt`：停在 `[254/1010]`，说明上次构建被中断
- `build/install.dir/bin/PXView.exe`：**不存在**（构建未完成，产物未生成）

**结论：cmake 配置阶段已完成且正确，build.ninja 已正确生成，仅需重跑 `ninja` 完成编译。**

## 待办步骤

### 步骤 1：完成 Release + -g 全量构建
在 `build/` 目录执行（按 AGENTS.md 规定的 compile-only 命令）：
```
cd build
ninja -j 16
ninja install
```
- 预期：1010 个目标全量重编（因 -g 是新增标志，所有 .obj 都会重生成）
- 关注：crash_handler.cpp / crash_reporter.cpp 是否编译通过；链接 PXView.exe 是否成功
- 若仍出现 `ninja -t restat ... Permission denied`（上次会话的遗留问题），重试一次即可——这是文件锁/权限瞬时问题，重试通常能过

### 步骤 2：验证构建产物
确认以下文件存在：
- `build/install.dir/bin/PXView.exe`（主程序，含 -g DWARF 调试信息）
- `build/install.dir/bin/addr2line.exe`（符号化工具，由 install 步骤从 mingw binutils 拷入）

### 步骤 3：addr2line 符号化端到端冒烟测试（可选但推荐）
验证 -g 是否真的让 addr2line 能解析出函数名+行号：
1. 启动 `install.dir/bin/PXView.exe`
2. 用任务管理器或 `taskkill /PID <pid>` 触发一次崩溃（或用附带的测试方式），确认：
   - 崩溃瞬间弹出 `MessageBoxA` "PXView Crash" 弹窗（游戏式即时反馈）
   - `%TEMP%/pxview_crash_last.txt` 已生成，含 `PXVIEW_CRASH_LOG_V1` magic + frames 列表
3. 再次启动 PXView，确认 800ms 后弹出 "PXView Crash Report" DSDialog：
   - 摘要区显示 timestamp / exception / address
   - 堆栈区显示 `#0 函数名  file:line`（**关键：不再是 `??`**，证明 -g 生效）
   - 复制/打开日志文件夹/关闭按钮工作正常
   - 关闭后日志文件被删除（不重复弹窗）

若堆栈仍全是 `??`：检查 `install.dir/bin/addr2line.exe` 是否存在、PXView.exe 是否真带 -g（`objdump -h PXView.exe | grep debug_info` 应有 `.debug_info` 段）。

## 假设与决策

1. **不再修改 crash 实现代码**——Phase 1 已审阅 5 个文件，逻辑正确（信号安全 handler、PIE 偏移回退、日志删除防重复、QTimer 接收者自动断连安全）。
2. **CMakeLists 的 `-g` 冗余**（`CMAKE_CXX_FLAGS_RELEASE` 与 `add_compile_options` 都加 -g，产生 `-g -O3 -g`）保留不清理——无害，且清理会触发再次 reconfigure 风险。
3. **headless 模式不装 crash handler**——main.cpp 的 `install_crash_handler()` 在 GUI 分支（line 281-286），headless 分支（line 181-244）未调用，符合用户要求的"只装 GUI 模式"。
4. **`add_compile_options(-O3 -g)` 在 Release if 块内**——单配置 Ninja 生成器下仅在 Release 配置时执行一次，不会污染 Debug；当前 `CMAKE_BUILD_TYPE=Release`，行为正确。

## 验证清单
- [ ] `ninja -j 16` 退出码 0，无 crash 相关编译错误
- [ ] `ninja install` 退出码 0
- [ ] `install.dir/bin/PXView.exe` 存在
- [ ] `install.dir/bin/addr2line.exe` 存在
- [ ] （可选）崩溃弹窗 + 下次启动符号化堆栈窗口均正常
