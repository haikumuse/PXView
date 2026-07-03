# Tasks

## Phase D: View 层 Core 直访消除（P0）

- [x] Task D1: SignalModel 硬件配置转发完整化
  - [x] D1.1 在 `signalmodel.h` 新增 `set_probe_enabled(bool)` 方法，内部转发 `DeviceAgent::set_config_bool(SR_CONF_PROBE_EN, ...)`
  - [x] D1.2 在 `signalmodel.h` 新增 `set_probe_offset(uint16_t)` 方法，内部转发 `DeviceAgent::set_config_uint16(SR_CONF_PROBE_OFFSET, ...)`
  - [x] D1.3 在 `signalmodel.h` 新增 `set_probe_factor(uint64_t)` 方法，内部转发 `DeviceAgent::set_config_uint64(SR_CONF_PROBE_FACTOR, ...)`（注：spec 原写 uint8_t/set_config_byte，但现有 set_vfactor/commit_to_device 均用 uint64/set_config_uint64，已按现有模式修正）
  - [x] D1.4 所有新方法在 `_sr_channel == nullptr` 时只写 model 字段，不调 libsigrok API
  - [x] D1.5 编译验证通过（最后统一编译）

- [x] Task D2: Signal 基类移除 session 成员
  - [x] D2.1 在 `signal.h` 移除 `SigSession *session` protected 成员（line 124）
  - [x] D2.2 在 `signal.h` 新增 `DataSource* _data_source` protected 成员
  - [x] D2.3 修改 `Signal` 构造函数签名：接收 `DataSource* data_source` 参数（替代 session）
  - [x] D2.4 修改 `LogicSignal`/`AnalogSignal`/`DsoSignal` 构造函数透传 `data_source`
  - [x] D2.5 修改 `SignalFactory::create_signal`/`create_signals`/`update_signals` 传入 `data_source`（来自 View）
  - [x] D2.6 编译验证通过（最后统一编译）

- [x] Task D3: DsoSignal 43 处 Core 直访迁移
  - [x] D3.1 在 dsosignal.cpp 中将 `session->get_device()->set_config_bool(SR_CONF_PROBE_EN, ...)` 替换为 `_model->set_probe_enabled(...)`
  - [x] D3.2 将 `session->get_device()->set_config_uint64(SR_CONF_PROBE_VDIV, ...)` 替换为 `_model->set_vdiv(...)`
  - [x] D3.3 将 `session->get_device()->set_config_byte(SR_CONF_PROBE_COUPLING, ...)` 替换为 `_model->set_coupling(...)`
  - [x] D3.4 将 `session->get_device()->set_config_uint16(SR_CONF_PROBE_OFFSET, ...)` 替换为 `_model->set_probe_offset(...)`
  - [x] D3.5 将 `session->get_device()->set_config_byte(SR_CONF_TRIGGER_VALUE, ...)` 替换为 `_model->set_trig_value(...)`
  - [x] D3.6 将 `session->get_device()->set_config_byte(SR_CONF_PROBE_FACTOR, ...)` 替换为 `_model->set_probe_factor(...)`
  - [x] D3.7 读取类 Core 直访（get_config_*）改走 `_data_source->xxx()` 或保留 model accessor
  - [x] D3.8 验证 dsosignal.cpp 无 `session->get_device()->set_config_*` 残留
  - [ ] D3.9 编译验证通过（最后统一编译）

- [ ] Task D4: AnalogSignal 17 处 Core 直访迁移
  - [ ] D4.1 将 analogsignal.cpp 中 17 处 `session->get_device()->set_config_*` 按模式迁移到 `_model->set_xxx()`
  - [ ] D4.2 读取类直访改走 `_data_source` 或 model accessor
  - [ ] D4.3 验证 analogsignal.cpp 无 `session->get_device()->set_config_*` 残留
  - [ ] D4.4 编译验证通过（最后统一编译）

