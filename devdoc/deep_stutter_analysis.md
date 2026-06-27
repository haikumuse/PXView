# 深层卡顿根因分析 — 为什么优化后还不如原版

> **对比项目:** PXView (当前) vs DSView-ori (原版)
> **日期:** 2026-05-17

---

## 核心结论

**之前的 8 个修复全部"治标不治本"**。它们优化的是 SlidingDrawer 拖动路径上的问题，但**真正的卡顿发生在 Viewport::doPaint() 本身**，而 doPaint 在窗口移动/resize 时被频繁调用。PXView 的 doPaint 比原版**重了很多**，以下是具体对比。

---

## 一、真正的差异：PXView 的 doPaint 做了原版不做的事

### 差异 1：LOGIC 模式额外增加了 QPixmap 双缓冲 + 每帧分配

**原版 DSView (dsview-ori)：** LOGIC 模式**直接绘制到屏幕**，无 QPixmap 中间层

```cpp
// dsview-ori/DSView/pv/view/viewport.cpp: paintSignals()
if (_view.session().get_device()->get_work_mode() == LOGIC) {
    // 直接画到 QPainter p (屏幕)
    for(auto t : traces){
        if (t->enabled()){
            logic_signal->paint_mid_align_sample(p, ...);  // ← 直接画屏幕
        }
    }
}
```

**PXView (当前)：** LOGIC 模式**通过 QPixmap 双缓冲**，且**每次数据变化都 new QPixmap(size())**

```cpp
// PXView/pv/view/viewport.cpp:549-576
if (_view.session().get_device()->get_work_mode() == LOGIC) {
    if (...条件变化...) {
        _pixmap = QPixmap(size());        // ← 每帧分配 ~4MB 像素缓冲 (1920x1080x4)
        _pixmap.fill(Qt::transparent);    // ← memset 4MB
        QPainter dbp(&_pixmap);           // ← 额外 QPainter 实例
        // ... 画到 _pixmap ...
    }
    p.drawPixmap(0, 0, _pixmap);          // ← 额外的 blit 操作
}
```

> [!CAUTION]
> **关键问题：** LOGIC 模式的 QPixmap 缓存**没有应用智能缓存阈值**！只有非 LOGIC 分支（DSO/Analog）使用了你之前添加的 `_pixmap_size` 4px 阈值。LOGIC 分支仍然是 `_pixmap = QPixmap(size())` 每次无条件分配。

对比代码：

| 分支 | 原版 DSView | PXView (当前) |
|------|-----------|--------------|
| LOGIC | 直接画到屏幕，无缓冲 | `QPixmap(size())` 无条件重建 |
| DSO/Analog | `QPixmap(size())` 无条件重建 | 有 4px 阈值智能缓存 ✅ |

**这意味着：** 窗口 resize 导致 Viewport size 变化时，LOGIC 模式的 pixmap **每帧都重建**，而这正是你的 UI 大部分时间所处的模式。

---

### 差异 2：新增的 Signal Group 卡片绘制（原版完全没有）

```cpp
// PXView/pv/view/viewport.cpp:372-417 — 这段代码原版完全不存在
if (_type == TIME_VIEW && _view.session().get_device()->get_work_mode() == LOGIC) {
    const auto &groups = _view.get_signal_groups();
    // 遍历所有 signal groups
    for (size_t idx = 0; idx < group_indices.size(); idx++) {
        // 对每个 group 中的每个 trace 计算边界
        for (auto gt : group.traces) {
            // 浮点运算
        }
        // 圆角矩形绘制 — 开启了反锯齿
        p.drawRoundedRect(cardRect, ...);
    }
}
```

每帧都要：
1. 排序所有 groups
2. 遍历每个 group 的每个 trace 计算边界
3. 为每个 group 绘制圆角矩形（带反锯齿 = CPU 密集）

---

### 差异 3：新增的 Divider 线绘制（原版没有）

```cpp
// PXView/pv/view/viewport.cpp:420-455 — 这段代码原版完全不存在
QColor dividerColor = AppConfig::Instance().GetThemeColor("@trace-divider-color");
// ...查找颜色...

std::set<Trace *> lastInGroup;
// ...遍历 groups 找最后一个 trace...

for (auto t : traces) {
    if (lastInGroup.count(t)) continue;
    p.drawLine(0, traceBottom, _view.get_view_width(), traceBottom);
}
```

每帧都要：
1. 查询 theme 颜色（`GetThemeColor` 做字符串查找）
2. 遍历所有 groups 构建 `lastInGroup` set
3. 为每个 trace 画分隔线

---

### 差异 4：paint_back 遍历带 vOffset 偏移（原版没有）

```cpp
// PXView 新增：
p.save();
p.translate(0, -_view.get_vOffset());  // ← 额外的坐标变换
// ... 画 dividers 和 group cards ...
for (auto t : traces) {
    t->paint_back(p, ...);
}
p.restore();
```

**原版：**
```cpp
// 直接画，无 save/restore/translate
for(auto t : traces){
    t->paint_back(p, ...);
}
```

---

## 二、为什么原版不卡

**原版 doPaint 对 LOGIC 模式的流程：**

```
paintEvent → doPaint:
  1. style()->drawPrimitive()     — 背景
  2. check_update()               — 状态检查（在绘制中，但原版UI简单所以影响小）
  3. paint_back()                  — 遍历 traces
  4. paintSignals():
     → 直接画到屏幕 QPainter    — 无 QPixmap 分配
  5. paint_fore()                  — 遍历 traces
```

