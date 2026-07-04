# 重定义 assert 宏消除 Windows 模态弹窗 Spec

## Why

当前 `assert()` 在 Debug 模式下触发 Windows C Runtime 模态对话框(Abort/Retry/Ignore)。该对话框**运行自己的消息泵**,会强制推进 qApp 事件循环,把排队的 `broadcast_async<T>` 事件强行派发,造成 EventBus `_broadcast_depth` 护栏被打穿,形成"assert 弹窗 → 消息泵重入 → EventBus 嵌套 → 又一个 assert 弹窗"的死循环,最终导致状态不一致与 SIGSEGV。

这造成两个严重问题：
1. **gdb 与直接运行行为不一致**：gdb 捕获 SIGABRT 信号后不弹窗,程序继续;直接运行则弹窗导致消息泵重入崩溃。开发者无法用 gdb 复现真实崩溃。
2. **打地鼠修复模式**：上一轮 spec 逐个清理 7 处 assert,但 sigsession.cpp 还有 13 处遗漏,每次切换 srstd 设备就暴露新的 assert 弹窗。438 处 assert 不可能逐个清理。

`project_memory.md` 已记录"assert 在 Release 应为 no-op,所有指针检查必须前置显式 if(!ptr) { log + return/throw; }"规则,但执行不到位。根本原因是规则与实际代码状态脱节,需要机制层强制执行。

## What Changes

### 修复 1: 创建 pxv_assert 宏,不弹 Windows 模态对话框
在 [log.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/log.h) 中新增 `pxv_assert(cond, fmt, args...)` 宏:
- Debug 模式下:`pxv_err` 记录 + `__debugbreak()`(触发 SIGTRAP,gdb 可捕获,不弹窗)
- Release 模式下:`pxv_err` 记录 + no-op(符合 project_memory.md 规则)
- 不调用 `assert()`,不触发 Windows C Runtime 模态对话框

### 修复 2: 重定义 assert 宏为 pxv_assert（全局替换）
在 [log.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/log.h) 末尾新增:
```c
// 重定义 assert 宏:不弹 Windows 模态对话框,改为 pxv_err + __debugbreak()
// 避免 assert 弹窗的消息泵重入打穿 EventBus 护栏。
// project_memory.md 规则:assert 在 Release 应为 no-op。
#undef assert
#define assert(cond) pxv_assert(cond, "Assertion failed: %s, file %s, line %d", #cond, __FILE__, __LINE__)
```

由于 log.h 已被 57 个文件 include(且这些文件都 include `<assert.h>`),重定义会自动传播。**关键约束**:log.h 必须在 `<assert.h>` 之后 include,且重定义必须在所有 include 之后。当前 log.h 第 27 行已 `#include <assert.h>`,重定义放在 log.h 末尾的 `#endif` 之前,确保覆盖。

### 修复 3: 确保 log.h 被所有含 assert 的文件 include
扫描 57 个 include `<assert.h>` 的文件,确认是否 include 了 log.h。未 include 的文件需要添加 `#include "log.h"`(或通过间接 include 已带入)。这一步是验证步骤,不强制改动——重定义通过 log.h 传播,只要文件 include 了 log.h 就生效。

### 不做的事（明确范围边界）
- 不逐个清理 438 处 assert 调用点。重定义宏后,所有 assert 自动变为不弹窗版本。
- 不引入 `Result<T>` 替换返回值类型。这是更大的架构改进。
- 不修改 libsigrok/libsigrokstd 上游代码(它们有自己的 assert.h,不受 PXView 重定义影响)。
- 不删除已有的 `if(!ptr) { pxv_err; return; }` 前置检查。这些是更优的修复,保留。
- 不修改 eventbus.h 的 `_broadcast_depth` 护栏逻辑(上一轮已改为 early-return)。
- 不修改 AnalogSnapshot 的 `_channel_num` 上限检查(上一轮已修复)。

## Impact

- **Affected specs**: `fix-assert-and-deviceagent-dispatch`(本 spec 是其根本性补丁,从"逐个清理 assert"升级为"机制层消除 assert 弹窗")
- **Affected code**:
  - `PXView/pv/log.h`(新增 pxv_assert 宏 + 重定义 assert 宏)
  - 57 个 include `<assert.h>` 的文件(自动传播,无需逐个修改)
