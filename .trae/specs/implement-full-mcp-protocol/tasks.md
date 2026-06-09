# Tasks

## Phase 1: MCP 协议握手（P0 — AI Agent 可连接）

- [x] Task 1: 重写 McpTransport 为标准 MCP 协议服务端
  - [x] 1.1: 实现 `initialize` 方法，返回 `protocolVersion`/`capabilities`/`serverInfo`
  - [x] 1.2: 实现 `tools/list` 方法，返回所有工具的 `name`/`description`/`inputSchema`（JSON Schema）
  - [x] 1.3: 实现 `tools/call` 方法路由，从 `params.name` + `params.arguments` 分发到具体处理器
  - [x] 1.4: 实现 `notifications/initialized` 处理（返回 204 无 body）
  - [x] 1.5: 实现 `ping` 方法
  - [x] 1.6: 改造错误响应格式为 MCP 标准（`isError: true` + `content[{"type":"text","text":"[ErrorCode] message"}]`）
  - [x] 1.7: 改造成功响应格式为 MCP 标准（`content[{"type":"text","text":"..."}]`）

- [x] Task 2: 定义完整 MCP 工具 Schema
  - [x] 2.1: 为 15 个工具编写 JSON Schema 输入定义（对齐 Logic 2 的参数结构）
  - [x] 2.2: 工具列表：`get_devices`, `start_capture`, `stop_capture`, `wait_capture`, `load_capture`, `save_capture`, `close_capture`, `add_analyzer`, `remove_analyzer`, `export_raw_data_csv`, `export_raw_data_binary`, `export_data_table_csv`, `get_capture_status`, `get_channels`, `get_analyzer_results`
  - [x] 2.3: 从工具列表中移除纯 UI 方法（`zoom_fit`/`zoom_in`/`zoom_out`）

- [x] Task 3: 改造 RpcDispatcher 支持 MCP 协议路由
  - [x] 3.1: 添加 `on_initialize` 处理器
  - [x] 3.2: 添加 `on_tools_list` 处理器（返回工具 Schema 列表）
  - [x] 3.3: 改造 `handle_request` 支持 `tools/call` 格式（`params.name` + `params.arguments`）
  - [x] 3.4: 统一响应包装为 MCP `content` 格式

## Phase 2: 核心缺失工具（P0 — AI Agent 可完整控制采集）

- [x] Task 4: 实现 wait_capture 阻塞等待工具
  - [x] 4.1: 在 ISessionService 中添加 `wait_capture_complete(uint64_t timeout_ms)` 方法
  - [x] 4.2: 在 SessionService 中实现：使用 QEventLoop 阻塞，监听 CaptureState 变化
  - [x] 4.3: 在 RpcDispatcher 中添加 `on_wait_capture` 处理器
  - [x] 4.4: McpTransport 对 wait_capture 请求使用 SSE 流式响应

- [x] Task 5: 改造 start_capture 支持 Logic 2 风格完整参数
  - [x] 5.1: 在 ISessionService 中添加 `configure_and_start` 方法（接受通道列表、采样率、毛刺滤波、采集模式）
  - [x] 5.2: 在 SessionService 中实现：先调用 `set_channel_enabled`/`set_sample_rate`/`set_glitch_filter`，再调用 `start_capture`
  - [x] 5.3: 在 RpcDispatcher 中改造 `on_start_capture` 支持 `logicDeviceConfiguration`/`captureConfiguration` 参数
  - [x] 5.4: 返回 `captureId`

- [x] Task 6: 实现 close_capture 工具
  - [x] 6.1: 在 ISessionService 中添加 `close_capture()` 方法
  - [x] 6.2: 在 SessionService 中实现：停止采集 + 清除数据 + 重置状态
  - [x] 6.3: 在 RpcDispatcher 中添加 `on_close_capture` 处理器

## Phase 3: 导出工具（P1 — AI Agent 可导出数据）

- [x] Task 7: 实现 export_raw_data_csv 完整参数
  - [x] 7.1: 在 ExportConfig 中添加 `analog_downsample_ratio`/`iso8601_timestamp` 字段
  - [x] 7.2: 在 SessionService::export_data 中实现降采样和 ISO8601 时间戳逻辑
  - [x] 7.3: 在 RpcDispatcher 中改造 `on_export_data` 支持新参数