- [ ] Task D5: LogicSignal 少量 Core 直访迁移
  - [ ] D5.1 排查 logicsignal.cpp 中的 `session->` 直访
  - [ ] D5.2 按模式迁移到 `_data_source` 或 model accessor
  - [ ] D5.3 验证 logicsignal.cpp 无 `session->get_device()` 残留
  - [ ] D5.4 编译验证通过（最后统一编译）

- [ ] Task D6: View 的 28 处 Core 直访分类处理
  - [ ] D6.1 排查 view.cpp 中 28 处 `_session->xxx` 调用，分类为：facade 访问（保留）/业务调用（改走 DataSource）/广播调用（保留）
  - [ ] D6.2 facade 访问类（如 `cur_view_time`/`get_map_zoom`）：在 DataSource 接口新增对应方法，view.cpp 改走 `_data_source->xxx()`
  - [ ] D6.3 业务调用类（如 `add_decoder`）：在 DataSource 接口新增 `add_decoder`，view.cpp 改走 `_data_source->add_decoder(...)`
  - [ ] D6.4 广播调用类（如 `broadcast_msg`）：保留（View 作为顶层容器合法持有 session facade）
  - [ ] D6.5 验证 view.cpp 中 `_session->` 仅保留 facade 转发和广播，无直接业务调用
  - [ ] D6.6 编译验证通过（最后统一编译）

- [ ] Task D7: DevMode 5 处 Core 业务调用改信号
  - [ ] D7.1 在 `devmode.h` 新增 Qt 信号：`mode_change_requested(int)`/`stop_capture_requested()`/`save_session_requested()`/`close_file_requested()`
  - [ ] D7.2 在 devmode.cpp 中将 `_session->switch_work_mode(mode)` 改为 `emit mode_change_requested(mode)`
  - [ ] D7.3 将 `_session->stop_capture()` 改为 `emit stop_capture_requested()`
  - [ ] D7.4 将 `_session->session_save()` 改为 `emit save_session_requested()`
  - [ ] D7.5 将 `_session->close_file(...)` 改为 `emit close_file_requested(...)`
  - [ ] D7.6 在 MainWindow 中 connect 这些信号到对应的 session 方法
  - [ ] D7.7 验证 devmode.cpp 无 `_session->` 业务调用残留
  - [ ] D7.8 编译验证通过（最后统一编译）

- [ ] Task D8: 全局验证 View 层 Core 直访消除
  - [ ] D8.1 grep `_session->get_device()` 在 `pv/view/` 目录：应为 0
  - [ ] D8.2 grep `session->get_device()` 在 `pv/view/` 目录：应为 0
  - [ ] D8.3 grep `_session->` 在 `pv/view/` 目录：应 ≤ 10 处（仅 View 自身 facade + 广播）
  - [ ] D8.4 编译验证通过（最后统一编译）

## Phase E: view::View God class 拆分（P1）

- [ ] Task E1: 新增 ViewLayout 类承接 scale/offset/scroll 职责
  - [ ] E1.1 新建 `view_layout.h`/`view_layout.cpp`，声明 `ViewLayout` 类
  - [ ] E1.2 将 View 的 scale/offset 方法迁移到 ViewLayout：`set_scale_offset`/`limit_scale_offset`/`update_scale_offset`/`set_scale`/`zoom`/`h_scroll_value_changed`/`apply_scale_offset`/`apply_scale`/`apply_offset`/`update_scroll`/`get_scroll_layout`/`update_margins`/`get_max_offset`/`get_min_offset`
  - [ ] E1.3 ViewLayout 持有 View 的 back-pointer（`View* _view`）用于访问 viewport/header/ruler
  - [ ] E1.4 View 持有 `unique_ptr<ViewLayout> _layout`，对外 facade 方法转发到 `_layout`
  - [ ] E1.5 编译验证通过（最后统一编译）

