## Phase D: View 层 Core 直访消除

- [x] SignalModel 新增 `set_probe_enabled(bool)` 转发方法
- [x] SignalModel 新增 `set_probe_offset(uint16_t)` 转发方法
- [x] SignalModel 新增 `set_probe_factor(uint64_t)` 转发方法（注：spec 原写 uint8_t，但现有 set_vfactor/commit_to_device 均用 uint64/set_config_uint64，已按现有模式修正）
- [x] 所有新方法在 `_sr_channel == nullptr` 时不崩溃
- [x] `view::Signal` 基类移除 `SigSession *session` protected 成员
- [x] `view::Signal` 基类新增 `DataSource* _data_source` protected 成员
- [x] `Signal` 构造函数接收 `DataSource*` 参数（替代 session）
- [x] `LogicSignal`/`AnalogSignal`/`DsoSignal` 构造函数透传 `data_source`
- [x] `SignalFactory::create_signal`/`create_signals`/`update_signals` 传入 `data_source`
- [x] dsosignal.cpp 无 `session->get_device()->set_config_*` 残留（原 43 处）
- [ ] analogsignal.cpp 无 `session->get_device()->set_config_*` 残留（原 17 处）
- [ ] logicsignal.cpp 无 `session->get_device()` 残留
- [ ] view.cpp 中 `_session->` 仅保留 facade 转发和广播
- [ ] DevMode 新增 `mode_change_requested`/`stop_capture_requested`/`save_session_requested`/`close_file_requested` 信号
- [ ] devmode.cpp 无 `_session->` 业务调用残留（原 5 处）
- [ ] MainWindow connect DevMode 信号到 session 方法
- [ ] grep `_session->get_device()` 在 `pv/view/` 目录：0 命中
- [ ] grep `session->get_device()` 在 `pv/view/` 目录：0 命中
- [ ] grep `_session->` 在 `pv/view/` 目录：≤ 10 命中（仅 View facade + 广播）

## Phase E: view::View God class 拆分

- [ ] 新建 `view_layout.h`/`view_layout.cpp`，ViewLayout 类声明
- [ ] ViewLayout 承接 scale/offset/scroll 方法（set_scale_offset/limit_scale_offset/update_scale_offset/set_scale/zoom/h_scroll_value_changed/apply_scale_offset/apply_scale/apply_offset/update_scroll/get_scroll_layout/update_margins/get_max_offset/get_min_offset）
- [ ] ViewLayout 持有 `View* _view` back-pointer
- [ ] View 持有 `unique_ptr<ViewLayout> _layout`
- [ ] View 对外 facade 方法转发到 `_layout`
- [ ] 新建 `view_cursors.h`/`view_cursors.cpp`，ViewCursors 类声明
- [ ] ViewCursors 承接 cursor 管理方法（set_cursor/cursor_update/make_cursors_order/update_cursor/set_trig_cursor_posistion/xcursor_*）
- [ ] View 持有 `unique_ptr<ViewCursors> _cursors`
- [ ] 新建 `view_derived_traces.h`/`view_derived_traces.cpp`，ViewDerivedTraces 类声明
- [ ] ViewDerivedTraces 承接 decoder/spectrum/math/lissajous 方法
- [ ] View 持有 `unique_ptr<ViewDerivedTraces> _derived`
- [ ] view.cpp 行数 < 800
- [ ] View 不再直接包含 scale/offset/cursor/derived-trace 实现代码
- [ ] View 对外 public API 不变（调用方无需改动）

## Phase F: view::Viewport God class 拆分

- [ ] 新建 `viewport_painter.h`/`viewport_painter.cpp`
- [ ] ViewportPainter 承接 paint 方法（paintEvent/doPaint/paintCursors/paintSignals/paintProgress/paintMeasure/paintMask/paintSearch）
- [ ] Viewport 持有 `unique_ptr<ViewportPainter> _painter`
- [ ] 新建 `viewport_interaction.h`/`viewport_interaction.cpp`
- [ ] ViewportInteraction 承接事件方法（mouse*/wheel*/key*/gesture* + 3 种模式 release）
- [ ] Viewport 持有 `unique_ptr<ViewportInteraction> _interaction`
- [ ] 新建 `viewport_drag.h`/`viewport_drag.cpp`
- [ ] ViewportDrag 承接 drag 方法（applyDragFrame/on_drag_timer/drag 状态）
- [ ] Viewport 持有 `unique_ptr<ViewportDrag> _drag`
- [ ] viewport.cpp 行数 < 1000
- [ ] Viewport 不再直接包含 paint/event/drag 实现代码

