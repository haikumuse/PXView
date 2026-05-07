# 数据快照隔离型多标签页完善 — 任务列表

## Phase 1: Signal 深拷贝基础设施

- [x] Task 1.1: 在 Signal 基类中添加 clone() 虚方法声明
  - 在 `pv/view/signal.h` 的 Signal 类中添加 `virtual Signal* clone() const = 0;` 纯虚方法
  - 位置：[signal.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/signal.h)

- [x] Task 1.2: LogicSignal 实现 clone()
  - 在 `pv/view/logicsignal.h` 中声明 `LogicSignal* clone() const override;`
  - 在 `pv/view/logicsignal.cpp` 中实现：拷贝通道索引、名称、颜色、使能状态，`_data` 设为 nullptr
  - 位置：[logicsignal.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/logicsignal.h)、[logicsignal.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/logicsignal.cpp)

- [x] Task 1.3: AnalogSignal 实现 clone()
  - 在 `pv/view/analogsignal.h` 中声明 `AnalogSignal* clone() const override;`
  - 在 `pv/view/analogsignal.cpp` 中实现：拷贝通道索引、名称、颜色、使能状态、缩放参数，`_data` 设为 nullptr
  - 位置：[analogsignal.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/analogsignal.h)、[analogsignal.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/analogsignal.cpp)

- [x] Task 1.4: DsoSignal 实现 clone()
  - 在 `pv/view/dsosignal.h` 中声明 `DsoSignal* clone() const override;`
  - 在 `pv/view/dsosignal.cpp` 中实现：拷贝通道索引、名称、颜色、使能状态、Vdiv/offset 等参数，`_data` 设为 nullptr
  - 位置：[dsosignal.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/dsosignal.h)、[dsosignal.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/dsosignal.cpp)

## Phase 2: SessionDocument 实现 DataSource 接口

- [x] Task 2.1: SessionDocument 继承 DataSource 接口
  - 修改 `pv/data/sessiondocument.h`：添加 `public data::DataSource` 继承
  - 实现所有 DataSource 纯虚方法
  - 位置：[sessiondocument.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/data/sessiondocument.h)、[sessiondocument.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/data/sessiondocument.cpp)

## Phase 3: View 数据隔离改造

- [x] Task 3.1: 修改 clone_signals_for_document() 使用深拷贝
  - 将 `_own_signals.push_back(sig)` 改为 `_own_signals.push_back(sig->clone())`
  - 位置：[view.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/view.cpp)

- [x] Task 3.2: 修改 set_data_document() 中的 _own_signals 填充逻辑
  - 将 `_own_signals.push_back(sig)` 改为 `_own_signals.push_back(sig->clone())`
  - 位置：[view.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/view.cpp)

- [x] Task 3.3: View 析构时释放 _own_signals
  - 在 `View::~View()` 中遍历 `_own_signals` 释放每个 Signal 对象
  - 位置：[view.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/view.cpp)

- [x] Task 3.4: 修改 get_traces() 不再回退到 _data_source->get_signals()
  - 将 `auto &sigs = _own_signals.empty() ? _data_source->get_signals() : _own_signals;`
  - 改为 `auto &sigs = _own_signals;`
  - 位置：[view.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/view.cpp)

- [x] Task 3.5: View 缩放参数优先从 _document 获取
  - 新增辅助方法 `DataSource* View::effective_data_source()`
  - 将所有 `_data_source->cur_*` 和 `_data_source->get_*_snapshot()` 调用替换为 `effective_data_source()->`
  - 位置：[view.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/view.cpp)

- [x] Task 3.6: 修改 set_data_source() 同时克隆信号
  - 在 `View::set_data_source()` 中，如果 `_own_signals` 为空，自动克隆信号
  - 位置：[view.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/view/view.cpp)

## Phase 4: TabContext 数据源切换

- [x] Task 4.1: 修改 TabContext::activate() 切换 View 数据源
  - 有数据时 set_data_source(document)，无数据时 set_data_source(session)
  - 调用 update_scale_offset() 和 signals_changed() 刷新
  - 位置：[tabcontext.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/tabcontext.cpp)

- [x] Task 4.2: 修改 MainWindow::on_new_tab_requested() 确保新标签为空
  - 新标签通过 activate() 自动走空数据分支，无需额外修改
  - 位置：[mainwindow.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/DSView/pv/mainwindow.cpp)

## Phase 5: 编译验证与整合

- [x] Task 5.1: 全量编译验证
  - ninja build 通过（84/84，退出码 0）
  - 修复了 -Wreorder 警告

- [x] Task 5.2: 整合性检查
  - 所有 include 依赖正确
  - 所有方法签名匹配
  - SessionDocument 的 DataSource 实现完整
  - Signal clone() 实现正确
