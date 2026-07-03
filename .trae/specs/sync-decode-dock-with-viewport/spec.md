# 解码结果 Dock 跟随波形可视范围过滤 Spec

## Why
当前解码结果 dock（`ProtocolDock`）始终全量展示 `DecoderStack` 中所有 annotation，用户缩放/拖动波形到某一段时，右侧列表仍显示全部结果，无法与屏幕可见波形段对应，定位困难（参考 Kingst VIS 的行为：列表只显示当前可见波形段的解析结果）。已有"点击列表项跳转波形"的单向指针（`item_clicked` → `show_region`），但缺少反向"波形可见范围 → 列表过滤"的联动。

## What Changes
- 在 `view::View` 新增 `visible_range_changed()` 信号，覆盖所有改变可视范围的出口（`set_scale_offset` / `h_scroll_value_changed` / `update_scale_offset` / `limit_scale_offset`）
- 信号发射使用 `QTimer` 单触发 debounce（默认 100ms），避免 drag/zoom 像素级连续触发
- 在 `view::DecoderModel` 新增 `set_visible_range(int64_t start_row, int64_t end_row)` / `clear_visible_range()` 接口，`rowCount`/`data` 按范围切片
- 在 `dock::ProtocolDock` 工具栏新增"列表跟随视口"开关按钮（toggle，默认 ON）
- 开关 ON 时：ProtocolDock 接收 `visible_range_changed` → 计算可视采样点范围 → 调 `RowData::get_visible_range` 取行索引区间 → 设进 `DecoderModel` → `beginResetModel`/`endResetModel` 刷新表格
- 开关 OFF 时：断开 connect，`DecoderModel` 清除范围，恢复全量展示（保持原行为）
- `item_clicked` 跳转波形过程中临时豁免过滤（保留当前选中行 + mark_index 行始终豁免过滤）

## Impact
- Affected specs: `optimize-dock-scroll-perf`（互补，本 spec 关注过滤逻辑而非绘制性能）、`optimize-decode-zoom-perf`（互补，本 spec 关注 dock 列表而非 paint_mid）
- Affected code:
  - `PXView/pv/view/view.h` — 新增 `visible_range_changed` 信号、debounce timer 成员、emit 出口
  - `PXView/pv/view/view.cpp` — `set_scale_offset` / `h_scroll_value_changed` / `update_scale_offset` / `limit_scale_offset` emit；debounce 实现
  - `PXView/pv/view/decodermodel.h` — 新增 `_visible_start_row` / `_visible_end_row` 成员、`set_visible_range` / `clear_visible_range` 方法
  - `PXView/pv/view/decodermodel.cpp` — `rowCount` / `data` 按范围切片
  - `PXView/pv/dock/protocoldock.h` — 新增"跟随视口"开关按钮成员、`on_visible_range_changed` 槽、`_follow_viewport` 标志
  - `PXView/pv/dock/protocoldock.cpp` — 工具栏加按钮、connect/disconnect、槽函数实现、`item_clicked` 豁免逻辑
- 无 BREAKING 变更：开关默认 ON 体现新功能，用户可关闭回到原行为；`DecoderModel` 默认 `_visible_start_row = -1` 表示全量，向后兼容
- 不动 Core 层（`DecoderStack` / `RowData` 现有 `get_visible_range` / `get_annotation_subset` 接口直接复用），符合 AGENTS.md "Core code must NOT #include QWidget" 硬约束

## ADDED Requirements

### Requirement: View 可视范围变化通知
系统 SHALL 在 `view::View` 中新增 `visible_range_changed()` 信号，当 scale 或 offset 变化导致可视采样点范围改变时发射。

#### Scenario: set_scale_offset 触发
- **WHEN** `View::set_scale_offset()` 改变了 `_scale` 或 `_offset`
- **THEN** 启动 debounce 定时器（100ms 单触发）
- **AND** 定时器超时后 emit `visible_range_changed()`

#### Scenario: 滚动条拖动触发
- **WHEN** `View::h_scroll_value_changed()` 直接赋值 `_offset`
- **THEN** 同样启动 debounce 定时器，超时后 emit 信号

#### Scenario: samplerate 变化导致 scale 重算
- **WHEN** `View::update_scale_offset()` 因 samplerate/sampletime 变化重算 scale
- **THEN** emit `visible_range_changed()`（经 debounce）

#### Scenario: 文档切换重算边界
- **WHEN** `View::limit_scale_offset()` 因文档切换重算边界
- **THEN** emit `visible_range_changed()`（经 debounce）

#### Scenario: 连续 drag/zoom 节流
- **WHEN** 用户连续拖动波形或滚轮缩放（每帧触发 set_scale_offset）
- **THEN** debounce 定时器反复重启，仅在最后一次变化后 100ms 才 emit 一次
- **AND** drag 过程中列表不刷新，drag 停止后才刷新

### Requirement: DecoderModel 可视范围切片
系统 SHALL 在 `view::DecoderModel` 中新增可视行范围切片能力，`rowCount` 和 `data` 按范围返回切片结果。

#### Scenario: 设置可视行范围
- **WHEN** 调用 `set_visible_range(int64_t start_row, int64_t end_row)` 且参数有效（start_row >= 0, end_row > start_row）
- **THEN** 保存到 `_visible_start_row` / `_visible_end_row`
- **AND** 调用 `beginResetModel()` / `endResetModel()` 刷新表格

