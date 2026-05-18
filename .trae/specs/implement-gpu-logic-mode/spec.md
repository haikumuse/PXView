# Logic 模式 GPU 加速渲染 Spec

## Why
PXView 在 Logic 模式下，百万级采样点的边沿渲染和边沿查找全部在 CPU 上通过 QPainter 完成，滚动/缩放时全量重绘导致帧率下降。利用 Qt6 QRhiWidget 将 Logic 模式的波形渲染和边沿查找迁移到 GPU，可显著提升渲染性能和交互流畅度。

## What Changes
- **BREAKING**: 放弃 Qt5 兼容性，项目仅支持 Qt6.6+
- 将 `Viewport` 在 Logic 模式下从 `QWidget` + QPainter 切换为 `QRhiWidget` + GPU 渲染管线
- LogicSignal 波形线段使用 GPU Vertex Buffer + GL_LINES 渲染
- DecodeTrace 标注使用 GPU 渲染（六边形/圆角矩形 + 文字图集）
- Logic 模式分组卡片使用 GPU SDF 圆角矩形渲染
- 虚线中心线使用 GPU 虚线着色器
- 边沿查找使用 GPU Compute Shader 并行扫描（替代 CPU Mipmap 遍历）
- Analog/DSO/Math/Spectrum/Lissajous 信号保持原有 CPU QPainter 渲染不变
- Ruler/Header 保持原有 QWidget + QPainter 不变
- 新增 Shader 文件（.vert/.frag/.comp）和 CMake 着色器编译集成
- CMakeLists.txt 添加 Qt6::GuiPrivate、Qt6::ShaderTools 依赖

## Impact
- Affected specs: restore-qt6-compatibility（Qt5 兼容性将被移除）
- Affected code:
  - `PXView/pv/view/viewport.h/cpp` — 核心改动，Logic 模式走 GPU 路径
  - `PXView/pv/view/logicsignal.h/cpp` — paint 方法拆分为数据准备 + GPU 渲染
  - `PXView/pv/view/decodetrace.h/cpp` — Logic 模式下标注 GPU 渲染
  - `PXView/pv/view/trace.h/cpp` — paint_back 虚线中心线 GPU 渲染
  - `PXView/pv/data/logicsnapshot.h/cpp` — 新增 GPU 边沿查找接口
  - `PXView/pv/view/view.h/cpp` — Viewport 类型切换逻辑
  - `CMakeLists.txt` — Qt6 依赖、着色器编译
  - 新增文件: GPU 渲染管线类、Shader 文件、GPU 边沿查找类

## ADDED Requirements

### Requirement: QRhiWidget Viewport 基础设施
系统 SHALL 在 Logic 模式下使用 QRhiWidget 替代 QWidget 作为 Viewport 的渲染表面。

#### Scenario: QRhiWidget 初始化
- **WHEN** 用户打开一个 Logic 模式的标签页
- **THEN** Viewport 创建 QRhiWidget 实例，在 `initialize()` 中创建 GPU 资源（Pipeline、Buffer、Shader），在 `render()` 中执行 GPU 绘制命令

#### Scenario: RHI 后端切换
- **WHEN** GPU 设备变更或 RHI 后端切换（如从 D3D11 切换到 Vulkan）
- **THEN** `rhiChanged()` 回调触发，所有 GPU 资源被释放并重建

#### Scenario: 非 Logic 模式回退
- **WHEN** 工作模式为 DSO 或 Analog
- **THEN** Viewport 使用原有 QWidget + QPainter 渲染，不创建 GPU 资源

### Requirement: LogicSignal GPU 波形渲染
系统 SHALL 将 LogicSignal 的波形线段通过 GPU Vertex Buffer + GL_LINES 渲染。

#### Scenario: 逐边沿模式渲染
- **WHEN** 可见区域内边沿数量 < max_togs（width / TogMaxScale）
- **THEN** CPU 端调用 `get_display_edges()` 获取边沿列表，将边沿数据上传为 GPU Vertex Buffer，使用 GL_LINES 拓扑渲染波形线段

#### Scenario: 逐像素脉冲块模式渲染
- **WHEN** 可见区域内边沿数量 >= max_togs
- **THEN** CPU 端调用 `get_display_edges()` 获取脉冲块数据，将每像素的跳变/电平数据上传为 GPU Buffer，在 Vertex Shader 中根据电平状态生成高低电平线段

