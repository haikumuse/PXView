# 解码结果 Dock 跟随波形可视范围过滤 Spec

> 基于 modernize-view-layer-v3 完成后的最新代码架构重写。View 已重构为门面，
> `_scale`/`_offset` 修改集中在 `ViewLayout`/`ViewDataSync`/`ViewCursors` 三个 friend 委托类。

## Why

当前解码结果 dock（`ProtocolDock`）始终全量展示 `DecoderStack` 中所有 annotation，用户缩放/拖动波形到某一段时，右侧列表仍显示全部结果，无法与屏幕可见波形段对应，定位困难（参考 Kingst VIS 的行为：列表只显示当前可见波形段的解析结果）。已有"点击列表项跳转波形"的单向指针（`item_clicked` → `_session->show_region()` → `View::show_region()`），但缺少反向"波形可见范围 → 列表过滤"的联动。

## 当前架构事实（modernize-view-layer-v3 后）

- `View` 是门面，`view.cpp` 中除构造函数初始化外**没有任何直接修改 `_scale`/`_offset` 的代码**，所有公开 API 都是 1 行转发函数。
- 真正修改 `_scale`/`_offset` 的位置集中在 3 个 friend 委托类：
  - `ViewLayout`（`view_layout.cpp`）— 6 个方法直接写字段：`set_scale_offset` / `limit_scale_offset` / `update_scale_offset` / `set_scale` / `zoom(steps, offset)` / `h_scroll_value_changed`
  - `ViewDataSync`（`view_data_sync.cpp`）— 10 处通过 `_view->set_scale_offset(...)` 间接调用（最终收敛到 `ViewLayout::set_scale_offset`）
  - `ViewCursors`（`view_cursors.cpp`）— 3 处通过 `_view->set_scale_offset(...)` 间接调用（同上）
- `View` signals 段（view.h:629-638）有 9 个信号，**无任何 scale/offset/visible-range 相关**。
- `View` **没有 debounce timer**（`_data_updated_timer` 是 QElapsedTimer 用于性能计时，不相关）。
- `DecoderModel` 极简：仅 1 个成员 `_decoder_stack`，重写 4 个方法（`rowCount`/`columnCount`/`data`/`headerData`），**无任何可见范围状态**。`rowCount` 直接返回 `list_annotation_size()` 全量。
- `RowData::get_visible_range(start_sample, end_sample)` **已存在**（`rowdata.cpp:151-171`），返回 `std::pair<size_t, size_t>` 半开区间 `[start_idx, end_idx)`，二分查找。
- `RowData::get_annotation_index(start_sample)` **已存在**（`rowdata.cpp:98-106`），采样点→索引映射，`upper_bound` 二分。
- `DecoderStack::get_annotation_index(row, sample)` 包装**已存在**（`nav_table_view` 在用）。
- `ProtocolDock` **没有 `_follow_viewport` 开关、没有 `_jumping_to_row` 标志**，mark_index 在 `DecoderStack` 上（通过 `set_mark_index(int)` 写入）。
- `ProtocolDock::nav_table_view()` 已实现反向跳转算法（View→表格），其采样点计算公式可复用：`offset_sample = _view->offset() * decoder_stack->samplerate() * _view->scale()`。
- `ProtocolDock` 工具栏用 `QPushButton` + `QHBoxLayout` 自行拼装，不用 `QToolBar`/`QAction`。

## What Changes

### 1. View 层新增 `visible_range_changed` 信号 + debounce 机制

