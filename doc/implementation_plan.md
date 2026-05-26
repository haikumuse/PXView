# 修复 WARN 及 FAIL 波形的实施计划

## 问题诊断
你要求“根据py的解码逻辑逆向出正确的波形数据”。这是一个非常精准的思路！
但我有一个好消息要告诉你：其实在你们仓库的 `protocol_synthesizer.py` 文件中，**已经有人（或者之前的开发过程）写好了多达 76 种协议的波形发生器（Generator）**！这其中包括了 `UART`, `USB`, `I2C`, `Z80`, `GPIB` 等等。

**为什么还会大面积出现 WARN？**
因为当前的 `generate_testdata.py` **完全没有调用** `protocol_synthesizer.py`！它的 `generate_input_bin` 函数写死了无脑输出 `101010` 的方波，导致这 76 个写好的发生器全在睡大觉。

## 实施方案

为了彻底消灭这些 WARN 和 FAIL，我计划分两步进行：

### 第一步：打通波形发生器引擎 (Hook up the Engine)
重构 `generate_testdata.py`，将现有的 76 个 Generator 动态映射到对应的测试用例上。
比如：
- 测 `uart_c` 时，自动调用 `UARTGenerator` 产生包含正确的 Start/Stop bit 的数据。
- 测 `usb_power_delivery_c` 时，调用 `USBGenerator` 产生符合 NRZI 和位填充（Bit Stuffing）的信号。

这一步就能瞬间解决大部分协议（包括 `c2`, `gpib`, `ethernet`, `lpc`, `z80` 等）的 0 annotations (WARN) 问题，同时也能修复 `uart`, `usb` 等因为随机数据触发的边界分歧 (FAIL)。

### 第二步：根据 Python 源码逆向缺失的发生器 (Reverse Engineering)
对于目前 `protocol_synthesizer.py` 中**没有**的协议（比如报 WARN 的 `tm1637`, `qspi`, `st7789`），我将完全按照你说的思路：**直接读取它们对应的 Python 解码器源码，逆向其状态机逻辑，然后在 `protocol_synthesizer.py` 中为它们手写专门的波形生成逻辑。**

#### 待逆向的协议示例：
1. **tm1637_c**: 读取 `tm1637/pd.py`，逆向其特有的 2-wire I2C-like 时序（无地址位，特定 ACK 规则），编写 `TM1637Generator`。
2. **qspi_c**: 读取 `smart_qspi/pd.py`，逆向 Quad SPI 的 4 数据线交替传输逻辑，编写 `QSPIGenerator`。

> [!IMPORTANT]
> 由于有 41 个 WARN，一次性逆向并手写所有缺失的发生器可能需要大量时间。
> **征求意见**：我们是否先执行第一步（连接现有的 76 个发生器），看看能直接消灭多少个 WARN/FAIL？然后再挑选剩余中最顽固的几个进行重点逆向手写？

## 验证计划
1. 修改完成后，重新运行 `python generate_testdata.py --overwrite`，让它使用合法的合成波形覆盖旧的垃圾数据。
2. 重新运行 `python run_all_tests.py --all --jobs 16`。
3. 检查控制台输出，期望看到 WARN 的数量锐减，FAIL 的数量大幅下降。
PS C:\Users\admin\Downloads\libsigrokdecode> cd c:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrokdecode\tests && python run_all_tests.py --all --jobs 16
Running 215 tests in parallel (16 jobs)...
[  1/215] a7105_c                   | PASS  |  5.7s
[  2/215] ad5593r_c                 | PASS  |  6.4s
[  3/215] ad79x0_c                  | PASS  |  6.8s
[  4/215] afsk_c                    | PASS  |  7.1s
[  5/215] ac97_c                    | WARN  |  7.1s
      -> All 0 annotations match (vacuous - no output from either decoder)
