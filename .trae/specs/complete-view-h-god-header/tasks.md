# Tasks

- [x] Task 1: 压缩 view.cpp 的 Phase K forwarder 块，使总行数 < 800
  - [x] SubTask 1.1: 移除装饰性注释分隔符（`// ====` 包围块），保留一行说明性注释
  - [x] SubTask 1.2: 折叠 `add_decoder` 多行签名到 1 行（签名行 + return 行 + `}`）
  - [x] SubTask 1.3: 移除 forwarder 之间的多余空行（保留块前一行空行作为分隔）
  - [x] SubTask 1.4: 验证 view.cpp 总行数 ≤ 799 —— 实际 796

- [x] Task 2: 评估 K3 进一步降 includes 的可行性（保守，不破坏编译）
  - [x] SubTask 2.1: 分析 `view_cursors.h` 是否能改为前向声明 —— 理论可（ViewCursors 仅 unique_ptr 成员），但保守不动
  - [x] SubTask 2.2: 分析 `view_glitch_filter.h` 是否能改前向声明 —— 不能（GlitchFilterMode 枚举用作 slot 参数 + FilterSnapshot vector 成员，需完整类型）
  - [x] SubTask 2.3: 分析 `dock_ui_state.h`、`pulse_analyzer.h`、`uimanager.h` —— 不能（值成员/基类/std::vector 完整类型）
  - [x] SubTask 2.4: 记录 K3 最终决策 —— 保留 12 个

- [x] Task 3: 评估 K4 SignalGroup 抽离决策
  - [x] SubTask 3.1: 确认 SignalGroup 使用面 —— 仅 view.h + view_signal_sync.cpp 2 处
  - [x] SubTask 3.2: 记录 K4 决策 —— 保留内联

- [x] Task 4: 生成最终验证报告
  - [x] SubTask 4.1: 统计 view.h 行数 / include 数 / 访问修饰符数 —— 801 / 12 / 5
  - [x] SubTask 4.2: 统计 view.cpp 行数 —— 796
  - [x] SubTask 4.3: 统计下沉 forwarder 数量 —— 39
  - [x] SubTask 4.4: 输出 7 项指标报告给用户

# Task Dependencies

- Task 1 → Task 4（必须先压缩再统计 view.cpp 行数）
- Task 2、Task 3 可与 Task 1 并行（只读分析，不改文件）
