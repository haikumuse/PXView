# Checklist

## A. Core 基础设施

- [x] `PXView/pv/data/pulse_analyzer.h` 存在并声明 `Pulse`/`Histogram` struct 和 4 个静态方法
- [x] `PulseAnalyzer::find_pulses` 复用 `LogicSnapshot::get_nxt_edge` 而非重新实现字节扫描
- [x] `PulseAnalyzer::preview_filter` 是纯函数(不修改 snapshot 状态、不调 Core)
- [x] `PulseAnalyzer::preview_filter` 的 filter_mode 语义与 `apply_glitch_filter` line 2337-2349 一致
- [x] `recommend_threshold` 在无明显间隙时返回 30% 分位数 + 1,无脉冲时返回 3
- [x] `pulse_analyzer.h/cpp` 在 CMakeLists.txt 的 `PXVIEW_CORE_SOURCES` 中注册
- [x] Core 层代码不依赖 QWidget/QMainWindow(符合 AGENTS.md 分层约束)

## B. LogicSnapshot 滤波区间持久化

- [x] `FillRange` struct 提升为 `LogicSnapshot` 的 public 嵌套类型
- [x] `_filtered_ranges_per_channel` 成员声明为 private
- [x] `get_filtered_ranges(sig_index)` 返回 `const std::vector<FillRange>&`(空时返回空 vector 引用而非抛异常)
- [x] `apply_glitch_filter` 内 push 到局部 fills 时同步 push 到 `_filtered_ranges_per_channel[sig_index]`
- [x] 批量刷盘(`apply_batch` 清空局部 fills)不清空持久化存储
- [x] `clear_glitch_filter` / backup 恢复路径清空 `_filtered_ranges_per_channel`
- [x] 持久化存储操作在 `_mutex` 锁内(线程安全)

## C. FilterProcessor 进度透传

- [x] `trigger_message(DSV_MSG_GLITCH_FILTER_PROGRESS, progress)` 携带真实 progress 值
- [x] typed event `GlitchFilterProgress.progress` 字段填入真实进度(若 OnMessage 翻译表覆盖此消息)
- [x] 无 `(void)progress;` 残留

## D. Header 右键菜单

- [x] LOGIC 模式下右键 LogicSignal 的 LABEL 区域显示滤波菜单项
- [x] 非 LogicSignal 目标(DecodeTrace 等)不显示滤波菜单项
- [x] 菜单项顺序:滤波组 → 分隔符 → 行高组(原菜单保留)
- [x] "🔍 滤除毛刺..." triggered → emit `show_glitch_filter_popup(LogicSignal*)`
- [x] "✕ 清除此通道滤波"仅当该通道已滤波时启用
- [x] "✕ 清除所有通道滤波"仅当任一通道已滤波时启用
- [x] F/I 快捷键在 popup 未打开时触发(不与 popup 内快捷键冲突)
- [x] 翻译资源键已添加到 cn/en/traditional signal_proc.json

## E. PulseHistogramWidget 直方图

- [x] `setData(Histogram)` 触发 `update()`
- [x] paintEvent 绘制 max_width 个柱子,高度按 count/max_count 比例
- [x] 推荐阈值橙色虚线 + "推荐"标签
- [x] 当前阈值蓝色实线 + "当前"标签
- [x] 宽度 <= threshold 的柱子变红 #ff5252
- [x] 宽度 == recommended 的柱子变橙 #ffb74d
- [x] QSS 暗色主题与项目一致

## F. GlitchFilterPopup 浮窗

- [x] Window flags 为 `Qt::Popup | Qt::FramelessWindowHint`
- [x] 宽度 420px,布局匹配 HTML 原型(标题/直方图/统计/类型/阈值/按钮)
- [x] `open_for_signal` 调用 PulseAnalyzer 计算 histogram + recommended_threshold
- [x] 已有滤波配置时滑块/ComboBox 用现有值,否则用推荐值
- [x] 拖滑块仅 emit `preview_changed`,不调 Core
- [x] 切换 ComboBox 仅 emit `preview_changed`,不调 Core
- [x] 直方图柱子着色随滑块实时更新
- [x] 统计行数字随滑块实时更新
- [x] "应用到本通道" emit `apply_requested(all=false)` 并关闭
- [x] "应用到所有逻辑通道" emit `apply_requested(all=true)` 并关闭
- [x] "取消"按钮 / Esc 键 / 外部点击 → emit `closed()` 并 hide
- [x] 预设下拉包含 3 个内置预设,选择后更新滑块/模式(不自动 Apply)
- [x] QGraphicsDropShadowEffect 阴影生效
- [x] QSS 圆角 8px 生效

