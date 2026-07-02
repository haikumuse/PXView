# purify-architecture-concepts Spec

## Why

`fix-remaining-architecture-issues` spec 自称完成了 SigSession 拆分、SessionDocument 拆分、SignalConfigStore 提取，但系统调研发现这些都是**"建了新结构但没切换调用点"的半成品**：

1. **死代码冒充已完成**：`SignalConfigStore::signal_config_to_json/from_json` 全工程零调用者，真实 .pxc 序列化仍走 `MainWindow::gen_config_json` 另一条路径，两条路径写入字段集几乎不相交。上一轮拆分实际未生效。
2. **死存储字段**：`SessionDocument` 持有 5 个永远不写入的字段（`_signal_models`/`_spectrum_stacks`/`_math_stack`/`_lissajous_model`/`_decoder_model`），与 `SigSession` 同名字段并存，是双真相源隐患。
3. **用户数据丢失 bug**：`visible`/`trig_type`/`v_offset`/`own_height` 在真实序列化路径中根本不写——用户隐藏通道、调整布局后重载 .pxc 全部丢失。
4. **enabled/visible 语义混淆**：5 处存储点，`signalfactory.cpp:62-63` 和 `view.cpp:2349` 直接把硬件 `enabled` 当 UI `visible` 用。
5. **Core 层残留 UI 概念**：`DecoderModel : public QAbstractTableModel` 在 `pv::data`、`annotation.h` 含 `QFont`/`QFontMetrics`、`ChannelConfig.visible` 是纯 UI 字段却存 Core。
6. **View 层绕过 Core**：`MainWindow::load_config_from_json` 直接改写 `sr_channel->vdiv/coupling/...`，`_trigger_widget->get_session()` 直接序列化触发配置，违反"TriggerConfig 是 Core 唯一真相源"硬约束。
7. **God class 残留**：`SigSession` 仍是 299 行头 + 2052 行实现，5 个 manager 全 `friend` 直访 `_session->_xxx`——"方法走了，数据留下"的不彻底拆分。`MainWindow` 4129 行、`View` 3239 行、`StoreSession` 1875 行。

AGENTS.md 自称 SigSession 284 行，实际 299 行，文档与代码脱节。

**用户明确解除兼容性约束**（"不考虑兼容性"）：.pxc 旧文件兼容、API 兼容都不再是阻碍，可以大刀阔斧重构。本 spec 收口全部 7 类概念不纯问题。

## What Changes

### 阶段 1（P0 零风险清理）
- **A1**: 删除 `SessionDocument` 5 个死存储字段（`_signal_models`/`_spectrum_stacks`/`_math_stack`/`_lissajous_model`/`_decoder_model`）及其 `set_*` 方法
- **A2**: 修正 AGENTS.md 行数声明（299 行而非 284 行）

> 注：`SignalConfigStore::signal_config_to_json/from_json` 当前是死代码（零调用者），但阶段 2 要启用为唯一序列化路径，故阶段 1 不删，保留作阶段 2 基础。

### 阶段 2（P0 序列化路径统一 + 用户数据 bug 修复）
- **B1**: 删除 `MainWindow::gen_config_json`/`load_config_from_json` 中直访 `sr_channel` 的逻辑，改走 `SignalConfigStore` 序列化 + `SignalModel` 写入
- **B2**: `SignalConfigStore::signal_config_to_json/from_json` 成为 .pxc channel 配置的**唯一序列化路径**
- **B3**: `visible`/`trig_type`/`v_offset`/`own_height`/`hw_offset`/`offset`/`zero_offset` 全部正确序列化（修复用户数据丢失 bug）
- **B4**: 删除 `MainWindow::load_channel_view_indexs` 死路径（仅 LOGIC 模式触发、只读 view_index）
- **B5**: trigger 序列化改走 `SigSession::trigger_config()`，删除 `_trigger_widget->get_session()` 直调

### 阶段 3（P1 enabled/visible 语义拆分）
- **C1**: `ChannelConfig` 删除 `visible` 字段（UI 概念，移出 Core）
- **C2**: `Signal::set_visible` 不再从 `model->enabled()` 派生，改由 View 层独立管理
- **C3**: `enabled` 单一真相源为 `SignalModel::_enabled`（写 `sr_channel->enabled`），`ChannelConfig.enabled` 仅作序列化载体，运行时不读写
- **C4**: `signalconfigstore.cpp:137-138` `visible` 缺省时不再用 `enabled` 兜底
- **C5**: `trace.h:194-197` `enabled()` 注释修正，明确 `enabled` = 硬件启用、`visible` = UI 可见，二者独立

