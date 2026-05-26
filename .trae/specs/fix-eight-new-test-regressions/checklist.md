# Fix 8 New C Decoder Test Regressions — Verification Checklist

## Already Fixed (verified by previous agent)
- [x] am230x_c test PASS — Byte annotation text format fixed
- [x] one_single_wire_c test PASS — Parity check text fixed for Python operator precedence bug
- [x] rgb_led_ws281x_c test PASS — Last bit decoding fixed using c_decoder_get_last_samplenum

## Quick Fixes
- [ ] spdif_c test PASS — "Unknown Preamble" annotation output matches Python
- [ ] dmx512_c test PASS — No extra "Interframe" annotations

## Zero-Annotation Decoders
- [ ] tdm_audio_c test PASS — Frame sync detection works, annotations match Python
- [ ] dali_c test PASS — Manchester decoding works, annotations match Python
- [ ] ook_c test PASS — OOK decoding works, annotations match Python

## Large-Deviation Decoders
- [ ] delta-sigma_c test PASS — Annotation sample ranges match Python
- [ ] t55xx_c test PASS — Annotation classes match Python output

## Build Verification
- [ ] ninja -C build compiles with no errors
- [ ] All 7 remaining decoders verified with `python run_all_tests.py --decoder DECODER_NAME` showing PASS
