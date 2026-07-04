# 修复 assert 弹窗连锁与 DeviceAgent 分发遗漏 Spec

## Why

切换到 libsigrokstd (srstd) demo 设备采集时,触发一系列 Windows 模态断言弹窗,弹窗的消息泵重入把 EventBus 的 `_broadcast_depth` 护栏打穿,造成 `Event broadcast loop detected (depth=2~8)` 刷屏 + 程序卡死。

调试文档（`devdoc/Debugging DSView Event Loop.md`）显示根因有两层：

1. **直接根因**：`DeviceAgent::set_config_byte` 仍然直接调用 `ds_set_actived_device_config`,绕过了 `LIB_SRSTD` 分支；其他 set_config_* 在上一轮已统一走 `set_config(key, gvar, ch, cg)`。srstd 设备的 byte 类型配置（如 SR_CONF_PROBE_HW_OFFSET）设置失败,返回值传递到下游 `assert(samplerate != 0)` / `assert(duration > 0)` / `assert(_unit_bytes > 0)` 等检查点,触发模态弹窗。
2. **架构根因**：`project_memory.md` 已记录"assert 在 Release 应为 no-op,所有指针检查必须前置显式 if(!ptr) { log + return/throw; }"这条规则,但执行不到位。EventBus 的 `assert(_broadcast_depth <= 1)` 也是 assert,自身就是模态弹窗的来源之一。一旦任何 assert 失败,Windows C++ Runtime 弹出模态对话框,该对话框强制推进 qApp 消息循环,把排队的 `broadcast_async<DataUpdated>` 强行派发,depth 直接被打穿,形成"assert 弹窗 → 消息泵重入 → EventBus 嵌套 → 又一个 assert 弹窗"的死循环。

## What Changes

### 修复 1: DeviceAgent::set_config_byte 统一分发（根因修复）
- [deviceagent.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/deviceagent.cpp) 第 782-801 行:`set_config_byte` 改为调用 `set_config(key, gvar, ch, cg)`,与其他 set_config_* 实现一致;删除直接 `ds_set_actived_device_config` 调用。

### 修复 2: 删除 DeviceAgent 中冗余的 assert(_dev_handle)（细节修复）
- 每个 `set_config_*` 函数已有 `if (!_dev_handle) { warn; return false; }` 前置检查,后续的 `assert(_dev_handle)` 是冗余的,且在 Release 下无效,删除以减少噪音。涉及 `set_config_int32/string/bool/uint64/uint16/uint32/int16/byte/double` 共 9 处。

### 修复 3: EventBus 护栏改用 early-return,不再 assert（架构根因修复）
- [eventbus.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/core/eventbus.h) 第 56-69 行（broadcast）、80-93 行（broadcast_sync）:删除 `assert(_broadcast_depth <= 1 && "Event broadcast loop detected")`,保留 `pxv_err` 警告 + `--_broadcast_depth; return;` 兜底。
- **理由**：EventBus 作为崩溃防线,自身不能成为模态弹窗的触发源。改为 early-return 后,即便有遗漏的下游 assert 触发模态弹窗,EventBus 也不会再被 depth 护栏自身的 assert 拉入连锁。

### 修复 4: 清理崩溃链路上的高风险 assert（架构根因修复）
聚焦于本次崩溃链涉及的 assert,按 project_memory.md 规则改为 `if(!cond) { pxv_err; return/throw; }`。**不追求清理全部 438 处 assert**,只清理直接相关的：

- [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/sigsession.cpp) `set_cur_snap_samplerate`: `assert(samplerate != 0)` → `if (samplerate == 0) { pxv_err; return; }`
- [samplingbar.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/toolbars/samplingbar.cpp) `update_sample_count_selector`: `assert(duration > 0)` → `if (duration <= 0) { pxv_err; return; }`（不再用 SR_SEC(1) 兜底值,直接 early-return 让 UI 保持上一次有效值）
- [analogsnapshot.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/analogsnapshot.cpp) 第 167 行: `assert(_unit_bytes > 0)` → `if (_unit_bytes <= 0) { pxv_err; return; }`
- [snapshot.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/snapshot.cpp) 第 119 行: `assert(samplerate > 0)` → `if (samplerate <= 0) { pxv_err; return; }`
- [logicsnapshot.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/logicsnapshot.cpp) 第 274 行附近 `assert(0)` "all channels disabled" → 已有 if 检查,把 `assert(0)` 改为 `pxv_err + return`
- [deviceagent.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/deviceagent.cpp) `inst()`: 已有 if 检查,删除 `assert(_dev_handle)`

### 不做的事（明确范围边界）
- 不清理全部 438 处 assert。其他 assert 作为长期债务,按主题逐步清理。
- 不引入 `Result<T>` 替换 DeviceAgent typed wrapper 的返回值类型。这是更大的架构改进,本 spec 不涉及。
- 不引入 set_config_* 静态分支覆盖检查脚本。靠人工 review + 编译期警告保证。
- 不修改 libsigrokstd 上游代码（除 `srstd_bridge.c` 已有的 unit_bits 修复保留）。
- 不回滚已删除的 fallback 兜底值（`get_sample_rate`/`get_sample_limit` 不再返回假数据）。

