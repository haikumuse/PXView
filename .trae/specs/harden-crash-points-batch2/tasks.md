# Tasks

> 优先级：P0（必然崩溃 / 异步期批量闪退）→ P3（静默失败 / 一致性）。
> 前置 spec：`harden-remaining-crash-risks`（代码改动已落地）、`fix-broadcast-sync-async-race`（已完成）、`fix-mmap-async-crash-risks`（已完成）。
> 与 `purify-architecture-concepts`（进行中）正交，可并行委托。

## 阶段 1（P0 必然崩溃 / 异步期批量闪退）

- [ ] Task 1: 抽 `MainWindow::safe_current_view()` + ICaptureCallback / on_signals_changed 入口 null 检查
  - [ ] SubTask 1.1: `mainwindow.h` 新增 `view::View* safe_current_view() const;` 声明，注释明确"可能返回 nullptr，调用方必须 if 守卫"
  - [ ] SubTask 1.2: `mainwindow.cpp` 实现 `safe_current_view()` 直接 return `current_view()`（命名变化作为后续替换的统一入口）；保留 `current_view()` 不删（内部调用点）
  - [ ] SubTask 1.3: 替换 `on_signals_changed` / `frame_began` / `frame_ended` / `receive_end` / `receive_data` / `receive_len` / `update_capture` 等 ICaptureCallback / ISessionStateCallback 入口（约 10 处）改为 `auto view = safe_current_view(); if (!view) { pxv_dbg(...); return; }`
  - [ ] SubTask 1.4: 替换 mainwindow.cpp 中其余 50+ 处 `current_view()->` 裸调为 `safe_current_view()->` + 守卫（或局部变量缓存 + null 检查）
  - [ ] SubTask 1.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error；grep `current_view()->` 无裸调残留（除 `safe_current_view` 内部 1 处）

- [ ] Task 2: 工作线程直碰 QWidget 三处修复
  - [ ] SubTask 2.1: `dialogs/calibration.cpp:312-315` `QtConcurrent::run([&]{ reload_value(); })` → `reload_value()` 内对 `QSlider::setRange/setValue`、`objectName()` 的访问改走 `QMetaObject::invokeMethod(this, [...]{ ... }, Qt::QueuedConnection)` marshal 回 GUI 线程；工作线程只读计算结果
  - [ ] SubTask 2.2: `dock/protocoldock.cpp:1035-1043` `QtConcurrent::run([&]{ search_done(); })` → `search_done()` 内对 `_ann_search_edit->text()` / `_matchs_label->setText` / `_model_proxy.setFilterFixedString` 的访问改走 `QMetaObject::invokeMethod` marshal 回 GUI 线程；工作线程只做匹配计算
  - [ ] SubTask 2.3: `dialogs/protocolexp.cpp:173-175` `QtConcurrent::run([&]{ save_proc(); })` → `save_proc()` 内对 `std::list<QCheckBox*>` 的 `isChecked()` / `property()` 访问改为预先在 GUI 线程收集状态快照传给 worker；不在工作线程读 QWidget
  - [ ] SubTask 2.4: 参照 `dock/deviceoptionsdock.cpp:553-582` 范式（QThreadPool/QtConcurrent worker 内通过 `QMetaObject::invokeMethod(this, [...])` 显式 marshal 回 GUI 线程）
  - [ ] SubTask 2.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error；GUI 测试——校准对话框、协议 dock 搜索、协议导出三个场景无 ASSERT/段错误

