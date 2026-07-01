# Tasks

> 优先级：P0（高崩溃风险）→ P2（防御性加固）。前置 spec `fix-state-sync-gaps-v2`、`fix-mmap-async-crash-risks`、`decouple-core-from-view-v2` 均已完成。

## P0：Release 空指针防御

- [x] Task 1: 审计并修复 `assert(ptr)` 守卫的空指针风险
  - [x] SubTask 1.1: Grep 全项目 `assert\([a-zA-Z_][a-zA-Z_0-9]*\)` 模式（单标识符 assert，疑似指针守卫），逐文件审查 40 个文件 201 处命中
  - [x] SubTask 1.2: 重点审查崩溃关键路径：`sigsession.cpp`（7 处加守卫）、`decoderstack.cpp`（11 处）、`mainwindow.cpp`（4 处）、`storesession.cpp`（7 处）、`logicsnapshot.cpp`（3 处）、`deviceagent.cpp`（31 处）
  - [x] SubTask 1.3: 凡是 `assert(ptr)` 后立即 `ptr->` 解引用的位置，补显式 `if(!ptr) { log + return/throw; }`；保留 `assert` 作为开发期断言
  - [ ] SubTask 1.4: 编译验证（`cd build && ninja -j 16 && ninja install`）无新增 warning — **受阻**：链接阶段报 libsigrok 驱动多重定义错误（`tiered-driver-compat-fix` spec 遗留），非本 spec 引入

## P1：DecoderStack 句柄稳定性

- [x] Task 2: `get_decoder_annotations` 改用句柄 ID + 版本号匹配
  - [x] SubTask 2.1: `DecoderStack` 新增 `uint64_t _handle_id`（构造时分配，单调递增）与 `uint64_t _version`（重建时递增）成员；提供 `handle_id()`/`version()` getter
  - [x] SubTask 2.2: `SigSession` 提供全局句柄分配器（`std::atomic<uint64_t>`），`add_decoder` 创建栈时分配 ID
  - [x] SubTask 2.3: `SessionService::add_decoder`（MCP）返回的 instance_id 改为 `"<handle_id>:<version>"` 字符串
  - [x] SubTask 2.4: `get_decoder_annotations` 匹配逻辑改为解析 `instance_id` 得到 (handle_id, version)，遍历栈时同时比对两者；版本不匹配返回 `DecoderNotFound`
  - [x] SubTask 2.5: 更新 MCP debug log（`pxview_mcp_debug.log`）输出 handle_id/version 而非裸指针
  - [x] SubTask 2.6: 更新 MCP 工具 schema 文档（rpc_dispatcher.cpp）说明 instance_id 格式
  - [ ] SubTask 2.7: 验证 MCP 流程：add_analyzer → start_capture → wait_capture → get_analyzer_results 正常匹配；restart_decoders 后旧 instance_id 返回 DecoderNotFound — **受阻**：同上链接错误

## P1：移除误导性 effective_data_source API

- [x] Task 3: 移除/重命名 `effective_data_source()`
  - [x] SubTask 3.1: Grep 所有 `effective_data_source` 调用点（view.cpp、view.h、ruler.cpp）
  - [x] SubTask 3.2: 区分两类用法：(a) 作为 SignalModel 源 → 改为 `_data_source`；(b) 作为快照源 → 重命名为 `document_snapshot_source()` 并在注释中明确「只返回快照，不返回 SignalModel」
  - [x] SubTask 3.3: 从 `view.h` 删除 `effective_data_source()` 声明，从 `view.cpp` 删除实现
  - [x] SubTask 3.4: 编译验证无引用残留 — Grep 确认全项目仅 1 处注释残留

## P1：SignalModel::type() 根因修复

- [x] Task 4: 在 DataSource/SignalModel 边界强制 SR_CHANNEL_* 转换
  - [x] SubTask 4.1: `SignalModel` 新增 `int sr_type() const` 方法，内部调 `api_type_to_sr_channel_type(_type)` 返回 `SR_CHANNEL_*`
  - [x] SubTask 4.2: Grep 所有 `api_type_to_sr_channel_type(model->type())` 或 `api_type_to_sr_channel_type(_model->type())` 调用点，改为 `model->sr_type()` — signal.cpp + storesession.cpp 完成
  - [x] SubTask 4.3: Grep 所有传 `model->type()` 给 libsigrok API（`get_snapshot`/`ds_set_*`）的调用点，改用 `sr_type()`
  - [x] SubTask 4.4: 保留 `type()` 返回 `api::ChannelType` 供 View 层 UI 使用，但在头文件注释标注「勿直接传给 libsigrok，用 sr_type()」
  - [x] SubTask 4.5: 编译验证 — Grep 确认 0 处残留 `api_type_to_sr_channel_type(.*->type())` 调用

