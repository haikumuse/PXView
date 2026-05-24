# C解码器API缺口补全与子Spec范本一致性修正 Spec

## Why

37批Python→C解码器移植子Spec全面审查发现：C解码器框架存在多个API缺口（OUTPUT\_LOGIC、BITS时间戳等），且子Spec普遍存在参考范本缺失、协议格式错误、代码bug等问题。需在C实现开始前补全API并修正子Spec，否则约30+个解码器将被迫简化或无法正常工作。

## What Changes

* 新增 `SRD_OUTPUT_LOGIC` 输出类型及 `c_decoder_put_logic()` API

* 扩展 BITS 消息格式增加 bit 级时间戳

* 验证并确保 `c_decoder_register_output()` 在 `recv_proto()` 中可安全调用

* 修正37批子Spec中的协议格式错误、代码bug、参考范本缺失等问题

## Impact

* Affected code: `libsigrokdecode/c_decoder_api.c`, `libsigrokdecode/c_decoders/c_decoder_utils.h`

* Affected specs: 全部37批子Spec均需修正参考范本引用，其中7批有严重协议格式/代码错误

* Affected decoders: 约30+个上层解码器受BITS时间戳影响，2个解码器受OUTPUT\_LOGIC影响

***

## ADDED Requirements

### Requirement: SRD\_OUTPUT\_LOGIC 输出类型支持

C解码器框架 SHALL 提供 `SRD_OUTPUT_LOGIC` 输出类型，用于输出逻辑通道数据（如GPIO状态）。

#### Scenario: PCA9571 输出8个GPIO通道

* **WHEN** PCA9571 C解码器读取到GPIO端口数据

* **THEN** 通过 `c_decoder_put_logic(di, ss, es, out_logic, channel_mask, values)` 输出各通道逻辑值

#### 需新增的API

```c
// 注册逻辑输出通道
int c_decoder_register_output(di, SRD_OUTPUT_LOGIC, "pca9571");

// 输出逻辑值（channel_mask标识哪些通道有值，values为通道值数组）
int c_decoder_put_logic(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    int output_id, uint32_t channel_mask, const uint8_t *values, int num_channels);
```

#### 受影响解码器

* Batch-20: PCA9571 (8通道GPIO)

* Batch-22: TCA6408A (8通道GPIO)

***

### Requirement: BITS消息增加bit级时间戳

C解码器框架 SHALL 在 BITS 协议消息中包含每个bit的起始/结束采样号，使上层C解码器能够实现精确的位级标注。

#### Scenario: SPI上层解码器接收BITS数据

* **WHEN** SPI C解码器输出BITS消息

* **THEN** 上层C解码器通过 `recv_proto()` 接收到的BITS消息data格式包含每个bit的ss/es

#### 当前BITS消息格式（无时间戳）

```
data[0] = have_mosi
data[1..8] = mosi_bits (每字节1bit, 最多64bit)
data[9] = have_miso
data[10..17] = miso_bits (每字节1bit, 最多64bit)
```

#### 建议新BITS消息格式（含时间戳）

```
data[0] = have_mosi
data[1] = mosi_bit_count (uint8_t)
data[2..2+count*17-1] = 每bit: [value(1B)][ss(8B LE)][es(8B LE)]
data[2+count*17] = have_miso
data[2+count*17+1] = miso_bit_count (uint8_t)
data[2+count*17+2..] = 每bit: [value(1B)][ss(8B LE)][es(8B LE)]
```

#### 受影响解码器（7个批次，约20+个解码器）

* Batch-20: MLX90614, MPU6050, MXC6225XU, Nunchuk

* Batch-21: RTC8564, SSD1306, ST25DV, TCS3472x, TPM\_TIS\_I2C

* Batch-22: TMP102

* Batch-23: AD5626, AD79x0, ADF435x

* Batch-26: RFM12

* Batch-27: SDCARD\_SPI, SPIFLASH, SPI\_TPM

* Batch-32: JTAG\_AVR, JTAG\_EJTAG, JTAG\_STM32

***

### Requirement: c\_decoder\_register\_output() 在 recv\_proto() 中可安全调用

C解码器框架 SHALL 允许在 `recv_proto()` 回调中调用 `c_decoder_register_output()` 动态注册输出流。

#### Scenario: i2cdemux动态注册输出流

* **WHEN** i2cdemux\_c在 `recv_proto()` 中收到I2C地址数据

* **THEN** 可安全调用 `c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "i2c-0x50")` 注册新的输出流

#### 受影响解码器

* Batch-19: i2cdemux, i2cfilter

***

## MODIFIED Requirements

### Requirement: C解码器依赖规则

C解码器 SHALL 遵循以下依赖规则：

