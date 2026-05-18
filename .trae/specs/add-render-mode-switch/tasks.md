# Tasks

- [x] Task 1: AppConfig 新增 renderMode 字段
  - [x] SubTask 1.1: 在 appconfig.h 的 AppOptions 结构体中新增 `int renderMode` 字段（0=Auto, 1=Software, 2=GPU），默认值为 0
  - [x] SubTask 1.2: 在 appconfig.cpp 的 LoadApp() 中读取 renderMode 配置，SaveApp() 中写入 renderMode 配置
  - [x] SubTask 1.3: 在 appconfig.h 中定义 RenderMode 枚举常量（RENDER_MODE_AUTO=0, RENDER_MODE_SOFTWARE=1, RENDER_MODE_GPU=2）

- [x] Task 2: 新增 DSV_MSG_RENDER_MODE_CHANGED 消息
  - [x] SubTask 2.1: 在 icallbacks.h 的 DSV_MSG 枚举中新增 `DSV_MSG_RENDER_MODE_CHANGED` 消息码

- [x] Task 3: GpuViewport 新增 GPU 可用性检测
  - [x] SubTask 3.1: 在 GpuViewport 中新增静态方法 `isGpuAvailable()`，返回 QRhi 是否可用（检查 RHI 后端是否为 Software 后端）
  - [x] SubTask 3.2: 在 GpuViewport 中新增成员方法 `isInitialized()` 返回 `_resourcesValid` 状态

- [x] Task 4: 修改 View::mode_changed() 为配置驱动
  - [x] SubTask 4.1: 在 View 中新增方法 `updateRenderMode()`，根据 AppConfig.renderMode 和当前工作模式计算是否启用 GPU
  - [x] SubTask 4.2: 修改 mode_changed() 调用 updateRenderMode() 替代硬编码 `mode == LOGIC`
  - [x] SubTask 4.3: updateRenderMode() 逻辑：RenderAuto→LOGIC时启用GPU; RenderSoftware→始终禁用; RenderGPU→始终启用（不可用时降级）

- [x] Task 5: 修改 Viewport/Header 的 set_gpu_mode 增加 GPU 可用性检查
  - [x] SubTask 5.1: 在 Viewport::set_gpu_mode() 中，当 enabled=true 时先检查 GpuViewport::isGpuAvailable()，不可用则不启用并记录日志
  - [x] SubTask 5.2: 在 Header::set_gpu_mode() 中，当 enabled=true 时先检查 GpuHeader 的 GPU 可用性，不可用则不启用并记录日志

- [x] Task 6: ApplicationParamDlg 新增渲染模式 UI
  - [x] SubTask 6.1: 在 ApplicationParamDlg 中新增"Render"分组，包含 QComboBox 渲染模式下拉框（Auto/Software/GPU）
  - [x] SubTask 6.2: 对话框打开时从 AppConfig 读取当前 renderMode 并设置下拉框当前值
  - [x] SubTask 6.3: 用户确认时检测 renderMode 是否变化，若变化则保存到 AppConfig 并广播 DSV_MSG_RENDER_MODE_CHANGED
  - [x] SubTask 6.4: 当 renderMode 为 RenderGPU 且 GPU 不可用时，在下拉框旁显示警告标签

- [x] Task 7: MainWindow 处理 DSV_MSG_RENDER_MODE_CHANGED
  - [x] SubTask 7.1: 在 MainWindow 的消息处理中新增 DSV_MSG_RENDER_MODE_CHANGED 分支
  - [x] SubTask 7.2: 收到消息后遍历所有 TabContext，对每个活跃的 View 调用 updateRenderMode()

- [x] Task 8: 修改 main.cpp 的 QT_WIDGETS_RHI 逻辑
  - [x] SubTask 8.1: 在 main.cpp 中根据 AppConfig.renderMode 决定是否设置 QT_WIDGETS_RHI 环境变量（RenderSoftware 时不设置或设为"0"）
  - [x] SubTask 8.2: 保留环境变量优先逻辑（用户预设的环境变量不被覆盖）

- [x] Task 9: 编译验证与功能测试
  - [x] SubTask 9.1: 增量编译通过
  - [x] SubTask 9.2: 验证 Auto 模式行为与修改前一致（LOGIC→GPU, DSO/Analog→CPU）
  - [x] SubTask 9.3: 验证 Software 模式下所有模式均使用 QPainter 渲染
  - [x] SubTask 9.4: 验证 GPU 模式下 LOGIC/DSO/Analog 均尝试 GPU 渲染
  - [x] SubTask 9.5: 验证运行时切换渲染模式即时生效
  - [x] SubTask 9.6: 验证配置持久化（重启应用后保持用户选择）

# Task Dependencies
- Task 1 → Task 4 (配置字段就绪后才能修改 View 逻辑)
- Task 1 → Task 6 (配置字段就绪后才能添加 UI)
- Task 2 → Task 6 (消息定义后 UI 才能广播)
- Task 2 → Task 7 (消息定义后 MainWindow 才能处理)
- Task 3 → Task 5 (GPU 检测方法就绪后才能在 set_gpu_mode 中使用)
- Task 4 + Task 5 → Task 7 (渲染逻辑和 GPU 检查就绪后 MainWindow 才能触发更新)
- Task 1 → Task 8 (配置字段就绪后 main.cpp 才能读取)
- Task 1, Task 2, Task 3 可并行
- Task 6, Task 7, Task 8 依赖前置任务，但三者之间可并行
