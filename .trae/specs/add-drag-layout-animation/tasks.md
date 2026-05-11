# Tasks

- [x] Task 1: 在 Trace 中添加动画状态属性
  - [x] 1.1 在 `trace.h` 中新增 `_anim_v_offset`（double）、`_animating`（bool）成员变量
  - [x] 1.2 新增 `get_anim_v_offset()`、`set_anim_v_offset()`、`is_animating()`、`set_animating()` 方法
  - [x] 1.3 新增 `_anim_start_v_offset`、`_anim_target_v_offset` 成员，用于存储动画起止值
  - [x] 1.4 新增 `start_animation(double target_v_offset)` 方法，记录当前 v_offset 为起始值，设置目标值，标记 `_animating = true`
  - [x] 1.5 新增 `stop_animation()` 方法，将 `_anim_v_offset` 同步到 `_v_offset`，标记 `_animating = false`

- [x] Task 2: 在 View 中实现布局动画驱动器
  - [x] 2.1 在 `view.h` 中新增 `QTimer *_layout_anim_timer`、`int _layout_anim_frame`、`static const int LayoutAnimFrames = 10`、`static const int LayoutAnimInterval = 16`
  - [x] 2.2 在 View 构造函数中初始化 timer，连接 timeout 信号到 `on_layout_anim_tick()` 槽
  - [x] 2.3 实现 `start_layout_animation()`：遍历所有 trace，对每个 trace 调用 `start_animation(target_v_offset)`，启动 timer
  - [x] 2.4 实现 `on_layout_anim_tick()`：递增帧计数，计算线性插值进度 `t = frame / LayoutAnimFrames`，对每个 animating 的 trace 更新 `_anim_v_offset`，触发 Header 和 Viewport 重绘，帧数达到后停止 timer 并调用 `finalize_layout_animation_internal()`
  - [x] 2.5 实现 `finalize_layout_animation_internal()`：对所有 trace 调用 `stop_animation()`，将 `_anim_v_offset` 写回 `_v_offset`，调用 `normalize_layout()`，恢复 Pixmap 缓存

- [x] Task 3: 修改 Header::mouseReleaseEvent 启动动画
  - [x] 3.1 在拖拽释放逻辑中（`_moveFlag == true` 分支），先记录所有 trace 的旧 v_offset，再调用 `signals_changed()` 计算新布局
  - [x] 3.2 对比新旧 v_offset，将有变化的 trace 标记为需要动画
  - [x] 3.3 调用 `start_layout_animation()` 启动动画，替代当前的瞬间重绘

- [x] Task 4: 修改 Header 和 Viewport 的 paintEvent 读取动画中间值
  - [x] 4.1 在 `Header::paintEvent` 中，绘制 trace label 时使用动画中间值
  - [x] 4.2 在 `Viewport::doPaint` 中，绘制分组卡片背景、trace 的 paint_back/paint_mid/paint_fore 时，同样使用动画中间值
  - [x] 4.3 在 `Viewport::doPaint` 中，动画期间强制设置 `_need_update = true` 禁用 QPixmap 缓存

- [x] Task 5: 处理动画期间的用户交互
  - [x] 5.1 在 `Header::mousePressEvent` 中，如果动画正在播放，立即停止动画
  - [x] 5.2 在 `View::start_layout_animation()` 中，如果已有动画在播放，先停止旧动画再启动新动画

- [x] Task 6: 为通道增删场景接入动画
  - [x] 6.1 在 `View::signals_changed()` 末尾，检测是否需要启动布局动画（非拖拽场景下，如解码通道添加/删除、通道 enable/disable 变化）
  - [x] 6.2 新增 `bool _skip_anim_on_signals_changed` 标志，在拖拽释放流程中设为 true（因为 Task 3 已单独处理），避免重复启动动画

# Task Dependencies
- Task 2 depends on Task 1
- Task 3 depends on Task 2
- Task 4 depends on Task 1
- Task 5 depends on Task 2, Task 3
- Task 6 depends on Task 2, Task 3
