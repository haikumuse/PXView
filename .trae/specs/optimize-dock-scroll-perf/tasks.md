# Tasks

- [x] Task 1: DeviceOptionsDock 不透明绘制属性
  - [x] SubTask 1.1: 在构造函数中，对 `_container_panel`、`_dynamic_panel`（创建后）、`mode_section`、`_glitch_filter_group`（创建后）设置 `setAttribute(Qt::WA_OpaquePaintEvent)` + `setAttribute(Qt::WA_NoSystemBackground)`
  - [x] SubTask 1.2: 在 `update_view()` 重建子 Widget 后，对新创建的 `mode_section`、`_glitch_filter_group` 等同样设置不透明属性
  - [x] SubTask 1.3: 在 `build_dynamic_panel()` 创建 `_dynamic_panel` 后设置不透明属性

- [x] Task 2: update_view 增量更新
  - [x] SubTask 2.1: `UpdateLanguage()` 保持调用 `update_view()`（语言变更影响大量字符串，全量重建可接受）
  - [x] SubTask 2.2: 将 `UpdateFont()` 改为遍历现有 Widget 更新字体，不调用 `update_view()`
  - [x] SubTask 2.3: 将 `UpdateTheme()` 改为调用 `update()` 触发样式重绘，不调用 `update_view()`
  - [x] SubTask 2.4: 保留 `update_view()` 供 `device_updated()`、`bind_context()` 等真正需要全量重建的场景使用

- [x] Task 3: 毛刺过滤面板扁平化
  - [x] SubTask 3.1: 重写 `build_glitch_filter_panel()`，移除 `row_container` 和 `content_widget` 中间容器，改用 `QGridLayout` 直接在 `ch_container` 中排列通道行
  - [x] SubTask 3.2: 同样修改 `rebuild_glitch_filter_panel()` 使用扁平化布局
  - [x] SubTask 3.3: 确保毛刺过滤面板的信号槽连接（checkbox/spinbox/button）在扁平化后仍正常工作

- [x] Task 4: try_resize_scroll 批量操作优化
  - [x] SubTask 4.1: 在 `try_resize_scroll()` 中，先调用 `_container_lay->setEnabled(false)` 禁用布局，再批量 `setFixedSize`，最后恢复布局

- [x] Task 5: SmoothScrollArea 动画期间禁用布局
  - [x] SubTask 5.1: 在 `handleVWheel()` 和 `handleHWheel()` 中，动画启动前对 `widget()->layout()` 调用 `setEnabled(false)`
  - [x] SubTask 5.2: 在动画 finished 回调中，对 `widget()->layout()` 调用 `setEnabled(true)` + `activate()`

# Task Dependencies
- [Task 2] depends on [Task 1]（增量更新需要先确保不透明属性设置正确）
- [Task 3] 无前置依赖，可独立开始
- [Task 4] 无前置依赖，可独立开始
- [Task 5] 无前置依赖，可独立开始
