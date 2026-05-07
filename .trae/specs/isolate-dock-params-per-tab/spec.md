# Dock 参数按标签独立保存 Spec

## Why
当前多标签页架构中，切换标签时右侧 Dock 的参数不会随标签切换而保存/恢复，导致用户在不同标签页的配置互相覆盖。需要让每个标签的 Dock 参数独立保存，切换标签时自动恢复到 Dock UI 和硬件。

## 核心设计原则
**只有活跃标签控制 Dock 行为**。切换标签时：
1. 保存当前标签的所有 Dock 参数到该标签的缓存
2. 从新标签的缓存恢复所有参数到 Dock UI 和硬件

所有参数（包括采样率、Vdiv、触发等硬件参数）都可以按标签独立保存，因为切换标签时只是重新配置硬件状态。

## What Changes
- **SessionDocument 添加所有 Dock 参数缓存字段**：搜索模式、测量配置、采集模式、采样率、采样深度、Vdiv、Coupling、触发配置等
- **各 Dock 的 bind_context/unbind_context 实现参数保存/恢复**
- **DeviceOptionsDock 实现 IContextAware 接口**
- **DsoTriggerDock 添加 get_session/set_session 序列化方法**
- **on_tab_changed 中添加所有 Dock 的 unbind/bind 调用**

## Impact
- Affected specs: fix-tab-channel-visibility（Signal._local_enabled）、isolate-decoder-per-tab（ProtocolDock 已独立）
- Affected code:
  - `pv/data/sessiondocument.h/cpp` — 添加缓存字段和序列化
  - `pv/dock/deviceoptionsdock.h/cpp` — 实现 IContextAware
  - `pv/dock/searchdock.cpp` — bind_context 恢复搜索模式
  - `pv/dock/measuredock.cpp` — bind_context 恢复测量配置
  - `pv/dock/triggerdock.cpp` — bind_context 恢复触发配置
  - `pv/dock/dsotrigerdock.h/cpp` — 添加序列化方法
  - `pv/toolbars/samplingbar.cpp` — bind_context 恢复采样参数
  - `pv/mainwindow.cpp` — on_tab_changed 中添加新组件的绑定

## 参数独立性总表

### 已独立（无需改动）
- 解码器列表+参数（SessionDocument::_decoder_stacks）
- 波形缩放/滚动位置（View::_scale/_offset）
- 波形排序/高度（View::_own_signals）
- 通道启用状态（Signal::_local_enabled）

### 需要添加独立保存

| 组件 | 参数 | 保存位置 | 恢复目标 |
|------|------|---------|---------|
| **SamplingBar** | 采样率 | SessionDocument | DeviceAgent + UI |
| **SamplingBar** | 采样深度/时间 | SessionDocument | DeviceAgent + UI |
| **SamplingBar** | 采集模式(Single/Repeat/Loop) | SessionDocument | SigSession + UI |
| **DeviceOptionsDock** | 通道使能 | SessionDocument | sr_channel->enabled + UI |
| **DeviceOptionsDock** | 通道模式 | SessionDocument | DeviceAgent + UI |
| **DeviceOptionsDock** | Operation Mode | SessionDocument | DeviceAgent + UI |
| **DeviceOptionsDock** | Vdiv(每通道) | SessionDocument | DeviceAgent + UI |
| **DeviceOptionsDock** | Coupling(每通道) | SessionDocument | DeviceAgent + UI |
| **DeviceOptionsDock** | Map配置(每通道) | SessionDocument | UI |
| **TriggerDock** | 全部触发参数 | SessionDocument | 硬件 + UI |
| **DsoTriggerDock** | 全部DSO触发参数 | SessionDocument | 硬件 + UI |
| **SearchDock** | 搜索模式(_pattern) | SessionDocument | UI |
| **MeasureDock** | 浮动测量开关 | SessionDocument | UI |
| **MeasureDock** | 距离/边沿测量行 | SessionDocument | UI |

