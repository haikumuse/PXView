# 完整 MCP 协议实现 Spec

## Why

当前 API 服务层（`pv/api/`）已实现基础架构：聚合根接口（IAppService/ISessionService）、三通道传输（WebSocket/MCP/Direct）、JSON-RPC 分发器。但 MCP 协议层只支持裸 JSON-RPC，不符合 MCP 规范（缺少 `initialize`/`tools/list`/`tools/call` 握手），工具定义不完整（缺 `wait_capture`/`close_capture`/二进制导出/数据表导出），WebSocket 事件推送不够实时，HTTP 传输不支持 SSE 长连接。需要全面对标 Logic 2 的 15 个 MCP 工具和实时推送能力，使 AI Agent 能真正控制 PXView。

## What Changes

- 重写 McpTransport 为标准 MCP 协议服务端（`initialize` + `tools/list` + `tools/call` + SSE 通知）
- 新增 `wait_capture` 工具（阻塞等待采集完成，对标 Logic 2 的 `wait_capture`）
- 新增 `close_capture` 工具（关闭会话释放资源）
- 新增 `export_raw_data_csv` 工具（完整参数：`analogDownsampleRatio`/`iso8601Timestamp`）
- 新增 `export_raw_data_binary` 工具（二进制原始数据导出）
- 新增 `export_data_table_csv` 工具（解码结果数据表导出）
- 改造 `start_capture` 支持 Logic 2 风格的完整参数（`logicDeviceConfiguration`/`captureConfiguration`/`glitchFilters`）
- 改造 `add_decoder` 支持 `analyzerLabel` 和等待解码完成
- 改造错误响应格式为 MCP 标准（`isError: true` + `content[0].text`）
- 增强 WebSocket 实时事件推送（采集进度/解码进度/状态变更）
- McpTransport 支持 SSE 流式响应（长阻塞操作如 `wait_capture`）
- 移除纯 UI 方法（`zoom_fit`/`zoom_in`/`zoom_out`）从 MCP 工具列表

## Impact

- Affected specs: `add-api-service-layer`（在其基础上增强）
- Affected code: `pv/api/mcp_transport.h/.cpp`, `pv/api/rpc_dispatcher.h/.cpp`, `pv/api/ws_transport.h/.cpp`, `pv/api/session_service.h/.cpp`, `pv/api/isession_service.h`, `pv/api/types.h`

---

## ADDED Requirements

### Requirement: MCP 协议握手

系统 SHALL 实现标准 MCP 协议握手，支持 AI Agent 通过 MCP 规范连接。

#### Scenario: initialize 握手
- **WHEN** AI Agent 发送 `{"jsonrpc":"2.0","method":"initialize","params":{"protocolVersion":"2025-03-26","capabilities":{},"clientInfo":{"name":"test"}},"id":1}`
- **THEN** 系统返回 `{"jsonrpc":"2.0","id":1,"result":{"protocolVersion":"2025-03-26","capabilities":{"tools":{}},"serverInfo":{"name":"pxview","version":"1.5.0"}}}`

#### Scenario: tools/list 发现工具
- **WHEN** AI Agent 发送 `{"jsonrpc":"2.0","method":"tools/list","id":2}`
- **THEN** 系统返回包含所有工具定义的列表，每个工具包含 `name`/`description`/`inputSchema`（JSON Schema 格式）

#### Scenario: tools/call 调用工具
- **WHEN** AI Agent 发送 `{"jsonrpc":"2.0","method":"tools/call","params":{"name":"get_devices","arguments":{}},"id":3}`
- **THEN** 系统调用对应工具并返回 `{"jsonrpc":"2.0","id":3,"result":{"content":[{"type":"text","text":"..."}]}}`

#### Scenario: MCP 标准错误格式
- **WHEN** 工具调用失败
- **THEN** 系统返回 `{"jsonrpc":"2.0","id":N,"result":{"content":[{"type":"text","text":"[ErrorCode] message"}],"isError":true}}`

#### Scenario: notifications/initialized 确认
- **WHEN** AI Agent 发送 `{"jsonrpc":"2.0","method":"notifications/initialized"}`
- **THEN** 系统返回 HTTP 204（无 body）

---

### Requirement: wait_capture 阻塞等待工具

系统 SHALL 提供 `wait_capture` MCP 工具，阻塞等待采集完成。

#### Scenario: 等待采集完成
- **WHEN** AI Agent 调用 `wait_capture`
- **AND** 采集正在进行
- **THEN** 系统阻塞直到采集完成（`CaptureState::Stopped`），然后返回成功

#### Scenario: 采集已完成
- **WHEN** AI Agent 调用 `wait_capture`
- **AND** 采集已经完成
- **THEN** 系统立即返回成功

#### Scenario: 无活动采集
- **WHEN** AI Agent 调用 `wait_capture`
- **AND** 没有活动采集
- **THEN** 系统返回错误 `[CaptureNotStarted] No capture in progress`

---

### Requirement: close_capture 会话关闭工具

系统 SHALL 提供 `close_capture` MCP 工具，关闭采集会话并释放资源。

#### Scenario: 关闭活动会话
- **WHEN** AI Agent 调用 `close_capture`
- **THEN** 系统停止采集（如果正在运行）、清除数据、释放资源

---

### Requirement: start_capture 完整参数

系统 SHALL 支持 Logic 2 风格的 `start_capture` 参数。

