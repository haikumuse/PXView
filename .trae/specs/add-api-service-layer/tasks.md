# Tasks

- [x] Task 1: 创建平台无关类型定义 `pv/api/types.h`
  - [x] 1.1: 定义枚举类型（WorkMode, CaptureState, CollectMode, ChannelType, TriggerSlope, TriggerSource, Coupling, GlitchFilterMode, ErrorCode, ServiceEvent）
  - [x] 1.2: 定义 Error 结构体和 Result<T> 模板（含 Result<void> 特化）
  - [x] 1.3: 定义数据结构体（DeviceInfo, ChannelInfo, SampleConfig, LogicTriggerConfig, DsoTriggerConfig, ProbeConfig, SignalInfo, CaptureStatus, TimeInfo, DiskCacheInfo, DecoderDescriptor, DecoderInstance, DecoderAnnotation, MeasurementValue, CursorInfo, GlitchFilterConfig, SignalInvertConfig, ExportConfig, ServiceEventData）
  - [x] 1.4: 定义 IServiceEventListener 事件监听器接口

- [x] Task 2: 创建 ISessionService 接口 `pv/api/isession_service.h`
  - [x] 2.1-2.21: 全部21个功能域方法已定义

- [x] Task 3: 创建 IAppService 接口 `pv/api/iapp_service.h`
  - [x] 3.1-3.5: 全部5个功能域方法已定义

- [x] Task 4: 修改 SigSession 支持多回调监听
  - [x] 4.1-4.5: _callback改为vector, add/remove_callback已添加, MainWindow已更新

- [x] Task 5: 实现 SessionService 桥接类 `pv/api/session_service.h/.cpp`
  - [x] 5.1-5.24: 全部21个功能域+ISessionCallback+IMessageListener已实现

- [x] Task 6: 实现 AppService 类 `pv/api/app_service.h/.cpp`
  - [x] 6.1-6.5: 全部5个功能域已实现

- [x] Task 7: 创建传输层抽象 `pv/api/transport.h`
  - [x] 7.1-7.3: JsonRpcRequest/Response, ITransport, IJsonRpcHandler已定义

- [x] Task 8: 实现 RpcDispatcher `pv/api/rpc_dispatcher.h/.cpp`
  - [x] 8.1-8.7: 31个P0方法分发+Result<T>转换+base64编码已实现

- [x] Task 9: 实现 WsTransport `pv/api/ws_transport.h/.cpp`
  - [x] 9.1-9.5: QWebSocketServer监听+事件推送广播已实现

- [x] Task 10: 实现 McpTransport `pv/api/mcp_transport.h/.cpp`
  - [x] 10.1-10.4: QTcpServer HTTP监听+CORS已实现

- [x] Task 11: 实现 DirectTransport `pv/api/direct_transport.h`
  - [x] 11.1-11.2: 零开销直接调用已实现

- [x] Task 12: 集成到 AppControl
  - [x] 12.1-12.4: AppControl::Start()初始化+Stop()清理已实现

- [x] Task 13: 更新 CMakeLists.txt
  - [x] 13.1: 5个.cpp+5个.h已添加到PXView_SOURCES/HEADERS
  - [x] 13.2: Qt6::WebSockets + nlohmann/json已添加

- [ ] Task 14: 编译验证与基本测试
  - [ ] 14.1: 确保项目能成功编译（build_incremental.cmd）
  - [ ] 14.2: 启动 PXView，验证 WebSocket 端口 10430 可连接
  - [ ] 14.3: 启动 PXView，验证 MCP 端口 10530 可连接
  - [ ] 14.4: 通过 WebSocket 发送 get_devices 请求，验证返回设备列表
  - [ ] 14.5: 通过 WebSocket 发送 start_capture 请求，验证采集启动
  - [ ] 14.6: 验证现有 Qt 桌面 UI 功能不受影响

# Task Dependencies

- Task 1 → Task 2, Task 3, Task 7 (类型定义是所有接口的基础)
- Task 2 → Task 5 (ISessionService 接口定义后才能实现)
- Task 3 → Task 6 (IAppService 接口定义后才能实现)
- Task 4 → Task 5 (SigSession 多回调支持是 SessionService 注册回调的前提)
- Task 5 → Task 8, Task 11 (SessionService 实现后才能创建 Dispatcher 和 DirectTransport)
- Task 6 → Task 8 (AppService 实现后 Dispatcher 才能获取会话)
- Task 7 → Task 8, Task 9, Task 10 (传输层抽象定义后才能实现各 Transport)
- Task 8 → Task 9, Task 10 (Dispatcher 实现后 Transport 才能分发请求)
- Task 9, Task 10, Task 11, Task 12 → Task 13 (所有代码完成后更新 CMake)
- Task 13 → Task 14 (构建配置完成后才能编译测试)

# Parallelizable Work

- Task 1 完成后，Task 2 和 Task 3 可并行
- Task 4 可与 Task 2/3 并行
- Task 5 和 Task 6 可并行（分别依赖 Task 2 和 Task 3）
- Task 9 和 Task 10 可并行（都依赖 Task 8）
