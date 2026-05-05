# UI 调试悬停组件 Spec

## Why
DSView 项目在开发和调试时，缺乏快速识别 UI 元素来源的能力。开发者需要反复翻找代码才能确认某个控件属于哪个类、objectName 是什么、在 widget 层级中的位置。一个鼠标悬停即可显示调试信息的组件能大幅提升开发效率。

## What Changes
- 新增 `pv/ui/debughelper.h` 和 `pv/ui/debughelper.cpp`，实现 `DebugHelper` 类
- `DebugHelper` 通过全局事件过滤器拦截 `MouseMove` 事件，获取鼠标下方的 QWidget 调试信息
- 以浮动 QLabel 形式显示调试信息面板（类名、objectName、父级链路、几何信息等）
- 通过快捷键 `Ctrl+Shift+D` 切换调试模式开关
- 在 `MainWindow` 中集成 `DebugHelper`，在构造时创建实例并注册快捷键
- 在 `CMakeLists.txt` 中添加新源文件

## Impact
- Affected specs: 无已有 spec
- Affected code:
  - `DSView/pv/ui/debughelper.h`（新增）
  - `DSView/pv/ui/debughelper.cpp`（新增）
  - `DSView/pv/mainwindow.h`（添加 DebugHelper 成员和快捷键声明）
  - `DSView/pv/mainwindow.cpp`（构造函数中初始化 DebugHelper，注册快捷键）
  - `CMakeLists.txt`（添加新源文件）

## ADDED Requirements

### Requirement: 调试悬停信息面板
系统 SHALL 提供一个可切换的调试模式，当调试模式开启时，鼠标悬停在任意 UI 元素上，自动显示该元素的调试信息面板。

#### Scenario: 开启调试模式
- **WHEN** 用户按下快捷键 `Ctrl+Shift+D`
- **AND** 调试模式当前为关闭状态
- **THEN** 调试模式开启，状态栏或控制台输出提示 "Debug Helper: ON"

#### Scenario: 关闭调试模式
- **WHEN** 用户按下快捷键 `Ctrl+Shift+D`
- **AND** 调试模式当前为开启状态
- **THEN** 调试模式关闭，调试信息面板隐藏，状态栏或控制台输出提示 "Debug Helper: OFF"

#### Scenario: 鼠标悬停显示调试信息
- **WHEN** 调试模式开启
- **AND** 用户将鼠标移动到某个 QWidget 上
- **THEN** 在鼠标附近显示一个浮动信息面板，包含以下信息：
  - 类名（含完整 C++ 类名，如 `pv::toolbars::SamplingBar`）
  - objectName（若为空则显示 `<unnamed>`）
  - 父级链路（从当前 widget 到顶层窗口的完整路径，格式：`ClassName(objectName) -> ClassName(objectName) -> ...`）
  - 几何信息（x, y, width, height）
  - 是否可见（visible）

#### Scenario: 鼠标移出控件
- **WHEN** 调试模式开启
- **AND** 鼠标移出当前控件（进入另一个控件或离开窗口）
- **THEN** 信息面板更新为新控件的调试信息

#### Scenario: 调试模式关闭时无干扰
- **WHEN** 调试模式关闭
- **THEN** 鼠标悬停行为与未安装调试组件时完全一致，无额外开销

### Requirement: 调试信息面板样式
调试信息面板 SHALL 使用半透明深色背景、等宽字体，与项目现有 dark/light 主题兼容。

#### Scenario: 面板视觉样式
- **WHEN** 调试信息面板显示
- **THEN** 面板具有以下视觉特征：
  - 深色半透明背景（rgba(30, 30, 30, 220)）
  - 浅色文字（#E0E0E0）
  - 等宽字体显示
  - 圆角边框
  - 面板不接收鼠标事件（`Qt::ToolTip` 窗口标志），不影响底层控件的交互

### Requirement: 高亮当前悬停控件
调试模式开启时，SHALL 在当前悬停控件周围绘制高亮边框，使开发者直观看到控件边界。

#### Scenario: 控件高亮显示
- **WHEN** 调试模式开启
- **AND** 鼠标悬停在某个控件上
- **THEN** 该控件周围显示一个 2px 的红色半透明边框，标识控件边界

#### Scenario: 鼠标移走后高亮消失
- **WHEN** 鼠标离开当前控件
- **THEN** 该控件的高亮边框消失
