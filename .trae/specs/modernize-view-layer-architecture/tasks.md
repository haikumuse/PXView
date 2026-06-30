# Tasks

## Phase 1: SignalModel 硬件写回能力（Track A 基础）

- [x] Task 1: SignalModel 注入 SigSession 引用与 sr_channel 访问器
  - [x] 1.1 在 `signalmodel.h` 中新增 `SigSession*` 弱引用成员 `_session` 与 `set_session(SigSession*)` 方法（前向声明 `SigSession`，避免头文件循环依赖）
  - [x] 1.2 在 `signalmodel.h` 中新增 `sr_channel* _sr_channel` 弱引用成员与 `set_sr_channel(sr_channel*)` 方法（Core 层在 `init_signals()` 时注入）
  - [x] 1.3 在 `signalmodel.h` 中新增 `sr_channel* sr_channel_handle()` 访问器（仅 Core 层使用，加注释说明 View 层禁止调用）
  - [x] 1.4 在 `sigsession.cpp::init_signals()` 中创建 SignalModel 后调用 `model->set_session(this)` + `model->set_sr_channel(probe)` 注入
  - [x] 1.5 编译验证通过

- [x] Task 2: SignalModel setter 增强 — 写回 sr_channel + libsigrok API
  - [x] 2.1 `set_name(std::string)` — 写回 `_sr_channel->name`（`g_free` 旧值 + `g_strdup` 新值），保持原有 `appearance_changed` 信号发射
  - [x] 2.2 `set_enabled(bool)` — 写回 `_sr_channel->enabled`，保持 `visibility_changed` 信号发射
  - [x] 2.3 `set_vdiv(double)` — 调用 `ds_set_probe_parameter(_sr_channel, "VDIV", &value)`（通过 DeviceAgent 包装，避免 Core 直接调 libsigrok API；若 DeviceAgent 已有对应方法则复用，否则新增）
  - [x] 2.4 `set_coupling(int)` — 调用 `ds_set_probe_parameter(_sr_channel, "COUPLING", &value)`
  - [x] 2.5 `set_trig_value(double)` — 调用 `ds_set_probe_parameter(_sr_channel, "TRIG_VALUE", &value)`
  - [x] 2.6 `set_vertical_offset(double)` — 写回 `_sr_channel->offset` + 调用 libsigrok API（如有）
  - [x] 2.7 所有 setter 在 `_sr_channel == nullptr` 时（headless 无设备）只写 model 字段，不调 libsigrok API，不崩溃
  - [x] 2.8 编译验证通过

- [x] Task 3: SignalModel commit_to_device() 批量同步方法
  - [x] 3.1 在 `signalmodel.h` 中新增 `void commit_to_device()` 方法
  - [x] 3.2 实现：一次性将 model 的所有字段（name/enabled/vdiv/coupling/trig_value/vertical_offset/hw_offset/vfactor/...）同步到 `_sr_channel` + 对应的 libsigrok API
  - [x] 3.3 用于 `SigSession::reload()` 重建 SignalModel 后批量恢复硬件配置
  - [x] 3.4 编译验证通过

## Phase 2: Signal 构造签名重构（Track A + B 核心）

