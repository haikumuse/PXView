# Checklist — 分层修复 sigrok 驱动 compat 缺口

> 图例：[x] 已验证完成 / [ ] 待验证或待执行

## Layer 1：compat 层补全

- [x] compat_config.h 含 `SR_MQ_TIME`(10100)/`SR_MQ_WIND_SPEED`/.../`SR_MQ_ELECTRIC_CHARGE` 全部缺失值 — compat_config.h:356-378
- [x] compat_config.h 含 `SR_UNIT_REVOLUTIONS_PER_MINUTE`/`SR_UNIT_VOLT_AMPERE`/`SR_UNIT_WATT`/.../`SR_UNIT_AMPERE_HOUR` 全部缺失值 — compat_config.h:380-399
- [x] compat_config.h 含 `SR_MQFLAG_DURATION`(0x20000)/`SR_MQFLAG_AVG`(0x40000)/`SR_MQFLAG_REFERENCE`/`SR_MQFLAG_UNSTABLE`/`SR_MQFLAG_FOUR_WIRE` — compat_config.h:409-421
- [x] compat_config.h 含 `SR_PACKET_INVALID`(-1)/`SR_PACKET_VALID`(0)/`SR_PACKET_NEED_RX`(1) — compat_config.h:439-445
- [x] compat_config.h 含 `SR_CONF_SWAP`(30159) — compat_config.h:154-156
- [x] compat_config.h 含 `SR_CONF_SIGNAL_GENERATOR`/`SR_CONF_ENABLED`/`SR_CONF_OUTPUT_FREQUENCY`/`SR_CONF_DUTY_CYCLE`（kingst-la2016/protocol.h 本地定义已就位）
- [x] compat_config.h 新增常量值不与 PXView `libsigrok.h` 已有枚举冲突（`SR_MQ_TIME` 用保留值 10100 避开 PXView `SR_MQ_HARMONIC_RATIO`=10015）
- [x] compat_config.h 含 `read_u16le_inc`/`read_u8_inc`/`read_u32le_inc`/`write_u16le_inc`/`write_u32le_inc`/`write_u24le_inc`/`write_u40le_inc`/`write_u8_inc`（`#ifndef compat_*_defined` 守卫）— compat_config.h:247-318
- [x] compat_helpers.h 含 `struct sr_resource` + `#define SR_RESOURCE_FIRMWARE 1` — compat_helpers.h:610-634（`#ifndef SR_RESOURCE_STRUCT_DEFINED` 守卫）
- [x] compat_helpers.h 声明 `sr_resource_open`/`sr_resource_read`/`sr_resource_close` — compat_helpers.h:659-666（`#ifndef COMPAT_SR_RESOURCE_DECLARED` 守卫）
- [x] compat_helpers.c 实现 `sr_resource_open/read/close`（用 `DS_RES_PATH` + fopen/fread/fclose）— compat_helpers.c:768/815/837
- [x] compat_helpers.h 含 `#define SR_LOG_SPEW 5` — compat_helpers.h:593-594（`#ifndef SR_LOG_SPEW` 守卫）
- [x] compat_helpers.h 声明 `sr_log_loglevel_get`（返回 int，默认 4）— compat_helpers.h:680；实现 compat_helpers.c:866
- [x] compat_helpers.h 声明 `sr_hexdump_new`/`sr_hexdump_free`（桩实现）— compat_helpers.h:707/714；实现 compat_helpers.c:880/908

## Layer 2：驱动本地 shim

- [x] openbench-logic-sniffer protocol.c 含 `convert_trigger()` 函数
- [x] openbench-logic-sniffer protocol.c 含 `ols_metadata_quirks()` 函数（Shrimp1.0 + DEMON_CORE）
- [x] openbench-logic-sniffer protocol.c `ols_get_metadata` 解析 10 个 token（非 4 个）
- [x] openbench-logic-sniffer protocol.c `ols_receive_data` 含通道组扩展代码
- [x] openbench-logic-sniffer protocol.c `ols_receive_data` 含前触发/后触发分割（`std_session_send_df_trigger`）
- [x] openbench-logic-sniffer protocol.c `ols_prepare_acquisition` 用 `changroup_mask |= (1 << i)` 计算
- [x] openbench-logic-sniffer protocol.h 含 Demon Core 命令宏 + CAPTURE_FLAG_* + DEVICE_FLAG_IS_DEMON_CORE
- [x] openbench-logic-sniffer protocol.h `dev_context` 含 `uint16_t device_flags` 字段
- [x] openbench-logic-sniffer protocol.c + api.c 编译通过
- [x] saleae-logic-pro 含 `sr_resource_load` 本地实现 + `usb_source_remove` 宏，编译通过
- [x] pipistrello-ols 含 `SR_CONF_SWAP` 配置项，编译通过
- [x] lecroy-logicstudio `dev_acquisition_start` 调用 `lls_setup_acquisition`，编译通过
- [x] asix-sigma 含 `sigma_fw_2_bitbang` 实现（非桩），编译通过
- [x] asix-sigma 含 `sr_sw_limits` 本地副本（appa-55ii 模板）
- [x] asix-sigma 无 `read_u16le` 本地重定义（用 compat_config.h 版本，前置 `#define compat_*_defined` 守卫）
- [x] asix-sigma `sigma_receive_data` 签名为 `(int fd, int revents, const struct sr_dev_inst *sdi)`
- [x] kingst-la2016 protocol.h 含 `sr_sw_limits` 本地副本（含 `get_remain`）— protocol.h:33-174
- [x] kingst-la2016 protocol.h 含 `feed_queue_logic` 前向声明 — protocol.h:176-196
- [x] kingst-la2016 protocol.c 含 `feed_queue_logic` 本地实现（alloc/free/submit_one/flush/send_trigger）— protocol.c:42-185
- [x] kingst-la2016 protocol.c `ezusb_upload_firmware` 用 3 参数（非 4 参数）— protocol.c:1180
- [x] kingst-la2016 `la2016_receive_data` 签名为 `(int fd, int revents, const struct sr_dev_inst *sdi)` — protocol.c:1747-1761
- [x] kingst-la2016 编译通过（api.c + protocol.c 均 EXITCODE=0）

