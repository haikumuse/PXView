# Tasks

- [x] Task 1: Fix iebus_c.c — read_bits() first bit start position
  - [x] SubTask 1.1: Save first_begin on first iteration in read_bits()
  - [x] SubTask 1.2: Set s->bits_begin = first_begin after loop

- [x] Task 2: Fix ir_nec_c.c — data_ok() unconditional state reset
  - [x] SubTask 2.1: Only reset data_len and ss_bit when is_extended && state == STATE_ADDRESS

- [x] Task 3: Fix ir_sirc/pd.py — self.matched integer bitmask
  - [x] SubTask 3.1: Change read_pulse() to extract edge/timeout via bitmask operations
  - [x] SubTask 3.2: Change read_bit() to extract timeout via bitmask operation
  - [x] SubTask 3.3: Regenerate expected_py.json for ir_sirc_c

- [x] Task 4: Compile all fixes
  - [x] SubTask 4.1: Touch iebus_c.c to update timestamp
  - [x] SubTask 4.2: Run ninja -C build to compile iebus_c.dll and ir_nec_c.dll
  - [x] SubTask 4.3: No compilation errors

- [x] Task 5: Verify all 4 decoders pass
  - [x] SubTask 5.1: python run_all_tests.py --decoder avclan_c → PASS
  - [x] SubTask 5.2: python run_all_tests.py --decoder iebus_c → PASS
  - [x] SubTask 5.3: python run_all_tests.py --decoder ir_nec_c → PASS
  - [x] SubTask 5.4: python run_all_tests.py --decoder ir_sirc_c → PASS

# Task Dependencies
- Task 4 depends on Tasks 1-3 (source code fixes must be in place)
- Task 5 depends on Task 4 (DLLs must be rebuilt before testing)
- Tasks 1, 2, 3 are already completed (source code changes applied)
