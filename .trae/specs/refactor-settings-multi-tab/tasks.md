# Tasks

- [x] Task 1: 扩展 AppConfig 添加快捷键和样式配置的数据结构与持久化
  - [x] SubTask 1.1: 在 appconfig.h 中定义 ShortcutOptions 结构体（功能ID → QKeySequence 字符串映射）和 StyleOptions 结构体（令牌名 → 颜色值映射）
  - [x] SubTask 1.2: 在 AppConfig 类中添加 ShortcutOptions 和 StyleOptions 成员及默认值初始化
  - [x] SubTask 1.3: 在 AppConfig 的 Save/Load 方法中添加快捷键和样式配置的序列化/反序列化逻辑
  - [x] SubTask 1.4: 在 icallbacks.h 中新增 DSV_MSG_SHORTCUT_CHANGED 和 DSV_MSG_STYLE_CHANGED 消息码

- [x] Task 2: 重构 ApplicationParamDlg 为多标签页设置对话框
  - [x] SubTask 2.1: 修改 ApplicationParamDlg::ShowDlg() 创建左侧导航列表 + 右侧内容面板的布局
  - [x] SubTask 2.2: 实现"显示选项"标签页，保留原有 Logic/Scope/UI 分组的全部设置项和保存逻辑
  - [x] SubTask 2.3: 实现标签页切换逻辑，点击左侧导航项时右侧内容面板切换

- [x] Task 3: 实现快捷键设置标签页
  - [x] SubTask 3.1: 创建快捷键表格控件，每行显示功能描述、当前快捷键、修改按钮
  - [x] SubTask 3.2: 实现按键捕获模式，用户按下按键组合后更新快捷键绑定
  - [x] SubTask 3.3: 实现快捷键冲突检测，冲突时提示用户
  - [x] SubTask 3.4: 实现"恢复默认"按钮功能
  - [x] SubTask 3.5: 实现快捷键配置保存到 AppConfig 并广播 DSV_MSG_SHORTCUT_CHANGED

- [x] Task 4: 改造 MainWindow::eventFilter 快捷键处理为查表方式
  - [x] SubTask 4.1: 定义快捷键功能ID枚举，建立功能ID到默认按键的映射表
  - [x] SubTask 4.2: 修改 eventFilter 中的快捷键处理逻辑，从硬编码 switch-case 改为查 AppConfig 中的快捷键配置
  - [x] SubTask 4.3: 在 MainWindow 中响应 DSV_MSG_SHORTCUT_CHANGED 消息，重新加载快捷键配置

- [x] Task 5: 实现样式定制标签页
  - [x] SubTask 5.1: 解析 dark.qss 文件，提取颜色令牌定义（@token-name: value 格式）
  - [x] SubTask 5.2: 创建样式编辑器 UI，按分类显示颜色令牌，每个令牌带颜色预览和颜色选择器
  - [x] SubTask 5.3: 实现颜色修改的实时预览功能
  - [x] SubTask 5.4: 实现样式应用逻辑：将用户自定义颜色替换到 QSS 模板中并重新应用
  - [x] SubTask 5.5: 实现"恢复默认"按钮功能
  - [x] SubTask 5.6: 实现样式配置保存到 AppConfig 并广播 DSV_MSG_STYLE_CHANGED

- [x] Task 6: 集成测试与验证
  - [x] SubTask 6.1: 验证设置对话框三个标签页切换正常
  - [x] SubTask 6.2: 验证显示选项功能与原版一致
  - [x] SubTask 6.3: 验证快捷键修改后 eventFilter 使用新配置
  - [x] SubTask 6.4: 验证样式修改后界面实时更新
  - [x] SubTask 6.5: 验证所有配置重启后持久化正确

# Task Dependencies
- [Task 2] depends on [Task 1] (需要 AppConfig 中的数据结构)
- [Task 3] depends on [Task 1] (需要 ShortcutOptions 数据结构)
- [Task 4] depends on [Task 1] (需要快捷键功能ID枚举和配置读取)
- [Task 5] depends on [Task 1] (需要 StyleOptions 数据结构)
- [Task 6] depends on [Task 2, 3, 4, 5]