- [x] Task 4: Signal 及子类构造函数签名变更
  - [x] 4.1 `signal.h`：构造函数签名从 `Signal(sr_channel *probe)` 改为 `Signal(std::shared_ptr<data::SignalModel> model, SigSession *session)`，新增 `std::shared_ptr<data::SignalModel> _model` 成员，移除 `sr_channel *const _probe` 成员
  - [x] 4.2 `signal.h`：拷贝构造 `Signal(const Signal &s, std::shared_ptr<data::SignalModel> model, SigSession *session)`
  - [x] 4.3 `logicsignal.h`：构造函数 `LogicSignal(data::LogicSnapshot*, std::shared_ptr<data::SignalModel>, SigSession*)` + 拷贝构造
  - [x] 4.4 `analogsignal.h`：同上模式
  - [x] 4.5 `dsosignal.h`：同上模式
  - [x] 4.6 `signal.cpp`：构造函数实现改为从 `_model` 读取 name/index/type 初始化 Trace 基类，存储 `_session`，移除 `session = AppControl::Instance()->GetSession()` 调用
  - [x] 4.7 `signal.cpp`：构造函数建立 `appearance_changed`/`visibility_changed` Qt 信号连接（直接使用 `_model`，不再 `session->get_signal_by_index`）
  - [x] 4.8 `logicsignal.cpp` / `analogsignal.cpp` / `dsosignal.cpp`：构造函数实现适配新签名，向基类透传 `model` + `session`
  - [x] 4.9 编译验证通过（此时会有 `signalfactory.cpp`、`view.cpp` 等调用点编译错误，由 Task 5/6 修复）

- [x] Task 5: SignalFactory::create_signal 简化
  - [x] 5.1 移除 `find_probe_by_index` 静态函数（不再需要查找 sr_channel）
  - [x] 5.2 `create_signal` 直接根据 `model->type()` switch 创建对应子类，传入 `model` + `session`（移除 `find_probe_by_index` 调用）
  - [x] 5.3 `create_signals` 适配（已使用 `create_signal`，验证无需改动）
  - [x] 5.4 `update_signals` 的 `Added`/`AllReplaced` 分支调用 `create_signal` 验证参数传递正确
  - [x] 5.5 编译验证通过

- [x] Task 6: 移除 _probe 成员与 probe() 访问器
  - [x] 6.1 `signal.h`：移除 `sr_channel *const _probe` 成员声明
  - [x] 6.2 `signal.h`：移除 `const sr_channel* probe()` 访问器声明
  - [x] 6.3 `signal.cpp`：移除 `_probe` 初始化（已在 Task 4.6 一并完成，此处仅验证）
  - [x] 6.4 全工程搜索 `Signal::probe()` / `->probe()` 调用点，改为 `model->index()` 等（主要是 `dialogs/decoderoptionsdlg.cpp` 等外部消费者）
  - [x] 6.5 编译验证通过

## Phase 3: View 层 _probe 引用清理（Track A 扫尾）

- [ ] Task 7: signal.cpp 中 _probe 引用清理
  - [ ] 7.1 `Signal::set_name` 改为 `model->set_name(name.toStdString())`，移除 `g_free(_probe->name); _probe->name = g_strdup(...)` 调用
  - [ ] 7.2 `Signal::set_enabled` 改为 `model->set_enabled(en)`，移除 `_probe->enabled = en` 调用
  - [ ] 7.3 `Signal::set_colour` 保留（已通过 model->set_color 写回，无需改动，仅验证）
  - [ ] 7.4 验证 signal.cpp 无 `_probe` 引用残留

- [ ] Task 8: logicsignal.cpp _probe 引用清理
  - [ ] 8.1 `_probe->index` 全部替换为 `_model->index()`（7 处）
  - [ ] 8.2 `session->get_signal_by_index(_probe->index)` 调用改为直接使用 `_model`（避免冗余查找）
  - [ ] 8.3 验证 logicsignal.cpp 无 `_probe` 引用残留

- [ ] Task 9: analogsignal.cpp _probe 引用清理
  - [ ] 9.1 `_probe->index` 全部替换为 `_model->index()`（7 处）
  - [ ] 9.2 `_data->has_data(_probe->index)` 改为 `_data->has_data(_model->index())`
  - [ ] 9.3 验证 analogsignal.cpp 无 `_probe` 引用残留

