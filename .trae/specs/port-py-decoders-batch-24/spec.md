# Python → C 解码器移植规格书 — Batch 24

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层协议输出范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |
| c_decoder_utils.h | BITS v2格式文档 | per-bit时间戳的BITS消息格式定义和解析示例 | <!-- Updated: 添加BITS v2格式参考 -->

## 概述

本规格书覆盖 5 个 SPI 上层协议解码器的 Python → C 移植工作。所有解码器均以 `inputs=['spi']` 为输入，通过 `recv_proto()` 回调接收 SPI C 解码器发送的协议数据，而非直接调用 `decode()` 处理原始采样。

**目标解码器列表**：

| # | Python id | C id | 芯片 | 复杂度 |
|---|-----------|------|------|--------|
| 1 | adns5020 | adns5020_c | Avago ADNS-5020 光学鼠标传感器 | ★☆☆ |
| 2 | as5047 | as5047_c | AMS AS5047 磁编码器 | ★★☆ |
| 3 | avr_isp | avr_isp_c | Atmel AVR ISP 编程协议 | ★★☆ |
| 4 | cc1101 | cc1101_c | TI CC1101 Sub-1GHz RF 收发器 | ★★★ |
| 5 | cyrf6936 | cyrf6936_c | Cypress CYRF6936 2.4GHz RF SoC | ★★★★ |

---

## 通用架构规范

### 1. 文件命名与位置

- 文件路径：`libsigrokdecode/c_decoders/{id}_c.c`
- CMake 注册：在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加解码器 id（不含 `_c` 后缀）

### 2. srd_c_decoder 结构体规范

```c
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",                    // Python id + "_c"
    .name = "XXX(C)",                 // 显示名 + "(C)" 后缀
    .longname = "Full Name (C)",      // 完整名 + "(C)"
    .desc = "... (C implementation)", // 描述 + "(C implementation)"
    .license = "gplv2+",             // 与 Python 版保持一致
    .channels = NULL,                 // SPI 上层解码器无直接通道
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = xxx_options,           // 如有选项
    .num_options = N,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,     // 第一列必须为 ""
    .num_annotation_rows = N,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,             // {"spi", NULL}
    .num_inputs = 1,
    .outputs = xxx_outputs,           // 如有输出
    .num_outputs = N,
    .binary = NULL,
    .num_binary = 0,
    .tags = xxx_tags,
    .num_tags = N,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,             // 空函数，SPI 上层不用
    .destroy = xxx_destroy,
    .recv_proto = xxx_recv_proto,     // ★ 核心回调
};
```

### 3. ann_labels 规范

每个 annotation class 的 label 定义为 `{"", "short_name", "long_description"}` 三元组，第一列固定为 `""`。

### 4. annotation_rows 规范

所有 annotation class 必须映射到某个 row。row 的 class 数组以 `-1` 结尾，但 `srd_c_ann_row` 结构体的 `num_classes` 字段记录实际数量（不含 -1）。

### 5. SPI recv_proto 数据格式

SPI C 解码器通过 `c_decoder_put_python()` 向上层发送以下协议命令：

| cmd 字符串 | data 格式 | 说明 |
|-----------|----------|------|
| `"DATA"` | `data[0]` = flags (bit0=have_mosi, bit1=have_miso), `data[1:9]` = mosi_byte (uint64_t LE), `data[9:17]` = miso_byte (uint64_t LE) | 每字节传输一次 |
| `"CS-CHANGE"` | `data[0]` = old_cs (0xFF=首次), `data[1]` = new_cs | CS 电平变化 |
| `"BITS"` | BITS v2 格式（per-bit ss/es时间戳） | 位级数据（多数上层解码器忽略），详见c_decoder_utils.h | <!-- Updated: BITS格式已升级为v2，包含per-bit时间戳 -->
| `"TRANSFER"` | 无 data | 一次 CS 低电平期间的完整传输结束 |

**DATA 解析示例**：
```c
static void parse_spi_data(const unsigned char *data, uint64_t data_len,
                           int *have_mosi, int *have_miso,
                           uint8_t *mosi_byte, uint8_t *miso_byte)
{
    if (data_len < 1) return;
    *have_mosi = (data[0] & 1) ? 1 : 0;
    *have_miso = (data[0] & 2) ? 1 : 0;
    uint64_t mv = 0, sv = 0;
    if (data_len >= 9) {
        for (int i = 0; i < 8; i++)
            mv |= ((uint64_t)data[1 + i]) << (8 * i);
    }
    if (data_len >= 17) {
        for (int i = 0; i < 8; i++)
            sv |= ((uint64_t)data[9 + i]) << (8 * i);
    }
    *mosi_byte = (uint8_t)mv;
    *miso_byte = (uint8_t)sv;
}
```

**CS-CHANGE 解析示例**：
```c
static void parse_cs_change(const unsigned char *data, uint64_t data_len,
                            int *cs_old, int *cs_new)
{
    *cs_old = (data_len > 0) ? (int)data[0] : -1;  // 0xFF = 首次/无效
    *cs_new = (data_len > 1) ? (int)data[1] : -1;
    if (*cs_old == 0xFF) *cs_old = -1;  // 首次无旧值
}
```

### 6. 私有状态管理

```c
typedef struct {
    // 状态机枚举
    int state;
    // 命令字节缓冲
    uint8_t mosi_bytes[MAX_CMD_LEN];
    uint8_t miso_bytes[MAX_CMD_LEN];
    int byte_count;
    // 采样位置
    uint64_t ss_cmd, es_cmd;
    uint64_t ss_block, es_block;
    // 输出 ID
    int out_ann;
    // 其他解码器特定字段
} xxx_state;
```

