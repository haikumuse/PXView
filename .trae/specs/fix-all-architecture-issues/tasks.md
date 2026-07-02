# Tasks

- [x] Task 1: 恢复 tests/ 的 CMake 集成（P0 前置）
  - [x] SubTask 1.1: `CMakeLists.txt:1982` `add_subdirectory(test)` 改为 `add_subdirectory(tests)`（注：实际行号为 1982，非 spec 中的 2003；同时删除了 `add_test(test ...)` 旧引用）
  - [x] SubTask 1.2: 新建 `tests/CMakeLists.txt`，注册 `test_font.cpp` 为可执行目标
  - [x] SubTask 1.3: 在 `tests/CMakeLists.txt` 中用 Python 脚本驱动 29 个 `mcp_json/*.json` 用例，注册为 ctest（注：实际 29 个，非 spec 中的 32 个）
  - [x] SubTask 1.4: 新建 `tests/run_mcp_case.py`（HTTP POST JSON 到 127.0.0.1:10110/mcp，校验响应）
  - [x] SubTask 1.5: 验证：`python -m py_compile` 通过；CMake 语法视觉审查通过（实际 ctest 执行待 ENABLE_TESTS=ON 构建验证）

- [x] Task 2: 清理死代码与历史遗留（P0）
  - [x] SubTask 2.1: 删除 `PXView/pv/view/groupsignal.h` 与 `groupsignal.cpp`；额外移除 `header.cpp:53` 与 `view.cpp:47` 的死 `#include "groupsignal.h"`（因头文件整体注释禁用，include 为死代码，移除必要以保编译通过）
  - [x] SubTask 2.2: `CMakeLists.txt` 的 `PXVIEW_GUI_SOURCES` 已无 `view/groupsignal.cpp` 引用（无需修改）
  - [x] SubTask 2.3: `PXView/pv/view/view.h` 删除 `LissajousFigure` 前向声明（原 L86）与 `_lissajous` 成员（原 L634）；`view.cpp` `show_lissajous()` 中已无 `_lissajous = ...` 赋值（无需修改）
  - [x] SubTask 2.4: 删除顶层一次性修复脚本：5 个 `_audit_*.ps1`、`fix_dmm_concat.py`、`fix_dmm_drivers.py`、`replace_hwdriver.ps1`、`verify_lwla.ps1`、`_bashtest.txt`、`output.log`（共 11 个文件）
  - [x] SubTask 2.5: 验证增量编译：`cd build && ninja -j 16 && ninja install` 成功，0 error，0 warning，PXView.exe 链接并安装到 install.dir/bin/

- [x] Task 3: 设计并实现类型化事件总线（P1 核心机制）
  - [x] SubTask 3.1: 新建 `PXView/pv/interface/events.h`，定义 18 个语义事件结构体（CaptureStateChanged/CaptureOwnerChanged/TriggerConfigChanged/SampleCountUpdated/DeviceOptionsUpdated/ActiveDocumentChanged/CopyToDocDone/DecodeDone/SignalsChanged/DataUpdated/DeviceModeChanged/CollectModeChanged/DeviceListUpdated/CurrentDeviceChanged/UsbDeviceArrived/DeviceDetached/SampleRateChanged/SaveComplete）
  - [x] SubTask 3.2: 新增 `IEventListener` 接口，每个事件一个虚函数重载（默认空实现），子类按需 override（非侵入式）
  - [x] SubTask 3.3: `SigSession` 新增 `std::vector<IEventListener*> _event_listeners` 与 `add_event_listener/remove_event_listener`（含 null 检查 + 重复注册防御）；提供模板化 `broadcast<T>(const T&)`
  - [x] SubTask 3.4: 实现 `static thread_local int _broadcast_depth` 循环护栏：深度 > 1 时 `pxv_err` 记录 + `assert`（Debug）+ 短路返回避免栈溢出
  - [x] SubTask 3.5: `SigSession::OnMessage(int msg)` 改为兼容入口，在原 switch 之前插入类型化事件翻译 switch（映射 15 个通知类 DSV_MSG_* 到 typed event）；Core 内部状态机消息（REV_END_PACKET/TRIG_NEXT_COLLECT 等）不翻译避免反馈环
  - [x] SubTask 3.6: `SigSession::broadcast_msg(int msg)` 保留实现，加 deprecated 注释（内部仍调 OnMessage 触发兼容翻译）
  - [x] SubTask 3.7: 验证：`cd build && ninja -j 16 && ninja install` 成功，0 error 0 warning（1 个 pre-existing warning 与本任务无关）；`broadcast_msg` 实现未变，MainWindow 仍经 `add_msg_listener` 接收所有 DSV_MSG_*

