# 解码器标签隔离 — 任务列表

## Phase 1: DecoderStack 快照来源改造

- [x] Task 1.1: DecoderStack 新增 _owner_document 成员和访问方法
  - 在 `pv/data/decoderstack.h` 中添加 `data::SessionDocument *_owner_document;` 私有成员
  - 添加 `void set_owner_document(data::SessionDocument *doc);` 和 `data::SessionDocument* get_owner_document();` 方法
  - 构造函数中初始化为 nullptr
  - 位置: [decoderstack.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/data/decoderstack.h)

- [x] Task 1.2: 修改 DecoderStack::do_decode_work() 快照获取逻辑
  - 优先从 `_owner_document->get_active_logic()` 获取 LogicSnapshot
  - 如果 _owner_document 为 null 或无数据，回退到原有逻辑（从 `_session->get_signals()` 获取）
  - 位置: [decoderstack.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/data/decoderstack.cpp) do_decode_work() 方法

- [x] Task 1.3: 在 add_decoder() 中设置 DecoderStack 的 _owner_document
  - 在 `SigSession::add_decoder()` 创建 DecodeTrace 后，调用 `trace->decoder()->set_owner_document(_active_document)`
  - 位置: [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp) add_decoder() 方法

## Phase 2: SigSession _decode_traces 代理化

- [x] Task 2.1: 移除 SigSession::_decode_traces 成员，添加代理访问方法
  - 移除 `sigsession.h` 中的 `std::vector<view::DecodeTrace*> _decode_traces;` 成员
  - 添加内联方法 `std::vector<view::DecodeTrace*>& decode_traces()` 返回 `_active_document->get_decode_traces()`
  - 添加空指针安全检查：_active_document 为 null 时返回静态空列表
  - 位置: [sigsession.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.h)

- [x] Task 2.2: 修改 SigSession::get_decode_signals() 使用代理
  - 将 `return _decode_traces;` 改为 `return _active_document ? _active_document->get_decode_signals() : _empty_decode_traces;`
  - 位置: [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp)

- [x] Task 2.3: 修改 add_decoder() 使用代理
  - 将 `_decode_traces.push_back(trace)` 改为 `decode_traces().push_back(trace)`
  - 移除 `_active_document->add_decode_trace(trace)` （因为 decode_traces() 已返回 document 的列表）
  - 修改索引参数：`_decode_traces.size()` 改为 `decode_traces().size()`
  - 位置: [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp)

- [x] Task 2.4: 修改 remove_decoder() 使用代理
  - 将所有 `_decode_traces` 访问改为 `decode_traces()`
  - 位置: [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp)

- [x] Task 2.5: 修改 rst_decoder_by_key_handel() 和 get_trace_index_by_key_handel() 使用代理
  - 将所有 `_decode_traces` 访问改为 `decode_traces()`
  - 位置: [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp)

- [x] Task 2.6: 修改 clear_all_decoder() 使用代理
  - 将所有 `_decode_traces` 访问改为 `decode_traces()`
  - 注意：此方法仅清除活跃 Document 的解码器
  - 位置: [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp)

- [x] Task 2.7: 修改 clear_decode_result() 使用代理
  - 将 `_decode_traces` 遍历改为 `decode_traces()` 遍历
  - 位置: [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp)

- [x] Task 2.8: 修改帧结束处理中的 _decode_traces 访问
  - 将帧结束处的 `_decode_traces` 遍历改为 `decode_traces()` 遍历
  - 移除 `_active_document->get_decode_traces() = _decode_traces;` 浅拷贝赋值
  - 位置: [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp) OnMessage DSV_MSG_REV_END_PACKET 处理

- [x] Task 2.9: 修改采集启动处理中的 _decode_traces 访问
  - 将采集启动处的 `_decode_traces` 遍历改为 `decode_traces()` 遍历
  - 位置: [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp) exec_capture() 方法

- [x] Task 2.10: 修改 have_decoded_result() 使用代理
  - 将 `_decode_traces` 访问改为 `decode_traces()`
  - 位置: [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp)

- [x] Task 2.11: 修改 add_decode_task() 和 decode_task_proc() 中的 _decode_traces 访问
  - 检查并修改所有在解码任务相关方法中对 `_decode_traces` 的直接访问
  - 位置: [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp)

## Phase 3: TabContext 标签切换更新 _active_document

- [x] Task 3.1: 修改 TabContext::activate() 更新 _active_document
  - 在 `activate()` 中调用 `_session->set_active_document(_document)`
  - 位置在设置 View 数据源之前
  - 位置: [tabcontext.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/tabcontext.cpp)

- [x] Task 3.2: 修改 MainWindow::on_frame_began() 确认 _active_document 更新
  - 检查 on_frame_began() 中的 set_active_document 调用是否与 activate() 冲突
  - 确保两者协同工作
  - 位置: [mainwindow.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/mainwindow.cpp)

