# SlidingDrawer 滑动时 Viewport 掉帧 — 真正根因分析

> **场景：** SlidingDrawer 在 Viewport 上方滑动（动画打开/关闭/拖动边框）时掉帧

---

## 一、问题的根本机制

### Widget 层级关系

```
_central_widget (QWidget)
├── _vertical_layout → _tab_widget → View → Viewport  ← 重绘昂贵
└── _sliding_drawer (SlidingDrawer)                    ← overlay 子 widget
```

`SlidingDrawer` 和 `Viewport` **是同一个父 widget (`_central_widget`) 的兄弟关系**。
SlidingDrawer 叠在 Viewport 上方。

### Qt 的 Widget 重绘机制（这是根因）

当 SlidingDrawer 调用 `setGeometry()` 移动位置时：

1. Qt 标记旧位置区域为 dirty
2. Qt 标记新位置区域为 dirty
3. **父 widget (`_central_widget`) 的 dirty 区域需要被重绘**
4. 由于 Viewport 占据了 `_central_widget` 的大部分面积，**Viewport 的 `paintEvent` 被触发**
5. `doPaint()` 执行全量绘制 → 300ms 卡顿帧

**每一帧动画**都会经历这个过程：

```
QPropertyAnimation → setSlideOffset() → positionOverlay() → setGeometry()
    → Qt 标记 _central_widget 上旧位置+新位置为 dirty
        → Viewport::paintEvent() 被触发
            → doPaint() 全量重绘
```

> [!CAUTION]
> **这就是为什么所有针对 applyPushMargin / timer / pixmap 的优化都无效的原因：**
> 即使你完全不动 layout margin，**仅仅是 `setGeometry()` 移动 SlidingDrawer 的位置，就已经触发了 Viewport 的重绘**。这是 Qt Widget 层面的底层行为，不是你代码层面能"优化"掉的。

---

## 二、排查方法：加性能埋点确认

在 **viewport.cpp** 的 `paintEvent` 中加入以下诊断代码，编译后运行，打开/关闭 SlidingDrawer 时观察控制台输出：

```cpp
void Viewport::paintEvent(QPaintEvent *event) {
  // ========== 诊断代码 START ==========
  static QElapsedTimer diagTimer;
  static int diagCount = 0;
  if (!diagTimer.isValid()) diagTimer.start();
  
  diagCount++;
  QElapsedTimer frameTimer;
  frameTimer.start();
  // ========== 诊断代码 END (part 1) ==========

  // ... 原有代码 ...
  doPaint();
  
  // ========== 诊断代码 START (part 2) ==========
  int paintMs = frameTimer.elapsed();
  if (paintMs > 10) {  // 只打印超过 10ms 的帧
    qDebug() << "Viewport::paintEvent took" << paintMs << "ms"
             << " updateRect:" << event->rect()
             << " widgetSize:" << size()
             << " count:" << diagCount;
  }
  // ========== 诊断代码 END ==========
}
```

同时在 **slidingdrawer.cpp** 的 `setSlideOffset` 中加：

```cpp
void SlidingDrawer::setSlideOffset(int offset) {
  offset = qBound(0, offset, _drawer_width);
  if (_slide_offset == offset)
    return;

  // ========== 诊断代码 ==========
  static QElapsedTimer diagTimer;
  if (!diagTimer.isValid()) diagTimer.start();
  QElapsedTimer frameTimer;
  frameTimer.start();
  // ========================================

  _slide_offset = offset;
  positionOverlay();

  // ========== 诊断代码 ==========
  int ms = frameTimer.elapsed();
  if (ms > 5) {
    qDebug() << "setSlideOffset took" << ms << "ms, offset=" << offset;
  }
  // ========================================
}
```

### 预期观察结果

- 打开/关闭 SlidingDrawer 动画期间，你会看到 `Viewport::paintEvent` 被**每帧触发**
- `updateRect` 会显示是 Viewport 的部分区域（= SlidingDrawer 移动前/后覆盖的区域）
- `paintEvent` 每次耗时 10-300ms（因为 `doPaint()` 做全量绘制）

---

## 三、修复方案

### 方案 A：动画期间让 Qt 不要重绘被覆盖的 Viewport（最推荐）

SlidingDrawer 是**不透明的**，它下面的 Viewport 区域根本不需要重绘。但 Qt 不知道这一点（因为默认 widget 可能是半透明的），所以 Qt 每帧都去重绘 Viewport。

