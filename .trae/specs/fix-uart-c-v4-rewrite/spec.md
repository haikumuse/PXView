# Fix uart_c.c v4 API Rewrite Bugs Spec

## Why

The uart_c.c decoder was rewritten to use the v4 C decoder API, but contains a critical compilation error (`c_cond_either_edge` does not exist) and several protocol output format mismatches with the Python source (pd.py). These bugs prevent compilation and produce incorrect protocol output for downstream decoders.

## What Changes

- Fix `c_cond_either_edge(b, rxtx)` → `c_cond_edge(b, rxtx)` on line 1102 (compilation blocker)
- Fix FRAME protocol output: swap field order to match Python `[ptype, rxtx, pdata]` convention
- Fix IDLE protocol output: add missing `0` data value to match Python `['IDLE', rxtx, 0]`
- Fix BREAK protocol output: add missing `0` data value to match Python `['BREAK', rxtx, 0]`

## Impact

- Affected code: `libsigrokdecode/c_decoders/uart_c.c`
- Affected specs: C decoder API v4 correctness, UART upper-layer decoder compatibility (LIN, MODBUS, etc.)
- Reference: `libsigrokdecode/decoders/uart/pd.py` (ground truth)

---

## ADDED Requirements

### Requirement: c_cond_edge for either-edge detection

The UART decoder SHALL use `c_cond_edge(b, rxtx)` for either-edge condition building, matching the Python `{rxtx: 'e'}` condition. The function `c_cond_either_edge` does not exist in the codebase.

#### Scenario: Either-edge condition in decode loop
- **WHEN** the decode loop builds the edge condition for BREAK detection
- **THEN** it calls `c_cond_edge(b, rxtx)`, which is the correct function name used by 50+ other C decoders

### Requirement: Protocol output field order matches Python convention

All `c_proto()` calls SHALL follow the Python convention `[ptype, rxtx, pdata]`, where `rxtx` is the second field after the command string.

#### Scenario: FRAME protocol output
- **WHEN** `handle_frame()` outputs a FRAME protocol message
- **THEN** the field order is `c_proto(di, ss, es, out, "FRAME", C_I32(rxtx), C_U8(datavalue), C_U8(frame_valid), NULL)`
- **AND** this matches Python `['FRAME', rxtx, (datavalue, frame_valid)]`

### Requirement: IDLE and BREAK protocol outputs include data value

The IDLE and BREAK protocol outputs SHALL include the `0` data value as the third field, matching the Python output format.

#### Scenario: IDLE protocol output
- **WHEN** `handle_idle()` outputs an IDLE protocol message
- **THEN** the output is `c_proto(di, ss, es, out, "IDLE", C_I32(rxtx), C_U8(0), NULL)`
- **AND** this matches Python `['IDLE', rxtx, 0]`

#### Scenario: BREAK protocol output
- **WHEN** `handle_break()` outputs a BREAK protocol message
- **THEN** the output is `c_proto(di, ss, es, out, "BREAK", C_I32(rxtx), C_U8(0), NULL)`
- **AND** this matches Python `['BREAK', rxtx, 0]`

## MODIFIED Requirements

None.

## REMOVED Requirements

None.