- [ ] Task 3: 触发器单一真相源运行时强制
  - [ ] SubTask 3.1: `data/signalmodel.cpp:200-232` `SignalModel::commit_trig()` 删除 `ds_trigger_probe_set` / `ds_trigger_set_en` 调用，改为只更新 Core `_trig_type` 字段；保留方法签名（向后兼容调用点）
  - [ ] SubTask 3.2: `dock/triggerdock.cpp:1301, 1343` else 分支调 `s->commit_trig()` 删除或改走 Core `TriggerConfig` 写入（与 view 分支统一）
  - [ ] SubTask 3.3: `api/session_service.cpp:1380-1385` `set_logic_trigger_config` 删除 `ds_trigger_set_stage` / `ds_trigger_set_en` 调用，改写 Core `_session->set_trigger_config(TriggerConfig::from_json(...))`
  - [ ] SubTask 3.4: `api/session_service.cpp:1342-1343, 1375` `get_logic_trigger_config` 改从 `_session->trigger_config().to_json()` 读，不再调 `ds_trigger_get_en` / `ds_trigger_get_pos`
  - [ ] SubTask 3.5: grep 全工程确认 `ds_trigger_*` 调用仅在 `sigsession.cpp::sync_trigger_to_libsigrok` 内（单一同步入口）+ libsigrok 内部
  - [ ] SubTask 3.6: 验证：`cd build && ninja -j 16 && ninja install` 0 error；GUI + MCP 测试——配置触发 → 保存 → 重新加载 → 触发配置恢复，且 GUI/MCP 互相不覆盖

- [ ] Task 4: Verify harden Task 1 对 14 处 `assert(false)` 后继续执行模式的覆盖率
  - [ ] SubTask 4.1: 逐个 verify 以下 14 处是否已加显式 `if(!ptr) { pxv_err(...); return; }` 守卫（与 harden Task 1 的 `assert(ptr)` 守卫模式互补）：
    - `core/datafeedparser.cpp:260-281`（meta/trigger/logic/dso/analog 五分支 payload NULL）
    - `data/decoderstack.cpp:719-722`（session==NULL 后继续调 srd API）
    - `data/decoderstack.cpp:830-833`（_decoder_status==NULL 后继续读）
    - `data/decoderstack.cpp:847-848`（pdata->pdo/di 三级链式）
    - `sigsession.cpp:524-527` 和 `:690-693`（!have_instance 后继续 get_channels）
    - `data/dsosnapshot.cpp:367-374`（order==-1 后 _ch_data[-1] 越界）
    - `data/logicsnapshot.cpp:597-602` 和 `:654-658`（_dest_ptr==NULL 写 NULL；index 越界 allocate_block）
    - `view/dsosignal.cpp:1719-1735`（_vDial==NULL 后 get_factor；height()==0 除零）
    - `dock/protocoldock.cpp:424, 439, 1230-1235`（dec->inputs NULL；id==NULL strncpy）
    - `data/decode/annotationrestable.cpp:131-134`（index 越界 m_resourceTable[index]）
    - `data/mathstack.cpp:118-122`（m1/m2 nullptr 后成员仍 NULL）
    - `view/view.cpp:1320-1324`（!have_instance 后 get_work_mode）
    - `core/capturemanager.cpp:130-133`（!have_instance 后访问 _device_status）
    - `deviceagent.cpp:404-413`（多处 assert(false) 后继续；返回未初始化 pattern_mode）
  - [ ] SubTask 4.2: 未加守卫的位置补 `if(!ptr) { pxv_err(...); return <default>; }`；保留 assert 作为开发期断言
  - [ ] SubTask 4.3: 验证：`cd build && ninja -j 16 && ninja install` 0 error；grep `assert\(false\)` 后续 5 行内必有 return/throw

## 阶段 2（P1 异步期 UAF / 共享状态无锁）

- [ ] Task 5: DsoSignal / AnalogSignal set_config_uint16 异步化
  - [ ] SubTask 5.1: `view/dsosignal.cpp:517-526, 573-583` `set_config_uint16` 内的 `config_changed` → `broadcast_msg(SAMPLE_COUNT_UPDATED)` 改为 `QMetaObject::invokeMethod(qApp, [this]{ ...broadcast_msg...; }, Qt::QueuedConnection)` 异步投递
  - [ ] SubTask 5.2: `view/analogsignal.cpp:353-359` 同样改异步
  - [ ] SubTask 5.3: 函数尾部不再访问任何 `this` 成员（`_model` 等）；本地 `auto model = _model;` 保活模式保留作为兜底
  - [ ] SubTask 5.4: 验证：`cd build && ninja -j 16 && ninja install` 0 error；GUI 测试——DSO/Analog 通道调整 offset → 不闪退；reload 在 set_config 栈帧展开后执行