## P2：add_decode_task 私有化

- [x] Task 5: `add_decode_task` 设为 private，强制漏斗
  - [x] SubTask 5.1: `sigsession.h` 把 `add_decode_task` 从 public 移到 private 区段
  - [x] SubTask 5.2: Grep 外部调用点（`view.cpp`、`session_service.cpp`、`mainwindow.cpp` 等），改为 `start_all_decode_tasks()`（批量）或 `rst_decoder()`（单栈重置）
  - [x] SubTask 5.3: 验证 `start_all_decode_tasks()` 与 `rst_decoder()` 内部均先调 `attach_data_to_signal(_view_data)` 再调 `add_decode_task`
  - [ ] SubTask 5.4: 编译验证 — **受阻**：同上链接错误

## P2：rebuild 重入护栏

- [x] Task 6: `rebuild_signals_from_config` 引入 RebuildGuard
  - [x] SubTask 6.1: `view.h` 新增 `bool _rebuild_in_progress = false;` 成员（**修正**：原计划放 sigsession.h，实际函数在 view.cpp，故放 view.h）
  - [x] SubTask 6.2: `view.cpp` `rebuild_signals_from_config` 顶部检查 `if (_rebuild_in_progress) return;`，否则置 true；函数体用 RAII guard（struct RebuildGuard { bool& flag; ~RebuildGuard(){flag=false;} }）确保异常路径也复位
  - [x] SubTask 6.3: 确认 `rebuild_signals_from_config` 内部不广播 `DEVICE_OPTIONS_UPDATED`（约定不变），护栏作为最后一道防线
  - [ ] SubTask 6.4: 编译验证 — **受阻**：同上链接错误

## P2：CAPTURE_OWNER_CHANGED 携带 is_working

- [x] Task 7: 广播消息携带工作状态标志
  - [x] SubTask 7.1: 评估两种方案：(A) 在 `broadcast_msg` 增加 `int param` 负载字段，`CAPTURE_OWNER_CHANGED` 携带 `is_working?1:0`；(B) 拆分为 `DSV_MSG_CAPTURE_OWNER_CHANGED_IDLE`（6033）与 `DSV_MSG_CAPTURE_OWNER_CHANGED_WORKING`（6034）两个消息。**采用方案 A**
  - [x] SubTask 7.2: `icallbacks.h` `IMessageListener::OnMessage` 签名增加 `int param`（默认 0，向后兼容现有调用）
  - [x] SubTask 7.3: `sigsession.cpp` `broadcast_msg` 增加 `param` 参数（默认 0），`start_capture` 广播 `CAPTURE_OWNER_CHANGED` 时传 `is_working()?1:0`
  - [x] SubTask 7.4: `mainwindow.cpp` `OnMessage` 的 `CAPTURE_OWNER_CHANGED` case 改用 `param` 判断，移除 `_session->is_working()` 调用
  - [x] SubTask 7.5: `session_service.cpp` `OnMessage` 的 `CAPTURE_OWNER_CHANGED` case 同步改用 `param`
  - [ ] SubTask 7.6: 编译验证所有 `broadcast_msg` 调用点（默认 param=0 不破坏其他消息） — **受阻**：同上链接错误

## P2：同步等待入口 on_main_thread 审计