- `view.h` signals 段新增 `void visible_range_changed();`
- `view.h` private 成员区新增 `QTimer *_viewport_change_timer;`（单触发，100ms）
- `view.h` private 方法区新增 `void schedule_visible_range_notify();`（启动 timer，重复调用会重启）
- `view.cpp` 构造函数初始化 timer 并 connect timeout → emit `visible_range_changed()`
- `view.cpp` 实现 `schedule_visible_range_notify()`：`_viewport_change_timer->start();`（QTimer::start 自带重启语义）
- **在 7 个出口调用 `schedule_visible_range_notify()`**（覆盖所有改变可视采样范围的路径）：
  - `ViewLayout::set_scale_offset`（`view_layout.cpp:55`）末尾 — 收敛 `view_data_sync.cpp`/`view_cursors.cpp` 的所有间接调用
  - `ViewLayout::limit_scale_offset`（`view_layout.cpp:79`）末尾
  - `ViewLayout::update_scale_offset`（`view_layout.cpp:104`）末尾
  - `ViewLayout::set_scale`（`view_layout.cpp:128`）末尾
  - `ViewLayout::zoom(double steps, int offset)`（`view_layout.cpp:143`）末尾 — 滚轮缩放
  - `ViewLayout::h_scroll_value_changed`（`view_layout.cpp:191`）末尾 — 滚动条拖动
  - `ViewDataSync::resizeEvent`（`view_data_sync.cpp:643`）末尾 — 窗口缩放改变 view_width，可视采样点范围随之改变

### 2. DecoderStack 新增 `get_visible_range` 包装（Core 层）

- `decoderstack.h` public 接口新增：
  ```cpp
  std::pair<size_t, size_t> get_visible_range(const decode::Row &row,
                                              uint64_t start_sample,
                                              uint64_t end_sample);
  ```
- `decoderstack.cpp` 实现：转发到对应 `RowData::get_visible_range`
- **不动 Core 层存储/查询逻辑**（`RowData` 现有接口直接复用），符合 AGENTS.md "Core code must NOT #include QWidget" 硬约束

### 3. DecoderModel 新增可视范围切片接口

- `decodermodel.h` private 成员区新增 `int64_t _visible_start_row = -1;` `int64_t _visible_end_row = -1;`（默认 -1 表示全量，向后兼容）
- `decodermodel.h` public 接口新增 `void set_visible_range(int64_t start_row, int64_t end_row);` `void clear_visible_range();`
- `decodermodel.cpp` 实现：
  - `set_visible_range`：保存参数 + `beginResetModel`/`endResetModel`
  - `clear_visible_range`：设 `_visible_start_row = -1` + reset
  - `rowCount`：若 `_visible_start_row >= 0`，返回 `max(0, min(_visible_end_row, 全量行数) - _visible_start_row)`；否则原逻辑
  - `data`：若 `_visible_start_row >= 0`，实际查询行号 = `_visible_start_row + index.row()`；否则原逻辑
  - `headerData` 垂直表头：若 `_visible_start_row >= 0`，返回 `section + _visible_start_row`（保持原行号显示，避免用户看到行号跳变）；否则原逻辑

### 4. ProtocolDock 新增"列表跟随视口"开关 + 联动槽

- `protocoldock.h` private 成员区新增：
  - `QToolButton *_follow_viewport_btn;`（checkable，默认 checked）
  - `bool _follow_viewport = true;`
  - `bool _jumping_to_row = false;`
- `protocoldock.h` private slots 新增：
  - `void on_follow_viewport_toggled(bool checked);`
  - `void on_visible_range_changed();`
- `protocoldock.cpp` 底部面板 `bot_title_layout`（`protocoldock.cpp:174-182`）新增 `QToolButton`（与 `_dn_nav_button` 同级，图标用 filter.svg，tooltip "列表跟随波形可视范围过滤"）
- `bind_context` 中 connect：
  - `_view->visible_range_changed` → `on_visible_range_changed`（仅当 `_follow_viewport` 为 true）
  - `_follow_viewport_btn->toggled` → `on_follow_viewport_toggled`
- `on_follow_viewport_toggled`：
  - checked=true：connect 信号 + 立即调一次 `on_visible_range_changed()`
  - checked=false：disconnect 信号 + 调 `_decoder_model->clear_visible_range()`
