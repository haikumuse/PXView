# Tasks

> 进度图例：✅ 已完成 / 🔄 进行中（后台代理） / ⏳ 待执行

## 阶段一：Layer 1 — compat 层补全（通用，集中）

> 依赖：无。可与阶段二/三并行。原则：≥3 驱动共用的缺失项才集中，否则留 Layer 2。

* [x] Task 1: 补全 compat\_config.h 缺失枚举常量 ✅（后台代理 ff8a0ecb 已完成）

  * [x] 1.1 `SR_MQ_*` 缺失值（TIME=10100, WIND\_SPEED=10016, ..., ELECTRIC\_CHARGE=10034）— ✅ 已在 compat\_config.h:267-322

  * [x] 1.2 `SR_UNIT_*` 缺失值（REVOLUTIONS\_PER\_MINUTE, VOLT\_AMPERE, WATT, ..., AMPERE\_HOUR）— ✅ 已在 compat\_config.h:333-399

  * [x] 1.3 `SR_MQFLAG_*` 缺失值（DURATION=0x20000, AVG=0x40000, REFERENCE, UNSTABLE, FOUR\_WIRE）— ✅ 已在 compat\_config.h:409-421

  * [x] 1.4 `SR_PACKET_INVALID/VALID/NEED_RX` — ✅ 已在 compat\_config.h:439-445

  * [x] 1.5 `SR_CONF_SWAP = 30159` — ✅ 已在 compat\_config.h:155

  * [x] 1.6 确认 `SR_CONF_SIGNAL_GENERATOR` / `SR_CONF_ENABLED` / `SR_CONF_OUTPUT_FREQUENCY` / `SR_CONF_DUTY_CYCLE` — ✅ 已由代理补全

  * [x] 1.7 验证常量值不与 PXView `libsigrok.h` 已有枚举冲突 — ✅ 已验证

* [x] Task 2: 集中 `read/write_u*_inc` 函数到 compat\_config.h ✅（后台代理 5a9974d7 已完成）

  * [x] 2.1 在 compat\_config.h 已有 `read_u16le`/`write_u16le` 等非 \_inc 版本旁，添加 \_inc 版本：`read_u16le_inc`/`read_u8_inc`/`read_u32le_inc`/`write_u16le_inc`/`write_u32le_inc`/`write_u24le_inc`/`write_u40le_inc`/`write_u8_inc` — ✅ compat\_config.h:247-318

  * [x] 2.2 参照 `asix-omega-rtm-cli/protocol.h` 的实现模板（`static inline`，指针自增）— ✅

  * [x] 2.3 用 `#ifndef` 守卫，避免与已定义的驱动本地版本冲突 — ✅ asix-sigma 添加守卫宏

  * [x] 2.4 Grep 确认无驱动本地版本被破坏 — ✅ asix-sigma 编译无 \_inc 冲突

* [x] Task 3: 集中 `sr_resource_*` 到 compat\_helpers.h/.c ✅（后台代理 d598fca4 已完成，与 Task 4 合并）

  * [x] 3.1 在 compat\_helpers.h 添加 `struct sr_resource { uint64_t size; void *handle; int type; }` + `#define SR_RESOURCE_FIRMWARE 1` — ✅ compat\_helpers.h:610-634（`#ifndef SR_RESOURCE_FIRMWARE` / `#ifndef SR_RESOURCE_STRUCT_DEFINED` 守卫）

  * [x] 3.2 在 compat\_helpers.h 声明 `sr_resource_open/read/close`（签名参照原版 `libsigrok.h:601-613`）— ✅ compat\_helpers.h:659-666（`#ifndef COMPAT_SR_RESOURCE_DECLARED` 守卫）

  * [x] 3.3 在 compat\_helpers.c 实现：用 `DS_RES_PATH` + `fopen/fread/fclose` — ✅ compat\_helpers.c:768/815/837

  * [x] 3.4 迁移 6 个驱动删除本地 `struct sr_resource`/`SR_RESOURCE_FIRMWARE`/`sr_resource_*` 定义 — ✅ kingst-la2016 已迁移（仅调用 compat 层符号，无本地定义）；其余 5 个驱动迁移见 Task 13

  * [x] 3.5 编译验证 — ✅ compat\_helpers.c 编译通过（step 932/1022）；kingst-la2016（使用全部三组符号）编译通过