1. **C解码器只能依赖已有C实现的底层解码器**：C解码器的 `inputs` 列表中引用的解码器必须已有C实现（如 `i2c`、`spi`、`uart`、`can`、`jtag` 等），不得依赖仅有Python实现的解码器。

2. **底层仅有Python实现的解码器标记为"阻塞"**：若C解码器所需的底层解码器仅有Python实现，则该C解码器在子Spec中标记为"阻塞"（blocked），直至底层解码器完成C移植后方可解除。

#### 受影响解码器（标记为"阻塞"）

* Batch-34: avclan\_c (inputs=\['iebus'], iebus仅有Python实现)

* Batch-36: ook\_oregon, ook\_vis, ltar\_smartdevice, ir\_ltto\_decode, sony\_md\_decode (下层均为Python解码器)

* Batch-37: sipi, pjon, tpm\_fifo\_tis, tm1637, tm1638, ltar\_smartdevice\_decode (下层均为Python解码器)

***

### Requirement: 子Spec协议格式错误修正

以下子Spec存在严重的协议格式描述错误，将导致实现后解码器完全无法工作：

#### 1. Batch-26/27/28: SPI DATA包格式错误 \[严重]

**当前**: spec描述SPI DATA格式为18字节：`data[0..7]=MOSI(LE uint64), data[8..15]=MISO(LE uint64), data[16]=flags`

**实际spi\_c.c输出** (17字节)：

```c
data[0] = (have_mosi ? 1 : 0) | (have_miso ? 2 : 0);  // 合并标志
data[1..8] = mosi_val (LE uint64)
data[9..16] = miso_val (LE uint64)
```

**修正**: 所有SPI上层解码器的recv\_proto代码必须修正DATA解析逻辑：

* `have_mosi = data[0] & 1`（非 `data[0]` 或 `data[16] & 1`）

* `have_miso = (data[0] >> 1) & 1`（非 `data[9]` 或 `data[16] >> 1`）

* MOSI从 `data[1]` 开始（非 `data[0]`）

* MISO从 `data[9]` 开始（非 `data[8]`）

* 总长度17字节（非18字节）

**受影响**: Batch-26全部5个解码器、Batch-27全部5个解码器、Batch-28全部2个解码器

#### 2. Batch-34: 4b5b\_c输出格式完全错误 \[严重]

**当前**: spec声称4b5b\_c输出 `"START"`, `"TERMINATE"`, `"RESET"`, `"DATA"` 命令

**实际4b5b\_c.c输出**: `"J"`, `"K"`, `"T"`, `"R"`, `"Q"`, `"H"`, `"L"`, `"IDLE"`, `"SET"`, `"DATA"` — 使用控制符号短名称

**修正**: ethernet\_c的recv\_proto必须基于正确的4b5b\_c输出格式重新设计状态机：

* 帧开始检测: `"J" + "K"` 序列（JK=SOS）

* 帧结束检测: `"T"` 符号

* 空闲检测: `"IDLE"` 或 `"SET"`

#### 3. Batch-31: UART C解码器不输出"IDLE"和"BREAK" \[严重]

**当前**: spec声称UART C解码器输出"IDLE"和"BREAK"命令，sbus\_futaba\_c的recv\_proto处理了这些事件

**实际uart\_c.c**: 不输出"IDLE"和"BREAK"命令（Python版本输出，C版本未实现）

**修正**: 删除或标注"IDLE"/"BREAK"分支为"C版本暂不支持"，或修改uart\_c.c添加这些输出

#### 4. Batch-32: JTAG C解码器IR TDO输出描述错误 \[中等]

**当前**: spec声称"JTAG C解码器当前未发送IR TDO命令"

**实际jtag\_c.c**: 同时发送IR TDI和IR TDO（第274-275行）

**修正**: 修正描述，添加对"IR TDO"和"DR TDO"的处理逻辑

#### 5. Batch-13: arm\_etmv3 recv\_proto数据读取错误 \[严重]

**当前**: `uint8_t byte = data[0]` — 错误地将rxtx标志当作字节值

**修正**: `uint8_t rxtx = data[0]; uint8_t byte = data[1]` — data\[0]是rxtx标志，data\[1]是字节值

#### 6. Batch-37: pjon strcmp逻辑bug \[严重]

**当前**: `strcmp(cmd, "IDLE") || strcmp(cmd, "FRAME_DATA") == 0` — 第一个strcmp缺少 `== 0`

**修正**: `strcmp(cmd, "IDLE") == 0 || strcmp(cmd, "FRAME_DATA") == 0`

#### 7. Batch-20: mpu6050 annotation枚举命名错误 \[中等]

**当前**: 使用DS1307风格的命名（ANN\_REG\_SECONDS, ANN\_REG\_MINUTES等），与MPU6050无关

