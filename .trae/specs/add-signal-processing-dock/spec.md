# 信号处理 Dock（毛刺过滤 + 信号取反）Spec

## Why
当前毛刺过滤功能嵌入在 DeviceOptionsDock 中，与设备配置混在一起，不利于扩展。用户需要将毛刺过滤独立为一个新的侧边栏 Dock，并新增信号取反功能（1→0, 0→1），信号处理流程为：原始信号 → 信号取反 → 毛刺滤波。

## What Changes
- 新建 `pv/dock/signalprocessingdock.h/.cpp`，创建 `SignalProcessingDock` 类，包含毛刺过滤和信号取反两个功能区域
- 在 `LogicSnapshot` 中新增 `invert_channel()` 方法，实现按通道翻转所有采样位（0→1, 1→0）
- 在 `SigSession` 中新增 `set_signal_invert()` / `clear_signal_invert()` / `is_signal_invert_active()` 方法
- 在 `SessionData` 中新增 `_signal_invert_active` 和 `_signal_invert_channels` 字段
- 从 `DeviceOptionsDock` 中移除毛刺过滤相关代码（UI、成员变量、方法）
- 在 `MainWindow` 中注册新的侧边栏 Dock 项（SIDEBAR_SIGNAL_PROCESSING），使用 audio-waveform 图标
- 在侧边栏中插入新项（位于 Options 和 Log 之间）
- 新增 SVG 图标文件 `audio-waveform.svg`（dark/light 两套）
- 在 `PXView.qrc` 中注册新图标
- 在 `icallbacks.h` 中新增信号取反相关消息常量
- 在 `SessionDocument` 中新增 `_dock_signal_processing_session` 字段
- 在 `DockOptions` 中新增 `signalProcessingDock` 字段
- 修改信号处理流程：先应用取反，再应用毛刺滤波（取反作用于原始数据，滤波作用于取反后数据）

## Impact
- Affected specs: `add-logic-glitch-filter`（毛刺过滤 UI 从 DeviceOptionsDock 迁移到新 Dock）
- Affected code:
  - `PXView/pv/dock/signalprocessingdock.h`（新建）
  - `PXView/pv/dock/signalprocessingdock.cpp`（新建）
  - `PXView/pv/dock/deviceoptionsdock.h`（修改：移除毛刺过滤成员）
  - `PXView/pv/dock/deviceoptionsdock.cpp`（修改：移除毛刺过滤代码）
  - `PXView/pv/data/logicsnapshot.h`（修改：新增 invert_channel）
  - `PXView/pv/data/logicsnapshot.cpp`（修改：实现 invert_channel）
  - `PXView/pv/sigsession.h`（修改：新增信号取反方法）
  - `PXView/pv/sigsession.cpp`（修改：实现信号取反流程，修改毛刺滤波流程以支持取反前置）
  - `PXView/pv/mainwindow.h`（修改：新增 SIDEBAR_SIGNAL_PROCESSING 枚举、drawer page、dock 成员）
  - `PXView/pv/mainwindow.cpp`（修改：注册新 Dock、创建 drawer 页面、处理点击事件）
  - `PXView/pv/interface/icallbacks.h`（修改：新增消息常量）
  - `PXView/pv/data/sessiondocument.h`（修改：新增会话字段）
  - `PXView/pv/config/appconfig.h`（修改：DockOptions 新增字段）
  - `PXView/icons/dark/audio-waveform.svg`（新建）
  - `PXView/icons/light/audio-waveform.svg`（新建）
  - `PXView/PXView.qrc`（修改：注册新图标）
  - `CMakeLists.txt`（修改：新增源文件）

## ADDED Requirements

### Requirement: 信号取反核心算法
系统 SHALL 在 `LogicSnapshot` 中提供 `invert_channel(int sig_index)` 方法，对指定通道的所有采样位执行翻转操作（0→1, 1→0），并正确重建 mipmap 索引。

#### Scenario: 对通道执行取反
- **WHEN** 调用 `invert_channel(sig_index)` 对某通道取反
- **THEN** 该通道所有采样位被翻转（0→1, 1→0）
- **AND** 受影响的 LeafBlock 的 mipmap 索引被正确重建
- **AND** 取反后 RootNode 的 tog/first/last 元数据正确更新

#### Scenario: 取反跨越多个 LeafBlock
- **WHEN** 通道数据跨越多个 LeafBlock
- **THEN** 每个 LeafBlock 的数据都被正确翻转
- **AND** 每个 LeafBlock 的 mipmap 被正确重建

### Requirement: 信号取反流程控制
系统 SHALL 在 `SigSession` 中实现信号取反流程，采用与毛刺滤波类似的"备份原始数据 + 后处理"架构：
1. 用户触发取反时，如果没有备份则先备份原始数据
2. 从原始数据恢复后，先应用取反，再应用毛刺滤波
3. 信号流向：原始信号 → 信号取反 → 毛刺滤波
4. 取反和滤波可以独立开关

