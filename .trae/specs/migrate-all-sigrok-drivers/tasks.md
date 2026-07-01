# Tasks

## 前置任务：基础设施扩展

- [ ] Task 0: 扩展 SCPI 通信后端
  - [ ] 0.1 从标准 sigrok 移植 `src/scpi/` 到 `libsigrok/hardware/compat/compat_scpi.c/h`
  - [ ] 0.2 实现 `sr_scpi_open/close/send/read` 等核心函数
  - [ ] 0.3 支持 TCP SCPI (VXI-11)、USB SCPI (USBTMC)、Serial SCPI
  - [ ] 0.4 在 CMakeLists.txt 添加 SCPI 后端编译选项

- [ ] Task 1: 开发驱动迁移自动化工具
  - [ ] 1.1 创建 `scripts/migrate_driver.py` 脚本
  - [ ] 1.2 实现驱动源码复制功能
  - [ ] 1.3 实现头文件替换功能（`#include <libsigrok/libsigrok.h>` → `#include "compat.h"`）
  - [ ] 1.4 实现 wrapper 函数模板生成
  - [ ] 1.5 实现 hwdriver.c 注册代码生成
  - [ ] 1.6 实现 CMakeLists.txt 条目生成

- [ ] Task 2: 开发驱动测试框架
  - [ ] 2.1 创建 `tests/driver_test/` 测试工具
  - [ ] 2.2 实现驱动编译测试（cmake + ninja）
  - [ ] 2.3 实现设备扫描测试（使用 demo 模式）
  - [ ] 2.4 实现配置读写测试（采样率/深度/通道）
  - [ ] 2.5 实现数据采集测试（使用 demo 数据）

## Batch 1: 高优先级逻辑分析仪（12 个驱动）

- [ ] Task 3: 迁移 saleae-logic-pro 驱动
  - [ ] 3.1 复制驱动源码
  - [ ] 3.2 适配 wrapper 函数
  - [ ] 3.3 注册驱动
  - [ ] 3.4 编译验证
  - [ ] 3.5 功能测试

- [ ] Task 4: 迁移 asix-sigma 驱动
  - [ ] 4.1 复制驱动源码
  - [ ] 4.2 适配 wrapper 函数
  - [ ] 4.3 注册驱动
  - [ ] 4.4 编译验证
  - [ ] 4.5 功能测试

- [ ] Task 5: 迁移 chronovu-la 驱动
  - [ ] 5.1 复制驱动源码
  - [ ] 5.2 适配 wrapper 函数
  - [ ] 5.3 注册驱动
  - [ ] 5.4 编译验证
  - [ ] 5.5 功能测试

- [ ] Task 6: 迁移 kingst-la2016 驱动
  - [ ] 6.1 复制驱动源码
  - [ ] 6.2 适配 wrapper 函数
  - [ ] 6.3 注册驱动
  - [ ] 6.4 编译验证
  - [ ] 6.5 功能测试

- [ ] Task 7: 迁移 openbench-logic-sniffer 驱动
  - [ ] 7.1 复制驱动源码
  - [ ] 7.2 适配 wrapper 函数
  - [ ] 7.3 注册驱动
  - [ ] 7.4 编译验证
  - [ ] 7.5 功能测试

- [ ] Task 8: 迁移 ikalogic-scanalogic2 驱动
  - [ ] 8.1 复制驱动源码
  - [ ] 8.2 适配 wrapper 函数
  - [ ] 8.3 注册驱动
  - [ ] 8.4 编译验证
  - [ ] 8.5 功能测试

- [ ] Task 9: 迁移 ikalogic-scanaplus 驱动
  - [ ] 9.1 复制驱动源码
  - [ ] 9.2 适配 wrapper 函数
  - [ ] 9.3 注册驱动
  - [ ] 9.4 编译验证
  - [ ] 9.5 功能测试

