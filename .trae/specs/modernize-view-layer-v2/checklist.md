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
- [x] analogsignal.cpp 无 `session->get_device()->set_config_*` 残留（原 17 处）
- [x] logicsignal.cpp 无 `session->get_device()` 残留
- [x] view.cpp 中 `_session->` 仅保留 facade 转发和广播（实际 4 处 lambda 转发，符合 D6.4）
- [x] DevMode 新增 `mode_change_requested`/`stop_capture_requested`/`save_session_requested`/`close_file_requested` 信号
- [x] devmode.cpp 无 `_session->` 业务调用残留（原 5 处）
- [x] MainWindow connect DevMode 信号到 session 方法
- [x] grep `_session->get_device()` 在 `pv/view/` 目录：0 命中
- [x] grep `session->get_device()` 在 `pv/view/` 目录：0 命中
- [x] grep `_session->` 在 `pv/view/` 目录：≤ 10 命中（实际 4 处，仅 D7.6 lambda 转发）

## Phase E: view::View God class 拆分

- [x] 新建 `view_layout.h`/`view_layout.cpp`，ViewLayout 类声明
- [x] ViewLayout 承接 scale/offset/scroll 方法（set_scale_offset/limit_scale_offset/update_scale_offset/set_scale/zoom/h_scroll_value_changed/apply_scale_offset/apply_scale/apply_offset/update_scroll/get_scroll_layout/update_margins/get_max_offset/get_min_offset）
- [x] ViewLayout 持有 `View* _view` back-pointer
- [x] View 持有 `unique_ptr<ViewLayout> _layout`
- [x] View 对外 facade 方法转发到 `_layout`（header inline forwarder）
- [x] 新建 `view_cursors.h`/`view_cursors.cpp`，ViewCursors 类声明
- [x] ViewCursors 承接 cursor 管理方法（set_cursor/cursor_update/make_cursors_order/update_cursor/set_trig_cursor_posistion/xcursor_*）
- [x] View 持有 `unique_ptr<ViewCursors> _cursors`
- [x] 新建 `view_derived_traces.h`/`view_derived_traces.cpp`，ViewDerivedTraces 类声明
- [x] ViewDerivedTraces 承接 decoder/spectrum/math/lissajous 方法
- [x] View 持有 `unique_ptr<ViewDerivedTraces> _derived`
- [x] view.cpp 删除 27 个死代码方法副本（2383→2040 行；目标 <800 未达，剩余为 active 实现：signals_changed/rebuild_signals/glitch filter 等，需进一步拆分到 view_signal_sync.cpp/view_glitch_filter.cpp）
- [x] View 不再直接包含 scale/offset/cursor/derived-trace 实现代码（已委托）
- [x] View 对外 public API 不变（调用方无需改动）

## Phase F: view::Viewport God class 拆分

- [x] 新建 `viewport_painter.h`/`viewport_painter.cpp`
- [x] ViewportPainter 承接 paint 方法（paintEvent/doPaint/paintCursors/paintSignals/paintProgress/paintMeasure；paintMask/paintSearch 在原代码中不存在，N/A）
- [x] Viewport 持有 `unique_ptr<ViewportPainter> _painter`
- [x] 新建 `viewport_interaction.h`/`viewport_interaction.cpp`
- [x] ViewportInteraction 承接事件方法（mouse*/wheel*/key*/gesture*/leave + 3 种模式 release + edge nav）
- [x] Viewport 持有 `unique_ptr<ViewportInteraction> _interaction`
- [x] 新建 `viewport_drag.h`/`viewport_drag.cpp`
- [x] ViewportDrag 承接 drag 方法（applyDragFrame/on_drag_timer/drag 状态）
- [x] Viewport 持有 `unique_ptr<ViewportDrag> _drag`
- [x] viewport.cpp 行数 < 1000（实际 596 行，原 2903 行）
- [x] Viewport 不再直接包含 paint/event/drag 实现代码（仅保留 thin forwarder）

## Phase G: view::DsoSignal God class 拆分

- [x] 新建 `dso_hardware_config.h`/`dso_hardware_config.cpp`（100 行 / 380 行）
- [x] DsoHardwareConfig 承接硬件配置方法（set_enable/set_vDialActive/go_vDialPre/go_vDialNext/get_vDialValue/get_vDialSel/init_vDial/set_acCoupling/set_factor/get_factor/get_zero_vpos/get_zero_ratio/get_hw_offset/set_zero_vpos/set_zero_ratio/ratio2value/ratio2pos/value2ratio/pos2ratio/load_settings/commit_settings）
- [x] DsoHardwareConfig 方法内部走 `_model->set_xxx()`
- [x] DsoSignal 持有 `unique_ptr<DsoHardwareConfig> _hw_config`
- [x] 新建 `dso_trigger_config.h`/`dso_trigger_config.cpp`（59 行 / 70 行）
- [x] DsoTriggerConfig 承接触发方法（get_trig_vrate/set_trig_vpos/set_trig_ratio）
- [x] DsoSignal 持有 `unique_ptr<DsoTriggerConfig> _trig_config`
- [x] 新建 `dso_measure.h`/`dso_measure.cpp`（103 行 / 481 行）
- [x] DsoMeasure 承接测量方法（get_measure/measure/get_hover/get_point/get_voltage×2/get_time/auto_set/autoV_end/autoH_end/auto_end/auto_start/update_measure_status/paint_hover_measure/call_auto_end）
- [x] DsoSignal 持有 `unique_ptr<DsoMeasure> _measure`
- [x] dsosignal.cpp 行数 < 800（实际 683 行）
- [x] DsoSignal 只保留 paint_back/mid/fore/trace/envelope/type_options/hover_measure + 协调方法
- [x] dsosignal.h 添加 3 个前置声明 + 3 个 friend 声明 + 3 个 unique_ptr 成员
- [x] cmake/gui_sources.cmake 添加 6 个新文件条目（3 个 .cpp + 3 个 .h）

## Phase H: view.h God header 治理

- [ ] view.h 成员按职责分组到 public/protected/private 三段
- [ ] view.h 访问修饰符切换次数 = 3（消除原 13 次）
- [x] view.h 用前置声明替代 `#include "signal.h"`
- [x] view.h 用前置声明替代 `#include "viewport.h"`
- [x] view.h 用前置声明替代 `#include "cursor.h"`
- [ ] view.h 的 #include 数从 14 减少到 ≤ 8（实际 15，含 3 个委托类 header 必须 include 以支持 inline forwarder；从原 28 降至 15）
- [ ] `SignalGroup` 结构移到 `signal_group.h`
- [ ] `FilterSnapshot` 结构移到独立头文件（如存在）

## Phase I: 集成验证

- [x] `cd build && ninja -j 16` 编译通过
- [x] `ninja install` 安装成功
- [x] `install.dir/bin/PXView.exe` 生成
- [x] grep `_session->get_device()` 在 `pv/view/` 目录：0 命中
- [x] grep `session->get_device()` 在 `pv/view/` 目录：0 命中
- [x] grep `_session->` 在 `pv/view/` 目录：≤ 10 命中（实际 4 处）
- [ ] grep `_probe` 在 `pv/view/` 目录：仅局部变量
- [ ] grep `#include <libsigrok.h>` 在 `pv/view/` 目录：仅 3 个合法文件
- [ ] view.cpp 行数 < 800（实际 2040，需进一步拆分 active 实现）
- [x] viewport.cpp 行数 < 1000（实际 505）
- [x] dsosignal.cpp 行数 < 800（实际 684）
- [ ] view.h #include 数 ≤ 8（实际 15，3 个委托类 header 必须 include）
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