## Layer 3：驱动业务逻辑修复

- [x] agilent-dmm 无 `sr_analog_init` 调用（改为扁平 `analog.probes`/`analog.num_samples`/... 赋值）— 已验证
- [ ] 其余 13 个 DMM 驱动无 `sr_analog_init` 调用（待 Task 15 全量编译验证）
- [x] agilent-dmm 数据帧含 `packet.status = SR_PKT_OK` + `ds_data_forward` — 已验证
- [ ] 14 个 DMM 驱动编译通过（待 Task 15）
- [x] siglent-sds 含 `siglent_sds_compat_receive` SCPI 缓冲区管理包装器 — protocol.h:163 + protocol.c:457
- [ ] hung-chang-dso-2100 `config_channel_set` 含通道合并逻辑（待验证）
- [ ] hantek-6252bd / hantek-dso2x15 / hantek-dso2c10 / rigol-ds / siglent-sds / uni-t-utd2025cl 无本地 `std_session_send_df_frame_begin/end` 定义（待 Task 15 验证）
- [ ] 全部示波器驱动编译通过（待 Task 15；fluke-45/rigol-dg/rigol-ds/siglent-sds 有预存编译错误需另行修复）

## Layer 4：迁移到 compat 层（去重）

- [x] kingst-la2016 无本地 `struct sr_resource`/`SR_RESOURCE_FIRMWARE`/`sr_resource_*`（用 compat 层）— Task 10 期间直接采用 compat 层
- [x] asix-sigma 无本地 `#define SR_RESOURCE_FIRMWARE 1`（用 compat 层）；`sigma_sr_resource_load` 保留为驱动特有包装器 — Task 13.1
- [x] saleae-logic-pro 无本地 `#define SR_RESOURCE_FIRMWARE 1`；`sr_resource_load` 保留为驱动特有包装器 — Task 13.2
- [x] saleae-logic16 无本地 `sr_resource_open/read/close` + 无本地 `struct sr_resource_compat`（用 compat 层规范签名）— Task 13.3
- [x] sysclk-lwla 仅调用 compat 层版本，无本地定义 — Task 13.4 确认
- [x] lecroy-logicstudio 仅调用 compat 层版本，无本地定义 — Task 13.5 确认
- [x] 无驱动本地定义 `read_u*_inc`/`write_u*_inc`（除 rdtech-dps 保留 BE 变体 + asix-sigma 保留 `write_u16be_inc`/`read_u24le_inc`，compat 未提供）— asix-omega-rtm-cli + itech-it8500 + asix-sigma 已迁移
- [x] 5 个驱动编译通过（sr_resource 迁移后）— Task 13.7 验证 8 个 .obj 全部通过

## 全量验证

- [ ] 启用所有已修复驱动后 `ninja -j 16` 无编译错误 — Task 15
- [ ] 启用所有已修复驱动后 `ninja -j 16` 无链接错误 — Task 15
- [ ] `audit-and-fix-migrated-drivers/tasks.md` Task 11/12 已勾选 — Task 16.1
- [ ] `add-sigrok-driver-compat-layer/spec.md` 已标注 compat 层扩展 — Task 16.2
- [ ] `migrate-all-sigrok-drivers/checklist.md` 已标注已修复驱动 — Task 16.3

## 架构原则验证

- [x] compat 层新增定义均有 `#ifndef` 守卫或 enum 类型保护 — 已验证（`compat_*_defined` / `SR_RESOURCE_STRUCT_DEFINED` / `COMPAT_SR_RESOURCE_DECLARED` / `SR_LOG_SPEW` 等）
- [x] compat 层新增常量值均不与 PXView 已有枚举冲突 — `SR_MQ_TIME`=10100 避开 PXView `SR_MQ_HARMONIC_RATIO`=10015
- [x] `sr_sw_limits` 保持驱动本地 `static inline`（未提取到 compat 层）— asix-sigma/kingst-la2016/appa-55ii 等均本地副本
- [x] `feed_queue_logic`/`feed_queue_analog` 保持驱动本地（未提取到 compat 层）— kingst-la2016/protocol.c:42-185 本地实现
- [ ] 无 ≥3 驱动共用的缺失定义残留在驱动本地（应已集中到 compat 层）— 待 Task 13/14 完成后验证
