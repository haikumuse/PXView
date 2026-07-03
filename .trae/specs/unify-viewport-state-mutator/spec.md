# 统一 View 可视范围状态 Mutator Spec

## Why
当前 `view::View` 的核心状态字段 `_scale` / `_offset` **没有统一的 mutator 入口**，散落在 view.cpp 至少 8 个函数、13 处直接赋值（见下表）。任何想订阅"可视范围变化"的消费者（解码 dock、mark、measure、cursor 等）都必须在 N 个出口手动加 `_viewport_change_timer->start()`，新增出口（如 touchpad gesture）极易遗漏，产生静默漏触 bug。

这违反了 project_memory 中明确的"真相源必须单一，避免双写一致性问题"和"生命周期管理必须使用 RAII，消除手动管理的错误风险"原则。前一个 spec `sync-decode-dock-with-viewport` 已经踩到这个坑：补了 4 个出口后仍漏了 `mode_changed`、`show_region` 等路径，导致 dock 不灵敏。

| 函数 | 行号 | 直接赋值点 |
|---|---|---|
| `zoom` | 657/671/677/678 | 4 处（已补 timer） |
| `set_scale_offset` | 937/938 | 2 处（已补 timer） |
| `limit_scale_offset` | 958/959 | 2 处（已补 timer） |
| `update_scale_offset` | 1263/1265/1268/1271 | 4 处（已补 timer） |
| `mode_changed` | 1285/1287 | 2 处（**漏 timer**） |
| `resizeEvent` | 1620/1627 | 2 处（已补 timer） |
| `h_scroll_value_changed` | 1650/1655/1658 | 3 处（已补 timer） |
| `show_region` | 2072/2075（局部变量，最终走 set_scale_offset） | 间接 |
| `set_scale` | 2550 | 1 处（已补 timer） |

`viewport.cpp` 的 8 处全部走 `set_scale_offset`，已覆盖。

## What Changes
- **新增 private 统一 mutator** `View::apply_scale_offset(double scale, int64_t offset)`：内部完成 clamp → 比较是否真的改变 → 写入 `_scale`/`_offset` → 启动 `_viewport_change_timer`
- **新增 private 统一 mutator** `View::apply_scale(double scale)`：仅改 `_scale`，内部委托 `apply_scale_offset(scale, _offset)`
- **新增 private 统一 mutator** `View::apply_offset(int64_t offset)`：仅改 `_offset`，内部委托 `apply_scale_offset(_scale, offset)`
- **删除所有直接赋值**：view.cpp 中所有 `_scale = ...` / `_offset = ...` 全部替换为 `apply_scale_offset(...)` / `apply_scale(...)` / `apply_offset(...)` 调用（13 处赋值点，涉及 8 个函数）
- **移除散落的 `_viewport_change_timer->start()` 调用**：因 mutator 内部已统一启动 timer，外部调用点全部删除（避免双重启动，虽然 QTimer::start 幂等但代码冗余）
- **保留** `set_scale_offset` / `set_scale` / `limit_scale_offset` / `update_scale_offset` / `h_scroll_value_changed` / `resizeEvent` / `zoom` / `mode_changed` / `show_region` 的**对外签名和语义不变**（仅内部实现改为委托 mutator）
- **删除** `mode_changed` 和 `show_region` 漏 timer 的 bug（重构后自然消除，不需单独打补丁）

## Impact
- Affected specs:
  - `sync-decode-dock-with-viewport` — 本 spec 是其架构级根治，重构后该 spec 的 4 个 timer.start() 调用点和 mode_changed/show_region 漏 timer 问题全部消除
  - `modernize-view-layer-architecture` — 互补，本 spec 关注状态 mutator，那个 spec 关注 sr_channel 解耦
  - `optimize-decode-zoom-perf` — 互补，本 spec 关注状态变更通知，那个 spec 关注 paint 性能
- Affected code:
  - `PXView/pv/view/view.h` — 新增 3 个 private mutator 声明
  - `PXView/pv/view/view.cpp` — 新增 3 个 mutator 实现；8 个函数内部 13 处赋值点改为委托；删除散落的 timer.start() 调用
- 无 BREAKING 变更：所有 public/private 函数签名不变，仅内部实现重构
- 不动 Core 层，符合 AGENTS.md "Core code must NOT #include QWidget" 硬约束
- 不动 `viewport.cpp`（其 8 处已走 set_scale_offset，自动受益）

## ADDED Requirements

### Requirement: 统一可视范围状态 Mutator
系统 SHALL 在 `view::View` 中提供 3 个 private 统一 mutator，作为 `_scale`/`_offset` 的唯一写入入口。

#### Scenario: apply_scale_offset 统一入口
- **WHEN** 任何代码需要同时改变 `_scale` 和 `_offset`
- **THEN** 调用 `apply_scale_offset(double scale, int64_t offset)`
- **AND** mutator 内部按顺序执行：clamp scale 到 [_minscale, _maxscale]、clamp offset 到 [get_min_offset(), get_max_offset()]
- **AND** 比较 new vs pre：若 _scale 或 _offset 实际改变，写入新值并启动 `_viewport_change_timer`
- **AND** 若未改变，不启动 timer（避免无意义刷新）

#### Scenario: apply_scale 单字段入口
- **WHEN** 任何代码仅需改变 `_scale`
- **THEN** 调用 `apply_scale(double scale)`
- **AND** 内部委托 `apply_scale_offset(scale, _offset)`

