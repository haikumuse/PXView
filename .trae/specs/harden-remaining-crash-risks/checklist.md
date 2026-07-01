# Checklist

## P0：Release 空指针防御

- [x] 审计完成 40 个文件 201 处 `assert(identifier)` 命中，分类为「指针守卫」与「逻辑断言」
- [x] 崩溃关键路径（sigsession.cpp/decoderstack.cpp/mainwindow.cpp/storesession.cpp/logicsnapshot.cpp/deviceagent.cpp）所有 `assert(ptr)` 后跟解引用的位置补显式 `if(!ptr)` 检查
- [x] Release 构建中传入 NULL 指针不再崩溃，而是早期 return + 日志（代码层面完成）
- [ ] `cd build && ninja -j 16 && ninja install` 编译通过，无新增 warning — **受阻**：链接阶段报 libsigrok 驱动多重定义错误（`tiered-driver-compat-fix` spec 遗留）

## P1：DecoderStack 句柄稳定性

- [x] `DecoderStack` 持有 `handle_id` + `version` 成员，构造时分配单调递增 ID
- [x] `SessionService::add_decoder`（MCP）返回 `"<handle_id>:<version>"` 格式 instance_id
- [x] `get_decoder_annotations` 解析 instance_id 比对 (handle_id, version)，不再用裸指针字符串
- [x] `restart_decoders` 后旧 instance_id 请求返回 `DecoderNotFound`，不访问已释放内存（代码层面完成）
- [x] MCP debug log 输出 handle_id/version
- [x] rpc_dispatcher.cpp MCP 工具 schema 文档更新 instance_id 格式说明
- [ ] MCP 流程验证：add_analyzer → start_capture → wait_capture → get_analyzer_results 正常返回 — **受阻**：同上链接错误

## P1：移除 effective_data_source

- [x] `view.h` 不再声明 `effective_data_source()`
- [x] `view.cpp` 不再实现 `effective_data_source()`
- [x] 所有原 `effective_data_source()` 调用点改为 `_data_source`（SignalModel 源）或 `document_snapshot_source()`（快照源，已重命名）
- [x] Grep 确认全项目无 `effective_data_source` 残留（仅 1 处注释提及历史命名）
- [x] 编译通过（编译阶段）

## P1：SignalModel::type() 根因修复

- [x] `SignalModel` 新增 `int sr_type() const` 方法返回 `SR_CHANNEL_*`
- [x] 所有传通道类型给 libsigrok API 的调用点改用 `sr_type()`，无手动 `api_type_to_sr_channel_type()` 转换
- [x] `type()` 头文件注释标注「勿直接传给 libsigrok，用 sr_type()」
- [x] 编译通过（编译阶段，Grep 验证 0 残留）

## P2：add_decode_task 私有化

- [x] `sigsession.h` 中 `add_decode_task` 位于 private 区段
- [x] 外部调用点均走 `start_all_decode_tasks()` 或 `rst_decoder()`
- [x] `start_all_decode_tasks()` 与 `rst_decoder()` 内部先调 `attach_data_to_signal(_view_data)` 再调 `add_decode_task`
- [ ] 编译通过（外部直接调 `add_decode_task` 会编译失败，符合预期） — **受阻**：同上链接错误

## P2：rebuild 重入护栏

- [x] `View` 持有 `_rebuild_in_progress` 成员（修正：位于 view.h 而非 sigsession.h，因函数在 view.cpp）
- [x] `rebuild_signals_from_config` 顶部检查重入并 return
- [x] RAII guard 保证异常路径也复位标志
- [ ] 模拟广播循环场景：rebuild 内部误广播 → 重入被拦截 → 无栈溢出 — **受阻**：需运行时验证
- [ ] 编译通过 — **受阻**：同上链接错误

## P2：CAPTURE_OWNER_CHANGED 携带 is_working

- [x] `broadcast_msg` 签名增加 `int param` 参数（默认 0）
- [x] `start_capture` 广播 `CAPTURE_OWNER_CHANGED` 时传 `is_working()?1:0`
- [x] `mainwindow.cpp` `OnMessage` 的 `CAPTURE_OWNER_CHANGED` case 用 `param` 判断，不调 `is_working()`
- [x] `session_service.cpp` `OnMessage` 同步改用 `param`
- [x] 其他 `broadcast_msg` 调用点默认 param=0 不破坏现有行为
- [ ] 编译通过 — **受阻**：同上链接错误

## P2：同步等待入口 on_main_thread 审计

- [x] Grep 找出所有「QueuedConnection + 阻塞等待」同步入口
- [x] `session_service.cpp` 所有同步 MCP 接口加 `on_main_thread()` 前置检查
- [x] 抽取 `invoke_or_call` 公共工具函数
- [ ] headless 模式 MCP 并发请求不冻结 — **受阻**：需运行时验证
- [ ] 编译通过 — **受阻**：同上链接错误

## 总体验证

- [ ] `cd build && ninja -j 16 && ninja install` 全量编译通过 — **受阻**：libsigrok 驱动多重定义错误
- [ ] PXView.exe GUI 模式启动正常 — **受阻**
- [ ] PXView.exe --headless 模式启动正常 — **受阻**
- [ ] 采集 → 解码 → 切 Tab → 关 Tab 流程无崩溃 — **受阻**
- [ ] MCP 并发请求不冻结、不崩溃 — **受阻**
- [ ] restart_decoders 后旧 instance_id 不命中（不崩溃） — **受阻**
- [x] AGENTS.md / project_memory.md 更新新增约束（sr_type()、句柄 ID、RebuildGuard、broadcast param）

## 验证状态说明

本 spec 所有 8 个 Task 的代码实施已完成。编译验证受阻于 `tiered-driver-compat-fix` spec Task 15 的 libsigrok 驱动多重定义链接错误（涉及 `sr_sw_limits_*`/`std_session_send_df_frame_end`/`abort_acquisition`/`sr_session_send_meta` 等函数在多个驱动 .c 文件中重复定义）。该问题非本 spec 引入，需在 `tiered-driver-compat-fix` spec 中解决后方可完成本 spec 的编译验证。
