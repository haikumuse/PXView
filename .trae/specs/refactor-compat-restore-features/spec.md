# 重构兼容层以最大化还原标准 sigrok 驱动功能 Spec

## Why

当前兼容层(`libsigrok/hardware/compat/`)虽已成功迁移 81 个标准 sigrok 驱动,但存在三处主动放弃功能的 stub,导致兼容驱动(尤其 sipeed-slogic-analyzer)在 PXView 中功能严重缩水(约 70% 还原度):

1. **软件触发完全失效**:`sr_session_trigger_get()` 是 stub 返回 NULL,`soft_trigger_logic_check()` 是 stub;`add-sigrok-driver-compat-layer/tasks.md:64` 明确记录"禁用触发器代码"决策。slogic 驱动 `protocol.c:470` 的 `if ((trigger = sr_session_trigger_get(...)))` 永远为 NULL,`devc->stl` 永不创建,`devc->trigger_fired = TRUE` 直接跳过触发等待——只能立即采集,无法设置边沿/电平/脉冲触发条件。
2. **设备状态查询失效**:`compat_dev_status_get_default` 返回 SR_ERR_NA,PXView 前端无法查询 slogic 实时状态(采集进度/带宽/错误)。
3. **多模式切换失效**:`compat_dev_mode_list_default` 返回 NULL,PXView 无法在 LA/DAQ/OSC 间切换(虽 slogic 是纯 LA,但前端 UI 依赖该回调判断设备能力)。

根因:PXView 触发体系(`ds_trigger_*` 硬件触发 + Core `TriggerConfig`,LA 专用 Simple/Adv/Serial)与标准 sigrok 触发体系(`sr_trigger/stage/match` + `soft_trigger_logic`,纯软件)完全不兼容,前次 spec 选择 stub 跳过。本次通过引入**触发桥接层**让两者互通,实现兼容驱动的软件触发,最大化还原 slogic 等设备的功能至约 95%。

## What Changes

### A. 触发桥接层(P0 核心)

- 在 `libsigrok/hardware/compat/compat_trigger.h/.c` 新增触发桥接模块
- 实现 `sr_session_trigger_get()` 真实版本:从 PXView Core `TriggerConfig`(经 `ds_get_actived_device_info` 或新增 accessor 获取)转换出标准 `struct sr_trigger`(single stage + per-channel `SR_TRIGGER_ZERO/ONE/RISING/FALLING/EDGE` matches)
- 实现 `soft_trigger_logic_check()` 真实算法:从上游 libsigrok 0.2.0 `soft_trigger.c` 移植边沿/电平/边沿匹配逻辑(单 stage)
- `soft_trigger_logic_new/free/reset` 补全为真实实现(当前已有结构体定义,仅 check 是 stub)
- **BREAKING**: 删除 `compat_helpers.h:399-401` 的 "trigger path is dead at runtime" 注释及相关 stub 行为;`sr_session_trigger_get` 不再返回 NULL

### B. 兼容驱动状态查询桥接(P1)

- `compat_dev_status_get_default` 改为从 `sdi->priv`(dev_context)推导状态:采集运行中返回 `SR_ST_ACTIVE` + `sr_status` 填充已采集字节数/总字节数;空闲返回 `SR_ST_INACTIVE`
- 仅对暴露 `devc->samples_got_nbytes` / `devc->samples_need_nbytes` 字段的驱动生效(slogic 已有);其他兼容驱动保持 SR_ERR_NA
- 通过 `dev_status_get` 回调返回的 `sr_status` 结构,PXView 前端 `DeviceAgent` 已能消费

### C. 多模式切换降级(P1)

- `compat_dev_mode_list_default` 改为返回单元素 GSList,内容为该设备当前模式(从 `devc->cur_samplechannel` 或 `drvopts[0]` 推导:含 `SR_CONF_LOGIC_ANALYZER` 返回 LA 模式)
- PXView 前端 `ds_get_actived_device_mode_list` 对单元素列表降级为"禁用模式切换 UI"

### D. 触发同步路径分流(P0 必需)

- `SigSession::sync_trigger_to_libsigrok()`(单点同步)增加设备类型判断:DSL 设备走 `ds_trigger_*` 硬件触发同步(现状不变);兼容驱动设备**跳过** `ds_trigger_*` 调用(因为兼容驱动用软件触发,TriggerConfig 经 `sr_session_trigger_get` 桥接传递)
- 兼容驱动设备的 `TriggerConfig::Simple` 仍由 `TriggerDock::commit_trigger` 写入 Core,但运行时由兼容层在 `dev_acquisition_start` 前读取并转换

