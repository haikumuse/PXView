# Tasks

## Phase 1: 纯数据模型层（无破坏性变更）

- [ ] Task 1: 创建 SignalModel 纯数据模型
  - [ ] 1.1 在 `PXView/pv/data/` 下新建 `signalmodel.h/cpp`，定义 SignalModel 类，包含通道索引、名称、类型（api::ChannelType）、启用状态、探头配置、触发状态（可读写，含 commit_trig()）、DSO 参数、颜色等属性
  - [ ] 1.2 SignalModel 不依赖任何 `pv::view::*` 或 Qt Widgets 头文件，仅依赖 `pv/api/types.h` 和标准库
  - [ ] 1.3 编译验证：SignalModel 可在无 Qt Widgets 的编译环境中通过

- [ ] Task 2: 创建 DecodeModel 纯数据模型
  - [ ] 2.1 在 `PXView/pv/data/` 下新建 `decodemodel.h/cpp`，定义 DecodeModel 类，包含实例 ID、解码器名称、通道映射、选项、进度等
  - [ ] 2.2 创建 SpectrumModel、LissajousModel、MathModel 等辅助模型

- [ ] Task 3: 重新设计 DataSource 接口
  - [ ] 3.1 在 `datasource.h` 中新增纯数据版方法：`get_signal_models()`、`get_decode_models()` 等
  - [ ] 3.2 保留旧方法但标记为 deprecated，避免一次性破坏所有调用点
  - [ ] 3.3 SigSession、SessionDocument、SessionSnapshot 实现新接口方法

## Phase 2: SigSession 去视图化

- [ ] Task 4: SigSession 添加 SignalModel 支持
  - [ ] 4.1 在 SigSession 中添加 `std::vector<SignalModel*> _signal_models` 成员
  - [ ] 4.2 修改 `init_signals()` 创建 SignalModel 而非 view::Signal
  - [ ] 4.3 实现 `get_signal_models()` 新接口
  - [ ] 4.4 保留 `get_signals()` 旧接口暂时返回空向量（标记 deprecated）

- [ ] Task 5: SigSession 移除 View 成员
  - [ ] 5.1 移除 `_spectrum_traces`、`_lissajous_trace`、`_math_trace`，替换为对应的 Model 对象
  - [ ] 5.2 移除 `#include "view/mathtrace.h"` 等直接 View 依赖
  - [ ] 5.3 修改 `add_decoder()` 返回 DecodeModel* 而非 view::Trace*& out_trace
  - [ ] 5.4 在 `libsigrok/lib_main.c` 中新增 `ds_set_datafeed_callback_ex(cb, void* user_data)` 和 `ds_set_event_callback_ex(cb, void* user_data)` API
  - [ ] 5.5 移除 `SigSession::_session` 静态成员，改用新的 _ex 回调 API 传递 this 指针

- [ ] Task 6: SessionDocument 和 SessionSnapshot 去视图化
  - [ ] 6.1 SessionDocument: 将 `_decode_traces` 替换为 `_decode_models`，`_signals` 替换为 `_signal_models`，`_spectrum_traces` 替换为 `_spectrum_models`
  - [ ] 6.2 SessionDocument: 实现新的 DataSource 接口方法
  - [ ] 6.3 SessionSnapshot: 将 `_signals`、`_decode_traces`、`_spectrum_traces`、`_lissajous_trace`、`_math_trace` 替换为对应的 Model 对象
  - [ ] 6.4 SessionSnapshot: 实现新的 DataSource 接口方法

## Phase 3: View 层适配

- [ ] Task 7: 创建 SignalFactory
  - [ ] 7.1 在 `PXView/pv/view/` 下新建 `signalfactory.h/cpp`
  - [ ] 7.2 实现 `create_signals(DataSource*)` 从 SignalModel 创建 view::Signal 子类
  - [ ] 7.3 实现 `update_signals()` 处理 SignalModel 变更
  - [ ] 7.4 Signal 对象通过 SignalModel 引用读取业务状态，不再自行持有业务数据

- [ ] Task 8: 适配 View 层所有消费代码
  - [ ] 8.1 修改 `view.cpp`、`viewport.cpp`、`header.cpp`、`ruler.cpp` 使用新 DataSource 接口
  - [ ] 8.2 修改所有 Dock 组件（triggerdock、dsotriggerdock、searchdock 等）使用 SignalModel 而非 view::Signal
  - [ ] 8.3 修改所有对话框（mathoptions、lissajousoptions、fftoptions、dsomeasure、decoderoptionsdlg、waitingdialog 等）
  - [ ] 8.4 修改 samplingbar、decoderstack、spectrumstack 等
  - [ ] 8.5 修改 storesession.cpp（10 处 get_signals() 调用），使用 SignalModel 读取通道信息
  - [ ] 8.6 移除所有 `static_cast<view::LogicSignal*>` 等类型转换，改为从 SignalModel 读取数据

