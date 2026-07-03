# View 层现代化 V2 Spec（View-Core 解耦 + God Class 拆分）

## Why

已有 spec `modernize-view-layer-architecture`（Track A/B/C）已完成 libsigrok 头文件污染清除、单例替代、增量拓扑更新，但 View 层仍存在两类**违反 AGENTS.md 硬约束**的深层问题：

1. **View 层绕过 DataSource 直访 Core**：View 层 106 处直接调用 `_session->get_device()->set_config_*` 操作硬件配置（SR_CONF_PROBE_*），其中 dsosignal.cpp 43 处、analogsignal.cpp 17 处、view.cpp 28 处。AGENTS.md 明确规定"DataSource 接口返回 Core 类型仅（SignalModel*/DecoderStack*/…）—— 让 API 层操作 headless"，但 View 层通过 Signal 基类持有的 `SigSession*` 成员直接访问 Core，违反 MV 模式和 Core/View 分层。

2. **4 个 God class 阻碍维护**：view.cpp 3072 行（~100+ 方法）、viewport.cpp 2903 行（48 方法）、dsosignal.cpp 1827 行（48 方法，6 种职责）、header.cpp 1126 行。view.h 被 19 个文件 include 且自身 include 14 个头文件，任何 view.h 改动触发大规模重编译。view.h 有 13 次访问修饰符切换，成员未按职责分组。

用户明确要求"尽量不留下技术债追求理想架构"，本 spec 系统性消除这两类问题，让 View 层真正成为"只通过 DataSource 接口访问 Core 的纯显示端"。

## 架构目标

