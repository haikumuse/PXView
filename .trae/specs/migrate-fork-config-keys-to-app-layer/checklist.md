# Checklist

## 阶段 1：删除 DSO 死代码

### dsvdef.h 键定义删除
- [ ] 25 个 DSO 残留键已从 dsvdef.h 删除（PROBE_VDIV/PROBE_COUPLING/TRIGGER_VALUE/MAX_DSO_SAMPLERATE/MAX_DSO_SAMPLELIMITS/MAX_TIMEBASE/MIN_TIMEBASE/TRIGGER_HOLDOFF/TRIGGER_MARGIN/TRIGGER_CHANNEL/CALI/ZERO/HAVE_ZERO/ZERO_SET/ZERO_LOAD/ZERO_COMB/ZERO_COMB_FGAIN/ZERO_DEFAULT/PROBE_COMB_COMP/PROBE_COMB_COMP_EN/PROBE_VGAIN/PROBE_VGAIN_DEFAULT/PROBE_VGAIN_RANGE/PROBE_PREOFF/PROBE_PREOFF_MARGIN）
- [ ] `SR_CONF_USB` (60088) 已从 dsvdef.h 删除

### DSO 专用文件删除
- [ ] `PXView/pv/dock/calibration.cpp/.h` 已删除
- [ ] `PXView/pv/dialogs/waitingdialog.cpp/.h` 已删除
- [ ] `PXView/pv/dock/dso_hardware_config.cpp/.h` 已删除
- [ ] `PXView/pv/dock/dsotriggerdock.cpp/.h` 已删除
- [ ] CMakeLists.txt 中上述文件引用已移除

### DSO 调用点清理
- [ ] `signalmodel.cpp:338-351` commit_settings 中 DSO 键提交路径已删除
- [ ] `samplingbar.cpp:679-686` DSO 时基查询已删除
- [ ] `samplingbar.cpp:937` MAX_DSO_SAMPLERATE 查询已删除
- [ ] `fftoptions.cpp:82` MAX_DSO_SAMPLELIMITS 查询已删除
- [ ] `session_service.cpp` DSO 触发 get/set 路径已删除
- [ ] `storesession.cpp` DSO 校准/状态导出路径已删除
- [ ] `deviceoptions.cpp` prop binding 中 DSO 相关 case 已删除

## 阶段 2：A 类应用层键迁移

### DeviceAgent Analog 通道缩放方法
- [ ] `deviceagent.h` 新增方法声明：get_probe_offset/get_probe_hw_offset/get_ref_min/get_ref_max/get_probe_map_unit/get_probe_map_min/get_probe_map_max/get_probe_map_default
- [ ] `deviceagent.cpp` 实现这些方法（支持设备调 sr_config，不支持返回默认值）
- [ ] `analogsignal.cpp` 改用 DeviceAgent 方法
- [ ] dsvdef.h 中 9 个 Analog 缩放键定义已删除

### 通道使能迁移
- [ ] `signalmodel.cpp:338` 冗余 `set_config_bool(SR_CONF_PROBE_EN, ...)` 调用已删除
- [ ] dsvdef.h 中 `SR_CONF_PROBE_EN` (60049) 定义已删除
- [ ] `deviceoptions.cpp` prop binding 中 PROBE_EN case 已删除

### 通用设备能力迁移
- [ ] `deviceagent.h` 新增方法：wait_upload/get_actual_samples/get_file_version/get_probe_configs/get_status/get_bandwidth_limit/get_bandwidth/is_rle_supported
- [ ] `deviceagent.cpp` 实现这些方法
- [ ] `triggerdock.cpp` `SR_CONF_TOTAL_CH_NUM` 改为 `SR_CONF_VLD_CH_NUM`
- [ ] `deviceoptions.cpp` `SR_CONF_CLOCK_TYPE` 改为 `SR_CONF_CLOCK_EDGE`
- [ ] `view_data_sync.cpp` `SR_CONF_ACTUAL_SAMPLES` 改用 DeviceAgent
- [ ] `mainwindow.cpp` `SR_CONF_FILE_VERSION` 改用 DeviceAgent
- [ ] `probeoptions.cpp` `SR_CONF_PROBE_CONFIGS` 改用 DeviceAgent
- [ ] `capturemanager.cpp` `SR_CONF_WAIT_UPLOAD` 改用 DeviceAgent
- [ ] `samplingbar.cpp` `SR_CONF_RLE_SUPPORT` 改用 DeviceAgent
- [ ] dsvdef.h 中 11 个通用键定义已删除

## 阶段 3：demo 设备修复

### demo 采样率离散化
- [ ] `SamplingBar::update_sample_rate_selector` 检测 GVariant 格式（离散 vs step）
- [ ] `generate_1_2_5_steps(min, max)` 工具函数已实现
- [ ] step 格式时生成离散列表，combo box 显示可选采样率

### limit_samples fallback
- [ ] `AppConfig` 添加 `default_sample_limit` 字段（默认 SR_MHZ(1)）+ QSettings 持久化
- [ ] `DeviceAgent::get_sample_limit()` 在驱动返回 0 时返回 fallback

## 阶段 4：编译验证
- [ ] `cd build && ninja -j 16 && ninja install` 通过

## 阶段 5：运行时验证（demo 设备）
- [ ] 日志无 "Invalid key 60040" 范围任何键
- [ ] 日志无 "Invalid key 60041"
- [ ] 日志无 "Invalid key 60042"
- [ ] 日志无 "Invalid key 60043"
- [ ] 日志无 "Invalid key 60044"
- [ ] 日志无 "Invalid key 60045"
- [ ] 日志无 "Invalid key 60046"
- [ ] 日志无 "Invalid key 60047"
- [ ] 日志无 "Invalid key 60048"
- [ ] 日志无 "Invalid key 60049"
- [ ] 日志无 "Invalid key 60050"
- [ ] 日志无 "Invalid key 60051"
- [ ] 日志无 "Invalid key 60052"
- [ ] 日志无 "Invalid key 60053"
- [ ] 日志无 "Invalid key 60054"
- [ ] 日志无 "Invalid key 60055"
- [ ] 日志无 "Invalid key 60056"
- [ ] 日志无 "Invalid key 60057"
- [ ] 日志无 "Invalid key 60058"
- [ ] 日志无 "Invalid key 60059"
- [ ] 日志无 "Invalid key 60060"
- [ ] 日志无 "Invalid key 60061"
- [ ] 日志无 "Invalid key 60062"
- [ ] 日志无 "Invalid key 60063"
- [ ] 日志无 "Invalid key 60064"
- [ ] 日志无 "Invalid key 60065"
- [ ] 日志无 "Invalid key 60066"
- [ ] 日志无 "Invalid key 60067"
- [ ] 日志无 "Invalid key 60068-60079"
- [ ] 日志无 "Invalid key 60080-60084"
- [ ] 日志无 "Invalid key 60088"
- [ ] demo 设备采样率 combo box 有可选值
- [ ] demo 设备能启动采集
- [ ] demo 设备 8 LOGIC + 5 ANALOG 同屏显示
- [ ] demo 设备 Analog 通道能正常显示（缩放正确）
- [ ] demo 设备通道使能切换正常

## 阶段 6：PXLogic 设备回归（若可用）
- [ ] 采样率 combo box 仍显示 PXLogic 硬件支持的离散列表
- [ ] OPERATION_MODE/CHANNEL_MODE 仍能正常查询
- [ ] VLD_CH_NUM 替代 TOTAL_CH_NUM 后正常工作
- [ ] CLOCK_EDGE 替代 CLOCK_TYPE 后正常工作
