# SPI上层协议解码器 Python→C 移植规格书 (Batch-27)

## 1. 概述

本规格书描述将5个SPI上层协议Python解码器移植为C解码器的详细技术规格。这5个解码器均以 `inputs=['spi']` 作为输入，通过 `recv_proto()` 回调接收SPI底层解码器输出的 `DATA`/`CS-CHANGE`/`BITS` 等协议数据包，而非直接从 logic 信号采样。

### 移植目标解码器

| # | Python ID | C文件名 | C ID | 协议描述 |
|---|-----------|---------|------|----------|
| 1 | `st25r39xx_spi` | `st25r39xx_spi_c.c` | `st25r39xx_spi_c` | STMicroelectronics ST25R39xx NFC芯片SPI协议 |
| 2 | `sdcard_spi` | `sdcard_spi_c.c` | `sdcard_spi_c` | SD卡SPI模式协议 |
| 3 | `spiflash` | `spiflash_c.c` | `spiflash_c` | xx25系列SPI NOR Flash芯片协议 |
| 4 | `spi_tpm` | `spi_tpm_c.c` | `spi_tpm_c` | TPM SPI事务协议(含BitLocker VMK提取) |
| 5 | `tpm_tis_spi` | `tpm_tis_spi_c.c` | `tpm_tis_spi_c` | TPM TIS 2.0 SPI协议 |

### 关键架构差异

- **底层解码器** (如 `spi_c.c`): `inputs=['logic']`, 使用 `decode()` 函数通过 `c_cond_wait()` 直接采样物理信号
- **上层解码器** (如本批5个): `inputs=['spi']`, 使用 `recv_proto()` 回调接收下层解码器输出的协议数据

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层协议输出范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |
| ds3231_c.c | 上层recv_proto范本 | I2C上层解码器、多寄存器块读写、STOP/START REPEAT处理 |
| c_decoder_utils.h | BITS v2格式文档 | BITS消息格式的权威定义和解析示例代码 |

---

## 2. SPI上层解码器 recv_proto() 机制详解

### 2.1 SPI底层解码器输出的协议包

SPI C解码器 (`spi_c.c`) 通过 `c_decoder_put_python()` 输出以下协议包：

| 协议包类型 | cmd字符串 | data格式 | 说明 |
|-----------|----------|---------|------|
| `DATA` | `"DATA"` | `[flags(1B)][mosi_val(8B)][miso_val(8B)]` | 一个SPI字/字节数据 |
| `BITS` | `"BITS"` | BITS v2格式（含per-bit时间戳），见下方详细布局 | 每个bit的值及ss/es |
| `CS-CHANGE` | `"CS-CHANGE"` | `[prev_cs(1B)][cur_cs(1B)]` 或空 | CS#信号变化 |
| `TRANSFER` | `"TRANSFER"` | 空 | 一次CS# assert到deassert的完整传输 |

**DATA包data字段详细布局** (共17字节):
```
offset 0:   flags (1 byte, bit0=have_mosi, bit1=have_miso)
offset 1-8: mosi_value (8 bytes, uint64_t little-endian)
offset 9-16: miso_value (8 bytes, uint64_t little-endian)
```

**BITS包data字段详细布局** (BITS v2格式，含per-bit时间戳):
```
data[0]                           = have_mosi (bit0) | have_miso (bit1)
data[1]                           = mosi_bit_count (uint8_t)
data[2 .. 2+mosi_count*17-1]      = MOSI bits, 每个17字节:
    [value(1B)][start_sample(8B LE)][end_sample(8B LE)]
data[2+mosi_count*17]             = 0x00 (reserved/alignment)
data[2+mosi_count*17+1]           = miso_bit_count (uint8_t)
data[2+mosi_count*17+2 ..]        = MISO bits, 每个17字节:
    [value(1B)][start_sample(8B LE)][end_sample(8B LE)]
```
<!-- Updated: BITS格式已从旧版(仅bit值)更新为v2(含per-bit ss/es时间戳)，与spi_c.c和i2c_c.c实际输出一致，详见c_decoder_utils.h -->

**CS-CHANGE包data字段**:
- 首次(无CS引脚时): data=NULL, data_len=0
- 首次(有CS引脚，decode()起始时): data=[0xFF, cs_value], data_len=2 (0xFF为初始哨兵值)
<!-- Updated: 补充了有CS引脚时的初始CS-CHANGE包格式，与spi_c.c第429行一致 -->
- 后续: data=[prev_cs, cur_cs], data_len=2
  - `prev_cs=0, cur_cs=1` → CS#上升沿(释放)
  - `prev_cs=1, cur_cs=0` → CS#下降沿(选中)
  - 注意: prev_cs实际存储为`1-cur_cs`(对二进制信号等于前一值)，初始为0xFF

### 2.2 recv_proto() 回调函数签名

```c
void (*recv_proto)(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

参数说明:
- `start_sample` / `end_sample`: 该协议包对应的采样点范围
- `cmd`: 协议包类型字符串，如 `"DATA"`, `"CS-CHANGE"`, `"BITS"`, `"TRANSFER"`
- `data` / `data_len`: 协议包携带的二进制数据

### 2.3 解码DATA包的辅助函数

```c
// 从DATA包提取mosi/miso值
static inline int spi_data_get_mosi(const unsigned char *data, uint64_t data_len, uint64_t *mosi_val) {
    if (data_len < 17) return -1;
    int have_mosi = data[0] & 1;
    if (!have_mosi) { *mosi_val = 0; return 0; } // no MOSI
    *mosi_val = 0;
    for (int i = 0; i < 8; i++)
        *mosi_val |= ((uint64_t)data[1 + i]) << (8 * i);
    return 1;
}

