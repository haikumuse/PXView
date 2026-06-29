# Tasks

## Phase 1: 纯数据模型层（无破坏性变更）

- [x] Task 1: 创建 SignalModel 纯数据模型
  - [x] 1.1 在 `PXView/pv/data/` 下新建 `signalmodel.h/cpp`，包含通道索引、名称、类型、启用状态、探头配置、触发状态、DSO 参数、颜色、毛刺过滤/信号反转、快照指针
  - [x] 1.2 SignalModel 仅依赖 Qt Core 和标准库，不依赖 `pv::view/*` 或 Qt Widgets
  - [x] 1.3 编译验证通过

- [x] Task 2: 创建 LissajousModel 纯数据模型
  - [x] 2.1 新建 `lissajousmodel.h/cpp`，包含 x_index/y_index/percent/enabled
  - [x] 2.2 不依赖 `pv::view/*` 或 Qt Widgets

- [x] Task 3: 重新设计 DataSource 接口（v2）
  - [x] 3.1 移除旧方法（get_signals/get_decode_signals/get_spectrum_traces/get_lissajous_trace/get_math_trace）
  - [x] 3.2 新增纯数据方法：get_signal_models/get_decoder_stacks/get_spectrum_stacks/get_math_stack/get_lissajous_model
  - [x] 3.3 移除 datasource.h 中所有 `view::*` 前向声明
  - [x] 3.4 添加 DecoderStack/SpectrumStack/MathStack/SignalModel/LissajousModel 前向声明

## Phase 2: Core 层去视图化

- [x] Task 4: SigSession 去视图化
  - [x] 4.1-4.4 成员变量替换完成（SignalModel/SpectrumStack/LissajousModel/MathStack）
  - [x] 4.5-4.8 init_signals/reload/接口实现完成
  - [x] 4.9 add_decoder 输出参数改为 DecoderStack*&，移除 DecodeTrace 创建
  - [x] 4.10-4.21 各方法适配完成
  - [x] 4.22 math_rebuild() 现在可创建 MathStack（Task 17 完成后已启用，构造函数改为接收通道索引 + SignalModel）

- [x] Task 5: libsigrok 回调 API 扩展
  - [x] 5.1 在 lib_main.c 中新增 ds_set_datafeed_callback_ex 和 ds_set_event_callback_ex
  - [x] 5.2 在 libsigrok.h 中声明新回调类型
  - [x] 5.3 ds_start_collect 检查已扩展为接受 _ex 回调
  - [x] 5.4 SigSession 使用新 API 注册回调并移除 _session 静态成员（init() 调用 ds_set_*_callback_ex(this)，静态回调通过 user_data 转发到实例方法；已通过编译验证）

- [x] Task 6: SessionDocument 去视图化
  - [x] 6.1-6.5 成员变量替换完成
  - [x] 6.6-6.7 移除旧方法，实现新接口
  - [x] 6.8 clear() 释放新对象
  - [x] 6.9-6.10 save/apply_signal_config 保持不变（通过 DeviceAgent 操作）

- [x] Task 7: SessionSnapshot 去视图化
  - [x] 7.1 同步去视图化完成
  - [x] 7.2 新接口方法返回默认空列表/nullptr

## Phase 3: View 层适配

- [x] Task 8: 创建 SignalFactory
  - [x] 8.1 新建 signalfactory.h/cpp
  - [x] 8.2 create_signal 从 SignalModel 创建 LogicSignal/DsoSignal/AnalogSignal
  - [x] 8.3 create_signals 批量创建
  - [x] 8.4 update_signals 增量更新
  - [x] 8.5 save_ui_state/restore_ui_state UI 状态保存恢复
  - [x] 8.6 CMakeLists.txt 已添加 signalfactory.cpp
  - [x] 8.7 **修复**: Qt `signals` 关键字冲突 — 变量名 `signals` 改为 `result`/`sig_list`（Qt 宏将 `signals` 替换为 `protected`）

- [x] Task 9: View 层持有并管理渲染对象（部分完成）
  - [x] 9.1-9.4 View 持有 _own_decode_traces/_own_spectrum_traces/_own_math_trace/_own_lissajous_trace
  - [x] 9.9 get_traces() 使用 View 自己的渲染对象列表
  - [x] 9.10 rebuild_signals() 使用 SignalFactory::create_signals
  - [x] 9.11 新增 mark_derived_traces_dirty()/sync_derived_traces() 懒同步机制（按指针身份增量同步）
  - [x] 9.12 新增 get_own_signals()/get_own_decode_traces()/get_own_spectrum_traces()/get_own_math_trace()/get_own_lissajous_trace() 访问方法
  - [x] 9.5 View::add_decoder() 完整实现（调用 Core 创建 DecoderStack 后创建 DecodeTrace）
  - [x] 9.6 View::remove_decoder() 实现
  - [x] 9.7 View::on_signals_changed() 通过 SignalFactory 增量更新
  - [x] 9.8 View::data_updated() 刷新数据指针

