# 显式 Document Context 解耦 MCP 与 UI Spec

## Why

单 `SigSession` 是硬件设备排他性约束下的正确设计，`_active_document` 作为 UI "当前显示 tab" 游标也是最优解。但当前 `_active_document` 被同时用作 "MCP/API 操作的目标 document"，导致两个语义混淆：

1. UI 切换 tab 的瞬间，并发 MCP 请求的操作目标被悄悄改变（decoder 落到错误 document、`get_analyzer_results` 读到错误 stack）。
2. 采集数据归属模糊：`copy_data_to_document` 用了 `_active_document` 而非已记录的 `_capture_owner_document`，用户采集过程中切 tab 会导致数据落到错误的 tab（已存在的 Bug）。

本 spec 将 `_active_document` 的"操作目标"语义从 SigSession 下放到调用方，让 MCP 显式持有自己的 document 引用，UI 路径保持零改动（通过默认参数兼容）。

## What Changes

- **Bug 修复**：`copy_data_to_document` 改用 `_capture_owner_document`（已存在但未使用的成员），修复采集过程中切 tab 数据归属错误。
- **MODIFIED**：`SigSession` 的 document 相关方法（`get_decoder_stacks`/`add_decoder`/`remove_decoder`/`rst_decoder`/`rst_decoder_by_key_handel`）新增 `SessionDocument* doc = nullptr` 默认参数，`doc` 为 null 时回退到 `_active_document`（UI 路径零改动）。
- **MODIFIED**：`SigSession::start_capture` 新增 `SessionDocument* owner = nullptr` 参数，显式指定采集归属 document；`_capture_owner_document = owner ? owner : _active_document`。
- **ADDED**：`SessionService` 新增 `_api_document` 成员，MCP 的所有 document 相关读写操作显式传 `_api_document`，不再依赖 `_active_document`。
- **ADDED**：headless 模式下 `AppControl`/`AppService` 启动时创建专属 `_api_document`，MCP 在无 GUI tab 时也有稳定 document 容器。
- **REMOVED**：`_empty_decoder_stacks` 迁移逻辑（`set_active_document` 中 headless 期 decoder 暂存→迁移的无锁路径），因 headless 已有 `_api_document` 不再需要。

## Impact

- **Affected code**:
  - `PXView/pv/sigsession.h` / `sigsession.cpp` — document 相关方法签名 + `copy_data_to_document` 调用点 + `_empty_decoder_stacks` 迁移逻辑
  - `PXView/pv/api/session_service.h` / `session_service.cpp` — 新增 `_api_document` 成员，18 处 `get_decoder_stacks()` 调用 + `add_decoder`/`remove_decoder` 调用传 doc
  - `PXView/pv/api/appservice.cpp` / `app_service.h` — headless 模式创建 `_api_document` 并注入 SessionService
  - `PXView/pv/appcontrol.cpp` — headless 启动路径协调
  - `PXView/pv/data/datasource.h` — `get_decoder_stacks` 接口签名同步（可选默认参数）
  - `PXView/pv/data/sessiondocument.h` / `sessiondocument.cpp` — 无需改动，已具备独立持有 decoder_stacks + snapshot 的能力
  - `PXView/pv/tabcontext.cpp` — 无需改动（UI 路径走默认参数）
  - `PXView/pv/mainwindow.cpp` — 无需改动（UI 路径走默认参数）
  - `PXView/pv/view/view.cpp` — 无需改动（UI 路径走默认参数）

- **Affected specs**: 无（这是数据流架构层改造，不涉及功能 spec）

## ADDED Requirements

### Requirement: MCP 专属 Document 上下文

系统 SHALL 为每个 `SessionService` 实例提供一个专属的 `SessionDocument* _api_document`，作为 MCP/API 操作的稳定目标容器。该 document 不随 GUI tab 切换而改变。

#### Scenario: headless 模式 MCP 操作稳定
- **WHEN** 应用以 `--headless` 模式启动，无 GUI tab
- **AND** MCP 客户端调用 `add_analyzer`
- **THEN** decoder 被添加到 `_api_document`，不经过 `_empty_decoder_stacks` 暂存
- **AND** 后续 `get_analyzer_results` 能从同一 `_api_document` 读到该 decoder 的结果

#### Scenario: GUI 模式 MCP 不受 tab 切换影响
- **WHEN** GUI 模式下用户在 tab A 触发 MCP 采集
- **AND** 采集过程中用户切换到 tab B
- **AND** MCP 客户端调用 `get_analyzer_results`
- **THEN** 结果来自 MCP 的 `_api_document`，而非 tab B 的 document

### Requirement: 采集数据归属显式化

系统 SHALL 在 `start_capture` 时显式记录采集归属 document（`_capture_owner_document`），采集完成后的 `copy_data_to_document` MUST 使用该归属 document，而非当前的 `_active_document`。

#### Scenario: 采集过程中切 tab 数据不串
- **WHEN** 用户在 tab A 发起采集，`_capture_owner_document = tabA_doc`
- **AND** 采集过程中用户切换到 tab B
- **AND** 采集结束触发 `copy_data_to_document`
- **THEN** 采集数据被拷贝到 `tabA_doc`（采集发起者），而非 `tabB_doc`

## MODIFIED Requirements

### Requirement: SigSession document 相关操作的显式上下文

`SigSession` 的 document 相关操作 SHALL 接受可选的 `SessionDocument* doc` 参数。当 `doc` 为 null 时回退到 `_active_document`（保持 UI 路径向后兼容）；当调用方（如 `SessionService`）显式传 doc 时，操作作用于该 doc 而非 `_active_document`。

涉及方法：
- `get_decoder_stacks(SessionDocument* doc = nullptr)`
- `add_decoder(..., SessionDocument* doc = nullptr)`
- `remove_decoder(int index, SessionDocument* doc = nullptr)`
- `remove_decoder_by_key_handel(void* handel, SessionDocument* doc = nullptr)`
- `rst_decoder(int index, SessionDocument* doc = nullptr)`
- `rst_decoder_by_key_handel(void* handel, SessionDocument* doc = nullptr)`
- `start_capture(bool instant, SessionDocument* owner = nullptr)`

#### Scenario: UI 路径零改动
- **WHEN** `View::add_decoder()` 调用 `_session->add_decoder(...)` 不传 doc 参数
- **THEN** decoder 落到 `_active_document`（与改造前行为一致）

#### Scenario: MCP 路径显式定向
- **WHEN** `SessionService::add_decoder()` 调用 `_session->add_decoder(..., _api_document)`
- **THEN** decoder 落到 `_api_document`，不受 `_active_document` 影响

## REMOVED Requirements

### Requirement: `_empty_decoder_stacks` 暂存与迁移

**Reason**: headless 模式已有专属 `_api_document`，decoder 直接落 `_api_document`，不再需要"无 document 时暂存到 `_empty_decoder_stacks`、首次 `set_active_document` 时迁移"的无锁路径。该路径存在半迁移状态被并发 MCP 请求读到的风险。

**Migration**: `set_active_document` 中迁移 `_empty_decoder_stacks` 到新 doc 的代码块（sigsession.cpp:2490-2501）删除。`_empty_decoder_stacks` 静态成员可保留为空容器（`get_decoder_stacks` 的兜底返回值），但不再有写入路径。