static inline int spi_data_get_miso(const unsigned char *data, uint64_t data_len, uint64_t *miso_val) {
    if (data_len < 17) return -1;
    int have_miso = (data[0] >> 1) & 1;
    if (!have_miso) { *miso_val = 0; return 0; } // no MISO
    *miso_val = 0;
    for (int i = 0; i < 8; i++)
        *miso_val |= ((uint64_t)data[9 + i]) << (8 * i);
    return 1;
}
```

### 2.4 decode() 函数

上层解码器的 `decode()` 函数为空函数体（不需要直接采样），仅 `recv_proto()` 处理数据：

```c
static void xxx_decode(struct srd_decoder_inst *di) {
    (void)di;
    // 上层解码器不需要decode(), 所有逻辑在recv_proto()中
}
```

---

## 3. 各解码器详细规格

---

### 3.1 st25r39xx_spi_c — ST25R39xx NFC芯片SPI协议

#### 3.1.1 Python元数据映射

| 属性 | Python值 | C映射 |
|------|---------|-------|
| id | `st25r39xx_spi` | `st25r39xx_spi_c` |
| name | `ST25R39xx (SPI mode)` | `ST25R39xx(C)` |
| longname | `STMicroelectronics ST25R39xx` | `STMicroelectronics ST25R39xx (C)` |
| desc | (见__init__.py) | `ST25R39xx NFC chip SPI protocol decoder (C implementation)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['spi']` | `{"spi", NULL}` |
| outputs | `[]` | `NULL` / `num_outputs=0` |
| tags | `['IC', 'Wireless/RF']` | `{"IC", "Wireless/RF", NULL}` |
| channels | 无 | `NULL, num_channels=0` |
| optional_channels | 无 | `NULL, num_optional_channels=0` |
| options | 无 | `NULL, num_options=0` |
| binary | 无 | `NULL, num_binary=0` |

#### 3.1.2 Annotations定义

```c
enum {
    ANN_BURST_READ = 0,     // Burst register read
    ANN_BURST_WRITE,        // Burst register write
    ANN_BURST_READB,        // Burst register SpaceB read
    ANN_BURST_WRITEB,       // Burst register SpaceB write
    ANN_BURST_READT,        // Burst register Test read
    ANN_BURST_WRITET,       // Burst register Test write
    ANN_DIRECTCMD,          // Direct command
    ANN_FIFO_WRITE,         // FIFO write
    ANN_FIFO_READ,          // FIFO read
    ANN_STATUS,             // Status register
    ANN_WARN,               // Warning
    NUM_ANN,
};

static const char *st25r39xx_spi_ann_labels[][3] = {
    {"", "Read", "Burst register read"},
    {"", "Write", "Burst register write"},
    {"", "ReadB", "Burst register SpaceB read"},
    {"", "WriteB", "Burst register SpaceB write"},
    {"", "ReadT", "Burst register Test read"},
    {"", "WriteT", "Burst register Test write"},
    {"", "Cmd", "Direct command"},
    {"", "FIFOW", "FIFO write"},
    {"", "FIFOR", "FIFO read"},
    {"", "status_reg", "Status register"},
    {"", "warning", "Warning"},
};
```

#### 3.1.3 Annotation Rows

```c
static const int st25r39xx_row_regs_classes[] = {
    ANN_BURST_READ, ANN_BURST_WRITE, ANN_BURST_READB, ANN_BURST_WRITEB,
    ANN_BURST_READT, ANN_BURST_WRITET, -1
};
static const int st25r39xx_row_cmds_classes[] = {ANN_DIRECTCMD, -1};
static const int st25r39xx_row_data_classes[] = {ANN_FIFO_WRITE, ANN_FIFO_READ, -1};
static const int st25r39xx_row_status_classes[] = {ANN_STATUS, -1};
static const int st25r39xx_row_warnings_classes[] = {ANN_WARN, -1};