[  6/215] adb_c                     | PASS  |  7.2s
[  7/215] amulet_ascii_c            | PASS  |  7.3s
[  8/215] arm_etmv3_c               | PASS  |  7.4s
[  9/215] adxl345_c                 | PASS  |  7.5s
[ 10/215] adns5020_c                | PASS  |  7.5s
[ 11/215] am230x_c                  | PASS  |  7.5s
[ 12/215] ad5626_c                  | PASS  |  7.6s
[ 13/215] ade77xx_c                 | PASS  |  7.7s
[ 14/215] adf435x_c                 | PASS  |  7.7s
[ 15/215] 4b5b_c                    | PASS  |  8.5s
[ 16/215] arm_itm_c                 | PASS  |  2.8s
[ 17/215] arm_tpiu_c                | PASS  |  4.8s
[ 18/215] adat_c                    | FAIL  | 11.7s
      -> 8377 matches, 16184 deviations found.
[ 19/215] arp_c                     | WARN  |  5.9s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 20/215] aud_c                     | WARN  |  6.8s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 21/215] avclan_c                  | PASS  |  7.3s
[ 22/215] as5047_c                  | PASS  |  7.7s
[ 23/215] atsha204a_c               | PASS  |  7.7s
[ 24/215] bean_c                    | WARN  |  7.5s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 25/215] bluetooth_h4_c            | PASS  |  7.5s
[ 26/215] boost_c                   | PASS  |  7.5s
[ 27/215] avr_isp_c                 | PASS  |  7.8s
[ 28/215] can_c                     | PASS  |  6.8s
[ 29/215] caliper_c                 | PASS  |  6.8s
[ 30/215] bh1750_c                  | PASS  |  7.9s
[ 31/215] avr_pdi_c                 | PASS  |  7.9s
[ 32/215] cc1101_c                  | PASS  |  5.4s
[ 33/215] c2_c                      | WARN  | 12.1s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 34/215] cec_c                     | PASS  |  6.1s
[ 35/215] can_fd_c                  | PASS  | 10.5s
[ 36/215] ccd_c                     | PASS  |  7.7s
[ 37/215] cfp_c                     | PASS  |  7.3s
[ 38/215] cjtag_c                   | PASS  |  7.6s
[ 39/215] carrera_c                 | PASS  | 10.7s
[ 40/215] counter_c                 | PASS  |  7.5s
[ 41/215] cjtag_oscan0_c            | PASS  |  7.5s
[ 42/215] crsf_c                    | PASS  |  7.6s
[ 43/215] cyrf6936_c                | PASS  |  7.5s
[ 44/215] dcf77_c                   | PASS  |  7.4s
[ 45/215] dali_c                    | FAIL  |  7.5s
      -> 0 matches, 13 deviations found.
[ 46/215] dmx512_c                  | FAIL  |  4.9s
      -> 80 matches, 5 deviations found.
[ 47/215] dcc_c                     | PASS  |  7.8s
[ 48/215] ds1307_c                  | PASS  |  4.0s
[ 49/215] delta-sigma_c             | FAIL  | 10.7s
      -> 192 matches, 190 deviations found.
