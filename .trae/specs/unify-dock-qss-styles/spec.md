# 统一 Dock 页面 QSS 样式 Spec

## Why
当前侧边 Dock 页面样式严重不统一：SlidingDrawer 标题栏/边缘拖拽区缺少 QSS 定义、各 Dock 内容 Widget 基类不同导致边框/背景/内边距不一致、7 处 C++ 代码内联 setStyleSheet 硬编码颜色值无法随主题切换、dark.qss 和 light.qss 中残留的 QDockWidget 样式已无用。需要统一 QSS 样式体系，消除内联硬编码，补全 SlidingDrawer 样式。

## What Changes
- 补全 SlidingDrawer 的 QSS 样式定义：`#sliding_drawer_titlebar`、`#sliding_drawer_title`、`#sliding_drawer_edge_grip`、`#sliding_drawer_stack`
- 为各 Dock 页面内容区域添加 QSS 选择器样式（通过 objectName 精确定位）
- 消除 7 处 C++ 内联 setStyleSheet 硬编码颜色，改用 QSS 选择器或 QPalette
- 统一各 Dock 内容 Widget 的 QScrollArea 边框处理（全部 NoFrame + QSS 控制）
- 统一各 Dock 内容区域的内边距（标准值 12,8,12,8）
- 删除 dark.qss / light.qss 中已无用的 QDockWidget 样式块
- 删除 stylesheet.qss 中已无用的 QDockWidget / QScrollArea #measureWidget 等旧样式块
- 在 dark.qss / light.qss 的 Color Tokens 注释区新增 Dock 相关语义令牌

## Impact
- Affected specs: `create-unified-sidebar-component`（SideBar 按钮样式需与此协调）、`convert-deviceoptions-to-sidebar`（DeviceOptionsDock 样式需统一）
- Affected code:
  - `PXView/themes/dark.qss`（修改：补全 SlidingDrawer + Dock 页面样式，删除 QDockWidget 样式）
  - `PXView/themes/light.qss`（修改：同上）
  - `PXView/stylesheet.qss`（修改：删除旧 QDockWidget / measureWidget 等样式）
  - `PXView/pv/dock/searchdock.cpp`（修改：删除内联 setStyleSheet，改用 objectName + QSS）
  - `PXView/pv/dock/deviceoptionsdock.cpp`（修改：删除 `this->setStyleSheet("QScrollArea{border:none;}")`，改用 QSS）
  - `PXView/pv/dock/measuredock.cpp`（修改：删除按钮颜色内联 setStyleSheet，改用 QPalette 获取主题色）
  - `PXView/pv/dock/protocoldock.cpp`（修改：删除 `_table_view->setStyleSheet(style)` 字体设置，改用 QSS）
  - `PXView/pv/dock/protocolitemlayer.cpp`（修改：删除 `color:green` / `color:red` 内联样式，改用动态属性 + QSS）
  - `PXView/pv/dock/searchcombobox.cpp`（修改：删除 `QScrollArea{border:none;}` 内联样式，改用 QSS）
  - `PXView/pv/widgets/slidingdrawer.cpp`（可能修改：为内部控件补充 objectName 或动态属性）
  - `PXView/pv/dock/triggerdock.cpp`（修改：统一 QScrollArea 边框和内边距）
  - `PXView/pv/dock/dsotriggerdock.cpp`（修改：统一 QScrollArea 边框和内边距）

## ADDED Requirements

### Requirement: SlidingDrawer 完整 QSS 样式
系统 SHALL 在 dark.qss 和 light.qss 中为 SlidingDrawer 的所有子控件提供完整的样式定义。

#### Scenario: 标题栏样式
- **WHEN** SlidingDrawer 打开
- **THEN** `#sliding_drawer_titlebar` 具有明确的背景色和底部分隔线
- **AND** 暗色主题：背景 `@bg-overlay`，底部分隔线 `1px solid @drawer-border`
- **AND** 亮色主题：背景 `@bg-overlay`，底部分隔线 `1px solid @drawer-border`

#### Scenario: 标题文字样式
- **WHEN** SlidingDrawer 打开
- **THEN** `#sliding_drawer_title` 具有明确的颜色、字体大小和粗细
- **AND** 暗色主题：颜色 `@fg-base`，字体 13pt bold
- **AND** 亮色主题：颜色 `@fg-base`，字体 13pt bold

#### Scenario: 边缘拖拽区样式
- **WHEN** 鼠标悬停在 SlidingDrawer 左边缘
- **THEN** `#sliding_drawer_edge_grip` 显示视觉提示（半透明高亮背景）
- **AND** 默认状态为透明背景

#### Scenario: 内容堆栈样式
- **WHEN** SlidingDrawer 打开
- **THEN** `#sliding_drawer_stack` 背景透明，无边框，不与内容区域产生视觉冲突

### Requirement: Dock 页面内容区域统一样式
系统 SHALL 为所有 Dock 页面内容区域提供统一的 QSS 样式，确保边框、背景、内边距一致。

#### Scenario: QScrollArea 无边框
- **WHEN** 任何 Dock 页面在 SlidingDrawer 中显示
- **THEN** 页面内的 QScrollArea 无边框（`border: none`）
- **AND** 由 SlidingDrawer 的 `border-left` 提供视觉分隔

#### Scenario: 内容区域背景透明
- **WHEN** 任何 Dock 页面在 SlidingDrawer 中显示
- **THEN** 内容区域背景透明，继承 SlidingDrawer 面板的背景色
- **AND** 不出现背景色不匹配的情况