#### Scenario: 多通道渲染
- **WHEN** 存在多个启用的 Logic 通道
- **THEN** 每个通道的波形数据作为独立的 Vertex Buffer 段，使用 Instanced Drawing 或多次 Draw Call 渲染，每通道使用不同的 Y 偏移和颜色 Uniform

#### Scenario: 波形颜色
- **WHEN** Logic 通道设置了自定义颜色
- **THEN** GPU 渲染使用该颜色作为 Fragment Shader 输出颜色

### Requirement: DecodeTrace GPU 标注渲染
系统 SHALL 在 Logic 模式下将 DecodeTrace 的协议标注通过 GPU 渲染。

#### Scenario: 范围标注渲染
- **WHEN** 标注的 start_sample != end_sample
- **THEN** 使用 GPU 绘制六边形标注框（2 个三角形），填充颜色根据标注类型确定，文本使用文字图集渲染

#### Scenario: 瞬时标注渲染
- **WHEN** 标注的 start_sample == end_sample
- **THEN** 使用 GPU 绘制圆角矩形（SDF 着色器），文本使用文字图集渲染

#### Scenario: 标注文本溢出
- **WHEN** 标注框宽度不足以容纳文本
- **THEN** 文本被截断并添加省略号，与当前 QPainter 行为一致

### Requirement: GPU 覆盖层渲染
系统 SHALL 在 GPU 波形之上渲染交互覆盖层元素。

#### Scenario: 分组卡片渲染
- **WHEN** Logic 模式下存在信号分组
- **THEN** 使用 GPU SDF 圆角矩形着色器渲染分组卡片背景

#### Scenario: 虚线中心线渲染
- **WHEN** 信号处于可见状态
- **THEN** 使用 GPU 虚线着色器渲染中心线（Qt::DotLine 风格）

#### Scenario: 光标渲染
- **WHEN** 用户添加了时间光标
- **THEN** 光标线使用 GPU 渲染（GL_LINES），光标标签使用 QPainter 叠加渲染

#### Scenario: 测量覆盖层渲染
- **WHEN** 用户在 Logic 模式下进行频率/边沿/跳转测量
- **THEN** 测量箭头、线段使用 GPU 渲染，测量值浮动面板使用 QPainter 叠加渲染

#### Scenario: 触发位置标记
- **WHEN** 存在触发位置
- **THEN** 触发位置竖线使用 GPU 渲染

### Requirement: GPU 边沿查找加速
系统 SHALL 使用 GPU Compute Shader 加速 Logic 模式的边沿查找操作。

#### Scenario: 搜索边沿
- **WHEN** 用户在 SearchDock 中执行逻辑信号搜索
- **THEN** 搜索算法使用 GPU Compute Shader 并行扫描 LogicSnapshot 数据，替代 CPU 端的 `get_nxt_edge()` / `get_pre_edge()` 串行遍历

#### Scenario: 测量边沿
- **WHEN** 用户在 Viewport 上进行边沿计数测量
- **THEN** 边沿计数使用 GPU Compute Shader 并行计算

#### Scenario: GPU 数据不可用回退
- **WHEN** GPU Compute Shader 不可用（如驱动不支持）或数据在磁盘缓存中
- **THEN** 回退到 CPU 端 `get_nxt_edge()` / `get_pre_edge()` 查找

### Requirement: 数据上传策略
系统 SHALL 高效地将 LogicSnapshot 数据上传到 GPU。

#### Scenario: 可见区域数据上传
- **WHEN** 视口范围变化（滚动/缩放）
- **THEN** 仅将可见区域对应的采样数据上传到 GPU Vertex Buffer，使用 `QRhiBuffer::Dynamic` + `uploadDynamicBuffer()` 增量更新

#### Scenario: 大数据量分页
- **WHEN** 可见区域的采样数据超过 GPU Buffer 容量
- **THEN** 数据分页上传，每页不超过 16MB，使用多帧逐步上传

#### Scenario: 环形缓冲区追加
- **WHEN** 实时采集模式下新数据到达
- **THEN** 新数据追加到 GPU Buffer 尾部，使用环形缓冲区策略避免全量重传

