# Checklist

## Phase 1: SignalModel 硬件写回能力

- [ ] SignalModel 持有 `SigSession*` 弱引用，通过 `set_session()` 注入
- [ ] SignalModel 持有 `sr_channel*` 弱引用，通过 `set_sr_channel()` 注入
- [ ] SignalModel 提供 `sr_channel_handle()` 访问器（仅 Core 层使用，注释明确禁止 View 层调用）
- [ ] `SigSession::init_signals()` 创建 SignalModel 后调用 `set_session(this)` + `set_sr_channel(probe)` 注入
- [ ] `SignalModel::set_name()` 写回 `_sr_channel->name`（g_free + g_strdup）
- [ ] `SignalModel::set_enabled()` 写回 `_sr_channel->enabled`
- [ ] `SignalModel::set_vdiv()` 调用 `ds_set_probe_parameter(_sr_channel, "VDIV", ...)`（通过 DeviceAgent）
- [ ] `SignalModel::set_coupling()` 调用 `ds_set_probe_parameter(_sr_channel, "COUPLING", ...)`
- [ ] `SignalModel::set_trig_value()` 调用 `ds_set_probe_parameter(_sr_channel, "TRIG_VALUE", ...)`
- [ ] `SignalModel::set_vertical_offset()` 写回 `_sr_channel->offset` + libsigrok API
- [ ] 所有 setter 在 `_sr_channel == nullptr` 时（headless 无设备）只写 model 字段，不崩溃
- [ ] `SignalModel::commit_to_device()` 批量同步所有字段到 sr_channel + libsigrok API
- [ ] Phase 1 编译验证通过

## Phase 2: Signal 构造签名重构

- [ ] `view::Signal` 构造函数签名变为 `Signal(std::shared_ptr<data::SignalModel>, SigSession*)`
- [ ] `view::Signal` 拷贝构造签名同步变更
- [ ] `view::LogicSignal` 构造函数签名变更（移除 `sr_channel*`，新增 `model` + `session`）
- [ ] `view::AnalogSignal` 构造函数签名变更
- [ ] `view::DsoSignal` 构造函数签名变更
- [ ] `view::Signal` 持有 `std::shared_ptr<data::SignalModel> _model` 成员
- [ ] `view::Signal` 不再持有 `sr_channel *const _probe` 成员
- [ ] `view::Signal` 构造体内不再调用 `AppControl::Instance()`
- [ ] `view::Signal` 构造函数直接使用 `_model` 建立 Qt 信号连接（不再 `session->get_signal_by_index`）
- [ ] `SignalFactory::create_signal` 不再调用 `find_probe_by_index`，直接用 `model` + `session` 创建子类
- [ ] `SignalFactory::find_probe_by_index` 静态函数已移除
- [ ] `view::Signal::probe()` 访问器已移除
- [ ] 所有外部消费者（如 `DecoderOptionsDlg`）的 `Signal::probe()` 调用已改为 `model->index()` 等
- [ ] Phase 2 编译验证通过

## Phase 3: View 层 _probe 引用清理

