# 多标签会话（Session Tabs）Spec

## Why
DSView 当前为严格单会话单视图架构，每次打开文件或重新采集都会替换当前内容，用户无法同时查看和对比多次采集的历史数据。参考行业标杆 Saleae Logic 2 的 Session Tabs 设计，实现多标签页功能，使用户可以在多个标签间切换浏览不同采集数据，并支持将标签拖拽为独立窗口以实现多屏对比。

## What Changes
- 新增 `pv/data/sessionsnapshot.h` 和 `pv/data/sessionsnapshot.cpp`，实现 `SessionSnapshot` 类，封装可独立存在的会话数据快照
- 新增 `pv/tabcontext.h` 和 `pv/tabcontext.cpp`，实现 `TabContext` 类，封装每个标签页的完整上下文（View + SessionSnapshot + 元信息）
- 新增 `pv/ui/draggabletabwidget.h` 和 `pv/ui/draggabletabwidget.cpp`，实现 `DraggableTabWidget` 类，支持标签拖出为独立窗口和还原
- 新增 `pv/ui/draggabletabbar.h` 和 `pv/ui/draggabletabbar.cpp`，实现 `DraggableTabBar` 类，捕获拖拽事件实现标签拖出逻辑
- 修改 `MainWindow`：将 `_vertical_layout` 中的唯一 `_view` 替换为 `DraggableTabWidget`，管理多个 `TabContext`
- 修改 `View`：增加数据源切换能力，支持从 `SigSession`（实时采集）或 `SessionSnapshot`（历史数据）获取数据
- 修改 `Dock` 窗口（MeasureDock、ProtocolDock、SearchDock、TriggerDock 等）：增加 `set_view()` 方法，支持运行时切换绑定的 View
- 修改 `SamplingBar`：增加上下文切换能力，标签切换时更新设备选择和采样参数
- 修改 `SigSession`：增加 `capture_snapshot()` 方法，用于在标签切换时拍摄当前数据快照
- 修改 `FileBar`：打开文件操作创建新标签页而非替换当前内容
- 修改 `CMakeLists.txt`：添加新源文件
- **BREAKING**: `MainWindow::_view` 将不再是唯一的 View 实例，所有直接引用 `_view` 的代码需改为通过当前活跃 TabContext 获取

## Impact
- Affected specs: 无已有 spec
- Affected code:
  - `DSView/pv/data/sessionsnapshot.h`（新增）
  - `DSView/pv/data/sessionsnapshot.cpp`（新增）
  - `DSView/pv/tabcontext.h`（新增）
  - `DSView/pv/tabcontext.cpp`（新增）
  - `DSView/pv/ui/draggabletabwidget.h`（新增）
  - `DSView/pv/ui/draggabletabwidget.cpp`（新增）
  - `DSView/pv/ui/draggabletabbar.h`（新增）
  - `DSView/pv/ui/draggabletabbar.cpp`（新增）
  - `DSView/pv/mainwindow.h`（修改：替换 _view 为 DraggableTabWidget，新增 TabContext 管理方法）
  - `DSView/pv/mainwindow.cpp`（修改：重写 setup_ui，实现标签管理逻辑）
  - `DSView/pv/view/view.h`（修改：增加数据源切换接口）
  - `DSView/pv/view/view.cpp`（修改：实现数据源切换逻辑）
  - `DSView/pv/dock/measuredock.h`（修改：增加 set_view 方法）
  - `DSView/pv/dock/measuredock.cpp`（修改：实现 set_view 逻辑）
  - `DSView/pv/dock/protocoldock.h`（修改：增加 set_view 方法）
  - `DSView/pv/dock/protocoldock.cpp`（修改：实现 set_view 逻辑）
  - `DSView/pv/dock/searchdock.h`（修改：增加 set_view 方法）
  - `DSView/pv/dock/searchdock.cpp`（修改：实现 set_view 逻辑）
  - `DSView/pv/dock/triggerdock.h`（修改：增加 set_view 方法）
  - `DSView/pv/dock/triggerdock.cpp`（修改：实现 set_view 逻辑）
  - `DSView/pv/dock/dsotriggerdock.h`（修改：增加 set_view 方法）
  - `DSView/pv/dock/dsotriggerdock.cpp`（修改：实现 set_view 逻辑）
  - `DSView/pv/toolbars/samplingbar.h`（修改：增加上下文切换接口）
  - `DSView/pv/toolbars/samplingbar.cpp`（修改：实现上下文切换逻辑）
  - `DSView/pv/toolbars/filebar.h`（修改：打开文件信号增加创建新标签语义）
  - `DSView/pv/toolbars/filebar.cpp`（修改：调整文件打开逻辑）
  - `DSView/pv/sigsession.h`（修改：增加 capture_snapshot 方法）
  - `DSView/pv/sigsession.cpp`（修改：实现快照拍摄逻辑）
  - `CMakeLists.txt`（修改：添加新源文件）

