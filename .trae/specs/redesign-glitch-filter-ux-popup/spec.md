# 毛刺滤波 UX 重设计:右键浮窗 + 直方图 + 实时预览 Spec

## Why
当前毛刺滤波入口仅在侧边 `SignalProcessingDock` 中,用户需逐通道勾选 CheckBox + 输 SpinBox + 点 Apply 才能看效果,且滤波结果在波形上完全不可见,调阈值等于盲调。HTML 原型(`prototype/glitch_filter_ux.html`)已验证"右键通道标签 → 弹出浮窗 → 拖滑块实时预览 → 一键应用到所有通道"的交互显著优于现有 dock。本 spec 将该 UX 以纯 QWidget 形式落地到 PXView,不引入 QML,保持与现有 QPainter 波形渲染栈一致。

## What Changes
- **新增 Core 工具类 `pv::data::PulseAnalyzer`**:提供脉冲扫描、脉冲宽度直方图、推荐阈值计算、纯函数式预览滤波
- **改 `LogicSnapshot`**:将局部 `FillRange` struct 提升为 public,新增 `_filtered_ranges_per_channel` 持久化存储 + `get_filtered_ranges(sig_index)` 访问器;`apply_glitch_filter` 内 push fills 时同步写入持久化存储;`clear_glitch_filter` 路径清空该存储
- **改 `FilterProcessor`**:修复进度回调 `(void)progress` 丢弃问题,透传进度数值到 typed event `GlitchFilterProgress`
- **改 `Header::contextMenuEvent`**:在 LOGIC 模式下于现有行高菜单前追加"滤除毛刺..."/"信号取反"/"清除此通道滤波"/"清除所有通道滤波"菜单项,emit `show_glitch_filter_popup` 信号给 View
- **新增 `pv::view::GlitchFilterPopup`**:Qt::Popup 风格浮窗,含直方图 widget、阈值滑块、模式 ComboBox、Apply/Apply-All/Cancel 按钮、统计标签;QSS 暗色主题匹配项目
- **新增 `pv::view::PulseHistogramWidget`**:自定义 QWidget paintEvent 绘制柱状图 + 推荐阈值橙线 + 当前阈值蓝线
- **改 `View`**:新增 `_glitch_filter_popup` 成员、`_preview_ranges` 缓存、`on_show_glitch_filter_popup`/`on_preview_changed`/`on_apply_requested` slot;监听 `DSV_MSG_GLITCH_FILTER_COMPLETED/CLEARED` 清预览
- **改 `LogicSignal::paint_mid_align`**:在 `drawLines()` 后渲染 overlay——已滤区间红色半透明(从 `LogicSnapshot::get_filtered_ranges` 取)、预览区间橙色半透明(从 `View::_preview_ranges` 取)
- **新增 View 层撤销栈**:`std::vector<FilterSnapshot>` 记录每次 Apply 前的通道配置,Ctrl+Z 恢复
- **新增预设管理**:popup 顶部预设下拉(I2C 抗串扰 / SPI 启动毛刺 / 通用 5 周期),持久化到 SessionDocument 现有 glitch_filter config
- **改 `SignalProcessingDock`**:监听 `DSV_MSG_GLITCH_FILTER_COMPLETED` 时若 popup 已打开则刷新其直方图;popup Apply 通过现有广播自动触发 dock 的 `update_glitch_filter_state()`

## Impact
- Affected specs:
  - `add-logic-glitch-filter` — 复用其 `apply_glitch_filter` 算法,新增 filtered_ranges 持久化
  - `enhance-decoder-search-and-glitch-filter` — 复用其 GlitchFilterMode 三向滤波
  - `add-signal-processing-dock` — dock 角色从"主操作入口"降级为"管理后台",但功能保留