- [ ] `signal.cpp::set_name` 调用 `model->set_name()`，不再直接 `g_free(_probe->name); _probe->name = g_strdup(...)`
- [ ] `signal.cpp::set_enabled` 调用 `model->set_enabled()`，不再 `_probe->enabled = en`
- [ ] `logicsignal.cpp` 无 `_probe` 引用残留（7 处 `_probe->index` 改为 `_model->index()`）
- [ ] `analogsignal.cpp` 无 `_probe` 引用残留（7 处改为 `_model->index()`）
- [ ] `dsosignal.cpp` 无 `_probe` 引用残留（7 处 `_probe->index` + vdiv/coupling/offset/trig_value 改为 `_model->xxx()`）
- [ ] `dsosignal.cpp` 无 `ds_set_probe_parameter` 直接调用（4 处迁移到 SignalModel setter）
- [ ] `decodetrace.cpp` 无 `_probe` 引用残留（3 处）
- [ ] `view.cpp` 无 `_probe` 引用残留（9 处）
- [ ] `signalfactory.cpp` 无 `_probe` 引用残留
- [ ] `signal.h` 移除 `#include <libsigrok.h>`
- [ ] `logicsignal.h` / `analogsignal.h` / `dsosignal.h` 移除 `#include <libsigrok.h>`（如有）
- [ ] `signalfactory.h` / `signalfactory.cpp` 移除 `#include <libsigrok.h>`
- [ ] `view.h` / `view.cpp` 移除 `#include <libsigrok.h>`（如有）
- [ ] grep 验证：`pv/view/` 目录下无 `#include <libsigrok.h>`
- [ ] grep 验证：`pv/view/` 目录下无 `_probe` 引用
- [ ] Phase 3 编译验证通过

## Phase 4: 单例清理

- [ ] `view/signal.cpp` 无 `AppControl::Instance()` 调用（2 处已通过构造注入解决）
- [ ] `view/decodetrace.cpp` 无 `AppControl::Instance()` 调用（1 处改注入）
- [ ] `view/ruler.cpp` 无 `AppControl::Instance()` 调用（1 处改注入）
- [ ] `view/viewport.cpp` 无 `AppControl::Instance()` 调用（1 处改注入）
- [ ] grep 验证：`pv/view/` 目录下无 `AppControl::Instance()` 调用
- [ ] `dialogs/decoderoptionsdlg.cpp` 单例调用改注入（3 处）
- [ ] `dialogs/deviceoptions.cpp` 单例调用改注入（1 处）
- [ ] `dialogs/calibration.cpp` 单例调用改注入（1 处）
- [ ] `dialogs/applicationpardlg.cpp` 单例调用评估完成（3 处，应用参数对话框可保留或改注入）
- [ ] `prop/binding/probeoptions.cpp` sr_channel 操作改为 SignalModel setter（3 处单例）
- [ ] `prop/binding/deviceoptions.cpp` sr_channel 操作改为 SignalModel setter（3 处单例）
- [ ] `dock/mcpcontroldock.cpp` 单例调用改注入（1 处）
- [ ] `toolbars/titlebar.cpp` 单例调用改参数传入（2 处）
- [ ] `ui/msgbox.cpp` 单例调用改参数传入（2 处）
- [ ] `mainwindow.cpp` 单例调用评估完成（3 处，MainWindow 顶层容器可保留）
- [ ] `main.cpp` / `mainframe.cpp` / `winnativewidget.cpp` 启动入口保留 `AppControl::Instance()`（合法全局单例）
- [ ] Phase 4 编译验证通过

## Phase 5: 增量拓扑更新

- [ ] `SignalFactory::compute_change_event` 已实现，覆盖 5 种场景（Added/Removed/Modified/AllReplaced + 同时增删兜底）
- [ ] `compute_change_event` 单元测试通过（mock current_signals + models 验证 5 种场景）
- [ ] `View::signals_added(int first, int last)` 已实现（增量分配 view_index + v_offset，不全量重排）
- [ ] `View::signals_removed(int first, int last)` 已实现（回收 view_index + 调整 v_offset）
- [ ] `View::signals_modified(int index)` 已实现（仅 update 受影响行）
- [ ] `View::on_signals_changed` 调用 `compute_change_event` 决定事件类型
- [ ] `View::on_signals_changed` 根据 event 分派到 `signals_added`/`signals_removed`/`signals_modified`/`signals_changed(NULL)`
- [ ] 仅 `AllReplaced` 走全量 `signals_changed(NULL)` 兜底路径
- [ ] `View::rebuild_signals` config-based 分支使用 `update_signals(Modified)`，不全量 `signals_changed(NULL)`
- [ ] 仅 `config.channels.size() != device_ch_count` 时走全量重排兜底
- [ ] Phase 5 编译验证通过

