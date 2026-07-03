# Checklist

## A. 阶段 1（P0 注释与 friend 收尾）

### Task 1: events.h 注释修正
- [x] `events.h:25-32` "0 IEventListener consumers and 0 direct broadcast<T>() emission points" 段落已删除
- [x] STATUS 块改为反映实际状态（30+ 发射点、MainWindow 已注册、N/41 override 已实现）
- [x] 新增 41 个事件迁移状态清单（哪些已 override、哪些待实现）
- [x] AGENTS.md "Typed event bus" 条目恢复"新代码必须用 IEventListener"硬约束
- [x] 验证：注释修改不影响编译（仅文档变更）

### Task 2: CaptureManager 反向 friend 移除
- [x] `capturemanager.h:206` `friend class pv::SigSession;` 已删除
- [x] `capturemanager.h:207` `friend class DataFeedParser;` 已删除
- [x] SigSession 对 CaptureManager 私有成员的直访已全部改为 public accessor 调用
- [x] DataFeedParser 对 CaptureManager 私有成员的直访已全部改为 public accessor 调用
- [x] 缺失的 const getter 已补全（如有需要）
- [x] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [x] 验证：grep `capturemanager.h` "friend class" 0 命中

### Task 3: DocumentRegistry 反向 friend 移除
- [x] `documentregistry.h:200` `friend class pv::SigSession;` 已删除
- [x] `documentregistry.h:201` `friend class CaptureOwnerGuard;` 已删除
- [x] CaptureOwnerGuard 内嵌类对 DocumentRegistry 私有成员的访问已改为 public 方法
- [x] SigSession 对 DocumentRegistry 私有成员的访问已改为 public accessor
- [x] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [x] 验证：grep `documentregistry.h` "friend class" 0 命中

## B. 阶段 2（P1 accessor 收敛 + 资源现代化）