## G. View 层 popup 持有与预览缓存

- [x] `_glitch_filter_popup` 成员声明
- [x] `_preview_ranges` map 成员声明
- [x] View 构造函数创建 popup 并连接 4 个信号(preview_changed/apply_requested/closed + Header show_glitch_filter_popup)
- [x] `on_show_glitch_filter_popup` 计算锚点(标签 rect 右侧 8px,mapToGlobal)
- [x] `on_preview_changed` 更新 `_preview_ranges` 并触发 `_viewport->update()`
- [x] `on_apply_requested` push 撤销栈 → 组装数组 → 调 `_session->set_glitch_filter` → 关闭 popup
- [x] `on_popup_closed` 清 `_preview_ranges` 并触发重绘
- [x] View 析构 delete popup
- [x] Header 的 `show_glitch_filter_popup` 信号已连接到 View slot

## H. LogicSignal overlay 渲染

- [x] `paint_mid_align` 在 `drawLines()` 后插入 overlay 渲染
- [x] 已滤区间用 `QColor(255, 82, 82, 90)` 半透明红色
- [x] 预览区间用 `QColor(255, 183, 77, 70)` 半透明橙色
- [x] 已滤 overlay 与预览 overlay 互斥(已滤时不显示预览)
- [x] 矩形 x 坐标按 `(start - offset) / samples_per_pixel` 计算
- [x] 矩形 y 范围为 signal 的 high_offset 到 low_offset
- [x] 仅绘制可见 x 范围内矩形(裁剪优化)

## I. View 层撤销栈

- [x] `FilterSnapshot` struct 定义含 thresholds/modes/was_active
- [x] `_filter_undo_stack` 成员声明,最大深度 20
- [x] `on_apply_requested` 调 set_glitch_filter 前 push 快照
- [x] `undo_filter()` pop 栈顶并根据 was_active 调 set/clear
- [x] Ctrl+Z 在 View 焦点下触发 `undo_filter()`
- [x] 不与现有 undo 操作冲突

## J. Dock 同步

- [x] popup Apply 后 dock 的 `update_glitch_filter_state()` 被触发(经 DSV_MSG_GLITCH_FILTER_COMPLETED → MainWindow OnMessage)
- [x] dock Apply 后若 popup 已打开,popup 直方图刷新
- [x] 两者状态一致(共享 Core `_glitch_filter_thresholds` / `_modes`)

## K. Toast 提示

- [x] Apply 后显示 "已对 X 通道应用滤波" toast
- [x] Apply-All 后显示 "已对所有逻辑通道应用滤波 (阈值 X)" toast
- [x] Clear 后显示 "X 通道滤波已清除" toast
- [x] Undo 后显示 "已撤销" toast

## L. 集成测试

- [x] 完整构建 `cd build && ninja -j 16 && ninja install` 通过
- [ ] 右键 D0 通道标签弹出浮窗
- [ ] 拖滑块时波形上橙色预览 overlay 实时变化
- [ ] Apply 后红色已滤 overlay 出现,popup 关闭
- [ ] Apply-All 后所有 LogicSignal 出现红色 overlay
- [ ] Ctrl+Z 撤销后 overlay 消失
- [ ] 打开 dock,勾选状态与 popup 一致
- [ ] dock Apply 后 popup(若打开)直方图刷新
- [ ] 预设选择后滑块/模式更新
- [ ] Esc 关闭浮窗,预览 overlay 清除
- [ ] 信号取反菜单项工作正常
- [ ] 清除单通道/所有通道滤波菜单项工作正常

## M. 架构合规

- [x] 新增 Core 代码不 #include QWidget/QMainWindow/QDialog
- [x] 新增 View 代码不直接调 `ds_*` libsigrok API(经 SigSession 转发)
- [x] `PulseAnalyzer` 不持有状态(纯静态方法)
- [x] popup 生命周期由 View 管理(RAII 或显式 delete)
- [x] 预览路径不触发 Core 后台线程(纯 View 层模拟)
- [x] Apply 路径复用现有 `set_glitch_filter` 异步流程(不新建线程)
- [x] 持久化存储线程安全(锁内操作)
- [x] 显式 `if(!ptr)` 检查,不依赖 `assert(ptr)`(AGENTS.md 约定)
- [x] 新增代码无 `volatile` 跨线程标志(用 `std::atomic`)
