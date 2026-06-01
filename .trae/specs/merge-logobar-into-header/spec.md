# 移除 LogoBar 左侧占位 Spec

## Why
当前 LogoBar 作为空的 QToolBar 通过 `addToolBar(Qt::LeftToolBarArea, _logo_bar)` 占据了 MainWindow 左侧约 10px 的工具栏区域，但其所有功能（语言切换、关于、帮助等）已通过 addAction 迁移到 TitleBar 的 Ribbon 中。这个空 LogoBar 导致整个 MainWindow 的中央区域被向右偏移，影响范围包括：

1. **Tab 栏**：TitleBar 中的 QTabBar 被向右推了约 10px
2. **Ribbon 面板**：被向右推了约 10px
3. **Ruler 左侧的 DevMode 按钮**：被向右推了约 10px
4. **Header（信号标签区）**：被向右推了约 10px
5. **Viewport（波形绘图区）**：被向右推了约 10px，导致 `@tabview-bg` 背景色在分组卡片之间只露出很窄的一条

移除这个空 LogoBar 的工具栏占位，可以让所有区域回归正确的对齐位置。

## What Changes
- 移除 LogoBar 作为 QToolBar 在左侧工具栏区域的占位（`addToolBar(Qt::LeftToolBarArea, _logo_bar)`）
- LogoBar 对象保留为纯动作容器（其 QAction 仍被 TitleBar 的 Ribbon 引用），但不再作为可见的 QToolBar 添加到布局中
- 不需要在 Header 中增加 Logo 占位区域（LogoBar 原本就没有可见内容）

## Impact
- Affected code:
  - `PXView/pv/mainwindow.cpp` — 移除 `addToolBar(Qt::LeftToolBarArea, _logo_bar)`，LogoBar 对象仍创建但不添加到工具栏区域
- 不受影响的代码：
  - `PXView/pv/toolbars/logobar.h` / `logobar.cpp` — 无需修改，LogoBar 仍作为 QAction 容器存在
  - `PXView/pv/view/view.cpp` — 无需修改，布局会自动适应
  - `PXView/pv/view/header.cpp` — 无需修改
  - `PXView/pv/view/viewport.cpp` — 无需修改

## ADDED Requirements

### Requirement: 移除 LogoBar 左侧工具栏占位
系统 SHALL 不再将 LogoBar 作为 QToolBar 添加到 MainWindow 的左侧工具栏区域。

#### Scenario: 主窗口无左侧 LogoBar 占位
- **WHEN** 应用启动并显示主窗口
- **THEN** MainWindow 左侧不再有 LogoBar 的 QToolBar 占位
- **AND** Tab 栏从窗口最左侧开始
- **AND** Ribbon 面板从窗口最左侧开始
- **AND** Ruler 左侧的 DevMode 按钮从正确的位置开始
- **AND** Header 和 Viewport 从窗口最左侧开始
- **AND** 波形绘图区的基础背景色（@tabview-bg）覆盖完整宽度
- **AND** 分组卡片覆盖完整的 Viewport 宽度，不再出现窄条背景

### Requirement: LogoBar 功能保持不变
系统 SHALL 保持 LogoBar 的所有现有功能（语言切换、关于、帮助、Bug 报告、更新检查），这些功能继续通过 TitleBar 的 Ribbon 菜单访问。

#### Scenario: Ribbon 菜单中的 LogoBar 功能
- **WHEN** 用户点击 Ribbon 中的 Display 或 Help 分类
- **THEN** 语言切换、关于、帮助、Bug 报告、更新等功能正常可用
- **AND** 行为与移除前完全一致
