# 搜索面板单行输入重构 Spec

## Why
当前 SearchDock 使用逐通道 QGridLayout 编辑器，当通道数量增加时面板不会自动扩展高度，导致部分通道编辑器不可见。同时，参考 atk-logic 项目的 QML 设计，搜索模式应采用单行文本输入方式（每个字符位对应一个通道），配合位范围标签，更加紧凑直观。

## What Changes
- 移除逐通道 QGridLayout 编辑器（每通道一个 QLineEdit），替换为单行文本输入框
- 新增位范围标签行（如 "15---8" "7---0"），显示在输入框上方
- 新增自定义 SearchPatternInput 控件，模拟 TriggerTextInput.qml 的行为：等宽字体、字符间距、仅接受 X/0/1/R/F/C、自动选中等
- 修改 `_pattern` 映射逻辑：从单行文本的每个字符位置解析出各通道的模式值
- 移除 QScrollArea（单行输入不需要滚动）
- 图例改为三列水平布局（与参考 QML 一致）
- **BREAKING**: 移除 `_search_lineEdit_vec`、`_search_grid` 成员，替换为 `_pattern_input` 和位范围标签

## Impact
- Affected specs: flatten-search-dock（前序 spec，已完成）
- Affected code:
  - `DSView/pv/dock/searchdock.h`（修改成员变量）
  - `DSView/pv/dock/searchdock.cpp`（重写布局和逻辑）
  - `DSView/pv/widgets/searchpatterninput.h`（新增）
  - `DSView/pv/widgets/searchpatterninput.cpp`（新增）

## ADDED Requirements

### Requirement: 单行搜索模式输入框
SearchDock SHALL 使用单行文本输入框来编辑所有通道的搜索模式，每个字符位置对应一个逻辑通道。

#### Scenario: 输入框显示
- **WHEN** SearchDock 可见
- **AND** 存在 N 个逻辑通道
- **THEN** 输入框中显示 N 个字符，每个字符代表对应通道的搜索模式
- **AND** 默认所有字符为 "X"
- **AND** 输入框使用等宽字体，字符间距适当以区分各位

#### Scenario: 输入字符限制
- **WHEN** 用户在输入框中按键
- **THEN** 仅接受 X/0/1/R/F/C 键（大小写不敏感），其他按键被忽略
- **AND** 输入自动转为大写
- **AND** 按下 X/0/1/R/F/C 键时，替换当前光标位置的字符
- **AND** 光标自动前进到下一个位置

#### Scenario: 光标行为
- **WHEN** 输入框获得焦点
- **THEN** 自动选中光标位置处的一个字符
- **AND** 左右方向键可移动光标
- **AND** Backspace/Delete 将当前位置字符重置为 "X"

#### Scenario: 模式实时更新
- **WHEN** 用户修改输入框中的任意字符
- **THEN** `_pattern` 映射表实时更新
- **AND** 搜索光标位置重置

### Requirement: 位范围标签
SearchDock SHALL 在输入框上方显示位范围标签，标识每组8位通道的索引范围。

#### Scenario: 位范围标签显示
- **WHEN** SearchDock 可见
- **AND** 存在逻辑通道
- **THEN** 在输入框上方显示位范围标签
- **AND** 每8个通道为一组，标签格式为 "高位---低位"（如 "15---8"、"7---0"）
- **AND** 标签水平排列，与输入框中对应字符位置对齐

### Requirement: 图例三列布局
SearchDock SHALL 以三列水平布局显示搜索模式图例。

#### Scenario: 图例显示
- **WHEN** SearchDock 可见
- **THEN** 图例以三列水平布局显示：
  - 列1: "X: 不关心"、"R: 上升沿"
  - 列2: "0: 低电平"、"F: 下降沿"
  - 列3: "1: 高电平"、"C: 所有沿"

### Requirement: 面板尺寸自适应
SearchDock SHALL 根据逻辑通道数量自动调整面板内容大小。

#### Scenario: 通道数量变化
- **WHEN** 逻辑通道数量增加或减少
- **THEN** 输入框长度和位范围标签自动调整
- **AND** 面板高度自适应内容，无需手动调整

## MODIFIED Requirements

### Requirement: SearchDock 布局
SearchDock 的布局从逐通道网格改为紧凑的单行输入布局：

新布局：
```
┌─────────────────────────────────┐
│  15---8        7---0            │  ← 位范围标签
│  ┌───────────────────────────┐  │
│  │ XXXXXXXXXXXXXXXX          │  │  ← 单行模式输入框
│  └───────────────────────────┘  │
│  ─────────────────────────────  │  ← 分隔线
│  X:不关心  0:低电平  1:高电平   │  ← 图例三列
│  R:上升沿  F:下降沿  C:所有沿   │
├─────────────────────────────────┤
│       [◀ Pre]    [Next ▶]      │  ← 导航按钮
└─────────────────────────────────┘
```

## REMOVED Requirements

### Requirement: 逐通道网格编辑器
**Reason**: 替换为单行文本输入方式，更紧凑直观。
**Migration**: 移除 `_search_grid`、`_search_lineEdit_vec`，新增 `_pattern_input` 控件。
