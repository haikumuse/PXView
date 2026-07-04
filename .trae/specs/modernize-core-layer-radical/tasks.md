# Tasks

## 阶段 1：SessionStateContext 中间层（彻底解除循环依赖）

- [ ] Task 1: 创建 SessionStateContext 类骨架
  - [ ] SubTask 1.1: 新建 `PXView/pv/core/sessionstatecontext.h`，定义 `core::SessionStateContext` 类，前向声明所有需要的状态类型（SessionData/DeviceAgent/SignalModel/SpectrumStack/MathStack/LissajousModel/TriggerConfig）
  - [ ] SubTask 1.2: 在头文件声明所有状态字段（private）+ 三段式 accessor（`xxx()` const getter / `xxx_mut()` mutable ref / `set_xxx(T)` setter），原子字段特殊处理（`is_working()`/`set_is_working()` 包装 atomic load/store）
  - [ ] SubTask 1.3: 新建 `sessionstatecontext.cpp`，实现构造函数初始化列表（字段默认值参考 sigsession.cpp 当前 SigSession 构造函数）
  - [ ] SubTask 1.4: 在 `PXView/pv/CMakeLists.txt` 的 `PXVIEW_CORE_SOURCES` 列表添加 `core/sessionstatecontext.cpp`（确认 Core 层独立编译）
  - [ ] SubTask 1.5: 验证：`cd build && ninja -j 16 && ninja install` 编译通过（SessionStateContext 未被引用，但应无错误）

- [ ] Task 2: 状态字段从 SigSession 迁移到 SessionStateContext
  - [ ] SubTask 2.1: 迁移 SessionData 相关：`_view_data` / `_capture_data` / `_data_list`（3 个字段）
  - [ ] SubTask 2.2: 迁移 DeviceAgent：`_device_agent`（含其内部状态，1 个字段）
  - [ ] SubTask 2.3: 迁移原子状态：`_is_working` / `_device_status`（2 个字段，atomic）
  - [ ] SubTask 2.4: 迁移触发状态：`_is_triged` / `_trigger_flag` / `_trigger_ch` / `_trig_time` / `_hw_replied`（5 个字段）
  - [ ] SubTask 2.5: 迁移会话状态：`_bClose` / `_is_saving` / `_dso_status` / `_dso_status_valid` / `_error` / `_error_pattern` / `_save_start` / `_save_end` / `_session_time` / `_map_zoom`（10 个字段）
  - [ ] SubTask 2.6: 迁移业务对象：`_signal_models` / `_spectrum_stacks` / `_math_stack` / `_lissajous_model` / `_trigger_config`（5 个字段）
  - [ ] SubTask 2.7: 迁移互斥锁：`_data_mutex` / `_sampling_mutex`（2 个字段，注意 mutex 不可移动，需用 std::unique_ptr<std::mutex> 或固定位置）
  - [ ] SubTask 2.8: 验证：grep `sigsession.h` 无 19+ private 字段（已迁移），grep `sessionstatecontext.h` 字段齐全
  - [ ] SubTask 2.9: 验证：`cd build && ninja -j 16 && ninja install` 编译通过

- [ ] Task 3: 5 个 manager 改用 SessionStateContext*
  - [ ] SubTask 3.1: `capturemanager.h`：构造函数签名 `(EventBus*, SigSession*)` → `(EventBus*, SessionStateContext*)`，成员 `SigSession *_session` → `SessionStateContext *_state`
  - [ ] SubTask 3.2: `documentregistry.h`：同上修改，CaptureOwnerGuard 内嵌类的 `_registry` 不变
  - [ ] SubTask 3.3: `decodetaskmanager.h`：同上修改
  - [ ] SubTask 3.4: `datafeedparser.h`：同上修改
  - [ ] SubTask 3.5: `filterprocessor.h`：同上修改
  - [ ] SubTask 3.6: `sigsession.h`：移除 `friend class core::XxxManager` 全部 5 处（manager 不再访问 SigSession 私有）
  - [ ] SubTask 3.7: `sigsession.cpp` init()：调整构造顺序——先 `_state = make_unique<SessionStateContext>()`，再 6 个 manager `make_unique<XxxManager>(_event_bus.get(), _state.get())`
  - [ ] SubTask 3.8: 验证：grep `pv/core/*.h` `SigSession *_session` 0 命中；grep `pv/core/*.h` `SessionStateContext *_state` 5 命中