```
┌─────────────────────────────────────────────────────────────┐
│  View 层 (Qt Widgets)                                       │
│  ┌─────────────────────────────────────────────────────────┐│
│  │ view::Signal 子类                                       ││
│  │  - 持有 SignalModel* (单一数据源)                        ││
│  │  - 不再持有 SigSession* (移除 signal.h:124 成员)         ││
│  │  - 硬件配置改走 SignalModel setter (model 转发到 Core)   ││
│  │  - 不再 #include <libsigrok.h>                          ││
│  └─────────────────────────────────────────────────────────┘│
│  ┌─────────────────────────────────────────────────────────┐│
│  │ view::View (协调者，<800 行)                             ││
│  │  - 持有 ViewLayout/ViewCursors/ViewDerivedTraces 协调者  ││
│  │  - on_signals_changed 增量分派 (已完成)                  ││
│  └─────────────────────────────────────────────────────────┘│
│  ┌─────────────────────────────────────────────────────────┐│
│  │ view::Viewport (<1000 行)                                ││
│  │  - 持有 ViewportPainter/ViewportInteraction 委托对象     ││
│  └─────────────────────────────────────────────────────────┘│
│  ┌─────────────────────────────────────────────────────────┐│
│  │ view::DsoSignal (<800 行)                                ││
│  │  - 持有 DsoHardwareConfig/DsoMeasure 委托对象            ││
│  │  - 只保留 paint + 协调                                   ││
│  └─────────────────────────────────────────────────────────┘│
└──────────────────────────────────────────────────┬──────────┘
                                                   │ DataSource 接口
┌──────────────────────────────────────────────────▼──────────┐
│  Core 层 (pxview-core, 无 Qt Widgets)                       │
│  ┌─────────────────────────────────────────────────────────┐│
│  │ SignalModel (单一状态源 + 硬件配置转发)                  ││
│  │  - set_vdiv/set_coupling/set_trig_value 已有 (Track A)   ││
│  │  - 新增 set_probe_enabled/set_probe_offset 转发          ││
│  └─────────────────────────────────────────────────────────┘│
│  ┌─────────────────────────────────────────────────────────┐│
│  │ DataSource 接口 (View→Core 唯一通道)                     ││
│  │  - 新增 hardware_config() 访问器返回 DeviceAgent 包装    ││
│  └─────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

## What Changes

### Track D: View 层 Core 直访消除（P0，必做）

- **MODIFIED**：`SignalModel` 新增硬件配置转发方法：`set_probe_enabled(bool)`/`set_probe_offset(uint16_t)`/`set_probe_factor(uint8_t)` 等，内部通过 `_sr_channel` + DeviceAgent 转发，与 Track A 的 `set_vdiv`/`set_coupling`/`set_trig_value` 模式一致
- **MODIFIED**：`view::Signal` 基类移除 `SigSession *session` protected 成员（signal.h:124），子类不再通过 session 访问 Core
- **MODIFIED**：`view::Signal` 基类新增 `DataSource* _data_source` protected 成员（由 View 注入，替代 session 直访）
- **MODIFIED**：`view::DsoSignal` 的 43 处 `session->get_device()->set_config_*` 替换为 `_model->set_xxx()`（model 转发到 Core）
- **MODIFIED**：`view::AnalogSignal` 的 17 处 `session->get_device()->set_config_*` 替换为 `_model->set_xxx()`
- **MODIFIED**：`view::View` 的 28 处 `_session->xxx` 评估分类：facade 访问（如 `cur_view_time`）保留并通过 DataSource 暴露；业务调用（如 `add_decoder`）改走 DataSource
- **MODIFIED**：`view::DevMode` 的 5 处 `_session->stop_capture/session_save/switch_work_mode/close_file` 改为发射 Qt 信号由 MainWindow 处理
- **ADDED**：`DataSource` 接口新增 `get_view_time()`/`get_map_zoom()`/`add_decoder()` 等 View 必需的 facade 方法，让 View 通过接口而非 session 指针访问
- **REMOVED**：`Signal::session()` 访问器移除（如果存在）

### Track E: view::View God class 拆分（P1）

- **ADDED**：`view::ViewLayout` 类承接 scale/offset/scroll 职责（吸收 `set_scale_offset`/`limit_scale_offset`/`update_scale_offset`/`set_scale`/`zoom`/`h_scroll_value_changed`/`apply_scale_offset`/`apply_scale`/`apply_offset`/`update_scroll`/`get_scroll_layout`/`update_margins`/`get_max_offset`/`get_min_offset`）
- **ADDED**：`view::ViewCursors` 类承接 cursor/xcursor 管理职责（吸收 `set_cursor`/`cursor_update`/`make_cursors_order`/`update_cursor`/`set_trig_cursor_posistion`/`xcursor_*` 方法）
- **ADDED**：`view::ViewDerivedTraces` 类承接 decoder/spectrum/math/lissajous 同步职责（吸收 `add_decoder`/`add_spectrum`/`add_math`/`add_lissajous`/`rst_decoder`/`reload_decoders`/`restart_decoders`）
- **MODIFIED**：`view::View` 退化为协调者（<800 行），持有 `unique_ptr<ViewLayout>`/`unique_ptr<ViewCursors>`/`unique_ptr<ViewDerivedTraces>`，对外保留 facade 方法转发到委托对象
- **MODIFIED**：`view::View::on_signals_changed`/`rebuild_signals` 保留在 View（已在 Track C 实现）
- **NOT CHANGED**：`view::View` 对外 public API 不变（facade 转发），保证调用方（mainwindow/dock/toolbar）无需改动

### Track F: view::Viewport God class 拆分（P1）

- **ADDED**：`view::ViewportPainter` 类承接所有 paint 职责（吸收 `paintEvent`/`doPaint`/`paintCursors`/`paintSignals`/`paintProgress`/`paintMeasure`/`paintMask`/`paintSearch`）
- **ADDED**：`view::ViewportInteraction` 类承接事件处理职责（吸收 `mousePressEvent`/`mouseMoveEvent`/`mouseReleaseEvent`/`wheelEvent`/`keyPressEvent`/`gestureEvent` + 3 种模式专属 release 处理）
- **ADDED**：`view::ViewportDrag` 类承接 drag frame 职责（吸收 `applyDragFrame`/`on_drag_timer`/drag 状态管理）
- **MODIFIED**：`view::Viewport` 退化为协调者（<1000 行），持有三个委托对象

### Track G: view::DsoSignal God class 拆分（P1）

- **ADDED**：`view::DsoHardwareConfig` 类承接硬件配置职责（吸收 `set_vdiv`/`set_coupling`/`set_factor`/`set_zero_offset`/`commit_hardware_config`，所有方法内部走 `_model->set_xxx`）
- **ADDED**：`view::DsoTriggerConfig` 类承接触发配置职责（吸收 `set_trig_vrate`/`set_trig_vpos`/`set_trig_ratio`/`get_trig_*`）
- **ADDED**：`view::DsoMeasure` 类承接测量职责（吸收 `get_measure`/`measure`/`get_hover_measure`/`get_voltage`/`get_time`/`auto_set`/`autoV_end`/`autoH_end`/`auto_end`/`auto_start`）
- **MODIFIED**：`view::DsoSignal` 退化为 paint + 协调（<800 行），持有三个委托对象

### Track H: view.h God header 治理（P1，与 Track E 协同）

- **MODIFIED**：view.h 整理访问修饰符为 3 段（public/protected/private），消除 13 次切换
- **MODIFIED**：view.h 用前置声明替代 `#include "signal.h"`/`#include "viewport.h"`/`#include "cursor.h"` 等，减少自身 include 数
- **ADDED**：`view_layout.h`/`view_cursors.h`/`view_derived_traces.h` 独立头文件（Track E 的产物），view.h 只 include 前置声明
- **MODIFIED**：view.h 内联的 `SignalGroup`/`FilterSnapshot` 等结构移到独立头文件