- [ ] Task 10: 迁移 zeroplus-logic-cube 驱动
  - [ ] 10.1 复制驱动源码
  - [ ] 10.2 适配 wrapper 函数
  - [ ] 10.3 注册驱动
  - [ ] 10.4 编译验证
  - [ ] 10.5 功能测试

- [ ] Task 11: 迁移 sysclk-lwla 驱动
  - [ ] 11.1 复制驱动源码
  - [ ] 11.2 适配 wrapper 函数
  - [ ] 11.3 注册驱动
  - [ ] 11.4 编译验证
  - [ ] 11.5 功能测试

- [ ] Task 12: 迁移 sysclk-sla5032 驱动
  - [ ] 12.1 复制驱动源码
  - [ ] 12.2 适配 wrapper 函数
  - [ ] 12.3 注册驱动
  - [ ] 12.4 编译验证
  - [ ] 12.5 功能测试

- [ ] Task 13: 迁移 ftdi-la 驱动
  - [ ] 13.1 复制驱动源码
  - [ ] 13.2 适配 wrapper 函数
  - [ ] 13.3 注册驱动
  - [ ] 13.4 编译验证
  - [ ] 13.5 功能测试

- [ ] Task 14: 迁移 lecroy-logicstudio 驱动
  - [ ] 14.1 复制驱动源码
  - [ ] 14.2 适配 wrapper 函数
  - [ ] 14.3 注册驱动
  - [ ] 14.4 编译验证
  - [ ] 14.5 功能测试

- [ ] Task 15: 迁移 ipdbg-la 驱动
  - [ ] 15.1 复制驱动源码
  - [ ] 15.2 适配 wrapper 函数
  - [ ] 15.3 注册驱动
  - [ ] 15.4 编译验证
  - [ ] 15.5 功能测试

## Batch 2: 示波器驱动（14 个驱动，依赖 SCPI 后端）

- [ ] Task 16: 迁移 rigol-ds 驱动
  - [ ] 16.1 复制驱动源码
  - [ ] 16.2 适配 SCPI 通信层
  - [ ] 16.3 适配 wrapper 函数
  - [ ] 16.4 注册驱动
  - [ ] 16.5 编译验证
  - [ ] 16.6 SCPI 通信测试

- [ ] Task 17: 迁移 siglent-sds 驱动
  - [ ] 17.1 复制驱动源码
  - [ ] 17.2 适配 SCPI 通信层
  - [ ] 17.3 适配 wrapper 函数
  - [ ] 17.4 注册驱动
  - [ ] 17.5 编译验证
  - [ ] 17.6 SCPI 通信测试

- [ ] Task 18: 迁移 hantek-dso 驱动
  - [ ] 18.1 复制驱动源码
  - [ ] 18.2 适配 wrapper 函数
  - [ ] 18.3 注册驱动
  - [ ] 18.4 编译验证
  - [ ] 18.5 USB 通信测试

- [ ] Task 19: 迁移 hantek-6xxx 驱动
  - [ ] 19.1 复制驱动源码
  - [ ] 19.2 适配 wrapper 函数
  - [ ] 19.3 注册驱动
  - [ ] 19.4 编译验证
  - [ ] 19.5 USB 通信测试

- [ ] Task 20: 迁移 hantek-4032l 驱动
  - [ ] 20.1 复制驱动源码
  - [ ] 20.2 适配 wrapper 函数
  - [ ] 20.3 注册驱动
  - [ ] 20.4 编译验证
  - [ ] 20.5 USB 通信测试

- [ ] Task 21: 迁移 lecroy-xstream 驱动
  - [ ] 21.1 复制驱动源码
  - [ ] 21.2 适配 SCPI 通信层
  - [ ] 21.3 适配 wrapper 函数
  - [ ] 21.4 注册驱动
  - [ ] 21.5 编译验证
  - [ ] 21.6 SCPI 通信测试

