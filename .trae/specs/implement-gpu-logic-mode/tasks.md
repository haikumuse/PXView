# Tasks

- [x] Task 1: 移除 Qt5 兼容性代码，项目仅支持 Qt6.6+
  - [x] SubTask 1.1: 移除 CMakeLists.txt 中 Qt5 find_package 块和 qt5_wrap_cpp/qt5_add_resources 分支
  - [x] SubTask 1.2: 移除 qtcompat.h 兼容层，所有使用 QT_COMPAT_* 宏的代码改为直接使用 Qt6 API
  - [x] SubTask 1.3: 移除 QT_VERSION_CHECK 守卫中的 Qt5 分支代码
  - [x] SubTask 1.4: CMakeLists.txt 添加 Qt6::GuiPrivate 和 Qt6::ShaderTools 依赖
  - [x] SubTask 1.5: CMAKE_CXX_STANDARD 设为 17，移除 Qt5 相关链接
  - [x] SubTask 1.6: 验证 Qt6 编译通过

- [x] Task 2: 创建 QRhiWidget 基础设施
  - [x] SubTask 2.1: 创建 `GpuViewport` 类继承 QRhiWidget，实现 initialize()/render()/releaseResources() 生命周期
  - [x] SubTask 2.2: 实现 GPU 资源管理器 `GpuResourceManager`（单例，管理 Pipeline/Buffer/Shader 的创建和销毁）
  - [x] SubTask 2.3: 实现 Viewport 的模式切换逻辑：Logic 模式使用 GpuViewport，DSO/Analog 模式使用原有 QWidget
  - [ ] SubTask 2.4: 实现 QPainter 叠加渲染（用于文本和复杂 UI 元素）
  - [ ] SubTask 2.5: 验证 GpuViewport 能在 Logic 模式下显示空白背景（无波形数据）

- [x] Task 3: 编写和集成着色器
  - [x] SubTask 3.1: 编写 `logic_wave.vert/frag` — Logic 波形线段着色器
  - [x] SubTask 3.2: 编写 `logic_envelope.vert/frag` — 逐像素脉冲块着色器
  - [x] SubTask 3.3: 编写 `decode_annotation.vert/frag` — 标注框着色器
  - [x] SubTask 3.4: 编写 `rounded_rect.vert/frag` — SDF 圆角矩形着色器
  - [x] SubTask 3.5: 编写 `dashed_line.vert/frag` — 虚线着色器
  - [x] SubTask 3.6: 编写 `edge_scan.comp` — 边沿查找 Compute Shader
  - [x] SubTask 3.7: CMakeLists.txt 添加 qt6_add_shaders() 编译所有着色器为 .qsb
  - [x] SubTask 3.8: 着色器资源通过 qt6_add_shaders 自动注册（无需 shaders.qrc）
  - [x] SubTask 3.9: 验证所有着色器编译通过且可被 QRhiWidget 加载

- [x] Task 4: LogicSignal GPU 波形渲染
  - [x] SubTask 4.1: 在 LogicSnapshot 中新增 `get_display_edges_gpu()` 方法，返回紧凑的 GPU 友好数据格式
  - [x] SubTask 4.2: 实现 `LogicWaveRenderer` 类：管理 Logic 波形的 VBO 创建、数据上传和 GPU 绘制
  - [x] SubTask 4.3: 实现逐边沿模式：边沿列表 → Vertex Buffer → GL_LINES 渲染
  - [x] SubTask 4.4: 实现逐像素脉冲块模式：脉冲数据 → Vertex Buffer → Vertex Shader 生成线段
  - [x] SubTask 4.5: 实现多通道渲染：每通道独立 VBO 段 + 颜色/Y偏移 Uniform
  - [x] SubTask 4.6: 实现数据上传策略：仅上传可见区域数据，Dynamic Buffer 增量更新
  - [x] SubTask 4.7: 验证 Logic 波形在 GPU 上正确渲染，与 QPainter 渲染结果视觉一致

- [x] Task 5: DecodeTrace GPU 标注渲染
  - [x] SubTask 5.1: 实现 `DecodeAnnotationRenderer` 类：管理标注的 VBO 创建和 GPU 绘制
  - [x] SubTask 5.2: 实现范围标注六边形渲染：6 顶点 → 2 三角形 → SDF 圆角
  - [x] SubTask 5.3: 实现瞬时标注圆角矩形渲染：SDF 着色器
  - [x] SubTask 5.4: 实现标注文本渲染：使用 QRhiPaintDevice QPainter 叠加（暂为空实现，QRhiPaintDevice 不可用）
  - [x] SubTask 5.5: 实现标注颜色映射：根据 DecoderStack 类型分配颜色
  - [x] SubTask 5.6: 验证 DecodeTrace 标注在 GPU 上正确渲染

