# Tasks

- [ ] Task 1: 项目初始化 — 创建 React + TypeScript + Vite 前端项目
  - [ ] SubTask 1.1: 在 `web/` 目录下初始化 Vite + React + TypeScript 项目
  - [ ] SubTask 1.2: 安装依赖：tailwindcss, lucide-react（图标）, @tailwindcss/vite
  - [ ] SubTask 1.3: 配置 Tailwind CSS 和基础样式（暗色主题）
  - [ ] SubTask 1.4: 创建项目结构：`src/components/`, `src/lib/`, `src/hooks/`, `src/types/`

- [ ] Task 2: MCP 客户端通信层 — 实现 JSON-RPC 2.0 客户端
  - [ ] SubTask 2.1: 定义 TypeScript 类型：`McpTool`, `McpToolCallResult`, `JsonRpcRequest`, `JsonRpcResponse`
  - [ ] SubTask 2.2: 实现 `McpClient` 类：`initialize()`, `listTools()`, `callTool(name, args)` 方法
  - [ ] SubTask 2.3: 实现 SSE 进度监听（`wait_capture` 的 `text/event-stream` 响应）
  - [ ] SubTask 2.4: 实现连接状态管理和错误处理

- [ ] Task 3: OpenAI 兼容 LLM 集成 — 实现 tool_use 循环
  - [ ] SubTask 3.1: 实现 `LlmClient` 类：`chat(messages, tools)` 方法，支持 OpenAI Chat Completions API
  - [ ] SubTask 3.2: 实现 MCP tools → OpenAI tools 格式转换器
  - [ ] SubTask 3.3: 实现 tool_use 循环：LLM 返回 tool_calls → 执行 MCP → 结果回传 LLM → 重复直到最终回复
  - [ ] SubTask 3.4: 实现设置面板：baseURL / apiKey / model 输入框，localStorage 持久化

- [ ] Task 4: 聊天界面 — 核心对话 UI
  - [ ] SubTask 4.1: 实现 `ChatMessage` 组件：用户消息、AI 文本回复、工具调用卡片
  - [ ] SubTask 4.2: 实现 `ToolCallCard` 组件：可折叠卡片，显示工具名、参数、状态图标、结果预览
  - [ ] SubTask 4.3: 实现 `ChatInput` 组件：输入框 + 发送按钮 + 停止按钮
  - [ ] SubTask 4.4: 实现 `ChatPanel` 组件：消息列表 + 自动滚动 + 输入区

- [ ] Task 5: 思考过程可视化 — 实时显示 AI 操作
  - [ ] SubTask 5.1: 在 `ToolCallCard` 中实现三态显示：进行中（旋转动画 + 已用时间）、成功（绿色对勾 + 结果摘要）、失败（红色叉 + 错误信息）
  - [ ] SubTask 5.2: 实现参数摘要：将 JSON 参数格式化为可读文本（如 "通道: 14, 解码器: pwm_c"）
  - [ ] SubTask 5.3: 实现结果预览：截断长结果，点击展开完整内容

- [ ] Task 6: 解码结果渲染 — 表格化展示
  - [ ] SubTask 6.1: 实现 `DecoderResultTable` 组件：解析 `get_analyzer_results` 返回的 JSON，渲染为表格
  - [ ] SubTask 6.2: 实现特定解码器格式化：pwm_c（占空比/周期）、uart_c（数据字节）等常见解码器
  - [ ] SubTask 6.3: 实现"复制为 CSV"按钮

- [ ] Task 7: 设备状态面板 — 侧边栏
  - [ ] SubTask 7.1: 实现 `DevicePanel` 组件：显示设备名称、USB 类型、工作模式
  - [ ] SubTask 7.2: 实现 `CaptureStatus` 组件：显示采集状态（空闲/采集中/已完成）
  - [ ] SubTask 7.3: 实现连接/断开按钮

- [ ] Task 8: 系统提示词和对话管理
  - [ ] SubTask 8.1: 编写默认系统提示词：PXView MCP 工具使用指南、推荐工作流、错误处理
  - [ ] SubTask 8.2: 实现对话历史管理：新对话、清空历史
  - [ ] SubTask 8.3: 实现 localStorage 持久化：保存 API 设置和对话历史

- [ ] Task 9: 整体布局和样式
  - [ ] SubTask 9.1: 实现主布局：左侧聊天面板（70%）+ 右侧设备/状态面板（30%）
  - [ ] SubTask 9.2: 实现顶部栏：PXView logo + 连接状态指示灯 + 设置按钮
  - [ ] SubTask 9.3: 实现响应式布局：移动端全屏聊天，侧边栏可折叠
  - [ ] SubTask 9.4: 实现暗色主题（主色调匹配 PXView 桌面应用）

# Task Dependencies
- Task 2 依赖 Task 1（项目结构）
- Task 3 依赖 Task 2（MCP 客户端）
- Task 4 依赖 Task 3（LLM 集成）
- Task 5 依赖 Task 4（聊天界面）
- Task 6 依赖 Task 4（聊天界面）
- Task 7 依赖 Task 2（MCP 客户端）
- Task 8 依赖 Task 3（LLM 集成）
- Task 9 依赖 Task 1（项目结构）
- Task 5, 6, 7, 8 可并行
