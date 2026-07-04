# Tasks

## Phase 1: 低风险零依赖修复（并行）

- [x] Task 1: （已删除 — 用户决定不做 ctest）

- [x] Task 2: 清理枚举文档与收紧边界
  - [x] SubTask 2.1: 更新 `AGENTS.md:66` 的 "Enum pitfall" 章节为当前状态描述（`SignalModel::_type` 为 `int` 存 `SR_CHANNEL_*`，转换仅在 `SessionService` 边界，`api_type_to_sr_channel_type()` 已移除）
  - [x] SubTask 2.2: 更新 7 处 stale devdoc 引用：`devdoc/项目模块拆解与数据流分析.md`（lines 144, 2468, 2713, 5883）、`devdoc/分析重构后View层问题.md`（line 3622）、`devdoc/项目架构状态同步分析.md`（line 473）、`devdoc/软件架构分析与解耦.md`（line 17481）
  - [x] SubTask 2.3: 在 `PXView/pv/api/types.h` 的 `ChannelType` enum 添加 `Unknown = 99` sentinel
  - [x] SubTask 2.4: 修改 `PXView/pv/api/session_service.cpp:358-369` 的 `sr_channel_type_to_api()`：`default:` 分支改为 `pxv_warn(...)` + `return ChannelType::Unknown;`
  - [x] SubTask 2.5: 在 `PXView/pv/data/signalmodel.cpp:76` 的 `set_type(int)` 加 `assert(type == SR_CHANNEL_LOGIC || type == SR_CHANNEL_DSO || ...)` 值域校验

- [x] Task 3: 修复 sigsession.cpp:955 silent 警告
  - [x] SubTask 3.1: 在 `PXView/pv/sigsession.cpp` 的 `add_decoder` 函数体开头加 `(void)silent;`（保留 API 兼容，不破坏 5 个调用者）

## Phase 2: ui_state bug 调查与修复

- [x] Task 4: 调查并修复 signalfactory.cpp:221 ui_state 参数
  - [x] SubTask 4.1: 调查 `DockUiState` 结构定义（grep `DockUiState` 找到定义文件），检查 `channel_layouts` 成员类型与是否被填充
  - [x] SubTask 4.2: 调查 `signalrebuilder.cpp:255` 调用点：检查调用 `update_signals(...,  &_view->_dock_ui_state)` 时 `channel_layouts` 是否有数据
  - [x] SubTask 4.3: 决策点 —— 若 `channel_layouts` 有数据：实现承诺功能（从 `ui_state->channel_layouts` 恢复持久化布局）；若无数据：执行 SubTask 4.4
  - [x] SubTask 4.4: 若无数据：删除 `update_signals` 的 `ui_state` 参数（`signalfactory.h` + `signalfactory.cpp`），修正注释，修正 5 个调用者（grep `update_signals(` 找全部调用点）
  - [x] SubTask 4.5: 编译验证无 unused-parameter 警告

## Phase 3: header/viewport DataSource 迁移

- [x] Task 5: 迁移 header.cpp 6 处 get_device() 调用
  - [x] SubTask 5.1: 在 `PXView/pv/view/header.cpp:452, 469, 470, 572, 589, 590` 将 `session.get_device()` 替换为 `_view.data_source()->device()`（已有 null-check，1:1 替换）

- [x] Task 6: 迁移 viewport_painter.cpp 3 处 get_device() 调用
  - [x] SubTask 6.1: 在 `PXView/pv/view/viewport_painter.cpp` paintEvent 的 DSO overlay 块（lines 833-913）顶部引入 `auto *dev = _viewport->_view.data_source()->device();`
  - [x] SubTask 6.2: 加 null-check `if (!dev) return;` 或 `if (dev) { ... }` 包裹 overlay 块
  - [x] SubTask 6.3: 替换 lines 841, 844, 901 的 `get_device()` 为 `dev->`

- [x] Task 7: 迁移 viewport.cpp 2 处 get_device() 调用
  - [x] SubTask 7.1: 在 `PXView/pv/view/viewport.cpp:286` 引入 `auto *dev = _view.data_source()->device();` + null-check，替换 `session().get_device()->is_file()`
  - [x] SubTask 7.2: 在 `PXView/pv/view/viewport.cpp:509` 同样处理

- [x] Task 8: 编译并 grep 验证 Phase 3
  - [x] SubTask 8.1: `cd build && ninja -j 16 && ninja install`
  - [x] SubTask 8.2: grep `session.get_device()\|session().get_device()` 在 `PXView/pv/view/{header,viewport,viewport_painter}.cpp` 应 0 命中

## Phase 4: libsigrok.h 显式 include 清理

