# 修复剩余 WARN 解码器生成器 Spec

## Why
15 个 C 解码器测试报 WARN（vacuous match），Python 和 C 解码器都输出 0 条 annotation。5 个已修复（AC97, BEAN, C2, DSI, GPIB），还剩 10 个需要修复。根本原因是生成的测试波形数据不合法，无法触发解码器状态机。

## What Changes
- 修复 `protocol_synthesizer.py` 中 10 个波形生成器的时序和通道映射
- 修复 `test_factory.py` 中部分解码器的生成器函数和通道数配置
- 不修改 C 解码器源代码
- 不修改 Python 解码器源代码

## 剩余 10 个解码器详细分析

### 1. LPCGenerator
- **问题**: `io_write()` 没有正确实现 LPC 协议状态机
- **C 解码器通道**: LFRAME(ch0), LCLK(ch1), LAD0-3(ch2-5)
- **Python 解码器逻辑**: 在 LCLK 上升沿采样，LFRAME# 低开始周期，START→CT/DR→ADDR→TAR(0xF)→SYNC→DATA→TAR2(0xF)
- **修复方案**: 重写 `io_write()` 方法，按正确顺序发送每个字段，每步在 LCLK 上升沿前设置 LAD 值

### 2. MCS48Generator
- **问题**: 通道映射错误，PSEN 时序不正确
- **C 解码器通道**: ALE(ch0), PSEN(ch1), D0-D7(ch2-9), A8-A11(ch10-13)
- **当前生成器通道**: ALE(ch0), RD(ch1), WR(ch2), PSEN(ch3), EA(ch4), D0-D7(ch5-12), A0(ch13)
- **Python 解码器逻辑**: 在 ALE 下降沿采样地址，在 PSEN 上升沿采样数据
- **修复方案**: 修正通道映射，ALE 高→低期间地址在 D0-D7 上，PSEN 低→高期间数据在 D0-D7 上

### 3. MIPIRFFEGenerator
- **问题**: 起始条件错误（I2C-like 而非 SSC）
- **C 解码器通道**: SCLK(ch0), SDATA(ch1)
- **Python 解码器逻辑**: SSC = SCLK 低电平期间 SDATA 上升沿，然后等待 SCLK 上升沿+SDATA 下降沿确认
- **修复方案**: 重写 `_start_condition()` 实现 SSC：SCLK 先拉低，然后 SDATA 上升沿，然后 SCLK 上升沿

### 4. MVBGenerator
- **问题**: CRC 计算可能不匹配解码器，需要验证
- **C 解码器通道**: MVB(ch0)，1 通道
- **Python 解码器逻辑**: Manchester II 编码，notch 长度检测，18 位前导码匹配，CRC-8 + parity
- **修复方案**: 验证 CRC 计算与 Python 解码器的 `encode_data()` 一致，确保 Manchester 编码正确

### 5. FSiGenerator
- **问题**: 当前生成器过于简单，没有实现 FSI 协议的 BREAK 检测和完整状态机
- **C 解码器通道**: DATA(ch0), CLOCK(ch1)
- **Python 解码器逻辑**: 数据电信号反转，需要 256 个连续 1（上升沿采样）触发 BREAK，然后 START→TX_SLAVE_ID→COMMAND→DIRECTION→ADDRESS→DATA_SIZE→TX_DATA→CRC→TAR
- **修复方案**: 完全重写，实现 BREAK（256 个时钟周期的 data=0 原始值=1），然后发送完整的 ABS_ADR 写命令

### 6. Z80Generator
- **问题**: 通道映射完全错误
- **C 解码器通道**: D0-D7(ch0-7), M1(ch8), RD(ch9), WR(ch10)
- **当前生成器通道**: MREQ(ch0), IORQ(ch1), RD(ch2), WR(ch3), M1(ch4), RFSH(ch5), A0(ch6), D0-D3(ch7-10)
- **Python 解码器逻辑**: 检测 MREQ=0 且 RD=0 时为 MEMRD 或 FETCH（M1=0），MREQ=0 且 WR=0 为 MEMWR
- **修复方案**: 修正通道映射为 D0-D7(ch0-7), M1(ch8), RD(ch9), WR(ch10)，正确实现 M1 周期

### 7. ST7735Generator（test_factory.py 中）
- **问题**: 使用 SPIGenerator 但 DC 信号处理不正确，且只发一条命令不会触发 description flush
- **C 解码器通道**: CS(ch0), CLK(ch1), MOSI(ch2), DC(ch3)
- **Python 解码器逻辑**: DC=0 为命令，DC=1 为数据，description 在下一个命令时才 flush
- **修复方案**: 不使用 SPIGenerator，手动生成 CS+CLK+MOSI+DC 信号，发送两条命令

### 8. ST7789Generator（test_factory.py 中）
- **问题**: 直接调用 _gen_st7735，但 ST7789 协议完全不同
- **C 解码器通道**: CSX(ch0), DCX(ch1), SDO(ch2), WRX(ch3)
- **Python 解码器逻辑**: CSX 下降沿开始，DCX 是时钟信号（上升沿采样 SDO），WRX 区分命令(0)/数据(1)
- **修复方案**: 完全重写 _gen_st7789，实现正确的 CSX+DCX+SDO+WRX 时序

### 9. TM1637Generator（test_factory.py 中，堆叠在 tmc_c 上）
- **问题**: 当前 _gen_tmc 使用 SPIGenerator，完全错误
- **TMC 解码器通道**: CLK(ch0), DIO(ch1), STB(ch2, optional)
- **TMC 解码器逻辑**: 2 线模式 START=DIO 下降沿（CLK 高时），8 位数据 LSB 先发（CLK 上升沿），ACK（CLK 下降沿），STOP=DIO 上升沿（CLK 高时）
- **修复方案**: 重写 _gen_tmc，实现 I2C-like 的 CLK/DIO 协议

### 10. IEEE488Generator
- **问题**: 当前只有 1 通道发送 DAV 脉冲，无法工作
- **C 解码器通道**: DIO1(ch0) 必选，DIO2-8+EOI+DAV+NRFD+NDAC+IFC+SRQ+ATN+REN+CLK(ch1-16) 可选
- **Python 解码器逻辑**: 1 通道时使用串行模式（IEC），需要 CLK+DATA+ATN；多通道时使用并行模式
- **修复方案**: 改为使用 3 通道（DATA+CLK+ATN）实现串行 IEC 模式，或使用 16 通道实现并行 GPIB 模式

## Impact
- 修改 `libsigrokdecode/tests/protocol_synthesizer.py`：修复 6 个生成器（LPC, MCS48, MIPI_RFFE, MVB, FSI, Z80）
- 修改 `libsigrokdecode/tests/test_factory.py`：修复 4 个生成器函数和通道配置（ST7735, ST7789, TMC/TM1637, IEEE488）
- 不修改 C/Python 解码器源代码
