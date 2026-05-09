# 统一侧边栏组件（SideBar）重构 Spec

## Why
当前右侧侧边栏按钮分散在 TrigBar 和 SamplingBar 两个类中，通过 QToolBar 逐个 addAction 注册。按钮的创建、图标、文字、checked 状态、互斥逻辑、可见性控制分散在多处，导致美化困难、维护割裂、新增按钮需改多处。需要创建统一的 SideBar 组件，将所有侧边栏按钮的创建、布局、状态管理集中到一个类中。

## What Changes
- 新建 `pv/widgets/sidebar.h/.cpp`，创建 `SideBar` 类（继承 `QWidget` + `IUiWindow`），统一管理所有侧边栏按钮
- SideBar 内部使用 `QVBoxLayout` 自定义布局，替代 `QToolBar`，支持分组、分割线、自定义间距
- SideBar 统一管理按钮的创建、图标、文字、checked 状态、互斥逻辑
- SideBar 提供 `addItem()` / `addSeparator()` / `setItemVisible()` / `setItemEnabled()` / `setItemChecked()` 等接口
- SideBar 发射统一的 `itemClicked(int index)` 信号，MainWindow 根据索引分发到具体逻辑
- 精简 TrigBar：删除所有侧边栏按钮成员（`_trig_button`、`_protocol_button`、`_measure_button`、`_search_button`、`_function_button`、`_setting_button` 及对应 QAction），保留菜单逻辑（Function 菜单、Display 菜单、主题切换）
- 精简 SamplingBar：删除侧边栏按钮成员（`_configure_button`、`_run_stop_button`、`_instant_button` 及对应 QAction），保留采样参数逻辑
- 修改 MainWindow：删除 `_right_tool_bar`（QToolBar），改用 SideBar；删除 `setupRightToolBar()`，改用 SideBar 的 `addItem()` 注册
- 修改 MainWindow：简化 `on_protocol`/`on_trigger`/`on_measure`/`on_search`/`on_device_options` 中的 DockOptions 互斥逻辑，由 SideBar 统一处理
- **BREAKING**: TrigBar 不再拥有侧边栏按钮，`_trig_action` 等公开成员被移除
- **BREAKING**: SamplingBar 不再拥有侧边栏按钮，`_configure_action`/`_run_stop_action`/`_instant_action` 被移除

## Impact
- Affected specs: `convert-deviceoptions-to-sidebar`（TrigBar 按钮相关部分需适配）
- Affected code:
  - `pv/widgets/sidebar.h`（新建）
  - `pv/widgets/sidebar.cpp`（新建）
  - `pv/mainwindow.h`（修改：删除 `_right_tool_bar`，新增 `_side_bar`，修改槽函数）
  - `pv/mainwindow.cpp`（修改：重写侧边栏注册和 Dock 互斥逻辑）
  - `pv/toolbars/trigbar.h`（修改：删除按钮成员，保留菜单成员）
  - `pv/toolbars/trigbar.cpp`（修改：删除按钮创建/信号/互斥逻辑，保留菜单逻辑）
  - `pv/toolbars/samplingbar.h`（修改：删除侧边栏按钮成员）
  - `pv/toolbars/samplingbar.cpp`（修改：删除侧边栏按钮创建/信号逻辑）
  - `CMakeLists.txt`（修改：新增源文件）

## ADDED Requirements

### Requirement: SideBar 统一侧边栏组件
系统 SHALL 提供统一的 SideBar 组件，集中管理所有侧边栏按钮的创建、布局、状态和交互。

#### Scenario: 添加侧边栏按钮
- **WHEN** 调用 `SideBar::addItem(icon, textId, defaultText, itemType)` 
- **THEN** SideBar 创建一个 XToolButton，设置图标和文字，添加到布局中
- **AND** 返回该按钮的索引

#### Scenario: 添加分割线
- **WHEN** 调用 `SideBar::addSeparator()`
- **THEN** SideBar 在布局中插入一条水平分割线

