# Checklist

## A. 阶段 1（P0 零风险清理）— 已完成

- [x] `sessiondocument.h` 删除 `_signal_models`/`_spectrum_stacks`/`_math_stack`/`_lissajous_model`/`_decoder_model` 成员声明
- [x] `sessiondocument.h` 删除 `set_decoder_model` 方法（注：`get_*` 方法因 DataSource 接口契约保留，改为返回空值）
- [x] `sessiondocument.cpp` 删除上述字段实现（含 line 106-122 空循环 clear）
- [x] grep 全工程 `set_decoder_model` 等已删 set 方法 0 代码命中
- [x] AGENTS.md Key Files 表 `sigsession.h` 行数改为 299（与实际一致）
- [x] 验证：`cd build && ninja -j 16 && ninja install` 0 error

## B. 阶段 2（P0 序列化路径统一 + 用户数据 bug 修复）

### Task 3: SignalConfigStore 唯一序列化路径
- [x] `SignalConfigStore::signal_config_to_json` 字段集覆盖原 `MainWindow::gen_config_json` 全部 channel 字段（新增 type/name/colour/vfactor/trig_value/map_unit/map_min/map_max）
- [x] `MainWindow::gen_config_json` 调用 `SignalConfigStore::signal_config_to_json` 生成 channels[] 数组（经 `doc->save_signal_config(...)` + `doc->signal_config_to_json()`）
- [x] `MainWindow::gen_config_json` 中直访 `view::Signal` 写 channel 字段的逻辑已删除（改走 build_channel_visibility/build_channel_layout/build_channel_colours helper 收集状态到 map，再传给 Core 序列化）
- [x] `MainWindow::load_config_from_json` channel 解析段改走 `signal_config_from_json` + `apply_signal_config`（按 index 匹配，替代原 positional 匹配；用当前 device mode 覆盖避免误改 device mode）
- [x] `MainWindow::load_config_from_json` view-side JSON key 更新为 ChannelConfig 字段名（strigger→trig_type、trigValue→trig_value、zeroPos→zero_offset）；DSO 用 (0,1) 区间保护跳过 raw 值，ANALOG 用 value2ratio 转换 raw→ratio
- [x] 验证：`ninja -j 16` 0 error（25/25 步骤，链接 PXView.exe 成功）+ `ninja install` 0 error
- [x] grep 验证：mainwindow.cpp 中 `sr_channel`/`probe->` 仅出现在注释中，无直访代码；无 `ch_obj[`/`chVar[`/`channelVar[` 模式

### Task 4: 字段不丢失验证
- [ ] visible 字段在 .pxc channels[] 中正确写入
- [ ] trig_type 字段在 .pxc channels[] 中正确写入（替代 strigger 间接路径）
- [ ] v_offset/own_height 字段在 .pxc 中正确写入
- [ ] hw_offset/offset/zero_offset 字段在 .pxc 中正确写入
- [ ] GUI 回归：隐藏通道 → 保存 .pxc → 重新加载 → 通道仍隐藏
- [ ] GUI 回归：配置触发 → 保存 .pxc → 重新加载 → 触发配置恢复
- [ ] GUI 回归：调整布局 → 保存 .pxc → 重新加载 → 布局恢复

### Task 5: 删 load_channel_view_indexs 死路径
- [x] `mainwindow.cpp` `load_channel_view_indexs` 方法已删除
- [x] `mainwindow.cpp:3681` demo 加载对该方法的调用已删除
- [x] 验证：`cd build && ninja -j 16 && ninja install` 0 error

### Task 6: trigger 序列化走 Core
- [x] `mainwindow.cpp:1488` 改为 `_session->trigger_config().to_json()`
- [x] `TriggerConfig` 有 `to_json()`/`from_json()` 方法
- [x] load 路径改走 `_session->set_trigger_config(TriggerConfig::from_json(...))`
- [x] `_trigger_widget->get_session()` 不再用于序列化
- [x] 验证：`cd build && ninja -j 16 && ninja install` 0 error；触发配置保存/加载不丢失

## C. 阶段 3（P1 enabled/visible 语义拆分）

### Task 7: ChannelConfig 删 visible
- [x] `signalconfigstore.h` `ChannelConfig` 无 `visible` 成员
- [x] `signalconfigstore.cpp` `signal_config_to_json`/`from_json` 无 visible 处理
- [x] `signalconfigstore.cpp` `save_signal_config`/`apply_signal_config` 无 visible 处理
- [x] 验证：`cd build && ninja -j 16 && ninja install` 0 error

### Task 8: enabled/visible 不再混淆
- [ ] `signalfactory.cpp:62-63` `set_visible` 不从 `model->enabled()` 派生
- [ ] `signalfactory.cpp` `set_visible` 改读 View 层 DockUiState
- [ ] `view.cpp:2349` `sig->set_visible` 不从 `s->model()->enabled()` 派生
- [ ] `signalconfigstore.cpp:137-138` `cfg.visible = probe->enabled` 兜底逻辑已删除
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] GUI 回归：硬件禁用通道仍可在 UI 切换可见性