### E. SR_PRIV Windows 根本问题(暂不做,记为 P2)

本次 spec **不**修复 `SR_PRIV` 在 Windows 为空的根本问题(影响所有 static 函数变全局符号)。当前已通过集中化共享符号(`std_session_send_df_frame_begin` 等)缓解已知冲突;新冲突出现时再做根修(改 `libsigrok.h:169` 为 `__declspec(dllexport)` 或强制 `static`)。**记入 spec 仅作为已知风险跟踪**。

## Impact

- **Affected specs**:
  - `add-sigrok-driver-compat-layer`(本次推翻其"禁用触发器代码"决策,新增桥接层)
  - `restructure-compat-and-migrate-slogic`(slogic 触发路径从 dead 变为 active,无源码改动,依赖 A 部分桥接)
  - `harden-crash-points-batch2`(其 `commit_trig 走 Core` 决策不变,但 `sync_trigger_to_libsigrok` 新增设备类型分流)
- **Affected code**:
  - `libsigrok/hardware/compat/compat_trigger.h`(新增)
  - `libsigrok/hardware/compat/compat_trigger.c`(新增)
  - `libsigrok/hardware/compat/compat_helpers.h`(删除 trigger stub,改 include compat_trigger.h)
  - `libsigrok/hardware/compat/compat_helpers.c`(删除 trigger stub 实现)
  - `libsigrok/hardware/compat/compat_driver.h`(`compat_dev_status_get_default`/`compat_dev_mode_list_default` 改为非空实现)
  `libsigrok/hardware/compat/CMakeLists.txt` 或 `CMake/libsigrok.cmake`(新增 compat_trigger.c 源)
  - `PXView/pv/sigsession.cpp` 或 `core/capturemanager.cpp`(`sync_trigger_to_libsigrok` 增加设备类型分流)
  - `PXView/pv/deviceagent.cpp`(可能需要暴露 `is_compat_driver()` accessor 给 Core)
- **Out of scope**:
  - DSL 原生驱动触发路径不变(`ds_trigger_*` 硬件触发)
  - 兼容驱动的 SCPI/Serial 通信后端不变
  - 标准 sigrok 多 stage 触发(advanced trigger)不支持(本次仅 single stage)
  - PXView `TriggerConfig::Adv/Serial` 模式不桥接到兼容驱动(仅 `Simple` 模式桥接,符合 slogic 等 LA 设备)

## ADDED Requirements

### Requirement: 触发桥接层

系统 SHALL 在 `libsigrok/hardware/compat/compat_trigger.h/.c` 提供触发桥接层,将 PXView Core `TriggerConfig`(Simple 模式)转换为标准 sigrok `struct sr_trigger`,并实现 `soft_trigger_logic_check` 真实算法。

#### Scenario: slogic 软件触发激活

- **WHEN** 用户在 PXView 中为 slogic 设备配置 Simple 触发(如 D0 通道上升沿),设置预触发比例 10%,启动采集
- **THEN** 兼容层 SHALL 通过 `sr_session_trigger_get()` 返回非 NULL 的 `sr_trigger*`,包含 D0 通道的 `SR_TRIGGER_RISING` match
- **AND** `soft_trigger_logic_new()` 创建 `stl`,`devc->trigger_fired` 初始为 FALSE
- **AND** 采集数据流入 `soft_trigger_logic_check()`,检测到 D0 上升沿后 `trigger_fired = TRUE`,丢弃预触发样本,转发剩余样本至 `ds_data_forward`
- **AND** 最终采集结果以触发点为参考点(预触发 10% + 后触发 90%)

#### Scenario: 无触发配置时立即采集

- **WHEN** 用户未配置任何触发(所有通道 trig_type=0),启动 slogic 采集
- **THEN** `sr_session_trigger_get()` SHALL 返回 NULL(或空 trigger)
- **AND** `devc->stl` 为 NULL,`devc->trigger_fired = TRUE`,立即转发所有采集数据(行为与现状等价)

#### Scenario: 触发匹配类型映射

- **WHEN** PXView `TriggerConfig::Simple` 的 per-channel `trig_type` 取值为 0/1/R/F/X
- **THEN** 桥接层 SHALL 映射:0→`SR_TRIGGER_ZERO`,1→`SR_TRIGGER_ONE`,R→`SR_TRIGGER_RISING`,F→`SR_TRIGGER_FALLING`,X→`SR_TRIGGER_EDGE`
- **AND** `trig_type=0`(无触发)的通道 SHALL NOT 加入 `sr_trigger_stage.matches`

### Requirement: 兼容驱动触发同步分流