- `reset()`: 首次调用时 `g_malloc0()` 分配，后续调用 `memset()` 清零
- `start()`: 调用 `c_decoder_register_output(di, SRD_OUTPUT_ANN, "xxx")` 注册输出
- `destroy()`: `g_free()` 释放私有数据

### 7. 注释输出宏

```c
C_ANN_PUT(di, start_sample, end_sample, out_ann, ann_class, text);
```

---

## 解码器详细规格

---

### 1. ADNS-5020 (`adns5020_c`)

#### 1.1 元数据映射

| 属性 | Python | C |
|------|--------|---|
| id | adns5020 | adns5020_c |
| name | ADNS-5020 | ADNS-5020(C) |
| longname | Avago ADNS-5020 | Avago ADNS-5020 (C) |
| desc | Bidirectional optical mouse sensor protocol. | Bidirectional optical mouse sensor protocol. (C implementation) |
| license | gplv2+ | gplv2+ |
| inputs | ['spi'] | {"spi", NULL} |
| outputs | [] | NULL |
| tags | ['IC', 'PC', 'Sensor'] | {"IC", "PC", "Sensor", NULL} |

#### 1.2 Annotations

| Index | Python (id, label) | C ann_labels |
|-------|-------------------|--------------|
| 0 | ('read', 'Register read commands') | {"", "read", "Register read commands"} |
| 1 | ('write', 'Register write commands') | {"", "write", "Register write commands"} |
| 2 | ('warning', 'Warnings') | {"", "warning", "Warnings"} |

NUM_ANN = 3

#### 1.3 Annotation Rows

| Row id | Row name | Classes |
|--------|----------|---------|
| read | Read | {0} |
| write | Write | {1} |
| warnings | Warnings | {2} |

#### 1.4 寄存器映射表

```c
static const struct { int addr; const char *name; } adns5020_regs[] = {
    {0x00, "Product_ID"},
    {0x01, "Revision_ID"},
    {0x02, "Motion"},
    {0x03, "Delta_X"},
    {0x04, "Delta_Y"},
    {0x05, "SQUAL"},
    {0x06, "Shutter_Upper"},
    {0x07, "Shutter_Lower"},
    {0x08, "Maximum_Pixel"},
    {0x09, "Pixel_Sum"},
    {0x0A, "Minimum_Pixel"},
    {0x0B, "Pixel_Grab"},
    {0x0D, "Mouse_Control"},
    {0x3A, "Chip_Reset"},
    {0x3F, "Inv_Rev_ID"},
    {0x63, "Motion_Burst"},
    {-1, NULL}  // 哨兵
};
```

#### 1.5 解码逻辑

**状态机**：无显式状态，通过 `mosi_bytes` 计数驱动。

**recv_proto 处理流程**：

1. **CS-CHANGE**：若 cs_old==0 && cs_new==1（上升沿），且 mosi_bytes 数量不为 0 或 2，输出 warning "Misplaced CS#!"，清空缓冲。
2. **DATA**：
   - 若 byte_count==0，记录 ss_cmd = start_sample
   - 追加 mosi 到 mosi_bytes，byte_count++
   - 若 byte_count != 2，返回（等待更多字节）
   - byte_count==2 时：es_cmd = end_sample
     - cmd = mosi_bytes[0], arg = mosi_bytes[1]
     - write = cmd & 0x80, reg = cmd & 0x7f
     - 查找 reg 描述（regs 表，>0x63 为 Unknown）
     - 若 write：输出 ANN_WRITE `"{reg_desc}: {arg}"`
     - 若 read：输出 ANN_READ `"{reg_desc}: {arg}"`
   - 清空 mosi_bytes，byte_count=0

**关键 C 代码片段**：

```c
enum {
    ANN_READ = 0,
    ANN_WRITE,
    ANN_WARN,
    NUM_ANN,
};

typedef struct {
    uint8_t mosi_bytes[2];
    int byte_count;
    uint64_t ss_cmd, es_cmd;
    int out_ann;
} adns5020_state;

static const char *adns5020_reg_name(int reg)
{
    for (int i = 0; adns5020_regs[i].name; i++) {
        if (adns5020_regs[i].addr == reg)
            return adns5020_regs[i].name;
    }
    if (reg > 0x63) return "Unknown";
    return "Reserved";
}

static void adns5020_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    adns5020_state *s = (adns5020_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "CS-CHANGE") == 0) {
        int cs_old = -1, cs_new = -1;
        parse_cs_change(data, data_len, &cs_old, &cs_new);
        if (cs_old == 0 && cs_new == 1) {
            if (s->byte_count != 0 && s->byte_count != 2) {
                C_ANN_PUT(di, s->ss_cmd, end_sample, s->out_ann, ANN_WARN, "Misplaced CS#!");
            }
            s->byte_count = 0;
        }
        return;
    }

    if (strcmp(cmd, "DATA") != 0) return;

    int have_mosi, have_miso;
    uint8_t mosi, miso;
    parse_spi_data(data, data_len, &have_mosi, &have_miso, &mosi, &miso);
    if (!have_mosi) return;

    if (s->byte_count == 0)
        s->ss_cmd = start_sample;
    s->mosi_bytes[s->byte_count++] = mosi;

    if (s->byte_count != 2) return;

    s->es_cmd = end_sample;
    uint8_t c = s->mosi_bytes[0], arg = s->mosi_bytes[1];
    int write = c & 0x80;
    int reg = c & 0x7f;
    const char *reg_desc = adns5020_reg_name(reg);

    char buf[128];
    if (write) {
        snprintf(buf, sizeof(buf), "%s: %02X", reg_desc, arg);
        C_ANN_PUT(di, s->ss_cmd, s->es_cmd, s->out_ann, ANN_WRITE, buf);
    } else {
        snprintf(buf, sizeof(buf), "%s: %02X", reg_desc, arg);
        C_ANN_PUT(di, s->ss_cmd, s->es_cmd, s->out_ann, ANN_READ, buf);
    }
    s->byte_count = 0;
}
```