- [x] Task 10: 适配 ProtocolDock
  - [x] 10.1 add_protocol() 调用 View::add_decoder()
  - [x] 10.2 rebuild_protocol_layers() 从 View::get_own_decode_traces() 获取
  - [x] 10.3 移除 get_decode_signals() 调用

- [ ] Task 11: 适配 View 层所有消费代码（分文件进行）
  - [x] 11.1 viewport.cpp/header.cpp 已修复（使用 get_own_* 方法）
  - [x] 11.2 Dock 组件适配
    - [x] 11.2.1 triggerdock.cpp — get_signals() → get_signal_models()
    - [x] 11.2.2 dsotriggerdock.cpp — get_signals() → get_signal_models()
    - [x] 11.2.3 searchdock.cpp / dialogs/search.cpp — get_signals() → get_signal_models()
    - [x] 11.2.4 protocoldock.cpp — get_decode_signals() → get_decoder_stacks()
  - [x] 11.3 对话框适配（务实策略：简单情况用 SignalModel，复杂情况加 TODO）
    - [x] 11.3.1 spectrumtrace.cpp — 构造函数用 SignalModel.color()，paint 用 _view->get_own_signals()
    - [x] 11.3.2 spectrumstack.cpp — 用 SignalModel.snapshot()/hw_offset()/vdiv()/vfactor() 替代 DsoSignal
    - [x] 11.3.3 mathoptions.cpp — 简单迭代用 SignalModel，math_rebuild 调用加 TODO
    - [x] 11.3.4 fftoptions.cpp — 用 SignalModel.name()/index()
    - [x] 11.3.5 lissajousoptions.cpp — set_show() 加 TODO（纯 View 关注点）
    - [x] 11.3.6 dsomeasure.cpp — 用 _view->get_own_signals()
    - [x] 11.3.7 protocollist.cpp — get_decode_signals() → get_decoder_stacks()
  - [x] 11.4 samplingbar.cpp 适配
  - [x] 11.5 storesession.cpp 从 SignalModel 读取通道信息
  - [x] 11.6 移除 static_cast<view::LogicSignal*> 等类型转换
  - [x] 11.7 DecoderStack 从 SignalModel 获取通道信息（decoderstack.cpp:427 的 _session->get_signals()）
  - [x] 11.8 mainwindow.cpp 适配
  - [x] 11.9 signalfactory.cpp 残留编译错误修复（如有）

- [x] Task 12: 适配 SessionService（去视图化 + 消除死锁）
  - [x] 12.1 移除 set_view() 和 _view 成员
  - [x] 12.2 新增 ServiceEvent 枚举值（ViewShowRegion/ViewZoomFit/ViewZoomIn/ViewZoomOut，及额外 ViewCursorAdded/Removed/Cleared）
  - [x] 12.3 View 操作通过 IServiceEventListener 广播
  - [x] 12.4 操作 SignalModel/DecoderStack
  - [x] 12.5 移除 invokeMethod(BlockingQueuedConnection)
  - [x] 12.6 移除线程检查
  - [x] 12.7 QApplication → QCoreApplication
  - [x] 12.8 wait_capture_complete 改用 condition_variable
  - [x] 12.9 processEvents 改为条件变量

## Phase 4: ISessionCallback 拆分

- [x] Task 13: 拆分 ISessionCallback（不保留向后兼容）
  - [x] 13.1 定义 IDataCallback/ICaptureCallback/ITriggerCallback/ISessionStateCallback
  - [x] 13.2 ISessionCallback 直接删除
  - [x] 13.3 MainWindow 实现需要的子接口

## Phase 5: Headless 模式

- [x] Task 14: main.cpp 支持 Headless 模式
- [x] Task 15: 验证 Headless 模式完整流程
  - [x] 15.1 MCP API 采集流程验证（get_devices → add_analyzer → start_capture → wait_capture → get_capture_status）
  - [x] 15.2 解码器结果查询验证（get_analyzer_results — 空结果因测试环境无真实 SPI 信号）
  - [x] 15.3 原始数据 CSV 导出验证（export_raw_data_csv — 成功生成 CSV 文件，500MHz 采样率、500K 样本）
  - [x] 15.4 修复发现的 Bug：
    - [x] add_analyzer 死锁（QMetaObject::invokeMethod + Qt::QueuedConnection 在主线程死锁）— 新增 on_main_thread() 检测
    - [x] Headless 模式解码器不启动（DSV_MSG_REV_END_PACKET else 分支缺少 frame_ended()/add_decode_task()）
    - [x] StoreSession::_suffix 未设置（SetFileName 不从扩展名推导 _suffix）
    - [x] StoreSession::export_start 空指针解引用（assert(snapshot) 在 Release 下无效）
    - [x] StoreSession::export_start 错误信息被清除（_error.clear() 在 return false 前清除了错误）
    - [x] ChannelType/SR_CHANNEL_* 枚举不匹配（SignalModel::type() 返回 ChannelType=0，get_snapshot() 期望 SR_CHANNEL_LOGIC=10000）