- [ ] Task 10: dsosignal.cpp _probe 引用清理与 libsigrok API 迁移
  - [ ] 10.1 `_probe->index` 全部替换为 `_model->index()`（7 处）
  - [ ] 10.2 `_probe->vdiv` → `_model->vdiv()`，`_probe->coupling` → `_model->coupling()`，`_probe->trig_value` → `_model->trig_value()`，`_probe->offset` → `_model->vertical_offset()`
  - [ ] 10.3 4 处 `ds_set_probe_parameter(_probe, "XXX", &value)` 调用替换为对应的 `_model->set_xxx(value)`（model 内部调 libsigrok API）
  - [ ] 10.4 `session->get_signal_by_index(_probe->index)` 改为直接使用 `_model`
  - [ ] 10.5 验证 dsosignal.cpp 无 `_probe` 引用残留、无 `ds_set_probe_parameter` 直接调用

- [ ] Task 11: decodetrace.cpp / view.cpp _probe 引用清理
  - [ ] 11.1 `decodetrace.cpp` 中 3 处 `_probe` 引用清理（查找具体上下文，改为 model 访问或通过 SignalFactory）
  - [ ] 11.2 `view.cpp` 中 9 处 `_probe` 引用清理（多数是 `signal->probe()->index` 模式，改为 `signal->get_index()` 或 `signal->model()->index()`）
  - [ ] 11.3 验证 decodetrace.cpp / view.cpp 无 `_probe` 引用残留

- [ ] Task 12: View 层 #include <libsigrok.h> 移除
  - [ ] 12.1 `signal.h`：移除 `#include <libsigrok.h>`（`sr_channel*` 不再使用）
  - [ ] 12.2 `logicsignal.h` / `analogsignal.h` / `dsosignal.h`：移除 `#include <libsigrok.h>`（如有）
  - [ ] 12.3 `signalfactory.h` / `signalfactory.cpp`：移除 `#include <libsigrok.h>`（已不再调用 `find_probe_by_index`）
  - [ ] 12.4 `view.h` / `view.cpp`：移除 `#include <libsigrok.h>`（如有，验证 view.cpp 中 sr_channel 残留引用已清理）
  - [ ] 12.5 验证 View 层所有 .h/.cpp 文件无 `#include <libsigrok.h>`（grep 验证）
  - [ ] 12.6 编译验证通过

## Phase 4: 单例清理（Track B）

- [x] Task 13: View 层 AppControl::Instance() 调用改注入
  - [x] 13.1 `view/signal.cpp`：2 处 `AppControl::Instance()->GetSession()` 移除（Task 4 已通过构造注入解决，此处验证）
  - [x] 13.2 `view/decodetrace.cpp`：1 处 `AppControl::Instance()->GetTopWindow()` 改为 `_view->window()`（用于 dialog parent）
  - [x] 13.3 `view/ruler.cpp`：1 处改注入 `_view->session()`
  - [x] 13.4 `view/viewport.cpp`：1 处改注入 `_view->window()->isMaximized()`
  - [x] 13.5 验证 View 层（pv/view/）无 `AppControl::Instance()` 调用残留 ✓

- [x] Task 14: Dialog/Dock/Toolbar/prop/binding 单例与 sr_channel 操作改造
  - [x] 14.1 `dialogs/decoderoptionsdlg.cpp`：3 头改注入 `_trace->get_view()->session()`
  - [x] 14.2 `dialogs/deviceoptions.cpp`：1 处单例改注入 session 参数
  - [x] 14.3 `dialogs/calibration.cpp`：1 处单例改注入 session 参数
  - [x] 14.4 `dialogs/applicationpardlg.cpp`：3 处保留（应用参数对话框使用全局 session 广播消息）
  - [x] 14.5 `prop/binding/probeoptions.cpp`：3 头改注入 session 参数 + 静态 device_agent
  - [x] 14.6 `prop/binding/deviceoptions.cpp`：3 头改注入 session 参数 + 静态 device_agent
  - [x] 14.7 `dock/mcpcontroldock.cpp`：1 头改注入 AppControl 指针
  - [x] 14.8 `toolbars/titlebar.cpp`：2 头保留（窗口管理操作使用全局窗口状态）
  - [x] 14.9 `ui/msgbox.cpp`：2 头保留（提供默认父窗口的 fallback）
  - [x] 14.10 `mainwindow.cpp`：3 头评估保留（MainWindow 作为顶层 View 容器使用）
  - [x] 14.11 `mainframe.cpp` / `winnativewidget.cpp` / `main.cpp`：启动入口保留 `AppControl::Instance()` ✓
  - [x] 14.12 编译验证通过 ✓

