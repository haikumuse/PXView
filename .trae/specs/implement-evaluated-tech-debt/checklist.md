# Checklist

## P1: LogicSnapshot glitch filter 拆分
- [x] `logicsnapshot_glitch_filter.h` 创建，含 10 个方法声明 + 3 个成员 + FillRange struct
- [x] `logicsnapshot_glitch_filter.cpp` 创建，含 8 个方法实现（~399 行）
- [x] `logicsnapshot.h` 含 `friend class LogicSnapshotGlitchFilter;` + `unique_ptr<LogicSnapshotGlitchFilter> _glitch_filter`
- [x] `logicsnapshot.h` 移除原 3 个成员（`_glitch_filtered`/`_filtered_ranges_per_channel`/`_empty_filtered_ranges`）
- [x] `logicsnapshot.cpp` 6 个公共方法转为 forwarder（或保留公共 API，转发到 _glitch_filter）
- [x] `logicsnapshot.cpp` 删除 `set_sample_range` 声明 + 实现（orphaned）
- [x] `logicsnapshot.cpp` 删除 `clone_data` 声明 + 实现（orphaned）
- [x] `logicsnapshot.cpp` 行数 = 1524（< 1900）
- [x] `PXView/pv/data/CMakeLists.txt` 含新文件
- [x] ninja install 编译通过 0 error
- [x] grep `set_sample_range\|clone_data` 在 logicsnapshot.{h,cpp} = 0 命中

## P2: pxview-data STATIC lib 提取
- [x] `PXView/pv/data/CMakeLists.txt` 创建，定义 `pxview-data` STATIC target
- [x] 根 `CMakeLists.txt` 含 `add_subdirectory(PXView/pv/data)`
- [x] `pxview-core` 的 `target_link_libraries` 含 `PUBLIC pxview-data`
- [x] `CMake/core_sources.cmake` 移除所有 `PXView/pv/data/` entries（28 个）
- [x] grep `PXView/pv/data/` 在 `CMake/core_sources.cmake` = 0 命中
- [x] ninja install 编译通过 0 error
- [x] 增量编译验证：touch pv/data/*.cpp 后 ninja 仅重编 pxview-data + PXView.exe

## P3: view.h dead slot 清理
- [x] `view.h` 删除 `marker_time_changed` 声明（line 644）
- [x] `view.h` 删除 `show_calibration` 声明（line 648）
- [x] `view.cpp` 删除 `marker_time_changed` 实现
- [x] `view.cpp` 删除 `View::show_calibration` 实现（不删 `ViewDataSync::show_calibration`）
- [x] 6 个 friend class 声明移到 `private:` 段开头
- [x] grep `marker_time_changed\|View::show_calibration` 在 view.{h,cpp} = 0 命中
- [x] ninja install 编译通过 0 error

## 最终验证
- [x] `cd build && ninja -j 16 && ninja install` 0 error
- [x] PXView.exe 生成
- [x] Headless 启动 + MCP API `tools/list` 返回 17 tools
- [x] AGENTS.md 更新（Key Files 加 logicsnapshot_glitch_filter.h，CMake leaf libraries 加 pv/data）
- [x] project_memory.md 追加完成记录
