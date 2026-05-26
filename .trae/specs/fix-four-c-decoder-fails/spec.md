# Fix 4 C Decoder FAIL Cases (avclan_c, iebus_c, ir_nec_c, ir_sirc_c) Spec

## Why
4 C decoder test cases show FAIL results with moderate deviation counts: avclan_c (2 deviations), iebus_c (2 deviations), ir_nec_c (3 deviations), ir_sirc_c (18 deviations). Root causes have been identified and fixes applied to source code, but compilation and verification are pending.

## What Changes
- Fix iebus_c.c: `read_bits()` now saves the first bit's start position so `bits_begin` points to the first bit instead of the last bit
- Fix ir_nec_c.c: `data_ok()` no longer unconditionally resets `data_len` and `ss_bit` when `data_len == want_len`; only resets for extended address mode
- Fix ir_sirc/pd.py: `self.matched` is an integer bitmask, not a tuple; changed `read_pulse()` and `read_bit()` to extract edge/timeout via bitmask operations
- avclan_c.c: No fix needed — it stacks on iebus_c, so the iebus_c fix resolves its deviations too
- Regenerate ir_sirc_c expected_py.json (was empty due to Python decoder crash)

## Impact
- Affected code: `libsigrokdecode/c_decoders/iebus_c.c`, `libsigrokdecode/c_decoders/ir_nec_c.c`, `libsigrokdecode/decoders/ir_sirc/pd.py`
- Affected test data: `libsigrokdecode/tests/testdata/ir_sirc_c/default/expected_py.json`
- avclan_c.c is NOT modified but benefits from iebus_c fix
- No breaking changes

## ADDED Requirements

### Requirement: iebus_c read_bits must save first bit's start position
The `read_bits()` function in iebus_c.c SHALL save the first bit's start position and set `s->bits_begin` to that value after the loop completes, so that the Master address annotation starts at the correct sample position.

#### Scenario: iebus_c Master address annotation range
- **WHEN** running `python run_all_tests.py --decoder iebus_c`
- **THEN** Master address annotation starts at sample 10000 (not 60000)
- **AND** result is PASS with 0 deviations

### Requirement: avclan_c must pass after iebus_c fix
Since avclan_c stacks on iebus_c (inputs = "iebus"), fixing iebus_c SHALL also fix avclan_c's deviations.

#### Scenario: avclan_c test passes
- **WHEN** running `python run_all_tests.py --decoder avclan_c`
- **THEN** result is PASS with 0 deviations

### Requirement: ir_nec_c data_ok must not unconditionally reset state
The `data_ok()` function in ir_nec_c.c SHALL only reset `data_len` and `ss_bit` when `is_extended && state == STATE_ADDRESS`, matching the Python decoder's behavior. The unconditional reset caused wrong Address# values and missing Command annotations.

#### Scenario: ir_nec_c annotation values and ranges
- **WHEN** running `python run_all_tests.py --decoder ir_nec_c`
- **THEN** Address# annotation has correct value and range
- **AND** Command annotation is present
- **AND** no spurious warning annotations
- **AND** result is PASS with 0 deviations

### Requirement: ir_sirc Python decoder must handle self.matched as integer
The ir_sirc Python decoder SHALL treat `self.matched` as an integer bitmask and extract edge/timeout via bitwise AND operations, not tuple unpacking. This fixes the TypeError that prevented any Python annotations from being produced.

#### Scenario: ir_sirc_c test passes
- **WHEN** running `python run_all_tests.py --decoder ir_sirc_c`
- **THEN** result is PASS with 0 deviations

## MODIFIED Requirements
None

## REMOVED Requirements
None