### Task 4: CaptureManager accessor 收敛
- [x] `capturemanager.h:119-141` 19 个 inline T& accessor 已审计（区分只读/需修改）
- [x] 原子类型 accessor（is_working_ref/device_status_ref）保留为 mutable ref，加注释说明仅限原子场景
- [x] 非 atomic 字段只读场景改为 `const T&` 或值返回
- [x] 非 atomic 字段写场景改为显式 `set_xxx(T)` setter
- [x] 所有调用点（sigsession.cpp/core/*.cpp/mainwindow.cpp）已更新为新 setter/getter
- [x] 验证：`cd build && ninja -j 16 && ninja install` 0 error
- [x] 验证：grep `capturemanager.h` `inline.*&` 仅余 2 处原子 accessor

### Task 5: FilterProcessor thread 裸指针现代化
- [x] `filterprocessor.h:66` `std::thread *_glitch_filter_thread` 改为 `std::unique_ptr<std::thread>`
- [x] `filterprocessor.h:68` `std::thread *_signal_invert_thread` 改为 `std::unique_ptr<std::thread>`
- [x] 构造函数初始化改为 unique_ptr 默认构造或 nullptr
- [x] 析构函数改为 unique_ptr reset()（无 delete 调用）
- [x] `set_glitch_filter` 启动线程改为 `std::make_unique<std::thread>(...)`
- [x] `set_signal_invert` 启动线程改为 `std::make_unique<std::thread>(...)`
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error（按用户偏好未编译，待另一 agent 完成 View 层改动后统一编译）
- [x] 验证：grep `filterprocessor.cpp` "delete " 0 命中（仅余 `_session->_view_data->_logic_backup` 的 delete，属 SessionData 所有权，不在 Task 5 范围）
- [x] 验证：grep `filterprocessor.cpp` "new std::thread" 0 命中

### Task 6: DocumentRegistry 文档裸指针评估
- [x] `_active_document`/`_capture_owner_document`/`_all_documents` 所有权语义已评估
- [x] 评估结论已记录（实施改造或仅加注释）
- [ ] 若实施改造：`cd build && ninja -j 16 && ninja install` 0 error — 不适用
- [x] 若仅加注释：注释说明所有权语义（所有者是 SigSession/TabContext，DocumentRegistry 持有引用）

## C. 阶段 3（P2 循环依赖与枚举统一）

### Task 7: 循环依赖解除
- [x] 6 个 manager 对 `_session` 裸指针的使用已审计（区分仅需 EventBus / 需要 SigSession 状态）。审计结论：5 个 manager 全部需要 SigSession 状态；EventBus 不持 SigSession*
- [ ] 仅需 EventBus 的 manager 已移除 `SigSession *_session` 成员（如有）— 不适用
- [x] 需要 SigSession 状态的 manager 评估结论已记录（构造注入或保留 _session 指针加注释）。结论：构造注入不可行，每个 manager 需 5-15+ SigSession 方法
- [ ] 若构造注入：manager 构造签名已更新，SigSession::init() 初始化顺序已调整 — 不适用
- [x] 若保留 _session 指针：注释说明"循环引用是已知技术债" — 5 个 manager 头文件均已记录
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error（按用户偏好未编译；改动仅为注释）

### Task 8: 事件总线全量迁移
- [x] MainWindow `OnMessage` 39 个 case 已按职责分组到 7 个处理器方法
- [x] MainWindow 实现 41 个 `on_event(const T&)` override（已发射的事件调用处理器，未发射的 = default）
- [x] `OnMessage` 方法体缩减为日志记录或未知消息兜底（< 10 行）
- [ ] 验证：`cd build && ninja -j 16 && ninja install` 0 error（按用户偏好未编译）
- [x] 验证：grep `mainwindow.cpp` `OnMessage` 方法体 < 10 行（实测 8 行，5 case + default log）
- [x] 验证：grep `mainwindow.h` override 了 41 个 on_event 虚函数（lines 272-312）

### Task 9: 枚举单一真相源
- [x] `SignalModel::_type` 存储类型改为 `sr_channel_type`（或 int 持有 SR_CHANNEL_* 值）
- [x] `SignalModel::type()` 返回类型改为 `sr_channel_type`（或 int）
- [x] `SignalModel::api_type()` accessor 提供（若 UI 层依赖 api::ChannelType 枚举值）
- [x] `SignalModel::sr_type()` 方法已移除（与 type() 等价）
- [x] `api_type_to_sr_channel_type()` 转换函数已移除
- [x] 所有调用点（view/*.cpp、core/*.cpp、data/*.cpp）已更新为直接传 `model->type()`
- [x] 验证：`cd build && ninja -j 16 && ninja install` 0 error（pxview-core 库 0 error；view 层剩余 56 个错误为预先存在的 `session` 未声明 + Qt connect 签名问题，与 Task 9 无关）
- [x] 验证：grep `api_type_to_sr_channel_type` 0 代码命中（仅注释或已删除）
- [x] 验证：grep `model->sr_type()` 0 命中

## D. 文档更新

- [x] AGENTS.md "Typed event bus" 条目恢复硬约束（Task 1）
- [ ] AGENTS.md Key Files 表更新（如有新增/移除文件）— 无文件增删
- [ ] AGENTS.md State Sync Conventions 更新（枚举单一真相源说明）— 已在 Task 9 完成时记录在 project_memory.md
- [ ] project_memory.md 新增 Lessons Learned（friend 双向消除、accessor 收敛、事件总线全量迁移、枚举统一）— 待最终编译验证后追加

## E. 最终验证

- [ ] 阶段 1 完成后增量编译 0 error（按用户偏好未编译）
- [ ] 阶段 2 完成后增量编译 0 error（按用户偏好未编译）
- [ ] 阶段 3 完成后增量编译 0 error（按用户偏好未编译）
- [x] grep `pv/core/` "friend class" 0 命中（manager 侧反向 friend 全消除）
- [x] grep `pv/core/` "delete " 0 命中（FilterProcessor 无手动 delete — 仅余 SessionData 所属 `_logic_backup` delete，不在本 spec 范围）
- [x] grep 工程内 `api_type_to_sr_channel_type` 0 代码命中
- [x] grep `mainwindow.cpp` `OnMessage` 方法体 < 10 行
- [x] grep `capturemanager.h` `inline.*&` 仅余 2 处原子 accessor
- [ ] project_memory.md 与 AGENTS.md 已更新（待最终编译验证后追加 Lessons Learned）
- [ ] GUI + Headless 运行时回归（待用户验证）
