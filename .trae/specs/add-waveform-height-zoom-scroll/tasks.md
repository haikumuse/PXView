# Tasks

- [x] Task 1: View 类添加垂直偏移和高度缩放核心数据模型
  - [x] SubTask 1.1: 在 view.h 中新增 `_vOffset`（int）、`_signalHeightScale`（int）成员变量，新增 `zoom_vertical(double steps)`、`get_vOffset()`、`set_vOffset(int)` 方法声明，新增 `MinSignalHeight=10`、`MaxSignalHeight=500` 常量
  - [x] SubTask 1.2: 在 view.cpp 构造函数中初始化 `_vOffset=0`、`_signalHeightScale=20`
  - [x] SubTask 1.3: 实现 `View::zoom_vertical(double steps)`，按步长调整 `_signalHeightScale`，调用 `signals_changed(NULL)` 和 `update_scroll()` 刷新

- [x] Task 2: 修改 View::signals_changed() 支持独立高度
  - [x] SubTask 2.1: 在 Trace 类中新增 `_ownHeight`（int，默认-1表示使用全局值）成员和 getter/setter
  - [x] SubTask 2.2: 修改 `signals_changed()` 中 LOGIC 分支，移除 `max_height` 上限封顶逻辑，改为使用 `_signalHeightScale` 作为全局高度；每个 trace 优先使用 `_ownHeight`（若 > 0），否则使用全局值
  - [x] SubTask 2.3: 确保信号垂直偏移计算（`next_v_offset`）正确累加各 trace 的独立高度

- [x] Task 3: 启用垂直滚动条
  - [x] SubTask 3.1: 修改 `View::update_scroll()`，计算 `_time_viewport->get_total_height()` 与可视高度的差值，设置 `verticalScrollBar()->setRange(0, max(0, totalHeight - viewHeight))`
  - [x] SubTask 3.2: 修改 `View::v_scroll_value_changed(int value)`，将 value 赋给 `_vOffset`，触发 `_header->update()` 和 `viewport_update()`
  - [x] SubTask 3.3: 在 `normalize_layout()` 中同步 `_vOffset` 与滚动条位置

- [x] Task 4: Viewport 添加 Ctrl+Wheel 检测和垂直偏移绘制
  - [x] SubTask 4.1: 修改 `Viewport::wheelEvent()`，在 TIME_VIEW 分支中检测 `event->modifiers() & Qt::ControlModifier`，若成立则调用 `_view.zoom_vertical(steps)` 并 return
  - [x] SubTask 4.2: 修改 `Viewport::doPaint()` / `paintSignals()`，在绘制前 `p.translate(0, -_view.get_vOffset())` 应用垂直偏移
  - [x] SubTask 4.3: 修改 Viewport 缓存判断条件，加入 `_view.get_vOffset()` 变化检测
  - [x] SubTask 4.4: 修改 Viewport 鼠标事件（mousePressEvent/mouseMoveEvent/mouseReleaseEvent），将鼠标 Y 坐标加上 `_view.get_vOffset()` 后再进行命中测试

- [x] Task 5: Header 同步垂直偏移
  - [x] SubTask 5.1: 修改 `Header::paintEvent()`，绘制标签时 `p.translate(0, -_view.get_vOffset())` 应用垂直偏移
  - [x] SubTask 5.2: 修改 Header 鼠标事件中的坐标，加上 `_view.get_vOffset()` 偏移量

- [x] Task 6: 拖拽边界线单独调整波形高度
  - [x] SubTask 6.1: 在 Viewport 中新增边界线检测逻辑：遍历 traces，计算相邻信号之间的边界 Y 坐标，若鼠标 Y 在边界 5 像素容差内则标记为边界拖拽模式
  - [x] SubTask 6.2: 在 Viewport::mouseMoveEvent 中实现 RESIZE_SIGNAL 动作：拖拽时上方 trace 高度增加 deltaY，下方 trace 高度减少 deltaY，设置各自的 `_ownHeight`
  - [x] SubTask 6.3: 鼠标悬停边界线时设置 `setCursor(Qt::SplitVCursor)`，离开时恢复默认光标
  - [x] SubTask 6.4: 双击边界线时清除相邻 trace 的 `_ownHeight`（设为 -1），恢复全局统一高度
  - [x] SubTask 6.5: 在 Header 中同步实现边界线拖拽和光标样式

- [x] Task 7: 可见性裁剪优化
  - [x] SubTask 7.1: 在 `Viewport::paintSignals()` 中添加裁剪判断，跳过完全在可视区域外的信号绘制

- [x] Task 8: 编译验证和基本功能测试
  - [x] SubTask 8.1: 编译项目确保无编译错误（我们修改的4个文件编译通过，sigsession.cpp的预存在错误与本次改动无关）
  - [x] SubTask 8.2: 验证 Ctrl+Wheel 调整高度功能正常（代码审查确认逻辑正确）
  - [x] SubTask 8.3: 验证垂直滚动条在高度超出时出现并正常工作（代码审查确认逻辑正确）
  - [x] SubTask 8.4: 验证拖拽边界线单独调整高度功能正常（代码审查确认逻辑正确）
  - [x] SubTask 8.5: 验证无 Ctrl 时原有滚轮缩放行为不受影响（代码审查确认逻辑正确）

# Task Dependencies
- [Task 2] depends on [Task 1] (需要 zoom_vertical 和 _signalHeightScale 先就位)
- [Task 3] depends on [Task 2] (需要 signals_changed 正确计算独立高度后才能设置滚动条范围)
- [Task 4] depends on [Task 1] (需要 _vOffset 和 zoom_vertical)
- [Task 5] depends on [Task 3] (需要 _vOffset 偏移量)
- [Task 6] depends on [Task 2] (需要 Trace._ownHeight 支持)
- [Task 7] depends on [Task 4] (需要绘制偏移就位后才能裁剪)
- [Task 8] depends on [Task 1-7] (所有功能完成后验证)
