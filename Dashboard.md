# CI Verification Dashboard

## Summary

| Total | PASS 🟢 | DEVIATION 🟡 | WARN 🟠 | FAIL 🔴 | ERROR 💥 | SKIP ⚪ |
|---|---|---|---|---|---|---|
| 215 | 202 | 2 | 1 | 6 | 4 | 0 |

## Failures & Errors

### ad5593r_c
**Status**: FAIL (3.5s)

```text
60 matches, 2 deviations found.
MISSED at sample 9: Py has class 5 (I²C slave is not compatible.) but C doesn't
EXTRA at sample 9: C has class 5 (I2C slave is not compatible.) but Py doesn't
```

### emmc_sd_c
**Status**: FAIL (2.2s)

```text
1 matches, 110 deviations found.
MISSED at sample 2008: Py has class 65 (Start bit) but C doesn't
MISSED at sample 2008: Py has class 71 (0) but C doesn't
EXTRA at sample 2008: C has class 128 (0) but Py doesn't
EXTRA at sample 2008: C has class 129 (Start bit) but Py doesn't
MISSED at sample 2012: Py has class 66 (Transmission: host) but C doesn't
MISSED at sample 2012: Py has class 71 (1) but C doesn't
EXTRA at sample 2012: C has class 128 (1) but Py doesn't
EXTRA at sample 2012: C has class 130 (Transmission: host) but Py doesn't
MISSED at sample 2016: Py has class 67 (Command: GO_IDLE_STATE (0)) but C doesn't
MISSED at sample 2016: Py has class 71 (0) but C doesn't
... and 100 more
```

### ethernet_c
**Status**: FAIL (4.8s)

```text
11657 matches, 3 deviations found.
MISSED at sample 5515: Py has class 0 (Frame Check Sequence: FAILED) but C doesn't
MISSED at sample 8715: Py has class 0 (Frame Check Sequence: FAILED) but C doesn't
MISSED at sample 9265: Py has class 0 (Frame Check Sequence: FAILED) but C doesn't
```

### mipi_rffe_c
**Status**: FAIL (2.2s)

```text
21 matches, 5 deviations found.
MISSED at sample 2108: Py has class 17 (Illegal Jump Edge) but C doesn't
MISSED at sample 2136: Py has class 12 (Bus Pack) but C doesn't
EXTRA at sample 2138: C has class 12 (Bus Pack) but Py doesn't
MISSED at sample 2454: Py has class 12 (Bus Pack) but C doesn't
EXTRA at sample 2458: C has class 12 (Bus Pack) but Py doesn't
```

### sdcard_sd_c
**Status**: FAIL (2.4s)

```text
48 matches, 16 deviations found.
MISSED at sample 2008: Py has class 0 (CMD0 (GO_IDLE_STATE): Reset all SD cards) but C doesn't
EXTRA at sample 2008: C has class 0 (CMD0 (GO_IDLE_STATE): Reset all SD cards) but Py doesn't
MISSED at sample 2008: Py has class 203 (Start bit) but C doesn't
EXTRA at sample 2008: C has class 203 (Start bit) but Py doesn't
MISSED at sample 2012: Py has class 204 (Transmission: host) but C doesn't
EXTRA at sample 2012: C has class 204 (Transmission: host) but Py doesn't
MISSED at sample 2016: Py has class 205 (Command: GO_IDLE_STATE (0)) but C doesn't
EXTRA at sample 2016: C has class 205 (Command: GO_IDLE_STATE (0)) but Py doesn't
MISSED at sample 2040: Py has class 206 (Argument: 0x00000000) but C doesn't
EXTRA at sample 2040: C has class 206 (Argument: 0x00000000) but Py doesn't
... and 6 more
```

### usb_request_c
**Status**: FAIL (2.2s)

```text
240 matches, 15 deviations found.
MISSED at sample 2118: Py has class 15 (DATA0 [ 80 06 00 01 00 00 40 00 ]) but C doesn't
EXTRA at sample 2118: C has class 15 (DATA0: 8 bytes) but Py doesn't
MISSED at sample 2134: Py has class 2 (PID: DATA0) but C doesn't
EXTRA at sample 2134: C has class 2 (PID: DATA0) but Py doesn't
MISSED at sample 2150: Py has class 8 (Databyte: 80) but C doesn't
EXTRA at sample 2150: C has class 8 (DATA: 80 06 00 01 00 00 40 00) but Py doesn't
MISSED at sample 2165: Py has class 8 (Databyte: 06) but C doesn't
MISSED at sample 2181: Py has class 8 (Databyte: 00) but C doesn't
MISSED at sample 2198: Py has class 8 (Databyte: 01) but C doesn't
MISSED at sample 2214: Py has class 8 (Databyte: 00) but C doesn't
... and 5 more
```

