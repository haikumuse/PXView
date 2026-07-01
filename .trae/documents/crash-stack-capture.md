# PXView 崩溃栈捕获功能实现计划

## Context（为什么做这个）

PXView 当前只有 main.cpp:334 的 `try/catch(std::exception)`，对访问违规、栈溢出、纯虚调用、CRT invalid parameter 等真崩溃**完全无能为力**——这些不抛 C++ 异常，直接被 OS 接管，用户只看到程序消失，无法报告问题。headless 模式甚至连 try/catch 都没有。

本需求：崩溃时抓堆栈写日志 + 游戏式即时弹窗提示，下次启动展示完整符号化堆栈，方便用户报告问题。

## 硬约束（关键技术决策）

1. **MinGW + DWARF 限制**：MinGW-w64 不生成 PDB，`dbghelp.dll` 的 `SymFromAddr` 只读 PDB 读不了 DWARF。所以 backward-cpp、Boost.Stacktrace windbg 后端在 Windows/MinGW 下**只能拿裸地址，拿不到函数名/行号**。
2. **Boost.Stacktrace 的 addr2line 后端 POSIX 专属**（依赖 execinfo.h/fork），Windows 不可用。
3. **可行路线**：Win32 `CaptureStackBackTrace` 抓地址栈（零 malloc）+ 手动 spawn `addr2line.exe`（mingw binutils 自带，2.46 版）符号化。
4. **两段式**：崩溃 handler 只抓地址写日志（信号安全，不 spawn 子进程不碰 Qt）；符号化放到下次启动进程健康时用 QProcess 调 addr2line 批量做。
5. **只装 GUI 模式**（用户已选）；headless 不装。
6. **主防线用 `SetUnhandledExceptionFilter`**（Win32 层），不受 `AppControl::Init()` 启动 Python 的 `signal()` 影响。

## 实现步骤

### 步骤 1：新建 `PXView/pv/crash/crash_log.h`（格式契约头）

纯 POD + 常量，无 Qt 依赖。被 handler（写）和 reporter（读）共享，避免格式漂移。

```cpp
namespace pv { namespace crash {
constexpr const char* CRASH_LOG_FILENAME = "pxview_crash_last.txt";
constexpr const char* CRASH_LOG_MAGIC   = "PXVIEW_CRASH_LOG_V1";
constexpr int MAX_FRAMES = 64;  // CaptureStackBackTrace 上限

struct CrashLogContext {      // install 时填，handler 里只读
    char log_path[MAX_PATH];  // UTF-8, %TEMP%/pxview_crash_last.txt
    char exe_path[MAX_PATH];  // UTF-8, GetModuleFileNameW(NULL) 转码
    void* exe_base;           // GetModuleHandleW(NULL)
};
}}
```

crash log 路径：`%TEMP%/pxview_crash_last.txt`（固定名，覆盖写最新一次；下次启动检测只需查一个已知路径）。用 `QStandardPaths::writableLocation(QStandardPaths::TempLocation)`，项目已有 4 处内联用法（applicationpardlg.cpp / mainwindow.cpp），不新增工具函数。

### 步骤 2：新建 `PXView/pv/crash/crash_handler.h` + `.cpp`（崩溃捕获 + 写日志 + 即时弹窗）

**公开 API**：
```cpp
namespace pv { namespace crash {
void install_crash_handler();   // main.cpp GUI 分支 pxv_log_init() 后调用
}}
```

**内部函数**：
- `crash_exception_filter(EXCEPTION_POINTERS*)` → 抓栈 + 写日志 + MessageBoxA + `_exit(1)`
- `crash_purecall_handler()` / `crash_invalid_param_handler(...)` / `crash_sigabrt_handler(int)`
- `write_crash_log(...)` — 信号安全：`_open`+`_write`+`_snprintf_s`+栈缓冲 `char buf[8192]`，零 malloc
- `show_crash_message_box(...)` — Win32 `MessageBoxA`

**注册**：`SetUnhandledExceptionFilter`（链式保存前 filter）+ `_set_purecall_handler` + `_set_invalid_parameter_handler` + `signal(SIGABRT)`。handler 入口检测 `IsDebuggerPresent()`，附加调试器时 `return EXCEPTION_CONTINUE_SEARCH` 让调试器接管。

**信号安全白名单**：`CaptureStackBackTrace` / `_open` / `_write` / `_snprintf_s` / `MessageBoxA` / `_exit` / `GetLocalTime` / `GetModuleHandleW` / `MultiByteToWideChar`。**禁用**：fwrite、QString、QMessageBox、exit、std::string、fopen。