- [x] Task 8: 实现 export_raw_data_binary 二进制导出
  - [x] 8.1: 在 ISessionService 中添加 `export_binary` 方法
  - [x] 8.2: 在 SessionService 中实现：将原始采样数据按通道写入二进制文件
  - [x] 8.3: 在 RpcDispatcher 中添加 `on_export_raw_data_binary` 处理器

- [x] Task 9: 实现 export_data_table_csv 解码结果导出
  - [x] 9.1: 在 ISessionService 中添加 `export_decoder_table` 方法
  - [x] 9.2: 在 SessionService 中实现：遍历解码注解，按行写入 CSV（支持进制选择）
  - [x] 9.3: 在 RpcDispatcher 中添加 `on_export_data_table_csv` 处理器

## Phase 4: 解码器增强（P1 — AI Agent 可完整控制解码）

- [x] Task 10: 改造 add_decoder 等待解码完成
  - [x] 10.1: 在 ISessionService::add_decoder 中添加 `wait_for_completion` 参数
  - [x] 10.2: 在 SessionService 中实现：添加解码器后轮询 `IsRunning()`/`get_progress()` 直到完成
  - [x] 10.3: 解码失败时自动移除已添加的解码器
  - [x] 10.4: 在 RpcDispatcher 中改造 `on_add_decoder` 支持 `analyzerLabel` 参数

## Phase 5: 实时推送与 SSE（P1 — 长连接场景）

- [x] Task 11: 增强 WebSocket 实时事件推送
  - [x] 11.1: 在 SessionService 中添加采集进度回调（`capture_progress` 事件）
  - [x] 11.2: 在 WsTransport::on_service_event 中实现进度推送（`on_capture_progress`/`on_decode_progress`）
  - [x] 11.3: 在 WsTransport 中实现状态变更推送（`on_capture_state_changed`）

- [x] Task 12: 实现 SSE 流式响应
  - [x] 12.1: 在 McpTransport 中添加 SSE 响应模式（`Content-Type: text/event-stream`）
  - [x] 12.2: 对 `wait_capture` 请求使用 SSE 推送进度，最终返回完成结果
  - [x] 12.3: 对普通请求仍使用标准 JSON 响应

## Phase 6: 编译验证与集成测试（P0 — 确保可运行）

- [x] Task 13: 编译验证
  - [x] 13.1: 确保项目能成功编译（`build_incremental.cmd`） — 21/21 步骤完成，零错误
  - [x] 13.2: 修复所有编译 warning 和 error — 仅剩旧代码 warning，新代码无 error

- [ ] Task 14: MCP 协议集成测试
  - [ ] 14.1: 启动 PXView，验证 MCP 端口 10530 可连接
  - [ ] 14.2: 发送 `initialize` 请求，验证握手成功
  - [ ] 14.3: 发送 `tools/list` 请求，验证返回工具列表
  - [ ] 14.4: 发送 `tools/call` get_devices，验证返回设备列表
  - [ ] 14.5: 发送 `tools/call` start_capture，验证采集启动
  - [ ] 14.6: 发送 `tools/call` wait_capture，验证阻塞等待
  - [ ] 14.7: 验证现有 Qt 桌面 UI 功能不受影响

# Task Dependencies

- Task 1 → Task 3 (MCP 协议路由依赖协议格式定义)
- Task 2 → Task 3 (工具 Schema 定义后才能实现 tools/list)
- Task 3 → Task 4, 5, 6, 7, 8, 9, 10 (MCP 路由就绪后才能添加新工具)
- Task 4, 5, 6 → Task 14 (核心工具完成后才能测试)
- Task 11 → Task 12 (事件推送增强是 SSE 的基础)
- Task 13 → Task 14 (编译通过后才能测试)

# Parallelizable Work

- Task 1 和 Task 2 可并行（协议格式 + 工具 Schema 定义）
- Task 4, 5, 6 可并行（三个独立工具）
- Task 7, 8, 9 可并行（三个导出工具）
- Task 11 和 Task 12 可并行（WebSocket 推送 + SSE）
