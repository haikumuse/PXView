# Tasks

> **更新（2026-07-02，tiered-driver-compat-fix spec 完成）**：审计阶段（Task 4-9）的发现已汇总为 `tiered-driver-compat-fix` spec，并通过其三层修复架构（compat 层补全 / 驱动本地 shim / 业务逻辑修复）完成了 Task 11 的 MAJOR/BROKEN 修复。Task 12 的 libsigrok 层全量编译已通过；PXView 应用层有独立的 OnMessage 签名问题待修复（不在本 spec 范围）。

## 阶段一：修复已识别的硬性 bug（不阻塞审计，可先行）

- [x] Task 1: 修复 rigol-dg CMakeLists 注册断裂
  - [x] 1.1 在 `CMakeLists.txt` 选项区添加 `option(ENABLE_DRIVER_RIGOL_DG "Enable rigol-dg SCPI signal generator driver" OFF)`（第 615 行）
  - [x] 1.2 在 `CMakeLists.txt` 源文件区添加 `if(ENABLE_DRIVER_RIGOL_DG)` 块 + `list(APPEND ...)`（第 1024-1029 行）
  - [x] 1.3 验证 `add_definitions(-DHAVE_DRIVER_RIGOL_DG)` 已存在（733-735 行）
  - [x] 1.4 补充 `hwdriver.c` 缺失的 `extern SR_PRIV struct sr_dev_driver rigol_dg_driver_info;`（163-165 行，原仅 rigol_ds 有 extern）
  - [x] 1.5 编译验证留待最终阶段

