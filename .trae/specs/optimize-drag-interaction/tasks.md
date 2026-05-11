# Tasks

- [ ] Task 1: 在 Trace 类中添加动画支持属性
  - [ ] 1.1 在 trace.h 中添加 `_anim_v_offset`（double）、`_anim_target_v_offset`（int）、`_anim_start_v_offset`（int）、`_is_animating`（bool）成员变量
  - [ ] 1.2 添加对应的 getter/setter 方法
  - [ ] 1.3 添加 `get_effective_v_offset()` 方法：动画中返回 `_anim_v_offset`，否则返回 `_v_offset`

- [ ] Task 2: 在 View 类中添加布局动画引擎
  - [ ] 2.1 在 view.h 中添加 QTimer* `_layout_anim_timer`、`_anim_frame`、`_anim_total_frames`（=8，约 120ms@60fps）、`_anim_traces`（记录参与动画的 trace 及其 start/target offset）
  - [ ] 2.2 实现 `start_layout_animation()`：记录每个 trace 的当前 v_offset 作为 start，目标 v_offset 作为 target，启动 QTimer（interval=16ms）
  - [ ] 2.3 实现 `layout_animation_step()`：每帧计算 ease_out_cubic 插值进度，更新每个参与动画的 trace 的 `_anim_v_offset`，触发 header_updated() + viewport_update()，动画结束后清理状态
  - [ ] 2.4 实现 `stop_layout_animation()`：立即停止动画，将所有 trace 的 v_offset 设为目标值
  - [ ] 2.5 实现 ease_out_cubic 缓动函数

- [ ] Task 3: 修改 Header 拖拽逻辑，添加占位指示线
  - [ ] 3.1 在 header.h 中添加 `_drag_target_y`（int）成员，记录当前拖拽目标位置的 Y 坐标
  - [ ] 3.2 修改 mouseMoveEvent：拖拽 Logic 通道时，计算目标插入位置并更新 `_drag_target_y`，被拖拽的 trace 仍然跟随鼠标移动，但其他 trace 不再被推动
  - [ ] 3.3 在 paintEvent 中：当 `_drag_target_y >= 0` 时，在该位置绘制一条水平高亮指示线（2px 高，使用主题 accent 颜色，半透明）
  - [ ] 3.4 修改 mouseReleaseEvent：释放时调用 signals_changed() 计算最终布局，然后调用 start_layout_animation() 启动过渡动画，而非直接 normalize_layout()

- [ ] Task 4: 修改 Header 和 Viewport 的绘制，支持动画中间值和拖拽透明度
  - [ ] 4.1 修改 Header::paintEvent 中 trace 的 v_offset 读取：使用 `get_effective_v_offset()` 替代 `get_v_offset()`
  - [ ] 4.2 修改 Viewport::doPaint 中 trace 的 v_offset 读取：使用 `get_effective_v_offset()` 替代 `get_v_offset()`
  - [ ] 4.3 在 Header::paintEvent 中：拖拽中的 trace 设置 painter opacity 为 0.6
  - [ ] 4.4 在 Viewport::doPaint 中：拖拽中的 trace 设置 painter opacity 为 0.6
  - [ ] 4.5 动画期间禁用 Viewport 的 QPixmap 缓存（设置 `_need_update = true`）

- [ ] Task 5: 处理动画期间的交互冲突
  - [ ] 5.1 在 Header::mousePressEvent 中：如果动画正在播放，先调用 stop_layout_animation() 停止动画
  - [ ] 5.2 在 View::wheelEvent 中：如果动画正在播放，先停止动画
  - [ ] 5.3 确保动画期间 Viewport 的实时采集数据刷新不受影响

- [ ] Task 6: 集成测试与视觉调优
  - [ ] 6.1 编译并运行，测试 Logic 模式下通道拖拽重排的占位线和过渡动画
  - [ ] 6.2 测试 DSO/Analog 模式下拖拽行为不变
  - [ ] 6.3 测试动画期间再次拖拽的冲突处理
  - [ ] 6.4 调优动画时长和缓动曲线参数

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 1]
- [Task 4] depends on [Task 1, Task 2]
- [Task 5] depends on [Task 2, Task 4]
- [Task 6] depends on [Task 3, Task 4, Task 5]
