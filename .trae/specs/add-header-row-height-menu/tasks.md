# Tasks

- [x] Task 1: 在三个语言的 dlg.json 中添加翻译字符串
  - [x] 在 `lang/cn/dlg.json` 末尾添加：IDS_DLG_RESET_ROW_HEIGHT（重置行高）、IDS_DLG_RESET_ALL_ROW_HEIGHT（重置所有行高）、IDS_DLG_SET_CHANNEL_HEIGHT（设置通道高度）、IDS_DLG_BATCH_SET_HEIGHT（批量设置）、IDS_DLG_CUSTOM_HEIGHT（自定义...）
  - [x] 在 `lang/en/dlg.json` 末尾添加对应的英文翻译
  - [x] 在 `lang/traditional/dlg.json` 末尾添加对应的繁体翻译

- [x] Task 2: 在 header.h 中声明菜单相关的私有方法和成员
  - [x] 添加 `QMenu* create_height_submenu(bool is_batch)` 私有方法声明
  - [x] 添加菜单 action 的 slot 声明：`on_reset_row_height()`、`on_reset_all_row_height()`、`on_set_channel_height()`、`on_batch_set_height()`

- [x] Task 3: 在 header.cpp 中实现右键菜单逻辑
  - [x] 实现 `create_height_submenu(bool is_batch)` 方法，构建包含预设高度（20/30/40/50/60/80/100px）和"自定义..."选项的子菜单
  - [x] 修改 `contextMenuEvent` 方法：在 LOGIC 模式下，当右键点击信号标签时构建并显示主菜单
  - [x] 实现 `on_reset_row_height()` slot：将当前信号的 `_ownHeight` 设为 -1，调用 `signals_changed(NULL)`
  - [x] 实现 `on_reset_all_row_height()` slot：将所有信号的 `_ownHeight` 设为 -1，调用 `signals_changed(NULL)`
  - [x] 实现预设高度和自定义高度的 slot：设置对应信号的高度值，受 MinSignalHeight/MaxSignalHeight 约束，调用 `signals_changed(NULL)`

# Task Dependencies
- Task 2 依赖 Task 1（翻译字符串 ID 需先确定）
- Task 3 依赖 Task 2（需要头文件中的声明）
