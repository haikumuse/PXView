# 迁移所有 sigrok 设备驱动 Spec

## Why
PXView 当前仅支持 5 个驱动（demo, DSLogic, DSCope, pxlogic）和 3 个兼容驱动（fx2lafw, saleae-logic16, raspberrypi-pico）。标准 sigrok 有 87 个硬件驱动，覆盖逻辑分析仪、示波器、万用表、电源等设备类型。需要系统性地迁移剩余驱动，扩展 PXView 对第三方硬件的支持。

## What Changes
- 分批次迁移剩余 84 个标准 sigrok 驱动
- 扩展 SCPI 后端支持（示波器/电源设备）
- 扩展 Serial 后端支持（万用表/传感器设备）
- 优化驱动迁移流程（自动化脚本 + 标准模板）
- 添加驱动分类和优先级体系

## Impact
- Affected specs: 扩展 `add-sigrok-driver-compat-layer` spec
- Affected code:
  - `libsigrok/hardware/compat/compat_scpi.c/h`（新增 SCPI 后端）
  - `libsigrok/hardware/<driver>/`（84 个新驱动目录）
  - `libsigrok/hwdriver.c`（扩展驱动列表）
  - `CMakeLists.txt`（添加驱动编译选项）

## 驱动分类统计

| 类别 | 数量 | 已迁移 | 待迁移 | 设备类型 |
|---|---|---|---|---|
| **逻辑分析仪** | 15 | 3 | 12 | USB 逻辑分析仪 |
| **示波器** | 14 | 0 | 14 | SCPI/USB 示波器 |
| **万用表** | 20 | 0 | 20 | 串口/USB DMM |
| **电源/负载** | 12 | 0 | 12 | SCPI 电源、电子负载 |
| **传感器/仪表** | 10 | 0 | 10 | 温度计、湿度计、功率计等 |
| **其他设备** | 15 | 0 | 15 | 信号发生器、LCR 测试仪、继电器等 |

**总计**：87 个驱动（含 demo/dslogic），已迁移 3 个，待迁移 84 个。

## 驱动优先级矩阵

### P0 - 逻辑分析仪（高需求）

| 驱动名 | 设备型号 | 连接方式 | 优先级 | 状态 |
|---|---|---|---|---|
| `fx2lafw` | Generic FX2、Saleae Logic、Hantek 6022BL | USB | P0 | ✓ 已迁移 |
| `saleae-logic16` | Saleae Logic16 | USB | P0 | ✓ 已迁移 |
| `saleae-logic-pro` | Saleae Logic Pro 8/16 | USB | P0 | 待迁移 |
| `asix-sigma` | ASIX Sigma/Sigma2 | USB | P0 | 待迁移 |
| `chronovu-la` | ChronoVu LA8/LA16 | USB | P0 | 待迁移 |
| `kingst-la2016` | Kingst LA2016 | USB | P0 | 待迁移 |
| `openbench-logic-sniffer` | OpenBench Logic Sniffer | USB/Serial | P1 | 待迁移 |
| `ikalogic-scanalogic2` | Ikalogic Scanalogic2 | USB | P1 | 待迁移 |
| `ikalogic-scanaplus` | Ikalogic ScanAPlus | USB | P1 | 待迁移 |
| `zeroplus-logic-cube` | Zeroplus Logic Cube | USB | P1 | 待迁移 |
| `sysclk-lwla` | Sysclk LWLA1016/LWLA1034 | USB | P1 | 待迁移 |
| `sysclk-sla5032` | Sysclk SLA5032 | USB | P1 | 待迁移 |
| `ftdi-la` | FTDI logic analyzer | USB | P2 | 待迁移 |
| `lecroy-logicstudio` | LeCroy LogicStudio | USB | P2 | 待迁移 |
| `ipdbg-la` | IPDBG logic analyzer | USB | P2 | 待迁移 |
| `raspberrypi-pico` | RP2040/Pico | USB | P1 | ✓ 已迁移 |

### P1 - 示波器（高需求）

