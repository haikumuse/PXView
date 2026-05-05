# 设备选项侧边栏化 Spec

## Why
当前设备选项（DeviceOptions）以模态对话框形式呈现，用户点击"Options"按钮后弹出对话框阻塞主窗口，无法在调整设备参数的同时观察波形变化。将其改为右侧 QDockWidget 侧边栏，用户可以在不关闭配置面板的情况下实时查看参数变更效果，提升操作效率和交互体验。

## What Changes
- 新建 `pv/dock/deviceoptionsdock.h/.cpp`，创建 `dock::DeviceOptionsDock` 类（继承 `QScrollArea` + `IUiWindow`），将 `dialogs::DeviceOptions` 的 UI 逻辑迁移到该类中
- 在 `MainWindow` 中创建 `QDockWidget` 容器并添加 `DeviceOptionsDock`，停靠在右侧区域
- 修改 `SamplingBar::on_configure()`：不再创建模态对话框，改为发射信号通知 MainWindow 切换 Dock 显隐
- 扩展 `DockOptions` 结构体，新增 `deviceOptionsDock` 字段，支持持久化
- 扩展 `TrigBar`，新增第 5 个切换按钮 `_device_options_button`，与其他 4 个按钮互斥
- 将原对话框的 `accept()` 提交逻辑拆分：设备属性即时 commit，通道/探针配置通过"应用"按钮统一提交
- 保留原 `pv/dialogs/deviceoptions.h/.cpp` 文件不删除，作为回退备选
- **BREAKING**: `SamplingBar::on_configure()` 不再创建模态对话框，调用方需适配新的信号机制

## Impact
- Affected specs: 无已有 spec
- Affected code:
  - `DSView/pv/dock/deviceoptionsdock.h`（新建：侧边栏内容 Widget 头文件）
  - `DSView/pv/dock/deviceoptionsdock.cpp`（新建：侧边栏内容 Widget 实现）
  - `DSView/pv/mainwindow.h`（修改：新增 Dock 成员变量和槽函数声明）
  - `DSView/pv/mainwindow.cpp`（修改：创建 Dock、连接信号、实现槽函数）
  - `DSView/pv/toolbars/samplingbar.h`（修改：新增信号声明）
  - `DSView/pv/toolbars/samplingbar.cpp`（修改：重写 on_configure 逻辑）
  - `DSView/pv/toolbars/trigbar.h`（修改：新增按钮和信号声明）
  - `DSView/pv/toolbars/trigbar.cpp`（修改：新增按钮点击处理和互斥逻辑）
  - `DSView/pv/config/appconfig.h`（修改：DockOptions 新增字段）
  - `DSView/pv/config/appconfig.cpp`（修改：新增字段初始化）
  - `DSView/pv/dialogs/deviceoptions.h`（不修改，保留）
  - `DSView/pv/dialogs/deviceoptions.cpp`（不修改，保留）
  - `DSView/CMakeLists.txt`（修改：新增源文件）

## ADDED Requirements

### Requirement: 设备选项侧边栏面板
系统 SHALL 提供一个 QDockWidget 侧边栏面板，用于显示和编辑设备选项，替代原有的模态对话框。

#### Scenario: 点击 Options 按钮打开侧边栏
- **WHEN** 用户点击右侧工具栏的 Options 按钮
- **THEN** 设备选项 Dock 在右侧区域显示
- **AND** 其他已打开的 Dock 面板自动关闭（互斥）
- **AND** Options 按钮呈现选中（checked）状态

#### Scenario: 再次点击 Options 按钮关闭侧边栏
- **WHEN** 用户再次点击已选中的 Options 按钮
- **THEN** 设备选项 Dock 隐藏
- **AND** Options 按钮取消选中状态

#### Scenario: 点击其他 Dock 按钮关闭设备选项
- **WHEN** 设备选项 Dock 可见时，用户点击 Trigger/Protocol/Measure/Search 按钮
- **THEN** 设备选项 Dock 自动隐藏
- **AND** 对应的 Dock 面板显示

### Requirement: 设备选项侧边栏内容
DeviceOptionsDock SHALL 包含与原模态对话框相同的所有功能区域。

#### Scenario: Mode 属性区域
- **WHEN** DeviceOptionsDock 可见
- **THEN** 面板顶部显示 "Mode" 分组框，包含所有设备属性控件（采样率、工作模式、缓冲选项等）
- **AND** 属性控件布局与原对话框一致（QGridLayout：标签 + 控件）

#### Scenario: Channel 通道区域（LOGIC 模式）
- **WHEN** DeviceOptionsDock 可见且设备工作在 LOGIC 模式
- **THEN** 面板下方显示 "Channel" 分组框
- **AND** 包含通道模式单选按钮、通道复选框网格、Enable All/Disable All 按钮
- **AND** 功能与原对话框完全一致

#### Scenario: Calibration 校准区域（DSO 模式）
- **WHEN** DeviceOptionsDock 可见且设备工作在 DSO 模式
- **THEN** 面板下方显示 "Calibration" 分组框
- **AND** 包含 Auto Calibration 和 Manual Calibration 按钮
- **AND** 功能与原对话框完全一致

