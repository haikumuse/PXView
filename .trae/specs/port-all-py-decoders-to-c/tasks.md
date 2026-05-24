# Tasks

## Phase 1: 子Spec编写 ✅ 全部完成

（37个子Spec已全部编写完成，略）

## Phase 2: C解码器实现（按优先级和依赖顺序）

### WAVE 1: 无阻塞底层解码器 (Batch 1-17, 可完全并行)

- [x] Task B1: 实现 Batch 1 解码器 (qspi, sdio, spi_dual_quad, uart-fast, cjtag) ✅
- [x] Task B2: 实现 Batch 2 解码器 (flexray, mipi_rffe, usb_power_delivery, iebus, spacewire) ✅
- [x] Task B3: 实现 Batch 3 解码器 (ac97, sdcard_sd, emmc_sd, swim, rvswd) ✅
- [x] Task B4: 实现 Batch 4 解码器 (tmc, sent, sle44xx, pjdl, onewire_link) ✅
- [x] Task B5: 实现 Batch 5 解码器 (adb, afsk, am230x, caliper, carrera) ✅
- [x] Task B6: 实现 Batch 6 解码器 (dcc, delta-sigma, dsi, em4100, em4305) ✅
- [x] Task B7: 实现 Batch 7 解码器 (eth_an, fsi, gpib, guess_bitrate, iec) ✅
- [x] Task B8: 实现 Batch 8 解码器 (ieee488, ir_irmp, ir_ltto, ir_rc6, ir_recoil) ✅
- [x] Task B9: 实现 Batch 9 解码器 (jitter, lfast, maple_bus, miller, morse) ✅
- [x] Task B10: 实现 Batch 10 解码器 (mvb, mcs48, one_single_wire, ook, opentherm) ✅
- [x] Task B11: 实现 Batch 11 解码器 (parallel, pcfx-ctrlr, rinnai-control-panel, rpm, sae_j1850_vpw) ✅
- [x] Task B12: 实现 Batch 12 解码器 (sda2506, signature, sony_md, st7735, st7789) ✅
- [x] Task B13: 实现 Batch 13 解码器 (z80, adat, arm_etmv3, aud, avr_pdi) ✅
- [x] Task B14: 实现 Batch 14 解码器 (bean, ccd, cjtag-oscan0, rgb_led_ws281x, stepper_motor) ✅
- [x] Task B15: 实现 Batch 15 解码器 (mipi_dsi, pxx1, qi, rc_encode, sdq) ✅
- [x] Task B16: 实现 Batch 16 解码器 (spi-fast, swi, t55xx, tdm_audio, timing) ✅
- [x] Task B17: 实现 Batch 17 解码器 (tlc5620, xy2-100) ✅

### WAVE 2: 无阻塞上层解码器 (Batch 18-35, 底层C解码器已完成)

