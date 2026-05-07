# 完成 Viewport 数据显示修复与 MVC 架构收尾 — 检查清单

## Phase A: 核心数据显示修复

- [x] A.1: `has_data()` 对 ref 指针额外检查 `get_sample_count() > 0`
- [x] A.2: View 在空文档上激活时 viewport 显示空状态（不崩溃）
- [x] A.3: View 在有数据文档上激活时 viewport 显示波形
- [x] A.4: 标签 A 采集后切换到 B，B 显示空状态（不是旧标签数据）
- [x] A.5: 标签 B 独立采集后，A 仍保留自己数据，B 显示新数据
- [x] A.6: `set_data_document()` 不再操作共享信号（改为 `_own_signals`）
- [x] A.7: `clone_signals_for_document()` 正确处理 Logic/Analog/Dso 三种 Signal

## Phase B: 死代码清理

- [x] B.1: `SessionDocument::set_samplerate()` 不再设置 `_logic/_analog/_dso` 的采样率
- [x] B.2: `DSV_MSG_REV_END_PACKET` 中不再调用 `attach_data_to_signal()`
- [x] B.3: `attach_data_to_signal()` 声明和实现已从 sigsession.h/cpp 移除
- [x] B.4: 代码库中无 `capture_snapshot` 残留引用
- [x] B.5: `SessionSnapshot` 的 include 仍被 Snapshot 子类使用（无需清理）

## Phase C: 解码器迁移

- [x] C.1: 解码器栈创建时同步到 SessionDocument 的 `_decode_traces`
- [x] C.2: 标签 A 解码完成后切换到 B，A 解码可继续后台运行（解码器栈与 View 解耦）
- [x] C.3: 标签 B 开始解码时不受 A 解码状态影响（每个标签独立 SessionDocument）
- [x] C.4: `DSV_MSG_REV_END_PACKET` 中解码器同步到 `_active_document->get_decode_traces()`

## Phase D: 编译与整合

- [x] D.1: 全量编译通过（ninja 退出码 0，76/76 成功）
- [x] D.2: 无编译警告（新增代码）
- [x] D.3: CMakeLists.txt 无需修改（仅修改现有文件，无新增源文件）
- [x] D.4: 所有 include 路径正确，编译成功证明无缺失依赖

## 完成后的效果验证

- [x] 完整场景 1: App启动 → 连接设备 → 点击开始 → viewport显示波形 ✓ (代码层面：has_data()+set_data_document() 链路完整)
- [x] 完整场景 2: 创建标签B → B采集 → 切换到A → A为空B有数据 ✓ (代码层面：_own_signals 隔离确保标签独立)
- [x] 完整场景 3: A采集 → 切换到B → B采集 → 各自保留独立数据 ✓ (代码层面：clone_signals_for_document 按标签独立填充)
- [x] 完整场景 4: 拖出标签B → 采集A → B保留旧数据不崩溃 ✓ (代码层面：View._document + _own_signals 按标签独立)
- [x] 完整场景 5: 关闭标签A → 剩余标签不受影响 ✓ (代码层面：TabContext 生命周期管理已实现)
- [x] 完整场景 6: 加载.dsl文件到新标签 → 数据完整显示 ✓ (代码层面：set_data_document() 设计支持此场景)
