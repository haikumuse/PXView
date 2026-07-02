# Checklist

## A. 阶段 1（P0 必然崩溃 / 异步期批量闪退）

### Task 1: safe_current_view + ICaptureCallback null 检查
- [ ] `mainwindow.h` 新增 `view::View* safe_current_view() const;` 声明
- [ ] `mainwindow.cpp` 实现 `safe_current_view()` 作为统一访问入口
- [ ] `on_signals_changed` / `frame_began` / `frame_ended` / `receive_end` / `receive_data` / `receive_len` / `update_capture` 等 ICaptureCallback / ISessionStateCallback 入口（约 10 处）改用 `safe_current_view()` + 守卫
- [ ] mainwindow.cpp 中其余 50+ 处 `current_view()->` 裸调全部替换
- [ ] grep `current_view()->` 在 mainwindow.cpp 无裸调残留（除 `safe_current_view` 内部 1 处）
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] GUI 回归：Tab 关闭后异步广播到达 → 不闪退

### Task 2: 工作线程直碰 QWidget 三处修复
- [ ] `dialogs/calibration.cpp:312-315` `reload_value()` 内 QSlider 访问改走 `QMetaObject::invokeMethod(this, [...], Qt::QueuedConnection)`
- [ ] `dock/protocoldock.cpp:1035-1043` `search_done()` 内 QWidget/QSortFilterProxyModel 访问改走 marshal
- [ ] `dialogs/protocolexp.cpp:173-175` `save_proc()` 内 QCheckBox 状态改为预先在 GUI 线程收集快照传给 worker
- [ ] grep `QtConcurrent::run` + `\[&\]` 在 PXView/pv/dialogs 和 PXView/pv/dock 下无工作线程直碰 QWidget 残留
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] GUI 回归：校准对话框 / 协议 dock 搜索 / 协议导出 三个场景无 ASSERT

### Task 3: 触发器单一真相源运行时强制
- [ ] `data/signalmodel.cpp:200-232` `SignalModel::commit_trig()` 无 `ds_trigger_probe_set` / `ds_trigger_set_en` 调用
- [ ] `dock/triggerdock.cpp:1301, 1343` else 分支不再调 `commit_trig()`，统一走 Core TriggerConfig 写入
- [ ] `api/session_service.cpp:1380-1385` `set_logic_trigger_config` 无 `ds_trigger_set_stage` / `set_en` 调用
- [ ] `api/session_service.cpp:1342-1343, 1375` `get_logic_trigger_config` 从 Core `_session->trigger_config().to_json()` 读
- [ ] grep `ds_trigger_*` 仅出现在 `sigsession.cpp::sync_trigger_to_libsigrok` + libsigrok 内部
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] GUI + MCP 回归：配置触发 → 保存 .pxc → 重新加载 → 触发配置恢复；GUI/MCP 互相不覆盖

### Task 4: assert(false) 后继续执行 14 处 verify + 兜底
- [ ] `core/datafeedparser.cpp:260-281` 五分支 payload NULL 守卫已加
- [ ] `data/decoderstack.cpp:719-722` session==NULL 后早期 return
- [ ] `data/decoderstack.cpp:830-833` _decoder_status==NULL 后早期 return
- [ ] `data/decoderstack.cpp:847-848` pdata->pdo/di 三级链式 NULL 检查
- [ ] `sigsession.cpp:524-527, 690-693` !have_instance 后早期 return
- [ ] `data/dsosnapshot.cpp:367-374` order==-1 后早期 return（不返回 _ch_data[-1]）
- [ ] `data/logicsnapshot.cpp:597-602, 654-658` NULL/越界后早期 return
- [ ] `view/dsosignal.cpp:1719-1735` _vDial==NULL / height()==0 后早期 return
- [ ] `dock/protocoldock.cpp:424, 439, 1230-1235` inputs/id NULL 后早期 return
- [ ] `data/decode/annotationrestable.cpp:131-134` index 越界后早期 return
- [ ] `data/mathstack.cpp:118-122` m1/m2 nullptr 后早期 return + 不构造半完成对象
- [ ] `view/view.cpp:1320-1324` !have_instance 后早期 return
- [ ] `core/capturemanager.cpp:130-133` !have_instance 后早期 return
- [ ] `deviceagent.cpp:404-413` 多处 assert(false) 后早期 return；不返回未初始化 pattern_mode
- [ ] grep `assert\(false\)` 后续 5 行内必有 return/throw
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

## B. 阶段 2（P1 异步期 UAF / 共享状态无锁）