### ook_c
**Status**: ERROR (91.7s)

```text
C error: Command '['C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\build.dir\\decoder_test.exe', '-d', 'ook_c', '-t', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\ook_c\\default', '-f', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\ook_c\\default\\actual_c.json', '--tolerance', '2', '--generate-only']' timed out after 30 seconds
```

### ook_oregon_c
**Status**: ERROR (91.8s)

```text
C error: Command '['C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\build.dir\\decoder_test.exe', '-d', 'ook_oregon_c', '-t', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\ook_oregon_c\\default', '-f', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\ook_oregon_c\\default\\actual_c.json', '--tolerance', '2', '--generate-only']' timed out after 30 seconds
```

### ook_vis_c
**Status**: ERROR (92.0s)

```text
C error: Command '['C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\build.dir\\decoder_test.exe', '-d', 'ook_vis_c', '-t', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\ook_vis_c\\default', '-f', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\ook_vis_c\\default\\actual_c.json', '--tolerance', '2', '--generate-only']' timed out after 30 seconds
```

### wiegand_c
**Status**: ERROR (91.0s)

```text
C error: Command '['C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\build.dir\\decoder_test.exe', '-d', 'wiegand_c', '-t', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\wiegand_c\\default', '-f', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\wiegand_c\\default\\actual_c.json', '--tolerance', '2', '--generate-only']' timed out after 30 seconds
```

## All Decoders

