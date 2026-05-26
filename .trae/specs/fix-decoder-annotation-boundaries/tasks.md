# Tasks

- [x] Task 1: Fix JTAG C decoder to emit state annotations on every rising TCK edge (except first)
  - [x] SubTask 1.1: Replace `if (newstate != oldstate)` guard with `if (priv->first)` check
  - [x] SubTask 1.2: Move `ss_state = samplenum` outside the conditional block
  - [x] SubTask 1.3: Rebuild jtag_c decoder DLL
  - [x] SubTask 1.4: Verify jtag_c test passes

- [x] Task 2: Fix PS/2 C decoder annotation boundaries
  - [x] SubTask 2.1: Add `bitwidth`/`half_bitwidth` calculation
  - [x] SubTask 2.2: Fix stop bit BIT annotation end from `samplenum` to `bit_ss[10] + half_bitwidth`
  - [x] SubTask 2.3: Fix Word annotation end from `bit_ss[8]` to `bit_ss[9]`
  - [x] SubTask 2.4: Fix Stop bit annotation end from `bitwidth` to `half_bitwidth`
  - [x] SubTask 2.5: Rebuild ps2_c decoder DLL
  - [x] SubTask 2.6: Verify ps2_c test passes

- [x] Task 3: Add Python output to Python ps2 decoder for stacked decoders
  - [x] SubTask 3.1: Add `outputs = ['ps2']` to ps2 Python decoder
  - [x] SubTask 3.2: Add `Ps2Packet` class definition
  - [x] SubTask 3.3: Register `OUTPUT_PYTHON` output in `start()`
  - [x] SubTask 3.4: Emit `Ps2Packet` in `handle_bits()` when frame is complete
  - [x] SubTask 3.5: Fix C ps2_c proto output end_sample from `samplenum` to `bit_ss[10] + half_bitwidth`
  - [x] SubTask 3.6: Rebuild ps2_c decoder DLL

- [x] Task 4: Verify all decoder tests pass
  - [x] SubTask 4.1: Run `ps2_keyboard_c` test and confirm PASS
  - [x] SubTask 4.2: Run `ps2_mouse_c` test and confirm PASS
  - [x] SubTask 4.3: Run `ccd_c` test and confirm PASS
  - [x] SubTask 4.4: Run `jtag_c` test and confirm PASS
  - [x] SubTask 4.5: Run `ps2_c` test and confirm PASS

# Task Dependencies
- [Task 2] depends on [Task 1] (independent, can run in parallel)
- [Task 3] depends on [Task 2] (ps2_c must be fixed first)
- [Task 4] depends on [Task 1], [Task 2], and [Task 3]
