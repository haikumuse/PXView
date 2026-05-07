# 文档-视图 MVC 架构重构 Phase 2-4 任务列表

## Phase 2: 零拷贝数据路由与 View 隔离（核心修复）

- [x] Task 2.1: 修复 `View::set_data_document()` 导致数据显示为空的 Bug
  - 修改 `set_data_document()`：先检查 SessionDocument 是否有数据（`has_data()`），无数据时不做 `set_data()` 切换，保留当前数据指向
  - 确保初始化时（空文档）不覆盖 Signal 的 `_data` 指针
  - 确保有数据的文档切换时正确更新 `_data`

- [x] Task 2.2: 实现 `SigSession` → `SessionDocument` 数据写入
  - 在 `SigSession` 中新增 `set_active_document(SessionDocument *doc)` 和 `get_active_document()` 方法
  - 在 `SigSession` 中新增 `copy_data_to_document(SessionDocument *doc)` 方法，复制 `_view_data` 逻辑/模拟/DSO 数据到目标文档
  - 修改 `SigSession::init_signals()`：新创建的 Signal 同时指向 `_view_data`（实时）和活跃文档

- [x] Task 2.3: 修改 `MainWindow::on_frame_ended()` 触发数据路由
  - 采集结束后调用 `_session->copy_data_to_document(active_ctx->document())`
  - 然后调用 `active_ctx->activate()` 刷新视图（此时 SessionDocument 已有数据）
  - 移除旧的 `current_view()->receive_end()` 直接调用

- [x] Task 2.4: 修改 `on_frame_began()` 适配新数据流
  - 帧开始时确保活跃标签的 SessionDocument 清空旧数据
  - 调用 `_session->set_active_document(current_document)`
  - 让 SigSession 知道当前采集数据应写入哪个文档

- [x] Task 2.5: View 自有的 Signal 初步实现（`_own_signals`）
  - 在 `View` 中添加 `std::vector<Signal*> _own_signals` 成员（已声明）
  - `clone_signals_for_document()` 实现：由于 Snapshot 不支持拷贝构造，**暂缓实现**
  - DsoSignal 暂时保持共享（因硬件交互特殊）

- [ ] Task 2.6: 让 View 的方法优先使用 `_own_signals`
  - 依赖 Task 2.5 完成，**暂缓**

- [x] Task 2.7: 编译验证 Phase 2

## Phase 3: 生命周期管理与 Safe Detach/Attach

- [x] Task 3.1: 创建 `SessionManager` 单例
  - 新建 `pv/sessionmanager.h` 和 `pv/sessionmanager.cpp`
  - `SessionManager::instance()` 返回全局单例
  - `create_context(View*, SigSession*, SessionDocument*)` → TabContext*
  - `destroy_context(TabContext*)` 安全销毁
  - `attach_context(TabContext*)` / `detach_context(TabContext*)` 管理浮动窗口状态
  - `get_active_context()` / `set_active_context(TabContext*)`

- [x] Task 3.2: 创建 `IContextAware` 接口
  - 新建 `pv/interface/icontextaware.h`
  - `virtual void bind_context(TabContext* ctx) = 0`
  - `virtual void unbind_context() = 0`

- [x] Task 3.3: 让 MainWindow 通过 SessionManager 管理标签
  - 修改 `setup_ui()`：通过 SessionManager 创建初始标签
  - 修改 `on_new_tab_requested()`：通过 SessionManager 创建新标签
  - 修改 `on_load_file()`：通过 SessionManager 创建加载标签
  - 修改 `remove_tab()`：通过 SessionManager 销毁标签
  - 修改 `on_tab_detach()`：调用 `SessionManager::detach_context()`

- [x] Task 3.4: 让 MeasureDock 实现 IContextAware
  - `bind_context(TabContext*)`：切换 View 指针，重新绑定测量信号
  - `unbind_context()`：断开当前 View 的信号连接，清空测量数据

- [x] Task 3.5: 让 ProtocolDock 实现 IContextAware
  - `bind_context(TabContext*)`：切换 View 指针，加载目标 SessionDocument 的解码器配置
  - `unbind_context()`：断开当前 View 的解码器信号，保留配置

- [x] Task 3.6: 让 SearchDock 实现 IContextAware
  - `bind_context(TabContext*)`：切换 View 指针，清空搜索结果
  - `unbind_context()`：断开当前 View 的搜索状态连接

- [x] Task 3.7: 让 SamplingBar 实现 IContextAware
  - `bind_context(TabContext*)`：根据标签状态设置只读/可编辑
  - 历史标签（HISTORICAL）：参数只读显示
  - 活跃标签（LIVE）：参数可编辑
  - `unbind_context()`：重置到默认状态

- [x] Task 3.8: 修改 `MainWindow::on_tab_changed()` 触发全局 bind/unbind
  - 旧标签调用 `unbind_context()`，新标签调用 `bind_context(new_ctx)`

- [x] Task 3.9: 修复浮动窗口关闭时的 Context 回收
  - 新增 `on_tab_attached()` slot，连接 `DraggableTabWidget::tabAttached` 信号
  - 从 `detached_ctx` 属性恢复 TabContext，调用 `SessionManager::attach_context()`

- [x] Task 3.10: 编译验证 Phase 3

## Phase 4: 硬件路由与解码器隔离

- [x] Task 4.1: 修改 `SigSession::data_feed_in()` 直接路由数据到 SessionDocument
  - 方案调整为：采集→`_capture_data`→`_view_data`管道不变
  - 在采集完成后通过 `copy_data_to_document()` 使用零拷贝快照引用设置数据

- [x] Task 4.2: SessionDocument 快照引用机制
  - 新增 `_ref_logic`/`_ref_analog`/`_ref_dso` 引用指针
  - 新增 `get_active_logic/analog/dso()` 方法（优先返回引用，其次返回自有快照）
  - `View::set_data_document()` 使用 `get_active_*()` 获取数据

- [x] Task 4.3: SessionDocument 新增解码器栈支持（骨架）
  - `SessionDocument` 新增 `_decoder_stacks` 成员（`std::vector<data::DecoderStack*>`）
  - 新增 `add_decoder_stack()`, `remove_decoder_stack()`, `get_decoder_stacks()` 方法

- [ ] Task 4.4: 修改解码流程从 SigSession 解耦
  - 依赖 Task 4.3 + Task 2.5 完成，**暂缓**

- [x] Task 4.5: 清理废弃代码
  - 移除 `SigSession::capture_snapshot()` 声明和实现（~40行深拷贝逻辑）

- [x] Task 4.6: 编译验证 Phase 4

## 任务依赖

- Task 2.1 → Task 2.2 → Task 2.3 → Task 2.4：数据流修复链 ✅
- Task 2.5 → Task 2.6：Signal 隔离链（暂缓）
- Phase 3 依赖 Phase 2 全部完成 ✅
- Task 3.3 依赖 Task 3.1（SessionManager） ✅
- Task 3.4-3.7 依赖 Task 3.2（IContextAware 接口） ✅
- Task 3.8 依赖 Task 3.4-3.7 ✅
- Phase 4 快照引用机制依赖 Phase 3 ✅
- Task 4.4 依赖 Task 4.3（暂缓）