- `on_visible_range_changed` 算法（复用 `nav_table_view` 的采样点计算）：
  1. `decoder_stack = _decoder_model->getDecoderStack()`；if(!decoder_stack) return;
  2. `samplerate = decoder_stack->samplerate()`；if(samplerate == 0) return;
  3. `scale = _view->scale()`；`offset = _view->offset()`
  4. `samples_per_pixel = samplerate * scale`
  5. `start_sample = (uint64_t)max(offset * samples_per_pixel, 0.0)`
  6. `viewport_width = _view->viewport()->width()`；if(viewport_width <= 0) return;
  7. `end_sample = (uint64_t)max((offset + viewport_width) * samples_per_pixel, 0.0)`
  8. 取当前 `filterKeyColumn` 对应的协议行（遍历 `decoder_stack->get_rows_lshow()`，找第 N 个 visible row，与 `nav_table_view` 算法一致）
  9. `[start_idx, end_idx) = decoder_stack->get_visible_range(row, start_sample, end_sample)`
  10. **mark_index 豁免**：`mark = decoder_stack->get_mark_index()`；若 `mark >= 0`，`mark_row = decoder_stack->get_annotation_index(row, mark)`；若 `mark_row >= end_idx`，扩展 `end_idx = mark_row + 1`；若 `mark_row < start_idx`，扩展 `start_idx = mark_row`
  11. `_decoder_model->set_visible_range(start_idx, end_idx)`

### 5. item_clicked 跳转防回环

- `item_clicked`（`protocoldock.cpp:774`）在调用 `_session->show_region(...)` 之前设 `_jumping_to_row = true`
- `on_visible_range_changed` 检查 `_jumping_to_row`：若为 true，则在 `set_visible_range` 之后用 `blockSignals` + `setCurrentIndex` 恢复当前选中行，然后用 `QTimer::singleShot(150ms)` 清除 `_jumping_to_row` 标志
- 跳转过程中 `show_region` 异步触发 `View::show_region` → `ViewDataSync::show_region` → `set_scale_offset` → `schedule_visible_range_notify` → 100ms 后 emit `visible_range_changed` → `on_visible_range_changed`，此时 `_jumping_to_row` 仍为 true，恢复选中行；150ms 后清除标志，后续正常过滤

## Impact

- Affected specs:
  - `optimize-dock-scroll-perf`（互补，本 spec 关注过滤逻辑而非绘制性能）
  - `optimize-decode-zoom-perf`（互补，本 spec 关注 dock 列表而非 paint_mid）
- Affected code:
  - `PXView/pv/view/view.h` — 新增 `visible_range_changed` 信号、`_viewport_change_timer` 成员、`schedule_visible_range_notify` 方法
  - `PXView/pv/view/view.cpp` — 构造函数初始化 timer + connect；实现 `schedule_visible_range_notify`
  - `PXView/pv/view/view_layout.cpp` — 6 个方法末尾调 `_view->schedule_visible_range_notify()`
  - `PXView/pv/view/view_data_sync.cpp` — `resizeEvent` 末尾调 `_view->schedule_visible_range_notify()`
  - `PXView/pv/data/decoderstack.h` / `decoderstack.cpp` — 新增 `get_visible_range` 包装
  - `PXView/pv/view/decodermodel.h` / `decodermodel.cpp` — 新增 `_visible_start_row`/`_visible_end_row` 成员、`set_visible_range`/`clear_visible_range` 接口、`rowCount`/`data`/`headerData` 切片
  - `PXView/pv/dock/protocoldock.h` / `protocoldock.cpp` — 新增开关按钮、槽函数、`item_clicked` 防回环
- 无 BREAKING 变更：
  - 开关默认 ON 体现新功能，用户可关闭回到原行为
  - `DecoderModel` 默认 `_visible_start_row = -1` 表示全量，向后兼容
  - `DecoderStack::get_visible_range` 是新增包装，不影响现有接口
- 不动 Core 层存储/查询逻辑（`RowData` 现有 `get_visible_range`/`get_annotation_index` 直接复用），符合 AGENTS.md "Core code must NOT #include QWidget" 硬约束

## ADDED Requirements

### Requirement: View 可视范围变化通知

系统 SHALL 在 `view::View` 中新增 `visible_range_changed()` 信号，当 scale 或 offset 变化导致可视采样点范围改变时发射。

#### Scenario: set_scale_offset 触发
- **WHEN** `ViewLayout::set_scale_offset()` 改变了 `_scale` 或 `_offset`（含 `ViewDataSync`/`ViewCursors` 的所有间接调用）
- **THEN** 启动 debounce 定时器（100ms 单触发）
- **AND** 定时器超时后 emit `visible_range_changed()`

#### Scenario: 滚动条拖动触发
- **WHEN** `ViewLayout::h_scroll_value_changed()` 直接赋值 `_offset`
- **THEN** 同样启动 debounce 定时器，超时后 emit 信号