### 阶段 4（P1 Core 残留 UI 概念清理）
- **D1**: `DecoderModel` 从 `pv::data` 移到 `pv::view`，或改为纯数据结构 + View 层 `QAbstractTableModel` 包装器
- **D2**: `annotation.h` 的 `QFont`/`QFontMetrics`/`_cached_width_font` 移到 View 层渲染类，Core 的 `Annotation` 仅保留数据字段
- **D3**: `datasource.h` 删除未使用的 `pv::view::*` 前向声明

### 阶段 5（P1 View 绕过 Core 修复）
- **E1**: `MainWindow::load_config_from_json` 改走 `SignalModel::set_vdiv`/`set_coupling`/... 等 Core API，不直改 `sr_channel`
- **E2**: `ds_dsl_option_value_to_code` 等 libsigrok C API 封装到 `DeviceAgent`/Core
- **E3**: `MainWindow::load_config_from_json` 中 `set_colour`/`set_trig`/`set_zero_ratio` 改通过 Core 写回 `SignalModel`

### 阶段 6（P2 UI 布局字段迁移到 View 层）— 解除延期
- **F1**: `ChannelConfig` 删除 `view_index`/`v_offset`/`own_height` 三字段
- **F2**: `ChannelLayoutState` 从 `pv::data` 移到 `pv::view` 命名空间
- **F3**: .pxc 新增顶层 `"uiLayout"` 段（与 `"channels"` 平级），由 View 层 `view::View::save_ui_layout_to_json`/`load_ui_layout_from_json` 独立序列化
- **F4**: `signalfactory.cpp` 恢复点改读 View 层 `DockUiState`（扩展含 `view_index`/`v_offset`/`own_height`/`visible` 按 channel index 索引的 map）
- **F5**: `Header` 拖动持久化、`TabContext::deactivate` 改走 View 层 `DockUiState`
- **F6**: 旧 .pxc 文件无 `uiLayout` 段时使用默认布局（不考虑兼容性，不迁移老数据）

### 阶段 7（P3 God class 拆分）
- **G1**: `SigSession` 数据真正下沉到 6 个 manager——每个 manager 持有自己的数据成员，移除 `friend class core::XxxManager`，改用公共 accessor 或构造注入
- **G2**: `MainWindow` 序列化逻辑抽到 `SessionConfigSerializer` 类，MainWindow 退化为协调者
- **G3**: `StoreSession` 拆分为 `PxcSerializer`（.pxc JSON）+ `CsvExporter`（CSV 导出）+ `DecoderJsonSerializer`
- **G4**: `View` 按职责拆分：`ViewRenderer`/`ViewLayout`/`SignalRebuilder`/`PopupManager`（评估可行性，至少抽出 `SignalRebuilder`）

## Impact

- **Affected specs**:
  - `fix-remaining-architecture-issues`（本 spec 揭示其 Task 10/11 是半成品，需继续完成）
  - `unify-signal-layout-state`（方向相反，本 spec 阶段 6 反向迁移其字段，需标注该 spec superseded）
  - `decouple-core-from-view-v2`（C1/C3 延续 Core/View 分离）
- **Affected code**:
  - `PXView/pv/data/signalconfigstore.h/.cpp`（A1 删死代码、B2/B3 序列化统一、C1 删 visible、F1 删三字段）
  - `PXView/pv/data/sessiondocument.h/.cpp`（A2 删死字段、F1 删三字段）
  - `PXView/pv/sigsession.h/.cpp`（A3 行数修正、G1 数据下沉）
  - `PXView/pv/mainwindow.cpp`（B1/B4/B5/E1/E3 序列化重构、G2 拆分）
  - `PXView/pv/view/signalfactory.cpp`（C2/C3 enabled/visible 拆分、F4 恢复点改读 View）
  - `PXView/pv/view/view.cpp`（C2 visible 独立、F3/F5 uiLayout 序列化、G4 拆分）
  - `PXView/pv/view/header.cpp`（F5 持久化改走 View）
  - `PXView/pv/view/dock_ui_state.h`（F4 扩展含布局字段）
  - `PXView/pv/view/trace.h/.cpp`（C5 注释修正）
  - `PXView/pv/data/decodermodel.h`（D1 移出 Core）
  - `PXView/pv/data/decode/annotation.h`（D2 UI 概念移出）
  - `PXView/pv/data/datasource.h`（D3 删死声明）
  - `PXView/pv/storesession.cpp`（G3 拆分）
  - `PXView/pv/tabcontext.cpp`（F5 布局收集改走 View）
  - `AGENTS.md`（A3 行数修正、阶段完成后更新结构说明）

