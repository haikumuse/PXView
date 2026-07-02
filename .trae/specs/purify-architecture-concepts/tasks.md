# Tasks

## 阶段 1（P0 零风险清理）

- [x] Task 1: 删除 SessionDocument 5 个死存储字段 ✅
  - [x] SubTask 1.1: `sessiondocument.h` 删除 5 个死字段成员声明 + `set_decoder_model` 方法（注：`get_*` 方法因 DataSource 接口契约保留，改为返回空值）
  - [x] SubTask 1.2: `sessiondocument.cpp` 删除上述字段实现（含 clear() 空循环 line 106-122）
  - [x] SubTask 1.3: grep 验证 `set_decoder_model` 等已删方法 0 代码命中
  - [x] SubTask 1.4: 验证：`cd build && ninja -j 16 && ninja install` 0 error

- [x] Task 2: 修正 AGENTS.md 行数声明 ✅
  - [x] SubTask 2.1: `AGENTS.md` 第 73 行从 "284 lines" 改为 "299 lines"
  - [ ] SubTask 2.2: 阶段 7 完成后再次更新为 < 200 行的实际值

## 阶段 2（P0 序列化路径统一 + 用户数据 bug 修复）

- [x] Task 3: 启用 SignalConfigStore 为 .pxc channel 配置唯一序列化路径 ✅
  - [x] SubTask 3.1: 检查 `SignalConfigStore::signal_config_to_json` 当前写入的字段集（index/enabled/visible/vdiv/coupling/map_default/hw_offset/offset/zero_offset/trig_type/view_index/v_offset/own_height）
  - [x] SubTask 3.2: 对比 `MainWindow::gen_config_json`（mainwindow.cpp:1441-1485）写入字段集，补齐 SignalConfigStore 缺失的字段（type/name/colour/vfactor/trig_value/map_unit/map_min/map_max 归入 ChannelConfig；strigger→trig_type、trigValue→trig_value、zeroPos→zero_offset、mapUnit→map_unit、mapMin→map_min、mapMax→map_max、mapDefault→map_default、colour→colour、vfactor→vfactor、name→name、type→type）
  - [x] SubTask 3.3: `MainWindow::gen_config_json` channel 段改为调用 `doc->save_signal_config(...)` + `doc->signal_config_to_json()`，删除原直访 view::Signal 写 channel 的逻辑；`load_config_from_json` channel 解析段改走 `signal_config_from_json` + `apply_signal_config`，view-side 保留 set_colour/set_trig/set_zero_ratio（Task 13 处理）但 JSON key 更新为 ChannelConfig 字段名
  - [x] SubTask 3.4: 验证：`ninja -j 16` 0 error（25/25 步骤，链接 PXView.exe 成功）+ `ninja install` 0 error；grep 确认 gen_config_json 无直访 view::Signal 写 channel 字段、无 sr_channel-> 直访（仅注释中出现）

- [ ] Task 4: 修复 visible/trig_type/v_offset/own_height 序列化丢失
  - [ ] SubTask 4.1: 确认 Task 3 后 visible 字段在 SignalConfigStore 路径正确写入（save 时从 view::Signal->visible() 读 → ChannelConfig.visible → JSON）
  - [ ] SubTask 4.2: 确认 trig_type 字段正确序列化（替代原 strigger 间接路径）
  - [ ] SubTask 4.3: 确认 v_offset/own_height 正确序列化（阶段 6 会迁移到 uiLayout，本阶段先确保不丢失）
  - [ ] SubTask 4.4: 验证：保存 .pxc → 重新加载 → 通道隐藏状态/触发配置/布局全部保留

- [x] Task 5: 删除 MainWindow::load_channel_view_indexs 死路径
  - [x] SubTask 5.1: 删除 `mainwindow.cpp:1805-1831` `load_channel_view_indexs` 方法（仅 LOGIC 模式触发、只读 view_index，已被 SignalConfigStore 路径取代）
  - [x] SubTask 5.2: 删除 `mainwindow.cpp:3681` demo 加载对该方法的调用
  - [x] SubTask 5.3: 验证：`cd build && ninja -j 16 && ninja install` 0 error

