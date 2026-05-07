# 文档-视图 MVC 架构重构 Phase 2-4 Spec

## Why
Phase 1 建立了 SessionDocument 和 TabContext 的骨架，但存在关键缺陷：
1. **无法显示采集数据** — `View.set_data_document()` 将 Signal 的 `_data` 指针切换到空的 SessionDocument，而 SigSession 采集的数据停留在内部的 `_view_data`，二者从未交汇
2. **Signal 仍为全局共享** — 所有 View 共用 SigSession 创建的一套 Signal 对象，切换标签时所有标签显示同一数据
3. **拖出窗口后 Context 泄漏** — 拖出的窗口关闭时 Context 存储在 View property 中未正确回收
4. **Dock/Toolbar 无法适配多标签** — Measure/Protocol/Search/SamplingBar 未随标签切换更新上下文

需要完成 Phase 2-4，从架构层面彻底解决上述问题。

## What Changes

### Phase 2: 零拷贝数据路由与 View 隔离
- 修改 `SigSession`：新增 `set_active_document()` / `get_active_document()` 方法，采集结束后将 `_view_data` 数据写入活跃 SessionDocument
- 修改 `MainWindow::on_frame_ended()`：采集完成后将 SigSession 数据复制到活跃标签的 SessionDocument，然后重新激活标签刷新视图
- **关键修复**：Signal 的 `_data` 指针不再被 `set_data_document` 永久覆盖。改为在 `set_data_document` 中先检查 SessionDocument 是否有数据，无数据时保留 SigSession 的原始 `_data` 指向（用于实时采集中间状态）
- 修改 `View::set_data_document()`：增加 has_data 判断逻辑，无数据时不做切换
- 修改 `View`：内部新增 `_own_signals` 向量存放 View 私有 Signal 对象，逐步迁移到 View 自有 Signal

### Phase 3: 生命周期管理与 Safe Detach/Attach
- 新增 `pv/sessionmanager.h/cpp`：SessionManager 单例，统一管理所有 TabContext 的创建、销毁、查找
- 重写 `MainWindow::on_tab_detach()`：被拖出窗口关闭时由 SessionManager 执行清理，而非让 Context 泄漏
- 重写 `MainWindow::remove_tab()`：通过 SessionManager 销毁 Context，确保 decoder/dispatcher 线程安全关闭
- 新增 `pv/interface/icontextaware.h`：IContextAware 接口（bind_context/unbind_context）
- 修改 `MeasurDock`、`ProtocolDock`、`SearchDock`、`SamplingBar`：实现 IContextAware 接口
- 修改 `MainWindow::on_tab_changed()`：切换标签时调用所有 IContextAware 组件的 unbind → bind 流程

### Phase 4: 硬件路由与解码器隔离
- 修改 `SigSession::data_feed_in()`：实时采集时直接将数据写入当前 LIVE 状态 SessionContext 的 SessionDocument（而非内部 `_capture_data`）
- 修改 `SigSession::attach_data_to_signal()`：适配 SessionDocument 路由模式
- 修改 `SessionDocument`：新增 `decoder_stack` 支持，每个 Document 拥有独立解码器栈
- 修改解码流程：将 DecodeTask 从 SigSession 迁移到 SessionDocument/.TabContext 级别
- 清理废弃代码：移除 `SigSession::capture_snapshot()` 和 `SessionSnapshot` 相关代码

## Impact
- Affected specs: add-multi-tab-sessions, fix-multi-tab-bugs
- Affected code:
  - `DSView/pv/sessionmanager.h`（新增）
  - `DSView/pv/sessionmanager.cpp`（新增）
  - `DSView/pv/interface/icontextaware.h`（新增）
  - `DSView/pv/sigsession.h`（修改：新增 SessionDocument 路由 API）
  - `DSView/pv/sigsession.cpp`（修改：数据路由逻辑）
  - `DSView/pv/mainwindow.h`（修改：接入 SessionManager）
  - `DSView/pv/mainwindow.cpp`（修改：生命周期管理、Dock 绑定、数据路由触发）
  - `DSView/pv/view/view.h`（修改：新增 _own_signals）
  - `DSView/pv/view/view.cpp`（修改：set_data_document 判断逻辑、自有 Signal）
  - `DSView/pv/tabcontext.h`（改名 SessionContext，完善生命周期方法）
  - `DSView/pv/tabcontext.cpp`（适配 SessionManager）
  - `DSView/pv/dock/measuredock.h`（实现 IContextAware）
  - `DSView/pv/dock/measuredock.cpp`（实现 bind/unbind_context）
  - `DSView/pv/dock/protocoldock.h`（实现 IContextAware）
  - `DSView/pv/dock/protocoldock.cpp`（实现 bind/unbind_context）
  - `DSView/pv/dock/searchdock.h`（实现 IContextAware）
  - `DSView/pv/dock/searchdock.cpp`（实现 bind/unbind_context）
  - `DSView/pv/toolbars/samplingbar.h`（实现 IContextAware）
  - `DSView/pv/toolbars/samplingbar.cpp`（实现 bind/unbind_context）
  - `DSView/pv/data/sessiondocument.h`（新增解码器栈支持）
  - `DSView/pv/data/sessiondocument.cpp`（解码器数据管理）
  - `CMakeLists.txt`（新增源文件）

---

## ADDED Requirements

### Requirement: 采集数据正确显示
系统 SHALL 在采集完成后将数据写入活跃标签的 SessionDocument 并刷新视图，确保采集数据可见。

