# Python → C 解码器移植规格书 — Batch 21

## 概述

本批次移植 5 个 I2C 上层协议解码器，均为 `inputs=['i2c']` 的堆叠解码器，使用 `recv_proto()` 回调接收 I2C 下层协议数据，而非直接 `decode()` 原始采样。

| # | Python ID | C ID | 器件名称 | 复杂度 |
|---|-----------|------|----------|--------|
| 1 | `rtc8564` | `rtc8564_c` | Epson RTC-8564 JE/NB 实时时钟 | ★★☆ |
| 2 | `ssd1306` | `ssd1306_c` | Solomon SSD1306 OLED 控制器 | ★★★★ |
| 3 | `st25dv` | `st25dv_c` | ST25DV NFC EEPROM | ★★★ |
| 4 | `tcs3472x` | `tcs3472x_c` | TCS3472x 颜色传感器 | ★★☆ |
| 5 | `tpm_tis_i2c` | `tpm_tis_i2c_c` | TPM TIS 2.0 over I2C | ★★★ |

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| i2c_c.c | 底层协议输出范本 | START/STOP条件检测、c_decoder_put_python()输出I2C协议数据、BITS v2格式 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机、7位地址检查 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |

- **C Decoder API**: `libsigrokdecode/libsigrokdecode.h` 中 `srd_c_decoder` 结构体
- **C Decoder 辅助宏**: `C_ANN_PUT`, `C_ANN_PUT_TYPE`, `C_ANN_PUT_VAL`, `c_decoder_register_output()`, `c_decoder_get_private()` 等
- **SRD_OUTPUT_LOGIC**: `c_decoder_register_output(di, SRD_OUTPUT_LOGIC, "xxx")` + `c_decoder_put_logic()` 用于逻辑输出通道 <!-- Updated: SRD_OUTPUT_LOGIC 和 c_decoder_put_logic() 已实现 -->
- **c_cond_wait_current()**: 在 start() 中获取初始采样位置，已实现 <!-- Updated: c_cond_wait_current() 已实现 -->
- **c_decoder_get_initial_pin()**: 获取通道初始电平，已实现 <!-- Updated: c_decoder_get_initial_pin() 已实现 -->

## C 解码器上层协议规范

### 核心原则

1. **文件命名**: `{decoder_id}_c.c`，如 `rtc8564_c.c`
2. **结构体 ID**: `.id = "xxx_c"`，如 `"rtc8564_c"`
3. **结构体 name**: `.name = "XXX(C)"`，如 `"RTC-8564(C)"`
4. **使用 `recv_proto()` 回调**，而非 `decode()`
5. **注册 python output**: 在 `start()` 中调用 `c_decoder_register_output(di, SRD_OUTPUT_ANN, "xxx")`
6. **ann_labels 第一列**: 必须为 `""`（空字符串）
7. **所有 ann class 必须映射到 row**
8. **channels/optional_channels**: 均为 NULL / 0（I2C 上层解码器不直接接触信号线）
9. **inputs**: `{"i2c", NULL}`
10. **outputs**: 视原始 Python 解码器是否定义，若无则 `NULL / 0`

### recv_proto() 函数签名

```c
static void xxx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
```

- `cmd`: I2C 协议命令字符串，如 `"START"`, `"START REPEAT"`, `"ADDRESS WRITE"`, `"ADDRESS READ"`, `"DATA WRITE"`, `"DATA READ"`, `"ACK"`, `"NACK"`, `"STOP"`, `"BITS"`
- `data`: 附加数据字节（如地址值、数据值）。**注意**：I2C C 解码器默认使用 `address_format=shifted`，因此 ADDRESS WRITE/READ 的 data 为 **7 位地址**（如 TCA6408A 地址为 0x20 而非 0x40）
- `data_len`: data 长度
- **BITS v2 格式**：BITS 消息现已包含 per-bit 时间戳，格式为 `data[0]=flags, data[1]=mosi_count, data[2..]=per-bit [value(1B)][ss(8B LE)][es(8B LE)], 0x00, miso_count, ...`。I2C 上层解码器通常忽略 BITS，如需使用可参考 spi_c.c / i2c_c.c 中的 BITS 输出代码 <!-- Updated: BITS v2 格式已实现，i2c_c.c 已输出 per-bit ss/es -->

### 状态机模式

I2C 上层解码器普遍采用状态机模式处理 I2C 事务流：

```
IDLE → (START) → GET_SLAVE_ADDR → (ADDRESS WRITE/READ) → GET_REG_ADDR → (DATA WRITE) → READ/WRITE_REGS → (STOP) → IDLE
```

---

## 1. rtc8564 — Epson RTC-8564 JE/NB 实时时钟

### 1.1 Python 元数据

| 属性 | 值 |
|------|-----|
| `id` | `rtc8564` |
| `name` | `RTC-8564` |
| `longname` | `Epson RTC-8564 JE/NB` |
| `desc` | `Realtime clock module protocol.` |
| `license` | `gplv2+` |
| `inputs` | `['i2c']` |
| `outputs` | `[]` |
| `tags` | `['Clock/timing']` |
| `channels` | 无 |
| `optional_channels` | 无 |
| `options` | 无 |
| `binary` | 无 |

### 1.2 Annotations (16 个)