static const struct srd_c_ann_row st25r39xx_spi_ann_rows[] = {
    {"regs", "Regs", st25r39xx_row_regs_classes, 6},
    {"cmds", "Commands", st25r39xx_row_cmds_classes, 1},
    {"data", "Data", st25r39xx_row_data_classes, 2},
    {"status", "Status register", st25r39xx_row_status_classes, 1},
    {"warnings", "Warnings", st25r39xx_row_warnings_classes, 1},
};
```

#### 3.1.4 状态机与解码逻辑

**状态变量**:
```c
typedef struct {
    int first;          // 是否为CS# assert后的第一个字节
    int cmd_type;       // 当前命令类型枚举
    int cmd_dat;        // 命令数据(地址或特殊值)
    int cmd_min;        // 最少后续字节数
    int cmd_max;        // 最多后续字节数(99999=无限)
    uint8_t mb_mosi[1024]; // 收集的MOSI数据字节
    uint8_t mb_miso[1024]; // 收集的MISO数据字节
    int mb_count;       // 收集的字节数
    uint64_t ss_mb;     // 多字节命令起始采样
    uint64_t es_mb;     // 多字节命令结束采样
    int cs_was_released; // CS#是否已释放过
    int requirements_met; // MISO/MOSI是否都可用
    int out_ann;
} st25r39xx_state;
```

**命令类型枚举**:
```c
enum st25r39xx_cmd {
    CMD_NONE = 0,
    CMD_WRITE,       // Space A Write
    CMD_READ,        // Space A Read
    CMD_WRITEB,      // Space B Write
    CMD_READB,       // Space B Read
    CMD_WRITET,      // Test Write
    CMD_READT,       // Test Read
    CMD_FIFO_WRITE,  // FIFO Write (0x80)
    CMD_FIFO_READ,   // FIFO Read (0x9F)
    CMD_DIRECT,      // Direct Command
    CMD_SPACE_B,     // Register Space-B Access (0xFB)
    CMD_TEST_ACCESS, // Register Test Access (0xFC)
};
```

**recv_proto()核心逻辑**:
1. 收到 `CS-CHANGE`: 检查CS#上升沿(释放)，若有未完成命令则 `finish_command()`
2. 收到 `DATA`:
   - 若 `first==true`: 解析命令字节 `mosi`，调用 `parse_command()`
     - `0x00-0x3F`: Space A Write (addr=mosi&0x3F)
     - `0x40-0x7F`: Space A Read (addr=mosi&0x3F)
     - `0x80`: FIFO Write
     - `0x9F`: FIFO Read
     - `0xA0/0xA8/0xAC`: Write (特殊地址)
     - `0xBF`: Read (特殊地址)
     - `0xC0-0xE8`: Direct Command
     - `0xFB`: Space B (保持first=true，下个字节再解析)
     - `0xFC`: TestAccess (保持first=true，下个字节再解析)
   - 若 `first==false`: 收集数据字节到 `mb_mosi[]/mb_miso[]`

**寄存器名称查找表** (硬编码在C文件中):
- `regsSpaceA[]`: 64个条目 (0x00-0x3F + 特殊地址)
- `regsSpaceB[]`: 14个条目
- `regsTest[]`: 1个条目
- `dir_cmd[]`: 35个条目

**关键C代码片段 — parse_command()**:
```c
static int st25r39xx_parse_command(st25r39xx_state *s, struct srd_decoder_inst *di,
                                   uint64_t ss, uint64_t es, uint8_t mosi)
{
    uint8_t addr = mosi & 0x3F;

    if (s->cmd_type == CMD_SPACE_B) {
        if ((mosi & 0xC0) == 0x00) { s->cmd_type = CMD_WRITEB; s->cmd_dat = addr; s->cmd_min = 1; s->cmd_max = 99999; return 0; }
        if ((mosi & 0xC0) == 0x40) { s->cmd_type = CMD_READB; s->cmd_dat = addr; s->cmd_min = 1; s->cmd_max = 99999; return 0; }
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARN, "Unknown address/command combination");
        return -1;
    }
    if (s->cmd_type == CMD_TEST_ACCESS) {
        if ((mosi & 0xC0) == 0x00) { s->cmd_type = CMD_WRITET; s->cmd_dat = addr; s->cmd_min = 1; s->cmd_max = 99999; return 0; }
        if ((mosi & 0xC0) == 0x40) { s->cmd_type = CMD_READT; s->cmd_dat = addr; s->cmd_min = 1; s->cmd_max = 99999; return 0; }
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARN, "Unknown address/command combination");
        return -1;
    }

    if (mosi <= 0x7F) {
        if ((mosi & 0xC0) == 0x00) { s->cmd_type = CMD_WRITE; s->cmd_dat = addr; s->cmd_min = 1; s->cmd_max = 99999; return 0; }
        if ((mosi & 0xC0) == 0x40) { s->cmd_type = CMD_READ; s->cmd_dat = addr; s->cmd_min = 1; s->cmd_max = 99999; return 0; }
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARN, "Unknown address/command combination");
        return -1;
    }

    if (mosi == 0x80) { s->cmd_type = CMD_FIFO_WRITE; s->cmd_dat = mosi; s->cmd_min = 1; s->cmd_max = 99999; return 0; }
    if (mosi == 0xA0 || mosi == 0xA8 || mosi == 0xAC) { s->cmd_type = CMD_WRITE; s->cmd_dat = mosi; s->cmd_min = 1; s->cmd_max = 99999; return 0; }
    if (mosi == 0xBF) { s->cmd_type = CMD_READ; s->cmd_dat = mosi; s->cmd_min = 1; s->cmd_max = 99999; return 0; }
    if (mosi == 0x9F) { s->cmd_type = CMD_FIFO_READ; s->cmd_dat = mosi; s->cmd_min = 1; s->cmd_max = 99999; return 0; }
    if (mosi >= 0xC0 && mosi <= 0xE8) { s->cmd_type = CMD_DIRECT; s->cmd_dat = mosi; s->cmd_min = 0; s->cmd_max = 0; return 0; }
    if (mosi == 0xFB) { s->cmd_type = CMD_SPACE_B; s->cmd_dat = mosi; s->cmd_min = 0; s->cmd_max = 0; return 0; }
    if (mosi == 0xFC) { s->cmd_type = CMD_TEST_ACCESS; s->cmd_dat = mosi; s->cmd_min = 0; s->cmd_max = 0; return 0; }

    C_ANN_PUT(di, ss, es, s->out_ann, ANN_WARN, "Unknown address/command combination");
    return -1;
}
```

---

### 3.2 sdcard_spi_c — SD卡SPI模式协议

#### 3.2.1 Python元数据映射

| 属性 | Python值 | C映射 |
|------|---------|-------|
| id | `sdcard_spi` | `sdcard_spi_c` |
| name | `SD card (SPI mode)` | `SD Card SPI(C)` |
| longname | `Secure Digital card (SPI mode)` | `Secure Digital card SPI mode (C)` |
| desc | (见__init__.py) | `SD card SPI mode low-level protocol decoder (C implementation)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['spi']` | `{"spi", NULL}` |
| outputs | `[]` | `NULL, num_outputs=0` |
| tags | `['Memory']` | `{"Memory", NULL}` |
| channels | 无 | `NULL, num_channels=0` |
| optional_channels | 无 | `NULL, num_optional_channels=0` |
| options | 无 | `NULL, num_options=0` |
| binary | 无 | `NULL, num_binary=0` |

#### 3.2.2 Annotations定义

Python原始定义有64个CMD + 64个ACMD + 5个R响应 + 2个通用 = 135个annotation。C实现需要全部映射：

```c
enum {
    ANN_CMD0 = 0, ANN_CMD1, ANN_CMD2, ..., ANN_CMD63,     // 0-63
    ANN_ACMD0, ANN_ACMD1, ..., ANN_ACMD63,                 // 64-127
    ANN_R1, ANN_R1B, ANN_R2, ANN_R3, ANN_R7,              // 128-132
    ANN_BIT,                                                 // 133
    ANN_BIT_WARNING,                                         // 134
    NUM_ANN = 135,
};
```

**简化方案**: 由于135个annotation label数量巨大，建议使用宏生成：

```c
// 使用宏批量生成ann_labels
#define CMD_ANN(i) {"", "cmd" #i, "CMD" #i}
#define ACMD_ANN(i) {"", "acmd" #i, "ACMD" #i}