[ 50/215] ds243x_c                  | WARN  |  5.6s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 51/215] ds3231_c                  | PASS  |  5.8s
[ 52/215] ds2408_c                  | WARN  |  7.8s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 53/215] ds28ea00_c                | WARN  |  6.8s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 54/215] edid_c                    | PASS  |  7.0s
[ 55/215] eeprom24xx_c              | PASS  |  6.9s
[ 56/215] eeprom93xx_c              | PASS  |  6.9s
[ 57/215] em4100_c                  | PASS  |  6.9s
[ 58/215] dsi_c                     | WARN  |  7.1s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 59/215] emmc_sd_c                 | WARN  |  6.9s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 60/215] enc28j60_c                | PASS  |  6.9s
[ 61/215] eth_an_c                  | PASS  |  6.8s
[ 62/215] ethernet_c                | WARN  |  6.7s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 63/215] flexray_c                 | PASS  |  6.2s
[ 64/215] em4305_c                  | PASS  |  7.5s
[ 65/215] fsi_c                     | WARN  |  4.9s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 66/215] gpib_c                    | WARN  |  5.0s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 67/215] graycode_c                | PASS  |  5.0s
[ 68/215] guess_bitrate_c           | PASS  |  5.4s
[ 69/215] hdcp_c                    | PASS  |  5.3s
[ 70/215] i2cdemux_c                | PASS  |  7.3s
[ 71/215] hdmi_scdc_c               | PASS  |  7.7s
[ 72/215] hdlc_c                    | PASS  |  7.8s
[ 73/215] i2c_packet_c              | PASS  |  7.7s
[ 74/215] i2s_c                     | PASS  |  7.8s
[ 75/215] iebus_c                   | PASS  |  7.9s
[ 76/215] ieee488_c                 | WARN  |  7.8s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 77/215] ipv4_c                    | WARN  |  7.6s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 78/215] iec_c                     | WARN  |  8.1s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 79/215] i2c_c                     | PASS  |  8.5s
[ 80/215] ir_irmp_c                 | WARN  |  7.0s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 81/215] ir_ltto_c                 | PASS  |  5.8s
[ 82/215] i2cfilter_c               | PASS  |  8.4s
[ 83/215] ir_ltto_decode_c          | PASS  |  5.2s
[ 84/215] ir_rc5_c                  | PASS  |  4.3s
[ 85/215] ir_nec_c                  | PASS  |  4.5s
[ 86/215] ir_rc6_c                  | PASS  |  5.7s
[ 87/215] ir_recoil_c               | PASS  |  6.6s
[ 88/215] ir_sirc_c                 | PASS  |  7.1s
[ 89/215] iso7816_c                 | PASS  |  7.6s
[ 90/215] j1708_c                   | PASS  |  7.6s
[ 91/215] jitter_c                  | PASS  |  8.1s
[ 92/215] jtag_c                    | PASS  |  8.0s
[ 93/215] jtag_stm32_c              | PASS  |  7.9s
[ 94/215] jtag_avr_c                | PASS  |  8.1s
[ 95/215] lfast_c                   | FAIL  |  7.9s
      -> 62 matches, 12 deviations found.
[ 96/215] lpc_c                     | WARN  |  7.8s
      -> All 0 annotations match (vacuous - no output from either decoder)
[ 97/215] lin_c                     | PASS  |  7.9s
[ 98/215] ltar_smartdevice_c        | PASS  |  7.8s
[ 99/215] lm75_c                    | PASS  |  7.9s
[100/215] ltar_smartdevice_decode_c | WARN  |  7.6s
      -> All 0 annotations match (vacuous - no output from either decoder)
[101/215] jtag_ejtag_c              | PASS  |  8.3s
[102/215] ltc242x_c                 | PASS  |  3.7s
[103/215] max6954_c                 | PASS  |  5.2s
[104/215] ltc26x7_c                 | PASS  |  6.3s
[105/215] maple_bus_c               | FAIL  |  6.5s
      -> 0 matches, 48 deviations found.
[106/215] max7219_c                 | PASS  |  5.9s
[107/215] mlx90614_c                | PASS  |  7.1s
[108/215] mipi_rffe_c               | WARN  |  7.7s
      -> All 0 annotations match (vacuous - no output from either decoder)
[109/215] midi_c                    | PASS  |  7.9s
[110/215] mpu6050_c                 | PASS  |  7.8s
[111/215] modbus_c                  | PASS  |  8.0s
[112/215] mrf24j40_c                | PASS  |  7.7s
[113/215] miller_c                  | PASS  |  8.2s
[114/215] mcs48_c                   | WARN  |  8.3s
      -> All 0 annotations match (vacuous - no output from either decoder)
[115/215] mdio_c                    | PASS  |  8.3s
[116/215] microwire_c               | PASS  |  8.3s
[117/215] morse_c                   | PASS  |  8.7s
[118/215] mxc6225xu_c               | PASS  |  5.0s
[119/215] mvb_c                     | WARN  |  5.1s
      -> All 0 annotations match (vacuous - no output from either decoder)
[120/215] nrf24l01_c                | PASS  |  4.9s
[121/215] nes_gamepad_c             | PASS  |  5.1s
[122/215] nrf905_c                  | PASS  |  6.0s
[123/215] numbers_and_state_c       | WARN  |  7.0s
      -> All 0 annotations match (vacuous - no output from either decoder)
