# 验证清单

## 阶段一：硬性 bug 修复

- [x] `CMakeLists.txt` 含 `option(ENABLE_DRIVER_RIGOL_DG ...)` 声明（第 615 行）
- [x] `CMakeLists.txt` 含 `if(ENABLE_DRIVER_RIGOL_DG)` 块及 `list(APPEND libsigrok_SOURCES libsigrok/hardware/rigol-dg/api.c libsigrok/hardware/rigol-dg/protocol.c)`（第 1024-1029 行）
- [x] `hwdriver.c` 含 `extern SR_PRIV struct sr_dev_driver rigol_dg_driver_info;`（第 163-165 行，子代理发现并补充）
- [ ] `rigol-dg` 启用后能通过编译（无 undefined reference to `rigol_dg_driver_info`）— 待最终编译验证
- [ ] `libsigrok/hardware/norma-dmm/` 含 api.c / protocol.c / protocol.h 三个文件
- [ ] `CMakeLists.txt` 含 norma-dmm 的 option + 源文件块 + add_definitions
- [ ] `hwdriver.c` 含 `extern struct sr_dev_driver norma_dmm_driver_info;` + 注册项 + `HAVE_DRIVER_NORMA_DMM` 守卫
- [ ] `norma-dmm` 启用后能通过编译
- [ ] `libsigrok/hardware/serial-dmm/` 含 api.c / protocol.c / protocol.h 三个文件
- [ ] serial-dmm 含全部 5 个 DMM parser 子模块（若 old 库有）
- [ ] `CMakeLists.txt` 含 serial-dmm 的 option + 源文件块 + add_definitions
- [ ] `hwdriver.c` 含 serial-dmm 的 extern + 注册项 + 守卫
- [ ] `serial-dmm` 启用后能通过编译

## 阶段二：57 个驱动功能等价性审计

### Audit-1 逻辑分析仪前半（8 个）
- [ ] fx2lafw 审计完成（OK / MINOR / MAJOR / BROKEN 标记）
- [ ] saleae-logic16 审计完成
- [ ] saleae-logic-pro 审计完成
- [ ] raspberrypi-pico 审计完成
- [ ] asix-sigma 审计完成
- [ ] chronovu-la 审计完成
- [ ] ftdi-la 审计完成
- [ ] kingst-la2016 审计完成

### Audit-2 逻辑分析仪后半（9 个）
- [ ] sysclk-lwla 审计完成（含 lwla.c / lwla1016.c / lwla1034.c）
- [ ] sysclk-sla5032 审计完成
- [ ] zeroplus-logic-cube 审计完成（含 analyzer.c / gl_usb.c）
- [ ] openbench-logic-sniffer 审计完成（含 protocol.c -56% 变小复核结论）
- [ ] ikalogic-scanalogic2 审计完成
- [ ] ikalogic-scanaplus 审计完成
- [ ] lecroy-logicstudio 审计完成
- [ ] ipdbg-la 审计完成
- [ ] sipeed-slogic-analyzer 审计完成

### Audit-3 示波器（14 个）
- [ ] rigol-ds 审计完成（含 di->context bug 修复保留）
- [ ] siglent-sds 审计完成
- [ ] hantek-dso 审计完成（含 4 个 SR_CONF 常量补充）
- [ ] hantek-6xxx 审计完成
- [ ] hantek-4032l 审计完成
- [ ] lecroy-xstream 审计完成（CMake 源列表完整）
- [ ] yokogawa-dlm 审计完成（含 protocol_wrappers.c）
- [ ] gwinstek-gds-800 审计完成
- [ ] hung-chang-dso-2100 审计完成
- [ ] link-mso19 审计完成
- [ ] uni-t-ut181a 审计完成
- [ ] rohde-schwarz-sme-0x 审计完成
- [ ] hameg-hmo 审计完成
- [ ] rigol-dg 审计完成

### Audit-4 万用表（16 个）
- [ ] fluke-dmm 审计完成（含 fluke-18x.c / fluke-28x.c / fluke-190.c）
- [ ] fluke-45 审计完成
- [ ] agilent-dmm 审计完成
- [ ] appa-55ii 审计完成
- [ ] uni-t-dmm 审计完成
- [ ] uni-t-ut32x 审计完成
- [ ] gwinstek-psp 审计完成
- [ ] mastech-ms6514 审计完成
- [ ] testo 审计完成
- [ ] lascar-el-usb 审计完成
- [ ] center-3xx 审计完成
- [ ] mic-985xx 审计完成
- [ ] tondaj-sl-814 审计完成
- [ ] kern-scale 审计完成
- [ ] teleinfo 审计完成
- [ ] pce-322a 审计完成

### Audit-5 电源/负载（11 个）
- [ ] siglent-sdl10x0 审计完成
- [ ] itech-it8500 审计完成
- [ ] maynuo-m97 审计完成
- [ ] korad-kaxxxxp 审计完成
- [ ] atten-pps3xxx 审计完成
- [ ] manson-hcs-3xxx 审计完成
- [ ] motech-lps-30x 审计完成
- [ ] rdtech-dps 审计完成
- [ ] rdtech-tc 审计完成（含 AES-256 解密 / CRC-16 本地实现）
- [ ] rdtech-um 审计完成（含 feed_queue_analog 适配）
- [ ] scpi-pps 审计完成（含 profiles.c）

### Audit-6 其他 + 变小文件复核
- [ ] pipistrello-ols 审计完成
- [ ] asix-sigma/protocol.c 变小复核完成（-33%，LEGALITIMATE 或 NEEDS_FIX）
- [ ] raspberrypi-pico/protocol.c 变小复核完成（-30%，LEGITIMATE 或 NEEDS_FIX）
- [ ] openbench-logic-sniffer/protocol.c 变小复核完成（-56%，LEGITIMATE 或 NEEDS_FIX）
- [ ] openbench-logic-sniffer/protocol.h 变小复核完成（-31%，LEGITIMATE 或 NEEDS_FIX）

## 阶段三：修复审计发现的问题

- [ ] 审计报告汇总完成（所有 MAJOR/BROKEN 驱动列出）
- [ ] 每个 MAJOR/BROKEN 驱动已按 old 库原版修复
- [ ] 修复后重新审计确认等价
- [ ] 修复后编译验证通过

## 阶段四：最终验证

- [ ] 同时启用 rigol-dg + norma-dmm + serial-dmm + 所有修复驱动，编译通过
- [ ] `cd build && ninja -j 16 && ninja install` 无错误
- [ ] PXView.exe 启动正常
- [ ] PXView 设备列表显示新增驱动
- [ ] `migrate-all-sigrok-drivers` spec 的 checklist.md 已勾选 norma-dmm / serial-dmm / rigol-dg
- [ ] 无回归（已迁移的 60 个驱动仍正常工作）