static const char *sdcard_spi_ann_labels[][3] = {
    CMD_ANN(0), CMD_ANN(1), ..., CMD_ANN(63),    // 0-63
    ACMD_ANN(0), ACMD_ANN(1), ..., ACMD_ANN(63),  // 64-127
    {"", "r1", "R1 response"},
    {"", "r1b", "R1b response"},
    {"", "r2", "R2 response"},
    {"", "r3", "R3 response"},
    {"", "r7", "R7 response"},
    {"", "bit", "Bit"},
    {"", "bit-warning", "Bit warning"},
};
```

#### 3.2.3 Annotation Rows

```c
static const int sdcard_row_bits_classes[] = {ANN_BIT, ANN_BIT_WARNING, -1};
// commands-replies行包含所有CMD/ACMD/R* annotation
static const int sdcard_row_cmds_classes[] = {
    ANN_CMD0, ANN_CMD1, ..., ANN_CMD63,
    ANN_ACMD0, ..., ANN_ACMD63,
    ANN_R1, ANN_R1B, ANN_R2, ANN_R3, ANN_R7, -1
};

static const struct srd_c_ann_row sdcard_spi_ann_rows[] = {
    {"bits", "Bits", sdcard_row_bits_classes, 2},
    {"commands-replies", "Commands/replies", sdcard_row_cmds_classes, 133},
};
```

#### 3.2.4 状态机

```c
enum sdcard_state {
    SDCARD_IDLE,
    SDCARD_GET_CMD_TOKEN,
    SDCARD_HANDLE_CMD0,
    SDCARD_HANDLE_CMD1,
    SDCARD_HANDLE_CMD9,
    SDCARD_HANDLE_CMD10,
    SDCARD_HANDLE_CMD16,
    SDCARD_HANDLE_CMD17,
    SDCARD_HANDLE_CMD24,
    SDCARD_HANDLE_CMD49,
    SDCARD_HANDLE_CMD55,
    SDCARD_HANDLE_CMD59,
    SDCARD_HANDLE_CMD999,
    SDCARD_GET_RESPONSE_R1,
    SDCARD_GET_RESPONSE_R1B,
    SDCARD_GET_RESPONSE_R2,
    SDCARD_GET_RESPONSE_R3,
    SDCARD_GET_RESPONSE_R7,
    SDCARD_HANDLE_DATA_CMD17,
    SDCARD_HANDLE_DATA_CMD24,
    SDCARD_DATA_RESPONSE,
    SDCARD_WAIT_BUSY,
};
```

**核心状态变量**:
```c
typedef struct {
    enum sdcard_state state;
    uint64_t ss, es;
    uint64_t ss_bit, es_bit;
    uint64_t ss_cmd, es_cmd;
    uint64_t ss_busy, es_busy;
    uint8_t cmd_token[6];       // 6字节命令token
    int cmd_token_count;
    int is_acmd;
    uint32_t blocklen;
    uint8_t read_buf[520];      // 数据读取缓冲区(最大512+额外)
    int read_buf_count;
    char cmd_str[64];
    int is_cmd24;
    int cmd24_start_token_found;
    int is_cmd17;
    int cmd17_start_token_found;
    int busy_first_byte;
    uint32_t arg;               // 命令参数
    int cmd_index;              // 命令索引
    int out_ann;
} sdcard_state;
```

**recv_proto()逻辑**:
1. `CS-CHANGE`: 重置状态到IDLE
2. `DATA`: 根据当前状态处理mosi/miso字节
   - IDLE: 忽略0xFF，进入GET_CMD_TOKEN
   - GET_CMD_TOKEN: 收集6字节命令token
   - HANDLE_CMD*: 调用对应命令处理函数
   - GET_RESPONSE_R*: 处理响应
   - HANDLE_DATA_CMD17/24: 处理数据块
   - DATA_RESPONSE: 处理数据响应token
   - WAIT_BUSY: 等待卡忙结束

**注意**: Python版本使用了BITS包来获取每个bit的ss/es，C版本可以简化为仅使用DATA包的ss/es，bit级标注可省略或简化。若需使用BITS包，须按BITS v2格式解析（含per-bit ss/es时间戳，每bit 17字节）。
<!-- Updated: 补充BITS v2格式说明，原描述未提及v2格式的per-bit时间戳 -->

---

### 3.3 spiflash_c — SPI NOR Flash芯片协议

#### 3.3.1 Python元数据映射

| 属性 | Python值 | C映射 |
|------|---------|-------|
| id | `spiflash` | `spiflash_c` |
| name | `SPI flash` | `SPI Flash(C)` |
| longname | `SPI flash chips` | `xx25 series SPI NOR flash chips (C)` |
| desc | (见__init__.py) | `xx25 series SPI NOR flash chip protocol decoder (C implementation)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['spi']` | `{"spi", NULL}` |
| outputs | `[]` | `NULL, num_outputs=0` |
| tags | `['IC', 'Memory']` | `{"IC", "Memory", NULL}` |
| channels | 无 | `NULL, num_channels=0` |
| optional_channels | 无 | `NULL, num_optional_channels=0` |
| binary | 无 | `NULL, num_binary=0` |

#### 3.3.2 Options

```c
static struct srd_decoder_option spiflash_options[] = {
    {"chip", "dec_spiflash_opt_chip", "Chip", NULL, NULL},
    {"format", "dec_spiflash_opt_format", "Data format", NULL, NULL},
};
```

在 `srd_c_decoder_entry()` 中初始化:
```c
// chip选项: 默认"macronix_mx25l1605d"
spiflash_options[0].def = g_variant_new_string("macronix_mx25l1605d");
GSList *chip_vals = NULL;
chip_vals = g_slist_append(chip_vals, g_variant_new_string("adesto_at45db161e"));
chip_vals = g_slist_append(chip_vals, g_variant_new_string("fidelix_fm25q32"));
chip_vals = g_slist_append(chip_vals, g_variant_new_string("macronix_mx25l1605d"));
chip_vals = g_slist_append(chip_vals, g_variant_new_string("macronix_mx25l3205d"));
chip_vals = g_slist_append(chip_vals, g_variant_new_string("macronix_mx25l6405d"));
chip_vals = g_slist_append(chip_vals, g_variant_new_string("winbond_w25q80dv"));
spiflash_options[0].values = chip_vals;