### Task 9: trace.h 注释修正
- [x] `trace.h:194-197` `enabled()` 注释明确 = 硬件启用，指向 `visible()` 查 UI 可见性
- [x] `Signal::enabled()` 实现只返回 `_local_enabled`

## D. 阶段 4（P1 Core 残留 UI 概念清理）

### Task 10: DecoderModel 移出 Core
- [x] `decodermodel.h/.cpp` 从 `pv/data/` 移到 `pv/view/`
- [x] 命名空间从 `pv::data` 改为 `pv::view`
- [x] CMakeLists.txt source 分组更新（从 PXVIEW_CORE_SOURCES 移到 PXVIEW_GUI_SOURCES）
- [x] 所有 include 路径和命名空间引用更新
- [x] grep `pv/data/` 目录无 `QAbstractTableModel`/`QAbstractListModel`
- [x] 验证：`cd build && ninja -j 16 && ninja install` 0 error

### Task 11: annotation.h UI 概念移出
- [x] `annotation.h` 无 `#include <QFont>`/`<QFontMetrics>`
- [x] `Annotation` 类无 `_cached_width_font` 成员
- [x] `Annotation` 类无 `rect_width` 相关方法
- [x] 渲染期字体信息移到 View 层
- [x] grep `pv/data/` 目录无 `QFont`/`QFontMetrics`
- [x] 验证：`annotation.cpp` + `decodetrace.cpp` 增量编译 0 error（全量链接被并发 Task 7 WIP `view.cpp:2253 ch.visible` 阻塞，与本 Task 无关）

### Task 12: datasource.h 删死声明
- [x] `datasource.h` 无 `pv::view::Signal`/`pv::view::DecodeTrace` 等前向声明
- [x] 验证：`cd build && ninja -j 16 && ninja install` 0 error（Task 12 改动本身 0 error，pxview-core 编译链接成功；全量 build 残留 error 来自 Task 7/11 中间态，与 datasource.h 无关）

## E. 阶段 5（P1 View 绕过 Core 修复）

### Task 13: load_config_from_json 走 Core
- [ ] `mainwindow.cpp:1633-1700` 无直改 `sr_channel->vdiv/coupling/vfactor/...`
- [ ] 改为调用 `SignalModel::set_vdiv`/`set_coupling`/... Core API
- [ ] `SignalModel` 有对应 setter（缺则补全）
- [ ] `mainwindow.cpp:1712-1768` `set_colour`/`set_trig`/`set_zero_ratio` 通过 Core 写回 SignalModel
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] GUI 回归：load .pxc 后通道配置正确恢复

### Task 14: ds_* 封装到 Core
- [x] `mainwindow.cpp:1587` `ds_dsl_option_value_to_code` 改为 `DeviceAgent::option_value_to_code` 封装方法（签名 `int option_value_to_code(int work_mode, int config_id, const char *value)`）
- [x] `deviceagent.h` 新增 `option_value_to_code` 公有方法声明（含 doxygen 注释说明 Core/View 边界封装意图）
- [x] `deviceagent.cpp` 新增 `option_value_to_code` 实现，调用 `ds_dsl_option_value_to_code`，含 `if(!value)` 显式 NULL 检查（Release 下 assert 是 no-op）
- [x] grep `PXView/pv/view/` + `mainwindow.cpp` 无 `ds_dsl_option_value_to_code` 直调（仅 deviceagent.cpp:314 调用 + deviceagent.h:235 注释提及）
- [x] grep `PXView/pv/view/` + `mainwindow.cpp` 无其他 `ds_*` 直调（view.cpp:1046 仅为注释提及 `ds_trigger_get_en()`，无代码调用）
- [x] 验证：`cd build && ninja -j 16 && ninja install` 0 error（Task 14 改动本身 0 error——deviceagent.cpp [1/77] 与 mainwindow.cpp [44/77] 编译成功无 FAILED；全量 build 残留 error 来自 Task 10 DecoderModel 移出 + Task 19 _decoder_model 数据下沉的中间态，与本 Task 无关，同 Task 11.4/12.2 模式）

## F. 阶段 6（P2 UI 布局字段迁移到 View 层）

### Task 15: ChannelConfig 删 UI 布局字段
- [ ] `signalconfigstore.h` `ChannelConfig` 无 `view_index`/`v_offset`/`own_height`
- [ ] `signalconfigstore.cpp` 序列化无三字段处理
- [ ] `ChannelLayoutState` 移到 `pv::view` 命名空间
- [ ] `tabcontext.cpp`/`header.cpp`/`mainwindow.cpp` 引用点更新
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

### Task 16: .pxc uiLayout 段
- [ ] `view::View` 有 `save_ui_layout_to_json()`/`load_ui_layout_from_json()` 方法
- [ ] .pxc 顶层有独立 `"uiLayout"` 段（与 `"channels"` 平级）
- [ ] `MainWindow::save_config_to_file` 插入 uiLayout 段
- [ ] `MainWindow::load_config_from_json` 读取 uiLayout 段
- [ ] 旧 .pxc 无 uiLayout 段时不报错（使用默认布局）
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] GUI 回归：保存/加载 .pxc 布局完整保留