### Task 5: DsoSignal / AnalogSignal set_config_uint16 异步化
- [ ] `view/dsosignal.cpp:517-526, 573-583` `broadcast_msg(SAMPLE_COUNT_UPDATED)` 改 `Qt::QueuedConnection` 异步投递
- [ ] `view/analogsignal.cpp:353-359` 同样改异步
- [ ] 函数尾部不访问 `this->_model` 等成员
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] GUI 回归：DSO/Analog 通道调整 offset → 不闪退

### Task 6: Broadcast 语义注释修正 + tabcontext 冗余 broadcast 删除
- [ ] `view/view.cpp:2468, 2562, 2618` 注释改为"broadcast 经 Qt::QueuedConnection 异步投递"
- [ ] grep "broadcast 是同步直接调用" 在 view.cpp 0 命中
- [ ] `tabcontext.cpp:87-105` reload 后不再 `broadcast_msg(DEVICE_OPTIONS_UPDATED)`
- [ ] Tab 切换时设备选项仍能正确刷新（通过其他路径或显式调用）
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] GUI 回归：Tab 切换不触发二次 reload（日志验证）

### Task 7: 裸 this 跨线程 lambda 加 QPointer 守卫
- [ ] `api/session_service.cpp:2384, 2819` QTimer::singleShot lambda 加 QPointer 守卫
- [ ] `api/ws_transport.cpp:147` invokeMethod lambda 加 QPointer 守卫
- [ ] `winnativewidget.cpp:330` QTimer::singleShot lambda 加 QPointer 守卫
- [ ] `dialogs/waitingdialog.cpp:111-113, 139-142` QtConcurrent::run 加 QPointer + future.wait
- [ ] `dialogs/calibration.cpp:285-287` 同上
- [ ] `data/mmap_allocator.cpp:253` fire-and-forget thread 改受控对象成员 + join
- [ ] `mainwindow.cpp:2840-2843` OnMessage 重投递 lambda 加 QPointer 守卫
- [ ] grep `invokeMethod\(qApp` 和 `singleShot\(0, qApp` 全部带 QPointer/weak_ptr 守卫
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

### Task 8: GUI 线程同步 join 改异步
- [ ] `core/documentregistry.cpp:33, 55` CaptureOwnerGuard 析构 join 加超时（5s）+ 失败 detach + 警告日志
- [ ] `sigsession.cpp:1587` OnMessage(DSV_MSG_REV_END_PACKET) 同步 join 改异步
- [ ] `api/session_service.cpp:3278, 3328` store.wait() 改 QtConcurrent::run + QFutureWatcher
- [ ] `dialogs/storeprogress.cpp:111-114` ~StoreProgress wait 加超时 + 取消机制
- [ ] `api/session_service.cpp:213-221` invoke_or_call 持锁路径断言或文档化
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] GUI 回归：repeat 模式采集 → Tab 关闭不冻结 UI；保存大文件 → UI 可响应

### Task 9: FilterProcessor 共享状态加锁 + restart_decoders 守卫
- [ ] `core/filterprocessor.h` 新增 `std::mutex _view_data_mutex;`
- [ ] `core/filterprocessor.cpp:48-127, 164-231` glitch_filter_task / signal_invert_task 读 _view_data 持锁
- [ ] `sigsession.cpp` clear_glitch_filter 写 _logic_backup 持同一 mutex
- [ ] `sigsession.cpp:2018-2027` restart_decoders 加 is_copy_in_progress 守卫
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] GUI 回归：开启 glitch filter 采集 → 中途 clear_glitch_filter → 不闪退

### Task 10: 跨对象持裸指针改弱引用 / 守卫
- [ ] `dock/protocoldock.cpp:520, 580` `layer->_trace` 改 `std::weak_ptr<DecoderStack>`
- [ ] `dock/protocoldock.cpp:712, 716, 717, 723` `decoder_model->setDecoderStack` 改弱引用
- [ ] `dialogs/decoderoptionsdlg.cpp:483` `DecoderGroupBox` 构造改传 shared_ptr
- [ ] `view/analogsignal.cpp` 10 处函数局部持 `sr_channel*` 改短期持 + 复检 `_model` 存活
- [ ] `view/dsosignal.cpp` 12 处函数局部持 `sr_channel*` 改短期持 + 复检 `_model` 存活
- [ ] `storesession.cpp:939-964` ChannelStateRestorer 加设备存活守卫
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] GUI 回归：添加/移除 decoder → protocoldock 不闪退；导出大文件中切设备 → 不闪退

## C. 阶段 3（P2 防御性加固 / 类型强制）