| Index | ID | 描述 |
|-------|----|------|
| 0 | `reg-0x00` | Register 0x00 |
| 1 | `reg-0x01` | Register 0x01 |
| 2 | `reg-0x02` | Register 0x02 |
| 3 | `reg-0x03` | Register 0x03 |
| 4 | `reg-0x04` | Register 0x04 |
| 5 | `reg-0x05` | Register 0x05 |
| 6 | `reg-0x06` | Register 0x06 |
| 7 | `reg-0x07` | Register 0x07 |
| 8 | `reg-0x08` | Register 0x08 |
| 9 | `read` | Read date/time |
| 10 | `write` | Write date/time |
| 11 | `bit-reserved` | Reserved bit |
| 12 | `bit-vl` | VL bit |
| 13 | `bit-century` | Century bit |
| 14 | `reg-read` | Register read |
| 15 | `reg-write` | Register write |

### 1.3 Annotation Rows

| Row ID | 名称 | 包含的 annotation indices |
|--------|------|--------------------------|
| `bits` | Bits | 0-8, 11, 12, 13 |
| `regs` | Register access | 14, 15 |
| `date-time` | Date/time | 9, 10 |

### 1.4 Decode 逻辑分析

**状态机**:
```
IDLE → GET SLAVE ADDR → GET REG ADDR → WRITE RTC REGS → (STOP) → IDLE
                                        ↘ (START REPEAT) → READ RTC REGS → READ RTC REGS2 → (STOP) → IDLE
```

**关键逻辑**:
- 使用 `bcd2int()` 将 BCD 编码转换为整数
- 寄存器地址自动递增（`self.reg += 1`）
- `handle_reg_0x%02x` 动态分发到各寄存器处理函数
- 寄存器 0x00-0x08 有具体处理，0x09-0x0f 为空（pass）
- BITS 包用于 bit-level 注解（`putd`, `putr`），C 实现中可简化
- VL bit (bit 7 of reg 0x02)、Century bit (bit 7 of reg 0x07) 有特殊标注
- STOP 时输出完整日期时间字符串

**I2C 从机地址**: 0xA2 (写) / 0xA3 (读)，但 Python 代码中未强制过滤

### 1.5 C 实现要点

1. **BCD 转换**: 自行实现 `bcd2int()` 辅助函数
2. **状态机**: 6 个状态枚举
3. **寄存器处理**: 用 switch-case 或函数指针数组
4. **BITS 注解简化**: C 解码器中 bit-level 注解（`putd`/`putr`）可省略，仅保留寄存器级别和日期时间级别注解。如需实现 bit-level 注解，BITS v2 格式已提供 per-bit 时间戳，可直接使用 <!-- Updated: BITS v2 已提供 per-bit ss/es，bit-level 注解现可精确实现 -->
5. **ann_labels**: 16 个条目，第一列为 `""`

### 1.6 关键 C 代码片段

```c
enum rtc8564_state {
    RTC8564_IDLE,
    RTC8564_GET_SLAVE_ADDR,
    RTC8564_GET_REG_ADDR,
    RTC8564_WRITE_RTC_REGS,
    RTC8564_READ_RTC_REGS,
    RTC8564_READ_RTC_REGS2,
};

typedef struct {
    enum rtc8564_state state;
    int reg;
    int hours, minutes, seconds;
    int days, weekdays, months, years;
    uint64_t ss, es, ss_block;
    int out_ann;
} rtc8564_state;

static int bcd2int(uint8_t b)
{
    return (b >> 4) * 10 + (b & 0x0f);
}

static void rtc8564_handle_reg(struct srd_decoder_inst *di,
    rtc8564_state *s, uint8_t reg, uint8_t b)
{
    char buf[256];
    switch (reg) {
    case 0x02: { /* Seconds / VL bit */
        int vl = (b >> 7) & 1;
        int sec = bcd2int(b & 0x7f);
        s->seconds = sec;
        snprintf(buf, sizeof(buf), "Second: %d, VL: %d", sec, vl);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, 2, buf);
        break;
    }
    case 0x03: { /* Minutes */
        int min = bcd2int(b & 0x7f);
        s->minutes = min;
        snprintf(buf, sizeof(buf), "Minute: %d", min);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, 3, buf);
        break;
    }
    /* ... 其他寄存器 ... */
    case 0x08: { /* Years */
        int year = bcd2int(b);
        s->years = year;
        snprintf(buf, sizeof(buf), "Year: %d", year);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, 8, buf);
        break;
    }
    default:
        break;
    }
}

static void rtc8564_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    rtc8564_state *s = (rtc8564_state *)c_decoder_get_private(di);
    if (!s) return;
    s->ss = start_sample;
    s->es = end_sample;
    uint8_t databyte = (data_len > 0) ? data[0] : 0;

    if (strcmp(cmd, "BITS") == 0) return;

    switch (s->state) {
    case RTC8564_IDLE:
        if (strcmp(cmd, "START") == 0) {
            s->state = RTC8564_GET_SLAVE_ADDR;
            s->ss_block = start_sample;
        }
        break;
    case RTC8564_GET_SLAVE_ADDR:
        if (strcmp(cmd, "ADDRESS WRITE") == 0)
            s->state = RTC8564_GET_REG_ADDR;
        break;
    case RTC8564_GET_REG_ADDR:
        if (strcmp(cmd, "DATA WRITE") == 0) {
            s->reg = databyte;
            s->state = RTC8564_WRITE_RTC_REGS;
        }
        break;
    case RTC8564_WRITE_RTC_REGS:
        if (strcmp(cmd, "START REPEAT") == 0) {
            s->state = RTC8564_READ_RTC_REGS;
        } else if (strcmp(cmd, "DATA WRITE") == 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Write register %02X: %02X", s->reg, databyte);
            C_ANN_PUT(di, s->ss, s->es, s->out_ann, 15, buf);
            rtc8564_handle_reg(di, s, s->reg, databyte);
            s->reg++;
        } else if (strcmp(cmd, "STOP") == 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Write date/time: %02d.%02d.%02d %02d:%02d:%02d",
                s->days, s->months, s->years, s->hours, s->minutes, s->seconds);
            C_ANN_PUT(di, s->ss_block, end_sample, s->out_ann, 10, buf);
            s->state = RTC8564_IDLE;
        }
        break;
    case RTC8564_READ_RTC_REGS:
        if (strcmp(cmd, "ADDRESS READ") == 0)
            s->state = RTC8564_READ_RTC_REGS2;
        break;
    case RTC8564_READ_RTC_REGS2:
        if (strcmp(cmd, "DATA READ") == 0) {
            char buf[64];
            snprintf(buf, sizeof(buf), "Read register %02X: %02X", s->reg, databyte);
            C_ANN_PUT(di, s->ss, s->es, s->out_ann, 14, buf);
            rtc8564_handle_reg(di, s, s->reg, databyte);
            s->reg++;
        } else if (strcmp(cmd, "STOP") == 0) {
            char buf[128];
            snprintf(buf, sizeof(buf), "Read date/time: %02d.%02d.%02d %02d:%02d:%02d",
                s->days, s->months, s->years, s->hours, s->minutes, s->seconds);
            C_ANN_PUT(di, s->ss_block, end_sample, s->out_ann, 9, buf);
            s->state = RTC8564_IDLE;
        }
        break;
    }
}
```

