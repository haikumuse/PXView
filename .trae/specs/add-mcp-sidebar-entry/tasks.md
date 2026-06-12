# Tasks

- [x] Task 1: 更换 MCP 端口 10530 → 10420
  - [x] SubTask 1.1: 修改 `PXView/pv/appcontrol.cpp` 中 `_mcp_transport` 的端口参数
  - [x] SubTask 1.2: 修改 `PXView/pv/api/mcp_transport.h` 中构造函数默认端口参数
  - [x] SubTask 1.3: 修改 `web/src/hooks/useAppStore.ts` 中 `mcpServerUrl` 默认值
  - [x] SubTask 1.4: 修改测试脚本 `test_pwm_capture.py`, `test_pwm_capture2.py`, `test_pwm_count.py`, `test_pwm_coverage.py`, `test_simple_capture.py`, `test_close.py`, `test_mcp.html` 中的端口

- [x] Task 2: 添加 workflow.svg 图标
  - [x] SubTask 2.1: 创建 `PXView/icons/dark/workflow.svg`（使用用户提供的 SVG，stroke="currentColor"）
  - [x] SubTask 2.2: 创建 `PXView/icons/light/workflow.svg`（同上）

- [x] Task 3: McpTransport 添加 GET 静态文件服务
  - [x] SubTask 3.1: 在 `mcp_transport.cpp` 的 `handle_http_request` 中添加 GET 请求分支，支持从 webui/ 目录返回静态文件
  - [x] SubTask 3.2: 实现 MIME 类型检测（.html → text/html, .js → application/javascript, .css → text/css, .svg → image/svg+xml, .json → application/json）
  - [x] SubTask 3.3: 实现 404 处理（文件不存在时返回 404）
  - [x] SubTask 3.4: 更新 CORS headers 中 Access-Control-Allow-Methods 加入 GET

- [x] Task 4: 创建 McpControlWidget 控制面板
  - [x] SubTask 4.1: 创建 `PXView/pv/dock/mcpcontroldock.h` 和 `.cpp`，继承 QWidget
  - [x] SubTask 4.2: 实现 UI 布局：标题 "MCP 服务" + 状态指示（运行中/已停止 + 地址）+ "打开 MCP 网页端聊天界面" 按钮 + "复制 Claude 配置" 按钮 + "重启 MCP 服务" 按钮
  - [x] SubTask 4.3: 实现"打开 MCP 网页端聊天界面"按钮 → `QDesktopServices::openUrl("http://127.0.0.1:10420/")`
  - [x] SubTask 4.4: 实现"复制 Claude 配置"按钮 → 生成 JSON 并复制到剪贴板
  - [x] SubTask 4.5: 实现"重启 MCP 服务"按钮 → 调用 AppControl 的 MCP 重启方法
  - [x] SubTask 4.6: 实现 MCP 服务状态查询（通过 AppControl 获取 McpTransport 运行状态）

- [x] Task 5: 集成到 MainWindow 侧边栏
  - [x] SubTask 5.1: 在 `mainwindow.h` 中添加 `_drawer_page_mcp` 和 `SIDEBAR_MCP` 成员变量
  - [x] SubTask 5.2: 在 `mainwindow.cpp` 的 `setupSideBar()` 中添加侧边栏按钮（"workflow.svg", "IDS_TOOLBAR_MCP", "MCP Server", DockItem, _drawer_page_mcp），放在"日志"按钮之后
  - [x] SubTask 5.3: 在 `mainwindow.cpp` 中将 McpControlWidget 添加到 SlidingDrawer 的一个新 page
  - [x] SubTask 5.4: 在 `dockItemClicked` 信号处理中添加 SIDEBAR_MCP 的分支
  - [x] SubTask 5.5: 添加 i18n 字符串（中文"MCP 服务"，英文"MCP Server"，繁体"MCP 服務"）

- [x] Task 6: CMake 构建集成
  - [x] SubTask 6.1: 将 `mcpcontroldock.h/.cpp` 添加到 CMakeLists.txt 的源文件列表
  - [ ] SubTask 6.2: 将 webui/ 静态文件目录添加到 CMake install 规则（npm run build 输出的 dist/ 复制到 install.dir/bin/webui/）— 延后处理，需先确认 web/ 目录的构建输出路径

# Task Dependencies
- [Task 2] depends on nothing (可并行)
- [Task 1] depends on nothing (可并行)
- [Task 3] depends on [Task 1] (端口变更后再改 GET 服务)
- [Task 4] depends on [Task 1] (端口变更后才能写正确的 URL)
- [Task 5] depends on [Task 2, Task 4] (需要图标和面板组件)
- [Task 6] depends on [Task 4, Task 5] (需要所有源文件就绪)
