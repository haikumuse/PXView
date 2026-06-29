# Tasks

- [x] Task 1: 修复 `copy_data_to_document` 采集归属 Bug
  - [ ] 1.1: 定位 `sigsession.cpp` 中 `DSV_MSG_REV_END_PACKET` 分支里 `copy_data_to_document` 的调用点（约 2220-2230 行），将 `auto doc = _active_document;` 改为 `auto doc = _capture_owner_document ? _capture_owner_document : _active_document;`
  - [ ] 1.2: 检查 `sigsession.cpp:2756-2757` 另一处 `copy_data_to_document(_active_document)` 调用，同样改用 `_capture_owner_document` 兜底逻辑
  - [ ] 1.3: 确认 `_capture_owner_document` 在 `start_capture`（sigsession.cpp:623）已正确赋值，无需改动
  - [ ] 1.4: 构建验证编译通过

- [x] Task 2: SigSession document 相关方法加显式 doc 参数（签名改造）
  - [ ] 2.1: `sigsession.h` 修改 `get_decoder_stacks` 签名：`std::vector<data::DecoderStack*>& get_decoder_stacks(data::SessionDocument* doc = nullptr) override;`，实现改为 `data::SessionDocument* target = doc ? doc : _active_document; return target ? target->get_decoder_stacks() : _empty_decoder_stacks;`
  - [ ] 2.2: `sigsession.h` 修改 `add_decoder` 签名：新增 `data::SessionDocument* doc = nullptr` 末尾参数；`sigsession.cpp:1523` 实现内把 `_active_document` 的引用改为 `doc ? doc : _active_document` 局部变量
  - [ ] 2.3: `sigsession.h` 修改 `remove_decoder(int index, data::SessionDocument* doc = nullptr)` / `remove_decoder_by_key_handel(void* handel, data::SessionDocument* doc = nullptr)` / `rst_decoder(int index, data::SessionDocument* doc = nullptr)` / `rst_decoder_by_key_handel(void* handel, data::SessionDocument* doc = nullptr)` 签名，实现内同样用 `doc ? doc : _active_document` 解析目标
  - [ ] 2.4: `sigsession.h` 修改 `start_capture(bool instant, data::SessionDocument* owner = nullptr)`，实现内 `_capture_owner_document = owner ? owner : _active_document;`
  - [ ] 2.5: `data/datasource.h` 接口的 `get_decoder_stacks` 同步加 `SessionDocument* doc = nullptr` 默认参数（保持接口一致性）
  - [ ] 2.6: 构建验证编译通过；UI 路径调用点（View/MainWindow/TabContext）零改动验证

- [x] Task 3: SessionService 新增 `_api_document` 成员与初始化路径
  - [ ] 3.1: `session_service.h` 新增 `private` 成员 `pv::data::SessionDocument* _api_document = nullptr;`，并在构造函数初始化
  - [ ] 3.2: `session_service.h` 新增 `public` setter `void set_api_document(pv::data::SessionDocument* doc);` 供 AppService 注入
  - [ ] 3.3: `appservice.cpp` 创建 `SessionService` 后，调用 `set_api_document(...)` 注入一个新建的 `SessionDocument`（通过 `SigSession::register_document` 注册）
  - [ ] 3.4: headless 模式下 `AppControl::Start()` 确保 AppService 创建时 `_api_document` 已就绪（GUI 模式同样创建，保持两条路径一致）
  - [ ] 3.5: 构建验证编译通过

- [ ] Task 4: SessionService 所有 document 读写操作传 `_api_document`
  - [ ] 4.1: `session_service.cpp` 中 18 处 `_session->get_decoder_stacks()` 调用改为 `_session->get_decoder_stacks(_api_document)`（涉及行：2022/2080/2717/2748/2785/2835/3301/3685 等，全量 grep 确认）
  - [ ] 4.2: `SessionService::add_decoder`（约 2055/2361 行）调用 `_session->add_decoder(...)` 末尾传 `_api_document`
  - [ ] 4.3: `SessionService::remove_decoder` / `clear_all_decoders` 调用传 `_api_document`
  - [ ] 4.4: `SessionService::start_capture` / `configure_and_start` 调用 `_session->start_capture(instant, _api_document)` 传入 owner
  - [ ] 4.5: `get_active_decoders` / `get_decoder_annotations` 等读取路径确认走 `_api_document`
  - [ ] 4.6: 构建验证编译通过

- [x] Task 5: 删除 `_empty_decoder_stacks` 迁移逻辑
  - [ ] 5.1: `sigsession.cpp:2489-2503` `set_active_document` 中迁移 `_empty_decoder_stacks` 到新 doc 的代码块删除（保留 `_active_document = doc;` 赋值）
  - [ ] 5.2: 全量 grep `_empty_decoder_stacks` 写入路径，确认仅剩 `get_decoder_stacks` 的兜底返回（无写入），或彻底删除该成员并改 `get_decoder_stacks` 兜底返回空静态容器
  - [ ] 5.3: 构建验证编译通过

- [x] Task 6: 验证（构建通过 + headless MCP 协议层验证通过；GUI 多 tab/并存场景需手动测试）
  - [~] 6.1: headless MCP 协议层验证：`initialize` 握手成功 + `tools/list` 正常 + `list_analyzers` 返回完整列表 + `add_analyzer` 代码路径正常执行（返回业务错误而非崩溃）；完整 `add_analyzer` 流程需设备 Logic 模式手动测试
  - [ ] 6.2: GUI 模式多 tab 验证：tab A 采集过程中切换到 tab B，确认采集数据落到 tab A（`_capture_owner_document`），tab B 不受影响 — 需 GUI 环境手动测试
  - [ ] 6.3: GUI 模式 MCP 与 UI 并存验证：UI 在 tab A 时，MCP 调 `add_analyzer`，确认 decoder 落 `_api_document` 而非 tab A 的 document；UI 切 tab 后 MCP `get_analyzer_results` 仍读 `_api_document` — 需 GUI 环境手动测试
  - [x] 6.4: 回归验证：UI 路径行为不变（GUI 构建后启动 + 绘制 + 正常关闭验证通过，走默认参数 → `_active_document`）

# Task Dependencies

- Task 1 独立，可与 Task 2、Task 3 并行
- Task 2 独立（签名改造，UI 调用点走默认参数零改动）
- Task 3 独立（SessionService 成员新增 + 初始化）
- Task 4 依赖 Task 2（需要新签名）+ Task 3（需要 `_api_document` 成员）
- Task 5 依赖 Task 3（headless 已有 `_api_document` 后才能删迁移逻辑）
- Task 6 依赖 Task 1/2/3/4/5 全部完成