---

## 2. ssd1306 — Solomon SSD1306 OLED 控制器

### 2.1 Python 元数据

| 属性 | 值 |
|------|-----|
| `id` | `ssd1306` |
| `name` | `SSD1306` |
| `longname` | `Solomon 1306` |
| `desc` | `Solomon SSD1306 OLED controller protocol.` |
| `license` | `gplv2+` |
| `inputs` | `['i2c']` |
| `outputs` | `[]` |
| `tags` | `['Display', 'IC']` |
| `channels` | 无 |
| `optional_channels` | 无 |
| `options` | 无 |
| `binary` | 无 |

### 2.2 Annotations

SSD1306 的注解非常复杂，分为 bit 级别注解和命令级别注解。

**Bit 级别注解 (10 个, index 0-9)**:
| Index | ID | 描述 |
|-------|----|------|
| 0 | `bit_display_addressing` | Display Addressing bit |
| 1 | `bit_reserved` | Reserved bit |
| 2 | `bit_start_line` | Start Line bit |
| 3 | `bit_continuation` | Continuation bit |
| 4 | `bit_data_command` | Data / Command bit |
| 5 | `bit_page` | Page bit |
| 6 | `bit_column` | Column bit |
| 7 | `bit_mux` | mux bit |
| 8 | `bit_parameter` | Parameter bit |
| 9 | `bit_last` | Last bit |

**命令级别注解 (30 个, index 10-39)**:
| Index | ID | 描述 |
|-------|----|------|
| 10 | `cmd_lowercolstart` | Set Lower Column Start Address |
| 11 | `cmd_highercolstart` | Set Higher Column Start Address |
| 12 | `cmd_displaymode` | Set Display Mode |
| 13 | `cmd_setcoladdress` | Set Column Address |
| 14 | `cmd_setpageaddress` | Set Page Address |
| 15 | `cmd_setfadeoutblinking` | Set Fade-out and Blinking |
| 16 | `cmd_righthorscroll` | Right horizontal scroll |
| 17 | `cmd_lefthorscroll` | Left horizontal scroll |
| 18 | `cmd_vertrighthorscroll` | Vertical and right horizontal scroll |
| 19 | `cmd_vertlefthorscroll` | Vertical and left horizontal scroll |
| 20 | `cmd_stopscrolling` | Stop scrolling |
| 21 | `cmd_activatescrolling` | Activate scrolling |
| 22 | `cmd_displaystartline` | Display start line |
| 23 | `cmd_setcontrast` | Set contrast control |
| 24 | `cmd_setchargepump` | Set charge pump |
| 25 | `cmd_mapcol0toseg0` | Map col addr0 to seg0 |
| 26 | `cmd_mapcol127toseg0` | Map col addr7f to seg0 |
| 27 | `cmd_setvertscrollarea` | Set vertical scroll area |
| 28 | `cmd_displayonresume` | Display on, resume to RAM |
| 29 | `cmd_displayonignore` | Display on, ignore RAM |
| 30 | `cmd_normaldisplay` | Normal display |
| 31 | `cmd_inversedisplay` | Inverse display |
| 32 | `cmd_setmultiplexratio` | Set multiplex ratio |
| 33 | `cmd_displayoff` | Display OFF |
| 34 | `cmd_displayon` | Display ON |
| 35 | `cmd_pgstartaddr` | Page start address |
| 36 | `cmd_comscanup` | COM scan 0 to mux |
| 37 | `cmd_comscandown` | COM scan mux to 0 |
| 38 | `cmd_setverticaloffset` | Set vertical offset |
| 39 | `cmd_displayclockratio` | Display clock ratio |

**额外注解 (5 个, index 40-44)**:
| Index | ID | 描述 |
|-------|----|------|
| 40 | `cmd_zoomin` | Set zoom-in |
| 41 | `cmd_prechargeperiod` | Set precharge period |
| 42 | `cmd_setcompins` | Set COM pins |
| 43 | `cmd_setvcomhdeselect` | Set Vcomh deselect |
| 44 | `cmd_nop` | No operation |