#### Scenario: 按钮点击互斥
- **WHEN** 用户点击一个 dock 类型按钮（Trigger/Decode/Measure/Search/Options）
- **THEN** 其他所有 dock 类型按钮自动取消选中
- **AND** 被点击的按钮呈现选中状态
- **AND** SideBar 发射 `itemClicked(int index)` 信号

#### Scenario: 非 dock 按钮点击
- **WHEN** 用户点击一个 action 类型按钮（Run/Stop/Instant）
- **THEN** 不影响其他按钮的选中状态
- **AND** SideBar 发射 `itemClicked(int index)` 信号

#### Scenario: 设置按钮可见性
- **WHEN** 调用 `SideBar::setItemVisible(int index, bool visible)`
- **THEN** 对应按钮显示或隐藏

#### Scenario: 设置按钮启用状态
- **WHEN** 调用 `SideBar::setItemEnabled(int index, bool enabled)`
- **THEN** 对应按钮启用或禁用

#### Scenario: 设置按钮选中状态
- **WHEN** 调用 `SideBar::setItemChecked(int index, bool checked)`
- **THEN** 对应按钮呈现选中或未选中状态

### Requirement: SideBar 按钮类型区分
SideBar SHALL 区分两种按钮类型：dock 类型（互斥切换）和 action 类型（独立触发）。

#### Scenario: dock 类型按钮
- **WHEN** 按钮类型为 `DockItem`
- **THEN** 按钮为 checkable，点击后与其他 dock 按钮互斥
- **AND** 点击已选中的 dock 按钮会取消选中

#### Scenario: action 类型按钮
- **WHEN** 按钮类型为 `ActionItem`
- **THEN** 按钮为非 checkable，点击后仅触发信号
- **AND** 不影响其他按钮状态

### Requirement: SideBar IUiWindow 接口实现
SideBar SHALL 实现 IUiWindow 接口，支持语言、主题和字体更新。

#### Scenario: 语言更新
- **WHEN** UiManager 广播语言更新
- **THEN** SideBar 更新所有按钮文本为当前语言

#### Scenario: 主题更新
- **WHEN** UiManager 广播主题更新
- **THEN** SideBar 更新所有按钮图标为当前主题

#### Scenario: 字体更新
- **WHEN** UiManager 广播字体更新
- **THEN** SideBar 更新所有按钮的字体大小

### Requirement: SideBar 布局自定义
SideBar SHALL 使用 QVBoxLayout 自定义布局，替代 QToolBar，支持灵活的布局控制。

#### Scenario: 按钮排列
- **WHEN** SideBar 显示
- **THEN** 按钮从上到下垂直排列
- **AND** 按钮之间有适当间距
- **AND** 分割线将按钮分组

#### Scenario: 按钮样式
- **WHEN** SideBar 显示
- **THEN** 按钮采用 `Qt::ToolButtonTextUnderIcon` 样式（图标在上，文字在下）
- **AND** 图标大小为 24x24

### Requirement: SideBar 与 SlidingDrawer 联动
SideBar 的 dock 类型按钮点击后，SHALL 自动联动 SlidingDrawer 打开对应页面。

#### Scenario: dock 按钮点击打开抽屉
- **WHEN** 用户点击一个 dock 类型按钮
- **THEN** SideBar 发射 `dockItemClicked(int index)` 信号
- **AND** MainWindow 接收信号后打开对应的 SlidingDrawer 页面

#### Scenario: SlidingDrawer 关闭时更新按钮状态
- **WHEN** SlidingDrawer 关闭
- **THEN** MainWindow 调用 `SideBar::clearAllChecked()` 取消所有 dock 按钮的选中状态

## MODIFIED Requirements

### Requirement: TrigBar 职责精简
TrigBar 不再拥有侧边栏按钮，仅保留菜单逻辑和主题切换逻辑。

原有职责：
```
侧边栏按钮（Trigger/Decode/Measure/Search/Function/Display）+ 菜单逻辑 + 主题切换
```

