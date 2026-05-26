#!/usr/bin/env python3
"""Analyze unconfigured decoders to understand their input types and channels."""
import os, re, sys

DECODERS_DIR = '../c_decoders'

unconfigured = [
    'a7105_c', 'ac97_c', 'ad5593r_c', 'ad5626_c', 'ad79x0_c', 'adat_c', 'adb_c',
    'ade77xx_c', 'adf435x_c', 'adns5020_c', 'adxl345_c', 'afsk_c', 'amulet_ascii_c',
    'arm_etmv3_c', 'arm_itm_c', 'arm_tpiu_c', 'as5047_c', 'atsha204a_c', 'avr_isp_c',
    'bh1750_c', 'bluetooth_h4_c', 'boost_c', 'cc1101_c', 'crsf_c', 'cyrf6936_c',
    'delta_sigma_c', 'ds1307_c', 'ds3231_c', 'edid_c', 'eeprom24xx_c', 'enc28j60_c',
    'hdcp_c', 'hdmi_scdc_c', 'i2c_c', 'i2c_packet_c', 'i2cdemux_c', 'i2cfilter_c',
    'j1708_c', 'jitter_c', 'lin_c', 'lm75_c', 'ltc242x_c', 'ltc26x7_c', 'max6954_c',
    'max7219_c', 'midi_c', 'mlx90614_c', 'modbus_c', 'mpu6050_c', 'mrf24j40_c',
    'mxc6225xu_c', 'nes_gamepad_c', 'nrf24l01_c', 'nrf905_c', 'nunchuk_c', 'pan1321_c',
    'pca9571_c', 'pn532_c', 'rfm12_c', 'rgb_led_spi_c', 'rtc8564_c', 'sbus_futaba_c',
    'scs_c', 'sdcard_spi_c', 'seven_segment_c', 'spi_c', 'spi_fast_c', 'spi_tpm_c',
    'spiflash_c', 'ssd1306_c', 'ssi32_c', 'st25dv_c', 'st25r39xx_spi_c', 'streletz_c',
    't55xx_c', 'tca6408a_c', 'tcs3472x_c', 'tmp102_c', 'tpm_tis_i2c_c', 'tpm_tis_spi_c',
    'uart_c', 'uart_fast_c', 'ufcs_c', 'usb_packet_c', 'usb_signalling_c', 'x2444m_c',
    'xfp_c'
]

for d_id in unconfigured:
    c_file = os.path.join(DECODERS_DIR, d_id + '.c')
    if not os.path.exists(c_file):
        print(f'{d_id}: FILE NOT FOUND')
        continue
    with open(c_file, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    # Extract inputs
    inputs = []
    input_match = re.search(r'static\s+const\s+char\s*\*?\s*\w+_inputs\s*\[\]\s*=\s*\{([^}]+)\}', content, re.DOTALL)
    if input_match:
        inputs = [i.strip().strip('"') for i in input_match.group(1).split(',') if i.strip() and 'NULL' not in i]

    # Extract channels
    channels = []
    ch_pat = r'static\s+struct\s+srd_channel\s+\w+_channels\s*\[\]\s*=\s*\{(.*?)\}\s*;'
    ch_match = re.search(ch_pat, content, re.DOTALL)
    if ch_match:
        channels = re.findall(r'\{\s*"([^"]+)"', ch_match.group(1))

    # Extract num_channels
    num_ch = len(channels) if channels else '?'
    nc_match = re.search(r'\.num_channels\s*=\s*(\d+)', content)
    if not channels and nc_match:
        num_ch = nc_match.group(1)

    input_str = ', '.join(inputs) if inputs else 'logic'
    ch_str = ', '.join(channels) if channels else str(num_ch)
    print(f'{d_id}: inputs=[{input_str}] channels=[{ch_str}]')
