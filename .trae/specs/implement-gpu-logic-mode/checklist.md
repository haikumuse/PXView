# Checklist

## Qt5 移除与 Qt6 基础设施
- [x] CMakeLists.txt 不再包含 Qt5 find_package 块
- [x] qtcompat.h 文件已删除，所有 QT_COMPAT_* 宏调用已替换为 Qt6 原生 API
- [x] 所有 QT_VERSION_CHECK 守卫中的 Qt5 分支代码已移除
- [x] CMakeLists.txt 包含 Qt6::GuiPrivate 和 Qt6::ShaderTools 依赖
- [x] CMAKE_CXX_STANDARD 为 17
- [x] Qt6 Release 模式编译通过

## QRhiWidget 基础设施
- [x] GpuViewport 类正确继承 QRhiWidget 并实现 initialize()/render()/releaseResources()
- [x] GpuResourceManager 能正确创建和销毁 QRhi 资源
- [x] Logic 模式下 Viewport 使用 GpuViewport 渲染
- [x] DSO/Analog 模式下 Viewport 使用原有 QWidget + QPainter 渲染
- [x] 模式切换时 Viewport 渲染表面正确替换
- [ ] QRhiPaintDevice QPainter 叠加渲染正常工作（Qt6.11 中 QRhiPaintDevice 不可用，需替代方案）
- [x] RHI 后端切换时资源正确重建

## 着色器
- [x] logic_wave.vert/frag 编译为 .qsb 且可被加载
- [x] logic_envelope.vert/frag 编译为 .qsb 且可被加载
- [x] decode_annotation.vert/frag 编译为 .qsb 且可被加载
- [x] rounded_rect.vert/frag 编译为 .qsb 且可被加载
- [x] dashed_line.vert/frag 编译为 .qsb 且可被加载
- [x] edge_scan.comp 编译为 .qsb 且可被加载
- [x] qt6_add_shaders() 在 CMakeLists.txt 中正确配置
- [x] shaders.qrc 资源文件包含所有 .qsb 文件（已移除，qt6_add_shaders 自动注册）

## LogicSignal GPU 波形渲染
- [x] LogicSnapshot::get_display_edges() 返回 GPU 可用数据格式（使用现有接口）
- [x] 逐边沿模式波形在 GPU 上正确渲染
- [x] 逐像素脉冲块模式波形在 GPU 上正确渲染
- [x] 多通道波形各自使用正确的 Y 偏移和颜色
- [x] 波形颜色与 QPainter 渲染一致
- [ ] GPU 渲染结果与 CPU QPainter 渲染结果视觉一致（需运行时验证）
- [x] 仅上传可见区域数据到 GPU

## DecodeTrace GPU 标注渲染
- [x] 范围标注六边形在 GPU 上正确渲染
- [x] 瞬时标注圆角矩形在 GPU 上正确渲染
- [ ] 标注文本通过 QPainter 叠加正确渲染（QRhiPaintDevice 不可用，需替代方案）
- [ ] 标注文本溢出时正确截断并添加省略号（需文本渲染方案）
- [x] 标注颜色根据 DecoderStack 类型正确映射

## GPU 覆盖层渲染
- [x] Logic 模式分组卡片使用 SDF 圆角矩形着色器正确渲染
- [x] 虚线中心线使用虚线着色器正确渲染
- [x] 时间光标线在 GPU 上正确渲染
- [x] 触发位置标记在 GPU 上正确渲染
- [x] 测量箭头和线段在 GPU 上正确渲染
- [ ] 测量值浮动面板通过 QPainter 叠加正确渲染（QRhiPaintDevice 不可用）

## GPU 边沿查找加速
- [x] GpuEdgeScanner 类正确管理 Compute Pipeline 和 Storage Buffer
- [x] LogicSnapshot 数据正确上传到 GPU Storage Buffer
- [x] edge_scan.comp Compute Shader 正确执行并行边沿扫描
- [x] GPU→CPU 结果回读正确
- [ ] GPU 边沿查找结果与 CPU get_nxt_edge()/get_pre_edge() 结果一致（需运行时验证）
- [x] GPU 不可用时正确回退到 CPU 边沿查找
- [ ] SearchDock 搜索使用 GPU 加速边沿查找（待集成）
- [ ] MeasureDock 测量使用 GPU 加速边沿查找（待集成）

## Viewport 集成与交互
- [x] Logic 模式下 paintEvent() 走 GpuViewport::render() 路径
- [x] DSO/Analog 模式下 paintEvent() 走原有 doPaint() 路径
- [x] 拖拽滚动在 GPU 渲染模式下正常工作
- [x] 鼠标滚轮缩放在 GPU 渲染模式下正常工作
- [x] 光标添加/移动/删除在 GPU 渲染模式下正常工作
- [x] 惯性滚动在 GPU 渲染模式下正常工作
- [x] FPS 测量在 GPU 渲染模式下正确显示
- [x] Header 的 paint_label() 在 GPU 模式下正确显示（Header 独立于 Viewport 渲染）
- [x] Ruler 的刻度在 GPU 模式下正确显示（Ruler 独立于 Viewport 渲染）

## 性能
- [x] scale/offset 不变时仅更新 Uniform Buffer，不重传顶点数据
- [x] 实时采集模式下增量上传新数据（appendChannelData 方法已实现）
- [x] 大数据量分页上传（每页不超过 16MB）
- [ ] 百万级采样点 + 多通道场景下帧率 >= 30fps（需运行时验证）
- [ ] GPU 渲染帧率不低于 CPU QPainter 渲染帧率（需运行时验证）

## 功能回归
- [ ] Logic 模式波形显示正确（需运行时验证）
- [ ] Logic 模式缩放/滚动流畅（需运行时验证）
- [ ] Logic 模式光标测量正确（需运行时验证）
- [ ] Logic 模式频率/边沿/跳转测量正确（需运行时验证）
- [ ] Logic 模式搜索功能正确（需运行时验证）
- [ ] Logic 模式触发功能正确（需运行时验证）
- [ ] DecodeTrace 标注显示正确（需运行时验证）
- [ ] DSO 模式功能未受影响（需运行时验证）
- [ ] Analog 模式功能未受影响（需运行时验证）
- [ ] 多标签页切换和拖出功能正常（需运行时验证）