新职责：
```
仅菜单逻辑（Function 菜单、Display 菜单）+ 主题切换信号
```

保留的成员：
- `_function_menu`、`_action_fft`、`_action_math`（Function 菜单）
- `_display_menu`、`_themes`、`_action_dispalyOptions`、`_action_lissajous`、`_dark_style`、`_light_style`（Display 菜单）
- `sig_setTheme(QString)` 信号
- `sig_show_lissajous(bool)` 信号

删除的成员：
- `_trig_button`、`_protocol_button`、`_measure_button`、`_search_button`、`_function_button`、`_setting_button`
- `_trig_action`、`_protocol_action`、`_measure_action`、`_search_action`、`_function_action`、`_display_action`
- `sig_protocol(bool)`、`sig_trigger(bool)`、`sig_measure(bool)`、`sig_search(bool)` 信号
- `protocol_clicked()`、`trigger_clicked()`、`measure_clicked()`、`search_clicked()` 槽函数
- `getDockOptions()`、`update_checked_status()` 方法
- `buttonGroup` 成员

### Requirement: SamplingBar 职责精简
SamplingBar 不再拥有侧边栏按钮，仅保留采样参数逻辑。

删除的成员：
- `_configure_button`、`_run_stop_button`、`_instant_button`（XToolButton）
- `_configure_action`、`_run_stop_action`、`_instant_action`（QAction*）
- `sig_device_options(bool)` 信号
- `on_configure()` 方法中的信号发射逻辑

### Requirement: MainWindow 侧边栏逻辑简化
MainWindow 的侧边栏相关逻辑由分散的多个槽函数简化为统一的 SideBar 信号处理。

原有逻辑：
```
setupRightToolBar() → 逐个 addAction
on_protocol(bool) → 手动管理 DockOptions 互斥 + 更新 checked 状态
on_trigger(bool) → 手动管理 DockOptions 互斥 + 更新 checked 状态
on_measure(bool) → 手动管理 DockOptions 互斥 + 更新 checked 状态
on_search(bool) → 手动管理 DockOptions 互斥 + 更新 checked 状态
on_device_options(bool) → 手动管理 DockOptions 互斥 + 更新 checked 状态
```

新逻辑：
```
setupSideBar() → addItem() 统一注册
on_side_bar_dock_clicked(int index) → SideBar 自动处理互斥，MainWindow 只需打开对应抽屉页面
on_side_bar_action_clicked(int index) → 执行对应操作（run/stop/instant）
```

### Requirement: DockOptions 互斥逻辑迁移
DockOptions 互斥逻辑从 TrigBar 的各个 clicked() 槽函数迁移到 SideBar 内部统一处理。

原有逻辑（分散在 4 个 clicked 槽函数中，每个都重复设置其他选项为 false）：
```cpp
void TrigBar::protocol_clicked() {
    opt->decodeDock = !opt->decodeDock;
    opt->triggerDock = false;
    opt->measureDock = false;
    opt->searchDock = false;
    opt->deviceOptionsDock = false;
}
```

新逻辑（SideBar 内部统一处理）：
```cpp
void SideBar::onDockItemClicked(int index) {
    // 自动互斥：取消其他 dock 按钮选中，设置当前按钮选中
    // 发射 dockItemClicked(index) 信号
}
// MainWindow 接收信号后更新 DockOptions
```

## REMOVED Requirements

### Requirement: QToolBar 作为右侧侧边栏容器
**Reason**: QToolBar 布局控制能力有限，无法支持自定义间距、分组、分割线等美化需求。
**Migration**: 替换为 SideBar（QWidget + QVBoxLayout），提供完全的布局控制能力。

### Requirement: TrigBar 拥有侧边栏按钮
**Reason**: 按钮分散在多个 Bar 类中导致维护割裂，统一到 SideBar 后更清晰。
**Migration**: 所有侧边栏按钮迁移到 SideBar 统一管理，TrigBar 仅保留菜单逻辑。