#### Scenario: 首次启用信号取反
- **WHEN** 用户首次启用某通道的信号取反
- **THEN** 系统备份当前原始数据（如果尚未备份）
- **AND** 从原始数据恢复后，对指定通道执行取反
- **AND** 如果毛刺滤波处于激活状态，在取反后数据上重新执行滤波
- **AND** 波形刷新显示取反后（或取反+滤波后）的数据

#### Scenario: 取消信号取反
- **WHEN** 用户取消所有通道的信号取反
- **THEN** 系统从原始备份恢复数据
- **AND** 如果毛刺滤波仍激活，在恢复的原始数据上重新执行滤波
- **AND** 波形刷新

#### Scenario: 取反和滤波同时启用
- **WHEN** 信号取反和毛刺滤波同时启用
- **THEN** 数据处理顺序为：原始数据 → 取反 → 滤波
- **AND** 修改取反设置时，从原始数据重新计算（先取反再滤波）
- **AND** 修改滤波设置时，在取反后数据上重新滤波

#### Scenario: 采集新数据时清除取反状态
- **WHEN** 开始新的数据采集
- **THEN** 系统清除取反状态，释放备份，恢复正常双缓冲采集流程

### Requirement: 信号处理 Dock UI
系统 SHALL 提供独立的 `SignalProcessingDock` 面板，包含信号取反和毛刺过滤两个功能区域，仅在 Logic 模式下显示内容。

#### Scenario: 信号取反区域
- **WHEN** 设备工作在 Logic 模式且 SignalProcessingDock 可见
- **THEN** 面板顶部显示"信号取反"区域
- **AND** 每个已启用的逻辑通道一行：通道复选框（Ch0, Ch1, ...）
- **AND** "全选"/"取消全选"按钮
- **AND** "应用取反"按钮 — 点击后启动取反流程
- **AND** "恢复原始数据"按钮 — 点击后恢复原始数据（仅在有处理时可用）
- **AND** 取反状态标签

#### Scenario: 毛刺过滤区域
- **WHEN** 设备工作在 Logic 模式且 SignalProcessingDock 可见
- **THEN** 面板下方显示"毛刺过滤"区域
- **AND** 每个已启用的逻辑通道一行：通道启用复选框 + "≤" 标签 + 阈值 SpinBox(1-999) + "采样周期" 标签
- **AND** "全选"/"取消全选"按钮
- **AND** 提示文字
- **AND** "应用滤波"按钮
- **AND** "恢复原始数据"按钮
- **AND** 滤波状态标签

#### Scenario: 非 Logic 模式
- **WHEN** 设备工作在 DSO 或 Analog 模式
- **THEN** 面板显示提示"此功能仅在 Logic 模式下可用"

### Requirement: 侧边栏注册
系统 SHALL 在侧边栏中新增"信号处理"Dock 项，位于 Options 和 Log 之间。

#### Scenario: 侧边栏显示
- **WHEN** 侧边栏可见
- **THEN** 在 Options 和 Log 之间显示"信号处理"按钮，使用 audio-waveform 图标
- **AND** 点击按钮打开/关闭信号处理 Dock

#### Scenario: 点击信号处理侧边栏按钮
- **WHEN** 用户点击侧边栏的信号处理按钮
- **THEN** SlidingDrawer 打开信号处理页面
- **AND** 其他已打开的 Dock 自动关闭（互斥）

### Requirement: 信号处理会话保存
系统 SHALL 支持将信号取反和毛刺滤波参数保存到会话中。

#### Scenario: 保存会话
- **WHEN** 用户保存包含信号处理数据的会话
- **THEN** 取反参数（每通道是否取反）和滤波参数被写入会话 JSON

#### Scenario: 加载会话
- **WHEN** 用户加载包含信号处理参数的会话文件
- **THEN** 系统恢复 UI 控件状态，自动执行信号处理并显示结果

## MODIFIED Requirements

### Requirement: DeviceOptionsDock 移除毛刺过滤
`DeviceOptionsDock` 中的毛刺过滤相关代码（`build_glitch_filter_panel`、`on_apply_glitch_filter`、`on_restore_original_data`、`_glitch_checkBox_list`、`_glitch_spinbox_list` 等）全部移除。毛刺过滤 UI 迁移到新的 `SignalProcessingDock`。

### Requirement: DockOptions 结构体
`DockOptions` 新增 `signalProcessingDock` 布尔字段。

### Requirement: 侧边栏枚举
`MainWindow` 的 SIDEBAR_* 枚举新增 `SIDEBAR_SIGNAL_PROCESSING`，位于 `SIDEBAR_OPTIONS` 和 `SIDEBAR_LOG` 之间。后续枚举值相应调整。

### Requirement: SigSession 毛刺滤波流程适配取反
`SigSession::glitch_filter_task` 在执行滤波前需检查信号取反状态：如果取反激活，应从取反后的数据上执行滤波，而非直接从原始备份上执行。整体流程变为：从原始备份恢复 → 应用取反 → 应用滤波。

## REMOVED Requirements

### Requirement: DeviceOptionsDock 中的毛刺过滤 UI
**Reason**: 毛刺过滤功能迁移到独立的 SignalProcessingDock
**Migration**: 所有毛刺过滤 UI 代码从 DeviceOptionsDock 移至 SignalProcessingDock