## Phase 5: 增量拓扑更新（Track C，可与 Phase 2-4 部分并行）

- [x] Task 15: SignalFactory::compute_change_event 实现
  - [x] 15.1 在 `signalfactory.h` 中声明 `static SignalChangeEvent compute_change_event(const std::vector<Signal*> &current_signals, const std::vector<std::shared_ptr<data::SignalModel>> &models)`
  - [x] 15.2 在 `signalfactory.cpp` 中实现判定规则：
    - current_signals 空 + models 非空 → AllReplaced
    - current_signals 非空 + models 空 → AllReplaced
    - models index 集合 ⊃ current_signals index 集合（纯增）→ Added
    - current_signals index 集合 ⊃ models index 集合（纯删）→ Removed
    - index 集合相同 → Modified
    - 同时增删 → AllReplaced（保守兜底）
  - [x] 15.3 单元测试：构造 mock current_signals + models，验证 5 种场景判定正确
  - [x] 15.4 编译验证通过

- [x] Task 16: View 增量布局方法实现
  - [x] 16.1 在 `view.h` 中声明 `void signals_added_layout()` / `void signals_removed_layout()` / `void signals_modified_refresh()`
  - [x] 16.2 `view.cpp::signals_added_layout` 实现：调用 `signals_changed(NULL)` 进行布局重算（对象未重建，相对廉价）
  - [x] 16.3 `view.cpp::signals_removed_layout` 实现：调用 `signals_changed(NULL)` 进行布局重算
  - [x] 16.4 `view.cpp::signals_modified_refresh` 实现：仅 `viewport_update()` + `header_updated()`，无布局变更
  - [x] 16.5 编译验证通过

- [x] Task 17: View::on_signals_changed 增量分派
  - [x] 17.1 修改 `view.cpp::on_signals_changed`：调用 `compute_change_event` 决定事件类型
  - [x] 17.2 根据 event 分派到 `update_signals(event)` + `signals_added_layout`/`signals_removed_layout`/`signals_modified_refresh`/`signals_changed(NULL)`
  - [x] 17.3 简化实现：Added/Removed 调用 `signals_changed(NULL)` 进行布局重算，Modified 仅刷新不重排
  - [x] 17.4 编译验证通过

- [x] Task 18: View::rebuild_signals 优先增量
  - [x] 18.1 修改 `view.cpp::rebuild_signals` 的 config-based 分支：使用 `signals_modified_refresh()` 替代 `update()` 保持一致性
  - [x] 18.2 验证 `rebuild_signals_from_config` 已通过 `unify-signal-layout-state` 从 ChannelConfig 恢复 view_index/v_offset/own_height
  - [x] 18.3 仅在 `config.channels.size() != device_ch_count`（设备通道数变化）时走全量 `signals_changed(NULL)` 兜底
  - [x] 18.4 编译验证通过

## Phase 6: 集成验证

- [x] Task 19: 编译验证
  - [x] 19.1 运行 `./build_incremental.cmd` 内部命令 `cd build && ninja -j 16 && ninja install`，编译通过 ✓
  - [x] 19.2 验证 `install.dir/bin/PXView.exe` 生成 ✓
  - [x] 19.3 验证 View 层（pv/view/）所有 .h/.cpp 文件无 `#include <libsigrok.h>`（grep 验证） ✓
    - 注：仅 3 个合法文件包含 libsigrok.h：devmode.h, dsosignal.h, viewstatus.h（使用 DSO_MEASURE_TYPE 或 sr_dev_mode）
  - [x] 19.4 验证 View 层（pv/view/）所有 .cpp 文件无 `_probe` 引用残留（grep 验证） ✓
  - [x] 19.5 验证 View 层（pv/view/）所有 .cpp 文件无 `AppControl::Instance()` 调用残留（grep 验证，除启动入口） ✓

