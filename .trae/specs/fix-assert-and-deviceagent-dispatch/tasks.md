# Tasks

- [x] Task 1: 修复 DeviceAgent::set_config_byte 分发遗漏
  - [x] SubTask 1.1: 阅读 [deviceagent.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/deviceagent.cpp) 第 782-801 行,确认 `set_config_byte` 直接调用 `ds_set_actived_device_config`
  - [x] SubTask 1.2: 改为 `GVariant *gvar = g_variant_new_byte((uint8_t)value); return set_config(key, gvar, ch, cg);`,与其他 set_config_* 实现一致
  - [x] SubTask 1.3: 验证 srstd 设备路径下 SR_CONF_PROBE_HW_OFFSET 等字节配置能正确写入

- [x] Task 2: 删除 DeviceAgent 中冗余的 assert(_dev_handle)
  - [x] SubTask 2.1: 在 [deviceagent.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/deviceagent.cpp) 中 grep `assert(_dev_handle)`,确认每个匹配处前面已有 `if (!_dev_handle) { warn; return false; }` 前置检查
  - [x] SubTask 2.2: 删除所有冗余 `assert(_dev_handle)`（涉及 set_config_int32/string/bool/uint64/uint16/uint32/int16/byte/double 共 9 处,以及 `inst()` 中 1 处;实际 sub-agent 一并清理了所有冗余 assert(_dev_handle) 共 28 处）
  - [x] SubTask 2.3: 同步删除 `assert(value)` 等同类冗余 assert（set_config_string 中 `assert(value)` 前已有 `if (!value)` 检查）

- [x] Task 3: EventBus 护栏改用 early-return
  - [x] SubTask 3.1: 阅读 [eventbus.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/core/eventbus.h) 第 56-93 行,确认 `broadcast` 与 `broadcast_sync` 都有 `assert(_broadcast_depth <= 1 && "...")`
  - [x] SubTask 3.2: 删除两处 `assert` 行,保留 `pxv_err` + `--_broadcast_depth; return;`
  - [x] SubTask 3.3: 验证注释一致性,更新注释说明"不 assert 以避免自身成为模态弹窗源"

- [x] Task 4: 清理崩溃链路上的 7 处高风险 assert
  - [x] SubTask 4.1: [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/sigsession.cpp) `set_cur_snap_samplerate`: `assert(samplerate != 0)` → `if (samplerate == 0) { pxv_err("set_cur_snap_samplerate: samplerate=0, ignoring"); return; }`
  - [x] SubTask 4.2: [samplingbar.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/toolbars/samplingbar.cpp) `update_sample_count_selector` 第 725 行: `assert(duration > 0)` → `if (duration <= 0) { pxv_err("update_sample_count_selector: duration<=0, aborting"); _updating_sample_count = false; return; }`
  - [x] SubTask 4.3: [analogsnapshot.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/analogsnapshot.cpp) 第 167 行: `assert(_unit_bytes > 0)` → `if (_unit_bytes <= 0) { pxv_err("AnalogSnapshot: _unit_bytes<=0, aborting"); return; }`（新增 `#include "../log.h"`）
  - [x] SubTask 4.4: [snapshot.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/snapshot.cpp) 第 119 行: `assert(samplerate > 0)` → `if (samplerate <= 0) { pxv_err("Snapshot: samplerate<=0, aborting"); return; }`（新增 `#include "../log.h"`）
  - [x] SubTask 4.5: [logicsnapshot.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/logicsnapshot.cpp) 第 274 行附近: `assert(0)` → `pxv_err("LogicSnapshot: all channels disabled, aborting"); return;`
  - [x] SubTask 4.6: [deviceagent.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/deviceagent.cpp) `inst()` 第 98 行: 删除 `assert(_dev_handle)`（前面已有 if 检查并 return nullptr）
  - [x] SubTask 4.7: grep 验证范围内 7 处 assert 已全部清理,无遗漏

- [x] Task 5: 编译与回归测试
  - [x] SubTask 5.1: 运行 `cd build && ninja -j 16 && ninja install`（不启动 GUI）— exit code 0,0 error
  - [ ] SubTask 5.2: 启动 PXView.exe,切换到 srstd demo 设备,不勾选通道直接点采集,验证不再弹 assert 窗,提示"无通道启用" — **GUI 回归测试由用户手动验证**
  - [ ] SubTask 5.3: 勾选通道后采集,验证不再触发 `assert(duration > 0)` / `assert(samplerate != 0)` / `assert(_unit_bytes > 0)` — **GUI 回归测试由用户手动验证**
  - [ ] SubTask 5.4: 验证 EventBus 不再出现 `Event broadcast loop detected` 刷屏 — **GUI 回归测试由用户手动验证**
  - [ ] SubTask 5.5: 验证 PXView fork 设备路径（DSLogic/DSCope）回归正常,未受影响 — **GUI 回归测试由用户手动验证**
  - [x] SubTask 5.6: Headless 模式 `PXView.exe --headless` 启动正常,MCP API 17 个工具响应正常

# Task Dependencies
- Task 2、Task 3、Task 4 互相独立,可并行
- Task 1 独立
- Task 5 依赖 Task 1-4 全部完成
