# 修复所有C解码器测试至全PASS Spec

## Why
当前215个C解码器测试中有15个FAIL和70个WARN（空真），需要全部修复至PASS，确保C解码器与Python解码器输出完全一致。

## What Changes
- 修复15个FAIL解码器的C解码器Bug或测试数据生成器
- 修复70个WARN解码器的测试数据生成器，使其产生有效协议数据

## Impact
- Affected code: `libsigrokdecode/c_decoders/*.c` (C解码器Bug修复)
- Affected code: `libsigrokdecode/tests/protocol_synthesizer.py` (协议生成器)
- Affected code: `libsigrokdecode/tests/test_factory.py` (测试数据配置)
- Affected code: `libsigrokdecode/decoders/*/pd.py` (Python解码器Bug)

## 当前状态：130 PASS / 70 WARN / 15 FAIL

### 15个FAIL解码器 — C解码器逻辑与Python不一致

| # | 解码器 | 匹配/偏差 | 根因分类 | 修复方向 |
|---|--------|-----------|----------|----------|
| 1 | **4b5b_c** | 2/18113 | C解码器逻辑Bug | 4b5b解码表或位序与Python不一致 |
| 2 | **avclan_c** | 19/2 | C解码器逻辑Bug | 注解文本差异（之前修过悬挂指针，可能还有其他问题） |
| 3 | **iebus_c** | 19/2 | C解码器逻辑Bug | 同avclan_c，注解文本差异 |
| 4 | **ir_nec_c** | 35/3 | C解码器逻辑Bug | NEC协议注解格式或时序差异 |
| 5 | **ir_sirc_c** | 0/18 | C解码器逻辑Bug | SIRC协议解码逻辑完全不一致 |
| 6 | **lfast_c** | 62/12 | C解码器逻辑Bug | sleep bit解释、payload注解格式差异 |
| 7 | **maple_bus_c** | 0/48 | C解码器逻辑Bug | Maple Bus帧解析逻辑不一致 |
| 8 | **nrzi_c** | 2/18113 | C解码器逻辑Bug | NRZI编码/解码逻辑严重不一致 |
| 9 | **qi_c** | 34/23 | C解码器逻辑Bug | Qi差分双相解码注解差异 |
| 10 | **rvswd_c** | 0/59 | C解码器逻辑Bug | RVSWD START/STOP检测逻辑不一致 |
| 11 | **sdio_c** | 48/9 | C解码器逻辑Bug | SDIO命令解析注解差异 |
| 12 | **sipi_c** | 62/14 | C解码器逻辑Bug | 继承lfast_c差异 + 自身差异 |
| 13 | **uart_c** | 127/614 | C解码器逻辑Bug | UART帧边界/end_sample计算差异 |
| 14 | **uart_fast_c** | 1/72 | C解码器逻辑Bug | 快速UART解码逻辑差异 |
| 15 | **usb_power_delivery_c** | 21/4 | C解码器逻辑Bug | USB PD BMC解码注解差异 |

### 70个WARN解码器 — 测试数据生成器问题（空真）

