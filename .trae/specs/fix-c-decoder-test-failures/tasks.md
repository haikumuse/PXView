# Tasks

## 第一批：测试基础设施修复（解除阻塞）

- [ ] Task 1: 修复Python DLL加载环境问题
  - [ ] 1.1: 在run_all_tests.py中设置PATH环境变量，添加Python DLL目录（如D:\msys64\mingw64\bin或Python安装目录）
  - [ ] 1.2: 验证18个之前因binascii DLL加载失败的解码器能成功运行Python参考输出

- [ ] Task 2: 修复hdlc_c崩溃问题
  - [ ] 2.1: 修复hdlc_c.c中`c_decoder_put_python()`调用传入NULL数据指针的问题
  - [ ] 2.2: 验证hdlc_c不再崩溃

## 第二批：C解码器超时修复

- [ ] Task 3: 修复5个C解码器超时问题
  - [ ] 3.1: 修复dali_c.c — 添加基于数据总长度的退出条件
  - [ ] 3.2: 修复maple_bus_c.c — 添加基于数据总长度的退出条件
  - [ ] 3.3: 修复ook_c.c — 添加基于数据总长度的退出条件
  - [ ] 3.4: 修复usb_signalling_c.c — 添加基于数据总长度的退出条件
  - [ ] 3.5: 修复wiegand_c.c — 添加基于数据总长度的退出条件
  - [ ] 3.6: 验证5个解码器不再超时

## 第三批：C解码器输出0注解修复（严重Bug）

- [ ] Task 4: 修复ieee488_c — 添加仅DATA通道的回退解码路径
  - [ ] 4.1: 当既没有CLK也没有DAV通道时，添加仅基于DATA通道的简化解码路径
  - [ ] 4.2: 验证ieee488_c在仅DATA通道时输出位注解

- [ ] Task 5: 修复miller_c — 超时后继续解码
  - [ ] 5.1: 修改miller_decode()，超时后flush bitstring并继续寻找下一条消息起始边沿
  - [ ] 5.2: 验证miller_c不再输出0注解

- [ ] Task 6: 修复swi_c — 无效数据时输出错误注解
  - [ ] 6.1: 当检测到无效波特间隔或不完整字时，输出错误注解而非静默跳过
  - [ ] 6.2: 验证swi_c输出错误注解

## 第四批：约2倍注解计数差异修复

- [ ] Task 7: 修复nrzi_c — 位注解范围计算错误
  - [ ] 7.1: 修改STATE_DECODE分支，确保每个位注解覆盖1个symbol_len而非2个
  - [ ] 7.2: 验证nrzi_c注解数量与Python一致

- [ ] Task 8: 修复opentherm_c — 缺少sync error注解
  - [ ] 8.1: 在IDLE状态中添加"Sync error: silence too short"注解输出
  - [ ] 8.2: 验证opentherm_c注解数量与Python一致

- [ ] Task 9: 修复sent_c — 每次迭代消耗2个下降沿
  - [ ] 9.1: 重构主循环，每次迭代只等待1个下降沿
  - [ ] 9.2: 验证sent_c注解数量与Python一致

## 第五批：重大注解计数差异修复

- [ ] Task 10: 修复can_c — 缺少位注解
  - [ ] 10.1: 在位解码循环中为每个解码的位添加ANN_BIT注解输出
  - [ ] 10.2: 验证can_c注解数量与Python一致

- [ ] Task 11: 修复microwire_c — 边沿检测偏移
  - [ ] 11.1: 调整边沿检测时机，确保在CS上升沿后的第一个SK上升沿就输出状态注解
  - [ ] 11.2: 验证microwire_c注解数量与Python一致

- [ ] Task 12: 修复morse_c — 多余SYMBOL注解
  - [ ] 12.1: 移除process_symbol()中的SYMBOL注解输出，SYMBOL仅由更高层处理
  - [ ] 12.2: 验证morse_c注解数量与Python一致

- [ ] Task 13: 修复timing_c — format=full时多余TERSE注解
  - [ ] 13.1: 删除format==0时的TERSE注解输出代码块
  - [ ] 13.2: 验证timing_c注解数量与Python一致

