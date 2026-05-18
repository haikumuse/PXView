# 渲染模式用户可切换选项 Spec

## Why
PXView 已实现双渲染管线（CPU QPainter + GPU QRhi），但 GPU 模式仅由工作模式硬编码控制（LOGIC 模式自动启用），用户无法自主选择渲染后端。当 GPU 驱动不兼容或出现渲染异常时，用户无法手动回退到软件渲染；反之，在 DSO/Analog 模式下用户也无法主动尝试 GPU 加速。需要提供一个用户可切换的渲染模式选项。

## What Changes
- 在 `AppOptions` 中新增 `renderMode` 字段（0=自动, 1=强制软件渲染, 2=强制GPU渲染）
- 在 `ApplicationParamDlg` 设置对话框中新增渲染模式选择 UI
- 修改 `View::mode_changed()` 逻辑，读取用户渲染模式配置而非硬编码 `mode == LOGIC`
- 修改 `main.cpp` 中 `QT_WIDGETS_RHI` 环境变量设置逻辑，根据用户配置决定
- 新增 GPU 可用性检测，GPU 不可用时自动降级到软件渲染并提示用户
- 新增 `DSV_MSG_RENDER_MODE_CHANGED` 消息，支持运行时切换渲染模式
- 修改 `Viewport::set_gpu_mode()` 和 `Header::set_gpu_mode()`，增加 GPU 可用性检查

## Impact
- Affected specs: implement-gpu-logic-mode（GPU 模式激活逻辑将改为配置驱动）
- Affected code:
  - `PXView/pv/config/appconfig.h/cpp` — 新增 renderMode 字段和读写
  - `PXView/pv/dialogs/applicationpardlg.h/cpp` — 新增渲染模式 UI
  - `PXView/pv/view/view.h/cpp` — mode_changed() 改为配置驱动
  - `PXView/pv/view/viewport.h/cpp` — set_gpu_mode() 增加 GPU 可用性检查
  - `PXView/pv/view/header.h/cpp` — set_gpu_mode() 增加 GPU 可用性检查
  - `PXView/pv/view/gpuviewport.h/cpp` — 新增 GPU 可用性检测静态方法
  - `PXView/pv/interface/icallbacks.h` — 新增 DSV_MSG_RENDER_MODE_CHANGED 消息
  - `PXView/main.cpp` — QT_WIDGETS_RHI 根据配置决定
  - `PXView/pv/mainwindow.h/cpp` — 处理 DSV_MSG_RENDER_MODE_CHANGED 消息

## ADDED Requirements

### Requirement: 渲染模式配置持久化
系统 SHALL 在 AppConfig 的 AppOptions 中持久化用户的渲染模式偏好。

#### Scenario: 渲染模式枚举值
- **WHEN** 系统定义渲染模式枚举
- **THEN** 包含三个值：`RenderAuto = 0`（自动，根据工作模式决定）、`RenderSoftware = 1`（强制软件渲染）、`RenderGPU = 2`（强制GPU渲染）

#### Scenario: 默认值
- **WHEN** 用户首次启动应用或配置文件中无 renderMode 字段
- **THEN** renderMode 默认为 `RenderAuto`（0），行为与当前硬编码逻辑一致

#### Scenario: 配置读写
- **WHEN** 用户修改渲染模式设置
- **THEN** 新值通过 AppConfig 的延迟保存机制持久化到 QSettings，应用重启后保持用户选择

### Requirement: 渲染模式设置 UI
系统 SHALL 在 ApplicationParamDlg 设置对话框中提供渲染模式选择控件。

#### Scenario: UI 控件位置
- **WHEN** 用户打开"Display options"设置对话框
- **THEN** 在现有三个分组（Logic、Scope、UI）之外新增"Render"（渲染）分组，包含渲染模式下拉框

#### Scenario: 下拉框选项
- **WHEN** 渲染模式下拉框展开
- **THEN** 显示三个选项："Auto"（自动）、"Software"（软件渲染）、"GPU Acceleration"（GPU加速）

#### Scenario: 当前值显示
- **WHEN** 设置对话框打开
- **THEN** 下拉框显示当前 AppConfig 中 renderMode 的值

#### Scenario: 保存并应用
- **WHEN** 用户修改渲染模式并确认
- **THEN** 配置被保存，DSV_MSG_RENDER_MODE_CHANGED 消息被广播，所有打开的标签页立即应用新的渲染模式

### Requirement: 渲染模式逻辑
系统 SHALL 根据用户选择的渲染模式和工作模式决定是否启用 GPU 渲染。