**特殊注解 (5 个, index 45-49)**:
| Index | ID | 描述 |
|-------|----|------|
| 45 | `cmd_gddram` | GDDRAM data write |
| 46 | `cmd_deviceaddress` | Device address |
| 47 | `cmd_controlbyte` | Control byte |
| 48 | `cmd_last` | Last command marker |
| 49 | `write_block` | Write block |
| 50 | `warning` | Warning |

**总计**: 51 个 annotation classes

### 2.3 Annotation Rows

| Row ID | 名称 | 包含的 annotation indices |
|--------|------|--------------------------|
| `bits` | Bits | 0-9 |
| `cmds` | Commands | 10-48 |
| `blockdata` | Block Data | 49 |
| `warnings` | Warnings | 50 |

### 2.4 Decode 逻辑分析

**状态机**:
```
IDLE → GET SLAVE ADDR → WRITE CONTROL BYTE → SSD COMMAND / SSD DATA → (回到 WRITE CONTROL BYTE)
```

**关键逻辑**:
- I2C 从机地址过滤: 仅接受 0x3C 和 0x3D
- 控制字节 (Control Byte): 0x80 表示后续为命令，0x40 表示后续为数据
- 命令有子状态: `COMMAND` 和 `PARAMETER`（部分命令需要参数字节）
- 命令范围归一化: 0x00-0x0F → 0x00, 0x10-0x1F → 0x10, 0x40-0x7F → 0x40, 0xB0-0xB7 → 0xB0
- `cmds2` 字典定义了每个命令的名称、annotation ID、文本、参数数量
- `blockstring` 累积命令块文本，在命令完成后输出

**复杂度分析**: 这是本批次最复杂的解码器，原因：
1. 51 个 annotation classes
2. 大量命令处理函数 (`handle_par_0x%02x`)
3. 命令范围归一化逻辑
4. 子状态 COMMAND/PARAMETER/PARAMETER2 切换
5. bit-level 注解（可简化）

### 2.5 C 实现要点

1. **命令表**: 用结构体数组定义 `cmds2` 等价物
2. **范围归一化**: 在 `recv_proto` 中实现
3. **子状态**: 在状态结构体中添加 `substate` 和 `prevreg` 字段
4. **blockstring**: 用 `char blockstring[256]` 替代 Python 字符串累积
5. **bit-level 注解简化**: 可省略 `putd`/`putr`/`put0`/`put1`，仅保留命令级和块级注解。如需实现，BITS v2 格式已提供 per-bit 时间戳 <!-- Updated: BITS v2 已提供 per-bit ss/es -->
6. **I2C 地址过滤**: 在 ADDRESS WRITE 时检查

### 2.6 关键 C 代码片段

```c
enum ssd1306_state {
    SSD1306_IDLE,
    SSD1306_GET_SLAVE_ADDR,
    SSD1306_WRITE_CONTROL_BYTE,
    SSD1306_SSD_COMMAND,
    SSD1306_SSD_DATA,
};

enum ssd1306_substate {
    SSD1306_SUB_COMMAND,
    SSD1306_SUB_PARAMETER,
    SSD1306_SUB_PARAMETER2,
};

typedef struct {
    uint8_t cmd_byte;    /* 原始命令字节 */
    int ann_id;          /* 对应的 annotation ID */
    const char *texts[3];/* 注解文本 */
    int has_param;       /* 是否需要参数 */
} ssd1306_cmd_entry;

/* 命令表 - 需要完整定义所有 30+ 条命令 */
static const ssd1306_cmd_entry ssd1306_cmds[] = {
    {0x00, ANN_LC, {"Set Lower Column Start Address", "Set L Col Start", "LC"}, 0},
    {0x10, ANN_HC, {"Set Higher Column Start Address", "Set H Col Start", "HC"}, 0},
    {0x20, ANN_DM, {"Set Display Mode", "Set Dsp Md", "DM"}, 1},
    /* ... 其余命令 ... */
};

typedef struct {
    enum ssd1306_state state;
    enum ssd1306_substate substate;
    int prevreg;
    char blockstring[256];
    uint64_t ss, es, ss_block, sscmd;
    int out_ann;
} ssd1306_state;

#define SSD1306_I2C_ADDRESS  0x3C
#define SSD1306_I2C_ADDRESS_2 0x3D

static void ssd1306_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ssd1306_state *s = (ssd1306_state *)c_decoder_get_private(di);
    if (!s) return;
    s->ss = start_sample;
    s->es = end_sample;
    uint8_t databyte = (data_len > 0) ? data[0] : 0;

    if (strcmp(cmd, "BITS") == 0) return;

    switch (s->state) {
    case SSD1306_IDLE:
        if (strcmp(cmd, "START") == 0) {
            s->state = SSD1306_GET_SLAVE_ADDR;
            s->ss_block = start_sample;
        }
        break;
    case SSD1306_GET_SLAVE_ADDR:
        if (strcmp(cmd, "ADDRESS WRITE") == 0) {
            if (databyte != SSD1306_I2C_ADDRESS && databyte != SSD1306_I2C_ADDRESS_2) {
                s->state = SSD1306_IDLE;
                return;
            }
            s->state = SSD1306_WRITE_CONTROL_BYTE;
        }
        break;
    case SSD1306_WRITE_CONTROL_BYTE:
        if (strcmp(cmd, "DATA WRITE") == 0) {
            if (databyte == 0x80)
                s->state = SSD1306_SSD_COMMAND;
            else if (databyte == 0x40)
                s->state = SSD1306_SSD_DATA;
            else
                s->state = SSD1306_IDLE;
            s->substate = SSD1306_SUB_COMMAND;
        } else if (strcmp(cmd, "STOP") == 0) {
            s->state = SSD1306_IDLE;
        }
        break;
    case SSD1306_SSD_COMMAND:
        if (strcmp(cmd, "DATA WRITE") == 0) {
            /* 命令处理逻辑 */
            s->state = SSD1306_WRITE_CONTROL_BYTE;
        }
        break;
    case SSD1306_SSD_DATA:
        if (strcmp(cmd, "DATA WRITE") == 0) {
            char buf[32];
            snprintf(buf, sizeof(buf), "GDDRAM data: 0x%02X", databyte);
            C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_GR, buf);
        } else if (strcmp(cmd, "STOP") == 0) {
            s->state = SSD1306_IDLE;
        }
        break;
    }
}
```