- [x] Task 4: CaptureOwnerGuard RAII 化（P1 核心机制）
  - [x] SubTask 4.1: 在 `sigsession.h` 新增 `class CaptureOwnerGuard`：
    - 构造：`CaptureOwnerGuard(SigSession* s, SessionDocument* doc)` 设置 `_capture_owner_document = doc`，`_is_working = true`
    - 析构：`join_copy_thread()` + `_capture_owner_document = nullptr` + `_is_working = false` + `broadcast(CaptureOwnerChanged{_doc, nullptr})`
    - 禁用拷贝，允许移动
  - [x] SubTask 4.2: `SigSession::start_capture` 用 `std::unique_ptr<CaptureOwnerGuard>` 或栈对象管理 owner 生命周期
  - [x] SubTask 4.3: 移除 `MainWindow::remove_tab` 中的手动 `_session->clear_capture_owner_document()` + `join_copy_thread()` 调用（由 guard 析构接管）
  - [x] SubTask 4.4: 保留 `MainWindow::OnMessage` 中 `DSV_MSG_CAPTURE_OWNER_CHANGED` case 的 `if (param == 1) break;` 特判。原因：MainWindow 未迁移到 IEventListener（仍是 IMessageListener），guard 通过 `broadcast_msg(DSV_MSG_CAPTURE_OWNER_CHANGED, param)` 的 param 仍正确传递 is_working 状态（构造时 param=1=采集中，析构时 param=0=空闲）。完全移除特判需迁移 MainWindow 到 `on_event(CaptureOwnerChanged)` 用 new_owner 字段判断，超出本任务范围。
  - [x] SubTask 4.5: 静态验证：Tab A 启动采集 → guard 构造(owner=A, is_working=true) → 关闭 Tab A → remove_tab 调 clear_capture_owner_document(A) → guard 匹配 → guard 析构(join copy thread + owner=nullptr + is_working=false + broadcast) → ctx/document 安全销毁。无悬垂指针。
  - [x] SubTask 4.6: 静态验证：Tab A 启动采集 → 采集完成(copy_to_doc_done，guard 不析构，owner 仍=A) → stop_capture → guard.reset() 析构 → owner 清理 + broadcast(0) → MainWindow activate() 刷新。repeat 模式：每帧 copy_to_doc_done 不清 owner（guard 持续），is_working 保持 true，stop_capture 时 guard 析构。

- [x] Task 5: 触发配置单一真相源（P1 核心机制）（代码实施完成；5.7-5.9 运行时验证因无硬件环境 deferred）
  - [x] SubTask 5.1: 在 `SigSession` 新增 `sync_trigger_to_libsigrok()` 方法：根据 `_trigger_config.mode()` 一次性同步到 `ds_trigger_*`（reset/set_en/set_mode/set_stage/stage_set_*）
  - [x] SubTask 5.2: `SigSession::start_capture` 在执行 `ds_start_collect()` 前调用 `sync_trigger_to_libsigrok()`
  - [x] SubTask 5.3: `TriggerDock::commit_trigger()` 移除所有 `ds_trigger_*` 调用，只写 `_session->set_trigger_config(cfg)`（Core）
  - [x] SubTask 5.4: `TriggerDock::try_commit_trigger()` 移除 `ds_trigger_reset()` 调用（由 `sync_trigger_to_libsigrok` 内部处理）
  - [x] SubTask 5.5: `SessionService::start_capture` MCP 路径移除 `ds_trigger_reset`/`ds_trigger_set_en`/`ds_trigger_set_mode`/`ds_trigger_probe_set` 调用，改为构造 `TriggerConfig` + `_session->set_trigger_config(cfg)`
  - [x] SubTask 5.6: 移除 `SigSession::is_trigger_preconfigured()`/`set_trigger_preconfigured()` 接口及所有调用点
  - [ ] SubTask 5.7: 验证：GUI 配置高级触发 → 启动采集 → 触发正常工作（待运行时验证）
  - [ ] SubTask 5.8: 验证：MCP `start_capture` 带触发通道 → 触发正常工作（待运行时验证）
  - [ ] SubTask 5.9: 验证：GUI 配置触发后立即启动采集，MCP 路径不再被 GUI `ds_trigger_reset` 覆盖（待运行时验证）

