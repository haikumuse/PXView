# 每标签页独立信号配置 Spec（方案B）

## Why
当前多标签页架构中，`SigSession::_signals` 是全局唯一的信号列表，所有 View 通过 `rebuild_signals()` 从该全局列表克隆信号。当切换标签改变硬件配置（通道数、使能状态、工作模式等）时，全局信号列表被重建，导致非活跃标签的 View 信号列表被意外改变，历史数据与信号列表不匹配。需要让每个标签页持有独立的信号配置，View 从自己的 SessionDocument 重建信号，从根本上解决信号列表全局唯一的矛盾。

## 核心设计原则
**每个 SessionDocument 持有独立的信号配置快照（SignalConfig），View 从自己的 Document 重建信号，不再依赖全局 `SigSession::_signals`。** 切换标签时恢复硬件配置到该标签的期望状态（非采集时），采集时仅保存配置不立即应用。

## What Changes
- **SessionDocument 新增 SignalConfig 结构体**：保存该标签的完整硬件配置快照（work_mode、operation_mode、channel_mode、每通道 enabled/vdiv/coupling 等）
- **SessionDocument 新增 `_signal_config` 字段**：每次采集结束或用户修改配置时自动保存
- **View 新增 `rebuild_signals_from_config()` 方法**：根据 SignalConfig 创建独立信号对象，不依赖 `SigSession::_signals`
- **TabContext::activate() 恢复硬件配置**：非采集状态下从 SignalConfig 恢复硬件寄存器；采集状态下将配置标记为"待生效"
- **SessionDocument 新增 `_pending_device_config` 字段**：采集中切换的配置先暂存，停止采集后自动应用
- **DeviceOptionsDock::get_session() 改为从 UI 控件读取**：确保 unbind_context 保存的是该标签的配置而非硬件实时值
- **ProtocolDock::unbind_context() 实现状态保存**：保存搜索关键词、展开状态等 UI 状态
- **MeasureDock::cursor_row_info 恢复 channelIndex**：序列化时保存 channelIndex 字段
- **remove_tab 中补全 Dock 绑定流程**：删除标签后对新活跃标签执行完整的 bind_context

## Impact
- Affected specs: isolate-tab-data-snapshot（Signal 深拷贝已实现）、isolate-dock-params-per-tab（Dock 参数保存已实现）、isolate-decoder-per-tab（解码器隔离已实现）
- Affected code:
  - `pv/data/sessiondocument.h/cpp` — 新增 SignalConfig 结构体、_signal_config、_pending_device_config
  - `pv/view/view.h/cpp` — 新增 rebuild_signals_from_config()、修改 rebuild_signals() 优先从 Document 重建
  - `pv/tabcontext.h/cpp` — activate() 恢复硬件配置、deactivate() 保存 SignalConfig
  - `pv/mainwindow.cpp` — on_tab_changed() 适配、remove_tab() 补全 Dock 绑定
  - `pv/dock/deviceoptionsdock.cpp` — get_session() 改为从 UI 读取、set_session() 条件应用
  - `pv/dock/protocoldock.cpp` — unbind_context() 保存 UI 状态
  - `pv/dock/measuredock.cpp` — cursor_row_info 序列化增加 channelIndex
  - `pv/sigsession.h/cpp` — 新增 apply_device_config() 方法、采集停止后检查 pending config

## ADDED Requirements

### Requirement 1: SessionDocument 持有独立 SignalConfig
每个 SessionDocument SHALL 持有一个 SignalConfig 结构体，记录该标签页的完整硬件配置快照。SignalConfig 在采集结束时自动从当前硬件状态保存，在用户修改 DeviceOptionsDock 配置时立即更新。

#### Scenario: 采集结束时自动保存 SignalConfig
- **GIVEN** 标签 A 处于 LIVE 状态，硬件配置为 LOGIC/16通道/buffer 模式
- **WHEN** 采集帧结束（on_frame_ended）
- **THEN** 标签 A 的 SessionDocument._signal_config 自动保存当前硬件配置
- **AND** _signal_config 包含 work_mode=LOGIC、channel_mode=16ch、operation_mode=buffer
- **AND** _signal_config 包含每通道的 enabled、vdiv、coupling 状态

#### Scenario: 用户修改配置时立即保存 SignalConfig
- **GIVEN** 标签 A 是活跃标签
- **WHEN** 用户在 DeviceOptionsDock 中将通道模式从 8ch 改为 16ch
- **THEN** 标签 A 的 SessionDocument._signal_config 立即更新 channel_mode=16ch
- **AND** 切换到其他标签再切回时，DeviceOptionsDock 显示 16ch 配置

#### Scenario: 新建标签继承当前 SignalConfig
- **GIVEN** 当前活跃标签的硬件配置为 LOGIC/16通道
- **WHEN** 用户点击 "+" 创建新标签
- **THEN** 新标签的 SessionDocument._signal_config 复制当前活跃标签的配置
- **AND** 新标签的 View 根据 SignalConfig 创建对应的信号对象

