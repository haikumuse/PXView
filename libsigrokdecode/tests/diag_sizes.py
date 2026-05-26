#!/usr/bin/env python3
"""Diagnostic script to check input.bin sizes and config for WARN decoders."""
import os, json

TESTDATA = 'testdata'
warn_list = [
    'ir_nec_c', 'nrzi_c', 'dali_c', 'i2s_c', 'dmx512_c', 'spdif_c',
    'gpib_c', 'z80_c', 'lpc_c', 'mcs48_c', 'rgb_led_ws281x_c',
    'am230x_c', 'bean_c', 'avr_pdi_c', 'c2_c', 'mipi_rffe_c',
    'sdq_c', 'swim_c', 'onewire_c', 'morse_c', 'em4100_c',
    'tdm_audio_c', 'tlc5620_c', 'delta-sigma_c', 'ieee488_c',
    'fsi_c', 'iec_c', 'mvb_c', 'ook_c', 'sony_md_c',
    'spacewire_c', 'spi_dual_quad_c', 'st7735_c', 'st7789_c',
    'sae_j1850_vpw_c', 'sdcard_sd_c', 'carrera_c', 'dsi_c',
    'one_single_wire_c', 'numbers_and_state_c', 'signature_c',
    'qspi_c', 'em4305_c', 'emmc_sd_c',
]

for d in warn_list:
    p = os.path.join(TESTDATA, d, 'default')
    if not os.path.exists(os.path.join(p, 'config.json')):
        print(f"{d:30} MISSING CONFIG")
        continue
    cfg = json.load(open(os.path.join(p, 'config.json')))
    bin_path = os.path.join(p, 'input.bin')
    bin_size = os.path.getsize(bin_path) if os.path.exists(bin_path) else 0
    sr = cfg.get('samplerate', 1000000)
    nc = cfg.get('num_channels', 2)
    sc = cfg.get('sample_count', 20000)
    expected_size = ((sc + 7) // 8) * nc
    match = "OK" if bin_size == expected_size else f"MISMATCH(exp={expected_size})"
    print(f"{d:30} sr={sr:>10} ch={nc:2} sc={sc:>8} bin={bin_size:>8} {match}")