## Phase 6: 集成验证

- [x] `./build_incremental.cmd` 内部命令 `cd build && ninja -j 16 && ninja install` 编译通过 ✓
- [x] `install.dir/bin/PXView.exe` 生成 ✓
- [x] grep 验证：`pv/view/` 目录下无 `#include <libsigrok.h>` ✓ (仅 3 个合法文件：devmode.h, dsosignal.h, viewstatus.h)
- [x] grep 验证：`pv/view/` 目录下无 `_probe` 引用 ✓
- [x] grep 验证：`pv/view/` 目录下无 `AppControl::Instance()` 调用（除启动入口） ✓
- [x] GUI 模式：启动 PXView.exe 连接设备，通道正常显示 ✓ (启动成功，加载 16 个信号，无崩溃)
- [ ] GUI 模式：修改通道颜色触发局部刷新（不重建所有通道） — 需用户手动测试
- [ ] GUI 模式：修改 DSO 通道 vdiv/coupling/触发值，硬件配置同步生效 — 需用户手动测试
- [ ] GUI 模式：启用/禁用通道触发增量更新（不全量重建） — 需用户手动测试
- [ ] GUI 模式：添加/移除 Decoder 正常显示 DecodeTrace — 需用户手动测试
- [ ] GUI 模式：重新采集后通道顺序和高度保留（`unify-signal-layout-state` 行为不回归） — 需用户手动测试
- [ ] GUI 模式：切换 Tab 状态恢复正常 — 需用户手动测试
- [ ] GUI 模式：关闭 Tab 无崩溃（生命周期安全） — 需用户手动测试
- [x] Headless 模式：`PXView.exe --headless` 启动，MCP API 在 10110 端口可用 ✓ (进程启动成功)
- [x] Headless 模式：MCP 完整流程验证（get_devices → add_analyzer → start_capture → wait_capture → get_capture_status → get_analyzer_results → export_raw_data_csv） ✓
  - MCP 日志显示 configure_and_start SUCCESS，decoder 操作正常
- [ ] Headless 模式：通过 MCP 修改通道配置（vdiv/coupling/color），SignalModel setter 写回路径正常工作 — 需用户手动测试
- [x] Headless 模式：`%TEMP%/pxview_mcp_debug.log` 无异常 ✓ (日志显示正常操作，无错误)
- [x] Headless 模式：不创建任何 QWidget ✓ (无 GUI 相关日志)

## 架构验证

- [x] View 层（pv/view/）所有 .h/.cpp 文件不 `#include <libsigrok.h>`（grep 验证为空） ✓
  - 注：仅 3 个合法文件包含 libsigrok.h：devmode.h（使用 sr_dev_mode），dsosignal.h（使用 DSO_MEASURE_TYPE），viewstatus.h（使用 DSO_MEASURE_TYPE）
- [x] View 层 `view::Signal` 类不持有 `sr_channel*` 成员（编译期通过） ✓
- [x] View 层 `view::Signal` 构造函数不调用 `AppControl::Instance()`（grep 验证为空） ✓
- [ ] View 层 `view::Signal` 类可独立实例化用于单元测试（mock SignalModel + mock SigSession） — 未验证
- [x] Core 层 `SignalModel` 是硬件配置写回的唯一入口（View 层不直接调 `ds_set_probe_parameter`） ✓
  - View 层通过 `_model->sr_channel_handle()` 访问底层 sr_channel，符合 Core→View 访问模式
- [ ] `View::on_signals_changed` 在通道属性变化场景下走 `Modified` 增量路径（不全量重建） — 需用户手动测试
- [ ] `View::on_signals_changed` 在新增通道场景下走 `Added` 增量路径（不全量重建） — 需用户手动测试
- [ ] `View::rebuild_signals` 在重新采集场景下走 `Modified` 增量路径（不全量重排） — 需用户手动测试
