* [x] 拖动期间 `_push_layout->setContentsMargins()` 不被调用（根因 1 修复验证）

* [x] `Viewport::doPaint()` 中不再调用 `check_update()`（根因 2 修复验证）

* [x] `setSlideOffset()` 中不再调用 `pw->update(dirtyRect)`（根因 3 修复验证）

* [x] 拖动/动画期间 `mode_check_timer` 和 `disk_cache_status_timer` 被暂停（根因 4/5 修复验证）

* [x] Viewport 小幅 resize（<4px）不触发 QPixmap 重新分配（根因 1 补充验证）

* [x] SlidingDrawer `resizeEvent` 后续调用不再执行 `raise()`（根因 6 修复验证）

* [x] 打开动画只触发一次 layout reflow（根因 7 修复验证）

* [x] 拖动开始时无 `QTimer::singleShot(0, removePushMargin)` 调用（根因 8 修复验证）

* [x] 拖动结束后 drawer 正确切换到 push 模式（margin 正确、位置正确）

* [x] 关闭动画后 drawer 正确隐藏

* [x] 编译无错误无警告