// format选项: 默认"hex"
spiflash_options[1].def = g_variant_new_string("hex");
GSList *fmt_vals = NULL;
fmt_vals = g_slist_append(fmt_vals, g_variant_new_string("hex"));
fmt_vals = g_slist_append(fmt_vals, g_variant_new_string("ascii"));
spiflash_options[1].values = fmt_vals;
```

#### 3.3.3 Annotations定义

Python版本从 `lists.py` 的 `cmds` OrderedDict 动态生成28个命令annotation + 3个通用 = 31个：

```c
enum {
    ANN_WRSR = 0,   // 0x01
    ANN_PP,          // 0x02
    ANN_READ,        // 0x03
    ANN_WRDI,        // 0x04
    ANN_RDSR,        // 0x05
    ANN_WREN,        // 0x06
    ANN_FAST_READ,   // 0x0b
    ANN_SE,          // 0x20
    ANN_RDSCUR,      // 0x2b
    ANN_WRSCUR,      // 0x2f
    ANN_RDSR2,       // 0x35
    ANN_CE,          // 0x60
    ANN_ESRY,        // 0x70
    ANN_DSRY,        // 0x80
    ANN_WRITE1,      // 0x82
    ANN_WRITE2,      // 0x85
    ANN_REMS,        // 0x90
    ANN_RDID,        // 0x9f
    ANN_RDP_RES,     // 0xab
    ANN_CP,          // 0xad
    ANN_ENSO,        // 0xb1
    ANN_DP,          // 0xb9
    ANN_READ2X,      // 0xbb
    ANN_EXSO,        // 0xc1
    ANN_CE2,         // 0xc7
    ANN_STATUS,      // 0xd7
    ANN_BE,          // 0xd8
    ANN_REMS2,       // 0xef
    ANN_BIT,         // 28
    ANN_FIELD,       // 29
    ANN_WARN,        // 30
    NUM_ANN = 31,
};
```

#### 3.3.4 Annotation Rows

```c
static const int spiflash_row_bits_classes[] = {ANN_BIT, -1};
static const int spiflash_row_fields_classes[] = {ANN_FIELD, -1};
static const int spiflash_row_commands_classes[] = {
    ANN_WRSR, ANN_PP, ANN_READ, ANN_WRDI, ANN_RDSR, ANN_WREN, ANN_FAST_READ,
    ANN_SE, ANN_RDSCUR, ANN_WRSCUR, ANN_RDSR2, ANN_CE, ANN_ESRY, ANN_DSRY,
    ANN_WRITE1, ANN_WRITE2, ANN_REMS, ANN_RDID, ANN_RDP_RES, ANN_CP,
    ANN_ENSO, ANN_DP, ANN_READ2X, ANN_EXSO, ANN_CE2, ANN_STATUS, ANN_BE, ANN_REMS2, -1
};
static const int spiflash_row_warnings_classes[] = {ANN_WARN, -1};

static const struct srd_c_ann_row spiflash_ann_rows[] = {
    {"bits", "Bits", spiflash_row_bits_classes, 1},
    {"fields", "Fields", spiflash_row_fields_classes, 1},
    {"commands", "Commands", spiflash_row_commands_classes, 28},
    {"warnings", "Warnings", spiflash_row_warnings_classes, 1},
};
```

#### 3.3.5 状态机

```c
typedef struct {
    int state;           // 当前命令ID (0x01-0xef) 或 0=NULL
    int cmdstate;        // 命令内字节计数器
    uint32_t addr;       // 当前地址
    uint8_t data_buf[4096]; // 数据缓冲区
    int data_count;      // 数据字节数
    int writestate;      // WREN状态
    int device_id;       // 设备ID
    uint64_t ss_cmd, es_cmd;
    uint64_t ss_field, es_field;
    uint64_t ss, es;
    int chip_index;      // 当前选择的芯片索引
    int format;          // 数据格式 0=hex, 1=ascii
    int out_ann;
} spiflash_state;
```

**recv_proto()逻辑**:
1. `CS-CHANGE`: 调用 `end_current_transaction()` (处理延迟输出)
2. `DATA`:
   - 若 `state==0(NULL)`: 第一个MOSI字节作为命令ID，查找命令表
   - 否则: 根据当前命令和 `cmdstate` 调用对应handler

**命令handler映射表** (使用函数指针数组):
```c
typedef void (*spiflash_cmd_handler)(struct srd_decoder_inst *di, spiflash_state *s,
                                     uint8_t mosi, uint8_t miso);

// 建立命令字节到handler的映射
static const struct { uint8_t cmd; spiflash_cmd_handler handler; } spiflash_cmd_table[] = {
    {0x01, spiflash_handle_wrsr},
    {0x02, spiflash_handle_pp},
    {0x03, spiflash_handle_read},
    // ... 所有28个命令
};
```

**关键handler示例 — handle_read()**:
```c
static void spiflash_handle_read(struct srd_decoder_inst *di, spiflash_state *s,
                                  uint8_t mosi, uint8_t miso)
{
    if (s->cmdstate == 1) {
        // Byte 1: Command ID
        spiflash_emit_cmd_byte(di, s);
    } else if (s->cmdstate >= 2 && s->cmdstate <= 4) {
        // Bytes 2/3/4: Address (24-bit MSB-first)
        s->addr |= (mosi << ((4 - s->cmdstate) * 8));
        char buf[64];
        int b = ((3 - (s->cmdstate - 2)) * 8) - 1;
        snprintf(buf, sizeof(buf), "Addr bits %d..%d", b, b-7);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_BIT, buf);
        if (s->cmdstate == 2) s->ss_field = s->ss;
        if (s->cmdstate == 4) {
            s->es_field = s->es;
            char addr_str[16];
            snprintf(addr_str, sizeof(addr_str), "@%06x", s->addr);
            C_ANN_PUT(di, s->ss_field, s->es_field, s->out_ann, ANN_FIELD, addr_str);
        }
    } else if (s->cmdstate >= 5) {
        // Bytes 5+: Data (until CS# deasserted)
        s->es_field = s->es;
        if (s->cmdstate == 5) s->ss_field = s->ss;
        if (s->data_count < (int)sizeof(s->data_buf))
            s->data_buf[s->data_count++] = miso;
    }
    s->cmdstate++;
}
```

**芯片信息查找表** (从lists.py移植):
```c
typedef struct {
    const char *key;
    const char *vendor;
    const char *model;
    uint32_t rdid_id;
    uint16_t rems_id;
    int page_size;
    int sector_size;
    int block_size;
} spiflash_chip_info;