| Decoder | Status | Time (s) | Detail |
|---|---|---|---|
| ad5593r_c | FAIL 🔴 | 3.5 | 60 matches, 2 deviations found. |
| emmc_sd_c | FAIL 🔴 | 2.2 | 1 matches, 110 deviations found. |
| ethernet_c | FAIL 🔴 | 4.8 | 11657 matches, 3 deviations found. |
| mipi_rffe_c | FAIL 🔴 | 2.2 | 21 matches, 5 deviations found. |
| sdcard_sd_c | FAIL 🔴 | 2.4 | 48 matches, 16 deviations found. |
| usb_request_c | FAIL 🔴 | 2.2 | 240 matches, 15 deviations found. |
| ook_c | ERROR 💥 | 91.7 | C error: Command '['C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\build.dir\\decoder_test.exe', '-d', 'ook_c', '-t', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\ook_c\\default', '-f', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\ook_c\\default\\actual_c.json', '--tolerance', '2', '--generate-only']' timed out after 30 seconds |
| ook_oregon_c | ERROR 💥 | 91.8 | C error: Command '['C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\build.dir\\decoder_test.exe', '-d', 'ook_oregon_c', '-t', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\ook_oregon_c\\default', '-f', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\ook_oregon_c\\default\\actual_c.json', '--tolerance', '2', '--generate-only']' timed out after 30 seconds |
| ook_vis_c | ERROR 💥 | 92.0 | C error: Command '['C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\build.dir\\decoder_test.exe', '-d', 'ook_vis_c', '-t', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\ook_vis_c\\default', '-f', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\ook_vis_c\\default\\actual_c.json', '--tolerance', '2', '--generate-only']' timed out after 30 seconds |
| wiegand_c | ERROR 💥 | 91.0 | C error: Command '['C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\build.dir\\decoder_test.exe', '-d', 'wiegand_c', '-t', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\wiegand_c\\default', '-f', 'C:\\Users\\admin\\Downloads\\DSView-main_2026_4_27cppnb\\libsigrokdecode\\tests\\testdata\\wiegand_c\\default\\actual_c.json', '--tolerance', '2', '--generate-only']' timed out after 30 seconds |
| hdlc_c | WARN 🟠 | 2.3 | All 0 annotations match (vacuous - no output from either decoder) |
| qspi_c | DEVIATION 🟡 | 5.2 | 0 matches, 45 deviations found. |
| spi_dual_quad_c | DEVIATION 🟡 | 8.6 | 114 matches, 304 deviations found. |
| 4b5b_c | PASS 🟢 | 4.7 | All 10867 annotations match |
| a7105_c | PASS 🟢 | 3.2 | All 3 annotations match |
| ac97_c | PASS 🟢 | 2.4 | All 510 annotations match |
| ad5626_c | PASS 🟢 | 4.5 | All 3 annotations match |
| ad79x0_c | PASS 🟢 | 3.3 | All 3 annotations match |
| adat_c | PASS 🟢 | 2.9 | All 750 annotations match |
| adb_c | PASS 🟢 | 3.0 | All 26 annotations match |
| ade77xx_c | PASS 🟢 | 3.5 | All 3 annotations match |
| adf435x_c | PASS 🟢 | 3.5 | All 3 annotations match |
| adns5020_c | PASS 🟢 | 3.3 | All 3 annotations match |
| adxl345_c | PASS 🟢 | 4.2 | All 3 annotations match |
| afsk_c | PASS 🟢 | 3.1 | All 2 annotations match |
| am230x_c | PASS 🟢 | 3.4 | All 51 annotations match |
| amulet_ascii_c | PASS 🟢 | 3.4 | All 169 annotations match |
| arm_etmv3_c | PASS 🟢 | 3.5 | All 161 annotations match |
| arm_itm_c | PASS 🟢 | 2.0 | All 169 annotations match |
| arm_tpiu_c | PASS 🟢 | 1.9 | All 161 annotations match |
| arp_c | PASS 🟢 | 4.1 | All 10867 annotations match |
| as5047_c | PASS 🟢 | 2.1 | All 3 annotations match |
| atsha204a_c | PASS 🟢 | 2.0 | All 61 annotations match |
| aud_c | PASS 🟢 | 1.9 | All 1 annotations match |
| avclan_c | PASS 🟢 | 2.1 | All 20 annotations match |
| avr_isp_c | PASS 🟢 | 2.2 | All 3 annotations match |
| avr_pdi_c | PASS 🟢 | 2.1 | All 1 annotations match |
| bean_c | PASS 🟢 | 2.1 | All 71 annotations match |
| bh1750_c | PASS 🟢 | 2.1 | All 61 annotations match |
| bluetooth_h4_c | PASS 🟢 | 2.1 | All 169 annotations match |
| boost_c | PASS 🟢 | 2.2 | All 161 annotations match |
| c2_c | PASS 🟢 | 1.9 | All 9 annotations match |
| caliper_c | PASS 🟢 | 2.0 | All 1 annotations match |
| can_c | PASS 🟢 | 2.0 | All 160 annotations match |
| can_fd_c | PASS 🟢 | 4.3 | All 572 annotations match |
| carrera_c | PASS 🟢 | 1.8 | All 5 annotations match |
| cc1101_c | PASS 🟢 | 2.1 | All 3 annotations match |
| ccd_c | PASS 🟢 | 2.0 | All 99 annotations match |
| cec_c | PASS 🟢 | 1.9 | All 1 annotations match |
| cfp_c | PASS 🟢 | 1.9 | All 72 annotations match |
| cjtag_c | PASS 🟢 | 2.0 | All 80 annotations match |
| cjtag_oscan0_c | PASS 🟢 | 5.7 | All 84 annotations match |
| counter_c | PASS 🟢 | 1.9 | All 10 annotations match |
| crsf_c | PASS 🟢 | 2.0 | All 169 annotations match |
| cyrf6936_c | PASS 🟢 | 2.1 | All 3 annotations match |
| dali_c | PASS 🟢 | 2.0 | All 13 annotations match |
| dcc_c | PASS 🟢 | 1.9 | All 47 annotations match |
| dcf77_c | PASS 🟢 | 1.9 | All 1 annotations match |
| delta-sigma_c | PASS 🟢 | 4.7 | All 768 annotations match |
| dmx512_c | PASS 🟢 | 1.9 | All 82 annotations match |
| ds1307_c | PASS 🟢 | 2.1 | All 60 annotations match |
| ds2408_c | PASS 🟢 | 2.1 | All 59 annotations match |
| ds243x_c | PASS 🟢 | 2.1 | All 131 annotations match |
| ds28ea00_c | PASS 🟢 | 2.1 | All 91 annotations match |
| ds3231_c | PASS 🟢 | 2.2 | All 60 annotations match |
| dsi_c | PASS 🟢 | 2.5 | All 13 annotations match |
| edid_c | PASS 🟢 | 2.4 | All 60 annotations match |
| eeprom24xx_c | PASS 🟢 | 2.4 | All 67 annotations match |
| eeprom93xx_c | PASS 🟢 | 2.3 | All 27 annotations match |
| em4100_c | PASS 🟢 | 2.4 | All 48 annotations match |
| em4305_c | PASS 🟢 | 4.0 | All 1 annotations match |
| enc28j60_c | PASS 🟢 | 2.5 | All 3 annotations match |
| eth_an_c | PASS 🟢 | 4.8 | All 17 annotations match |
| flexray_c | PASS 🟢 | 2.4 | All 20 annotations match |
| fsi_c | PASS 🟢 | 2.3 | All 166 annotations match |
| gpib_c | PASS 🟢 | 2.7 | All 9 annotations match |
| graycode_c | PASS 🟢 | 3.4 | All 35 annotations match |
| guess_bitrate_c | PASS 🟢 | 2.2 | All 2 annotations match |
| hdcp_c | PASS 🟢 | 2.3 | All 60 annotations match |
| hdmi_scdc_c | PASS 🟢 | 2.3 | All 60 annotations match |
| i2c_c | PASS 🟢 | 2.2 | All 86 annotations match |
| i2c_packet_c | PASS 🟢 | 2.1 | All 61 annotations match |
| i2cdemux_c | PASS 🟢 | 2.0 | All 60 annotations match |
| i2cfilter_c | PASS 🟢 | 2.0 | All 60 annotations match |
| i2s_c | PASS 🟢 | 2.0 | All 1 annotations match |
| iebus_c | PASS 🟢 | 1.9 | All 1 annotations match |
| iec_c | PASS 🟢 | 1.9 | All 6 annotations match |
| ieee488_c | PASS 🟢 | 2.0 | All 30 annotations match |
| ipv4_c | PASS 🟢 | 4.4 | All 10867 annotations match |
| ir_irmp_c | PASS 🟢 | 2.6 | All 1 annotations match |
| ir_ltto_c | PASS 🟢 | 2.0 | All 14 annotations match |
| ir_ltto_decode_c | PASS 🟢 | 2.0 | All 14 annotations match |
| ir_nec_c | PASS 🟢 | 3.5 | All 86 annotations match |
| ir_rc5_c | PASS 🟢 | 1.9 | All 19 annotations match |
| ir_rc6_c | PASS 🟢 | 1.9 | All 26 annotations match |
| ir_recoil_c | PASS 🟢 | 1.9 | All 12 annotations match |
| ir_sirc_c | PASS 🟢 | 4.3 | All 18 annotations match |
| iso7816_c | PASS 🟢 | 2.3 | All 2 annotations match |
| j1708_c | PASS 🟢 | 2.3 | All 165 annotations match |
| jitter_c | PASS 🟢 | 2.2 | All 15 annotations match |
| jtag_avr_c | PASS 🟢 | 2.2 | All 16 annotations match |
| jtag_c | PASS 🟢 | 2.3 | All 13 annotations match |
| jtag_ejtag_c | PASS 🟢 | 2.5 | All 16 annotations match |
| jtag_stm32_c | PASS 🟢 | 2.5 | All 16 annotations match |
| lfast_c | PASS 🟢 | 3.8 | All 9997 annotations match |
| lin_c | PASS 🟢 | 2.4 | All 162 annotations match |
| lm75_c | PASS 🟢 | 2.3 | All 61 annotations match |
| lpc_c | PASS 🟢 | 2.2 | All 6 annotations match |
| ltar_smartdevice_c | PASS 🟢 | 2.1 | All 39 annotations match |
| ltar_smartdevice_decode_c | PASS 🟢 | 2.1 | All 1047 annotations match |
| ltc242x_c | PASS 🟢 | 2.5 | All 3 annotations match |
| ltc26x7_c | PASS 🟢 | 2.5 | All 60 annotations match |
| maple_bus_c | PASS 🟢 | 2.2 | All 84 annotations match |
| max6954_c | PASS 🟢 | 2.3 | All 3 annotations match |
| max7219_c | PASS 🟢 | 2.4 | All 3 annotations match |
| mcs48_c | PASS 🟢 | 4.2 | All 1 annotations match |
| mdio_c | PASS 🟢 | 2.3 | All 72 annotations match |
| microwire_c | PASS 🟢 | 2.3 | All 1 annotations match |
| midi_c | PASS 🟢 | 2.4 | All 161 annotations match |
| miller_c | PASS 🟢 | 2.6 | All 4999 annotations match |
| mipi_dsi_c | PASS 🟢 | 2.2 | All 2 annotations match |
| mlx90614_c | PASS 🟢 | 2.1 | All 60 annotations match |
| modbus_c | PASS 🟢 | 2.1 | All 169 annotations match |
| morse_c | PASS 🟢 | 2.0 | All 38 annotations match |
| mpu6050_c | PASS 🟢 | 2.6 | All 60 annotations match |
| mrf24j40_c | PASS 🟢 | 2.5 | All 3 annotations match |
| mvb_c | PASS 🟢 | 4.0 | All 24 annotations match |
| mxc6225xu_c | PASS 🟢 | 2.4 | All 60 annotations match |
| nes_gamepad_c | PASS 🟢 | 2.6 | All 3 annotations match |
| nrf24l01_c | PASS 🟢 | 2.5 | All 3 annotations match |
| nrf905_c | PASS 🟢 | 2.8 | All 3 annotations match |
| nrzi_c | PASS 🟢 | 2.6 | All 768 annotations match |
| numbers_and_state_c | PASS 🟢 | 2.7 | All 16 annotations match |
| nunchuk_c | PASS 🟢 | 2.6 | All 61 annotations match |
| one_single_wire_c | PASS 🟢 | 4.9 | All 42 annotations match |
| onewire_c | PASS 🟢 | 2.6 | All 39 annotations match |
| onewire_link_c | PASS 🟢 | 3.9 | All 39 annotations match |
| onewire_network_c | PASS 🟢 | 3.9 | All 43 annotations match |
| opentherm_c | PASS 🟢 | 2.6 | All 13 annotations match |
| pan1321_c | PASS 🟢 | 2.6 | All 161 annotations match |
| parallel_c | PASS 🟢 | 2.6 | All 5 annotations match |
| pca9571_c | PASS 🟢 | 2.5 | All 61 annotations match |
| pcfx_ctrlr_c | PASS 🟢 | 5.9 | All 39 annotations match |
| pjdl_c | PASS 🟢 | 2.4 | All 8 annotations match |
| pjon_c | PASS 🟢 | 2.4 | All 8 annotations match |
| pn532_c | PASS 🟢 | 2.3 | All 161 annotations match |
| ps2_c | PASS 🟢 | 2.2 | All 15 annotations match |
| ps2_keyboard_c | PASS 🟢 | 2.2 | All 47 annotations match |
| ps2_mouse_c | PASS 🟢 | 2.6 | All 46 annotations match |
| pwm_c | PASS 🟢 | 2.6 | All 14 annotations match |
| pxx1_c | PASS 🟢 | 2.6 | All 14 annotations match |
| qi_c | PASS 🟢 | 2.7 | All 47 annotations match |
| rc_encode_c | PASS 🟢 | 2.7 | All 2 annotations match |
| rfm12_c | PASS 🟢 | 4.5 | All 3 annotations match |
| rgb_led_spi_c | PASS 🟢 | 2.8 | All 3 annotations match |
| rgb_led_ws281x_c | PASS 🟢 | 7.2 | All 26 annotations match |
| rinnai_control_panel_c | PASS 🟢 | 5.2 | All 10 annotations match |
| rpm_c | PASS 🟢 | 2.6 | All 2 annotations match |
| rtc8564_c | PASS 🟢 | 2.4 | All 60 annotations match |
| rvswd_c | PASS 🟢 | 2.2 | All 59 annotations match |
| sae_j1850_vpw_c | PASS 🟢 | 2.3 | All 33 annotations match |
| sbus_futaba_c | PASS 🟢 | 2.5 | All 177 annotations match |
| scs_c | PASS 🟢 | 2.5 | All 167 annotations match |
| sda2506_c | PASS 🟢 | 2.8 | All 17 annotations match |
| sdcard_spi_c | PASS 🟢 | 2.4 | All 3 annotations match |
| sdio_c | PASS 🟢 | 2.5 | All 67 annotations match |
| sdq_c | PASS 🟢 | 3.1 | All 5622 annotations match |
| sent_c | PASS 🟢 | 2.2 | All 1 annotations match |
| seven_segment_c | PASS 🟢 | 6.7 | All 5 annotations match |
| signature_c | PASS 🟢 | 2.3 | All 1 annotations match |
| sipi_c | PASS 🟢 | 2.7 | All 69 annotations match |
| sle44xx_c | PASS 🟢 | 2.8 | All 37 annotations match |
| sony_md_c | PASS 🟢 | 2.4 | All 17 annotations match |
| sony_md_decode_c | PASS 🟢 | 2.4 | All 17 annotations match |
| spacewire_c | PASS 🟢 | 2.3 | All 40 annotations match |
| spdif_c | PASS 🟢 | 2.2 | All 26 annotations match |
| spi_c | PASS 🟢 | 2.2 | All 5 annotations match |
| spi_fast_c | PASS 🟢 | 7.8 | All 4963 annotations match |
| spi_tpm_c | PASS 🟢 | 3.1 | All 3 annotations match |
| spiflash_c | PASS 🟢 | 2.3 | All 3 annotations match |
| ssd1306_c | PASS 🟢 | 2.2 | All 60 annotations match |
| ssi32_c | PASS 🟢 | 7.2 | All 3 annotations match |
| st25dv_c | PASS 🟢 | 2.5 | All 61 annotations match |
| st25r39xx_spi_c | PASS 🟢 | 2.7 | All 3 annotations match |
| st7735_c | PASS 🟢 | 2.3 | All 19 annotations match |
| st7789_c | PASS 🟢 | 2.5 | All 21 annotations match |
| stepper_motor_c | PASS 🟢 | 2.3 | All 18 annotations match |
| streletz_c | PASS 🟢 | 6.8 | All 161 annotations match |
| swd_c | PASS 🟢 | 2.2 | All 5 annotations match |
| swi_c | PASS 🟢 | 1.9 | All 4 annotations match |
| swim_c | PASS 🟢 | 2.1 | All 3 annotations match |
| t55xx_c | PASS 🟢 | 2.0 | All 79 annotations match |
| tca6408a_c | PASS 🟢 | 1.9 | All 61 annotations match |
| tcs3472x_c | PASS 🟢 | 2.0 | All 60 annotations match |
| tdm_audio_c | PASS 🟢 | 4.6 | All 8 annotations match |
| timing_c | PASS 🟢 | 6.4 | All 18 annotations match |
| tlc5620_c | PASS 🟢 | 1.1 | All 1 annotations match |
| tm1637_c | PASS 🟢 | 2.7 | All 27 annotations match |
| tm1638_c | PASS 🟢 | 6.5 | All 55 annotations match |
| tmc_c | PASS 🟢 | 2.9 | All 22 annotations match |
| tmp102_c | PASS 🟢 | 2.7 | All 61 annotations match |
| tpm_fifo_tis_c | PASS 🟢 | 2.7 | All 25 annotations match |
| tpm_tis_i2c_c | PASS 🟢 | 2.5 | All 60 annotations match |
| tpm_tis_spi_c | PASS 🟢 | 2.4 | All 3 annotations match |
| uart_c | PASS 🟢 | 1.3 | All 221 annotations match |
| uart_fast_c | PASS 🟢 | 4.4 | All 1 annotations match |
| udp_c | PASS 🟢 | 4.5 | All 10867 annotations match |
| ufcs_c | PASS 🟢 | 2.9 | All 165 annotations match |
| usb_packet_c | PASS 🟢 | 2.2 | All 35 annotations match |
| usb_power_delivery_c | PASS 🟢 | 2.8 | All 31 annotations match |
| usb_signalling_c | PASS 🟢 | 2.6 | All 154 annotations match |
| x2444m_c | PASS 🟢 | 2.8 | All 3 annotations match |
| xfp_c | PASS 🟢 | 2.8 | All 62 annotations match |
| xy2_100_c | PASS 🟢 | 3.6 | All 37 annotations match |
| z80_c | PASS 🟢 | 1.5 | All 5 annotations match |
