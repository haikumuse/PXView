# Tasks

- [x] Task 1: 在 log.h 中定义 pxv_assert 宏 + 重定义 assert 宏
  - [ ] SubTask 1.1: 阅读 [log.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/log.h) 当前结构,确认 `#include <assert.h>` 在第 27 行
  - [ ] SubTask 1.2: 在 log.h 的 `#endif` 之前新增 `pxv_assert(cond, fmt, args...)` 宏定义:
    ```c
    // pxv_assert: 不弹 Windows 模态对话框的 assert 替代。
    // Debug: pxv_err + __debugbreak()(挂 gdb 时捕获 SIGTRAP)或 abort()(未挂 gdb 时直接终止)
    // Release: pxv_err + no-op(符合 project_memory.md 规则)
    #ifdef NDEBUG
      #define pxv_assert(cond, fmt, args...) \
        do { if (!(cond)) { pxv_err(fmt, ## args); } } while (0)
    #else
      #ifdef _WIN32
        #define pxv_assert(cond, fmt, args...) \
          do { if (!(cond)) { pxv_err(fmt, ## args); \
               if (IsDebuggerPresent()) { __debugbreak(); } \
               else { abort(); } \
          } } while (0)
      #else
        #define pxv_assert(cond, fmt, args...) \
          do { if (!(cond)) { pxv_err(fmt, ## args); abort(); } } while (0)
      #endif
    #endif
    ```
  - [ ] SubTask 1.3: 在 pxv_assert 定义之后新增 assert 宏重定义:
    ```c
    // 重定义 assert 宏:不弹 Windows 模态对话框,改为 pxv_assert。
    // 避免 assert 弹窗的消息泵重入打穿 EventBus 护栏。
    #undef assert
    #define assert(cond) pxv_assert(cond, "Assertion failed: %s, file %s, line %d", #cond, __FILE__, __LINE__)
    ```
  - [ ] SubTask 1.4: 确认 log.h 需要 include `<windows.h>`(用于 `IsDebuggerPresent` 和 `__debugbreak`)或使用 `<intrin.h>`(仅 `__debugbreak`)。优先用 `<intrin.h>` + 自定义 `IsDebuggerPresent` 调用,避免 windows.h 污染。验证当前 log.h 是否已 include windows.h,若未 include 则添加最小化的 `extern "C" int IsDebuggerPresent(void);` 声明(避免引入完整 windows.h)
  - [ ] SubTask 1.5: 验证 log.h 末尾的 `#endif` 是 `_PXV_LOG_H_` 守卫,新增内容在 `#endif` 之前

- [x] Task 2: 验证重定义对第三方头文件的影响
  - [ ] SubTask 2.1: 编译,观察是否有 Qt/glib 头文件中的 assert 调用受影响(编译警告/错误)
  - [ ] SubTask 2.2: 若有冲突,改为在 pxv_assert 定义后不重定义 assert,而是在每个 PXView 源文件的 assert 调用点替换为 pxv_assert(回退方案,工作量大但安全)
  - [ ] SubTask 2.3: 若无冲突,验证 Debug 模式下 assert 失败时 gdb 捕获 SIGTRAP 而非弹窗

- [x] Task 3: 验证 gdb 与直接运行行为一致性
  - [ ] SubTask 3.1: 启动 gdb,运行 PXView.exe,切换到 srstd demo 设备,触发 assert(若仍有),验证 gdb 捕获 SIGTRAP 不弹窗
  - [ ] SubTask 3.2: 直接运行 PXView.exe,切换到 srstd demo 设备,验证 assert 失败时 abort() 直接终止,不弹 Windows 模态对话框
  - [ ] SubTask 3.3: 验证 Release 模式下 assert 失败时 no-op(继续执行,仅记录 pxv_err)

- [x] Task 4: 回归测试
  - [ ] SubTask 4.1: 编译 `cd build && ninja -j 16 && ninja install`,0 error
  - [ ] SubTask 4.2: PXView fork 设备(DSLogic/DSCope)回归正常
  - [ ] SubTask 4.3: srstd demo 设备切换/采集,验证无弹窗、无 EventBus 刷屏
  - [ ] SubTask 4.4: Headless 模式 `PXView.exe --headless` 启动正常,MCP API 17 工具响应

# Task Dependencies
- Task 1 独立(核心改动)
- Task 2 依赖 Task 1(验证编译)
- Task 3 依赖 Task 2(验证行为)
- Task 4 依赖 Task 3(回归)