- [ ] Task 9: 适配 SessionService（去视图化 + 消除死锁）
  - [ ] 9.1 移除 `set_view(view::View*)` 和 `_view` 成员
  - [ ] 9.2 在 `api/types.h` 的 ServiceEvent 枚举中新增 ViewShowRegion、ViewZoomFit、ViewZoomIn、ViewZoomOut 事件类型
  - [ ] 9.3 View 操作改为通过 IServiceEventListener 广播事件，MainWindow 注册为 listener 处理
  - [ ] 9.4 SessionService 内部操作 SignalModel/DecodeModel 而非 view::Signal/view::DecodeTrace
  - [ ] 9.5 移除所有 `QMetaObject::invokeMethod(qApp, ..., Qt::BlockingQueuedConnection)` 调用，改为直接在调用者线程通过 mutex 保护操作 SigSession
  - [ ] 9.6 移除 `QThread::currentThread() == qApp->thread()` 线程检查
  - [ ] 9.7 将 `#include <QApplication>` 替换为 `#include <QCoreApplication>`
  - [ ] 9.8 Headless 模式下 `wait_capture_complete()` 改用 `std::condition_variable` 替代 `QEventLoop`
  - [ ] 9.9 `configure_and_start()` 中的 `processEvents()` 改为条件变量等待设备模式切换完成

## Phase 4: ISessionCallback 拆分

- [ ] Task 10: 拆分 ISessionCallback
  - [ ] 10.1 定义 `IDataCallback`、`ICaptureCallback`、`ITriggerCallback`、`ISessionStateCallback` 子接口
  - [ ] 10.2 `ISessionCallback` 继承所有子接口并提供默认空实现（向后兼容）
  - [ ] 10.3 MainWindow 可选择性 override 子接口方法

## Phase 5: Headless 模式

- [ ] Task 11: main.cpp 支持 Headless 模式
  - [ ] 11.1 解析 `--headless`、`--ws-port`、`--mcp-port` 命令行参数
  - [ ] 11.2 Headless 模式使用 QCoreApplication，不创建 MainFrame/MainWindow
  - [ ] 11.3 正常初始化 SigSession、DeviceAgent、SessionService
  - [ ] 11.4 启动 WsTransport 和 McpTransport

- [ ] Task 12: 验证 Headless 模式完整流程
  - [ ] 12.1 验证 Headless 启动不触发 QWidget 创建
  - [ ] 12.2 通过 MCP API 测试采集→解码→导出完整流程
  - [ ] 12.3 通过 WS API 测试设备列表、配置、采集流程
  - [ ] 12.4 验证 Headless 模式下 add_decoder 不使用 invokeMethod，不死锁
  - [ ] 12.5 验证 Headless 模式下 wait_capture_complete 使用 condition_variable，不依赖 QEventLoop
  - [ ] 12.6 验证 Headless + WS 新 GUI 场景：WS 客户端可读取波形数据并接收实时事件推送

## Phase 6: CMake 库分离

- [ ] Task 13: CMake 重构为 pxview-core 静态库
  - [ ] 13.1 定义 `pxview-core` 静态库目标，包含 SigSession、DeviceAgent、数据层、API 层
  - [ ] 13.2 pxview-core 不链接 Qt::Widgets，仅链接 Qt::Core、Qt::Concurrent
  - [ ] 13.3 最终 PXView 可执行文件链接 pxview-core + View 层代码
  - [ ] 13.4 验证 pxview-core 可独立编译
  - [ ] 13.5 确认 session_service.cpp 中无 `#include <QApplication>` 依赖

# Task Dependencies

- Task 1, 2, 3 可并行执行（Phase 1 无依赖）
- Task 4 依赖 Task 1（SignalModel 需先存在）
- Task 5 依赖 Task 4（先添加新接口再移除旧成员）
- Task 5.4, 5.5 可独立于 Task 5.1-5.3 执行（libsigrok API 修改与 SigSession 去视图化无直接依赖）
- Task 6 依赖 Task 2, 3（DecodeModel 和新 DataSource 接口需先存在）
- Task 7 依赖 Task 3, 4（SignalFactory 需要新 DataSource 接口和 SignalModel）
- Task 8 依赖 Task 7（View 层适配需要 SignalFactory）
- Task 9 依赖 Task 5, 6（SessionService 适配需要 Core 层先完成去视图化）
- Task 9.5-9.9（死锁消除）依赖 Task 5.3（add_decoder 返回 DecodeModel 而非 DecodeTrace 后才可移除 invokeMethod）
- Task 10 可与 Task 8 并行（接口拆分不影响现有代码）
- Task 11 依赖 Task 5, 6, 9（Headless 需要完整的去视图化和死锁消除）
- Task 12 依赖 Task 11
- Task 13 依赖 Task 5, 6, 9（库分离需要去视图化完成）
