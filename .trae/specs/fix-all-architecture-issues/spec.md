# fix-all-architecture-issues Spec

## Why

前期架构分析识别出 PXView 存在 7 大类架构问题：构建系统 monolithic、Core/View 分层泄漏、消息广播系统脆弱、数据流并发复杂、接口设计过重、外部依赖锁死、设计一致性陷阱。其中部分子问题已被 `decouple-core-from-view-v2` 和 `fix-state-sync-gaps-v2` 在代码层落地，但：
1. 这两个 spec 的 checklist 从未真正验证，回归状态未知
2. 我新提出的机制层修复（类型化事件总线消除循环、owner RAII 消除 use-after-free、触发配置单一真相源）尚未实施
3. 测试集成失效，导致上述修复无法被自动化兜底

本 spec 聚焦于**机制层根治**（而非继续打补丁），用类型系统与 RAII 从源头消除反复出现的 bug 类型。

## What Changes

### A. 测试集成修复（P0 前置）
- **BREAKING**：`CMakeLists.txt` 第 2003 行 `add_subdirectory(test)` 改为 `add_subdirectory(tests)`
- 新建 `tests/CMakeLists.txt`，把 `mcp_json/*.json` 注册为 ctest 用例（Python 驱动 HTTP POST 到 10110）
- 取消注释 `test_font.cpp` 的 `add_executable`

### B. 死代码清理（P0）
- 删除 `PXView/pv/view/groupsignal.h/.cpp`（整文件被注释禁用）+ 从 CMake 源列表移除
- 删除 `view.h` 中 `LissajousFigure *_lissajous` 成员 + `show_lissajous()` 中的赋值（前向声明无定义）
- 删除顶层一次性修复脚本：`_audit_*.ps1`、`fix_dmm_*.py`、`replace_hwdriver.ps1`、`verify_lwla.ps1`、`_bashtest.txt`、`output.log`

### C. 类型化事件总线替代 DSV_MSG_* 裸 int（P1 核心机制）
- 新建 `PXView/pv/interface/events.h`：为每个语义事件定义结构体（`CaptureStateChanged`/`CaptureOwnerChanged`/`TriggerConfigChanged`/`SampleCountUpdated` 等），事件携带完整上下文
- 新增 `IEventListener` 接口，用虚函数重载替代 `OnMessage(int)`
- `SigSession` 内部新增 `std::vector<IEventListener*> _event_listeners`，提供模板化 `broadcast<T>(const T&)`
- **广播循环护栏**：`broadcast<T>()` 内部 `thread_local int _broadcast_depth`，深度 > 1 时断言失败（开发期捕获循环）
- 保留 `OnMessage(int)` 作为兼容入口，内部翻译为对应事件再分发；新代码强制用类型化接口
- **BREAKING**：移除 `is_trigger_preconfigured` 标志位（被触发单一真相源替代后不再需要）

### D. _capture_owner_document RAII 化（P1 核心机制）
- 新建 `CaptureOwnerGuard` 类（`sigsession.h`）：构造时设置 owner，析构时 `join_copy_thread()` + 清空 owner + 广播 `CaptureOwnerChanged`
- `start_capture` 用 guard 管理生命周期，禁用拷贝、允许移动
- `_is_working` 状态纳入 guard，`CaptureOwnerChanged` 消费者不再需要特判 `is_working()`
- **BREAKING**：移除手动 `clear_capture_owner_document()` 调用点，统一由 guard 析构管理

### E. 触发配置单一真相源（P1 核心机制）
- `SigSession::sync_trigger_to_libsigrok()` 成为**唯一**的 Core→libsigrok 同步点，在 `start_capture` 内部调用
- `TriggerDock::commit_trigger()` 移除 `ds_trigger_*` 镜像调用，只写 Core `TriggerConfig`
- `SessionService` MCP 路径移除 `ds_trigger_*` 直接调用，改为写 `TriggerConfig`
- **BREAKING**：移除 `is_trigger_preconfigured` 标志位与 `set_trigger_preconfigured()` 接口

### F. 已完成 spec 的 checklist 验证（P1 兜底）
- 执行 `decouple-core-from-view-v2/checklist.md` Phase 7 "集成验证" 部分（之前未勾选的 6 项）
- 执行 `fix-state-sync-gaps-v2/checklist.md` 全部 13 个分组的验证项

## Impact

- **Affected specs**:
  - `decouple-core-from-view-v2`（验证其 checklist 未完成项）
  - `fix-state-sync-gaps-v2`（验证其 checklist 未完成项；本 spec 的 C/D/E 是其架构层延伸）
  - `refactor-trigger-state-management`（不存在，本 spec 的 E 等效实现）
