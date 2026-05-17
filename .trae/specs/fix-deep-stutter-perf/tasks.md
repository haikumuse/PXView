# Tasks

- [x] Task 1: LOGIC 模式 QPixmap 智能缓存（P0）
  - [x] 1.1: 修改 `viewport.cpp` 的 `paintSignals()` LOGIC 分支（约第 549 行）：将 `_pixmap = QPixmap(size())` 替换为与非 LOGIC 分支相同的 4px 阈值缓存逻辑
  - [x] 1.2: 验证 LOGIC 分支的 QPixmap 分配逻辑与 DSO/Analog 分支一致

- [x] Task 2: Signal Group 卡片预计算缓存（P1）
  - [x] 2.1: 在 `viewport.h` 中新增结构体 `CachedGroupCard { QRectF rect; }` 和成员变量 `std::vector<CachedGroupCard> _cached_group_cards` 以及 `bool _group_cards_dirty = true`
  - [x] 2.2: 在 `viewport.h` 中新增公共方法 `void invalidate_group_cache()`
  - [x] 2.3: 在 `viewport.cpp` 中实现 `invalidate_group_cache()`：设置 `_group_cards_dirty = true`
  - [x] 2.4: 修改 `doPaint()` 中的 signal group 卡片绘制代码（约第 372-417 行）：当 `_group_cards_dirty` 为 true 时，执行排序和边界计算，将结果存入 `_cached_group_cards`，然后设置 `_group_cards_dirty = false`；当为 false 时，直接使用 `_cached_group_cards` 绘制
  - [x] 2.5: 在 `View::signals_changed()` 中调用 viewport 的 `invalidate_group_cache()`，确保 signals 变化时缓存失效
  - [x] 2.6: 验证 signals 未变化时 doPaint 不执行 group 排序

- [x] Task 3: Theme 颜色缓存（P2）
  - [x] 3.1: 在 `viewport.h` 中新增成员变量 `QColor _cached_divider_color`
  - [x] 3.2: 修改 `viewport.cpp` 构造函数：初始化 `_cached_divider_color` 为无效颜色
  - [x] 3.3: 修改 `doPaint()` 中的 divider 颜色获取逻辑（约第 420-427 行）：优先使用 `_cached_divider_color`，仅在缓存无效时回退到 `GetThemeColor` 查找
  - [x] 3.4: 修改 `Viewport::UpdateTheme()`（当前为空函数）：查询 `GetThemeColor("@trace-divider-color")` 并更新 `_cached_divider_color`
  - [x] 3.5: 验证 doPaint 中不再每帧调用 GetThemeColor

- [x] Task 4: Resize 节流（P4）
  - [x] 4.1: 在 `viewport.h` 中新增 `QTimer _resize_throttle_timer` 和 `bool _resize_pending = false`
  - [x] 4.2: 在 `viewport.cpp` 构造函数中初始化 `_resize_throttle_timer.setSingleShot(true); _resize_throttle_timer.setInterval(16);` 并连接到 slot
  - [x] 4.3: 修改 `resizeEvent()`：设置 `_resize_pending = true`，启动 `_resize_throttle_timer`
  - [x] 4.4: 在 `paintEvent()` 中：如果 `_resize_pending` 为 true，仅绘制背景色并返回，跳过完整 doPaint
  - [x] 4.5: 在 throttle timer timeout slot 中：如果 `_resize_pending`，则 `QWidget::update()` 触发重绘并重置标志
  - [x] 4.6: 验证连续 resize 期间不会每帧都触发完整 doPaint

- [x] Task 5: 编译验证
  - [x] 5.1: 执行 `build_incremental.cmd` 确保无编译错误
  - [x] 5.2: 检查无运行时警告或崩溃

# Task Dependencies
- Task 1 独立，可立即开始
- Task 2 涉及 viewport.h/cpp 和 view.cpp，与 Task 3/4 可并行
- Task 3 独立，可并行
- Task 4 独立，可并行
- Task 5 依赖所有其他 Task