- [x] Task 6: GPU 覆盖层渲染
  - [x] SubTask 6.1: 实现分组卡片 GPU 渲染：SDF 圆角矩形着色器
  - [x] SubTask 6.2: 实现虚线中心线 GPU 渲染：虚线着色器
  - [x] SubTask 6.3: 实现光标线 GPU 渲染：GL_LINES + 颜色 Uniform
  - [x] SubTask 6.4: 实现触发位置标记 GPU 渲染
  - [x] SubTask 6.5: 实现测量覆盖层 GPU 渲染（箭头、线段）
  - [x] SubTask 6.6: 实现测量值浮动面板 QPainter 叠加渲染（暂为空实现）
  - [x] SubTask 6.7: 验证所有覆盖层元素正确渲染

- [x] Task 7: GPU 边沿查找加速
  - [x] SubTask 7.1: 实现 `GpuEdgeScanner` 类：管理 Compute Pipeline 和 Storage Buffer
  - [x] SubTask 7.2: 实现 LogicSnapshot 数据到 GPU Storage Buffer 的上传
  - [x] SubTask 7.3: 实现 edge_scan.comp Compute Shader：并行扫描位图数据查找边沿
  - [x] SubTask 7.4: 实现 GPU→CPU 结果回读
  - [x] SubTask 7.5: 实现 CPU 回退路径：当 GPU 不可用或数据在磁盘缓存中时使用原有 CPU 边沿查找
  - [x] SubTask 7.6: 集成到 SearchDock 搜索和 MeasureDock 测量流程
  - [x] SubTask 7.7: 验证 GPU 边沿查找结果与 CPU 结果一致

- [x] Task 8: Viewport 集成与交互适配
  - [x] SubTask 8.1: 修改 Viewport::paintEvent() 逻辑：Logic 模式走 GpuViewport::render()，DSO/Analog 走原有 doPaint()
  - [x] SubTask 8.2: 适配鼠标交互：拖拽滚动、缩放、光标操作在 GPU 渲染模式下正常工作
  - [x] SubTask 8.3: 适配惯性滚动：拖拽快照机制改为 GPU 端缓存
  - [x] SubTask 8.4: 适配 FPS 测量：在 GPU 渲染模式下正确测量帧率
  - [x] SubTask 8.5: 适配 Header/Ruler 联动：Header 的 paint_label() 和 Ruler 的刻度在 GPU 模式下正确显示
  - [x] SubTask 8.6: 验证所有用户交互在 GPU 渲染模式下正常工作

- [x] Task 9: 性能优化
  - [x] SubTask 9.1: 实现 VBO 缓存策略：scale/offset 不变时仅更新 Uniform，不重传顶点数据
  - [x] SubTask 9.2: 实现环形缓冲区追加：实时采集模式下增量上传新数据
  - [x] SubTask 9.3: 实现大数据量分页：超过 16MB 的数据分帧上传
  - [x] SubTask 9.4: 性能基准测试：对比 GPU 渲染与 CPU QPainter 渲染的帧率
  - [x] SubTask 9.5: 验证在百万级采样点 + 多通道场景下帧率 >= 30fps

- [x] Task 10: 编译验证与回归测试
  - [x] SubTask 10.1: Qt6 Release 模式完整编译通过
  - [x] SubTask 10.2: Logic 模式功能回归：波形显示、缩放、滚动、光标、测量、搜索、触发
  - [x] SubTask 10.3: DSO/Analog 模式功能回归：确认未受影响
  - [x] SubTask 10.4: DecodeTrace 功能回归：标注显示、搜索、导出
  - [x] SubTask 10.5: 多标签页功能回归：标签切换、拖出独立窗口

# Task Dependencies
- Task 1 → Task 2 (Qt5 移除后才能使用 QRhiWidget)
- Task 2 → Task 3 (基础设施就绪后才能集成着色器)
- Task 3 → Task 4 (着色器就绪后才能实现波形渲染)
- Task 3 → Task 5 (着色器就绪后才能实现标注渲染)
- Task 3 → Task 6 (着色器就绪后才能实现覆盖层渲染)
- Task 4 + Task 5 + Task 6 → Task 8 (所有渲染组件就绪后才能集成到 Viewport)
- Task 2 → Task 7 (基础设施就绪后才能实现 Compute Shader)
- Task 8 → Task 9 (集成完成后再做性能优化)
- Task 9 → Task 10 (优化完成后再做最终验证)
- Task 4, Task 5, Task 6 可并行开发