---

## 3. st25dv — ST25DV NFC EEPROM

### 3.1 Python 元数据

| 属性 | 值 |
|------|-----|
| `id` | `st25dv` |
| `name` | `ST25DV` |
| `longname` | `ST25DV` |
| `desc` | `ST25DV NFC EEPROM` |
| `license` | `mit` |
| `inputs` | `['i2c']` |
| `outputs` | `['st25dv']` |
| `tags` | `['Embedded/industrial']` |
| `channels` | 无 |
| `optional_channels` | 无 |
| `options` | 无 |
| `binary` | 无 |

### 3.2 Annotations (5 个)

| Index | ID | 描述 |
|-------|----|------|
| 0 | `sys` | System |
| 1 | `data` | Data |
| 2 | `read` | Read |
| 3 | `write` | Write |
| 4 | `error` | Error |

### 3.3 Annotation Rows

| Row ID | 名称 | 包含的 annotation indices |
|--------|------|--------------------------|
| `regs` | Register access | 0, 1, 2, 3, 4 |

### 3.4 Decode 逻辑分析

**状态机** (基于 step 编号):
```
step 0: BEFORE START → (START/START REPEAT) → step 1
step 1: BEFORE ADDRESS → (ADDRESS WRITE) → step 2
step 2: (ACK) → step 3
step 3: BEFORE REG MSB → (DATA WRITE, MSB) → step 4
step 4: (ACK) → step 5
step 5: BEFORE REG LSB → (DATA WRITE, LSB) → step 6
step 6: (ACK) → step 7
step 7: BEFORE FIRST DATA → (DATA WRITE → WRITE / START REPEAT → READ) → step 8
step 8: BEFORE SECOND DATA → (DATA WRITE/READ / STOP / START REPEAT) → step 0/1
```

**关键逻辑**:
- **2 字节寄存器地址**: 先发 MSB，再发 LSB（与大多数 I2C 器件不同）
- **寄存器定义**: 使用 `Register` 和 `Field` 类定义了 30+ 个寄存器
- **I2C 地址**: 0x53 (DATA, 0xA6<<1) 和 0x57 (SYSTEM, 0xAE<<1)
- **寄存器值注解**: 根据寄存器定义的 Field 列表，输出字段级描述
- **多字节寄存器**: 部分寄存器长度 > 1（如 I2CPASSWD 17 字节, MAILBOX_RAM 256 字节）

### 3.5 C 实现要点

1. **2 字节寄存器地址**: 状态机需要额外步骤处理 MSB + LSB
2. **寄存器表**: 用 C 结构体数组定义寄存器元数据
3. **Field 解析**: 实现字段提取和格式化函数
4. **多字节寄存器**: 需要累积数据字节直到达到寄存器长度
5. **I2C 地址区分**: 0x53 为数据区，0x57 为系统区

### 3.6 关键 C 代码片段

```c
enum st25dv_step {
    ST25DV_STEP_BEFORE_START = 0,
    ST25DV_STEP_BEFORE_ADDRESS = 1,
    ST25DV_STEP_AFTER_ADDR_ACK = 2,
    ST25DV_STEP_BEFORE_REG_MSB = 3,
    ST25DV_STEP_AFTER_REG_MSB_ACK = 4,
    ST25DV_STEP_BEFORE_REG_LSB = 5,
    ST25DV_STEP_AFTER_REG_LSB_ACK = 6,
    ST25DV_STEP_BEFORE_FIRST_DATA = 7,
    ST25DV_STEP_BEFORE_SECOND_DATA = 8,
};

typedef struct {
    uint8_t name[32];   /* short name */
    uint8_t longname[64];/* long name */
    int length;          /* register length in bytes */
    /* fields 简化: 直接在处理函数中硬编码 */
} st25dv_register;

typedef struct {
    enum st25dv_step step;
    uint8_t address;
    uint16_t reg_address;
    const char *op; /* "READ" or "WRITE" */
    uint8_t data[256];
    int data_len;
    uint64_t ss, es;
    uint64_t reg_start_sample, reg_end_sample;
    uint64_t data_start_sample;
    int out_ann;
} st25dv_state;

#define ST25DV_DATA_ADDR   0x53  /* 0xA6 >> 1 */
#define ST25DV_SYSTEM_ADDR 0x57  /* 0xAE >> 1 */

static void st25dv_annotate_register_value(struct srd_decoder_inst *di,
    st25dv_state *s, uint8_t byte)
{
    char buf[256];
    /* 根据 reg_address 查找寄存器定义，格式化输出 */
    /* 简化版本: 直接输出寄存器地址和值 */
    snprintf(buf, sizeof(buf), "Reg %04X: %02X", s->reg_address, byte);
    int ann_code = (strcmp(s->op, "READ") == 0) ? 2 : 3;
    C_ANN_PUT(di, s->ss, s->es, s->out_ann, ann_code, buf);
    s->data[s->data_len++] = byte;
}
```