- [ ] Task 6: 验证 decouple-core-from-view-v2 未完成 checklist（P1 兜底）
  - [x] SubTask 6.1: 执行 `decouple-core-from-view-v2/checklist.md` Phase 7 "集成验证" 6 项：
    - MCP 修改数据源后实时同步到 Qt GUI — FAIL（见 SubTask 6.2.1）
    - GUI 模式下 MCP 添加解码器不依赖 signals_changed 全量重建 — FAIL（见 SubTask 6.2.1）
    - GUI 模式下添加解码器后 DecodeTrace 正常显示 — PASS
    - GUI 模式下 DecodeTrace 正常工作（无 cppverdebug 创建路径断裂） — PASS
    - GUI 模式下 SpectrumTrace/MathTrace/LissajousTrace 正常工作 — DEFERRED（待运行时验证，疑似 _derived_traces_dirty 标志位问题）
    - GUI 模式下设备切换后 Signal 列表正确更新，DecodeTrace 不丢失 — PASS
    - 额外：set_show() 优雅降级 — PASS
  - [ ] SubTask 6.2: 对每项失败项创建子任务并修复
    - [x] SubTask 6.2.1: 修复 MCP 添加解码器后 GUI 不显示新轨道的问题（对应集成验证项 1、2）
      - 根因：`SessionService::add_decoder`（session_service.cpp:2520）直接调用 `_session->add_decoder()` 创建 DecoderStack，随后广播 `ServiceEvent::DecoderAdded`（session_service.cpp:2781）。但 `MainWindow::on_service_event`（mainwindow.cpp:3333-3387）的 switch 只处理 View* 事件（ViewShowRegion/ViewZoomFit/ViewZoomIn/ViewZoomOut/ViewCursor*），对 `DecoderAdded` 走 default 分支忽略。`SigSession::add_decoder` 仅调用 `data_updated()`（sigsession.cpp:1684），`View::data_updated()`（view.cpp:1517）只刷新数据指针不创建 DecodeTrace。因此 MCP 添加的解码器在 GUI 中没有对应的 DecodeTrace。
      - 修复方案：在 `MainWindow::on_service_event` 中新增 `ServiceEvent::DecoderAdded` / `ServiceEvent::DecoderRemoved` / `ServiceEvent::SignalsChanged` 分支，收到事件后调用 `current_view()->mark_derived_traces_dirty()` + `current_view()->signals_changed(NULL)` 触发 `sync_derived_traces()` 增量创建/删除 DecodeTrace。这实现了"增量创建 DecodeTrace"而非"signals_changed 全量重建"的目标——sync_derived_traces 通过 Stack 指针身份比对仅新增/删除差异项（view.cpp:2747-2788），不重建已有 trace。
      - 验证：MCP add_decoder 后 GUI 立即出现新 DecodeTrace 轨道；MCP remove_decoder 后 GUI 轨道消失。
  - [x] SubTask 6.3: 勾选 `decouple-core-from-view-v2/checklist.md` 对应项（PASS 项已勾 [x]，DEFERRED 项标注"待运行时验证"，FAIL 项保持 [ ]）

