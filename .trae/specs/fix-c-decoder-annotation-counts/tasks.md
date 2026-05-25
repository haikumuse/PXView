# Tasks

- [ ] Task 1: 修复 timing_c.c — 移除full模式下的TERSE annotation输出
  - [ ] SubTask 1.1: 删除timing_c.c中第231-236行的TERSE annotation输出块（`if (s->format == 0) { ... C_ANN_PUT(... ANN_TERSE ...) }`）

- [ ] Task 2: 修复 morse_c.c — 移除process_symbol()中的SYMBOL annotation输出
  - [ ] SubTask 2.1: 删除process_symbol()中所有C_ANN_PUT(... ANN_SYMBOL ...)调用（共4处：dit/dah、word gap、letter gap、intra-char gap）

- [ ] Task 3: 修复 sent_c.c — 主循环改为每次迭代只消耗1个下降沿
  - [ ] SubTask 3.1: 将主循环从"每次迭代等待2个下降沿"改为"每次迭代等待1个下降沿，用上次迭代结束位置作为last_samplenum"
  - [ ] SubTask 3.2: 将初始等待下降沿后的`last_samplenum = samplenum`移到循环内`last_samplenum = samplenum`的位置调整

- [ ] Task 4: 修复 opentherm_c.c — handle_timing_error设置last_frame_edge
  - [ ] SubTask 4.1: 在handle_timing_error()函数中添加`s->last_frame_edge = s->c_samplenum;`，与Python版本一致

- [ ] Task 5: 修复 nrzi_c.c — STATE_DECODE中bit annotation覆盖1个symbol_len
  - [ ] SubTask 5.1: 修改STATE_DECODE中的c_cond_skip参数或annotation范围计算，确保每个bit annotation覆盖恰好1个symbol_len

- [ ] Task 6: 编译验证所有修改
  - [ ] SubTask 6.1: 增量构建所有修改的C解码器
  - [ ] SubTask 6.2: 确保无编译错误和警告

# Task Dependencies
- [Task 1-5] 可并行
- [Task 6] 依赖 [Task 1-5]
