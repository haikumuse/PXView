# Tasks

- [x] Task 1: Core 脉冲分析工具类 PulseAnalyzer
  - [x] SubTask 1.1: 新建 `PXView/pv/data/pulse_analyzer.h`,声明 `Pulse`/`Histogram` struct 和 `find_pulses`/`build_histogram`/`recommend_threshold`/`preview_filter` 静态方法
  - [x] SubTask 1.2: 新建 `PXView/pv/data/pulse_analyzer.cpp`,实现 `find_pulses`(复用 `LogicSnapshot::get_nxt_edge` 边沿扫描)
  - [x] SubTask 1.3: 实现 `build_histogram`(仅统计 width<=max_width 的短脉冲)
  - [x] SubTask 1.4: 实现 `recommend_threshold`(最大间隙启发式 + 30% 分位数兜底)
  - [x] SubTask 1.5: 实现 `preview_filter`(纯函数,filter_mode 语义对齐 `apply_glitch_filter` 第 2337-2349 行)
  - [x] SubTask 1.6: 在 CMakeLists.txt 的 `PXVIEW_CORE_SOURCES` 中注册 pulse_analyzer.h/cpp
  - [x] SubTask 1.7: 编译验证 `cd build && ninja -j 16` 通过

- [x] Task 2: LogicSnapshot 滤波区间持久化
  - [x] SubTask 2.1: 在 `logicsnapshot.h` 将 `FillRange` struct 从 cpp 局部提升为 public 嵌套类型(`pv::data::LogicSnapshot::FillRange`)
  - [x] SubTask 2.2: 新增私有成员 `std::map<int, std::vector<FillRange>> _filtered_ranges_per_channel;`
  - [x] SubTask 2.3: 新增 public 方法 `const std::vector<FillRange>& get_filtered_ranges(int sig_index) const;`
  - [x] SubTask 2.4: 修改 `apply_glitch_filter` 实现:每次 push 到局部 fills 时同步 push 到 `_filtered_ranges_per_channel[sig_index]`(注意:批量刷盘清空局部 fills 不影响持久化)
  - [x] SubTask 2.5: 在 FilterProcessor `clear_glitch_filter` 和 backup 恢复路径中清空 `_filtered_ranges_per_channel`(通过新增 `LogicSnapshot::clear_filtered_ranges()` 方法或直接在 `clear_glitch_filter` 调用后 clear)
  - [x] SubTask 2.6: 编译验证通过

- [x] Task 3: FilterProcessor 进度透传修复
  - [x] SubTask 3.1: 修改 `filterprocessor.cpp:108-114` 进度回调,将 progress 值透传到 `trigger_message(DSV_MSG_GLITCH_FILTER_PROGRESS, progress)`
  - [x] SubTask 3.2: 若 MainWindow OnMessage 翻译为 typed event,确保 `GlitchFilterProgress.progress` 字段填入 param 值
  - [x] SubTask 3.3: 编译验证通过

- [x] Task 4: Header 右键菜单入口
  - [x] SubTask 4.1: 在 `header.h` 新增信号 `void show_glitch_filter_popup(pv::view::LogicSignal* sig);` 和 `void clear_glitch_filter_requested(bool all_channels);`
  - [x] SubTask 4.2: 在 `header.h` 添加 `QMenu* create_filter_submenu(bool all_channels);` 私有方法声明
  - [x] SubTask 4.3: 在 `header.cpp` 的 `contextMenuEvent` 中,检测 `_context_trace` 是否为 `LogicSignal*` 且其 `data()` 非空
  - [x] SubTask 4.4: 若是 LogicSignal,在现有行高菜单前插入"🔍 滤除毛刺..."(F)、"↔ 信号取反"(I)、"✕ 清除此通道滤波"、"✕ 清除所有通道滤波"菜单项 + 分隔符
  - [x] SubTask 4.5: 菜单项 triggered → emit `show_glitch_filter_popup` 或 `clear_glitch_filter_requested`
  - [x] SubTask 4.6: 在 `header.cpp` 添加 F/I 快捷键处理(在 keyPressEvent 中,当 popup 未打开时)
  - [x] SubTask 4.7: 添加翻译资源键到 `lang/cn/signal_proc.json`、`lang/en/signal_proc.json`、`lang/traditional/signal_proc.json`
  - [x] SubTask 4.8: 编译验证通过