## ADDED Requirements

### Requirement: SignalConfigStore 是 .pxc channel 配置唯一序列化路径

The system SHALL route all .pxc channel configuration serialization through `SignalConfigStore::signal_config_to_json/from_json`. `MainWindow` SHALL NOT directly access `sr_channel` fields for serialization. `MainWindow::gen_config_json` channel-writing logic SHALL be removed.

#### Scenario: 保存 .pxc 时 visible 字段不丢失
- **WHEN** 用户隐藏某通道后保存 .pxc
- **THEN** `visible` 字段写入 .pxc 的 `channels[]` 数组
- **AND** 重新加载该 .pxc 后通道保持隐藏状态

#### Scenario: 保存 .pxc 时触发配置不丢失
- **WHEN** 用户配置 Logic 通道边沿触发后保存 .pxc
- **THEN** `trig_type` 字段写入 .pxc
- **AND** 重新加载后触发配置恢复

#### Scenario: 序列化路径单一
- **WHEN** grep 工程内 `gen_config_json` 写 channel 字段的代码
- **THEN** 仅 `SignalConfigStore::signal_config_to_json` 一处写入 channel JSON

### Requirement: enabled/visible 语义独立

The system SHALL treat `enabled` (hardware enable, Core-owned) and `visible` (UI visibility, View-owned) as independent concepts. `SignalFactory` SHALL NOT derive `visible` from `enabled`. `ChannelConfig` SHALL NOT contain `visible` field.

#### Scenario: 硬件禁用通道仍可在 UI 中切换可见性
- **WHEN** 通道硬件 enabled=false
- **THEN** 通道 UI visible 状态独立保留，不被迫 false
- **AND** 用户可在 UI 中切换该通道可见性而不影响硬件启用状态

#### Scenario: signalfactory 不混淆 enabled 与 visible
- **WHEN** `SignalFactory::apply_model_properties` 创建 Signal
- **THEN** `set_enabled(model->enabled())` 与 `set_visible(...)` 分离，visible 来自 View 层 DockUiState

### Requirement: UI 布局状态由 View 层独立持久化

The system SHALL persist `view_index`/`v_offset`/`own_height`/`visible` as a View-layer concern in a dedicated `"uiLayout"` section of .pxc, serialized by `view::View::save_ui_layout_to_json/load_ui_layout_from_json`. `ChannelConfig` SHALL NOT contain these fields. `ChannelLayoutState` SHALL live in `pv::view` namespace.

#### Scenario: .pxc 文件结构分层清晰
- **WHEN** 读取 .pxc 文件
- **THEN** 顶层有独立的 `"uiLayout"` 段（按 channel index 索引的布局 map）
- **AND** `"channels"` 段仅含硬件配置（vdiv/coupling/enabled/trig_type 等），无 UI 布局字段

#### Scenario: ChannelConfig 是纯硬件配置
- **WHEN** 检查 `ChannelConfig` 结构体定义
- **THEN** 仅含 `index`/`enabled`/`vdiv`/`coupling`/`map_default`/`hw_offset`/`offset`/`zero_offset`/`trig_type`
- **AND** 不含 `view_index`/`v_offset`/`own_height`/`visible`

### Requirement: SigSession manager 持有自己的数据

The system SHALL move data members from `SigSession` to their owning managers. `friend class core::XxxManager` declarations SHALL be removed. Managers SHALL access state via their own members or constructor-injected references, not direct `friend` access to `SigSession::_xxx`.

#### Scenario: CaptureManager 不通过 friend 访问 SigSession 内部
- **WHEN** 检查 `CaptureManager` 实现
- **THEN** 捕获相关状态（`_is_working`/`_capture_data`/`_view_data`/6 套 DsTimer）由 `CaptureManager` 自己持有
- **AND** 不出现 `_session->_xxx` 直访

#### Scenario: SigSession 退化为纯 facade
- **WHEN** 检查 `SigSession` 私有成员
- **THEN** 仅持有 6 个 `unique_ptr<Manager>` + 必要的协调状态（如 `_device_agent`）
- **AND** 行数 < 200（头文件）

### Requirement: Core 层不含 UI 概念

The system SHALL keep `pv::data` namespace free of UI concepts. `DecoderModel` SHALL NOT inherit `QAbstractTableModel` while in `pv::data`. `Annotation` SHALL NOT hold `QFont`/`QFontMetrics`. `DataSource` SHALL NOT forward-declare `pv::view::*` types.