### **BREAKING** Changes

- `view::Signal::session` protected 成员移除（Track D）— View 层内部改动，子类改用 `_model` + `_data_source`
- `view::Signal::session()` 访问器移除（如存在）— 外部消费者改用 `View::session()`（View 仍持有 session facade）
- `view::View` 的若干 private 方法移到 `ViewLayout`/`ViewCursors`/`ViewDerivedTraces`（Track E）— 因原为 private，无外部影响
- `view::Viewport` 的若干 private 方法移到 `ViewportPainter`/`ViewportInteraction`/`ViewportDrag`（Track F）— 同上
- `view::DsoSignal` 的若干方法移到委托类（Track G）— 部分原 protected 方法可能被同文件 test 调用，需验证

## Impact

- **Affected specs**:
  - `modernize-view-layer-architecture`（前置，已完成 Track A/B/C）— 本 spec 是其 V2 续作，完成 Track A 未覆盖的 106 处 Core 直访
  - `unify-viewport-state-mutator`（已完成）— Track E 的 ViewLayout 吸收 apply_scale_offset/apply_scale/apply_offset mutator
  - `sync-decode-dock-with-viewport`（已完成）— Track E 的 ViewLayout 吸收 visible_range_changed 信号
  - `decouple-core-from-view-v2`（已完成）— 本 spec 是其反向解耦（View 不再直访 Core）

- **Affected code**:
  - `PXView/pv/data/signalmodel.h/.cpp` — Track D：新增 set_probe_enabled/set_probe_offset 转发
  - `PXView/pv/data/datasource.h` — Track D：新增 View 必需的 facade 方法
  - `PXView/pv/view/signal.h/.cpp` — Track D：移除 session 成员，新增 _data_source 成员
  - `PXView/pv/view/dsosignal.h/.cpp` — Track D+G：43 处 Core 直访迁移 + 拆分 3 个委托类
  - `PXView/pv/view/analogsignal.h/.cpp` — Track D：17 处 Core 直访迁移
  - `PXView/pv/view/logicsignal.h/.cpp` — Track D：少量 session 直访迁移
  - `PXView/pv/view/view.h/.cpp` — Track D+E+H：28 处直访迁移 + 拆分 3 个委托类 + header 治理
  - `PXView/pv/view/viewport.h/.cpp` — Track F：拆分 3 个委托类
  - `PXView/pv/view/header.h/.cpp` — Track H：header 治理（仅前置声明优化）
  - `PXView/pv/view/devmode.h/.cpp` — Track D：5 处 Core 业务调用改信号
  - **新增文件**：view_layout.h/.cpp、view_cursors.h/.cpp、view_derived_traces.h/.cpp、viewport_painter.h/.cpp、viewport_interaction.h/.cpp、viewport_drag.h/.cpp、dso_hardware_config.h/.cpp、dso_trigger_config.h/.cpp、dso_measure.h/.cpp（共 9 对新文件）