static const spiflash_chip_info spiflash_chips[] = {
    {"adesto_at45db161e", "Adesto", "AT45DB161E", ...},
    {"fidelix_fm25q32", "FIDELIX", "FM25Q32", ...},
    {"macronix_mx25l1605d", "Macronix", "MX25L1605D", ...},
    // ...
};
```

---

### 3.4 spi_tpm_c — TPM SPI事务协议

#### 3.4.1 Python元数据映射

| 属性 | Python值 | C映射 |
|------|---------|-------|
| id | `spi_tpm` | `spi_tpm_c` |
| name | `SPI TPM` | `SPI TPM(C)` |
| longname | `SPI TPM transactions` | `SPI TPM transactions (C)` |
| desc | (见__init__.py) | `TPM SPI transaction decoder with VMK extraction (C implementation)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['spi']` | `{"spi", NULL}` |
| outputs | `[]` | `NULL, num_outputs=0` |
| tags | `['IC', 'TPM', 'BitLocker']` | `{"IC", "TPM", "BitLocker", NULL}` |
| channels | 无 | `NULL, num_channels=0` |
| optional_channels | 无 | `NULL, num_optional_channels=0` |
| binary | 无 | `NULL, num_binary=0` |

#### 3.4.2 Options

```c
static struct srd_decoder_option spi_tpm_options[] = {
    {"tpm_version", "dec_spi_tpm_opt_tpm_version", "TPM Version 1.2 or 2.0", NULL, NULL},
};
```

在 `srd_c_decoder_entry()` 中:
```c
spi_tpm_options[0].def = g_variant_new_string("2.0");
GSList *ver_vals = NULL;
ver_vals = g_slist_append(ver_vals, g_variant_new_string("2.0"));
ver_vals = g_slist_append(ver_vals, g_variant_new_string("1.2"));
spi_tpm_options[0].values = ver_vals;
```

#### 3.4.3 Annotations定义

```c
enum {
    ANN_READ = 0,    // Read register operation
    ANN_WRITE,       // Write register operation
    ANN_ADDRESS,     // Register address
    ANN_WAIT,        // Wait
    ANN_DATA,        // Data
    ANN_VMK,         // Extracted BitLocker VMK
    NUM_ANN = 6,
};

static const char *spi_tpm_ann_labels[][3] = {
    {"", "Read", "Read register operation"},
    {"", "Write", "Write register operation"},
    {"", "Address", "Register address"},
    {"", "Wait", "Wait"},
    {"", "Data", "Data"},
    {"", "VMK", "Extracted BitLocker VMK"},
};
```

#### 3.4.4 Annotation Rows

```c
static const int spi_tpm_row_transactions_classes[] = {ANN_READ, ANN_WRITE, ANN_ADDRESS, ANN_WAIT, ANN_DATA, -1};
static const int spi_tpm_row_vmk_classes[] = {ANN_VMK, -1};

static const struct srd_c_ann_row spi_tpm_ann_rows[] = {
    {"Transactions", "TPM transactions", spi_tpm_row_transactions_classes, 5},
    {"B-VMK", "BitLocker Volume Master Key", spi_tpm_row_vmk_classes, 1},
};
```

#### 3.4.5 状态机

```c
enum tpm_transaction_state {
    TPM_TS_NONE = 0,
    TPM_TS_READ,
    TPM_TS_WRITE,
    TPM_TS_READ_ADDRESS,
    TPM_TS_WAIT,
    TPM_TS_TRANSFER_DATA,
};

typedef struct {
    enum tpm_transaction_state state;
    // 当前事务
    int operation;          // 0=WRITE, 0x80=READ
    int transfer_size;      // 数据字节数
    uint8_t address[3];     // 3字节寄存器地址
    int addr_count;
    uint8_t data[256];      // 数据缓冲区
    int data_count;
    int wait_count;
    uint64_t ss_op, es_op;
    uint64_t ss_addr, es_addr;
    uint64_t ss_data, es_data;
    uint64_t ss_wait, es_wait;
    uint64_t ss, es;
    // VMK提取
    uint8_t vmk_queue[12];  // 环形缓冲区
    int vmk_queue_count;
    uint64_t vmk_queue_ss[12];
    int saving_vmk;
    uint8_t vmk[32];
    int vmk_count;
    uint64_t vmk_ss, vmk_es;
    // 配置
    int tpm_version;        // 0=2.0, 1=1.2
    uint8_t end_wait;       // 0x01
    uint8_t wait_mask;      // 0x00 (2.0) or 0xFE (1.2)
    int out_ann;
} spi_tpm_state;
```

**recv_proto()逻辑**:
1. `CS-CHANGE`: 调用 `end_current_transaction()` 重置
2. `DATA`:
   - 若 `state==NONE`: 解析第一个MOSI字节
     - bit7=1: READ事务, `transfer_size = (mosi & 0x3F) + 1`
     - bit7=0: WRITE事务, `transfer_size = (mosi & 0x3F) + 1`
   - `READ_ADDRESS`: 收集3字节地址
   - `WAIT`: 等待MISO返回 `end_wait` 值
   - `TRANSFER_DATA`: 收集数据字节，完成后输出annotation

**VMK提取逻辑**:
- 仅在READ事务且地址匹配 `TPM_DATA_FIFO_0` 时收集MISO数据
- 维护12字节环形缓冲区检测VMK header (`2c000[0-6]000[1-9]000[0-1]000[0-5]200000`)
- 检测到header后收集后续32字节作为VMK

**FIFO寄存器查找**: 需要在C中实现简化的RangeDict，用线性搜索+范围匹配：

```c
typedef struct {
    uint16_t start;
    uint16_t end;
    const char *name;
} tpm_fifo_reg_range;

