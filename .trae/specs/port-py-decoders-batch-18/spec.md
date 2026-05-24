# Python → C 解码器移植规格书 — Batch 18

## 概述

本批次移植 5 个 I2C 上层协议解码器：**ad5593r**, **adxl345**, **atsha204a**, **bh1750**, **eeprom24xx**。

所有解码器均为 I2C 上层解码器（`inputs=['i2c']`），通过 `recv_proto()` 回调接收 I2C 下层协议数据，而非直接实现 `decode()` 回调。

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |
| i2c_c.c | 底层协议输出范本 | START/STOP条件检测、c_decoder_put_python()输出I2C协议数据 |
- `recv_proto()` 回调签名: `void (*recv_proto)(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- `decode()` 回调留空: `static void xxx_decode(struct srd_decoder_inst *di) { (void)di; }`
- `reset()` 中首次分配私有数据: `if (!c_decoder_get_private(di)) { c_decoder_set_private(di, g_malloc0(sizeof(xxx_state))); }`
- `destroy()` 中释放: `g_free(priv); c_decoder_set_private(di, NULL);`
- `srd_c_decoder_entry()` 中初始化选项默认值（GVariant）


## 1. ad5593r — Analog Devices AD5593R 12-bit ADC/DAC

### Python 元数据

| 属性 | 值 |
|------|-----|
| id | `"ad5593r"` |
| name | `"AD5593R"` |
| longname | `"Analog Devices AAD5593"` |
| desc | `"Analog Devices AD5593R 12-bit configurable ADC/DAC."` |
| license | `"gplv3+"` |
| inputs | `["i2c"]` |
| outputs | `[]` |
| tags | `["IC", "Analog/digital"]` |
| options | `{"id": "Vref", "desc": "Reference voltage (V)", "default": 2.5}` |

### annotations (6 个)

| Index | id | label |
|-------|----|-------|
| 0 | register | Register |
| 1 | field | Field |
| 2 | ptr_byte | Pointer Byte |
| 3 | slave_addr | Slave Address |
| 4 | data_byte | Data Byte |
| 5 | warning | Warning |

### annotation_rows (3 行)

| Row id | label | classes |
|--------|-------|---------|
| packet | Packets | (2, 3, 5, 4) |
| registers | Registers | (0,) |
| fields | Fields | (1,) |

### 解码逻辑分析

**状态机**: IDLE → GET SLAVE ADDR → GET POINTER BYTE → GET DATA HIGH → GET DATA LOW → (循环 DATA HIGH/LOW)

**关键点**:
1. AD5593R 使用 Pointer Byte 确定操作类型（opcode 在 bits[4:7]）
2. 数据为 16-bit（2 字节），先高字节后低字节
3. 使用 BITS 包进行 bit-level 注释（但 C 解码器不需要BITS — C上层解码器通过recv_proto接收BITS包但通常忽略） <!-- Updated: 明确C上层解码器对BITS包的处理方式 -->
4. 从 I2C 地址包中提取 R/W 位（LSB）判断操作类型
5. Slave 地址验证: `0b0010000` (0x10) 或 `0b0010001` (0x11)
6. Pointer Byte opcode 映射到 CONFIG_MODE / DAC_WR / ADC_RD / DAC_RD / GPIO_RD / REG_RD
7. 后续 Data Bytes 根据 Pointer Byte 的类型解析为不同的寄存器

**C 实现要点**:
- 不处理 BITS 包（C 上层解码器通过recv_proto接收BITS包但通常忽略，如需bit-level注释可解析BITS v2格式） <!-- Updated: BITS v2格式已在spi_c.c和i2c_c.c中实现，C上层解码器可选择解析 -->
- Pointer Byte 从 `DATA WRITE` 的 data 字节提取
- `data[0]` 的 bits[4:7] 为 opcode
- 16-bit 数据: 第一个 `DATA WRITE/READ` 为高字节，第二个为低字节
- 需要维护 `databyte_register` 字符串决定后续数据如何解析
- 需要实现所有寄存器字段的解析和注释

### recv_proto 实现伪代码

```c
static void ad5593r_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ad5593r_state *s = (ad5593r_state *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    if (strcmp(cmd, "STOP") == 0) {
        s->state = AD5593R_IDLE;
        return;
    }

    switch (s->state) {
    case AD5593R_IDLE:
        if (strcmp(cmd, "START") == 0)
            s->state = AD5593R_GET_SLAVE_ADDR;
        break;
    case AD5593R_GET_SLAVE_ADDR:
        if (strcmp(cmd, "ADDRESS WRITE") == 0) {
            uint8_t addr = (data_len > 0) ? data[0] : 0;
            // 验证 slave 地址 0x10 或 0x11
            if (addr != 0x10 && addr != 0x11) {
                C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_WARNING,
                    "I2C slave is not compatible.");
            } else {
                C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_SLAVE_ADDR,
                    "I2C Slave address");
            }
            s->state = AD5593R_GET_POINTER_BYTE;
        } else if (strcmp(cmd, "ADDRESS READ") == 0) {
            uint8_t addr = (data_len > 0) ? data[0] : 0;
            if (addr != 0x10 && addr != 0x11) {
                C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_WARNING,
                    "I2C slave is not compatible.");
            }
            s->state = AD5593R_GET_DATA_HIGH;
        }
        break;
    case AD5593R_GET_POINTER_BYTE:
        if (strcmp(cmd, "DATA WRITE") == 0 || strcmp(cmd, "DATA READ") == 0) {
            uint8_t ptr_byte = (data_len > 0) ? data[0] : 0;
            C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_PTR_BYTE, "Pointer Byte");
            // 解析 opcode (bits[4:7])
            uint8_t opcode = (ptr_byte >> 4) & 0x0F;
            // 根据 opcode 设置 databyte_register 并输出字段注释
            handle_pointer_byte(di, s, ptr_byte, opcode);
            s->state = AD5593R_GET_DATA_HIGH;
        }
        break;
    case AD5593R_GET_DATA_HIGH:
        if (strcmp(cmd, "DATA WRITE") == 0 || strcmp(cmd, "DATA READ") == 0) {
            s->data_high = (data_len > 0) ? data[0] : 0;
            s->ss_block = s->ss;
            s->state = AD5593R_GET_DATA_LOW;
        }
        break;
    case AD5593R_GET_DATA_LOW:
        if (strcmp(cmd, "DATA WRITE") == 0 || strcmp(cmd, "DATA READ") == 0) {
            s->data_low = (data_len > 0) ? data[0] : 0;
            s->es_block = s->es;
            C_ANN_PUT(di, s->ss_block, s->es_block, s->out_ann, ANN_DATA_BYTE, "Data Bytes");
            uint16_t data16 = (s->data_high << 8) | s->data_low;
            handle_data_bytes(di, s, data16);
            s->state = AD5593R_GET_DATA_HIGH; // 可能有连续数据
        }
        break;
    }
}
```

### 寄存器映射表（C 常量数组）

需要在 C 中实现以下映射:
- `CONFIG_MODE_BITS_MAP`: opcode → 寄存器名称字符串
- `REG_SEL_RD_MAP`: 读回寄存器选择
- Pointer Byte 寄存器字段定义
- Data Byte 寄存器字段定义（ADC_RESULT, DAC_WR, DAC_DATA_RD, GPIO_INPUT/OUTPUT 等）

---

## 2. adxl345 — Analog Devices ADXL345 3-axis Accelerometer

### ⚠️ 重要发现：此解码器输入为 SPI，不是 I2C！

Python 源码中 `inputs = ['spi']`，解码逻辑基于 SPI 协议（CS-CHANGE, MOSI/MISO 双通道）。

**由于 spi_c.c 已实现为 C 解码器（输出 BITS/DATA/CS-CHANGE 协议数据），adxl345_c 应作为 SPI 上层解码器实现（inputs=['spi']），通过 recv_proto() 接收 spi_c 的协议数据，而非适配为 I2C。** 这符合"C解码器只能依赖已有C实现的底层解码器"的规则。

<!-- Updated: 原spec计划将adxl345适配为I2C解码器，但spi_c.c已存在，应改为SPI上层解码器 -->

### Python 元数据 (SPI 版本)

| 属性 | 值 |
|------|-----|
| id | `"adxl345"` |
| name | `"ADXL345"` |
| longname | `"Analog Devices ADXL345"` |
| desc | `"Analog Devices ADXL345 3-axis accelerometer."` |
| license | `"gplv2+"` |
| inputs | `["spi"]` |
| outputs | `[]` |
| tags | `["IC", "Sensor"]` |

### C 版本元数据（SPI 上层解码器 — 修正方案）

<!-- Updated: 改为SPI上层解码器，与Python原版一致，依赖已实现的spi_c.c -->

| 属性 | 值 |
|------|-----|
| id | `"adxl345_c"` |
| name | `"ADXL345(C)"` |
| longname | `"Analog Devices ADXL345 (C)"` |
| desc | `"Analog Devices ADXL345 3-axis accelerometer. (C implementation)"` |
| inputs | `{"spi", NULL}` | <!-- Updated: 使用spi而非i2c -->
| outputs | `NULL` |
| tags | `{"IC", "Sensor", NULL}` |

### annotations (6 个)

| Index | id | label |
|-------|----|-------|
| 0 | read | Read |
| 1 | write | Write |
| 2 | mb | Multiple bytes |
| 3 | reg-address | Register address |
| 4 | reg-data | Register data |
| 5 | warning | Warning |

### annotation_rows (2 行)

| Row id | label | classes |
|--------|-------|---------|
| reg | Registers | (0, 1, 2, 3) |
| data | Data | (4, 5) |

### SPI 协议适配（修正方案）

<!-- Updated: 改为SPI上层解码器方案，通过recv_proto()接收spi_c输出的协议数据 -->

ADXL345 SPI 通信格式（与Python原版一致）:
- spi_c 输出的协议命令: "DATA"（17字节格式）、"BITS"（v2格式）、"CS-CHANGE"
- ADXL345 使用 SPI Mode 3（CPOL=1, CPHA=1）
- 写寄存器: CS-CHANGE(asserted) → DATA(MOSI: 写地址+数据) → CS-CHANGE(deasserted)
- 读寄存器: CS-CHANGE(asserted) → DATA(MOSI: 读地址) → DATA(MISO: 数据) → CS-CHANGE(deasserted)

Register Address Byte 格式 (8-bit):
- Bit 7: R/W (0=write, 1=read)
- Bit 6: MB (0=single byte, 1=multi-byte)
- Bit 5-0: Address[5:0]

### recv_proto 实现伪代码（SPI 上层解码器方案）

<!-- Updated: 完全重写recv_proto，基于spi_c输出的DATA/CS-CHANGE命令 -->

```c
static void adxl345_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    adxl345_state *s = (adxl345_state *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    if (strcmp(cmd, "CS-CHANGE") == 0) {
        // data[0]=old_cs, data[1]=new_cs (spi_c CS-CHANGE格式)
        uint8_t new_cs = (data_len > 1) ? data[1] : 0;
        if (new_cs == 0) {
            // CS asserted (active low) - 开始新传输
            s->state = ADXL345_GET_FIRST_BYTE;
            s->byte_count = 0;
        } else {
            // CS deasserted - 结束传输
            s->state = ADXL345_IDLE;
        }
        return;
    }

    if (strcmp(cmd, "DATA") == 0 && data_len >= 17) {
        // SPI DATA 格式: data[0]=flags, data[1..8]=mosi_val(LE), data[9..16]=miso_val(LE)
        uint8_t flags = data[0];
        int have_mosi = flags & 1;
        int have_miso = flags & 2;
        uint64_t mosi_val = 0, miso_val = 0;
        for (int i = 0; i < 8; i++) {
            mosi_val |= (uint64_t)data[1 + i] << (8 * i);
            miso_val |= (uint64_t)data[9 + i] << (8 * i);
        }

        switch (s->state) {
        case ADXL345_GET_FIRST_BYTE:
            // 第一个字节是地址字节（MOSI方向）
            if (have_mosi) {
                uint8_t reg_byte = (uint8_t)mosi_val;
                s->is_read_op = (reg_byte >> 7) & 1;
                s->is_multi = (reg_byte >> 6) & 1;
                s->address = reg_byte & 0x3F;
                // 输出 R/W 和 MB 注释
                C_ANN_PUT(di, s->ss, s->es, s->out_ann,
                    s->is_read_op ? ANN_READ : ANN_WRITE,
                    s->is_read_op ? "READ REG" : "WRITE REG");
                C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_MB,
                    s->is_multi ? "MULTIPLE BYTES" : "SINGLE BYTE");
                s->state = ADXL345_GET_DATA;
            }
            break;
        case ADXL345_GET_DATA:
            if (s->is_read_op && have_miso) {
                handle_register_data(di, s, (uint8_t)miso_val);
                if (s->is_multi) s->address++;
            } else if (!s->is_read_op && have_mosi) {
                handle_register_data(di, s, (uint8_t)mosi_val);
                if (s->is_multi) s->address++;
            }
            break;
        default:
            break;
        }
    }

    // BITS 包可选处理（用于bit-level注释）
    if (strcmp(cmd, "BITS") == 0) {
        // C 上层解码器可选择忽略BITS或用于bit-level注释
        return;
    }
}
```

### 寄存器处理函数映射

需要实现 `handle_reg_0x1d` 到 `handle_reg_0x39` 的所有寄存器处理函数。关键寄存器:
- 0x1D-0x29: 阈值/时间配置（带缩放因子）
- 0x27, 0x2A-0x2B: 位域解码
- 0x2C: BW_RATE（速率码映射）
- 0x2D: POWER_CTL
- 0x2E-0x30: 中断配置
- 0x31: DATA_FORMAT
- 0x32-0x37: XYZ 数据（16-bit, 两字节组合）
- 0x38-0x39: FIFO 控制/状态

---

## 3. atsha204a — Microchip ATSHA204A Crypto Authentication

### Python 元数据

| 属性 | 值 |
|------|-----|
| id | `"atsha204a"` |
| name | `"ATSHA204A"` |
| longname | `"Microchip ATSHA204A"` |
| desc | `"Microchip ATSHA204A family crypto authentication protocol."` |
| license | `"gplv2+"` |
| inputs | `["i2c"]` |
| outputs | `[]` |
| tags | `["Security/crypto", "IC", "Memory"]` |

### annotations (9 个)

| Index | id | label |
|-------|----|-------|
| 0 | waddr | Word address |
| 1 | count | Count |
| 2 | opcode | Opcode |
| 3 | param1 | Param1 |
| 4 | param2 | Param2 |
| 5 | data | Data |
| 6 | crc | CRC |
| 7 | status | Status |
| 8 | warning | Warning |

### annotation_rows (3 行)

| Row id | label | classes |
|--------|-------|---------|
| frame | Frame | (0, 1, 2, 3, 4, 5, 6) |
| status | Status | (7,) |
| warnings | Warnings | (8,) |

### 解码逻辑分析

**状态机**: IDLE → GET SLAVE ADDR → READ REGS / WRITE REGS → STOP

**关键点**:
1. 写操作(TX): 收集所有 DATA WRITE 字节到 `bytes` 缓冲区，STOP 时调用 `output_tx_bytes()`
2. 读操作(RX): 收集所有 DATA READ 字节到 `bytes` 缓冲区，STOP 时调用 `output_rx_bytes()`
3. TX 帧格式: Word Address + Count + Opcode + Param1 + Param2[2] + Data[N] + CRC[2]
4. RX 帧格式: Count + Data[N] + CRC[2]
5. Word Address 值: 0x00=RESET, 0x01=SLEEP, 0x02=IDLE, 0x03=COMMAND
6. Opcode 映射: 0x01=Pause, 0x02=Read, 0x08=MAC, 0x11=HMAC, 0x12=Write, 等
7. Param1 根据 Opcode 有不同含义（Mode/Zone/Random/Selector）
8. Param2 根据 Opcode 有不同含义（SlotID/KeyID/Address/TargetKey）
9. Data 根据 Opcode 有不同格式（CheckMac 有 ClientChal+ClientResp+OtherData 等）
10. Status 响应: 0x00=success, 0x01=checkmac fail, 0x03=parse error, 0x0f=execution error, 0x11=ready, 0xff=CRC error

### recv_proto 实现伪代码

```c
static void atsha204a_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    atsha204a_state *s = (atsha204a_state *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    switch (s->state) {
    case ATSHA204A_IDLE:
        if (strcmp(cmd, "START") == 0) {
            s->state = ATSHA204A_GET_SLAVE_ADDR;
            s->ss_block = start_sample;
        }
        break;
    case ATSHA204A_GET_SLAVE_ADDR:
        if (strcmp(cmd, "ADDRESS READ") == 0)
            s->state = ATSHA204A_READ_REGS;
        else if (strcmp(cmd, "ADDRESS WRITE") == 0)
            s->state = ATSHA204A_WRITE_REGS;
        break;
    case ATSHA204A_READ_REGS:
        if (strcmp(cmd, "DATA READ") == 0) {
            uint8_t b = (data_len > 0) ? data[0] : 0;
            // 追加到 bytes 缓冲区
            s->bytes[s->num_bytes].ss = start_sample;
            s->bytes[s->num_bytes].es = end_sample;
            s->bytes[s->num_bytes].val = b;
            s->num_bytes++;
        } else if (strcmp(cmd, "STOP") == 0) {
            s->es_block = end_sample;
            s->opcode = -1; // 重置 opcode
            if (s->num_bytes > 0)
                output_rx_bytes(di, s);
            s->num_bytes = 0;
            s->waddr = -1;
            s->state = ATSHA204A_IDLE;
        }
        break;
    case ATSHA204A_WRITE_REGS:
        if (strcmp(cmd, "DATA WRITE") == 0) {
            uint8_t b = (data_len > 0) ? data[0] : 0;
            s->bytes[s->num_bytes].ss = start_sample;
            s->bytes[s->num_bytes].es = end_sample;
            s->bytes[s->num_bytes].val = b;
            s->num_bytes++;
        } else if (strcmp(cmd, "STOP") == 0) {
            s->es_block = end_sample;
            output_tx_bytes(di, s);
            s->num_bytes = 0;
            s->state = ATSHA204A_IDLE;
        }
        break;
    }
}
```

### 字节缓冲区设计

由于 ATSHA204A 的帧可能很长（如 CheckMac 有 84 字节），需要较大的缓冲区:
```c
#define ATSHA204A_MAX_BYTES 256
typedef struct {
    uint64_t ss;
    uint64_t es;
    uint8_t val;
} atsha204a_byte_entry;

