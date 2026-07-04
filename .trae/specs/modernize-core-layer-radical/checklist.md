# Checklist

## A. 阶段 1：SessionStateContext 中间层

### Task 1: SessionStateContext 类骨架
- [ ] `PXView/pv/core/sessionstatecontext.h` 已创建，定义 `core::SessionStateContext` 类
- [ ] 头文件前向声明所有状态类型（SessionData/DeviceAgent/SignalModel/SpectrumStack/MathStack/LissajousModel/TriggerConfig）
- [ ] 三段式 accessor 模式声明完成（`xxx()` const / `xxx_mut()` mutable / `set_xxx(T)` setter）
- [ ] 原子字段特殊处理（`is_working()`/`set_is_working()` 包装 atomic load/store）
- [ ] `sessionstatecontext.cpp` 已创建，构造函数初始化列表完整
- [ ] `PXView/pv/CMakeLists.txt` `PXVIEW_CORE_SOURCES` 已添加 `core/sessionstatecontext.cpp`
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 编译通过

### Task 2: 状态字段迁移
- [ ] SessionData 3 字段迁移完成（`_view_data`/`_capture_data`/`_data_list`）
- [ ] DeviceAgent 1 字段迁移完成（`_device_agent`）
- [ ] 原子状态 2 字段迁移完成（`_is_working`/`_device_status`）
- [ ] 触发状态 5 字段迁移完成（`_is_triged`/`_trigger_flag`/`_trigger_ch`/`_trig_time`/`_hw_replied`）
- [ ] 会话状态 10 字段迁移完成（`_bClose`/`_is_saving`/`_dso_status`/`_dso_status_valid`/`_error`/`_error_pattern`/`_save_start`/`_save_end`/`_session_time`/`_map_zoom`）
- [ ] 业务对象 5 字段迁移完成（`_signal_models`/`_spectrum_stacks`/`_math_stack`/`_lissajous_model`/`_trigger_config`）
- [ ] 互斥锁 2 字段迁移完成（`_data_mutex`/`_sampling_mutex`，用 unique_ptr<std::mutex> 包装）
- [ ] 验证：grep `sigsession.h` 无 19+ private 字段
- [ ] 验证：grep `sessionstatecontext.h` 字段齐全（28+ 字段）
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 编译通过

### Task 3: 5 个 manager 改用 SessionStateContext*
- [ ] `capturemanager.h` 构造函数签名改为 `(EventBus*, SessionStateContext*)`，成员 `_state` 替换 `_session`
- [ ] `documentregistry.h` 同上修改
- [ ] `decodetaskmanager.h` 同上修改
- [ ] `datafeedparser.h` 同上修改
- [ ] `filterprocessor.h` 同上修改
- [ ] `sigsession.h` 移除 `friend class core::XxxManager` 全部 5 处
- [ ] `sigsession.cpp` init() 调整构造顺序（SessionStateContext 先于 manager 构造）
- [ ] 验证：grep `pv/core/*.h` `SigSession *_session` 0 命中
- [ ] 验证：grep `pv/core/*.h` `SessionStateContext *_state` 5 命中

### Task 4: 替换 263 处 _session->_xxx 深链式直访
- [ ] `capturemanager.cpp` 104 处替换完成
- [ ] `datafeedparser.cpp` 81 处替换完成
- [ ] `filterprocessor.cpp` 64 处替换完成
- [ ] `decodetaskmanager.cpp` 11 处替换完成
- [ ] `documentregistry.cpp` 3 处替换完成
- [ ] `sigsession.cpp` 100+ 处 `this->_xxx` / `_xxx` 直访替换为 `_state->xxx()`
- [ ] 验证：grep `pv/core/*.cpp` `_session->_` 0 命中
- [ ] 验证：grep `sigsession.cpp` `_state->` 100+ 命中
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

### Task 5: SigSession 退化为 thin facade
- [ ] `sigsession.h` private 区仅余 `unique_ptr<SessionStateContext> _state` + 6 个 `unique_ptr<manager>` + `unique_ptr<EventBus> _event_bus` + `_device_event` + `_next_decoder_handle_id`
- [ ] public 方法体改为 forward stub（`is_working()` → `_state->is_working()` 等）
- [ ] 私有 helper 方法移除或转发到 manager
- [ ] 验证：sigsession.h 行数 < 150（原 284）
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

