# Spec 审核报告：PXView Core/View 分离与 Headless 模式

## 总体评价

> [!TIP]
> **质量：优秀。** 相比之前的方案，这份 spec 有三个根本性改进：
> 1. 补充了 DataSource 接口改造和 SessionDocument 去视图化（之前遗漏的两个 P0 问题）
> 2. 引入了 SignalModel 中间层设计，而非简陋的 `ChannelInfo{index, name, type}`
> 3. 增加了线程安全保证和 CMake 库分离的明确要求
>
> 以下按 **spec.md → tasks.md → checklist.md** 顺序审核。

---

## 一、spec.md 审核

### ✅ 正确且到位的设计

| 设计决策 | 评价 |
|----------|------|
| DataSource 接口从返回 `view::Signal*` 改为返回 `SignalModel*` | ✅ 命中了之前审核的核心问题 |
| SessionDocument 同步去视图化 | ✅ 补上了之前遗漏 |
| SessionService 移除 `set_view()` 和 `_view` | ✅ 对应 [session_service.h:57,293](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/api/session_service.h#L57) |
| SignalModel 使用 `api::ChannelType` 枚举而非字符串 | ✅ 修正了之前的问题 |
| ISessionCallback 拆分 + 默认空实现保持向后兼容 | ✅ 务实的渐进策略 |
| Headless 模式用 QCoreApplication | ✅ 正确选择 |
| 移除独立 CLI，优先 Headless + MCP/WS | ✅ 聚焦核心价值 |

### ⚠️ 需要注意的问题

#### 1. `ds_set_datafeed_callback` 不支持 `void*` 用户数据

> [!WARNING]
> spec.md 第232行提到"C 回调桥接改用 libsigrok 的用户数据指针传递机制（`ds_session_datafeed_callback_set` 的 `void*` 参数）"。
>
> 但实际 API 签名是：
> ```c
> // libsigrok.h:1399-1400
> typedef void (*ds_datafeed_callback_t)(const struct sr_dev_inst *sdi,
>                                        const struct sr_datafeed_packet *packet);
> 
> // libsigrok.h:1420
> SR_API void ds_set_datafeed_callback(ds_datafeed_callback_t cb);
> ```
>
> **没有 `void*` 用户数据参数！** 回调签名里只有 `sdi` 和 `packet`，没有上下文指针。`ds_session_datafeed_callback_set` 这个函数也**不存在**。

**影响**：移除 `SigSession::_session` 静态成员需要**修改 libsigrok C API** 来新增带 `void* user_data` 的回调设置函数。这是一个跨层改动，tasks.md 中 Task 5.4 也需要同步更新。

**建议**：
- 在 `libsigrok/lib_main.c` 中新增 `ds_set_datafeed_callback_ex(ds_datafeed_callback_ex_t cb, void *user_data)`
- 或者暂时保留 `_session` 静态成员（风险可控，因为当前本身就是单实例），将其降级为 P2

#### 2. SessionSnapshot 也实现了 DataSource，但 spec 未提及

`SessionSnapshot` ([sessionsnapshot.h:50](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/sessionsnapshot.h#L50)) 也实现了 `DataSource` 接口，也持有 `view::Signal*`、`view::DecodeTrace*` 等成员（[sessionsnapshot.h:101-105](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/sessionsnapshot.h#L101-L105)）。

spec 中覆盖了 `SigSession`、`SessionDocument` 的去视图化，但**遗漏了 `SessionSnapshot`**。

**建议**：在 Impact 部分增加 `PXView/pv/data/sessionsnapshot.h/cpp`，在 Requirement 中添加 SessionSnapshot 去视图化。

#### 3. `storesession.cpp` 是 `get_signals()` 的重度消费者

`storesession.cpp` 有 **10 处** 调用 `_session->get_signals()`，是调用最密集的文件之一。它获取 Signal 后主要用于：
- 读取 `signal_type()`（Logic/Analog/DSO）
- 读取 `get_index_list()`（通道索引）
- 读取 `enabled()` 状态
- 确定导出时包含哪些通道

这些信息 **都可以从 SignalModel 获取**，改造路径清晰。但 spec 的 Impact 部分未列出 `storesession.cpp`。

**建议**：在 Affected code 中补充 `PXView/pv/storesession.cpp`。

#### 4. SignalModel 属性列表应更具体

spec 说 SignalModel 包含"触发配置（触发类型、阈值等）"，但当前 `view::LogicSignal` 的触发信息并非属性（它调用 `ds_trigger_probe_set()` 全局 API），而是通过 `set_trig()`/`get_trig()` 管理一个本地枚举值 (`NONTRIG/POSTRIG/NEGTRIG/...`)。

SignalModel 需要明确：**是持有触发状态的只读快照，还是持有可写的触发配置？** 如果可写，还需要提供 `commit_trig()` 之类的方法来同步到 libsigrok。

**建议**：明确 SignalModel 对触发状态的拥有权模型。

#### 5. View 操作的"事件/回调机制"需要更具体

spec 第137行："View 相关操作（show_region、zoom_fit、zoom_in、zoom_out）SHALL 通过新的事件/回调机制通知 View 层"。

当前 `SessionService::zoom_fit()` 的实现直接调用 `_view->zoom_fit()`（[session_service.h:250](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/api/session_service.h#L250)）。改为事件通知的话，谁来监听这个事件？MainWindow？View？

**建议**：可以通过已有的 `IServiceEventListener` 机制，新增 `ServiceEvent::ViewZoomFit` 等事件类型。MainWindow 或 View 注册为 listener 来处理。在 spec 中明确这个设计。

---

## 二、tasks.md 审核

### ✅ 任务分解合理的部分

| 方面 | 评价 |
|------|------|
| 6 个 Phase 的分层结构 | ✅ Phase 1 无破坏性变更，Phase 2 去视图化，Phase 3 适配，合理的渐进路径 |
| Phase 1 "保留旧方法但标记 deprecated" (Task 3.2) | ✅ 务实，避免一次性 big bang |
| Task 依赖图 | ✅ 清晰且正确 |
| Phase 5 验证任务独立于实现 | ✅ 测试驱动思维 |

### ⚠️ 需要修正或补充的任务

#### Task 5.4 — 静态成员移除方案不可行

如上所述，`ds_set_datafeed_callback` **没有 `void*` 用户数据参数**。Task 5.4 描述的"改用 libsigrok 的 void* 用户数据传递回调上下文"无法在不修改 libsigrok API 的情况下完成。

**建议**：
- **选项 A**（推荐）：将 Task 5.4 降为可选项。当前静态 `_session` 是单实例场景，风险可控。
- **选项 B**：新增 Task 5.4a："修改 `libsigrok/lib_main.c`，新增 `ds_set_datafeed_callback_ex(cb, void* user_data)` 和 `ds_set_event_callback_ex(cb, void* user_data)` API"。

#### Task 6 — 遗漏 SessionSnapshot

Task 6 只覆盖了 `SessionDocument`，但 `SessionSnapshot` 也需要同样的改造。

**建议**：增加 Task 6.5："将 `SessionSnapshot` 的 `_signals`、`_decode_traces`、`_spectrum_traces`、`_lissajous_trace`、`_math_trace` 替换为对应的 Model 对象"。

#### Task 8 — 调用点数量不准确

checklist.md 第12行说"55 处调用"，但实际 `get_signals()` 调用点约 **50 处**（含声明/定义）。`static_cast<view::*>` 有约 **16 处**。建议更新为实际数字或写为"所有调用点"。

#### Task 8 — 遗漏 storesession.cpp

Task 8 列出了需要适配的文件，但 **遗漏了 `storesession.cpp`**（10 处 `get_signals()` 调用，是第二大消费者）。还遗漏了 `decoderoptionsdlg.cpp`（2 处）和 `waitingdialog.cpp`（1 处）。

**建议**：
```
8.6 修改 storesession.cpp 使用 SignalModel 读取通道信息进行数据保存
8.7 修改 decoderoptionsdlg.cpp 和 waitingdialog.cpp
```

#### Task 9.2 — 事件通知设计缺乏具体细节

"View 操作改为通过 IServiceEventListener 事件通知" — 需要明确新增哪些 `ServiceEvent` 枚举值。

**建议**：
```
9.2 在 api/types.h 的 ServiceEvent 枚举中新增 ViewShowRegion、ViewZoomFit、
    ViewZoomIn、ViewZoomOut 事件类型，SessionService 改为广播这些事件
```

#### Task 13 — CMake 分离细节不足

Task 13 说"pxview-core 不链接 Qt::Widgets，仅链接 Qt::Core、Qt::Concurrent"。但需要确认：

1. `SigSession` 当前使用 `QDateTime`、`QString` — 这些在 `Qt::Core` 中，✅ 没问题
2. `SessionDocument` 使用 `QJsonObject`、`QJsonArray` — `Qt::Core`，✅ 没问题
3. `SessionService` 使用 `QEventLoop`、`QTimer` — `Qt::Core`，✅ 没问题
4. `SessionService` 使用 `QApplication::processEvents()` — ⚠️ 这是 `Qt::Widgets`！

**问题**：`SessionService::configure_and_start()` 多处调用 `QCoreApplication::processEvents()`，如果用 `QCoreApplication` 则没问题，但代码中 `#include <QApplication>` 会引入 Qt::Widgets 依赖。

**建议**：Task 13 增加子任务："13.5 将 `session_service.cpp` 中的 `#include <QApplication>` 改为 `#include <QCoreApplication>`"。

---

## 三、checklist.md 审核

### ✅ 覆盖良好的检查项

大部分检查项与 spec 和 tasks 对应良好。特别好的有：
- 第6项："SigSession 不再 #include 任何 pv/view/*.h 头文件" — 可编译验证
- 第18项："pxview-core 静态库可独立编译，不依赖 Qt::Widgets" — 明确的交付标准
- 第15-17项：Headless 端到端验证

### ⚠️ 缺少的检查项

| 缺少项 | 原因 |
|--------|------|
| `SessionSnapshot` 不再持有 view::* 类型成员 | spec 遗漏了 SessionSnapshot |
| `storesession.cpp` 已适配 SignalModel | tasks 遗漏了最大消费者之一 |
| `session_service.cpp` 不再 `#include <QApplication>` | CMake 分离的前提 |
| View 层 `effective_data_source()` 调用链正常工作 | 这是 DataSource 接口变更后最易出 bug 的路径 |
| 解码器在 Headless 模式下可正常运行 | 当前 `add_decoder()` 内部创建 `view::DecodeTrace`，去视图化后需要验证 |

---

## 四、关键风险提醒

> [!CAUTION]
> ### 风险 1：`SigSession::add_decoder()` 的输出参数
> 
> 当前签名：
> ```cpp
> bool add_decoder(srd_decoder *const dec, bool silent, DecoderStatus *dstatus,
>                  std::list<pv::data::decode::Decoder *> &sub_decoders,
>                  view::Trace *&out_trace);  // ← 返回 View 层对象
> ```
> 这个方法通过 `out_trace` 输出一个 `view::Trace*`。去视图化后需要改为返回 `DecodeModel*` 或 index。**SessionService** 的 `add_decoder()` 实现 ([session_service.cpp:2236](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/api/session_service.cpp#L2236)) 用 `static_cast<view::DecodeTrace*>(out_trace)` 获取 DecodeTrace。这是一个需要特别小心处理的接口变更。

> [!CAUTION]
> ### 风险 2：DecoderStack 内部也调用 `get_signals()`
> 
> `decoderstack.cpp` 使用 `_session->get_signals()` 来查找解码器需要的信号通道。这意味着**数据层**（非 View 层）也依赖 `view::Signal*`。去视图化后 DecoderStack 需要改为从 SignalModel 获取通道信息。

---

## 五、总结

> [!IMPORTANT]
> **整体质量优秀**，三份文档形成了完整的 spec → tasks → checklist 闭环。主要需要补充的：
> 
> 1. **SessionSnapshot 遗漏**：它也是 DataSource 的实现者，需要同步去视图化
> 2. **静态成员移除方案不可行**：libsigrok 的回调 API 没有 `void*` 参数，建议降级为可选
> 3. **storesession.cpp 遗漏**：10 处 `get_signals()` 调用，是最大消费者之一
> 4. **QApplication 依赖**：session_service.cpp 可能引入 Qt::Widgets，需要改为 QCoreApplication
> 
> 以上 4 项修复后，spec 即可作为正式的改造指导文档使用。
