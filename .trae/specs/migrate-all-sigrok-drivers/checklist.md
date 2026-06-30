# 验证清单

## 前置基础设施

- [ ] SCPI 通信后端移植完成，支持 TCP SCPI (VXI-11)、USB SCPI (USBTMC)、Serial SCPI
- [ ] `compat_scpi.c/h` 包含 `sr_scpi_open/close/send/read` 等核心函数
- [ ] 驱动迁移自动化脚本 `scripts/migrate_driver.py` 可运行
- [ ] 自动化脚本能正确替换头文件 include
- [ ] 自动化脚本能生成 wrapper 函数模板
- [ ] 自动化脚本能生成 hwdriver.c 注册代码
- [ ] 自动化脚本能生成 CMakeLists.txt 条目
- [ ] 驱动测试框架 `tests/driver_test/` 可运行
- [ ] 测试框架能验证驱动编译
- [ ] 测试框架能验证设备扫描（demo 模式）
- [ ] 测试框架能验证配置读写

## Batch 1: 逻辑分析仪驱动（12 个）

- [ ] `saleae-logic-pro` 驱动编译通过
- [ ] `saleae-logic-pro` 驱动出现在设备列表
- [ ] `asix-sigma` 驱动编译通过
- [ ] `asix-sigma` 驱动出现在设备列表
- [ ] `chronovu-la` 驱动编译通过
- [ ] `chronovu-la` 驱动出现在设备列表
- [ ] `kingst-la2016` 驱动编译通过
- [ ] `kingst-la2016` 驱动出现在设备列表
- [ ] `openbench-logic-sniffer` 驱动编译通过
- [ ] `openbench-logic-sniffer` 驱动出现在设备列表
- [ ] `ikalogic-scanalogic2` 驱动编译通过
- [ ] `ikalogic-scanalogic2` 驱动出现在设备列表
- [ ] `ikalogic-scanaplus` 驱动编译通过
- [ ] `ikalogic-scanaplus` 驱动出现在设备列表
- [ ] `zeroplus-logic-cube` 驱动编译通过
- [ ] `zeroplus-logic-cube` 驆动出现在设备列表
- [ ] `sysclk-lwla` 驱动编译通过
- [ ] `sysclk-lwla` 驱动出现在设备列表
- [ ] `sysclk-sla5032` 驱动编译通过
- [ ] `sysclk-sla5032` 驱动出现在设备列表
- [ ] `ftdi-la` 驱动编译通过
- [ ] `ftdi-la` 驱动出现在设备列表
- [ ] `lecroy-logicstudio` 驱动编译通过
- [ ] `lecroy-logicstudio` 驱动出现在设备列表
- [ ] `ipdbg-la` 驱动编译通过
- [ ] `ipdbg-la` 驱动出现在设备列表

## Batch 2: 示波器驱动（14 个）

- [ ] `rigol-ds` 驱动编译通过（SCPI 依赖）
- [ ] `rigol-ds` 驱动出现在设备列表
- [ ] `siglent-sds` 驱动编译通过（SCPI 依赖）
- [ ] `siglent-sds` 驱动出现在设备列表
- [ ] `hantek-dso` 驱动编译通过
- [ ] `hantek-dso` 驱动出现在设备列表
- [ ] `hantek-6xxx` 驱动编译通过
- [ ] `hantek-6xxx` 驱动出现在设备列表
- [ ] `hantek-4032l` 驱动编译通过
- [ ] `hantek-4032l` 驱动出现在设备列表
- [ ] `lecroy-xstream` 驱动编译通过（SCPI 依赖）
- [ ] `lecroy-xstream` 驱动出现在设备列表
- [ ] `yokogawa-dlm` 驱动编译通过（SCPI 依赖）
- [ ] `yokogawa-dlm` 驱动出现在设备列表
- [ ] `gwinstek-gds-800` 驱动编译通过（Serial 依赖）
- [ ] `gwinstek-gds-800` 驱动出现在设备列表
- [ ] `hung-chang-dso-2100` 驱动编译通过
- [ ] `hung-chang-dso-2100` 驱动出现在设备列表
- [ ] `link-mso19` 驱动编译通过
- [ ] `link-mso19` 驱动出现在设备列表
- [ ] `uni-t-ut181a` 驱动编译通过
- [ ] `uni-t-ut181a` 驱动出现在设备列表
- [ ] `rohde-schwarz-sme-0x` 驱动编译通过（SCPI 依赖）
- [ ] `rohde-schwarz-sme-0x` 驱动出现在设备列表
- [ ] `hameg-hmo` 驱动编译通过（SCPI 依赖）
- [ ] `hameg-hmo` 驱动出现在设备列表
- [ ] `rigol-dg` 驱动编译通过（SCPI 依赖）
- [ ] `rigol-dg` 驱动出现在设备列表