[124/215] onewire_c                 | PASS  |  9.0s
[125/215] ook_c                     | FAIL  |  9.3s
      -> 0 matches, 18 deviations found.
[126/215] onewire_network_c         | PASS  |  9.7s
[127/215] ook_oregon_c              | FAIL  |  9.7s
      -> 0 matches, 18 deviations found.
[128/215] one_single_wire_c         | PASS  |  9.9s
[129/215] nunchuk_c                 | PASS  | 10.0s
[130/215] onewire_link_c            | PASS  | 10.0s
[131/215] opentherm_c               | PASS  |  9.0s
[132/215] ook_vis_c                 | FAIL  |  9.3s
      -> 0 matches, 29 deviations found.
[133/215] parallel_c                | PASS  |  8.3s
[134/215] pca9571_c                 | PASS  |  8.2s
[135/215] pcfx_ctrlr_c              | WARN  |  5.3s
      -> All 0 annotations match (vacuous - no output from either decoder)
[136/215] pan1321_c                 | PASS  |  9.3s
[137/215] pjdl_c                    | PASS  |  3.7s
[138/215] nrzi_c                    | PASS  | 11.4s
[139/215] pjon_c                    | PASS  |  5.2s
[140/215] pn532_c                   | PASS  |  5.9s
[141/215] ps2_c                     | PASS  |  6.7s
[142/215] ps2_keyboard_c            | PASS  |  7.2s
[143/215] pwm_c                     | PASS  |  7.8s
[144/215] pxx1_c                    | PASS  |  7.7s
[145/215] ps2_mouse_c               | PASS  |  8.0s
[146/215] rc_encode_c               | PASS  |  7.9s
[147/215] qspi_c                    | WARN  |  8.0s
      -> All 0 annotations match (vacuous - no output from either decoder)
[148/215] rgb_led_spi_c             | PASS  |  7.8s
[149/215] rgb_led_ws281x_c          | FAIL  |  7.8s
      -> 24 matches, 2 deviations found.
[150/215] qi_c                      | FAIL  |  8.2s
      -> 37 matches, 17 deviations found.
[151/215] rpm_c                     | PASS  |  7.3s
[152/215] mipi_dsi_c                | WARN  | 26.7s
      -> All 0 annotations match (vacuous - no output from either decoder)
[153/215] rtc8564_c                 | PASS  |  4.7s
[154/215] rfm12_c                   | PASS  |  8.8s
[155/215] rvswd_c                   | FAIL  |  4.3s
      -> 0 matches, 59 deviations found.
[156/215] sae_j1850_vpw_c           | WARN  |  4.5s
      -> All 0 annotations match (vacuous - no output from either decoder)
[157/215] rinnai_control_panel_c    | PASS  | 11.2s
[158/215] sbus_futaba_c             | PASS  |  5.2s
[159/215] sda2506_c                 | PASS  |  6.3s
[160/215] sdcard_sd_c               | WARN  |  6.6s
      -> All 0 annotations match (vacuous - no output from either decoder)
[161/215] scs_c                     | PASS  |  7.5s
[162/215] sent_c                    | PASS  |  7.3s
[163/215] signature_c               | WARN  |  7.3s
      -> All 0 annotations match (vacuous - no output from either decoder)
[164/215] sdio_c                    | FAIL  |  7.4s
      -> 48 matches, 9 deviations found.
[165/215] sdq_c                     | PASS  |  7.5s
[166/215] sdcard_spi_c              | PASS  |  7.8s
[167/215] sony_md_c                 | PASS  |  7.0s
[168/215] sipi_c                    | FAIL  |  7.5s
      -> 62 matches, 14 deviations found.
[169/215] sony_md_decode_c          | PASS  |  6.6s
[170/215] spacewire_c               | WARN  |  5.4s
      -> All 0 annotations match (vacuous - no output from either decoder)
