# Tasks

## Phase 1: Quick fixes — small deviation counts (spdif_c, dmx512_c)

- [ ] Task 1: Fix spdif_c.c — 1 deviation (missing "Unknown Preamble")
  - [ ] SubTask 1.1: Run `python show_diff.py spdif_c` to see exact difference
  - [ ] SubTask 1.2: Read spdif_c.c and spdif/pd.py to understand preamble detection logic
  - [ ] SubTask 1.3: Fix C decoder to output "Unknown Preamble" when Python does
  - [ ] SubTask 1.4: Compile and verify with `python run_all_tests.py --decoder spdif_c`

- [ ] Task 2: Fix dmx512_c.c — 5 deviations (extra Interframe annotations)
  - [ ] SubTask 2.1: Run `python show_diff.py dmx512_c` to see exact differences
  - [ ] SubTask 2.2: Read dmx512_c.c and dmx512/pd.py to understand Interframe logic
  - [ ] SubTask 2.3: Fix C decoder to not produce extra Interframe annotations
  - [ ] SubTask 2.4: Compile and verify with `python run_all_tests.py --decoder dmx512_c`

## Phase 2: Zero-annotation decoders (tdm_audio_c, dali_c, ook_c)

- [ ] Task 3: Fix tdm_audio_c.c — 0 matches, 8 deviations (frame sync detection failure)
  - [ ] SubTask 3.1: Run `python show_diff.py tdm_audio_c` to see Python output
  - [ ] SubTask 3.2: Read tdm_audio_c.c and tdm_audio/pd.py to compare frame sync logic
  - [ ] SubTask 3.3: Debug why C decoder's `c_cond_rise(cb, CLK)` + `c_decoder_get_pin(di, FRAME, samplenum)` fails to detect SYNC
  - [ ] SubTask 3.4: Examine generated test data (input.bin) to verify SYNC signal timing
  - [ ] SubTask 3.5: Fix C decoder frame sync detection or generator SYNC timing
  - [ ] SubTask 3.6: Compile and verify with `python run_all_tests.py --decoder tdm_audio_c`

- [ ] Task 4: Fix dali_c.c — 0 matches, 13 deviations (Manchester decoding issue)
  - [ ] SubTask 4.1: Run `python show_diff.py dali_c` to see Python output
  - [ ] SubTask 4.2: Read dali_c.c and dali/pd.py to compare Manchester decoding logic
  - [ ] SubTask 4.3: Debug why C decoder's Manchester phase detection fails with generated data
  - [ ] SubTask 4.4: Fix C decoder Manchester decoding or generator timing
  - [ ] SubTask 4.5: Compile and verify with `python run_all_tests.py --decoder dali_c`

- [ ] Task 5: Fix ook_c.c — 0 matches, 18 deviations
  - [ ] SubTask 5.1: Run `python show_diff.py ook_c` to see Python output
  - [ ] SubTask 5.2: Read ook_c.c and ook/pd.py to compare OOK decoding logic
  - [ ] SubTask 5.3: Debug why C decoder produces 0 annotations
  - [ ] SubTask 5.4: Fix C decoder OOK decoding or generator timing
  - [ ] SubTask 5.5: Compile and verify with `python run_all_tests.py --decoder ook_c`

## Phase 3: Large-deviation decoders (delta-sigma_c, t55xx_c)

- [ ] Task 6: Fix delta-sigma_c.c — 192 matches, 190 deviations (sample range mismatch)
  - [ ] SubTask 6.1: Run `python show_diff.py delta-sigma_c` to see pattern of deviations
  - [ ] SubTask 6.2: Read delta-sigma_c.c and delta-sigma/pd.py to compare annotation range logic
  - [ ] SubTask 6.3: Fix C decoder annotation sample ranges to match Python
  - [ ] SubTask 6.4: Compile and verify with `python run_all_tests.py --decoder delta-sigma_c`

- [ ] Task 7: Fix t55xx_c.c — 76 matches, 180 deviations (extra annotation classes)
  - [ ] SubTask 7.1: Run `python show_diff.py t55xx_c` to see pattern of deviations
  - [ ] SubTask 7.2: Read t55xx_c.c and t55xx/pd.py to compare annotation class output
  - [ ] SubTask 7.3: Fix C decoder to only output annotation classes Python produces, or add missing classes to Python
  - [ ] SubTask 7.4: Compile and verify with `python run_all_tests.py --decoder t55xx_c`

## Phase 4: Final verification

- [ ] Task 8: Compile all fixes and run full test suite
  - [ ] SubTask 8.1: Run `ninja -C build` to compile all modified C decoders
  - [ ] SubTask 8.2: Run `python run_all_tests.py --decoder tdm_audio_c` and verify PASS
  - [ ] SubTask 8.3: Run `python run_all_tests.py --decoder dali_c` and verify PASS
  - [ ] SubTask 8.4: Run `python run_all_tests.py --decoder dmx512_c` and verify PASS
  - [ ] SubTask 8.5: Run `python run_all_tests.py --decoder ook_c` and verify PASS
  - [ ] SubTask 8.6: Run `python run_all_tests.py --decoder spdif_c` and verify PASS
  - [ ] SubTask 8.7: Run `python run_all_tests.py --decoder delta-sigma_c` and verify PASS
  - [ ] SubTask 8.8: Run `python run_all_tests.py --decoder t55xx_c` and verify PASS

# Task Dependencies
- Tasks 1-7 can be parallelized (independent decoders)
- Task 8 depends on Tasks 1-7
- Within each task, subtasks are sequential (diagnose → fix → verify)
