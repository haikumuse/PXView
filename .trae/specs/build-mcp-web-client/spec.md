# PXView MCP 网页客户端 Spec

## Why

PXView 已有完整的 MCP 服务器（15+ 工具），但用户只能通过 AI 编程工具（Claude Code/Codex）或 Python 脚本调用。需要一个现代化网页 UI，让用户通过自然语言对话控制逻辑分析仪，并直观看到 AI 正在执行哪些 MCP 工具调用、每步的状态和结果。

## What Changes

- 新建独立的前端项目 `web/`，基于 React + TypeScript + Vite
- 实现 MCP JSON-RPC 2.0 客户端，直连 PXView MCP 服务器（`http://127.0.0.1:10430`）
- 集成 OpenAI Chat Completions API（支持 function calling / tool_use），LLM 自动调用 MCP 工具
- 实现"思考过程可视化"：显示 LLM 的 tool_calls 和每步执行结果
- 实现解码结果表格渲染、设备状态卡片、采集进度条
- 支持用户自定义 OpenAI 兼容 API 端点（baseURL + apiKey + model）

## Impact

- Affected specs: `implement-full-mcp-protocol`（使用其 MCP 服务器）
- Affected code: 无 C++ 代码修改，纯前端项目
- PXView MCP 服务器需已启用 CORS（当前已支持 `Access-Control-Allow-Origin: *`）

---

## ADDED Requirements

### Requirement: MCP 客户端通信层

系统 SHALL 提供 MCP JSON-RPC 2.0 客户端，能向 PXView MCP 服务器发送 `initialize`、`tools/list`、`tools/call` 请求。

#### Scenario: 连接 MCP 服务器
- **WHEN** 用户打开网页并点击"连接"
- **THEN** 客户端发送 `initialize` 请求到 `http://127.0.0.1:10430`，收到能力信息后发送 `notifications/initialized`，然后调用 `tools/list` 获取可用工具列表

#### Scenario: MCP 服务器不可用
- **WHEN** MCP 服务器未启动
- **THEN** 显示连接失败提示，提供重试按钮

### Requirement: OpenAI 兼容 LLM 集成

系统 SHALL 通过 OpenAI Chat Completions API 与 LLM 通信，将 MCP 工具列表转换为 LLM 的 `tools` 参数，LLM 返回的 `tool_calls` 自动转换为 MCP `tools/call` 请求。

#### Scenario: 用户发送自然语言消息
- **WHEN** 用户输入"采集通道14的PWM信号并解码"
- **THEN** 系统将用户消息 + MCP 工具定义发送给 LLM，LLM 返回 tool_calls（如 `get_devices`），系统自动执行 MCP 调用，将结果回传 LLM，循环直到 LLM 返回最终文本回复

#### Scenario: 自定义 API 端点
- **WHEN** 用户配置 `baseURL`、`apiKey`、`model`（如 `https://api.openai.com/v1`、`sk-xxx`、`gpt-4o`）
- **THEN** 系统使用该端点调用 LLM，支持任何 OpenAI 兼容 API（包括本地 Ollama、vLLM 等）

### Requirement: 思考过程可视化

系统 SHALL 在聊天界面中实时显示 LLM 的每一步 tool_call 和执行结果，让用户直观看到 AI 正在做什么。

#### Scenario: 显示工具调用步骤
- **WHEN** LLM 返回 tool_calls
- **THEN** 界面显示可折叠的工具调用卡片，包含：工具名称、参数摘要、执行状态（进行中/成功/失败）、执行结果预览

#### Scenario: 长时间操作进度
- **WHEN** 执行 `wait_capture` 等长时间操作
- **THEN** 显示进度指示器（旋转动画 + 已用时间），操作完成后自动更新状态

### Requirement: 解码结果渲染

系统 SHALL 将 `get_analyzer_results` 返回的 JSON 数据渲染为可读的表格和摘要。

#### Scenario: PWM 解码结果
- **WHEN** AI 获取到 pwm_c 解码器的结果
- **THEN** 界面显示表格：序号 | 时间戳 | 占空比 | 周期，并提供"复制为 CSV"按钮

#### Scenario: 通用解码结果
- **WHEN** AI 获取到任意解码器的结果
- **THEN** 界面显示通用表格（annotation 行），包含：序号 | 开始样本 | 结束样本 | 类型 | 值

### Requirement: 设备状态面板

系统 SHALL 在侧边栏显示当前设备状态信息。

#### Scenario: 设备已连接
- **WHEN** MCP 服务器返回设备列表
- **THEN** 侧边栏显示设备名称、连接类型（USB 2.0/3.0）、工作模式、通道数

#### Scenario: 采集状态
- **WHEN** 采集正在进行
- **THEN** 侧边栏显示"采集中"状态指示

### Requirement: 系统提示词

系统 SHALL 预设系统提示词，引导 LLM 正确使用 MCP 工具。

#### Scenario: 默认系统提示词
- **WHEN** 用户开始新对话
- **THEN** 系统自动注入提示词，包含：PXView MCP 工具使用指南、推荐工作流（先 add_analyzer 再 start_capture）、错误处理建议

### Requirement: 对话历史管理

系统 SHALL 维护完整的对话历史，包括用户消息、LLM 回复、tool_calls 和 tool_results。

#### Scenario: 新对话
- **WHEN** 用户点击"新对话"
- **THEN** 清空对话历史，保留系统提示词和 MCP 连接

#### Scenario: 对话上下文
- **WHEN** LLM 在多轮对话中
- **THEN** 系统将完整的 tool_calls 和 tool_results 作为历史消息发送给 LLM，确保 LLM 知道之前执行了什么