## ADDED Requirements

### Requirement: SessionSnapshot 数据快照
系统 SHALL 提供 `SessionSnapshot` 类，能够从 `SigSession` 中提取可独立存在的会话数据快照，使非活跃标签可以独立浏览历史数据。

#### Scenario: 从活跃会话拍摄快照
- **WHEN** 用户切换到另一个标签页
- **AND** 当前标签页关联着活跃的 SigSession（正在或已完成采集）
- **THEN** 系统自动调用 `SigSession::capture_snapshot()` 拍摄当前数据快照
- **AND** 快照包含：LogicSnapshot、AnalogSnapshot、DsoSnapshot 的完整数据副本
- **AND** 快照包含：采样率、采样深度、触发位置等元信息
- **AND** 快照包含：信号通道列表（Signal）的配置信息（名称、颜色、使能状态等）

#### Scenario: 从文件加载快照
- **WHEN** 用户打开一个 .dsl 数据文件
- **THEN** 系统创建一个新的 SessionSnapshot 并从文件中加载数据
- **AND** 该快照包含文件中的所有波形数据和解码数据

#### Scenario: 快照数据的独立性
- **WHEN** 一个标签页持有 SessionSnapshot
- **AND** 活跃的 SigSession 开始新的采集
- **THEN** 该标签页的快照数据不受新采集影响
- **AND** 该标签页的 View 可以正常浏览、缩放、测量快照数据

### Requirement: TabContext 标签上下文
系统 SHALL 提供 `TabContext` 类，封装每个标签页的完整上下文信息。

#### Scenario: TabContext 包含的元素
- **WHEN** 创建一个新的标签页
- **THEN** TabContext 包含以下元素：
  - `View* view`：该标签的波形视图实例
  - `SessionSnapshot* snapshot`：该标签的数据快照（可为 null，表示实时采集标签）
  - `QString title`：标签标题
  - `QString file_path`：关联的文件路径（可为空）
  - `bool is_live`：是否为活跃采集标签
  - `QDateTime timestamp`：创建时间

#### Scenario: 活跃采集标签
- **WHEN** 当前标签页是活跃采集标签（is_live = true）
- **THEN** 该标签的 View 直接绑定 SigSession，实时显示采集数据
- **AND** 该标签不持有独立的 SessionSnapshot（snapshot 为 null）

#### Scenario: 历史/文件标签
- **WHEN** 当前标签页是历史数据或文件数据标签（is_live = false）
- **THEN** 该标签的 View 绑定自身的 SessionSnapshot
- **AND** 该标签可以独立浏览、缩放、测量，不受 SigSession 状态影响

### Requirement: DraggableTabWidget 可拖拽标签组件
系统 SHALL 提供 `DraggableTabWidget` 类，继承自 `QTabWidget`，支持将标签页拖拽为独立浮动窗口，以及将浮动窗口还原回标签栏。

#### Scenario: 拖拽标签出窗口
- **WHEN** 用户按住标签栏中的某个标签并拖拽到标签栏区域之外
- **THEN** 该标签从 DraggableTabWidget 中移除
- **AND** 创建一个新的独立浮动窗口（DetachedWindow）
- **AND** 该标签的 View 被设置为浮动窗口的中央部件
- **AND** 浮动窗口的标题显示标签名称
- **AND** 浮动窗口可自由移动和缩放

#### Scenario: 关闭浮动窗口还原标签
- **WHEN** 用户关闭浮动窗口
- **THEN** 该标签的 View 被重新添加到 DraggableTabWidget 中
- **AND** 标签标题和图标保持不变
- **AND** 标签被添加到原来的位置（如果可能）或末尾

#### Scenario: 拖拽标签回标签栏
- **WHEN** 用户将浮动窗口拖拽到 DraggableTabWidget 的标签栏区域
- **THEN** 浮动窗口关闭
- **AND** 标签被重新插入到标签栏中拖拽释放的位置