- [x] Task 9: 清理 dso_measure.cpp（部分清理，保留 sr_status 值类型依赖）
  - [x] SubTask 9.1: 在 `PXView/pv/deviceagent.h` 加 `bool is_roll_mode(bool &roll);` 和 `int get_channel_count();` 声明
  - [x] SubTask 9.2: 在 `PXView/pv/deviceagent.cpp` 实现（`is_roll_mode` 调 `get_config_bool(SR_CONF_ROLL, roll)`；`get_channel_count` 调 `g_slist_length(get_channels())`）
  - [x] SubTask 9.3: 在 `PXView/pv/view/dso_measure.cpp:319` 替换 `get_config_bool(SR_CONF_ROLL, ...)` 为 `is_roll_mode(...)`
  - [x] SubTask 9.4: 在 `PXView/pv/view/dso_measure.cpp:508` 替换 `g_slist_length(device()->get_channels())` 为 `device()->get_channel_count()`
  - [x] SubTask 9.5: 在 `PXView/pv/view/dso_measure.cpp:515` 替换 `SR_GHZ(1)` 为 `1000000000ULL`
  - [x] SubTask 9.6: 保留 `#include <libsigrok.h>` 但加注释说明"sr_status 值类型真依赖，需引入 Core DsoMeasureStatus 镜像结构才能彻底移除（独立 spec）"

- [x] Task 10: 清理 dso_hardware_config.cpp（完全移除显式 include）
  - [x] SubTask 10.1: 在 `PXView/pv/view/dso_hardware_config.h` 加 `struct sr_channel;` 前向声明
  - [x] SubTask 10.2: 在 `PXView/pv/deviceagent.h` 加 8 个 typed 包装声明：`get_unit_bits`、`get_ref_min`、`get_ref_max`、`get_probe_vdiv`、`get_probe_factor`、`get_probe_coupling`、`get_probe_offset`、`get_probe_hw_offset`、`get_trigger_value`（每个接受 `sr_channel*` 或无参，返回 bool + out 参数）
  - [x] SubTask 10.3: 在 `PXView/pv/deviceagent.h` 加 `QVector<uint64_t> get_probe_vdiv_list();` 声明
  - [x] SubTask 10.4: 在 `PXView/pv/deviceagent.cpp` 实现上述 9 个方法（forwarding 到 `get_config_*(SR_CONF_*, ...)`，`get_probe_vdiv_list` 移走 `init_vDial` 的 GVariant 代码）
  - [x] SubTask 10.5: 在 `PXView/pv/view/dso_hardware_config.cpp` 替换 8 处 `get_config_*(SR_CONF_*, ...)` 为 typed 包装
  - [x] SubTask 10.6: 在 `PXView/pv/view/dso_hardware_config.cpp::init_vDial` 替换 GVariant 代码为 `device()->get_probe_vdiv_list()`
  - [x] SubTask 10.7: 移除 `PXView/pv/view/dso_hardware_config.cpp:28` 的 `#include <libsigrok.h>`
  - [x] SubTask 10.8: 编译验证

## Phase 5: LogicSnapshot 拆分 Phase 1（DiskCacheWriter 提取）

- [x] Task 11: 提取 LogicSnapshotDiskCacheWriter
  - [x] SubTask 11.1: 创建 `PXView/pv/data/logicsnapshot_diskcache_writer.h`，定义 `LogicSnapshotDiskCacheWriter` 类，声明方法：`set_disk_cache_config`、`is_disk_cache_active`、`get_disk_write_speed_mbps`、`get_disk_write_queue_depth`、`get_disk_total_blocks_written`、`ensure_all_blocks_hot`、`async_write_worker`、`is_mmap_slot_fresh`、`mark_mmap_slot_written`、`clear_mmap_slot_written`、`clear_mmap_slot_by_abs`、`start`、`drain_and_join`
  - [x] SubTask 11.2: 移动 `_disk_cache_config`、`_mmap_alloc`、`_mmap_slot_written`、`AsyncPayload` struct、`_async_queue`、`_async_mutex`、`_async_cv`、`_async_drain_cv`、`_async_thread`、`_async_running`、`_async_bytes_written`、`_async_write_speed_mbps`、`_async_queue_depth`、`_async_queue_bytes_size`、`ASYNC_HIGH_WATERMARK`、`ASYNC_LOW_WATERMARK` 到新类
  - [x] SubTask 11.3: 创建 `PXView/pv/data/logicsnapshot_diskcache_writer.cpp`，移动方法实现（~600 行），新类持有 `LogicSnapshot*` 反向指针或 `IChunkAllocator` 接口以调用 `allocate_block`/`_mmap_alloc->get_block_data`
  - [x] SubTask 11.4: 在 `logicsnapshot.h` 持有 `std::unique_ptr<LogicSnapshotDiskCacheWriter> _disk_cache_writer;`，移除原成员，公共方法转发到 `_disk_cache_writer->...`
  - [x] SubTask 11.5: 在 `logicsnapshot.cpp` 移除方法实现，转发实现保留
  - [x] SubTask 11.6: 删除 5 个孤儿 friend test 声明（logicsnapshot.h:355-359，无测试源码存在）
  - [x] SubTask 11.7: 在 `CMake/core_sources.cmake` 加入 `logicsnapshot_diskcache_writer.cpp/.h`
  - [x] SubTask 11.8: 编译验证，确认行数：`logicsnapshot.cpp` < 2000，`logicsnapshot.h` < 300

