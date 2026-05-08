# Python 解码器重写为 C 语言计划

## 当前状态总结

### 已完成的 C 解码器（14个，已注册到 CMakeLists.txt）
| 解码器 | 类型 | 状态 |
|--------|------|------|
| spi_c | 完整协议 | ✅ 已完成 |
| i2c_c | 完整协议 | ✅ 已完成 |
| uart_c | 完整协议 | ✅ 已完成 |
| can_c | 完整协议 | ✅ 已完成 |
| jtag_c | 完整协议 | ✅ 已完成 |
| swd_c | 完整协议 | ✅ 已完成 |
| onewire_c | 完整协议 | ✅ 已完成 |
| i2s_c | 完整协议 | ✅ 已完成 |
| lin_c | 完整协议 | ✅ 代码已写，未注册到CMake |
| hdlc_c | 完整协议 | ✅ 代码已写，未注册到CMake |
| microwire_c | 完整协议 | ✅ 代码已写，未注册到CMake |
| mdio_c | 完整协议 | ✅ 代码已写，未注册到CMake |
| ps2_c | 完整协议 | ✅ 代码已写，未注册到CMake |
| dmx512_c | 完整协议 | ✅ 代码已写，未注册到CMake |

**⚠️ 重要：CMakeLists.txt 中 C_DECODERS 列表仅包含前8个，后6个（lin_c, hdlc_c, microwire_c, mdio_c, ps2_c, dmx512_c）需要添加进去。**

### Stub C 解码器（6个，仅框架）
| 解码器 | 状态 |
|--------|------|
| pwm_c | ✅ Stub |
| counter_c | ✅ Stub |
| graycode_c | ✅ Stub |
| numbers_and_state_c | ✅ Stub |
| seven_segment_c | ✅ Stub |
| ds1307_c | ✅ Stub |
| ds3231_c | ✅ Stub |
| lm75_c | ✅ Stub |

---

## 待重写的 Python 解码器清单

### 第一优先级：`inputs = ['logic']` 的底层协议解码器（直接处理原始信号，性能收益最大）