typedef struct {
    enum atsha204a_state state;
    int waddr;
    int opcode;
    uint64_t ss, es;
    uint64_t ss_block, es_block;
    atsha204a_byte_entry bytes[ATSHA204A_MAX_BYTES];
    int num_bytes;
    int out_ann;
} atsha204a_state;
```

---

## 4. bh1750 — Digital Ambient Light Sensor

### Python 元数据

| 属性 | 值 |
|------|-----|
| id | `"bh1750"` |
| name | `"BH1750"` |
| longname | `"Digital ambient light sensor BH1750"` |
| desc | `"Digital 16bit Serial Output Type Ambient Light Sensor IC."` |
| license | `"gplv2+"` |
| inputs | `["i2c"]` |
| outputs | `["bh1750"]` | <!-- Updated: C版需在start()中注册SRD_OUTPUT_PYTHON输出(proto_id="bh1750")，供上层解码器消费 --> |
| tags | `["Embedded/industrial"]` |

### options (2 个)

| id | desc | default | values |
|----|------|---------|--------|
| radix | Number format | "Hex" | ("Hex", "Dec", "Oct", "Bin") |
| params | Datasheet parameter used | "Typical" | ("Typical", "Maximal", "Minimal") |

### annotations (23 个)

通过 `hlp.create_annots()` 动态生成，实际枚举:

**AnnAddrs** (0-1):
| Index | id | labels |
|-------|----|--------|
| 0 | addr_gnd | ADDR grounded, ADDR_GND, AG |
| 1 | addr_vcc | ADDR powered, ADDR_VCC, AV |

**AnnRegs** (2-13):
| Index | id | labels |
|-------|----|--------|
| 2 | pwrdwn | Power down, Pwr Dwn, Off, D |
| 3 | pwrup | Power up, Pwr Up, On, U |
| 4 | reset | Reset light register, Reset light, Reset, Rst, R |
| 5 | mthigh | Measurement time high bits, Mtime Hbits, MTH, H |
| 6 | mtlow | Measurement time low bits, Mtime Lbits, MTL, L |
| 7 | mchigh | Continuous measurement high resolution, Cont high, CH |
| 8 | mchigh2 | Continuous measurement double high res, Cont double, CH2 |
| 9 | mclow | Continuous measurement low resolution, Cont low, CL |
| 10 | mohigh | One time measurement high resolution, One high, OH |
| 11 | mohigh2 | One time measurement double high res, One double, OH2 |
| 12 | molow | One time measurement low resolution, One low, OL |
| 13 | data | Illuminance data register, Illuminance, Light, L |

**AnnBits** (14-15):
| Index | id | labels |
|-------|----|--------|
| 14 | reserved | Reserved, Rsvd, R |
| 15 | data_mt | Measurement time, MT, M |

**AnnInfo** (16-23):
| Index | id | labels |
|-------|----|--------|
| 16 | warn | Warnings, Warn, W |
| 17 | badadd | Unknown slave address, Unknown address, Unknown, Unk, U |
| 18 | check | Slave presence check, Slave check, Check, Chk, C |
| 19 | write | Write, Wr, W |
| 20 | read | Read, Rd, R |
| 21 | sense | Sensitivity, Sense, S |
| 22 | light | Ambient light, Light, L |
| 23 | mtreg | Measurement time register, MTreg, MTR, R |
| 24 | mtime | Measurement time, MTime, MT, T |

### annotation_rows (4 行)

| Row id | label | classes |
|--------|-------|---------|
| bits | Bits | (14, 15) |
| regs | Registers | (0..13) |
| info | Info | (18..24) |
| warnings | Warnings | (16, 17) |

### 解码逻辑分析

**状态机**: IDLE → ADDRESS SLAVE → REGISTER ADDRESS → REGISTER DATA

**关键点**:
1. Slave 地址: 0x23 (GND) 或 0x5C (VCC)
2. BH1750 没有传统寄存器地址 — 写入的第一个字节就是命令/寄存器
3. 命令格式: 高 3 位为命令码，低 5 位可能为参数
4. MTreg (Measurement Time Register): 通过 MTHIGH (0x40-0x43) 和 MTLOW (0x60-0x6F) 修改
5. 光照计算: `light = rawdata * sensitivity`, 其中 `sensitivity = (1/accuracy) * MTREG_TYP / mtreg`
6. 双分辨率模式 (MCHIGH2/MOHIGH2) 灵敏度减半
7. 读数据为 2 字节（大端），组成 16-bit 原始值
8. 支持 START REPEAT 进行连续读操作

### recv_proto 实现伪代码

```c
static void bh1750_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    bh1750_state *s = (bh1750_state *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    if (strcmp(cmd, "BITS") == 0) return; // C 解码器忽略

    switch (s->state) {
    case BH1750_IDLE:
        if (strcmp(cmd, "START") == 0) {
            s->ssb = start_sample;
            s->state = BH1750_ADDRESS_SLAVE;
        }
        break;
    case BH1750_ADDRESS_SLAVE:
        if (strcmp(cmd, "ADDRESS WRITE") == 0) {
            uint8_t addr = (data_len > 0) ? data[0] : 0;
            if (addr == 0x23 || addr == 0x5C) {
                s->addr = addr;
                s->is_write = 1;
                // 输出地址注释
                int ann = (addr == 0x23) ? ANN_ADDR_GND : ANN_ADDR_VCC;
                C_ANN_PUT(di, s->ss, s->es, s->out_ann, ann, ...);
                s->state = BH1750_REGISTER_ADDRESS;
            } else {
                C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_BADADD, ...);
                s->state = BH1750_IDLE;
            }
        } else if (strcmp(cmd, "ADDRESS READ") == 0) {
            uint8_t addr = (data_len > 0) ? data[0] : 0;
            if (addr == 0x23 || addr == 0x5C) {
                s->addr = addr;
                s->is_write = 0;
                s->state = BH1750_REGISTER_ADDRESS;
            } else {
                s->state = BH1750_IDLE;
            }
        }
        break;
    case BH1750_REGISTER_ADDRESS:
        if (strcmp(cmd, "DATA WRITE") == 0) {
            uint8_t reg = (data_len > 0) ? data[0] : 0;
            handle_register(di, s, reg);
            s->state = BH1750_REGISTER_DATA;
        } else if (strcmp(cmd, "DATA READ") == 0) {
            s->data_bytes[s->num_data++] = (data_len > 0) ? data[0] : 0;
            s->state = BH1750_REGISTER_DATA;
        } else if (strcmp(cmd, "STOP") == 0 || strcmp(cmd, "START REPEAT") == 0) {
            // 无数据传输，仅检查从设备存在
            C_ANN_PUT(di, s->ssb, s->es, s->out_ann, ANN_CHECK, "Slave presence check");
            s->state = (strcmp(cmd, "START REPEAT") == 0) ? BH1750_ADDRESS_SLAVE : BH1750_IDLE;
        }
        break;
    case BH1750_REGISTER_DATA:
        if (strcmp(cmd, "DATA WRITE") == 0 || strcmp(cmd, "DATA READ") == 0) {
            uint8_t b = (data_len > 0) ? data[0] : 0;
            if (s->is_write) {
                // 写模式数据
            } else {
                // 读模式数据
                if (s->num_data == 0) s->ssd = start_sample;
                s->data_bytes[s->num_data++] = b;
            }
        } else if (strcmp(cmd, "START REPEAT") == 0) {
            handle_data(di, s);
            s->ssb = start_sample;
            s->state = BH1750_ADDRESS_SLAVE;
        } else if (strcmp(cmd, "STOP") == 0) {
            handle_data(di, s);
            s->state = BH1750_IDLE;
        }
        break;
    }
}
```

---

## 5. eeprom24xx — 24xx Series I2C EEPROM

### Python 元数据

| 属性 | 值 |
|------|-----|
| id | `"eeprom24xx"` |
| name | `"24xx EEPROM"` |
| longname | `"24xx I2C EEPROM"` |
| desc | `"24xx series I2C EEPROM protocol."` |
| license | `"gplv2+"` |
| inputs | `["i2c"]` |
| outputs | `[]` |
| tags | `["IC", "Memory"]` |

### options (2 个)

| id | desc | default | values |
|----|------|---------|--------|
| chip | Chip | "generic" | (所有 chips.keys()) |
| addr_counter | Initial address counter value | 0 | int |

### annotations (21 个)

| Index | id | label |
|-------|----|-------|
| 0 | warnings | Warnings |
| 1 | control-code | Control code |
| 2 | address-pin | Address pin (A0/A1/A2) |
| 3 | rw-bit | Read/write bit |
| 4 | word-addr-byte | Word address byte |
| 5 | data-byte | Data byte |
| 6 | control-word | Control word |
| 7 | word-addr | Word address |
| 8 | data | Data |
| 9 | byte-write | Byte write |
| 10 | page-write | Page write |
| 11 | cur-addr-read | Current address read |
| 12 | random-read | Random read |
| 13 | seq-random-read | Sequential random read |
| 14 | seq-cur-addr-read | Sequential current address read |
| 15 | ack-polling | Acknowledge polling |
| 16 | set-bank-addr | Set bank address |
| 17 | read-bank-addr | Read bank address |
| 18 | set-wp | Set write protection |
| 19 | clear-all-wp | Clear all write protection |
| 20 | read-wp | Read write protection status |

### annotation_rows (4 行)

| Row id | label | classes |
|--------|-------|---------|
| bits-bytes | Bits/bytes | (1, 2, 3, 4, 5) |
| fields | Fields | (6, 7, 8) |
| ops | Operations | (9..20) |
| warnings | Warnings | (0,) |

### binary (1 个)

| Index | id | label |
|-------|----|-------|
| 0 | binary | Binary |

### 解码逻辑分析

**⚠️ 这是最复杂的解码器** — 状态机有 17+ 个状态，处理多种读写操作。

**状态机** (Python 使用动态方法分发 `handle_{state_lower}`):

1. `WAIT FOR START` → 等待 START/START REPEAT
2. `GET CONTROL WORD` → 处理 ADDRESS READ/WRITE
3. `READ GET ACK NACK AFTER CONTROL WORD` → ACK/NACK
4. `READ GET WORD ADDR OR BYTE` → DATA READ
5. `READ GET ACK NACK AFTER WORD ADDR OR BYTE` → ACK→RESTART, NACK→cur_addr_read
6. `READ GET RESTART` → RESTART
7. `READ BYTE` → DATA READ
8. `READ GET ACK NACK AFTER BYTE WAS READ` → ACK→继续读, NACK→结束
9. `WRITE GET ACK NACK AFTER CONTROL WORD` → ACK→W GET WORD ADDR
10. `WRITE GET WORD ADDR` → DATA WRITE
11. `WRITE GET ACK AFTER WORD ADDR` → ACK→确定读写方向
12. `WRITE DETERMINE EEPROM READ OR WRITE` → START REPEAT→随机读, DATA WRITE→写
13. `WRITE WRITE BYTE` → DATA WRITE/STOP/START REPEAT
14. `WRITE GET ACK NACK AFTER BYTE WAS WRITTEN` → ACK→继续写
15. `R2 GET CONTROL WORD` → ADDRESS READ (随机读第二阶段)
16. `R2 GET ACK AFTER ADDR READ` → ACK
17. `R2 READ BYTE` → DATA READ
18. `R2 GET ACK NACK AFTER BYTE WAS READ` → ACK→继续, NACK→结束
19. `GET STOP AFTER LAST BYTE` → STOP

**操作类型**:
- Byte Write: ADDR WRITE → Word Addr → 1 Data Byte → STOP
- Page Write: ADDR WRITE → Word Addr → N Data Bytes → STOP
- Current Address Read: ADDR READ → 1 Data Byte → NACK → STOP
- Random Read: ADDR WRITE → Word Addr → START REPEAT → ADDR READ → 1 Data Byte → NACK → STOP
- Sequential Random Read: 同 Random Read 但多个 Data Byte
- Sequential Current Address Read: ADDR READ → N Data Bytes → NACK → STOP

**芯片配置**: 通过 `lists.py` 中的 `chips` 字典定义，包含 vendor, model, size, page_size, addr_bytes (1 or 2), addr_pins, max_speed。Python版有16个芯片条目。 <!-- Updated: 明确芯片条目数量为16个 -->

### C 实现要点

1. **状态机**: 使用 enum 定义所有状态，recv_proto 中用 switch-case
2. **芯片配置**: 使用 C 结构体数组替代 Python 字典
3. **数据缓冲**: 需要存储所有 packet 的 (ss, es, databyte) 用于后续注释
4. **地址计数器**: 维护 `addr_counter`，每次数据字节后自增
5. **Binary 输出**: 使用 `c_decoder_put_binary()` 输出 EEPROM 数据
6. **ACK/NACK 处理**: Python 版本处理 ACK/NACK，C 版本在 recv_proto 中也需要处理

### recv_proto 状态枚举

```c
enum eeprom24xx_state {
    EEPROM24XX_WAIT_FOR_START,
    EEPROM24XX_GET_CONTROL_WORD,
    EEPROM24XX_R_GET_ACK_NACK_AFTER_CW,
    EEPROM24XX_R_GET_WORD_ADDR_OR_BYTE,
    EEPROM24XX_R_GET_ACK_NACK_AFTER_WORD_ADDR_OR_BYTE,
    EEPROM24XX_R_GET_RESTART,
    EEPROM24XX_R_READ_BYTE,
    EEPROM24XX_R_GET_ACK_NACK_AFTER_BYTE_READ,
    EEPROM24XX_W_GET_ACK_NACK_AFTER_CW,
    EEPROM24XX_W_GET_WORD_ADDR,
    EEPROM24XX_W_GET_ACK_AFTER_WORD_ADDR,
    EEPROM24XX_W_DETERMINE_READ_OR_WRITE,
    EEPROM24XX_W_WRITE_BYTE,
    EEPROM24XX_W_GET_ACK_NACK_AFTER_BYTE_WRITTEN,
    EEPROM24XX_R2_GET_CONTROL_WORD,
    EEPROM24XX_R2_GET_ACK_AFTER_ADDR_READ,
    EEPROM24XX_R2_READ_BYTE,
    EEPROM24XX_R2_GET_ACK_NACK_AFTER_BYTE_READ,
    EEPROM24XX_GET_STOP_AFTER_LAST_BYTE,
};
```

### 芯片配置结构体

```c
typedef struct {
    const char *key;
    const char *vendor;
    const char *model;
    int size;         // total bytes
    int page_size;    // page size in bytes
    int page_wraparound;
    int addr_bytes;   // 1 or 2
    int addr_pins;    // 0-3
    int max_speed;    // kHz
} eeprom24xx_chip;