#### Scenario: 单次采集正确显示
- **GIVEN** 用户连接了设备，标签 A 处于 LIVE 状态
- **WHEN** 用户点击 Start 开始采集，采集完成后
- **THEN** 标签 A 的视图显示本次采集的波形数据
- **AND** SessionDocument A 包含本次采集的 Logic/Dso/Analog 数据

#### Scenario: 切换标签后视图刷新
- **GIVEN** 标签 A 有采集数据，标签 B 是新标签（空 SessionDocument）
- **WHEN** 用户从标签 A 切换到标签 B
- **THEN** 标签 B 显示空 SessionDocument 对应的空视图
- **AND** 切换回标签 A 时，标签 A 恢复显示其 SessionDocument 的采集数据

### Requirement: 零拷贝标签切换
标签切换 SHALL 仅更新 View 显示的 Signal `_data` 指针，不进行任何数据内存拷贝。

#### Scenario: 标签切换无内存拷贝
- **GIVEN** 标签 A 有 1GB 采集数据
- **WHEN** 用户切换到标签 B
- **THEN** 标签 A 的 SessionDocument 数据保持不变
- **AND** 切换操作在 O(1) 时间内完成（仅指针切换）

### Requirement: 安全的拖出/还原/关闭
标签拖出为独立窗口时 SHALL 不销毁 TabContext。关闭浮动窗口时 SHALL 还原标签。关闭标签时 SHALL 通过 SessionManager 正确回收所有资源。

#### Scenario: 拖出窗口
- **WHEN** 用户将标签拖出标签栏
- **THEN** 创建一个新的浮动 QMainWindow 包含该标签的 View
- **AND** 原 TabContext 生命周期仍由全局管理器维护
- **AND** 浮动窗口关闭事件被拦截

#### Scenario: 还原标签
- **WHEN** 浮动窗口被关闭
- **THEN** View 重新添加到主窗口的 DraggableTabWidget 中
- **AND** TabContext 重新加入 MainWindow 的 context 列表
- **AND** 原 SessionDocument 数据完整保留

#### Scenario: 关闭标签
- **WHEN** 用户点击标签的关闭按钮
- **THEN** SessionManager 销毁该 TabContext
- **AND** 释放 SessionDocument 内存
- **AND** 停止该标签的解码线程
- **AND** 至少保留一个标签页

### Requirement: Dock 窗口动态上下文绑定
所有 Dock 窗口 SHALL 实现 IContextAware 接口，随标签切换自动绑定/解绑当前活跃的 View 和 SessionDocument。

#### Scenario: 标签切换时 Dock 跟随更新
- **GIVEN** 标签 A 有数据，标签 B 无数据
- **WHEN** 用户从标签 A 切换到标签 B
- **THEN** MeasureDock 清空 A 的测量数据，绑定到 B 的 View
- **AND** ProtocolDock 清空 A 的解码器配置，绑定到 B 的 View
- **AND** SearchDock 清空搜索状态，绑定到 B 的 View

#### Scenario: 浮动窗口焦点变化
- **GIVEN** 标签 B 被拖出为浮动窗口
- **WHEN** 用户点击浮动窗口使其获得焦点
- **THEN** 主窗口的 Dock 组件切换到标签 B 的上下文

### Requirement: 硬件数据直接路由到 SessionDocument
实时采集时，硬件数据 SHALL 直接写入当前 LIVE 标签的 SessionDocument，不再经过 SigSession 内部数据池中转。

#### Scenario: 实时采集数据路由
- **WHEN** 设备正在进行采集
- **THEN** 每个数据包直接写入活跃 SessionDocument 的 Logic/Dso/Analog Snapshot
- **AND** 活跃标签的 View 实时更新渲染

### Requirement: 独立的解码器栈
每个 SessionDocument SHALL 拥有自己的解码器栈，后台标签的解码任务 SHALL 能独立运行不被阻塞。

#### Scenario: 后台标签解码继续
- **GIVEN** 标签 A 正在解码，用户切换到标签 B
- **WHEN** 标签 A 进入 HISTORICAL 状态
- **THEN** 标签 A 的解码任务继续在后台运行
- **AND** 解码完成后标签 A 的 DecodeTrace 自动更新

---

## MODIFIED Requirements

### Requirement: View 的 Signal 所有权（修正 Phase 1）
原 Phase 1 中 View 仍依赖 `_data_source->get_signals()`（全局共享 Signal）。Phase 2 修正为：View 拥有自己独立的 Signal 对象列表（`_own_signals`），启动时从设备配置克隆，指向本 View 的 SessionDocument 数据。

#### Scenario: 每个 View 有独立 Signal
- **GIVEN** 标签 A 和标签 B 都绑定同一设备配置
- **WHEN** 标签 A 激活时
- **THEN** 标签 A 的 View 用自己的 Signal 对象渲染
- **AND** 标签 A 切换到标签 B 后
- **THEN** 标签 B 的 View 用自己独立的 Signal 对象渲染 B 的 SessionDocument 数据

---

## REMOVED Requirements

### Requirement: SigSession::capture_snapshot()
**Reason**: Phase 2 实现零拷贝切换后，不再需要 `capture_snapshot()` 进行深拷贝。标签切换仅切换指针，不创建快照。
**Migration**: 所有调用 `capture_snapshot()` 的地方改为通过 SessionManager 获取目标标签的 SessionDocument 指针。