**修正**: 改为MPU6050相关命名（ANN\_REG\_SMPLRT\_DIV, ANN\_REG\_CONFIG等）

#### 8. Batch-25: max6954使用不支持的printf格式 \[轻微]

**当前**: `snprintf(buf, sizeof(buf), "0b%08b", val)` — C标准库不支持 `%b`

**修正**: 实现自定义二进制格式化辅助函数

#### 9. Batch-17: xy2-100 idn包含连字符 \[轻微]

**当前**: `dec_xy2-100_chan_clk` — 连字符不是合法C标识符

**修正**: 改为 `dec_xy2_100_chan_clk`

***

### Requirement: 子Spec参考范本一致性

37批子Spec SHALL 修正参考范本引用，确保所有解码器正确引用4个标准范本（spi\_c.c, can\_fd\_c.c, uart\_c.c, i2c\_c.c）和2个上层范本（lm75\_c.c, ds1307\_c.c）。

#### 参考范本引用规则

| 解码器类型                        | 应参考的范本                            | 说明                    |
| ---------------------------- | --------------------------------- | --------------------- |
| 底层解码器 (inputs=\['logic'])    | spi\_c.c, can\_fd\_c.c            | 条件构建器模式、状态机模式         |
| I2C上层解码器 (inputs=\['i2c'])   | lm75\_c.c, ds1307\_c.c, i2c\_c.c  | recv\_proto模式 + 数据源格式 |
| SPI上层解码器 (inputs=\['spi'])   | lm75\_c.c, ds1307\_c.c, spi\_c.c  | recv\_proto模式 + 数据源格式 |
| UART上层解码器 (inputs=\['uart']) | lm75\_c.c, ds1307\_c.c, uart\_c.c | recv\_proto模式 + 数据源格式 |
| 其他上层解码器                      | lm75\_c.c, ds1307\_c.c            | recv\_proto通用模式       |

#### 审核结果汇总

| 批次    | 引用标准范本                   | 引用上层范本                 | 严重问题                                               |
| ----- | ------------------------ | ---------------------- | -------------------------------------------------- |
| 01-05 | 全部缺失                     | 全部缺失                   | Batch-03 c\_cond\_or错误描述; Batch-04 modbus缺UART输出格式 |
| 06-10 | 全部缺失                     | 全部缺失                   | Batch-08仅参考ir\_nec\_c等非标准范本                        |
| 11-15 | 全部缺失                     | 全部缺失                   | Batch-13 arm\_etmv3 data索引错误                       |
| 16-17 | Batch-16隐式参考spi\_c       | 全部缺失                   | Batch-17 xy2-100 idn命名问题                           |
| 18-20 | 全部缺失                     | Batch-18/20仅参考lm75\_c  | Batch-20 mpu6050 annotation命名错误                    |
| 21-22 | 全部缺失                     | 仅参考lm75\_c             | I2C地址格式不一致                                         |
| 23-24 | 全部缺失                     | 全部缺失                   | 违反规则3                                              |
| 25    | spi\_c.c, i2c\_c.c       | lm75\_c.c, ds3231\_c.c | %b格式; license错误                                    |
| 26-28 | Batch-27/28参考lm75+spi\_c | Batch-27/28部分          | SPI DATA格式错误(17字节非18字节)                            |
| 29-30 | 全部缺失                     | 全部缺失                   | Batch-30 modbus/pn532缺Python输出                     |
| 31    | uart\_c.c                | lm75\_c.c              | UART不输出IDLE/BREAK                                  |
| 32    | 全部缺失                     | 全部缺失                   | IR TDO描述错误                                         |
| 33    | 无(合理)                    | lm75\_c.c, ds1307\_c.c | 质量最高，问题最少                                          |
| 34    | 全部缺失                     | 全部缺失                   | 4b5b\_c输出格式完全错误                                    |
| 35    | 全部缺失                     | 全部缺失                   | ps2\_c阻塞项                                          |
| 36    | 无(合理)                    | 仅lm75\_c(部分)           | 下层Python解码器输出格式未文档化                                |
| 37    | 全部缺失                     | 全部缺失                   | strcmp bug; 下层输出格式未文档化                             |

#### Batch-03: RVSWD条件等待描述错误

* **当前**: spec声称"c\_cond\_wait不支持OR条件列表"

* **修正**: `c_cond_or` API正是为此设计的，删除错误描述，使用标准 `c_cond_or` 模式

#### Batch-31: UART上层解码器参考不完整

* **当前**: 仅参考 `lm75_c.c` 的 recv\_proto 模式

