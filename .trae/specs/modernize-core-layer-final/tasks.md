# Tasks

## 阶段 1（P0 低风险高回报：注释与 friend 收尾）

- [ ] Task 1: 修正 events.h 头部过时注释
  - [ ] SubTask 1.1: 删除 `events.h:25-32` "0 IEventListener consumers and 0 direct broadcast<T>() emission points" 段落，改为反映实际状态："30+ broadcast<T>() emission points in sigsession.cpp/decodetaskmanager.cpp; MainWindow registered as IEventListener; N/41 on_event overrides implemented (see migration status table below)"
  - [ ] SubTask 1.2: 在 STATUS 块下方新增迁移状态清单（41 个事件列出哪些已 override、哪些待实现），方便后续开发者追踪进度
  - [ ] SubTask 1.3: 同步更新 AGENTS.md "Typed event bus (HARD CONSTRAINT)" 条目——恢复"新代码必须用 IEventListener"硬约束（因事件总线已基础运转，不再是死代码）
  - [ ] SubTask 1.4: 验证：注释修改不影响编译（仅文档变更）

- [ ] Task 2: 移除 CaptureManager 反向 friend
  - [ ] SubTask 2.1: 检查 `capturemanager.h:206-207` 两处 friend 的实际使用——grep SigSession/DataFeedParser 实现中对 CaptureManager 私有成员的直访点
  - [ ] SubTask 2.2: 将所有 `_session->capture_manager()->_xxx` 私有直访改为 `_session->capture_manager()->xxx()` public accessor 调用；若有缺失的 accessor 则补全（参考已有 19 个 inline accessor，部分需新增 const 版本）
  - [ ] SubTask 2.3: DataFeedParser 对 CaptureManager 的访问同样改走 public accessor（feed_in_logic/feed_in_dso 等方法内）
  - [ ] SubTask 2.4: 删除 `capturemanager.h:206` `friend class pv::SigSession;` + `:207` `friend class DataFeedParser;`
  - [ ] SubTask 2.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error；grep `capturemanager.h` "friend class" 0 命中

- [ ] Task 3: 移除 DocumentRegistry 反向 friend
  - [ ] SubTask 3.1: 检查 `documentregistry.h:200-201` 两处 friend 的实际使用——grep SigSession/CaptureOwnerGuard 对 DocumentRegistry 私有成员的直访点
  - [ ] SubTask 3.2: CaptureOwnerGuard 是内嵌类（documentregistry.h:60-75），改为通过 DocumentRegistry public 方法访问（如 `acquire_capture_owner`/`release_capture_owner`/`get_capture_owner_document` 等已有方法）；若需新增方法则补全
  - [ ] SubTask 3.3: SigSession 对 DocumentRegistry 的访问改走 public accessor（已有 `signal_models()`/`spectrum_stacks()`/`trigger_config()` 等）
  - [ ] SubTask 3.4: 删除 `documentregistry.h:200` `friend class pv::SigSession;` + `:201` `friend class CaptureOwnerGuard;`
  - [ ] SubTask 3.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error；grep `documentregistry.h` "friend class" 0 命中

## 阶段 2（P1 中风险：accessor 收敛 + 资源现代化）

