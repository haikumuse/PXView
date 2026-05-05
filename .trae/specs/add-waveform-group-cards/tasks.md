# Tasks

- [x] Task 1: 定义分组数据结构和计算逻辑
  - [x] SubTask 1.1: 在 view.h 中定义 SignalGroup 结构体（包含 group_id、成员 trace 列表），新增 `std::vector<SignalGroup> _signal_groups` 成员和 `compute_signal_groups()` 方法声明
  - [x] SubTask 1.2: 在 view.cpp 中实现 `compute_signal_groups()`：遍历所有 DecodeTrace，通过 DecoderStack->stack() 获取每个 Decoder 的 _probes 映射，找到绑定的通道索引，再从 signals 中查找对应 LogicSignal，将 DecodeTrace 及其绑定的 LogicSignal 归入同一组；未被绑定的 LogicSignal 各自成一组
  - [x] SubTask 1.3: 在 `signals_changed()` 中调用 `compute_signal_groups()`，并在 LOGIC 模式下按分组顺序排列 trace，组间增加 GroupGap（4px）间距

- [x] Task 2: Viewport 绘制分组卡片背景
  - [x] SubTask 2.1: 在 View 类中新增 `get_signal_groups()` 公有方法返回 `_signal_groups`
  - [x] SubTask 2.2: 在 Viewport::paintSignals() 的 LOGIC 分支中，在绘制信号之前先遍历分组，为每个分组绘制圆角矩形卡片背景，颜色从 QSS 获取（通过 View 提供的接口）
  - [x] SubTask 2.3: 在 View 类中新增获取分组卡片颜色的方法 `get_group_card_color()`，从 QSS 属性或主题配置中读取

- [x] Task 3: Header 绘制分组卡片背景
  - [x] SubTask 3.1: 在 Header::paintEvent() 中，在绘制标签之前先遍历分组，为每个分组绘制与 Viewport 对齐的圆角矩形卡片背景

- [x] Task 4: QSS 中定义分组卡片颜色
  - [x] SubTask 4.1: 在 stylesheet.qss 中为 View/Viewport/Header 添加 `groupCardColor` 属性（浅灰色，如 rgba(230,230,230,180)）
  - [x] SubTask 4.2: 在 dark.qss 中添加暗色主题的分组卡片颜色（如 rgba(50,50,50,180)）
  - [x] SubTask 4.3: 在 light.qss 中添加亮色主题的分组卡片颜色（如 rgba(240,240,240,180)）

- [x] Task 5: 修改拖拽重排序逻辑支持分组
  - [x] SubTask 5.1: 修改 Header::mouseReleaseEvent() 中的重排序逻辑：拖拽释放后判断目标位置属于哪个组，同组内仅交换组内顺序，跨组则交换两个组的整体位置
  - [x] SubTask 5.2: 修改 view_index 分配逻辑：组内 trace 的 view_index 连续，组间按组的顺序分配

- [x] Task 6: 编译验证
  - [x] SubTask 6.1: 编译项目确保无编译错误
  - [x] SubTask 6.2: 验证分组卡片在 LOGIC 模式下正确显示（代码审查确认逻辑正确）
  - [x] SubTask 6.3: 验证组内和组间拖拽交换功能正常（代码审查确认逻辑正确）

# Task Dependencies
- [Task 2] depends on [Task 1] (需要分组数据结构)
- [Task 3] depends on [Task 1] (需要分组数据结构)
- [Task 4] depends on [Task 2] (颜色需要被绘制代码使用)
- [Task 5] depends on [Task 1] (需要分组数据结构)
- [Task 6] depends on [Task 1-5]