#### Scenario: Channel 通道区域（ANALOG 模式）
- **WHEN** DeviceOptionsDock 可见且设备工作在 ANALOG 模式
- **THEN** 面板下方显示 "Channel" 分组框
- **AND** 包含通道标签页，每页有 Enable 复选框和探针属性编辑器
- **AND** 功能与原对话框完全一致

### Requirement: 应用按钮提交通道配置
DeviceOptionsDock SHALL 提供"应用"按钮，用于统一提交通道启用/禁用状态和探针选项。

#### Scenario: 点击应用按钮
- **WHEN** 用户点击"应用"按钮
- **THEN** 系统提交所有设备属性（`p->commit()`）
- **THEN** 系统提交通道启用状态（`probe->enabled = checkbox->isChecked()`）
- **THEN** 系统提交探针选项（`probe_props->commit()`）
- **AND** 校验至少有一个通道启用，否则弹出警告
- **AND** 广播 `DSV_MSG_DEVICE_OPTIONS_UPDATED` 消息
- **AND** 广播 `DSV_MSG_END_DEVICE_OPTIONS` 消息

#### Scenario: 无通道启用警告
- **WHEN** 用户点击"应用"按钮但所有通道均未启用
- **THEN** 弹出警告消息"All channel disabled! Please enable at least one channel."
- **AND** 不执行任何提交操作

### Requirement: 模式变化自动重建面板
DeviceOptionsDock SHALL 在设备工作模式变化时自动重建通道区域。

#### Scenario: 工作模式变化
- **WHEN** 设备工作模式发生变化（通过定时器检测或属性变更）
- **THEN** 通道区域（动态面板）被销毁并重建
- **AND** 面板内容与新工作模式匹配

#### Scenario: 定时器生命周期
- **WHEN** DeviceOptionsDock 可见
- **THEN** 模式检查定时器启动（100ms 间隔）
- **WHEN** DeviceOptionsDock 隐藏
- **THEN** 模式检查定时器停止

### Requirement: IUiWindow 接口实现
DeviceOptionsDock SHALL 实现 IUiWindow 接口，支持语言、主题和字体更新。

#### Scenario: 语言更新
- **WHEN** UiManager 广播语言更新
- **THEN** DeviceOptionsDock 更新所有标签文本为当前语言

#### Scenario: 主题更新
- **WHEN** UiManager 广播主题更新
- **THEN** DeviceOptionsDock 更新图标和样式

#### Scenario: 字体更新
- **WHEN** UiManager 广播字体更新
- **THEN** DeviceOptionsDock 更新所有控件的字体大小

### Requirement: Dock 状态持久化
设备选项 Dock 的开关状态 SHALL 持久化到 AppConfig 中。

#### Scenario: 程序重启后恢复状态
- **WHEN** 程序启动并加载配置
- **THEN** 根据 DockOptions.deviceOptionsDock 的值决定设备选项 Dock 是否显示

#### Scenario: 切换工作模式时恢复状态
- **WHEN** 设备工作模式切换
- **THEN** 根据新模式对应的 DockOptions 恢复设备选项 Dock 的显隐状态

### Requirement: 设备更新时重建面板
DeviceOptionsDock SHALL 在设备热插拔或切换时重建整个面板内容。

#### Scenario: 设备更新
- **WHEN** 接收到 device_updated 信号
- **THEN** DeviceOptionsDock 重新初始化属性绑定和通道面板

## MODIFIED Requirements

### Requirement: SamplingBar.on_configure 行为
`SamplingBar::on_configure()` 从创建模态对话框改为发射信号通知 MainWindow 切换 Dock 显隐。

原有行为：
```
on_configure() → 创建 dialogs::DeviceOptions → dlg.exec() → accept后处理
```

新行为：
```
on_configure() → emit sig_device_options_toggle() → MainWindow 切换 Dock 显隐
```

### Requirement: DockOptions 结构体
`DockOptions` 新增 `deviceOptionsDock` 布尔字段，用于持久化设备选项 Dock 的开关状态。

原有结构：
```cpp
struct DockOptions {
    bool decodeDock;
    bool triggerDock;
    bool measureDock;
    bool searchDock;
};
```

新结构：
```cpp
struct DockOptions {
    bool decodeDock;
    bool triggerDock;
    bool measureDock;
    bool searchDock;
    bool deviceOptionsDock;
};
```

### Requirement: TrigBar 互斥切换逻辑
TrigBar 的互斥切换逻辑扩展为 5 个按钮，新增 `_device_options_button` 与其他按钮互斥。

## REMOVED Requirements

### Requirement: 模态对话框阻塞主窗口
**Reason**: 设备选项改为侧边栏后，不再需要模态阻塞主窗口。
**Migration**: `dialogs::DeviceOptions` 文件保留不删除，但不再被 `SamplingBar::on_configure()` 调用。`config_device()` 公开方法行为同步修改。