---

### 2. AS5047 (`as5047_c`)

#### 2.1 元数据映射

| 属性 | Python | C |
|------|--------|---|
| id | as5047 | as5047_c |
| name | as5047 | AS5047(C) |
| longname | as5047 | AS5047 (C) |
| desc | as5047 | AS5047 magnetic rotary encoder. (C implementation) |
| license | JSON | gplv2+ | <!-- Updated: Python版license字段为'JSON'（非标准标识符），但pd.py文件头为GPLv2+，C版应遵循文件头使用gplv2+ -->
| inputs | ['spi'] | {"spi", NULL} |
| outputs | [] | NULL |
| tags | ['Embedded/industrial'] | {"Embedded/industrial", NULL} |

#### 2.2 Annotations

| Index | Python (id, label) | C ann_labels |
|-------|-------------------|--------------|
| 0 | ('commandframe', 'command frame') | {"", "commandframe", "command frame"} |
| 1 | ('readdataframe', 'read data frame') | {"", "readdataframe", "read data frame"} |
| 2 | ('writedataframe', 'write data frame') | {"", "writedataframe", "write data frame"} |
| 3 | ('registerread', 'register read') | {"", "registerread", "register read"} |
| 4 | ('registerwrite', 'register write') | {"", "registerwrite", "register write"} |
| 5 | ('warning', 'warning') | {"", "warning", "warning"} |
| 6 | ('field', 'field') | {"", "field", "field"} |

NUM_ANN = 7

#### 2.3 Annotation Rows

| Row id | Row name | Classes |
|--------|----------|---------|
| fields | fields | {6} |
| frames | frames | {0, 1, 2} |
| transactions | transactions | {3, 4} |
| warnings | warnings | {5} |

#### 2.4 寄存器映射表

```c
static const struct { uint16_t addr; const char *name; } as5047_regs[] = {
    {0x0000, "NOP"},
    {0x0001, "ERRFL"},
    {0x0003, "PROG"},
    {0x0016, "ZPOSM"},
    {0x0017, "ZPOSL"},
    {0x0018, "SETTINGS1"},
    {0x0019, "SETTINGS2"},
    {0x3FFC, "DIAAGC"},
    {0x3FFD, "MAG"},
    {0x3FFE, "ANGLEUNC"},
    {0x3FFF, "ANGLECOM"},
    {0xFFFF, NULL}  // 哨兵
};
```

#### 2.5 解码逻辑

**状态机**：

```
INIT ──(DATA, 第1帧)──> READ / WRITE ──(DATA, 第2帧)──> INIT
```

- `INIT`：接收第一帧（命令帧），解析 bit14=R/W，bits13:0=register addr
  - bit14=1 → `READ` 状态，输出 ANN_COMMANDFRAME "read from {reg} (0x{reg:04x})"
  - bit14=0 → `WRITE` 状态，输出 ANN_COMMANDFRAME "write to {reg} (0x{reg:04x})"
  - 检查 MOSI 奇偶校验（popcount%2!=0 → warning）
- `READ`：接收第二帧（数据帧）
  - 检查 MISO 奇偶校验
  - 检查 MISO bit14（error flag）
  - data = miso & 0x3FFF
  - 输出 ANN_READDATAFRAME "read data frame: 0x{data:04x}"
  - 输出 ANN_REGISTERREAD "Read 0x{data:04x} from {reg}"（跨帧，ss=transaction_start）
- `WRITE`：接收第二帧
  - data = mosi & 0x3FFF
  - 输出 ANN_WRITEDATAFRAME "write data frame: 0x{data:04x}"
  - 输出 ANN_REGISTERWRITE "Write 0x{data:04x} to {reg}"（跨帧）

**奇偶校验函数**：

```c
static int popcount_parity(uint16_t v)
{
    int count = 0;
    while (v) { count += v & 1; v >>= 1; }
    return count % 2;  // 1 = 奇数 = 校验错误
}
```

**关键 C 代码片段**：

```c
enum {
    AS5047_STATE_INIT = 0,
    AS5047_STATE_READ,
    AS5047_STATE_WRITE,
};

typedef struct {
    int state;
    uint64_t transaction_start;
    uint16_t current_reg;
    int out_ann;
} as5047_state;

static const char *as5047_reg_name(uint16_t addr)
{
    for (int i = 0; as5047_regs[i].name; i++) {
        if (as5047_regs[i].addr == addr)
            return as5047_regs[i].name;
    }
    return "unknown";
}

static void as5047_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    as5047_state *s = (as5047_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "CS-CHANGE") == 0) {
        // CS 上升沿时重置状态
        int cs_old = -1, cs_new = -1;
        parse_cs_change(data, data_len, &cs_old, &cs_new);
        if (cs_old == 0 && cs_new == 1) {
            s->state = AS5047_STATE_INIT;
        }
        return;
    }

    if (strcmp(cmd, "DATA") != 0) return;

    int have_mosi, have_miso;
    uint8_t mosi_b, miso_b;
    parse_spi_data(data, data_len, &have_mosi, &have_miso, &mosi_b, &miso_b);

    // AS5047 是 16-bit 帧，需要两字节组装
    // 注意：SPI 每次传输 8 位，16 位需要两次 DATA 回调
    // 此处简化为假设 SPI wordsize=16 或需要字节组装逻辑
    // ...（详见完整实现）
}
```