#### Scenario: 标签栏内拖拽排序
- **WHEN** 用户在标签栏内拖拽标签
- **THEN** 标签在标签栏内重新排序，不触发拖出窗口行为

### Requirement: 新建标签页
系统 SHALL 支持通过 "+" 按钮创建新的空白采集标签页。

#### Scenario: 点击新建标签按钮
- **WHEN** 用户点击标签栏右侧的 "+" 按钮
- **THEN** 创建一个新的 TabContext（is_live = true）
- **AND** 新标签页自动切换为当前活跃标签
- **AND** 新标签页的 View 绑定到 SigSession
- **AND** 标签标题默认为 "Session N"（N 为递增序号）

### Requirement: 关闭标签页
系统 SHALL 支持关闭标签页。

#### Scenario: 点击标签关闭按钮
- **WHEN** 用户点击标签上的关闭按钮（×）
- **THEN** 如果该标签有未保存的数据，弹出确认对话框
- **AND** 用户确认后，该标签的 TabContext 被销毁
- **AND** View 和 SessionSnapshot 被释放
- **AND** 如果关闭的是活跃采集标签，SigSession 停止采集

#### Scenario: 关闭最后一个标签
- **WHEN** 用户关闭最后一个标签页
- **THEN** 自动创建一个新的空白采集标签页（确保始终至少有一个标签）

#### Scenario: 关闭活跃采集标签
- **WHEN** 用户关闭当前活跃采集标签
- **THEN** SigSession 停止采集
- **AND** 自动将下一个标签设为活跃标签
- **AND** 如果下一个标签是历史标签，SamplingBar 更新为只读状态

### Requirement: 标签页切换
系统 SHALL 支持在多个标签页之间切换，切换时自动更新所有 UI 组件的绑定关系。

#### Scenario: 从活跃标签切换到历史标签
- **WHEN** 用户点击一个历史数据标签
- **THEN** 当前活跃标签的数据被拍摄快照保存
- **AND** 新标签的 View 设为当前显示
- **AND** Dock 窗口（MeasureDock、ProtocolDock 等）重新绑定到新标签的 View
- **AND** SamplingBar 更新为只读状态（不可启动采集）
- **AND** 标题栏更新为新标签的标题

#### Scenario: 从历史标签切换到活跃采集标签
- **WHEN** 用户点击活跃采集标签
- **THEN** 新标签的 View 重新绑定到 SigSession
- **AND** Dock 窗口重新绑定到新标签的 View
- **AND** SamplingBar 恢复为可操作状态

#### Scenario: 采集进行中切换标签
- **WHEN** SigSession 正在采集数据
- **AND** 用户切换到另一个标签
- **THEN** 采集不中断，继续在后台运行
- **AND** 原标签保持为活跃采集标签（is_live = true）
- **AND** 切换到的新标签显示其快照数据

### Requirement: 打开文件创建新标签
系统 SHALL 在打开数据文件时创建新标签页，而非替换当前内容。

#### Scenario: 打开 .dsl 文件
- **WHEN** 用户通过 FileBar 或菜单打开一个 .dsl 数据文件
- **THEN** 创建一个新的 TabContext（is_live = false）
- **AND** 文件数据加载到该标签的 SessionSnapshot 中
- **AND** 标签标题设为文件名（不含路径和扩展名）
- **AND** 新标签自动切换为当前活跃标签

#### Scenario: 打开文件时当前标签正在采集
- **WHEN** 用户打开文件
- **AND** 当前活跃标签正在进行采集
- **THEN** 采集不中断
- **AND** 新文件标签作为非活跃标签创建

### Requirement: 标签页右键菜单
系统 SHALL 提供标签页的右键上下文菜单。

#### Scenario: 右键点击标签
- **WHEN** 用户右键点击标签栏中的某个标签
- **THEN** 显示上下文菜单，包含以下选项：
  - "重命名"：编辑标签标题
  - "关闭"：关闭该标签
  - "关闭其他"：关闭除该标签外的所有标签
  - "关闭右侧所有"：关闭该标签右侧的所有标签
  - "导出数据"：导出该标签的数据（如果有的话）

### Requirement: 标签页状态指示
系统 SHALL 在标签上显示状态指示，区分不同类型的标签。

