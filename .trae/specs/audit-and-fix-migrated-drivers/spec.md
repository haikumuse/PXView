# 审计并修复已迁移 sigrok 驱动 Spec

## Why

`migrate-all-sigrok-drivers` spec 已推进至 57 个驱动迁移完成，但先前完整性扫描发现三类问题：
1. **构建注册断裂**：`rigol-dg` 目录文件齐全、`hwdriver.c` 已注册，但 `CMakeLists.txt` 缺 `option()` 与 `list(APPEND ...)` → 驱动永远无法编译。
2. **空目录残留**：`norma-dmm`、`serial-dmm` 目录存在但 0 文件，是子代理未完成的产物，未注册到 CMakeLists/hwdriver.c。
3. **疑似功能裁剪**：4 个文件相对原库显著变小（`openbench-logic-sniffer/protocol.c` -56%、`asix-sigma/protocol.c` -33%、`raspberrypi-pico/protocol.c` -30%、`openbench-logic-sniffer/protocol.h` -31%），可能丢失功能。

同时，先前仅做了文件名/大小级别的核对，未对每个驱动的**功能等价性**做逐函数比对。需要在 `C:\Users\admin\Downloads\old\libsigrok\src\hardware\` 原版基础上系统性审计 57 个已迁移驱动，确保迁移未引入静默功能缺失。

## What Changes

### A. 修复已识别的硬性 bug

- **rigol-dg CMakeLists 三处补全**：
  - 在选项区（约 597-655 行）添加 `option(ENABLE_DRIVER_RIGOL_DG "Enable rigol-dg SCPI signal generator driver" OFF)`
  - 在源文件区（约 901-1265 行）添加 `if(ENABLE_DRIVER_RIGOL_DG)` 块 + `list(APPEND libsigrok_SOURCES libsigrok/hardware/rigol-dg/api.c protocol.c)`
- **norma-dmm / serial-dmm 空目录处理**：
  - 评估 old 库源文件（各 3 个文件约 60-100KB），决定：补完迁移并注册，或删除空目录
  - 鉴于 spec Batch 3 已规划这两个驱动，**优先补完迁移**而非删除

### B. 多 agent 并行审计 57 个已迁移驱动

按设备类别分 6 批，每批 1 个子代理，对比 old 库原版逐函数核验功能等价性：

| 批次 | 范围 | 驱动数 |
|---|---|---|
| Audit-1 | Batch 1 逻辑分析仪前半（fx2lafw, saleae-logic16, saleae-logic-pro, raspberrypi-pico, asix-sigma, chronovu-la, ftdi-la, kingst-la2016） | 8 |
| Audit-2 | Batch 1 逻辑分析仪后半（sysclk-lwla, sysclk-sla5032, zeroplus-logic-cube, openbench-logic-sniffer, ikalogic-scanalogic2, ikalogic-scanaplus, lecroy-logicstudio, ipdbg-la, sipeed-slogic-analyzer） | 9 |
| Audit-3 | Batch 2 示波器（rigol-ds, siglent-sds, hantek-dso, hantek-6xxx, hantek-4032l, lecroy-xstream, yokogawa-dlm, gwinstek-gds-800, hung-chang-dso-2100, link-mso19, uni-t-ut181a, rohde-schwarz-sme-0x, hameg-hmo, rigol-dg） | 14 |
| Audit-4 | Batch 3 万用表（fluke-dmm, fluke-45, agilent-dmm, appa-55ii, uni-t-dmm, uni-t-ut32x, gwinstek-psp, mastech-ms6514, testo, lascar-el-usb, center-3xx, mic-985xx, tondaj-sl-814, kern-scale, teleinfo, pce-322a） | 16 |
| Audit-5 | Batch 4 电源/负载（siglent-sdl10x0, itech-it8500, maynuo-m97, korad-kaxxxxp, atten-pps3xxx, manson-hcs-3xxx, motech-lps-30x, rdtech-dps, rdtech-tc, rdtech-um, scpi-pps） | 11 |
| Audit-6 | Batch 5 其他（pipistrello-ols）+ 已迁移 4 个文件显著变小的复核（openbench-logic-sniffer/asix-sigma/raspberrypi-pico） | 4 |

### C. 修复审计发现的问题

- 对审计标记为 MAJOR/BROKEN 的驱动，按 old 库原版补回缺失逻辑
- 复审变小文件是否为合法重构（移除未用代码）或功能丢失

## Impact

- Affected specs: 扩展 `migrate-all-sigrok-drivers`（修正其已迁移驱动的完整性问题）
- Affected code:
  - `CMakeLists.txt`（rigol-dg 注册补全）
  - `libsigrok/hardware/norma-dmm/`（补完迁移）
  - `libsigrok/hardware/serial-dmm/`（补完迁移）
  - `libsigrok/hwdriver.c`（norma-dmm/serial-dmm 注册）
  - 4 个变小文件所在驱动目录（按审计结果修复）
  - 其他审计发现问题的驱动目录

## ADDED Requirements

### Requirement: 驱动构建注册完整性

每一个已迁移驱动的目录 SHALL 同时具备三项 CMakeLists/hwdriver.c 注册：
1. `option(ENABLE_DRIVER_<NAME> ...)` 选项声明
2. `if(ENABLE_DRIVER_<NAME>)` 块内的 `list(APPEND libsigrok_SOURCES ...)` 源文件条目
3. `add_definitions(-DHAVE_DRIVER_<NAME>)` 宏定义
4. `hwdriver.c` 内 `extern struct sr_dev_driver <name>_driver_info;` + 注册项 + `HAVE_DRIVER_<NAME>` 守卫

#### Scenario: rigol-dg 可编译
- **WHEN** 用户设置 `ENABLE_DRIVER_RIGOL_DG=ON`
- **THEN** CMake SHALL 将 `libsigrok/hardware/rigol-dg/api.c` 和 `protocol.c` 加入编译目标
- **AND** 链接器 SHALL 找到 `rigol_dg_driver_info` 符号

#### Scenario: norma-dmm / serial-dmm 完整迁移
- **WHEN** 用户设置 `ENABLE_DRIVER_NORMA_DMM=ON` 或 `ENABLE_DRIVER_SERIAL_DMM=ON`
- **THEN** 该驱动 SHALL 完整出现在设备列表
- **AND** 目录内 SHALL 含至少 api.c/protocol.c/protocol.h 三个文件

### Requirement: 驱动功能等价性审计

每一个已迁移驱动 SHALL 通过与 `C:\Users\admin\Downloads\old\libsigrok\src\hardware\<driver>\` 原版的逐函数对比审计，验证以下方面未发生静默丢失：

1. **scan/scan_complete 逻辑**：设备扫描、USB VID/PID 匹配、串口探测保留
2. **config_get/config_set/config_list**：所有 `SR_CONF_*` 配置项保留（采样率、采样深度、通道、耦合、触发等）
3. **dev_open/dev_close**：设备打开/关闭流程保留
4. **acquisition_start/acquisition_stop**：采集启停逻辑保留
5. **receive_data/feed_frame**：数据接收与上送逻辑保留
6. **协议解析**：`protocol.c` 内设备私有协议解析逻辑保留
7. **dev_context 结构体**：设备状态字段保留

#### Scenario: 审计通过
- **WHEN** 子代理对比驱动 X 的 old 与 new 版本
- **THEN** 上述 7 项 SHALL 全部等价（允许 compat 层适配带来的签名/包装变化）
- **AND** 输出审计报告标记 OK / MINOR / MAJOR / BROKEN

#### Scenario: 审计发现 MAJOR/BROKEN
- **WHEN** 审计发现功能丢失（如 SR_CONF 项缺失、协议解析分支被删）
- **THEN** SHALL 创建修复任务，按 old 库原版补回
- **AND** 修复后重审

### Requirement: 变小文件复核

对 4 个显著变小的文件 SHALL 复核：
- `openbench-logic-sniffer/protocol.c`（-56%）
- `openbench-logic-sniffer/protocol.h`（-31%）
- `asix-sigma/protocol.c`（-33%）
- `raspberrypi-pico/protocol.c`（-30%）

#### Scenario: 合法重构
- **WHEN** 变小原因为移除未用代码、消除冗余、compat 层接管
- **THEN** SHALL 在审计报告中标记 LEGITIMATE，附 diff 说明
- **AND** 不做修复

#### Scenario: 功能丢失
- **WHEN** 变小原因为协议分支/配置项/解析逻辑被删
- **THEN** SHALL 按 old 库原版补回
- **AND** 修复后重审

## MODIFIED Requirements

### Requirement: 迁移完成定义

`migrate-all-sigrok-drivers` spec 中"驱动编译通过"的完成定义扩展为：
1. 目录存在且文件齐全（非空）
2. CMakeLists.txt 三项注册完整（option + list + add_definitions）
3. hwdriver.c 注册完整（extern + 注册项 + 宏守卫）
4. 通过功能等价性审计（OK 或 MINOR）

## REMOVED Requirements

无删除项。
