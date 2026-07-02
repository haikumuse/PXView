# fix-remaining-architecture-issues Spec

## Why

`fix-all-architecture-issues` 完成后，三路并行调研发现仍存在 10 项架构问题，分三层：
1. **3 项真实运行时 bug（P0）**：Math/Spectrum/Lissajous 波形不显示（dirty 标志短路）、Tab 关闭 View→Document UAF、glitch/invert 线程析构未 join
2. **2 项上一轮 spec 半成品（P1-P2）**：类型化事件总线是死代码（0 消费者、0 直接发射点）、View 层仍直调 `ds_trigger_*`（违反 AGENTS.md 明文约定）
3. **5 项结构性技术债（P2-P3）**：跨线程标志全为 `volatile bool` UB、SigSession 上帝类（3219 行/49 成员）、CMakeLists.txt 1889 行单文件、SessionDocument 6 角色混淆、MainWindow::OnMessage 504 行上帝方法

本 spec 收口全部，按优先级分 4 阶段实施。P3 阶段为长期演进，可分批执行。

## What Changes

### 阶段 1（P0 紧急 bug 修复）
- **A1**: `View::signals_modified_refresh` 末尾调 `mark_derived_traces_dirty()`，修复 Math/Spectrum/Lissajous 波形不显示（当前需切 Tab 才出现）
- **A2**: `MainWindow::remove_tab` 销毁顺序修复 —— `destroy_context` 前加 `clear_all_documents_decoders(doc)` 停 decoder 线程；`deleteLater` 前加 `view->set_data_document(nullptr)` 解绑
- **A3**: `SigSession::Close()` + `~SigSession()` join glitch_filter/signal_invert 线程 + delete 对象；`_glitch_filter_running`/`_signal_invert_running` 改 `std::atomic<bool>`

### 阶段 2（P1 分层与并发加固）
- **B2**: `LogicSignal::commit_trig` 移除 7 处 `ds_trigger_*` 直调，只写 `SignalModel::set_trig_type()`；`view.cpp:983` `ds_trigger_get_en()` 改查 Core `trigger_config()`
- **C4**: `volatile bool _is_working`/`_copy_in_progress`/`volatile int _device_status` 改 `std::atomic`；CaptureOwnerGuard 内用锁统一更新三态，消除中间态窗口

### 阶段 3（P2 半成品收口）
- **B1.1**: 更新 AGENTS.md/project_memory.md，承认类型化事件总线当前是前置基础设施（0 消费者），移除"新代码必须用 IEventListener"的误导性硬约束
- **B1.2**（依赖 C5）: MainWindow 拆分后的子组件注册为 IEventListener；补全 26 个未翻译 DSV_MSG_* 的事件结构体与翻译表；处理 4 个双重死代码事件

### 阶段 4（P3 结构性重构，长期演进）
- **C5**: `MainWindow::OnMessage` 按职责拆分为多个处理器方法（设备切换/采集状态/UI 选项/数据更新/毛刺反相/触发/解码），OnMessage 退化为路由 switch
- **C1**: `SigSession` 拆分为 CaptureManager/DecodeTaskManager/DataFeedParser/DocumentRegistry/EventBus/FilterProcessor，SigSession 退化为 facade
- **C2**: `CMakeLists.txt` 拆分为 `cmake/deps.cmake`/`core_sources.cmake`/`gui_sources.cmake`/`decoders.cmake`/`install_packaging.cmake`，主文件用 `include()` 组装
- **C3**: `SessionDocument` 拆分为 SessionDocument（纯数据）+ SignalConfigStore（序列化），移除 DeviceAgent 耦合，UI 布局字段下沉 View 层 DockUiState，移除 `friend class TabContext`

## Impact
- **Affected specs**: `fix-all-architecture-issues`（B1 揭示 Task 3 是死代码，需修正其 spec 描述）、`decouple-core-from-view-v2`（C1/C3 延续 Core/View 分离）
- **Affected code**:
  - `PXView/pv/view/view.cpp`（A1、B2）
  - `PXView/pv/mainwindow.cpp`（A2、C5）
  - `PXView/pv/sigsession.h/.cpp`（A3、C1、C4）
  - `PXView/pv/view/logicsignal.cpp`（B2）
  - `PXView/pv/data/sessiondocument.h/.cpp`（C3）
  - `CMakeLists.txt` + `cmake/*.cmake`（C2）
  - `PXView/pv/interface/events.h`、`icallbacks.h`（B1.2）
  - `AGENTS.md`、`project_memory.md`（B1.1）