- Affected code:
  - `PXView/pv/data/pulse_analyzer.h/cpp` — **新增** Core 工具类
  - `PXView/pv/data/logicsnapshot.h/cpp` — FillRange 提升 public + 持久化存储
  - `PXView/pv/core/filterprocessor.cpp` — 进度透传
  - `PXView/pv/view/header.h/cpp` — contextMenuEvent 扩展 + show_glitch_filter_popup 信号
  - `PXView/pv/view/glitchfilterpopup.h/cpp` — **新增** 浮窗
  - `PXView/pv/view/pulsehistogramwidget.h/cpp` — **新增** 直方图 widget
  - `PXView/pv/view/view.h/cpp` — popup 持有 + preview 缓存 + slot
  - `PXView/pv/view/logicsignal.h/cpp` — paint_mid_align overlay
  - `PXView/pv/dock/signalprocessingdock.cpp` — 监听消息刷新
  - `PXView/pv/data/sessiondocument.h/cpp` — 预设字段持久化(可选,首版可跳过)
  - `CMakeLists.txt` — 新增 4 个源文件到 PXVIEW_GUI_SOURCES / PXVIEW_CORE_SOURCES

## ADDED Requirements

### Requirement: Core 脉冲分析能力
系统 SHALL 在 `pv::data::PulseAnalyzer` 中提供静态方法,扫描 `LogicSnapshot` 单通道的脉冲序列并构建脉冲宽度直方图。

#### Scenario: 扫描脉冲序列
- **WHEN** 调用 `PulseAnalyzer::find_pulses(snap, sig_index, max_samples)`
- **THEN** 返回 `std::vector<Pulse>`,每个 Pulse 含 `{start, end, level}`,pulse.width() = end - start
- **AND** 扫描复用 `LogicSnapshot::get_nxt_edge` 边沿检测,不重复实现
- **AND** max_samples 限制扫描范围,默认 100000 防止超大采集卡顿

#### Scenario: 构建直方图
- **WHEN** 调用 `PulseAnalyzer::build_histogram(pulses, max_width=30)`
- **THEN** 返回 `Histogram{width_counts, max_width}`,仅统计 `pulse.width() <= max_width` 的短脉冲(长脉冲非毛刺候选)

#### Scenario: 推荐阈值
- **WHEN** 调用 `PulseAnalyzer::recommend_threshold(hist)`
- **THEN** 返回基于"最大间隙"启发式的推荐阈值:在排序后的宽度分布中找最大跳变间隙,推荐值 = 间隙右侧
- **AND** 若无明显间隙,返回较小宽度的 30% 分位数 + 1
- **AND** 若无脉冲,返回默认值 3

#### Scenario: 纯函数式预览
- **WHEN** 调用 `PulseAnalyzer::preview_filter(pulses, threshold, mode)`
- **THEN** 返回 `std::vector<Pulse>` 中会被滤除的子集
- **AND** 此函数为纯函数,不修改任何状态,不调 Core
- **AND** 滤波判定逻辑与 `LogicSnapshot::apply_glitch_filter` 的 filter_mode 语义一致(Both 全滤、High 仅高基准低毛刺、Low 仅低基准高毛刺)

### Requirement: LogicSnapshot 滤波区间持久化
系统 SHALL 在 `LogicSnapshot` 中持久化每个通道被滤除的区间列表,供 View 层渲染 overlay。

#### Scenario: 滤波时记录区间
- **WHEN** `apply_glitch_filter(sig_index, threshold, ...)` 执行滤波
- **THEN** 每个 push 到局部 fills 的 FillRange 同步追加到 `_filtered_ranges_per_channel[sig_index]`
- **AND** 批量刷盘(`apply_batch` 清空局部 fills)不影响持久化存储

#### Scenario: 访问已滤区间
- **WHEN** 调用 `get_filtered_ranges(sig_index)`
- **THEN** 返回 `const std::vector<FillRange>&`,若该通道未滤波则返回空 vector 引用

