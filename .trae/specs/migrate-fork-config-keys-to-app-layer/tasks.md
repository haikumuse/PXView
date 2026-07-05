# Tasks

## 阶段 1：删除 DSO 死代码（B + D 类，26 个键）

- [ ] Task 1: 删除 dsvdef.h 中 26 个 DSO/零调用键定义
  - [ ] SubTask 1.1: 删除 25 个 DSO 残留键（PROBE_VDIV/PROBE_COUPLING/TRIGGER_VALUE/MAX_DSO_SAMPLERATE/MAX_DSO_SAMPLELIMITS/MAX_TIMEBASE/MIN_TIMEBASE/TRIGGER_HOLDOFF/TRIGGER_MARGIN/TRIGGER_CHANNEL/CALI/ZERO/HAVE_ZERO/ZERO_SET/ZERO_LOAD/ZERO_COMB/ZERO_COMB_FGAIN/ZERO_DEFAULT/PROBE_COMB_COMP/PROBE_COMB_COMP_EN/PROBE_VGAIN/PROBE_VGAIN_DEFAULT/PROBE_VGAIN_RANGE/PROBE_PREOFF/PROBE_PREOFF_MARGIN）
  - [ ] SubTask 1.2: 删除 `SR_CONF_USB` (60088) 零调用键

- [ ] Task 2: 删除 DSO 专用整体死代码文件
  - [ ] SubTask 2.1: 删除 `PXView/pv/dock/calibration.cpp/.h`（DSO 校准 dock）
  - [ ] SubTask 2.2: 删除 `PXView/pv/dialogs/waitingdialog.cpp/.h`（DSO 零点校准对话框）
  - [ ] SubTask 2.3: 删除 `PXView/pv/dock/dso_hardware_config.cpp/.h`（DSO 硬件配置）
  - [ ] SubTask 2.4: 删除 `PXView/pv/dock/dsotriggerdock.cpp/.h`（DSO 触发 dock）
  - [ ] SubTask 2.5: 从 CMakeLists.txt 移除上述文件引用

- [ ] Task 3: 清理 DSO 键的散布调用点
  - [ ] SubTask 3.1: `signalmodel.cpp:338-351` 删除 commit_settings 中 DSO 键提交路径（PROBE_VDIV/PROBE_COUPLING/PROBE_OFFSET/PROBE_HW_OFFSET/TRIGGER_VALUE/PROBE_EN）—— PROBE_OFFSET/HW_OFFSET 在 Task 5 迁移，这里先删 DSO 部分
  - [ ] SubTask 3.2: `samplingbar.cpp:679-686` 删除 DSO 时基查询（MAX_TIMEBASE/MIN_TIMEBASE）
  - [ ] SubTask 3.3: `samplingbar.cpp:937` 删除 MAX_DSO_SAMPLERATE 查询
  - [ ] SubTask 3.4: `fftoptions.cpp:82` 删除 MAX_DSO_SAMPLELIMITS 查询
  - [ ] SubTask 3.5: `session_service.cpp` 删除 DSO 触发 get/set 路径（TRIGGER_HOLDOFF/MARGIN/CHANNEL）
  - [ ] SubTask 3.6: `storesession.cpp` 删除 DSO 校准/状态导出路径
  - [ ] SubTask 3.7: `deviceoptions.cpp` prop binding 中删除 DSO 相关 switch case（ZERO/HAVE_ZERO/CALI/STATUS/CLOCK_TYPE/BANDWIDTH_LIMIT/BANDWIDTH）

## 阶段 2：迁移 A 类 20 个应用层键

- [ ] Task 4: DeviceAgent 新增 Analog 通道缩放方法
  - [ ] SubTask 4.1: `deviceagent.h` 新增方法声明：`get_probe_offset(ch)`/`get_probe_hw_offset(ch)`/`get_ref_min()`/`get_ref_max()`/`get_probe_map_unit(ch)`/`get_probe_map_min(ch)`/`get_probe_map_max(ch)`/`get_probe_map_default(ch)`
  - [ ] SubTask 4.2: `deviceagent.cpp` 实现这些方法，内部对支持的设备调 sr_config_get，不支持的返回默认值
  - [ ] SubTask 4.3: `analogsignal.cpp` 改用 DeviceAgent 方法替代 `get_config_*(SR_CONF_PROBE_OFFSET/PROBE_HW_OFFSET/REF_MIN/REF_MAX/UNIT_BITS/PROBE_MAP_*)`
  - [ ] SubTask 4.4: 删除 dsvdef.h 中 9 个 Analog 缩放键定义（PROBE_OFFSET/PROBE_HW_OFFSET/REF_MIN/REF_MAX/UNIT_BITS/PROBE_MAP_UNIT/MIN/MAX/PROBE_MAP_DEFAULT）

- [ ] Task 5: 通道使能迁移到上游 sr_channel->enabled 字段
  - [ ] SubTask 5.1: `signalmodel.cpp:338` 删除冗余 `set_config_bool(SR_CONF_PROBE_EN, ...)` 调用（`signalmodel.cpp:322` 已直接设置 `sr_channel->enabled`）
  - [ ] SubTask 5.2: 删除 dsvdef.h 中 `SR_CONF_PROBE_EN` (60049) 定义
  - [ ] SubTask 5.3: 删除 `deviceoptions.cpp` prop binding 中 PROBE_EN case