- **Core/View 边界**：Track D 让 View 层完全脱离 SigSession 直访，所有 Core 访问走 DataSource 接口。Core/View 边界更清晰，headless 模式更纯粹。

- **向后兼容**：内部架构重构，不影响 `.pxc` 文件格式、MCP API、WS API。View 对外 public API 通过 facade 转发保持不变。

## ADDED Requirements

### Requirement: SignalModel 硬件配置转发完整化

`SignalModel` SHALL 提供完整的硬件配置转发方法，覆盖 View 层所有 `session->get_device()->set_config_*` 操作，让 View 层只调 model setter 即可完成硬件配置。

新增/增强方法：
- `set_probe_enabled(bool)` — 转发 `ds_set_probe_parameter(SR_CONF_PROBE_EN, ...)`
- `set_probe_offset(uint16_t)` — 转发 `ds_set_probe_parameter(SR_CONF_PROBE_OFFSET, ...)`
- `set_probe_factor(uint8_t)` — 转发 `ds_set_probe_parameter(SR_CONF_PROBE_FACTOR, ...)`
- 已有（Track A）：`set_vdiv`/`set_coupling`/`set_trig_value`/`set_vertical_offset`

#### Scenario: View 修改 DSO 通道启用状态通过 model 转发
- **WHEN** 用户在 UI 中启用/禁用 DSO 通道
- **THEN** View 调用 `model->set_probe_enabled(en)`
- **AND** `SignalModel::set_probe_enabled` 内部调用 `DeviceAgent::set_probe_parameter(_sr_channel, SR_CONF_PROBE_EN, ...)`
- **AND** View 层不直接调用 `session->get_device()->set_config_bool`

#### Scenario: Headless 模式下通过 MCP 修改通道启用状态
- **WHEN** MCP API 调用 `set_channel_config(channel_index, "enabled", value)`
- **THEN** SessionService 调用 `SignalModel::set_probe_enabled(value)`
- **AND** 硬件配置同步路径与 GUI 模式完全一致

### Requirement: View 层不持有 SigSession 直访指针

`view::Signal` 及其子类 SHALL NOT 持有 `SigSession*` 成员，SHALL 通过 `DataSource*` 接口访问 Core 层 facade。

#### Scenario: Signal 子类访问 Core facade
- **WHEN** DsoSignal 需要读取 samplerate
- **THEN** 通过 `_data_source->get_samplerate()` 访问
- **AND** 不通过 `_session->cur_snap_samplerate()`

#### Scenario: Signal 子类修改硬件配置
- **WHEN** DsoSignal 需要修改 vdiv
- **THEN** 通过 `_model->set_vdiv(value)` 访问
- **AND** 不通过 `_session->get_device()->set_config_uint64(SR_CONF_PROBE_VDIV, ...)`

### Requirement: DevMode 通过信号通知 Core 业务调用

`view::DevMode` SHALL NOT 直接调用 `_session->stop_capture()`/`session_save()`/`switch_work_mode()`/`close_file()`，SHALL 发射 Qt 信号由 MainWindow 处理。

#### Scenario: DevMode 切换工作模式
- **WHEN** 用户在 DevMode widget 中切换模式
- **THEN** DevMode 发射 `mode_change_requested(int mode)` 信号
- **AND** MainWindow 接收信号并调用 `_session->switch_work_mode(mode)`

### Requirement: view::View 拆分为协调者 + 3 个委托类

`view::View` SHALL 持有 `unique_ptr<ViewLayout>`/`unique_ptr<ViewCursors>`/`unique_ptr<ViewDerivedTraces>`，将 scale/offset/cursor/derived-trace 职责委托出去，自身退化为协调者（<800 行）。

#### Scenario: scale/offset 操作委托给 ViewLayout
- **WHEN** 用户滚轮缩放波形
- **THEN** View 接收事件并转发到 `_layout->zoom(anchor, factor)`
- **AND** ViewLayout 内部调用 `apply_scale_offset` mutator（来自 `unify-viewport-state-mutator` spec）
- **AND** View 自身不包含 scale/offset 实现代码

