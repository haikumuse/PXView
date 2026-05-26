# Fix qi_c Decoder Test Failure Spec

## Why

The `qi_c` C decoder produces 0 annotations while the Python `qi` decoder produces 47 annotations from the same synthesized test data. The C decoder must match the Python decoder's output exactly for the test to pass.

## What Changes

- **qi_c.c**: Fix the decode logic so the C decoder produces annotations matching the Python decoder
- **qi_c.c**: Remove debug `fprintf(stderr, ...)` statements added during prior debugging
- **generate_testdata.py**: Increase `sample_count` for `qi_c` test data (default 10000 is insufficient; Qi packet at 2kHz/1MHz needs ~19000 samples minimum)
- **qi_c.c**: Fix checksum annotation end-sample reference (`bitsi[10]` is invalid after `bits_len` is reset between bytes)

## Impact

- Affected code: `libsigrokdecode/c_decoders/qi_c.c`, `libsigrokdecode/tests/generate_testdata.py`
- Affected tests: `qi_c` decoder test in `run_all_tests.py`

## ADDED Requirements

### Requirement: qi_c decoder produces annotations matching Python qi decoder

The C decoder SHALL produce the same annotations (class, start_sample, end_sample, text) as the Python decoder for the same input data.

#### Scenario: Preamble detection
- **GIVEN** synthesized Qi test data with preamble [1,1,1,1,0]
- **WHEN** the C decoder processes the data
- **THEN** the preamble SHALL be detected and the state SHALL transition from IDLE to DATA
- **AND** bit annotations SHALL be output for each bit in the DATA state

#### Scenario: Byte processing
- **WHEN** 11 bits are accumulated in DATA state
- **THEN** `qi_process_byte` SHALL be called
- **AND** annotations SHALL be output for start bit, data byte, parity bit, and stop bit with correct ranges

#### Scenario: Packet processing
- **WHEN** a complete packet is accumulated
- **THEN** `qi_process_packet` SHALL be called
- **AND** packet data and checksum annotations SHALL be output with correct ranges

### Requirement: qi_c test data has sufficient sample count

The test data for `qi_c` SHALL have enough samples to contain at least one complete Qi packet (preamble + header + data + checksum).

#### Scenario: Test data generation
- **WHEN** `generate_testdata.py` generates test data for `qi_c`
- **THEN** `sample_count` SHALL be at least 20000 (to accommodate a full packet at 2kHz/1MHz)
- **AND** the input.bin SHALL contain valid differential bi-phase encoded Qi data

## MODIFIED Requirements

### Requirement: Checksum annotation end-sample

The checksum annotation in `qi_process_packet` SHALL use the last byte's stop bit end sample (not `bitsi[10]` which is invalid after `bits_len` is reset).

#### Scenario: Checksum annotation range
- **WHEN** a packet is complete and checksum is verified
- **THEN** the checksum annotation SHALL span from `bytesi[last]` to the stop bit end sample of the last byte
- **AND** the stop bit end sample SHALL be stored persistently (not in `bitsi` which is cleared between bytes)

## REMOVED Requirements

(none)