- [x] Task 8: 统一同步等待入口的主线程检查
  - [x] SubTask 8.1: Grep `QMetaObject::invokeMethod.*QueuedConnection` + `wait`/`condition_variable` 组合模式，找出所有「投递到主线程 + 阻塞等待」的同步入口
  - [x] SubTask 8.2: 重点审查 `session_service.cpp`：`wait_capture_complete`、`add_analyzer`（已修）、`rst_analyzer`、`remove_analyzer`、`configure_and_start`、`export_*` 等同步 MCP 接口
  - [x] SubTask 8.3: 每个同步入口顶部加 `if (on_main_thread()) { 直接调用目标 + return; }` 前置检查，避免投递到自身队列后阻塞
  - [x] SubTask 8.4: 抽取公共工具函数 `invoke_or_call(QObject* ctx, F&& fn)`：在主线程则直接 fn()，否则 `QMetaObject::invokeMethod(ctx, fn, Qt::BlockingQueuedConnection)`，消除重复样板
  - [ ] SubTask 8.5: 编译验证 + headless 模式 MCP 压力测试（并发请求不冻结） — **受阻**：同上链接错误

# Task Dependencies

- Task 1（assert 审计）独立，可并行
- Task 2（句柄 ID）独立，可并行
- Task 3（effective_data_source 移除）独立，可并行
- Task 4（sr_type）独立，可并行
- Task 5（add_decode_task private）独立，可并行
- Task 6（RebuildGuard）独立，可并行
- Task 7（广播负载）修改 `broadcast_msg` 签名，Task 8 不涉及广播，可并行；但 Task 7 改动 `OnMessage` 签名需全项目同步编译
- Task 8（on_main_thread）独立，可并行

# 并行执行建议

- **第一批（全部独立，可同时启动 8 个 sub-agent）**：Task 1、Task 2、Task 3、Task 4、Task 5、Task 6、Task 8
- **第二批（签名变更，单独批次避免冲突）**：Task 7（改 `broadcast_msg` 签名涉及全项目，建议串行）
- **第三批（编译验证）**：所有 Task 完成后统一 `ninja -j 16 && ninja install`

# 风险与回退

- Task 7 改 `broadcast_msg` 签名是大面积改动，若编译失败可回退为方案 B（新增消息码），不影响其他 Task
- Task 2 句柄 ID 改动 MCP 接口契约（instance_id 格式），需同步更新外部 MCP 客户端文档；旧客户端传裸指针字符串将匹配失败——这是预期行为（裸指针本就不稳定），但需在 changelog 标注 **BREAKING**
- Task 3 移除 `effective_data_source` 是 **BREAKING**（View 内部 API，无外部影响）

# 不在本 spec 范围

- 磁盘缓存错误冒泡到 UI（`fix-mmap-async-crash-risks` 已部分覆盖，剩余 UI 提示单独 spec）
- 单例初始化顺序问题（架构级重构，单独 spec）
- 215 个 C 解码 DLL ABI 版本校验（独立 spec）
- libsigrok 驱动 compat 修复（`tiered-driver-compat-fix` 进行中，独立）—— **当前编译受阻的根因**
- crash_handler 跨平台支持（Linux/macOS，独立 spec）

# 实施总结

| Task | 代码实施 | 编译验证 | 说明 |
|------|---------|---------|------|
| Task 1 | ✅ | ⚠️ 受阻 | 40 文件审查完成，~120 处 `if(!ptr)` 守卫添加，编译阶段通过，链接阶段受阻 |
| Task 2 | ✅ | ⚠️ 受阻 | DecoderStack 句柄 ID + 版本号完成，MCP instance_id 格式更新 |
| Task 3 | ✅ | ⚠️ 受阻 | effective_data_source 移除，document_snapshot_source 重命名 |
| Task 4 | ✅ | ⚠️ 受阻 | sr_type() 完成，3 处调用点迁移，Grep 验证 0 残留 |
| Task 5 | ✅ | ⚠️ 受阻 | add_decode_task 私有化，外部调用迁移到 start_all_decode_tasks |
| Task 6 | ✅ | ⚠️ 受阻 | RebuildGuard 添加（修正位置：view.h/view.cpp 而非 sigsession） |
| Task 7 | ✅ | ⚠️ 受阻 | broadcast_msg param 参数，OnMessage 签名同步 |
| Task 8 | ✅ | ⚠️ 受阻 | invoke_or_call 工具函数，同步入口前置检查 |

**编译受阻根因**：libsigrok 驱动 .c 文件多重定义错误（`sr_sw_limits_*`/`std_session_send_df_frame_end`/`abort_acquisition`/`sr_session_send_meta`），属 `tiered-driver-compat-fix` spec Task 15 未完成项，非本 spec 引入。本 spec 代码改动已通过编译阶段（无 undefined reference to `broadcast_msg(int)` 报错）。