---

## 4. tcs3472x — TCS3472x 颜色传感器

### 4.1 Python 元数据

| 属性 | 值 |
|------|-----|
| `id` | `tcs3472x` |
| `name` | `TCS3472X` |
| `longname` | `TCS3472X` |
| `desc` | `Color light-to-digital converter with IR filter` |
| `license` | `gplv2+` |
| `inputs` | `['i2c']` |
| `outputs` | `[]` |
| `tags` | `['Embedded/industrial']` |
| `channels` | 无 |
| `optional_channels` | 无 |
| `binary` | 无 |

### 4.2 Options

| ID | 描述 | 默认值 | 可选值 |
|----|------|--------|--------|
| `device_address` | I2C device address | `'0x29'` | `('0x29', '0x39')` |

### 4.3 Annotations (1 个)

| Index | ID | 描述 |
|-------|----|------|
| 0 | `register` | Register |

### 4.4 Annotation Rows

| Row ID | 名称 | 包含的 annotation indices |
|--------|------|--------------------------|
| `registers` | Data | 0 |

### 4.5 Decode 逻辑分析

**状态机** (基于 `State` 类):
```
state_initial → (START) → start
start → (ADDRESS WRITE) → address_write → (ACK) → ack_address_write → (DATA WRITE) → data_write_command → (ACK) → ack_data_write → (STOP) → state_initial
ack_data_write → (DATA WRITE) → data_write → (ACK) → ack_data_write
start → (ADDRESS READ) → address_read → (ACK) → ack_address_read → (DATA READ) → data_read → (ACK/NACK) → ...
```

**关键逻辑**:
- 使用 `State` 类实现通用状态机框架
- 寄存器命令字节: bit 7 = R/W 方向, bit 6 = 地址自动递增, bit 5-0 = 寄存器地址
- `register_id` 全局变量跟踪当前寄存器
- `byte_values` 列表累积读/写的数据字节
- `register_set` 字典定义了 20 个寄存器的名称和格式化函数
- I2C 地址过滤: 通过 `device_address` 选项
- `interpret_byte_values()` 在状态回到 initial 时输出注解

### 4.6 C 实现要点

1. **选项**: 需要定义 `device_address` 选项
2. **状态机**: 简化为枚举 + switch-case，不需要通用 State 框架
3. **寄存器表**: 用结构体数组定义 20 个寄存器
4. **命令字节解析**: 提取 R/W、自动递增、寄存器地址
5. **多字节值**: 部分寄存器为 16 位（2 字节），需要累积

### 4.7 关键 C 代码片段

```c
enum tcs3472x_state {
    TCS3472X_INITIAL,
    TCS3472X_START,
    TCS3472X_ADDR_WRITE,
    TCS3472X_ACK_ADDR_WRITE,
    TCS3472X_ACK_DATA_WRITE,
    TCS3472X_ADDR_READ,
    TCS3472X_ACK_ADDR_READ,
    TCS3472X_ACK_DATA_READ,
    TCS3472X_DATA_WRITE_CMD,
    TCS3472X_DATA_WRITE,
    TCS3472X_DATA_READ,
};

typedef struct {
    enum tcs3472x_state state;
    int register_id;
    uint8_t byte_values[8];
    int num_bytes;
    uint64_t sequence_start, sequence_end;
    int out_ann;
    int device_address;
} tcs3472x_state;

/* 寄存器定义 */
typedef struct {
    uint8_t addr;
    const char *name;
    int width; /* 1 或 2 字节 */
} tcs3472x_register;

static const tcs3472x_register tcs3472x_regs[] = {
    {0x00, "ENABLE", 1},
    {0x01, "ATIME", 1},
    {0x03, "WTIME", 1},
    {0x04, "AILTL", 2},
    {0x05, "AILTH", 1},
    {0x06, "AIHTL", 2},
    {0x07, "AIHTH", 1},
    {0x0C, "PERS", 1},
    {0x0D, "CONFIG", 1},
    {0x0F, "CONTROL", 1},
    {0x12, "ID", 1},
    {0x13, "STATUS", 1},
    {0x14, "CDATAL", 2},
    {0x16, "RDATAL", 2},
    {0x18, "GDATAL", 2},
    {0x1A, "BDATAL", 2},
    {0xFF, NULL, 0} /* 哨兵 */
};
```

---

## 5. tpm_tis_i2c — TPM TIS 2.0 over I2C

### 5.1 Python 元数据

| 属性 | 值 |
|------|-----|
| `id` | `tpm_tis_i2c` |
| `name` | `TPM TIS 2.0 I2C` |
| `longname` | `Trusted Platform Module Interface (TIS 2.0) over Inter-Integrated Circuit Bus` |
| `desc` | `Trusted Platform Module Interface (TIS 2.0) over Inter-Integrated Circuit Bus` |
| `license` | `gplv3+` |
| `inputs` | `['i2c']` |
| `outputs` | `['tpm-tis']` |
| `tags` | `['TPM']` |
| `channels` | 无 |
| `optional_channels` | 无 |
| `options` | 无 |
| `binary` | 无 |