| # | 解码器名 | 协议描述 | 复杂度 | 备注 |
|---|----------|----------|--------|------|
| 1 | can-fd | CAN FD 协议 | ★★★★ | CAN扩展，高优先级 |
| 2 | usb_signalling | USB 信号层 | ★★★★ | USB底层，使用广泛 |
| 3 | usb_power_delivery | USB PD | ★★★★ | USB供电协议 |
| 4 | ethernet | 以太网 (需4b5b) | ★★★★ | 需先完成4b5b |
| 5 | lpc | Low Pin Count | ★★★ | PC总线 |
| 6 | parallel | 并口 | ★★ | 简单 |
| 7 | spdif | S/PDIF音频 | ★★★ | 数字音频 |
| 8 | spacewire | SpaceWire | ★★★ | 航天通信 |
| 9 | cec | HDMI CEC | ★★★ | 消费电子控制 |
| 10 | nrzi | NRZI编码 | ★★ | 编码层，简单 |
| 11 | 4b5b | 4B/5B编码 | ★★ | 编码层，ethernet依赖 |
| 12 | iso7816 | 智能卡 | ★★★ | 智能卡协议 |
| 13 | iebus | IEBus | ★★★ | 汽车总线 |
| 14 | dcf77 | DCF77 时钟 | ★★ | 无线时钟 |
| 15 | dali | DALI 照明 | ★★ | 照明控制 |
| 16 | dcc | DCC 模型火车 | ★★ | 模型火车控制 |
| 17 | ir_nec | NEC红外 | ★★ | 红外遥控 |
| 18 | ir_rc5 | RC5红外 | ★★ | 红外遥控 |
| 19 | ir_sirc | SIRC红外 | ★★ | 红外遥控 |
| 20 | opentherm | OpenTherm | ★★ | 暖通控制 |
| 21 | sdcard_sd | SD卡(SD模式) | ★★★ | 存储协议 |
| 22 | sdio | SDIO | ★★★ | SD输入输出 |
| 23 | qspi | QSPI | ★★★ | 四线SPI |
| 24 | spi_dual_quad | Dual/Quad SPI | ★★★ | SPI变体 |
| 25 | ac97 | AC97音频 | ★★★ | 音频编解码 |
| 26 | tmc | TMC步进驱动 | ★★★ | 步进电机 |
| 27 | sent | SENT传感器 | ★★★ | 汽车传感器 |
| 28 | z80 | Z80 CPU | ★★★★ | CPU总线 |
| 29 | mcs48 | MCS-48 CPU | ★★★ | CPU总线 |
| 30 | flexray | FlexRay | ★★★★ | 汽车总线 |
| 31 | mipi_rffe | MIPI RFFE | ★★★ | 射频前端 |
| 32 | c2 | C2协议 | ★★ | 调试接口 |
| 33 | avr_pdi | AVR PDI | ★★★ | 调试接口 |
| 34 | cjtag | cJTAG | ★★★ | JTAG变体 |
| 35 | swim | SWIM | ★★ | STM8调试 |
| 36 | rvswd | RISC-V SWD | ★★★ | RISC-V调试 |
| 37 | wiegand | Wiegand | ★★ | 门禁协议 |
| 38 | usb_signalling | USB信号 | ★★★★ | USB底层 |
| 39 | emmc_sd | eMMC | ★★★★ | 嵌入式存储 |
| 40 | sdq | SDQ | ★★ | Apple协议 |
| 41 | tdm_audio | TDM音频 | ★★ | 音频 |
| 42 | guess_bitrate | 猜测比特率 | ★★ | 工具类 |
| 43 | jitter | 抖动测量 | ★★ | 工具类 |
| 44 | timing | 时序分析 | ★★ | 工具类 |
| 45 | signature | 信号签名 | ★★ | 工具类 |
| 46 | rpm | RPM测量 | ★★ | 工具类 |
| 47 | bean | BEAN协议 | ★★ | 汽车协议 |
| 48 | maple_bus | Maple总线 | ★★ | 游戏机 |
| 49 | sda2506 | SDA2506 | ★★ | 传感器 |
| 50 | stepper_motor | 步进电机 | ★★ | 简单 |
| 51 | rgb_led_ws281x | WS281x LED | ★★ | LED驱动 |
| 52 | pxx1 | PXX1 RC | ★★ | 遥控协议 |
| 53 | qi | Qi无线充电 | ★★★ | 无线充电 |
| 54 | st7735 | ST7735 LCD | ★★ | LCD驱动 |
| 55 | st7789 | ST7789 LCD | ★★ | LCD驱动 |
| 56 | sae_j1850_vpw | SAE J1850 VPW | ★★★ | 汽车总线 |
| 57 | morse | 莫尔斯电码 | ★★ | 简单 |
| 58 | afsk | AFSK | ★★ | 音频频移键控 |
| 59 | delta-sigma | Delta-Sigma | ★★ | ADC |
| 60 | miller | Miller编码 | ★★ | 编码层 |
| 61 | caliper | 卡尺 | ★★ | 简单 |
| 62 | carrera | Carrera | ★★ | 简单 |
| 63 | ccd | CCD | ★★ | 简单 |
| 64 | aud | AUD | ★★ | 简单 |
| 65 | adat | ADAT | ★★ | 音频 |
| 66 | adb | Apple Desktop Bus | ★★ | Apple总线 |
| 67 | dsi | DSI | ★★★ | 显示接口 |
| 68 | fsi | FSI | ★★★ | IBM总线 |
| 69 | gpib | GPIB | ★★★ | 仪器总线 |
| 70 | ieee488 | IEEE-488 | ★★★ | 仪器总线 |
| 71 | iec | IEC总线 | ★★ | 简单 |
| 72 | lfast | LFAST | ★★★ | 汽车总线 |
| 73 | mvb | MVB | ★★★ | 列车总线 |
| 74 | pcfx-ctrlr | PC-FX控制器 | ★★ | 简单 |
| 75 | pjdl | PJDL | ★★ | 简单 |
| 76 | pxx1 | PXX1 | ★★ | 简单 |
| 77 | rinnai-control-panel | 林内控制面板 | ★★ | 简单 |
| 78 | sle44xx | SLE44xx | ★★ | 智能卡 |
| 79 | sony_md | Sony MD | ★★ | 简单 |
| 80 | swi | SWI | ★★ | 简单 |
| 81 | t55xx | T55xx RFID | ★★ | RFID |
| 82 | em4100 | EM4100 RFID | ★★ | RFID |
| 83 | em4305 | EM4305 RFID | ★★ | RFID |
| 84 | ir_irmp | IR IRMP | ★★★ | 红外多协议 |
| 85 | ir_rc6 | IR RC6 | ★★ | 红外 |
| 86 | ir_ltto | IR LTTO | ★★ | 红外 |
| 87 | ir_recoil | IR Recoil | ★★ | 红外 |
| 88 | one_single_wire | 单线协议 | ★★ | 简单 |
| 89 | ook | OOK | ★★ | 调制 |
| 90 | rc_encode | RC编码 | ★★ | 简单 |
| 91 | st25r39xx_spi | ST25R39xx | ★★ | NFC |
| 92 | tlc5620 | TLC5620 DAC | ★★ | DAC |
| 93 | xy2-100 | XY2-100 | ★★ | 简单 |