#### Scenario: 清空已滤区间
- **WHEN** FilterProcessor 执行 `clear_glitch_filter` 或从 backup 恢复
- **THEN** `_filtered_ranges_per_channel` 被清空
- **AND** 后续 `get_filtered_ranges` 返回空

#### Scenario: FillRange 类型公开
- **WHEN** View 层引用 FillRange 类型
- **THEN** `FillRange` 为 `LogicSnapshot` 的 public 嵌套 struct,含 `{uint64_t start; uint64_t end; bool level;}`
- **AND** 不破坏 `apply_glitch_filter` 内现有的局部 FillRange 用法

### Requirement: FilterProcessor 进度透传
系统 SHALL 在滤波进度回调中保留 progress 数值并透传到 typed event。

#### Scenario: 进度数值传递
- **WHEN** `apply_glitch_filter_all` 的 progress_callback 被调用(参数 progress 为 0-100)
- **THEN** `DSV_MSG_GLITCH_FILTER_PROGRESS` 通过 `trigger_message(code, param)` 携带 progress 值
- **AND** 若 typed event 路径启用,`broadcast<GlitchFilterProgress>({progress})` 携带真实进度
- **AND** View 层可在 popup 中显示百分比进度条

### Requirement: Header 右键菜单入口
系统 SHALL 在 LOGIC 模式下,于 Header 通道标签的右键菜单中提供毛刺滤波入口。

#### Scenario: 右键 LogicSignal 标签
- **WHEN** 用户在 LOGIC 模式下右键点击某 LogicSignal 的 LABEL 区域
- **THEN** contextMenuEvent 在现有行高菜单前插入分隔符 + 4 个菜单项:
  - "🔍 滤除毛刺..." (快捷键 F)
  - "↔ 信号取反" (快捷键 I)
  - "✕ 清除此通道滤波" (仅当该通道已滤波时启用)
  - "✕ 清除所有通道滤波" (仅当任一通道已滤波时启用)
- **AND** 非 LogicSignal 目标(如 DecodeTrace)不显示这些菜单项

#### Scenario: 触发浮窗
- **WHEN** 用户点击"滤除毛刺..."菜单项
- **THEN** Header emit `show_glitch_filter_popup(LogicSignal*)` 信号
- **AND** View 收到信号后打开 GlitchFilterPopup,锚定在通道标签右侧 8px

#### Scenario: 快捷键支持
- **WHEN** popup 已打开且用户按 F 键
- **THEN** 不重复打开(避免嵌套)
- **WHEN** 鼠标悬停在通道标签上按 F 键(popup 未打开)
- **THEN** 打开该通道的滤波浮窗

### Requirement: GlitchFilterPopup 浮窗控件
系统 SHALL 提供 `pv::view::GlitchFilterPopup` 浮窗,布局匹配 HTML 原型。

#### Scenario: 浮窗布局
- **WHEN** popup 打开
- **THEN** 显示以下控件自上而下:
  1. 标题栏:"<通道名> (<自定义标签>) 毛刺滤波" + 关闭按钮 ×
  2. "脉冲宽度分布"区域标题
  3. PulseHistogramWidget 直方图(高度 140px)
  4. 统计行:"将滤除: X 个脉冲" | "剩余有效脉冲: Y 个"
  5. 类型选择行:Both/High/Low ComboBox
  6. 阈值行:滑块 + 数值显示 + "cycles" 单位
  7. 底部按钮:"应用到本通道" | "应用到所有逻辑通道" | "取消"