**终止**：`_exit(1)`（跳过 atexit/DTOR，避免二次崩溃）。

**已知限制**：`EXCEPTION_STACK_OVERFLOW`（0xC00000FD）时栈几乎耗尽，`CaptureStackBackTrace` 自身可能再栈溢出，日志可能写不出，由 Windows WER 接管。文档标注，后续可用 `SetThreadStackGuarantee` 预留栈空间解决。

### 步骤 3：新建 `PXView/pv/crash/crash_reporter.h` + `.cpp`（下次启动符号化 + Qt 弹窗）

**公开 API**：
```cpp
namespace pv { namespace crash {
bool show_crash_report_if_exists(QWidget *parent);  // main.cpp 延迟调用
}}
```

**内部**：
- `parse_crash_log(path)` → `ParsedCrashLog{ timestamp, ex_code_name, ex_addr, exe_path, exe_base, QList<quint64> frames }`
- `symbolicate_frames(addr2line_exe, target_exe, exe_base, frames)` → 单次 QProcess 调 `addr2line.exe -e <exe> -f -C <addr1> ... <addrN>`，3 秒超时，解析每地址 2 行输出（函数名、`文件:行号`）
- `CrashReportDialog : public pv::dialogs::DSDialog`（声明在 cpp 内）

**地址换算**：主策略传绝对运行时地址（MinGW 非 PIE 默认生效）；若所有帧返回 `??`，回退用 `offset = addr - exe_base`。跨模块帧（`addr` 不在 `[exe_base, exe_base+256MB)` 内）标 `[external: 0x...]` 不送符号化。

**addr2line.exe 定位**：`QCoreApplication::applicationDirPath() + "/addr2line.exe"`；不存在则回退 `D:/msys64/mingw64/bin/addr2line.exe`；都没有则跳过符号化只显示裸地址，不崩。

### 步骤 4：crash log 文件格式（纯文本 UTF-8）

```
PXVIEW_CRASH_LOG_V1
timestamp=2026-07-01 14:30:25
exception_code=0xC0000005
exception_code_name=EXCEPTION_ACCESS_VIOLATION
exception_address=0x00007FF6A1234567
exe_path=C:/install.dir/bin/PXView.exe
exe_base=0x00007FF6A1000000
frame_count=42
[frames]
0x00007FF6A1234567
0x00007FF6A1234890
...
[end_frames]
```

handler 内置异常码表映射 `exception_code` → `exception_code_name`（ACCESS_VIOLATION / INT_DIVIDE_BY_ZERO / STACK_OVERFLOW / ILLEGAL_INSTRUCTION 等）。

### 步骤 5：main.cpp 改动（4 处）

1. **第 50-52 行后加 include**：
   ```cpp
   #ifdef _WIN32
   #include "pv/crash/crash_handler.h"
   #include "pv/crash/crash_reporter.h"
   #endif
   ```

2. **第 277 行 `pxv_log_init()` 之后**插入：
   ```cpp
   #ifdef _WIN32
       pv::crash::install_crash_handler();
   #endif
   ```
   时机理由：pxv_log_init 后（可记录安装）、AppControl::Init()（第 325 行）前（避免与 Python signal 竞争；主防线是 Win32 filter 不受影响）；Application 构造后（MessageBoxA 有窗口站）。

3. **第 354 行 `w.ShowHelpDocAsync();` 之后、第 356 行 `a.exec()` 之前**插入：
   ```cpp
   #ifdef _WIN32
       QTimer::singleShot(800, &w, [&w]() {
           pv::crash::show_crash_report_if_exists(&w);
       });
   #endif
   ```
   镜像 ShowHelpDocAsync 的 `QTimer::singleShot(300,...)` 模式（mainframe.cpp:1058）；800ms 晚于 help-doc 的 300ms，让主窗口先绘制。

4. **headless 分支（178-241 行）不动**。

### 步骤 6：CMakeLists.txt 改动（2 处）

1. **第 417-426 行 WIN32 块**追加源文件（`PXVIEW_GUI_SOURCES` 在 293 行 set，APPEND 安全）：
   ```cmake
   if(WIN32)
       list(APPEND PXVIEW_GUI_SOURCES
           PXView/pv/winnativewidget.cpp
           PXView/pv/winshadow.cpp
           PXView/pv/wintaskbarprogress.cpp
           PXView/pv/crash/crash_handler.cpp
           PXView/pv/crash/crash_reporter.cpp
       )
   endif()
   ```
   **注意**：头文件**不**加到这个 WIN32 块的 `list(APPEND PXView_HEADERS)`——第 428 行有 `set(PXView_HEADERS ...)`（覆盖式），会把这里的 APPEND 丢掉。

