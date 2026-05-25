# C解码器annotation数量修复 — 验证清单

## timing_c.c 验证
- [ ] format==0（full模式）时不再输出ANN_TERSE annotation
- [ ] format!=0时ANN_TERSE annotation输出不受影响
- [ ] ANN_TIME和ANN_AVG annotation输出不受影响

## morse_c.c 验证
- [ ] process_symbol()不再输出ANN_SYMBOL annotation
- [ ] flush_letter()中ANN_LETTER annotation输出不受影响
- [ ] flush_word()中ANN_WORD annotation输出不受影响
- [ ] morse_decode()中ANN_TIME和ANN_UNITS annotation输出不受影响

## sent_c.c 验证
- [x] 主循环每次迭代只等待1个下降沿（而非2个）
- [x] last_samplenum正确记录上一次下降沿位置
- [x] period计算使用正确的last_samplenum
- [x] 所有annotation输出（ANN_TICK、ANN_CAL、ANN_SC、ANN_DATA、ANN_CRC、ANN_PAUSE、ANN_WARNING）使用正确的last_samplenum

## opentherm_c.c 验证
- [x] handle_timing_error()设置last_frame_edge = c_samplenum
- [x] IDLE状态"silence too short"检查在timing error后能正确触发
- [x] 其他handle_timing_error调用点不受影响

## nrzi_c.c 验证
- [x] STATE_DECODE中每个bit annotation覆盖恰好1个symbol_len
- [x] bit annotation数量与Python版本一致
- [x] ANN_PREAMBLE annotation输出不受影响
- [x] 协议输出（out_python）不受影响

## 编译验证
- [x] 所有修改的C解码器编译通过
- [x] 无编译错误和警告