#### Scenario: 清除可视范围恢复全量
- **WHEN** 调用 `clear_visible_range()`
- **THEN** `_visible_start_row = -1` 表示全量
- **AND** 调用 `beginResetModel()` / `endResetModel()` 刷新表格

#### Scenario: rowCount 按范围切片
- **WHEN** `_visible_start_row >= 0`
- **THEN** `rowCount()` 返回 `min(_visible_end_row - _visible_start_row, 全量行数 - _visible_start_row)`

#### Scenario: data 按范围偏移
- **WHEN** `_visible_start_row >= 0`
- **AND** QTableView 请求 `index.row() = R` 的数据
- **THEN** 实际查询 `DecoderStack::list_annotation(col, _visible_start_row + R)`

#### Scenario: 默认全量行为
- **WHEN** `_visible_start_row = -1`（未设置范围）
- **THEN** `rowCount` 和 `data` 行为与改动前完全一致（向后兼容）

### Requirement: ProtocolDock "列表跟随视口"开关
系统 SHALL 在 `dock::ProtocolDock` 工具栏新增"列表跟随视口"toggle 按钮，控制列表是否跟随波形可视范围过滤。

#### Scenario: 默认开启
- **WHEN** ProtocolDock 首次构造
- **THEN** 工具栏显示"列表跟随视口"toggle 按钮，默认选中（ON）
- **AND** connect `View::visible_range_changed` 到 `ProtocolDock::on_visible_range_changed`

#### Scenario: 用户关闭开关
- **WHEN** 用户点击 toggle 按钮切换为 OFF
- **THEN** disconnect `visible_range_changed` 信号
- **AND** 调用 `DecoderModel::clear_visible_range()` 恢复全量展示
- **AND** 按钮状态持久化到会话配置（可选，若现有 dock 配置机制支持）

#### Scenario: 用户重新开启
- **WHEN** 用户点击 toggle 按钮切换为 ON
- **THEN** 重新 connect `visible_range_changed` 信号
- **AND** 立即触发一次 `on_visible_range_changed()` 同步当前范围

#### Scenario: 开关状态有 tooltip
- **WHEN** 鼠标悬停在 toggle 按钮
- **THEN** 显示 tooltip "列表跟随波形可视范围过滤（仅显示当前屏幕可见波形的解析结果）"

### Requirement: 可视范围过滤逻辑
系统 SHALL 在 `ProtocolDock::on_visible_range_changed()` 槽中计算可视采样点范围并过滤列表。

#### Scenario: 计算可视采样点范围
- **WHEN** `on_visible_range_changed()` 被触发
- **AND** `_decoder_model` 当前绑定了有效的 `DecoderStack`
- **THEN** 计算 `start_sample = _view->offset() * (decoder_stack->samplerate() * _view->scale())`
- **AND** 计算 `end_sample = (_view->offset() + viewport_width) * (decoder_stack->samplerate() * _view->scale())`
- **AND** 调用 `decoder_stack->get_row_data(row)->get_visible_range(start_sample, end_sample)` 取 `[start_idx, end_idx)` 索引区间
- **AND** 调用 `_decoder_model->set_visible_range(start_idx, end_idx)`

#### Scenario: 多 row 取并集
- **WHEN** `DecoderStack` 有多个可见 row
- **THEN** 对每个 row 取 `get_visible_range`，使用最大 end_idx 作为切片上界（保证所有 row 的可见 annotation 都在切片内）
- **OR** 仅对当前显示的主 row 过滤（实现简单，MVP 方案）

#### Scenario: 无 DecoderStack 绑定
- **WHEN** `_decoder_model` 未绑定 `DecoderStack`
- **THEN** 槽函数直接 return，不操作

### Requirement: 点击列表项跳转波形的豁免
系统 SHALL 在 `item_clicked` 跳转波形过程中临时豁免过滤，避免跳转后选中行丢失。

#### Scenario: 跳转过程中豁免
- **WHEN** 用户点击列表项触发 `item_clicked`
- **THEN** 设置 `_jumping_to_row = true` 标志
- **AND** 调用 `show_region` 跳转波形
- **AND** 在 `on_visible_range_changed` 槽中检查 `_jumping_to_row`，若为 true 则仅更新范围不重置选中行
- **AND** 跳转完成后（debounce 定时器触发后）清除 `_jumping_to_row` 标志

#### Scenario: mark_index 行始终豁免过滤
- **WHEN** `DecoderStack::set_mark_index` 设置了标记行
- **AND** mark 行不在当前可视范围切片内
- **THEN** mark 行仍应在列表中可见（追加到切片末尾，或豁免过滤）

## MODIFIED Requirements

### Requirement: DecoderModel 行数与数据查询
原实现 `rowCount` 直接返回 `decoder_stack->list_annotation_size()`，`data` 直接调 `list_annotation(col, row)`。新实现 SHALL 在设置了可视范围时按范围切片，未设置时保持原行为。

### Requirement: ProtocolDock 工具栏
原工具栏仅有搜索、导航等按钮。新实现 SHALL 新增"列表跟随视口"toggle 按钮，与现有按钮风格一致。

## REMOVED Requirements
无（向后兼容，开关 OFF 即恢复原行为）。