static const eeprom24xx_chip chip_table[] = {
    {"generic", "", "Generic", 128, 8, 1, 1, 3, 400},
    {"microchip_24aa65", "Microchip", "24AA65", 8192, 64, 1, 2, 3, 400},
    // ... 其余芯片
};
```

---

## 通用 C 解码器模板

每个解码器文件 `{id}_c.c` 的结构:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

// 1. 注释枚举
enum { ANN_XXX = 0, ..., NUM_ANN };

// 2. 状态枚举
enum xxx_state { XXX_IDLE, ..., };

// 3. 私有数据结构
typedef struct {
    enum xxx_state state;
    // ... 状态变量
    int out_ann;
} xxx_state;

// 4. 输入/标签/选项/注释标签/注释行 声明
static const char *xxx_inputs[] = {"i2c", NULL};
static const char *xxx_tags[] = {..., NULL};
static struct srd_decoder_option xxx_options[] = {...};
static const char *xxx_ann_labels[][3] = {...};
static const struct srd_c_ann_row xxx_ann_rows[] = {...};

// 5. 辅助函数
static void xxx_putx(...) { C_ANN_PUT(...); }
static void xxx_putb(...) { C_ANN_PUT(...); }

// 6. recv_proto 回调
static void xxx_recv_proto(struct srd_decoder_inst *di, ...) { ... }

// 7. reset/start/decode/destroy 回调
static void xxx_reset(struct srd_decoder_inst *di) { ... }
static void xxx_start(struct srd_decoder_inst *di) { ... }
static void xxx_decode(struct srd_decoder_inst *di) { (void)di; }
static void xxx_destroy(struct srd_decoder_inst *di) { ... }

// 8. srd_c_decoder 结构体
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    .longname = "...",
    .desc = "... (C implementation)",
    .license = "...",
    .channels = NULL, .num_channels = 0,
    .optional_channels = NULL, .num_optional_channels = 0,
    .options = xxx_options, .num_options = N,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = N,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs, .num_inputs = 1,
    .outputs = NULL, .num_outputs = 0,
    .binary = NULL, .num_binary = 0,
    .tags = xxx_tags, .num_tags = N,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,
    .destroy = xxx_destroy,
    .recv_proto = xxx_recv_proto,
};

// 9. 入口函数
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void) {
    // 初始化选项默认值 (GVariant)
    return &xxx_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void) {
    return SRD_C_DECODER_API_VERSION;
}
```

---

## CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加:
```
ad5593r_c
adxl345_c
atsha204a_c
bh1750_c
eeprom24xx_c
```

---

## 解码器依赖关系

<!-- Updated: 新增依赖关系说明，明确C解码器只能依赖已有C实现的底层解码器 -->

| 解码器 | 输入类型 | 依赖的底层C解码器 | 状态 |
|--------|----------|-------------------|------|
| ad5593r_c | i2c | i2c_c.c ✅ | 可实现 |
| adxl345_c | spi | spi_c.c ✅ | 可实现（已修正为SPI上层解码器） |
| atsha204a_c | i2c | i2c_c.c ✅ | 可实现 |
| bh1750_c | i2c | i2c_c.c ✅ | 可实现 |
| eeprom24xx_c | i2c | i2c_c.c ✅ | 可实现 |