**⚠️ 重要注意事项**：AS5047 使用 16-bit SPI 帧。若 SPI 解码器配置为 8-bit word，则每个 16-bit 帧会产生两次 DATA 回调，需要在 C 代码中做字节组装（先收高字节，再收低字节）。若 SPI 配置为 16-bit word，则每次 DATA 回调直接包含完整的 16-bit 值。建议在 C 实现中添加字节组装逻辑以兼容两种配置。

---

### 3. AVR ISP (`avr_isp_c`)

#### 3.1 元数据映射

| 属性 | Python | C |
|------|--------|---|
| id | avr_isp | avr_isp_c |
| name | AVR ISP | AVR ISP(C) |
| longname | AVR In-System Programming | AVR In-System Programming (C) |
| desc | Atmel AVR In-System Programming (ISP) protocol. | Atmel AVR In-System Programming (ISP) protocol. (C implementation) |
| license | gplv2+ | gplv2+ |
| inputs | ['spi'] | {"spi", NULL} |
| outputs | [] | NULL |
| tags | ['Debug/trace'] | {"Debug/trace", NULL} |

#### 3.2 Annotations

| Index | Python (id, label) | C ann_labels |
|-------|-------------------|--------------|
| 0 | ('pe', 'Programming enable') | {"", "pe", "Programming enable"} |
| 1 | ('rsb0', 'Read signature byte 0') | {"", "rsb0", "Read signature byte 0"} |
| 2 | ('rsb1', 'Read signature byte 1') | {"", "rsb1", "Read signature byte 1"} |
| 3 | ('rsb2', 'Read signature byte 2') | {"", "rsb2", "Read signature byte 2"} |
| 4 | ('ce', 'Chip erase') | {"", "ce", "Chip erase"} |
| 5 | ('rfb', 'Read fuse bits') | {"", "rfb", "Read fuse bits"} |
| 6 | ('rhfb', 'Read high fuse bits') | {"", "rhfb", "Read high fuse bits"} |
| 7 | ('refb', 'Read extended fuse bits') | {"", "refb", "Read extended fuse bits"} |
| 8 | ('rlb', 'Read lock bits') | {"", "rlb", "Read lock bits"} |
| 9 | ('reem', 'Read EEPROM memory') | {"", "reem", "Read EEPROM memory"} |
| 10 | ('rp', 'Read program memory') | {"", "rp", "Read program memory"} |
| 11 | ('lpmp', 'Load program memory page') | {"", "lpmp", "Load program memory page"} |
| 12 | ('wp', 'Write program memory') | {"", "wp", "Write program memory"} |
| 13 | ('warning', 'Warning') | {"", "warning", "Warning"} |
| 14 | ('dev', 'Device') | {"", "dev", "Device"} |

NUM_ANN = 15

#### 3.3 Annotation Rows

| Row id | Row name | Classes |
|--------|----------|---------|
| commands | Commands | {0,1,2,3,4,5,6,7,8,9,10,11,12} |
| warnings | Warnings | {13} |
| devs | Devices | {14} |

#### 3.4 命令分发表

```c
// 命令识别规则（4字节 MOSI 命令）
// [0xAC, 0x53, *, *] → Programming Enable
// [0xAC, 0x80|*, *, *] (cmd[1] & 0x80) → Chip Erase
// [0x50, 0x00, 0x00, *] → Read Fuse Bits
// [0x58, 0x08, 0x00, *] → Read Fuse High Bits
// [0x50, 0x08, 0x00, *] → Read Extended Fuse Bits
// [0x30, *, 0x00, *] → Read Signature Byte 0
// [0x30, *, 0x01, *] → Read Signature Byte 1
// [0x30, *, 0x02, *] → Read Signature Byte 2
// [0x58, 0x00, *, *] → Read Lock Bits
// [0xA0, *, *, *] (cmd[1] & 0xC0 == 0x00) → Read EEPROM Memory
// [0x20|0x28, *, *, *] (cmd[1] & 0xF0 == 0x00) → Read Program Memory
// [0x40|0x48, *, *, *] (cmd[1] & 0xF0 == 0x00) → Load Program Memory Page
// [0x4C, *, *, *] (cmd[1] & 0xF0 == 0x00) → Write Program Memory Page
```

#### 3.5 设备查找表

```c
static const struct { uint8_t fam; uint8_t part; const char *name; } avr_devices[] = {
    {0x90, 0x01, "AT90S1200"},
    {0x91, 0x01, "AT90S2313"},
    {0x92, 0x01, "AT90S4414"},
    {0x92, 0x05, "ATmega48"},
    {0x93, 0x01, "AT90S8515"},
    {0x93, 0x0A, "ATmega88"},
    {0x94, 0x06, "ATmega168"},
    {0xFF, 0xFF, "Device code erased, or target missing"},
    {0x01, 0x02, "Device locked"},
    {0, 0, NULL}
};

static const char *vendor_code_name(uint8_t code)
{
    if (code == 0x1E) return "Atmel";
    if (code == 0x00) return "Device locked";
    return "Unknown";
}
```

#### 3.6 解码逻辑

**状态机**：IDLE → 收集 4 字节 → 分发命令 → IDLE

- 收集 4 个 MOSI/MISO 字节对
- byte_count==0 时记录 ss_cmd
- byte_count==4 时记录 es_cmd，调用 handle_command()
- handle_command() 根据 cmd[0:2] 模式匹配，调用对应处理函数
- 各处理函数输出对应 annotation class，部分做 sanity check 并输出 warning

**关键 C 代码片段**：

