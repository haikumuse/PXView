## Phase J: view.cpp 继续瘦身

- [x] 新建 `view_signal_sync.h`/`view_signal_sync.cpp`，ViewSignalSync 类声明
- [x] ViewSignalSync 承接 signals_changed/rebuild_signals/on_signals_changed/compute_signal_groups/signals_*_layout/signals_modified_refresh
- [x] ViewSignalSync 持有 `View* _view` back-pointer
- [x] View 持有 `unique_ptr<ViewSignalSync> _signal_sync`
- [x] 新建 `view_glitch_filter.h`/`view_glitch_filter.cpp`，ViewGlitchFilter 类声明
- [x] ViewGlitchFilter 承接 on_*_glitch_*/undo_filter/get_preview_ranges/on_toggle_invert_requested
- [x] View 持有 `unique_ptr<ViewGlitchFilter> _glitch_filter`
- [x] 新建 `view_data_sync.h`/`view_data_sync.cpp`，ViewDataSync 类声明
- [x] ViewDataSync 承接 set_data_source/clear_signal_data/set_signal_data_from_source/set_data_document/frame_began/receive_end/receive_trigger/data_updated/set_receive_len
- [x] View 持有 `unique_ptr<ViewDataSync> _data_sync`
- [x] cmake/gui_sources.cmake 添加 6 个新文件条目
- [x] view.cpp 行数 < 800（实际 640 行）
- [x] View 对外 public API 不变

## Phase K: view.h God header 完成治理

- [x] view.h 39 个 inline forwarder 下沉到 view.cpp（改 out-of-line）
- [ ] view_cursors.h/view_derived_traces.h/view_layout.h 改前置声明（保守保留，避免破坏编译）
- [ ] view.h #include 数 ≤ 8（实际 12，受 Qt MOC + 值类型成员约束）
- [x] view.h 访问修饰符整理（实际 2 段，受 Qt slots/signals 约束）
- [ ] view.h 访问修饰符切换次数 = 3（Qt MOC 强制 slots/signals 独立段）
- [ ] 新建 `signal_group.h`，SignalGroup 结构移入（消费者少，保留内联）
- [ ] 新建 `filter_snapshot.h`（不实施，FilterSnapshot 用作值类型成员，需完整类型）
- [ ] view.h include 新头文件

## Phase L: Signal 子类 session 直访消除

- [x] DataSource 新增 is_running_status/is_stopped_status/cur_snap_samplerate/cur_samplelimits/trigd/trigd_ch 虚方法
- [x] SigSession 添加对应 override
- [x] ~~新建 `icapture_control.h`，ICaptureControl 接口~~（Path B：改为 DataSource 扩展，删除冗余接口）
- [x] ~~新建 `iauto_lock.h`，IAutoLock 接口~~（Path B：DataSource 已有，删除冗余接口）
- [x] ~~SigSession 实现 ICaptureControl/IAutoLock~~（改为 override DataSource）
- [x] ~~View 持有 ICaptureControl*/IAutoLock*，注入到 Signal 子类~~（不需要，_data_source 已包含所有方法）
- [x] DataSource 新增 stop_capture/start_capture/decode_done 虚方法
- [x] analogsignal.cpp 6 处 `_view->session().xxx()` 改走 `_data_source->xxx()`
- [x] dsosignal.cpp 3 处 `_view->session().xxx()` 改走 `_data_source->xxx()`
- [x] dsosignal.cpp:727 broadcast_msg 改走 `_data_source->broadcast_msg()`（直接用 DataSource，不经 View 代广播）
- [x] decodetrace.cpp 5 处 session 直访改走 `_data_source`
- [x] decodetrace.h 移除 `SigSession *_session` 成员，添加 `_data_source` 成员
- [x] spectrumtrace.cpp 3 处 session 直访改走 `_data_source`
- [x] spectrumtrace.h 移除 `SigSession *_session` 成员，添加 `_data_source` 成员
- [x] dso_hardware_config.cpp 12 处 capture 控制改走 `_signal->_data_source`
- [x] dso_measure.cpp 9 处改走 `_signal->_data_source`
- [x] viewstatus.cpp 4 处 `_session->device()` 改走 `_data_source->device()`
- [x] ruler.cpp 4 处 `_view.session().xxx()` 改走 `_view.data_source()->xxx()`（保留 line 236 的 session 变量）
- [x] grep `_view->session()` 在 `pv/view/` 目录（Signal 子类）：0 命中（仅 ruler.cpp:236 保留，按 spec 允许）