#### Scenario: apply_offset 单字段入口
- **WHEN** 任何代码仅需改变 `_offset`
- **THEN** 调用 `apply_offset(int64_t offset)`
- **AND** 内部委托 `apply_scale_offset(_scale, offset)`

#### Scenario: clamp 顺序保持
- **WHEN** mutator 执行 clamp
- **THEN** scale 先 clamp，offset 后 clamp（offset 边界可能依赖 scale，保持与原 set_scale_offset 行为一致）

### Requirement: 删除所有直接 _scale/_offset 赋值
系统 SHALL 删除 view.cpp 中所有 `_scale = ...` 和 `_offset = ...` 直接赋值，全部替换为 mutator 调用。

#### Scenario: zoom 函数改造
- **WHEN** `View::zoom()` 需要改变 _scale 和 _offset
- **THEN** 内部所有 `_scale = ...` / `_offset = ...` 替换为 `apply_scale_offset(...)` / `apply_scale(...)` / `apply_offset(...)`
- **AND** 删除函数内已存在的 `_viewport_change_timer->start()` 调用（mutator 内部已启动）

#### Scenario: set_scale_offset 函数改造
- **WHEN** `View::set_scale_offset(double scale, int64_t offset)` 被调用
- **THEN** 函数体简化为：保存 _preScale/_preOffset → 调 `apply_scale_offset(scale, offset)` → viewport_update/update_scroll/_header->update()/_ruler->update()
- **AND** 删除函数内已存在的 `_viewport_change_timer->start()` 调用

#### Scenario: limit_scale_offset 函数改造
- **WHEN** `View::limit_scale_offset()` 被调用（文档切换重算边界）
- **THEN** 内部 `_scale = max(min(...))` / `_offset = max(min(...))` 替换为 `apply_scale_offset(_scale, _offset)`（先 clamp 再调 mutator 二次确认）
- **OR** 直接调 `apply_scale_offset(_scale, _offset)` 让 mutator 内部 clamp（更简洁，推荐）
- **AND** 删除函数内已存在的 `_viewport_change_timer->start()` 调用

#### Scenario: update_scale_offset 函数改造
- **WHEN** `View::update_scale_offset()` 因 samplerate 变化重算 scale
- **THEN** 内部 4 处赋值替换为 mutator 调用
- **AND** 删除函数内已存在的 `_viewport_change_timer->start()` 调用

#### Scenario: mode_changed 函数改造
- **WHEN** `View::mode_changed()` 因虚拟设备模式切换重算 scale
- **THEN** 内部 `_scale = WellSamplesPerPixel * 1.0 / samplerate` 和 `_scale = max(min(...))` 替换为 `apply_scale(...)`
- **AND** 修复原漏 timer 的 bug（mutator 内部自动启动）

#### Scenario: resizeEvent 函数改造
- **WHEN** `View::resizeEvent()` 因窗口缩放重算 scale
- **THEN** 内部 `_scale = _session->cur_view_time() / width` 和 `_scale = _maxscale` 替换为 `apply_scale(...)`
- **AND** 删除函数内已存在的 `_viewport_change_timer->start()` 调用

#### Scenario: h_scroll_value_changed 函数改造
- **WHEN** `View::h_scroll_value_changed(int value)` 因滚动条拖动改 _offset
- **THEN** 内部 3 处 `_offset = ...` 替换为 `apply_offset(...)`
- **AND** 删除函数内已存在的 `_viewport_change_timer->start()` 调用

#### Scenario: set_scale 函数改造
- **WHEN** `View::set_scale(double scale)` 被调用（程序化设 scale）
- **THEN** 函数体简化为：保存 _preScale → 调 `apply_scale(scale)` → viewport_update/update_scroll/_header->update()/_ruler->update()
- **AND** 删除函数内已存在的 `_viewport_change_timer->start()` 调用

#### Scenario: show_region 函数验证
- **WHEN** `View::show_region()` 计算新 scale/offset
- **THEN** 因其最终走 `set_scale_offset`，无需单独改造（自动受益）
- **AND** 验证 show_region 路径下 timer 正常启动

### Requirement: 无残留直接赋值
重构完成后，view.cpp 中**不允许存在任何** `_scale = ...` 或 `_offset = ...` 直接赋值（grep 0 命中，排除注释和字符串）。

#### Scenario: grep 验证
- **WHEN** 执行 `grep -nE "_scale\s*=|_offset\s*=" PXView/pv/view/view.cpp`
- **THEN** 仅返回注释行（如 `// layout.v_offset = ...`）和局部变量赋值（如 `double next_v_offset = ...`、`const double new_scale = ...`）
- **AND** 不返回任何 `_scale = ...` 或 `_offset = ...` 成员赋值

## MODIFIED Requirements

### Requirement: View 可视范围变化通知
原实现（`sync-decode-dock-with-viewport` spec）在 4 个出口（set_scale_offset/h_scroll_value_changed/update_scale_offset/limit_scale_offset）+ 后续补丁 3 个出口（zoom/resizeEvent/set_scale）手动调 `_viewport_change_timer->start()`，共 7 个出口。新实现 SHALL 通过统一 mutator 自动启动 timer，无需在调用点手动启动，并覆盖全部 8 个出口（含原漏掉的 mode_changed）。

### Requirement: View 内部状态字段写入
原实现 `_scale` / `_offset` 为 private 成员，但 view.cpp 内多个函数直接赋值。新实现 SHALL 仅通过 3 个 private mutator 写入，保证单一入口。

## REMOVED Requirements
无（纯重构，对外行为不变，无 BREAKING）。