## Batch 3: 万用表驱动（20 个）

- [ ] `fluke-dmm` 驱动编译通过（Serial 依赖）
- [ ] `fluke-dmm` 驱动出现在设备列表
- [ ] `fluke-45` 驱动编译通过
- [ ] `fluke-45` 驱动出现在设备列表
- [ ] `agilent-dmm` 驱动编译通过
- [ ] `agilent-dmm` 驱动出现在设备列表
- [ ] `appa-55ii` 驱动编译通过
- [ ] `appa-55ii` 驱动出现在设备列表
- [ ] `uni-t-dmm` 驱动编译通过（USB-HID 依赖）
- [ ] `uni-t-dmm` 驱动出现在设备列表
- [ ] `uni-t-ut32x` 驱动编译通过
- [ ] `uni-t-ut32x` 驱动出现在设备列表
- [ ] `norma-dmm` 驱动编译通过
- [ ] `norma-dmm` 驱动出现在设备列表
- [ ] `gwinstek-psp` 驱动编译通过
- [ ] `gwinstek-psp` 驱动出现在设备列表
- [ ] `mastech-ms6514` 驱动编译通过
- [ ] `mastech-ms6514` 驱动出现在设备列表
- [ ] `testo` 驱动编译通过
- [ ] `testo` 驱动出现在设备列表
- [ ] `lascar-el-usb` 驱动编译通过
- [ ] `lascar-el-usb` 驱动出现在设备列表
- [ ] `mooshimeter-dmm` 驱动编译通过（Bluetooth 依赖）
- [ ] `mooshimeter-dmm` 驱动出现在设备列表
- [ ] `scpi-dmm` 驱动编译通过（SCPI 依赖）
- [ ] `scpi-dmm` 驱动出现在设备列表
- [ ] `serial-dmm` 驱动编译通过
- [ ] `serial-dmm` 驱动出现在设备列表
- [ ] `center-3xx` 驱动编译通过
- [ ] `center-3xx` 驱动出现在设备列表
- [ ] `mic-985xx` 驱动编译通过
- [ ] `mic-985xx` 驱动出现在设备列表
- [ ] `tondaj-sl-814` 驱动编译通过
- [ ] `tondaj-sl-814` 驱动出现在设备列表
- [ ] `kern-scale` 驱动编译通过
- [ ] `kern-scale` 驱动出现在设备列表
- [ ] `teleinfo` 驱动编译通过
- [ ] `teleinfo` 驱动出现在设备列表
- [ ] `pce-322a` 驱动编译通过
- [ ] `pce-322a` 驱动出现在设备列表

## Batch 4: 电源/负载驱动（12 个）

