# Tasks

- [ ] Task 1: Add diagnostic fprintf to qi_handle_transition and qi_add_bit
  - [ ] SubTask 1.1: Add fprintf to qi_handle_transition showing: transition length `l`, htl, deq contents, tolerance check results, which branch taken
  - [ ] SubTask 1.2: Add fprintf to qi_add_bit showing: bit value, bits_len, state, whether preamble detected
  - [ ] SubTask 1.3: Add fprintf to qi_process_byte showing: data_val, bits array contents, annotation ranges

- [ ] Task 2: Increase qi_c test data sample_count in generate_testdata.py
  - [ ] SubTask 2.1: Add special case for qi_c to set sample_count=25000 (enough for full packet at 2kHz/1MHz)
  - [ ] SubTask 2.2: Regenerate test data with `python generate_testdata.py --overwrite qi_c`

- [ ] Task 3: Build and run qi_c test to collect diagnostic output
  - [ ] SubTask 3.1: Touch qi_c.c and rebuild with `build_incremental.cmd`
  - [ ] SubTask 3.2: Run `python run_all_tests.py qi_c` and capture diagnostic stderr output
  - [ ] SubTask 3.3: Analyze diagnostic output to identify root cause of 0 annotations

- [ ] Task 4: Fix the root cause bug in qi_c.c
  - [ ] SubTask 4.1: Based on diagnostic analysis, fix the decode logic (likely in qi_handle_transition, qi_add_bit, or qi_decode)
  - [ ] SubTask 4.2: Fix checksum annotation end-sample: store last byte's stop-bit end sample persistently instead of using bitsi[10]
  - [ ] SubTask 4.3: Verify bits_to_uint matches Python's reduce-based implementation

- [ ] Task 5: Remove debug fprintf statements from qi_c.c
  - [ ] SubTask 5.1: Remove all fprintf(stderr, "DBG ...") lines from qi_decode, qi_handle_transition, qi_add_bit, qi_process_byte

- [ ] Task 6: Rebuild and verify test passes
  - [ ] SubTask 6.1: Touch qi_c.c and rebuild
  - [ ] SubTask 6.2: Run `python show_diff.py qi_c` to verify 0 deviations
  - [ ] SubTask 6.3: Run `python run_all_tests.py qi_c` to verify PASS

# Task Dependencies

- Task 1 and Task 2 can run in parallel
- Task 3 depends on Task 1 and Task 2
- Task 4 depends on Task 3
- Task 5 depends on Task 4
- Task 6 depends on Task 5