#### Scenario: 内容区域内边距统一
- **WHEN** 任何 Dock 页面在 SlidingDrawer 中显示
- **THEN** 内容区域具有统一的内边距（水平 12px，垂直 8px）
- **AND** 不同 Dock 页面切换时内容与边缘的间距一致

### Requirement: 消除内联 setStyleSheet 硬编码颜色
系统 SHALL 不在 C++ 代码中使用 setStyleSheet 设置颜色值，所有颜色值只出现在 .qss 文件中。

#### Scenario: SearchDock 表格样式
- **WHEN** SearchDock 的搜索结果表格显示
- **THEN** 网格线颜色、表头样式由 QSS `#dock_search_result_view` 选择器控制
- **AND** 不再使用 `setStyleSheet("...#d0d0d0...")` 硬编码颜色

#### Scenario: DeviceOptionsDock 滚动区域边框
- **WHEN** DeviceOptionsDock 显示
- **THEN** QScrollArea 无边框由 QSS 选择器控制
- **AND** 不再使用 `this->setStyleSheet("QScrollArea{border:none;}")` 内联样式

#### Scenario: MeasureDock 按钮颜色
- **WHEN** MeasureDock 的光标按钮显示
- **THEN** 按钮颜色通过 QPalette 获取主题色（`palette().color(QPalette::Window)` 等）
- **AND** 不再硬编码 `rgb(240,240,240)` 等颜色值

#### Scenario: ProtocolDock 表头字体
- **WHEN** ProtocolDock 的解码数据表显示
- **THEN** 表头字体大小由 QSS `#dock_protocol_page QHeaderView` 选择器控制
- **AND** 不再使用 `_table_view->setStyleSheet("#DecodedDataView QHeaderView{font-size: %1pt}")` 内联样式

#### Scenario: ProtocolItemLayer 进度标签颜色
- **WHEN** 协议解码进度标签显示
- **THEN** 成功/错误状态颜色由 QSS 动态属性选择器 `[status="ok"]` / `[status="error"]` 控制
- **AND** 不再使用 `setStyleSheet("color:green;")` / `setStyleSheet("color:red;")` 内联样式

#### Scenario: SearchComboBox 滚动区域边框
- **WHEN** SearchComboBox 的下拉列表显示
- **THEN** QScrollArea 无边框由 QSS 选择器控制
- **AND** 不再使用 `setStyleSheet("QScrollArea{border:none;}")` 内联样式

### Requirement: Dock 相关语义颜色令牌
系统 SHALL 在 dark.qss 和 light.qss 的 Color Tokens 注释区新增 Dock 相关语义令牌。

#### Scenario: 暗色主题新增令牌
- **WHEN** 查看 dark.qss 的 Color Tokens 注释
- **THEN** 包含以下新增令牌：
  - `@drawer-title-bg`（标题栏背景）
  - `@drawer-title-fg`（标题文字颜色）
  - `@drawer-title-border`（标题栏底部分隔线颜色）
  - `@drawer-edge-hover`（边缘拖拽区悬停背景）
  - `@dock-gridline`（Dock 内表格网格线颜色）
  - `@dock-groupbox-border`（Dock 内 QGroupBox 边框颜色）
  - `@dock-groupbox-title`（Dock 内 QGroupBox 标题颜色）
  - `@dock-status-ok`（成功状态颜色，替代 green）
  - `@dock-status-error`（错误状态颜色，替代 red）

#### Scenario: 亮色主题新增令牌
- **WHEN** 查看 light.qss 的 Color Tokens 注释
- **THEN** 包含与暗色主题对应的亮色值令牌

## MODIFIED Requirements

### Requirement: dark.qss SlidingDrawer 样式块
dark.qss 的 SlidingDrawer 样式块从仅 2 个选择器扩展为完整的 6 个选择器。

原有样式：
```css
#sliding_drawer_panel { background: transparent; }
#sliding_drawer_panel_content { background: @bg-elevated; border-left: 1px solid @drawer-border; }
```

新增样式：
```css
#sliding_drawer_titlebar { background: @drawer-title-bg; border-bottom: 1px solid @drawer-title-border; }
#sliding_drawer_title { color: @drawer-title-fg; font-size: 13pt; font-weight: bold; }
#sliding_drawer_edge_grip { background: transparent; }
#sliding_drawer_edge_grip:hover { background: @drawer-edge-hover; }
#sliding_drawer_stack { background: transparent; border: none; }
```

### Requirement: light.qss SlidingDrawer 样式块
light.qss 的 SlidingDrawer 样式块同步扩展，与 dark.qss 结构一致。

### Requirement: stylesheet.qss 清理
stylesheet.qss 中删除以下已无用的样式块：
- `QDockWidget` 及 `QDockWidget::title` 和 `QDockWidget > QWidget` 选择器（第122-146行）
- `QScrollArea #measureWidget` / `#dsoTriggerWidget` / `#triggerWidget` / `#protocolWidget` 选择器（第148-156行）

## REMOVED Requirements

### Requirement: QDockWidget 样式定义
**Reason**: 所有 Dock 内容已迁移到 SlidingDrawer，QDockWidget 仅作为空壳存在（未 addDockWidget），其样式定义不再生效。
**Migration**: 从 dark.qss 和 light.qss 中删除 `QDockWidget` 及其子选择器（`::close-button`、`::float-button`）的样式定义。

### Requirement: 内联 setStyleSheet 硬编码颜色
**Reason**: 内联颜色值无法随主题切换，导致暗色主题下浅色值不可见，亮色主题下深色值不协调。
**Migration**: 所有颜色值迁移到 QSS 文件，通过 objectName 选择器或动态属性选择器控制。
