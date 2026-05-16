# Tasks

- [x] Task 1: 创建 SideBarButton 自定义控件
  - [x] 1.1 创建 `pv/widgets/sidebarbutton.h`，定义 SideBarButton 类（继承 QWidget），包含属性：iconName、alternateIconName、text、isCheckable、isChecked、isRunning、itemType、drawerPageIndex、选中/悬停/按下状态标志
  - [x] 1.2 创建 `pv/widgets/sidebarbutton.cpp`，实现 paintEvent 自绘：drawBackground（悬停半透明、选中高亮）、drawIndicator（左侧 4px 圆角指示条）、drawIcon（QIcon 居中绘制在上方，运行时用 alternateIcon）、drawText（下方居中）
  - [x] 1.3 实现 mousePressEvent/mouseReleaseEvent/enterEvent/leaveEvent 处理交互状态
  - [x] 1.4 设置固定尺寸 64×58，添加 clicked 信号
  - [x] 1.5 实现 setRunning(bool) 方法：切换图标为 alternateIcon 或恢复原始 icon，触发 update()

- [x] Task 2: 创建新 SVG 图标文件
  - [x] 2.1 创建 dark 主题图标：zap.svg、binary.svg、ruler.svg、search.svg、sliders.svg（描边色用浅色 #E0E0E0）
  - [x] 2.2 创建 light 主题图标：zap.svg、binary.svg、ruler.svg、search.svg、sliders.svg（描边色用深色 #424242）
  - [x] 2.3 创建 play.svg（dark+light，填充 #00E676）
  - [x] 2.4 创建 step-forward.svg（dark+light，填充 #FFC400）
  - [x] 2.5 创建 stop.svg（dark+light，实心填充 #e74c3c）
  - [x] 2.6 在 PXView.qrc 中注册所有新图标文件

- [x] Task 3: 重构 SideBar 使用 SideBarButton
  - [x] 3.1 修改 sidebar.h：ItemInfo 中 button 类型从 XToolButton* 改为 SideBarButton*，新增 alternateIconName 字段，移除 XToolButton 头文件依赖，添加 SideBarButton 头文件
  - [x] 3.2 修改 addItem() 签名：增加 alternateIconName 参数（默认空），创建 SideBarButton 替代 XToolButton，设置 alternateIcon
  - [x] 3.3 新增 setItemRunning(int index, bool running) 方法
  - [x] 3.4 修改 onButtonClicked()：适配 SideBarButton 的信号和状态
  - [x] 3.5 修改 UpdateTheme()：使用 SideBarButton 的图标更新接口，运行状态按钮需用 alternateIcon 路径
  - [x] 3.6 修改 UpdateLanguage()/UpdateFont()：适配 SideBarButton
  - [x] 3.7 修改 setItemVisible/setItemEnabled/setItemChecked/clearAllChecked：适配 SideBarButton

- [x] Task 4: 更新 MainWindow 采集状态联动
  - [x] 4.1 在 setupSideBar() 中更新图标名（zap.svg、binary.svg 等），为开始/立即按钮传入 alternateIconName "stop.svg"
  - [x] 4.2 修改 on_side_bar_action_clicked()：SIDEBAR_RUNSTOP 和 SIDEBAR_INSTANT 判断当前运行状态，运行中则执行停止
  - [x] 4.3 在采集开始回调中调用 _side_bar->setItemRunning(SIDEBAR_RUNSTOP/INSTANT, true)
  - [x] 4.4 在采集结束回调中调用 _side_bar->setItemRunning(SIDEBAR_RUNSTOP/INSTANT, false)
  - [x] 4.5 更新 update_view_status() 中侧边栏按钮的启用/禁用/可见性逻辑

- [x] Task 5: 更新 QSS 样式
  - [x] 5.1 更新 dark.qss 和 light.qss 中 #sidebar 相关样式，适配新的 SideBarButton 控件
  - [x] 5.2 移除不再需要的 QToolButton 侧边栏样式覆盖

- [x] Task 6: 编译验证
  - [x] 6.1 执行 CMake 构建，确保无编译错误
  - [x] 6.2 检查无运行时崩溃

# Task Dependencies
- [Task 2] depends on nothing (可并行)
- [Task 1] depends on nothing (可并行)
- [Task 3] depends on [Task 1] (需要 SideBarButton 类)
- [Task 4] depends on [Task 3] (需要重构后的 SideBar)
- [Task 5] depends on [Task 3] (需要新控件结构)
- [Task 6] depends on [Task 3, Task 4, Task 5]
