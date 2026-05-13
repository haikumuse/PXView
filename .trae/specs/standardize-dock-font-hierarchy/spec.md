# Dock 字号层级规范 Spec

## Why
当前所有 Dock 页面使用同一个基础字号（`AppConfig::Instance().appOptions.fontSize`），通过 `setFont()` 统一设置，没有视觉层级区分。标题、标签、内容文字大小一致，导致信息层次不清、可读性差。需要建立字号层级标准，将字号定义集中在 QSS 中管理，所有 Dock 统一遵循。

## What Changes
- 在 QSS 中建立 4 级字号层级标准：主标题 18px、二级标题 16px、标签 14px、内容 12px
- 新增 `#dock_label`（14px）和 `#dock_content`（12px）QSS 选择器
- 为 `#dock_section_title` 添加 `font-size: 16px`（当前仅有 color 和 font-weight）
- 修改 `#sliding_drawer_title` 的 `font-size` 从 13pt 改为 18px
- 在 C++ 代码中为标签和内容控件添加对应的 objectName
- 在 C++ 代码中用层级字号替代统一的 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 调用
- 新增 `pv/ui/dockfonts.h` 定义字号常量和辅助函数
- 修改 `ui::set_form_font()` 或新增 `ui::set_dock_form_font()` 支持层级字号
- 在 Color Tokens 注释区新增字号令牌说明

## Impact
- Affected specs: `unify-dock-qss-styles`（QSS 选择器扩展需与此协调）
- Affected code:
  - `PXView/themes/dark.qss`（修改：添加字号层级 QSS 规则）
  - `PXView/themes/light.qss`（修改：同上）
  - `PXView/pv/ui/dockfonts.h`（新增：字号常量和辅助函数）
  - `PXView/pv/ui/fn.h` / `fn.cpp`（修改：新增 `set_dock_form_font()`）
  - `PXView/pv/dock/deviceoptionsdock.cpp`（修改：使用层级字号，添加 objectName）
  - `PXView/pv/dock/measuredock.cpp`（修改：同上）
  - `PXView/pv/dock/triggerdock.cpp`（修改：同上）
  - `PXView/pv/dock/dsotriggerdock.cpp`（修改：同上）
  - `PXView/pv/dock/protocoldock.cpp`（修改：同上）
  - `PXView/pv/dock/searchdock.cpp`（修改：同上）
  - `PXView/pv/toolbars/samplingbar.cpp`（修改：使用层级字号，添加 objectName）

## ADDED Requirements

### Requirement: Dock 字号层级 QSS 定义
系统 SHALL 在 dark.qss 和 light.qss 中定义 4 级字号层级，作为所有 Dock 页面的字号标准。

#### Scenario: 主标题字号
- **WHEN** SlidingDrawer 标题栏显示 Dock 页面名称（如"设备选项"）
- **THEN** `#sliding_drawer_title` 的 `font-size` 为 18px
- **AND** `font-weight` 为 bold

#### Scenario: 二级标题字号
- **WHEN** Dock 页面内容区域显示分节标题（如"通道选择"、"毛刺过滤"、"采集选项"、"Mouse"、"Distance"等）
- **THEN** `#dock_section_title` 的 `font-size` 为 16px
- **AND** `font-weight` 为 bold

#### Scenario: 标签字号
- **WHEN** Dock 页面显示字段标签（如"设备"、"采样深度"、"采样率"、"捕获模式"等）
- **THEN** `#dock_label` 的 `font-size` 为 14px

#### Scenario: 内容字号
- **WHEN** Dock 页面显示内容控件文字（如 QComboBox 选项"500 ms"、"20 MHz"，QPushButton 文字"单次"、"重复"、"全部启用"等）
- **THEN** `#dock_content` 的 `font-size` 为 12px

### Requirement: 字号层级 C++ 常量
系统 SHALL 在 `pv/ui/dockfonts.h` 中定义字号常量，与 QSS 中的字号值保持一致，供 C++ 布局计算使用。

#### Scenario: 常量定义
- **WHEN** C++ 代码需要获取 Dock 字号
- **THEN** 可通过 `DockFontSizes::MainTitle`（18）、`DockFontSizes::SectionTitle`（16）、`DockFontSizes::Label`（14）、`DockFontSizes::Content`（12）获取
- **AND** 可通过 `dock_font_main_title()`、`dock_font_section_title()`、`dock_font_label()`、`dock_font_content()` 获取对应的 QFont 对象

### Requirement: Dock 表单层级字号设置
系统 SHALL 提供 `ui::set_dock_form_font()` 函数，替代 `ui::set_form_font()` 在 Dock 页面中的使用，按层级设置字体。

