# QRibbon 性能分析计划

## 目标
分析旧版 DSView 的 QRibbon 为什么动画流畅，而当前 PXView 的 TitleBar 动画卡顿。

## 背景
通过日志对比发现：
- 旧版 QRibbon 和当前 TitleBar 的 **布局传播路径完全相同**（每帧都触发 Viewport resize + paint）
- 但 QRibbon 视觉上更流畅
- 需要找出 QRibbon 流畅的真正原因

## 分析文件清单
1. `c:\Users\admin\Downloads\c35783e9\DSView\pv\QRibbon\QRibbon.cpp` — QRibbon 核心实现
2. `c:\Users\admin\Downloads\c35783e9\DSView\pv\QRibbon\QRibbon.h` — QRibbon 头文件
3. `c:\Users\admin\Downloads\c35783e9\DSView\pv\QRibbon\qribbon.ui` — QRibbon UI 设计
4. `c:\Users\admin\Downloads\c35783e9\DSView\pv\QRibbon\QRibbon.qrc` — 资源文件
5. `c:\Users\admin\Downloads\c35783e9\DSView\pv\mainwindow.cpp` — QRibbon 集成方式
6. `c:\Users\admin\Downloads\c35783e9\DSView\pv\mainwindow.h` — MainWindow 声明

## 分析维度

### 1. 动画属性对比
| 维度 | QRibbon | TitleBar |
|------|---------|----------|
| 动画目标属性 | `minimumHeight` | `minimumHeight` |
| 动画时长 | 默认（可能不同） | 250ms |
| 缓动曲线 | `QEasingCurve::Linear` | `QEasingCurve::OutCubic` / `makeTailwindCurve()` |
| 动画帧率 | ？ | ？ |

### 2. 渲染路径对比
- QRibbon 的 `_ribbonPanel` 是如何渲染的？（QTabWidget + QWidget）
- TitleBar 的 `_ribbonPanel` 是如何渲染的？（QStackedWidget + QHBoxLayout）
- QRibbon 是否使用了 `WA_TranslucentBackground`？
- TitleBar 是否使用了 `WA_TranslucentBackground`？

### 3. 布局结构对比
- QRibbon 的 centralWidget 结构
- TitleBar 的 centralWidget 结构（QGridLayout + Border + MainWindow）
- 嵌套层级差异

### 4. 关键差异点排查
- **QRibbon 的 `QTabWidget` vs TitleBar 的 `QTabBar`**：QTabWidget 可能有更高效的渲染
- **QRibbon 的 `QWidget` + `QHBoxLayout` vs TitleBar 的 `QToolButton`**：按钮创建方式差异
- **QRibbon 没有 `_ribbonContainer` 浮动层**：QRibbon 的 panel 直接作为 QMenuBar 的一部分
- **TitleBar 有 `_ribbonContainer` 浮动层**：额外的 QWidget 层级

## 实施步骤

### 步骤 1：读取并分析 QRibbon 核心代码
- 读取 `QRibbon.cpp` 完整代码
- 重点关注 `expandTab()`、`hideTab()`、`clickTab()` 的实现
- 分析 `_ribbonPanel`（即 `ui->widgetContainer`）的渲染方式

### 步骤 2：读取并分析 QRibbon UI 设计
- 读取 `qribbon.ui`
- 了解 `widgetContainer`、`tabWidgetMenuBar` 的结构
- 对比 TitleBar 的 `_ribbonPanel` 结构

### 步骤 3：读取并分析 QRibbon 集成方式
- 读取 `mainwindow.cpp` 中 QRibbon 的创建和集成代码
- 对比 TitleBar 在 `mainframe.cpp` 中的集成方式

### 步骤 4：识别关键差异
- 列出 QRibbon 和 TitleBar 的所有差异点
- 评估每个差异点对性能的影响
- 找出最可能导致流畅度差异的因素

### 步骤 5：提出优化方案
- 基于分析结果，提出具体的优化建议
- 可能包括：简化渲染层级、移除不必要的属性、调整动画参数等

## 预期成果
- 明确 QRibbon 流畅的真正原因
- 提出可实施的优化方案
- 改善当前 PXView 的 Ribbon 动画性能
