# Checklist

## Bug 修复验证
- [x] `copy_data_to_document` 调用点使用 `_capture_owner_document`（兜底 `_active_document`），不再直接用 `_active_document`
- [x] `sigsession.cpp:2224` 处 `auto doc = _active_document;` 已改为 `_capture_owner_document` 兜底
- [x] `sigsession.cpp:2757` 处 `copy_data_to_document(_active_document)` 已改为 `_capture_owner_document` 兜底

## SigSession 签名改造验证
- [x] `get_decoder_stacks(SessionDocument* doc = nullptr)` 签名已加，实现内 `doc ? doc : _active_document` 解析
- [x] `add_decoder(..., SessionDocument* doc = nullptr)` 签名已加，实现内目标 doc 解析正确
- [x] `remove_decoder(int index, SessionDocument* doc = nullptr)` 签名已加
- [x] `remove_decoder_by_key_handel(void* handel, SessionDocument* doc = nullptr)` 签名已加
- [x] `rst_decoder(int index, SessionDocument* doc = nullptr)` 签名已加
- [x] `rst_decoder_by_key_handel(void* handel, SessionDocument* doc = nullptr)` 签名已加
- [x] `start_capture(bool instant, SessionDocument* owner = nullptr)` 签名已加，`_capture_owner_document = owner ? owner : _active_document`
- [x] `data/datasource.h` 接口 `get_decoder_stacks` 同步加默认参数（含 SessionDocument/SessionSnapshot override 同步）
- [x] UI 路径调用点（View/MainWindow/TabContext）零改动，走默认参数（构建通过 + GUI 启动验证）

## SessionService `_api_document` 验证
- [x] `session_service.h` 新增 `_api_document` 成员及 `set_api_document` setter
- [x] `appservice.cpp` 创建 SessionService 后注入新建的 `SessionDocument` 并 `register_document`
- [x] headless 模式 `AppControl::Start()` 路径 `_api_document` 已就绪（统一 AppService 创建路径）
- [x] GUI 模式同样创建 `_api_document`（两条路径一致）
- [x] 所有权清晰：SessionService 析构 unregister + delete（无泄漏无 double free）

## SessionService 操作定向验证
- [x] 14 处 `_session->get_decoder_stacks()` 均改为 `get_decoder_stacks(_api_document)`（grep 确认无遗漏无参调用）
- [x] `SessionService::add_decoder` 传 `_api_document`
- [x] `SessionService::remove_decoder` 传 `_api_document`（clear_all_decoders 走 SigSession 方法无需传）
- [x] `SessionService::start_capture` / `configure_and_start` 传 owner = `_api_document`
- [x] `get_active_decoders` / `get_decoder_annotations` 读取路径走 `_api_document`

## `_empty_decoder_stacks` 迁移逻辑删除验证
- [x] `set_active_document` 中迁移代码块已删除，仅剩 `_active_document = doc;`
- [x] `_empty_decoder_stacks` 无写入路径（仅保留兜底返回读取 + 注释）
- [x] `add_decoder` 的 else 分支写入路径也删除（避免 static 容器堆积泄漏）
- [x] headless 模式 decoder 直接落 `_api_document`，不经过暂存

## 构建验证
- [x] `build_incremental.cmd` 编译通过（exit code 0），无错误，仅 1 个 unused parameter 警告（既存）

## 运行时验证
- [x] headless MCP 协议层：`initialize` 握手成功（返回 pxview v1.5.0），`tools/list` 正常，`list_analyzers` 返回完整解码器列表，`add_analyzer` 代码路径正常执行（返回业务错误"需 Logic 模式"而非崩溃/超时）
- [x] headless 启动：`--headless` 模式启动成功，MCP port 10110 / WS port 10430 就绪，`_api_document` 注入不破坏启动
- [x] UI 回归：GUI 启动 + 绘制 + 正常关闭（构建日志验证 Header::paintEvent + SigSession::Close 正常）
- [ ] add_analyzer 完整流程（decoder 落 `_api_document` 验证）— 需设备在 Logic 模式手动测试（业务前置条件，非改动正确性问题）
- [ ] GUI 多 tab：采集过程中切 tab，数据落到采集发起者 tab — 需 GUI 环境手动测试
- [ ] GUI + MCP 并存：UI 切 tab 不影响 MCP `get_analyzer_results` 结果 — 需 GUI 环境手动测试