### Task 11: EventBus dispatch_to 拷贝快照 + broadcast_msg weak_ptr
- [ ] `core/eventbus.h` `dispatch_to<Iface>()` 拷贝快照后遍历
- [ ] `core/eventbus.h:69-71` typed `broadcast<T>()` 拷贝快照
- [ ] `core/eventbus.cpp:47-54, 56-62` `broadcast_msg` / `trigger_message` lambda 捕获改 weak_ptr / QPointer
- [ ] EventBus 析构时取消 qApp 队列中的 pending lambda（abort 标志或 weak_ptr 失效）
- [ ] 单元测试：listener 在 OnMessage 里同步 add/remove listener → 不 UAF
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

### Task 12: ~View disconnect 顺序
- [ ] `view/view.cpp:312-322` `~View` 范围 for 中 delete 前先 disconnect signals
- [ ] Signal / DecodeTrace 有 `disconnect_signals()` 公共方法
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] ASan/UBSan 报告无析构链 UAF

### Task 13: DecodeTaskManager 析构顺序核查
- [ ] `sigsession.h` unique_ptr 成员声明顺序正确（DecodeTaskManager 析构在 DecoderStack 之前）
- [ ] 或改用 shared_ptr 注入避免析构顺序耦合
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] ASan/UBSan 报告无析构期 UAF

### Task 14: emit 显式 Qt::QueuedConnection
- [ ] `dock/searchdock.cpp:326` connect 加 `Qt::QueuedConnection`
- [ ] `dialogs/protocolexp.cpp:188` connect 加 `Qt::QueuedConnection`
- [ ] `storesession.cpp` 14 处信号订阅全部加 `Qt::QueuedConnection`
- [ ] `mainwindow.cpp:732-768` 一大批 `_event` connect 加 `Qt::QueuedConnection`
- [ ] grep 默认 `connect(` 后无第 5 参数的跨线程 emit 连接 → 0 命中
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

## D. 阶段 4（P3 静默失败 / 一致性）

### Task 15: 解码任务静默失败兜底
- [ ] `view/view.cpp:2476-2477` add_decoder silent 路径在 Core 层加 assert + log
- [ ] `api/session_service.cpp:2849-2905` add_analyzer 嵌套 QEventLoop::exec 改 QFutureWatcher
- [ ] MCP 流程 `add_analyzer` → `start_capture` → `wait_capture` → `get_analyzer_results` 正常
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

### Task 16: storesession type() → sr_type() 一致性
- [ ] `storesession.cpp:820` `(int)m->type()` 改 `m->sr_type()`
- [ ] `_export_channel_type` 赋值来源是 `SR_CHANNEL_*`
- [ ] 全文件 grep `m->type()` 与 `m->sr_type()` 风格一致性审计完成
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] 导出回归：只勾选 logic 通道 → 导出 CSV → 只含 logic 通道数据

## E. 最终验证

- [ ] 阶段 1 完成后增量编译 0 error + Tab 关闭时序竞态不闪退回归
- [ ] 阶段 2 完成后增量编译 0 error + DsoSignal/Analog set_config 不 UAF 回归 + FilterProcessor 加锁无死锁
- [ ] 阶段 3 完成后增量编译 0 error + EventBus listener 增删不 UAF + ASan/UBSan 无析构链 UAF
- [ ] 阶段 4 完成后增量编译 0 error + MCP add_analyzer 流程正常
- [ ] grep `current_view()->` 在 mainwindow.cpp 仅 `safe_current_view` 内部 1 处
- [ ] grep `ds_trigger_*` 仅在 `sigsession.cpp::sync_trigger_to_libsigrok` + libsigrok 内部
- [ ] grep `assert\(false\)` 后续 5 行内必有 return/throw
- [ ] grep `QtConcurrent::run\(\[&\]` 在 PXView/pv/dialogs 和 PXView/pv/dock 下无工作线程直碰 QWidget
- [ ] grep `invokeMethod\(qApp` 和 `singleShot\(0, qApp` 全部带 QPointer/weak_ptr 守卫
- [ ] grep 默认 `connect(` 后无第 5 参数的跨线程 emit 连接 → 0 命中
- [ ] ASan/UBSan headless 模式 MCP 压力测试无 UAF/死锁
- [ ] project_memory.md 新增 Lessons Learned（safe_current_view 兜底、commit_trig 走 Core、EventBus 拷贝快照）
- [ ] AGENTS.md State Sync Conventions 更新（safe_current_view 说明 + 工作线程碰 QWidget 禁令）