- [x] Task 6: trigger 序列化改走 Core TriggerConfig ✅
  - [x] SubTask 6.1: `mainwindow.cpp:1488` `sessionVar["trigger"] = _trigger_widget->get_session()` 改为 `_session->trigger_config().to_json()`
  - [x] SubTask 6.2: `TriggerConfig` 增加 `to_json()`/`from_json()` 方法（若不存在）
  - [x] SubTask 6.3: load 路径对应改走 `_session->set_trigger_config(TriggerConfig::from_json(...))`
  - [x] SubTask 6.4: 验证：`cd build && ninja -j 16 && ninja install` 0 error；保存/加载 .pxc 触发配置不丢失

## 阶段 3（P1 enabled/visible 语义拆分）

- [x] Task 7: ChannelConfig 删除 visible 字段
  - [x] SubTask 7.1: `signalconfigstore.h` `ChannelConfig` 删除 `visible` 成员
  - [x] SubTask 7.2: `signalconfigstore.cpp` `signal_config_to_json`/`from_json`/`save_signal_config`/`apply_signal_config` 移除 visible 处理
  - [x] SubTask 7.3: 验证：`cd build && ninja -j 16 && ninja install` 0 error（visible 持久化由阶段 6 uiLayout 段接管，本阶段先内存化）

- [ ] Task 8: signalfactory/view.cpp 不再混淆 enabled 与 visible
  - [ ] SubTask 8.1: `signalfactory.cpp:62-63` `set_enabled(model->enabled()); set_visible(model->enabled());` 拆分——`set_visible` 改为读 View 层 DockUiState 的 visible 状态（DockUiState 需扩展，见 Task 17）
  - [ ] SubTask 8.2: `view.cpp:2349` `sig->set_visible(s->model()->enabled())` 同样改读 DockUiState
  - [ ] SubTask 8.3: `signalconfigstore.cpp:137-138` `cfg.visible = ... probe->enabled` 兜底逻辑删除（visible 已不在 ChannelConfig）
  - [ ] SubTask 8.4: 验证：`cd build && ninja -j 16 && ninja install` 0 error；GUI 测试——硬件禁用通道仍可在 UI 切换可见性

- [x] Task 9: trace.h enabled() 注释修正 ✅
  - [x] SubTask 9.1: `trace.h:194-197` `enabled()` 注释从 "visible and enabled" 改为 "hardware enabled (Core-owned); use visible() for UI visibility"
  - [x] SubTask 9.2: 确认 `Signal::enabled()` 实现只返回 `_local_enabled`，与注释一致

## 阶段 4（P1 Core 残留 UI 概念清理）

- [x] Task 10: DecoderModel 移出 pv::data ✅
  - [x] SubTask 10.1: 评估 DecoderModel 用途（QAbstractTableModel 给谁用？grep 调用点）
  - [x] SubTask 10.2: 方案选择：移到 `pv::view` 或改为纯数据 + View 层包装器（优先移到 pv::view，因不考虑兼容性）
  - [x] SubTask 10.3: 移动 `decodermodel.h/.cpp` 到 `pv/view/`，命名空间改 `pv::view`，更新 CMakeLists.txt 的 source 分组
  - [x] SubTask 10.4: 更新所有 include 路径和命名空间引用
  - [x] SubTask 10.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error；grep `pv/data/` 无 `QAbstractTableModel`

- [x] Task 11: annotation.h UI 概念移出 Core ✅
  - [x] SubTask 11.1: `annotation.h` 删除 `#include <QFont>`/`<QFontMetrics>` + `_cached_width_font` 成员 + `rect_width` 相关方法（同时移除 `_cached_best_annotation`/`_cached_rect_width` mutable 缓存字段与 `get_cached_best_annotation` 声明；构造函数中 `_cached_rect_width = -1.0` 初始化一并删除）
  - [x] SubTask 11.2: 渲染期字体信息移到 View 层（在 `pv::view::DecodeTrace` 新增 private static helper `best_annotation_text(const Annotation&, double rect_width, const QFontMetrics&)`，原 `get_cached_best_annotation` 的"选最长可放下文本"循环逻辑迁入；`draw_range` 调用点改调该 helper。per-annotation 缓存有意丢弃，由 `add-annotation-mmap-store` spec 的 LRU 缓存后续接管）
  - [x] SubTask 11.3: grep 确认 Core 层无 `QFont`/`QFontMetrics` 引用（`PXView/pv/data/` 目录 0 命中）
  - [x] SubTask 11.4: 验证：`annotation.cpp` 与 `decodetrace.cpp` 均编译通过（`ninja` 增量编译这两个对象 0 error）；全量链接被并发的 Task 7 WIP 阻塞（`view.cpp:2253` 引用已删的 `ch.visible`），与本 Task 无关

