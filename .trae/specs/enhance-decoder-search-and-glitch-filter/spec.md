# 解码器搜索过滤 + 毛刺滤波方向性增强 Spec

## Why
1. 解码器搜索弹窗目前混合显示 C 解码器和 Python 解码器（共 250+ 个），用户无法按类型快速筛选，查找效率低。
2. 当前毛刺滤波对高脉冲和低脉冲完全对称处理（both 模式），但实际场景中高电平的干扰是低脉冲杂波、低电平的干扰是高脉冲杂波，需要方向性滤波。

## What Changes
- 在 `SearchComboBox` 搜索弹窗中新增解码器类型过滤下拉框（All / C / Python）
- 在 `SearchDataItem` 中新增 `_is_c_decoder` 字段，用于过滤判断
- 在 `SignalProcessingDock` 毛刺过滤区域新增滤波方向选项（Both / High / Low）
- 在 `LogicSnapshot::apply_glitch_filter` 中新增 `filter_mode` 参数，支持三种滤波方向
- 在 `SigSession` 滤波流程中传递滤波方向参数
- 在 `SessionDocument` 会话保存中新增滤波方向字段

## Impact
- Affected specs: `add-logic-glitch-filter`（滤波算法新增方向参数）、`add-signal-processing-dock`（UI 新增方向选项）
- Affected code:
  - `PXView/pv/dock/searchcombobox.h` — SearchDataItem 新增字段
  - `PXView/pv/dock/searchcombobox.cpp` — 新增过滤 UI 和过滤逻辑
  - `PXView/pv/dock/protocoldock.cpp` — 传递 is_c_decoder 信息
  - `PXView/pv/dock/signalprocessingdock.h` — 新增方向选择成员
  - `PXView/pv/dock/signalprocessingdock.cpp` — 新增方向选择 UI、传递方向参数
  - `PXView/pv/data/logicsnapshot.h` — apply_glitch_filter 新增 filter_mode 参数
  - `PXView/pv/data/logicsnapshot.cpp` — 实现方向性滤波算法
  - `PXView/pv/sigsession.h` — set_glitch_filter 新增方向参数
  - `PXView/pv/sigsession.cpp` — 传递方向参数
  - `PXView/pv/data/sessiondocument.h` — 新增滤波方向保存字段

## ADDED Requirements

### Requirement: 解码器搜索类型过滤
系统 SHALL 在解码器搜索弹窗（SearchComboBox）中提供类型过滤功能，允许用户按解码器实现类型筛选。

#### Scenario: 搜索弹窗显示类型过滤
- **WHEN** 用户点击 ProtocolDock 的搜索按钮打开搜索弹窗
- **THEN** 搜索输入框下方显示一个过滤下拉框，包含三个选项："All"、"C"、"Python"
- **AND** 默认选中"All"

#### Scenario: 选择 C 解码器过滤
- **WHEN** 用户在过滤下拉框中选择"C"
- **THEN** 列表中只显示 C 解码器（`srd_decoder::is_c_decoder == TRUE` 的解码器）
- **AND** 搜索关键词过滤仍在 C 解码器范围内生效

#### Scenario: 选择 Python 解码器过滤
- **WHEN** 用户在过滤下拉框中选择"Python"
- **THEN** 列表中只显示 Python 解码器（`srd_decoder::is_c_decoder == FALSE` 的解码器）
- **AND** 搜索关键词过滤仍在 Python 解码器范围内生效

#### Scenario: 选择 All 过滤
- **WHEN** 用户在过滤下拉框中选择"All"
- **THEN** 列表显示所有解码器（C 和 Python 混合），与当前行为一致

#### Scenario: 过滤与关键词搜索联动
- **WHEN** 用户同时设置了类型过滤和关键词
- **THEN** 列表同时满足类型过滤和关键词匹配两个条件

