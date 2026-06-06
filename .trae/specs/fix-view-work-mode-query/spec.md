# 修复 View 工作模式查询错误 Spec

## Why
多标签页架构下，viewport 和 header 中通过 `_view.session().get_device()->get_work_mode()` 查询工作模式，返回的是硬件设备的模式而非当前标签页的模式。当硬件处于 LOGIC 模式而当前标签页显示 DSO 数据时，模式检查返回 LOGIC，导致 DSO 信号被当作 Logic 信号处理——可以拖动改变高度、高度不自适应。

## What Changes
- 在 `View` 中添加 `get_work_mode()` 方法，优先从 `_document->get_signal_config().work_mode` 获取模式
- 将 `viewport.cpp` 和 `header.cpp` 中所有 `_view.session().get_device()->get_work_mode()` 替换为 `_view.get_work_mode()`

## Impact
- Affected specs: add-multi-tab-sessions, fix-dso-analog-hw-offset-config
- Affected code:
  - `PXView/pv/view/view.h`（添加 `get_work_mode()` 声明）
  - `PXView/pv/view/view.cpp`（实现 `get_work_mode()`）
  - `PXView/pv/view/viewport.cpp`（替换模式查询）
  - `PXView/pv/view/header.cpp`（替换模式查询）

## ADDED Requirements

### Requirement: View 提供正确的标签页工作模式查询
`View::get_work_mode()` SHALL 优先从当前标签页的 `SessionDocument` 获取工作模式，若无 document 或无配置则回退到设备模式。

#### Scenario: 当前标签页有 document 配置时返回 document 模式
- **WHEN** `_document` 存在且 `has_signal_config()` 为 true
- **THEN** `get_work_mode()` 返回 `_document->get_signal_config().work_mode`

#### Scenario: 当前标签页无 document 配置时返回设备模式
- **WHEN** `_document` 不存在或 `has_signal_config()` 为 false
- **THEN** `get_work_mode()` 返回 `_device_agent->get_work_mode()`

### Requirement: viewport 和 header 使用 View 的模式查询
viewport.cpp 和 header.cpp 中所有工作模式查询 SHALL 使用 `_view.get_work_mode()` 而非 `_view.session().get_device()->get_work_mode()`。

#### Scenario: DSO 标签页下拖动不改变信号高度
- **WHEN** 硬件设备处于 LOGIC 模式，但当前标签页显示 DSO 数据
- **THEN** `_view.get_work_mode()` 返回 DSO，拖动改变高度的逻辑被正确屏蔽

#### Scenario: Logic 标签页下拖动正常工作
- **WHEN** 当前标签页显示 Logic 数据
- **THEN** `_view.get_work_mode()` 返回 LOGIC，拖动改变高度功能正常

## MODIFIED Requirements
无

## REMOVED Requirements
无