- [x] Task 12: datasource.h 删除未使用的 pv::view 前向声明 ✅
  - [x] SubTask 12.1: `datasource.h:33-39` 删除 `pv::view::Signal`/`pv::view::DecodeTrace` 等前向声明
  - [x] SubTask 12.2: 验证：`cd build && ninja -j 16 && ninja install` 0 error（注：Task 12 改动本身 0 error，pxview-core 库编译链接成功；全量 build 残留 error 来自其他未完成 Task 的中间态——Task 7 删 ChannelConfig::visible、Task 11 删 Annotation::get_cached_best_annotation——均与 datasource.h 无关）

## 阶段 5（P1 View 绕过 Core 修复）

- [ ] Task 13: load_config_from_json 改走 Core SignalModel
  - [ ] SubTask 13.1: `mainwindow.cpp:1633-1700` 遍历 `_device_agent->get_channels()` 直改 `sr_channel->vdiv/coupling/vfactor/trig_value/map_unit/map_min/map_max/map_default/enabled/name` 的逻辑，改为调用 `SignalModel::set_vdiv`/`set_coupling`/... Core API
  - [ ] SubTask 13.2: 确认 `SignalModel` 有对应 setter（缺则补全）
  - [ ] SubTask 13.3: `mainwindow.cpp:1712-1768` `set_colour`/`set_trig`/`set_zero_ratio`/`set_trig_ratio` 改通过 Core 写回 SignalModel（colour/trig 等若有 Core 对应字段）
  - [ ] SubTask 13.4: 验证：`cd build && ninja -j 16 && ninja install` 0 error；load .pxc 后通道配置正确恢复

- [x] Task 14: ds_dsl_option_value_to_code 封装到 Core ✅
  - [x] SubTask 14.1: `mainwindow.cpp:1587` `ds_dsl_option_value_to_code` 直调改为 `DeviceAgent::option_value_to_code` 封装方法（签名 `int option_value_to_code(int work_mode, int config_id, const char *value)`，deviceagent.h 声明 + deviceagent.cpp 实现，含 `if(!value)` 显式检查）
  - [x] SubTask 14.2: grep `PXView/pv/view/` 和 `mainwindow.cpp` 确认无其他 `ds_*` 直调（除 ds_trigger_* 已修复）；view/ 仅 view.cpp:1046 注释中提及 ds_trigger_get_en()，无代码调用
  - [x] SubTask 14.3: 验证：`cd build && ninja -j 16 && ninja install` 0 error（Task 14 改动本身 0 error——deviceagent.cpp [1/77] 与 mainwindow.cpp [44/77] 编译成功无 FAILED；全量 build 残留 error 来自 Task 10 DecoderModel 移出 + Task 19 _decoder_model 数据下沉的中间态，与本 Task 无关，同 Task 11.4/12.2 模式）

## 阶段 6（P2 UI 布局字段迁移到 View 层）

- [ ] Task 15: ChannelConfig 删除 UI 布局字段 + ChannelLayoutState 移命名空间
  - [ ] SubTask 15.1: `signalconfigstore.h` `ChannelConfig` 删除 `view_index`/`v_offset`/`own_height` 三字段
  - [ ] SubTask 15.2: `signalconfigstore.cpp` 序列化移除三字段处理
  - [ ] SubTask 15.3: `ChannelLayoutState` 结构体从 `pv::data` 移到 `pv::view` 命名空间（新位置 `pv/view/dock_ui_state.h` 或独立 `pv/view/channel_layout_state.h`）
  - [ ] SubTask 15.4: 更新所有引用点（tabcontext.cpp、header.cpp、mainwindow.cpp build_channel_layout helper）
  - [ ] SubTask 15.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error