#### Scenario: 层级字号设置
- **WHEN** Dock 页面调用 `ui::set_dock_form_font(widget, baseFont)`
- **THEN** QLabel（非 dock_section_title）使用 14px 字号
- **AND** QPushButton / QComboBox / QSpinBox / QCheckBox / QRadioButton / QLineEdit 使用 12px 字号
- **AND** 名为 `dock_section_title` 的 QLabel 保持 16px 字号
- **AND** QTabWidget / QGroupBox 使用 14px 字号（与标签同级）

### Requirement: 所有 Dock 页面统一应用字号层级
系统 SHALL 在所有 Dock 页面中统一应用字号层级标准。

#### Scenario: DeviceOptionsDock 字号
- **WHEN** DeviceOptionsDock 显示
- **THEN** 分节标题（"Channel"、"毛刺过滤"、"Mode"）为 16px
- **AND** 属性标签（"Enable:"、属性名标签）为 14px
- **AND** 下拉框、按钮、复选框、单选按钮文字为 12px
- **AND** 采样设置区域标签（"设备"、"采样深度"等）为 14px
- **AND** 采样设置区域下拉框为 12px

#### Scenario: MeasureDock 字号
- **WHEN** MeasureDock 显示
- **THEN** 分节标题（"Mouse"、"Distance"、"Edge"、"Cursor"）为 16px
- **AND** 标签文字为 14px
- **AND** 按钮和下拉框文字为 12px

#### Scenario: TriggerDock 字号
- **WHEN** TriggerDock 显示
- **THEN** 标签文字为 14px
- **AND** 输入框和下拉框文字为 12px

#### Scenario: DsoTriggerDock 字号
- **WHEN** DsoTriggerDock 显示
- **THEN** 标签文字为 14px
- **AND** 下拉框和按钮文字为 12px

#### Scenario: ProtocolDock 字号
- **WHEN** ProtocolDock 显示
- **THEN** 标签文字为 14px
- **AND** 下拉框和按钮文字为 12px

#### Scenario: SearchDock 字号
- **WHEN** SearchDock 显示
- **THEN** 标签文字为 14px
- **AND** 输入框和下拉框文字为 12px

### Requirement: QSS Color Tokens 字号令牌注释
系统 SHALL 在 dark.qss 和 light.qss 的 Color Tokens 注释区新增字号令牌说明。

#### Scenario: 字号令牌注释
- **WHEN** 查看 dark.qss 或 light.qss 的 Color Tokens 注释
- **THEN** 包含以下字号令牌说明：
  - `@dock-font-main-title: 18px`（主标题）
  - `@dock-font-section-title: 16px`（二级标题）
  - `@dock-font-label: 14px`（标签）
  - `@dock-font-content: 12px`（内容）

## MODIFIED Requirements

### Requirement: #sliding_drawer_title 字号
`#sliding_drawer_title` 的 `font-size` 从 `13pt` 修改为 `18px`，与主标题层级一致。

原有样式：
```css
#sliding_drawer_title { color: @drawer-title-fg; font-size: 13pt; font-weight: bold; }
```

修改后：
```css
#sliding_drawer_title { color: @drawer-title-fg; font-size: 18px; font-weight: bold; }
```

### Requirement: #dock_section_title 字号
`#dock_section_title` 新增 `font-size: 16px` 属性。

原有样式：
```css
#dock_section_title { color: @dock-section-title-fg; font-weight: bold; }
```

修改后：
```css
#dock_section_title { color: @dock-section-title-fg; font-size: 16px; font-weight: bold; }
```

### Requirement: DeviceOptionsDock 字号设置方式
DeviceOptionsDock 中所有 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 调用替换为层级字号函数调用。

### Requirement: MeasureDock 字号设置方式
MeasureDock 中所有 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 调用替换为层级字号函数调用。

### Requirement: TriggerDock 字号设置方式
TriggerDock 中所有 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 调用替换为层级字号函数调用。

### Requirement: DsoTriggerDock 字号设置方式
DsoTriggerDock 中所有 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 调用替换为层级字号函数调用。

### Requirement: ProtocolDock 字号设置方式
ProtocolDock 中所有 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 调用替换为层级字号函数调用。

### Requirement: SearchDock 字号设置方式
SearchDock 中所有 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 调用替换为层级字号函数调用。

### Requirement: SamplingBar 采样设置字号设置方式
SamplingBar 中 `font.setPointSizeF(AppConfig::Instance().appOptions.fontSize)` 调用替换为层级字号函数调用。

## REMOVED Requirements

### Requirement: 单一基础字号设置
**Reason**: 所有 Dock 页面使用同一个 `AppConfig::Instance().appOptions.fontSize` 无法体现信息层级，导致标题与内容无视觉区分。
**Migration**: 替换为 4 级字号层级（18px/16px/14px/12px），通过 `dockfonts.h` 常量和 QSS 双重定义确保一致性。
