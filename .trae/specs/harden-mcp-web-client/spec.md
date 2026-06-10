# MCP Web 客户端健壮性重构 Spec

## Why

当前 MCP Web 客户端能跑通 happy path，但在非正常路径（用户停止、网络断开、后端超时、工具调用失败）下存在大量状态不一致、请求泄漏、死循环等问题。核心根因是：缺少请求生命周期管理、双重消息状态未统一、后端 API 参数不匹配、无上下文窗口管理。需要系统性重构，使客户端从"能跑"变为"健壮"。

## What Changes

- **前端请求生命周期管理**：统一 AbortController 管理，停止时正确清理所有异步操作和 UI 状态
- **统一消息数据模型**：消除 `chatHistory`（模块级）与 `messages`（Zustand）的双重状态，改为单一数据源 + 派生视图
- **上下文窗口管理**：chatHistory 自动截断，防止超出 LLM token 限制
- **后端 SSE 超时参数修复**：`mcp_transport.cpp` 中 `timeout_seconds` → `timeoutSeconds`，与 schema 一致
- **后端 `stop_capture` 等待确认**：stop_capture 后轮询等待采集真正停止
- **前端错误恢复**：MCP 自动重连、工具调用失败重试、`stop_capture` 失败反馈
- **DecoderResultTable 接入**：工具调用结果智能识别并格式化展示
- **React ErrorBoundary**：防止渲染异常白屏

## Impact

- Affected specs: `build-mcp-web-client`（在其基础上重构）
- Affected code:
  - `web/src/hooks/useAppStore.ts` — 重构核心状态管理
  - `web/src/lib/llm-client.ts` — abort 逻辑完善
  - `web/src/lib/mcp-client.ts` — abort + 重连
  - `web/src/components/ChatInput.tsx` — 停止按钮已修复
  - `web/src/components/ChatMessage.tsx` — 接入 DecoderResultTable
  - `web/src/components/ToolCallCard.tsx` — 智能结果渲染
  - `web/src/components/ChatPanel.tsx` — ErrorBoundary
  - `PXView/pv/api/mcp_transport.cpp` — SSE 超时参数修复
  - `PXView/pv/api/session_service.cpp` — stop_capture 等待确认

---

## ADDED Requirements

### Requirement: 请求生命周期管理

系统 SHALL 对每个 `sendMessage` 调用建立完整的请求生命周期，包括创建、执行、完成、取消四个阶段，确保任何阶段都能安全退出。

#### Scenario: 用户点击停止按钮
- **WHEN** 用户在 LLM 流式生成或 MCP 工具执行期间点击停止
- **THEN** 系统立即中断 LLM fetch 请求和 MCP fetch 请求，将当前 assistant 消息标记为 `isStreaming: false`（追加"[已停止]"标记），设置 `isProcessing: false`，不显示错误消息

#### Scenario: 停止时正在执行 MCP 工具
- **WHEN** 用户停止时，一个或多个 MCP 工具调用正在 `await` 中
- **THEN** 系统中断所有进行中的 MCP fetch 请求，将对应工具状态标记为 `error`（result 为"Cancelled by user"），不继续执行后续工具，不回传结果给 LLM

#### Scenario: 停止后消息状态完整
- **WHEN** 停止操作完成后
- **THEN** 所有 DisplayMessage 的 `isStreaming` 和 `isToolRunning` 均为 `false`，没有残留的"思考中"或"执行工具中"状态

### Requirement: 统一消息数据模型

系统 SHALL 使用单一数据源管理对话状态，消除 `chatHistory`（模块级 `ChatMessage[]`）与 `messages`（Zustand `DisplayMessage[]`）的双重状态。

#### Scenario: 新消息添加
- **WHEN** 用户发送消息或 LLM 回复
- **THEN** 数据只写入 Zustand store 的 `messages` 数组，`chatHistory`（发送给 LLM 的格式）通过 `messages` 派生计算，不独立维护

#### Scenario: 清空对话
- **WHEN** 用户点击"新对话"
- **THEN** `messages` 被清空，派生的 `chatHistory` 自动为空，没有残留的模块级变量