| 驱动名 | 设备型号 | 连接方式 | 优先级 | SCPI |
|---|---|---|---|---|
| `rigol-ds` | Rigol DS1000/DS2000/DS4000 | USB/SCPI | P1 | ✓ |
| `siglent-sds` | Siglent SDS1000/SDS2000 | USB/SCPI | P1 | ✓ |
| `hantek-dso` | Hantek DSO 系列 | USB | P1 | ✗ |
| `hantek-6xxx` | Hantek 6000 系列 | USB | P2 | ✗ |
| `hantek-4032l` | Hantek 4032L | USB | P2 | ✗ |
| `lecroy-xstream` | LeCroy XStream | SCPI | P2 | ✓ |
| `yokogawa-dlm` | Yokogawa DLM | SCPI | P2 | ✓ |
| `gwinstek-gds-800` | GWInstek GDS-800 | Serial | P2 | ✓ |
| `hung-chang-dso-2100` | Hung Chang DSO-2100 | USB | P3 | ✗ |
| `link-mso19` | Link MSO19 | USB | P3 | ✗ |
| `uni-t-ut181a` | UNI-T UT181A | USB | P3 | ✗ |
| `rohde-schwarz-sme-0x` | Rohde & Schwarz SME-0x | SCPI | P3 | ✓ |
| `hameg-hmo` | HAMEG HMO | SCPI | P3 | ✓ |
| `rigol-dg` | Rigol DG 信号发生器 | SCPI | P3 | ✓ |

### P2 - 万用表（中需求）

| 驱动名 | 设备型号 | 连接方式 | 优先级 |
|---|---|---|---|
| `fluke-dmm` | Fluke 18x/28x/190 系列 | Serial | P2 |
| `fluke-45` | Fluke 45 | Serial | P2 |
| `agilent-dmm` | Agilent/Keysight U123x/U125x | Serial | P2 |
| `appa-55ii` | APPA 55II | Serial | P2 |
| `uni-t-dmm` | UNI-T UT61x/UT71x | USB-HID | P2 |
| `uni-t-ut32x` | UNI-T UT32x 温度计 | USB-HID | P3 |
| `norma-dmm` | Norma DM9x/Siemens B102x | Serial | P3 |
| `gwinstek-psp` | GWInstek PSP | Serial | P3 |
| `mastech-ms6514` | Mastech MS6514 | Serial | P3 |
| `testo` | Testo 温度计 | Serial | P3 |
| `lascar-el-usb` | Lascar EL-USB 数据记录器 | USB | P3 |
| `mooshimeter-dmm` | Mooshimeter DMM | Bluetooth | P3 |
| `scpi-dmm` | Generic SCPI DMM | SCPI | P3 |
| `serial-dmm` | Generic serial DMM | Serial | P3 |
| `center-3xx` | Center 3xx | Serial | P3 |
| `mic-985xx` | MIC 985xx | Serial | P3 |
| `tondaj-sl-814` | Tondaj SL-814 | Serial | P3 |
| `kern-scale` | Kern 电子秤 | Serial | P3 |
| `teleinfo` | Teleinfo (French power meter) | Serial | P3 |
| `pce-322a` | PCE-322A 噪音计 | Serial | P3 |

### P3 - 电源/负载（低需求）

| 首动名 | 设备型号 | 连接方式 | 优先级 | SCPI |
|---|---|---|---|---|
| `rigol-dg` | Rigol DG 信号发生器 | SCPI | P3 | ✓ |
| `siglent-sdl10x0` | Siglent SDL10x0 电源 | SCPI | P3 | ✓ |
| `gwinstek-gpd` | GWInstek GPD 电源 | Serial | P3 | ✓ |
| `itech-it8500` | Itech IT8500 电子负载 | Serial | P3 | ✓ |
| `maynuo-m97` | Maynuo M97 电子负载 | Serial | P3 | ✗ |
| `korad-kaxxxxp` | Korad KAxxxxP 电源 | Serial | P3 | ✗ |
| `atten-pps3xxx` | Atten PPS3xxx 电源 | Serial | P3 | ✗ |
| `manson-hcs-3xxx` | Manson HCS-3xxx 电源 | Serial | P3 | ✗ |
| `motech-lps-30x` | Motech LPS-30x 电源 | Serial | P3 | ✗ |
| `rdtech-dps` | RDTech DPS 电源 | Serial | P3 | ✗ |
| `rdtech-tc` | RDTech TC 电源 | Serial | P3 | ✗ |
| `rdtech-um` | RDTech UM 电源 | USB | P3 | ✗ |