- [ ] Task 6: Broadcast 语义注释修正 + tabcontext 冗余 broadcast 删除
  - [ ] SubTask 6.1: `view/view.cpp:2468, 2562, 2618` 删除"broadcast 是同步直接调用（非 Qt queued）"等过时注释，改写为"broadcast 经 `Qt::QueuedConnection` 异步投递到 qApp 事件循环"
  - [ ] SubTask 6.2: 评估这三处是否需要显式同步等待 reload 完成（如条件变量）；如不需要则保留异步语义，如需要则改为显式同步调用 + 等待
  - [ ] SubTask 6.3: `tabcontext.cpp:87-105` 删除 reload 后的 `broadcast_msg(DEVICE_OPTIONS_UPDATED)`（避免 OnMessage → rebuild_signals_from_config → 二次 reload）
  - [ ] SubTask 6.4: 确认删除后 Tab 切换时设备选项仍能正确刷新（如需通过其他路径或显式调用刷新）
  - [ ] SubTask 6.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error；GUI 测试——Tab 切换不触发二次 reload（用日志或断点验证）

- [ ] Task 7: 裸 this 跨线程 lambda 加 QPointer 守卫
  - [ ] SubTask 7.1: `api/session_service.cpp:2384, 2819` `QTimer::singleShot(0, qApp, [this, decoder_stack]{...})` 改为 lambda 内 `QPointer<SessionService> guard(this); if (!guard) return; ...` 守卫
  - [ ] SubTask 7.2: `api/ws_transport.cpp:147` `QMetaObject::invokeMethod(qApp, [this, msg]{ send_to_clients(msg); }, ...)` 同样加 QPointer 守卫
  - [ ] SubTask 7.3: `winnativewidget.cpp:330` `QTimer::singleShot(0, self->_childWidget, [=]{ self->setShadowStatus(st); })` 改为捕获 QPointer 守卫 `self`
  - [ ] SubTask 7.4: `dialogs/waitingdialog.cpp:111-113, 139-142` `QtConcurrent::run([&]{ _device_agent->set_config_bool(...); })` 改为 `[&, this]()` 捕获 + QPointer 守卫；并加 future.wait() 或 QFutureWatcher 确保对话框析构前 worker 完成
  - [ ] SubTask 7.5: `dialogs/calibration.cpp:285-287` 同 SubTask 7.4
  - [ ] SubTask 7.6: `data/mmap_allocator.cpp:253` `std::thread([file_to_delete]{...})` fire-and-forget 改为受控对象成员 + join 析构期，或显式 detach + 进程退出前 join
  - [ ] SubTask 7.7: `mainwindow.cpp:2840-2843` OnMessage 跨线程重投递 lambda 捕获 `this`，加 QPointer 守卫
  - [ ] SubTask 7.8: 验证：`cd build && ninja -j 16 && ninja install` 0 error；grep `invokeMethod\(qApp` 和 `singleShot\(0, qApp` 全部带 QPointer/weak_ptr 守卫

- [ ] Task 8: GUI 线程同步 join 改异步
  - [ ] SubTask 8.1: `core/documentregistry.cpp:33, 55` `CaptureOwnerGuard` 析构 `join_copy_thread()` 改为加超时（如 5s `wait_for`），超时后 detach + 警告日志；move-assign 运算符同样处理
  - [ ] SubTask 8.2: `sigsession.cpp:1587` `OnMessage(DSV_MSG_REV_END_PACKET)` 同步 `copy_thread().join()` 改异步——投递到 worker 线程或用 `QtConcurrent::run` 后台等待，OnMessage 提前返回
  - [ ] SubTask 8.3: `api/session_service.cpp:3278, 3328` `store.wait()` 改 `QtConcurrent::run` + `QFutureWatcher` 异步等待，UI 期间可响应
  - [ ] SubTask 8.4: `dialogs/storeprogress.cpp:111-114` `~StoreProgress()` `wait()` 加超时（如 5s）+ 取消机制（如 `_store_session->cancel()`）；`closeEvent` 中 `is_busy()` 检查加固
  - [ ] SubTask 8.5: `api/session_service.cpp:213-221` `invoke_or_call` 在持锁路径上加断言 `assert(!locks_held_on_current_thread())` 禁止跨线程 BlockingQueued（如可行）；或文档化"持锁禁用 invoke_or_call"约束
  - [ ] SubTask 8.6: 验证：`cd build && ninja -j 16 && ninja install` 0 error；GUI 测试——repeat 模式采集 → Tab 关闭不冻结 UI；保存大文件 → UI 可响应