- [ ] Task 4: 替换 263 处 _session->_xxx 深链式直访
  - [ ] SubTask 4.1: `capturemanager.cpp` 104 处：`_session->_view_data` → `_state->view_data()`；`_session->_device_agent` → `_state->device_agent()`；`_session->_is_working` → `_state->is_working()` 等等，逐处替换
  - [ ] SubTask 4.2: `datafeedparser.cpp` 81 处：同上模式替换，注意 `feed_in_*` 内的 `_session->_capture_data->get_logic()->...` 改为 `_state->capture_data()->get_logic()->...`
  - [ ] SubTask 4.3: `filterprocessor.cpp` 64 处：三层深链式 `_session->_view_data->_logic_backup` → `_state->view_data()->_logic_backup`（_logic_backup 仍是 SessionData 的 public 字段，无需再封装）或新增 `SessionStateContext::view_logic_backup()` 短路 accessor
  - [ ] SubTask 4.4: `decodetaskmanager.cpp` 11 处：替换 `_session->_signal_models` / `_session->decode_traces()` 等
  - [ ] SubTask 4.5: `documentregistry.cpp` 3 处：替换 `_session->is_working()` 等
  - [ ] SubTask 4.6: SigSession.cpp 内 100+ 处 `this->_xxx` / `_xxx` 直访改为 `_state->xxx()`（因字段已下沉）
  - [ ] SubTask 4.7: 验证：grep `pv/core/*.cpp` `_session->_` 0 命中；grep `sigsession.cpp` `_state->` 100+ 命中
  - [ ] SubTask 4.8: 验证：`cd build && ninja -j 16 && ninja install` 0 error

- [ ] Task 5: SigSession 退化为 thin facade
  - [ ] SubTask 5.1: `sigsession.h` private 区仅保留 `unique_ptr<SessionStateContext> _state` + 6 个 `unique_ptr<manager>` + `unique_ptr<EventBus> _event_bus` + `_device_event` + `_next_decoder_handle_id`（无法下沉的 ID 生成器）
  - [ ] SubTask 5.2: public 方法体改为 forward stub：`is_working() { return _state->is_working(); }`、`set_error(s) { _state->set_error(s); }` 等等
  - [ ] SubTask 5.3: 移除 SigSession 私有 helper 方法（`data_updated()` / `set_receive_data_len()` / `receive_header()` / `frame_began()` 等），改为 manager 直接通过 `_state` + `_event_bus` 完成等价工作
  - [ ] SubTask 5.4: 验证：sigsession.h 行数从 284 → < 150 行（thin facade 应远小于原 God class）
  - [ ] SubTask 5.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error

## 阶段 2：DocumentRegistry 所有权上移

- [ ] Task 6: DocumentRegistry 改持 unique_ptr<SessionDocument>
  - [ ] SubTask 6.1: `documentregistry.h` 成员 `std::vector<SessionDocument*> _all_documents` → `std::vector<std::unique_ptr<SessionDocument>> _owned_documents`
  - [ ] SubTask 6.2: `_active_document` 改为 `size_t _active_document_index = SIZE_MAX`（无活动文档标记）
  - [ ] SubTask 6.3: `_capture_owner_document` 改为 `size_t _capture_owner_index = SIZE_MAX`
  - [ ] SubTask 6.4: CaptureOwnerGuard 内嵌类成员 `data::SessionDocument *_doc` → `size_t _doc_index`，`doc()` 方法返回 `_registry->_owned_documents[_doc_index].get()`
  - [ ] SubTask 6.5: 新增 `SessionDocument* get_document_by_index(size_t) const` 公有 accessor 供 TabContext/SessionService 获取弱引用
  - [ ] SubTask 6.6: `register_document(SessionDocument*)` 改为 `take_document(std::unique_ptr<SessionDocument>)`，返回分配的 index
  - [ ] SubTask 6.7: 新增 `release_document(size_t index)` 方法，从 _owned_documents 移除并销毁（注意 index 失效后需通知所有弱引用持有者，或采用 stable index 方案如 std::vector<std::unique_ptr> + 标记删除）
  - [ ] SubTask 6.8: 新增 `create_api_document()` 工厂方法（包装 `make_unique<SessionDocument>` + `take_document`），供 SessionService 使用
  - [ ] SubTask 6.9: 验证：grep `documentregistry.h` 无 `SessionDocument *_` 裸指针成员（除弱引用 accessor 返回值）