[171/215] sle44xx_c                 | PASS  |  7.6s
[172/215] seven_segment_c           | PASS  |  8.6s
[173/215] spi_c                     | PASS  |  4.9s
[174/215] spdif_c                   | FAIL  |  8.6s
      -> 18 matches, 1 deviations found.
[175/215] spi_tpm_c                 | PASS  |  5.8s
[176/215] spiflash_c                | PASS  |  6.0s
[177/215] ssd1306_c                 | PASS  |  6.4s
[178/215] ssi32_c                   | PASS  |  6.4s
[179/215] st25dv_c                  | PASS  |  6.4s
[180/215] st7735_c                  | WARN  |  6.5s
      -> All 0 annotations match (vacuous - no output from either decoder)
[181/215] st25r39xx_spi_c           | PASS  |  6.5s
[182/215] stepper_motor_c           | PASS  |  6.5s
[183/215] streletz_c                | PASS  |  6.4s
[184/215] swd_c                     | PASS  |  6.4s
[185/215] swim_c                    | FAIL  |  6.2s
      -> 1 matches, 2 deviations found.
[186/215] swi_c                     | PASS  |  6.4s
[187/215] st7789_c                  | WARN  |  7.3s
      -> All 0 annotations match (vacuous - no output from either decoder)
[188/215] spi_fast_c                | PASS  |  9.5s
[189/215] t55xx_c                   | FAIL  |  5.3s
      -> 76 matches, 180 deviations found.
[190/215] spi_dual_quad_c           | WARN  | 12.3s
      -> All 0 annotations match (vacuous - no output from either decoder)
[191/215] tca6408a_c                | PASS  |  5.7s
[192/215] tcs3472x_c                | PASS  |  5.9s
[193/215] tdm_audio_c               | FAIL  |  6.0s
      -> 0 matches, 8 deviations found.
[194/215] timing_c                  | PASS  |  6.0s
[195/215] tlc5620_c                 | WARN  |  6.0s
      -> All 0 annotations match (vacuous - no output from either decoder)
[196/215] tm1637_c                  | WARN  |  6.2s
      -> All 0 annotations match (vacuous - no output from either decoder)
[197/215] tm1638_c                  | WARN  |  6.3s
      -> All 0 annotations match (vacuous - no output from either decoder)
[198/215] tmc_c                     | WARN  |  6.4s
      -> All 0 annotations match (vacuous - no output from either decoder)
[199/215] tpm_fifo_tis_c            | WARN  |  6.2s
      -> All 0 annotations match (vacuous - no output from either decoder)
[200/215] tpm_tis_i2c_c             | PASS  |  6.2s
[201/215] uart_c                    | FAIL  |  6.2s
      -> 127 matches, 614 deviations found.
[202/215] tmp102_c                  | PASS  |  7.4s
[203/215] tpm_tis_spi_c             | PASS  |  7.2s
[204/215] udp_c                     | WARN  |  5.7s
      -> All 0 annotations match (vacuous - no output from either decoder)
[205/215] ufcs_c                    | PASS  |  5.7s
[206/215] usb_power_delivery_c      | FAIL  |  6.0s
      -> 21 matches, 4 deviations found.
[207/215] usb_packet_c              | PASS  |  6.9s
[208/215] wiegand_c                 | PASS  |  6.2s
[209/215] x2444m_c                  | PASS  |  5.8s
[210/215] xfp_c                     | PASS  |  5.7s
[211/215] usb_request_c             | WARN  |  6.5s
      -> All 0 annotations match (vacuous - no output from either decoder)
[212/215] usb_signalling_c          | PASS  |  6.6s
[213/215] z80_c                     | WARN  |  5.7s
      -> All 0 annotations match (vacuous - no output from either decoder)
[214/215] uart_fast_c               | FAIL  | 10.9s
      -> 33 matches, 32 deviations found.
[215/215] xy2_100_c                 | PASS  |  6.7s

======================================================================
SUMMARY
======================================================================
  Total: 215  PASS: 153  WARN: 41  FAIL: 21  ERROR: 0  SKIP: 0
PS C:\Users\admin\Downloads\DSView-main_2026_4_27cppnb\libsigrokdecode\tests>
