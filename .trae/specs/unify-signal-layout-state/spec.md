# 统一信号轨道布局状态 Spec

## Why

PXView 的 UI 布局状态（通道顺序 `view_index`、垂直偏移 `v_offset`、轨道高度 `own_height`）目前分散在三处，且没有任何一处是持久化的"单一状态源"：

1. `view::Signal` 对象 — 临时持有，rebuild 时丢失
2. `SessionDocument::ChannelConfig` — 持久化通道配置，但**不包含布局字段**
3. `SignalFactory::save_ui_state` — 仅在 `update_signals(AllReplaced)` 调用期间存活的瞬态备份（[signalfactory.cpp:236](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/signalfactory.cpp#L236)）

后果：`View::rebuild_signals_from_config()`（在 `TabContext::activate()`、`MainWindow::OnMessage(DSV_MSG_DEVICE_OPTIONS_UPDATED)` 等路径触发）显式重置 `view_index`（按 config 顺序重新分配）和 DSO/Analog 的 `own_height`（设为 -1）：

```cpp
// view.cpp:2154-2161 — 当前的重置逻辑
if (config.work_mode == DSO || config.work_mode == ANALOG) {
    signal->set_own_height(-1);
}
if (ch.enabled) {
    signal->set_view_index(view_index++);
} else {
    signal->set_view_index(-1);
}
```

用户反馈："重新采集之后波形轨道的顺序还有高度什么的都会被重置"。这是架构层面的状态管理问题，不是单点 bug —— 三条 rebuild 路径状态保留行为不一致：

| Rebuild 路径 | 触发场景 | 布局保留 |
|---|---|---|
| `on_signals_changed()` → `update_signals(AllReplaced)` | `signals_changed()` 广播（reload/attach_data） | ✓（瞬态 save/restore） |
| `rebuild_signals_from_config()` | `TabContext::activate`、`rebuild_signals` config 分支 | ✗（重置 view_index + own_height） |
| `rebuild_signals()` create 分支 | config 不匹配设备通道数 | ✗（全量重建，无保留） |

本 spec 将 `ChannelConfig` 扩展为 UI 布局状态的**单一持久化状态源**，让所有 rebuild 路径都从同一份配置恢复，消除"重置"行为。这是"状态管理统一为单一状态源"架构方向的第一步落地，聚焦于已暴露的布局状态痛点。

## What Changes

- **ADDED**：`ChannelConfig` 新增三个字段 `view_index`、`v_offset`、`own_height`（默认 -1 / 0 / -1，表示"未设置/使用派生默认"）。
- **ADDED**：`ChannelLayoutState` 结构体（`{int view_index; int v_offset; int own_height;}`）用于 `save_signal_config` 参数传递。
- **MODIFIED**：`SessionDocument::save_signal_config` 新增 `channel_layout` 参数，将 View 层的布局状态写入 `ChannelConfig`。
- **MODIFIED**：`signal_config_to_json` / `signal_config_from_json` 序列化三个新字段（向后兼容：缺失字段使用默认值）。
- **MODIFIED**：`View::rebuild_signals_from_config` 从 `ChannelConfig` 恢复 `view_index` / `v_offset` / `own_height`，**不再重置**。
- **MODIFIED**：`View::rebuild_signals` 的 config-based 分支不再二次重置布局（由 `rebuild_signals_from_config` 统一处理）。
- **MODIFIED**：`TabContext::deactivate` 收集 View 布局状态传入 `save_signal_config`。
- **MODIFIED**：`MainWindow` 中所有 6 处 `save_signal_config` 调用点补 `channel_layout` 参数。

## Impact

- **Affected code**:
  - `PXView/pv/data/sessiondocument.h` — `ChannelConfig` 结构体扩展 + `ChannelLayoutState` 定义 + `save_signal_config` 签名
  - `PXView/pv/data/sessiondocument.cpp` — 序列化 + `save_signal_config` 实现
  - `PXView/pv/view/view.cpp` — `rebuild_signals_from_config` 恢复逻辑（[L2154-2161](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/view.cpp#L2154)）+ `rebuild_signals` 清理（[L2233-2236](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/view.cpp#L2233)）
  - `PXView/pv/tabcontext.cpp` — `deactivate` 收集布局（[L140-148](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/tabcontext.cpp#L140)）
  - `PXView/pv/mainwindow.cpp` — 6 处 `save_signal_config` 调用补参数（L370、L2420、L2884、L2955、L2987、L3623）
- **Affected specs**: `fix-state-sync-gaps-v2`（前置，已完成）、`explicit-document-context`（正交，不冲突）
- **Core/View 边界**: 新字段为纯 int 数据，无 Qt Widgets 依赖，`ChannelConfig` 仍在 Core 层合规。
- **向后兼容**: `.pxc` 文件读取旧版本（无布局字段）时使用默认值，不破坏老 session 文件。

## ADDED Requirements

### Requirement: ChannelConfig 持久化 UI 布局状态

`ChannelConfig` SHALL 包含 `view_index`、`v_offset`、`own_height` 三个整数字段，作为 UI 布局状态的单一持久化状态源。默认值 `view_index=-1`、`v_offset=0`、`own_height=-1` 表示"未设置，使用派生默认"。

#### Scenario: 旧 .pxc 文件向后兼容
- **WHEN** 加载不包含布局字段的旧 `.pxc` 文件
- **THEN** 三个字段使用默认值（view_index=-1, v_offset=0, own_height=-1）
- **AND** 不报错，不丢失其他配置

#### Scenario: 布局状态在 tab 切换后恢复
- **WHEN** 用户在 tab A 调整通道顺序和高度
- **AND** 切换到 tab B 再切回 tab A
- **THEN** tab A 的通道顺序和高度与离开时一致

### Requirement: save_signal_config 写入布局状态

`SessionDocument::save_signal_config` SHALL 接受 `channel_layout` 参数（`std::map<int, ChannelLayoutState>`），将每个通道的布局状态写入对应 `ChannelConfig`。

#### Scenario: deactivate 保存布局
- **WHEN** `TabContext::deactivate` 被调用
- **THEN** 从 `_view->get_own_signals()` 收集每个通道的 view_index/v_offset/own_height
- **AND** 传入 `save_signal_config` 持久化到 `ChannelConfig`

## MODIFIED Requirements

### Requirement: rebuild_signals_from_config 从配置恢复布局

`View::rebuild_signals_from_config` SHALL 从 `ChannelConfig` 恢复 `view_index`、`v_offset`、`own_height`，不再显式重置。

- `view_index`：若 `config.view_index >= 0` 则使用配置值；否则按启用顺序派生（向后兼容）。
- `v_offset`：直接使用 `config.v_offset`。
- `own_height`：若 `config.own_height >= 0` 则使用配置值；否则 DSO/Analog 保持 -1（自动高度），Logic 由 Trace 构造函数使用主题默认。

#### Scenario: 重新采集后布局保留
- **WHEN** 用户调整通道顺序和高度
- **AND** 触发重新采集（`DSV_MSG_DEVICE_OPTIONS_UPDATED` → `rebuild_signals` → `rebuild_signals_from_config`）
- **THEN** 通道顺序和高度与采集前一致

#### Scenario: 首次创建通道使用派生默认
- **WHEN** `ChannelConfig.view_index == -1`（新通道，未持久化）
- **THEN** 按启用顺序派生 view_index（与旧行为一致）
- **AND** DSO/Analog 的 own_height 保持 -1（自动高度，与旧行为一致）

### Requirement: rebuild_signals 不二次重置布局

`View::rebuild_signals` 的 config-based 分支调用 `rebuild_signals_from_config` 后，SHALL NOT 再对 `own_height` 做任何重置。布局状态由 `rebuild_signals_from_config` 统一恢复。

## REMOVED Requirements

### Requirement: rebuild_signals_from_config 重置 view_index 和 own_height

**Reason**: 该行为是"无持久化状态源"时的临时补丁，导致用户调整被频繁重置。已被"从 ChannelConfig 恢复"取代。
**Migration**: [view.cpp:2154-2161](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/view/view.cpp#L2154) 的 `set_own_height(-1)` 和 `set_view_index(view_index++)` 删除，替换为从 config 恢复的逻辑。
