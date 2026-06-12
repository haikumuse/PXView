# MCP 服务侧边栏入口 Spec

## Why

PXView 已有完整的 MCP 服务器和网页客户端，但用户无法从 Qt GUI 中发现或使用这些功能。需要在侧边栏添加 MCP 服务入口，让用户一键打开网页端聊天界面或获取 MCP 开发者接入配置。同时，当前 MCP 端口 10530 与 Logic 2 冲突，需要更换端口。

## What Changes

- 在右侧边栏（SideBar）添加 "MCP 服务" 按钮，使用 workflow SVG 图标，染色逻辑与其他按钮一致
- 点击按钮弹出 SlidingDrawer 页面，包含两个功能区：网页端聊天界面入口 + MCP 开发者接入配置
- 将 MCP 服务器默认端口从 10530 改为 10420（避免与 Logic 2 的 10530 冲突）
- 将网页客户端默认连接端口从 10530 改为 10420
- 在 McpTransport 中添加 GET 请求处理，支持返回静态网页文件（webui/ 目录）
- 添加 workflow.svg 图标到 icons/dark/ 和 icons/light/ 目录

## Impact

- Affected specs: `build-mcp-web-client`（端口变更）, `harden-mcp-web-client`（端口变更）, `implement-full-mcp-protocol`（端口变更）
- Affected code:
  - `PXView/pv/appcontrol.cpp` — 端口 10530 → 10420
  - `PXView/pv/api/mcp_transport.h` — 默认端口参数变更 + GET 请求支持
  - `PXView/pv/api/mcp_transport.cpp` — 添加 GET 静态文件服务 + 端口变更
  - `PXView/pv/mainwindow.h` — 新增 _drawer_page_mcp 和 _sidebar_mcp_index 成员
  - `PXView/pv/mainwindow.cpp` — 添加侧边栏按钮 + drawer 页面 + 事件处理
  - `PXView/pv/dock/` — 新增 McpControlWidget（MCP 控制面板）
  - `PXView/icons/dark/workflow.svg` — 新增图标
  - `PXView/icons/light/workflow.svg` — 新增图标
  - `web/src/hooks/useAppStore.ts` — 默认端口 10530 → 10420
  - `web/src/lib/mcp-client.ts` — 如有硬编码端口需变更
  - 测试脚本 `test_*.py` — 端口变更

---

## ADDED Requirements

### Requirement: 侧边栏 MCP 服务按钮

系统 SHALL 在右侧边栏的"日志"按钮上方添加一个 "MCP 服务" 按钮，使用 workflow 图标（两个连接的方框），按钮类型为 DockItem，点击后打开 SlidingDrawer 中的 MCP 控制面板页面。

#### Scenario: 侧边栏显示 MCP 服务按钮
- **WHEN** 应用启动完成
- **THEN** 右侧边栏在"日志"按钮上方显示 "MCP 服务" 按钮，图标为 workflow.svg，文字为"MCP 服务"（中文）/ "MCP Server"（英文），染色逻辑与其他 DockItem 一致（未选中用 @sidebar-icon-color，选中用 @sidebar-accent）

#### Scenario: 点击 MCP 服务按钮
- **WHEN** 用户点击侧边栏的 MCP 服务按钮
- **THEN** SlidingDrawer 从右侧滑出，显示 MCP 控制面板页面，侧边栏指示器动画移动到该按钮位置

### Requirement: MCP 控制面板

系统 SHALL 在 SlidingDrawer 中提供一个 MCP 控制面板，包含两个功能区：网页端聊天界面入口和 MCP 开发者接入配置。

#### Scenario: 打开 MCP 网页端聊天界面
- **WHEN** 用户点击面板中的"打开 MCP 网页端聊天界面"按钮
- **THEN** 系统调用 `QDesktopServices::openUrl` 在外部浏览器中打开 `http://127.0.0.1:10420/`

#### Scenario: MCP 服务状态显示
- **WHEN** MCP 控制面板显示
- **THEN** 面板显示 MCP 服务运行状态（运行中/已停止）和监听地址（127.0.0.1:10420）

#### Scenario: 复制 Claude 配置
- **WHEN** 用户点击"复制 Claude 配置"按钮
- **THEN** 系统将 Claude Desktop 所需的 JSON 配置复制到剪贴板，格式为：
  ```json
  {
    "mcpServers": {
      "pxview": {
        "url": "http://127.0.0.1:10420",
        "transport": "http"
      }
    }
  }
  ```

#### Scenario: 重启 MCP 服务
- **WHEN** 用户点击"重启 MCP 服务"按钮
- **THEN** 系统停止并重新启动 MCP 传输层。如果当前正在采集中，按钮应禁用或弹出确认对话框

### Requirement: MCP 端口变更

系统 SHALL 将 MCP 服务器默认监听端口从 10530 改为 10420，避免与 Logic 2 的默认端口冲突。

#### Scenario: MCP 服务器启动在新端口
- **WHEN** PXView 启动
- **THEN** MCP 服务器监听在 127.0.0.1:10420（而非 10530）

#### Scenario: 网页客户端连接新端口
- **WHEN** 用户打开网页客户端且未自定义 MCP 服务器地址
- **THEN** 默认连接地址为 `http://127.0.0.1:10420`

### Requirement: MCP 静态文件服务

系统 SHALL 在 McpTransport 中支持 GET 请求，返回 webui/ 目录下的静态文件，使外部浏览器可以通过 `http://127.0.0.1:10420/` 直接访问网页客户端。

#### Scenario: 浏览器访问根路径
- **WHEN** 浏览器发送 GET / 请求到 MCP 服务器
- **THEN** 服务器返回 `webui/index.html` 文件（Content-Type: text/html）

#### Scenario: 浏览器访问静态资源
- **WHEN** 浏览器发送 GET /assets/xxx.js 请求
- **THEN** 服务器返回对应的 JS/CSS 文件，带正确的 Content-Type

#### Scenario: 文件不存在
- **WHEN** 浏览器请求不存在的文件
- **THEN** 服务器返回 404

#### Scenario: POST 请求不受影响
- **WHEN** MCP 客户端发送 POST / 请求（JSON-RPC）
- **THEN** 行为与当前完全一致，GET 处理不影响 POST 路径

### Requirement: Workflow 图标

系统 SHALL 在 icons/dark/ 和 icons/light/ 目录下添加 workflow.svg 图标文件，图标内容为用户提供的 SVG（两个连接的圆角矩形），使用 `stroke="currentColor"` 以支持主题染色。

---

## MODIFIED Requirements

### Requirement: McpTransport 默认端口

`McpTransport` 构造函数的默认端口参数 SHALL 从 10530 改为 10420。

### Requirement: 网页客户端默认连接地址

`useAppStore.ts` 中的 `mcpServerUrl` 默认值 SHALL 从 `http://127.0.0.1:10530` 改为 `http://127.0.0.1:10420`。

---

## REMOVED Requirements

（无移除项）
