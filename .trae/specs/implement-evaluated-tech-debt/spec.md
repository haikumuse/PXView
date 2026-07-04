# 实施已评估为值得做的技术债务 Spec

## Why

`evaluate-tech-debt-priorities` spec 评估出 3 项值得做的技术债务（P1/P2/P3）。本 spec 实施这 3 项，完成剩余高 ROI 重构。

## What Changes

### P1: LogicSnapshot glitch filter 拆分
- 创建 `PXView/pv/data/logicsnapshot_glitch_filter.h` 和 `.cpp`，定义 `LogicSnapshotGlitchFilter` 类
- 移动 10 个 glitch filter 方法 + 3 个成员变量 + 1 个 nested struct (`FillRange`) 到新类
- 文件作用域 enum `GlitchFilterMode` 保留在 logicsnapshot.h（被 view 层和 core 层共用）
- LogicSnapshot 持有 `mutable std::unique_ptr<LogicSnapshotGlitchFilter> _glitch_filter;`
- LogicSnapshot 声明 `friend class LogicSnapshotGlitchFilter;`（friend back-pointer 模式，与 DiskCacheWriter 一致）
- LogicSnapshot 公共 glitch filter 方法转为 1-line forwarder 到 `_glitch_filter->...`
- **删除 2 个 orphaned 方法**：`set_sample_range`（无外部调用者）、`clone_data`（无外部调用者）
- `recalc_mipmap` 从 private 提升为 LogicSnapshotGlitchFilter 的私有方法
- 在 `CMake/core_sources.cmake` 加入 `logicsnapshot_glitch_filter.cpp`

### P2: pv/data/ 提取为 STATIC lib
- 创建 `PXView/pv/data/CMakeLists.txt`，定义 `add_library(pxview-data STATIC ...)`
- 在根 `CMakeLists.txt` 加 `add_subdirectory(PXView/pv/data)`
- 在 `pxview-core` 的 `target_link_libraries` 加 `PUBLIC pxview-data`
- 从 `CMake/core_sources.cmake` 移除 27 个 `PXView/pv/data/*.cpp` entries + 1 个 .h entry
- `pxview-data` 链接：`Qt6::Core` PUBLIC + `libsigrok::libsigrok` + `libsigrokdecode::libsigrokdecode` + `FFTW3::FFTW3` + `Boost::boost` + `glib::glib` PRIVATE
- `target_include_directories(pxview-data PUBLIC ${CMAKE_SOURCE_DIR}/PXView)` 让 .cpp 层依赖（pv/sigsession.h 等）能解析

### P3: view.h access 段整理（修正范围）
- **修正评估目标**：原"5→3 段合并"受 Qt MOC 约束不可行（signals:/slots: 必须独立段）。实际目标调整为：
  - 删除 2 个 dead private slots：`marker_time_changed`（无 connect，无调用者）、`show_calibration`（无 connect，无调用者）
  - 将 6 个 friend class 声明从 `private:` 中间移到 `private:` 段开头（cosmetic，friend 不受 access 影响）
- `private slots:` 段从 12 个减到 10 个
- **不合并 access 段**（Qt MOC 约束）

## Impact

- Affected specs: `evaluate-tech-debt-priorities`（前序评估）、`complete-remaining-modernization-debt`（前序实施）
- Affected code:
  - P1: `PXView/pv/data/logicsnapshot.h`、`logicsnapshot.cpp`、新文件 `logicsnapshot_glitch_filter.h/.cpp`、`CMake/core_sources.cmake`、调用点 `core/filterprocessor.cpp`、`view/header.cpp`、`view/logicsignal.cpp`（无需改动，forwarder 保持公共 API）
  - P2: `CMake/core_sources.cmake`、根 `CMakeLists.txt`、新文件 `PXView/pv/data/CMakeLists.txt`
  - P3: `PXView/pv/view/view.h`、`view.cpp`（删除 2 个 dead slot 实现）
