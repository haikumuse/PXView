# 波形分组卡片显示 Spec

## Why
当前所有波形（逻辑信号和解码器）平铺显示，没有视觉分组。解码器与其源信号之间的关联关系不够直观。需要用卡片式分组将同组波形框在一起，使信号与解码器的从属关系一目了然，同时支持组内和组间的拖拽交换。

## What Changes
- 新增分组概念：解码器包含的源信号与该解码器同属一组；不属于任何解码器的信号各自成一组
- 在 Viewport 和 Header 中绘制分组卡片背景（圆角矩形），颜色在 QSS 中定义
- 支持组内信号交换顺序
- 支持组与组之间交换顺序
- 分组计算逻辑放在 View 层

## Impact
- Affected code: `view.h/view.cpp`, `viewport.h/viewport.cpp`, `header.h/header.cpp`
- Affected QSS: `stylesheet.qss`, `dark.qss`, `light.qss`

## ADDED Requirements

### Requirement: 分组计算
系统 SHALL 根据解码器与源信号的绑定关系自动计算波形分组。

#### Scenario: 解码器绑定源信号形成一组
- **WHEN** 存在一个 DecodeTrace，其 DecoderStack 中的 Decoder 绑定了通道索引为 0、2 的信号
- **THEN** 通道 0 和通道 2 的 LogicSignal 与该 DecodeTrace 属于同一组

#### Scenario: 未被任何解码器绑定的信号自成一组
- **WHEN** 一个 LogicSignal 的通道索引未被任何 DecodeTrace 绑定
- **THEN** 该 LogicSignal 自成一组（单信号组）

#### Scenario: 多个解码器绑定同一信号
- **WHEN** 两个 DecodeTrace 都绑定了通道索引为 0 的信号
- **THEN** 通道 0 的信号与两个 DecodeTrace 同属一组

#### Scenario: 无解码器时每个信号自成一组
- **WHEN** 不存在任何 DecodeTrace
- **THEN** 每个 LogicSignal 各自独立成组

### Requirement: 分组卡片背景绘制
系统 SHALL 在 Viewport 和 Header 中为每个分组绘制卡片背景。

#### Scenario: 卡片样式
- **WHEN** 波形区域被绘制
- **THEN** 每个分组用一个圆角矩形框住组内所有信号，卡片颜色比背景色略深或略浅（在 QSS 中定义），卡片之间有间距

#### Scenario: Header 同步绘制
- **WHEN** Header 区域被绘制
- **THEN** Header 中的分组卡片与 Viewport 中的分组垂直对齐，使用相同的分组颜色

### Requirement: 组内信号交换
系统 SHALL 支持在同一组内拖拽交换信号的显示顺序。

#### Scenario: 拖拽组内信号
- **WHEN** 用户在 Header 中拖拽一个信号到同组内另一个信号的位置
- **THEN** 两个信号的显示顺序交换，仍在同一卡片内

### Requirement: 组间交换
系统 SHALL 支持拖拽交换不同组的整体顺序。

#### Scenario: 拖拽整组移动
- **WHEN** 用户拖拽一个信号到另一组的位置
- **THEN** 被拖拽信号所在的整个组与目标组交换位置

### Requirement: QSS 主题颜色定义
系统 SHALL 在 QSS 样式表中定义分组卡片的颜色。

#### Scenario: 亮色主题
- **WHEN** 使用亮色主题
- **THEN** 分组卡片使用比白色背景略深的颜色（如浅灰色）

#### Scenario: 暗色主题
- **WHEN** 使用暗色主题
- **THEN** 分组卡片使用比深色背景略浅的颜色（如稍亮的灰色）

## MODIFIED Requirements

### Requirement: View::signals_changed() 分组布局
原逻辑：所有 trace 按 _view_index 排序后顺序排列。

修改为：先计算分组，组内 trace 按 _view_index 排序，组间也按 _view_index 排序。组与组之间增加 GroupGap（如 4px）间距。每个 trace 的 v_offset 计算需要考虑组间距。

### Requirement: Header 拖拽重排序
原逻辑：拖拽释放后按 Y 位置排序分配 view_index。

修改为：拖拽释放后判断目标位置属于哪个组。如果目标在同一组内，仅交换组内顺序；如果目标在不同组，交换两个组的整体位置。

## REMOVED Requirements

无移除的需求。