- [ ] Task 22: 迁移 yokogawa-dlm 驱动
  - [ ] 22.1 复制驱动源码
  - [ ] 22.2 适配 SCPI 通信层
  - [ ] 22.3 适配 wrapper 函数
  - [ ] 22.4 注册驱动
  - [ ] 22.5 编译验证
  - [ ] 22.6 SCPI 通信测试

- [ ] Task 23: 迁移 gwinstek-gds-800 驱动
  - [ ] 23.1 复制驱动源码
  - [ ] 23.2 适配 Serial 通信层
  - [ ] 23.3 适配 wrapper 函数
  - [ ] 23.4 注册驱动
  - [ ] 23.5 编译验证
  - [ ] 23.6 Serial 通信测试

- [ ] Task 24: 迁移 hung-chang-dso-2100 驱动
  - [ ] 24.1 复制驱动源码
  - [ ] 24.2 适配 wrapper 函数
  - [ ] 24.3 注册驱动
  - [ ] 24.4 编译验证
  - [ ] 24.5 USB 通信测试

- [ ] Task 25: 迁移 link-mso19 驱动
  - [ ] 25.1 复制驱动源码
  - [ ] 25.2 适配 wrapper 函数
  - [ ] 25.3 注册驱动
  - [ ] 25.4 编译验证
  - [ ] 25.5 USB 通信测试

- [ ] Task 26: 迁移 uni-t-ut181a 驱动
  - [ ] 26.1 复制驱动源码
  - [ ] 26.2 适配 wrapper 函数
  - [ ] 26.3 注册驱动
  - [ ] 26.4 编译验证
  - [ ] 26.5 USB 通信测试

- [ ] Task 27: 迁移 rohde-schwarz-sme-0x 驱动
  - [ ] 27.1 复制驱动源码
  - [ ] 27.2 适配 SCPI 通信层
  - [ ] 27.3 适配 wrapper 函数
  - [ ] 27.4 注册驱动
  - [ ] 27.5 编译验证
  - [ ] 27.6 SCPI 通信测试

- [ ] Task 28: 迁移 hameg-hmo 驱动
  - [ ] 28.1 复制驱动源码
  - [ ] 28.2 适配 SCPI 通信层
  - [ ] 28.3 适配 wrapper 函数
  - [ ] 28.4 注册驱动
  - [ ] 28.5 编译验证
  - [ ] 28.6 SCPI 通信测试

- [ ] Task 29: 迁移 rigol-dg 驱动（信号发生器）
  - [ ] 29.1 复制驱动源码
  - [ ] 29.2 适配 SCPI 通信层
  - [ ] 29.3 适配 wrapper 函数
  - [ ] 29.4 注册驱动
  - [ ] 29.5 编译验证
  - [ ] 29.6 SCPI 通信测试

## Batch 3: 万用表驱动（20 个驱动）

- [ ] Task 30: 迁移 fluke-dmm 驱动
  - [ ] 30.1 复制驱动源码
  - [ ] 30.2 适配 Serial 通信层
  - [ ] 30.3 适配 wrapper 函数
  - [ ] 30.4 注册驱动
  - [ ] 30.5 编译验证
  - [ ] 30.6 Serial 通信测试

- [ ] Task 31: 迁移 fluke-45 驱动
  - [ ] 31.1 复制驱动源码
  - [ ] 31.2 适配 Serial 通信层
  - [ ] 31.3 适配 wrapper 函数
  - [ ] 31.4 注册驱动
  - [ ] 31.5 编译验证
  - [ ] 31.6 Serial 通信测试

- [ ] Task 32: 迁移 agilent-dmm 驱动
  - [ ] 32.1 复制驱动源码
  - [ ] 32.2 适配 Serial 通信层
  - [ ] 32.3 适配 wrapper 函数
  - [ ] 32.4 注册驱动
  - [ ] 32.5 编译验证
  - [ ] 32.6 Serial 通信测试

