# Tasks

- [ ] Task 1: 修复 LPCGenerator（protocol_synthesizer.py）
  - [ ] 重写 `io_write()` 方法实现完整 LPC I/O 写周期
  - [ ] 正确实现 LFRAME# 低→START(0x0)→CT/DR(0x2)→ADDR(4 nibbles)→TAR(0xF, 2 cycles)→SYNC(0x0)→DATA(2 nibbles LSN-first)→TAR2(0xF, 2 cycles)
  - [ ] 每步在 LCLK 上升沿前设置 LAD 值，LFRAME# 在 START 阶段后拉高

- [ ] Task 2: 修复 MCS48Generator（protocol_synthesizer.py）
  - [ ] 修正通道映射为 ALE(ch0), PSEN(ch1), D0-D7(ch2-9), A8-A11(ch10-13)
  - [ ] 修正 idle 状态：ALE=0, PSEN=1（active low）
  - [ ] 修正 opcode_fetch()：ALE 上升沿时地址在 D0-D7+A8-A11，ALE 下降沿锁存，PSEN 下降沿→上升沿时数据在 D0-D7

- [ ] Task 3: 修复 MIPIRFFEGenerator（protocol_synthesizer.py）
  - [ ] 重写 `_start_condition()` 实现 SSC：SCLK 拉低→SDATA 上升沿→SCLK 上升沿→SDATA 下降沿
  - [ ] 重写 `_write_bit()`：数据在 SCLK 下降沿设置，SCLK 上升沿采样
  - [ ] 实现 Register Write (R0W) 命令：SA(4bit) + cmd_bit_0=1 → P(1bit) → BP

- [ ] Task 4: 修复 MVBGenerator（protocol_synthesizer.py）
  - [ ] 验证 CRC 计算与 Python 解码器 `encode_data()` 一致
  - [ ] 确保 Manchester II 编码产生正确的 notch 长度（1 tick = sr/3e6 samples）
  - [ ] 确保前导码后数据正确编码

- [ ] Task 5: 修复 FSiGenerator（protocol_synthesizer.py）
  - [ ] 完全重写，实现 BREAK 检测（256 个时钟周期 data=0 即 raw=1）
  - [ ] 实现 ABS_ADR 写命令：START→TX_SLAVE_ID(2bit)→COMMAND(3bit=100)→DIRECTION(1bit=0=Write)→ADDRESS(21bit)→DATA_SIZE(1bit=0=BYTE)→TX_DATA(8bit)→CRC(4bit)→TAR
  - [ ] 数据电信号反转：逻辑1→raw=0，逻辑0→raw=1

- [ ] Task 6: 修复 Z80Generator（protocol_synthesizer.py）
  - [ ] 修正通道映射为 D0-D7(ch0-7), M1(ch8), RD(ch9), WR(ch10)
  - [ ] 添加 MREQ(ch11) 和 IORQ(ch12) 作为可选通道
  - [ ] 重写 m1_cycle()：M1=0+MREQ=0+RD=0 为 FETCH 周期，正确设置 8 位数据线

- [ ] Task 7: 修复 ST7735 生成器（test_factory.py）
  - [ ] 重写 _gen_st7735 不使用 SPIGenerator
  - [ ] 手动生成 CS=0 + CLK+MOSI+DC 信号
  - [ ] 发送两条命令（如 SWRESET + SLPOUT）确保 description flush

- [ ] Task 8: 修复 ST7789 生成器（test_factory.py）
  - [ ] 重写 _gen_st7789，实现 CSX+DCX+SDO+WRX 时序
  - [ ] CSX 下降沿开始，DCX 上升沿采样 SDO，WRX=0 为命令，WRX=1 为数据
  - [ ] 发送两条命令确保 cmd_data flush

- [ ] Task 9: 修复 TMC/TM1637 生成器（test_factory.py）
  - [ ] 重写 _gen_tmc，实现 I2C-like 的 CLK+DIO 协议
  - [ ] START: DIO 下降沿（CLK 高时）→ 8 位数据 LSB 先发（CLK 上升沿）→ ACK（CLK 下降沿）→ STOP: DIO 上升沿（CLK 高时）
  - [ ] 发送 TM1637 命令序列：DATA_CMD(0x40) + ADDR_CMD(0xC0) + DATA(0x06) + STOP

- [ ] Task 10: 修复 IEEE488 生成器（test_factory.py + protocol_synthesizer.py）
  - [ ] 改为使用 3 通道（DATA=ch0, CLK=ch16, ATN=ch14）实现串行 IEC 模式
  - [ ] 或改为使用 16 通道实现并行 GPIB 模式（复用 GPIBGenerator）
  - [ ] 更新 test_factory.py 中的 num_channels 配置

- [ ] Task 11: 运行所有修复后的测试验证
  - [ ] 对每个解码器运行 `python run_all_tests.py --decoder <decoder_id>`
  - [ ] 确认所有 10 个解码器从 WARN 变为 PASS
  - [ ] 确认之前已修复的 5 个解码器无回归

# Task Dependencies
- Task 1-6 可并行执行（修改 protocol_synthesizer.py 中不同的生成器）
- Task 7-10 可并行执行（修改 test_factory.py 中不同的生成器函数）
- Task 11 依赖所有前置任务
