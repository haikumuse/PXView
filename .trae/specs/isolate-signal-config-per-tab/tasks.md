# Tasks

- [x] Task 1: SessionDocument 新增 SignalConfig 结构体和字段
  - [x] SubTask 1.1: 在 sessiondocument.h 中定义 SignalConfig 结构体（work_mode、operation_mode、channel_mode、ChannelConfig 向量）
  - [x] SubTask 1.2: 在 SessionDocument 中新增 _signal_config 和 _pending_device_config 字段
  - [x] SubTask 1.3: 实现 SignalConfig 的序列化/反序列化方法（toJSON/fromJSON），用于文件保存/加载
  - [x] SubTask 1.4: 实现 SessionDocument::save_signal_config()，从 DeviceAgent 读取当前硬件配置并保存到 _signal_config
  - [x] SubTask 1.5: 实现 SessionDocument::apply_signal_config()，将 _signal_config 写入 DeviceAgent 恢复硬件配置
  - [x] SubTask 1.6: 实现 SessionDocument::apply_pending_config()，检查并应用 _pending_device_config

- [x] Task 2: View 新增 rebuild_signals_from_config() 方法
  - [x] SubTask 2.1: 在 view.h 中声明 rebuild_signals_from_config(const SignalConfig &config) 方法
  - [x] SubTask 2.2: 实现 rebuild_signals_from_config()：根据 SignalConfig 创建对应的 Signal 子类对象（LogicSignal/AnalogSignal/DsoSignal）
  - [x] SubTask 2.3: 修改 rebuild_signals()：优先调用 rebuild_signals_from_config()，仅当 Document 无 SignalConfig 时回退到从 _data_source->get_signals() 克隆
  - [x] SubTask 2.4: 确保新创建的信号对象的数据指针指向 SessionDocument 的 Snapshot（如果有数据）

- [x] Task 3: TabContext::activate() 恢复硬件配置
  - [x] SubTask 3.1: 修改 activate()：在非采集状态下调用 SessionDocument::apply_signal_config() 恢复硬件
  - [x] SubTask 3.2: 修改 activate()：在采集状态下将配置保存到 _pending_device_config，不写硬件
  - [x] SubTask 3.3: 修改 activate()：调用 View::rebuild_signals_from_config() 从 SignalConfig 重建信号
  - [x] SubTask 3.4: 修改 deactivate()：调用 SessionDocument::save_signal_config() 保存当前硬件配置

- [x] Task 4: 采集停止后自动应用 pending 配置
  - [x] SubTask 4.1: 在 MainWindow DSV_MSG_END_COLLECT_WORK 处理器中检查当前活跃标签的 _pending_device_config
  - [x] SubTask 4.2: 如果有 pending 配置，调用 SessionDocument::apply_pending_config() 应用到硬件
  - [x] SubTask 4.3: 应用后清除 _pending_device_config 并刷新 DeviceOptionsDock UI

- [x] Task 5: 修复 DeviceOptionsDock::get_session() 数据来源
  - [x] SubTask 5.1: 修改 get_session()：work_mode 从 UI 控件或缓存读取，不从 _device_agent 读取
  - [x] SubTask 5.2: 修改 get_session()：通道使能从 _probes_checkBox_list 读取，不从 probe->enabled 读取
  - [x] SubTask 5.3: 修改 get_session()：模拟参数从 UI 控件读取，不从 _device_agent 读取
  - [x] SubTask 5.4: 验证 bind_context → set_session → unbind_context → get_session 往返一致性

- [x] Task 6: 实现 ProtocolDock::unbind_context() 状态保存
  - [x] SubTask 6.1: 在 SessionDocument 中新增 _dock_protocol_search_text 和 _dock_protocol_expanded_states 字段
  - [x] SubTask 6.2: 修改 ProtocolDock::unbind_context()：保存搜索关键词和协议层展开状态
  - [x] SubTask 6.3: 修改 ProtocolDock::bind_context()：从 SessionDocument 恢复搜索关键词和展开状态

- [x] Task 7: 修复 MeasureDock cursor_row_info 序列化
  - [x] SubTask 7.1: 修改 MeasureDock::unbind_context()：在 edge_rows JSON 中增加 "channelIndex" 字段（已存在）
  - [x] SubTask 7.2: 修改 MeasureDock::bind_context()：从 JSON 恢复 channelIndex 字段（已存在）

- [x] Task 8: 修复 remove_tab Dock 绑定流程
  - [x] SubTask 8.1: 修改 MainWindow::remove_tab()：删除标签后对新活跃标签执行完整的 unbind_context + bind_context
  - [x] SubTask 8.2: 移除 remove_tab() 中临时 disconnect currentChanged 信号的 workaround，改为正确处理 on_tab_changed 流程

- [x] Task 9: 采集结束时自动保存 SignalConfig
  - [x] SubTask 9.1: 修改 MainWindow::on_frame_ended()：在 copy_data_to_document 之后调用 save_signal_config()
  - [x] SubTask 9.2: 修改 DSV_MSG_DEVICE_OPTIONS_UPDATED 处理器：变更时立即保存到 SessionDocument._signal_config

- [x] Task 10: 集成测试和验证
  - [x] SubTask 10.1: 所有修改的 C++ 文件编译通过（ninja 无 error）
  - [x] SubTask 10.2: IDE 诊断无错误
  - [ ] SubTask 10.3: 运行时验证（需要完整构建后手动测试）

# Task Dependencies
- [Task 2] depends on [Task 1] (SignalConfig 结构体必须先定义)
- [Task 3] depends on [Task 1] and [Task 2] (activate 需要 SignalConfig 和 rebuild_signals_from_config)
- [Task 4] depends on [Task 1] and [Task 3] (pending config 需要 SignalConfig 和 activate 逻辑)
- [Task 9] depends on [Task 1] (save_signal_config 需要 SignalConfig)
- [Task 10] depends on all other tasks
- [Task 5], [Task 6], [Task 7], [Task 8] are independent and can be parallelized