## Phase 4: DecodeTrace::paint_back() 采样率来源修复

- [x] Task 4.1: 修改 DecodeTrace::paint_back() 使用 Document 采样率
  - 将 `_session->cur_snap_samplerate()` 替换为从 _owner_document 或 View 的 effective_data_source 获取采样率
  - 方案：添加 `samplerate()` 方法到 DecodeTrace，优先从 _owner_document 获取，回退到 _session
  - 位置: [decodetrace.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/decodetrace.cpp) paint_back() 方法

## Phase 5: ProtocolDock 标签切换重建

- [x] Task 5.1: 实现 ProtocolDock::rebuild_protocol_layers() 方法
  - 清除所有现有 ProtocolItemLayer 控件
  - 遍历活跃 Document 的 DecodeTrace 列表
  - 为每个 DecodeTrace 创建 ProtocolItemLayer（名称、进度条、设置/删除按钮、格式选择）
  - 连接 decoded_progress 信号
  - 位置: [protocoldock.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/dock/protocoldock.h)、[protocoldock.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/dock/protocoldock.cpp)

- [x] Task 5.2: 修改 ProtocolDock::bind_context() 调用 rebuild_protocol_layers()
  - 在 bind_context() 末尾调用 rebuild_protocol_layers()
  - 位置: [protocoldock.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/dock/protocoldock.cpp)

- [x] Task 5.3: 修改 ProtocolDock::add_protocol_by_id() 使用重建而非手动添加
  - 添加解码器后调用 rebuild_protocol_layers() 或手动添加单个 ProtocolItemLayer
  - 确保新添加的解码器正确出现在 ProtocolItemLayer 列表中
  - 位置: [protocoldock.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/dock/protocoldock.cpp)

## Phase 6: 设备切换/关闭清除所有 Document

- [x] Task 6.1: 新增 SigSession::clear_all_documents_decoders() 方法
  - 遍历所有已注册的 SessionDocument，清除每个 Document 的 _decode_traces
  - 需要 SigSession 持有所有 Document 的列表（或通过 MainWindow 回调）
  - 方案：在 SigSession 中维护 `_all_documents` 列表，或通过回调通知 MainWindow 清除
  - 位置: [sigsession.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.h)、[sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp)

- [x] Task 6.2: 修改 set_device() 和 Close() 调用 clear_all_documents_decoders()
  - 在设备切换和 Session 关闭时清除所有 Document 的解码器
  - 位置: [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp)

- [x] Task 6.3: 修改 MainWindow 在标签关闭时从 _all_documents 移除
  - remove_tab() 中从 SigSession 的文档列表移除被关闭标签的 Document
  - 新建标签时添加到列表
  - 位置: [mainwindow.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/mainwindow.cpp)

## Phase 7: SessionDocument _decoder_stacks 同步维护

- [x] Task 7.1: 确保 SessionDocument._decoder_stacks 与 _decode_traces 同步
  - 当 DecodeTrace 被添加到 Document 时，同步添加其 DecoderStack 到 _decoder_stacks
  - 当 DecodeTrace 被移除时，同步移除其 DecoderStack
  - 位置: [sessiondocument.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/data/sessiondocument.cpp)

## Phase 8: 编译验证与整合

- [x] Task 8.1: 全量编译验证
  - ninja build 通过
  - 无新增编译警告

- [ ] Task 8.2: 功能场景验证
  - 场景 1: Tab A 添加解码器 → 切换到 Tab B → Tab B 无解码器 → 切回 Tab A → 解码器仍在
  - 场景 2: Tab A 添加解码器并采集 → 切换到 Tab B → Tab B 启动新采集 → 切回 Tab A → 解码器注释仍在
  - 场景 3: Tab A 有 SPI 解码器 → Tab B 有 I2C 解码器 → 切换标签 → ProtocolDock 显示正确的解码器
  - 场景 4: 解码器在非活跃标签的解码区域标记位置正确

# Task Dependencies
- [Task 2.*] depends on [Task 1.1, Task 1.3] (代理化需要 _owner_document 已设置)
- [Task 3.1] depends on [Task 2.1] (activate 需要 _active_document 代理已就绪)
- [Task 4.1] depends on [Task 1.1] (paint_back 需要 _owner_document)
- [Task 5.*] depends on [Task 2.*] (ProtocolDock 重建需要代理化完成)
- [Task 6.*] depends on [Task 2.*] (设备切换清除需要代理化完成)
- [Task 7.1] depends on [Task 2.*] (_decoder_stacks 同步需要代理化完成)
- [Task 8.*] depends on [Task 1-7 全部完成]

# Parallelizable Work
- Task 1.1, 1.2 可先独立完成
- Task 2.1-2.11 需要顺序完成（同一文件的连续修改）
- Task 3.1, 4.1 可并行
- Task 5.1-5.3 需要顺序完成
- Task 6.1-6.3 需要顺序完成