| # | 解码器 | 协议类型 | 通道数 |
|---|--------|----------|--------|
| 1 | ac97_c | AC97音频总线 | 多通道 |
| 2 | adat_c | ADAT音频 | 1 |
| 3 | adb_c | Apple Desktop Bus | 1 |
| 4 | afsk_c | 音频频移键控 | 1 |
| 5 | am230x_c | AM230x温湿度 | 1 |
| 6 | arp_c | ARP网络协议(stack) | stack |
| 7 | aud_c | AUD | 1 |
| 8 | avr_pdi_c | AVR PDI | 1 |
| 9 | bean_c | BEAN协议 | 1 |
| 10 | c2_c | C2接口 | 1 |
| 11 | carrera_c | Carrera | 1 |
| 12 | dali_c | DALI照明 | 1 |
| 13 | delta-sigma_c | Delta-Sigma ADC | 1 |
| 14 | dmx512_c | DMX512灯光 | 1 |
| 15 | ds2408_c | DS2408(stack) | stack |
| 16 | ds243x_c | DS243x(stack) | stack |
| 17 | ds28ea00_c | DS28EA00(stack) | stack |
| 18 | dsi_c | DSI显示 | 多通道 |
| 19 | em4305_c | EM4305 RFID | 1 |
| 20 | emmc_sd_c | eMMC/SD | 多通道 |
| 21 | ethernet_c | 以太网 | 多通道 |
| 22 | fsi_c | FSI | 2 |
| 23 | gpib_c | GPIB仪器总线 | 16 |
| 24 | iec_c | IEC协议 | 3 |
| 25 | ieee488_c | IEEE488(stack) | stack |
| 26 | ipv4_c | IPv4(stack) | stack |
| 27 | ir_ltto_c | IR LTTO | 1 |
| 28 | ir_ltto_decode_c | IR LTTO Decode(stack) | stack |
| 29 | ir_irmp_c | IR IRMP | 1 |
| 30 | ir_recoil_c | IR Recoil | 1 |
| 31 | jtag_avr_c | JTAG AVR(stack) | stack |
| 32 | jitter_c | Jitter分析(stack) | stack |
| 33 | jtag_ejtag_c | JTAG EJTAG(stack) | stack |
| 34 | jtag_stm32_c | JTAG STM32(stack) | stack |
| 35 | lpc_c | LPC总线 | 6 |
| 36 | ltar_smartdevice_decode_c | LTAR(stack) | stack |
| 37 | mcs48_c | MCS48 | 14 |
| 38 | mipi_rffe_c | MIPI RFFE | 2 |
| 39 | mipi_dsi_c | MIPI DSI | 多通道 |
| 40 | mvb_c | MVB | 1 |
| 41 | numbers_and_state_c | 数字和状态 | 0 |
| 42 | one_single_wire_c | 单线 | 1 |
| 43 | ook_c | OOK调制 | 1 |
| 44 | ook_oregon_c | OOK Oregon(stack) | stack |
| 45 | ook_vis_c | OOK VIS(stack) | stack |
| 46 | pcfx_ctrlr_c | PC-FX控制器 | 2 |
| 47 | qspi_c | QSPI | 多通道 |
| 48 | rgb_led_ws281x_c | WS281x LED | 1 |
| 49 | sae_j1850_vpw_c | J1850 VPW | 1 |
| 50 | sdcard_sd_c | SD Card SD模式 | 多通道 |
| 51 | sdq_c | SDQ | 1 |
| 52 | seven_segment_c | 七段数码管(stack) | stack |
| 53 | signature_c | Signature分析(stack) | stack |
| 54 | sony_md_c | Sony MD | 1 |
| 55 | sony_md_decode_c | Sony MD Decode(stack) | stack |
| 56 | spacewire_c | SpaceWire | 2 |
| 57 | spdif_c | S/PDIF | 1 |
| 58 | spi_dual_quad_c | SPI Dual Quad | 多通道 |
| 59 | st7735_c | ST7735 LCD(stack) | stack |
| 60 | st7789_c | ST7789 LCD(stack) | stack |
| 61 | t55xx_c | T55xx RFID | 1 |
| 62 | tdm_audio_c | TDM音频 | 1 |
| 63 | tlc5620_c | TLC5620 DAC | 多通道 |
| 64 | tm1637_c | TM1637 | 2 |
| 65 | tm1638_c | TM1638 | 2 |
| 66 | tmc_c | TMC | 多通道 |
| 67 | tpm_fifo_tis_c | TPM FIFO(stack) | stack |
| 68 | udp_c | UDP(stack) | stack |
| 69 | usb_request_c | USB Request(stack) | stack |
| 70 | z80_c | Z80 CPU | 11 |

## ADDED Requirements

### Requirement: 所有C解码器测试必须PASS
系统 SHALL 确保所有215个C解码器测试结果为PASS（0 FAIL, 0 WARN）。

#### Scenario: 全量测试通过
- **WHEN** 运行 `python run_all_tests.py --all --jobs 4`
- **THEN** 输出 215 PASS, 0 FAIL, 0 WARN, 0 ERROR

### Requirement: FAIL解码器修复（C解码器逻辑Bug）
每个FAIL解码器 SHALL 修复C解码器逻辑使其与Python解码器输出一致。主要修复方向：
1. 注解文本格式对齐（hex格式、短文本变体、错误消息措辞）
2. end_sample边界计算修正
3. 状态机逻辑修正（START/STOP检测、sleep bit解释）
4. 协议解码逻辑修正（4b5b表、NRZI编码、Maple Bus帧解析）

### Requirement: WARN解码器修复（测试数据生成器）
每个WARN解码器 SHALL 通过改进测试数据生成器使其产生有效协议数据，让Python和C解码器都能输出注解。主要修复方向：
1. 为1通道简单协议生成有效信号模式
2. 为多通道总线协议生成正确的通道间时序
3. 为stack解码器确保上游解码器先产生输出
4. 为复杂协议（Z80、GPIB、LPC等）生成简化的有效序列

## MODIFIED Requirements
无

## REMOVED Requirements
无