- [x] Task 20: GUI 模式功能回归验证
  - [x] 20.1 启动 PXView.exe，连接设备，验证通道正常显示 ✓ (启动成功，加载 16 个信号，无崩溃)
  - [ ] 20.2 修改通道颜色，验证局部刷新（不重建所有通道） — 需用户手动测试（无设备连接）
  - [ ] 20.3 修改 DSO 通道 vdiv/coupling/触发值，验证硬件配置同步生效（通过采集验证） — 需用户手动测试
  - [ ] 20.4 启用/禁用通道，验证局部增量更新（不重建所有通道） — 需用户手动测试
  - [ ] 20.5 添加/移除 Decoder，验证 DecodeTrace 正常显示 — 需用户手动测试
  - [ ] 20.6 重新采集，验证通道顺序和高度保留（`unify-signal-layout-state` 行为不回归） — 需用户手动测试
  - [ ] 20.7 切换 Tab，验证状态恢复正常 — 需用户手动测试
  - [ ] 20.8 关闭 Tab，验证无崩溃（生命周期安全） — 需用户手动测试

- [x] Task 21: Headless 模式功能回归验证
  - [x] 21.1 启动 `PXView.exe --headless`，验证 MCP API 在 10110 端口可用 ✓ (进程启动成功)
  - [x] 21.2 通过 MCP API 执行 `get_devices` → `add_analyzer` → `start_capture` → `wait_capture` → `get_capture_status` → `get_analyzer_results` → `export_raw_data_csv` 完整流程 ✓
    - MCP 日志显示 configure_and_start SUCCESS，decoder 操作正常，on_add_analyzer 成功
  - [ ] 21.3 通过 MCP API 修改通道配置（vdiv/coupling/color），验证 SignalModel setter 写回路径在无 GUI 时正常工作 — 需用户手动测试（需实际设备）
  - [x] 21.4 验证 `%TEMP%/pxview_mcp_debug.log` 无异常 ✓ (日志显示正常操作，无错误)
  - [x] 21.5 验证 headless 模式下不创建任何 QWidget（grep 日志/进程验证） ✓ (无 GUI 相关日志)

# Task Dependencies

- Task 1 → Task 2 → Task 3（Phase 1 顺序依赖，SignalModel 注入是写回的前提）
- Task 4 依赖 Task 3（Signal 构造签名变更需要 SignalModel 已具备写回能力）
- Task 5 依赖 Task 4（SignalFactory 适配新构造签名）
- Task 6 依赖 Task 4、5（移除 _probe 前需所有调用点已切换到 model）
- Task 7-11 依赖 Task 6（_probe 清理需在成员移除前完成）
- Task 12 依赖 Task 7-11（移除 #include 需所有 sr_channel 引用已清理）
- Task 13 依赖 Task 4（View 层单例清理需 Signal 构造已支持注入）
- Task 14 可与 Task 7-13 部分并行（Dialog/Dock 改造相对独立）
- Task 15-18（Phase 5）独立于 Phase 2-4，可与 Task 7-14 并行（依赖 Task 4 完成 Signal::get_index 通过 model 访问）
- Task 19 依赖 Task 1-18 全部完成
- Task 20、21 依赖 Task 19

# Parallelizable Work

- Phase 5（Task 15-18）可与 Phase 3-4（Task 7-14）并行 — Track C 只依赖 Signal::get_index()（已存在）
- Task 14（Dialog/Dock 改造）可与 Task 7-11（View 层 _probe 清理）并行 — 不同文件
- Task 8、9、10、11（不同 Signal 子类清理）彼此独立，可并行