#### Scenario: Auto 模式
- **WHEN** renderMode 为 RenderAuto
- **THEN** 行为与当前逻辑一致：LOGIC 模式启用 GPU，DSO/Analog 模式使用 CPU QPainter

#### Scenario: 强制软件渲染
- **WHEN** renderMode 为 RenderSoftware
- **THEN** 所有工作模式（LOGIC/DSO/Analog）均使用 CPU QPainter 渲染，GpuViewport/GpuHeader 被隐藏

#### Scenario: 强制 GPU 渲染
- **WHEN** renderMode 为 RenderGPU
- **THEN** 所有工作模式均尝试启用 GPU 渲染；若 GPU 不可用则自动降级到软件渲染并提示用户

#### Scenario: 模式切换即时生效
- **WHEN** 用户在运行时切换渲染模式
- **THEN** 所有已打开的 Viewport 和 Header 立即应用新的渲染模式，无需重启应用

### Requirement: GPU 可用性检测
系统 SHALL 在启用 GPU 渲染前检测 GPU 是否可用。

#### Scenario: GPU 不可用检测
- **WHEN** 用户选择 GPU 渲染模式或 Auto 模式下工作模式为 LOGIC
- **THEN** 系统检查 QRhi 是否成功初始化（GpuViewport 的 initialize() 是否被调用且 _resourcesValid 为 true）

#### Scenario: GPU 不可用降级
- **WHEN** GPU 初始化失败或 RHI 后端不可用
- **THEN** 系统自动回退到软件渲染，并在状态栏或日志中提示用户"GPU 不可用，已切换到软件渲染"

#### Scenario: 强制 GPU 模式下 GPU 不可用
- **WHEN** renderMode 为 RenderGPU 但 GPU 不可用
- **THEN** 系统回退到软件渲染，在设置对话框的渲染模式下拉框旁显示警告图标和提示文字"GPU not available"

### Requirement: QT_WIDGETS_RHI 环境变量控制
系统 SHALL 根据用户渲染模式配置控制 QT_WIDGETS_RHI 环境变量。

#### Scenario: Auto 或 GPU 模式
- **WHEN** renderMode 为 RenderAuto 或 RenderGPU
- **THEN** main.cpp 中设置 `QT_WIDGETS_RHI=1`，启用 Qt Widgets 的 RHI 后端

#### Scenario: 强制软件渲染
- **WHEN** renderMode 为 RenderSoftware
- **THEN** main.cpp 中不设置 `QT_WIDGETS_RHI`（或设为 "0"），禁用 Qt Widgets 的 RHI 后端

#### Scenario: 环境变量优先
- **WHEN** 用户在系统环境中已设置 QT_WIDGETS_RHI
- **THEN** 系统环境变量的值优先，应用配置不覆盖

### Requirement: 渲染模式变更消息
系统 SHALL 在渲染模式变更时广播消息通知所有组件。

#### Scenario: 消息定义
- **WHEN** 定义新的消息类型
- **THEN** 在 icallbacks.h 的 DSV_MSG 枚举中新增 `DSV_MSG_RENDER_MODE_CHANGED`

#### Scenario: 消息广播
- **WHEN** 用户在设置对话框中修改渲染模式并确认
- **THEN** ApplicationParamDlg 广播 DSV_MSG_RENDER_MODE_CHANGED 消息

#### Scenario: 消息处理
- **WHEN** MainWindow 收到 DSV_MSG_RENDER_MODE_CHANGED 消息
- **THEN** 遍历所有 TabContext，对每个活跃的 View 调用其渲染模式更新方法

## MODIFIED Requirements

### Requirement: Viewport 渲染调度
原要求：Viewport 在 Logic 模式下使用 QRhiWidget + GPU 渲染管线，在 DSO/Analog 模式下使用 QWidget + QPainter 渲染。模式切换时动态替换 Viewport 内部渲染表面。
修改为：Viewport 根据 AppConfig.renderMode 和工作模式共同决定渲染路径。RenderAuto 时行为不变；RenderSoftware 时所有模式走 QPainter；RenderGPU 时所有模式尝试走 QRhiWidget（GPU 不可用时降级）。用户可在运行时通过设置对话框切换。

### Requirement: GPU 模式激活条件
原要求：GPU 模式仅在 LOGIC 工作模式下自动启用，由 View::mode_changed() 中 `mode == LOGIC` 硬编码控制。
修改为：GPU 模式由 AppConfig.renderMode 和工作模式共同决定。View::mode_changed() 读取 AppConfig.renderMode，结合当前工作模式计算是否启用 GPU。

## REMOVED Requirements

无移除的需求。