- **AND** 浮窗宽度 420px,Qt::Popup | Qt::FramelessWindowHint
- **AND** QSS 暗色主题与项目一致(背景 #252932、边框 #3a3f4b、圆角 8px)
- **AND** QGraphicsDropShadowEffect 提供阴影

#### Scenario: 打开时填充推荐值
- **WHEN** popup 为某通道打开
- **THEN** 调用 `PulseAnalyzer::find_pulses` + `build_histogram` + `recommend_threshold`
- **AND** 直方图渲染柱子 + 推荐阈值橙色虚线
- **AND** 若该通道已有滤波配置,滑块/ComboBox 用现有值;否则用推荐值
- **AND** 统计行显示当前阈值下将滤除/剩余的脉冲数

#### Scenario: 拖滑块实时预览
- **WHEN** 用户拖动阈值滑块
- **THEN** 仅 emit `preview_changed(sig, threshold, mode)` 信号,不调 Core
- **AND** 直方图柱子按阈值重新着色(将被滤除的宽度变红)
- **AND** 当前阈值蓝色线移动到新位置
- **AND** 统计行数字实时更新
- **AND** View 收到信号后更新 `_preview_ranges` 并触发 Viewport 重绘

#### Scenario: 切换模式实时预览
- **WHEN** 用户切换 Both/High/Low ComboBox
- **THEN** 同上 emit `preview_changed`,直方图重新着色,统计行更新

#### Scenario: 应用到本通道
- **WHEN** 用户点击"应用到本通道"
- **THEN** emit `apply_requested(sig, threshold, mode, all_channels=false)`
- **AND** View 调用 `_session->set_glitch_filter(thresholds, modes)`(thresholds/modes 仅该通道非零)
- **AND** popup 关闭

#### Scenario: 应用到所有逻辑通道
- **WHEN** 用户点击"应用到所有逻辑通道"
- **THEN** emit `apply_requested(sig, threshold, mode, all_channels=true)`
- **AND** View 对所有 LogicSignal 组装统一 threshold + mode 数组
- **AND** 调用 `_session->set_glitch_filter(...)`
- **AND** popup 关闭
- **AND** 显示 toast "已对所有逻辑通道应用滤波 (阈值 X)"

#### Scenario: 取消/Esc/外部点击
- **WHEN** 用户点击"取消"按钮、按 Esc、或点击浮窗外区域
- **THEN** popup 关闭,emit `closed()` 信号
- **AND** View 清除 `_preview_ranges` 并触发重绘

### Requirement: PulseHistogramWidget 直方图控件
系统 SHALL 提供自定义 QWidget 绘制脉冲宽度分布直方图。

#### Scenario: 渲染柱子
- **WHEN** `setData(Histogram)` 被调用后触发 `update()`
- **THEN** paintEvent 绘制 max_width 个柱子,高度按 count/max_count 比例
- **AND** 默认柱色灰色 #4a5060,圆角顶部

#### Scenario: 阈值线
- **WHEN** `setThresholds(recommended, current)` 被调用
- **THEN** 绘制橙色虚线(推荐值,带"推荐"标签)和蓝色实线(当前值,带"当前"标签)
- **AND** 阈值线位置随滑块拖动实时更新

#### Scenario: 柱子着色
- **WHEN** `setFilterThreshold(threshold)` 被调用
- **THEN** 宽度 <= threshold 的柱子变红色 #ff5252
- **AND** 宽度 == recommended 的柱子变橙色 #ffb74d

### Requirement: View 层实时预览 overlay
系统 SHALL 在 LogicSignal 波形上叠加渲染滤波 overlay。

#### Scenario: 渲染已滤区间(红色)
- **WHEN** `LogicSnapshot::is_glitch_filtered()` 为 true 且 `get_filtered_ranges(sig_index)` 非空
- **THEN** `LogicSignal::paint_mid_align` 在 `drawLines()` 后用半透明红色 `QColor(255, 82, 82, 90)` 绘制每个 FillRange 矩形
- **AND** 矩形 x 坐标按 `(start - offset) / samples_per_pixel` 计算,y 范围为 signal 的 high_offset 到 low_offset

#### Scenario: 渲染预览区间(橙色)
- **WHEN** `View::_preview_ranges` 中存在该 signal 的条目
- **THEN** 用半透明橙色 `QColor(255, 183, 77, 70)` 绘制每个预览 Pulse 矩形
- **AND** 预览 overlay 与已滤 overlay 互斥(已滤时不显示预览)

#### Scenario: 性能约束
- **WHEN** overlay 渲染
- **THEN** 仅绘制当前可见 x 范围内的矩形(裁剪到 viewport)
- **AND** 矩形数量超过 1000 时合并相邻矩形避免 QPainter 调用爆炸

### Requirement: View 层撤销栈
系统 SHALL 提供 Ctrl+Z 撤销最近一次滤波操作。

#### Scenario: Apply 前压栈
- **WHEN** 用户点击 Apply/Apply-All
- **THEN** View 在调用 `set_glitch_filter` 前 push 当前 `_glitch_filter_thresholds` + `_glitch_filter_modes` 快照到 `_filter_undo_stack`
- **AND** 栈最大深度 20

#### Scenario: Ctrl+Z 撤销
- **WHEN** 用户按 Ctrl+Z 且 `_filter_undo_stack` 非空
- **THEN** pop 栈顶,调用 `_session->set_glitch_filter(thresholds, modes)` 恢复
- **AND** 若栈顶为空快照(对应"清空滤波"前的状态),调用 `clear_glitch_filter()`
- **AND** 显示 toast "已撤销"

#### Scenario: 与现有 undo 不冲突
- **WHEN** 现有项目有其他 undo 操作(若有)
- **THEN** 滤波 undo 独立栈,不干扰
- **AND** Ctrl+Z 在 View 焦点下优先触发滤波 undo

### Requirement: 预设管理
系统 SHALL 在 popup 顶部提供预设下拉,一键套用常用配置。

#### Scenario: 选择预设
- **WHEN** 用户在预设下拉选择"I2C 抗串扰 (2 cycles, Both)"
- **THEN** 滑块设为 2,模式设为 Both
- **AND** 直方图/统计行/预览 overlay 同步更新(不自动 Apply)
- **AND** 用户可继续调整或点 Apply

#### Scenario: 内置预设
- **WHEN** popup 打开
- **THEN** 预设下拉包含:"预设..."(占位)、"I2C 抗串扰 (2 cycles, Both)"、"SPI 启动毛刺 (3 cycles, High)"、"通用 5 周期滤波"

### Requirement: 与现有 Dock 同步
系统 SHALL 确保 popup 与 SignalProcessingDock 状态一致。

#### Scenario: popup Apply 触发 dock 刷新
- **WHEN** popup Apply → `_session->set_glitch_filter` → 广播 `DSV_MSG_GLITCH_FILTER_COMPLETED`
- **THEN** 现有 [mainwindow.cpp:3239-3251] 已调用 `_signal_processing_widget->update_glitch_filter_state()`
- **AND** 无需新增代码

#### Scenario: dock Apply 触发 popup 刷新
- **WHEN** dock Apply → 同上广播
- **THEN** 若 popup 已打开,GlitchFilterPopup 监听该消息后重算目标通道直方图 + 刷新滑块默认值

## MODIFIED Requirements

### Requirement: Header 上下文菜单(原 [add-header-row-height-menu])
Header 的 contextMenuEvent 在 LOGIC 模式下 SHALL 同时提供行高管理菜单与滤波操作菜单,滤波菜单项在前,行高菜单项在后,中间用分隔符隔开。仅当目标 Trace 为 LogicSignal 且持有有效 LogicSnapshot 时显示滤波菜单项。

### Requirement: SignalProcessingDock 角色(原 [add-signal-processing-dock])
SignalProcessingDock 从"主操作入口"降级为"管理后台",保留现有功能作为批量调整与状态总览。新增的 GlitchFilterPopup 是单通道快速操作的首选入口,两者通过共享 Core 状态 + DSV_MSG 广播保持同步。

## REMOVED Requirements

(无移除需求——现有 dock 功能保留,本 spec 仅新增交互路径)