- [ ] Task 33: 迁移 appa-55ii 驱动
  - [ ] 33.1 复制驱动源码
  - [ ] 33.2 适配 Serial 通信层
  - [ ] 33.3 适配 wrapper 函数
  - [ ] 33.4 注册驱动
  - [ ] 33.5 编译验证
  - [ ] 33.6 Serial 通信测试

- [ ] Task 34: 迁移 uni-t-dmm 驱动
  - [ ] 34.1 复制驱动源码
  - [ ] 34.2 适配 USB-HID 通信层（需新增 compat_hid.c）
  - [ ] 34.3 适配 wrapper 函数
  - [ ] 34.4 注册驱动
  - [ ] 34.5 编译验证
  - [ ] 34.6 USB-HID 通信测试

- [ ] Task 35: 迁移 uni-t-ut32x 驱动（温度计）
  - [ ] 35.1 复制驱动源码
  - [ ] 35.2 适配 USB-HID 通信层
  - [ ] 35.3 适配 wrapper 函数
  - [ ] 35.4 注册驱动
  - [ ] 35.5 编译验证
  - [ ] 35.6 USB-HID 通信测试

- [ ] Task 36: 迁移 norma-dmm 驱动
  - [ ] 36.1 复制驱动源码
  - [ ] 36.2 适配 Serial 通信层
  - [ ] 36.3 适配 wrapper 函数
  - [ ] 36.4 注册驱动
  - [ ] 36.5 编译验证
  - [ ] 36.6 Serial 通信测试

- [ ] Task 37: 迁移 gwinstek-psp 驱动
  - [ ] 37.1 复制驱动源码
  - [ ] 37.2 适配 Serial 通信层
  - [ ] 37.3 适配 wrapper 函数
  - [ ] 37.4 注册驱动
  - [ ] 37.5 编译验证
  - [ ] 37.6 Serial 通信测试

- [ ] Task 38: 迁移 mastech-ms6514 驱动
  - [ ] 38.1 复制驱动源码
  - [ ] 38.2 适配 Serial 通信层
  - [ ] 38.3 适配 wrapper 函数
  - [ ] 38.4 注册驱动
  - [ ] 38.5 编译验证
  - [ ] 38.6 Serial 通信测试

- [ ] Task 39: 迁移 testo 驱动（温度计）
  - [ ] 39.1 复制驱动源码
  - [ ] 39.2 适配 Serial 通信层
  - [ ] 39.3 适配 wrapper 函数
  - [ ] 39.4 注册驱动
  - [ ] 39.5 编译验证
  - [ ] 39.6 Serial 通信测试

- [ ] Task 40: 迁移 lascar-el-usb 驱动（数据记录器）
  - [ ] 40.1 复制驱动源码
  - [ ] 40.2 适配 USB 通信层
  - [ ] 40.3 适配 wrapper 函数
  - [ ] 40.4 注册驱动
  - [ ] 40.5 编译验证
  - [ ] 40.6 USB 通信测试

- [ ] Task 41: 迁移 mooshimeter-dmm 驱动（蓝牙 DMM）
  - [ ] 41.1 复制驱动源码
  - [ ] 41.2 适配 Bluetooth 通信层（需新增 compat_bt.c）
  - [ ] 41.3 适配 wrapper 函数
  - [ ] 41.4 注册驱动
  - [ ] 41.5 编译验证
  - [ ] 41.6 Bluetooth 通信测试