**PXView doPaint 对 LOGIC 模式的流程：**

```
paintEvent → doPaint:
  1. style()->drawPrimitive()     — 背景
  2. save/translate(-vOffset)     — ⚡新增
  3. signal group 排序+卡片绘制    — ⚡新增（每帧O(n²)）
  4. divider 颜色查找+线绘制       — ⚡新增
  5. paint_back()                  — 遍历 traces
  6. restore                       — ⚡新增
  7. paintSignals():
     → QPixmap(size()) 分配       — ⚡每帧 ~4MB 分配
     → fill(transparent)          — ⚡每帧 memset
     → 画到 _pixmap               — 与原版相同
     → drawPixmap 到屏幕           — ⚡额外 blit
  8. save/translate(-vOffset)     — ⚡新增
  9. paint_fore()                  — 遍历 traces
  10. restore                      — ⚡新增
```

> [!IMPORTANT]
> **对比结论：** PXView 的每帧 doPaint 比原版多了 **5 个额外操作步骤**，其中最重的是 QPixmap 分配 + signal group 卡片绘制。这些**在每次窗口移动/resize 触发的 paintEvent 中都会执行**。

---

## 三、为什么"优化"后反而不如原版

之前的 8 个优化修复的都是 **SlidingDrawer 和定时器** 的问题，但是：

1. **窗口拖动（title bar drag）** 根本不走 SlidingDrawer 代码路径！`WinNativeWidget::WndProc` 处理 `WM_MOVE`，调用 `ResizeChild()`，直接触发 Qt 的 `resizeEvent` → `paintEvent` → `doPaint()`。SlidingDrawer 的优化**对窗口拖动零影响**。

2. **窗口边框 resize** 同样通过 `WinNativeWidget::WndProc` 的 `WM_SIZE` → `ResizeChild()` → Qt resize → paint 路径。

3. 之前把 `check_update()` 从 `doPaint` 移到了定时器，但 **定时器从未 start()**（`_check_update_timer` 只做了 `setInterval(80)` 和 `connect`，没有 `.start()`）。这意味着 `check_update()` 现在**完全不被调用**了，可能导致功能问题。

---

## 四、真正需要做的修复

### P0：给 LOGIC 模式的 QPixmap 也加智能缓存

当前只有 DSO/Analog 分支有：

```diff
// viewport.cpp:549 — LOGIC 分支
-      _pixmap = QPixmap(size());
-      _pixmap.fill(Qt::transparent);
+      QSize curSize = size();
+      if (_pixmap.isNull() || qAbs(curSize.width() - _pixmap_size.width()) > 4 
+          || qAbs(curSize.height() - _pixmap_size.height()) > 4) {
+        _pixmap = QPixmap(curSize);
+        _pixmap_size = curSize;
+      }
+      _pixmap.fill(Qt::transparent);
```

### P1：缓存 signal group 卡片绘制结果

每帧都做 sort + 遍历 + `drawRoundedRect` 是不必要的。Groups 只在 signals 变化时才需要重新计算：

```cpp
// 缓存策略：
// 1. 在 signals_changed() 时预计算 group card rects
// 2. 在 doPaint 中只 drawRoundedRect 预计算好的 rects
// 3. 不需要每帧重新 sort
```

### P2：缓存 Theme 颜色查找

每帧调用 `AppConfig::Instance().GetThemeColor("@trace-divider-color")` 做字符串 map 查找。应该在构造函数或 `UpdateTheme()` 时缓存到成员变量。

### P3：修复 `_check_update_timer` 的启动

```diff
// viewport.cpp 构造函数中
  _check_update_timer.setInterval(80);
  connect(&_check_update_timer, &QTimer::timeout, this, [this]() {
    _view.session().check_update();
  });
+ _check_update_timer.start();
```

### P4：考虑在 resize 期间跳过重绘

窗口拖动/resize 期间，Qt 事件队列中有大量 `WM_SIZE` → `resizeEvent` → `paintEvent` 级联。可以在 `resizeEvent` 中设置节流：

```cpp
void Viewport::resizeEvent(QResizeEvent *e) {
  QWidget::resizeEvent(e);
  // 节流：连续 resize 时最多 16ms 刷新一次
  if (!_resize_throttle_timer.isActive()) {
    _resize_throttle_timer.start(16);
  }
}
```

---

## 五、修复优先级总结

| 优先级 | 修复项 | 预期效果 |
|--------|--------|---------|
| **P0** | LOGIC 模式 QPixmap 智能缓存 | 消除 resize 时每帧 4MB 分配 |
| **P1** | Signal group 卡片预计算 | 减少每帧 O(n²) 计算 |
| **P2** | Theme 颜色缓存 | 消除每帧字符串查找 |
| **P3** | 启动 `_check_update_timer` | 修复功能回归 |
| **P4** | resize 节流 | 减少连续 resize 时的总帧数 |

> [!WARNING]
> **最重要的发现：** 所有之前的 8 个优化都是针对 SlidingDrawer 路径的。但用户抱怨的"拖动窗口卡顿"走的是 `WinNativeWidget` → `ResizeChild` → Qt resize → `doPaint` 路径，与 SlidingDrawer **完全无关**。这就是为什么做了这么多优化还是没有改善。