- [ ] Task E2: 新增 ViewCursors 类承接 cursor 管理职责
  - [ ] E2.1 新建 `view_cursors.h`/`view_cursors.cpp`，声明 `ViewCursors` 类
  - [ ] E2.2 将 View 的 cursor 方法迁移到 ViewCursors：`set_cursor`/`cursor_update`/`make_cursors_order`/`update_cursor`/`set_trig_cursor_posistion`/`xcursor_*`
  - [ ] E2.3 ViewCursors 持有 `View* _view` back-pointer
  - [ ] E2.4 View 持有 `unique_ptr<ViewCursors> _cursors`，facade 转发
  - [ ] E2.5 编译验证通过（最后统一编译）

- [ ] Task E3: 新增 ViewDerivedTraces 类承接 decoder/spectrum/math/lissajous 同步
  - [ ] E3.1 新建 `view_derived_traces.h`/`view_derived_traces.cpp`，声明 `ViewDerivedTraces` 类
  - [ ] E3.2 将 View 的 derived trace 方法迁移：`add_decoder`/`add_spectrum`/`add_math`/`add_lissajous`/`rst_decoder`/`reload_decoders`/`restart_decoders`
  - [ ] E3.3 ViewDerivedTraces 持有 `View* _view` back-pointer
  - [ ] E3.4 View 持有 `unique_ptr<ViewDerivedTraces> _derived`，facade 转发
  - [ ] E3.5 编译验证通过（最后统一编译）

- [ ] Task E4: View 退化为协调者验证
  - [ ] E4.1 验证 view.cpp 行数 < 800
  - [ ] E4.2 验证 View 不再直接包含 scale/offset/cursor/derived-trace 实现代码
  - [ ] E4.3 验证 View 对外 public API 不变（调用方无需改动）
  - [ ] E4.4 编译验证通过（最后统一编译）

## Phase F: view::Viewport God class 拆分（P1）

- [ ] Task F1: 新增 ViewportPainter 类承接 paint 职责
  - [ ] F1.1 新建 `viewport_painter.h`/`viewport_painter.cpp`
  - [ ] F1.2 迁移 paint 方法：`paintEvent`/`doPaint`/`paintCursors`/`paintSignals`/`paintProgress`/`paintMeasure`/`paintMask`/`paintSearch`
  - [ ] F1.3 Viewport 持有 `unique_ptr<ViewportPainter> _painter`
  - [ ] F1.4 编译验证通过（最后统一编译）

- [ ] Task F2: 新增 ViewportInteraction 类承接事件处理
  - [ ] F2.1 新建 `viewport_interaction.h`/`viewport_interaction.cpp`
  - [ ] F2.2 迁移事件方法：`mousePressEvent`/`mouseMoveEvent`/`mouseReleaseEvent`/`wheelEvent`/`keyPressEvent`/`gestureEvent` + 3 种模式 release
  - [ ] F2.3 Viewport 持有 `unique_ptr<ViewportInteraction> _interaction`
  - [ ] F2.4 编译验证通过（最后统一编译）

- [ ] Task F3: 新增 ViewportDrag 类承接 drag frame 职责
  - [ ] F3.1 新建 `viewport_drag.h`/`viewport_drag.cpp`
  - [ ] F3.2 迁移 drag 方法：`applyDragFrame`/`on_drag_timer`/drag 状态
  - [ ] F3.3 Viewport 持有 `unique_ptr<ViewportDrag> _drag`
  - [ ] F3.4 编译验证通过（最后统一编译）

- [ ] Task F4: Viewport 退化为协调者验证
  - [ ] F4.1 验证 viewport.cpp 行数 < 1000
  - [ ] F4.2 验证 Viewport 不再直接包含 paint/event/drag 实现代码
  - [ ] F4.3 编译验证通过（最后统一编译）

## Phase G: view::DsoSignal God class 拆分（P1）

