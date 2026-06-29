# SignalModel 属性变更通知架构修复

## Context（背景与动机）

**问题**：通过 MCP 设置上升沿触发后，Header 不显示上升沿选中标记。

**根因**：触发状态散落在三处，无同步机制——
1. `ds_trigger_probe_set()`（libsigrok 设备）— MCP 已更新 ✓
2. `SignalModel::_trig_type`（Core）— MCP 已更新，但 setter 是纯赋值，无通知
3. `LogicSignal::_trig`（View）— Header 读取此值渲染，但只在对象创建时从 model 拷贝一次，之后与 model 脱钩

`SignalModel::set_trig_type()` 是哑赋值，不通知任何人；`LogicSignal` 不持有 model 引用，无法监听变更。导致 MCP 更新 model 后 View 永远不知道。

**目标架构**：`SignalModel` 成为触发状态的单一数据源。`set_trig_type()` 发出属性变更通知，View 层 `LogicSignal` 自动监听并更新本地副本 → Header repaint。

## 修复方案：SignalModel 继承 QObject + Qt 信号槽

**为什么用 Qt signals/slots**：LogicSignal 已是 QObject（通过 Trace→SelectableItem→QObject）。Qt 信号槽天然处理生命周期——任一方被 `delete` 时连接自动断开，无悬空指针风险。SignalModel 继承 QObject 需 Qt6::Core（Core 层已允许，只禁 Widgets/Svg）。

### 改动 1：SignalModel 继承 QObject（核心）

**文件**：`PXView/pv/data/signalmodel.h`、`signalmodel.cpp`

- 类声明改为 `class SignalModel : public QObject`，加 `Q_OBJECT` 宏
- `#include <QObject>`
- 删除 `= default` 拷贝构造/赋值（QObject 禁止拷贝；验证确认无代码拷贝 SignalModel）
- 新增 `signals: void trig_type_changed(int trig_type);`
- `set_trig_type()` 改为：赋值后 `if (old != new) emit trig_type_changed(_trig_type);`
- 构造函数：保留无参版本；`init_signals()`/`reload()` 中的 `new SignalModel()` 调用不需改（无 parent，手动 delete 管理，与现有一致）

### 改动 2：CMakeLists.txt 加入 moc 处理

**文件**：`CMakeLists.txt`（第 427 行 `PXView_HEADERS` 列表）

- 添加 `PXView/pv/data/signalmodel.h` 到列表
- `qt6_wrap_cpp` 会自动生成 `moc_signalmodel.cpp`，编译进可执行文件（第 846 行注释已说明此机制）

### 改动 3：SignalFactory 建立信号槽连接

**文件**：`PXView/pv/view/signalfactory.cpp`（`apply_model_properties` 函数，第 73-90 行）

在 LogicSignal 分支中，`set_trig()` 之后加 `connect`：
```cpp
if (auto *logic_sig = dynamic_cast<LogicSignal*>(signal)) {
    logic_sig->set_trig(model->trig_type());
    QObject::connect(model, &data::SignalModel::trig_type_changed,
                     logic_sig, &LogicSignal::set_trig);
}
```
- `apply_model_properties` 在 `create_signal()`（新建）和 `update_signals(Modified)`（属性刷新）都会调用，覆盖所有路径
- LogicSignal 被 `delete` 或 SignalModel 被 `delete` 时，Qt 自动断开连接

### 改动 4：init_signals() 末尾通知 View 重建

**文件**：`PXView/pv/sigsession.cpp`（`init_signals()` 函数末尾，约第 1027 行）

`init_signals()` 销毁旧 SignalModel、创建新 model，但不通知 View。旧 LogicSignal 的连接已自动断开（model 被 delete），但新 model 没有对应的 LogicSignal 连接。

- 在 `init_signals()` 末尾调用 `signals_changed();`（触发 ISessionStateCallback 回调链）
- 这确保 init_signals 后 View 会重建 LogicSignal，从新 model 读取属性并建立 connect

### 改动 5：修复 View::on_signals_changed() 死代码

**文件**：`PXView/pv/mainwindow.cpp`（`on_signals_changed()` 第 2348 行）

当前 `MainWindow::on_signals_changed()` 只调用 `view->signals_changed(NULL)`（布局刷新），不调用 `View::on_signals_changed()`（SignalFactory 重建）。导致即使回调触发，View 也不从 model 重建 LogicSignal。

改为同时调用两者：
```cpp
void MainWindow::on_signals_changed() {
    current_view()->on_signals_changed();  // SignalFactory::update_signals(AllReplaced) 重建
    current_view()->signals_changed(NULL);   // 布局刷新
}
```

`View::on_signals_changed()`（view.cpp:2400）调用 `SignalFactory::update_signals(AllReplaced)`，会 save UI state → delete 旧 signal → create 新 signal（含 connect）→ restore UI state。对少量 channel（<16）性能可接受。

### 改动 6：MCP start_capture 时序调整

**文件**：`PXView/pv/api/session_service.cpp`（第 654-696 行）

当前顺序：`init_signals()` → `processEvents()` → block 2c (set_trig_type)。改为：`init_signals()` → block 2c → 触发 View 重建。

具体：把 block 2c（设置新 model 的 trig_type）移到 `init_signals()` 之后、`processEvents()` 之前。这样当 View 重建时（由 init_signals 的 signals_changed 回调触发，经 processEvents 派发），model 已有正确 trig_type，LogicSignal 创建即读到正确值。

有了信号通知后，MCP 后续修改 trig_type（如 `set_trigger` RPC）无需重建，直接 `model->set_trig_type()` → emit → LogicSignal 更新。

## 生命周期安全分析

| 场景 | 行为 | 安全性 |
|------|------|--------|
| SignalModel 被 delete（init_signals/reload） | Qt 自动断开所有 connect | LogicSignal 不崩溃，只是不再收到通知 |
| LogicSignal 被 delete（AllReplaced 重建） | Qt 自动断开 connect | SignalModel 不持悬空引用 |
| MCP 设 trig_type | emit 信号 → 已连接的 LogicSignal::set_trig | Header repaint ✓ |
| init_signals 后无 LogicSignal 连接新 model | init_signals 末尾 signals_changed → View 重建 → connect 建立 | 覆盖 ✓ |

## 不改动的部分

- `LogicSignal` 不新增 SignalModel 指针成员——connect 不需要持有指针，Qt 内部管理连接
- `commit_trig()`（SignalModel 和 LogicSignal 各有一份）保持不变——它们直接写 libsigrok，与信号通知无关
- `DataSource` 接口不变
- 回调接口（ISessionStateCallback）不变

## 验证方案

1. **编译验证**：`build_incremental.cmd` 编译通过，无 moc/链接错误
2. **MCP 触发显示**：headless 模式下 MCP `start_capture(triggerType="rising")` → 检查 PXView.log 确认 trig_type 设置成功
3. **GUI 触发显示**：启动 GUI，设置上升沿触发 → Header 显示上升沿选中标记
4. **状态保持**：切换 tab 后返回，触发状态保持
5. **回归测试**：现有 GUI 触发设置流程（TriggerDock）不受影响——`LogicSignal::set_trig()` 仍可直接调用，现在多了"model 变更自动同步"路径，两者互补
