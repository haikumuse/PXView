# SamplingBar 与 DeviceOptionsDock 集成设计文档

## 1. 概述
本文档总结了 `SamplingBar` 设置项（采样率、深度、设备选择、采集模式）集成到侧边栏 `DeviceOptionsDock` 的实现逻辑。该设计旨在提供一个统一、整洁且具备良好交互体验的硬件配置界面。

## 2. 核心架构
### 2.1 控件供应
- **SamplingBar**：负责创建设置小部件。通过 `createSamplingSettingsWidget(QWidget *parent)` 方法生成一个包含网格布局（QGridLayout）的容器。
- **UI 约束**：为保证专业外观，下拉框（DsComboBox）被强制设置为固定宽度 `200px`，并配合布局列宽约束（`setColumnMinimumWidth`）。

### 2.2 容器承载
- **DeviceOptionsDock**：作为主容器，负责管理采样设置和设备特定参数。
- **增量更新**：新增了 `update_widgets_status()` 方法，用于在不销毁 UI 的情况下更新启用/禁用状态。

### 2.3 生命周期管理
- **MainWindow**：在 `setup_ui` 阶段完成控件创建、嵌套和初始绑定。
- **SlidingDrawer**：提供平滑的抽屉式弹出交互。
- **Scroll Area**：通过外部 `QScrollArea` 包装，防止在侧边栏空间受限时出现布局挤压。

## 3. 关键问题与解决方案

### 3.1 采样开始时的自动滚动跳动
- **现象**：开始采样时，Dock 会自动滚动到底部。
- **原因**：原始逻辑触发了 `DSV_MSG_DEVICE_OPTIONS_UPDATED` 消息，导致 Dock 全量销毁并重建。
- **方案**：
    - 引入新消息 `DSV_MSG_CAPTURE_STATE_CHANGED`。
    - 采样开始时仅通过 `update_widgets_status()` 禁用控件，**不再销毁布局**。
- **结果**：滚动条位置锁定，用户体验平滑。

### 3.2 初始渲染时的宽度过窄
- **现象**：首次打开时，下拉框显示非常窄，切换标签后才恢复正常。
- **原因**：Qt 布局在初始化时未获得准确的尺寸建议，导致自适应策略将其压缩。
- **方案**：
    - 强制使用 `setFixedWidth(200)`。
    - 在 `bind_context` 时显式调用 `adjustSize()` 触发几何计算。
- **结果**：初始化即显示标准的 200px 宽度。

### 3.3 标签页切换后的参数丢失
- **现象**：在不同采样率的标签页间切换时，数值无法同步，或首个标签页不保存设置。
- **原因**：绑定时机滞后且仅在 `unbind` 时保存，导致数据同步不及时。
- **方案**：
    - **即时保存**：在采样率、深度、模式的所有 `on_change` 回调中增加对 `SessionDocument` 的实时更新。
    - **启动绑定**：修复了 `setup_ui` 中首个标签页缺失 `bind_context` 的 Bug。
- **结果**：参数随标签页即时跳变，逻辑严丝合缝。

## 4. 数据流图
1. **交互层**：用户修改 UI -> 触发 `on_change` -> 同时更新 `DeviceAgent` 硬件状态和 `SessionDocument` 持久化状态。
2. **消息层**：`SigSession` 状态改变 -> 发送轻量级消息 -> `MainWindow` 转发 -> `Dock` 更新启用状态。
3. **恢复层**：切换标签页 -> `MainWindow` 调用 `bind_context` -> 从 `SessionDocument` 读取值 -> 阻塞信号并回填 UI -> 刷新布局。