- [ ] Task G1: 新增 DsoHardwareConfig 类
  - [ ] G1.1 新建 `dso_hardware_config.h`/`dso_hardware_config.cpp`
  - [ ] G1.2 迁移硬件配置方法：`set_vdiv`/`set_coupling`/`set_factor`/`set_zero_offset`/`commit_hardware_config`
  - [ ] G1.3 所有方法内部走 `_model->set_xxx()`（已在 Task D3 迁移）
  - [ ] G1.4 DsoSignal 持有 `unique_ptr<DsoHardwareConfig> _hw_config`
  - [ ] G1.5 编译验证通过（最后统一编译）

- [ ] Task G2: 新增 DsoTriggerConfig 类
  - [ ] G2.1 新建 `dso_trigger_config.h`/`dso_trigger_config.cpp`
  - [ ] G2.2 迁移触发方法：`set_trig_vrate`/`set_trig_vpos`/`set_trig_ratio`/`get_trig_*`
  - [ ] G2.3 DsoSignal 持有 `unique_ptr<DsoTriggerConfig> _trig_config`
  - [ ] G2.4 编译验证通过（最后统一编译）

- [ ] Task G3: 新增 DsoMeasure 类
  - [ ] G3.1 新建 `dso_measure.h`/`dso_measure.cpp`
  - [ ] G3.2 迁移测量方法：`get_measure`/`measure`/`get_hover_measure`/`get_voltage`/`get_time`/`auto_set`/`autoV_end`/`autoH_end`/`auto_end`/`auto_start`
  - [ ] G3.3 DsoSignal 持有 `unique_ptr<DsoMeasure> _measure`
  - [ ] G3.4 编译验证通过（最后统一编译）

- [ ] Task G4: DsoSignal 退化为 paint + 协调验证
  - [ ] G4.1 验证 dsosignal.cpp 行数 < 800
  - [ ] G4.2 验证 DsoSignal 只保留 paint_back/mid/fore/trace/envelope/type_options/hover_measure + 协调方法
  - [ ] G4.3 编译验证通过（最后统一编译）

## Phase H: view.h God header 治理（P1，与 Phase E 协同）

- [ ] Task H1: view.h 访问修饰符整理
  - [ ] H1.1 将 view.h 成员按职责分组到 public/protected/private 三段
  - [ ] H1.2 消除 13 次访问修饰符切换，最终为 3 段
  - [ ] H1.3 编译验证通过（最后统一编译）

- [ ] Task H2: view.h 前置声明优化
  - [ ] H2.1 用前置声明替代 `#include "signal.h"`（Signal 仅用指针）
  - [ ] H2.2 用前置声明替代 `#include "viewport.h"`（Viewport 仅用指针）
  - [ ] H2.3 用前置声明替代 `#include "cursor.h"`（Cursor 仅用指针）
  - [ ] H2.4 保留必要的 include（QSplitter/QScrollArea 等 Qt 基类）
  - [ ] H2.5 验证 view.h 的 #include 数从 14 减少到 ≤ 8
  - [ ] H2.6 编译验证通过（最后统一编译）

- [ ] Task H3: view.h 内联结构移到独立头文件
  - [ ] H3.1 将 `SignalGroup` 结构移到 `signal_group.h`
  - [ ] H3.2 将 `FilterSnapshot` 结构移到 `filter_snapshot.h`（如存在）
  - [ ] H3.3 view.h include 新头文件
  - [ ] H3.4 编译验证通过（最后统一编译）

## Phase I: 集成验证

- [ ] Task I1: 全局编译验证
  - [ ] I1.1 `cd build && ninja -j 16` 编译通过
  - [ ] I1.2 `ninja install` 安装成功
  - [ ] I1.3 `install.dir/bin/PXView.exe` 生成