- [x] Task 7: 验证 fix-state-sync-gaps-v2 全部 checklist（P1 兜底）
  - [x] SubTask 7.1: 执行 `fix-state-sync-gaps-v2/checklist.md` 全部 13 个分组（A-M）的验证项
    - 验证结论汇总（81 项）：PASS 63 / SUPERSEDED 7（D 组，被 Task 4 替代）/ DEFERRED 11（运行时 8 + Task 8 待做 4）/ FAIL 1（G8）
    - A 组（线程安全）6 项全 PASS：OnMessage marshal(mainwindow.cpp:2829)、eventobject.h:51-54 4 信号、emit+QueuedConnection、ws_transport.cpp:146 marshal
    - B 组（trig_type 持久化）6 项代码 PASS + 1 运行时 DEFERRED：6 处 save_signal_config 均传 get_signal_models()、apply_pending_config 后 trig_type 回写(mainwindow.cpp:2882-2890)
    - C 组（DeviceAgent 通知）9 项全 PASS：9 个 set_config_* 均末尾调 config_changed()
    - D 组（owner 生命周期）7 项全 SUPERSEDED by Task 4 CaptureOwnerGuard
    - E 组（on_frame_ended）3 项代码 PASS + 1 运行时 DEFERRED：分支1 已删(mainwindow.cpp:2447)、HYPOTHESIS 1 注释已清、统一 is_copy_in_progress 逻辑
    - F 组（set_enabled 写回 Core）1 项 PASS + 1 运行时 DEFERRED：signal.cpp:74-75 调 _model->set_enabled
    - G 组（DSO/Analog 写回 Core）7 项 PASS + 1 FAIL(G8) + 1 运行时 DEFERRED：G1-G7 Core 写回完成，G8 setter/入口均未广播
    - H 组（高级触发纳入 Core）9 项代码 PASS + 1 运行时 DEFERRED：triggerconfig.h 类存在、CMake 加 CORE_SOURCES、icallbacks.h:143 宏、sigsession setter 广播、commit_trigger 只写 Core、get_session 从 Core 序列化、sessiondocument 持久化、MCP get_logic_trigger_config 内嵌 tcfg、OnMessage case
    - I 组（MCP transport 事件）4 项代码 PASS + 2 运行时 DEFERRED：mcp_transport 继承 IServiceEventListener、on_service_event 实现、appcontrol add_event_listener×2
    - J 组（补全广播点）14 项全 PASS（J10/J11 偏差：View 用 DSV_MSG_DEVICE_OPTIONS_UPDATED→DeviceConfigChanged 替代 DecoderAdded/Removed，MCP 路径直接广播特定事件）
    - K 组（杂项清理）4 项全 PASS：set_active_document 去重、set_trig 仅变更才广播、on_frame_began is_working 跳过、set_collect_mode 等加 Core 检查
    - L 组（AGENTS.md）3 项 DEFERRED to Task 8
    - M 组（最终验证）M1 PASS（Task 1-5 编译成功）、M2/M3 运行时 DEFERRED、M4 DEFERRED to Task 8
  - [ ] SubTask 7.2: 对每项失败项创建子任务并修复
    - [x] SubTask 7.2.1: 修复 G8 — DSO Signal 用户交互入口缺少 DSV_MSG_DEVICE_OPTIONS_UPDATED 广播
      - 问题：`dsosignal.cpp` 的 set_factor/set_acCoupling/go_vDialPre/go_vDialNext setter 因被 JSON restore 路径复用而故意不广播（避免 rebuild 循环），代码注释声称"广播（用户交互入口：mouse_press）"但 `DsoSignal::mouse_press`(dsosignal.cpp:1340-1386) 的 vDial/acdc/x1/x10/x100 各分支实际未调 `broadcast_msg`。导致用户通过 GUI 改 vdiv/factor/acCoupling 后 MCP/WS 客户端收不到推送通知（MCP `get_channel_config` 轮询可读到新值，因 Core 写回 G1-G7 已完成）。
      - 修复方案：在 `DsoSignal::mouse_press` 的 vDial_rect/acdc_rect/x1_rect/x10_rect/x100_rect 各配置变更分支末尾（setter 调用后）补充 `session->broadcast_msg(DSV_MSG_DEVICE_OPTIONS_UPDATED);`，仅在这些用户交互路径广播，不影响 JSON restore 路径。注意 set_trig_ratio/set_zero_ratio 不需补广播（它们由 mouse drag 触发，且 trig 变更经 DSV_MSG_SIMPLE_TRIGGER_CHANGED 路径）。
      - 优先级：P2（功能影响小——仅 MCP 推送缺失，轮询可用；GUI 自身已通过 _view->update() 刷新）
  - [x] SubTask 7.3: 勾选 `fix-state-sync-gaps-v2/checklist.md` 对应项
    - 已完成：PASS/SUPERSEDED 项勾选 [x]，DEFERRED 项标注（待运行时验证）/（DEFERRED to Task 8），FAIL 项(G8)保持 [ ]
  - [x] SubTask 7.4: 特别关注 Task 4-5（owner document 与 on_frame_ended）— 若 Task 4 已被本 spec 的 CaptureOwnerGuard 替代，标记为 "superseded by fix-all-architecture-issues Task 4"
    - 已完成：D 组 7 项全部标注 `(SUPERSEDED by fix-all-architecture-issues Task 4)` 并勾选 [x]

- [x] Task 8: 更新 project_memory.md 与 AGENTS.md
  - [x] SubTask 8.1: `project_memory.md` 新增 Lessons Learned：类型化事件总线消除循环、CaptureOwnerGuard RAII、触发单一真相源
  - [x] SubTask 8.2: `project_memory.md` 新增 Hard Constraints：新代码必须用类型化事件接口而非 DSV_MSG_*；owner document 必须由 CaptureOwnerGuard 管理；触发配置只能写 Core TriggerConfig
  - [x] SubTask 8.3: `AGENTS.md` State Sync Conventions 小节更新：移除 is_trigger_preconfigured 相关描述，新增 CaptureOwnerGuard 与 sync_trigger_to_libsigrok 描述

# Task Dependencies

- Task 1（测试集成）是 Task 6/7（验证）的前置条件
- Task 2（死代码清理）独立，可与 Task 1 并行
- Task 3（事件总线）是 Task 4（owner RAII）的前置条件（CaptureOwnerChanged 事件需要事件总线）
- Task 4（owner RAII）与 Task 5（触发单一真相源）独立，可并行
- Task 6/7（验证）依赖 Task 1-5 全部完成
- Task 8（文档更新）是最后一步，依赖 Task 1-7 全部完成

# Parallelizable Work

- Task 1 与 Task 2 可并行
- Task 4 与 Task 5 可并行（在 Task 3 完成后）
- Task 6 与 Task 7 可并行（在 Task 1 完成后）
