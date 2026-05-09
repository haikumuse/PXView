# Logic 模式软件毛刺滤波 Spec

## Why
PXView 的 Logic 模式当前仅有硬件级 1 采样周期滤波（SR_FILTER_1T），无法滤除宽度大于 1 个采样周期的毛刺脉冲。用户在实际使用中经常遇到窄脉冲干扰，需要可配置阈值的软件毛刺滤波功能，参考 atk-logic 项目的实现方案适配到 PXView 架构。

## What Changes
- 在 `LogicSnapshot` 中新增就地修改采样数据的方法（`set_sample_range`）和 mipmap 重建方法（`recalc_mipmap`）
- 在 `LogicSnapshot` 中新增毛刺滤波核心算法方法（`apply_glitch_filter`）
- 在 `SigSession` 中新增毛刺滤波流程控制（克隆数据、启动滤波、恢复原始数据）
- 在 `SessionData` 中新增原始数据备份指针和滤波状态管理
- 在 `DeviceOptionsDock` 中 Logic 模式下新增"毛刺过滤"分组框，支持按通道独立设置滤波阈值（1-999 采样周期）
- 在 `icallbacks.h` 中新增滤波相关消息常量
- 支持滤波设置的会话保存/恢复（通过 DeviceOptionsDock 的 get_session/set_session）

## Impact
- Affected specs: 无直接影响其他 spec
- Affected code:
  - `PXView/pv/data/logicsnapshot.h/cpp` — 新增数据修改和滤波方法
  - `PXView/pv/data/snapshot.h/cpp` — 基类可能需要扩展
  - `PXView/pv/sigsession.h/cpp` — 滤波流程控制
  - `PXView/pv/dock/deviceoptionsdock.h/cpp` — 新增毛刺过滤分组框 UI
  - `PXView/pv/interface/icallbacks.h` — 新增消息常量

## ADDED Requirements

### Requirement: LogicSnapshot 数据修改能力
系统 SHALL 提供 `LogicSnapshot::set_sample_range()` 方法，能够将指定通道的 [start, end) 采样区间设置为指定电平值，并正确处理以下情况：
- 目标区间跨越多个 LeafBlock
- 目标 LeafBlock 已被压缩释放（tog==0, lbp==NULL），需要重新分配内存
- 修改后清除对应 LeafBlock 的 mipmap 索引标记（使其需要重建）

#### Scenario: 修改已压缩的恒值块
- **WHEN** 调用 `set_sample_range()` 修改一个已被压缩释放（lbp==NULL）的 LeafBlock 中的部分采样
- **THEN** 系统重新分配 LeafBlock 内存，用原恒值填充整个块，再修改目标区间，并标记该块需要 mipmap 重建

#### Scenario: 修改跨越多个 LeafBlock 的区间
- **WHEN** 调用 `set_sample_range()` 修改跨越两个 LeafBlock 边界的区间
- **THEN** 系统分别处理每个 LeafBlock，对每个受影响的块标记 mipmap 重建

### Requirement: LogicSnapshot Mipmap 重建
系统 SHALL 提供 `LogicSnapshot::recalc_mipmap()` 方法，对指定通道的指定 LeafBlock 重新计算全部 4 级 Mip-map 索引和 RootNode 元数据（tog/first/last），确保边沿搜索等功能在数据修改后仍然正确。

#### Scenario: 数据修改后重建 mipmap
- **WHEN** `set_sample_range()` 修改了某个 LeafBlock 的数据
- **THEN** 后续调用 `recalc_mipmap()` 正确重建该块的 Level1/2/3 mipmap 和 RootNode 元数据
- **AND** 如果修改后整个块变为恒值（无跳变），则释放该块内存（与 `calc_mipmap` 的优化一致）

### Requirement: 毛刺滤波核心算法
系统 SHALL 在 `LogicSnapshot` 中提供 `apply_glitch_filter()` 方法，实现以下算法：
1. 对指定通道，从采样起点开始，利用 `get_nxt_edge()` 逐段扫描电平区间
2. 如果某段电平宽度 ≤ 阈值，则翻转该段电平（使其与前后电平一致）
3. 翻转后回退检查：如果翻转导致前后段合并后产生新的短脉冲，则回退重新检查
4. 所有修改完成后，对受影响的 LeafBlock 调用 `recalc_mipmap()` 重建索引

#### Scenario: 滤除短脉冲
- **WHEN** 通道数据为 `高(100) 低(3) 高(200)`，阈值设为 5
- **THEN** 宽度为 3 的低脉冲被翻转为高电平，结果为 `高(303)`

#### Scenario: 级联滤波
- **WHEN** 通道数据为 `高(100) 低(2) 高(2) 低(200)`，阈值设为 3
- **THEN** 宽度为 2 的高脉冲被翻转为低电平后，与前后低电平合并
- **AND** 回退检查确保合并结果正确，最终为 `低(304)`

