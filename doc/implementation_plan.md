# Optimizing Libsigrokdecode Test Harness

We have successfully reduced the number of `WARN` (vacuous match) tests from 43 to 36 and increased `PASS` tests to 61 by integrating `protocol_synthesizer.py` correctly and injecting the appropriate `samplerate` configurations to slow-speed generators.

However, there are still 36 WARNs, 10 FAILs, and 6 ERRORs. Below is the proposed plan to systematically resolve them.

## User Review Required

> [!WARNING]
> Several C decoders (e.g., `hdlc_c`) are crashing with access violations (`Exited with code 3221225477`). Fixing these requires modifying the C source code in `c_decoders/`. Does the user approve modifying the C decoder source code, or should we skip these crashing tests for now?

## Open Questions

- For the `FAIL` cases where the C decoder outputs annotations that differ slightly in timing from the Python decoder (e.g., `graycode_c`, `miller_c`), should we adjust the C decoder logic to perfectly match Python, or add a wider tolerance window to the test runner?

## Proposed Changes

### 1. Fix ERROR States (Option Type Mismatches)
Decoders like `uart_c`, `uart_fast_c`, and `stepper_motor_c` fail with: `Option 'X' should have the same type as the default value.`
This occurs because the test harness generates `config.json` with arbitrary types (e.g., parsing a float for an integer option).
- **Action**: Modify `generate_testdata.py` to inspect `decoder_info["options"]` and strictly enforce the exact Python type (int, float, str) defined by the option's default value.

### 2. Fix WARN States (Protocol Stimuli Mismatches)
The 36 remaining WARNs are primarily due to insufficient or incorrect test data generation:

- **`st7735_c` / `st7789_c`**: The Python decoder waits for a *subsequent* command to flush the current command's description. 
  - **Fix**: Update `generate_testdata.py` to send *two* commands sequentially to force annotation emission.
- **`dsi_c` / `mipi_dsi_c`**: The Python decoder uses strict sample comparisons (`== (edges[-1] + int(halfbit * 1.5))`). Due to float-to-int rounding at 1MHz, edges are missed.
  - **Fix**: Set `synth_sr = 600000` (600kHz) where `1.5 * halfbit` equates to a perfect integer, eliminating rounding errors.
- **`qspi_c` / `spi_dual_quad_c`**: They expect 4-bit parallel data lines, but we only send 1-bit SPI.
  - **Fix**: Add a lightweight `QSPIGenerator` to `protocol_synthesizer.py` that pulses `io0` through `io3` simultaneously.
- **`z80_c`, `mcs48_c`, `maple_bus_c`**: Need longer signal patterns or specific opcode sequences to register an instruction cycle.

### 3. Fix FAIL States (Timing/Logic Discrepancies)
Decoders like `miller_c`, `lfast_c`, `qi_c`, and `graycode_c` successfully decode but produce thousands of deviations.
- **Action**: Use the `view_file` tool to compare `actual_c.json` and `expected_py.json` for one of these failing tests. We will identify whether the offset is systematic (e.g., off by 1 sample) and fix the corresponding `c_decoders/<decoder>.c` logic.

## Verification Plan

### Automated Tests
Run the full test suite to verify the changes:
```bash
python run_all_tests.py --all --jobs 16
```
The goal is to reach 0 WARNs and 0 ERRORs, and significantly reduce the FAIL count.
