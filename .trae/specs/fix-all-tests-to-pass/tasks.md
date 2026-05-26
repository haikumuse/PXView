# Tasks

- [ ] Task 1: 修复15个FAIL解码器（C解码器逻辑Bug）
  - [ ] SubTask 1.1: 修复4b5b_c — 4b5b解码表或位序与Python不一致（2/18113）
  - [ ] SubTask 1.2: 修复nrzi_c — NRZI编码/解码逻辑严重不一致（2/18113）
  - [x] SubTask 1.3: 修复uart_c/uart_fast_c — UART帧边界/end_sample计算差异（127/614, 1/72）
  - [ ] SubTask 1.4: 修复avclan_c/iebus_c — 注解文本差异（19/2）
  - [ ] SubTask 1.5: 修复ir_nec_c/ir_sirc_c — IR协议解码逻辑差异（35/3, 0/18）
  - [ ] SubTask 1.6: 修复lfast_c/sipi_c — sleep bit解释、payload注解格式（62/12, 62/14）
  - [ ] SubTask 1.7: 修复maple_bus_c — Maple Bus帧解析逻辑（0/48）
  - [ ] SubTask 1.8: 修复qi_c — Qi差分双相解码注解差异（34/23）
  - [ ] SubTask 1.9: 修复rvswd_c — RVSWD START/STOP检测逻辑（0/59）
  - [ ] SubTask 1.10: 修复sdio_c — SDIO命令解析注解差异（48/9）
  - [ ] SubTask 1.11: 修复usb_power_delivery_c — USB PD BMC解码注解差异（21/4）

- [ ] Task 2: 修复70个WARN解码器（测试数据生成器）
  - [ ] SubTask 2.1: 修复1通道简单协议（adat_c, adb_c, afsk_c, am230x_c, aud_c, avr_pdi_c, bean_c, c2_c, carrera_c, dali_c, delta-sigma_c, dmx512_c, dsi_c, em4305_c, mvb_c, one_single_wire_c, ook_c, rgb_led_ws281x_c, sae_j1850_vpw_c, sdq_c, sony_md_c, spdif_c, t55xx_c, tdm_audio_c, ir_ltto_c, ir_irmp_c, ir_recoil_c）— 约27个
  - [ ] SubTask 2.2: 修复多通道总线协议（ac97_c, emmc_sd_c, ethernet_c, fsi_c, gpib_c, iec_c, lpc_c, mcs48_c, mipi_rffe_c, mipi_dsi_c, pcfx_ctrlr_c, qspi_c, sdcard_sd_c, spi_dual_quad_c, spacewire_c, tlc5620_c, tm1637_c, tm1638_c, tmc_c, z80_c）— 约20个
  - [ ] SubTask 2.3: 修复stack解码器（arp_c, ds2408_c, ds243x_c, ds28ea00_c, ieee488_c, ipv4_c, ir_ltto_decode_c, jtag_avr_c, jitter_c, jtag_ejtag_c, jtag_stm32_c, ltar_smartdevice_decode_c, numbers_and_state_c, ook_oregon_c, ook_vis_c, seven_segment_c, signature_c, sony_md_decode_c, st7735_c, st7789_c, tpm_fifo_tis_c, udp_c, usb_request_c）— 约23个

- [ ] Task 3: 编译验证和全量测试
  - [ ] SubTask 3.1: ninja -C build 编译所有修改的C解码器
  - [ ] SubTask 3.2: python test_factory.py 重新生成测试数据
  - [ ] SubTask 3.3: python run_all_tests.py --all --jobs 4 验证215 PASS

# Task Dependencies
- Task 1 和 Task 2 可以并行执行
- SubTask 1.6 (sipi_c) 依赖 SubTask 1.6 (lfast_c)
- SubTask 2.3 (stack解码器) 依赖 SubTask 2.1-2.2 (上游解码器)
- Task 3 依赖 Task 1 和 Task 2