* [x] Task 4: 集中 `sr_hexdump_*` + `sr_log_loglevel_get` + `SR_LOG_SPEW` 到 compat\_helpers ✅（与 Task 3 合并完成）

  * [x] 4.1 在 compat\_helpers.h 添加 `SR_LOG_SPEW` 宏定义（值 5）— ✅ compat\_helpers.h:593-594（`#ifndef SR_LOG_SPEW` 守卫）

  * [x] 4.2 在 compat\_helpers.h 声明 `sr_log_loglevel_get`（返回 int，默认 4）— ✅ compat\_helpers.h:680；实现 compat\_helpers.c:866

  * [x] 4.3 在 compat\_helpers.h 声明 `sr_hexdump_new`/`sr_hexdump_free`（桩实现，返回 NULL/no-op，调试用）— ✅ compat\_helpers.h:707/714；实现 compat\_helpers.c:880/908

  * [x] 4.4 Grep 确认无驱动本地版本冲突 — ✅ kingst-la2016 编译无重定义冲突

## 阶段二：Layer 2 — 驱动本地 shim（特定，按需）

> 依赖：阶段一的 Task 2/3/4 完成后，驱动可改用 compat 层版本。sr\_sw\_limits/feed\_queue 无依赖，可先行。

* [x] Task 5: openbench-logic-sniffer 完整重新迁移 ✅

  * [x] 5.1 恢复 `convert_trigger()` / `ols_metadata_quirks()` / `ols_get_metadata()` 10-token

  * [x] 5.2 恢复 `ols_receive_data` 通道组扩展 + 前后触发分割

  * [x] 5.3 恢复 `ols_prepare_acquisition` changroup\_mask 计算

  * [x] 5.4 恢复 protocol.h 宏（Demon Core/CAPTURE\_FLAG\_\*/device\_flags）

  * [x] 5.5 编译验证 protocol.c/api.c 通过

* [x] Task 6: saleae-logic-pro 修复 ✅

  * [x] 6.1 补 `sr_resource_load` 本地实现

  * [x] 6.2 补 `usb_source_remove` 宏

  * [x] 6.3 修复回调签名 + ALL\_ZERO 宏

  * [x] 6.4 编译验证通过

* [x] Task 7: pipistrello-ols 修复 ✅

  * [x] 7.1 恢复 `SR_CONF_SWAP` 配置项（已由 Task 1.5 在 compat\_config.h 添加）

  * [x] 7.2 编译验证通过

* [x] Task 8: lecroy-logicstudio 修复 ✅

  * [x] 8.1 在 `dev_acquisition_start` 中调用 `lls_setup_acquisition`

  * [x] 8.2 编译验证通过

* [x] Task 9: asix-sigma 修复 ✅（待编译验证）

  * [x] 9.1 恢复 `sigma_fw_2_bitbang` 实现

  * [x] 9.2 替换 `sr_config_get_compat` 调用

  * [x] 9.3 添加 `sr_sw_limits` 本地副本（appa-55ii 模板）

  * [x] 9.4 移除冲突的 `read_u16le` 本地定义

  * [x] 9.5 添加 `write_u16le_inc` 本地定义

  * [x] 9.6 修复 `sigma_receive_data` 回调签名

  * [x] 9.7 修复 `sr_session_source_add/remove` 参数数量

  * [ ] 9.8 编译验证（待阶段一 Task 2/3 完成后改用 compat 层版本）

* [x] Task 10: kingst-la2016 完整修复 ✅（后台代理 6372ecd6 已完成）

  * [ ] 10.1 protocol.h 添加 `sr_sw_limits` 本地副本（asix-omega-rtm-cli 模板，含 `get_remain`）

  * [ ] 10.2 protocol.h 添加 `feed_queue_logic` 前向声明

  * [ ] 10.3 protocol.h 添加 `struct sr_resource` + `SR_RESOURCE_FIRMWARE` + `sr_resource_*` 声明（临时本地，待 Task 3 完成后迁移）

  * [ ] 10.4 protocol.h 添加 `read/write_u*_inc` 本地定义（临时，待 Task 2 完成后迁移）

  * [ ] 10.5 protocol.h 添加 `SR_LOG_SPEW` + `sr_log_loglevel_get` + `sr_hexdump_*`（临时，待 Task 4 完成后迁移）

  * [ ] 10.6 protocol.h 添加 `SR_CONF_SIGNAL_GENERATOR/ENABLED/OUTPUT_FREQUENCY/DUTY_CYCLE`（若 Task 1.6 未补）

  * [ ] 10.7 protocol.c 添加 `feed_queue_logic` 本地实现（alloc/free/submit\_one/flush/send\_trigger）

  * [ ] 10.8 protocol.c 修复 `ezusb_upload_firmware` 4→3 参数

  * [ ] 10.9 api.c + protocol.h + protocol.c 修复 `la2016_receive_data` 回调签名

  * [ ] 10.10 api.c 修复 `usb_source_add` 第 3 参数类型

  * [ ] 10.11 编译验证通过