- [ ] Task 7: TabContext 改持弱引用
  - [ ] SubTask 7.1: `tabcontext.h` 成员 `SessionDocument *_document` 保留（弱引用语义），但构造函数改为接收 `SessionDocument*` 而非 `new SessionDocument`
  - [ ] SubTask 7.2: `tabcontext.cpp:52` `delete _document` 改为 `_document_registry->release_document(_doc_index)`（TabContext 持有 DocumentRegistry* 引用 + size_t index）
  - [ ] SubTask 7.3: MainWindow 创建新 Tab 时改为 `auto doc_idx = _session->document_registry()->take_document(make_unique<SessionDocument>(...)); auto* doc = _session->document_registry()->get_document_by_index(doc_idx); auto* tab = new TabContext(doc, doc_idx, _session->document_registry());`
  - [ ] SubTask 7.4: 验证：grep `tabcontext.cpp` `delete _document` 0 命中；grep `tabcontext.cpp` `new SessionDocument` 0 命中

- [ ] Task 8: SessionService 改用 DocumentRegistry 工厂
  - [ ] SubTask 8.1: `session_service.h` 移除 `SessionDocument *_api_document` 成员，改为 `size_t _api_doc_index`
  - [ ] SubTask 8.2: `session_service.cpp:65/301` `new pv::data::SessionDocument(session)` 改为 `_session->document_registry()->create_api_document()` 返回 index
  - [ ] SubTask 8.3: `session_service.cpp:307/322` `delete _api_document` 改为 `_session->document_registry()->release_document(_api_doc_index)`
  - [ ] SubTask 8.4: `app_service.cpp` 同步修改（如有 new SessionDocument 调用）
  - [ ] SubTask 8.5: 验证：grep `pv/api/*.cpp` `new SessionDocument` 0 命中；grep `pv/api/*.cpp` `delete _api_document` 0 命中
  - [ ] SubTask 8.6: 验证：`cd build && ninja -j 16 && ninja install` 0 error

## 阶段 3：legacy IMessageListener 完全移除

- [ ] Task 9: EventBus 新增 broadcast_sync<T>()
  - [ ] SubTask 9.1: `eventbus.h` 新增模板方法 `template<typename T> void broadcast_sync(const T& ev)`——直接同步调用所有 IEventListener::on_event(ev)，不通过 Qt::QueuedConnection 队列
  - [ ] SubTask 9.2: 复用 `broadcast<T>()` 的 thread_local `_broadcast_depth` 循环护栏，防止同步嵌套递归
  - [ ] SubTask 9.3: 添加单元测试或断言：`broadcast_sync` 在主线程调用时立即执行 on_event，不进事件队列
  - [ ] SubTask 9.4: 验证：`cd build && ninja -j 16 && ninja install` 0 error

- [ ] Task 10: 新增 StoreConfPrev 事件结构体
  - [ ] SubTask 10.1: `events.h` 新增 `struct StoreConfPrev {}` 结构体（对应原 DSV_MSG_STORE_CONF_PREV）
  - [ ] SubTask 10.2: `IEventListener` 新增 `virtual void on_event(const StoreConfPrev &) {}` 默认实现
  - [ ] SubTask 10.3: MainWindow override `on_event(const StoreConfPrev &)`，调用原 OnMessage 中 DSV_MSG_STORE_CONF_PREV case 的处理逻辑
  - [ ] SubTask 10.4: 验证：grep `events.h` `StoreConfPrev` 命中（结构体 + IEventListener 虚函数）