## Phase 6: CMake 库分离

- [x] Task 16: CMake 重构为 pxview-core 静态库
  - [x] 16.1 拆分 PXView_SOURCES 为 PXVIEW_CORE_SOURCES + PXVIEW_GUI_SOURCES（清理原列表中的重复项）
  - [x] 16.2 新增 QT_CORE_LIBS / QT_GUI_LIBS 变量分离 Qt 模块依赖
  - [x] 16.3 add_library(pxview-core STATIC ${PXVIEW_CORE_SOURCES}) 仅链接 Qt6::Core/Gui/GuiPrivate/Network/Concurrent/WebSockets
  - [x] 16.4 add_executable 使用 ${PXVIEW_GUI_SOURCES} + 链接 pxview-core + Qt6::Widgets/Svg
  - [x] 16.5 Windows 专属源文件（winnativewidget/winshadow/wintaskbarprogress/applogo.rc）追加到 PXVIEW_GUI_SOURCES
  - [x] 16.6 构建验证通过：libpxview-core.a 已生成，PXView.exe 启动正常

## Phase 7: 新发现的问题（实施过程中发现）

- [x] Task 17: MathStack 去视图化
  - [x] 17.1 修改 MathStack 构造函数，移除 view::DsoSignal* 参数，改为接收通道索引 + SignalModel
  - [x] 17.2 MathStack 从 SignalModel 获取 vdiv/hw_offset/vfactor 等
  - [x] 17.3 SigSession::math_rebuild() 移除 TODO 注释，完整实现 MathStack 创建
  - [x] 17.4 View 层 MathTrace 从 MathStack 创建并关联

- [x] Task 18: SignalModel 属性同步验证
  - [x] 18.1 验证 SignalModel.vdiv() 与 DsoSignal.get_vDialValue() 返回值一致
  - [x] 18.2 验证 SignalModel.vfactor() 与 DsoSignal.get_factor() 返回值一致
  - [x] 18.3 验证 SignalModel.hw_offset() 与 DsoSignal.get_hw_offset() 返回值一致
  - [x] 18.4 在 init_signals() 中确保 SignalModel 的属性从 DeviceAgent 正确填充

- [x] Task 19: Dialog 访问 View 的路径规范化
  - [x] 19.1 确认哪些 Dialog 有 View 引用（如 dsomeasure.cpp 有 View&）
  - [x] 19.2 对于无 View 引用的 Dialog（如 mathoptions），通过 SignalModel 获取数据或加 TODO 跳过
  - [x] 19.3 TrigBar 无 View 指针的问题 — 使用 SignalModel 替代或通过参数传递 View

# Task Dependencies

- Task 1, 2 可并行（Phase 1）✅ 已完成
- Task 3 依赖 Task 1, 2 ✅ 已完成
- Task 4 依赖 Task 3 ✅ 已完成
- Task 5 独立 ✅ 已完成（5.4 已迁移到 _ex 回调 API）
- Task 6 依赖 Task 3 ✅ 已完成
- Task 7 依赖 Task 3, 6 ✅ 已完成
- Task 8 依赖 Task 3, 4 ✅ 已完成
- Task 9 依赖 Task 8 ✅ 已完成
- Task 10 依赖 Task 9 ✅ 已完成
- Task 11 依赖 Task 9 ✅ 已完成
- Task 12 依赖 Task 4, 6, 7 ✅ 已完成
- Task 13 可与 Task 11 并行 ✅ 已完成
- Task 14 依赖 Task 4, 6, 7, 12 ✅ 已完成
- Task 15 依赖 Task 14 ✅ 已完成（Headless MCP 流程验证通过，修复 6 个 Bug）
- Task 16 依赖 Task 4, 6, 7, 12 ✅ 已完成
- Task 17 依赖 Task 4（MathStack 去视图化需要 Core 层完成）
- Task 18 依赖 Task 4（验证 SignalModel 属性正确性）
- Task 19 依赖 Task 9, 11（Dialog 适配需要 View 层完成）