#### Scenario: 活跃采集标签指示
- **WHEN** 一个标签是活跃采集标签
- **THEN** 标签标题旁显示绿色圆点指示器（●）
- **AND** 采集进行中时标签标题加粗显示

#### Scenario: 历史数据标签指示
- **WHEN** 一个标签是历史数据标签
- **THEN** 标签标题旁不显示指示器
- **AND** 标签标题正常显示

#### Scenario: 文件数据标签指示
- **WHEN** 一个标签是文件数据标签
- **THEN** 标签标题旁显示文件图标（📁）

### Requirement: Dock 窗口动态绑定
系统 SHALL 支持在标签切换时动态更新 Dock 窗口的 View 绑定。

#### Scenario: 标签切换时 Dock 窗口更新
- **WHEN** 用户切换到另一个标签页
- **THEN** MeasureDock 断开与旧 View 的信号连接，连接到新 View 的信号
- **AND** ProtocolDock 更新为新标签的解码数据
- **AND** SearchDock 更新为新标签的搜索上下文
- **AND** TriggerDock 更新为新标签的触发设置（如果是活跃采集标签）

#### Scenario: 浮动窗口中的 Dock 窗口
- **WHEN** 一个标签被拖出为浮动窗口
- **THEN** 浮动窗口不包含独立的 Dock 窗口
- **AND** 主窗口的 Dock 窗口不随浮动窗口的标签切换而更新
- **AND** 浮动窗口中的 View 可独立浏览和缩放，但协议分析等高级功能需通过主窗口操作

### Requirement: 标签页数据保存
系统 SHALL 支持保存标签页的数据。

#### Scenario: 保存当前标签数据
- **WHEN** 用户触发保存操作（Ctrl+S 或 FileBar 保存按钮）
- **THEN** 保存当前活跃标签的数据
- **AND** 如果是活跃采集标签，保存 SigSession 的当前数据
- **AND** 如果是历史/文件标签，保存该标签 SessionSnapshot 的数据
- **AND** 文件标签的 file_path 更新为保存路径

### Requirement: 标签页标题重命名
系统 SHALL 支持双击标签标题进行重命名。

#### Scenario: 双击标签重命名
- **WHEN** 用户双击标签栏中的标签标题
- **THEN** 标题变为可编辑状态（行编辑框）
- **AND** 用户输入新标题后按 Enter 确认
- **AND** 按 Escape 取消编辑，恢复原标题

## MODIFIED Requirements

### Requirement: MainWindow 中央区域布局
MainWindow 的中央区域从单一 View 改为 DraggableTabWidget 容器。

原有布局：
```
_central_widget
  └── _vertical_layout
        └── _view (唯一 View)
```

新布局：
```
_central_widget
  └── _vertical_layout
        └── _tab_widget (DraggableTabWidget)
              ├── Tab[0]: TabContext { View + SessionSnapshot }
              ├── Tab[1]: TabContext { View + SessionSnapshot }
              └── "+" 按钮
```

### Requirement: View 数据获取方式
View 的数据获取方式从直接持有 `SigSession*` 改为通过 `DataSource` 接口间接获取。

原有方式：
```cpp
_session->get_signals()
_session->cur_snap_samplerate()
```

新方式：
```cpp
_data_source->get_signals()
_data_source->cur_snap_samplerate()
```

其中 `DataSource` 是一个接口，`SigSession` 和 `SessionSnapshot` 都实现该接口。

### Requirement: SamplingBar 上下文切换
SamplingBar 在标签切换时需更新其关联的 Session 和 View。

原有行为：SamplingBar 持有固定的 `_session` 和 `_device_agent`。
新行为：SamplingBar 在标签切换时调用 `set_context(SigSession*, View*)` 更新关联。

## REMOVED Requirements

### Requirement: MainWindow 持有唯一 _view
**Reason**: 多标签架构下，MainWindow 不再持有唯一 View，而是通过 DraggableTabWidget 管理多个 TabContext，每个 TabContext 持有独立的 View。
**Migration**: 所有通过 `MainWindow::_view` 访问 View 的代码改为通过 `MainWindow::current_view()` 获取当前活跃标签的 View。

### Requirement: 打开文件替换当前内容
**Reason**: 打开文件操作应创建新标签页而非替换当前内容。
**Migration**: `FileBar::sig_load_file` 信号的槽函数从替换当前 Session 数据改为创建新 TabContext。