- [ ] Task 42: 迁移 scpi-dmm 驱动（通用 SCPI DMM）
  - [x] 42.1 复制驱动源码（protocol.h/protocol.c/api.c 已创建于 libsigrok/hardware/scpi-dmm/）
  - [x] 42.2 适配 SCPI 通信层（sr_scpi_scan 用 di->priv、sr_scpi_source_add 保持 sdi->session 参数）
  - [x] 42.3 适配 wrapper 函数（8 个 compat 包装函数齐全：init/cleanup/scan/config_get/config_set/config_list/acquisition_start/acquisition_stop；driver_info 字段补全 driver_type/dev_mode_list/dev_destroy/dev_status_get/priv=NULL；移除 SR_REGISTER_DEV_DRIVER；扁平 sr_datafeed_analog 适配；本地 sr_sw_limits + sr_atoi/sr_atod_ascii/sr_atod_ascii_digits 实现；SR_CONF/SR_MQ/SR_MQFLAG 宏守卫；14 条转换规则全部应用并通过 grep 验证）
  - [ ] 42.4 注册驱动（用户明确要求暂不修改 CMakeLists.txt/hwdriver.c，待后续批量注册）
  - [ ] 42.5 编译验证（依赖 42.4）
  - [ ] 42.6 SCPI 通信测试（依赖实际硬件）

- [ ] Task 43: 迁移 serial-dmm 驱动（通用 Serial DMM）
  - [ ] 43.1 复制驱动源码
  - [ ] 43.2 适配 Serial 通信层
  - [ ] 43.3 适配 wrapper 函数
  - [ ] 43.4 注册驱动
  - [ ] 43.5 编译验证
  - [ ] 43.6 Serial 通信测试

- [ ] Task 44: 迁移 center-3xx 驱动
  - [ ] 44.1 复制驱动源码
  - [ ] 44.2 适配 Serial 通信层
  - [ ] 44.3 适配 wrapper 函数
  - [ ] 44.4 注册驱动
  - [ ] 44.5 编译验证
  - [ ] 44.6 Serial 通信测试

- [ ] Task 45: 迁移 mic-985xx 驱动
  - [ ] 45.1 复制驱动源码
  - [ ] 45.2 适配 Serial 通信层
  - [ ] 45.3 适配 wrapper 函数
  - [ ] 45.4 注册驱动
  - [ ] 45.5 编译验证
  - [ ] 45.6 Serial 通信测试

- [ ] Task 46: 迁移 tondaj-sl-814 驱动
  - [ ] 46.1 复制驱动源码
  - [ ] 46.2 适配 Serial 通信层
  - [ ] 46.3 适配 wrapper 函数
  - [ ] 46.4 注册驱动
  - [ ] 46.5 编译验证
  - [ ] 46.6 Serial 通信测试

- [ ] Task 47: 迁移 kern-scale 驱动（电子秤）
  - [ ] 47.1 复制驱动源码
  - [ ] 47.2 适配 Serial 通信层
  - [ ] 47.3 适配 wrapper 函数
  - [ ] 47.4 注册驱动
  - [ ] 47.5 编译验证
  - [ ] 47.6 Serial 通信测试

- [ ] Task 48: 迁移 teleinfo 驱动（法国功率表）
  - [ ] 48.1 复制驱动源码
  - [ ] 48.2 适配 Serial 通信层
  - [ ] 48.3 适配 wrapper 函数
  - [ ] 48.4 注册驱动
  - [ ] 48.5 编译验证
  - [ ] 48.6 Serial 通信测试

- [ ] Task 49: 迁移 pce-322a 驱动（噪音计）
  - [ ] 49.1 复制驱动源码
  - [ ] 49.2 适配 Serial 通信层
  - [ ] 49.3 适配 wrapper 函数
  - [ ] 49.4 注册驱动
  - [ ] 49.5 编译验证
  - [ ] 49.6 Serial 通信测试

## Batch 4: 电源/负载驱动（12 个驱动）

- [ ] Task 50: 迁移 siglent-sdl10x0 驱动（电源）
  - [ ] 50.1 复制驱动源码
  - [ ] 50.2 适配 SCPI 通信层
  - [ ] 50.3 适配 wrapper 函数
  - [ ] 50.4 注册驱动
  - [ ] 50.5 编译验证
  - [ ] 50.6 SCPI 通信测试

