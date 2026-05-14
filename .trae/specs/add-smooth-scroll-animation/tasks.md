# Tasks

- [x] Task 1: 创建 SmoothScrollBar 自定义控件
  - [x] SubTask 1.1: 创建 `PXView/pv/widgets/smoothscrollbar.h`，定义 SmoothScrollBar 类（继承 QScrollBar），声明 Q_PROPERTY、构造函数、关键方法和成员变量
  - [x] SubTask 1.2: 创建 `PXView/pv/widgets/smoothscrollbar.cpp`，实现核心逻辑：
    - `smoothSetValue(int value)`: 启动 QPropertyAnimation（OutCubic, 300ms）从当前值过渡到目标值
    - `immediateSetValue(int value)`: 直接设置值，不触发动画
    - 重写 `sliderChange()`: 检测滑块拖拽时跳过动画
    - 重写 `wheelEvent()`: 拦截滚轮事件，计算目标值后调用 smoothSetValue，并实现连续滚轮加速逻辑（100ms 窗口内 count>3 步幅加倍，count>6 再加倍）
    - 连续滚轮事件合并：新事件到来时停止当前动画，从当前值重新计算目标
  - [x] SubTask 1.3: 在 `CMakeLists.txt` 中添加新文件

- [x] Task 2: 创建 SmoothScrollArea 自定义控件
  - [x] SubTask 2.1: 创建 `PXView/pv/widgets/smoothscrollarea.h`，定义 SmoothScrollArea 类（继承 QScrollArea）
  - [x] SubTask 2.2: 创建 `PXView/pv/widgets/smoothscrollarea.cpp`，实现核心逻辑：
    - 重写 `wheelEvent()`: 拦截滚轮事件，计算目标滚动位置，使用 QPropertyAnimation（OutCubic, 250ms）平滑过渡 verticalScrollBar()->value()
    - 连续滚轮事件合并：新事件到来时停止当前动画，从当前位置重新计算目标
  - [x] SubTask 2.3: 在 `CMakeLists.txt` 中添加新文件

- [x] Task 3: 修改 View 类使用 SmoothScrollBar
  - [x] SubTask 3.1: 在 `view.h` 中添加 `#include "widgets/smoothscrollbar.h"`，添加 SmoothScrollBar 成员指针
  - [x] SubTask 3.2: 在 `view.cpp` 构造函数中创建 SmoothScrollBar 实例并安装到 QScrollArea（通过 `setHorizontalScrollBar` / `setVerticalScrollBar`）
  - [x] SubTask 3.3: 修改 `update_scroll()` 方法：在 `_updating_scroll = true` 块内调用 `immediateSetValue()` 而非 `setSliderPosition()`
  - [x] SubTask 3.4: `h_scroll_value_changed()` 和 `v_scroll_value_changed()` 通过 SmoothScrollBar 的 valueChanged 信号自动驱动，动画期间持续更新
  - [x] SubTask 3.5: 保留 View 的 `margin-top: RulerHeight` 样式，SmoothScrollBar 继承 QScrollBar 所以 QSS 样式仍然生效
  - [x] SubTask 3.6: 添加 `stopScrollAnimations()` 方法，在 `set_scale_offset()` 中调用以停止动画

- [x] Task 4: 修改所有 Dock 类基类为 SmoothScrollArea
  - [x] SubTask 4.1: DeviceOptionsDock — 将基类从 QScrollArea 改为 SmoothScrollArea，添加头文件，修改构造函数初始化列表
  - [x] SubTask 4.2: TriggerDock — 同上
  - [x] SubTask 4.3: DsoTriggerDock — 同上
  - [x] SubTask 4.4: MeasureDock — 同上
  - [x] SubTask 4.5: SearchDock — 同上
  - [x] SubTask 4.6: ProtocolDock — 同上
  - [x] SubTask 4.7: SearchComboBox — 将内部 `_scroll` (QScrollArea*) 替换为 SmoothScrollArea

- [x] Task 5: 为 SearchDock 和 ProtocolDock 的 QTableView 添加 QScroller 惯性滚动
  - [x] SubTask 5.1: 在 `searchdock.cpp` 中为 `_result_view` 配置 QScroller：LeftMouseButtonGesture，Overshoot 距离为 0，DecelerationFactor 为 0.5
  - [x] SubTask 5.2: 在 `protocoldock.cpp` 中为 `_table_view` 配置同样的 QScroller
  - [x] SubTask 5.3: 通过设置 DragStartDistance 为 0.02 确保 QScroller 不干扰 QTableView 的单元格点击和选择操作

- [x] Task 6: 编译验证
  - [x] SubTask 6.1: 编译项目，确保无编译错误（ninja build 通过）

# Task Dependencies
- [Task 3] depends on [Task 1] (View 需要 SmoothScrollBar)
- [Task 4] depends on [Task 2] (Dock 需要 SmoothScrollArea)
- [Task 5] 独立于 Task 1-4，可并行执行
- [Task 6] depends on [Task 1, 2, 3, 4, 5]
