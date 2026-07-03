# modernize-core-layer-final Spec

## Why

前序 spec `fix-remaining-architecture-issues` 阶段 3 与 `purify-architecture-concepts` Task 19 声称完成了"类型化事件总线迁移"和"friend 移除"，但实际代码核查发现两者均为半成品：

- **事件总线注释与代码矛盾**：[events.h:25-32](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/interface/events.h#L25) 头部注释写"0 IEventListener consumers and 0 direct broadcast<T>() emission points"，但实际 sigsession.cpp 已有 30+ 处 `broadcast<T>()` 调用、decodetaskmanager.cpp:189 也调用了。MainWindow 已注册 IEventListener 但只 override 了 1 个 `on_event(CaptureStateChanged)`，且实现是 re-dispatch 回 OnMessage 老路径——41 个事件 40 个未被消费。
- **friend 反模式仅单向移除**：`purify-architecture-concepts` Task 19 删除了 SigSession 侧的 `friend class core::XxxManager`，但 manager 侧反向 friend 仍残留 4 处（CaptureManager→SigSession+DataFeedParser，DocumentRegistry→SigSession+CaptureOwnerGuard）。
- **accessor 暴露变相 public**：[CaptureManager.h:119-141](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/core/capturemanager.h#L119) 把 19 个 private 字段全部以 `inline T&` 公有返回，封装边界完全穿透——拆类只是把"private 字段在 SigSession"换成"public ref accessor 在 CaptureManager"。
- **循环依赖**：6 个 manager 全部持有 `EventBus *_event_bus; SigSession *_session;` 双裸指针，SigSession 又持有 manager 的 `unique_ptr`——双向引用未通过依赖注入解耦。
- **裸指针资源**：[FilterProcessor](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/core/filterprocessor.cpp) 用 `std::thread *_glitch_filter_thread` 裸指针 + 手动 new/delete/join（filterprocessor.cpp:14, 27-38, 64-69, 219-224），违反 RAII 原则。
- **枚举双真相源**：[signalmodel.h:67](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/signalmodel.h#L67) `type()` 返回 `api::ChannelType` (0/1/2)，但 libsigrok 期望 `SR_CHANNEL_*` (10000+)，靠 `sr_type()` 转换函数同步——这是典型的"两个真相源靠约定同步"陷阱（已被 project_memory Lessons Learned 记录但仍未消除）。

本 spec 收口这 6 项技术债，按风险递增分 3 阶段实施。

## What Changes

### 阶段 1（P0 低风险高回报：注释与 friend 收尾）

- **A1**: 修正 [events.h:25-32](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/interface/events.h#L25) 头部注释——删除"0 消费者、0 发射点"过时声明，改为反映实际状态（30+ 发射点、MainWindow 已注册、40/41 事件待消费 override 实现）
- **A2**: 移除 CaptureManager 2 处反向 friend：`friend class pv::SigSession`（capturemanager.h:206）+ `friend class DataFeedParser`（capturemanager.h:207）——改为通过 CaptureManager 已有的 public accessor 访问，或新增必要的 const getter
- **A3**: 移除 DocumentRegistry 2 处反向 friend：`friend class pv::SigSession`（documentregistry.h:200）+ `friend class CaptureOwnerGuard`（documentregistry.h:201）——CaptureOwnerGuard 是内嵌类，改为内嵌类内通过 public 方法访问；SigSession 改用 DocumentRegistry 现有 public accessor

### 阶段 2（P1 中风险：accessor 收敛 + 资源现代化）

- **B1**: CaptureManager 19 个 `inline T&` accessor 收敛为方法而非字段引用——保留必要的 setter（如 `set_is_working(bool)`、`set_device_status(int)`），其他只读字段改为 `const T&` 或值返回；引用返回的 mutable accessor 仅在确有跨线程原子需求时保留
- **B2**: FilterProcessor 的 `std::thread *_glitch_filter_thread` 裸指针改为 `std::unique_ptr<std::thread>`（或直接 `std::thread` + `joinable()` 检查），消除手动 new/delete——同样处理 `_signal_invert_thread`
- **B3**: 评估 DocumentRegistry 的 `_active_document`/`_capture_owner_document`/`_all_documents` 裸指针——所有者仍是 SigSession（通过 SessionDocument* 注入），文档保留为裸指针但加注释说明所有权语义；若可改为 SessionDocument 自身用 shared_ptr 持有则一并改造（高风险，评估后决定）

### 阶段 3（P2 较高风险：循环依赖与枚举统一）

- **C1**: 解除 manager ↔ SigSession 双向裸指针循环依赖——保持 manager 持有 `EventBus*`（已是 unique_ptr 持有，借用合理）；评估 `SigSession*` 是否可改为只持有 EventBus，所需 SigSession 状态通过 manager 构造时注入的引用或回调传递
- **C2**: 完成事件总线全量迁移——MainWindow 实现剩余 40 个 `on_event` override（按职责分组到现有 7 个处理器方法），移除 OnMessage 39-case 路由 switch（或保留为兜底空函数仅记录未处理消息）
- **C3**: 消除 `api::ChannelType` 与 `SR_CHANNEL_*` 双真相源——SignalModel 内部统一存储 `sr_channel_type` 值，`type()` 直接返回 `sr_channel_type`（或新增 `api_type()` accessor），移除 `api_type_to_sr_channel_type()` 转换函数及其所有调用点

## Impact

- **Affected specs**:
  - `fix-remaining-architecture-issues`（B1.2 阶段 3 声称"MainWindow IEventListener 注册"完成，实际仅 1/41 override——本 spec 阶段 3 C2 真正完成全量迁移）
  - `purify-architecture-concepts`（Task 19 声称"5 处 friend 全删"，实际仅删 SigSession 侧 5 处，manager 侧 4 处反向 friend 残留——本 spec 阶段 1 A2/A3 真正完成 friend 消除）
- **Affected code**:
  - `PXView/pv/interface/events.h`（A1 注释修正）
  - `PXView/pv/core/capturemanager.h/.cpp`（A2 friend 移除 + B1 accessor 收敛）
  - `PXView/pv/core/documentregistry.h/.cpp`（A3 friend 移除 + B3 文档指针评估）
  - `PXView/pv/core/filterprocessor.h/.cpp`（B2 thread 裸指针现代化）
  - `PXView/pv/core/decodetaskmanager.h/.cpp`、`PXView/pv/core/datafeedparser.h/.cpp`（A2/A3 friend 移除影响）
  - `PXView/pv/sigsession.h/.cpp`（A2/A3 friend 移除影响 + C1 循环依赖解除 + C3 枚举统一）
  - `PXView/pv/mainwindow.h/.cpp`（C2 事件总线全量迁移）
  - `PXView/pv/data/signalmodel.h/.cpp`（C3 枚举统一）
  - `PXView/pv/data/signalfactory.cpp`、`PXView/pv/view/*.cpp`（C3 影响所有用 type() 的边界点）
  - `AGENTS.md`、`project_memory.md`（文档更新）

## ADDED Requirements

### Requirement: 事件总线注释反映实际状态

The system SHALL keep `events.h` header comments accurate. The "STATUS" block SHALL reflect the actual number of IEventListener consumers and broadcast<T>() emission points, not stale "0 consumers / 0 emission points" claims.

#### Scenario: events.h 注释准确
- **WHEN** 开发者打开 `events.h`
- **THEN** STATUS 块描述当前实际状态：30+ broadcast<T>() 发射点、MainWindow 已注册为 IEventListener、N/41 事件已实现 override
- **AND** 不出现"0 consumers / 0 emission points"过时声明

### Requirement: Manager 反向 friend 完全消除

The system SHALL NOT declare `friend class SigSession` or any sibling manager friend in `CaptureManager`/`DocumentRegistry`/`DecodeTaskManager`/`DataFeedParser`/`FilterProcessor` header files. Managers SHALL access SigSession state via public accessor methods or constructor-injected references, not friend access to private members.

#### Scenario: CaptureManager 无 friend 声明
- **WHEN** 检查 `capturemanager.h`
- **THEN** 文件内 `grep "friend class"` 0 命中

#### Scenario: DocumentRegistry 无 friend 声明
- **WHEN** 检查 `documentregistry.h`
- **THEN** 文件内 `grep "friend class"` 0 命中（CaptureOwnerGuard 内嵌类通过 public 方法访问 DocumentRegistry，不依赖 friend）

#### Scenario: DataFeedParser 不通过 friend 访问 CaptureManager
- **WHEN** DataFeedParser 需要读写 capture_data / view_data / is_triged 等字段
- **THEN** 通过 `_session->capture_manager()->xxx()` public accessor 访问，不依赖 friend 直访

### Requirement: CaptureManager accessor 收敛为方法

The system SHALL NOT expose CaptureManager private fields via `inline T&` mutable reference accessors that effectively make them public. Mutable accessors SHALL be explicit setter methods (e.g. `set_is_working(bool)`). Read-only accessors SHALL return `const T&` or by value.

#### Scenario: is_working 访问方式
- **WHEN** 外部代码需要修改 _is_working
- **THEN** 调用 `set_is_working(bool)`，不获取 `is_working_ref()` 引用直接赋值

#### Scenario: view_data 访问方式
- **WHEN** 外部代码需要读取 view_data
- **THEN** 调用 `view_data()` 返回 `SessionData*`（值返回或 const 引用），不通过 `SessionData*&` mutable 引用修改

### Requirement: FilterProcessor 后台线程 RAII 管理

The system SHALL manage background threads (`_glitch_filter_thread` / `_signal_invert_thread`) using `std::unique_ptr<std::thread>` or direct `std::thread` member + `joinable()` check, NOT raw `std::thread*` pointers with manual new/delete.

#### Scenario: 析构自动清理
- **WHEN** FilterProcessor 析构
- **THEN** unique_ptr 自动销毁 thread 对象（先 join 再析构），不出现 `delete _thread` 手动释放代码

#### Scenario: 启动新线程
- **WHEN** `set_glitch_filter` 启动新 filter 线程
- **THEN** 通过 `std::make_unique<std::thread>(...)` 或 `= std::thread(...)` 赋值，不出现 `new std::thread`

### Requirement: 枚举单一真相源

The system SHALL treat `sr_channel_type` (SR_CHANNEL_LOGIC/DSO/ANALOG, 10000+) as the single source of truth for channel type. `api::ChannelType` (0/1/2) SHALL NOT coexist as a parallel enum requiring conversion. `SignalModel::type()` SHALL return `sr_channel_type` directly, and `api_type_to_sr_channel_type()` conversion function SHALL be removed.

#### Scenario: SignalModel::type() 返回 sr_channel_type
- **WHEN** 检查 `signalmodel.h` `type()` 方法签名
- **THEN** 返回类型是 `sr_channel_type`（或 `int` 持有 SR_CHANNEL_* 值），不是 `api::ChannelType`

#### Scenario: 无 api_type_to_sr_channel_type 转换函数
- **WHEN** grep 工程内 `api_type_to_sr_channel_type`
- **THEN** 0 代码命中（仅注释或已删除）

#### Scenario: libsigrok 边界无类型转换
- **WHEN** 调用 libsigrok API 需要 channel type
- **THEN** 直接传 `model->type()`，不需要 `model->sr_type()` 转换层

### Requirement: 事件总线全量迁移完成

The system SHALL migrate MainWindow from the 39-case `OnMessage(int)` switch to typed `on_event( const T&)` overrides for all 41 event structs. The legacy `OnMessage` SHALL be removed or reduced to a logging-only fallback for unknown messages.

#### Scenario: MainWindow 实现全部 on_event override
- **WHEN** 检查 `mainwindow.h` MainWindow 类声明
- **THEN** override 了 IEventListener 全部 41 个 `on_event(const T&)` 虚函数（或显式 = default 跳过不关心的）

#### Scenario: OnMessage 不再是主路由
- **WHEN** 检查 `mainwindow.cpp` `OnMessage` 方法
- **THEN** 方法体不超过 10 行，仅做日志记录或直接 return，无 case-by-case 状态分发逻辑

## MODIFIED Requirements

### Requirement: 类型化事件总线（fix-remaining-architecture-issues 阶段 3）
[原：B1.2 已完成 41 个事件结构体 + 38/43 翻译表 + MainWindow IEventListener 注册]
修改为：41 个事件结构体已建，30+ broadcast<T>() 发射点已存在，但 MainWindow 仅 override 1/41 事件（CaptureStateChanged），且实现是 re-dispatch 回 OnMessage。本 spec 阶段 3 C2 真正完成全量 on_event override 迁移。

### Requirement: SigSession manager friend 消除（purify-architecture-concepts Task 19）
[原：5 处 `friend class core::XxxManager` 全删，manager 通过 accessor 访问]
修改为：SigSession 侧 5 处 friend 已删，但 manager 侧 4 处反向 friend 残留（CaptureManager→SigSession+DataFeedParser，DocumentRegistry→SigSession+CaptureOwnerGuard）。本 spec 阶段 1 A2/A3 真正完成双向 friend 消除。

## REMOVED Requirements

### Requirement: events.h "0 consumers / 0 emission points" 注释
**Reason**: 注释与实际代码矛盾——已有 30+ broadcast<T>() 发射点和 1 个 IEventListener 消费者，注释误导开发者认为事件总线是死代码。
**Migration**: A1 修正注释为实际状态描述。

### Requirement: api_type_to_sr_channel_type() 转换函数
**Reason**: api::ChannelType 与 sr_channel_type 双真相源靠转换函数同步是脆弱设计，已被 project_memory Lessons Learned 记录为陷阱但未消除。
**Migration**: C3 SignalModel 内部统一存储 sr_channel_type，移除转换函数及所有调用点。

### Requirement: FilterProcessor 裸指针 std::thread*
**Reason**: 手动 new/delete/join 违反 RAII 原则，易在异常路径泄漏。
**Migration**: B2 改为 std::unique_ptr<std::thread> 或直接 std::thread 成员。