### Requirement: 滤波流程控制（克隆 + 后处理）
系统 SHALL 在 `SigSession` 中实现毛刺滤波流程，采用"克隆原始数据 + 后处理"架构：
1. 用户触发滤波时，克隆当前 `_view_data` 中的 `LogicSnapshot` 作为工作副本
2. 在后台线程中对工作副本执行 `apply_glitch_filter()`
3. 滤波完成后，将 `_view_data` 切换到滤波后的数据
4. 保留原始数据备份，支持随时恢复
5. 再次执行滤波时，从原始数据重新克隆，避免累积误差

#### Scenario: 首次执行滤波
- **WHEN** 用户首次设置滤波参数并确认
- **THEN** 系统克隆当前 LogicSnapshot，在克隆数据上执行滤波，显示滤波后波形
- **AND** 原始数据被保留在备份中

#### Scenario: 取消滤波恢复原始数据
- **WHEN** 用户将所有通道的滤波阈值设为 0 或点击"恢复原始数据"
- **THEN** 系统将 `_view_data` 切换回原始数据备份，释放克隆数据

#### Scenario: 修改滤波参数重新滤波
- **WHEN** 用户修改滤波参数后再次确认
- **THEN** 系统从原始数据备份重新克隆，在新克隆上执行滤波，替换当前显示数据

#### Scenario: 采集新数据时清除滤波状态
- **WHEN** 开始新的数据采集
- **THEN** 系统清除滤波状态，释放克隆数据和原始备份，恢复正常的双缓冲采集流程

### Requirement: 毛刺滤波 UI 集成到 DeviceOptionsDock
系统 SHALL 在 `DeviceOptionsDock` 中 Logic 模式下新增"毛刺过滤"分组框（QGroupBox），位于"Channel"分组框和"Mode"分组框之间，包含以下元素：
1. 分组框标题："毛刺过滤"（Glitch Filter）
2. 每个已启用的逻辑通道一行，包含：通道启用复选框 + "≤" 标签 + 阈值 SpinBox(1-999) + "采样周期" 标签
3. 全选/取消全选按钮
4. 提示文字："勾选通道后，小于设定宽度的脉冲将被滤除"
5. "应用滤波"按钮 — 点击后启动滤波流程
6. "恢复原始数据"按钮 — 点击后恢复原始数据（仅在有滤波数据时可用）
7. 滤波状态标签 — 显示"已滤波"或"未滤波"

#### Scenario: Logic 模式下显示毛刺过滤分组框
- **WHEN** 设备工作在 Logic 模式
- **THEN** DeviceOptionsDock 在"Channel"和"Mode"分组框之间显示"毛刺过滤"分组框
- **AND** 分组框中列出所有已启用的逻辑通道

#### Scenario: 非 Logic 模式下隐藏毛刺过滤分组框
- **WHEN** 设备工作在 DSO 或 Analog 模式
- **THEN** 不显示"毛刺过滤"分组框

#### Scenario: 应用滤波
- **WHEN** 用户勾选通道并设置阈值后点击"应用滤波"按钮
- **THEN** 系统启动滤波流程，按钮变为禁用状态
- **AND** 状态栏显示滤波进度

#### Scenario: 恢复原始数据
- **WHEN** 用户点击"恢复原始数据"按钮
- **THEN** 系统恢复原始数据，释放克隆数据
- **AND** 滤波状态标签变为"未滤波"

#### Scenario: 无采集数据时
- **WHEN** 尚未采集数据
- **THEN** "应用滤波"和"恢复原始数据"按钮均为禁用状态

#### Scenario: 采集进行中
- **WHEN** 正在采集数据
- **THEN** "应用滤波"和"恢复原始数据"按钮均为禁用状态

#### Scenario: 滤波完成后
- **WHEN** 滤波流程完成
- **THEN** 波形自动刷新显示滤波后数据
- **AND** 滤波状态标签显示"已滤波"
- **AND** "恢复原始数据"按钮变为可用

### Requirement: 滤波进度反馈
系统 SHALL 在滤波处理过程中显示进度反馈，因为大数据量滤波可能耗时较长。

#### Scenario: 滤波进行中
- **WHEN** 后台线程正在执行毛刺滤波
- **THEN** 状态栏显示进度百分比和"正在执行毛刺滤波..."提示
- **AND** "应用滤波"按钮变为禁用状态，防止重复触发

#### Scenario: 滤波完成
- **WHEN** 滤波线程完成
- **THEN** 波形自动刷新显示滤波后数据
- **AND** 状态栏显示"毛刺滤波完成"

### Requirement: 滤波设置会话保存
系统 SHALL 通过 `DeviceOptionsDock` 的 `get_session()`/`set_session()` 机制支持将毛刺滤波参数保存到会话中，并在加载会话时恢复滤波设置。

#### Scenario: 保存会话
- **WHEN** 用户保存包含滤波数据的会话
- **THEN** 滤波参数（每通道阈值、是否启用）被写入会话 JSON 的 `glitch_filter` 字段

#### Scenario: 加载会话
- **WHEN** 用户加载包含滤波参数的会话文件
- **THEN** 系统读取滤波参数，恢复 UI 控件状态，自动执行毛刺滤波并显示结果