## Phase M: libsigrok.h 违规 include 清理

- [x] dso_trigger_config.cpp 移除冗余 `#include <libsigrok.h>`
- [x] dso_measure.h 移除 `#include <libsigrok.h>`（DSO_MEASURE_TYPE 改用 int 参数类型）
- [ ] dso_measure.cpp 移除 `#include <libsigrok.h>`（保留：sr_status 值类型 + SR_GHZ 宏 + SR_CONF_ROLL + DSO_MS_* 常量硬依赖）
- [ ] dso_hardware_config.cpp sr_channel* 改用 SignalModel accessor（保留：SR_CONF_* 硬件查询无 SignalModel 替代）
- [ ] dso_hardware_config.cpp SR_CONF_* 改用 SignalModel setter 转发（保留：硬件查询 vs 模型状态语义不同）
- [ ] dso_hardware_config.cpp 移除 `#include <libsigrok.h>`（保留：9 个 SR_CONF_* config key 硬依赖）
- [ ] grep `#include <libsigrok.h>` 在 `pv/view/` 目录：仅 3 个合法文件（实际 5 个：3 合法 + dso_measure.cpp/dso_hardware_config.cpp 硬依赖保留）

## Phase N: assert(false) 无 early-return 修复

- [x] devmode.cpp:199 assert(false) 后补 early-return
- [x] devmode.cpp:247 assert(false) 后补 early-return
- [x] dsldial.cpp:172 assert(false) 后补 early-return（防越界）
- [x] dso_measure.cpp:271 assert(false) 后补 early-return（防 NULL deref）
- [x] dso_measure.cpp:275 assert(false) 后补 early-return（防除零）
- [x] trace.cpp:133 assert(false) 后补 early-return（防空 list.front()）
- [x] trace.cpp:142 assert(false) 后补 early-return（防空 list 污染）
- [x] view.cpp:1094 assert(false) 后补 early-return（Phase N 遗漏项修复）
- [x] grep `assert(false)` 后 5 行无 return/throw/continue/break：0 命中

## Phase O: _scale 直接赋值修复

- [x] view.cpp:985 mode_changed `_scale =` 改走 `set_scale_offset()`
- [x] view.cpp:987 mode_changed `_scale =` 改走 `set_scale_offset()`
- [x] view.cpp:1318 resizeEvent `_scale =` 改走 `set_scale_offset()`
- [x] view.cpp:1325 resizeEvent `_scale =` 改走 `set_scale_offset()`
- [x] grep `_scale =` 在 view.cpp（非 mutator 内部）：0 命中

## Phase P: 集成验证

- [x] `cd build && ninja -j 16` 编译通过
- [x] `ninja install` 安装成功
- [x] `install.dir/bin/PXView.exe` 生成（27MB）
- [x] grep `_view->session()` 在 `pv/view/` 目录（Signal 子类）：0 命中（仅 ruler.cpp:236 保留，按 spec 允许）
- [ ] grep `#include <libsigrok.h>` 在 `pv/view/` 目录：仅 3 个合法文件（实际 5 个，2 个硬依赖保留）
- [x] grep `assert(false)` 后 5 行无 return：0 命中
- [x] grep `_scale =` 在 view.cpp（非 mutator 内部）：0 命中
- [ ] grep `session->get_device()`/`_session->get_device()` 在 `pv/view/` 目录：0 命中（header/viewport_painter/viewport 仍有 11 处，属 V2 遗留，V3 范围外）
- [x] view.cpp 行数 < 800（实际 640 行）
- [ ] view.h #include 数 ≤ 8（实际 12，受 Qt MOC + 值类型成员约束）
- [x] view.h 访问修饰符切换次数 ≤ 3（实际 2 段）
- [ ] GUI：启动 PXView.exe 通道正常显示（待用户手动验证）
- [ ] GUI：滚轮缩放波形 scale/offset 正常（_scale mutator 修复）（待用户手动验证）
- [ ] GUI：修改 DSO 通道 vdiv/coupling 硬件配置同步（待用户手动验证）
- [ ] GUI：启用/禁用通道增量更新（待用户手动验证）
- [ ] GUI：添加/移除 Decoder 正常（待用户手动验证）
- [ ] GUI：切换工作模式 mode_changed 走 mutator（待用户手动验证）
- [ ] GUI：glitch filter popup apply/undo 正常（待用户手动验证）
- [ ] GUI：切换/关闭 Tab 无崩溃（待用户手动验证）