### 5.2 Annotations (5 个)

| Index | ID | 描述 |
|-------|----|------|
| 0 | `address` | Address |
| 1 | `data_read` | Data (Read) |
| 2 | `data_write` | Data (Write) |
| 3 | `transaction` | Transaction |
| 4 | `warning` | Warning |

### 5.3 Annotation Rows

| Row ID | 名称 | 包含的 annotation indices |
|--------|------|--------------------------|
| `protocol` | Protocol | 0, 1, 2 |
| `transactions` | Transactions | 3 |
| `warnings` | Warnings | 4 |

### 5.4 Decode 逻辑分析

**Python 实现特点**: 使用 Python coroutine/generator 模式（`yield from`），非常独特。

**事务流程**:
```
START → ADDRESS WRITE → ACK → DATA WRITE (TIS register addr) → ACK →
  [Read path]:  START REPEAT → ADDRESS READ → ACK → DATA READ* → NACK → STOP
  [Write path]: DATA WRITE* → ACK → STOP
```

**关键逻辑**:
- TIS 寄存器地址为 1 字节
- 读操作: START REPEAT → ADDRESS READ → 多个 DATA READ 直到 NACK
- 写操作: 多个 DATA WRITE 直到 STOP
- 事务注解: 包含地址 + 数据的完整描述
- 使用 `binascii.hexlify()` 格式化数据
- `_finish_annotations()` 辅助函数: 移除比更详细注解更长的简短注解
- 错误处理: 如果收到意外的 I2C 命令，输出 warning 注解并重置

### 5.5 C 实现要点

1. **Coroutine → 状态机**: 将 Python generator 逻辑转换为显式状态机
2. **数据累积**: 读/写操作都需要累积多个数据字节
3. **hex 格式化**: 用 `snprintf` + 循环实现
4. **事务注解**: 在 STOP 时输出完整的 transaction 注解
5. **错误恢复**: 遇到意外命令时重置状态机

### 5.6 关键 C 代码片段

```c
enum tpm_tis_state {
    TPM_TIS_IDLE,
    TPM_TIS_ADDR_WRITE,
    TPM_TIS_ADDR_ACK,
    TPM_TIS_REG_ADDR,
    TPM_TIS_REG_ADDR_ACK,
    TPM_TIS_WAIT_OP,       /* 等待 START REPEAT 或 DATA WRITE */
    TPM_TIS_READ_ADDR_READ,
    TPM_TIS_READ_ADDR_ACK,
    TPM_TIS_READ_DATA,
    TPM_TIS_WRITE_DATA,
};

typedef struct {
    enum tpm_tis_state state;
    uint8_t tis_addr;
    uint8_t data[256];
    int data_len;
    int reading;           /* 1=read, 0=write */
    uint64_t addr_ss, data_ss, data_es;
    int out_ann;
} tpm_tis_state;

static void tpm_tis_output_transaction(struct srd_decoder_inst *di,
    tpm_tis_state *s)
{
    char buf[512];
    const char *op = s->reading ? "Read" : "Write";
    const char *arrow = s->reading ? "->" : "<-";

    /* 数据 hex 字符串 */
    char hex[512];
    int pos = 0;
    for (int i = 0; i < s->data_len && pos < (int)sizeof(hex) - 4; i++)
        pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X", s->data[i]);

    /* Transaction 注解 */
    snprintf(buf, sizeof(buf), "%s %02X %s %s", op, s->tis_addr, arrow, hex);
    C_ANN_PUT(di, s->addr_ss, s->data_es, s->out_ann, 3, buf);
}

static void tpm_tis_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    tpm_tis_state *s = (tpm_tis_state *)c_decoder_get_private(di);
    if (!s) return;
    uint8_t databyte = (data_len > 0) ? data[0] : 0;

    if (strcmp(cmd, "BITS") == 0) return;

    switch (s->state) {
    case TPM_TIS_IDLE:
        if (strcmp(cmd, "START") == 0)
            s->state = TPM_TIS_ADDR_WRITE;
        break;
    case TPM_TIS_ADDR_WRITE:
        if (strcmp(cmd, "ADDRESS WRITE") == 0) {
            s->addr_ss = start_sample;
            s->state = TPM_TIS_ADDR_ACK;
        }
        break;
    case TPM_TIS_ADDR_ACK:
        if (strcmp(cmd, "ACK") == 0)
            s->state = TPM_TIS_REG_ADDR;
        break;
    case TPM_TIS_REG_ADDR:
        if (strcmp(cmd, "DATA WRITE") == 0) {
            s->tis_addr = databyte;
            char buf[16];
            snprintf(buf, sizeof(buf), "%02X", databyte);
            C_ANN_PUT(di, s->addr_ss, end_sample, s->out_ann, 0, buf);
            s->state = TPM_TIS_REG_ADDR_ACK;
        }
        break;
    case TPM_TIS_REG_ADDR_ACK:
        if (strcmp(cmd, "ACK") == 0)
            s->state = TPM_TIS_WAIT_OP;
        break;
    case TPM_TIS_WAIT_OP:
        if (strcmp(cmd, "START REPEAT") == 0) {
            s->reading = 1;
            s->state = TPM_TIS_READ_ADDR_READ;
        } else if (strcmp(cmd, "STOP") == 0) {
            s->reading = 1;
            s->state = TPM_TIS_IDLE;
            /* 需要新的 START */
        } else if (strcmp(cmd, "DATA WRITE") == 0) {
            s->reading = 0;
            s->data_len = 0;
            s->data[s->data_len++] = databyte;
            s->data_ss = start_sample;
            s->state = TPM_TIS_WRITE_DATA;
        }
        break;
    case TPM_TIS_READ_ADDR_READ:
        if (strcmp(cmd, "ADDRESS READ") == 0) {
            s->data_len = 0;
            s->data_ss = start_sample;
            s->state = TPM_TIS_READ_ADDR_ACK;
        }
        break;
    case TPM_TIS_READ_ADDR_ACK:
        if (strcmp(cmd, "ACK") == 0)
            s->state = TPM_TIS_READ_DATA;
        break;
    case TPM_TIS_READ_DATA:
        if (strcmp(cmd, "DATA READ") == 0) {
            if (s->data_len < (int)sizeof(s->data))
                s->data[s->data_len++] = databyte;
            /* 等待 ACK 或 NACK */
        } else if (strcmp(cmd, "ACK") == 0) {
            /* 继续读 */
        } else if (strcmp(cmd, "NACK") == 0) {
            /* 读完成，等待 STOP */
        } else if (strcmp(cmd, "STOP") == 0) {
            s->data_es = end_sample;
            /* 输出 data_read 注解 */
            char hex[512];
            int pos = 0;
            for (int i = 0; i < s->data_len && pos < (int)sizeof(hex) - 4; i++)
                pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X", s->data[i]);
            C_ANN_PUT(di, s->data_ss, s->data_es, s->out_ann, 1, hex);
            tpm_tis_output_transaction(di, s);
            s->state = TPM_TIS_IDLE;
        }
        break;
    case TPM_TIS_WRITE_DATA:
        if (strcmp(cmd, "DATA WRITE") == 0) {
            if (s->data_len < (int)sizeof(s->data))
                s->data[s->data_len++] = databyte;
        } else if (strcmp(cmd, "STOP") == 0) {
            s->data_es = end_sample;
            char hex[512];
            int pos = 0;
            for (int i = 0; i < s->data_len && pos < (int)sizeof(hex) - 4; i++)
                pos += snprintf(hex + pos, sizeof(hex) - pos, "%02X", s->data[i]);
            C_ANN_PUT(di, s->data_ss, s->data_es, s->out_ann, 2, hex);
            tpm_tis_output_transaction(di, s);
            s->state = TPM_TIS_IDLE;
        }
        break;
    }
}
```