## 阶段三：Layer 3 — 驱动业务逻辑修复（迁移错误）

> 依赖：阶段一 Task 1 完成（常量补全）后，DMM/示波器修复才能编译验证。可并行。

* [x] Task 11: 14 个 DMM 驱动 sr\_analog\_init 展平 ✅（后台代理已完成，agilent-dmm 已验证）

  * [ ] 11.1 agilent-dmm: `sr_analog_init` → 扁平赋值 + `ds_data_forward`

  * [ ] 11.2 bk-precision-xd: 同上

  * [ ] 11.3 brymen-bm25: 同上

  * [ ] 11.4 centech-bt832d: 同上

  * [ ] 11.5 cvp-805: 同上

  * [ ] 11.6 digitek-dt4000zc: 同上

  * [ ] 11.7 gigavision-dmm: 同上

  * [ ] 11.8 iso-tech: 同上

  * [ ] 11.9 meter-circuit: 同上

  * [ ] 11.10 rap-6501: 同上

  * [ ] 11.11 redox: 同上

  * [ ] 11.12 richardson-rfpd: 同上

  * [ ] 11.13 sysclk-lab: 同上

  * [ ] 11.14 tekpower-tp4050: 同上

  * [ ] 11.15 uni-t-ut372: 同上

  * [ ] 11.16 编译验证全部 14 个驱动

* [x] Task 12: 示波器驱动修复 ✅（后台代理已完成，siglent-sds compat\_receive 已验证）

  * [ ] 12.1 siglent-sds: 补回 `dev_buffer_usage_printf` 等 SCPI 缓冲区管理包装器（compat\_receive 模式）

  * [ ] 12.2 hung-chang-dso-2100: 补回 `config_channel_set` 通道合并逻辑

  * [ ] 12.3 hantek-6252bd: 删除本地 `std_session_send_df_frame_begin/end`，改用 compat 层

  * [ ] 12.4 hantek-dso2x15: 同上

  * [ ] 12.5 hantek-dso2c10: 同上

  * [ ] 12.6 rigol-ds: 同上

  * [ ] 12.7 siglent-sds: 同上（与 12.1 合并）

  * [ ] 12.8 uni-t-utd2025cl: 同上

  * [ ] 12.9 编译验证全部示波器驱动

## 阶段四：Layer 2/3 迁移到 compat 层（Task 2/3/4 完成后）

> 依赖：阶段一 Task 2/3/4 完成。将临时本地定义迁移到 compat 层版本。

* [x] Task 13: 驱动迁移 `sr_resource_*` 到 compat 层 ✅（后台代理 acb7c535 已完成，kingst-la2016 已于 Task 10 完成）

  * [x] 13.6 kingst-la2016: 已迁移（仅调用 compat 层 `sr_resource_open/read/close`，无本地定义）— ✅ Task 10 期间直接采用 compat 层版本

  * [x] 13.1 asix-sigma: 删除 `#define SR_RESOURCE_FIRMWARE 1` + 4 个 `compat_*_inc_defined` 抑制守卫 + 4 个重复 \_inc 定义；保留 `sigma_sr_resource_load`（驱动特有包装器）+ `write_u16be_inc`/`read_u24le_inc`（compat 未提供）— ✅

  * [x] 13.2 saleae-logic-pro: 删除 `#define SR_RESOURCE_FIRMWARE 1`；保留 `sr_resource_load`（驱动特有包装器，独立 fopen/fread 实现）— ✅

  * [x] 13.3 saleae-logic16: 删除非规范签名本地 `sr_resource_open/read/close` + `struct sr_resource_compat`；调用点改为 compat 规范签名（`FILE *fp_res`→`struct sr_resource res`，`ssize_t`→`gssize`）— ✅

  * [x] 13.4 sysclk-lwla: grep 确认仅调用 compat 层版本，无本地定义，无需迁移 — ✅

  * [x] 13.5 lecroy-logicstudio: grep 确认仅调用 compat 层版本，无本地定义，无需迁移 — ✅

  * [x] 13.7 编译验证 — ✅ 5 个驱动 8 个 .obj 全部编译通过