2. **头文件加到第 428 行的顶层 `set(PXView_HEADERS ...)`** 末尾（仅声明，无 Q_OBJECT 不需 MOC）：
   ```
       PXView/pv/crash/crash_log.h
       PXView/pv/crash/crash_handler.h
       PXView/pv/crash/crash_reporter.h
   ```

3. **第 1662 行 Installation 节开头**新增 addr2line 打包：
   ```cmake
   if(WIN32)
       find_program(ADDR2LINE_EXE
           NAMES addr2line.exe addr2line
           HINTS "D:/msys64/mingw64/bin" "C:/msys64/mingw64/bin"
                 "$ENV{MSYSTEM_PREFIX}/bin" "$ENV{MINGW_PREFIX}/bin")
       if(ADDR2LINE_EXE)
           message(STATUS "addr2line found: ${ADDR2LINE_EXE}")
           install(PROGRAMS "${ADDR2LINE_EXE}" DESTINATION bin RENAME addr2line.exe)
       else()
           message(WARNING "addr2line.exe not found — crash symbolication disabled")
       endif()
   endif()
   ```
   `install(PROGRAMS ...)` 而非 `install(TARGETS ...)`——addr2line 是外部预编译 exe 无 CMake 目标；`DESTINATION bin` 与 PXView.exe 同目录（第 1677 行）。

### 步骤 7：报告窗口 UI

`CrashReportDialog : public pv::dialogs::DSDialog`（dsdialog.h:56 `layout()` 返回 QVBoxLayout*，:57 `setTitle(QString)`）。纯代码构建（项目 UIS 列表为空，惯例）。

布局：
- 顶部 QLabel：摘要（时间 + 异常名 + 地址）
- 中间 QPlainTextEdit（只读，等宽字体 `QFontDatabase::systemFont(QFontDatabase::FixedFont)`），逐帧 `#i  func  file:line`，参考 LogDock（logdock.h:84）模式
- 底部 3 个 QPushButton：复制（clipboard）、打开日志文件夹（`explorer.exe /select,path`，参考 applicationpardlg.cpp）、关闭（accept）
- `exec()` 模态；关闭后 `QFile::remove(crash_log_path)`，下次启动不再弹

## 关键文件

| 文件 | 动作 |
|------|------|
| `PXView/pv/crash/crash_log.h` | 新建（格式契约头） |
| `PXView/pv/crash/crash_handler.h` / `.cpp` | 新建（崩溃捕获 + 写日志 + MessageBoxA） |
| `PXView/pv/crash/crash_reporter.h` / `.cpp` | 新建（addr2line 符号化 + Qt 报告窗） |
| `PXView/main.cpp` | 改 4 处（include / 安装 handler / 延迟弹报告窗） |
| `CMakeLists.txt` | 改 3 处（WIN32 源文件 / 顶层头文件 / addr2line install） |

## 验证

### 构建
```
cd build && ninja -j 16 && ninja install
```
检查 `install.dir/bin/addr2line.exe` 存在（CMake install 生效）。

### 触发崩溃测试（临时调试入口）
环境变量 `PXVIEW_CRASH_TEST=<type>` 在 `control->Start()` 后检测，触发：
- `div0`：`volatile int z=0; volatile int x=1/z;` → EXCEPTION_INT_DIVIDE_BY_ZERO
- `null`：`volatile int* p=nullptr; volatile int v=*p;` → EXCEPTION_ACCESS_VIOLATION
- `purecall`：悬空 vtable 调虚函数 → `_set_purecall_handler`
- `abort`：`abort()` → SIGABRT + UnhandledExceptionFilter

### 验证 handler
1. 触发 `null` 崩溃
2. 检查 `%TEMP%/pxview_crash_last.txt` 存在，内容含 magic、异常码 `0xC0000005`、`exception_address`、≥10 帧
3. `MessageBoxA` 弹出（异常名 + 地址 + 日志路径）
4. 进程退出码 1

### 验证符号化
1. 触发崩溃后重启 PXView，等 800ms 报告窗弹出
2. 顶部帧应显示真实函数名 + `main.cpp:NN` 行号
3. 交叉验证：手动 `install.dir/bin/addr2line.exe -e PXView.exe -f -C 0x<地址>` 输出一致
4. addr2line 缺失回退：删除 `bin/addr2line.exe`，重启验证报告窗仍弹出，帧显示裸地址不崩

### 验证调试器不抢断点
调试模式（gdb 附加）触发崩溃，handler 应 `return EXCEPTION_CONTINUE_SEARCH`，让调试器接管。
