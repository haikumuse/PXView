# Tasks

## Phase 1: Simple text-format fixes (sdio_c, usb_power_delivery_c)

- [ ] Task 1: Fix sdio_c.c — add missing short-form annotation texts
  - [ ] SubTask 1.1: Run `python show_diff.py sdio_c` to see exact annotation differences
  - [ ] SubTask 1.2: Read sdio_c.c and sdio/pd.py to identify all text format gaps
  - [ ] SubTask 1.3: Add missing 'S' to Start bit annotation texts
  - [ ] SubTask 1.4: Add missing 'T: host'/'T: slave' and 'T' to Transmission annotation texts
  - [ ] SubTask 1.5: Add missing 'CMD0'/'Cmd'/'C' to Command annotation texts
  - [ ] SubTask 1.6: Add missing 'A' to Argument annotation texts
  - [ ] SubTask 1.7: Add missing 'SB'/'S' to Stuff bits annotation texts
  - [ ] SubTask 1.8: Add missing 'C' to CRC annotation texts
  - [ ] SubTask 1.9: Add missing 'E' to End bit annotation texts
  - [ ] SubTask 1.10: Add missing class 138 (Start of Data) annotation output
  - [ ] SubTask 1.11: Fix CMD0 description from 'GO_IDLE_STATE' to 'Reset all SD cards' if needed

- [ ] Task 2: Fix usb_power_delivery_c.c — missing and extra annotation classes
  - [ ] SubTask 2.1: Run `python show_diff.py usb_power_delivery_c` to see exact annotation differences
  - [ ] SubTask 2.2: Read usb_power_delivery_c.c and usb_power_delivery/pd.py to identify class gaps
  - [ ] SubTask 2.3: Add missing class 8 (No EOP) warning annotation
  - [ ] SubTask 2.4: Add missing class 9 (SRC/SNK message type) annotation
  - [ ] SubTask 2.5: Add missing class 11 (payload data object detail) annotation
  - [ ] SubTask 2.6: Fix extra class 12 annotation that Python doesn't produce

## Phase 2: Annotation range fixes (qi_c)

- [ ] Task 3: Fix qi_c.c — annotation sample ranges
  - [ ] SubTask 3.1: Run `python show_diff.py qi_c` to see exact annotation differences
  - [ ] SubTask 3.2: Read qi_c.c and qi/pd.py to confirm all range differences
  - [ ] SubTask 3.3: Fix data byte annotation range from (bytestart, bitsi[10]) to (bitsi[0], bitsi[8])
  - [ ] SubTask 3.4: Fix parity bit range from (bitsi[9], bitsi[9]) to (bitsi[8], bitsi[9])
  - [ ] SubTask 3.5: Fix stop bit range from (bitsi[10], bitsi[10]) to (bitsi[9], bitsi[10])
  - [ ] SubTask 3.6: Fix checksum annotation range from (bytesi[last], bytesi[last]) to (bytesi[-1], samplenum) equivalent
  - [ ] SubTask 3.7: Verify start bit range fix (already done: bytestart to bitsi[0])

## Phase 3: Header/bitpack fixes (lfast_c, sipi_c)

- [ ] Task 4: Fix lfast_c.c — header fields, sleep bit, warning text, bitpack
  - [ ] SubTask 4.1: Run `python show_diff.py lfast_c` to see exact annotation differences
  - [ ] SubTask 4.2: Read lfast_c.c and lfast/pd.py to confirm all differences
  - [ ] SubTask 4.3: Fix header field end_sample calculations to use actual bit_len from real sample positions
  - [ ] SubTask 4.4: Fix sleep bit interpretation to match Python's bit_count==0 after timeout logic
  - [ ] SubTask 4.5: Change "Invalid sync pattern" to "Wrong Sync Value: AAAA"
  - [ ] SubTask 4.6: Fix bitpack to use LSB-first order matching Python's bitpack()
  - [ ] SubTask 4.7: Remove extra bit annotation at sample 370 that Python doesn't produce

- [ ] Task 5: Fix sipi_c.c — inherited lfast_c issues plus own differences
  - [ ] SubTask 5.1: Run `python show_diff.py sipi_c` to see exact annotation differences
  - [ ] SubTask 5.2: Read sipi_c.c and sipi/pd.py to confirm all differences
  - [ ] SubTask 5.3: Fix header field range issues (same as lfast_c)
  - [ ] SubTask 5.4: Fix "Header too short" annotation position
  - [ ] SubTask 5.5: Fix sleep bit annotation position

## Phase 4: Fundamental state machine rewrites (maple_bus_c, rvswd_c)

- [ ] Task 6: Fix maple_bus_c.c — fundamental state machine alignment
  - [ ] SubTask 6.1: Run `python show_diff.py maple_bus_c` to see exact annotation differences
  - [ ] SubTask 6.2: Read maple_bus_c.c and maple_bus/pd.py thoroughly
  - [ ] SubTask 6.3: Align start pattern detection with Python's wait({0: 'l', 1: 'h'})
  - [ ] SubTask 6.4: Implement pending_bit mechanism matching Python
  - [ ] SubTask 6.5: Implement counta/countb tracking and initial flag
  - [ ] SubTask 6.6: Align bit reading with Python's wait([{0: 'f'}, {1: 'f'}])

- [ ] Task 7: Fix rvswd_c.c — START text, STOP detection, state machine
  - [ ] SubTask 7.1: Run `python show_diff.py rvswd_c` to see exact annotation differences
  - [ ] SubTask 7.2: Read rvswd_c.c and rvswd/pd.py thoroughly
  - [ ] SubTask 7.3: Add 'S' short-form text to START annotation
  - [ ] SubTask 7.4: Fix STOP detection to use exclusive matching (only third condition matches)
  - [ ] SubTask 7.5: Align state machine so only START is produced for test data where Python only sees START

## Phase 5: Compile and verify

- [ ] Task 8: Compile all fixes
  - [ ] SubTask 8.1: Run `ninja -C build` to compile all modified C decoders
  - [ ] SubTask 8.2: Ensure no compilation errors or warnings

- [ ] Task 9: Verify each decoder passes
  - [ ] SubTask 9.1: `python run_all_tests.py --decoder sdio_c`
  - [ ] SubTask 9.2: `python run_all_tests.py --decoder usb_power_delivery_c`
  - [ ] SubTask 9.3: `python run_all_tests.py --decoder qi_c`
  - [ ] SubTask 9.4: `python run_all_tests.py --decoder lfast_c`
  - [ ] SubTask 9.5: `python run_all_tests.py --decoder sipi_c`
  - [ ] SubTask 9.6: `python run_all_tests.py --decoder maple_bus_c`
  - [ ] SubTask 9.7: `python run_all_tests.py --decoder rvswd_c`

# Task Dependencies
- Task 5 (sipi_c) depends on Task 4 (lfast_c) — sipi_c inherits lfast_c issues
- Task 8 depends on Tasks 1-7
- Task 9 depends on Task 8
- Tasks 1, 2, 3, 6, 7 can be parallelized (independent decoders)
