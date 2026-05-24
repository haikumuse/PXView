# Header 右键菜单设置行高 Spec

## Why
当前 Header 区域的 `contextMenuEvent` 是空壳，用户无法通过右键菜单快速管理信号行高。用户需要便捷地重置行高、单独设置通道高度、批量设置所有通道高度，而不必逐个拖拽边界线。

## What Changes
- 在 Header 的 `contextMenuEvent` 中构建右键菜单，包含：重置行高、重置所有行高、设置通道高度（子菜单）、批量设置（子菜单）
- "设置通道高度"子菜单提供预设高度选项（20/30/40/50/60/80/100px）和自定义输入
- "批量设置"子菜单提供同样的预设高度选项和自定义输入，应用到所有信号
- 在三个语言的 `dlg.json` 中添加对应的翻译字符串

## Impact
- Affected code: `PXView/pv/view/header.h`, `PXView/pv/view/header.cpp`, `lang/cn/dlg.json`, `lang/en/dlg.json`, `lang/traditional/dlg.json`
- Affected behavior: Header 区域右键菜单从无功能变为提供行高管理功能

## ADDED Requirements

### Requirement: Header 右键上下文菜单
系统 SHALL 在用户右键点击 Header 区域的信号标签时，显示上下文菜单，提供行高管理选项。

#### Scenario: 右键点击已选中的信号标签
- **WHEN** 用户右键点击 Header 中某个信号的标签区域
- **THEN** 显示包含以下选项的上下文菜单：
  - "重置行高" — 将当前信号行高恢复为默认（清除 own_height）
  - "重置所有行高" — 将所有信号行高恢复为默认
  - "设置通道高度" → 子菜单
  - "批量设置" → 子菜单

#### Scenario: 右键点击空白区域或未选中信号
- **WHEN** 用户右键点击 Header 区域但不在任何信号标签上
- **THEN** 不显示菜单（保持现有行为）

### Requirement: 重置行高
系统 SHALL 支持通过右键菜单将单个信号的行高重置为默认值。

#### Scenario: 点击"重置行高"
- **WHEN** 用户在右键菜单中点击"重置行高"
- **THEN** 当前信号的 `_ownHeight` 被设为 -1（使用全局高度），调用 `signals_changed(NULL)` 刷新布局

### Requirement: 重置所有行高
系统 SHALL 支持通过右键菜单将所有信号的行高重置为默认值。

#### Scenario: 点击"重置所有行高"
- **WHEN** 用户在右键菜单中点击"重置所有行高"
- **THEN** 所有信号的 `_ownHeight` 被设为 -1，调用 `signals_changed(NULL)` 刷新布局

### Requirement: 设置通道高度子菜单
系统 SHALL 提供子菜单用于设置当前信号的行高。

#### Scenario: 展开"设置通道高度"子菜单
- **WHEN** 用户将鼠标悬停在"设置通道高度"上
- **THEN** 展开子菜单，显示预设高度选项：20px、30px、40px、50px、60px、80px、100px，以及"自定义..."选项

#### Scenario: 选择预设高度
- **WHEN** 用户在子菜单中点击某个预设高度（如 40px）
- **THEN** 当前信号的 `_ownHeight` 被设为该值（受 MinSignalHeight/MaxSignalHeight 约束），调用 `signals_changed(NULL)` 刷新布局

#### Scenario: 选择自定义高度
- **WHEN** 用户在子菜单中点击"自定义..."
- **THEN** 弹出 QInputDialog 输入对话框，用户输入高度值后确认
- **THEN** 当前信号的 `_ownHeight` 被设为输入值（受 MinSignalHeight/MaxSignalHeight 约束），调用 `signals_changed(NULL)` 刷新布局

#### Scenario: 自定义输入值无效
- **WHEN** 用户输入的值小于 MinSignalHeight 或大于 MaxSignalHeight
- **THEN** 值被限制在有效范围内

### Requirement: 批量设置子菜单
系统 SHALL 提供子菜单用于批量设置所有信号的行高。

#### Scenario: 展开"批量设置"子菜单
- **WHEN** 用户将鼠标悬停在"批量设置"上
- **THEN** 展开子菜单，显示与"设置通道高度"相同的预设高度选项和"自定义..."选项

#### Scenario: 批量选择预设高度
- **WHEN** 用户在子菜单中点击某个预设高度（如 30px）
- **THEN** 所有信号的 `_ownHeight` 被设为该值（受 MinSignalHeight/MaxSignalHeight 约束），调用 `signals_changed(NULL)` 刷新布局

#### Scenario: 批量自定义高度
- **WHEN** 用户在子菜单中点击"自定义..."
- **THEN** 弹出 QInputDialog 输入对话框，用户输入高度值后确认
- **THEN** 所有信号的 `_ownHeight` 被设为输入值（受 MinSignalHeight/MaxSignalHeight 约束），调用 `signals_changed(NULL)` 刷新布局

### Requirement: 菜单仅在 LOGIC 模式下显示
系统 SHALL 仅在 LOGIC 模式下显示行高管理右键菜单。

#### Scenario: 非 LOGIC 模式下右键
- **WHEN** 当前设备工作模式不是 LOGIC
- **THEN** 不显示行高管理菜单

## MODIFIED Requirements

### Requirement: Header::contextMenuEvent
原逻辑：获取点击位置的 Trace，若不满足条件则直接返回，不创建任何菜单。

修改为：获取点击位置的 Trace 后，构建包含行高管理选项的 QMenu 并显示。若不在信号标签上，仍不显示菜单。

## REMOVED Requirements
无