- [ ] Task 6: 通用设备能力键迁移到 DeviceAgent 方法
  - [ ] SubTask 6.1: `deviceagent.h` 新增方法：`wait_upload()`/`get_actual_samples()`/`get_file_version()`/`get_probe_configs()`/`get_status()`/`get_bandwidth_limit(ch)`/`get_bandwidth(ch)`/`is_rle_supported()`
  - [ ] SubTask 6.2: `deviceagent.cpp` 实现这些方法（内部按设备能力 fallback）
  - [ ] SubTask 6.3: `triggerdock.cpp` 改 `SR_CONF_TOTAL_CH_NUM` (60062) 为 `SR_CONF_VLD_CH_NUM` (60023)（pxlogic 已实现）
  - [ ] SubTask 6.4: `deviceoptions.cpp` prop binding 中 `SR_CONF_CLOCK_TYPE` 改为 `SR_CONF_CLOCK_EDGE`（pxlogic 已实现）
  - [ ] SubTask 6.5: `view_data_sync.cpp` 改 `SR_CONF_ACTUAL_SAMPLES` 为 DeviceAgent 方法
  - [ ] SubTask 6.6: `mainwindow.cpp` 改 `SR_CONF_FILE_VERSION` 为 DeviceAgent 方法
  - [ ] SubTask 6.7: `probeoptions.cpp` 改 `SR_CONF_PROBE_CONFIGS` 为 DeviceAgent 方法
  - [ ] SubTask 6.8: `capturemanager.cpp` 改 `SR_CONF_WAIT_UPLOAD` 为 DeviceAgent 方法
  - [ ] SubTask 6.9: `samplingbar.cpp` 改 `SR_CONF_RLE_SUPPORT` 为 DeviceAgent 方法
  - [ ] SubTask 6.10: 删除 dsvdef.h 中 11 个通用键定义（WAIT_UPLOAD/TOTAL_CH_NUM/ACTUAL_SAMPLES/FILE_VERSION/PROBE_CONFIGS/STATUS/CLOCK_TYPE/BANDWIDTH_LIMIT/BANDWIDTH/RLE_SUPPORT/PROBE_EN）

## 阶段 3：修复 demo 设备使用问题

- [ ] Task 7: demo 采样率离散化
  - [ ] SubTask 7.1: 在 `SamplingBar::update_sample_rate_selector` (samplingbar.cpp:545) 检测 GVariant 返回值格式（离散列表 vs step 格式）
  - [ ] SubTask 7.2: 实现 `generate_1_2_5_steps(min, max)` 工具函数：在 [min, max] 范围按 1-2-5 序列取点（1Hz/2Hz/5Hz/10Hz/.../1GHz）
  - [ ] SubTask 7.3: step 格式时调用该函数生成离散列表，填充 combo box

- [ ] Task 8: limit_samples 应用层 fallback
  - [ ] SubTask 8.1: `AppConfig` 添加 `uint64_t default_sample_limit = SR_MHZ(1)` 字段 + QSettings 持久化
  - [ ] SubTask 8.2: `DeviceAgent::get_sample_limit()` (deviceagent.cpp:357) 在驱动返回 0 时返回 `AppConfig::default_sample_limit`

## 阶段 4：验证

- [ ] Task 9: 编译验证
  - [ ] SubTask 9.1: `cd build && ninja -j 16 && ninja install` 通过

- [ ] Task 10: 运行时验证（demo 设备）
  - [ ] SubTask 10.1: 日志无 "Invalid key 60040-60088" 范围任何键
  - [ ] SubTask 10.2: demo 设备采样率 combo box 有可选值（1Hz ~ 1GHz）
  - [ ] SubTask 10.3: demo 设备能启动采集（不再因 limit_samples=0 阻塞）
  - [ ] SubSub 10.4: demo 设备 8 LOGIC + 5 ANALOG 同屏显示
  - [ ] SubTask 10.5: demo 设备 Analog 通道能正常显示（缩放正确，无崩溃）
  - [ ] SubTask 10.6: demo 设备通道使能切换正常工作

- [ ] Task 11: PXLogic 设备回归验证（若硬件可用）
  - [ ] SubTask 11.1: 采样率 combo box 仍显示 PXLogic 硬件支持的离散列表
  - [ ] SubTask 11.2: OPERATION_MODE/CHANNEL_MODE 仍能正常查询（这些键在 libsigrok.h 中，不在本 spec 范围）
  - [ ] SubTask 11.3: VLD_CH_NUM 查询替代 TOTAL_CH_NUM 后正常工作
  - [ ] SubTask 11.4: CLOCK_EDGE 查询替代 CLOCK_TYPE 后正常工作

# Task Dependencies

- 阶段 1（Task 1-3）可并行执行，互不依赖
- 阶段 2（Task 4-6）依赖阶段 1 完成（避免编译冲突）
- Task 4/5/6 之间可并行
- 阶段 3（Task 7-8）独立，可与阶段 1/2 并行
- 阶段 4（Task 9-11）依赖所有前置任务完成