static const tpm_fifo_reg_range tpm2_fifo_regs[] = {
    {0x0000, 0x0000, "TPM_ACCESS_0"},
    {0x0008, 0x000B, "TPM_INT_ENABLE_0"},
    // ... (从lists.py移植所有条目)
    {0xFFFF, 0xFFFF, NULL} // 终止标记
};

static const char *spi_tpm_find_register(uint16_t addr, int tpm_version) {
    const tpm_fifo_reg_range *regs = (tpm_version == 0) ? tpm2_fifo_regs : tpm1_fifo_regs;
    for (int i = 0; regs[i].name != NULL; i++) {
        if (addr >= regs[i].start && addr <= regs[i].end)
            return regs[i].name;
    }
    return "Unknown";
}
```

---

### 3.5 tpm_tis_spi_c — TPM TIS 2.0 SPI协议

#### 3.5.1 Python元数据映射

| 属性 | Python值 | C映射 |
|------|---------|-------|
| id | `tpm_tis_spi` | `tpm_tis_spi_c` |
| name | `TPM TIS 2.0 SPI` | `TPM TIS 2.0 SPI(C)` |
| longname | `Trusted Platform Module Interface (TIS 2.0) over Serial Peripheral Bus` | `Trusted Platform Module Interface (TIS 2.0) over SPI (C)` |
| desc | (见__init__.py) | `TPM TIS 2.0 over SPI protocol decoder (C implementation)` |
| license | `gplv3+` | `gplv3+` |
| inputs | `['spi']` | `{"spi", NULL}` |
| outputs | `['tpm-tis']` | `{"tpm-tis", NULL}`, `num_outputs=1` |
| tags | `['TPM']` | `{"TPM", NULL}` |
| channels | 无 | `NULL, num_channels=0` |
| optional_channels | 无 | `NULL, num_optional_channels=0` |
| options | 无 | `NULL, num_options=0` |
| binary | 无 | `NULL, num_binary=0` |

#### 3.5.2 Annotations定义

```c
enum {
    ANN_RW_LENGTH = 0,  // RW/Length
    ANN_ADDRESS,        // Address
    ANN_WAIT_STATE,     // Wait State
    ANN_DATA,           // Data
    ANN_TRANSACTION,    // Transaction
    ANN_WARNING,        // Warning
    NUM_ANN = 6,
};

static const char *tpm_tis_spi_ann_labels[][3] = {
    {"", "rw-length", "RW/Length"},
    {"", "address", "Address"},
    {"", "wait-state", "Wait State"},
    {"", "data", "Data"},
    {"", "transaction", "Transaction"},
    {"", "warning", "Warning"},
};
```

#### 3.5.3 Annotation Rows

```c
static const int tpm_tis_row_protocol_classes[] = {ANN_RW_LENGTH, ANN_ADDRESS, ANN_WAIT_STATE, ANN_DATA, -1};
static const int tpm_tis_row_transactions_classes[] = {ANN_TRANSACTION, -1};
static const int tpm_tis_row_warnings_classes[] = {ANN_WARNING, -1};

static const struct srd_c_ann_row tpm_tis_spi_ann_rows[] = {
    {"protocol", "Protocol", tpm_tis_row_protocol_classes, 4},
    {"transactions", "Transactions", tpm_tis_row_transactions_classes, 1},
    {"warnings", "Warnings", tpm_tis_row_warnings_classes, 1},
};
```

#### 3.5.4 状态机

Python版本使用协程(coroutine)实现，C版本改为显式状态机：

```c
enum tpm_tis_state {
    TIS_GET_RW_LENGTH = 0,
    TIS_GET_ADDR_BYTE2,
    TIS_GET_ADDR_BYTE1,
    TIS_GET_ADDR_BYTE0,
    TIS_GET_DATA,       // 循环状态，需计数
};

