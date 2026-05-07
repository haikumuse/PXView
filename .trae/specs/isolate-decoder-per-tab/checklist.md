# 解码器标签隔离 — 检查清单

## Phase 1: DecoderStack 快照来源改造

- [x] 1.1: DecoderStack 新增 _owner_document 成员，有 set/get 方法，构造函数初始化为 nullptr
- [x] 1.2: DecoderStack::do_decode_work() 优先从 _owner_document 获取快照，无数据时回退到 _session->get_signals()
- [x] 1.3: SigSession::add_decoder() 创建 DecodeTrace 后调用 set_owner_document(_active_document)

## Phase 2: SigSession _decode_traces 代理化

- [x] 2.1: SigSession::_decode_traces 成员已移除，decode_traces() 代理方法返回 _active_document->get_decode_traces()
- [x] 2.2: get_decode_signals() 返回 _active_document->get_decode_signals()，_active_document 为 null 时安全返回空列表
- [x] 2.3: add_decoder() 使用 decode_traces() 代理，不再同时添加到 _decode_traces 和 _active_document
- [x] 2.4: remove_decoder() 使用 decode_traces() 代理
- [x] 2.5: rst_decoder_by_key_handel() 和 get_trace_index_by_key_handel() 使用 decode_traces() 代理
- [x] 2.6: clear_all_decoder() 使用 decode_traces() 代理，仅清除活跃 Document 的解码器
- [x] 2.7: clear_decode_result() 使用 decode_traces() 代理，仅清除活跃 Document 的解码结果
- [x] 2.8: 帧结束处理使用 decode_traces() 代理，移除浅拷贝赋值 `_active_document->get_decode_traces() = _decode_traces`
- [x] 2.9: 采集启动处理使用 decode_traces() 代理
- [x] 2.10: have_decoded_result() 使用 decode_traces() 代理
- [x] 2.11: 解码任务相关方法使用 decode_traces() 代理

## Phase 3: TabContext 标签切换更新 _active_document

- [x] 3.1: TabContext::activate() 调用 _session->set_active_document(_document)
- [x] 3.2: on_frame_began() 和 activate() 的 _active_document 更新不冲突

## Phase 4: DecodeTrace::paint_back() 采样率来源修复

- [x] 4.1: DecodeTrace::paint_back() 使用 _owner_document 或回退到 _session 获取采样率

## Phase 5: ProtocolDock 标签切换重建

- [x] 5.1: rebuild_protocol_layers() 正确清除旧层并从活跃 Document 的 DecodeTrace 创建新层
- [x] 5.2: bind_context() 调用 rebuild_protocol_layers()
- [x] 5.3: add_protocol_by_id() 后 ProtocolItemLayer 正确更新

## Phase 6: 设备切换/关闭清除所有 Document

- [x] 6.1: clear_all_documents_decoders() 方法存在且遍历所有 Document 清除解码器
- [x] 6.2: set_device() 和 Close() 调用 clear_all_documents_decoders()
- [x] 6.3: 标签关闭时从文档列表移除，新建标签时添加

## Phase 7: SessionDocument _decoder_stacks 同步维护

- [x] 7.1: SessionDocument._decoder_stacks 与 _decode_traces 保持同步

## Phase 8: 编译与功能验证

- [x] 8.1: 全量编译通过，无新增编译警告
- [ ] 8.2: 场景 1 — Tab A 添加解码器 → 切换 Tab B → 切回 Tab A → 解码器仍在
- [ ] 8.3: 场景 2 — Tab A 解码器有注释 → Tab B 新采集 → 切回 Tab A → 注释仍在
- [ ] 8.4: 场景 3 — Tab A 有 SPI、Tab B 有 I2C → 切换标签 → ProtocolDock 显示正确解码器
- [ ] 8.5: 场景 4 — 非活跃标签的解码区域标记位置正确