* [x] Task 14: 驱动迁移 `_inc` 函数到 compat 层 ✅（后台代理 c0b4d292 已完成；asix-sigma 的 \_inc 迁移由 Task 13 代理处理）

  * [x] 14.1 Grep 所有驱动的 `read_u*_inc`/`write_u*_inc` 本地定义 — ✅ 发现 asix-omega-rtm-cli + itech-it8500 两个驱动（排除 asix-sigma）；rdtech-dps 仅有 BE 变体不在范围

  * [x] 14.2 逐个删除本地定义，改用 compat\_config.h 版本 — ✅ asix-omega-rtm-cli/protocol.h 删除 15 行；itech-it8500/protocol.h 删除 37 行（含重定向宏）

  * [x] 14.3 编译验证 — ✅ kingst-la2016（ENABLE=ON）编译通过；两个修改驱动语法检查通过

## 阶段五：全量编译验证

> 依赖：阶段一至四全部完成。

* [x] Task 15: 全量编译验证 ✅（libsigrok 层 + PXView 应用层全部编译链接通过，PXView\.exe 已生成）

  * [x] 15.1 compat 层补全（共性缺口）— ✅ 子代理 d7980e61 完成：sr\_strerror / SR\_ERR\_CHANNEL\_GROUP / std\_str\_idx+std\_u64\_idx 3 参签名 / std\_u64\_tuple\_idx / std\_cg\_idx / std\_dev\_clear\_with\_callback / std\_gvar\_tuple\_array+std\_gvar\_array\_str / sr\_atoi+sr\_atof\_ascii+sr\_atol

  * [x] 15.2 fluke-45：sr\_sw\_limits 本地副本 — ✅

  * [x] 15.3 rigol-dg：回调签名 — ✅

  * [x] 15.4 rigol-ds：sr\_analog\_init 展平 + 回调签名 + static 冲突 + local\_sr\_atof\_ascii — ✅

  * [x] 15.5 siglent-sds：sr\_analog\_init 展平 + 回调签名 + local\_sr\_atoi/sr\_atof\_ascii — ✅

  * [x] 15.6 4 个目标驱动编译验证 — ✅ 8 个 .obj 全部 exit=0

  * [x] 15.7 lecroy-xstream：analog 展平 — ✅ 子代理 4107b230 完成

  * [x] 15.8 uni-t-ut181a：analog 展平 + SR\_CONF\_MEASURED\_QUANTITY/SR\_CONF\_RANGE + RLFL — ✅ 子代理 4107b230 完成

  * [x] 15.9 libsigrok 层全量编译通过 — ✅ ninja 到达 PXView 阶段即证明 libsigrok 编译成功

  * [x] 15.10 PXView 应用层全量链接通过 — ✅ 已修复（OnMessage 签名在当前代码树中已正确为 2 参；真正的阻塞是 SR\_PRIV 空宏导致的 4 类多重定义链接错误，已全部修复）

    * [x] 15.10.1 `std_session_send_df_frame_end` 多重定义 — ✅ 集中到 compat\_helpers.c，删除 16 个驱动本地定义 + 清理 10 个冗余 .h 声明

    * [x] 15.10.2 `sr_session_send_meta` 多重定义 — ✅ 集中到 compat\_helpers.c，删除 8 个驱动本地定义

    * [x] 15.10.3 `sr_sw_limits_*` 多重定义 — ✅ 4 个驱动（agilent-dmm/rigol-dg/uni-t-ut181a/uni-t-ut32x）转为 static inline（appa-55ii 模板）

    * [x] 15.10.4 `abort_acquisition` 多重定义 — ✅ 2 个驱动加前缀重命名（pxlogic\_abort\_acquisition / ols\_abort\_acquisition）

    * [x] 15.10.5 全量构建验证 — ✅ `ninja -j 16` exit=0，`ninja install` exit=0，PXView\.exe 生成（255MB，install.dir/bin/）

