# 加固剩余崩溃风险 Spec

## Why

前一轮架构分析识别出 8 类崩溃风险。其中大部分（R5 线程回归、`_capture_owner_document` 生命周期、`on_frame_ended` 双启动、trig_type 持久化、mmap 异步写、双缓冲 swap 等）已由 `fix-state-sync-gaps-v2` 与 `fix-mmap-async-crash-risks` 修复。

但仍有若干问题只是「单点修复」或「约定式约束」，未在类型系统/RAII/接口边界层面强制——任何新增调用点或重构都可能复发崩溃。本 spec 把这些剩余风险做结构性加固，让约束变成「无法绕过」而非「需记住」。

## What Changes

- **P0** 全局审计 `assert(ptr)` 守卫：在 Release 构建中 `assert` 是空操作，凡是用 `assert` 守卫指针随后解引用的位置，补显式 `if(!ptr)` 检查或早期 return。
- **P1** `get_decoder_annotations` 用裸指针字符串匹配 DecoderStack：改为句柄 ID + 版本号，栈销毁/重建后旧 ID 不可命中。
- **P1** 移除/重命名误导性 API `effective_data_source()`：当前 View 内已改用 `_data_source`，但 API 仍存在，新增调用点误用即复发 `traces=1` 越界。
- **P1** `SignalModel::type()` 根因修复：当前返回 `api::ChannelType`（0/1/2），libsigrok 期望 `SR_CHANNEL_*`（10000+）。在 DataSource 接口边界强制转换，根除调用点记忆负担。
- **P2** `add_decode_task` 设为 private：强制所有解码启动路径走 `start_all_decode_tasks()` / `rst_decoder()` 漏斗，编译期保证 `attach_data_to_signal` 不被遗漏。
- **P2** `rebuild_signals_from_config` 重入护栏：引入 `RebuildGuard` RAII，重入时直接 return，把「不要广播」约定升级为「无法重入」。
- **P2** `DSV_MSG_CAPTURE_OWNER_CHANGED` 携带 `is_working` 标志：当前订阅者必须各自判 `is_working()`，新增订阅者漏判即崩溃。改为消息负载携带标志，或拆分为 IDLE/WORKING 两个消息。
- **P2** 同步等待入口 `on_main_thread()` 审计：`add_analyzer` 死锁已修，但 `wait_capture_complete` 等其他同步等待路径仍有主线程自死锁风险，统一加前置检查。

## Impact

- Affected specs:
  - `fix-state-sync-gaps-v2`（前置，已完成）—— 本 spec 在其基础上做结构性加固，不重复修复
  - `fix-mmap-async-crash-risks`（前置，已完成）—— 磁盘缓存错误冒泡不在本 spec 范围
  - `decouple-core-from-view-v2`（前置，已完成）—— DataSource 接口为本 spec 的 P1 修复点
  - `tiered-driver-compat-fix`（进行中，独立）—— 驱动 compat 与本 spec 不冲突
- Affected code:
  - `PXView/pv/sigsession.h/cpp` — `add_decode_task` 可见性、`rebuild_signals_from_config` 护栏、广播负载
  - `PXView/pv/data/datasource.h` — 类型转换边界
  - `PXView/pv/data/signalmodel.h/cpp` — `type()` 返回值或新增 `sr_type()`
  - `PXView/pv/view/view.h/cpp` — 移除 `effective_data_source()`
  - `PXView/pv/api/session_service.cpp` — `get_decoder_annotations` 句柄匹配
  - `PXView/pv/interface/icallbacks.h` — 广播负载结构（如改消息携带标志）
  - 全项目 40 个文件 201 处 `assert(ptr)` — 审计后选择性补 `if(!ptr)`

## ADDED Requirements

### Requirement: Release 构建下的空指针防御
系统 SHALL 在所有「`assert(ptr)` 后立即解引用 `ptr`」的位置，提供 Release 构建也生效的空指针防御（显式 `if(!ptr)` 早期 return 或抛异常）。`assert` 仅作为开发期断言，不得作为唯一的空指针守卫。

#### Scenario: Release 构建遇到空指针
- **WHEN** Release 构建中某回调/数据路径传入 NULL 指针，且该路径存在 `assert(ptr)` 守卫
- **THEN** 系统 不 SHALL 解引用 NULL 导致崩溃，而是早期 return 并记录日志