## Phase 6: CMake 模块化 Phase 1

- [x] Task 12: Triage 4 个孤儿文件
  - [x] SubTask 12.1: grep `csvexporter` / `pxcserializer` / `sessionconfigserializer` / `signalrebuilder` 在 `PXView/pv/` 找引用
  - [x] SubTask 12.2: 若被引用 → 加入 `CMake/core_sources.cmake` 或 `gui_sources.cmake`；若死代码 → 从磁盘删除
  - [x] SubTask 12.3: 编译验证

- [x] Task 13: 提取 pv/interface/ 为 INTERFACE 库
  - [x] SubTask 13.1: 创建 `PXView/pv/interface/CMakeLists.txt`，定义 `add_library(pxview-interface INTERFACE)` + `target_include_directories(pxview-interface INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})`
  - [x] SubTask 13.2: 在根 `CMakeLists.txt` 加 `add_subdirectory(PXView/pv/interface)`
  - [x] SubTask 13.3: 在 `pxview-core` 的 `target_link_libraries` 加 `PUBLIC pxview-interface`
  - [x] SubTask 13.4: 从 `CMake/core_sources.cmake` 移除 interface 相关 entries（仅 headers，无 .cpp）
  - [x] SubTask 13.5: 编译验证

- [x] Task 14: 提取 pv/utility/ 为 STATIC 库
  - [x] SubTask 14.1: 创建 `PXView/pv/utility/CMakeLists.txt`，定义 `add_library(pxview-utility STATIC encoding.cpp path.cpp array.cpp)` + `target_link_libraries(pxview-utility PUBLIC Qt6::Core)`
  - [x] SubTask 14.2: 在根 `CMakeLists.txt` 加 `add_subdirectory(PXView/pv/utility)`
  - [x] SubTask 14.3: 在 `pxview-core` 的 `target_link_libraries` 加 `PUBLIC pxview-utility`
  - [x] SubTask 14.4: 从 `CMake/core_sources.cmake` 移除 utility .cpp entries
  - [x] SubTask 14.5: 编译验证

- [x] Task 15: 提取 pv/config/ 为 STATIC 库
  - [x] SubTask 15.1: 创建 `PXView/pv/config/CMakeLists.txt`，定义 `add_library(pxview-config STATIC appconfig.cpp shortcutdefs.cpp)` + `target_link_libraries(pxview-config PUBLIC Qt6::Core nlohmann_json::nlohmann_json)`
  - [x] SubTask 15.2: 在根 `CMakeLists.txt` 加 `add_subdirectory(PXView/pv/config)`
  - [x] SubTask 15.3: 在 `pxview-core` 的 `target_link_libraries` 加 `PUBLIC pxview-config`
  - [x] SubTask 15.4: 从 `CMake/core_sources.cmake` 移除 config .cpp entries
  - [x] SubTask 15.5: 编译验证

## Phase 7: 最终验证

- [x] Task 16: 全量编译与回归
  - [x] SubTask 16.1: `cd build && ninja -j 16 && ninja install`（0 error）
  - [x] SubTask 16.2: Headless 启动验证：`PXView.exe --headless`，MCP API `tools/list` 返回 17 tools
  - [x] SubTask 16.3: grep 验证：
    - `session.get_device()` / `session().get_device()` 在 view/{header,viewport,viewport_painter}.cpp = 0
    - `api_type_to_sr_channel_type` 在所有 .md / .cpp / .h = 0
    - `#include <libsigrok.h>` 在 dso_hardware_config.cpp = 0
    - unused-parameter 警告 = 0

- [x] Task 17: 文档更新
  - [x] SubTask 17.1: 更新 AGENTS.md 反映 Phase 1-6 改动
  - [x] SubTask 17.2: 更新 project_memory.md 追加完成记录

# Task Dependencies

- Task 2, 3 可并行（无依赖）
- Task 4 独立（ui_state 调查）
- Task 5, 6, 7 可并行（独立文件），Task 8 依赖 5+6+7
- Task 9, 10 可并行（独立文件）
- Task 11 独立（LogicSnapshot 拆分）
- Task 12 独立（孤儿文件 triage）
- Task 13, 14, 15 顺序执行（CMake 累积改动）
- Task 16 依赖全部前序任务
- Task 17 依赖 Task 16