- [ ] Task 9: FilterProcessor 共享状态加锁 + restart_decoders 守卫
  - [ ] SubTask 9.1: `core/filterprocessor.h` 新增 `std::mutex _view_data_mutex;` 成员
  - [ ] SubTask 9.2: `core/filterprocessor.cpp:48-127, 164-231` `glitch_filter_task` / `signal_invert_task` 读 `_session->_view_data` / `_view_data->_logic_backup` / `_glitch_filter_active/thresholds/modes` / `_signal_invert_*` 时持 `std::lock_guard<std::mutex> lock(_view_data_mutex);`
  - [ ] SubTask 9.3: `sigsession.cpp` `clear_glitch_filter` 写 `_view_data->_logic_backup` 时同样持 `_view_data_mutex`（需暴露给 FilterProcessor 或注入 shared_ptr<mutex>）
  - [ ] SubTask 9.4: `sigsession.cpp:2018-2027` `restart_decoders()` 加 `if (is_copy_in_progress()) { /* 异步队列等待 */ } else { copy_data_to_document(doc); }`；如异步队列复杂可先记日志 + return，让用户重试
  - [ ] SubTask 9.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error；GUI 测试——开启 glitch filter 采集 → 中途 clear_glitch_filter → 不闪退；repeat 模式 restart_decoders → 不闪退

- [ ] Task 10: 跨对象持裸指针改弱引用 / 守卫
  - [ ] SubTask 10.1: `dock/protocoldock.cpp:520, 580` `layer->_trace = stack.get()` 改为 `layer->_trace = std::weak_ptr<DecoderStack>(stack)`；使用处 `auto sp = layer->_trace.lock(); if (!sp) { skip; }`
  - [ ] SubTask 10.2: `dock/protocoldock.cpp:712, 716, 717, 723` `decoder_model->setDecoderStack(decode_sigs.at(0).get())` 同样改为传 `shared_ptr<DecoderStack>` 或弱引用
  - [ ] SubTask 10.3: `dialogs/decoderoptionsdlg.cpp:483` `new DecoderGroupBox(_trace->decoder().get(), ...)` 改为传 `shared_ptr` 或弱引用
  - [ ] SubTask 10.4: `view/analogsignal.cpp` `137/147/260/268/276/284/292/300/309/357` 和 `view/dsosignal.cpp` `186/241/282/323/431/469/519/553/579/594/628/1319` 函数局部持 `sr_channel*` 改为短期持——取指针后立即用，不跨函数调用；函数体首部 `if (!_model) return;` 守卫，函数体中如跨调用则复检 `_model` 存活
  - [ ] SubTask 10.5: `storesession.cpp:939-964` `ChannelStateRestorer` RAII 构造时保存 `sdi` 弱引用（如 `QPointer` 或 `std::weak_ptr`），dtor 中 `if (sdi 已释放) { skip; } else { restore }`；或在 dtor 中 try/catch + log
  - [ ] SubTask 10.6: 验证：`cd build && ninja -j 16 && ninja install` 0 error；GUI 测试——添加/移除 decoder → protocoldock 不闪退；导出大文件中切设备 → 不闪退

## 阶段 3（P2 防御性加固 / 类型强制）

- [ ] Task 11: EventBus dispatch_to 拷贝快照 + broadcast_msg weak_ptr
  - [ ] SubTask 11.1: `core/eventbus.h` `dispatch_to<Iface>()` 模板实现改为 `auto snapshot = _callbacks; for (auto *cb : snapshot) fn(iface);`——拷贝快照后遍历，listener 在回调中 add/remove 不影响当前遍历
  - [ ] SubTask 11.2: `core/eventbus.h:69-71` typed `broadcast<T>()` 同样改为拷贝快照
  - [ ] SubTask 11.3: `core/eventbus.cpp:47-54, 56-62` `broadcast_msg` / `trigger_message` lambda 捕获 `EventBus* this` 改为 `std::weak_ptr<EventBus>` 或 `QPointer<EventBus>`；EventBus 析构时显式取消 qApp 队列中的 pending lambda（如用 abort 标志）
  - [ ] SubTask 11.4: 评估 EventBus 改为 `std::shared_ptr` 持有是否可行（让 weak_ptr 守卫生效）；如不可行则用 abort 标志
  - [ ] SubTask 11.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error；单元测试——listener 在 OnMessage 里同步 add/remove listener → 不 UAF