### P4 - 其他设备

| 驱动名 | 设备类型 | 连接方式 |
|---|---|---|
| `serial-lcr` | LCR 测试仪 | Serial |
| `juntek-jds6600` | Juntek JDS6600 信号发生器 | Serial |
| `asix-omega-rtm-cli` | ASIX Omega RTM-CLI | USB |
| `greatfet` | GreatFET | USB |
| `microchip-pickit2` | Microchip PICkit2 | USB |
| `arachnid-labs-re-load-pro` | Re:load Pro 电子负载 | USB |
| `dcttech-usbrelay` | USB 继电器 | USB |
| `icstation-usbrelay` | USB 继电器 | USB |
| `conrad-digi-35-cpu` | Conrad Digi 35 CPU | Serial |
| `baylibre-acme` | Baylibre ACME | GPIO |
| `beaglelogic` | BeagleLogic | Linux GPIO |
| `pipistrello-ols` | Pipistrello OLS | USB |
| `colead-slm` | Colead SLM 噪音计 | USB |
| `devantech-eth008` | Devantech ETH008 网络继电器 | TCP |
| `atorch` | Atorch 电源 | Serial |

## ADDED Requirements

### Requirement: 批次迁移计划
系统 SHALL 按以下批次迁移驱动，每批次包含验证测试：

#### Batch 1: 高优先级逻辑分析仪（12 个驱动）
- [ ] `saleae-logic-pro`
- [ ] `asix-sigma`
- [ ] `chronovu-la`
- [ ] `kingst-la2016`
- [ ] `openbench-logic-sniffer`
- [ ] `ikalogic-scanalogic2`
- [ ] `ikalogic-scanaplus`
- [ ] `zeroplus-logic-cube`
- [ ] `sysclk-lwla`
- [ ] `sysclk-sla5032`
- [ ] `ftdi-la`
- [ ] `lecroy-logicstudio`

#### Batch 2: 示波器驱动（14 个驱动，需 SCPI 后端）
- [ ] `rigol-ds`
- [ ] `siglent-sds`
- [ ] `hantek-dso`
- [ ] `hantek-6xxx`
- [ ] `hantek-4032l`
- [ ] `lecroy-xstream`
- [ ] `yokogawa-dlm`
- [ ] `gwinstek-gds-800`
- [ ] `hung-chang-dso-2100`
- [ ] `link-mso19`
- [ ] `uni-t-ut181a`
- [ ] `rohde-schwarz-sme-0x`
- [ ] `hameg-hmo`
- [ ] `rigol-dg`

#### Batch 3: 万用表驱动（20 个驱动，需 Serial 后端）
- [ ] `fluke-dmm`
- [ ] `fluke-45`
- [ ] `agilent-dmm`
- [ ] `appa-55ii`
- [ ] `uni-t-dmm`
- [ ] `uni-t-ut32x`
- [ ] `norma-dmm`
- [ ] `gwinstek-psp`
- [ ] `mastech-ms6514`
- [ ] `testo`
- [ ] `lascar-el-usb`
- [ ] `mooshimeter-dmm`
- [ ] `scpi-dmm`
- [ ] `serial-dmm`
- [ ] `center-3xx`
- [ ] `mic-985xx`
- [ ] `tondaj-sl-814`
- [ ] `kern-scale`
- [ ] `teleinfo`
- [ ] `pce-322a`

#### Batch 4: 电源/负载驱动（12 个驱动）
- [ ] `siglent-sdl10x0`
- [ ] `gwinstek-gpd`
- [ ] `itech-it8500`
- [ ] `maynuo-m97`
- [ ] `korad-kaxxxxp`
- [ ] `atten-pps3xxx`
- [ ] `manson-hcs-3xxx`
- [ ] `motech-lps-30x`
- [ ] `rdtech-dps`
- [ ] `rdtech-tc`
- [ ] `rdtech-um`