## ADDED Requirements

### Requirement: 派生 Trace 懒同步触发
The system SHALL ensure `View::signals_modified_refresh` marks derived traces dirty, so Math/Spectrum/Lissajous traces are created immediately after `math_rebuild`/`spectrum_rebuild`/`lissajous_rebuild`.

#### Scenario: Math 重建后波形立即显示
- **WHEN** 用户在 MathOptions 对话框确认启用 Math
- **THEN** MathTrace 立即创建并显示，无需切换 Tab

### Requirement: Tab 关闭无悬垂指针
The system SHALL ensure Tab removal joins decoder threads, detaches View→Document pointer, and deletes document before View receives any paint event.

#### Scenario: 关闭正在解码的 Tab
- **WHEN** 用户关闭持有捕获 owner 的 Tab
- **THEN** copy 线程 + decoder 线程均 join，View 解绑 document 指针，document 安全销毁，无 UAF

### Requirement: 后台线程生命周期管理
The system SHALL join all background threads (decode/copy/glitch_filter/signal_invert) in `SigSession::Close()` and destructor, and use `std::atomic` for cross-thread running flags.

#### Scenario: 应用关闭时毛刺滤波任务仍在运行
- **WHEN** app 关闭且 glitch_filter_task 仍在跑
- **THEN** Close() join 该线程后才销毁 _view_data，无 UAF

### Requirement: View 层不直调 libsigrok trigger API
The system SHALL route all trigger state changes through Core `SignalModel::set_trig_type()` / `set_trigger_config()`, with `ds_trigger_*` called only by `sync_trigger_to_libsigrok()`.

#### Scenario: 用户点 LogicSignal 触发边沿
- **WHEN** 用户在 LogicSignal 上设置边沿触发
- **THEN** 仅 SignalModel::set_trig_type 被调用，ds_trigger_probe_set 不被 View 层直接调用

### Requirement: 跨线程标志原子性
The system SHALL use `std::atomic` for all cross-thread state flags (_is_working/_copy_in_progress/_device_status/_glitch_filter_running/_signal_invert_running).

#### Scenario: 并发读取 is_working
- **WHEN** GUI 线程读取 is_working() 而 worker 线程写入
- **THEN** 无数据竞争 UB，弱内存架构上可见性正确

## MODIFIED Requirements

### Requirement: 类型化事件总线（fix-all-architecture-issues Task 3）
[原：18 事件结构体 + IEventListener + broadcast<T>，新代码强制用]
修改为：当前为前置基础设施（0 消费者、0 直接发射点）。B1.1 修正 AGENTS.md 措辞为"推荐接口，待 MainWindow 拆分后迁移"；B1.2 在 C5 完成后真正迁移并恢复硬约束。

### Requirement: MainWindow::OnMessage
[原：504 行/39 case 上帝方法，直接操控 10+ widget]
修改为：按职责拆分为多个处理器方法，每个处理器注册为 IEventListener 消费对应类型化事件；OnMessage 退化为 < 80 行路由 switch。

### Requirement: SigSession
[原：3219 行/49 成员/20+ 职责上帝类]
修改为：拆分为 CaptureManager/DecodeTaskManager/DataFeedParser/DocumentRegistry/EventBus/FilterProcessor，SigSession 退化为协调 facade，持有各 manager 的 unique_ptr。

### Requirement: SessionDocument
[原：6 角色混淆 + DeviceAgent 耦合 + UI 布局字段 + friend TabContext]
修改为：纯数据模型，序列化下沉 SignalConfigStore，UI 布局字段下沉 View 层 DockUiState，移除 DeviceAgent 依赖与 friend 声明。

### Requirement: CMakeLists.txt
[原：1889 行单文件混合依赖/源清单/安装/打包/测试]
修改为：拆分为 `cmake/*.cmake` 多文件，主 CMakeLists.txt 用 `include()` 组装，行数 < 100。

## REMOVED Requirements

### Requirement: AGENTS.md "新代码必须用 IEventListener" 硬约束
**Reason**: 调研发现 0 消费者、0 直接发射点，硬约束误导开发者认为已生效。
**Migration**: B1.1 改为"推荐接口，待 C5 拆分后迁移"；B1.2 在 C5 完成后真正迁移并恢复硬约束。