### Requirement: QPainter 混合渲染
系统 SHALL 在 GPU 渲染的波形之上使用 QPainter 叠加渲染文本和复杂 UI 元素。

#### Scenario: 文字图集渲染
- **WHEN** 需要渲染 DecodeTrace 标注文本、测量值文本
- **THEN** 使用 QRhiPaintDevice（Qt 6.7+）在 GPU render pass 之后用 QPainter 渲染文本

#### Scenario: 复杂 UI 元素
- **WHEN** 需要渲染浮动面板、触发图标等复杂 UI 元素
- **THEN** 使用 QPainter 叠加渲染，不强制 GPU 化

### Requirement: 着色器管理
系统 SHALL 管理所有 GPU 着色器的编译和加载。

#### Scenario: 着色器编译
- **WHEN** 项目构建时
- **THEN** CMake 使用 `qt6_add_shaders()` 将 GLSL 着色器编译为 .qsb 格式（包含 SPIR-V + HLSL + MSL + GLSL 多后端），嵌入资源文件

#### Scenario: 着色器加载
- **WHEN** QRhiWidget 初始化时
- **THEN** 从 Qt 资源系统加载 .qsb 着色器文件，创建 QRhiGraphicsPipeline

#### Scenario: 着色器列表
- **WHEN** 系统运行
- **THEN** 以下着色器可用：
  - `logic_wave.vert/frag` — Logic 波形线段渲染
  - `logic_envelope.vert/frag` — 逐像素脉冲块渲染
  - `decode_annotation.vert/frag` — 标注框渲染
  - `rounded_rect.vert/frag` — SDF 圆角矩形（分组卡片）
  - `dashed_line.vert/frag` — 虚线中心线
  - `edge_scan.comp` — 边沿查找 Compute Shader

### Requirement: CMake 构建集成
系统 SHALL 在 CMakeLists.txt 中正确配置 Qt6 GPU 渲染所需的依赖和构建步骤。

#### Scenario: Qt6 依赖
- **WHEN** CMake 配置项目
- **THEN** find_package 包含 Qt6::GuiPrivate 和 Qt6::ShaderTools

#### Scenario: 着色器编译集成
- **WHEN** 执行 ninja 构建
- **THEN** 着色器 .vert/.frag/.comp 文件被自动编译为 .qsb 并嵌入资源

#### Scenario: C++ 标准
- **WHEN** CMake 配置项目
- **THEN** CMAKE_CXX_STANDARD 设置为 17（Qt6 最低要求）

## MODIFIED Requirements

### Requirement: Viewport 渲染调度
原要求：Viewport 使用 QWidget + QPainter 统一渲染所有信号类型。
修改为：Viewport 在 Logic 模式下使用 QRhiWidget + GPU 渲染管线，在 DSO/Analog 模式下使用 QWidget + QPainter 渲染。模式切换时动态替换 Viewport 内部渲染表面。

### Requirement: LogicSnapshot 数据访问
原要求：LogicSnapshot 通过 `get_display_edges()` 返回 CPU 端边沿列表。
修改为：LogicSnapshot 新增 `get_display_edges_gpu()` 方法，返回适合 GPU Buffer 上传的紧凑数据格式（边沿位置数组 + 电平状态数组），同时保留原有 `get_display_edges()` 供 CPU 回退使用。

### Requirement: 双缓冲缓存
原要求：Viewport 使用 QPixmap 双缓冲缓存波形渲染结果。
修改为：Logic 模式下不再使用 QPixmap 双缓冲，改为 GPU 端 VBO 缓存。当 scale/offset 不变时，仅更新 Uniform Buffer 中的偏移量，不重新上传顶点数据。DSO/Analog 模式保持原有 QPixmap 双缓冲。

## REMOVED Requirements

### Requirement: Qt5 兼容性
**Reason**: QRhiWidget 仅在 Qt6.6+ 可用，且需要 Qt6::GuiPrivate 私有 API。GPU 渲染管线与 Qt5 的 QOpenGLWidget 架构不兼容。
**Migration**: 移除所有 `QT_VERSION_CHECK` 守卫和 `qtcompat.h` 兼容层，CMakeLists.txt 仅查找 Qt6。移除 Qt5WinExtras 替代代码（WinTaskbarProgress 可保留，因为它不依赖 Qt 版本）。