### 第二优先级：`inputs != ['logic']` 的上层解码器（依赖其他解码器输出，性能收益较小但仍有价值）

| # | 解码器名 | 输入类型 | 复杂度 | 备注 |
|---|----------|----------|--------|------|
| 1 | eeprom93xx | microwire | ★★ | EEPROM |
| 2 | spiflash | spi | ★★ | Flash |
| 3 | eeprom24xx | i2c | ★★ | EEPROM |
| 4 | i2c_packet | i2c | ★★ | I2C数据包 |
| 5 | i2cdemux | i2c | ★★ | I2C解复用 |
| 6 | i2cfilter | i2c | ★★ | I2C过滤 |
| 7 | ps2_keyboard | ps2 | ★★ | 键盘 |
| 8 | ps2_mouse | ps2 | ★★ | 鼠标 |
| 9 | midi | uart | ★★ | MIDI |
| 10 | modbus | uart | ★★★ | Modbus |
| 11 | bluetooth_h4 | uart | ★★★ | 蓝牙H4 |
| 12 | jtag_avr | jtag | ★★★ | AVR JTAG |
| 13 | jtag_stm32 | jtag | ★★★ | STM32 JTAG |
| 14 | jtag_ejtag | jtag | ★★★ | EJTAG |
| 15 | onewire_network | onewire_link | ★★ | 1-Wire网络层 |
| 16 | ds2408 | onewire_network | ★★ | 1-Wire设备 |
| 17 | ds243x | onewire_network | ★★ | 1-Wire设备 |
| 18 | ds28ea00 | onewire_network | ★★ | 1-Wire设备 |
| 19 | usb_packet | usb_signalling | ★★★ | USB包 |
| 20 | usb_request | usb_packet | ★★★ | USB请求 |
| 21 | cfp | mdio | ★★ | MDIO上层 |
| 22 | 4b5b→ethernet→arp→ipv4→udp | 链式 | ★★★★ | 网络协议栈 |
| 23 | 各种SPI设备 | spi | ★★ | cc1101, nrf24l01等 |
| 24 | 各种I2C设备 | i2c | ★★ | bh1750, mpu6050等 |

---

## 实施步骤

### 步骤1：修复现有问题
- 将 lin_c, hdlc_c, microwire_c, mdio_c, ps2_c, dmx512_c 添加到 CMakeLists.txt 的 C_DECODERS 列表
- 验证所有14个C解码器编译通过

### 步骤2：按优先级重写第一批（高价值底层协议）
1. can-fd_c - CAN FD协议（CAN扩展，用户多）
2. usb_signalling_c - USB信号层
3. nrzi_c - NRZI编码（ethernet依赖）
4. 4b5b_c - 4B/5B编码（ethernet依赖）
5. spdif_c - S/PDIF音频
6. iso7816_c - 智能卡
7. lpc_c - LPC总线
8. cec_c - HDMI CEC
9. ir_nec_c - NEC红外
10. dcf77_c - DCF77时钟

### 步骤3：继续重写更多底层协议
- 根据用户需求继续从第一优先级列表中选择

### 步骤4：重写上层解码器
- 当底层解码器完成后，按需重写依赖它们的上层解码器

---

## 编译指令

```bash
# 增量编译（推荐日常使用）
/c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/build_incremental.cmd

# 完整重新配置编译
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../install.dir -DENABLE_DEBUG_HELPER=ON
```

---

## C 解码器模板结构

每个C解码器需包含：
1. `#include "libsigrokdecode.h"` 和必要头文件
2. 注解枚举 `enum { ANN_XXX, NUM_ANN }`
3. 状态枚举 `enum xxx_state { ... }`
4. 私有数据结构 `struct xxx_priv { ... }`
5. 通道定义 `static struct srd_channel xxx_channels[]`
6. 选项定义 `static struct srd_decoder_option xxx_options[]`
7. 注解标签 `static const char *xxx_ann_labels[][3]`
8. 注解行 `static const struct srd_c_ann_row xxx_ann_rows[]`
9. 输入/输出/标签定义
10. reset/start/decode/destroy 回调函数
11. `struct srd_c_decoder xxx_c_decoder` 导出结构
12. `srd_c_decoder_entry()` 和 `srd_c_decoder_api_version()` 导出函数