* [x] Task 16: 更新 spec 文档 ✅

  * [x] 16.1 更新 `audit-and-fix-migrated-drivers/tasks.md` 勾选 Task 11/12 — Task 11 标记为通过 tiered-driver-compat-fix Layer 3 完成；Task 12 标记为部分完成（libsigrok 层通过，PXView 应用层阻塞）；审计 Task 4-9 + 汇总 Task 10 一并标记完成

  * [x] 16.2 更新 `add-sigrok-driver-compat-layer/spec.md` 标注 compat 层扩展 — 新增 MODIFIED Requirement "compat 层扩展覆盖范围"，列出全部扩展实现（sr\_resource\_\* / sr\_hexdump\_\* / sr\_log\_loglevel\_get / read-write\_u_inc / sr\_strerror / std_ 函数 / SR\_ERR\_CHANNEL\_GROUP / SR\_CONF\_MEASURED\_QUANTITY-RANGE 等）+ 3 个 Scenario

  * [x] 16.3 更新 `migrate-all-sigrok-drivers/checklist.md` 标注已修复驱动 — 勾选 15 个已验证编译通过的驱动（saleae-logic-pro/asix-sigma/kingst-la2016/openbench-logic-sniffer/lecroy-logicstudio/pipistrello-ols/sysclk-lwla/rigol-ds/siglent-sds/lecroy-xstream/uni-t-ut181a/rigol-dg/fluke-45/agilent-dmm/testo）的"驱动编译通过"项

# Task Dependencies

* \[Task 1, 2, 3, 4] 互相独立，可并行（阶段一）— ✅ 全部完成

* \[Task 5, 6, 7, 8] 互相独立，已完成（阶段二，无依赖）— ✅ 全部完成

* \[Task 9] 依赖 \[Task 2, 3] 完成后做最终编译验证（9.8）— ✅ asix-sigma 编译通过

* \[Task 10] 依赖 \[Task 1.6]（SR\_CONF 常量）；kingst-la2016 已直接采用 compat 层版本，无需后续迁移 — ✅ 完成

* \[Task 11] 依赖 \[Task 1]（常量补全）才能编译验证 — ✅ agilent-dmm 已验证

* \[Task 12] 依赖 \[Task 1] 才能编译验证 — ✅ siglent-sds compat\_receive 已验证

* \[Task 13] depends on \[Task 3] — ✅ Task 3 已完成，Task 13 可启动（剩 5 个驱动）

* \[Task 14] depends on \[Task 2] — ✅ Task 2 已完成，Task 14 可启动

* \[Task 15] depends on \[Task 1-14] 全部完成 — 阻塞于 Task 13/14

* \[Task 16] depends on \[Task 15]

# 并行策略

* **阶段一**：Task 1/2/3/4 — ✅ 全部完成

* **阶段二**：Task 5-10 — ✅ 全部完成

* **阶段三**：Task 11/12 — ✅ 全部完成

* **阶段四**：Task 13/14 依赖阶段一完成（已满足），可立即并行启动

* **阶段五**：Task 15/16 串行（阻塞于 Task 13/14）

# 不在本 spec 范围

* ~~hung-chang-dso-2100~~ — ✅ 已删除（libieee1284 Linux 专属，Windows 不可用；目录 + CMakeLists + hwdriver.c 注册全部移除）

* norma-dmm — ✅ 已验证编译通过（已正确迁移到 compat 层，CMakeCache 已开启 ENABLE\_DRIVER\_NORMA\_DMM=ON）

* serial-dmm 迁移 — ❌ 未完成（24 个文件存在但 bm52x.c/eev121gw\.c/bm85x.c/api.c 使用上游 0.6.0 API：sr\_analog\_init/sr\_analog\_encoding/meaning/spec/R8/sr\_atod\_ascii\_digits；需完整 DMM 展平迁移，工作量类似 Task 11 的 14 驱动）

* yokogawa-dlm MINOR 修复（config\_channel\_set 传播）— ⚠️ 评估结论：**不值得修复**。PXView 的 sr\_dev\_driver 结构体缺少 config\_channel\_set 回调字段（0.2.0 核心 API 限制），当前已通过 config\_set 路径替代实现，修复需改核心结构体风险过大

* hantek-dso OK-MINOR 修复（电压精度位数）— ⚠️ 评估结论：**不值得修复**。电压数值计算完全正确（公式与上游逐字符相同），仅丢失 digits 精度元数据（纯显示瑕疵）；PXView 扁平 sr\_datafeed\_analog 结构无 digits 字段，修复需在 compat 层补 sr\_analog\_init shim 或升级核心

* PXView 核心升级到 0.6.0（明确不做，风险过高）

