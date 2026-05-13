# 移除 Dock 内 QGroupBox 替换为子标题+分隔线 Spec

## Why
当前 Dock 面板内使用 `QGroupBox` 对参数进行分组，QGroupBox 自带边框和标题样式，视觉上显得笨重且占用空间。将其替换为轻量的子标题标签 + 组间水平分隔线，可以在保持逻辑分组的同时使界面更简洁、现代。

## What Changes
- 在所有 Dock 面板中移除 `QGroupBox`，替换为 `QLabel`（子标题）+ `QFrame`（水平分隔线）
- 子标题样式通过 QSS 选择器 `#dock_section_title` 控制
- 分隔线样式通过 QSS 选择器 `#dock_section_separator` 控制
- 在 dark.qss 和 light.qss 中新增对应的样式定义
- 删除 dark.qss / light.qss 中 `QGroupBox`、`QGroupBox::title`、`QGroupBox:disabled` 的边框/标题样式（保留 `QGroupBox::indicator` 样式，因为 QGroupBox 的 indicator 与 QCheckBox 共用样式定义）
- 删除 `@dock-groupbox-border`、`@dock-groupbox-title` 颜色令牌，新增 `@dock-section-title-fg`、`@dock-section-separator-color` 令牌
- **BREAKING**: `QGroupBox` 不再用于 Dock 面板内的分组容器，但 QSS 中仍保留 `QGroupBox::indicator` 相关样式（因为其他地方可能仍使用 QGroupBox 的可勾选功能）

## Impact
- Affected specs: `unify-dock-qss-styles`（需同步更新 QSS 令牌和样式定义）
- Affected code:
  - `PXView/pv/dock/measuredock.cpp`（修改：移除 4 个 QGroupBox，替换为 QLabel + QFrame）
  - `PXView/pv/dock/measuredock.h`（修改：移除 QGroupBox 成员变量，新增 QLabel 成员变量）
  - `PXView/pv/dock/deviceoptionsdock.cpp`（修改：移除 3 处 QGroupBox，替换为 QLabel + QFrame）
  - `PXView/pv/dock/deviceoptionsdock.h`（修改：移除 QGroupBox 成员变量，新增 QWidget 成员变量）
  - `PXView/pv/dock/triggerdock.cpp`（修改：移除 QGroupBox 列表和串口 QGroupBox，替换为 QLabel + QFrame）
  - `PXView/pv/dock/triggerdock.h`（修改：移除 QGroupBox 成员变量，新增 QLabel 成员变量）
  - `PXView/themes/dark.qss`（修改：新增子标题和分隔线样式，修改 QGroupBox 样式）
  - `PXView/themes/light.qss`（修改：同上）

## ADDED Requirements

### Requirement: Dock 分组子标题样式
系统 SHALL 在 Dock 面板中使用 `QLabel`（objectName 为 `dock_section_title`）作为分组子标题，替代 QGroupBox 的标题。

#### Scenario: 子标题显示
- **WHEN** Dock 面板中某个分组显示
- **THEN** 分组标题以 QLabel 形式显示在分组内容上方
- **AND** QLabel 的 objectName 为 `dock_section_title`
- **AND** 暗色主题：文字颜色 `@dock-section-title-fg`（`#cccccc`），字体加粗
- **AND** 亮色主题：文字颜色 `@dock-section-title-fg`（`#333333`），字体加粗

### Requirement: Dock 分组分隔线样式
系统 SHALL 在 Dock 面板中使用 `QFrame`（objectName 为 `dock_section_separator`，shape 为 `HLine`）作为组间分隔线。

#### Scenario: 分隔线显示
- **WHEN** Dock 面板中两个分组之间
- **THEN** 分隔线以 QFrame（HLine）形式显示在分组之间
- **AND** QFrame 的 objectName 为 `dock_section_separator`
- **AND** 暗色主题：颜色 `@dock-section-separator-color`（`#3f3f46`），1px solid
- **AND** 亮色主题：颜色 `@dock-section-separator-color`（`#d4d4d8`），1px solid
- **AND** 分隔线上下各有 8px 间距

### Requirement: MeasureDock 分组替换
MeasureDock SHALL 将所有 QGroupBox 替换为子标题 + 分隔线布局，保持原有控件布局不变。

#### Scenario: 鼠标测量分组
- **WHEN** MeasureDock 显示
- **THEN** "鼠标测量"分组显示为子标题 QLabel + 内容布局（原 `_mouse_groupBox` 内的 QGridLayout）
- **AND** 原有 `_fen_checkBox`、`_width_label` 等控件的位置和功能不变

#### Scenario: 光标距离分组
- **WHEN** MeasureDock 显示
- **THEN** "光标距离"分组显示为子标题 QLabel + 内容布局（原 `_dist_groupBox` 内的 QGridLayout）
- **AND** 原有 `_dist_add_btn`、距离行等控件的位置和功能不变

#### Scenario: 边沿测量分组
- **WHEN** MeasureDock 显示
- **THEN** "边沿测量"分组显示为子标题 QLabel + 内容布局（原 `_edge_groupBox` 内的 QGridLayout）
- **AND** 原有 `_edge_add_btn`、边沿行等控件的位置和功能不变

