# Tasks

- [ ] Task 1: Fix `c_cond_either_edge` compilation error
  - [ ] Replace `c_cond_either_edge(b, rxtx)` with `c_cond_edge(b, rxtx)` on line 1102 of uart_c.c
- [ ] Task 2: Fix FRAME protocol output field order
  - [ ] In `handle_frame()`, change `C_U8(s->datavalue[rxtx]), C_I32(rxtx)` to `C_I32(rxtx), C_U8(s->datavalue[rxtx])` to match Python `[ptype, rxtx, pdata]` convention
- [ ] Task 3: Fix IDLE protocol output missing data value
  - [ ] In `handle_idle()`, change `c_proto(di, ss, es, s->out_python, "IDLE", C_I32(rxtx), NULL)` to `c_proto(di, ss, es, s->out_python, "IDLE", C_I32(rxtx), C_U8(0), NULL)`
- [ ] Task 4: Fix BREAK protocol output missing data value
  - [ ] In `handle_break()`, change `c_proto(di, ss, es, s->out_python, "BREAK", C_I32(rxtx), NULL)` to `c_proto(di, ss, es, s->out_python, "BREAK", C_I32(rxtx), C_U8(0), NULL)`
- [ ] Task 5: Verify compilation
  - [ ] Run incremental build to confirm uart_c.c compiles without errors

# Task Dependencies

- Task 5 depends on Tasks 1-4 (all fixes must be applied before verification)
- Tasks 1-4 are independent and can be done in parallel