- [ ] Task 12: ~View disconnect 顺序
  - [ ] SubTask 12.1: `view/view.cpp:312-322` `~View` 范围 for 中 delete 改为：先 `for (auto sig : _own_signals) sig->disconnect_signals();` 显式断开所有 signal/slot，再 `for (auto sig : _own_signals) delete sig;`
  - [ ] SubTask 12.2: 同样处理 `_own_decode_traces`
  - [ ] SubTask 12.3: 评估 Signal/DecodeTrace 是否需要 `disconnect_signals()` 公共方法；如已存在则用，否则新增
  - [ ] SubTask 12.4: 验证：`cd build && ninja -j 16 && ninja install` 0 error；GUI 测试——Tab 关闭 → 不闪退；ASan/UBSan 报告无析构链 UAF

- [ ] Task 13: DecodeTaskManager 析构顺序核查
  - [ ] SubTask 13.1: `sigsession.h` 核查 `unique_ptr<DecodeTaskManager>` / `unique_ptr<DocumentRegistry>` / `unique_ptr<DecoderStack>` 等成员声明顺序（C++ 析构逆序）
  - [ ] SubTask 13.2: 确保 DecodeTaskManager 析构在 DecoderStack 析构之前（即声明在后）；或改用 shared_ptr 注入避免析构顺序耦合
  - [ ] SubTask 13.3: `core/decodetaskmanager.cpp:23-37` `~DecodeTaskManager` `stop()` 内 join `_decode_threads` 期间，确保 DecoderStack 仍存活（如用 shared_ptr 注入则自动保证）
  - [ ] SubTask 13.4: 验证：`cd build && ninja -j 16 && ninja install` 0 error；ASan/UBSan 报告无析构期 UAF

- [ ] Task 14: emit 显式 Qt::QueuedConnection
  - [ ] SubTask 14.1: `dock/searchdock.cpp:326` `connect(this, &SearchDock::search_result_found, this, &SearchDock::on_search_result)` 加 `Qt::QueuedConnection`
  - [ ] SubTask 14.2: `dialogs/protocolexp.cpp:188` `connect` 加 `Qt::QueuedConnection`
  - [ ] SubTask 14.3: `storesession.cpp` 信号订阅（`progress_updated` 等 14 处）全部加 `Qt::QueuedConnection`
  - [ ] SubTask 14.4: `mainwindow.cpp:732-768` 一大批 `connect(&_event, &EventObject::X, this, &MainWindow::on_X)` 加 `Qt::QueuedConnection`
  - [ ] SubTask 14.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error；grep 默认 `connect(` 后无第 5 参数的跨线程 emit 连接，全部显式 QueuedConnection

## 阶段 4（P3 静默失败 / 一致性）

- [ ] Task 15: 解码任务静默失败兜底
  - [ ] SubTask 15.1: `view/view.cpp:2476-2477` `add_decoder` silent 路径在 Core 层 `sigsession.cpp` `add_decoder` 内加 `if (silent) { pxv_warn("add_decoder silent=true, caller MUST call start_all_decode_tasks/rst_decoder"); }` + `assert(!silent || caller_will_start);`（如可行）
  - [ ] SubTask 15.2: `api/session_service.cpp:2849-2905` `add_analyzer` 嵌套 `QEventLoop::exec()` 改 `QFutureWatcher` + 信号等待；避免外层 queued broadcast 重入
  - [ ] SubTask 15.3: 评估嵌套事件循环移除后 MCP 行为是否一致（`wait_capture` / `get_analyzer_results` 仍能拿到结果）
  - [ ] SubTask 15.4: 验证：`cd build && ninja -j 16 && ninja install` 0 error；MCP 测试——`add_analyzer` → `start_capture` → `wait_capture` → `get_analyzer_results` 流程正常

