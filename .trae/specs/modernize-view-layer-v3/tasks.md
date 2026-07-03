# Tasks

## Phase J: view.cpp 继续瘦身（P0，达 <800 行）

- [x] Task J1: 新增 ViewSignalSync 类承接信号同步职责
  - [x] J1.1 新建 `view_signal_sync.h`/`view_signal_sync.cpp`，声明 ViewSignalSync 类
  - [x] J1.2 迁移方法：compute_signal_groups/signals_changed/rebuild_signals_from_config/rebuild_signals/on_signals_changed/signals_added_layout/signals_removed_layout/signals_modified_refresh
  - [x] J1.3 ViewSignalSync 持有 `View* _view` back-pointer（friend View 模式）
  - [x] J1.4 View 持有 `unique_ptr<ViewSignalSync> _signal_sync`，facade 转发
  - [ ] J1.5 编译验证通过（最后统一编译）

- [x] Task J2: 新增 ViewGlitchFilter 类承接 glitch filter 职责
  - [x] J2.1 新建 `view_glitch_filter.h`/`view_glitch_filter.cpp`，声明 ViewGlitchFilter 类
  - [x] J2.2 迁移方法：on_show_glitch_filter_popup/on_clear_glitch_filter_requested/on_toggle_invert_requested/on_glitch_preview_changed/on_glitch_apply_requested/on_glitch_popup_closed/get_preview_ranges/undo_filter/on_glitch_filter_completed/on_glitch_filter_cleared
  - [x] J2.3 ViewGlitchFilter 持有 `View* _view` back-pointer
  - [x] J2.4 View 持有 `unique_ptr<ViewGlitchFilter> _glitch_filter`，facade 转发
  - [ ] J2.5 编译验证通过

- [x] Task J3: 新增 ViewDataSync 类承接数据同步职责
  - [x] J3.1 新建 `view_data_sync.h`/`view_data_sync.cpp`，声明 ViewDataSync 类
  - [x] J3.2 迁移方法：set_data_source/clear_signal_data/set_signal_data_from_source/set_data_document/clone_signals_for_document/document_snapshot_source/frame_began/receive_end/receive_trigger/data_updated/set_receive_len
  - [x] J3.3 ViewDataSync 持有 `View* _view` back-pointer
  - [x] J3.4 View 持有 `unique_ptr<ViewDataSync> _data_sync`，facade 转发
  - [ ] J3.5 编译验证通过

- [x] Task J4: view.cpp 瘦身验证
  - [x] J4.1 验证 view.cpp 行数 < 800（实际 594 行）
  - [x] J4.2 验证 View 对外 public API 不变（所有方法保留原签名，仅替换为 forwarder）
  - [x] J4.3 cmake/gui_sources.cmake 添加 6 个新文件条目（3 .cpp，与 Phase E 模式一致）

## Phase K: view.h God header 完成治理（P1，与 Phase J 协同）

- [ ] Task K1: view.h inline forwarder 下沉
  - [ ] K1.1 将 37 个 delegate forwarder（`_cursors->xxx()`/`_layout->xxx()`/`_derived->xxx()`）从 view.h 移到 view.cpp（改 out-of-line）
  - [ ] K1.2 view_cursors.h/view_derived_traces.h/view_layout.h 改前置声明
  - [ ] K1.3 验证 view.h #include 数 ≤ 8
  - [ ] K1.4 编译验证通过

- [ ] Task K2: view.h 访问修饰符整理
  - [ ] K2.1 将成员按职责分组到 public/protected/private 三段
  - [ ] K2.2 消除 13 次访问修饰符切换，最终为 3 段
  - [ ] K2.3 编译验证通过

- [ ] Task K3: 内联结构移到独立头文件
  - [ ] K3.1 新建 `signal_group.h`，移入 SignalGroup 结构
  - [ ] K3.2 新建 `filter_snapshot.h`（或下沉 view.cpp），移入 FilterSnapshot 结构
  - [ ] K3.3 view.h include 新头文件
  - [ ] K3.4 编译验证通过

## Phase L: Signal 子类 session 直访消除（P1）

- [x] Task L1: DataSource 接口扩展
  - [x] L1.1 在 datasource.h 新增 `is_running_status()`/`is_stopped_status()` 虚方法
  - [x] L1.2 新增 `cur_snap_samplerate()`/`cur_samplelimits()` 虚方法（已有则确认）
  - [x] L1.3 新增 `get_active_document()`/`get_signal_models()` 虚方法（已有则确认）
  - [x] L1.4 新增 `trigd()`/`trigd_ch()` 虚方法
  - [x] L1.5 SigSession 添加对应 override
  - [ ] L1.6 编译验证通过

- [x] Task L2: 新增 ICaptureControl/IAutoLock 接口
  - [x] L2.1 新建 `icapture_control.h`，声明 ICaptureControl 接口（stop_capture/start_capture/refresh）
  - [x] L2.2 新建 `iauto_lock.h`，声明 IAutoLock 接口（get_data_auto_lock/data_auto_lock）
  - [x] L2.3 SigSession 实现两个接口
  - [ ] L2.4 View 持有 `ICaptureControl* _capture_control`/`IAutoLock* _auto_lock`，注入到 Signal 子类
  - [ ] L2.5 编译验证通过