## B. 阶段 2：DocumentRegistry 所有权上移

### Task 6: DocumentRegistry 改持 unique_ptr<SessionDocument>
- [ ] `documentregistry.h` 成员 `_all_documents` 改为 `std::vector<std::unique_ptr<SessionDocument>> _owned_documents`
- [ ] `_active_document` 改为 `size_t _active_document_index = SIZE_MAX`
- [ ] `_capture_owner_document` 改为 `size_t _capture_owner_index = SIZE_MAX`
- [ ] CaptureOwnerGuard 内嵌类 `_doc` 改为 `size_t _doc_index`，`doc()` 返回 `_registry->_owned_documents[_doc_index].get()`
- [ ] 新增 `SessionDocument* get_document_by_index(size_t) const` 公有 accessor
- [ ] `register_document(SessionDocument*)` 改为 `take_document(std::unique_ptr<SessionDocument>)`，返回 index
- [ ] 新增 `release_document(size_t index)` 方法（标记删除或稳定 index 方案）
- [ ] 新增 `create_api_document()` 工厂方法
- [ ] 验证：grep `documentregistry.h` 无 `SessionDocument *_` 裸指针成员（除弱引用 accessor）

### Task 7: TabContext 改持弱引用
- [ ] `tabcontext.h` 成员 `_document` 保留（弱引用语义），新增 `size_t _doc_index` + `DocumentRegistry* _registry`
- [ ] `tabcontext.cpp` 构造函数改为接收 `(SessionDocument*, size_t, DocumentRegistry*)`
- [ ] `tabcontext.cpp:52` `delete _document` 改为 `_registry->release_document(_doc_index)`
- [ ] MainWindow 创建 Tab 时改用 `take_document` + `get_document_by_index`
- [ ] 验证：grep `tabcontext.cpp` `delete _document` 0 命中
- [ ] 验证：grep `tabcontext.cpp` `new SessionDocument` 0 命中

### Task 8: SessionService 改用 DocumentRegistry 工厂
- [ ] `session_service.h` 移除 `SessionDocument *_api_document`，改为 `size_t _api_doc_index`
- [ ] `session_service.cpp:65/301` `new SessionDocument` 改为 `create_api_document()`
- [ ] `session_service.cpp:307/322` `delete _api_document` 改为 `release_document(_api_doc_index)`
- [ ] `app_service.cpp` 同步修改
- [ ] 验证：grep `pv/api/*.cpp` `new SessionDocument` 0 命中
- [ ] 验证：grep `pv/api/*.cpp` `delete _api_document` 0 命中
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

## C. 阶段 3：legacy IMessageListener 完全移除

### Task 9: EventBus broadcast_sync<T>()
- [ ] `eventbus.h` 新增 `template<typename T> void broadcast_sync(const T& ev)` 模板方法
- [ ] 实现同步直发（不通过 Qt::QueuedConnection 队列）
- [ ] 复用 `broadcast<T>()` 的 thread_local `_broadcast_depth` 循环护栏
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

### Task 10: StoreConfPrev 事件结构体
- [ ] `events.h` 新增 `struct StoreConfPrev {}` 结构体
- [ ] `IEventListener` 新增 `virtual void on_event(const StoreConfPrev &) {}` 默认实现
- [ ] MainWindow override `on_event(const StoreConfPrev &)`，调用原 DSV_MSG_STORE_CONF_PREV 处理逻辑
- [ ] 验证：grep `events.h` `StoreConfPrev` 命中（结构体 + IEventListener 虚函数）

### Task 11: pre/post ordering 改用 broadcast_sync
- [ ] `DSV_MSG_CURRENT_DEVICE_CHANGE_PREV` 调用点改为 `broadcast_sync<CurrentDeviceChanged>({})`
- [ ] `DSV_MSG_START_COLLECT_WORK_PREV` 调用点改为 `broadcast_sync<StartCollectWork>({})`
- [ ] `DSV_MSG_STORE_CONF_PREV` 调用点改为 `broadcast_sync<StoreConfPrev>({})`
- [ ] `DSV_MSG_CAPTURE_OWNER_CHANGED` 同步语义评估完成并迁移
- [ ] 验证：grep `sigsession.cpp` `broadcast_sync<` 3-4 命中
- [ ] 验证：grep `sigsession.cpp` `DSV_MSG_.*_PREV` 0 命中