### Requirement 2: View 从 SignalConfig 重建信号
View SHALL 新增 `rebuild_signals_from_config(const SignalConfig &config)` 方法，根据 SignalConfig 创建独立的信号对象列表，不再依赖 `SigSession::_signals` 全局列表。

#### Scenario: 从 SignalConfig 创建信号
- **GIVEN** 标签 A 的 SignalConfig 为 LOGIC/16通道/通道0-7使能
- **WHEN** 标签 A 被激活，调用 rebuild_signals_from_config()
- **THEN** 创建 16 个 LogicSignal 对象，通道 0-7 的 enabled=true，通道 8-15 的 enabled=false
- **AND** 信号对象的 view_index 按通道顺序分配
- **AND** 信号对象的数据指向标签 A 的 SessionDocument 的 Snapshot

#### Scenario: 非活跃标签信号不受全局信号列表变化影响
- **GIVEN** 标签 A（8通道）和标签 B（16通道）各有独立信号
- **WHEN** 从标签 A 切换到标签 B，SigSession::_signals 被重建为 16 通道
- **THEN** 标签 A 的 View 的 _own_signals 仍为 8 个 LogicSignal
- **AND** 标签 A 的信号列表不受全局变化影响

#### Scenario: 有数据的标签从 Document 重建信号
- **GIVEN** 标签 A 有历史数据（8通道采集），SessionDocument 有 SignalConfig
- **WHEN** 切换到标签 A
- **THEN** View 从 SessionDocument._signal_config 重建 8 个 LogicSignal
- **AND** 信号的数据指向 SessionDocument 的 LogicSnapshot
- **AND** 波形正常显示

### Requirement 3: TabContext::activate() 恢复硬件配置
标签激活时 SHALL 根据是否正在采集，决定是否立即恢复硬件配置。

#### Scenario: 非采集状态下恢复硬件配置
- **GIVEN** 标签 A 的 SignalConfig 为 LOGIC/16通道/stream 模式
- **AND** SigSession 当前未在采集
- **WHEN** 标签 A 被激活
- **THEN** 硬件配置恢复为 LOGIC/16通道/stream 模式
- **AND** SigSession::reload() 被调用更新信号列表
- **AND** DeviceOptionsDock UI 显示 16通道/stream 模式

#### Scenario: 采集中不恢复硬件配置，标记为待生效
- **GIVEN** 标签 A 的 SignalConfig 为 LOGIC/16通道
- **AND** 标签 B 的 SignalConfig 为 LOGIC/8通道
- **AND** SigSession 正在以 8 通道配置采集
- **WHEN** 从标签 B 切换到标签 A
- **THEN** 硬件配置不变（仍为 8 通道采集）
- **AND** 标签 A 的 _pending_device_config 保存 16 通道配置
- **AND** View 从 SignalConfig 重建 16 通道的信号对象（仅 UI 层面）
- **AND** 用户看到标签 A 的信号列表为 16 通道（但只有 8 通道有实时数据）

#### Scenario: 采集停止后自动应用待生效配置
- **GIVEN** 标签 A 有 _pending_device_config（16通道）
- **AND** 采集刚刚停止
- **WHEN** 标签 A 仍然是活跃标签
- **THEN** 自动应用 _pending_device_config 到硬件
- **AND** _pending_device_config 被清除
- **AND** DeviceOptionsDock UI 更新为 16 通道

### Requirement 4: DeviceOptionsDock::get_session() 从 UI 控件读取
DeviceOptionsDock::get_session() SHALL 从 UI 控件（checkbox、radio button 等）读取状态，而非从硬件实时值读取，确保 unbind_context 保存的是该标签页的配置。

#### Scenario: unbind_context 保存 UI 状态
- **GIVEN** 标签 A 的 DeviceOptionsDock UI 显示 16 通道使能
- **AND** 由于某种原因硬件实际只有 8 通道使能（如采集中间接修改）
- **WHEN** 用户从标签 A 切换到标签 B
- **THEN** 标签 A 的 SessionDocument._dock_device_options_session 保存 UI 显示的 16 通道状态
- **AND** 不保存硬件实际的 8 通道状态

#### Scenario: bind_context 恢复 UI 状态到硬件
- **GIVEN** 标签 A 的 SessionDocument 保存了 16 通道配置
- **AND** SigSession 未在采集
- **WHEN** 切换到标签 A
- **THEN** DeviceOptionsDock::set_session() 将 16 通道配置写入硬件
- **AND** UI 更新为 16 通道

### Requirement 5: ProtocolDock::unbind_context() 保存 UI 状态
ProtocolDock::unbind_context() SHALL 保存当前 UI 状态（搜索关键词、协议层展开/折叠状态），切换回该标签时恢复。

