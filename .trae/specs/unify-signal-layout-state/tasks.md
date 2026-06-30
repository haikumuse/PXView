# Tasks

- [ ] Task 1: 扩展 ChannelConfig 结构体与 ChannelLayoutState 定义
  - [ ] SubTask 1.1: `sessiondocument.h` `ChannelConfig` 新增 `int view_index=-1; int v_offset=0; int own_height=-1;` 字段，更新构造函数初始化列表
  - [ ] SubTask 1.2: `sessiondocument.h` 新增 `ChannelLayoutState` 结构体（`{int view_index; int v_offset; int own_height;}`），用于 save_signal_config 参数传递

- [ ] Task 2: 序列化布局字段
  - [ ] SubTask 2.1: `sessiondocument.cpp` `signal_config_to_json` 在 ch_obj 中写入 `view_index`/`v_offset`/`own_height`
  - [ ] SubTask 2.2: `signal_config_from_json` 读取三字段，缺失时使用默认值（向后兼容旧 .pxc）

- [ ] Task 3: 修改 save_signal_config 接受布局参数
  - [ ] SubTask 3.1: `sessiondocument.h` `save_signal_config` 签名新增 `const std::map<int, ChannelLayoutState> &channel_layout = {}`
  - [ ] SubTask 3.2: `sessiondocument.cpp` 实现中按 channel index 匹配 `channel_layout`，写入 `cfg.view_index`/`cfg.v_offset`/`cfg.own_height`；map 中无此 index 时保持默认值

- [ ] Task 4: rebuild_signals_from_config 从配置恢复布局
  - [ ] SubTask 4.1: `view.cpp:2154-2161` 删除 `set_own_height(-1)` 和 `set_view_index(view_index++)` 重置逻辑
  - [ ] SubTask 4.2: 替换为：若 `ch.view_index >= 0` 用配置值，否则按启用顺序派生（保留 view_index 局部变量用于派生）
  - [ ] SubTask 4.3: `v_offset` 直接 `signal->set_v_offset(ch.v_offset)`
  - [ ] SubTask 4.4: `own_height`：若 `ch.own_height >= 0` 用配置值，否则 DSO/Analog 保持 -1（自动高度）、Logic 不调用 set_own_height（由 Trace 构造函数处理主题默认）

- [ ] Task 5: rebuild_signals 清理二次重置
  - [ ] SubTask 5.1: `view.cpp:2233-2236` 删除 `rebuild_signals` create 分支中对 DSO/Analog 的 `set_own_height(-1)`（新建信号场景下 create_signals 已用默认高度，无需重置）
  - [ ] SubTask 5.2: 验证 rebuild_signals 的 config-based 分支不再二次重置（rebuild_signals_from_config 已统一处理）

- [ ] Task 6: TabContext::deactivate 收集布局
  - [ ] SubTask 6.1: `tabcontext.cpp:140-148` 扩展收集逻辑：遍历 `_view->get_own_signals()`，除 visibility 外同时收集 view_index/v_offset/own_height 到 `std::map<int, ChannelLayoutState> channel_layout`
  - [ ] SubTask 6.2: 将 `channel_layout` 传入 `save_signal_config`

- [ ] Task 7: MainWindow 6 处 save_signal_config 调用补参数
  - [ ] SubTask 7.1: `mainwindow.cpp:370` setup_ui 路径（无 view，传空 map）
  - [ ] SubTask 7.2: `mainwindow.cpp:2420` 路径从 `current_view()->get_own_signals()` 收集布局传入
  - [ ] SubTask 7.3: `mainwindow.cpp:2884` 路径收集布局传入
  - [ ] SubTask 7.4: `mainwindow.cpp:2955` DSV_MSG_DEVICE_OPTIONS_UPDATED 路径收集布局传入
  - [ ] SubTask 7.5: `mainwindow.cpp:2987` 路径收集布局传入
  - [ ] SubTask 7.6: `mainwindow.cpp:3623` new_doc 路径（无 view，传空 map）

- [ ] Task 8: 构建验证 + checklist 核对
  - [ ] SubTask 8.1: `cd build && ninja -j 16 && ninja install` 构建成功，无新增 warning
  - [ ] SubTask 8.2: 逐项核对 `checklist.md`
  - [ ] SubTask 8.3: GUI 回归：调整通道顺序/高度 → 重新采集 → 验证布局保留
  - [ ] SubTask 8.4: GUI 回归：调整通道顺序/高度 → 切 tab 再切回 → 验证布局保留
  - [ ] SubTask 8.5: GUI 回归：加载旧 .pxc 文件 → 验证不报错、布局使用默认值

# Task Dependencies
- Task 1（结构体扩展）是所有任务的前置
- Task 2、Task 3 依赖 Task 1，可并行
- Task 4 依赖 Task 1（读取新字段）
- Task 5 依赖 Task 4（避免双重处理）
- Task 6、Task 7 依赖 Task 3（save_signal_config 新签名）
- Task 8 依赖所有 Task 完成

# 并行执行建议
- 第一批：Task 1
- 第二批（并行）：Task 2、Task 3
- 第三批（并行）：Task 4、Task 6、Task 7
- 第四批：Task 5（依赖 Task 4）
- 第五批：Task 8
