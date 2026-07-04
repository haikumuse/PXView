# Checklist

## DeviceAgent 分发统一
- [x] `DeviceAgent::set_config_byte` 调用 `set_config(key, gvar, ch, cg)`,不再直接调用 `ds_set_actived_device_config`
- [x] grep `ds_set_actived_device_config` 在 [deviceagent.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/deviceagent.cpp) 中仅出现在 `set_config` 函数体内（一处）,不出现在任何 typed wrapper 中
- [x] grep `ds_get_actived_device_config` 在 deviceagent.cpp 中仅出现在 `get_config` 函数体内（一处）

## DeviceAgent 冗余 assert 清理
- [x] deviceagent.cpp 中 `assert(_dev_handle)` 出现 0 次（所有 set_config_* 与 inst 中的冗余 assert 已删除）
- [x] deviceagent.cpp 中 `assert(value)` 出现 0 次（set_config_string 中的冗余 assert 已删除）
- [x] 每个 set_config_* 函数体仍有 `if (!_dev_handle) { pxv_warn; return false; }` 前置检查保留

## EventBus 护栏
- [x] [eventbus.h](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/core/eventbus.h) 中 `assert(_broadcast_depth <= 1` 出现 0 次
- [x] `broadcast<T>()` 在 `_broadcast_depth > 1` 时仍记录 `pxv_err` 警告
- [x] `broadcast<T>()` 在 `_broadcast_depth > 1` 时执行 `--_broadcast_depth; return;`
- [x] `broadcast_sync<T>()` 同样满足上述两条
- [x] 注释更新,说明"不 assert 以避免自身成为模态弹窗源"

## 崩溃链路 assert 清理
- [x] [sigsession.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/sigsession.cpp) `set_cur_snap_samplerate` 中无 `assert(samplerate != 0)`,改为 `if (samplerate == 0) { pxv_err; return; }`
- [x] [samplingbar.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/toolbars/samplingbar.cpp) `update_sample_count_selector` 中无 `assert(duration > 0)`,改为 early-return 且恢复 `_updating_sample_count = false`
- [x] [analogsnapshot.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/analogsnapshot.cpp) 第 167 行附近无 `assert(_unit_bytes > 0)`,改为 early-return
- [x] [snapshot.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/snapshot.cpp) 第 119 行附近无 `assert(samplerate > 0)`,改为 early-return
- [x] [logicsnapshot.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/data/logicsnapshot.cpp) "all channels disabled" 路径无 `assert(0)`,改为 `pxv_err + return`
- [x] [deviceagent.cpp](file:///c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/PXView/pv/deviceagent.cpp) `inst()` 中无 `assert(_dev_handle)`

## 编译与回归
- [x] `cd build && ninja -j 16 && ninja install` 编译成功,0 error
- [ ] PXView.exe 启动正常 — **GUI 由用户手动验证**
- [ ] srstd demo 设备:不勾选通道采集,弹"无通道启用"提示,不弹 assert 窗 — **GUI 由用户手动验证**
- [ ] srstd demo 设备:勾选通道采集,正常采集,无 assert 弹窗,无 EventBus 刷屏 — **GUI 由用户手动验证**
- [ ] PXView fork 设备（DSLogic/DSCope）回归正常,采集/配置无回归 — **GUI 由用户手动验证**
- [x] Headless 模式 `PXView.exe --headless` 启动正常,MCP API 17 个工具响应正常

## 范围边界
- [x] 未清理范围外的 assert（438 处中仅清理本 spec 涉及的 7 处高风险 assert + DeviceAgent 中 28 处冗余 assert）
- [x] 未引入 `Result<T>` 替换 DeviceAgent typed wrapper 返回值
- [x] 未引入 set_config_* 分支覆盖静态检查脚本
- [x] 未修改 libsigrokstd 上游代码（保留 srstd_bridge.c 已有的 unit_bits 修复）
- [x] 未回滚已删除的 `get_sample_rate`/`get_sample_limit` fallback 兜底值
