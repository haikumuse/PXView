# View 层现代化架构重构 Spec

## Why

经过 `decouple-core-from-view-v2` 和 `unify-signal-layout-state` 等重构之后，PXView 的 Core/View 分层、`shared_ptr` 生命周期管理、Qt Signal/Slot 响应式绑定已经达到现代 C++ 客户端架构的优秀水准。但 View 层仍存在三处历史包袱，阻碍架构进一步现代化：

1. **libsigrok 数据结构污染 View 层**：`view::Signal` 仍持有 `sr_channel *const _probe` 成员，`Signal::set_name()` 直接 `_probe->name = g_strdup(...)`，`DsoSignal` 直接调用 `ds_set_probe_parameter(_probe, ...)`。View 层 9 个文件、98 处 `_probe` 引用，与 libsigrok 头文件强耦合，违反"View 只认识 SignalModel"的 MV 原则。
2. **全局单例滥用**：`Signal::Signal()` 构造函数隐式调用 `session = AppControl::Instance()->GetSession()`，全工程 18 个文件、32 处 `AppControl::Instance()` 调用。这让 `Signal` 类无法独立测试、无法被复用、隐式依赖全局启动顺序。
3. **拓扑变化重算过重**：`View::on_signals_changed()` 对所有 `signals_changed` 事件统一使用 `SignalFactory::AllReplaced`（[view.cpp:2461-2462](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/view.cpp#L2461)），即使只新增一个通道也会"全删全建"。`SignalFactory::update_signals` 已实现 `Added`/`Removed`/`Modified` 三个增量分支但从未被调用。

本 spec 通过**三条独立但协同的现代化轨道**消除这三处历史包袱，让 View 层真正成为"只依赖 SignalModel 的纯显示端"。

## 架构定位

```
┌─────────────────────────────────────────────────────────┐
│  View 层 (Qt Widgets)                                   │
│  ┌───────────────────────────────────────────────────┐  │
│  │ view::Signal 子类                                  │  │
│  │  - 持有 SignalModel* (单一数据源)                  │  │
│  │  - 持有 SigSession* (依赖注入，不再用 AppControl)  │  │
│  │  - 不再持有 sr_channel*                            │  │
│  │  - 不再 #include <libsigrok.h>                     │  │
│  │  - 只通过 model->set_xxx() 写回 Core              │  │
│  └───────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────┐  │
│  │ view::View                                         │  │
│  │  - on_signals_changed() 按差异分发                │  │
│  │    Added/Removed/Modified，仅 AllReplaced 兜底     │  │
│  │  - 增量布局：只重排受影响行，不全量 signals_changed│  │
│  └───────────────────────────────────────────────────┘  │
└──────────────────────────┬──────────────────────────────┘
                           │ Qt Signal/Slot + Diff
┌──────────────────────────▼──────────────────────────────┐
│  Core 层 (pxview-core, 无 Qt Widgets)                   │
│  ┌───────────────────────────────────────────────────┐  │
│  │ SignalModel (单一状态源)                           │  │
│  │  - 已有 name/index/type/enabled/color/vdiv/...     │  │
│  │  - set_xxx() 写回 DeviceAgent + sr_channel         │  │
│  │  - emit appearance_changed / visibility_changed    │  │
│  └───────────────────────────────────────────────────┘  │
│  ┌───────────────────────────────────────────────────┐  │
│  │ DeviceAgent — libsigrok 唯一调用者                 │  │
│  │  (View 不再直接调用 ds_set_probe_parameter)        │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

## What Changes

### Track A: 清除 libsigrok 对 View 层的污染

- **ADDED**：`SignalModel` 新增写回 DeviceAgent 的能力 — `set_vdiv()`/`set_coupling()`/`set_trig_value()`/`set_vertical_offset()` 等 setter 在内部通过 `DeviceAgent` 同步到 `sr_channel` 字段和 `ds_set_probe_parameter()` API，让 View 层只调 model setter 即可完成硬件配置
- **ADDED**：`SignalModel` 新增 `set_name()`/`set_enabled()` 的硬件写回路径（当前只写 model 字段，未同步到 `sr_channel`，依赖 `SigSession::reload()` 重建）
- **ADDED**：`SignalModel` 新增 `commit_to_device()` 方法，将所有累积变更一次性同步到 `sr_channel` + libsigrok API
- **MODIFIED**：`view::Signal` 构造函数签名从 `Signal(sr_channel *probe)` 改为 `Signal(std::shared_ptr<data::SignalModel> model, SigSession *session)`
- **MODIFIED**：`LogicSignal`/`AnalogSignal`/`DsoSignal` 构造函数签名同步变更，移除 `sr_channel*` 参数
- **MODIFIED**：`Signal::set_name()` 不再 `_probe->name = g_strdup(...)`，改为 `model->set_name(...)`（model 内部负责同步到 sr_channel）
- **MODIFIED**：`Signal::set_enabled()` 不再 `_probe->enabled = en`，改为 `model->set_enabled(...)`
- **MODIFIED**：`DsoSignal` 中 `ds_set_probe_parameter(_probe, "TRIG_VALUE", ...)` 等 4 处 libsigrok 调用替换为 `model->set_trig_value()` 等 setter（model 内部调用 `ds_set_probe_parameter`）
- **MODIFIED**：`SignalFactory::create_signal` 不再调用 `find_probe_by_index`，直接从 `SignalModel` 创建 Signal
- **MODIFIED**：`SignalFactory::create_signals` 同步适配
- **MODIFIED**：`view::Signal` 子类读取 `_probe->index`/`_probe->vdiv`/`_probe->coupling`/`_probe->offset`/`_probe->trig_value` 全部改为 `model->index()`/`model->vdiv()` 等
- **MODIFIED**：`view::Signal` 子类中 `_data->has_data(_probe->index)` 改为 `_data->has_data(model->index())`
- **MODIFIED**：`view::Signal` 子类中 `getSignalColor(_probe->index)` 改为 `getSignalColor(model->index())`
- **REMOVED**：`view::Signal::_probe` 成员变量移除
- **REMOVED**：`view::Signal::probe()` 访问器移除
- **REMOVED**：`SignalFactory::find_probe_by_index` 静态函数移除
- **REMOVED**：View 层所有 `#include <libsigrok.h>` 移除（保留 `signalmodel.h` 间接依赖）

### Track B: 用依赖注入替代全局单例

- **MODIFIED**：`view::Signal` 构造函数接收 `SigSession *session` 参数（由 Track A 的新签名一并落地），不再在构造体内 `session = AppControl::Instance()->GetSession()`
- **MODIFIED**：`view::Signal` 子类构造函数透传 `session` 给基类
- **MODIFIED**：`SignalFactory::create_signal`/`create_signals`/`update_signals` 已接收 `SigSession *session` 参数，确保向下传递
- **MODIFIED**：`view::DecodeTrace` 中 `AppControl::Instance()->GetSession()` 调用替换为构造时注入的 session 引用
- **MODIFIED**：`view::ruler.cpp`、`view::viewport.cpp`、`view::decodetrace.cpp` 中 4 处 View 层 `AppControl::Instance()->GetSession()` 替换为通过 `_view->session()` 或构造注入访问
- **MODIFIED**：`dialogs/decoderoptionsdlg.cpp`、`dialogs/deviceoptions.cpp`、`dialogs/calibration.cpp`、`dialogs/applicationpardlg.cpp`、`prop/binding/probeoptions.cpp`、`prop/binding/deviceoptions.cpp`、`dock/mcpcontroldock.cpp`、`toolbars/titlebar.cpp`、`ui/msgbox.cpp`、`mainwindow.cpp`、`mainframe.cpp`、`winnativewidget.cpp`、`main.cpp` 中的 `AppControl::Instance()` 调用按场景分类处理：
  - 启动入口（main.cpp/mainframe.cpp）：保留 `AppControl::Instance()`，这是全局单例的合法使用场景
  - 业务对话框/Dock：改为通过父组件注入 `SigSession*` 或 `AppControl*`
  - 工具类（msgbox/titlebar）：若仅用 `AppControl` 获取配置，改为参数传入所需数据
- **NOT CHANGED**：`appcontrol.cpp` 自身实现、`main.cpp` 启动序列 — `AppControl` 作为全局单例在启动入口合法保留

### Track C: 实现真正的增量拓扑更新

- **ADDED**：`SignalFactory::compute_change_event(current_signals, models)` 静态方法，按 `channel_index` 比对当前 Signal 列表与新 SignalModel 列表，返回 `SignalChangeEvent`（Added/Removed/Modified/AllReplaced）
- **MODIFIED**：`View::on_signals_changed()` 调用 `compute_change_event` 决定事件类型，仅在确实发生大规模替换时使用 `AllReplaced`，否则使用 `Added`/`Removed`/`Modified`
- **ADDED**：`View::signals_added(int first, int last)` / `View::signals_removed(int first, int last)` / `View::signals_modified(int index)` 增量布局方法，只重排受影响行的 `v_offset`/`view_index`，不调用全量 `signals_changed(NULL)`
- **MODIFIED**：`View::on_signals_changed()` 根据 `compute_change_event` 结果分派到 `signals_added`/`signals_removed`/`signals_modified`，仅 `AllReplaced` 走全量 `signals_changed(NULL)`
- **MODIFIED**：`View::rebuild_signals()` 的 config-based 分支保留作为"设备通道数变化"的兜底，但内部改用 `SignalFactory::update_signals(Modified)` 而非 `signals_changed(NULL)`
- **NOT CHANGED**：`SignalFactory::update_signals` 的 `Added`/`Removed`/`Modified` 分支实现已就绪，本 track 仅激活其调用路径

### **BREAKING** Changes

- `view::Signal` 及子类构造函数签名变更（`sr_channel*` → `SignalModel*` + `SigSession*`）— View 层内部改动，不影响 Core 层 API
- `view::Signal::_probe` 成员移除 — 所有 View 层代码必须改用 `model->xxx()` 访问
- `view::Signal::probe()` 方法移除 — 外部消费者（如 `DecoderOptionsDlg`）改用 `model->index()` 等访问

## Impact

- **Affected specs**:
  - `decouple-core-from-view-v2`（前置，已完成）— 本 spec 在其基础上进一步消除 View 层的 libsigrok 依赖
  - `unify-signal-layout-state`（前置，已完成）— 本 spec 的 Track C 与其"单一状态源"方向一致，增量更新时复用 `ChannelConfig` 的布局字段
  - `explicit-document-context`（正交，不冲突）

- **Affected code**:
  - `PXView/pv/data/signalmodel.h` / `signalmodel.cpp` — Track A：setter 写回 DeviceAgent + `commit_to_device()`
  - `PXView/pv/view/signal.h` / `signal.cpp` — Track A+B：构造签名变更、移除 `_probe`、移除 `AppControl` 依赖
  - `PXView/pv/view/logicsignal.h` / `logicsignal.cpp` — Track A：构造签名变更、`_probe->index` 改 `model->index()`
  - `PXView/pv/view/analogsignal.h` / `analogsignal.cpp` — Track A：同上
  - `PXView/pv/view/dsosignal.h` / `dsosignal.cpp` — Track A：构造签名变更、4 处 `ds_set_probe_parameter` 调用迁移到 model setter、`_probe->vdiv`/`coupling`/`offset`/`trig_value` 改 model 读取
  - `PXView/pv/view/signalfactory.h` / `signalfactory.cpp` — Track A+C：移除 `find_probe_by_index`、`create_signal` 签名简化、新增 `compute_change_event`
  - `PXView/pv/view/view.h` / `view.cpp` — Track C：`on_signals_changed` 分派、新增 `signals_added`/`signals_removed`/`signals_modified`
  - `PXView/pv/view/decodetrace.cpp` — Track B：`AppControl::Instance()->GetSession()` 改注入
  - `PXView/pv/view/ruler.cpp` / `viewport.cpp` — Track B：4 处单例调用改注入
  - `PXView/pv/dialogs/decoderoptionsdlg.cpp` — Track A+B：`Signal::probe()` 改 `model->index()`、单例改注入
  - `PXView/pv/dialogs/deviceoptions.cpp` / `calibration.cpp` / `applicationpardlg.cpp` — Track B：单例改注入
  - `PXView/pv/prop/binding/probeoptions.cpp` / `deviceoptions.cpp` — Track A+B：`sr_channel*` 操作改 model setter
  - `PXView/pv/dock/mcpcontroldock.cpp` — Track B：单例改注入
  - `PXView/pv/toolbars/titlebar.cpp` — Track B：单例改参数传入
  - `PXView/pv/ui/msgbox.cpp` — Track B：单例改参数传入
  - `PXView/pv/mainwindow.cpp` — Track B：3 处单例评估保留/改注入
  - `CMakeLists.txt` — Track A：View 层源文件不再 `target_include_directories(${libsigrok_INCLUDE_DIR})`（可选，验证编译通过后移除）

- **Core/View 边界**: Track A 让 View 层完全脱离 libsigrok 头文件，Core 层 `SignalModel` 接管所有 `sr_channel` 字段写回职责。Core/View 边界更清晰。

- **向后兼容**: 内部架构重构，不影响 `.pxc` 文件格式、MCP API、WS API。

## ADDED Requirements

### Requirement: SignalModel 硬件配置写回能力

`SignalModel` SHALL 在 setter 方法中通过 `DeviceAgent` 将变更同步到底层 `sr_channel` 结构和 libsigrok API，让 View 层只调用 model setter 即可完成硬件配置。

涉及 setter（新增或增强写回逻辑）：
- `set_name(std::string)` — 同步到 `_sr_channel->name`（g_strdup 替换）
- `set_enabled(bool)` — 同步到 `_sr_channel->enabled`
- `set_vdiv(double)` — 调用 `ds_set_probe_parameter(_sr_channel, "VDIV", ...)`
- `set_coupling(int)` — 调用 `ds_set_probe_parameter(_sr_channel, "COUPLING", ...)`
- `set_trig_value(double)` — 调用 `ds_set_probe_parameter(_sr_channel, "TRIG_VALUE", ...)`
- `set_vertical_offset(double)` — 同步到 `_sr_channel->offset` + libsigrok API

`SignalModel` SHALL 持有 `SigSession*`（或 `DeviceAgent*`）弱引用以访问 `sr_channel`，通过 `set_session(SigSession*)` 在 `init_signals()` 时注入。

`SignalModel` SHALL 新增 `sr_channel* sr_channel_handle()` 访问器（仅 Core 层使用），供 `SigSession::reload()` 等内部路径访问底层结构。View 层 SHALL NOT 调用此方法。

#### Scenario: View 修改 DSO 通道 vdiv 通过 model setter 同步到硬件
- **WHEN** 用户在 UI 中修改 DSO 通道 vdiv
- **THEN** View 调用 `model->set_vdiv(new_vdiv)`
- **AND** `SignalModel::set_vdiv` 内部调用 `DeviceAgent::set_probe_parameter(_sr_channel, "VDIV", ...)`
- **AND** `_sr_channel->vdiv` 字段同步更新
- **AND** View 层不直接调用 `ds_set_probe_parameter`

#### Scenario: Headless 模式下通过 MCP API 修改 vdiv
- **WHEN** MCP API 调用 `set_channel_config(channel_index, "vdiv", value)`
- **THEN** SessionService 调用 `SignalModel::set_vdiv(value)`
- **AND** 硬件配置同步路径与 GUI 模式完全一致

### Requirement: Signal 构造函数依赖注入

`view::Signal` 及其子类 SHALL 通过构造函数参数接收 `std::shared_ptr<data::SignalModel>` 和 `SigSession*`，不再隐式调用 `AppControl::Instance()->GetSession()`。

新签名：
```cpp
class Signal : public Trace {
protected:
    Signal(std::shared_ptr<data::SignalModel> model, SigSession *session);
    Signal(const Signal &s, std::shared_ptr<data::SignalModel> model, SigSession *session);
    // ...
};

class LogicSignal : public Signal {
public:
    LogicSignal(data::LogicSnapshot *data,
                std::shared_ptr<data::SignalModel> model,
                SigSession *session);
    // ...
};
// DsoSignal / AnalogSignal 同理
```

`Signal` SHALL 持有 `std::shared_ptr<data::SignalModel> _model` 成员（共享所有权，保证 Core 层释放 SignalModel 后 View 层仍可访问已渲染对象直到下一次 `update_signals`）。

`Signal` SHALL NOT 持有 `sr_channel*` 成员。

`Signal` SHALL NOT 在构造体内调用 `AppControl::Instance()`。

#### Scenario: Signal 创建通过构造注入
- **WHEN** `SignalFactory::create_signal` 创建新的 `LogicSignal`
- **THEN** 传入 `model` 和 `session` 参数
- **AND** 构造体内不调用 `AppControl::Instance()`
- **AND** 对象通过 `_model` 访问 name/index/type/enabled 等字段

#### Scenario: Signal 可独立实例化用于单元测试
- **WHEN** 单元测试构造一个 `Signal` 子类
- **THEN** 可注入 mock `SignalModel` 和 mock `SigSession`
- **AND** 不触发任何全局单例访问

### Requirement: SignalFactory 增量变更事件计算

`SignalFactory` SHALL 提供 `compute_change_event` 静态方法，比对当前 `view::Signal*` 列表与新 `SignalModel*` 列表，返回精确的 `SignalChangeEvent`。

```cpp
static SignalChangeEvent compute_change_event(
    const std::vector<Signal*> &current_signals,
    const std::vector<std::shared_ptr<data::SignalModel>> &models);
```

判定规则：
- 若 `current_signals` 为空且 `models` 非空 → `AllReplaced`（首次创建）
- 若 `current_signals` 非空且 `models` 为空 → `AllReplaced`（全部移除）
- 若两边均有元素且 index 集合相同但属性可能不同 → `Modified`
- 若 `models` 的 index 集合是 `current_signals` 的超集 → `Added`
- 若 `current_signals` 的 index 集合是 `models` 的超集 → `Removed`
- 若同时有增删 → `AllReplaced`（保守兜底，避免多次增量分派的复杂度）
- 若 index 集合完全相同且数量大但只少量属性变化 → `Modified`

#### Scenario: 新增单个通道触发 Added 事件
- **WHEN** 设备从 8 通道变为 9 通道，新增 index=8
- **AND** `on_signals_changed()` 调用 `compute_change_event`
- **THEN** 返回 `Added`
- **AND** `update_signals(Added)` 只创建 index=8 的新 Signal，不删除/重建其他 8 个

#### Scenario: 移除单个通道触发 Removed 事件
- **WHEN** 设备从 8 通道变为 7 通道，移除 index=7
- **AND** `on_signals_changed()` 调用 `compute_change_event`
- **THEN** 返回 `Removed`
- **AND** `update_signals(Removed)` 只删除 index=7 的 Signal，保留其他 7 个

#### Scenario: 通道属性变化触发 Modified 事件
- **WHEN** 用户通过 MCP 修改某通道颜色，`SignalModel::appearance_changed` 触发 `signals_changed`
- **AND** `on_signals_changed()` 调用 `compute_change_event`
- **THEN** 返回 `Modified`
- **AND** `update_signals(Modified)` 只调用 `apply_model_properties` 刷新属性，不重建对象

### Requirement: View 增量布局更新

`View` SHALL 提供三个增量布局方法，仅重排受影响行，不触发全量 `signals_changed(NULL)` 重排：

```cpp
void signals_added(int first, int last);    // 新增 [first, last] 行
void signals_removed(int first, int last);  // 移除 [first, last] 行
void signals_modified(int index);           // 修改 index 行属性
```

`signals_added` SHALL：
- 为新 Signal 分配 `view_index`（基于现有最大 view_index + 1）
- 计算新增行的 `v_offset`（基于上一行 v_offset + own_height + margin）
- 调用 `viewport_update()` 刷新视口（不调用 `signals_changed(NULL)`）

`signals_removed` SHALL：
- 回收被删 Signal 的 `view_index`
- 调整后续行的 `v_offset`（向上填补空隙）
- 调用 `viewport_update()` 刷新视口

`signals_modified` SHALL：
- 仅 `update()` 受影响行的视口区域
- 不调整布局

#### Scenario: 新增单个通道不触发全量重排
- **WHEN** `compute_change_event` 返回 `Added`
- **THEN** `View::on_signals_changed` 调用 `update_signals(Added)` + `signals_added(first, last)`
- **AND** 不调用 `signals_changed(NULL)`
- **AND** 现有通道的 `view_index`/`v_offset`/`own_height` 不变
- **AND** UI 响应时间从 O(N) 降为 O(1)（N 为通道总数）

#### Scenario: 通道属性变化不触发重排
- **WHEN** `compute_change_event` 返回 `Modified`
- **THEN** `View::on_signals_changed` 调用 `update_signals(Modified)` + `signals_modified(index)`
- **AND** 不调用 `signals_changed(NULL)`
- **AND** 不调整任何通道的 `view_index`/`v_offset`

## MODIFIED Requirements

### Requirement: View::on_signals_changed 增量分派

`View::on_signals_changed()` SHALL 根据 `SignalFactory::compute_change_event` 的结果分派到对应的增量更新路径，仅在大规模替换时回退到 `AllReplaced` 全量重建。

```cpp
void View::on_signals_changed() {
    auto event = SignalFactory::compute_change_event(_own_signals, _data_source->get_signal_models());
    SignalFactory::update_signals(_own_signals, _data_source, _session, event);

    switch (event) {
    case Added:
        signals_added(/* first */, /* last */);
        break;
    case Removed:
        signals_removed(/* first */, /* last */);
        break;
    case Modified:
        signals_modified(/* index */);
        break;
    case AllReplaced:
    default:
        signals_changed(NULL);  // 全量布局兜底
        break;
    }
    mark_derived_traces_dirty();
    update_scroll();
}
```

#### Scenario: 设备切换触发 AllReplaced
- **WHEN** 用户从 8 通道设备切换到 16 通道设备
- **THEN** `compute_change_event` 返回 `AllReplaced`（同时增删，超出 Added/Removed 单调变化范围）
- **AND** 走全量 `signals_changed(NULL)` 重排路径

#### Scenario: 采集完成触发 Modified
- **WHEN** 采集完成触发 `signals_changed`，通道列表未变
- **THEN** `compute_change_event` 返回 `Modified`
- **AND** 只调用 `apply_model_properties` 刷新属性（如快照指针）
- **AND** 不删除/重建 Signal 对象

### Requirement: View::rebuild_signals 优先增量

`View::rebuild_signals()` SHALL 在 config-based 分支中优先使用 `SignalFactory::update_signals(Modified)`，仅在全量重建不可避时调用 `signals_changed(NULL)`。

#### Scenario: 重新采集后通道列表未变
- **WHEN** 用户重新采集，`rebuild_signals()` 被调用
- **AND** `config.channels.size() == device_ch_count`
- **THEN** 调用 `rebuild_signals_from_config(config)` + `update_signals(Modified)`
- **AND** 不调用 `signals_changed(NULL)` 全量重排
- **AND** 通道顺序和高度与采集前一致（依赖 `unify-signal-layout-state` 的 ChannelConfig 布局字段）

## REMOVED Requirements

### Requirement: view::Signal 持有 sr_channel* 成员

**Reason**: 违反"View 只认识 SignalModel"的 MV 原则，导致 View 层与 libsigrok 头文件强耦合，98 处 `_probe` 引用散布 9 个文件。`SignalModel` 已具备承载所有 `sr_channel` 字段的能力（Track A 增强后），View 层无需直接访问 `sr_channel`。

**Migration**:
- `view::Signal::_probe` 成员移除，替换为 `std::shared_ptr<data::SignalModel> _model`
- `view::Signal::probe()` 访问器移除，外部消费者改用 `model->index()` 等
- 所有 `_probe->xxx` 读取改为 `_model->xxx()`
- 所有 `_probe->xxx = yyy` 写入改为 `_model->set_xxx(yyy)`
- 所有 `ds_set_probe_parameter(_probe, ...)` 调用迁移到 `SignalModel` setter（model 内部调用 libsigrok API）

### Requirement: view::Signal 隐式依赖 AppControl 单例

**Reason**: 构造函数内 `session = AppControl::Instance()->GetSession()` 是隐藏的全局依赖，阻碍单元测试、复用性、显式依赖图。`SigSession` 应通过构造注入。

**Migration**:
- `view::Signal` 构造函数接收 `SigSession *session` 参数
- 子类构造函数透传 `session`
- `SignalFactory::create_signal` 已接收 `session` 参数，向下传递
- View 层其他文件中的 `AppControl::Instance()->GetSession()` 改通过 `_view->session()` 或构造注入访问
- 启动入口（main.cpp/mainframe.cpp）保留 `AppControl::Instance()` 的合法使用

### Requirement: View::on_signals_changed 统一使用 AllReplaced

**Reason**: 对所有 `signals_changed` 事件统一使用 `AllReplaced` 导致"全删全建"，UI 状态频繁丢失、性能浪费。`SignalFactory::update_signals` 已实现 `Added`/`Removed`/`Modified` 增量分支但从未被调用。

**Migration**:
- `View::on_signals_changed` 调用 `compute_change_event` 决定事件类型
- 分派到 `signals_added`/`signals_removed`/`signals_modified` 增量布局方法
- 仅 `AllReplaced` 走全量 `signals_changed(NULL)` 兜底路径
