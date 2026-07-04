# Tasks

## Phase 1: P1 — LogicSnapshot glitch filter 拆分

- [x] Task 1: 创建 LogicSnapshotGlitchFilter 类
  - [x] SubTask 1.1: 创建 `PXView/pv/data/logicsnapshot_glitch_filter.h`，定义 `class LogicSnapshotGlitchFilter`，声明 10 个方法（apply_glitch_filter/apply_glitch_filter_all/is_glitch_filtered/set_glitch_filtered/get_filtered_ranges/clear_filtered_ranges/invert_channel/recalc_mipmap + 持有 LogicSnapshot* 反向指针）
  - [x] SubTask 1.2: 移动 3 个成员变量到新类：`_glitch_filtered` (bool)、`_filtered_ranges_per_channel` (map)、`_empty_filtered_ranges` (static const)
  - [x] SubTask 1.3: FillRange struct 保留在 LogicSnapshot.h（外部调用方 view/header.cpp、view/logicsignal.cpp 使用 LogicSnapshot::FillRange，不迁移）
  - [x] SubTask 1.4: 创建 `PXView/pv/data/logicsnapshot_glitch_filter.cpp`，移动 8 个方法实现（apply_glitch_filter ~274行 + apply_glitch_filter_all + is_glitch_filtered + set_glitch_filtered + get_filtered_ranges + clear_filtered_ranges + invert_channel + recalc_mipmap）
  - [x] SubTask 1.5: 在 logicsnapshot.h 加 `friend class LogicSnapshotGlitchFilter;` 声明（pv::data 命名空间内）
  - [x] SubTask 1.6: 在 logicsnapshot.h 持有 `mutable std::unique_ptr<LogicSnapshotGlitchFilter> _glitch_filter;`，移除原 3 个成员
  - [x] SubTask 1.7: 在 logicsnapshot.cpp 构造函数初始化 `_glitch_filter = std::make_unique<LogicSnapshotGlitchFilter>(this)`，析构函数自动 unique_ptr 析构

- [x] Task 2: 转发 LogicSnapshot 公共 glitch filter 方法
  - [x] SubTask 2.1: 在 logicsnapshot.cpp 将 7 个公共方法转为 1-line forwarder：`apply_glitch_filter(...)` → `_glitch_filter->apply_glitch_filter(...)`（progress_callback 用 std::move 转发）
  - [x] SubTask 2.2: 同上 `apply_glitch_filter_all` / `is_glitch_filtered` / `set_glitch_filtered` / `get_filtered_ranges` / `clear_filtered_ranges` / `invert_channel`
  - [x] SubTask 2.3: 在 logicsnapshot.cpp 移除原方法实现（~505 行）

- [x] Task 3: 删除 orphaned 方法
  - [x] SubTask 3.1: 从 logicsnapshot.h 删除 `set_sample_range(uint64_t, uint64_t, bool, int)` 声明
  - [x] SubTask 3.2: 从 logicsnapshot.cpp 删除 `set_sample_range` 实现
  - [x] SubTask 3.3: 从 logicsnapshot.h 删除 `clone_data()` 声明
  - [x] SubTask 3.4: 从 logicsnapshot.cpp 删除 `clone_data` 实现

- [x] Task 4: 更新 CMake
  - [x] SubTask 4.1: 在 `PXView/pv/data/CMakeLists.txt` 加入 `logicsnapshot_glitch_filter.cpp`（pxview-data STATIC lib，非 core_sources.cmake）

- [x] Task 5: 编译验证 P1
  - [x] SubTask 5.1: `cd build && ninja -j 16 && ninja install`（0 error）— EXIT_CODE=0，PXView.exe 生成
  - [x] SubTask 5.2: grep `set_sample_range\|clone_data` 在 `PXView/pv/data/logicsnapshot.{h,cpp}` = 0 命中
  - [x] SubTask 5.3: 统计 `logicsnapshot.cpp` 行数 = 1524（< 1900 ✓），glitch_filter.cpp = 399 行

## Phase 2: P2 — pv/data/ 提取为 STATIC lib