**解决方案：** 在动画期间，对 Viewport 设置 `setUpdatesEnabled(false)`，动画结束后再恢复。

在 **mainwindow.cpp** 中已有的 `animationStateChanged` 连接中加入：

```cpp
connect(_sliding_drawer, &widgets::SlidingDrawer::animationStateChanged, this,
        [this](bool active) {
          // 暂停/恢复后台定时器
          // ...（已有代码）...
          
          // ★ 关键修复：动画期间冻结 Viewport 重绘
          pv::view::View *view = current_view();
          if (view && view->get_time_view()) {
            view->get_time_view()->setUpdatesEnabled(!active);
          }
        });
```

同样对 `dragStateChanged` 也加：

```cpp
connect(_sliding_drawer, &widgets::SlidingDrawer::dragStateChanged, this,
        [this](bool active) {
          // ...（已有代码）...
          
          // ★ 关键修复：拖动期间冻结 Viewport 重绘
          pv::view::View *view = current_view();
          if (view && view->get_time_view()) {
            view->get_time_view()->setUpdatesEnabled(!active);
          }
        });
```

**原理：** `setUpdatesEnabled(false)` 会让 Qt 跳过该 widget 的所有 `paintEvent`，即使 Qt 检测到 dirty region 也不会去绘制它。动画结束后恢复 `setUpdatesEnabled(true)` 会自动触发一次 update。

**优点：** 零侵入 doPaint，零 CPU 开销
**缺点：** 动画期间 Viewport 是"冻结"的快照，如果 Viewport 内容在动画期间变化（如实时采集），会看到短暂的画面停滞

---

### 方案 B：使用 g_drag_snapshot 快照（已有的基础设施）

你已经有了 `g_drag_active` 和 `g_drag_snapshot` 机制！在 `paintEvent` 中已经有：

```cpp
if (g_drag_active && !g_drag_snapshot.isNull()) {
    QPainter p(this);
    p.drawPixmap(0, 0, g_drag_snapshot);
    return;  // ← 跳过 doPaint()！
}
```

**但这个机制目前只在 tab 拖拽时使用。** 可以扩展到 SlidingDrawer 动画/拖动期间使用：

在动画开始前拍快照：
```cpp
// SlidingDrawer 动画开始时
pv::view::Viewport *vp = current_view()->get_time_view();
vp->g_drag_snapshot = vp->grab();  // 截图当前画面
vp->g_drag_active = true;

// 动画结束后
vp->g_drag_active = false;
vp->g_drag_snapshot = QPixmap();
```

**优点：** 复用已有机制，Viewport paintEvent 变成简单的 drawPixmap（<1ms）
**缺点：** 需要一次 grab() 的开销；画面冻结

---

### 方案 C：SlidingDrawer 使用独立 native window（终极方案）

让 SlidingDrawer 变成独立的 native window，而不是 Qt 子 widget：

```cpp
SlidingDrawer::SlidingDrawer(QWidget *parent) : QWidget(parent, Qt::Window | Qt::FramelessWindowHint) {
    setAttribute(Qt::WA_NativeWindow);
    // ...
}
```

**原理：** native window 有自己的 backing store，移动时不会导致父 widget 或兄弟 widget 重绘
**优点：** 根本消除问题
**缺点：** 需要处理 z-order、位置同步等复杂问题

---

## 四、建议的实施顺序

1. **先加诊断代码**（第二节），确认 Viewport paintEvent 确实在动画期间被频繁触发
2. **实施方案 A**（setUpdatesEnabled）— 最简单，5 行代码
3. 如果方案 A 的"画面冻结"不可接受，改用**方案 B**（g_drag_snapshot）
4. 长期考虑**方案 C**（native window）

> [!IMPORTANT]
> **核心结论：** 问题不在 doPaint 有多重、不在 applyPushMargin 有没有调用、不在定时器有没有暂停。问题在于 **Qt 的 widget compositing 模型会在 SlidingDrawer 每次 setGeometry() 时触发 Viewport 的 paintEvent**。只要 doPaint 不是瞬时的（它永远不可能是瞬时的），就会掉帧。解决方案是**阻止 paintEvent 被触发**或**让 paintEvent 变成 O(1) 的快照绘制**。