## Phase G: view::DsoSignal God class 拆分

- [ ] 新建 `dso_hardware_config.h`/`dso_hardware_config.cpp`
- [ ] DsoHardwareConfig 承接硬件配置方法（set_vdiv/set_coupling/set_factor/set_zero_offset/commit_hardware_config）
- [ ] DsoHardwareConfig 方法内部走 `_model->set_xxx()`
- [ ] DsoSignal 持有 `unique_ptr<DsoHardwareConfig> _hw_config`
- [ ] 新建 `dso_trigger_config.h`/`dso_trigger_config.cpp`
- [ ] DsoTriggerConfig 承接触发方法（set_trig_vrate/set_trig_vpos/set_trig_ratio/get_trig_*）
- [ ] DsoSignal 持有 `unique_ptr<DsoTriggerConfig> _trig_config`
- [ ] 新建 `dso_measure.h`/`dso_measure.cpp`
- [ ] DsoMeasure 承接测量方法（get_measure/measure/get_hover_measure/get_voltage/get_time/auto_set/autoV_end/autoH_end/auto_end/auto_start）
- [ ] DsoSignal 持有 `unique_ptr<DsoMeasure> _measure`
- [ ] dsosignal.cpp 行数 < 800
- [ ] DsoSignal 只保留 paint_back/mid/fore/trace/envelope/type_options/hover_measure + 协调方法

## Phase H: view.h God header 治理

- [ ] view.h 成员按职责分组到 public/protected/private 三段
- [ ] view.h 访问修饰符切换次数 = 3（消除原 13 次）
- [ ] view.h 用前置声明替代 `#include "signal.h"`
- [ ] view.h 用前置声明替代 `#include "viewport.h"`
- [ ] view.h 用前置声明替代 `#include "cursor.h"`
- [ ] view.h 的 #include 数从 14 减少到 ≤ 8
- [ ] `SignalGroup` 结构移到 `signal_group.h`
- [ ] `FilterSnapshot` 结构移到独立头文件（如存在）

## Phase I: 集成验证

- [ ] `cd build && ninja -j 16` 编译通过
- [ ] `ninja install` 安装成功
- [ ] `install.dir/bin/PXView.exe` 生成
- [ ] grep `_session->get_device()` 在 `pv/view/` 目录：0 命中
- [ ] grep `session->get_device()` 在 `pv/view/` 目录：0 命中
- [ ] grep `_session->` 在 `pv/view/` 目录：≤ 10 命中
- [ ] grep `_probe` 在 `pv/view/` 目录：仅局部变量
- [ ] grep `#include <libsigrok.h>` 在 `pv/view/` 目录：仅 3 个合法文件
- [ ] view.cpp 行数 < 800
- [ ] viewport.cpp 行数 < 1000
- [ ] dsosignal.cpp 行数 < 800
- [ ] view.h #include 数 ≤ 8
- [ ] view.h 访问修饰符切换次数 = 3
- [ ] GUI：启动 PXView.exe 通道正常显示
- [ ] GUI：滚轮缩放波形 scale/offset 正常
- [ ] GUI：修改 DSO 通道 vdiv/coupling 硬件配置同步
- [ ] GUI：启用/禁用通道增量更新
- [ ] GUI：添加/移除 Decoder 正常
- [ ] GUI：切换工作模式（DevMode）信号通知链路正常
- [ ] GUI：重新采集通道顺序和高度保留
- [ ] GUI：切换/关闭 Tab 无崩溃
- [ ] Headless：MCP API 在 10110 端口可用
- [ ] Headless：MCP 完整流程通过
- [ ] Headless：`%TEMP%/pxview_mcp_debug.log` 无异常
- [ ] Headless：不创建 QWidget
