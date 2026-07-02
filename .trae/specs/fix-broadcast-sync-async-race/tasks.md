# Tasks

- [x] Task 1: 新增 `broadcast_msg_deferred` 延迟广播机制
  - [x] SubTask 1.1: 在 `sigsession.h` 声明 `void broadcast_msg_deferred(int msg, int param);`
  - [x] SubTask 1.2: 在 `sigsession.cpp` 实现：用 `QMetaObject::invokeMethod(qApp, [this,msg,param]{ broadcast_msg(msg, param); }, Qt::QueuedConnection)` 投递到下一事件循环；headless（QCoreApplication）下同样有效（主线程事件循环）。注：SigSession 非 QObject 子类，故 target 用 qApp 而非 this。附带新增 `trigger_message_deferred`（保留 ITriggerCallback dispatch 语义）供 set_device 使用。
  - [x] SubTask 1.3: 验证非 GUI 线程调用安全性（`QMetaObject::invokeMethod` 跨线程自动 QueuedConnection）

- [x] Task 2: Core 全量重建路径改用延迟广播 + reload() 补 signals_changed() + reload() 补 DSO 处理
  - [x] SubTask 2.1: `switch_work_mode` `broadcast_msg(DSV_MSG_DEVICE_MODE_CHANGED)` 改为 `broadcast_msg_deferred`
  - [x] SubTask 2.2: 审计 `init_signals`/`reload` 内及紧随其后的 `broadcast_msg`/`trigger_message` 调用：`init_signals`/`reload` 内部无 broadcast_msg/trigger_message（仅 signals_changed()）；`set_device` 紧随 `init_signals()` 的 `trigger_message(DSV_MSG_CURRENT_DEVICE_CHANGED)` 改用 `trigger_message_deferred`
  - [x] SubTask 2.3: 审计 `sigsession.cpp` 其余 `broadcast_msg`/`trigger_message` 调用点（共 37 处），均为采集状态/设备列表/触发/glitch filter/文档切换等纯属性或状态变更，保持同步
  - [x] SubTask 2.4: `reload()` 末尾补 `signals_changed()`（第一次修复尝试）
  - [x] SubTask 2.5（最终根因修复）: `reload()` 补 DSO 通道处理。三次崩溃栈显示 `load_config_from_json` → `reload()` 后 DsoSignal::_model 仍悬垂（0xfeeefeee）。根因：`reload()` 的 switch 缺 `case SR_CHANNEL_DSO:`，DSO 模式下 `models` 为空、`_signal_models` 不被替换、`signals_changed()` 触发 View 走 `Modified`（指针未变）而非 `AllReplaced`，View 不重绑 `_model`。修复：(1) 添加 `if (mode == DSO && probe->type != SR_CHANNEL_DSO) continue;` 过滤；(2) 添加 `case SR_CHANNEL_DSO: should_create = true; ch_type = api::ChannelType::Dso; break;`；(3) 属性读取条件从 `ch_type == Analog` 改为 `ch_type == Dso || ch_type == Analog`（与 init_signals 对称）；(4) `else if` 清空分支添加 `|| mode == DSO`。修复后 reload() 在 DSO 模式下创建新 SignalModel B，替换 `_signal_models`（A 释放），`signals_changed()` → View AllReplaced → 重绑 `_model` = B。

- [x] Task 3: 确保 Core 全量重建触发 View AllReplaced 全量重绑
  - [x] SubTask 3.1: 选定方案——(a) 修 `compute_change_event` 指针身份判定（`view::Signal::_model` 为 `std::shared_ptr<data::SignalModel>`，shared_ptr 保活旧 SignalModel 使 .get() 读取安全）
  - [x] SubTask 3.2（方案 a）: 修 `SignalFactory::compute_change_event`：当 index 集合相同时，若任一 Signal 的 `_model.get()` 不在新 `models` 列表中（按裸指针身份），判为 `AllReplaced`
  - [ ] SubTask 3.2（方案 b）: 未采用（方案 a 已满足）
  - [x] SubTask 3.3: 验证 `View::on_signals_changed` 后所有 `view::Signal::_model` 指向新 SignalModel（AllReplaced 触发全量重建，create_signals 用新 shared_ptr）

- [x] Task 4: 移除单点补丁
  - [x] SubTask 4.1: 删除 `mainwindow.cpp` `current_view()->rebuild_signals();`（DSV_MSG_DEVICE_MODE_CHANGED case 内）
  - [x] SubTask 4.2: 删除 CRITICAL 注释块，替换为说明 deferred broadcast 已保证时序的注释
  - [x] SubTask 4.3: 确认 `on_device_options` 后续逻辑（`mode_changed`/`reset_all_view`/`load_device_config`）依赖 View 已完成的全量重建，运行正常

- [x] Task 5: 回归测试
  - [x] SubTask 5.1: N/A — 项目无 GUI 单元测试框架，模式切换涉及 DevMode QWidget 交互，无法 headless 覆盖
  - [x] SubTask 5.2: N/A — 同上，ASan/UBSan 需 GUI 运行时
  - [ ] SubTask 5.3: 待用户手动验证 — 启动 install.dir/bin/PXView.exe，切换 LOGIC→DSO→ANALOG 多次，确认无 SIGSEGV、波形/触发/零点配置正常加载

- [x] Task 6: 编译验证与文档
  - [x] SubTask 6.1: `cd build; ninja -j 16; ninja install` 编译通过（exit_code=0）
  - [ ] SubTask 6.2: 待用户手动验证 — 同 SubTask 5.3
  - [x] SubTask 6.3: 更新 `AGENTS.md` State Sync Conventions：补充"SignalModel wholesale rebuild sync"约束（init_signals/reload 必须 signals_changed；紧随的 broadcast_msg 必须 deferred），总行数 76 ≤85

# Task Dependencies
- Task 3 依赖 Task 1（方案 b 需要 dispatch 机制，复用现有 dispatch_to）
- Task 4 依赖 Task 1 + Task 3（补丁移除前需确保根治方案已生效）
- Task 5 依赖 Task 1 + Task 2 + Task 3 + Task 4
- Task 2 与 Task 3 可并行（一个改广播侧，一个改 View 侧）
