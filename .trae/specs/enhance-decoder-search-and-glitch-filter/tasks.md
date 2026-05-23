# Tasks

- [x] Task 1: 解码器搜索类型过滤
  - [x] SubTask 1.1: 在 `SearchDataItem` 中新增 `_is_c_decoder` 字段
  - [x] SubTask 1.2: 在 `SearchComboBox::AddDataItem()` 中新增 `is_c_decoder` 参数并存储
  - [x] SubTask 1.3: 将搜索图标按钮替换为 QComboBox 下拉框（All / C / Python），点击时弹出搜索面板
  - [x] SubTask 1.4: 在 `SearchComboBox::on_keyword_changed()` 中结合类型过滤条件进行显示/隐藏判断
  - [x] SubTask 1.5: 在 `ProtocolDock::show_protocol_select()` 中，通过 `dec->is_c_decoder` 传递解码器类型信息，并传入当前下拉框选择

- [x] Task 2: 毛刺滤波方向性算法
  - [x] SubTask 2.1: 在 `logicsnapshot.h` 中定义 `GlitchFilterMode` 枚举（BOTH/HIGH/LOW），修改 `apply_glitch_filter` 签名新增 `filter_mode` 参数
  - [x] SubTask 2.2: 在 `logicsnapshot.cpp` 的 `apply_glitch_filter` 中实现方向性滤波逻辑
  - [x] SubTask 2.3: 在 `sigsession.h` 中修改 `set_glitch_filter` 签名，新增每通道方向向量参数
  - [x] SubTask 2.4: 在 `sigsession.cpp` 的 `glitch_filter_task` 中传递方向向量到 `apply_glitch_filter_all`

- [x] Task 3: 毛刺滤波方向 UI
  - [x] SubTask 3.1: 在 `signalprocessingdock.h` 中新增 `_glitch_mode_combo_list` 向量
  - [x] SubTask 3.2: 在 `signalprocessingdock.cpp` 的 `build_glitch_filter_panel()` 中，每通道"采样周期"后新增方向选择 QComboBox（B/H/L）
  - [x] SubTask 3.3: 在 `on_apply_glitch_filter()` 中读取每通道方向选择并传递给 `set_glitch_filter()`
  - [x] SubTask 3.4: 在 `get_session()` / `set_session()` 中按通道保存/恢复滤波方向参数

# Task Dependencies
- Task 2 必须在 Task 3 之前完成（算法先于 UI）
- Task 1 和 Task 2/3 无依赖，可并行