- [ ] Task 4: CaptureManager 19 个 inline T& accessor 收敛为方法
  - [ ] SubTask 4.1: 审计 `capturemanager.h:119-141` 19 个 accessor 的实际使用模式——区分"只读访问"（应改 const& 或值返回）与"需要修改"（应改显式 setter）
  - [ ] SubTask 4.2: 原子类型（`std::atomic<bool>& is_working_ref()`、`std::atomic<int>& device_status_ref()`）保留为 mutable ref accessor（原子操作需要 ref 才能调用 store/load），但加注释说明仅限原子场景
  - [ ] SubTask 4.3: 非 atomic 字段（`SessionData*& view_data()`、`SessionData*& capture_data()`、`std::vector<SessionData*>& data_list()`、`bool& is_triged()`、`QDateTime& trig_time()`、`bool& trigger_flag()`、`uint8_t& trigger_ch()`、`bool& hw_replied()`、`bool& bClose()`、`bool& is_saving()`、`sr_status& dso_status()`、`bool& dso_status_valid()`、`int& error()`、`uint64_t& error_pattern()`、`uint64_t& save_start()`、`uint64_t& save_end()`、`QDateTime& session_time()`、`int& map_zoom()`）改为：只读场景返回 `const T&` 或值；写场景提供显式 `set_xxx(T)` setter
  - [ ] SubTask 4.4: 更新所有调用点（grep `_capture_manager->xxx()` 在 sigsession.cpp/core/*.cpp/mainwindow.cpp 的所有命中）改用新 setter/getter
  - [ ] SubTask 4.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error；grep `capturemanager.h` `inline.*&` 仅余 2 处原子 accessor（is_working_ref/device_status_ref）

- [ ] Task 5: FilterProcessor thread 裸指针现代化
  - [ ] SubTask 5.1: `filterprocessor.h:66-68` `std::thread *_glitch_filter_thread` 改为 `std::unique_ptr<std::thread>`；`std::thread *_signal_invert_thread` 同样改为 `std::unique_ptr<std::thread>`
  - [ ] SubTask 5.2: `filterprocessor.cpp:14-15` 构造函数初始化改为 `nullptr`（unique_ptr 默认构造即可，无需显式初始化）
  - [ ] SubTask 5.3: `filterprocessor.cpp:27-38` 析构函数改为：`if (_glitch_filter_thread) { if (_glitch_filter_thread->joinable()) _glitch_filter_thread->join(); _glitch_filter_thread.reset(); }`（unique_ptr 自动释放，无需 delete）
  - [ ] SubTask 5.4: `filterprocessor.cpp:64-69` `set_glitch_filter` 启动线程改为 `_glitch_filter_thread = std::make_unique<std::thread>(...)`；`filterprocessor.cpp:219-224` `set_signal_invert` 同样改为 `std::make_unique<std::thread>`
  - [ ] SubTask 5.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error；grep `filterprocessor.cpp` "delete" 0 命中、"new std::thread" 0 命中

- [ ] Task 6: 评估 DocumentRegistry 文档裸指针现代化（评估后决定是否实施）
  - [ ] SubTask 6.1: 评估 `_active_document`/`_capture_owner_document`/`_all_documents` 裸指针的所有权语义——所有者是 SigSession 还是外部 TabContext？改为 shared_ptr/weak_ptr 是否需要全局影响？
  - [ ] SubTask 6.2: 若评估结论为"改动风险高且收益低"（所有者明确、生命周期已由 CaptureOwnerGuard 管理），仅加注释说明所有权语义，不实施改造
  - [ ] SubTask 6.3: 若评估结论为"可安全改造"，则 `_all_documents` 改为 `std::vector<std::weak_ptr<SessionDocument>>` 或 `std::vector<std::shared_ptr<SessionDocument>>`（取决于 TabContext 持有方式）
  - [ ] SubTask 6.4: 验证（仅 SubTask 6.3 实施时）：`cd build && ninja -j 16 && ninja install` 0 error

## 阶段 3（P2 较高风险：循环依赖与枚举统一）

- [ ] Task 7: 解除 manager ↔ SigSession 循环依赖
  - [ ] SubTask 7.1: 审计 6 个 manager 对 `_session` 裸指针的实际使用——区分"仅需 EventBus"（可移除 _session 指针）与"需要 SigSession 状态"（需保留）
  - [ ] SubTask 7.2: 对于仅需 EventBus 的 manager（评估 DecodeTaskManager 是否属于此类），移除 `SigSession *_session` 成员，构造函数只接收 `EventBus*`
  - [ ] SubTask 7.3: 对于需要 SigSession 状态的 manager，评估可否改为构造时注入所需引用（如 CaptureManager 需要的 device_agent/view_data 等通过构造参数传入），避免持有 SigSession* 长期引用
  - [ ] SubTask 7.4: 若 SubTask 7.2/7.3 改动风险过高（如 DataFeedParser 需要访问大量 SigSession 状态），保留 `SigSession*` 但加注释说明"循环引用是已知技术债，需 SigSession 拆分进一步完成后才能解除"
  - [ ] SubTask 7.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error

- [ ] Task 8: 事件总线全量迁移（MainWindow 实现 40 个剩余 on_event override）
  - [ ] SubTask 8.1: 审计 `mainwindow.cpp` `OnMessage` 39 个 case 的实际处理逻辑——按职责分组到现有 7 个处理器方法（on_data_updated/on_capture_state_changed/on_signals_changed/on_trigger_changed/on_device_options_updated/on_session_state_changed/on_decode_done）
  - [ ] SubTask 8.2: 为每个事件结构体在 MainWindow 实现 `on_event(const T&)` override——直接调用对应处理器方法，移除 OnMessage case 路由
  - [ ] SubTask 8.3: 对于 OnMessage 翻译表（sigsession.cpp:1505-1554 broadcast<T>() 调用）中已发射的事件，MainWindow on_event override 替代 OnMessage case；对于未发射的事件，on_event override 实现为空（= default）
  - [ ] SubTask 8.4: 缩减 `OnMessage` 方法体——仅保留日志记录或未知消息兜底，不超过 10 行
  - [ ] SubTask 8.5: 验证：`cd build && ninja -j 16 && ninja install` 0 error；grep `mainwindow.cpp` `OnMessage` 方法体 < 10 行；grep `mainwindow.h` override 了 41 个 on_event 虚函数

- [x] Task 9: 消除 api::ChannelType 与 SR_CHANNEL_* 双真相源
  - [x] SubTask 9.1: 审计 `signalmodel.h` `_type` 成员当前存储类型——是 `api::ChannelType` 还是 `int`？审计 `api_type_to_sr_channel_type()` 所有调用点（grep 工程内）
  - [x] SubTask 9.2: `SignalModel::_type` 改为存储 `sr_channel_type`（或 `int` 持有 SR_CHANNEL_* 值）；构造函数、setter 接收 `sr_channel_type`
  - [x] SubTask 9.3: `SignalModel::type()` 返回类型改为 `sr_channel_type`（或 `int`）；保留 `api_type()` accessor 返回 `api::ChannelType` 仅供 UI 层使用（若有 UI 依赖 api::ChannelType 枚举值）
  - [x] SubTask 9.4: 移除 `SignalModel::sr_type()` 方法（已与 type() 等价）
  - [x] SubTask 9.5: 移除 `api_type_to_sr_channel_type()` 转换函数及其所有调用点
  - [x] SubTask 9.6: 更新所有调用 `model->type()` 传给 libsigrok API 的边界点（grep `model->type()` 在 view/*.cpp、core/*.cpp、data/*.cpp）——直接传 `model->type()`，无需转换
  - [x] SubTask 9.7: 验证：`cd build && ninja -j 16 && ninja install` 0 error；grep `api_type_to_sr_channel_type` 0 代码命中（仅注释或已删除）；grep `model->sr_type()` 0 命中

## Task Dependencies

- Task 1（注释修正）无依赖，可并行
- Task 2/3（friend 移除）依赖 Task 1（注释修正后明确实际状态）——可并行
- Task 4（accessor 收敛）依赖 Task 2（friend 移除后访问路径清晰）
- Task 5（FilterProcessor 资源现代化）独立，可与 Task 4 并行
- Task 6（DocumentRegistry 文档指针评估）独立，可与 Task 4/5 并行
- Task 7（循环依赖解除）依赖 Task 4（accessor 收敛后依赖关系清晰）
- Task 8（事件总线全量迁移）依赖 Task 1（注释修正后迁移状态明确）——可与 Task 2-7 并行
- Task 9（枚举统一）独立，可与 Task 4-8 并行

## Parallelizable Work

- 阶段 1：Task 1 + Task 2 + Task 3 部分并行（Task 2/3 依赖 Task 1 注释修正，但 Task 1 仅文档变更可快速完成）
- 阶段 2：Task 4 + Task 5 + Task 6 互不依赖，可全并行
- 阶段 3：Task 7 + Task 8 + Task 9 互不依赖，可全并行（但 Task 7 依赖阶段 2 Task 4 完成）

## 风险控制

- **阶段 1 Task 2/3（friend 移除）**：中风险——需确认所有私有直访点已改为 public accessor，遗漏会导致编译错误。建议每删一个 friend 后立即编译验证。
- **阶段 2 Task 4（accessor 收敛）**：中风险——19 个 accessor 的调用点可能很多（grep 命中可能上百处），需逐个替换并验证。建议先改 setter/getter 签名，让所有调用点编译失败，再逐个修复（编译器驱动重构）。
- **阶段 2 Task 5（FilterProcessor thread 现代化）**：低风险——unique_ptr 与裸指针语义等价，改动局部。
- **阶段 3 Task 7（循环依赖解除）**：高风险——可能需要重新设计 manager 构造签名，影响 SigSession::init() 初始化顺序。建议先评估可行性，若风险过高则仅加注释标记技术债。
- **阶段 3 Task 8（事件总线全量迁移）**：中风险——39 个 case 改为 41 个 on_event override，工作量大但模式固定。建议按职责分组批量迁移，每组迁移后立即编译验证。
- **阶段 3 Task 9（枚举统一）**：高风险——影响所有用 type() 的边界点，可能涉及 view/*.cpp 多处。建议先 grep 审计调用点数量，若超过 50 处则分批迁移。
