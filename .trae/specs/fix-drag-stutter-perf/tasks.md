# Tasks

- [x] Task 1: 重构 SlidingDrawer 拖动为全程 Overlay 模式（根因 1 + 8）
  - [x] 1.1: 修改 `mouseMoveEvent()`：删除 `_drag_margin_removed` 标志和 `QTimer::singleShot(0, removePushMargin)` 逻辑，拖动期间完全不调用 `removePushMargin()`
  - [x] 1.2: 修改 `finishDrag()`：拖动结束时调用 `applyPushMargin()` + `_slide_offset = 0` + `positionOverlay()`，一次性切换到 push 模式
  - [x] 1.3: 修改 `setDrawerWidth()`：拖动期间（`apply_push = false`）仅调用 `setFixedSize()` + `positionOverlay()`，不触碰 margin
  - [x] 1.4: 验证拖动期间 `_push_layout->setContentsMargins()` 不被调用

- [x] Task 2: 统一开/关动画的 margin 切换（根因 7）
  - [x] 2.1: 修改 `open()`：删除 `removePushMargin()` 调用（打开时 drawer 从 overlay 滑入，不需要 remove margin）
  - [x] 2.2: 修改 `close()`：保留 `removePushMargin()` 调用（从 push 模式切换到 overlay），但确保只调用一次
  - [x] 2.3: 修改动画 finished 回调：打开结束后 `applyPushMargin()` + `_slide_offset = 0` + `positionOverlay()`（已有），关闭结束后 `hide()`（已有）
  - [x] 2.4: 验证开/关动画各只触发一次 layout reflow

- [x] Task 3: 将 `check_update()` 从 `doPaint()` 中移出（根因 2）
  - [x] 3.1: 在 `Viewport` 类中新增 `QTimer _check_update_timer`，间隔 80ms
  - [x] 3.2: 在 `Viewport` 构造函数中初始化定时器，连接到 slot 调用 `_view.session().check_update()`
  - [x] 3.3: 从 `Viewport::doPaint()` 中删除 `_view.session().check_update()` 调用
  - [x] 3.4: 在 `Viewport::showEvent()` 中启动定时器，`hideEvent()` 中停止定时器
  - [x] 3.5: 验证 paint 路径中不再有 check_update 调用

- [x] Task 4: Viewport QPixmap 智能缓存（根因 1 补充）
  - [x] 4.1: 在 `Viewport` 类中新增 `QSize _pixmap_size` 成员变量，记录当前 pixmap 的实际尺寸
  - [x] 4.2: 修改 `paintSignals()` 中的 `_pixmap = QPixmap(size())` 逻辑：仅当 `size()` 与 `_pixmap_size` 差异超过 4 像素时才重建 QPixmap
  - [x] 4.3: 当 size 变化小于阈值时，复用已有 `_pixmap`，但设置 `_need_update = true` 以触发内容重绘
  - [x] 4.4: 更新 `_pixmap_size` 在每次重建后
  - [x] 4.5: 验证小幅 resize 不触发 QPixmap 重新分配

- [x] Task 5: 拖动/动画期间暂停后台定时器（根因 4 + 5）
  - [x] 5.1: 在 `SlidingDrawer` 中新增信号 `dragStateChanged(bool active)` 和 `animationStateChanged(bool active)`
  - [x] 5.2: 在 `mouseMoveEvent()` 拖动开始时 emit `dragStateChanged(true)`，在 `finishDrag()` 时 emit `dragStateChanged(false)`
  - [x] 5.3: 在 `open()`/`close()` 动画开始时 emit `animationStateChanged(true)`，在动画 finished 回调中 emit `animationStateChanged(false)`
  - [x] 5.4: 在 `MainWindow` 中连接 `dragStateChanged` 和 `animationStateChanged` 信号，暂停/恢复 `_disk_cache_status_timer`
  - [x] 5.5: 在 `DeviceOptionsDock` 中连接信号，暂停/恢复 `_mode_check_timer`（通过 MainWindow 中转或直接连接）
  - [x] 5.6: 验证拖动期间两个定时器不触发

- [x] Task 6: 消除 `setSlideOffset()` 的父 widget 脏区域更新（根因 3）
  - [x] 6.1: 修改 `setSlideOffset()`：删除 `pw->update(dirtyRect)` 调用
  - [x] 6.2: 依赖 `Qt::WA_OpaquePaintEvent` 属性（动画期间已设置）确保 drawer 移动后旧区域被系统正确处理
  - [x] 6.3: 验证动画期间不触发父 widget 的级联重绘

- [x] Task 7: 优化 `resizeEvent` 中的冗余 raise() 调用（根因 6）
  - [x] 7.1: 在 `SlidingDrawer` 类中新增 `bool _child_raised = false` 成员变量
  - [x] 7.2: 修改 `resizeEvent()`：`raise()` 调用改为仅在 `!_child_raised` 时执行，执行后设置 `_child_raised = true`
  - [x] 7.3: 在 `showEvent()` 或 drawer 首次显示时重置 `_child_raised = false`
  - [x] 7.4: 验证后续 resize 不再调用 raise()

- [x] Task 8: 编译验证
  - [x] 8.1: 执行 `build_incremental.cmd` 确保无编译错误
  - [x] 8.2: 检查无运行时警告或崩溃

# Task Dependencies
- Task 1 和 Task 2 相互依赖（都涉及 SlidingDrawer 的 margin 逻辑），应一起完成
- Task 3 独立，可并行
- Task 4 独立，可并行
- Task 5 依赖 Task 1/2（需要新的信号），应在 Task 1/2 之后
- Task 6 独立，可并行
- Task 7 独立，可并行
- Task 8 依赖所有其他 Task