## Impact

- **Affected specs**: 无直接关联 spec。本 spec 是 `modernize-core-layer-radical` 的补丁,修复 typed event bus 引入后未覆盖的模态弹窗重入边界情况。
- **Affected code**:
  - `PXView/pv/deviceagent.cpp`（set_config_byte 修复 + 冗余 assert 清理）
  - `PXView/pv/core/eventbus.h`（broadcast/broadcast_sync 移除 assert）
  - `PXView/pv/sigsession.cpp`（set_cur_snap_samplerate assert）
  - `PXView/pv/toolbars/samplingbar.cpp`（duration assert）
  - `PXView/pv/data/analogsnapshot.cpp`（_unit_bytes assert）
  - `PXView/pv/data/snapshot.cpp`（samplerate assert）
  - `PXView/pv/data/logicsnapshot.cpp`（all channels disabled assert）
- **风险**：低。所有改动都是把 assert 改为更安全的 early-return,或把遗漏分支补齐。无接口变更,无数据流变更。srstd demo 设备路径回归测试即可验证。

## ADDED Requirements

### Requirement: EventBus 模态弹窗免疫
EventBus 的 `broadcast<T>()` 与 `broadcast_sync<T>()` 在检测到 `_broadcast_depth > 1` 时 SHALL 仅记录 `pxv_err` 警告并 early-return,不得触发 `assert`。这保证 EventBus 自身不会成为模态弹窗的来源,即便下游 listener 触发了 assert 弹窗导致 qApp 消息泵重入,EventBus 也不会被自身的 assert 拉入连锁。

#### Scenario: 下游 assert 弹窗触发消息泵重入
- **WHEN** 某个 listener 的 `on_event` 内部触发 Windows 模态 assert 弹窗
- **AND** 弹窗的消息泵派发了排队的 `broadcast_async<DataUpdated>` 事件
- **AND** 该事件进入 `broadcast<T>()` 导致 `_broadcast_depth` 升到 2
- **THEN** EventBus SHALL 记录 `pxv_err("Event broadcast loop detected (depth=2), suppressing")`
- **AND** SHALL `--_broadcast_depth; return;`
- **AND** SHALL NOT 触发任何 `assert`

### Requirement: DeviceAgent 配置分发统一
所有 `DeviceAgent::set_config_*` 与 `get_config_*` typed wrapper SHALL 通过统一的 `set_config(key, gvar, ch, cg)` / `get_config(key, ch, cg)` 入口分发,由该入口根据 `_device_lib` 选择 `ds_*` 或 `srstd_glue_*` 后端。typed wrapper 不得直接调用 `ds_set_actived_device_config` / `ds_get_actived_device_config`。

#### Scenario: srstd 设备的 byte 类型配置
- **WHEN** 调用 `DeviceAgent::set_config_byte(SR_CONF_PROBE_HW_OFFSET, value, probe, NULL)` 且 `_device_lib == LIB_SRSTD`
- **THEN** 调用链 SHALL 为 `set_config_byte → set_config → srstd_glue_dev_config_set`
- **AND** SHALL NOT 直接调用 `ds_set_actived_device_config`

## MODIFIED Requirements

### Requirement: 指针/不变量检查（project_memory.md 已有规则的执行）
按 `project_memory.md` "assert(ptr) 是 Release 下的空操作,所有指针检查必须前置显式 if(!ptr) { log + return/throw; } 检查" 与 "assert(false) 后必须有 early-return" 规则,本 spec 范围内的高风险 assert SHALL 改为显式 `if(!cond) { pxv_err; return; }` 模式。本 spec 不一次性清理全部 assert,只清理崩溃链涉及的 7 处（sigsession/samplingbar/analogsnapshot/snapshot/logicsnapshot/deviceagent）。

#### Scenario: samplerate 为 0
- **WHEN** `SigSession::set_cur_snap_samplerate(0)` 被调用（因设备配置获取失败）
- **THEN** SHALL 记录 `pxv_err` 警告并 early-return
- **AND** SHALL NOT 触发 `assert(samplerate != 0)`
- **AND** SHALL NOT 继续传播 0 值到下游

#### Scenario: duration 计算为 0
- **WHEN** `SamplingBar::update_sample_count_selector` 计算出 `duration <= 0`
- **THEN** SHALL 记录 `pxv_err` 警告并 early-return
- **AND** SHALL NOT 触发 `assert(duration > 0)`
- **AND** SHALL NOT 用 `SR_SEC(1)` 兜底值掩盖错误

## REMOVED Requirements

### Requirement: assert 作为 Release 下的硬崩溃触发器
**Reason**: Windows 模态断言弹窗会接管 qApp 消息循环,把排队的 async 事件强行派发,造成 EventBus 护栏被打穿。assert 在 Release 下本应是 no-op（project_memory.md 已记录）,但执行不到位。
**Migration**: 范围内 7 处 assert 改为 `if(!cond) { pxv_err; return; }`。范围外的 assert 作为长期债务保留,后续主题化清理。