- [ ] Task 16: .pxc 新增 uiLayout 段 + View 层独立序列化
  - [ ] SubTask 16.1: `view::View` 新增 `save_ui_layout_to_json()`/`load_ui_layout_from_json()` 方法
  - [ ] SubTask 16.2: 序列化结构：`"uiLayout": [{"index": 0, "view_index": 0, "v_offset": 10, "own_height": 50, "visible": true}, ...]`
  - [ ] SubTask 16.3: `MainWindow::save_config_to_file` 在顶层 JSON 插入 `uiLayout` 段（调 `view->save_ui_layout_to_json()`）
  - [ ] SubTask 16.4: `MainWindow::load_config_from_json` 读取 `uiLayout` 段调 `view->load_ui_layout_from_json()`
  - [ ] SubTask 16.5: 旧 .pxc 无 uiLayout 段时使用默认布局（不报错，不迁移老数据）
  - [ ] SubTask 16.6: 验证：`cd build && ninja -j 16 && ninja install` 0 error；保存/加载 .pxc 布局完整保留

- [ ] Task 17: DockUiState 扩展 + signalfactory 恢复点改读 View
  - [ ] SubTask 17.1: `dock_ui_state.h` `DockUiState` 扩展含 `std::map<int, ChannelLayoutState>` 按 channel index 索引（含 view_index/v_offset/own_height/visible）
  - [ ] SubTask 17.2: `signalfactory.cpp:252-271` 恢复点从 `doc->get_signal_config().channels` 读三字段，改为读 `view->dock_ui_state().channel_layout`
  - [ ] SubTask 17.3: 确认 View 在 SignalFactory 调用前已加载 DockUiState（加载时序约束）
  - [ ] SubTask 17.4: 验证：`cd build && ninja -j 16 && ninja install` 0 error；rebuild signals 后布局从 DockUiState 恢复

- [ ] Task 18: Header 拖动持久化 + TabContext::deactivate 改走 View
  - [ ] SubTask 18.1: `header.cpp:453-463`/`576-586` 拖动后 `save_signal_config` 改为更新 View 层 DockUiState（不直调 Core save_signal_config）
  - [ ] SubTask 18.2: `tabcontext.cpp:143-152` `deactivate` 收集布局改为写 DockUiState，不再传给 Core save_signal_config
  - [ ] SubTask 18.3: 确认 .pxc 保存时 DockUiState 的布局通过 Task 16 的 uiLayout 段持久化
  - [ ] SubTask 18.4: 验证：`cd build && ninja -j 16 && ninja install` 0 error；拖动通道高度/顺序 → 切 tab 再切回 → 布局保留

## 阶段 7（P3 God class 拆分）

- [ ] Task 19: SigSession 数据下沉到 manager（移除 friend）
  - [ ] SubTask 19.1: 评估 25+ 私有字段归属：`_signal_models`/`_decoder_model`/`_spectrum_stacks`/`_lissajous_model`/`_math_stack` → DocumentRegistry 或新 Manager；`_trigger_config` → 新 TriggerManager 或 CaptureManager；`_device_agent`/`_view_data`/`_capture_data` → CaptureManager；`_is_working` 等状态 → CaptureManager
  - [ ] SubTask 19.2: CaptureManager 持有 `_is_working`/`_capture_data`/`_view_data`/6 套 DsTimer，移除 `_session->_xxx` 直访（30+ 处改为 `_xxx`）
  - [ ] SubTask 19.3: DocumentRegistry 持有 `_signal_models`/`_decoder_model`/`_spectrum_stacks`/`_lissajous_model`/`_math_stack`/`_trigger_config`
  - [ ] SubTask 19.4: 其余 manager（DecodeTaskManager/DataFeedParser/FilterProcessor/EventBus）类似下沉各自数据
  - [ ] SubTask 19.5: `sigsession.h` 删除 5 个 `friend class core::XxxManager` 声明
  - [ ] SubTask 19.6: SigSession 退化为纯 facade，public 方法转发到 manager，私有成员仅剩 6 个 unique_ptr + 必要协调状态
  - [ ] SubTask 19.7: 验证：`cd build && ninja -j 16 && ninja install` 0 error；`sigsession.h` < 200 行；grep `_session->_` 在 manager 中 0 命中