#### Scenario: cursor 管理委托给 ViewCursors
- **WHEN** 用户设置时间游标
- **THEN** View 接收并转发到 `_cursors->set_cursor(...)`
- **AND** View 自身不包含 cursor 实现代码

### Requirement: view::Viewport 拆分为协调者 + 3 个委托类

`view::Viewport` SHALL 持有 `unique_ptr<ViewportPainter>`/`unique_ptr<ViewportInteraction>`/`unique_ptr<ViewportDrag>`，将 paint/event/drag 职责委托出去，自身退化为协调者（<1000 行）。

#### Scenario: paint 委托给 ViewportPainter
- **WHEN** Viewport::paintEvent 触发
- **THEN** Viewport 调用 `_painter->doPaint(...)`
- **AND** Viewport 自身不包含 paint 实现代码

### Requirement: view::DsoSignal 拆分为协调者 + 3 个委托类

`view::DsoSignal` SHALL 持有 `unique_ptr<DsoHardwareConfig>`/`unique_ptr<DsoTriggerConfig>`/`unique_ptr<DsoMeasure>`，将硬件配置/触发/测量职责委托出去，自身退化为 paint + 协调（<800 行）。

#### Scenario: 硬件配置委托给 DsoHardwareConfig
- **WHEN** 用户修改 DSO 通道 vdiv
- **THEN** DsoSignal 调用 `_hw_config->set_vdiv(value)`
- **AND** DsoHardwareConfig 内部调 `_model->set_vdiv(value)`
- **AND** DsoSignal 自身不包含 set_config_* 调用

### Requirement: view.h God header 治理

`view.h` SHALL 整理访问修饰符为 3 段（public/protected/private），SHALL 用前置声明替代非必要 include，SHALL 将内联结构定义移到独立头文件。

#### Scenario: view.h include 数减少
- **WHEN** 重构完成
- **THEN** view.h 的 `#include` 数从 14 个减少到 ≤ 8 个
- **AND** view.h 用前置声明替代 `#include "signal.h"`/`#include "viewport.h"` 等

#### Scenario: view.h 访问修饰符整理
- **WHEN** 重构完成
- **THEN** view.h 只有 3 段访问修饰符（public/protected/private）
- **AND** 消除原 13 次切换

## MODIFIED Requirements

### Requirement: View 层 Core 访问通道

原实现：View 层通过 `Signal::session` 成员和 `View::_session` 成员直接访问 SigSession 的所有 public 方法（106 处直访）。

新实现：View 层通过 `DataSource*` 接口访问 Core facade，`Signal` 基类不持有 session 指针，`View` 持有 session 指针仅用于 facade 转发（不暴露给子类）。

### Requirement: view::View 类规模

原实现：view.cpp 3072 行，~100+ 方法，承担数据绑定/文档绑定/UI 状态/捕获/缩放/分组/配色/cursors/derived traces 等十余种职责。

新实现：view.cpp <800 行，承担协调者角色，scale/offset 委托 ViewLayout，cursor 委托 ViewCursors，derived trace 委托 ViewDerivedTraces。

## REMOVED Requirements

### Requirement: view::Signal 持有 SigSession* 成员

**Reason**: 违反 Core/View 分层，导致 106 处 Core 直访（dsosignal 43 处/analogsignal 17 处/view 28 处等），让 View 层成为 Core 的"全权代理人"而非纯显示端。

**Migration**:
- `view::Signal::session` protected 成员移除
- 子类改用 `_model->set_xxx()` 修改硬件配置（Track D 转发）
- 子类改用 `_data_source->get_xxx()` 访问 Core facade（如 samplerate）
- `View::_session` 保留作为 facade 转发，不暴露给 Signal 子类

### Requirement: view::View 承担十余种职责

**Reason**: God class 阻碍维护，3072 行 100+ 方法导致任何改动都需理解全部上下文，违反单一职责原则。

**Migration**:
- scale/offset/scroll 职责移到 `ViewLayout`
- cursor/xcursor 职责移到 `ViewCursors`
- decoder/spectrum/math/lissajous 职责移到 `ViewDerivedTraces`
- View 保留 on_signals_changed/rebuild_signals 协调逻辑 + facade 转发
