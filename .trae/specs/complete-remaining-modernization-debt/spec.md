# 完成剩余现代化债务 Spec

## Why
前序 spec（`modernize-core-layer-radical`、`modernize-view-layer-v3`）已完成事件总线、Core/View 分层、View 现代化等核心架构。但调查报告发现仍有 7 项剩余债务：枚举文档过时、unused-parameter 警告（含 1 处实为 bug）、header/viewport `get_device()` V2 遗留、libsigrok.h 显式 include 残留、LogicSnapshot God class 拆分（Phase 1）、CMake 模块化（Phase 1）。这些债务风险低、收益明确，应一次性清理。

## What Changes
- **清理枚举文档与收紧边界**：更新 AGENTS.md 的过时 "enum pitfall" 警告；更新 7 处 stale devdoc 引用；在 `SignalModel::set_type(int)` 加 Debug assert 验证值域；修正 `SessionService::sr_channel_type_to_api()` 的 `default: return ChannelType::Logic` 静默误映射（改为日志警告 + 新增 `ChannelType::Unknown=99` sentinel）
- **修复 2 处 unused-parameter 警告**：
  - `sigsession.cpp:955` `silent` 参数 → 加 `(void)silent;`（保留 API 兼容）
  - `signalfactory.cpp:221` `ui_state` 参数 → 调查 `DockUiState::channel_layouts` 是否有数据；若有数据则实现承诺的"从 ui_state 恢复持久化布局"功能，若无数据则删除参数 + 修正注释 + 修正 5 个调用者
- **迁移 header/viewport 11 处 `get_device()` 调用到 `DataSource::device()`**：
  - header.cpp（6 处，已有 null-check，1:1 替换）
  - viewport_painter.cpp（3 处，引入局部 `auto *dev = ...` + null-check）
  - viewport.cpp（2 处，加 null-check 修复潜在 NPE）
- **移除 libsigrok.h 显式 include（声明性清理）**：
  - dso_measure.cpp：`SR_GHZ(1)` → 字面量；加 `DeviceAgent::is_roll_mode()` / `get_channel_count()` 包装消除 `SR_CONF_ROLL` / `g_slist_length`；前向声明 `sr_status` 不足以消除 include（值类型真依赖），保留显式 include 但加注释说明
  - dso_hardware_config.cpp：前向声明 `struct sr_channel;`；加 8 个 typed `DeviceAgent::get_*()` 包装消除 `SR_CONF_*`；加 `DeviceAgent::get_probe_vdiv_list()` 移走 GVariant 代码；移除显式 include
- **拆分 LogicSnapshot Phase 1（提取 DiskCacheWriter）**：将 cluster D（~600 行，独立线程/mutex/cv/队列/mmap）提取为 `LogicSnapshotDiskCacheWriter` 类，由 `LogicSnapshot` 持有 `unique_ptr`，方法转发保持向后兼容
- **CMake 模块化 Phase 1**：
  - Triage 4 个孤儿文件（csvexporter.cpp / pxcserializer.cpp / sessionconfigserializer.cpp / signalrebuilder.cpp）—— 加入源列表或删除
  - 提取 `pv/interface/` 为 INTERFACE 库（3 headers only）
  - 提取 `pv/utility/` 为 STATIC 库（3 leaf .cpp）
  - 提取 `pv/config/` 为 STATIC 库（2 leaf .cpp）

## Impact
- **Affected specs**: `modernize-core-layer-radical`（已完成，无影响）、`modernize-view-layer-v3`（header/viewport 迁移补完 V3 范围）、`harden-remaining-crash-risks`（unused-parameter bug 修复）
- **Affected code**:
  - 枚举文档：`AGENTS.md`、`devdoc/*.md`、`PXView/pv/api/session_service.cpp`、`PXView/pv/data/signalmodel.cpp`
  - 警告修复：`PXView/pv/sigsession.cpp`、`PXView/pv/view/signalfactory.{h,cpp}`、5 个调用者
  - View 迁移：`PXView/pv/view/{header,viewport,viewport_painter}.cpp`
  - libsigrok.h 清理：`PXView/pv/view/{dso_measure,dso_hardware_config}.cpp`、`PXView/pv/deviceagent.{h,cpp}`
  - LogicSnapshot 拆分：`PXView/pv/data/logicsnapshot.{h,cpp}`、新增 `PXView/pv/data/logicsnapshot_diskcache_writer.{h,cpp}`
  - CMake 模块化：新增 `PXView/pv/{interface,utility,config}/CMakeLists.txt`、修改 `CMake/core_sources.cmake`、`CMake/gui_sources.cmake`、`CMakeLists.txt`

## ADDED Requirements

### Requirement: 枚举文档与代码一致性
系统 SHALL 在 AGENTS.md、devdoc 中准确描述当前 `SignalModel::_type` 为 `int` 存 `SR_CHANNEL_*` 的状态，不引用已删除的 `api_type_to_sr_channel_type()` 函数。

#### Scenario: 开发者查阅文档不被误导
- **WHEN** 开发者阅读 AGENTS.md 的 channel type 章节
- **THEN** 文档说明 `SignalModel::type()` 返回 `int` 存 `SR_CHANNEL_*`，转换仅存在于 `SessionService` 的 MCP 边界，不推荐新增转换函数

