# 主线程性能优化 Spec

## Why
基于火焰图分析，Qt 动画卡顿的根因不是动画系统本身（仅占 0.19%），而是动画触发了全量重绘（31%）+ 级联布局更新（11%）+ 同步磁盘 I/O（fdatasync 阻塞）。侧边栏切换时 `AppConfig::SaveFrame()` 同步写磁盘也会导致主线程冻结。MeasureDock 在光标移动时反复 delete/new 组件并重新解析 SVG 图标，进一步加重主线程负担。

## What Changes
- 修改 `AppConfig::SaveFrame()` 为异步延迟保存，使用 `QTimer::singleShot` 消抖，避免主线程同步磁盘 I/O
- 修改 `AppConfig::SaveApp()` / `SaveHistory()` 同样支持异步延迟保存
- 修改 `SlidingDrawer` 动画期间设置 `Qt::WA_NoSystemBackground` + `Qt::WA_OpaquePaintEvent` 属性，减少系统背景重绘
- 修改 `Viewport::paintEvent` 支持局部更新（`update(rect)` 替代 `update()`），动画期间只标记脏区域
- 修改 `MeasureDock::build_cursor_pannel()` 改为组件复用模式，避免 delete/new 循环
- 新增 `IconCache` 工具类，将 SVG 图标预渲染为 `QPixmap` 缓存，避免重复解析

## Impact
- Affected code: `appconfig.h/cpp`, `slidingdrawer.cpp`, `viewport.cpp`, `measuredock.cpp`, 所有使用 `QIcon(iconPath+"/xxx.svg")` 的文件
- 新增文件: `iconcache.h/cpp`
- Affected specs: `add-smooth-scroll-animation`（动画性能互补）

## ADDED Requirements

### Requirement: AppConfig 异步延迟保存
系统 SHALL 将 `AppConfig::SaveFrame()`、`SaveApp()`、`SaveHistory()` 改为异步延迟保存模式。

#### Scenario: 多次快速调用 SaveFrame 只触发一次实际写盘
- **WHEN** `SaveFrame()` 在 2 秒内被多次调用
- **THEN** 只执行一次实际的 `QSettings` 写入操作
- **AND** 使用 `QTimer::singleShot(2000)` 实现消抖

#### Scenario: 应用退出时立即同步保存
- **WHEN** 应用即将退出（`QCoreApplication::aboutToQuit` 信号）
- **THEN** 取消所有待定定时器，立即执行所有未完成的保存操作
- **AND** 确保配置数据不丢失

#### Scenario: 动画期间不触发同步磁盘 I/O
- **WHEN** `SlidingDrawer` 动画进行中
- **THEN** `SaveFrame()` 调用不会阻塞主线程
- **AND** 实际写盘延迟到动画结束后执行

### Requirement: SlidingDrawer 绘制优化
系统 SHALL 为 `SlidingDrawer` 添加绘制优化属性，减少动画期间的重绘开销。

#### Scenario: 动画期间设置不透明绘制属性
- **WHEN** `SlidingDrawer` 开始打开/关闭动画
- **THEN** 设置 `Qt::WA_OpaquePaintEvent` 和 `Qt::WA_NoSystemBackground` 属性
- **AND** 动画结束后恢复默认属性

#### Scenario: 动画期间 Viewport 局部更新
- **WHEN** `SlidingDrawer` 动画每帧更新位置
- **THEN** Viewport 只重绘被 Drawer 遮挡/露出的矩形区域
- **AND** 不触发整个 Viewport 的全量重绘

### Requirement: MeasureDock 组件复用
系统 SHALL 修改 `MeasureDock::build_cursor_pannel()` 为组件复用模式。

#### Scenario: 光标数量未变化时仅更新文本
- **WHEN** `cursor_update()` 被调用且光标数量未变化
- **THEN** 仅调用 `QLabel::setText()` 更新已有标签的文本
- **AND** 不执行任何 `deleteLater()` 或 `new` 操作

#### Scenario: 光标数量增加时追加新组件
- **WHEN** 光标数量从 N 增加到 M（M > N）
- **THEN** 保留前 N 个组件，仅创建 M-N 个新组件追加到布局末尾

#### Scenario: 光标数量减少时移除多余组件
- **WHEN** 光标数量从 N 减少到 M（M < N）
- **THEN** 保留前 M 个组件，仅对后 N-M 个组件调用 `deleteLater()`

### Requirement: SVG 图标缓存（IconCache）
系统 SHALL 提供 `IconCache` 工具类，将 SVG 图标预渲染为 `QPixmap` 缓存。

#### Scenario: 首次请求图标时渲染并缓存
- **WHEN** 通过 `IconCache::pixmap(path, size)` 请求一个尚未缓存的 SVG 图标
- **THEN** 使用 `QSvgRenderer` 渲染为 `QPixmap` 并存入 `QHash<QString, QPixmap>` 缓存
- **AND** 返回该 QPixmap

#### Scenario: 后续请求直接返回缓存
- **WHEN** 通过 `IconCache::pixmap(path, size)` 请求已缓存的图标
- **THEN** 直接返回缓存的 `QPixmap`，不重新解析 SVG

#### Scenario: 主题切换时清空缓存
- **WHEN** 应用主题从 dark 切换到 light 或反之
- **THEN** 清空所有缓存，下次请求时使用新主题的图标路径重新渲染

#### Scenario: 全局替换 QIcon(iconPath+"/xxx.svg") 为缓存版本
- **WHEN** 代码中存在 `QIcon(iconPath + "/del.svg")` 等模式
- **THEN** 替换为 `IconCache::icon(iconPath + "/del.svg")` 调用
- **AND** 所有工具栏、Dock、按钮中的 SVG 图标加载均走缓存路径