- [ ] Task 51: 迁移 gwinstek-gpd 驱动（电源）
  - [ ] 51.1 复制驱动源码
  - [ ] 51.2 适配 Serial 通信层
  - [ ] 51.3 适配 wrapper 函数
  - [ ] 51.4 注册驱动
  - [ ] 51.5 编译验证
  - [ ] 51.6 Serial 通信测试

- [ ] Task 52: 迁移 itech-it8500 驱动（电子负载）
  - [ ] 52.1 复制驱动源码
  - [ ] 52.2 适配 Serial 通信层
  - [ ] 52.3 适配 wrapper 函数
  - [ ] 52.4 注册驱动
  - [ ] 52.5 编译验证
  - [ ] 52.6 Serial 通信测试

- [ ] Task 53: 迁移 maynuo-m97 驱动（电子负载）
  - [ ] 53.1 复制驱动源码
  - [ ] 53.2 适配 Serial 通信层
  - [ ] 53.3 适配 wrapper 函数
  - [ ] 53.4 注册驱动
  - [ ] 53.5 编译验证
  - [ ] 53.6 Serial 通信测试

- [ ] Task 54: 迁移 korad-kaxxxxp 驱动（电源）
  - [ ] 54.1 复制驱动源码
  - [ ] 54.2 适配 Serial 通信层
  - [ ] 54.3 适配 wrapper 函数
  - [ ] 54.4 注册驱动
  - [ ] 54.5 编译验证
  - [ ] 54.6 Serial 通信测试

- [ ] Task 55: 迁移 atten-pps3xxx 驱动（电源）
  - [ ] 55.1 复制驱动源码
  - [ ] 55.2 适配 Serial 通信层
  - [ ] 55.3 适配 wrapper 函数
  - [ ] 55.4 注册驱动
  - [ ] 55.5 编译验证
  - [ ] 55.6 Serial 通信测试

- [ ] Task 56: 迁移 manson-hcs-3xxx 驱动（电源）
  - [ ] 56.1 复制驱动源码
  - [ ] 56.2 适配 Serial 通信层
  - [ ] 56.3 适配 wrapper 函数
  - [ ] 56.4 注册驱动
  - [ ] 56.5 编译验证
  - [ ] 56.6 Serial 通信测试

- [ ] Task 57: 迁移 motech-lps-30x 驱动（电源）
  - [ ] 57.1 复制驱动源码
  - [ ] 57.2 适配 Serial 通信层
  - [ ] 57.3 适配 wrapper 函数
  - [ ] 57.4 注册驱动
  - [ ] 57.5 编译验证
  - [ ] 57.6 Serial 通信测试

- [ ] Task 58: 迁移 rdtech-dps 驱动（电源）
  - [ ] 58.1 复制驱动源码
  - [ ] 58.2 适配 Serial 通信层
  - [ ] 58.3 适配 wrapper 函数
  - [ ] 58.4 注册驱动
  - [ ] 58.5 编译验证
  - [ ] 58.6 Serial 通信测试

- [ ] Task 59: 迁移 rdtech-tc 驱动（电源）
  - [ ] 59.1 复制驱动源码
  - [ ] 59.2 适配 Serial 通信层
  - [ ] 59.3 适配 wrapper 函数
  - [ ] 59.4 注册驱动
  - [ ] 59.5 编译验证
  - [ ] 59.6 Serial 通信测试

- [ ] Task 60: 迁移 rdtech-um 驱动（电源）
  - [ ] 60.1 复制驱动源码
  - [ ] 60.2 适配 USB 通信层
  - [ ] 60.3 适配 wrapper 函数
  - [ ] 60.4 注册驱动
  - [ ] 60.5 编译验证
  - [ ] 60.6 USB 通信测试

- [ ] Task 61: 迁移 scpi-pps 驱动（通用 SCPI 电源）
  - [ ] 61.1 复制驱动源码
  - [ ] 61.2 适配 SCPI 通信层
  - [ ] 61.3 适配 wrapper 函数
  - [ ] 61.4 注册驱动
  - [ ] 61.5 编译验证
  - [ ] 61.6 SCPI 通信测试