```c
typedef struct {
    uint8_t mosi_bytes[4];
    uint8_t miso_bytes[4];
    int byte_count;
    uint64_t ss_cmd, es_cmd;
    uint64_t ss_device;
    uint8_t xx, yy, zz, mm;
    uint8_t vendor_code;
    uint8_t part_fam_flash_size;
    uint8_t part_number;
    int out_ann;
} avr_isp_state;

static void avr_isp_handle_command(struct srd_decoder_inst *di, avr_isp_state *s,
    const uint8_t *cmd, const uint8_t *ret)
{
    // Programming Enable: [0xAC, 0x53, *, *]
    if (cmd[0] == 0xAC && cmd[1] == 0x53) {
        C_ANN_PUT(di, s->ss_cmd, s->es_cmd, s->out_ann, ANN_PE, "Programming enable");
        if (ret[1] != 0xAC || ret[2] != 0x53 || ret[3] != cmd[2]) {
            C_ANN_PUT(di, s->ss_cmd, s->es_cmd, s->out_ann, ANN_WARN,
                      "Warning: Unexpected bytes in reply!");
        }
        return;
    }
    // ... 其他命令处理
}
```

**⚠️ BITS 包处理**：Python 版处理 `BITS` 类型包来存储位值。C 版 recv_proto 中 SPI 发送的 BITS 包可忽略（上层解码器通常不需要位级数据），但需确保不因收到 BITS 包而中断状态机。

---

### 4. CC1101 (`cc1101_c`)

#### 4.1 元数据映射

| 属性 | Python | C |
|------|--------|---|
| id | cc1101 | cc1101_c |
| name | CC1101 | CC1101(C) |
| longname | Texas Instruments CC1101 | Texas Instruments CC1101 (C) |
| desc | Low-power sub-1GHz RF transceiver chip. | Low-power sub-1GHz RF transceiver chip. (C implementation) |
| license | gplv2+ | gplv2+ |
| inputs | ['spi'] | {"spi", NULL} |
| outputs | [] | NULL |
| tags | ['IC', 'Wireless/RF'] | {"IC", "Wireless/RF", NULL} |

#### 4.2 Annotations

| Index | Python (id, label) | C ann_labels |
|-------|-------------------|--------------|
| 0 | ('strobe', 'Command strobe') | {"", "strobe", "Command strobe"} |
| 1 | ('single_read', 'Single register read') | {"", "single_read", "Single register read"} |
| 2 | ('single_write', 'Single register write') | {"", "single_write", "Single register write"} |
| 3 | ('burst_read', 'Burst register read') | {"", "burst_read", "Burst register read"} |
| 4 | ('burst_write', 'Burst register write') | {"", "burst_write", "Burst register write"} |
| 5 | ('status_read', 'Status read') | {"", "status_read", "Status read"} |
| 6 | ('status', 'Status register') | {"", "status", "Status register"} |
| 7 | ('warning', 'Warning') | {"", "warning", "Warning"} |

NUM_ANN = 8

#### 4.3 Annotation Rows

| Row id | Row name | Classes |
|--------|----------|---------|
| cmd | Commands | {0} |
| data | Data | {1, 2, 3, 4, 5} |
| status | Status register | {6} |
| warnings | Warnings | {7} |

#### 4.4 寄存器/命令映射表

```c
// 配置寄存器 (addr < 0x30)
static const struct { uint8_t addr; const char *name; } cc1101_regs[] = {
    {0x00, "IOCFG2"}, {0x01, "IOCFG1"}, {0x02, "IOCFG0"},
    {0x03, "FIFOTHR"}, {0x04, "SYNC1"}, {0x05, "SYNC0"},
    {0x06, "PKTLEN"}, {0x07, "PKTCTRL1"}, {0x08, "PKTCTRL0"},
    {0x09, "ADDR"}, {0x0A, "CHANNR"}, {0x0B, "FSCTRL1"},
    {0x0C, "FSCTRL0"}, {0x0D, "FREQ2"}, {0x0E, "FREQ1"},
    {0x0F, "FREQ0"}, {0x10, "MDMCFG4"}, {0x11, "MDMCFG3"},
    {0x12, "MDMCFG2"}, {0x13, "MDMCFG1"}, {0x14, "MDMCFG0"},
    {0x15, "DEVIATN"}, {0x16, "MCSM2"}, {0x17, "MCSM1"},
    {0x18, "MCSM0"}, {0x19, "FOCCFG"}, {0x1A, "BSCFG"},
    {0x1B, "AGCTRL2"}, {0x1C, "AGCTRL1"}, {0x1D, "AGCTRL0"},
    {0x1E, "WOREVT1"}, {0x1F, "WOREVT0"}, {0x20, "WORCTRL"},
    {0x21, "FREND1"}, {0x22, "FREND0"}, {0x23, "FSCAL3"},
    {0x24, "FSCAL2"}, {0x25, "FSCAL1"}, {0x26, "FSCAL0"},
    {0x27, "RCCTRL1"}, {0x28, "RCCTRL0"}, {0x29, "FSTEST"},
    {0x2A, "PTEST"}, {0x2B, "AGCTEST"}, {0x2C, "TEST2"},
    {0x2D, "TEST1"}, {0x2E, "TEST0"},
    {0xFF, NULL}
};

// 状态寄存器 (0x30-0x3F, 只读)
static const struct { uint8_t addr; const char *name; } cc1101_status_regs[] = {
    {0x30, "PARTNUM"}, {0x31, "VERSION"}, {0x32, "FREQEST"},
    {0x33, "LQI"}, {0x34, "RSSI"}, {0x35, "MARCSTATE"},
    {0x36, "WORTIME1"}, {0x37, "WORTIME0"}, {0x38, "PKTSTATUS"},
    {0x39, "VCO_VC_DAC"}, {0x3A, "TXBYTES"}, {0x3B, "RXBYTES"},
    {0x3C, "RCCTRL1_STATUS"}, {0x3D, "RCCTRL0_STATUS"},
    {0x3E, "PATABLE"}, {0x3F, "FIFO"},
    {0xFF, NULL}
};

// 命令选通
static const struct { uint8_t addr; const char *name; } cc1101_strobes[] = {
    {0x30, "SRES"}, {0x31, "SFSTXON"}, {0x32, "SXOFF"},
    {0x33, "SCAL"}, {0x34, "SRX"}, {0x35, "STX"},
    {0x36, "SIDLE"}, {0x37, ""}, {0x38, "SWOR"},
    {0x39, "SPWD"}, {0x3A, "SFRX"}, {0x3B, "SFTX"},
    {0x3C, "SWORRST"}, {0x3D, "SNOP"},
    {0xFF, NULL}
};

// 状态寄存器 STATE 域
static const char *cc1101_status_states[] = {
    "IDLE", "RX", "TX", "FSTXON", "CALIBRATE", "SETTLING",
    "RXFIFO_OVERFLOW", "TXFIFO_OVERFLOW"
};
```