### Task 12: 移除 IMessageListener 接口与 DSV_MSG_* 宏
- [ ] `interface/icallbacks.h` 删除 `IMessageListener` 类定义
- [ ] `interface/icallbacks.h` 删除全部 43 个 `DSV_MSG_*` 宏定义
- [ ] `interface/icallbacks.h` 删除 `add_msg_listener` / `remove_msg_listener` 接口
- [ ] `sigsession.h` 移除 `IMessageListener` 基类继承 + `OnMessage` 方法声明
- [ ] `sigsession.cpp` 删除 `OnMessage` 方法实现
- [ ] `mainwindow.h` 移除 `OnMessage` 方法声明
- [ ] `mainwindow.cpp` 删除 `OnMessage` 方法实现
- [ ] 验证：grep 工程内 `DSV_MSG_` 0 代码命中（仅注释或已删除）
- [ ] 验证：grep 工程内 `IMessageListener` 0 命中
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

## D. 阶段 4：DataUpdated 事件接线

### Task 13: DataFeedParser 发射 DataUpdated
- [ ] `datafeedparser.cpp` `feed_in_logic` 末尾添加 `broadcast<interface::DataUpdated>({})`
- [ ] `feed_in_dso` 末尾同上
- [ ] `feed_in_analog` 末尾同上
- [ ] `events.h` 移除 DataUpdated 注释的 "dead-code" / "no emitter" 字样
- [ ] MainWindow::on_event(DataUpdated) 实现具体逻辑（调用 on_data_updated 处理器）
- [ ] 验证：grep `datafeedparser.cpp` `broadcast<interface::DataUpdated>` 3 命中
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

## E. 阶段 5：编译验证与回归测试

### Task 14: 编译与回归
- [ ] `cd build && ninja -j 16 && ninja install` 0 error
- [ ] GUI 模式启动 `install.dir/bin/PXView.exe` 无崩溃
- [ ] Headless 模式启动 `install.dir/bin/PXView.exe --headless` 无崩溃
- [ ] `ctest` 全部测试用例通过
- [ ] MCP 端到端测试通过（get_devices → add_analyzer → start_capture → wait_capture → get_capture_status → get_analyzer_results → export_raw_data_csv）

### Task 15: 文档更新
- [ ] `AGENTS.md` Key Files 表新增 `sessionstatecontext.h/.cpp`，移除 `icallbacks.h` IMessageListener 引用
- [ ] `AGENTS.md` State Sync Conventions 更新——单一状态同步通道（broadcast<T> + broadcast_sync<T>）
- [ ] `project_memory.md` 新增 Lessons Learned（SessionStateContext / 所有权上移 / IMessageListener 移除 / DataUpdated 接线）

## F. 最终架构验证

- [ ] grep `pv/core/*.h` `SigSession *_session` 0 命中（循环依赖彻底解除）
- [ ] grep `pv/core/*.cpp` `_session->_` 0 命中（封装穿透消除）
- [ ] grep `pv/core/*.h` `friend class` 0 命中（manager 无任何 friend）
- [ ] grep `documentregistry.h` `SessionDocument *_` 0 命中（除弱引用 accessor 返回值）
- [ ] grep `pv/api/*.cpp` `new SessionDocument` 0 命中
- [ ] grep `pv/api/*.cpp` `delete _api_document` 0 命中
- [ ] grep `tabcontext.cpp` `delete _document` 0 命中
- [ ] grep 工程内 `IMessageListener` 0 命中
- [ ] grep 工程内 `DSV_MSG_` 0 代码命中（仅注释或已删除）
- [ ] grep `mainwindow.cpp` `OnMessage` 0 命中（方法已删除）
- [ ] grep `datafeedparser.cpp` `broadcast<interface::DataUpdated>` 3 命中
- [ ] grep `events.h` `StoreConfPrev` 命中（结构体 + IEventListener 虚函数）
- [ ] sigsession.h 行数 < 150（thin facade）
- [ ] project_memory.md 与 AGENTS.md 已更新
- [ ] GUI + Headless 运行时回归通过