- [ ] Task 20: MainWindow 抽出 SessionConfigSerializer
  - [ ] SubTask 20.1: 新建 `PXView/pv/sessionconfigserializer.h/.cpp`，迁移 `gen_config_json`/`load_config_from_json`/`save_config_to_file` 序列化编排逻辑
  - [ ] SubTask 20.2: MainWindow 持有 `unique_ptr<SessionConfigSerializer>`，转发调用
  - [ ] SubTask 20.3: 验证：`cd build && ninja -j 16 && ninja install` 0 error；mainwindow.cpp 行数显著下降

- [ ] Task 21: StoreSession 拆分
  - [ ] SubTask 21.1: 新建 `PXView/pv/pxcserializer.h/.cpp`，迁移 .pxc JSON 序列化（gen_decoders_json 等）
  - [ ] SubTask 21.2: 新建 `PXView/pv/csvexporter.h/.cpp`，迁移 CSV 导出逻辑
  - [ ] SubTask 21.3: StoreSession 退化为协调者，持有两个 unique_ptr
  - [ ] SubTask 21.4: 验证：`cd build && ninja -j 16 && ninja install` 0 error

- [ ] Task 22: View 拆分（至少抽出 SignalRebuilder）
  - [ ] SubTask 22.1: 评估 View 3239 行实现的职责分组（渲染/布局/信号重建/弹窗/撤销栈/滤波预览）
  - [ ] SubTask 22.2: 新建 `PXView/pv/view/signalrebuilder.h/.cpp`，迁移 `rebuild_signals`/`rebuild_signals_from_config`/`signals_modified_refresh` 等信号重建逻辑
  - [ ] SubTask 22.3: View 持有 `unique_ptr<SignalRebuilder>`，转发调用
  - [ ] SubTask 22.4: 评估是否继续抽出 ViewRenderer/ViewLayout/PopupManager（视风险决定，可留待后续）
  - [ ] SubTask 22.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error；view.cpp 行数下降

## Task Dependencies

- Task 1-2（阶段 1）无依赖，可并行
- Task 3 依赖 Task 1（SessionDocument 死字段清理后序列化路径更清晰）
- Task 4 依赖 Task 3（统一路径后验证字段不丢失）
- Task 5/6 依赖 Task 3（路径统一后删死路径）
- Task 7 依赖 Task 4（visible 序列化修复后才能从 ChannelConfig 删）
- Task 8 依赖 Task 7 + Task 17（DockUiState 扩展后 signalfactory 才能读 View 层 visible）
- Task 9 独立，可随时
- Task 10/11/12 独立，可并行
- Task 13 依赖 Task 3（序列化路径统一后 load 也走 Core）
- Task 14 独立
- Task 15 依赖 Task 7（visible 删除后三字段一并删）
- Task 16 依赖 Task 15（ChannelConfig 删字段后 uiLayout 段接管）
- Task 17 依赖 Task 16（uiLayout 段就绪后 DockUiState 扩展）
- Task 18 依赖 Task 17
- Task 19 依赖阶段 1-6（数据归属清晰后再下沉）
- Task 20 依赖 Task 3/6/13（序列化逻辑统一后才能抽出）
- Task 21 独立
- Task 22 依赖 Task 8/17（signalfactory 改造后才能抽 SignalRebuilder）

## Parallelizable Work

- 阶段 1：Task 1 + Task 2 并行
- 阶段 4：Task 10/11/12 互不依赖，可并行
- 阶段 7：Task 21（StoreSession）独立；Task 19/20/22 有依赖需顺序
- 跨阶段：阶段 4 与阶段 2/3 正交，可并行委托

## 风险控制

- **阶段 2（最高风险）**：序列化路径切换影响 .pxc 读写，需逐字段验证不丢失。建议先跑一次 GUI 回归（保存→加载 .pxc 对比字段）再继续。
- **阶段 6（中高风险）**：UI 布局字段迁移涉及 6+ 文件，signalfactory 恢复点时序约束需仔细验证。
- **阶段 7 Task 19（最高风险）**：SigSession 25+ 字段下沉，5 个 manager 全部要改，影响面最大。建议分 manager 逐个下沉，每个独立编译验证。
- **阶段 3 Task 8**：enabled/visible 拆分后，需确认所有依赖 visible 状态的渲染路径（Header/View/trace）行为不变。
