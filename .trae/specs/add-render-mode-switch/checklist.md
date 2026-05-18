# Checklist

## AppConfig 渲染模式配置
- [x] AppOptions 结构体包含 renderMode 字段（int 类型，默认值 0）
- [x] appconfig.h 定义了 RENDER_MODE_AUTO=0, RENDER_MODE_SOFTWARE=1, RENDER_MODE_GPU=2 枚举常量
- [x] LoadApp() 正确读取 renderMode 配置
- [x] SaveApp() 正确写入 renderMode 配置
- [x] 首次启动（无配置文件）时 renderMode 默认为 0（Auto）

## 消息定义
- [x] icallbacks.h 中 DSV_MSG 枚举包含 DSV_MSG_RENDER_MODE_CHANGED

## GPU 可用性检测
- [x] GpuViewport 提供静态方法检测 GPU 是否可用
- [x] GPU 不可用时 set_gpu_mode(true) 不启用 GPU 并记录日志

## 渲染模式逻辑
- [x] View::updateRenderMode() 根据 renderMode + 工作模式正确计算 GPU 启用状态
- [x] Auto 模式：LOGIC→GPU, DSO/Analog→CPU（与原行为一致）
- [x] Software 模式：所有模式均使用 CPU QPainter
- [x] GPU 模式：所有模式尝试 GPU 渲染，不可用时降级
- [x] mode_changed() 调用 updateRenderMode() 而非硬编码 mode == LOGIC

## 设置对话框 UI
- [x] ApplicationParamDlg 包含"Render"分组和渲染模式下拉框
- [x] 下拉框有三个选项：Auto / Software / GPU Acceleration
- [x] 打开对话框时下拉框显示当前 renderMode 值
- [x] 修改渲染模式后保存到 AppConfig 并广播 DSV_MSG_RENDER_MODE_CHANGED
- [x] GPU 模式下 GPU 不可用时显示警告提示

## MainWindow 消息处理
- [x] MainWindow 处理 DSV_MSG_RENDER_MODE_CHANGED 消息
- [x] 收到消息后所有活跃 View 的渲染模式被更新

## main.cpp 环境变量
- [x] RenderSoftware 模式下 QT_WIDGETS_RHI 不被设置（或设为"0"）
- [x] Auto/GPU 模式下 QT_WIDGETS_RHI 设为"1"
- [x] 用户预设的 QT_WIDGETS_RHI 环境变量不被覆盖

## 运行时行为
- [x] 运行时切换渲染模式即时生效，无需重启
- [x] 配置持久化，重启应用后保持用户选择
- [x] GPU 降级时用户得到提示（日志或状态栏）