## Batch 5: 其他设备（15 个驱动）

- [ ] Task 62: 迁移 serial-lcr 驱动（LCR 测试仪）
  - [ ] 62.1 复制驱动源码
  - [ ] 62.2 适配 Serial 通信层
  - [ ] 62.3 适配 wrapper 函数
  - [ ] 62.4 注册驱动
  - [ ] 62.5 编译验证
  - [ ] 62.6 Serial 通信测试

- [ ] Task 63: 迁移 juntek-jds6600 驱动（信号发生器）
  - [ ] 63.1 复制驱动源码
  - [ ] 63.2 适配 Serial 通信层
  - [ ] 63.3 适配 wrapper 函数
  - [ ] 63.4 注册驱动
  - [ ] 63.5 编译验证
  - [ ] 63.6 Serial 通信测试

- [ ] Task 64: 迁移 asix-omega-rtm-cli 驱动
  - [ ] 64.1 复制驱动源码
  - [ ] 64.2 适配 USB 通信层
  - [ ] 64.3 适配 wrapper 函数
  - [ ] 64.4 注册驱动
  - [ ] 64.5 编译验证
  - [ ] 64.6 USB 通信测试

- [ ] Task 65: 迁移 greatfet 驱动
  - [ ] 65.1 复制驱动源码
  - [ ] 65.2 适配 USB 通信层
  - [ ] 65.3 适配 wrapper 函数
  - [ ] 65.4 注册驱动
  - [ ] 65.5 编译验证
  - [ ] 65.6 USB 通信测试

- [ ] Task 66: 迁移 microchip-pickit2 驱动
  - [ ] 66.1 复制驱动源码
  - [ ] 66.2 适配 USB 通信层
  - [ ] 66.3 适配 wrapper 函数
  - [ ] 66.4 注册驱动
  - [ ] 66.5 编译验证
  - [ ] 66.6 USB 通信测试

- [ ] Task 67: 迁移 arachnid-labs-re-load-pro 驱动（电子负载）
  - [ ] 67.1 复制驱动源码
  - [ ] 67.2 适配 USB 通信层
  - [ ] 67.3 适配 wrapper 函数
  - [ ] 67.4 注册驱动
  - [ ] 67.5 编译验证
  - [ ] 67.6 USB 通信测试

- [ ] Task 68: 迁移 dcttech-usbrelay 驱动（USB 继电器）
  - [ ] 68.1 复制驱动源码
  - [ ] 68.2 适配 USB 通信层
  - [ ] 68.3 适配 wrapper 函数
  - [ ] 68.4 注册驱动
  - [ ] 68.5 编译验证
  - [ ] 68.6 USB 通信测试

- [ ] Task 69: 迁移 icstation-usbrelay 驱动（USB 继电器）
  - [ ] 69.1 复制驱动源码
  - [ ] 69.2 适配 USB 通信层
  - [ ] 69.3 适配 wrapper 函数
  - [ ] 69.4 注册驱动
  - [ ] 69.5 编译验证
  - [ ] 69.6 USB 通信测试

- [ ] Task 70: 迁移 conrad-digi-35-cpu 驱动
  - [ ] 70.1 复制驱动源码
  - [ ] 70.2 适配 Serial 通信层
  - [ ] 70.3 适配 wrapper 函数
  - [ ] 70.4 注册驱动
  - [ ] 70.5 编译验证
  - [ ] 70.6 Serial 通信测试

- [ ] Task 71: 迁移 baylibre-acme 驱动（GPIO）
  - [ ] 71.1 复制驱动源码
  - [ ] 71.2 适配 GPIO 通信层（Linux only）
  - [ ] 71.3 适配 wrapper 函数
  - [ ] 71.4 注册驱动
  - [ ] 71.5 编译验证（Linux）
  - [ ] 71.6 GPIO 通信测试