- [x] Task B18: 实现 Batch 18 解码器 (ad5593r, adxl345, atsha204a, bh1750, eeprom24xx) ✅
- [x] Task B19: 实现 Batch 19 解码器 (edid, i2c_packet, i2cdemux, i2cfilter, ltc26x7) ✅
- [x] Task B20: 实现 Batch 20 解码器 (mlx90614, mpu6050, mxc6225xu, nunchuk, pca9571) ✅
- [x] Task B21: 实现 Batch 21 解码器 (rtc8564, ssd1306, st25dv, tcs3472x, tpm_tis_i2c) ✅
- [x] Task B22: 实现 Batch 22 解码器 (xfp, hdcp, hdmi_scdc, tca6408a, tmp102) ✅
- [x] Task B23: 实现 Batch 23 解码器 (a7105, ad5626, ad79x0, ade77xx, adf435x) ✅
- [x] Task B24: 实现 Batch 24 解码器 (adns5020, as5047, avr_isp, cc1101, cyrf6936) ✅
- [x] Task B25: 实现 Batch 25 解码器 (enc28j60, ltc242x, max6954, max7219, mrf24j40) ✅
- [x] Task B26: 实现 Batch 26 解码器 (nes_gamepad, nrf24l01, nrf905, rfm12, ssi32) ✅
- [x] Task B27: 实现 Batch 27 解码器 (st25r39xx_spi, sdcard_spi, spiflash, spi_tpm, tpm_tis_spi) ✅
- [x] Task B28: 实现 Batch 28 解码器 (x2444m, rgb_led_spi) ✅
- [x] Task B29: 实现 Batch 29 解码器 (arm_itm, arm_tpiu, bluetooth_h4, boost, crsf) ✅
- [x] Task B30: 实现 Batch 30 解码器 (j1708, midi, modbus, pan1321, pn532) ✅
- [x] Task B31: 实现 Batch 31 解码器 (sbus_futaba, scs, ufcs, amulet_ascii, streletz) ✅
- [x] Task B32: 实现 Batch 32 解码器 (jtag_avr, jtag_ejtag, jtag_stm32) ✅
- [x] Task B33: 实现 Batch 33 解码器 (onewire_network, ds2408, ds243x, ds28ea00, eeprom93xx) ✅
- [x] Task B34: 实现 Batch 34 解码器 (avclan, ethernet, arp, ipv4, udp) ✅
- [x] Task B35: 实现 Batch 35 解码器 (cfp, ps2_keyboard, ps2_mouse, usb_packet, usb_request) ✅

### WAVE 3: 被阻塞的解码器 (需WAVE 1完成后解锁) ✅ 全部完成

- [x] Task B36: 实现 Batch 36 解码器 (ook_oregon, ook_vis, ltar_smartdevice, ir_ltto_decode, sony_md_decode) ✅
- [x] Task B37: 实现 Batch 37 解码器 (sipi, pjon, tpm_fifo_tis, tm1637, tm1638, ltar_smartdevice_decode) ✅
- [x] Task B34-avclan: 实现 avclan_c (Batch 34的阻塞部分) ✅ (avclan_c已在B34中一起实现)

## Phase 2 完成总结

**全部37个批次、178个C解码器已实现完成！**

实施轮次：
- Round 1: B1, B2, B18, B19 (20 decoders)
- Round 2: B3, B4, B20, B21 (20 decoders)
- Round 3: B5, B6, B22, B23 (20 decoders)
- Round 4: B7, B8, B24, B25 (20 decoders)
- Round 5: B9, B10, B26, B27 (20 decoders)
- Round 6: B11, B12, B28, B29 (17 decoders)
- Round 7: B13, B14, B30, B31 (20 decoders)
- Round 8: B15, B16, B32, B33 (18 decoders)
- Round 9: B17, B34, B35 (12 decoders)
- Round 10: B36, B37 (11 decoders)

# Task Dependencies — 全部已满足 ✅

## Phase 3: 编译验证

- [ ] Task V1: 增量编译验证 Batch 1-9 解码器 (Round 1-5 产出的底层解码器)
- [ ] Task V2: 增量编译验证 Batch 10-17 解码器 (Round 5-9 产出的底层解码器)
- [ ] Task V3: 增量编译验证 Batch 18-28 解码器 (Round 1-6 产出的上层解码器)
- [ ] Task V4: 增量编译验证 Batch 29-37 解码器 (Round 6-10 产出的上层解码器)
- [ ] Task V5: 修复所有编译错误和警告
- [ ] Task V6: 最终全量编译确认通过

# 实施策略

1. **每批次由1个子Agent实现**，每次最多4个子Agent并行 ✅
2. **每个子Agent读取对应batch的spec.md**，按spec中的详细规格实现 ✅
3. **实现步骤**（每个解码器）：✅
   - 创建 `{decoder_id}_c.c` 文件
   - 定义通道、选项、注解数组
   - 实现状态结构体和所有回调
   - 在 CMakeLists.txt 中注册
4. **WAVE 1 和 WAVE 2 并行执行** ✅
5. **WAVE 3 在 WAVE 1 完成后执行** ✅
