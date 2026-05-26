# Fix C Decoder Annotation Boundaries Spec

## Why
The C decoders for JTAG and PS/2 produced annotation boundaries that didn't match their Python counterparts, causing test failures. The test infrastructure compares Python and C decoder outputs with ±2 sample tolerance, and mismatches in annotation start/end samples or emission frequency caused FAIL results for `jtag_c`, `ps2_c`, `ps2_keyboard_c`, and `ps2_mouse_c`. Additionally, the Python ps2 decoder lacked Python output for stacked decoders, so ps2_keyboard and ps2_mouse Python decoders never received data.

## What Changes
- **JTAG C decoder** (`jtag_c.c`): Changed state annotation emission from "only on state change" to "on every rising TCK edge (except the first)", matching Python decoder behavior
- **PS/2 C decoder** (`ps2_c.c`): Fixed three annotation boundary mismatches:
  1. BIT annotation for stop bit (i=10): changed end from `samplenum` (zero-width) to `bit_ss[10] + half_bitwidth`
  2. Word annotation: changed end from `bit_ss[8]` to `bit_ss[9]`
  3. Stop bit annotation: changed end from `bit_ss[10] + bitwidth` to `bit_ss[10] + half_bitwidth`
- **PS/2 C decoder** (`ps2_c.c`): Fixed proto output end_sample from `samplenum` to `bit_ss[10] + half_bitwidth` to match Python decoder's Python output boundaries
- **PS/2 Python decoder** (`ps2/pd.py`): Added `outputs = ['ps2']`, `Ps2Packet` class, `OUTPUT_PYTHON` registration, and `Ps2Packet` emission in `handle_bits()` so stacked decoders (ps2_keyboard, ps2_mouse) receive data

## Impact
- Affected decoders: `jtag_c`, `ps2_c`, `ps2_keyboard_c`, `ps2_mouse_c`
- Affected code: `libsigrokdecode/c_decoders/jtag_c.c`, `libsigrokdecode/c_decoders/ps2_c.c`, `libsigrokdecode/decoders/ps2/pd.py`
- `ccd_c` was already passing and requires no changes

## ADDED Requirements

### Requirement: JTAG C decoder must emit state annotations on every rising TCK edge
The JTAG C decoder SHALL emit a state annotation on every rising TCK edge after the first one, matching the Python decoder's behavior. The annotation covers the interval from the previous edge to the current edge, using the OLD state as the annotation class.

#### Scenario: JTAG state annotation emission
- **WHEN** a rising TCK edge is detected
- **AND** it is not the first rising edge
- **THEN** the decoder SHALL emit a state annotation from the previous edge sample to the current edge sample, with the annotation class corresponding to the state that was active during that interval

### Requirement: PS/2 C decoder must match Python annotation boundaries
The PS/2 C decoder SHALL produce annotation boundaries that match the Python decoder within ±2 samples:

1. BIT annotations: each bit annotation SHALL span from `bit_ss[i]` to `bit_ss[i+1]` for i=0..9, and from `bit_ss[10]` to `bit_ss[10] + half_bitwidth` for i=10 (stop bit)
2. Word annotation: SHALL span from `bit_ss[1]` to `bit_ss[9]`
3. Stop bit annotation: SHALL span from `bit_ss[10]` to `bit_ss[10] + half_bitwidth`

#### Scenario: PS/2 frame annotation boundaries
- **WHEN** a complete PS/2 frame (start + 8 data + parity + stop) is decoded
- **THEN** the BIT, Word, and Stop bit annotations SHALL have boundaries matching the Python decoder output within ±2 samples tolerance

### Requirement: PS/2 Python decoder must produce Python output for stacked decoders
The PS/2 Python decoder SHALL register a Python output and emit `Ps2Packet` objects when a complete frame is decoded, so that stacked decoders (ps2_keyboard, ps2_mouse) can receive decoded byte data.

#### Scenario: PS/2 stacked decoder data flow
- **WHEN** the PS/2 Python decoder completes decoding a frame
- **THEN** it SHALL emit a `Ps2Packet(val=word, host=host_flag, pok=parity_ok, ack=False)` via `OUTPUT_PYTHON`
- **AND** the stacked decoder's `decode()` method SHALL receive this `Ps2Packet` as the `data` parameter

### Requirement: PS/2 C decoder proto output end_sample must match Python output
The PS/2 C decoder's proto output end_sample SHALL use `bit_ss[10] + half_bitwidth` instead of the raw `samplenum`, matching the Python decoder's `bits[10].es` boundary.

#### Scenario: PS/2 C decoder proto output boundary
- **WHEN** the PS/2 C decoder sends a "BYTE" proto command to a stacked decoder
- **THEN** the end_sample SHALL be `bit_ss[10] + half_bitwidth`, matching the Python decoder's stop bit end boundary

## MODIFIED Requirements
None (these are new alignment requirements, not modifications to existing documented requirements)

## REMOVED Requirements
None