- [ ] Task 11: SigSession pre/post ordering 改用 broadcast_sync
  - [ ] SubTask 11.1: 替换 `DSV_MSG_CURRENT_DEVICE_CHANGE_PREV` 调用点为 `broadcast_sync<CurrentDeviceChanged>({})`
  - [ ] SubTask 11.2: 替换 `DSV_MSG_START_COLLECT_WORK_PREV` 调用点为 `broadcast_sync<StartCollectWork>({})`
  - [ ] SubTask 11.3: 替换 `DSV_MSG_STORE_CONF_PREV` 调用点为 `broadcast_sync<StoreConfPrev>({})`
  - [ ] SubTask 11.4: 评估 `DSV_MSG_CAPTURE_OWNER_CHANGED` 是否需要同步语义——若需同步则改 `broadcast_sync<CaptureOwnerChanged>`，否则保留 `broadcast<CaptureOwnerChanged>`
  - [ ] SubTask 11.5: 验证：grep `sigsession.cpp` `broadcast_sync<` 命中 3-4 处；grep `sigsession.cpp` `DSV_MSG_.*_PREV` 0 命中

- [ ] Task 12: 移除 IMessageListener 接口与 DSV_MSG_* 宏
  - [ ] SubTask 12.1: `interface/icallbacks.h` 删除 `IMessageListener` 类定义
  - [ ] SubTask 12.2: `interface/icallbacks.h` 删除全部 43 个 `DSV_MSG_*` 宏定义
  - [ ] SubTask 12.3: `interface/icallbacks.h` 删除 `add_msg_listener` / `remove_msg_listener` 接口
  - [ ] SubTask 12.4: `sigsession.h` 移除 `IMessageListener` 基类继承，移除 `OnMessage` 方法声明
  - [ ] SubTask 12.5: `sigsession.cpp` 删除 `OnMessage` 方法实现（39 case 翻译表 + 5 case fallback 全部移除）
  - [ ] SubTask 12.6: `mainwindow.h` 移除 `OnMessage` 方法声明（如有继承自 IMessageListener）
  - [ ] SubTask 12.7: `mainwindow.cpp` 删除 `OnMessage` 方法实现（5 case fallback 全部删除）
  - [ ] SubTask 12.8: grep 工程内 `DSV_MSG_` 0 代码命中（仅注释或已删除）
  - [ ] SubTask 12.9: grep 工程内 `IMessageListener` 0 命中
  - [ ] SubTask 12.10: 验证：`cd build && ninja -j 16 && ninja install` 0 error

## 阶段 4：DataUpdated 事件接线

- [ ] Task 13: DataFeedParser 发射 DataUpdated
  - [ ] SubTask 13.1: `datafeedparser.cpp` `feed_in_logic` 末尾（return 前）添加 `_event_bus->broadcast<interface::DataUpdated>({})`
  - [ ] SubTask 13.2: `feed_in_dso` 末尾同上
  - [ ] SubTask 13.3: `feed_in_analog` 末尾同上
  - [ ] SubTask 13.4: `events.h` 移除 DataUpdated 注释中的"dead-code" / "no emitter" 字样，改为"emitted by DataFeedParser::feed_in_*"
  - [ ] SubTask 13.5: MainWindow::on_event(DataUpdated) 实现具体逻辑——调用 `on_data_updated` 处理器（替代原 DSV_MSG_DATA_UPDATED 路径）
  - [ ] SubTask 13.6: 验证：grep `datafeedparser.cpp` `broadcast<interface::DataUpdated>` 3 命中
  - [ ] SubTask 13.7: 验证：`cd build && ninja -j 16 && ninja install` 0 error

## 阶段 5：编译验证与回归测试

- [ ] Task 14: 编译与回归
  - [ ] SubTask 14.1: `cd build && ninja -j 16 && ninja install` 0 error
  - [ ] SubTask 14.2: 启动 `install.dir/bin/PXView.exe`，验证 GUI 模式启动无崩溃
  - [ ] SubTask 14.3: 启动 `install.dir/bin/PXView.exe --headless`，验证 Headless 模式启动无崩溃
  - [ ] SubTask 14.4: `ctest` 全部测试用例通过
  - [ ] SubTask 14.5: MCP 端到端测试：用 curl 或 Postman 调用 `get_devices` → `add_analyzer` → `start_capture` → `wait_capture` → `get_capture_status` → `get_analyzer_results` → `export_raw_data_csv`，全流程无错误

