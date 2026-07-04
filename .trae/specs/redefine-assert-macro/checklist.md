# Checklist

## log.h 宏定义
- [x] [log.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/log.h) 中新增 `pxv_assert(cond, fmt, args...)` 宏
- [x] `pxv_assert` 在 Release 模式下为 `pxv_err + no-op`
- [x] `pxv_assert` 在 Debug 模式下为 `pxv_err + __debugbreak()`(挂 gdb)或 `abort()`(未挂 gdb)
- [x] `pxv_assert` 在 Windows 平台使用 `IsDebuggerPresent()` 判断(最小化声明,不引入完整 windows.h)
- [x] `pxv_assert` 在非 Windows 平台使用 `abort()`
- [x] log.h 末尾 `#endif` 之前新增 `#undef assert` + `#define assert(cond) pxv_assert(...)`
- [x] 重定义不影响 log.h 已有的 `#include <assert.h>`(undef 后重定义)

## 编译验证
- [x] `cd build && ninja -j 16 && ninja install` 编译成功,0 error
- [x] 无 Qt/glib 头文件 assert 调用受影响的警告/错误
- [x] 57 个 include `<assert.h>` 的文件编译通过

## 行为验证
- [ ] gdb 模式下 assert 失败:触发 SIGTRAP,gdb 捕获,不弹窗(需用户手动验证)
- [ ] 直接运行(Debug 模式)assert 失败:调用 `abort()`,不弹 Windows 模态对话框(需用户手动验证)
- [x] Release 模式下 assert 失败:no-op,仅记录 pxv_err(代码层已确认,宏定义正确)
- [x] assert 失败时记录的 pxv_err 包含条件、文件、行号

## 回归测试
- [ ] PXView fork 设备(DSLogic/DSCope)回归正常,采集/配置无回归(需用户手动验证)
- [ ] srstd demo 设备切换/采集,无弹窗、无 EventBus 刷屏、无 SIGSEGV(需用户手动验证)
- [x] Headless 模式 `PXView.exe --headless` 启动正常,MCP API 17 工具响应正常

## 范围边界
- [ ] 未逐个清理 438 处 assert 调用点(通过宏重定义自动覆盖)
- [ ] 未引入 `Result<T>` 替换返回值类型
- [ ] 未修改 libsigrok/libsigrokstd 上游代码
- [ ] 未删除已有的 `if(!ptr) { pxv_err; return; }` 前置检查(保留为更优修复)
- [ ] 未修改 eventbus.h 的 `_broadcast_depth` 护栏逻辑(已为 early-return)
- [ ] 未修改 AnalogSnapshot 的 `_channel_num` 上限检查(已修复)
