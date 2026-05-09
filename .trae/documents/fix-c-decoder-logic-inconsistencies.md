# 修复 C 解码器与 Python 解码器逻辑不一致的问题

## 摘要

对比所有 C 解码器与对应 Python 解码器的 `decode()` 函数逻辑，发现以下 C 解码器存在与 Python 版本不一致的问题，需要修复。

## 当前状态分析

### 已修复的解码器
- **I2C** (`i2c_c.c`)：已在之前的会话中修复了 `STATE_FIND_ADDRESS` 和 `STATE_FIND_ACK` 缺少 OR 条件的问题

### 需要修复的解码器

#### 1. SPI (`spi_c.c`) — 严重问题

**文件路径**：
- C: `libsigrokdecode/c_decoders/spi_c.c`
- Python: `libsigrokdecode/decoders/0-spi/pd.py`

**问题 1：缺少 CS 边沿检测作为 wait 条件**

Python 版本 (pd.py:262-268)：
```python
wait_cond = [{0: 'r'}]  # CLK 上升沿
if self.have_cs:
    self.have_cs = len(wait_cond)
    wait_cond.append({3: 'e'})  # CS 边沿变化
```

C 版本 (spi_c.c:71-74)：
```c
c_cond_rise(cb, CLK);
c_cond_or(cb);
c_cond_fall(cb, CLK);
// 缺少 CS 边沿检测！
```

**影响**：当 CS 信号变化时，C 解码器不会立即响应，可能导致：
- CS 变低后第一个 bit 被丢失
- CS 变高后仍然继续采样数据
- 字节边界错误

**问题 2：缺少 CS 变化时重置解码器状态**

Python 版本 (pd.py:226-231)：
```python
if self.have_cs and (first or (self.matched & (0b1 << self.have_cs))):
    self.reset_decoder_state()
```

C 版本没有在 CS 变化时重置 `bit_count`、`mosi_byte`、`miso_byte`。

**影响**：如果 CS 在一个字节传输中间变化，C 解码器会继续从上次的 bit_count 开始，导致数据错位。

**问题 3：同时等待上升和下降沿，而非只等待采样边沿**

Python 版本根据 CPOL/CPHA 只等待一个方向的边沿（上升或下降），C 版本同时等待两个方向的边沿然后根据 CPHA 过滤。虽然逻辑上等价，但效率较低，且与 Python 行为不一致。

**问题 4：`miso_str` 格式化 bug**

C 版本 (spi_c.c:123)：
```c
snprintf(mosi_str, sizeof(mosi_str), "0x%02X", s->mosi_byte);
snprintf(mosi_str, sizeof(mosi_str), "0x%02X", s->miso_byte);  // 应该是 miso_str！
```

第二行写入了 `mosi_str` 而不是 `miso_str`，导致 MISO 数据覆盖了 MOSI 数据。

**修复方案**：
1. 添加 CS 边沿检测作为 OR 条件（如果 CS 通道存在）
2. CS 变化时重置解码器状态
3. 根据 CPOL/CPHA 只等待采样边沿
4. 修复 `miso_str` 变量名 bug

---

#### 2. I2C (`i2c_c.c`) — 已修复，但需验证

**文件路径**：
- C: `libsigrokdecode/c_decoders/i2c_c.c`
- Python: `libsigrokdecode/decoders/0-i2c/pd.py`

已在之前的会话中修复了 `STATE_FIND_ADDRESS` 和 `STATE_FIND_ACK` 的 OR 条件。需要确认当前文件状态与修复一致。

---

### 无需修复的解码器（逻辑与 Python 一致）

以下解码器经过对比，C 版本的 wait 条件和状态机逻辑与 Python 版本基本一致：

- **UART** (`uart_c.c`)：状态机和 wait 条件逻辑一致
- **CAN** (`can_c.c`)：边沿检测和位采样逻辑一致
- **SWD** (`swd_c.c`)：wait 条件和状态机逻辑一致
- **1-Wire** (`onewire_c.c`)：脉冲测量和状态机逻辑一致
- **PS/2** (`ps2_c.c`)：wait 条件和状态机逻辑一致
- **MDIO** (`mdio_c.c`)：MDC 边沿检测和状态机逻辑一致
- **CEC** (`cec_c.c`)：wait 条件和 ACK 超时逻辑一致
- **USB Signalling** (`usb_signalling_c.c`)：wait 条件和状态机逻辑一致
- **LPC** (`lpc_c.c`)：LCLK 上升沿等待和状态机逻辑一致
- **Microwire** (`microwire_c.c`)：CS/SK/SI/SO 边沿检测逻辑一致
- **HDLC** (`hdlc_c.c`)：CLK 边沿检测和 ENABLE 处理逻辑一致
- **I2S** (`i2s_c.c`)：SCK/WS/SD 处理逻辑一致
- **JTAG** (`jtag_c.c`)：TCK/TMS/TDI 边沿处理逻辑一致
- **ISO7816** (`iso7816_c.c`)：CLK/DATA 处理逻辑一致
- 以及其他简单解码器（PWM、NRZI、Graycode、DMX512、DCF77、SPDIF、LIN、4b5b、Wiegand、IR NEC/RC5/SIRC、C2、DALI、CAN-FD）

## 修改计划

### 修改 1：修复 SPI C 解码器 (`libsigrokdecode/c_decoders/spi_c.c`)

**当前代码** (spi_c.c:59-130)：
```c
static void spi_decode(struct srd_decoder_inst *di)
{
    // ...
    while (1) {
        srd_cond_builder *cb = c_cond_new();
        c_cond_rise(cb, CLK);
        c_cond_or(cb);
        c_cond_fall(cb, CLK);
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        // ... 没有 CS 边沿检测，没有 CS 变化重置
    }
}
```

**修改后**：
1. 添加 `have_cs` 标志和 CS 通道检测
2. 根据 CPOL/CPHA 只等待采样边沿
3. 添加 CS 边沿检测作为 OR 条件
4. CS 变化时重置解码器状态
5. 修复 `miso_str` 变量名 bug

具体修改：
- 在 `spi_decode` 开头检测 CS 通道是否存在
- 修改 wait 条件：只等待采样边沿 + CS 边沿（如果存在）
- 在处理逻辑中：CS 边沿匹配时重置状态，非采样边沿时跳过
- 修复第 123 行 `snprintf(mosi_str, ...)` → `snprintf(miso_str, ...)`

### 修改 2：验证 I2C C 解码器修复

确认 `i2c_c.c` 中 `STATE_FIND_ADDRESS` 和 `STATE_FIND_ACK` 的 OR 条件修复已正确应用。

## 假设与决策

1. **只修复 wait 条件和状态机逻辑差异**，不重构代码结构
2. **C 解码器的功能应与 Python 版本等价**，但保持 C 语言的编码风格
3. **SPI 的 CPOL/CPHA 处理**：保持与 Python 版本一致，只等待采样边沿
4. **CS 通道可选**：与 Python 版本一致，CS 是可选通道

## 验证步骤

1. 编译修改后的 C 解码器 DLL
2. 使用 SPI 协议数据测试：验证 CS 变化时解码器正确重置
3. 使用 I2C 协议数据测试：验证重复 START 和 STOP 条件被正确检测
4. 对比 C 和 Python 解码器的输出注解，确认结果一致
