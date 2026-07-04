# Checklist

## Phase 1: 低风险修复
- [x] （ctest 任务已删除）
- [x] `AGENTS.md:66` 不再引用 `api_type_to_sr_channel_type()`
- [x] 7 处 stale devdoc 引用已更新
- [x] `api/types.h` 含 `ChannelType::Unknown = 99`
- [x] `session_service.cpp::sr_channel_type_to_api` 的 default 分支返回 `Unknown` + pxv_warn
- [x] `signalmodel.cpp::set_type` 含 Debug assert 值域校验
- [x] `sigsession.cpp::add_decoder` 含 `(void)silent;`
- [x] 编译无 unused-parameter 警告（silent 处）

## Phase 2: ui_state bug
- [x] `DockUiState::channel_layouts` 调查结论明确（有/无数据）
- [x] 若有数据：`update_signals` 实现从 `ui_state->channel_layouts` 恢复布局
- [x] 若无数据：`update_signals` 删除 `ui_state` 参数，5 个调用者修正
- [x] 编译无 unused-parameter 警告（ui_state 处）

## Phase 3: header/viewport 迁移
- [x] `header.cpp` 6 处 `session.get_device()` 替换为 `_view.data_source()->device()`
- [x] `viewport_painter.cpp` 3 处替换为局部 `dev->` + null-check
- [x] `viewport.cpp` 2 处替换 + null-check 修复潜在 NPE
- [x] grep `session.get_device()` / `session().get_device()` 在 view/{header,viewport,viewport_painter}.cpp = 0 命中
- [x] ninja install 编译通过

## Phase 4: libsigrok.h 清理
- [x] `deviceagent.h` 含 `is_roll_mode()` / `get_channel_count()` 声明
- [x] `deviceagent.h` 含 8 个 typed `get_*()` 包装 + `get_probe_vdiv_list()` 声明
- [x] `deviceagent.cpp` 实现上述方法
- [x] `dso_measure.cpp` 用 `is_roll_mode()` / `get_channel_count()` / `1000000000ULL`，保留 include + 注释说明
- [x] `dso_hardware_config.h` 含 `struct sr_channel;` 前向声明
- [x] `dso_hardware_config.cpp` 用 typed 包装 + `get_probe_vdiv_list()`
- [x] `dso_hardware_config.cpp` 移除 `#include <libsigrok.h>`
- [x] ninja install 编译通过

## Phase 5: LogicSnapshot 拆分
- [x] `logicsnapshot_diskcache_writer.h` / `.cpp` 创建
- [x] `logicsnapshot.h` 持有 `unique_ptr<LogicSnapshotDiskCacheWriter>`，移除 15+ 原成员
- [x] `logicsnapshot.cpp` 从 2498 行降至 < 2000 行
- [x] `logicsnapshot.h` 从 365 行降至 < 300 行
- [x] 5 个孤儿 friend test 声明删除
- [x] `CMake/core_sources.cmake` 含新文件
- [x] ninja install 编译通过

## Phase 6: CMake 模块化
- [x] 4 个孤儿文件 triage 完成（加入源列表或删除）
- [x] `PXView/pv/interface/CMakeLists.txt` 创建，定义 `pxview-interface` INTERFACE target
- [x] `PXView/pv/utility/CMakeLists.txt` 创建，定义 `pxview-utility` STATIC target
- [x] `PXView/pv/config/CMakeLists.txt` 创建，定义 `pxview-config` STATIC target
- [x] 根 `CMakeLists.txt` 含 3 个 `add_subdirectory()` 调用
- [x] `pxview-core` 的 `target_link_libraries` 含 `pxview-interface` / `pxview-utility` / `pxview-config`
- [x] `CMake/core_sources.cmake` 移除已提取模块的 .cpp entries
- [x] ninja install 编译通过

## Phase 7: 最终验证
- [x] `cd build && ninja -j 16 && ninja install` 0 error
- [x] PXView.exe 生成
- [x] Headless 启动 + MCP API `tools/list` 返回 17 tools
- [x] grep 验证全部 0 命中：
  - `session.get_device()` / `session().get_device()` 在 view/{header,viewport,viewport_painter}.cpp
  - `api_type_to_sr_channel_type` 在所有文件
  - `#include <libsigrok.h>` 在 dso_hardware_config.cpp
- [x] 编译无 unused-parameter 警告
- [x] AGENTS.md 更新反映 Phase 1-6 改动
- [x] project_memory.md 追加完成记录