- Affected docs: `AGENTS.md`（更新 Key Files 表加 logicsnapshot_glitch_filter.h）、`project_memory.md`（追加完成记录）

## ADDED Requirements

### Requirement: LogicSnapshotGlitchFilter 提取
系统 SHALL 将 glitch filter 逻辑从 LogicSnapshot God class 提取为独立类 `LogicSnapshotGlitchFilter`，使用 friend back-pointer 模式访问 LogicSnapshot 私有状态。

#### Scenario: 提取后编译通过
- **WHEN** `cd build && ninja -j 16 && ninja install` 执行
- **THEN** 0 error，PXView.exe 生成

#### Scenario: 提取后 glitch filter 功能不变
- **WHEN** 外部代码调用 `LogicSnapshot::apply_glitch_filter_all(...)` / `clear_filtered_ranges()` / `is_glitch_filtered()` / `get_filtered_ranges(int)` / `invert_channel(int)` / `set_glitch_filtered(bool)`
- **THEN** 调用通过 forwarder 转发到 `LogicSnapshotGlitchFilter`，行为与提取前一致

#### Scenario: 删除 orphaned 方法
- **WHEN** grep `set_sample_range\|clone_data` 在 `PXView/pv/data/logicsnapshot.h` 和 `.cpp`
- **THEN** 0 命中（已删除）

#### Scenario: 行数下降
- **WHEN** 统计 `logicsnapshot.cpp` 行数
- **THEN** 从 2303 行降至 ~1800 行（移动 ~505 行）

### Requirement: pxview-data STATIC lib 提取
系统 SHALL 将 `pv/data/` 目录下的 27 个 .cpp 提取为独立 STATIC 库 `pxview-data`，由 `pxview-core` PUBLIC 链接。

#### Scenario: 提取后编译通过
- **WHEN** `cd build && ninja -j 16 && ninja install` 执行
- **THEN** 0 error，PXView.exe 生成

#### Scenario: core_sources.cmake 减少 entries
- **WHEN** grep `PXView/pv/data/` 在 `CMake/core_sources.cmake`
- **THEN** 0 命中（全部移到 pv/data/CMakeLists.txt）

#### Scenario: data 层修改不触发 core 重编
- **WHEN** 修改 `pv/data/signalmodel.cpp` 后执行 `ninja -j 16`
- **THEN** 仅 `pxview-data` 和 `PXView.exe` 重编，`pxview-core` 不重编

### Requirement: view.h dead slot 清理
系统 SHALL 删除 2 个无 Qt connect、无调用者的 dead private slots。

#### Scenario: 删除后编译通过
- **WHEN** `cd build && ninja -j 16 && ninja install` 执行
- **THEN** 0 error，PXView.exe 生成

#### Scenario: dead slots 已删除
- **WHEN** grep `marker_time_changed\|show_calibration` 在 `PXView/pv/view/view.h` 和 `view.cpp`
- **THEN** 0 命中（show_calibration 的 ViewDataSync::show_calibration 不受影响，是不同类的方法）

## MODIFIED Requirements

### Requirement: LogicSnapshot 职责边界
LogicSnapshot 现持有 `unique_ptr<LogicSnapshotDiskCacheWriter>` 和 `unique_ptr<LogicSnapshotGlitchFilter>` 两个委托，自身仅保留 storage (cluster A) / query (cluster B) / diagnostics (cluster E) / loop mode (cluster F) 职责。

## REMOVED Requirements

### Requirement: LogicSnapshot::set_sample_range
**Reason**: 无外部调用者（orphaned code），apply_glitch_filter 内部使用自己的 apply_batch lambda 而非调用此方法
**Migration**: 直接删除，无调用点需更新

### Requirement: LogicSnapshot::clone_data
**Reason**: 无外部调用者（orphaned code）
**Migration**: 直接删除，无调用点需更新

### Requirement: view.h "5→3 access 段合并" 目标
**Reason**: Qt MOC 强制 signals:/slots: 必须独立段，无法合并
**Migration**: 改为删除 2 个 dead slots + friend 声明位置整理
