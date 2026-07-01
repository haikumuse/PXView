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
- [x] rigol-ds 无 `sr_analog_init` 调用（展平为 `analog.probes/mq/unit/mqflags`）— Task 15.4
- [x] siglent-sds 无 `sr_analog_init` 调用（展平）— Task 15.5
- [x] lecroy-xstream 无 `sr_analog_init` 调用（展平）— Task 15.7
- [x] uni-t-ut181a 无 `sr_analog_init` 调用（展平）— Task 15.8
- [x] agilent-dmm 数据帧含 `packet.status = SR_PKT_OK` + `ds_data_forward` — 已验证
- [x] rigol-ds/siglent-sds/lecroy-xstream/uni-t-ut181a 数据帧含 `packet.status = SR_PKT_OK` + `ds_data_forward` — Task 15.4/15.5/15.7/15.8
- [x] fluke-45 含 `sr_sw_limits` 本地副本（appa-55ii 模板）— Task 15.2
- [x] rigol-dg 回调签名为 `(int fd, int revents, const struct sr_dev_inst *sdi)` — Task 15.3
- [x] rigol-ds/siglent-sds 回调签名为 `(int fd, int revents, const struct sr_dev_inst *sdi)` — Task 15.4/15.5
- [x] siglent-sds 含 `siglent_sds_compat_receive` SCPI 缓冲区管理包装器 — protocol.h:163 + protocol.c:457
- [x] 全部目标驱动编译通过（fluke-45/rigol-dg/rigol-ds/siglent-sds/lecroy-xstream/uni-t-ut181a）— Task 15.6/15.9

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

- [x] libsigrok 层全量编译通过（ninja 到达 PXView 阶段即证明）— Task 15.9
- [x] 6 个目标驱动 .obj 全部编译通过（fluke-45/rigol-dg/rigol-ds/siglent-sds/lecroy-xstream/uni-t-ut181a）— Task 15.6/15.9
- [x] 回归检查：之前能编译的驱动（asix-sigma/kingst-la2016/saleae-logic-pro/rigol-ds/pipistrello-ols/siglent-sds 等）未因 compat 层修改而破坏
- [x] PXView 应用层 `ninja -j 16` 全量通过 — ✅ Task 15.10 已修复（SR_PRIV 空宏导致 4 类多重定义链接错误已全部解决；`ninja -j 16` exit=0，`ninja install` exit=0，PXView.exe 255MB 已生成）
- [x] `audit-and-fix-migrated-drivers/tasks.md` Task 11/12 已勾选 — Task 16.1 ✅（Task 11 标记完成，Task 12 部分完成，审计 Task 4-9 + 汇总 Task 10 一并完成）
- [x] `add-sigrok-driver-compat-layer/spec.md` 已标注 compat 层扩展 — Task 16.2 ✅（新增 MODIFIED Requirement "compat 层扩展覆盖范围"）
- [x] `migrate-all-sigrok-drivers/checklist.md` 已标注已修复驱动 — Task 16.3 ✅（15 个驱动"驱动编译通过"项已勾选）

## 架构原则验证

- [x] compat 层新增定义均有 `#ifndef` 守卫或 enum 类型保护 — 已验证（`compat_*_defined` / `SR_RESOURCE_STRUCT_DEFINED` / `COMPAT_SR_RESOURCE_DECLARED` / `SR_LOG_SPEW` / `COMPAT_SR_STRERROR_DECLARED` / `COMPAT_STD_IDX_DECLARED` 等）
- [x] compat 层新增常量值均不与 PXView 已有枚举冲突 — `SR_MQ_TIME`=10100 避开 PXView `SR_MQ_HARMONIC_RATIO`=10015；`SR_CONF_*` 用保留区 30150-30161
- [x] `sr_sw_limits` 保持驱动本地 `static inline`（未提取到 compat 层）— asix-sigma/kingst-la2016/appa-55ii/fluke-45 等均本地副本
- [x] `feed_queue_logic`/`feed_queue_analog` 保持驱动本地（未提取到 compat 层）— kingst-la2016/protocol.c:42-185 本地实现
- [x] 无 ≥3 驱动共用的缺失定义残留在驱动本地（应已集中到 compat 层）— sr_resource_*/_inc/sr_strerror/std_* 等已集中；仅 rdtech-dps 的 BE _inc 变体和 asix-sigma 的 `write_u16be_inc`/`read_u24le_inc` 保留本地（compat 未提供）