#### Scenario: 保存和恢复搜索关键词
- **GIVEN** 标签 A 的 ProtocolDock 搜索框中输入了 "SPI"
- **WHEN** 用户从标签 A 切换到标签 B
- **THEN** "SPI" 被保存到标签 A 的 SessionDocument
- **WHEN** 用户切回标签 A
- **THEN** ProtocolDock 搜索框恢复显示 "SPI"

#### Scenario: 保存和恢复协议层展开状态
- **GIVEN** 标签 A 的 ProtocolDock 中 SPI 协议层处于展开状态
- **WHEN** 用户从标签 A 切换到标签 B 再切回
- **THEN** SPI 协议层仍处于展开状态

### Requirement 6: MeasureDock cursor_row_info 完整序列化
MeasureDock 的 cursor_row_info 序列化 SHALL 包含 channelIndex 字段，恢复时正确还原。

#### Scenario: 保存和恢复 channelIndex
- **GIVEN** 标签 A 的 MeasureDock 有一条边沿测量行，channelIndex=2
- **WHEN** 用户从标签 A 切换到标签 B 再切回
- **THEN** 该边沿测量行的 channelIndex 仍为 2

### Requirement 7: remove_tab 补全 Dock 绑定流程
MainWindow::remove_tab() 在删除标签后 SHALL 对新的活跃标签执行完整的 bind_context 流程，确保 Dock 状态正确。

#### Scenario: 删除标签后 Dock 绑定到新活跃标签
- **GIVEN** 有标签 A（活跃）和标签 B
- **WHEN** 用户删除标签 A
- **THEN** 标签 B 变为活跃标签
- **AND** 所有 Dock 的 bind_context() 被调用，绑定到标签 B 的 TabContext
- **AND** Dock UI 显示标签 B 的配置

### Requirement 8: work_mode 跨标签限制
当不同标签页的 SignalConfig 中 work_mode 不同时，系统 SHALL 在切换时给出警告或自动处理。

#### Scenario: 切换到不同 work_mode 的标签（非采集状态）
- **GIVEN** 标签 A 为 LOGIC 模式，标签 B 为 DSO 模式
- **AND** SigSession 未在采集
- **WHEN** 从标签 A 切换到标签 B
- **THEN** 调用 SigSession::switch_work_mode(DSO) 切换工作模式
- **AND** 全局信号列表被重建
- **AND** 标签 B 的 View 从自己的 SignalConfig 重建信号
- **AND** 标签 A 的 View 信号列表不受影响（因为从 Document 重建）

#### Scenario: 切换到不同 work_mode 的标签（采集中）
- **GIVEN** 标签 A 为 LOGIC 模式（正在采集），标签 B 为 DSO 模式
- **WHEN** 从标签 A 切换到标签 B
- **THEN** 不切换 work_mode（采集不能中断）
- **AND** 标签 B 的 _pending_device_config 保存 DSO 模式配置
- **AND** 标签 B 的 View 仍从 SignalConfig 重建 DSO 信号（UI 层面），但无实时数据
- **AND** 停止采集后自动应用 DSO 配置

## MODIFIED Requirements

### Requirement: View::rebuild_signals() 信号来源
**Before**: 从 `_data_source->get_signals()`（全局 SigSession::_signals）克隆信号
**After**: 优先从 `_document->_signal_config` 重建信号；如果 Document 无 SignalConfig，回退到从 `_data_source->get_signals()` 克隆

### Requirement: TabContext::activate() 操作
**Before**: 仅调用 `_session->set_active_document(_document)` 和 `_view->set_data_document/set_data_source`
**After**: 额外执行：1) 从 SignalConfig 恢复硬件配置（非采集时）；2) 从 SignalConfig 重建 View 信号；3) 采集中将配置标记为 pending

### Requirement: DeviceOptionsDock::get_session() 数据来源
**Before**: 从 `_device_agent` 读取硬件实时值
**After**: 从 UI 控件读取用户设置的值

### Requirement: ProtocolDock::unbind_context() 实现
**Before**: 空实现
**After**: 保存搜索关键词、协议层展开状态到 SessionDocument

### Requirement: MainWindow::remove_tab() Dock 绑定
**Before**: 删除标签后不执行 Dock bind_context
**After**: 删除标签后对新活跃标签执行完整的 unbind_context + bind_context 流程

## REMOVED Requirements

### Requirement: View 信号列表依赖全局 SigSession::_signals
**Reason**: 每个 View 从自己的 SessionDocument._signal_config 重建信号，不再依赖全局信号列表
**Migration**: View::rebuild_signals() 优先调用 rebuild_signals_from_config()，仅当 Document 无 SignalConfig 时回退到旧逻辑

### Requirement: 切换标签时必须重建全局信号列表
**Reason**: 全局信号列表仅用于实时采集标签的数据源，非活跃标签从自己的 SignalConfig 重建信号
**Migration**: SigSession::_signals 仍用于实时采集，但不再影响非活跃标签的 View