* **修正**: 补充 `uart_c.c` 输出格式文档，明确UART通过 `c_decoder_put_python()` 输出的cmd类型（注意C版本不含"IDLE"/"BREAK"）

#### Batch-33: OneWire/Microwire上层解码器参考不完整

* **当前**: 仅参考 `lm75_c.c` 和 `ds1307_c.c`

* **修正**: 补充 `onewire_c.c` 和 `microwire_c.c` 输出格式文档

#### Batch-20/22: OUTPUT\_LOGIC参考缺失

* **当前**: 标注"C框架可能不支持，暂不实现"

* **修正**: 在OUTPUT\_LOGIC API实现后，更新spec添加 `c_decoder_put_logic()` 使用说明

***

## REMOVED Requirements

### Requirement: Python→C proto桥接

**Reason**: C解码器应自成体系，不依赖Python解码器。Python→C桥接增加了引擎复杂度且违反C解码器独立性原则。改为依赖规则：C解码器只能依赖已有C实现的底层解码器，底层仅有Python实现的解码器标记为"阻塞"。
**Migration**: 受影响的C解码器（avclan\_c、Batch-36/37解码器）标记为"阻塞"，直至其底层Python解码器完成C移植。

### Requirement: subprocess/objdump/ELF解析

**Reason**: C DLL中不应调用外部进程，这是C解码器的根本限制
**Migration**: ARM ETMv3和ARM ITM解码器的objdump/ELF功能永久省略，3个相关options保留但不生效

***

## API缺口优先级排序

| 优先级 | API缺口                                         | 影响解码器数 | 影响批次                       |
| --- | --------------------------------------------- | ------ | -------------------------- |
| P0  | SPI DATA包格式修正（17字节非18字节）                      | 12     | Batch-26/27/28             |
| P0  | 4b5b\_c输出格式修正                                 | 5      | Batch-34                   |
| P0  | BITS消息bit级时间戳                                 | \~20+  | Batch-20/21/22/23/26/27/32 |
| P1  | c\_decoder\_register\_output() recv\_proto安全性 | 2      | Batch-19                   |
| P1  | UART C解码器补充IDLE/BREAK输出                       | 5      | Batch-31                   |
| P2  | SRD\_OUTPUT\_LOGIC                            | 2      | Batch-20/22                |
| P3  | c\_decoder\_put\_meta\_int/double             | 2      | Batch-02/04                |
| P3  | c\_decoder\_get\_last\_samplenum              | 1+     | Batch-15                   |

注：P3项已在 `align-c-decoder-api-with-python` spec中定义，此处不再重复。

## 子Spec修正清单

| 批次       | 修正内容                                                           | 类型       | 严重程度 |
| -------- | -------------------------------------------------------------- | -------- | ---- |
| Batch-03 | 删除"c\_cond\_wait不支持OR条件"错误描述                                   | 错误修正     | 严重   |
| Batch-04 | 补充uart\_c.c输出格式文档                                              | 参考补充     | 高    |
| Batch-13 | 修正arm\_etmv3 recv\_proto data\[0]→data\[1]                     | 代码bug    | 严重   |
| Batch-19 | 补充c\_decoder\_register\_output()在recv\_proto中的安全性说明            | 补充       | 中    |
| Batch-20 | 修正mpu6050 annotation命名; 补充OUTPUT\_LOGIC说明                      | 错误修正+补充  | 中    |
| Batch-22 | 补充OUTPUT\_LOGIC API使用说明（待API实现后）                               | 补充       | 中    |
| Batch-25 | 修正%b格式; 修正license                                              | 代码bug    | 轻微   |
| Batch-26 | 修正SPI DATA格式为17字节; 修正CS-CHANGE处理                               | 格式错误     | 严重   |
| Batch-27 | 修正SPI DATA格式为17字节; 修正辅助函数                                      | 格式错误     | 严重   |
| Batch-28 | 修正SPI DATA格式为17字节                                              | 格式错误     | 严重   |
| Batch-31 | 标注UART不输出IDLE/BREAK; 补充uart\_c.c格式                             | 格式错误+补充  | 严重   |
| Batch-32 | 修正IR TDO描述; 添加lm75\_c/ds1307\_c参考                              | 错误修正+补充  | 高    |
| Batch-33 | 补充onewire\_c.c/microwire\_c.c输出格式文档                            | 参考补充     | 中    |
| Batch-34 | 修正4b5b\_c输出格式; 添加标准范本引用                                        | 格式错误     | 严重   |
| Batch-37 | 修正pjon strcmp bug; 文档化下层输出格式                                   | 代码bug+补充 | 严重   |
| 全部37批    | 添加标准范本引用（spi\_c/can\_fd\_c/uart\_c/i2c\_c + lm75\_c/ds1307\_c） | 参考补充     | 高    |