### Requirement: ChannelType::Unknown sentinel 替代静默误映射
系统 SHALL 在 `SessionService::sr_channel_type_to_api()` 中对 `SR_CHANNEL_DECODER/FFT/LISSAJOUS/MATH/GROUP` 返回新的 `ChannelType::Unknown=99` 而非静默映射为 `Logic`，并日志警告。

#### Scenario: 未知 SR 通道类型不再静默误分类
- **WHEN** `sr_channel_type_to_api()` 接收 `SR_CHANNEL_DECODER`（或 FFT/LISSAJOUS/MATH/GROUP）
- **THEN** 返回 `ChannelType::Unknown`，并写入 `pxv_warn` 日志记录原始 SR 值

### Requirement: SignalModel::set_type Debug 值域校验
系统 SHALL 在 `SignalModel::set_type(int)` 的 Debug 构建中 assert 验证传入值属于 `SR_CHANNEL_*` 范围，防止任意 int 静默传播。

#### Scenario: Debug 构建捕获非法通道类型
- **WHEN** Debug 构建中调用 `set_type(42)`（非 SR_CHANNEL_* 值）
- **THEN** assert 失败，开发者立即发现错误

### Requirement: SignalFactory::update_signals 的 ui_state 参数行为明确
系统 SHALL 在 `SignalFactory::update_signals()` 中要么实现 `ui_state->channel_layouts` 恢复持久化布局的功能（若 `DockUiState` 有数据），要么删除参数并修正注释与全部调用者（若无数据或功能从未实现）。

#### Scenario: ui_state 参数不再是无用警告
- **WHEN** 编译 `signalfactory.cpp`
- **THEN** 无 unused-parameter 警告，且参数行为与注释一致

### Requirement: header/viewport 通过 DataSource 访问设备
系统 SHALL 在 `header.cpp`、`viewport.cpp`、`viewport_painter.cpp` 中通过 `_view.data_source()->device()` 访问设备，不直接调用 `_view.session().get_device()`。

#### Scenario: View 层不再绕过 DataSource 访问设备
- **WHEN** grep `session.get_device()` / `session().get_device()` 在 `PXView/pv/view/{header,viewport,viewport_painter}.cpp`
- **THEN** 0 命中

#### Scenario: viewport.cpp / viewport_painter.cpp 加 null-check
- **WHEN** `data_source()->device()` 返回 nullptr
- **THEN** 不发生 NPE，安全跳过设备相关逻辑

### Requirement: dso_hardware_config.cpp 不再显式 include libsigrok.h
系统 SHALL 在 `dso_hardware_config.cpp` 移除 `#include <libsigrok.h>`，通过 `DeviceAgent` typed 包装和前向声明消除依赖。

#### Scenario: dso_hardware_config.cpp 编译不依赖显式 libsigrok.h
- **WHEN** 移除 `#include <libsigrok.h>` 后编译
- **THEN** 编译成功（传递性 include 通过 signalmodel.h/sigsession.h 仍可见，但显式依赖消除）

### Requirement: LogicSnapshotDiskCacheWriter 子组件
系统 SHALL 将 LogicSnapshot 的磁盘缓存/异步写入子系统（cluster D）提取为独立的 `LogicSnapshotDiskCacheWriter` 类，由 `LogicSnapshot` 持有 `unique_ptr`，公共方法转发保持向后兼容。

#### Scenario: 提取后 LogicSnapshot 行数显著降低
- **WHEN** 提取 DiskCacheWriter 后
- **THEN** `logicsnapshot.cpp` 从 2498 行降至 ~1900 行，`logicsnapshot.h` 从 365 行降至 ~280 行，15+ 成员移至新类

#### Scenario: 提取后行为不变
- **WHEN** 磁盘缓存采集 + 回放
- **THEN** 行为与提取前一致（async 写入线程、backpressure、mmap slot 标记、drain-and-join）

### Requirement: CMake 模块化 Phase 1
系统 SHALL 为 `pv/interface/`、`pv/utility/`、`pv/config/` 创建独立 CMakeLists.txt 并提取为独立 target（INTERFACE/STATIC 库），消除 4 个孤儿 .cpp 文件。

#### Scenario: 孤儿文件被 triage
- **WHEN** 检查 `csvexporter.cpp` / `pxcserializer.cpp` / `sessionconfigserializer.cpp` / `signalrebuilder.cpp`
- **THEN** 要么加入源列表（若被引用），要么从磁盘删除（若死代码）

#### Scenario: 子模块 CMake target 存在
- **WHEN** 检查 `PXView/pv/{interface,utility,config}/CMakeLists.txt`
- **THEN** 三个文件均存在，分别定义 `pxview-interface`（INTERFACE）、`pxview-utility`（STATIC）、`pxview-config`（STATIC）target

#### Scenario: 编译通过
- **WHEN** `cd build && ninja -j 16 && ninja install`
- **THEN** 编译成功，PXView.exe 生成，行为不变

## MODIFIED Requirements

### Requirement: AGENTS.md 反映当前架构
AGENTS.md 的 "Enum pitfall" 章节（line 66）将更新为描述 `SignalModel::_type` 为 `int` 存 `SR_CHANNEL_*` 的当前状态，删除对已移除函数 `api_type_to_sr_channel_type()` 的引用。

## REMOVED Requirements

### Requirement: `api_type_to_sr_channel_type()` 函数引用
**Reason**: 函数已在前序 spec 中移除，文档残留引用误导开发者
**Migration**: 所有引用替换为 "`SessionService::sr_channel_type_to_api()` 在 MCP 边界转换" 的描述