#### Scenario: 光标分组
- **WHEN** MeasureDock 显示
- **THEN** "光标"分组显示为子标题 QLabel + 内容布局（原 `_cursor_groupBox` 内的 QGridLayout）
- **AND** 原有 `_time_label` 等控件的位置和功能不变

### Requirement: DeviceOptionsDock 分组替换
DeviceOptionsDock SHALL 将所有 QGroupBox 替换为子标题 + 分隔线布局，保持原有控件布局不变。

#### Scenario: 动态面板分组
- **WHEN** DeviceOptionsDock 显示
- **THEN** `_dynamic_panel`（原 QGroupBox）替换为 QWidget 容器 + 子标题 QLabel
- **AND** 原有通道配置控件的位置和功能不变
- **AND** `_dynamic_panel` 的类型从 `QGroupBox*` 变为 `QWidget*`

#### Scenario: Mode 属性分组
- **WHEN** DeviceOptionsDock 显示
- **THEN** "Mode" 分组（原局部 QGroupBox `props_box`）替换为子标题 QLabel + 内容布局
- **AND** 原有属性表单控件的位置和功能不变

#### Scenario: 毛刺过滤分组
- **WHEN** DeviceOptionsDock 显示且设备为 LOGIC 模式
- **THEN** "毛刺过滤"分组（原 `_glitch_filter_group` QGroupBox）替换为子标题 QLabel + 内容布局
- **AND** 原有通道勾选和阈值控件的位置和功能不变
- **AND** `_glitch_filter_group` 的类型从 `QGroupBox*` 变为 `QWidget*`

### Requirement: TriggerDock 分组替换
TriggerDock SHALL 将所有 QGroupBox 替换为子标题 + 分隔线布局，保持原有控件布局不变。

#### Scenario: 阶段触发分组
- **WHEN** TriggerDock 显示
- **THEN** 每个阶段的 QGroupBox（`_stage_groupBox_list`）替换为 QWidget 容器 + 子标题 QLabel
- **AND** 原有阶段触发控件的位置和功能不变
- **AND** `_stage_groupBox_list` 的类型从 `QVector<QGroupBox*>` 变为 `QVector<QWidget*>`

#### Scenario: 串口触发分组
- **WHEN** TriggerDock 显示
- **THEN** 串口触发 QGroupBox（`_serial_groupBox`）替换为 QWidget 容器 + 子标题 QLabel
- **AND** 原有串口触发控件的位置和功能不变
- **AND** `_serial_groupBox` 的类型从 `QGroupBox*` 变为 `QWidget*`

### Requirement: 新增 QSS 颜色令牌
系统 SHALL 在 dark.qss 和 light.qss 中新增 Dock 分组相关的语义颜色令牌。

#### Scenario: 暗色主题新增令牌
- **WHEN** 查看 dark.qss 的 Color Tokens 注释
- **THEN** 包含以下新增令牌：
  - `@dock-section-title-fg`（子标题文字颜色，值 `#cccccc`）
  - `@dock-section-separator-color`（分隔线颜色，值 `#3f3f46`）

#### Scenario: 亮色主题新增令牌
- **WHEN** 查看 light.qss 的 Color Tokens 注释
- **THEN** 包含以下新增令牌：
  - `@dock-section-title-fg`（子标题文字颜色，值 `#333333`）
  - `@dock-section-separator-color`（分隔线颜色，值 `#d4d4d8`）

## MODIFIED Requirements

### Requirement: QGroupBox QSS 样式
dark.qss 和 light.qss 中的 `QGroupBox`、`QGroupBox::title`、`QGroupBox:disabled` 样式块修改为无边框无标题样式（因为 Dock 内不再使用 QGroupBox 作为分组容器）。

原有样式：
```css
QGroupBox { border: 1px solid @border; border-radius: 2px; margin-top: 20px; }
QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; padding-left: 10px; padding-right: 10px; padding-top: 10px; }
QGroupBox:disabled { border: 1px solid @border-subtle; }
```

修改后样式：
```css
QGroupBox { border: none; margin-top: 0px; }
QGroupBox::title { /* 不再需要 */ }
QGroupBox:disabled { border: none; }
```

保留 `QGroupBox::indicator` 相关样式不变（与 QCheckBox 共用样式定义）。

### Requirement: DeviceOptionsDock rebuild_glitch_filter_panel 逻辑
`rebuild_glitch_filter_panel()` 方法中通过 `qobject_cast<QGroupBox*>` 查找 Mode 分组的逻辑需修改为通过 objectName 查找。

原有逻辑：
```cpp
QGroupBox *box = qobject_cast<QGroupBox*>(w);
if (box && box->title() == "Mode") { ... }
```

修改后逻辑：
```cpp
if (w && w->objectName() == "dock_mode_section") { ... }
```

## REMOVED Requirements

### Requirement: QGroupBox 作为 Dock 分组容器
**Reason**: QGroupBox 的边框和标题样式视觉上过于笨重，替换为轻量的子标题 + 分隔线。
**Migration**: 所有 Dock 内的 QGroupBox 替换为 QWidget 容器 + QLabel 子标题 + QFrame 分隔线。

### Requirement: @dock-groupbox-border 和 @dock-groupbox-title 颜色令牌
**Reason**: QGroupBox 不再用于 Dock 分组，相关令牌不再需要。
**Migration**: 替换为 `@dock-section-title-fg` 和 `@dock-section-separator-color` 令牌。
