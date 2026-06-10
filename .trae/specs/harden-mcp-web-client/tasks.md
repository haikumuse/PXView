# Tasks

- [x] Task 1: 统一消息数据模型 — 重构 useAppStore 消除双重状态
  - [x] 1.1: 定义 `ConversationMessage` 统一类型，包含 LLM 所需字段（role, content, tool_calls, tool_call_id）和 UI 所需字段（id, isStreaming, isToolRunning, toolCallStatuses）
  - [x] 1.2: 将 `chatHistory` 从模块级变量改为从 `messages` 派生的函数 `messagesToChatHistory(messages: ConversationMessage[]): ChatMessage[]`
  - [x] 1.3: 重构 `sendMessage` 使用统一数据模型，所有状态变更通过 Zustand set
  - [x] 1.4: 重构 `clearChat` 只清空 `messages`，无需额外清理模块级变量
  - [x] 1.5: 重构 `stopGeneration` 正确清理：标记当前消息 `isStreaming: false, isToolRunning: false`，追加"[已停止]"，将进行中工具标记为 `error: "Cancelled by user"`

- [x] Task 2: 请求生命周期管理 — 完善 AbortController 和停止逻辑
  - [x] 2.1: 将 `abortController` 移入 Zustand store（或通过 store action 管理），确保 stopGeneration 和 sendMessage 互斥
  - [x] 2.2: `sendMessage` 入口检查 `isProcessing`，如果为 true 则拒绝新请求
  - [x] 2.3: `llm-client.ts` chatStream 在 abort 退出时调用 `callbacks.onDone(partialMessage)` 而非静默返回，确保 UI 状态更新
  - [x] 2.4: `mcp-client.ts` callTool 在 abort 时抛出 `AbortError`（而非返回空结果），让调用方统一处理
  - [x] 2.5: `sendMessage` 的工具执行循环中，catch AbortError 时标记工具为 `error: "Cancelled"`，不继续执行后续工具，不回传结果给 LLM

- [x] Task 3: 上下文窗口管理 — 防止 chatHistory 无限增长
  - [x] 3.1: 在 `messagesToChatHistory` 派生函数中实现截断策略：保留 system prompt + 最近 30 条消息
  - [x] 3.2: 工具结果超过 4000 字符时截断并追加"[结果已截断，共 N 字符]"
  - [x] 3.3: 同一工具连续失败 3 次时，在回传给 LLM 的 tool result 中追加"[此工具已连续失败 3 次，请尝试其他方法]"

- [x] Task 4: MCP 自动重连
  - [x] 4.1: 在 `useAppStore` 中添加 `reconnectStatus: 'idle' | 'reconnecting' | 'failed'` 状态
  - [x] 4.2: `disconnectMcp` 时如果 `isProcessing` 为 true，先调用 `stopGeneration`
  - [x] 4.3: MCP 请求失败时自动触发重连：最多 6 次，间隔 5 秒
  - [x] 4.4: 重连成功后刷新 `mcpTools` 和 `deviceInfo`
  - [x] 4.5: 重连全部失败后显示"连接已断开"状态，提供手动连接按钮

- [x] Task 5: 后端 SSE 超时参数修复
  - [x] 5.1: 修改 `mcp_transport.cpp` 第 398 行，同时检查 `timeoutSeconds` 和 `timeout_seconds`，优先使用 `timeoutSeconds`

- [x] Task 6: 后端 stop_capture 等待确认
  - [x] 6.1: 修改 `session_service.cpp` 的 `stop_capture()`，在调用 `_session->stop_capture()` 后添加轮询等待（最多 3 秒，每 100ms 检查 `is_working()`），确保采集真正停止

- [x] Task 7: 智能结果渲染 — 接入 DecoderResultTable
  - [x] 7.1: 在 `ToolCallCard` 中根据 `toolCall.name` 判断结果类型：`get_analyzer_results` → DecoderResultTable，`get_devices` → 设备卡片，其他 → 原始文本
  - [x] 7.2: 修改 `ToolCallCard` 的结果展示区域，条件渲染不同组件
  - [x] 7.3: 为 `get_devices` 结果创建简洁的设备信息卡片（名称 + USB 类型 badge）

- [x] Task 8: React ErrorBoundary
  - [x] 8.1: 创建 `ErrorBoundary` 组件（class component，getDerivedStateFromError + componentDidCatch）
  - [x] 8.2: 在 `ChatPanel` 中为每条消息包裹 ErrorBoundary，渲染错误时显示"消息渲染错误"占位符
  - [x] 8.3: 在 `App` 最外层包裹 ErrorBoundary，渲染错误时显示"应用错误"+ 重载按钮

# Task Dependencies

- Task 2 depends on Task 1（统一数据模型后才能正确管理 abort 状态）
- Task 3 depends on Task 1（截断逻辑依赖派生函数）
- Task 4 depends on Task 2（重连需要先停止当前请求）
- Task 5, Task 6 独立（后端修复，可与前端并行）
- Task 7, Task 8 独立（纯 UI 改进，可并行）
