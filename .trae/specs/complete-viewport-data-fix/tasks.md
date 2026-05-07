# 完成 Viewport 数据显示修复与 MVC 架构收尾 — 任务列表

## Phase A: 核心数据显示修复（阻塞性问题）

- [x] Task A.1: 修复 `SessionDocument::has_data()` 的数据检查逻辑
  - 修改 `has_data()`：ref 指针非空时，额外检查 `_ref_*->get_sample_count() > 0`
  - 确保只有实际有数据的 Snapshot 才被判定为有数据
  - 位置：[sessiondocument.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/data/sessiondocument.cpp)

- [x] Task A.2: 实现 `View::clone_signals_for_document()` 填充 `_own_signals`
  - 从 SigSession 的 `_signals` 列表克隆信号对象到 View 的 `_own_signals`
  - 克隆时不拷贝数据，仅创建新的 Signal 对象并将 `_data` 设为 document 的 `get_active_*()`
  - Signal 子类（LogicSignal, AnalogSignal, DsoSignal）需要支持拷贝构造或 clone() 方法
  - 位置：[view.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/view.cpp)

- [x] Task A.3: 修改 `View::set_data_document()` 操作 `_own_signals`
  - 将迭代目标从 `_data_source->get_signals()` 改为 `_own_signals`
  - 如果 `_own_signals` 为空，先调用 `clone_signals_for_document(doc)` 填充
  - 如果 `_own_signals` 已存在，只更新每个信号的 `_data` 指针
  - 位置：[view.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/view.cpp#L270-L294)

- [x] Task A.4: 修改 View 的 paint/get_signal 等方法优先使用 `_own_signals`
  - 所有使用 `_data_source->get_signals()` 的渲染路径改为使用 `_own_signals`
  - 包括：`signals_changed()`, viewport paint, trace rendering, cursor probing 等
  - 位置：[view.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/view.cpp)

## Phase B: 死代码清理和冗余消除

- [x] Task B.1: 清理 `SessionDocument::set_samplerate()` 的死代码
  - 移除 `_logic.set_samplerate(rate)`, `_analog.set_samplerate(rate)`, `_dso.set_samplerate(rate)`
  - 只保留 `_samplerate = rate; if (rate > 0)` 分支作为元数据存储
  - 位置：[sessiondocument.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/data/sessiondocument.cpp)

- [x] Task B.2: 移除 `DSV_MSG_REV_END_PACKET` 中的 `attach_data_to_signal()` 调用
  - 在 [sigsession.cpp:2267](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp#L2267) 行移除 `attach_data_to_signal(_view_data)`
  - 保留 `_view_data = _capture_data` 交换逻辑不变
  - 数据绑定交给 `on_frame_ended()` → `set_data_document()` 统一处理

- [x] Task B.3: 移除 `attach_data_to_signal()` 方法定义和声明
  - 从 sigsession.h 移除声明
  - 从 sigsession.cpp 移除实现
  - 检查其他调用点，确保无遗漏

- [x] Task B.4: 确认 `capture_snapshot()` 残留引用已全部清理
  - 全局搜索 `capture_snapshot` 确保无残留调用
  - 清理 `SessionSnapshot` 的 include（如果不再使用）

## Phase C: 解码器迁移到 SessionDocument（架构完成）

- [x] Task C.1: 扩展 `SessionDocument` 的解码器栈支持
  - 完善 `_decoder_stacks` 的管理（已在骨架中）
  - 新增 `get_decode_traces()` 和 `add_decode_trace()` 方法
  - 新增 `_decode_traces` 成员
  - 位置：[sessiondocument.h/cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/data/sessiondocument.h)

- [x] Task C.2: 修改 `add_decode_trace` 从 SigSession 同步到 SessionDocument
  - SigSession::add_decode_task() 中新增 `_active_document->add_decode_trace(trace)` 同步
  - SigSession 仍保留 `_decode_traces` 列表以维持现有解码任务调度

- [x] Task C.3: 解码器栈归属变更
  - 解码器栈通过双向同步保持 SigSession 和 SessionDocument 之间的一致性
  - 解码完成后通知对应的 TabContext（通过现有 SigSession 信号链）
  - `decode_done` 回调链路保持不变，通过 SessionDocument 引用可路由到正确 TabContext

- [x] Task C.4: 修改 `DSV_MSG_REV_END_PACKET` 中的解码器同步逻辑
  - 在 capture 结束时将 `_decode_traces` 同步到 `_active_document->get_decode_traces()`
  - 位置：[sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/sigsession.cpp#L2288-L2290)

## Phase D: 编译验证 + 最终整合

- [x] Task D.1: 全量编译验证
  - ninja build 通过（76/76，退出码 0）
  - 所有修改编译通过
  - 仅有预存在的编译警告，无新增警告

- [x] Task D.2: 整合性检查
  - 所有 include 依赖正确
  - 所有方法签名匹配
  - CMakeLists.txt 无需修改（仅修改现有文件，无新增源文件）

## 任务依赖

```
A.1 (has_data fix)
 ├── A.2 (clone_signals) ──┬── A.3 (set_data_document 改 _own_signals)
 │                         │        │
 │                         │        ├── A.4 (paint/get_signal 改用 _own_signals)
 │                         │        │
 │                         │        └── D.1 (编译)
 │                         │
 └── B.1 (死代码清理) ──────┤
                             │
B.2 (移除 attach_data_to_signal) ──┬── B.3 (移除方法本身)
                                   │
B.4 (capture_snapshot 残留) ────────┤
                                   │
C.1 (解码器栈扩展) ──┬── C.2 (调度迁移) ── C.4 (END PACKET 改)
                    │        │
                    │        └── C.3 (decode_done 回调路由)
                    │
                    └── D.2 (整合检查)
```

**可并行执行**：
- A.1 和 B.1 可并行
- A.2 依赖 A.1 完成后开始
- B.2-B.4 与 A 阶段独立，可并行
- C.1-C.4 独立于 A/B 阶段，可并行
- D.1 依赖所有阶段完成