#### Scenario: 滚轮缩放触发
- **WHEN** `ViewLayout::zoom(steps, offset)` 直接修改 `_scale`/`_offset`（不经过 `set_scale_offset`）
- **THEN** 同样启动 debounce 定时器，超时后 emit 信号

#### Scenario: samplerate/timebase 变化触发
- **WHEN** `ViewLayout::update_scale_offset()` 因 samplerate/sampletime 变化重算 scale
- **THEN** emit `visible_range_changed()`（经 debounce）

#### Scenario: 文档切换/边界裁剪触发
- **WHEN** `ViewLayout::limit_scale_offset()` 或 `ViewLayout::set_scale()` 重算边界
- **THEN** emit `visible_range_changed()`（经 debounce）

#### Scenario: 窗口缩放触发
- **WHEN** `ViewDataSync::resizeEvent()` 因窗口缩放改变 view_width（可视采样点范围随之改变）
- **THEN** emit `visible_range_changed()`（经 debounce）

#### Scenario: 连续 drag/zoom 节流
- **WHEN** 用户连续拖动波形或滚轮缩放（每帧触发 `set_scale_offset`）
- **THEN** debounce 定时器反复重启，仅最后一次触发 emit
- **AND** 不会因像素级连续触发导致 dock 列表频繁 reset 卡顿

### Requirement: DecoderModel 按可视范围切片

系统 SHALL 在 `view::DecoderModel` 中新增 `set_visible_range`/`clear_visible_range` 接口，`rowCount`/`data`/`headerData` 按范围切片。

#### Scenario: 切片激活时 rowCount
- **WHEN** `_visible_start_row >= 0`
- **THEN** `rowCount` 返回 `max(0, min(_visible_end_row, 全量行数) - _visible_start_row)`

#### Scenario: 切片激活时 data
- **WHEN** `_visible_start_row >= 0`
- **THEN** `data(index)` 实际查询行号 = `_visible_start_row + index.row()`

#### Scenario: 切片激活时 headerData 垂直表头
- **WHEN** `_visible_start_row >= 0`
- **THEN** `headerData(section, Vertical)` 返回 `section + _visible_start_row`（保持原行号显示）

#### Scenario: 全量模式向后兼容
- **WHEN** `_visible_start_row = -1`（默认值）
- **THEN** `rowCount`/`data`/`headerData` 行为与改动前完全一致

### Requirement: ProtocolDock 列表跟随视口

系统 SHALL 在 `dock::ProtocolDock` 工具栏新增"列表跟随视口"toggle 按钮（默认 ON），开关 ON 时接收 `visible_range_changed` 信号并按可视采样点范围过滤列表。

#### Scenario: 开关 ON 时拖动波形
- **WHEN** 用户拖动波形改变可视范围
- **THEN** 100ms debounce 后列表 `rowCount` 只显示可视范围内的 annotation

#### Scenario: 开关 OFF 时恢复全量
- **WHEN** 用户关闭开关
- **THEN** disconnect 信号 + `clear_visible_range`，列表恢复全量展示

#### Scenario: 点击列表项跳转波形
- **WHEN** 用户点击列表项触发 `item_clicked` → `show_region` 跳转
- **THEN** 跳转过程中 `_jumping_to_row = true`，跳转完成后 `on_visible_range_changed` 恢复选中行
- **AND** 150ms 后清除 `_jumping_to_row` 标志，后续正常过滤

#### Scenario: mark_index 行豁免过滤
- **WHEN** `decoder_stack->get_mark_index()` 对应的行不在当前切片范围
- **THEN** 扩展切片范围包含 mark 行（保证跳转后选中行不丢失）

#### Scenario: 无 DecoderStack 绑定
- **WHEN** `_decoder_model->getDecoderStack()` 返回 null
- **THEN** `on_visible_range_changed` 直接 return，不修改 model

#### Scenario: samplerate 为 0
- **WHEN** `decoder_stack->samplerate() == 0`
- **THEN** `on_visible_range_changed` 直接 return，不修改 model

### Requirement: DecoderStack get_visible_range 包装