- [ ] Task 72: 迁移 beaglelogic 驱动（Linux GPIO）
  - [ ] 72.1 复制驱动源码
  - [ ] 72.2 适配 Linux GPIO 通信层
  - [ ] 72.3 适配 wrapper 函数
  - [ ] 72.4 注册驱动
  - [ ] 72.5 编译验证（Linux）
  - [ ] 72.6 GPIO 通信测试

- [ ] Task 73: 迁移 pipistrello-ols 驱动
  - [ ] 73.1 复制驱动源码
  - [ ] 73.2 适配 USB 通信层
  - [ ] 73.3 适配 wrapper 函数
  - [ ] 73.4 注册驱动
  - [ ] 73.5 编译验证
  - [ ] 73.6 USB 通信测试

- [ ] Task 74: 迁移 colead-slm 驱动（噪音计）
  - [ ] 74.1 复制驱动源码
  - [ ] 74.2 适配 USB 通信层
  - [ ] 74.3 适配 wrapper 函数
  - [ ] 74.4 注册驱动
  - [ ] 74.5 编译验证
  - [ ] 74.6 USB 通信测试

- [ ] Task 75: 迁移 devantech-eth008 驱动（网络继电器）
  - [ ] 75.1 复制驱动源码
  - [ ] 75.2 适配 TCP 通信层（需新增 compat_tcp.c）
  - [ ] 75.3 适配 wrapper 函数
  - [ ] 75.4 注册驱动
  - [ ] 75.5 编译验证
  - [ ] 75.6 TCP 通信测试

- [ ] Task 76: 迁移 atorch 驱动（电源）
  - [ ] 76.1 复制驱动源码
  - [ ] 76.2 适配 Serial 通信层
  - [ ] 76.3 适配 wrapper 函数
  - [ ] 76.4 注册驱动
  - [ ] 76.5 编译验证
  - [ ] 76.6 Serial 通信测试

## 最终验证任务

- [ ] Task 77: 验证所有驱动编译通过
  - [ ] 77.1 运行 `cmake -DENABLE_ALL_COMPAT_DRIVERS=ON`
  - [ ] 77.2 运行 `ninja -j 16`
  - [ ] 77.3 检查编译错误和警告
  - [ ] 77.4 修复所有编译问题

- [ ] Task 78: 验证驱动分类和设备列表
  - [ ] 78.1 运行 PXView 查看设备列表
  - [ ] 78.2 验证驱动分类过滤器
  - [ ] 78.3 验证驱动信息显示

- [ ] Task 79: 验证驱动功能（使用 demo 模式）
  - [ ] 79.1 运行 `driver_test --all --mode demo`
  - [ ] 79.2 验证扫描、配置、采集流程
  - [ ] 79.3 验证数据正确性

- [ ] Task 80: 文档和总结
  - [ ] 80.1 更新 AGENTS.md 驱动列表
  - [ ] 80.2 编写驱动迁移指南
  - [ ] 80.3 编写用户手册驱动章节
  - [ ] 80.4 总结迁移经验和最佳实践

# Task Dependencies

- [Task 3-15] depends on [Task 1, Task 2]（自动化工具和测试框架）
- [Task 16-29] depends on [Task 0, Task 1, Task 2]（SCPI 后端）
- [Task 30-49] depends on [Task 1, Task 2]（Serial 后端已在 add-sigrok-driver-compat-layer 完成）
- [Task 50-61] depends on [Task 0, Task 1, Task 2]
- [Task 62-76] depends on [Task 1, Task 2, Task 0]（部分依赖 SCPI/TCP）
- [Task 77-80] depends on [Task 3-76]

# 并行策略

- **前置阶段**：Task 0/1/2 可并行
- **Batch 1-5**：批次内驱动可并行迁移（使用自动化脚本）
- **验证阶段**：Task 77/78/79 可并行