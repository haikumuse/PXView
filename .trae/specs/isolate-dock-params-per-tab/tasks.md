# Tasks

## Phase 1: SessionDocument 添加 Dock 参数缓存

- [ ] Task 1.1: 在 SessionDocument 中添加所有 Dock 参数缓存字段
  - 添加采样参数：`uint64_t _sample_rate = 0;`、`uint64_t _sample_limit = 0;`、`int _collect_mode = 0;`
  - 添加搜索模式：`std::map<uint16_t, QString> _search_pattern;`
  - 添加测量配置：`bool _measure_fen_enabled = false;`、`QJsonArray _measure_dist_rows;`、`QJsonArray _measure_edge_rows;`
  - 添加触发配置：`QJsonObject _trigger_session;`、`QJsonObject _dso_trigger_session;`
  - 添加设备选项：`QJsonObject _device_options_session;`（包含通道使能、通道模式、Operation Mode、Vdiv、Coupling、Map配置）
  - 在 `get_session()` / `set_session()` 中添加序列化/反序列化
  - 位置：[sessiondocument.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/data/sessiondocument.h)、[sessiondocument.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/data/sessiondocument.cpp)

## Phase 2: DeviceOptionsDock 实现 IContextAware

- [ ] Task 2.1: DeviceOptionsDock 继承 IContextAware 并实现 bind/unbind
  - 在 `deviceoptionsdock.h` 中添加 `public IContextAware`
  - 实现 `bind_context(TabContext *ctx)`: 调用 `update_view()` 刷新界面，从 SessionDocument 恢复设备选项参数到硬件和 UI
  - 实现 `unbind_context()`: 保存当前设备选项参数到 SessionDocument
  - 位置：[deviceoptionsdock.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/dock/deviceoptionsdock.h)、[deviceoptionsdock.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/dock/deviceoptionsdock.cpp)

- [ ] Task 2.2: DeviceOptionsDock 添加 get_session/set_session 方法
  - `QJsonObject get_session()`: 序列化当前通道使能、通道模式、Operation Mode、Vdiv、Coupling、Map配置
  - `void set_session(QJsonObject &obj)`: 从 JSON 恢复配置到硬件和 UI
  - 位置：[deviceoptionsdock.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/dock/deviceoptionsdock.cpp)

## Phase 3: SamplingBar 采样参数保存/恢复

- [ ] Task 3.1: SamplingBar 在 bind/unbind_context 中保存/恢复采样参数
  - `unbind_context()`: 保存当前采样率、采样深度、采集模式到 SessionDocument
  - `bind_context()`: 从 SessionDocument 恢复采样率、采样深度到 DeviceAgent，恢复采集模式到 SigSession 和 UI
  - 位置：[samplingbar.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/toolbars/samplingbar.cpp)

## Phase 4: SearchDock 搜索模式保存/恢复

- [ ] Task 4.1: SearchDock 在 bind/unbind_context 中保存/恢复搜索模式
  - `unbind_context()`: 将 `_pattern` 保存到 SessionDocument
  - `bind_context()`: 从 SessionDocument 恢复 `_pattern`，在 `rebuild_pattern()` 之后应用到 UI 控件
  - 位置：[searchdock.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/dock/searchdock.cpp)

## Phase 5: MeasureDock 测量配置保存/恢复

- [ ] Task 5.1: MeasureDock 在 bind/unbind_context 中保存/恢复测量配置
  - `unbind_context()`: 保存浮动测量开关、距离/边沿测量行到 SessionDocument
  - `bind_context()`: 从 SessionDocument 恢复浮动测量开关、距离/边沿测量行
  - 位置：[measuredock.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/dock/measuredock.cpp)

## Phase 6: TriggerDock 触发配置保存/恢复

- [ ] Task 6.1: TriggerDock 在 bind/unbind_context 中保存/恢复触发配置
  - `unbind_context()`: 调用 `get_session()` 保存到 SessionDocument
  - `bind_context()`: 从 SessionDocument 调用 `set_session()` 恢复触发配置到硬件和 UI
  - 位置：[triggerdock.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/dock/triggerdock.cpp)

- [ ] Task 6.2: DsoTriggerDock 添加 get_session/set_session 方法
  - 添加 `QJsonObject get_session();` 和 `void set_session(QJsonObject &obj);`
  - 序列化：触发位置、触发源、触发类型、Holdoff、噪声灵敏度
  - 位置：[dsotrigerdock.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/dock/dsotrigerdock.h)、[dsotrigerdock.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/dock/dsotrigerdock.cpp)

- [ ] Task 6.3: DsoTriggerDock 在 bind/unbind_context 中保存/恢复触发配置
  - `unbind_context()`: 调用 `get_session()` 保存到 SessionDocument
  - `bind_context()`: 从 SessionDocument 调用 `set_session()` 恢复触发配置
  - 位置：[dsotrigerdock.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/dock/dsotrigerdock.cpp)

## Phase 7: MainWindow on_tab_changed 更新

- [ ] Task 7.1: 在 on_tab_changed 中添加所有 Dock 的 unbind/bind 调用
  - 添加 `_device_options_widget->unbind_context()` 和 `_device_options_widget->bind_context(new_ctx)`
  - 替换现有的 `_device_options_widget->update_view()` 为 bind_context
  - 确保 TriggerDock/DsoTriggerDock 的 unbind/bind 在标签切换时被调用
  - 位置：[mainwindow.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/mainwindow.cpp)

## Phase 8: 编译验证

- [ ] Task 8.1: 全量编译验证
  - 确保无编译错误
  - 验证切换标签时各 Dock 参数正确保存/恢复

# Task Dependencies
- [Task 2.1, 2.2] depend on [Task 1.1]
- [Task 3.1] depends on [Task 1.1]
- [Task 4.1] depends on [Task 1.1]
- [Task 5.1] depends on [Task 1.1]
- [Task 6.1] depends on [Task 1.1]
- [Task 6.3] depends on [Task 1.1, Task 6.2]
- [Task 7.1] depends on [Task 2.1, 3.1, 4.1, 5.1, 6.1, 6.3]
- [Task 8.1] depends on [Task 1-7 全部完成]

# Parallelizable Work
- Task 2.x, 3.1, 4.1, 5.1, 6.1, 6.2 可并行执行（都依赖 Task 1.1）
- Task 6.3 depends on Task 6.2
