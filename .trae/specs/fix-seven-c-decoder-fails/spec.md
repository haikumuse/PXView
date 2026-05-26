# Fix 7 C Decoder FAIL Cases Spec

## Why
7 C decoder implementations diverge from their Python counterparts, causing test FAIL results. The deviations range from simple text format differences (sdio_c) to fundamental state machine logic mismatches (maple_bus_c, rvswd_c with 0 matches). Fixing these will eliminate the remaining FAIL cases.

## What Changes
- Fix qi_c.c annotation sample ranges (start bit, data byte, parity, stop bit, checksum)
- Fix lfast_c.c header field end_sample calculations, sleep bit logic, warning text, and bitpack order
- Fix sipi_c.c inherited lfast_c issues plus its own header/CRC range issues
- Fix maple_bus_c.c fundamental state machine to match Python's start detection and bit reading
- Fix rvswd_c.c START annotation text, STOP detection logic, and state machine alignment
- Fix sdio_c.c missing short-form annotation texts across multiple annotation classes
- Fix usb_power_delivery_c.c missing annotation classes (No EOP, SRC/SNK message type, payload detail) and extra annotation class

## Impact
- Affected code: `libsigrokdecode/c_decoders/qi_c.c`
- Affected code: `libsigrokdecode/c_decoders/lfast_c.c`
- Affected code: `libsigrokdecode/c_decoders/sipi_c.c`
- Affected code: `libsigrokdecode/c_decoders/maple_bus_c.c`
- Affected code: `libsigrokdecode/c_decoders/rvswd_c.c`
- Affected code: `libsigrokdecode/c_decoders/sdio_c.c`
- Affected code: `libsigrokdecode/c_decoders/usb_power_delivery_c.c`
- No breaking changes

## ADDED Requirements

### Requirement: qi_c annotation sample ranges must match Python
The C decoder SHALL output annotations with the same sample ranges as the Python decoder:
- Start bit: `(bytestart, bitsi[0])` not `(bitsi[0], bitsi[0])`
- Data byte: `(bitsi[0], bitsi[8])` not `(bytestart, bitsi[10])`
- Parity bit: `(bitsi[8], bitsi[9])` not `(bitsi[9], bitsi[9])`
- Stop bit: `(bitsi[9], bitsi[10])` not `(bitsi[10], bitsi[10])`
- Checksum: `(bytesi[-1], samplenum)` not `(bytesi[last], bytesi[last])`

#### Scenario: qi_c test passes
- **WHEN** running `python run_all_tests.py --decoder qi_c`
- **THEN** result is PASS with 0 deviations

### Requirement: lfast_c header fields, sleep bit, and bitpack must match Python
The C decoder SHALL:
1. Use actual bit_len from real sample positions for header field end_sample calculations (like Python's `(es_bit - ss_header) / 8`)
2. Fix sleep bit interpretation to match Python's `bit_count==0` after timeout logic
3. Change "Invalid sync pattern" warning to "Wrong Sync Value: AAAA"
4. Fix bitpack to use LSB-first order matching Python's `sum([b << i for i, b in enumerate(bits)])`

#### Scenario: lfast_c test passes
- **WHEN** running `python run_all_tests.py --decoder lfast_c`
- **THEN** result is PASS with 0 deviations

### Requirement: sipi_c must match Python after lfast_c fix
The C decoder SHALL inherit lfast_c fixes and resolve its own header/CRC range issues.

#### Scenario: sipi_c test passes
- **WHEN** running `python run_all_tests.py --decoder sipi_c`
- **THEN** result is PASS with 0 deviations

### Requirement: maple_bus_c state machine must match Python
The C decoder SHALL align its start detection and bit reading logic with Python's:
- Start pattern detection using `wait({0: 'l', 1: 'h'})` equivalent
- Bit reading with `pending_bit` mechanism
- `counta`/`countb` tracking and `initial` flag

#### Scenario: maple_bus_c test passes
- **WHEN** running `python run_all_tests.py --decoder maple_bus_c`
- **THEN** result is PASS with 0 deviations

### Requirement: rvswd_c START/STOP detection must match Python
The C decoder SHALL:
1. Add 'S' short-form text to START annotation
2. Fix STOP detection to use exclusive matching (only third condition matches, not just "among matches")
3. Align state machine so only START annotation is produced for test data that Python only sees START in

#### Scenario: rvswd_c test passes
- **WHEN** running `python run_all_tests.py --decoder rvswd_c`
- **THEN** result is PASS with 0 deviations

### Requirement: sdio_c annotation texts must include all short forms
The C decoder SHALL add missing short-form annotation texts:
- Start bit: add 'S'
- Transmission: add 'T: host'/'T: slave' and 'T'
- Command: add 'CMD0'/'Cmd'/'C'
- Argument: add 'A'
- Stuff bits: add 'SB'/'S'
- CRC: add 'C'
- End bit: add 'E'
- Add missing class 138 (Start of Data) annotation

#### Scenario: sdio_c test passes
- **WHEN** running `python run_all_tests.py --decoder sdio_c`
- **THEN** result is PASS with 0 deviations

### Requirement: usb_power_delivery_c annotation classes must match Python
The C decoder SHALL:
1. Add missing class 8 (No EOP) warning annotation
2. Add missing class 9 (SRC/SNK message type) annotation
3. Add missing class 11 (payload data object detail) annotation
4. Remove or fix extra class 12 annotation that Python doesn't produce

#### Scenario: usb_power_delivery_c test passes
- **WHEN** running `python run_all_tests.py --decoder usb_power_delivery_c`
- **THEN** result is PASS with 0 deviations

## MODIFIED Requirements
None

## REMOVED Requirements
None
