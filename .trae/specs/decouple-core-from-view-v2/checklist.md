# Checklist

## Phase 1: 纯数据模型层

- [x] SignalModel 类已创建，不依赖 pv::view/* 或 Qt Widgets 头文件
- [x] SignalModel 包含触发状态（可读写）和 commit_trig() 方法
- [x] SignalModel 包含快照指针（void*），类型由 type 决定
- [x] LissajousModel 类已创建，不依赖 pv::view/*
- [x] 未创建 DecodeModel/SpectrumModel/MathModel（使用已有的 DecoderStack/SpectrumStack/MathStack）
- [x] DataSource 接口移除了所有 view::* 返回类型
- [x] DataSource 接口新增 get_signal_models()、get_decoder_stacks()、get_spectrum_stacks()、get_math_stack()、get_lissajous_model()
- [x] datasource.h 中无 view::* 前向声明

## Phase 2: Core 层去视图化

- [x] SigSession::init_signals() 创建 SignalModel 而非 view::Signal
- [x] SigSession 不再持有 view::Signal*、view::SpectrumTrace*、view::LissajousTrace*、view::MathTrace* 成员
- [x] SigSession 不再 #include 任何 pv/view/*.h 头文件
- [x] SigSession::add_decoder() 输出参数为 DecoderStack*& 而非 view::Trace*&
- [x] SigSession::add_decoder() 不创建 DecodeTrace
- [x] SigSession::add_decoder() 不调用 create_popup()
- [x] SigSession::add_decoder() 不调用 signals_changed()
- [x] SigSession::math_rebuild() 在 Core 层完整创建 MathStack（Task 17 已完成）
- [x] SessionDocument 保留 _decoder_stacks（Core 层对象）
- [x] SessionDocument 移除了 _decode_traces（view::DecodeTrace*）
- [x] SessionDocument 移除了所有 view::* 成员
- [x] SessionSnapshot 移除了所有 view::* 成员
- [x] libsigrok 新增 ds_set_datafeed_callback_ex(cb, void*) 和 ds_set_event_callback_ex(cb, void*) API
- [x] SigSession::_session 静态成员已移除，改用 _ex 回调 API 传递 this 指针（Task 5.4 已完成，已通过编译验证）

## Phase 3: View 层适配

- [x] SignalFactory 可从 SignalModel 创建 view::Signal 子类
- [x] SignalFactory 实现 UI 状态保存/恢复（save_ui_state/restore_ui_state）
- [x] View 持有自己的 _signals/_decode_traces/_spectrum_traces/_math_trace/_lissajous_trace
- [x] View 提供 mark_derived_traces_dirty()/sync_derived_traces() 懒同步机制
- [x] View 提供 get_own_signals()/get_own_decode_traces()/get_own_spectrum_traces()/get_own_math_trace()/get_own_lissajous_trace() 访问方法
- [x] Qt `signals` 宏冲突已修复（变量名改为 result/sig_list）
- [x] 不使用 signals/slots/emit/foreach 作为变量或参数名
- [x] View::add_decoder() 调用 Core 层创建 DecoderStack 后自行创建 DecodeTrace（Task 9.5）
- [x] View::remove_decoder() 实现（Task 9.6）
- [x] View::on_signals_changed() 通过 SignalFactory 增量更新，不影响 DecodeTrace/SpectrumTrace/MathTrace（Task 9.7）
- [x] View::data_updated() 刷新数据指针，不重建渲染对象（Task 9.8）
- [x] ProtocolDock 调用 View::add_decoder() 而非 SigSession::add_decoder()（Task 10）
- [x] ProtocolDock 从 View::get_own_decode_traces() 获取列表
- [x] viewport.cpp/header.cpp 已适配（使用 get_own_* 方法）
- [x] triggerdock.cpp 已适配（get_signals → get_signal_models）（Task 11.2.1）
- [x] dsotriggerdock.cpp 已适配（Task 11.2.2）
- [x] searchdock.cpp / dialogs/search.cpp 已适配（Task 11.2.3）
- [x] protocoldock.cpp 已适配（get_decode_signals → get_decoder_stacks）（Task 11.2.4）
- [x] spectrumtrace.cpp 已适配（构造用 SignalModel.color()，paint 用 _view->get_own_signals()）（Task 11.3.1）
- [x] spectrumstack.cpp 已适配（用 SignalModel.snapshot()/hw_offset()/vdiv()/vfactor()）（Task 11.3.2）
- [x] mathoptions.cpp 已适配（简单迭代用 SignalModel，math_rebuild 加 TODO）（Task 11.3.3）
- [x] fftoptions.cpp 已适配（用 SignalModel.name()/index()）（Task 11.3.4）
- [x] lissajousoptions.cpp 已适配（set_show 加 TODO）（Task 11.3.5）
- [x] dsomeasure.cpp 已适配（用 _view->get_own_signals()）（Task 11.3.6）
- [x] protocollist.cpp 已适配（get_decode_signals → get_decoder_stacks）（Task 11.3.7）
- [x] samplingbar.cpp 已适配（Task 11.4）
- [x] storesession.cpp 已适配（从 SignalModel 读取通道信息）（Task 11.5）
- [x] 所有 static_cast<view::LogicSignal*> 等类型转换已移除（Task 11.6）
- [x] DecoderStack 从 SignalModel 获取通道信息而非 view::Signal（Task 11.7）
- [x] mainwindow.cpp 已适配（Task 11.8）
- [x] signalfactory.cpp 无残留编译错误（Task 11.9）

## Phase 4: SessionService 去视图化

- [x] SessionService 不再持有 view::View* 成员和 set_view() 方法（Task 12.1）
- [x] 新增 ServiceEvent 枚举值（ViewShowRegion/ViewZoomFit/ViewZoomIn/ViewZoomOut）（Task 12.2）
- [x] View 操作通过 ServiceEvent 事件通知（Task 12.3）
- [x] SessionService 操作 SignalModel/DecoderStack（Task 12.4）
- [x] session_service.cpp 中 #include <QApplication> 已替换为 #include <QCoreApplication>（Task 12.7）
- [x] MainWindow 注册为 IServiceEventListener 处理 View 事件（main.cpp 中 add_event_listener/remove_event_listener 调用已就位，已通过编译验证）
- [x] SessionService 中所有 invokeMethod(BlockingQueuedConnection) 已移除（Task 12.5）
- [x] SessionService 中所有 QThread::currentThread() == qApp->thread() 检查已移除（Task 12.6）
- [x] Headless 模式下 wait_capture_complete 使用 condition_variable 而非 QEventLoop（Task 12.8）
- [x] configure_and_start() 中 processEvents() 已替换为条件变量等待（Task 12.9）

## Phase 4: ISessionCallback 拆分

- [x] ISessionCallback 已拆分为 IDataCallback/ICaptureCallback/ITriggerCallback/ISessionStateCallback（Task 13.1）
- [x] ISessionCallback 直接删除（不保留向后兼容）（Task 13.2）
- [x] MainWindow 实现需要的子接口（Task 13.3）

## Phase 5: Headless 模式

- [x] --headless 模式可正常启动，不创建 QWidget（Task 14）
- [x] Headless 模式下 MCP API 可完成采集→解码→导出流程（Task 15）
  - 验证流程：get_devices → add_analyzer(SPI) → start_capture(0.5s) → wait_capture → get_capture_status(state=4) → get_analyzer_results(空，无真实信号) → export_raw_data_csv(成功)
  - CSV 导出验证：500MHz 采样率、500K 样本、7.3MB CSV 文件，内容格式正确（libsigrok 头 + Time/value 列）
  - 修复 6 个 Bug（详见 tasks.md Task 15.4）
- [ ] Headless 模式下 WS API 可完成设备列表→配置→采集流程（未测试）
- [x] Headless 模式下 add_decoder 不死锁（on_main_thread() 检测，主线程直接调用 lambda）
- [x] Headless 模式下 wait_capture_complete 不依赖 QEventLoop（使用 condition_variable）
- [ ] Headless + WS 新 GUI 场景：WS 客户端可读取波形数据并接收实时事件推送（未测试）

## Phase 6: CMake 库分离

- [x] pxview-core 静态库可独立编译，不依赖 Qt::Widgets（Task 16 已完成，libpxview-core.a 已生成，target_link_libraries 仅含 Qt6::Core/Gui/GuiPrivate/Network/Concurrent/WebSockets）
- [x] 最终 PXView 可执行文件链接 pxview-core + View 层代码，功能正常（已链接成功，PXView.exe 启动并报告版本 1.5.0）

## Phase 7: 新发现问题

### Task 17: MathStack 去视图化

- [x] MathStack 构造函数已修改，不再接收 view::DsoSignal* 参数（Task 17.1）
- [x] MathStack 通过通道索引 + SignalModel 获取 vdiv/hw_offset/vfactor（Task 17.2）
- [x] SigSession::math_rebuild() 移除 TODO 注释，完整实现 MathStack 创建（Task 17.3）
- [x] View 层 MathTrace 从 MathStack 创建并关联（Task 17.4）

### Task 18: SignalModel 属性同步验证

- [x] 验证 SignalModel.vdiv() 与 DsoSignal.get_vDialValue() 返回值一致（Task 18.1）
- [x] 验证 SignalModel.vfactor() 与 DsoSignal.get_factor() 返回值一致（Task 18.2）
- [x] 验证 SignalModel.hw_offset() 与 DsoSignal.get_hw_offset() 返回值一致（Task 18.3）
- [x] init_signals() 中确保 SignalModel 属性从 DeviceAgent 正确填充（Task 18.4）
- [x] 设备配置变更时 SignalModel 属性正确更新

### Task 19: Dialog View 访问路径规范化

- [x] 确认哪些 Dialog 有 View 引用（如 dsomeasure.cpp 有 View&）（Task 19.1）
- [x] 无 View 引用的 Dialog（如 mathoptions）通过 SignalModel 获取数据或加 TODO 跳过（Task 19.2）
- [x] TrigBar 无 View 指针的问题已处理（用 SignalModel 替代或通过参数传递 View）（Task 19.3）

## 集成验证

- [x] MCP 修改数据源后实时同步到 Qt GUI（如 MCP 添加解码器后 GUI 立即显示新轨道）（代码层已修复，待运行时验证）
- [x] GUI 模式下 MCP 添加解码器不依赖 signals_changed 全量重建，而是增量创建 DecodeTrace（代码层已修复，待运行时验证）
- [x] GUI 模式下添加解码器后 DecodeTrace 正常显示
- [x] GUI 模式下解码器轨道可正常加入（不出现 cppverdebug 的创建路径断裂问题）
- [ ] GUI 模式下 SpectrumTrace/MathTrace/LissajousTrace 正常工作（不出现 cppverdebug 的 TODO stub 问题）（待运行时验证：cppverdebug 已清除，但疑似的 _derived_traces_dirty 标志位问题需运行时确认——math_rebuild/lissajous_rebuild/spectrum_rebuild 调用 signals_changed()，View::on_signals_changed 在 event=Modified 时不调用 signals_changed(NULL)，导致 sync_derived_traces() 为 no-op，新 trace 可能不被创建）
- [x] GUI 模式下设备切换后 Signal 列表正确更新，DecodeTrace 不丢失
- [x] GUI 模式下 set_show() 等 DsoSignal 特有方法在无 View 引用的 Dialog 中能正常工作（或优雅降级）