#### Scenario: Core 层无 Qt Model/View 框架类
- **WHEN** grep `pv/data/` 目录下 `QAbstractTableModel`/`QAbstractListModel`
- **THEN** 0 命中

#### Scenario: Annotation 是纯数据
- **WHEN** 检查 `pv::data::decode::Annotation` 类
- **THEN** 仅含数据字段（annotations/decoder/row/etc.），不含 `QFont`/`_cached_width_font`

## MODIFIED Requirements

### Requirement: SigSession（fix-remaining-architecture-issues Task 10）
[原：拆分为 6 manager，SigSession 退化为 facade，284 行]
修改为：实际 299 行，5 manager 全 friend 直访 `_session->_xxx`，数据未下沉。本 spec 阶段 7 真正完成数据下沉，目标 < 200 行。

### Requirement: SessionDocument（fix-remaining-architecture-issues Task 11）
[原：纯数据模型，序列化下沉 SignalConfigStore]
修改为：仍持有 5 个死存储字段，SignalConfigStore 序列化是死代码。本 spec 阶段 1/2 清理死代码并切换调用点，阶段 6 删除 UI 布局字段。

### Requirement: MainWindow::OnMessage（fix-remaining-architecture-issues Task 9）
[原：拆分为 79 行路由 + 7 处理器]
修改为：OnMessage 已拆，但 MainWindow 整体仍 4129 行，序列化逻辑占大头。本 spec 阶段 7 抽出 `SessionConfigSerializer`。

### Requirement: AGENTS.md 文档准确性
[原：自称 SigSession 284 行]
修改为：阶段 1 修正为 299 行，阶段 7 完成后更新为实际行数（< 200）。

## REMOVED Requirements

### Requirement: SignalConfigStore::signal_config_to_json/from_json 作为死代码的状态
**Reason**: 全工程零外部调用者，是上一轮拆分未切换调用点的残留。阶段 2 启用为唯一序列化路径后不再是死代码。
**Migration**: 阶段 2 将 `MainWindow::gen_config_json` 的 channel 写入逻辑迁移到 `SignalConfigStore::signal_config_to_json`，删除 MainWindow 那条路径。阶段 1 不删，保留作阶段 2 基础。

### Requirement: MainWindow::gen_config_json 直接写 channel JSON
**Reason**: 绕开 Core SignalConfigStore，导致字段集不一致、visible/trig_type/v_offset/own_height 丢失。
**Migration**: 阶段 2 改走 `SignalConfigStore::signal_config_to_json`，MainWindow 仅负责编排（调 store 序列化 + 组装顶层 JSON）。

### Requirement: MainWindow::load_config_from_json 直改 sr_channel
**Reason**: 绕开 Core SignalModel 真相源，View 层直访 libsigrok 内部结构。
**Migration**: 阶段 5 改走 `SignalModel::set_vdiv`/`set_coupling`/... Core API。

### Requirement: ChannelConfig 持有 UI 布局字段（unify-signal-layout-state spec）
**Reason**: 该 spec 方向错误，把 UI 字段塞进 Core 导致概念不纯。本 spec 阶段 6 反向迁移到 View 层。
**Migration**: `unify-signal-layout-state` spec 标记为 superseded。布局字段移到 `pv::view::DockUiState` + .pxc `uiLayout` 段。

### Requirement: ChannelConfig::visible 字段
**Reason**: UI 可见性是 View 概念，不应在 Core 数据结构。
**Migration**: 阶段 3 删除，visible 状态移到 View 层 DockUiState，随阶段 6 一起持久化到 `uiLayout` 段。

### Requirement: friend class core::XxxManager（5 处）
**Reason**: god class 拆分"方法走了数据留下"的不彻底形态，manager 不是独立单元只是方法外挂。
**Migration**: 阶段 7 数据下沉到 manager，manager 持有自己的成员，移除 friend 声明。

### Requirement: DecoderModel : public QAbstractTableModel 在 pv::data
**Reason**: Qt Model/View 框架的 Model 概念混入 Core 数据层，返回 `Qt::DisplayRole`/`Qt::AlignLeft` 等 UI 常量。
**Migration**: 阶段 4 移到 `pv::view`，或改为纯数据结构 + View 层包装器。

### Requirement: Annotation 持有 QFont/QFontMetrics
**Reason**: 渲染期概念混入 Core 数据类。
**Migration**: 阶段 4 移到 View 层渲染类，Core 的 `Annotation` 仅保留数据字段。
