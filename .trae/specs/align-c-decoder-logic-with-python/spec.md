# C 解码器逻辑对齐 Python 版本 Spec

## Why
C 解码器的解码逻辑与 Python 版本存在多处差异，包括错误的初始状态、反转的极性逻辑、缺失的选项处理、缺失的输出类型等。这些差异导致 C 解码器产生不正确或不完整的解码结果，需要以 Python 版本为准进行对齐。

## What Changes
- 修复 I2S C 解码器 WS 极性反转 bug
- 修复 JTAG C 解码器初始状态错误（TEST-LOGIC-RESET → RUN-TEST/IDLE）
- 修复 SPI C 解码器 format 选项被忽略的问题
- 为 UART C 解码器添加缺失的 parity 类型（zero/one/ignore）和 format 类型（oct/bin）
- 为所有有 decode 逻辑的 C 解码器添加 OUTPUT_PYTHON 输出（I2C, SPI, UART, JTAG, SWD, HDLC, I2S）
- 为 I2S C 解码器添加 bit_shift 选项的实际处理逻辑
- 为 JTAG C 解码器修复 SHIFT 状态首 bit 跳过逻辑和 bitstring 格式
- 为 SPI C 解码器添加 CS-CHANGE 通知

## Impact
- Affected code:
  - `libsigrokdecode/c_decoders/i2s_c.c` — WS 极性修复、bit_shift 处理
  - `libsigrokdecode/c_decoders/jtag_c.c` — 初始状态修复、SHIFT 首bit 跳过、bitstring 格式
  - `libsigrokdecode/c_decoders/spi_c.c` — format 选项生效、CS-CHANGE
  - `libsigrokdecode/c_decoders/uart_c.c` — parity 类型扩展、format 类型扩展
  - `libsigrokdecode/c_decoders/i2c_c.c` — OUTPUT_PYTHON 输出
  - `libsigrokdecode/c_decoders/swd_c.c` — OUTPUT_PYTHON 输出
  - `libsigrokdecode/c_decoders/hdlc_c.c` — OUTPUT_PYTHON 输出

## ADDED Requirements

### Requirement: I2S WS 极性正确映射
I2S C 解码器的 WS 极性映射应当与 Python 版本和 I2S 规范一致。

#### Scenario: ws_polarity=left-high 时 WS=1 表示左声道
- **WHEN** 用户选择 ws_polarity="left-high" 且 WS 信号为 1
- **THEN** 当前采样属于左声道

#### Scenario: ws_polarity=left-low 时 WS=0 表示左声道
- **WHEN** 用户选择 ws_polarity="left-low" 且 WS 信号为 0
- **THEN** 当前采样属于左声道

### Requirement: JTAG 初始状态为 RUN-TEST/IDLE
JTAG C 解码器的初始状态应当与 Python 版本一致，从 RUN-TEST/IDLE 开始。

#### Scenario: JTAG 解码器启动时状态
- **WHEN** JTAG C 解码器开始解码
- **THEN** 初始状态为 RUN-TEST/IDLE（而非 TEST-LOGIC-RESET）

### Requirement: JTAG SHIFT 状态跳过首 bit
JTAG C 解码器在进入 SHIFT-DR/SHIFT-IR 状态时，应当跳过第一个时钟周期的 bit（该 bit 与进入 SHIFT 状态的 bit 相同）。

#### Scenario: SHIFT-DR 首 bit 跳过
- **WHEN** JTAG 状态机从 CAPTURE-DR 进入 SHIFT-DR
- **THEN** 第一个 TCK 上升沿的 TDI/TDO 不计入 bitstring

### Requirement: SPI format 选项生效
SPI C 解码器应当正确读取和使用 format 选项来格式化数据输出。

#### Scenario: SPI format=dec 时输出十进制
- **WHEN** 用户选择 format="dec"
- **THEN** MISO/MOSI 数据以十进制格式显示

### Requirement: UART 支持完整 parity 类型
UART C 解码器应当支持与 Python 版本相同的 6 种 parity 类型。

#### Scenario: parity=zero 时校验位始终为 0
- **WHEN** 用户选择 parity="zero"
- **THEN** 发送校验位始终为 0，接收时期望校验位为 0

#### Scenario: parity=one 时校验位始终为 1
- **WHEN** 用户选择 parity="one"
- **THEN** 发送校验位始终为 1，接收时期望校验位为 1

#### Scenario: parity=ignore 时不检查校验位
- **WHEN** 用户选择 parity="ignore"
- **THEN** 校验位存在但不进行校验检查

### Requirement: C 解码器 OUTPUT_PYTHON 输出
有 decode 逻辑的 C 解码器应当注册并输出 SRD_OUTPUT_PYTHON，使上层解码器可以接收协议数据。

#### Scenario: I2C C 解码器输出协议数据
- **WHEN** I2C C 解码器检测到 START 条件
- **THEN** 通过 OUTPUT_PYTHON 输出 `['START', None]`，上层解码器可接收

#### Scenario: SPI C 解码器输出数据
- **WHEN** SPI C 解码器完成一个字传输
- **THEN** 通过 OUTPUT_PYTHON 输出 `['DATA', mosi_byte, miso_byte]`

### Requirement: I2S bit_shift 选项处理
I2S C 解码器应当正确处理 bit_shift 选项，支持 "right-shifted by one" 模式。

#### Scenario: bit_shift=right-shifted-by-one
- **WHEN** 用户选择 bit_shift="right-shifted by one"
- **THEN** 数据位相对于 WS 边沿右移一个 SCK 周期

### Requirement: SPI CS-CHANGE 通知
SPI C 解码器应当在 CS 信号变化时输出 CS-CHANGE 通知。

#### Scenario: CS 从高变低
- **WHEN** SPI CS 信号从非活跃变为活跃
- **THEN** 输出 CS-CHANGE annotation

## MODIFIED Requirements

### Requirement: I2S WS 极性映射
原逻辑：`ws_is_left = ws_polarity_left_high ? (ws == 0) : (ws == 1)`（反转的）
修改为：`ws_is_left = ws_polarity_left_high ? (ws == 1) : (ws == 0)`

### Requirement: JTAG 初始状态
原逻辑：`priv->state = TEST_LOGIC_RESET`
修改为：`priv->state = RUN_TEST_IDLE`

### Requirement: SPI format 处理
原逻辑：`spi_put_data()` 硬编码 `const char *fmt = "hex"`
修改为：从选项中读取 format 值并传递给 `spi_format_value()`

### Requirement: UART parity 类型
原逻辑：仅支持 PARITY_NONE, PARITY_ODD, PARITY_EVEN
修改为：新增 PARITY_ZERO, PARITY_ONE, PARITY_IGNORE

## REMOVED Requirements

无移除的需求。