系统 SHALL 在 `SigSession::sync_trigger_to_libsigrok()` 增加设备类型判断,对兼容驱动设备跳过 `ds_trigger_*` 硬件触发同步。

#### Scenario: DSL 设备触发同步不变

- **WHEN** 当前活动设备是 DSL 原生驱动(如 dslogic/dscope/dsl)
- **THEN** `sync_trigger_to_libsigrok()` SHALL 调用 `ds_trigger_probe_set`/`ds_trigger_set_en`/`ds_trigger_set_pos` 等(现状不变)

#### Scenario: 兼容驱动设备跳过硬件触发同步

- **WHEN** 当前活动设备是兼容驱动(如 sipeed-slogic-analyzer)
- **THEN** `sync_trigger_to_libsigrok()` SHALL 跳过所有 `ds_trigger_*` 调用
- **AND** SHALL 仅保证 Core `TriggerConfig` 已就绪(由 `TriggerDock::commit_trigger` 写入)
- **AND** 触发配置在兼容驱动 `dev_acquisition_start` 内经 `sr_session_trigger_get()` 桥接读取

### Requirement: 兼容驱动状态查询

系统 SHALL 让 `compat_dev_status_get_default` 从 `sdi->priv` 推导设备状态,返回非 SR_ERR_NA 结果(对暴露采集进度字段的驱动)。

#### Scenario: slogic 采集进度查询

- **WHEN** slogic 采集运行中,PXView 前端调用 `ds_get_actived_device_status()`
- **THEN** SHALL 返回 SR_OK,`sr_status` 结构包含已采集字节数/总字节数(从 `devc->samples_got_nbytes`/`devc->samples_need_nbytes` 填充)
- **AND** 设备状态字段标记为 `SR_ST_ACTIVE`

#### Scenario: 无采集进度字段的兼容驱动

- **WHEN** 兼容驱动 `dev_context` 未暴露 `samples_got_nbytes`/`samples_need_nbytes` 字段(如 scpi-dmm)
- **THEN** `compat_dev_status_get_default` SHALL 返回 SR_ERR_NA(行为与现状等价)

### Requirement: 兼容驱动多模式降级

系统 SHALL 让 `compat_dev_mode_list_default` 返回单元素模式列表,内容为该设备当前模式。

#### Scenario: slogic 模式列表查询

- **WHEN** PXView 前端调用 `ds_get_actived_device_mode_list()` 查询 slogic 设备
- **THEN** SHALL 返回单元素 GSList,模式为 LA(因 slogic drvopts 含 `SR_CONF_LOGIC_ANALYZER`)
- **AND** PXView 前端检测到单元素列表 SHALL 禁用模式切换 UI

#### Scenario: 多模式兼容驱动(未来扩展)

- **WHEN** 兼容驱动 drvopts 同时含 `SR_CONF_LOGIC_ANALYZER` 和 `SR_CONF_OSCILLOSCOPE`(如 hantek-6xxx)
- **THEN** `compat_dev_mode_list_default` SHALL 返回两元素 GSList
- **AND** PXView 前端 SHALL 允许模式切换(通过 `ds_set_actived_device_mode`)

## MODIFIED Requirements

### Requirement: 兼容层 stub 实现策略

原 `add-sigrok-driver-compat-layer` spec 中"禁用触发器代码(PXView 无 struct sr_trigger 定义,sr_session_trigger_get 返回 NULL)"的决策 SHALL 被推翻。兼容层 SHALL 提供触发桥接层实现真实功能。`struct sr_trigger`/`sr_trigger_stage`/`sr_trigger_match`/`struct soft_trigger_logic` 已在 `compat_helpers.h` 定义,本次仅补全 `sr_session_trigger_get` 和 `soft_trigger_logic_check` 为真实实现。

### Requirement: compat_dev_status_get_default / compat_dev_mode_list_default

原 `compat_driver.h` 中两个默认回调返回 SR_ERR_NA / NULL 的 stub 行为 SHALL 改为真实推导(见 ADDED Requirements)。其他默认回调(`compat_dev_destroy_default`)不变。

## REMOVED Requirements

### Requirement: 兼容层触发路径 dead at runtime

**Reason**: 该 stub 导致 slogic 等兼容驱动无法使用软件触发,功能还原度仅 70%。本次通过触发桥接层激活真实路径。
**Migration**: `compat_helpers.h` 删除 "trigger path is dead at runtime" 注释;`sr_session_trigger_get` 和 `soft_trigger_logic_check` 改为真实实现;调用点(slogic `protocol.c:470` 等)无需修改(代码路径自然激活)。
