# Phase 2-4 检查清单

## Phase 2: 数据路由与 View 隔离

- [x] 2.1: `set_data_document()` 在空文档时不覆盖 Signal._data，BUG 修复生效
- [x] 2.2: 采集完成后数据正确写入活跃 SessionDocument（copy_data_to_document + set_snapshot_refs）
- [ ] 2.3: 采集完成后视图自动刷新显示波形数据（需运行时测试验证）
- [ ] 2.4: 新建标签 B 后采集，标签 A/B 各自显示正确数据（A=历史，B=新采集）（需运行时测试）
- [ ] 2.5: 从标签 B 切换回标签 A，A 恢复显示其原始数据（需运行时测试）
- [ ] 2.6: 新建标签 C（无数据）时，C 显示空状态无误（需运行时测试）
- [ ] 2.7: View._own_signals 克隆与 SessionDocument 数据正确绑定（暂缓：Snapshot 不支持拷贝）
- [ ] 2.8: 标签切换时不发生深拷贝，切换速度 < 100ms（需运行时测试）
- [x] 2.9: DsoSignal 在共享模式下，所有标签 DSO 数据显示一致（共享 `_view_data`）
- [x] 2.10: Phase 2 全部文件编译通过（ninja 退出码 0）

## Phase 3: 生命周期管理与 Safe Detach/Attach

- [x] 3.1: SessionManager 单例创建成功，无重复实例（编译验证）
- [ ] 3.2: 拖出标签 → 创建浮动窗口 → 窗口非空白（有数据则显示数据）（需运行时测试）
- [ ] 3.3: 关闭浮动窗口 → View 还原到主窗口标签栏 → 数据完整（需运行时测试）
- [ ] 3.4: 关闭标签 → 标签移除 → 剩余标签至少 1 个 → 无崩溃（需运行时测试）
- [ ] 3.5: 关闭标签 → SessionDocument 内存释放 → 无泄漏（需运行时工具验证）
- [x] 3.6: MeasureDock 实现了 IContextAware::bind_context/unbind_context（编译验证）
- [x] 3.7: ProtocolDock 实现了 IContextAware::bind_context/unbind_context（编译验证）
- [x] 3.8: SearchDock 实现了 IContextAware::bind_context/unbind_context（编译验证）
- [x] 3.9: SamplingBar 实现了 IContextAware::bind_context（LIVE时可编辑，HISTORICAL时只读）（编译验证）
- [ ] 3.10: 浮动窗口获得焦点 → 主窗口 Dock 切换到对应上下文（需运行时测试）
- [x] 3.11: Phase 3 全部文件编译通过（ninja 退出码 0）

## Phase 4: 硬件路由与解码器隔离

- [x] 4.1: 实时采集后数据通过快照引用同步写入活跃 SessionDocument（编译验证）
- [ ] 4.2: 实时采集中间，活跃标签 View 实时刷新显示波形（需运行时测试）
- [x] 4.3: SessionDocument 独立解码器栈骨架创建完成（编译验证）
- [ ] 4.4: 标签 A 解码完成后切换到 B，A 解码继续后台运行（暂缓：解码器迁移未完成）
- [ ] 4.5: 后台标签 A 解码完成后，切换回 A 时 DecodeTrace 已更新（暂缓）
- [ ] 4.6: 标签 B 开始解码时不受 A 解码状态影响（暂缓）
- [x] 4.7: `capture_snapshot()` 声明和实现已移除
- [x] 4.8: SessionDocument 替代旧的 SessionSnapshot（capture_snapshot 已清理）
- [x] 4.9: Phase 4 全部文件编译通过（ninja 退出码 0）

## 全流程端到端

- [ ] 创建 3 个标签 → 每个独立采集 → 每个显示各自数据（需运行时测试）
- [ ] 拖出标签 2 → 采集标签 1 → 标签 2 仍保持旧数据（需运行时测试）
- [ ] 关闭标签 2 → 标签 1/3 不受影响 → 无崩溃（需运行时测试）
- [ ] 标签 1 添加解码器 → 切换到标签 3 → 标签 1 解码继续（暂缓）
- [ ] 加载 .dsl 文件到新标签 → 数据完整显示（需运行时测试）