- [ ] Task L3: AnalogSignal 6 处 session 直访迁移
  - [ ] L3.1 analogsignal.cpp:175 is_stopped_status → _data_source->is_stopped_status()
  - [ ] L3.2 analogsignal.cpp:189/225 cur_snap_samplerate → _data_source->cur_snap_samplerate()
  - [ ] L3.3 analogsignal.cpp:320/482 is_running_status/is_stopped_status → _data_source
  - [ ] L3.4 analogsignal.cpp:573 cur_samplelimits → _data_source->cur_samplelimits()
  - [ ] L3.5 编译验证通过

- [ ] Task L4: DsoSignal 4 处 session 直访迁移
  - [ ] L4.1 dsosignal.cpp:203/204 trigd/trigd_ch → _data_source->trigd()/trigd_ch()
  - [ ] L4.2 dsosignal.cpp:466 is_stopped_status → _data_source->is_stopped_status()
  - [ ] L4.3 dsosignal.cpp:727 broadcast_msg 改由 View 代广播（emit 信号）
  - [ ] L4.4 编译验证通过

- [ ] Task L5: DecodeTrace 5 处 session 直访迁移
  - [ ] L5.1 decodetrace.cpp:181/182/185 get_active_document/cur_snap_samplerate → _data_source
  - [ ] L5.2 decodetrace.cpp:701/728 is_stopped_status → _data_source->is_stopped_status()
  - [ ] L5.3 移除 decodetrace.h 的 `SigSession *_session` 成员（改用 _data_source）
  - [ ] L5.4 编译验证通过

- [ ] Task L6: SpectrumTrace 3 处 session 直访迁移
  - [ ] L6.1 spectrumtrace.cpp:94 get_signal_models → _data_source->get_signal_models()
  - [ ] L6.2 spectrumtrace.cpp:388/389 cur_snap_samplerate → _data_source->cur_snap_samplerate()
  - [ ] L6.3 移除 spectrumtrace.h 的 `SigSession *_session` 成员
  - [ ] L6.4 编译验证通过

- [ ] Task L7: DsoHardwareConfig 12 处 capture 控制迁移
  - [ ] L7.1 dso_hardware_config.cpp:64-141 stop_capture/start_capture/refresh → _capture_control
  - [ ] L7.2 dso_hardware_config.cpp:375 is_running_status → _data_source->is_running_status()
  - [ ] L7.3 编译验证通过

- [ ] Task L8: DsoMeasure 6 处迁移
  - [ ] L8.1 dso_measure.cpp:191/305/391 is_stopped/running_status → _data_source
  - [ ] L8.2 dso_measure.cpp:314/359/392 get_data_auto_lock/data_auto_lock → _auto_lock
  - [ ] L8.3 编译验证通过

- [ ] Task L9: viewstatus/ruler 5 处 device() 迁移
  - [ ] L9.1 viewstatus.cpp 4 处 `_session->device()` → `_data_source->device()`（需 ViewStatus 持有 _data_source）
  - [ ] L9.2 ruler.cpp:236/239 `_view.session()` → `_view.data_source()`
  - [ ] L9.3 编译验证通过

## Phase M: libsigrok.h 违规 include 清理（P1）

- [ ] Task M1: dso_trigger_config.cpp 移除冗余 include
  - [ ] M1.1 移除 dso_trigger_config.cpp:25 `#include <libsigrok.h>`（未使用任何 sr_* 符号）
  - [ ] M1.2 编译验证通过

- [ ] Task M2: dso_measure.h/cpp 移除 libsigrok.h
  - [ ] M2.1 dso_measure.h:32 移除 `#include <libsigrok.h>`，DSO_MEASURE_TYPE 用前置声明或包装枚举
  - [ ] M2.2 dso_measure.cpp:28 移除（若 sr_status 仍需，改用 datasource.h 的前置声明）
  - [ ] M2.3 编译验证通过

- [ ] Task M3: dso_hardware_config.cpp 移除 libsigrok.h
  - [ ] M3.1 sr_channel* 局部变量改用 SignalModel accessor（`_model->sr_channel_handle()` 已有）
  - [ ] M3.2 SR_CONF_* 宏改用 SignalModel setter 转发（已有 set_vdiv/set_coupling 等）
  - [ ] M3.3 移除 dso_hardware_config.cpp:28 `#include <libsigrok.h>`
  - [ ] M3.4 编译验证通过

## Phase N: assert(false) 无 early-return 修复（P0，硬约束）

- [x] Task N1: devmode.cpp 2 处 assert 修复
  - [x] N1.1 devmode.cpp:199 assert(false) 后补 `return;`
  - [x] N1.2 devmode.cpp:247 assert(false) 后补 `return;`
  - [x] N1.3 编译验证通过

