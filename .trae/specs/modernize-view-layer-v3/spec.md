# View 层现代化 V3 Spec（剩余债务清理 + 硬约束修复）

## Why

V2 spec 完成了 Phase D（Core 直访消除）、Phase E（view.cpp 死代码清理）、Phase F/G（Viewport/DsoSignal 拆分）、Phase H 部分（view.h include 优化）。但全面审计发现 9 大类剩余债务，涵盖硬约束违规（assert(false) 无 early-return、_scale 直接赋值）、spec 未达项（view.cpp/view.h 目标行数）、架构精神未贯彻（Signal 子类 22 处 `_view->session()` 直访）、libsigrok 污染回流（4 处违规 include）。本 spec 系统性清理这些债务，让 view 层真正达到 AGENTS.md 硬约束和理想架构目标。

## What Changes

### Track J: view.cpp 继续瘦身（达 <800 行）

- **ADDED**：`view_signal_sync.cpp` 承接 signals_changed/rebuild_signals_from_config/rebuild_signals/on_signals_changed/compute_signal_groups/signals_*_layout/signals_modified_refresh（~708 行）
- **ADDED**：`view_glitch_filter.cpp` 承接 on_*_glitch_*/undo_filter/get_preview_ranges/on_toggle_invert_requested（~217 行）
- **ADDED**：`view_data_sync.cpp` 承接 set_data_source/clear_signal_data/set_signal_data_from_source/set_data_document/clone_signals_for_document/frame_began/receive_end/receive_trigger/data_updated/set_receive_len（~267 行）
- **MODIFIED**：view.cpp 退化为协调者（<800 行），仅保留构造析构/事件处理/forwarder/颜色/度量/坐标转换

### Track K: view.h God header 完成治理

- **MODIFIED**：view.h 访问修饰符从 13 段整理为 3 段（public/protected/private）
- **MODIFIED**：view.h inline forwarder（37 个）下沉到 view.cpp（改 out-of-line），让 view_cursors.h/view_derived_traces.h/view_layout.h 改前置声明
- **MODIFIED**：view.h #include 数从 15 降至 ≤8
- **ADDED**：`signal_group.h` 承接 SignalGroup 结构
- **ADDED**：`filter_snapshot.h` 承接 FilterSnapshot 结构（或下沉到 view.cpp）

### Track L: Signal 子类 session 直访消除

- **MODIFIED**：DataSource 接口新增 `is_running_status()`/`is_stopped_status()`/`cur_snap_samplerate()`/`cur_samplelimits()`/`get_active_document()`/`get_signal_models()`/`trigd()`/`trigd_ch()` 方法（已有部分 facade，补全）
- **ADDED**：`ICaptureControl` 接口（或 DataSource 扩展）承载 `stop_capture()`/`start_capture()`/`refresh()` capture 生命周期控制
- **ADDED**：`IAutoLock` 接口承载 `get_data_auto_lock()`/`data_auto_lock()`
- **MODIFIED**：analogsignal.cpp 6 处 `_view->session().xxx()` 改走 `_data_source->xxx()`
- **MODIFIED**：dsosignal.cpp 3 处 `_view->session().xxx()` 改走 `_data_source->xxx()`，1 处 broadcast_msg 改由 View 代广播
- **MODIFIED**：decodetrace.cpp 5 处 session 直访改走 `_data_source`
- **MODIFIED**：spectrumtrace.cpp 3 处 session 直访改走 `_data_source`
- **MODIFIED**：dso_hardware_config.cpp 12 处 capture 生命周期调用改走 ICaptureControl
- **MODIFIED**：dso_measure.cpp 6 处改走 DataSource/IAutoLock
- **MODIFIED**：viewstatus.cpp/ruler.cpp 5 处 `device()->get_work_mode()` 改走 `_data_source->device()`

### Track M: libsigrok.h 违规 include 清理

- **MODIFIED**：dso_hardware_config.cpp 移除 `#include <libsigrok.h>`，sr_channel* 改用 SignalModel accessor
- **MODIFIED**：dso_measure.cpp/dso_measure.h 移除 `#include <libsigrok.h>`，DSO_MEASURE_TYPE 用前置声明或包装
- **MODIFIED**：dso_trigger_config.cpp 移除冗余 `#include <libsigrok.h>`

### Track N: assert(false) 无 early-return 修复（硬约束 L92）

- **MODIFIED**：devmode.cpp:199/247 补 early-return
- **MODIFIED**：dsldial.cpp:172 补 early-return（防越界）
- **MODIFIED**：dso_measure.cpp:271/275 补 early-return（防 NULL deref/除零）
- **MODIFIED**：trace.cpp:133/142 补 early-return（防空 list.front()）

### Track O: _scale 直接赋值修复（硬约束 L43）

- **MODIFIED**：view.cpp:985/987（mode_changed）改走 `set_scale_offset()` mutator
- **MODIFIED**：view.cpp:1318/1325（resizeEvent）改走 `set_scale_offset()` mutator

## Impact

- **Affected specs**:
  - `modernize-view-layer-v2`（前置，Phase D/E/F/G/H 部分完成）— 本 spec 是其 V3 续作，完成剩余 view.cpp 瘦身和 view.h 治理
  - `harden-crash-points-batch2`（已完成）— Track N 是其 view 层补丁
  - `unify-viewport-state-mutator`（已完成）— Track O 是其遗留修复