## ADDED Requirements

### Requirement: SessionDocument 缓存所有 Dock 参数
SessionDocument SHALL 缓存所有 Dock 参数，在切换标签时保存/恢复。

#### Scenario: 切换标签时恢复采样参数
- **GIVEN** 标签A采样率100MHz/深度1M，标签B采样率10MHz/深度10M
- **WHEN** 用户从标签B切换到标签A
- **THEN** 硬件采样率恢复为100MHz，采样深度恢复为1M
- **AND** SamplingBar UI 显示100MHz/1M

#### Scenario: 切换标签时恢复搜索模式
- **GIVEN** 标签A的 SearchDock 中设置了通道0搜索"0101"
- **WHEN** 用户切换到标签B再切回标签A
- **THEN** 标签A的 SearchDock 中通道0仍显示"0101"

#### Scenario: 切换标签时恢复测量配置
- **GIVEN** 标签A的 MeasureDock 中开启了浮动测量，添加了2行距离测量
- **WHEN** 用户切换到标签B再切回标签A
- **THEN** 标签A的 MeasureDock 浮动测量仍开启，2行距离测量仍存在

#### Scenario: 切换标签时恢复采集模式
- **GIVEN** 标签A设置为 Repeat 模式，标签B设置为 Single 模式
- **WHEN** 用户从标签B切换到标签A
- **THEN** SamplingBar 显示 Repeat 模式

#### Scenario: 切换标签时恢复触发配置
- **GIVEN** 标签A设置了上升沿触发CH0，标签B设置了下降沿触发CH1
- **WHEN** 用户从标签B切换到标签A
- **THEN** 硬件触发配置恢复为上升沿触发CH0
- **AND** TriggerDock UI 显示上升沿触发CH0

#### Scenario: 切换标签时恢复 Vdiv
- **GIVEN** 标签A的CH0设置为1V/div，标签B的CH0设置为5V/div
- **WHEN** 用户从标签B切换到标签A
- **THEN** 硬件 Vdiv 恢复为1V/div
- **AND** DeviceOptionsDock 显示1V/div

### Requirement: DeviceOptionsDock 实现 IContextAware
DeviceOptionsDock SHALL 实现 IContextAware 接口，在 bind_context 时恢复通道配置，在 unbind_context 时保存当前配置。

#### Scenario: 切换标签时 DeviceOptionsDock 刷新
- **GIVEN** 标签A为16通道配置，标签B为8通道配置
- **WHEN** 用户从标签B切换到标签A
- **THEN** DeviceOptionsDock 显示16通道配置

### Requirement: DsoTriggerDock 添加序列化方法
DsoTriggerDock SHALL 添加 get_session/set_session 方法，支持触发配置的 JSON 序列化和反序列化。

#### Scenario: 保存/恢复 DSO 触发配置
- **WHEN** 调用 DsoTriggerDock::get_session()
- **THEN** 返回包含触发位置、触发源、触发类型、Holdoff、噪声灵敏度的 JSON 对象
- **WHEN** 调用 DsoTriggerDock::set_session(json)
- **THEN** 从 JSON 恢复触发配置到硬件和 UI

## MODIFIED Requirements

### Requirement: SearchDock::bind_context 恢复搜索模式
**Before**: bind_context 调用 rebuild_pattern() 重建通道输入控件，不恢复之前的搜索条件
**After**: bind_context 从 SessionDocument 恢复搜索模式到 UI 控件

### Requirement: MeasureDock::bind_context 恢复测量配置
**Before**: bind_context 只切换 session/view 指针
**After**: bind_context 从 SessionDocument 恢复浮动测量开关、距离/边沿测量行

### Requirement: SamplingBar::bind_context 恢复采样参数
**Before**: bind_context 只切换 session/view 指针，设置 readonly
**After**: bind_context 从 SessionDocument 恢复采样率、采样深度、采集模式到硬件和 UI

## REMOVED Requirements
无