#### 4.5 命令字节解析

```c
// 命令字节格式：bits7:6 = R/W + Burst, bits5:0 = 地址
// addr < 0x30 或 addr==0x3E/0x3F: 配置寄存器
//   0x00 = Write (单字节), 0x40 = Burst write, 0x80 = Read (单字节), 0xC0 = Burst read
// addr >= 0x30 (非 0x3E/0x3F): 命令选通/状态
//   0x00-0x3F = Strobe, 0xC0 = Status read

enum cc1101_cmd_type {
    CC1101_CMD_WRITE = 0,
    CC1101_CMD_BURST_WRITE,
    CC1101_CMD_READ,
    CC1101_CMD_BURST_READ,
    CC1101_CMD_STROBE,
    CC1101_CMD_STATUS_READ,
    CC1101_CMD_UNKNOWN,
};

static int cc1101_parse_command(uint8_t b, int *addr, int *min_bytes, int *max_bytes)
{
    *addr = b & 0x3F;
    if (*addr < 0x30 || *addr == 0x3E || *addr == 0x3F) {
        switch (b & 0xC0) {
        case 0x00: *min_bytes = 1; *max_bytes = 1; return CC1101_CMD_WRITE;
        case 0x40: *min_bytes = 1; *max_bytes = 99999; return CC1101_CMD_BURST_WRITE;
        case 0x80: *min_bytes = 1; *max_bytes = 1; return CC1101_CMD_READ;
        case 0xC0: *min_bytes = 1; *max_bytes = 99999; return CC1101_CMD_BURST_READ;
        }
    } else {
        if ((b & 0x40) == 0x00) { *min_bytes = 0; *max_bytes = 0; return CC1101_CMD_STROBE; }
        if ((b & 0xC0) == 0xC0) { *min_bytes = 1; *max_bytes = 99999; return CC1101_CMD_STATUS_READ; }
    }
    return CC1101_CMD_UNKNOWN;
}
```

#### 4.6 解码逻辑

**状态机**：

```
FIRST_BYTE ──(DATA, cmd byte)──> COLLECT_DATA ──(DATA, data bytes)──> ... ──(CS-CHANGE, rising)──> FIRST_BYTE
```

- `first = true`：第一个 DATA 包是命令字节
  - 解析命令类型、地址
  - 第一个 MISO 字节始终是状态寄存器，解码并输出 ANN_STATUS
  - 若为 Strobe 命令，立即输出 ANN_STROBE
  - 否则设置 ss_mb，开始收集数据字节
- `first = false`：后续 DATA 包是数据字节
  - 收集到 mb 缓冲
  - 若达到 max_bytes 或 CS 上升沿，调用 finish_command()
- CS 上升沿：处理已收集的数据，调用 finish_command()，重置状态

**Status 寄存器解码**：

```c
static void cc1101_decode_status(struct srd_decoder_inst *di, cc1101_state *s,
    uint64_t ss, uint64_t es, uint8_t status, const char *label)
{
    char buf[256];
    const char *chip_rdy = (status & 0x80) ? "CHIP_RDYn is high! " : "";
    int state_idx = (status & 0x70) >> 4;
    int fifo = status & 0x0F;
    const char *fifo_dir = (s->cmd_type == CC1101_CMD_READ ||
                            s->cmd_type == CC1101_CMD_BURST_READ ||
                            s->cmd_type == CC1101_CMD_STATUS_READ)
                           ? "available in RX FIFO" : "free in TX FIFO";
    snprintf(buf, sizeof(buf), "%s = %02X; %sSTATE is %s, %d bytes %s",
             label, status, chip_rdy, cc1101_status_states[state_idx], fifo, fifo_dir);
    C_ANN_PUT(di, ss, es, s->out_ann, ANN_STATUS, buf);
}
```

---

### 5. CYRF6936 (`cyrf6936_c`)

#### 5.1 元数据映射

| 属性 | Python | C |
|------|--------|---|
| id | cyrf6936 | cyrf6936_c |
| name | CYRF6936 | CYRF6936(C) |
| longname | Cypress CYRF6936 WirelessUSB(TM) LP 2.4 GHz Radio SoC | Cypress CYRF6936 WirelessUSB LP 2.4 GHz Radio SoC (C) |
| desc | 2.4GHz transceiver chip. | 2.4GHz transceiver chip. (C implementation) |
| license | gplv3+ | gplv3+ |
| inputs | ['spi'] | {"spi", NULL} |
| outputs | ['cyrf6936'] | {"cyrf6936", NULL} |
| tags | ['Embedded/industrial'] | {"Embedded/industrial", NULL} |