---

## 通用实现规范

### 文件结构模板

```c
/*
 * Copyright (C) [year] [author]
 * License: [license]
 *
 * [Decoder description]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

/* ===== Annotation 枚举 ===== */
enum {
    ANN_XXX = 0,
    /* ... */
    NUM_ANN,
};

/* ===== 状态枚举 ===== */
enum xxx_state {
    XXX_IDLE,
    /* ... */
};

/* ===== 私有数据结构 ===== */
typedef struct {
    enum xxx_state state;
    /* ... */
    int out_ann;
} xxx_state;

/* ===== 静态数据 ===== */
static const char *xxx_inputs[] = {"i2c", NULL};
static const char *xxx_tags[] = {"...", NULL};
static const char *xxx_ann_labels[][3] = {
    {"", "id", "description"},
    /* ... */
};
/* annotation_rows */
/* options (if any) */

/* ===== 辅助函数 ===== */

/* ===== recv_proto ===== */
static void xxx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    /* ... */
}

/* ===== 生命周期回调 ===== */
static void xxx_reset(struct srd_decoder_inst *di) { /* ... */ }
static void xxx_start(struct srd_decoder_inst *di) { /* ... */ }
static void xxx_decode(struct srd_decoder_inst *di) { (void)di; }
static void xxx_destroy(struct srd_decoder_inst *di) { /* ... */ }

/* ===== 解码器结构体 ===== */
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    .longname = "...",
    .desc = "...",
    .license = "...",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = N,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = xxx_tags,
    .num_tags = 1,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,
    .destroy = xxx_destroy,
    .recv_proto = xxx_recv_proto,
};

/* ===== 导出函数 ===== */
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &xxx_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
```

### CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加:
```
rtc8564_c
ssd1306_c
st25dv_c
tcs3472x_c
tpm_tis_i2c_c
```

### 注解简化原则

| Python 特性 | C 处理方式 |
|-------------|-----------|
| `putd(bit1, bit2, ...)` bit-level 注解 | 可省略；如需实现，BITS v2 格式已提供 per-bit 时间戳 <!-- Updated: BITS v2 已实现 --> |
| `putr(bit)` 保留位注解 | 可省略；如需实现，BITS v2 格式已提供 per-bit 时间戳 <!-- Updated: BITS v2 已实现 --> |
| `put0(bit)` / `put1(bit)` 固定位注解 | **省略** |
| 多级注解文本 `['long', 'medium', 'short']` | 保留 1-2 级 |
| `blockstring` 累积文本 | 用 `char[]` + `snprintf` 替代 |
| Python `getattr(self, 'handle_reg_0x%02x' % reg)` | C `switch-case` 或函数指针数组 |
| Python `bcd2int()` | C 自行实现 |
| Python coroutine/generator | C 显式状态机 |

### 测试方法

1. 使用 I2C 捕获文件（.sr 会话文件）加载到 PXView
2. 添加 I2C 解码器 → 添加对应的 C 解码器堆叠
3. 对比 Python 解码器和 C 解码器的注解输出
4. 验证关键事务（读、写、重复启动）的注解正确性