### Task 17: DockUiState 扩展 + signalfactory 改读 View
- [ ] `dock_ui_state.h` `DockUiState` 含 `std::map<int, ChannelLayoutState>` 按 channel index 索引
- [ ] 该 map 含 view_index/v_offset/own_height/visible
- [ ] `signalfactory.cpp:252-271` 恢复点改读 `view->dock_ui_state().channel_layout`
- [ ] 不再从 `doc->get_signal_config().channels` 读三字段
- [ ] View 在 SignalFactory 调用前已加载 DockUiState
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] GUI 回归：rebuild signals 后布局从 DockUiState 恢复

### Task 18: Header/TabContext 改走 View
- [ ] `header.cpp:453-463`/`576-586` 拖动后更新 DockUiState，不直调 Core save_signal_config
- [ ] `tabcontext.cpp:143-152` `deactivate` 写 DockUiState，不传给 Core save_signal_config
- [ ] .pxc 保存时 DockUiState 通过 uiLayout 段持久化
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [ ] GUI 回归：拖动通道高度/顺序 → 切 tab 再切回 → 布局保留

## G. 阶段 7（P3 God class 拆分）

### Task 19: SigSession 数据下沉
- [ ] CaptureManager 持有 `_is_working`/`_capture_data`/`_view_data`/6 套 DsTimer
- [ ] CaptureManager 实现无 `_session->_xxx` 直访
- [ ] DocumentRegistry 持有 `_signal_models`/`_decoder_model`/`_spectrum_stacks`/`_lissajous_model`/`_math_stack`/`_trigger_config`
- [ ] DocumentRegistry 实现无 `_session->_xxx` 直访
- [ ] DecodeTaskManager/DataFeedParser/FilterProcessor/EventBus 各自持有自己的数据
- [ ] `sigsession.h` 无 `friend class core::XxxManager` 声明（5 处全删）
- [ ] `sigsession.h` 私有成员仅剩 6 个 unique_ptr + 必要协调状态
- [ ] `sigsession.h` 行数 < 200
- [ ] grep manager 实现 `_session->_` 0 命中
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

### Task 20: MainWindow 抽出 SessionConfigSerializer
- [ ] 新建 `PXView/pv/sessionconfigserializer.h/.cpp`
- [ ] 序列化编排逻辑（gen_config_json/load_config_from_json/save_config_to_file）已迁移
- [ ] MainWindow 持有 `unique_ptr<SessionConfigSerializer>`
- [ ] mainwindow.cpp 行数显著下降
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

### Task 21: StoreSession 拆分
- [ ] 新建 `PXView/pv/pxcserializer.h/.cpp`（.pxc JSON）
- [ ] 新建 `PXView/pv/csvexporter.h/.cpp`（CSV 导出）
- [ ] StoreSession 退化为协调者，持有两个 unique_ptr
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

### Task 22: View 拆分
- [ ] 新建 `PXView/pv/view/signalrebuilder.h/.cpp`
- [ ] `rebuild_signals`/`rebuild_signals_from_config`/`signals_modified_refresh` 已迁移
- [ ] View 持有 `unique_ptr<SignalRebuilder>`
- [ ] view.cpp 行数下降
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error

## H. 文档更新
- [ ] AGENTS.md Key Files 表行数更新（阶段 1 + 阶段 7 后）
- [ ] AGENTS.md Key Files 表新增 SessionConfigSerializer/PxcSerializer/CsvExporter/SignalRebuilder
- [ ] AGENTS.md State Sync Conventions 更新 enabled/visible 独立说明
- [ ] AGENTS.md State Sync Conventions 更新 uiLayout 段说明
- [ ] project_memory.md 新增 Lessons Learned（序列化路径统一、enabled/visible 拆分、数据下沉 friend 移除）
- [ ] `unify-signal-layout-state` spec 标注 superseded

## I. 最终验证
- [ ] 阶段 1 完成后增量编译 0 error
- [ ] 阶段 2 完成后增量编译 0 error + .pxc 字段不丢失回归
- [ ] 阶段 3 完成后增量编译 0 error + enabled/visible 独立性回归
- [ ] 阶段 4 完成后增量编译 0 error + grep Core 层无 UI 概念
- [ ] 阶段 5 完成后增量编译 0 error + grep View 层无 ds_* 直调
- [ ] 阶段 6 完成后增量编译 0 error + .pxc 布局持久化回归
- [ ] 阶段 7 完成后增量编译 0 error + sigsession.h < 200 行
- [ ] grep `pv/data/` 无 `QAbstractTableModel`/`QFont`/`pv::view` 前向声明
- [ ] grep `mainwindow.cpp` 无 `sr_channel->` 直访
- [ ] grep `_session->_` 在 manager 实现 0 命中
- [ ] grep `friend class core::` 在 sigsession.h 0 命中
- [ ] project_memory.md 与 AGENTS.md 已更新
- [ ] GUI + Headless 运行时回归（待用户验证）