- [ ] Task 16: storesession type() → sr_type() 一致性
  - [ ] SubTask 16.1: `storesession.cpp:820` `(int)m->type() != _export_channel_type` 改为 `m->sr_type() != _export_channel_type`
  - [ ] SubTask 16.2: 确认 `_export_channel_type` 赋值来源是 `SR_CHANNEL_*`（如 `m->sr_type()`）；若不是则统一来源
  - [ ] SubTask 16.3: 全文件 grep `m->type()` 与 `m->sr_type()` 风格一致性审计；非 libsigrok 边界用 `type()`，传给 libsigrok/导出过滤用 `sr_type()`
  - [ ] SubTask 16.4: 验证：`cd build && ninja -j 16 && ninja install` 0 error；导出测试——只勾选 logic 通道 → 导出 CSV → 只含 logic 通道数据

## Task Dependencies

- Task 1-4（阶段 1）互不依赖，可并行
- Task 5（DsoSignal 异步化）独立，可并行
- Task 6（broadcast 注释 + tabcontext）独立，可并行
- Task 7（裸 this lambda 守卫）独立，可并行
- Task 8（GUI 线程 join 异步）独立，可并行
- Task 9（FilterProcessor 加锁）独立，可并行
- Task 10（跨对象弱引用）独立，可并行
- Task 11（EventBus 拷贝快照）影响所有 listener，建议单独批次，验证后再合并
- Task 12（~View disconnect）独立，可与 Task 11 并行
- Task 13（析构顺序核查）独立，可与 Task 11 并行
- Task 14（emit Qt::QueuedConnection）独立，可并行
- Task 15-16（P3）独立，可并行

## Parallelizable Work

- **第一批 P0（4 个 sub-agent 并行）**：Task 1、Task 2、Task 3、Task 4
- **第二批 P1（6 个 sub-agent 并行）**：Task 5、Task 6、Task 7、Task 8、Task 9、Task 10
- **第三批 P2（Task 11 单独 + 3 个并行）**：Task 11 单独批次（EventBus 影响大）；Task 12、Task 13、Task 14 并行
- **第四批 P3（2 个并行）**：Task 15、Task 16
- **跨阶段**：阶段 1 / 阶段 4 与 `purify-architecture-concepts` 正交，可并行委托

## 风险控制

- **阶段 1 Task 1（最高风险）**：替换 60+ 处 `current_view()->` 涉及面广，建议先 grep 全部命中点，逐文件替换并独立编译验证；可拆为多个小 PR
- **阶段 2 Task 8（高风险）**：`OnMessage(DSV_MSG_REV_END_PACKET)` 同步 join 改异步影响采集数据流，需 headless 模式 MCP 压力测试验证不丢帧
- **阶段 2 Task 9（中风险）**：FilterProcessor 加锁可能导致死锁（持锁期间触发 broadcast 又被队列回 GUI 线程），需评估锁粒度，建议用 `shared_mutex` 读写分离
- **阶段 3 Task 11（最高风险）**：EventBus `dispatch_to` 拷贝快照改 + `broadcast_msg` weak_ptr 改影响全应用事件分发枢纽，建议先单元测试 listener 增删场景，再合入
- **阶段 4 Task 15（中风险）**：`add_analyzer` 嵌套事件循环移除可能影响 MCP 同步语义，需评估 MCP 客户端兼容性

## 不在本 spec 范围

- `harden-remaining-crash-risks` Task 1-8 已覆盖的 8 类风险（本 spec A4 仅 verify + 兜底）
- `fix-broadcast-sync-async-race` 已覆盖的 `init_signals` 后广播延迟投递（本 spec B1 是 set_config 触发，互补不重叠）
- `fix-mmap-async-crash-risks` 已覆盖的 mmap 异步写入链路（本 spec B4 mmap_allocator fire-and-forget thread 是进程退出期风险，互补）
- `purify-architecture-concepts` 阶段 7 God class 拆分（本 spec Task 8/9 与之正交，但 Task 8/9 完成后数据下沉会更顺）
- libsigrok 驱动 compat 修复（独立 spec `tiered-driver-compat-fix`）
- 跨平台 crash_handler 支持（独立 spec）