系统 SHALL 在 `data::DecoderStack` 新增 `get_visible_range(row, start_sample, end_sample)` 包装，转发到 `RowData::get_visible_range`。

#### Scenario: 取可见范围行索引
- **WHEN** ProtocolDock 调用 `decoder_stack->get_visible_range(row, start_sample, end_sample)`
- **THEN** 返回 `std::pair<size_t, size_t>` 半开区间 `[start_idx, end_idx)`

## 技术设计细节

### 可视范围采样点计算（复用 nav_table_view 算法）

```cpp
// nav_table_view 现有算法（protocoldock.cpp:864）：
uint64_t offset_sample = _view->offset() * decoder_stack->samplerate() * _view->scale();

// on_visible_range_changed 扩展算法：
double scale = _view->scale();
int64_t offset = _view->offset();
uint64_t samplerate = decoder_stack->samplerate();
double samples_per_pixel = samplerate * scale;
uint64_t start_sample = (uint64_t)std::max(offset * samples_per_pixel, 0.0);
int viewport_width = _view->viewport()->width();
uint64_t end_sample = (uint64_t)std::max((offset + viewport_width) * samples_per_pixel, 0.0);
```

### 取当前 filterKeyColumn 对应协议行（复用 nav_table_view 算法）

```cpp
// nav_table_view 现有算法（protocoldock.cpp:866-875）：
std::map<const pv::data::decode::Row, bool> rows = decoder_stack->get_rows_lshow();
int column = _model_proxy.filterKeyColumn();
for (auto i = rows.begin(); i != rows.end(); i++) {
    if ((*i).second && column-- == 0) {
        // (*i).first 就是当前协议行
        break;
    }
}
```

### 7 个出口覆盖所有可视范围变化路径的证明

| 修改路径 | 触发方法 | 是否经 set_scale_offset 收敛 |
|---------|---------|----------------------------|
| ViewDataSync::capture_init | set_scale_offset | ✓ |
| ViewDataSync::show_region | set_scale_offset | ✓ |
| ViewDataSync::timebase_changed | set_scale_offset | ✓ |
| ViewDataSync::mode_changed | set_scale_offset | ✓ |
| ViewDataSync::auto_set_max_scale | set_scale | ✗（单独触发） |
| ViewDataSync::scroll_to_logic_last_data_time | set_scale_offset | ✓ |
| ViewDataSync::resizeEvent | set_scale_offset | ✓（但 view_width 也变，需单独触发） |
| ViewCursors::set_trig_cursor_posistion | set_scale_offset | ✓ |
| ViewCursors::set_search_pos | set_scale_offset | ✓ |
| ViewCursors::set_cursor_middle | set_scale_offset | ✓ |
| ViewLayout::set_scale_offset | 直接写字段 | -（本方法触发） |
| ViewLayout::limit_scale_offset | 直接写字段 | ✗（单独触发） |
| ViewLayout::update_scale_offset | 直接写字段 | ✗（单独触发） |
| ViewLayout::set_scale | 直接写字段 | ✗（单独触发） |
| ViewLayout::zoom | 直接写字段 | ✗（单独触发） |
| ViewLayout::h_scroll_value_changed | 直接写字段 | ✗（单独触发） |
| dso_measure.cpp zoom 调用 | View::zoom | 经 ViewLayout::zoom |
| viewport_interaction.cpp zoom 调用 | View::zoom | 经 ViewLayout::zoom |

结论：7 个出口（`set_scale_offset` / `limit_scale_offset` / `update_scale_offset` / `set_scale` / `zoom` / `h_scroll_value_changed` / `resizeEvent`）覆盖所有路径。

## 验证

- 编译：`cd build && ninja -j 16 && ninja install`
- Headless 启动 + MCP API tools/list 返回 17 tools（确认无回归）
- GUI 手动验证：
  1. 开关 ON 时拖动波形 → 列表跟随过滤
  2. 开关 ON 时滚轮缩放波形 → 列表跟随过滤
  3. 开关 ON 时窗口缩放 → 列表跟随过滤
  4. 开关 OFF 时 → 列表恢复全量
  5. 点击列表项跳转波形 → 选中行不丢失
  6. 多 decoder stack 切换 → 过滤逻辑正常
