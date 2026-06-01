# Tasks

- [x] Task 1: 移除 LogoBar 的 QToolBar 占位
  - [x] 在 mainwindow.cpp 中注释掉 `addToolBar(Qt::LeftToolBarArea, _logo_bar)`
  - [x] 确认 LogoBar 对象仍然被创建（其 QAction 被 TitleBar 引用）
  - [x] 确认 LogoBar 的 eventFilter 注册仍然保留

- [ ] Task 2: 验证布局和功能
  - [ ] 编译并运行
  - [ ] 验证 Tab 栏从窗口最左侧开始
  - [ ] 验证 Ribbon 面板从窗口最左侧开始
  - [ ] 验证 Ruler 左侧的 DevMode 按钮位置正确
  - [ ] 验证 Header 和 Viewport 从窗口最左侧开始
  - [ ] 验证分组卡片覆盖完整宽度
  - [ ] 验证 Ribbon 中的语言切换、关于、帮助功能正常
  - [ ] 验证子窗口（SubMainFrame）不受影响

# Task Dependencies
- Task 2 depends on Task 1