typedef struct {
    enum tpm_tis_state state;
    int reading;            // 1=read, 0=write
    int length;             // 数据字节数
    uint32_t addr;          // 24位地址
    uint8_t addr_bytes[3];  // 地址字节缓冲
    int addr_idx;
    uint8_t data[256];      // 数据缓冲
    int data_count;
    int wait_state;         // 是否有wait state
    // 采样点记录
    uint64_t rwl_ss, rwl_es;
    uint64_t addr2_ss, addr2_es;
    uint64_t addr1_ss, addr1_es;
    uint64_t addr0_ss, addr0_es;
    uint64_t data_ss, data_es;
    int out_ann;
    int out_python;
} tpm_tis_state;
```

**recv_proto()逻辑** (逐字节状态机):

1. `TIS_GET_RW_LENGTH`:
   - `reading = (mosi & 0x80) == 0x80`
   - `length = (mosi & 0x7F) + 1`
   - 输出RW/Length annotation
   - 转到 `TIS_GET_ADDR_BYTE2`

2. `TIS_GET_ADDR_BYTE2`:
   - `addr_bytes[2] = mosi`
   - 转到 `TIS_GET_ADDR_BYTE1`

3. `TIS_GET_ADDR_BYTE1`:
   - `addr_bytes[1] = mosi`
   - 转到 `TIS_GET_ADDR_BYTE0`

4. `TIS_GET_ADDR_BYTE0`:
   - `addr_bytes[0] = mosi`
   - `wait_state = (miso == 0)`
   - `addr = addr_bytes[2]<<16 | addr_bytes[1]<<8 | addr_bytes[0]`
   - 输出Address annotation
   - 转到 `TIS_GET_DATA`

5. `TIS_GET_DATA` (循环):
   - 收集 `length` 个字节
   - 完成后: 输出Wait State/Data/Transaction annotations
   - 重置状态到 `TIS_GET_RW_LENGTH`

**duplex warning**: Python版本检查非传输方向的字节是否为0，C版本同样实现：
- Read时检查mosi是否为0
- Write时检查miso是否为0

**Python输出**: 此解码器有 `outputs=['tpm-tis']`，需要注册python输出：
```c
s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "tpm-tis");
```

**注意**: `c_decoder_put_python()` 仅向C解码器实例的 `recv_proto()` 传递数据，不会传递给Python上层解码器。因此 `tpm-tis` 输出目前只能被C解码器消费。Python解码器 `tpm_fifo_tis`（`inputs=['tpm-tis']`）无法接收此输出，除非将来实现 `tpm_fifo_tis_c`。
<!-- Updated: 补充c_decoder_put_python的C→Python传递限制说明 -->

Transaction完成时输出python数据：
```c
// 输出TRANSACTION python包
unsigned char py_data[270]; // reading(1) + addr(4) + data_len(1) + data(256)
py_data[0] = (unsigned char)s->reading;
py_data[1] = (unsigned char)(s->addr >> 16);
py_data[2] = (unsigned char)(s->addr >> 8);
py_data[3] = (unsigned char)(s->addr);
py_data[4] = (unsigned char)s->data_count;
memcpy(py_data + 5, s->data, s->data_count);
c_decoder_put_python(di, s->rwl_ss, s->data_es, s->out_python, "TRANSACTION", py_data, 5 + s->data_count);
```

---

## 4. 通用C解码器模板

### 4.1 文件结构模板

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

// 1. Annotation枚举
enum { ... NUM_ANN };

// 2. 状态枚举
enum xxx_state { ... };

// 3. 状态结构体
typedef struct { ... } xxx_state;

// 4. SPI DATA包解析辅助函数
static int spi_data_get_mosi(const unsigned char *data, uint64_t data_len, uint64_t *val) { ... }
static int spi_data_get_miso(const unsigned char *data, uint64_t data_len, uint64_t *val) { ... }

// 5. 查找表(寄存器名/命令名等)
static const char *xxx_regs[] = { ... };

// 6. Annotation labels
static const char *xxx_ann_labels[][3] = { ... };

// 7. Annotation rows
static const int xxx_row_xxx_classes[] = { ..., -1 };
static const struct srd_c_ann_row xxx_ann_rows[] = { ... };

// 8. Options (如有)
static struct srd_decoder_option xxx_options[] = { ... };

// 9. Inputs/Outputs/Tags
static const char *xxx_inputs[] = {"spi", NULL};
static const char *xxx_outputs[] = { ... }; // 或 NULL
static const char *xxx_tags[] = { ... };

// 10. 内部辅助函数
static void xxx_handle_xxx(...) { ... }

// 11. reset
static void xxx_reset(struct srd_decoder_inst *di) { ... }

// 12. start
static void xxx_start(struct srd_decoder_inst *di) { ... }

// 13. decode (空函数)
static void xxx_decode(struct srd_decoder_inst *di) { (void)di; }

// 14. recv_proto (核心逻辑)
static void xxx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len) { ... }

// 15. destroy
static void xxx_destroy(struct srd_decoder_inst *di) { ... }

// 16. srd_c_decoder结构体
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    ...
    .recv_proto = xxx_recv_proto,
};

// 17. 入口函数
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void) { ... }
SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void) { return SRD_C_DECODER_API_VERSION; }
```

### 4.2 SPI DATA包解析辅助函数(所有解码器共用)

```c
static inline int spi_proto_get_mosi(const unsigned char *data, uint64_t data_len, uint8_t *mosi_val)
{
    if (data_len < 17 || !(data[0] & 1)) {
        *mosi_val = 0;
        return 0; // no MOSI
    }
    *mosi_val = (uint8_t)data[1]; // SPI wordsize=8时，低字节即有效值
    return 1;
}

static inline int spi_proto_get_miso(const unsigned char *data, uint64_t data_len, uint8_t *miso_val)
{
    if (data_len < 17 || !((data[0] >> 1) & 1)) {
        *miso_val = 0;
        return 0; // no MISO
    }
    *miso_val = (uint8_t)data[9]; // SPI wordsize=8时，低字节即有效值
    return 1;
}

static inline int spi_proto_cs_change_get_values(const unsigned char *data, uint64_t data_len,
                                                  uint8_t *prev, uint8_t *cur)
{
    if (data_len < 2) {
        *prev = 0xFF; *cur = 0xFF;
        return -1;
    }
    *prev = data[0];
    *cur = data[1];
    return 0;
}
```

**注意**: 以上辅助函数假设SPI wordsize=8(默认值)。对于非8-bit wordsize的场景，需要从完整的8字节uint64_t中提取值。但由于这5个上层解码器都是面向字节的协议，wordsize=8是唯一合理配置，因此简化为只取低字节即可。

**注意**: `spi_proto_cs_change_get_values()` 在 `data_len<2` 时返回-1并设置 `prev=cur=0xFF`。调用者应检查返回值：若返回-1，表示无CS引脚，CS应视为始终asserted（等效于 `cs_asserted=1`）。
<!-- Updated: 补充无CS引脚时CS-CHANGE辅助函数的处理说明 -->

---

## 5. CMakeLists.txt 修改

在 `CMakeLists.txt` 第837行的 `C_DECODERS` 列表中追加5个新解码器：

```
set(C_DECODERS ... st25r39xx_spi_c sdcard_spi_c spiflash_c spi_tpm_c tpm_tis_spi_c)
```

---

## 6. 复杂度评估与风险

| 解码器 | 代码行数估计 | 复杂度 | 主要风险 |
|--------|------------|--------|---------|
| st25r39xx_spi_c | ~600行 | 中 | 寄存器查找表较大(100+条目)，Space B/TestAccess两级命令解析 |
| sdcard_spi_c | ~800行 | 高 | 135个annotation，复杂状态机(20+状态)，CMD/ACMD切换，数据块传输 |
| spiflash_c | ~900行 | 高 | 28个命令handler，芯片选项，延迟输出(on_end_transaction)，2READ双I/O |
| spi_tpm_c | ~700行 | 中高 | VMK提取(正则匹配→C字符串匹配)，RangeDict查找表，TPM版本切换 |
| tpm_tis_spi_c | ~400行 | 中 | 协程→状态机转换，python输出，duplex warning |

### 最高风险项

1. **sdcard_spi**: 135个annotation class + 20+状态 → 建议最后实现
2. **spiflash**: 延迟输出(on_end_transaction)机制在C中需用标志位+CS-CHANGE回调实现
3. **spi_tpm VMK提取**: 正则匹配需转为C字符串比较，环形缓冲区逻辑需仔细移植