- **Affected code**:
  - `CMakeLists.txt`（A、B）
  - `PXView/pv/interface/events.h`（C，新建）
  - `PXView/pv/interface/icallbacks.h`（C，兼容层）
  - `PXView/pv/sigsession.h/.cpp`（C、D、E）
  - `PXView/pv/dock/triggerdock.cpp`（E）
  - `PXView/pv/api/session_service.cpp`（E）
  - `PXView/pv/view/view.h`（B，删除死代码）
  - `PXView/pv/view/groupsignal.h/.cpp`（B，删除）
  - `tests/CMakeLists.txt`（A，新建）

## ADDED Requirements

### Requirement: 类型化事件总线
The system SHALL provide a typed event bus where each semantic state change is represented by a dedicated struct carrying full context, replacing the raw `int` DSV_MSG_* broadcast.

#### Scenario: 广播循环捕获
- **WHEN** 一个事件的消费者在处理过程中又触发了同类事件
- **THEN** `thread_local _broadcast_depth > 1` 触发断言失败（开发期）或日志警告（Release），阻止无限递归

#### Scenario: 事件携带上下文
- **WHEN** `CaptureOwnerChanged` 事件被广播
- **THEN** 事件对象包含 `old_owner` 和 `new_owner` 两个字段，消费者无需回查 `SigSession` 状态

#### Scenario: 兼容性
- **WHEN** 旧代码调用 `broadcast_msg(DSV_MSG_*)`
- **THEN** `SigSession::OnMessage` 内部翻译为对应类型化事件再分发，不破坏现有消费者

### Requirement: CaptureOwnerGuard RAII
The system SHALL manage `_capture_owner_document` lifecycle via RAII, eliminating manual `clear_capture_owner_document()` calls and use-after-free risk.

#### Scenario: Tab 关闭时自动清理
- **WHEN** 用户关闭正在采集的 Tab
- **THEN** `CaptureOwnerGuard` 析构自动 `join_copy_thread()` + 清空 owner + 广播 `CaptureOwnerChanged`，无需 `remove_tab` 手动调用

#### Scenario: is_working 状态一致性
- **WHEN** `start_capture` 设置 owner
- **THEN** `_is_working` 状态由 guard 管理，`CaptureOwnerChanged` 消费者无需特判 `is_working()`

### Requirement: 触发配置单一真相源
The system SHALL treat Core `TriggerConfig` as the single source of truth for trigger state, with `ds_trigger_*` libsigrok API synchronized only at `start_capture` time.

#### Scenario: GUI 提交触发
- **WHEN** 用户在 `TriggerDock` 配置高级触发并点击应用
- **THEN** 仅 Core `TriggerConfig` 被更新并广播 `TriggerConfigChanged`，`ds_trigger_*` 不被调用

#### Scenario: 采集启动时同步
- **WHEN** `SigSession::start_capture` 执行
- **THEN** `sync_trigger_to_libsigrok()` 一次性把 Core `TriggerConfig` 同步到 libsigrok `ds_trigger_*`

#### Scenario: MCP 触发配置
- **WHEN** MCP 客户端通过 `start_capture` 设置触发通道
- **THEN** 仅写 Core `TriggerConfig`，无需 `is_trigger_preconfigured` 标志位保护

### Requirement: 测试集成恢复
The system SHALL integrate `tests/` directory into CMake ctest framework.

#### Scenario: MCP JSON 用例纳入 ctest
- **WHEN** 开发者执行 `ctest`
- **THEN** 32 个 `mcp_json/*.json` 用例被自动执行，结果报告到 ctest

## MODIFIED Requirements

### Requirement: DSV_MSG_* 广播系统
[原系统：40+ 个裸 int 消息码，靠文档约定广播时机]
修改为：保留 `DSV_MSG_*` 宏作为兼容层，但新增类型化事件总线作为推荐接口。新代码强制用类型化接口，旧代码逐步迁移。

## REMOVED Requirements

### Requirement: `is_trigger_preconfigured` 标志位
**Reason**: 触发配置单一真相源后，`ds_trigger_*` 只在 `start_capture` 同步，GUI 路径不会 `ds_trigger_reset()` 覆盖 MCP 预置，标志位不再需要。
**Migration**: 删除 `SigSession::is_trigger_preconfigured()`/`set_trigger_preconfigured()` 接口及所有调用点。

### Requirement: 手动 `clear_capture_owner_document()` 调用
**Reason**: CaptureOwnerGuard RAII 自动管理生命周期，手动调用易漏。
**Migration**: 删除 `MainWindow::remove_tab` 中的手动 `clear_capture_owner_document()` 调用，由 guard 析构接管。

### Requirement: 死代码 `groupsignal` 与 `LissajousFigure`
**Reason**: `groupsignal` 整文件被注释禁用但仍参与编译；`LissajousFigure` 前向声明无定义、仅赋值未使用。
**Migration**: 直接删除文件与 CMake 引用，无功能影响。
