# Tasks

- [x] Task 1: 创建 EdgeNavButton 浮动按钮组件
  - [x] SubTask 1.1: 创建 `PXView/pv/view/edge_nav_button.h`，定义 EdgeNavButton 类（QWidget 子类），包含方向属性（Previous/Next）、点击信号、启用/禁用状态、悬停高亮
  - [x] SubTask 1.2: 创建 `PXView/pv/view/edge_nav_button.cpp`，实现按钮绘制（圆角矩形背景 + 方向箭头图标）、鼠标事件处理、主题颜色适配
  - [x] SubTask 1.3: 在 CMakeLists.txt 中添加新源文件

- [x] Task 2: 在 Viewport 中集成边沿导航按钮
  - [x] SubTask 2.1: 在 viewport.h 中添加 EdgeNavButton 指针成员（_prev_edge_btn, _next_edge_btn）、当前悬停信号指针（_hover_logic_signal）、按钮可见性控制方法
  - [x] SubTask 2.2: 在 Viewport 构造函数中创建两个 EdgeNavButton 作为 Viewport 的子控件，初始隐藏
  - [x] SubTask 2.3: 在 mouseMoveEvent 中检测鼠标悬停的逻辑信号行，更新按钮位置和启用/禁用状态
  - [x] SubTask 2.4: 在 leaveEvent 中隐藏按钮
  - [x] SubTask 2.5: 连接按钮点击信号到跳转处理槽函数

- [x] Task 3: 实现边沿导航跳转逻辑
  - [x] SubTask 3.1: 在 Viewport 中实现 `navigate_to_edge(Direction dir)` 方法：获取悬停信号的 LogicSnapshot，从鼠标位置调用 get_nxt_edge/get_pre_edge，计算目标偏移量，调用 View::set_scale_offset 滚动视口
  - [x] SubTask 3.2: 跳转时将搜索光标移动到目标边沿位置（调用 View::set_search_pos 或直接设置搜索光标索引）
  - [x] SubTask 3.3: "下一个边沿"将目标放在视口左侧 25% 位置，"上一个边沿"将目标放在视口右侧 25% 位置

- [x] Task 4: 实现按钮启用/禁用状态判断
  - [x] SubTask 4.1: 在鼠标移动时，从当前鼠标位置对应的采样索引分别调用 get_nxt_edge 和 get_pre_edge，判断是否存在更远的边沿
  - [x] SubTask 4.2: 根据判断结果设置按钮的 enabled 状态

- [x] Task 5: 添加快捷键支持
  - [x] SubTask 5.1: 在 Viewport::keyPressEvent 中处理 Alt+Left / Alt+Right 快捷键
  - [x] SubTask 5.2: 快捷键仅在鼠标悬停在逻辑信号行上且会话停止状态时生效

- [x] Task 6: 主题和样式适配
  - [x] SubTask 6.1: EdgeNavButton 的 UpdateTheme 方法跟随主题颜色变化
  - [x] SubTask 6.2: 确保 Viewport 的 UpdateTheme 调用时更新按钮样式

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 2]
- [Task 4] depends on [Task 2]
- [Task 5] depends on [Task 3]
- [Task 6] depends on [Task 1]