### Requirement: DecoderStack 句柄稳定性
系统 SHALL 用稳定的句柄 ID + 版本号标识 DecoderStack，而非裸指针字符串。栈销毁后其句柄 ID 不可被新栈复用命中；栈重建后版本号递增使旧句柄失效。

#### Scenario: MCP 请求命中已销毁栈
- **WHEN** MCP `get_analyzer_results` 请求携带已销毁栈的句柄 ID
- **THEN** 系统 SHALL 返回 `DecoderNotFound`，不 SHALL 访问已释放内存

#### Scenario: 栈重建后旧句柄失效
- **WHEN** `restart_decoders` 销毁旧栈并创建新栈
- **THEN** 旧栈句柄 ID 的版本号 SHALL 不匹配新栈，旧 ID 请求返回 `DecoderNotFound`

### Requirement: DataSource 接口类型边界
DataSource 接口 SHALL 在返回 `SignalModel` 的类型信息时，提供 libsigrok 期望的 `SR_CHANNEL_*` 值（LOGIC=10000/DSO=10001/ANALOG=10002），调用点 SHALL NOT 需要手动调用 `api_type_to_sr_channel_type()` 转换。

#### Scenario: 调用 get_snapshot
- **WHEN** 任何调用点通过 DataSource 接口获取通道类型并传给 `get_snapshot()`
- **THEN** 类型值 SHALL 直接是 `SR_CHANNEL_*`，无需调用点转换

### Requirement: 解码启动漏斗强制
`add_decode_task` SHALL 为 private，外部调用点 SHALL 只能通过 `start_all_decode_tasks()` 或 `rst_decoder()` 启动解码任务，这两个公开方法 SHALL 在启动任务前调用 `attach_data_to_signal()`。

#### Scenario: 新增解码启动路径
- **WHEN** 开发者新增一个解码启动路径
- **THEN** 编译 SHALL 强制其走 `start_all_decode_tasks()` / `rst_decoder()`，无法直接调用 `add_decode_task`

### Requirement: rebuild 重入护栏
`rebuild_signals_from_config` SHALL 通过 RAII 护栏防止重入。重入时 SHALL 直接 return，不执行重建逻辑，不广播任何消息。

#### Scenario: 广播循环复发
- **WHEN** `rebuild_signals_from_config` 内部（误）广播 `DEVICE_OPTIONS_UPDATED`，触发 OnMessage 再次调用 rebuild
- **THEN** 重入 SHALL 被护栏拦截直接 return，不 SHALL 无限递归栈溢出

### Requirement: CAPTURE_OWNER_CHANGED 携带工作状态
`DSV_MSG_CAPTURE_OWNER_CHANGED` SHALL 在消息负载中携带 `is_working` 标志，订阅者 SHALL 根据负载标志决定是否 activate，不 SHALL 各自调用 `is_working()` 查询。

#### Scenario: 采集进行中 owner 变更
- **WHEN** `start_capture` 设置 owner 后广播 `CAPTURE_OWNER_CHANGED`，此时 `is_working=true`
- **THEN** 订阅者 SHALL 跳过 `activate()`，不 SHALL 重建波形轨道

## MODIFIED Requirements

### Requirement: effective_data_source 接口
**原行为**：`View::effective_data_source()` 在 `has_data()` 时返回 `_document`，否则返回 `_session`。`SessionDocument::_signal_models` 从不填充，误用导致空 vector → AllReplaced 删除所有 Signal → Header 越界。

**新行为**：移除 `effective_data_source()`。SignalModel 的唯一来源是 `SigSession`（经 `_data_source`）。需要快照数据时显式调用 `document_snapshot_source()` 或类似命名方法，方法名 SHALL 表明其只返回快照不返回 SignalModel。

## REMOVED Requirements

### Requirement: effective_data_source 作为 SignalModel 源
**Reason**：`SessionDocument::_signal_models` 永不填充，该 API 语义误导，已导致一次 `traces=1` 越界 bug。
**Migration**：所有 `effective_data_source()` 调用点改为 `_data_source`（= `_session`）。需要快照的路径改用 `document_snapshot_source()`（重命名后）。