- [ ] Task 14: 修复swim_c — 位检测逻辑差异
  - [ ] 14.1: 对齐位检测逻辑，确保只在有效帧结构中输出位注解
  - [ ] 14.2: 验证swim_c注解数量与Python一致

- [ ] Task 15: 修复ps2_c — 位检测起始位置偏移
  - [ ] 15.1: 对齐位检测起始位置与Python一致
  - [ ] 15.2: 验证ps2_c注解数量与Python一致

- [ ] Task 16: 修复z80_c — end_sample错误和缺少警告注解
  - [ ] 16.1: 修复end_sample计算（当前经常为0）
  - [ ] 16.2: 添加初始状态"Illegal transition"警告注解
  - [ ] 16.3: 验证z80_c注解数量与Python一致

## 第六批：文本格式差异修复

- [ ] Task 17: 修复caliper_c — 缺少小数点
  - [ ] 17.1: 格式化测量值时始终使用"%.1fmm"格式
  - [ ] 17.2: 验证caliper_c文本格式与Python一致

- [ ] Task 18: 修复dcc_c — 缺少时序类型后缀
  - [ ] 18.1: 在时序注解文本中添加类型后缀如"(sync)"
  - [ ] 18.2: 验证dcc_c文本格式与Python一致

- [ ] Task 19: 修复pwm_c — 数值和单位之间缺少空格
  - [ ] 19.1: 将"%.1fμs"改为"%.1f μs"
  - [ ] 19.2: 验证pwm_c文本格式与Python一致

- [ ] Task 20: 修复seven_segment_c — 缺少小数点标记
  - [ ] 20.1: 检查DP通道状态，激活时在数字字符后追加"."
  - [ ] 20.2: 验证seven_segment_c文本格式与Python一致

## 第七批：其他差异修复

- [ ] Task 21: 修复graycode_c和numbers_and_state_c — 起始采样位置偏移
  - [ ] 21.1: 调整graycode_c起始采样位置从1改为0
  - [ ] 21.2: 调整numbers_and_state_c起始采样位置从1改为0
  - [ ] 21.3: 验证两个解码器注解数量与Python一致

- [ ] Task 22: 修复i2c_c — 缺少颜色注解
  - [ ] 22.1: 添加ATK颜色注解输出（与Python一致的颜色值）
  - [ ] 22.2: 验证i2c_c注解数量与Python一致

- [ ] Task 23: 修复spi_c — 位采样起始位置差异
  - [ ] 23.1: 对齐CS边沿检测和首个时钟采样点
  - [ ] 23.2: 验证spi_c位采样起始位置与Python一致

- [ ] Task 24: 修复jitter_c和onewire_link_c — 缺少文本变体
  - [ ] 24.1: jitter_c输出多个文本变体（如["Missed clock", "MC"]）
  - [ ] 24.2: onewire_link_c输出多个文本变体（如["Time slot not long enough", "Slot too short", "SLOT < 60.0"]）
  - [ ] 24.3: 验证两个解码器文本变体与Python一致

- [ ] Task 25: 修复pjdl_c — 过多错误注解
  - [ ] 25.1: 不在每个无效边沿都输出错误注解，仅在特定协议违规时输出
  - [ ] 25.2: 验证pjdl_c注解数量合理

## 第八批：编译验证和回归测试

- [ ] Task 26: 编译验证所有修改的C解码器
  - [ ] 26.1: 增量构建所有修改的C解码器
  - [ ] 26.2: 确保无编译错误和警告

- [ ] Task 27: 重新运行全部测试验证修复效果
  - [ ] 27.1: 运行run_all_tests.py --all
  - [ ] 27.2: 统计PASS/FAIL/ERROR数量，确认改善
  - [ ] 27.3: 对仍有FAIL的解码器进行二次分析和修复

# Task Dependencies
- Task 1 无依赖（基础设施修复，解除18个解码器的阻塞）
- Task 2-3 无依赖（可并行）
- Task 4-6 依赖 Task 2（需要构建环境正常）
- Task 7-16 可并行（不同解码器互不依赖）
- Task 17-25 可并行（不同解码器互不依赖）
- Task 26 依赖 Task 4-25（所有修改完成后编译）
- Task 27 依赖 Task 26（编译通过后测试）