- [x] Task 6: 创建 pxview-data STATIC 库
  - [x] SubTask 6.1: 创建 `PXView/pv/data/CMakeLists.txt`，定义 `add_library(pxview-data STATIC ...)` 含 27 个 .cpp + 1 个 .h（pulse_analyzer.h）+ logicsnapshot_glitch_filter.cpp
  - [x] SubTask 6.2: `target_link_libraries(pxview-data PUBLIC Qt6::Core)` + PRIVATE 链接 FFTW + glib（libsigrok/libsigrokdecode 是源码编译非 lib target）
  - [x] SubTask 6.3: `target_include_directories(pxview-data PUBLIC ${CMAKE_SOURCE_DIR}/PXView)` 让 .cpp 层依赖（pv/sigsession.h 等）能解析
  - [x] SubTask 6.4: 在根 `CMakeLists.txt` 加 `add_subdirectory(PXView/pv/data)`（在 `add_subdirectory(PXView/pv/utility)` 之后）
  - [x] SubTask 6.5: 在 `pxview-core` 的 `target_link_libraries` 加 `PUBLIC pxview-data`
  - [x] SubTask 6.6: 从 `CMake/core_sources.cmake` 移除 28 个 `PXView/pv/data/` entries（lines 30-57）

- [x] Task 7: 编译验证 P2
  - [x] SubTask 7.1: `cd build && ninja -j 16 && ninja install`（0 error）
  - [x] SubTask 7.2: grep `PXView/pv/data/` 在 `CMake/core_sources.cmake` = 0 命中
  - [x] SubTask 7.3: 增量编译验证：touch `pv/data/signalmodel.cpp`，`ninja -j 16` 仅重编 pxview-data + PXView.exe

## Phase 3: P3 — view.h dead slot 清理

- [x] Task 8: 删除 2 个 dead private slots
  - [x] SubTask 8.1: 从 `PXView/pv/view/view.h` line 644 删除 `void marker_time_changed();` 声明
  - [x] SubTask 8.2: 从 `PXView/pv/view/view.cpp` 删除 `marker_time_changed` 实现（~line 496）
  - [x] SubTask 8.3: 从 `PXView/pv/view/view.h` line 648 删除 `void show_calibration();` 声明
  - [x] SubTask 8.4: 从 `PXView/pv/view/view.cpp` 删除 `View::show_calibration` 实现（~line 558，注意不要删除 `ViewDataSync::show_calibration`）

- [x] Task 9: 整理 friend 声明位置
  - [x] SubTask 9.1: 将 6 个 friend class 声明（lines 691-698）从 `private:` 中间移到 `private:` 段开头（friend 不受 access 影响，纯 cosmetic）

- [x] Task 10: 编译验证 P3
  - [x] SubTask 10.1: `cd build && ninja -j 16 && ninja install`（0 error）
  - [x] SubTask 10.2: grep `marker_time_changed\|View::show_calibration` 在 `PXView/pv/view/view.{h,cpp}` = 0 命中

## Phase 4: 最终验证

- [x] Task 11: 全量回归验证
  - [x] SubTask 11.1: `cd build && ninja -j 16 && ninja install`（0 error）
  - [x] SubTask 11.2: Headless 启动验证：`PXView.exe --headless`，MCP API `tools/list` 返回 17 tools
  - [x] SubTask 11.3: grep 验证全部 0 命中：
    - `set_sample_range\|clone_data` 在 `PXView/pv/data/logicsnapshot.{h,cpp}`
    - `PXView/pv/data/` 在 `CMake/core_sources.cmake`
    - `marker_time_changed\|View::show_calibration` 在 `PXView/pv/view/view.{h,cpp}`

- [x] Task 12: 文档更新
  - [x] SubTask 12.1: 更新 AGENTS.md Key Files 表，加 `logicsnapshot_glitch_filter.h` 行
  - [x] SubTask 12.2: 更新 project_memory.md 追加完成记录

# Task Dependencies

- Task 1-4 顺序执行（P1 内部依赖）
- Task 5 依赖 Task 1-4
- Task 6 独立于 P1（CMake 改动），可与 Task 1-4 并行
- Task 7 依赖 Task 6
- Task 8-9 独立（P3 view.h 改动），可与 P1/P2 并行
- Task 10 依赖 Task 8-9
- Task 11 依赖 Task 5 + 7 + 10
- Task 12 依赖 Task 11
