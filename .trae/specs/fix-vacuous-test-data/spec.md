# Fix Vacuous Test Data Spec

## Why
70 C decoders produce 0 annotations from both Python and C decoders because the generated test data doesn't trigger decoder output. The generators exist but produce data that doesn't match what the decoders expect (wrong timing, missing idle periods, channel mismatches, wrong protocol structure).

## Current State
- **129 PASS**, **70 WARN**, **15 FAIL**, **0 ERROR**
- All generators have been implemented in `protocol_synthesizer.py`
- All decoder configs exist in `test_factory.py`
- The issue is that the generated data doesn't trigger decoder annotations

## The 70 WARN Decoders

### 1-channel simple (27)
adat_c, adb_c, afsk_c, am230x_c, aud_c, avr_pdi_c, bean_c, c2_c, carrera_c, dali_c, delta-sigma_c, dmx512_c, dsi_c, em4305_c, mvb_c, one_single_wire_c, ook_c, rgb_led_ws281x_c, sae_j1850_vpw_c, sdq_c, sony_md_c, spdif_c, t55xx_c, tdm_audio_c, ir_ltto_c, ir_irmp_c, ir_recoil_c

### Multi-channel bus (20)
ac97_c, emmc_sd_c, ethernet_c, fsi_c, gpib_c, iec_c, lpc_c, mcs48_c, mipi_rffe_c, mipi_dsi_c, pcfx_ctrlr_c, qspi_c, sdcard_sd_c, spi_dual_quad_c, spacewire_c, tlc5620_c, tm1637_c, tm1638_c, tmc_c, z80_c

### Stack decoders (23)
arp_c, ds2408_c, ds243x_c, ds28ea00_c, ieee488_c, ipv4_c, ir_ltto_decode_c, jtag_avr_c, jitter_c, jtag_ejtag_c, jtag_stm32_c, ltar_smartdevice_decode_c, numbers_and_state_c, ook_oregon_c, ook_vis_c, seven_segment_c, signature_c, sony_md_decode_c, st7735_c, st7789_c, tpm_fifo_tis_c, udp_c, usb_request_c

## Root Causes

### Category 1: Generators produce wrong protocol structure
Many generators produce data that looks vaguely like the protocol but doesn't follow the exact timing/state machine that the decoder expects. Examples:
- **DALI**: Manchester timing may not match decoder's edge/half-bit detection
- **DMX512**: BREAK may not be long enough, or MAB timing wrong
- **SPDIF**: BMC timing may not produce 3 distinct pulse widths for clock recovery
- **AM230x**: Start signal timing doesn't match decoder thresholds
- **OOK**: Sends raw bits instead of Manchester-encoded preamble + data + timeout
- **WS281x**: T0H/T1H timing doesn't match decoder thresholds
- **C2**: Reset detection needs long clock high, generator may not produce it

### Category 2: Wrong generator assigned
- **bean_c**: Uses `_gen_uart` but BEAN is not UART, it's pulse-width based
- **numbers_and_state_c**: Uses `_gen_numbers_and_state` which just toggles bits
- **mipi_dsi_c**: Uses `_gen_dsi` which sends raw NRZ, not DSI LP/HS
- **aud_c**: Uses `_gen_i2s` but AUD has 6 channels with different protocol

### Category 3: Channel count mismatches
- **tm1637_c/tm1638_c**: Configured with 2 channels but `_gen_tmc` uses SPI (4 channels)
- **tmc_c**: Configured with 2 channels but `_gen_tmc` uses SPI (4 channels)

### Category 4: Stack decoders depend on upstream WARN decoders
- **4b5b_c → ethernet_c → arp_c/ipv4_c/udp_c**: NRZI upstream must produce annotations
- **ook_c → ook_oregon_c/ook_vis_c**: OOK upstream must produce annotations
- **sony_md_c → sony_md_decode_c**: Sony MD upstream must produce annotations
- **jtag_c → jtag_avr_c/jtag_ejtag_c/jtag_stm32_c**: JTAG upstream must produce annotations
- **onewire → ds2408_c/ds243x_c/ds28ea00_c**: OneWire upstream must produce annotations

## What Changes
- Fix protocol timing in generators to match decoder expectations
- Fix channel count mismatches in DECODER_CONFIG
- Add proper generators for decoders using wrong generators
- Fix stack decoder configurations where upstream decoders need to be fixed first
- Target: Convert at least 50 of 70 WARN decoders to PASS

## Impact
- Affected code: `libsigrokdecode/tests/protocol_synthesizer.py`, `libsigrokdecode/tests/test_factory.py`
- No changes to C decoder source code
- No changes to Python decoder source code