#### Scenario: 带 logicDeviceConfiguration 启动
- **WHEN** AI Agent 调用 `start_capture` 并提供 `logicDeviceConfiguration`（含 `digitalChannels`/`analogChannels`/`digitalSampleRate`/`analogSampleRate`/`digitalThresholdVolts`/`glitchFilters`）
- **THEN** 系统先配置通道启用/禁用、采样率、阈值电压、毛刺滤波器，再启动采集

#### Scenario: 带 captureConfiguration 启动
- **WHEN** AI Agent 调用 `start_capture` 并提供 `captureConfiguration`（含 `timedCaptureMode.durationSeconds` 或 `manualCaptureMode.trimDataSeconds`）
- **THEN** 系统配置采集模式后启动采集

---

### Requirement: export_raw_data_csv 完整参数

系统 SHALL 支持 `export_raw_data_csv` 工具，含 `analogDownsampleRatio` 和 `iso8601Timestamp` 参数。

#### Scenario: 带降采样导出
- **WHEN** AI Agent 调用 `export_raw_data_csv` 并指定 `analogDownsampleRatio=100`
- **THEN** 模拟通道数据按 100:1 降采样后导出

---

### Requirement: export_raw_data_binary 二进制导出工具

系统 SHALL 提供 `export_raw_data_binary` 工具，导出原始二进制数据。

#### Scenario: 导出逻辑通道二进制数据
- **WHEN** AI Agent 调用 `export_raw_data_binary` 并指定 `digitalChannels=[0,1,2,3]` 和 `directory="/tmp/export"`
- **THEN** 系统将指定通道的原始采样数据以二进制格式写入目录

---

### Requirement: export_data_table_csv 解码结果导出工具

系统 SHALL 提供 `export_data_table_csv` 工具，导出解码器数据表为 CSV。

#### Scenario: 导出指定解码器的数据表
- **WHEN** AI Agent 调用 `export_data_table_csv` 并指定 `analyzers=[{"analyzerId":"spi_c_0","radixType":4}]` 和 `filepath="/tmp/decode.csv"`
- **THEN** 系统将解码注解以 CSV 格式导出，数值按指定进制显示

---

### Requirement: add_decoder 等待解码完成

系统 SHALL 在 `add_decoder` 工具中等待解码完成后再返回。

#### Scenario: 解码器添加并完成解码
- **WHEN** AI Agent 调用 `add_decoder` 并指定 `analyzerName="spi_c"`
- **THEN** 系统添加解码器，等待解码进度达到 100%，然后返回 `analyzerId`

#### Scenario: 解码器解码失败
- **WHEN** AI Agent 调用 `add_decoder` 但解码过程出错
- **THEN** 系统自动移除已添加的解码器，返回错误

---

### Requirement: SSE 流式响应

McpTransport SHALL 支持 SSE（Server-Sent Events）流式响应，用于长阻塞操作。

#### Scenario: wait_capture 使用 SSE 推送进度
- **WHEN** AI Agent 调用 `wait_capture` 且采集正在进行
- **THEN** 系统通过 SSE 推送采集进度通知，直到采集完成后返回最终结果

#### Scenario: 普通 MCP 请求仍用 JSON 响应
- **WHEN** AI Agent 调用非阻塞工具（如 `get_devices`）
- **THEN** 系统返回标准 JSON 响应（不使用 SSE）

---

### Requirement: WebSocket 实时事件推送增强

WsTransport SHALL 增强实时事件推送，支持采集进度和解码进度。

#### Scenario: 采集进度推送
- **WHEN** 采集正在进行且进度更新
- **THEN** WsTransport 向所有连接的客户端推送 `{"type":"notification","method":"on_capture_progress","params":{"progress":50}}`

#### Scenario: 解码进度推送
- **WHEN** 解码正在进行且进度更新
- **THEN** WsTransport 推送 `{"type":"notification","method":"on_decode_progress","params":{"decoder_id":"spi_c","progress":0.75}}`

#### Scenario: 状态变更推送
- **WHEN** 采集状态从 Recording 变为 Stopped
- **THEN** WsTransport 推送 `{"type":"notification","method":"on_capture_state_changed","params":{"state":"stopped"}}`

---

## MODIFIED Requirements

### Requirement: McpTransport 协议处理

McpTransport SHALL 从裸 JSON-RPC 改为标准 MCP 协议，支持 `initialize`/`tools/list`/`tools/call`/`notifications/*` 方法路由，错误响应使用 MCP 标准格式（`isError: true` + `content` 数组）。

### Requirement: RpcDispatcher 方法路由

RpcDispatcher SHALL 支持 MCP 协议的三种方法（`initialize`/`tools/list`/`tools/call`），其中 `tools/call` 通过 `params.name` 路由到具体工具处理器，工具参数从 `params.arguments` 读取。

### Requirement: start_capture 参数扩展

ISessionService::start_capture SHALL 支持可选的完整配置参数（通道列表、采样率、毛刺滤波器、采集模式），在启动采集前先应用这些配置。

---

## REMOVED Requirements

### Requirement: 纯 UI 方法暴露为 MCP 工具
**Reason**: `zoom_fit`/`zoom_in`/`zoom_out` 是纯 UI 操作，AI Agent 不需要控制视图缩放
**Migration**: 从 RpcDispatcher 的工具列表中移除，ISessionService 接口保留（DirectTransport 仍可使用）
