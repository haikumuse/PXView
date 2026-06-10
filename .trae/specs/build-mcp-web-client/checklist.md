* [x] MCP 客户端能成功连接 PXView MCP 服务器（initialize + tools/list）
  - `mcp-client.ts` connect() 完整实现：发送 initialize（protocolVersion: 2024-11-05）→ notifications/initialized → tools/list

* [x] MCP 客户端能调用 tools/call 并正确处理 JSON-RPC 响应
  - `mcp-client.ts` callTool() 通过 sendRequest() 发送 tools/call，sendRequest() 构造 JSON-RPC 请求体，解析响应，检查 json.error

* [x] SSE 进度监听在 wait\_capture 期间正常工作
  - `mcp-client.ts` callTool() 检测 Content-Type 为 text/event-stream 时调用 handleSSEResponse()，使用 ReadableStream 手动解析 SSE 事件，支持 onProgress 回调
  - `useAppStore.ts` 对 wait_capture 工具传入 onProgress 回调，更新 captureProgress 和 captureStatus

* [x] OpenAI Chat Completions API 调用正常，tool\_calls 自动转换为 MCP 调用
  - `llm-client.ts` chat() 发送 Chat Completions 请求，`mcpToOpenAITools()` 转换工具格式；`useAppStore.ts` sendMessage() 循环处理 tool_calls 并调用 mcpClient.callTool()

* [x] 支持自定义 baseURL/apiKey/model（包括本地 Ollama）
  - `SettingsDrawer.tsx` 提供四个配置字段；`llm-client.ts` 支持自定义 baseUrl/apiKey/model，apiKey 为空时不发送 Authorization header（兼容 Ollama）

* [x] 聊天界面显示用户消息和 AI 文本回复
  - `ChatMessage.tsx` 区分用户/AI 消息（右/左对齐，不同背景色），`ChatPanel.tsx` 渲染消息列表并自动滚动

* [x] ToolCallCard 显示工具名、参数摘要、执行状态（进行中/成功/失败）
  - `ToolCallCard.tsx` 显示工具名、StatusIcon（running=spinning loader, success=check, error=x）、friendly label

* [x] ToolCallCard 可折叠展开查看完整参数和结果
  - `ToolCallCard.tsx` expanded 状态控制折叠/展开，展开后显示完整参数 JSON 和结果文本，长结果支持 Show more

* [x] 长时间操作（wait\_capture）显示进度指示器和已用时间
  - `ToolCallCard.tsx` running 状态时使用 useEffect + setInterval 每秒更新 liveElapsed，显示 "{seconds}s"；完成后显示 "{ms}ms"

* [x] 解码结果渲染为可读表格（通用格式 + PWM 专用格式）
  - `DecoderResultTable.tsx` 解析 JSON 数据，isPwmResult() 检测 PWM 结果，pwmColumns()/genericColumns() 映射友好列名，支持 CSV 复制

* [x] 设备状态面板显示设备名称、USB 类型、工作模式
  - `DevicePanel.tsx` 显示设备名称、USB 类型 badge（USB3 高亮）、工作模式、采集状态

* [x] 系统提示词包含 PXView MCP 工具使用指南
  - `system-prompt.ts` 包含推荐工作流（5步）、关键规则、可用操作列表

* [x] 对话历史完整保留，新对话可清空
  - `useAppStore.ts` clearChat() 清空 chatHistory 和 messages，对话期间历史完整保留在内存中

* [x] API 设置和对话历史持久化到 localStorage
  - API 设置：`useAppStore.ts` loadSettings()/saveSettings() 持久化到 pxview-mcp-settings
  - 对话历史：loadChatMessages()/saveChatMessages() 持久化到 pxview-mcp-chat，最多 100 条，500ms 防抖保存
  - clearChat() 同时清除 localStorage 中的对话历史

* [x] 暗色主题，布局匹配 PXView 桌面应用风格
  - `index.css` @theme 定义暗色配色方案（#0D1117 主背景等），组件使用语义化颜色变量，布局为左侧聊天+右侧设备面板

* [x] 响应式布局，移动端可用
  - `App.tsx` 使用 md: 断点，桌面端设备面板为侧边栏，移动端为底部抽屉，含浮动切换按钮