### Requirement: 毛刺滤波方向选项
系统 SHALL 在毛刺过滤 UI 中提供滤波方向选择，支持三种模式：Both（双向）、High（仅滤高电平上的低脉冲杂波）、Low（仅滤低电平上的高脉冲杂波）。

#### Scenario: 毛刺过滤区域显示方向选项
- **WHEN** 设备工作在 Logic 模式且 SignalProcessingDock 可见
- **THEN** 毛刺过滤区域在通道列表上方显示一个方向选择下拉框
- **AND** 下拉框包含三个选项："Both"（双向滤波）、"High"（高电平滤波）、"Low"（低电平滤波）
- **AND** 默认选中"Both"（与当前行为一致）

#### Scenario: Both 模式滤波行为
- **WHEN** 用户选择"Both"模式并应用滤波
- **THEN** 对高脉冲和低脉冲均进行滤波（宽度 ≤ 阈值的脉冲被滤除），与当前行为完全一致

#### Scenario: High 模式滤波行为
- **WHEN** 用户选择"High"模式并应用滤波
- **THEN** 仅对高电平区间上的低脉冲杂波进行滤波
- **AND** 具体行为：当基准电平为高（1）时，宽度 ≤ 阈值的低脉冲被拉高（滤除低电平杂波）；当基准电平为低（0）时，宽度 ≤ 阈值的高脉冲**不被滤除**（保留高电平信号）

#### Scenario: Low 模式滤波行为
- **WHEN** 用户选择"Low"模式并应用滤波
- **THEN** 仅对低电平区间上的高脉冲杂波进行滤波
- **AND** 具体行为：当基准电平为低（0）时，宽度 ≤ 阈值的高脉冲被拉低（滤除高电平杂波）；当基准电平为高（1）时，宽度 ≤ 阈值的低脉冲**不被滤除**（保留低电平信号）

#### Scenario: 方向选项与阈值配合
- **WHEN** 用户修改方向选项或阈值后点击"应用滤波"
- **THEN** 系统从原始备份重新计算，按新的方向和阈值执行滤波

### Requirement: 滤波方向会话保存
系统 SHALL 支持将滤波方向参数保存到会话中。

#### Scenario: 保存会话
- **WHEN** 用户保存包含滤波数据的会话
- **THEN** 滤波方向（"both"/"high"/"low"）被写入会话 JSON 的 `glitch_filter` 字段中

#### Scenario: 加载会话
- **WHEN** 用户加载包含滤波方向参数的会话文件
- **THEN** 系统恢复方向选择 UI 状态，并按保存的方向执行滤波

## MODIFIED Requirements

### Requirement: LogicSnapshot 毛刺滤波算法
`LogicSnapshot::apply_glitch_filter()` 新增 `filter_mode` 参数（枚举类型：`GLITCH_FILTER_BOTH`、`GLITCH_FILTER_HIGH`、`GLITCH_FILTER_LOW`），默认值为 `GLITCH_FILTER_BOTH`。

算法修改点（在判定 `pulse_len <= threshold` 后）：
- **BOTH 模式**：与当前逻辑完全一致，无论基准电平高低，所有短脉冲均被滤除
- **HIGH 模式**：仅当 `accepted_level == true`（基准为高电平）时，才将低脉冲拉高；当 `accepted_level == false` 时，跳过该脉冲（不滤除高电平上的杂波）
- **LOW 模式**：仅当 `accepted_level == false`（基准为低电平）时，才将高脉冲拉低；当 `accepted_level == true` 时，跳过该脉冲（不滤除低电平上的杂波）

### Requirement: SigSession 滤波流程
`SigSession::set_glitch_filter()` 新增方向参数，传递到 `glitch_filter_task()` 和 `LogicSnapshot::apply_glitch_filter()`。

### Requirement: SignalProcessingDock 毛刺过滤 UI
在毛刺过滤区域的通道列表上方，新增一行包含方向选择 QComboBox 的布局，与现有 UI 风格一致。

## REMOVED Requirements
无