- [ ] `siglent-sdl10x0` 驱动编译通过（SCPI 依赖）
- [ ] `siglent-sdl10x0` 驱动出现在设备列表
- [ ] `gwinstek-gpd` 驱动编译通过
- [ ] `gwinstek-gpd` 驱动出现在设备列表
- [ ] `itech-it8500` 驱动编译通过
- [ ] `itech-it8500` 驱动出现在设备列表
- [ ] `maynuo-m97` 驱动编译通过
- [ ] `maynuo-m97` 驱动出现在设备列表
- [ ] `korad-kaxxxxp` 驱动编译通过
- [ ] `korad-kaxxxxp` 驱动出现在设备列表
- [ ] `atten-pps3xxx` 驱动编译通过
- [ ] `atten-pps3xxx` 驱动出现在设备列表
- [ ] `manson-hcs-3xxx` 驱动编译通过
- [ ] `manson-hcs-3xxx` 驱动出现在设备列表
- [ ] `motech-lps-30x` 驱动编译通过
- [ ] `motech-lps-30x` 驱动出现在设备列表
- [ ] `rdtech-dps` 驱动编译通过
- [ ] `rdtech-dps` 驱动出现在设备列表
- [ ] `rdtech-tc` 驱动编译通过
- [ ] `rdtech-tc` 驱动出现在设备列表
- [ ] `rdtech-um` 驱动编译通过
- [ ] `rdtech-um` 驱动出现在设备列表
- [ ] `scpi-pps` 驱动编译通过（SCPI 依赖）
- [ ] `scpi-pps` 驱动出现在设备列表

## Batch 5: 其他设备驱动（15 个）

- [ ] `serial-lcr` 驱动编译通过
- [ ] `serial-lcr` 驱动出现在设备列表
- [ ] `juntek-jds6600` 驱动编译通过
- [ ] `juntek-jds6600` 驱动出现在设备列表
- [ ] `asix-omega-rtm-cli` 驱动编译通过
- [ ] `asix-omega-rtm-cli` 驱动出现在设备列表
- [ ] `greatfet` 驱动编译通过
- [ ] `greatfet` 驱动出现在设备列表
- [ ] `microchip-pickit2` 驱动编译通过
- [ ] `microchip-pickit2` 驱动出现在设备列表
- [ ] `arachnid-labs-re-load-pro` 驱动编译通过
- [ ] `arachnid-labs-re-load-pro` 驱动出现在设备列表
- [ ] `dcttech-usbrelay` 驱动编译通过
- [ ] `dcttech-usbrelay` 驱动出现在设备列表
- [ ] `icstation-usbrelay` 驱动编译通过
- [ ] `icstation-usbrelay` 驱动出现在设备列表
- [ ] `conrad-digi-35-cpu` 驱动编译通过
- [ ] `conrad-digi-35-cpu` 驱动出现在设备列表
- [ ] `baylibre-acme` 驱动编译通过（Linux only）
- [ ] `baylibre-acme` 驱动出现在设备列表（Linux）
- [ ] `beaglelogic` 驱动编译通过（Linux only）
- [ ] `beaglelogic` 驱动出现在设备列表（Linux）
- [ ] `pipistrello-ols` 驱动编译通过
- [ ] `pipistrello-ols` 驱动出现在设备列表
- [ ] `colead-slm` 驱动编译通过
- [ ] `colead-slm` 驱动出现在设备列表
- [ ] `devantech-eth008` 驱动编译通过（TCP 依赖）
- [ ] `devantech-eth008` 驱动出现在设备列表
- [ ] `atorch` 驱动编译通过
- [ ] `atorch` 驱动出现在设备列表

## 最终验证

- [ ] 所有 84 个驱动编译通过（`cmake -DENABLE_ALL_COMPAT_DRIVERS=ON`）
- [ ] PXView 设备列表显示所有驱动分类
- [ ] 驱动分类过滤器正常工作（逻辑分析仪/示波器/万用表/电源/其他）
- [ ] 驱动信息正确显示（厂商/型号/版本）
- [ ] PXView 原有 DSL 驱动功能不受影响
- [ ] 已迁移的 3 个驱动（fx2lafw/saleae-logic16/raspberrypi-pico）仍然正常工作
- [ ] AGENTS.md 更新驱动列表
- [ ] 驱动迁移指南文档完成
- [ ] 用户手册驱动章节完成

## 功能验证（可选，需实际硬件）

- [ ] 使用实际 Saleae Logic Pro 设备测试扫描/配置/采集
- [ ] 使用实际 Rigol DS 示波器测试 SCPI 通信
- [ ] 使用实际 Fluke DMM 测试 Serial 通信
- [ ] 使用实际 Siglent 电源测试 SCPI 通信