- **风险**: 中。
  - **风险点1**: 重定义 assert 宏可能影响第三方头文件(如 Qt/glib)中的 assert 调用,如果它们在 log.h 之后 include。需要验证 Qt/glib 头文件不依赖 assert 的弹窗行为。
  - **风险点2**: `__debugbreak()` 在非 gdb 环境下会触发 SIGTRAP,如果用户未挂调试器,程序会崩溃。需要 fallback:Debug 模式下若未挂调试器,改为 `pxv_err + abort()`(直接终止,不弹窗)。
  - **风险点3**: 重定义 assert 可能与 C++ 标准库的 `cassert` 冲突。需要验证 `<cassert>` 与 `<assert.h>` 的一致性。
- **缓解**: 先在 log.h 中定义 `pxv_assert`,仅在 PXView 代码中替换 assert 为 pxv_assert(可选,作为安全网);重定义 assert 宏作为全局方案,失败时回退到 pxv_assert 显式替换。

## ADDED Requirements

### Requirement: assert 宏不弹 Windows 模态对话框
PXView 代码中所有 `assert(cond)` 调用 SHALL 在失败时:
1. 通过 `pxv_err` 记录错误信息(含条件、文件、行号)
2. Debug 模式下:若挂载调试器,触发 `__debugbreak()`(SIGTRAP,gdb 可捕获);若未挂调试器,调用 `abort()`(直接终止,不弹窗)
3. Release 模式下:no-op(符合 project_memory.md 规则)
4. SHALL NOT 调用 C Runtime 的 `assert()` 函数,避免触发 Windows 模态对话框

#### Scenario: assert 失败时挂载 gdb
- **WHEN** PXView 在 gdb 下运行,某处 `assert(cond)` 失败
- **THEN** SHALL 记录 `pxv_err("Assertion failed: %s, file %s, line %d", ...)`
- **AND** SHALL 触发 `__debugbreak()`(SIGTRAP)
- **AND** gdb SHALL 捕获 SIGTRAP,程序停在 assert 处
- **AND** SHALL NOT 弹出 Windows 模态对话框

#### Scenario: assert 失败时未挂调试器
- **WHEN** PXView 直接运行(无 gdb),某处 `assert(cond)` 失败
- **THEN** SHALL 记录 `pxv_err("Assertion failed: %s, file %s, line %d", ...)`
- **AND** Debug 模式下 SHALL 调用 `abort()`(直接终止,不弹窗)
- **AND** Release 模式下 SHALL no-op(继续执行)
- **AND** SHALL NOT 弹出 Windows 模态对话框

#### Scenario: assert 失败触发 EventBus 重入
- **WHEN** 某 listener 的 `on_event` 内部 `assert(cond)` 失败
- **AND** 直接运行模式(Debug + 无 gdb)
- **THEN** SHALL 调用 `abort()` 直接终止程序
- **AND** SHALL NOT 弹出模态对话框运行消息泵
- **AND** SHALL NOT 触发 EventBus `_broadcast_depth` 护栏被打穿

### Requirement: pxv_assert 宏定义
`PXView/pv/log.h` SHALL 定义 `pxv_assert(cond, fmt, args...)` 宏,行为与重定义后的 `assert` 一致,但支持自定义错误消息格式。这提供显式 API 供开发者在新代码中使用,避免依赖 assert 宏重定义。

## MODIFIED Requirements

### Requirement: project_memory.md 规则的机制层强制执行
`project_memory.md` 已记录"assert 在 Release 应为 no-op"规则。本 spec 通过重定义 assert 宏,在机制层强制执行该规则,不再依赖开发者记忆。所有现有 438 处 assert 自动符合规则,无需逐个清理。

## REMOVED Requirements

### Requirement: 逐个清理 assert 调用点
**Reason**: 438 处 assert 逐个清理成本高、易遗漏、无法验证完整性。重定义宏一次性消除所有 assert 弹窗风险。
**Migration**: 已清理的 7 处 assert(改为 `if(!cond) { pxv_err; return; }`)保留,这些是更优的修复(有 early-return 防止状态传播)。未清理的 431 处 assert 通过宏重定义自动变为安全版本。