- **Affected code**:
  - `PXView/pv/view/view.cpp` — Track J/K/O：瘦身 + header 整理 + _scale mutator
  - `PXView/pv/view/view.h` — Track K：访问修饰符 + include + forwarder 下沉
  - `PXView/pv/data/datasource.h/.cpp` — Track L：扩展 facade 方法
  - `PXView/pv/view/analogsignal.cpp` — Track L：6 处 session 直访迁移
  - `PXView/pv/view/dsosignal.cpp` — Track L：4 处 session 直访迁移
  - `PXView/pv/view/decodetrace.cpp` — Track L：5 处 session 直访迁移
  - `PXView/pv/view/spectrumtrace.cpp` — Track L：3 处 session 直访迁移
  - `PXView/pv/view/dso_hardware_config.cpp` — Track L/M：capture 控制迁移 + libsigrok 清理
  - `PXView/pv/view/dso_measure.cpp/.h` — Track L/M：AutoLock 迁移 + libsigrok 清理
  - `PXView/pv/view/dso_trigger_config.cpp` — Track M：移除冗余 include
  - `PXView/pv/view/viewstatus.cpp`/`ruler.cpp` — Track L：device() 改走 _data_source
  - `PXView/pv/view/devmode.cpp`/`dsldial.cpp`/`trace.cpp` — Track N：assert 修复
  - **新增文件**：view_signal_sync.h/.cpp、view_glitch_filter.h/.cpp、view_data_sync.h/.cpp、signal_group.h、filter_snapshot.h、icapture_control.h、iauto_lock.h（共 7 对新文件）

## ADDED Requirements

### Requirement: view.cpp 降级为协调者

`view::View` SHALL 将 signals_changed/rebuild_signals/glitch filter/data sync 职责委托出去，自身退化为协调者（<800 行）。

#### Scenario: 信号同步委托给 ViewSignalSync
- **WHEN** signals_changed 事件触发
- **THEN** View 转发到 `_signal_sync->signals_changed()`
- **AND** View 自身不包含 rebuild_signals 实现代码

#### Scenario: glitch filter 委托给 ViewGlitchFilter
- **WHEN** 用户触发 glitch filter popup
- **THEN** View 转发到 `_glitch_filter->on_show_glitch_filter_popup()`
- **AND** View 自身不包含 filter apply/undo 实现代码

### Requirement: view.h header 治理完成

`view.h` SHALL 整理访问修饰符为 3 段，SHALL 将 inline forwarder 下沉到 view.cpp，SHALL 将内联结构移到独立头文件。

#### Scenario: view.h include 数达标
- **WHEN** 重构完成
- **THEN** view.h 的 `#include` 数 ≤ 8
- **AND** view_cursors.h/view_derived_traces.h/view_layout.h 改为前置声明

#### Scenario: 访问修饰符整理
- **WHEN** 重构完成
- **THEN** view.h 只有 3 段访问修饰符（public/protected/private）
- **AND** 消除原 13 次切换

### Requirement: Signal 子类不直访 session facade

`view::Signal` 及其子类 SHALL NOT 通过 `_view->session()` 访问 SigSession，SHALL 通过 `_data_source` 接口访问 Core facade。

#### Scenario: Signal 子类查询采样率
- **WHEN** DsoSignal 需要读取 samplerate
- **THEN** 通过 `_data_source->cur_snap_samplerate()` 访问
- **AND** 不通过 `_view->session().cur_snap_samplerate()`

#### Scenario: DsoHardwareConfig 控制 capture 生命周期
- **WHEN** DsoHardwareConfig 需要 stop_capture
- **THEN** 通过 `_capture_control->stop_capture()` 访问
- **AND** 不通过 `_signal->_view->session().stop_capture()`

### Requirement: assert(false) 必须有 early-return

所有 `assert(false)` 后续 5 行内 MUST 有 `return`/`throw`/`continue`/`break`，违反 Hard Constraints L92。

#### Scenario: assert(false) 后 early-return
- **WHEN** assert(false) 在 Release 下被跳过
- **THEN** 后续代码不执行（因有 early-return）
- **AND** 不导致 UB/NULL deref/越界

### Requirement: _scale/_offset 必须走 mutator

View 层 `_scale`/`_offset` 状态 MUST 通过 `set_scale_offset()` mutator 写入，禁止直接赋值，违反 Hard Constraints L43。

#### Scenario: mode_changed 修改 _scale
- **WHEN** work mode 切换
- **THEN** 通过 `set_scale_offset(scale, offset())` 修改
- **AND** 不通过 `_scale = xxx` 直接赋值

## REMOVED Requirements

### Requirement: Signal 子类通过 _view->session() 访问 session

**Reason**: 违反 spec D6 精神，22 处直访让 Signal 子类成为 session 的全权代理人，与 Core/View 分层冲突。

**Migration**:
- 数据层查询（samplerate/status）改走 `_data_source->xxx()`
- capture 生命周期控制改走 `ICaptureControl` 接口
- auto_lock 状态改走 `IAutoLock` 接口