#### 5.2 Annotations

| Index | Python (id, label) | C ann_labels |
|-------|-------------------|--------------|
| 0 | ('write', 'Write') | {"", "write", "Write"} |
| 1 | ('read', 'Read') | {"", "read", "Read"} |
| 2 | ('tx-data', 'Payload sent to the device') | {"", "tx-data", "Payload sent to the device"} |
| 3 | ('rx-data', 'Payload read from the device') | {"", "rx-data", "Payload read from the device"} |
| 4 | ('state', 'State change') | {"", "state", "State change"} |
| 5 | ('warning', 'Warnings') | {"", "warning", "Warnings"} |
| 6 | ('wait', 'Wait') | {"", "wait", "Wait"} |

NUM_ANN = 7

#### 5.3 Annotation Rows

| Row id | Row name | Classes |
|--------|----------|---------|
| cmd | Commands | {0, 1, 2, 3} |
| warnings | Warnings | {4, 5} |
| delays | Delays | {6} |

#### 5.4 Options

```c
static struct srd_decoder_option cyrf6936_options[] = {
    {"spi3pin", "dec_cyrf6936_opt_spi3pin",
     "SPI 3-pin mode with MOSI/MISO combined as SDAT on the MOSI pin",
     NULL, NULL},
    {"delaysplit", "dec_cyrf6936_opt_delaysplit",
     "annotate delays (in us) larger than... (0 = off)",
     NULL, NULL},
    {"invert_mosi", "dec_cyrf6936_opt_invert_mosi",
     "Invert MOSI",
     NULL, NULL},
    {"invert_miso", "dec_cyrf6936_opt_invert_miso",
     "Invert MISO",
     NULL, NULL},
};
```

#### 5.5 Binary Output

```c
static const struct srd_decoder_binary cyrf6936_binary[] = {
    {0, "txpayload", "Transfer payload"},
    {1, "rxpayload", "Receive payload"},
};
```

**注意**：C 解码器的 binary 输出需要通过 `c_decoder_register_output(di, SRD_OUTPUT_BINARY, "xxx")` 注册，并通过 `c_decoder_put_binary()` 输出。`c_decoder_put_binary()` 已在 C decoder API 中实现，可直接使用。 <!-- Updated: c_decoder_put_binary()已实现，不再需要"暂不实现" -->

#### 5.6 寄存器映射表

```c
static const struct { uint8_t addr; const char *name; int width; } cyrf6936_regs[] = {
    {0x00, "CHANNEL_ADR", 1}, {0x01, "TX_LENGTH_ADR", 1},
    {0x02, "TX_CTRL_ADR", 1}, {0x03, "TX_CFG_ADR", 1},
    {0x04, "TX_IRQ_STATUS_ADR", 1}, {0x05, "RX_CTRL_ADR", 1},
    {0x06, "RX_CFG_ADR", 1}, {0x07, "RX_IRQ_STATUS_ADR", 1},
    {0x08, "RX_STATUS_ADR", 1}, {0x09, "RX_COUNT_ADR", 1},
    {0x0A, "RX_LENGTH_ADR", 1}, {0x0B, "PWR_CTRL_ADR", 1},
    {0x0C, "XTAL_CTRL_ADR", 1}, {0x0D, "IO_CFG_ADR", 1},
    {0x0E, "GPIO_CTRL_ADR", 1}, {0x0F, "XACT_CFG_ADR", 1},
    {0x10, "FRAMING_CFG_ADR", 1}, {0x11, "DATA32_THOLD_ADR", 1},
    {0x12, "DATA64_THOLD_ADR", 1}, {0x13, "RSSI_ADR", 1},
    {0x14, "EOP_CTRL_ADR", 1}, {0x15, "CRC_SEED_LSB_ADR", 1},
    {0x16, "CRC_SEED_MSB_ADR", 1}, {0x17, "TX_CRC_LSB_ADR", 1},
    {0x18, "TX_CRC_MSB_ADR", 1}, {0x19, "RX_CRC_LSB_ADR", 1},
    {0x1A, "RX_CRC_MSB_ADR", 1}, {0x1B, "TX_OFFSET_LSB_ADR", 1},
    {0x1C, "TX_OFFSET_MSB_ADR", 1}, {0x1D, "MODE_OVERRIDE_ADR", 1},
    {0x1E, "RX_OVERRIDE_ADR", 1}, {0x1F, "TX_OVERRIDE_ADR", 1},
    {0x20, "TX_BUFFER_ADR", 16}, {0x21, "RX_BUFFER_ADR", 16},
    {0x22, "SOP_CODE_ADR", 8}, {0x23, "DATA_CODE_ADR", 16},
    {0x24, "PREAMBLE_ADR", 3}, {0x25, "MFG_ID_ADR", 6},
    {0x26, "XTAL_CFG_ADR", 1}, {0x27, "CLK_OFFSET_ADR", 1},
    {0x28, "CLK_EN_ADR", 1}, {0x29, "RX_ABORT_ADR", 1},
    {0x32, "AUTO_CAL_TIME_ADR", 1}, {0x35, "AUTO_CAL_OFFSET_ADR", 1},
    {0x39, "ANALOG_CTRL_ADR", 1},
    {0xFF, NULL, 0}
};
```

#### 5.7 命令字节解析