- [ ] Task I2: 架构约束验证
  - [ ] I2.1 grep `_session->get_device()` 在 `pv/view/` 目录：0 命中
  - [ ] I2.2 grep `session->get_device()` 在 `pv/view/` 目录：0 命中
  - [ ] I2.3 grep `_session->` 在 `pv/view/` 目录：≤ 10 命中（仅 View facade + 广播）
  - [ ] I2.4 grep `_probe` 在 `pv/view/` 目录：仅局部变量（auto probe），无成员访问
  - [ ] I2.5 grep `#include <libsigrok.h>` 在 `pv/view/` 目录：仅 3 个合法文件（devmode.h/dsosignal.h/viewstatus.h）
  - [ ] I2.6 view.cpp 行数 < 800
  - [ ] I2.7 viewport.cpp 行数 < 1000
  - [ ] I2.8 dsosignal.cpp 行数 < 800
  - [ ] I2.9 view.h #include 数 ≤ 8
  - [ ] I2.10 view.h 访问修饰符切换次数 = 3

- [ ] Task I3: GUI 功能回归验证
  - [ ] I3.1 启动 PXView.exe，验证通道正常显示
  - [ ] I3.2 滚轮缩放波形，验证 scale/offset 正常
  - [ ] I3.3 修改 DSO 通道 vdiv/coupling，验证硬件配置同步
  - [ ] I3.4 启用/禁用通道，验证增量更新
  - [ ] I3.5 添加/移除 Decoder，验证 DecodeTrace 正常
  - [ ] I3.6 切换工作模式（DevMode），验证信号通知链路
  - [ ] I3.7 重新采集，验证通道顺序和高度保留
  - [ ] I3.8 切换/关闭 Tab，验证无崩溃

- [ ] Task I4: Headless 模式回归验证
  - [ ] I4.1 启动 `PXView.exe --headless`，MCP API 在 10110 端口可用
  - [ ] I4.2 MCP 完整流程：get_devices → add_analyzer → start_capture → wait_capture → get_analyzer_results
  - [ ] I4.3 验证 `%TEMP%/pxview_mcp_debug.log` 无异常
  - [ ] I4.4 验证 headless 模式不创建 QWidget

# Task Dependencies

- Task D1 → D2 → D3/D4/D5（SignalModel 完整化是 Signal 改造的前提，Signal 改造是子类迁移的前提）
- Task D3/D4/D5 可并行（不同 Signal 子类）
- Task D6 独立于 D3-D5（View 自身的 session 直访分类）
- Task D7 独立于 D3-D6（DevMode 改信号）
- Task D8 依赖 D1-D7 全部完成
- Phase E（View 拆分）依赖 Phase D 完成（避免合并冲突）
- Phase F（Viewport 拆分）独立于 Phase E，可并行
- Phase G（DsoSignal 拆分）依赖 Task D3 完成（先迁移 Core 直访，再拆分类）
- Phase H（view.h 治理）与 Phase E 协同（E 拆分时 H 同步整理 header）
- Phase I 依赖所有前置 Phase 完成

# Parallelizable Work

- Task D3（DsoSignal）/D4（AnalogSignal）/D5（LogicSignal）可并行 — 不同文件
- Task D6（View）/D7（DevMode）可与 D3-D5 并行 — 不同文件
- Phase E（View 拆分）/Phase F（Viewport 拆分）/Phase G（DsoSignal 拆分）— 不同文件，但都依赖 Phase D 完成
- **重要：所有 Task 完成后才统一编译（Phase I），不每完成一个 Task 就编译**（用户明确要求）

# 实施顺序建议

1. Phase D（Task D1-D8）：先完成 Core 直访消除，这是 P0 阻塞
2. Phase E（Task E1-E4）：View 拆分（依赖 D 完成）
3. Phase F（Task F1-F4）：Viewport 拆分（可与 E 并行）
4. Phase G（Task G1-G4）：DsoSignal 拆分（依赖 D3 完成）
5. Phase H（Task H1-H3）：view.h 治理（与 E 协同）
6. Phase I（Task I1-I4）：集成验证

**注意**：每个 Phase 内部 Task 完成后不立即编译，全部 Phase 完成后统一编译（Phase I1）。
