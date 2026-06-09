# 边沿导航按钮 Spec

## Why
用户在查看逻辑信号波形时，需要快速跳转到上一个/下一个边沿位置。目前只能通过鼠标拖拽或搜索功能手动定位，效率低下。参考 Saleae Logic 2 的实现，在鼠标悬停到逻辑信号行时，显示左右两个浮动按钮，点击即可跳转到视口外的最近边沿。

## What Changes
- 在 Viewport 中新增边沿导航浮动按钮组件（EdgeNavButton），鼠标悬停到逻辑信号行时在行左右两侧显示
- 新增 Viewport 中的边沿导航逻辑：从鼠标悬停位置查找上一个/下一个边沿，并滚动视口到目标位置
- 新增快捷键支持：Alt+Left 跳转上一个边沿，Alt+Right 跳转下一个边沿
- 复用已有的 `LogicSnapshot::get_nxt_edge()` / `get_pre_edge()` 和 `View::set_scale_offset()`

## Impact
- Affected code: `PXView/pv/view/viewport.h`, `PXView/pv/view/viewport.cpp`, `PXView/pv/view/view.h`, `PXView/pv/view/view.cpp`
- 新增文件: `PXView/pv/view/edge_nav_button.h`, `PXView/pv/view/edge_nav_button.cpp`

## ADDED Requirements

### Requirement: 边沿导航浮动按钮

系统 SHALL 在逻辑模式下，当鼠标悬停到逻辑信号行区域时，在信号行的左侧和右侧各显示一个浮动按钮。

#### Scenario: 鼠标悬停显示按钮
- **WHEN** 用户在逻辑模式下，鼠标悬停到某个逻辑信号行区域
- **AND** 当前会话处于停止状态（有已捕获数据）
- **THEN** 在该信号行的左侧显示"上一个边沿"按钮，右侧显示"下一个边沿"按钮
- **AND** 按钮垂直居中于信号行内，水平偏移 5px

#### Scenario: 鼠标离开隐藏按钮
- **WHEN** 鼠标离开逻辑信号行区域
- **THEN** 两个浮动按钮立即隐藏

#### Scenario: 无更远边沿时按钮禁用
- **WHEN** 当前视口左侧之外没有更多边沿
- **THEN** "上一个边沿"按钮显示为禁用状态（灰色，不可点击）
- **WHEN** 当前视口右侧之外没有更多边沿
- **THEN** "下一个边沿"按钮显示为禁用状态

### Requirement: 点击按钮跳转到边沿

系统 SHALL 在用户点击边沿导航按钮时，将视口滚动到目标边沿位置。

#### Scenario: 点击"下一个边沿"按钮
- **WHEN** 用户点击"下一个边沿"按钮
- **THEN** 系统从当前鼠标悬停位置对应的采样索引开始，调用 `LogicSnapshot::get_nxt_edge()` 查找下一个边沿
- **AND** 将视口滚动到目标边沿位置，使目标边沿出现在视口左侧 25% 位置
- **AND** 搜索光标移动到目标边沿位置

#### Scenario: 点击"上一个边沿"按钮
- **WHEN** 用户点击"上一个边沿"按钮
- **THEN** 系统从当前鼠标悬停位置对应的采样索引开始，调用 `LogicSnapshot::get_pre_edge()` 查找上一个边沿
- **AND** 将视口滚动到目标边沿位置，使目标边沿出现在视口右侧 25% 位置
- **AND** 搜索光标移动到目标边沿位置

#### Scenario: 无目标边沿时点击无效果
- **WHEN** 按钮处于禁用状态
- **THEN** 点击无任何效果

### Requirement: 快捷键跳转边沿

系统 SHALL 支持通过键盘快捷键跳转到上一个/下一个边沿。

#### Scenario: Alt+Right 跳转下一个边沿
- **WHEN** 用户按下 Alt+Right
- **AND** 鼠标悬停在某个逻辑信号行上
- **AND** 当前会话处于停止状态
- **THEN** 效果等同于点击"下一个边沿"按钮

#### Scenario: Alt+Left 跳转上一个边沿
- **WHEN** 用户按下 Alt+Left
- **AND** 鼠标悬停在某个逻辑信号行上
- **AND** 当前会话处于停止状态
- **THEN** 效果等同于点击"上一个边沿"按钮

### Requirement: 按钮样式

#### Scenario: 按钮外观
- **THEN** 按钮为圆角矩形（3px 圆角），带 1px 边框
- **AND** 背景色跟随主题面板背景色，边框色跟随主题边框色
- **AND** 按钮内显示方向箭头图标（左箭头 / 右箭头）
- **AND** 按钮尺寸约 24x24 像素
- **AND** 悬停时按钮高亮显示

### Requirement: 仅逻辑模式生效

#### Scenario: 非逻辑模式不显示
- **WHEN** 当前工作模式不是 LOGIC 模式
- **THEN** 不显示边沿导航按钮

#### Scenario: 无数据时不显示
- **WHEN** 当前会话没有已捕获数据（初始状态）
- **THEN** 不显示边沿导航按钮