- [x] Task N2: dsldial.cpp 1 处 assert 修复
  - [x] N2.1 dsldial.cpp:172 assert(false) 后补 `return _value[0];` 或 `return 0;`（防越界）
  - [x] N2.2 编译验证通过

- [x] Task N3: dso_measure.cpp 2 处 assert 修复
  - [x] N3.1 dso_measure.cpp:271 assert(false) 后补 `return;`（防 NULL deref）
  - [x] N3.2 dso_measure.cpp:275 assert(false) 后补 `return;`（防除零）
  - [x] N3.3 编译验证通过

- [x] Task N4: trace.cpp 2 处 assert 修复
  - [x] N4.1 trace.cpp:133 assert(false) 后补 `return 0;`（防空 list.front()）
  - [x] N4.2 trace.cpp:142 assert(false) 后补 `return;`（防空 list 污染）
  - [x] N4.3 编译验证通过

## Phase O: _scale 直接赋值修复（P0，硬约束）

- [x] Task O1: view.cpp mode_changed 修复
  - [x] O1.1 view.cpp:985 `_scale = WellSamplesPerPixel * 1.0 / samplerate;` 改走 `set_scale_offset(...)`
  - [x] O1.2 view.cpp:987 `_scale = max(min(...));` 改走 `set_scale_offset(...)`
  - [x] O1.3 编译验证通过

- [x] Task O2: view.cpp resizeEvent 修复
  - [x] O2.1 view.cpp:1318 `_scale = _data_source->cur_view_time() / width;` 改走 `set_scale_offset(...)`
  - [x] O2.2 view.cpp:1325 `if (_scale > _maxscale) _scale = _maxscale;` 改走 `set_scale_offset(...)`
  - [x] O2.3 编译验证通过

## Phase P: 集成验证

- [ ] Task P1: 全局编译验证
  - [ ] P1.1 `cd build && ninja -j 16` 编译通过
  - [ ] P1.2 `ninja install` 安装成功
  - [ ] P1.3 `install.dir/bin/PXView.exe` 生成

- [ ] Task P2: 架构约束验证
  - [ ] P2.1 grep `_view->session()` 在 `pv/view/` 目录（Signal 子类）：0 命中
  - [ ] P2.2 grep `#include <libsigrok.h>` 在 `pv/view/` 目录：仅 3 个合法文件
  - [ ] P2.3 grep `assert(false)` 后 5 行无 return：0 命中
  - [ ] P2.4 grep `_scale =` 在 view.cpp（非 mutator 内部）：0 命中
  - [x] P2.5 view.cpp 行数 < 800（实际 594 行）
  - [ ] P2.6 view.h #include 数 ≤ 8
  - [ ] P2.7 view.h 访问修饰符切换次数 = 3
  - [ ] P2.8 grep `session->get_device()`/`_session->get_device()` 在 `pv/view/` 目录：0 命中

- [ ] Task P3: GUI 功能回归验证
  - [ ] P3.1 启动 PXView.exe，验证通道正常显示
  - [ ] P3.2 滚轮缩放波形，验证 scale/offset 正常（_scale mutator 修复）
  - [ ] P3.3 修改 DSO 通道 vdiv/coupling，验证硬件配置同步
  - [ ] P3.4 启用/禁用通道，验证增量更新
  - [ ] P3.5 添加/移除 Decoder，验证 DecodeTrace 正常
  - [ ] P3.6 切换工作模式，验证 mode_changed 走 mutator
  - [ ] P3.7 glitch filter popup，验证 apply/undo 正常
  - [ ] P3.8 切换/关闭 Tab，验证无崩溃

# Task Dependencies

- Task J1/J2/J3 可并行（不同方法集）
- Task K1 依赖 Phase J 完成（forwarder 下沉前需委托类就位）
- Task K2/K3 独立于 K1（访问修饰符整理 + 结构移头文件）
- Task L1/L2 是 L3-L9 的前置（接口先就位）
- Task L3/L4/L5/L6/L7/L8/L9 可并行（不同文件）
- Task M1 独立（仅删冗余 include）
- Task M2/M3 依赖 L8/L7 完成（迁移后再清 libsigrok）
- Task N1-N4 可并行（不同文件）
- Task O1/O2 独立
- Phase P 依赖所有前置 Phase 完成

# Parallelizable Work

- Phase J（view.cpp 瘦身）/Phase N（assert 修复）/Phase O（_scale 修复）— 不同文件，可并行
- Phase L3-L9（Signal 子类迁移）— 不同文件，可并行（依赖 L1/L2 接口就位）
- Phase M1（删冗余 include）独立可并行

# 实施顺序建议

1. Phase N + Phase O（硬约束修复，P0 最高优先级）
2. Phase L1 + L2（接口就位）
3. Phase J（view.cpp 瘦身）
4. Phase L3-L9（Signal 子类迁移，并行）
5. Phase M（libsigrok 清理，依赖 L 完成）
6. Phase K（view.h 治理，依赖 J 完成）
7. Phase P（集成验证）

**注意**：所有 Phase 完成后统一编译（Phase P1），不每完成一个 Task 就编译。