- [ ] Task 15: 文档更新
  - [ ] SubTask 15.1: `AGENTS.md` 更新 Key Files 表——新增 `sessionstatecontext.h/.cpp`，移除 `icallbacks.h` 中 IMessageListener 引用
  - [ ] SubTask 15.2: `AGENTS.md` State Sync Conventions 更新——单一状态同步通道（broadcast<T> + broadcast_sync<T>），移除"legacy IMessageListener paths remain"条目
  - [ ] SubTask 15.3: `project_memory.md` 新增 Lessons Learned——SessionStateContext 中间层、DocumentRegistry 所有权上移、IMessageListener 完全移除、DataUpdated 接线

# Task Dependencies

- Task 1（SessionStateContext 骨架）无依赖
- Task 2（字段迁移）依赖 Task 1
- Task 3（manager 改 *_state）依赖 Task 2
- Task 4（替换 263 处直访）依赖 Task 3
- Task 5（SigSession thin facade）依赖 Task 4
- Task 6（DocumentRegistry unique_ptr）独立，可与 Task 1-5 并行
- Task 7（TabContext 弱引用）依赖 Task 6
- Task 8（SessionService 工厂）依赖 Task 6，可与 Task 7 并行
- Task 9（broadcast_sync）独立，可与 Task 1-8 并行
- Task 10（StoreConfPrev 事件）依赖 Task 9
- Task 11（pre/post ordering 改用 broadcast_sync）依赖 Task 9 + Task 10
- Task 12（移除 IMessageListener）依赖 Task 11
- Task 13（DataUpdated 接线）独立，可与 Task 1-12 并行
- Task 14（编译回归）依赖 Task 1-13 全部完成
- Task 15（文档更新）依赖 Task 14 验证通过

# Parallelizable Work

- 阶段 1：Task 1 → Task 2 → Task 3 → Task 4 → Task 5 严格串行（依赖链）
- 阶段 2：Task 6 → (Task 7 + Task 8 并行)
- 阶段 3：Task 9 → Task 10 → Task 11 → Task 12 严格串行
- 阶段 4：Task 13 独立
- 跨阶段并行：阶段 1 / 阶段 2 / 阶段 3 Task 9 / 阶段 4 可全并行（无文件冲突）

# 风险控制

- **阶段 1 Task 4（263 处直访替换）**：高风险——工作量大，每处需确认 accessor 签名匹配。建议先完成 Task 1-3 让编译器报错，再逐文件修复（编译器驱动重构）。每修一个文件立即编译验证。
- **阶段 1 Task 5（SigSession thin facade）**：高风险——SigSession 是 Core 层中心节点，瘦身可能暴露隐藏的私有方法依赖。建议保留必要的 private helper 转发方法，逐步消除。
- **阶段 2 Task 6（DocumentRegistry 所有权）**：中风险——stable index 方案需谨慎设计，避免 close tab 后 index 失效。建议采用 `std::vector<std::unique_ptr>` + 标记删除（unique_ptr 为 nullptr 表示已释放），index 在 vector 生命周期内稳定。
- **阶段 3 Task 12（移除 IMessageListener）**：中风险——需确认无遗漏的 IMessageListener 子类。建议先 grep 工程内 `IMessageListener` 和 `DSV_MSG_` 全部命中，逐一替换后再删除接口。
- **阶段 3 Task 9（broadcast_sync）**：低风险——新增方法，不破坏现有代码。
- **阶段 4 Task 13（DataUpdated 接线）**：低风险——3 处新增 broadcast 调用。
- **跨阶段冲突**：阶段 1 修改 sigsession.h/.cpp + 5 个 manager；阶段 2 修改 documentregistry + tabcontext + session_service；阶段 3 修改 eventbus + icallbacks + mainwindow。**冲突点**：阶段 1 Task 3 修改 manager 头文件，阶段 3 Task 11 修改 sigsession.cpp 内的 broadcast 调用——建议阶段 1 完成后再启动阶段 3 Task 11。