```c
// 命令字节格式：bit7 = R/W (1=write, 0=read), bit6 = increment, bits5:0 = address
static void cyrf6936_parse_command(uint8_t b, int *addr, int *dir_wr, int *inc)
{
    *addr = b & 0x3F;
    *dir_wr = (b & 0x80) >> 7;  // 1=write, 0=read
    *inc = (b & 0x40) >> 6;     // 1=auto increment
}
```

#### 5.8 寄存器解码函数

CYRF6936 的 Python 实现使用 `RegDecode` 类和 `@RDecode` 装饰器系统来解码每个寄存器的位域。C 实现需要为每个寄存器编写独立的解码函数。这是本批次中最复杂的部分。

**简化策略**：对于 C 实现，建议采用以下分层方案：

1. **基础层**：所有寄存器输出 `"read/write(inc)(REG_NAME) = 0xHH"` 格式
2. **增强层**：对关键寄存器（0x00 CHANNEL, 0x03 TX_CFG, 0x0D IO_CFG 等）实现位域解码
3. **完整层**：所有寄存器的完整位域解码（可后续迭代）

**关键寄存器解码示例（CHANNEL_ADR 0x00）**：

```c
static void cyrf6936_decode_reg_0x00(struct srd_decoder_inst *di, cyrf6936_state *s,
    uint8_t val, int dir_wr)
{
    uint8_t channel = val & 0x7F;
    const char *speed_type;
    if ((channel % 3 == 0) && channel <= 96)
        speed_type = "100us_fast";
    else if ((channel % 2 == 0) && channel <= 94)
        speed_type = "180us_medium";
    else if (channel <= 97)
        speed_type = "270us_slow";
    else
        speed_type = "not_valid";

    char buf[128];
    double freq_ghz = (2400.0 + (channel * 98.0 / 0x62)) / 1000.0;
    snprintf(buf, sizeof(buf), "CHANNEL %d (%.3fGHz, %s)", channel, freq_ghz, speed_type);

    const char *multi = s->inc ? "_inc" : "";
    const char *rw = dir_wr ? "write" : "read";
    char out[256];
    snprintf(out, sizeof(out), "%s%s(%s, \"%s\")", rw, multi,
             cyrf6936_reg_name(s->addr), buf);
    C_ANN_PUT(di, s->mb_s, s->mb_e, s->out_ann,
              dir_wr ? ANN_WRITE : ANN_READ, out);

    if (channel > 0x62) {
        char wbuf[64];
        snprintf(wbuf, sizeof(wbuf), "Warn: Channel# > %d", 0x62);
        C_ANN_PUT(di, s->mb_s, s->mb_e, s->out_ann, ANN_WARN, wbuf);
    }
}
```

#### 5.9 解码逻辑

**状态机**：

```
FIRST_BYTE ──(DATA, cmd byte)──> COLLECT_DATA ──(DATA, data bytes)──> ... ──(CS-CHANGE, rising)──> FIRST_BYTE
```

- `first = true`：命令字节
  - 解析 addr, dir_wr, inc
  - 查找寄存器宽度
  - 第一个 MISO 字节被丢弃（检查是否为 0xFF 或 0x00）
- `first = false`：数据字节
  - 收集到 mb 缓冲
  - 达到 max 字节数时，调用 finish_command() 解码
  - 若 inc=1，地址自增，重置 mb 继续收集
- CS 上升沿：finish_command()，重置状态
- CS 下降沿：记录时间戳用于 delay 计算
- IO_CFG_ADR (0x0D) 特殊处理：检测 SPI 3-pin / 4-pin 模式切换

**SPI 3-pin 模式**：当 `spi3pin=1` 时，MISO 数据从 MOSI 线读取（读操作时）。

**Delay 标注**：当 `delaysplit > 0` 且 `samplerate > 0` 时，计算 CS 释放到下次 CS 拉低的时间差，若超过阈值则输出 ANN_WAIT。

---

## 通用工具函数

以下工具函数建议在每个解码器文件中内联实现（或提取到公共头文件）：

```c
// SPI DATA 包解析
static void parse_spi_data(const unsigned char *data, uint64_t data_len,
    int *have_mosi, int *have_miso, uint8_t *mosi_byte, uint8_t *miso_byte);

// SPI CS-CHANGE 包解析
static void parse_cs_change(const unsigned char *data, uint64_t data_len,
    int *cs_old, int *cs_new);

// 寄存器名查找
static const char *xxx_reg_name(int addr);

// 人口计数奇偶校验
static int popcount_parity(uint16_t v);
```

---

## CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加：

```cmake
adns5020_c
as5047_c
avr_isp_c
cc1101_c
cyrf6936_c
```

---

## 移植风险与注意事项

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| AS5047 16-bit SPI 帧需要字节组装 | 解码逻辑复杂化 | 添加 2 字节组装缓冲，兼容 8/16-bit word |
| CYRF6936 寄存器解码函数数量大（30+） | 代码量大，易出错 | 分层实现，先基础层后增强层 |
| CYRF6936 binary 输出 | C API 已支持 | 使用c_decoder_put_binary()实现 | <!-- Updated: c_decoder_put_binary()已实现 -->
| CYRF6936 3-pin SPI 模式 | 需要在读操作时从 MOSI 取数据 | 添加 spi3pin 选项和条件判断 |
| CYRF6936 delay 标注需要 samplerate | metadata 回调 | 在 recv_proto 外还需实现 metadata 回调 |
| AVR ISP BITS 包处理 | Python 版处理，C 版可忽略 | 确保不因 BITS 包中断状态机 |
| CC1101 burst 传输无上限 | 需要动态缓冲或合理上限 | 设置 MAX_BURST_BYTES 限制 |