#### Scenario: 停止生成后对话上下文
- **WHEN** 用户停止生成后再次发送消息
- **THEN** 派生的 `chatHistory` 只包含完整的消息（已停止的消息标记为截断），不包含不完整的工具调用循环

### Requirement: 上下文窗口管理

系统 SHALL 自动管理发送给 LLM 的上下文大小，防止超出 token 限制。

#### Scenario: 长对话自动截断
- **WHEN** 对话历史超过 50 条消息或估计 token 数超过模型上下文窗口的 80%
- **THEN** 系统保留系统提示词 + 最近 30 条消息，丢弃更早的历史

#### Scenario: 工具结果过长截断
- **WHEN** 单个工具调用结果超过 4000 字符
- **THEN** 系统截断结果到 4000 字符并追加"[结果已截断，共 N 字符]"提示

### Requirement: MCP 自动重连

系统 SHALL 在 MCP 连接断开后自动尝试重连。

#### Scenario: PXView 重启后自动重连
- **WHEN** MCP 服务器暂时不可用（如 PXView 重启）
- **THEN** 系统每 5 秒尝试重连一次，最多尝试 6 次，重连期间显示"重连中…"状态

#### Scenario: 重连成功
- **WHEN** 重连成功
- **THEN** 系统刷新工具列表和设备信息，恢复可用状态

#### Scenario: 重连全部失败
- **WHEN** 6 次重连均失败
- **THEN** 显示"连接已断开"状态，提供手动"重新连接"按钮

### Requirement: 工具调用失败恢复

系统 SHALL 在 MCP 工具调用失败时提供恢复机制。

#### Scenario: stop_capture 失败反馈
- **WHEN** `stop_capture` 返回错误（如设备正在上传数据）
- **THEN** 系统将错误信息回传给 LLM，LLM 可决定重试或建议用户等待后重试

#### Scenario: 工具调用网络错误
- **WHEN** MCP 工具调用因网络错误失败
- **THEN** 系统将错误信息回传给 LLM（而非直接显示给用户），LLM 可决定重试

#### Scenario: 工具调用连续失败防护
- **WHEN** 同一工具连续失败 3 次
- **THEN** 系统停止重试循环，向用户显示错误摘要和建议

### Requirement: 智能结果渲染

系统 SHALL 根据 MCP 工具名称和返回数据格式，自动选择最佳渲染方式。

#### Scenario: 解码器结果表格渲染
- **WHEN** 工具名为 `get_analyzer_results` 且返回数据为 JSON 数组
- **THEN** 使用 `DecoderResultTable` 组件渲染，而非原始文本

#### Scenario: 设备列表卡片渲染
- **WHEN** 工具名为 `get_devices` 且返回数据为 JSON 数组
- **THEN** 使用简洁的设备卡片列表展示，而非原始 JSON

#### Scenario: 通用文本渲染
- **WHEN** 返回数据无法识别为结构化数据
- **THEN** 使用当前原始文本展示

### Requirement: React ErrorBoundary

系统 SHALL 在关键组件外包裹 ErrorBoundary，防止单个组件渲染异常导致整个应用白屏。

#### Scenario: 组件渲染异常
- **WHEN** 任何聊天消息组件渲染时抛出异常
- **THEN** ErrorBoundary 捕获异常，显示"消息渲染错误"占位符，其他消息正常显示

---

## MODIFIED Requirements

### Requirement: SSE 超时参数名修复

`mcp_transport.cpp` 中 `handle_sse_wait_capture` 的超时参数解析 SHALL 同时支持 `timeoutSeconds`（camelCase，与 MCP schema 一致）和 `timeout_seconds`（snake_case，向后兼容）。

#### Scenario: 使用 camelCase 参数
- **WHEN** MCP 客户端发送 `wait_capture` 请求含 `timeoutSeconds: 60`
- **THEN** SSE 模式下超时为 60 秒（当前因参数名不匹配，始终为默认 300 秒）

### Requirement: stop_capture 等待确认

`SessionService::stop_capture()` SHALL 在发送停止命令后等待采集真正停止，而非立即返回。

#### Scenario: stop_capture 后立即 start_capture
- **WHEN** 用户停止采集后立即开始新采集
- **THEN** 新采集能正常启动（当前可能因设备仍在停止中而失败）

---

## REMOVED Requirements

（无移除项）
