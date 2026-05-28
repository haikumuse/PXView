# Tasks

- [x] Task 1: Add diagnostic fprintf to qi_handle_transition and qi_add_bit
  - [x] SubTask 1.1: Add fprintf to qi_handle_transition showing: transition length `l`, htl, deq contents, tolerance check results, which branch taken
  - [x] SubTask 1.2: Add fprintf to qi_add_bit showing: bit value, bits_len, state, whether preamble detected
  - [x] SubTask 1.3: Add fprintf to qi_process_byte showing: data_val, bits array contents, annotation ranges

- [x] Task 2: Increase qi_c test data sample_count in generate_testdata.py
  - [x] SubTask 2.1: Add special case for qi_c to set sample_count=25000 (enough for full packet at 2kHz/1MHz)
  - [x] SubTask 2.2: Regenerate test data with `python generate_testdata.py --overwrite qi_c`

- [x] Task 3: Build and run qi_c test to collect diagnostic output
  - [x] SubTask 3.1: Touch qi_c.c and rebuild with `build_incremental.cmd`
  - [x] SubTask 3.2: Run `python run_all_tests.py qi_c` and capture diagnostic stderr output
  - [x] SubTask 3.3: Analyze diagnostic output to identify root cause of 0 annotations

- [x] Task 4: Fix the root cause bugs in qi_c.c
  - [x] SubTask 4.1: Remove `s->deq_len = 0` from BACK TO IDLE branch (Python doesn't clear deque)
  - [x] SubTask 4.2: Increase bits/bitsi array size from 12 to 64 (Python's list is unbounded)
  - [x] SubTask 4.3: Change checksum error annotation class from ANN_CHECKSUM_ERR (7) to ANN_CHECKSUM_OK (6) (Python always uses class 6)

- [x] Task 5: Remove debug fprintf statements from qi_c.c
  - [x] SubTask 5.1: Remove all fprintf(stderr, "DBG ...") lines from qi_decode, qi_handle_transition, qi_add_bit

- [x] Task 6: Rebuild and verify test passes
  - [x] SubTask 6.1: Touch qi_c.c and rebuild
  - [x] SubTask 6.2: Run `python show_diff.py qi_c` to verify 0 deviations
  - [x] SubTask 6.3: Run `python run_all_tests.py --decoder qi_c` to verify PASS

# Task Dependencies

- Task 1 and Task 2 can run in parallel
- Task 3 depends on Task 1 and Task 2
- Task 4 depends on Task 3
- Task 5 depends on Task 4
- Task 6 depends on Task 5