- [x] Task 5: PulseHistogramWidget 直方图控件
  - [x] SubTask 5.1: 新建 `PXView/pv/view/pulsehistogramwidget.h`,声明 `pv::view::PulseHistogramWidget : public QWidget`,Q_OBJECT
  - [x] SubTask 5.2: 新建 `PXView/pv/view/pulsehistogramwidget.cpp`,实现 `setData(Histogram)`、`setThresholds(recommended, current)`、`setFilterThreshold(threshold)` slots
  - [x] SubTask 5.3: 实现 `paintEvent`:绘制背景 + 柱子(灰/红/橙三色)+ 推荐阈值橙虚线(带"推荐"标签)+ 当前阈值蓝实线(带"当前"标签)+ x 轴宽度刻度
  - [x] SubTask 5.4: 应用 QSS 暗色主题(背景 #1a1d24、边框 #2a2f38、圆角 4px)
  - [x] SubTask 5.5: 在 CMakeLists.txt 的 `PXVIEW_GUI_SOURCES` 注册
  - [x] SubTask 5.6: 编译验证通过

- [x] Task 6: GlitchFilterPopup 浮窗控件
  - [x] SubTask 6.1: 新建 `PXView/pv/view/glitchfilterpopup.h`,声明 `pv::view::GlitchFilterPopup : public QWidget`,Q_OBJECT,window flags `Qt::Popup | Qt::FramelessWindowHint`
  - [x] SubTask 6.2: 声明信号 `preview_changed(LogicSignal*, uint32_t, GlitchFilterMode)`、`apply_requested(LogicSignal*, uint32_t, GlitchFilterMode, bool all)`、`cleared(LogicSignal*, bool all)`、`closed()`
  - [x] SubTask 6.3: 声明 `open_for_signal(LogicSignal* sig, const QPoint& anchor_pos, View& view)` 方法
  - [x] SubTask 6.4: 新建 `glitchfilterpopup.cpp`,实现布局:标题栏 + 直方图区 + 统计行 + 类型 ComboBox + 阈值 Slider+QLabel + 底部按钮行 + 顶部预设下拉
  - [x] SubTask 6.5: 实现 `open_for_signal`:调用 `PulseAnalyzer::find_pulses` + `build_histogram` + `recommend_threshold`,填充直方图与控件默认值,移动浮窗到锚点位置
  - [x] SubTask 6.6: 实现 slider `sliderMoved` → emit `preview_changed` + 更新直方图着色 + 更新统计行
  - [x] SubTask 6.7: 实现 ComboBox `currentIndexChanged` → 同上
  - [x] SubTask 6.8: 实现"应用到本通道"按钮 → emit `apply_requested(all=false)` + close
  - [x] SubTask 6.9: 实现"应用到所有逻辑通道"按钮 → emit `apply_requested(all=true)` + close
  - [x] SubTask 6.10: 实现"取消"按钮 + Esc 键 + focusOutEvent → emit `closed()` + hide
  - [x] SubTask 6.11: 实现预设下拉 `currentIndexChanged` → 设置 slider/mode(不自动 Apply)
  - [x] SubTask 6.12: 应用 QSS 暗色主题(背景 #252932、边框 #3a3f4b、圆角 8px)+ QGraphicsDropShadowEffect 阴影
  - [x] SubTask 6.13: 在 CMakeLists.txt 的 `PXVIEW_GUI_SOURCES` 注册
  - [x] SubTask 6.14: 编译验证通过

- [x] Task 7: View 层 popup 持有与预览缓存
  - [x] SubTask 7.1: 在 `view.h` 新增成员 `GlitchFilterPopup* _glitch_filter_popup = nullptr;` 和 `std::map<LogicSignal*, std::vector<pv::data::PulseAnalyzer::Pulse>> _preview_ranges;`
  - [x] SubTask 7.2: 在 `view.h` 新增 slot `void on_show_glitch_filter_popup(pv::view::LogicSignal* sig);`
  - [x] SubTask 7.3: 在 `view.h` 新增 slot `void on_preview_changed(LogicSignal*, uint32_t, GlitchFilterMode);`
  - [x] SubTask 7.4: 在 `view.h` 新增 slot `void on_apply_requested(LogicSignal*, uint32_t, GlitchFilterMode, bool all);`
  - [x] SubTask 7.5: 在 `view.h` 新增 slot `void on_popup_closed();`
  - [x] SubTask 7.6: 在 `view.cpp` 构造函数中创建 `_glitch_filter_popup = new GlitchFilterPopup(*this, this);` 并连接信号
  - [x] SubTask 7.7: 实现 `on_show_glitch_filter_popup`:计算锚点(通道标签 rect 右侧 8px,mapToGlobal)→ `_glitch_filter_popup->open_for_signal(sig, anchor, *this)`
  - [x] SubTask 7.8: 实现 `on_preview_changed`:缓存 pulses → `_preview_ranges[sig] = PulseAnalyzer::preview_filter(cached_pulses, threshold, mode)` → `_viewport->update()`
  - [x] SubTask 7.9: 实现 `on_apply_requested`:push 撤销栈 → 组装 thresholds/modes(单通道或全通道)→ `_session->set_glitch_filter(...)` → 关闭 popup
  - [x] SubTask 7.10: 实现 `on_popup_closed`:`_preview_ranges.clear()` → `_viewport->update()`
  - [x] SubTask 7.11: 在 View 析构中 delete `_glitch_filter_popup`
  - [x] SubTask 7.12: 连接 Header 的 `show_glitch_filter_popup` 信号到 View 的 `on_show_glitch_filter_popup` slot(在 `view.cpp:265-268` 现有 connect 块附近)
  - [x] SubTask 7.13: 编译验证通过

- [x] Task 8: LogicSignal overlay 渲染
  - [x] SubTask 8.1: 在 `logicsignal.h` 新增 `friend class pv::view::View;` 或提供访问器让 View 设置预览状态(或直接由 View 通过 `_preview_ranges` map 查询)
  - [x] SubTask 8.2: 在 `logicsignal.cpp` 的 `paint_mid_align` 中,`p.drawLines(wave_lines.data(), wave_lines.size())` 之后(line 227 附近)插入 overlay 渲染
  - [x] SubTask 8.3: 渲染已滤区间(红色):查询 `_data->is_glitch_filtered()` + `get_filtered_ranges(model->index())`,裁剪到可见 x 范围,绘制 `QColor(255, 82, 82, 90)` 矩形
  - [x] SubTask 8.4: 渲染预览区间(橙色):查询 `_view->_preview_ranges[this]`(需 friend 或 View 提供 public accessor `get_preview_ranges(LogicSignal*)`),绘制 `QColor(255, 183, 77, 70)` 矩形
  - [x] SubTask 8.5: 矩形数量超过 1000 时合并相邻矩形(可选优化,首版可跳过)
  - [x] SubTask 8.6: 编译验证通过

- [x] Task 9: View 层撤销栈
  - [x] SubTask 9.1: 在 `view.h` 新增 struct `FilterSnapshot { std::vector<uint32_t> thresholds; std::vector<GlitchFilterMode> modes; bool was_active; };`
  - [x] SubTask 9.2: 新增成员 `std::vector<FilterSnapshot> _filter_undo_stack;`
  - [x] SubTask 9.3: 在 `on_apply_requested` 调用 `set_glitch_filter` 前 push 当前状态
  - [x] SubTask 9.4: 新增 `void undo_filter()` public 方法:pop 栈顶,根据 was_active 调用 `set_glitch_filter` 或 `clear_glitch_filter`
  - [x] SubTask 9.5: 在 MainWindow 中连接 Ctrl+Z 快捷键到当前 View 的 `undo_filter()`(检查不与现有 undo 冲突)
  - [x] SubTask 9.6: 编译验证通过

- [x] Task 10: Dock 同步刷新
  - [x] SubTask 10.1: 在 `GlitchFilterPopup` 注册为 `IEventListener` 或在 MainWindow OnMessage 中转发 `DSV_MSG_GLITCH_FILTER_COMPLETED/CLEARED` 到 popup
  - [x] SubTask 10.2: popup 收到消息后若 `_target_sig` 仍有效,重算直方图 + 刷新滑块默认值
  - [x] SubTask 10.3: 编译验证通过

- [x] Task 11: Toast 提示
  - [x] SubTask 11.1: 在 View 或 MainWindow 中实现 `show_toast(QString msg, QString type)` 方法(若项目已有 toast 基础设施则复用) — **复用现有 `pv::ui::Toast::show(parent, msg, level)`**
  - [x] SubTask 11.2: 在 Apply/Apply-All/Clear/Undo 操作后调用 `show_toast`(在 Task 7/9 View 集成中调用)
  - [x] SubTask 11.3: 编译验证通过

- [ ] Task 12: 集成测试与构建验证
  - [x] SubTask 12.1: 完整构建 `cd build && ninja -j 16 && ninja install` 通过
  - [ ] SubTask 12.2: 启动 PXView.exe,加载一个有毛刺的 .pxc 文件,右键 D0 通道标签,验证弹出浮窗
  - [ ] SubTask 12.3: 拖动滑块,验证波形上橙色预览 overlay 实时变化
  - [ ] SubTask 12.4: 点击"应用到本通道",验证红色已滤 overlay 出现,popup 关闭
  - [ ] SubTask 12.5: 点击"应用到所有逻辑通道",验证所有 LogicSignal 出现红色 overlay
  - [ ] SubTask 12.6: Ctrl+Z 撤销,验证 overlay 消失
  - [ ] SubTask 12.7: 打开 SignalProcessingDock,验证 dock 的勾选状态与 popup 一致
  - [ ] SubTask 12.8: dock 中 Apply,验证 popup(若已打开)直方图刷新
  - [ ] SubTask 12.9: 选择预设,验证滑块/模式更新
  - [ ] SubTask 12.10: Esc 关闭浮窗,验证预览 overlay 清除

# Task Dependencies
- Task 2, 3 依赖 Task 1(共用 LogicSnapshot/PulseAnalyzer 概念,但可并行)
- Task 4 独立(Header 改造,不依赖 Core)
- Task 5 独立(直方图 widget,可先做)
- Task 6 依赖 Task 1(用 PulseAnalyzer)和 Task 5(用 PulseHistogramWidget)
- Task 7 依赖 Task 4(连接 Header 信号)、Task 6(持有 popup)
- Task 8 依赖 Task 1(用 preview_filter 结果)、Task 7(读 _preview_ranges)
- Task 9 依赖 Task 7(在 on_apply_requested 中压栈)
- Task 10 依赖 Task 6
- Task 11 独立
- Task 12 依赖所有前序任务

# Parallelizable Work
- Task 1, 5, 11 可完全并行(无依赖)
- Task 2, 3 可并行(改不同文件)
- Task 4 可与 Task 1-3 并行(纯 Header 改造)