#### Batch 5: 其他设备（15 个驱动）
- [ ] `serial-lcr`
- [ ] `juntek-jds6600`
- [ ] `asix-omega-rtm-cli`
- [ ] `greatfet`
- [ ] `microchip-pickit2`
- [ ] `arachnid-labs-re-load-pro`
- [ ] `dcttech-usbrelay`
- [ ] `icstation-usbrelay`
- [ ] `conrad-digi-35-cpu`
- [ ] `baylibre-acme`
- [ ] `beaglelogic`
- [ ] `pipistrello-ols`
- [ ] `colead-slm`
- [ ] `devantech-eth008`
- [ ] `atorch`
- [ ] `ipdbg-la`

### Requirement: SCPI 通信后端
系统 SHALL 提供标准 sigrok 的 SCPI 通信后端，支持示波器和电源设备的 SCPI 命令通信。

#### Scenario: SCPI 设备通信
- **WHEN** SCPI 驱动（如 rigol-ds）通过 `sr_scpi_send()` 发送命令
- **THEN** 兼容层 SHALL 通过 TCP/USB/Serial SCPI 后端将命令发送到设备

### Requirement: 驱动迁移自动化工具
系统 SHALL 提供驱动迁移脚本，自动化完成：
1. 从标准 sigrok 复制驱动源码
2. 替换头文件 include
3. 生成 wrapper 函数模板
4. 生成 CMakeLists.txt 条目
5. 生成 hwdriver.c 注册代码

#### Scenario: 自动迁移脚本使用
- **WHEN** 开发者运行 `scripts/migrate_driver.py saleae-logic-pro`
- **THEN** 脚本 SHALL 自动完成驱动迁移的 80% 工作，剩余 20% 需手动调试

### Requirement: 驱动测试框架
系统 SHALL 提供驱动测试框架，验证：
1. 驱动编译通过
2. 设备扫描功能
3. 配置读写功能
4. 数据采集功能（使用 demo 数据）

#### Scenario: 驱动编译测试
- **WHEN** 新驱动添加到 CMakeLists.txt
- **THEN** 编译 SHALL 成功完成，无错误

#### Scenario: 驱动功能测试
- **WHEN** 运行 `driver_test --driver rigol-ds --mode demo`
- **THEN** 测试 SHALL 验证扫描、配置、采集流程

## MODIFIED Requirements

### Requirement: 驱动分类体系
现有驱动 SHALL 添加分类标签：
- `DRIVER_CLASS_LOGIC_ANALYZER`
- `DRIVER_CLASS_OSCILLOSCOPE`
- `DRIVER_CLASS_MULTIMETER`
- `DRIVER_CLASS_POWER_SUPPLY`
- `DRIVER_CLASS_SENSOR`

### Requirement: 设备列表 UI
PXView 设备列表 SHALL 添加过滤器，按驱动分类筛选设备。

## REMOVED Requirements

无删除项。

## 迁移工作量估算

| 批次 | 驱动数 | 工作量（小时） | 依赖项 |
|---|---|---|---|
| Batch 1 | 12 | 24 (12×2) | USB 后端 ✓ |
| Batch 2 | 14 | 42 (14×3) | SCPI 后端（新增） |
| Batch 3 | 20 | 40 (20×2) | Serial 后端 ✓ |
| Batch 4 | 12 | 36 (12×3) | SCPI 后端 ✓ |
| Batch 5 | 15 | 30 (15×2) | 多种后端 |
| **SCPI 后端** | - | 16 | 从标准 sigrok 移植 |
| **自动化工具** | - | 8 | Python 脚本开发 |
| **测试框架** | - | 12 | driver_test 工具 |
| **总计** | 84 | **208 小时** | - |

**乐观估算**：104 小时（2 人并行，2 周）
**保守估算**：208 小时（单人，5 周）