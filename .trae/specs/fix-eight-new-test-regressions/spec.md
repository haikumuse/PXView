# Fix 8 New C Decoder Test Regressions Spec

## Why
After test data generator improvements, 8 decoders regressed from WARN (empty truth) to FAIL. The generators now produce valid protocol data that Python decoders can process, but the C decoders either produce 0 annotations or mismatched annotations. Three have already been fixed (am230x_c, one_single_wire_c, rgb_led_ws281x_c); 7 remain.

## What Changes
- Fix tdm_audio_c.c — C decoder produces 0 annotations while Python produces 8 (frame sync detection failure)
- Fix dali_c.c — C decoder produces 0 annotations while Python produces 13 (Manchester decoding issue)
- Fix dmx512_c.c — C has 5 extra "Interframe" annotations Python doesn't produce (80 matches, 5 deviations)
- Fix ook_c.c — C decoder produces 0 annotations while Python produces 18
- Fix spdif_c.c — Python outputs "Unknown Preamble" at sample 180 that C doesn't (18 matches, 1 deviation)
- Fix delta-sigma_c.c — sample range/grouping mismatch between C and Python (192 matches, 190 deviations)
- Fix t55xx_c.c — Python only outputs Start gap/Write gap (cls 1,2) while C outputs much more detailed annotations (76 matches, 180 deviations)
- May need to fix `protocol_synthesizer.py` generators if the issue is in test data rather than C decoder logic

## Impact
- Affected code: `libsigrokdecode/c_decoders/tdm_audio_c.c`
- Affected code: `libsigrokdecode/c_decoders/dali_c.c`
- Affected code: `libsigrokdecode/c_decoders/dmx512_c.c`
- Affected code: `libsigrokdecode/c_decoders/ook_c.c`
- Affected code: `libsigrokdecode/c_decoders/spdif_c.c`
- Affected code: `libsigrokdecode/c_decoders/delta-sigma_c.c`
- Affected code: `libsigrokdecode/c_decoders/t55xx_c.c`
- Possibly affected: `libsigrokdecode/tests/protocol_synthesizer.py` (if generator fixes needed)
- No breaking changes

## ADDED Requirements

### Requirement: tdm_audio_c must produce annotations matching Python
The C decoder SHALL detect frame sync and output audio data annotations matching the Python decoder. Currently produces 0 annotations because frame sync detection (`frame != s->lastframe && frame == 1` at CLK rising edge) fails. The generator's SYNC pulse timing was already extended but the C decoder still doesn't detect frames.

#### Scenario: tdm_audio_c test passes
- **WHEN** running `python run_all_tests.py --decoder tdm_audio_c`
- **THEN** result is PASS with 0 deviations

### Requirement: dali_c must produce annotations matching Python
The C decoder SHALL decode Manchester-encoded DALI frames matching the Python decoder. Currently produces 0 annotations — the Manchester phase detection logic may not match the generator's timing.

#### Scenario: dali_c test passes
- **WHEN** running `python run_all_tests.py --decoder dali_c`
- **THEN** result is PASS with 0 deviations

### Requirement: dmx512_c must not produce extra Interframe annotations
The C decoder SHALL match Python's annotation output exactly. Currently has 5 extra "Interframe" annotations that Python doesn't produce.

#### Scenario: dmx512_c test passes
- **WHEN** running `python run_all_tests.py --decoder dmx512_c`
- **THEN** result is PASS with 0 deviations

### Requirement: ook_c must produce annotations matching Python
The C decoder SHALL decode OOK-modulated signals matching the Python decoder. Currently produces 0 annotations while Python produces 18.

#### Scenario: ook_c test passes
- **WHEN** running `python run_all_tests.py --decoder ook_c`
- **THEN** result is PASS with 0 deviations

### Requirement: spdif_c must output Unknown Preamble matching Python
The C decoder SHALL output "Unknown Preamble" annotation when Python does. Currently missing 1 annotation that Python produces.

#### Scenario: spdif_c test passes
- **WHEN** running `python run_all_tests.py --decoder spdif_c`
- **THEN** result is PASS with 0 deviations

### Requirement: delta-sigma_c annotation ranges must match Python
The C decoder SHALL output annotations with the same sample ranges and grouping as the Python decoder. Currently 190 deviations out of 382 total.

#### Scenario: delta-sigma_c test passes
- **WHEN** running `python run_all_tests.py --decoder delta-sigma_c`
- **THEN** result is PASS with 0 deviations

### Requirement: t55xx_c annotation output must match Python
The C decoder SHALL only output annotation classes that Python outputs. Currently outputs detailed annotations (cls 0,4,5) that Python doesn't produce, causing 180 deviations.

#### Scenario: t55xx_c test passes
- **WHEN** running `python run_all_tests.py --decoder t55xx_c`
- **THEN** result is PASS with 0 deviations

## MODIFIED Requirements
None

## REMOVED Requirements
None