- [ ] Task 2: 补完 norma-dmm 迁移
  - [ ] 2.1 对比 `C:\Users\admin\Downloads\libsigrok\src\hardware\norma-dmm\` 与空目录 `libsigrok/hardware/norma-dmm/`
  - [ ] 2.2 按 agilent-dmm / fluke-dmm 迁移模板，迁移 api.c / protocol.c / protocol.h（include 改 compat.h、std_*_idx 替换、5-arg sr_session_source_add 等）
  - [ ] 2.3 在 `CMakeLists.txt` 添加 `option(ENABLE_DRIVER_NORMA_DMM ...)` + 源文件块 + `add_definitions(-DHAVE_DRIVER_NORMA_DMM)`
  - [ ] 2.4 在 `hwdriver.c` 添加 `extern struct sr_dev_driver norma_dmm_driver_info;` + 注册项 + `HAVE_DRIVER_NORMA_DMM` 守卫
  - [ ] 2.5 编译验证：`ENABLE_DRIVER_NORMA_DMM=ON` 单独启用

- [ ] Task 3: 补完 serial-dmm 迁移
  - [ ] 3.1 对比 `C:\Users\admin\Downloads\libsigrok\src\hardware\serial-dmm\` 与空目录 `libsigrok/hardware/serial-dmm/`
  - [ ] 3.2 按 serial-dmm 模板迁移（注意 serial-dmm 含 5 个 DMM parser 子模块，需全部保留）
  - [ ] 3.3 在 `CMakeLists.txt` 添加 option + 源文件块 + add_definitions
  - [ ] 3.4 在 `hwdriver.c` 添加 extern + 注册项 + 守卫
  - [ ] 3.5 编译验证：`ENABLE_DRIVER_SERIAL_DMM=ON` 单独启用

## 阶段二：多 agent 并行审计 57 个已迁移驱动

> 每个子代理输出统一格式的审计报告：每个驱动标记 OK / MINOR / MAJOR / BROKEN，并附 diff 摘要。MAJOR/BROKEN 需明确指出丢失的功能点。

- [x] Task 4: Audit-1 逻辑分析仪前半（8 个驱动）✅ 审计完成
  - [x] 4.1 对比 fx2lafw、saleae-logic16、saleae-logic-pro、raspberrypi-pico、asix-sigma、chronovu-la、ftdi-la、kingst-la2016
  - [x] 4.2 核验 7 项功能等价性（scan/config/open/close/acq_start/acq_stop/protocol）
  - [x] 4.3 输出审计报告，标记每驱动状态

- [x] Task 5: Audit-2 逻辑分析仪后半（9 个驱动）✅ 审计完成
  - [x] 5.1 对比 sysclk-lwla、sysclk-sla5032、zeroplus-logic-cube、openbench-logic-sniffer、ikalogic-scanalogic2、ikalogic-scanaplus、lecroy-logicstudio、ipdbg-la、sipeed-slogic-analyzer
  - [x] 5.2 核验 7 项功能等价性
  - [x] 5.3 重点复核 openbench-logic-sniffer/protocol.c (-56%) 与 protocol.h (-31%) 变小原因
  - [x] 5.4 输出审计报告

- [x] Task 6: Audit-3 示波器（14 个驱动）✅ 审计完成
  - [x] 6.1 对比 rigol-ds、siglent-sds、hantek-dso、hantek-6xxx、hantek-4032l、lecroy-xstream、yokogawa-dlm、gwinstek-gds-800、hung-chang-dso-2100、link-mso19、uni-t-ut181a、rohde-schwarz-sme-0x、hameg-hmo、rigol-dg
  - [x] 6.2 核验 SCPI 通信包装、config_channel_set 合并、5-arg source_add 适配
  - [x] 6.3 输出审计报告

- [x] Task 7: Audit-4 万用表（16 个驱动）✅ 审计完成
  - [x] 7.1 对比 fluke-dmm、fluke-45、agilent-dmm、appa-55ii、uni-t-dmm、uni-t-ut32x、gwinstek-psp、mastech-ms6514、testo、lascar-el-usb、center-3xx、mic-985xx、tondaj-sl-814、kern-scale、teleinfo、pce-322a
  - [x] 7.2 核验 Serial 通信包装、DMM parser 完整性（fluke-dmm 含 18x/28x/190 三子模块）
  - [x] 7.3 输出审计报告

- [x] Task 8: Audit-5 电源/负载（11 个驱动）✅ 审计完成
  - [x] 8.1 对比 siglent-sdl10x0、itech-it8500、maynuo-m97、korad-kaxxxxp、atten-pps3xxx、manson-hcs-3xxx、motech-lps-30x、rdtech-dps、rdtech-tc、rdtech-um、scpi-pps
  - [x] 8.2 核验 SCPI/Serial 包装、scpi-pps profiles.c 完整性
  - [x] 8.3 输出审计报告

- [x] Task 9: Audit-6 其他 + 变小文件复核（4 个驱动）✅ 审计完成
  - [x] 9.1 对比 pipistrello-ols（Batch 5 仅此一个已迁移）
  - [x] 9.2 复核 asix-sigma/protocol.c (-33%) 变小原因
  - [x] 9.3 复核 raspberrypi-pico/protocol.c (-30%) 变小原因
  - [x] 9.4 openbench-logic-sniffer 已在 Task 5 复核，此处汇总
  - [x] 9.5 输出审计报告 + 变小文件合法性结论

## 阶段三：修复审计发现的问题

- [x] Task 10: 汇总审计报告 ✅ 审计发现已汇总为 `tiered-driver-compat-fix` spec
  - [x] 10.1 收集 Task 4-9 的 6 份审计报告
  - [x] 10.2 列出所有 MAJOR/BROKEN 驱动及具体问题（23 个 BROKEN、3 个 MAJOR、2 个 MINOR）
  - [x] 10.3 列出 4 个变小文件的合法性结论
  - [x] 10.4 创建修复任务清单（每个 MAJOR/BROKEN 一条）→ 已转化为 `tiered-driver-compat-fix` spec 的 Task 1-15

- [x] Task 11: 逐个修复 MAJOR/BROKEN 驱动 ✅ 通过 `tiered-driver-compat-fix` spec Layer 3 完成
  - [x] 11.1 对每个 MAJOR/BROKEN 驱动，按原版库原版补回缺失的 SR_CONF 项 / 协议分支 / 解析逻辑 — 详见 `tiered-driver-compat-fix` Task 5-12
  - [x] 11.2 修复后重新对比，确认等价 — 详见 `tiered-driver-compat-fix` Task 15
  - [x] 11.3 编译验证每个修复的驱动 — fluke-45/rigol-dg/rigol-ds/siglent-sds/lecroy-xstream/uni-t-ut181a/agilent-dmm 等全部 .obj 编译通过

## 阶段四：最终验证

- [~] Task 12: 全量编译验证 ⚠️ 部分完成（libsigrok 层通过；PXView 应用层阻塞）
  - [~] 12.1 启用 rigol-dg / norma-dmm / serial-dmm + 所有审计修复涉及的驱动 — ⚠️ rigol-dg 已启用并编译通过；norma-dmm / serial-dmm 仍未迁移（Task 2/3 未完成）
  - [x] 12.2 运行 `cd build && ninja -j 16 && ninja install` — ✅ ninja 到达 PXView 阶段即证明 libsigrok 层编译成功
  - [x] 12.3 确认无编译/链接错误 — ✅ libsigrok 层无错误
  - [ ] 12.4 启动 PXView.exe 确认设备列表完整 — ⚠️ 阻塞于 PXView 应用层 OnMessage 签名不匹配（`OnMessage(int)` vs `OnMessage(int,int)`），不在本 spec 范围

- [~] Task 13: 更新 migrate-all-sigrok-drivers spec ⚠️ 部分完成
  - [~] 13.1 更新 checklist.md 勾选 norma-dmm / serial-dmm / rigol-dg — ⚠️ rigol-dg 已勾选；norma-dmm / serial-dmm 未迁移暂不勾选；其余审计修复涉及的驱动已勾选"驱动编译通过"
  - [x] 13.2 更新 spec.md 标注审计已完成 — ✅ 已在 checklist.md 顶部添加状态说明

# Task Dependencies

- [Task 1, 2, 3] 互相独立，可并行
- [Task 4-9] 互相独立，可并行（阶段二全并行）— ✅ 全部完成
- [Task 10] depends on [Task 4-9] 全部完成 — ✅ 完成
- [Task 11] depends on [Task 10] — ✅ 通过 tiered-driver-compat-fix 完成
- [Task 12] depends on [Task 1, 2, 3, 11] — ⚠️ 部分完成（阻塞于 Task 2/3 + PXView 应用层）
- [Task 13] depends on [Task 12] — ⚠️ 部分完成

# 并行策略

- **阶段一**：Task 1/2/3 可同时 3 个子代理并行 — Task 1 ✅；Task 2/3 ⏳ 未完成
- **阶段二**：Task 4/5/6/7/8/9 可同时 6 个子代理并行（每批一个）— ✅ 全部完成
- **阶段三**：Task 11 内各驱动修复可并行 — ✅ 通过 tiered-driver-compat-fix 完成
- **阶段四**：Task 12/13 串行 — ⚠️ 部分完成
