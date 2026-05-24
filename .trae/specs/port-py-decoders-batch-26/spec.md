# SPI上层协议解码器Python→C移植规格书

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层协议输出范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |
| c_decoder_utils.h | BITS v2格式文档 | per-bit时间戳的BITS消息格式定义和解析示例 | <!-- Updated: 添加BITS v2格式参考 -->

## 1. 概述

将5个SPI上层协议Python解码器移植为C解码器。所有解码器的共同特征：
- **inputs = ['spi']** — 堆叠在SPI解码器之上
- **使用recv_proto()回调** — 不实现decode()，而是接收SPI下层解码器输出的python协议数据
- **无channels/optional_channels** — 信号通道由SPI下层解码器处理
- **outputs = []** — 不输出python协议数据给更上层解码器（仅输出annotation）

## 2. C解码器架构模式

### 2.1 上层解码器核心模式（recv_proto）

上层解码器**不实现decode()函数**（或实现空函数），而是通过`.recv_proto`回调接收下层解码器的python输出。

```c
// recv_proto回调签名
static void xxx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

### 2.2 SPI上层解码器收到的协议命令

SPI解码器输出的python协议命令（cmd字符串）：

| cmd | 含义 | data格式 |
|-----|------|----------|
| `"DATA"` | 一个字数据传输完成 | data[0]=标志(bit0=have_mosi,bit1=have_miso), data[1..8]=MOSI值(LE uint64), data[9..16]=MISO值(LE uint64) |
| `"BITS"` | 位级别数据 | BITS v2 格式（per-bit ss/es时间戳），详见c_decoder_utils.h | <!-- Updated: BITS格式已升级为v2，包含per-bit时间戳 -->
| `"CS-CHANGE"` | CS信号变化 | data[0]=旧值(0xFF表示首次), data[1]=新值 |
| `"TRANSFER"` | CS从assert到deassert的完整传输 | 无data |

**DATA命令解析示例**（关键，所有5个解码器都需要）：
```c
if (strcmp(cmd, "DATA") == 0 && data_len >= 17) {
    int have_mosi = data[0] & 1;
    int have_miso = (data[0] >> 1) & 1;
    uint64_t mosi_val = 0, miso_val = 0;
    for (int i = 0; i < 8; i++)
        mosi_val |= ((uint64_t)data[1 + i] << (8 * i));
    for (int i = 0; i < 8; i++)
        miso_val |= ((uint64_t)data[9 + i] << (8 * i));
    uint8_t mosi_byte = (uint8_t)mosi_val;
    uint8_t miso_byte = (uint8_t)miso_val;
    // ... 处理逻辑
}
```

### 2.3 文件结构模板

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

// 1. annotation枚举
enum { ANN_XXX = 0, ..., NUM_ANN };

// 2. 状态结构体
typedef struct {
    int out_ann;
    // ... 解码器特有状态
} xxx_state;

// 3. 静态数据（channels, options, ann_labels, ann_rows, inputs, outputs, tags）

// 4. recv_proto实现
static void xxx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len) { ... }

// 5. reset/start/decode(空)/destroy
static void xxx_reset(struct srd_decoder_inst *di) { ... }
static void xxx_start(struct srd_decoder_inst *di) { ... }
static void xxx_decode(struct srd_decoder_inst *di) { (void)di; }
static void xxx_destroy(struct srd_decoder_inst *di) { ... }

// 6. srd_c_decoder结构体（.recv_proto = xxx_recv_proto）
struct srd_c_decoder xxx_c_decoder = { ... };

// 7. 导出函数
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void) { ... }
SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void) { ... }
```

### 2.4 关键约定

- **文件名**：`{decoder_id}_c.c`，其中`-`替换为`_`（如`nrf24l01_c.c`）
- **`.id`**：`"xxx_c"`（如`"nes_gamepad_c"`）
- **`.name`**：`"XXX(C)"`（如`"NES gamepad(C)"`）
- **ann_labels第一列**：必须为`""`（空字符串），API内部处理i+7偏移
- **所有annotation class必须映射到annotation_rows**
- **inputs**：`{"spi", NULL}`
- **outputs**：`NULL`（无输出给更上层）
- **channels/optional_channels**：均为NULL（信号由SPI下层处理）
- **decode()**：空函数`(void)di;`
- **c_decoder_register_output()**：仅注册`SRD_OUTPUT_ANN`
- **Options初始化**：在`srd_c_decoder_entry()`中用`g_variant_new_*()`设置默认值和可选值列表

---

## 3. 解码器详细规格

### 3.1 nes_gamepad — NES游戏手柄

#### 3.1.1 Python元数据

| 属性 | 值 |
|------|-----|
| id | `nes_gamepad` |
| name | `NES gamepad` |
| longname | `Nintendo Entertainment System gamepad` |
| desc | `NES gamepad button states.` |
| inputs | `['spi']` |
| outputs | `[]` |
| tags | `['Retro computing']` |
| license | `gplv2+` |

**Options**:
| id | desc | default | values |
|----|------|---------|--------|
| variant | Gamepad variant | 'Standard gamepad' | ('Standard gamepad',) |

**Annotations**:
| index | id | name |
|-------|----|------|
| 0 | button | Button state |
| 1 | no-press | No button press |
| 2 | not-connected | Gamepad unconnected |

**Annotation rows**:
| id | name | classes |
|----|------|---------|
| buttons | Button states | (0,) |
| no-presses | No button presses | (1,) |
| not-connected-vals | Gamepad unconnected | (2,) |

#### 3.1.2 Python解码逻辑分析

- 仅处理`ptype == 'DATA'`
- 只使用MISO数据（`miso`值）
- `miso == 0xFF` → 无按钮按下（ANN_NO_PRESS）
- `miso == 0x00` → 手柄未连接（ANN_NOT_CONNECTED）
- 其他值：按位解析，bit=0表示按钮按下
  - bit0=A, bit1=B, bit2=Select, bit3=Start, bit4=North, bit5=South, bit6=West, bit7=East
  - 拼接按下按钮名称，用" + "连接

#### 3.1.3 C实现规划

**文件**：`nes_gamepad_c.c`

**状态结构体**：
```c
typedef struct {
    int out_ann;
    int variant; // 0=Standard gamepad
} nes_gamepad_state;
```

**recv_proto逻辑**：
```c
static void nes_gamepad_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    nes_gamepad_state *s = (nes_gamepad_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") != 0 || data_len < 17) return;

    // 解析MISO值
    int have_miso = (data[0] >> 1) & 1;
    uint64_t miso_val = 0;
    for (int i = 0; i < 8; i++)
        miso_val |= ((uint64_t)data[9 + i] << (8 * i));
    uint8_t miso = (uint8_t)miso_val;

    if (miso == 0xFF) {
        C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_NO_PRESS, "No button is pressed");
    } else if (miso == 0x00) {
        C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_NOT_CONNECTED, "Gamepad is not connected");
    } else {
        static const char *buttons[] = {"A", "B", "Select", "Start", "North", "South", "West", "East"};
        char buf[128];
        int pos = 0;
        for (int i = 0; i < 8; i++) {
            if (!(miso & (1 << i))) { // bit=0 means pressed
                if (pos > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, " + ");
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", buttons[i]);
            }
        }
        C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_BUTTON, buf);
    }
}
```

**元数据映射**：
```c
static const char *nes_gamepad_inputs[] = {"spi", NULL};
static const char *nes_gamepad_outputs[] = {NULL};  // 无输出
static const char *nes_gamepad_tags[] = {"Retro computing", NULL};

static struct srd_decoder_option nes_gamepad_options[] = {
    {"variant", "dec_nes_gamepad_opt_variant", "Gamepad variant", NULL, NULL},
};

static const char *nes_gamepad_ann_labels[][3] = {
    {"", "button", "Button state"},
    {"", "no-press", "No button press"},
    {"", "not-connected", "Gamepad unconnected"},
};

static const int nes_gamepad_row_buttons_classes[] = {ANN_BUTTON, -1};
static const int nes_gamepad_row_no_presses_classes[] = {ANN_NO_PRESS, -1};
static const int nes_gamepad_row_not_connected_classes[] = {ANN_NOT_CONNECTED, -1};

static const struct srd_c_ann_row nes_gamepad_ann_rows[] = {
    {"buttons", "Button states", nes_gamepad_row_buttons_classes, 1},
    {"no-presses", "No button presses", nes_gamepad_row_no_presses_classes, 1},
    {"not-connected-vals", "Gamepad unconnected", nes_gamepad_row_not_connected_classes, 1},
};

struct srd_c_decoder nes_gamepad_c_decoder = {
    .id = "nes_gamepad_c",
    .name = "NES gamepad(C)",
    .longname = "Nintendo Entertainment System gamepad (C)",
    .desc = "NES gamepad button states. (C implementation)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = nes_gamepad_options,
    .num_options = 1,
    .num_annotations = NUM_ANN,
    .ann_labels = nes_gamepad_ann_labels,
    .num_annotation_rows = 3,
    .annotation_rows = nes_gamepad_ann_rows,
    .inputs = nes_gamepad_inputs,
    .num_inputs = 1,
    .outputs = nes_gamepad_outputs,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = nes_gamepad_tags,
    .num_tags = 1,
    .reset = nes_gamepad_reset,
    .start = nes_gamepad_start,
    .decode = nes_gamepad_decode,
    .destroy = nes_gamepad_destroy,
    .recv_proto = nes_gamepad_recv_proto,
};
```

**srd_c_decoder_entry()**：
```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    nes_gamepad_options[0].def = g_variant_new_string("Standard gamepad");
    GSList *variant_vals = NULL;
    variant_vals = g_slist_append(variant_vals, g_variant_new_string("Standard gamepad"));
    nes_gamepad_options[0].values = variant_vals;
    return &nes_gamepad_c_decoder;
}
```

---

### 3.2 nrf24l01 — Nordic nRF24L01(+) 无线收发器

#### 3.2.1 Python元数据

| 属性 | 值 |
|------|-----|
| id | `nrf24l01` |
| name | `nRF24L01(+)` |
| longname | `Nordic Semiconductor nRF24L01(+)` |
| desc | `2.4GHz RF transceiver chip.` |
| inputs | `['spi']` |
| outputs | `[]` |
| tags | `['IC', 'Wireless/RF']` |
| license | `gplv2+` |

**Options**:
| id | desc | default | values |
|----|------|---------|--------|
| chip | Chip type | 'nrf24l01' | ('nrf24l01', 'xn297') |

**Annotations**:
| index | id | name |
|-------|----|------|
| 0 (ann_cmd) | cmd | Commands sent to the device |
| 1 (ann_tx) | tx-data | Payload sent to the device |
| 2 (ann_reg) | register | Registers read from the device |
| 3 (ann_rx) | rx-data | Payload read from the device |
| 4 (ann_warn) | warning | Warnings |

**Annotation rows**:
| id | name | classes |
|----|------|---------|
| commands | Commands | (0, 1) |
| responses | Responses | (2, 3) |
| warnings | Warnings | (4,) |

#### 3.2.2 Python解码逻辑分析

**核心状态机**：
- `self.first` — 是否是CS assert后的第一个字节
- `self.cmd` — 当前命令名称
- `self.dat` — 命令附加数据（如寄存器地址）
- `self.min`/`self.max` — 命令后最少/最多数据字节数
- `self.mb` — 收集的(mosi, miso)字节对列表
- `self.cs_was_released` — CS是否曾被释放

**处理流程**：
1. `CS-CHANGE`：CS上升沿（释放）时处理已收集的命令数据
2. `TRANSFER`：同上处理
3. `DATA` + `cs_was_released`：
   - 第一个字节：MOSI=命令字节，MISO=STATUS寄存器
   - 后续字节：收集到mb列表
   - 超过max字节数：警告"excess byte"

**命令解析**（parse_command）：

| 命令字节模式 | 命令名 | dat | min | max |
|-------------|--------|-----|-----|-----|
| `b & 0xe0 == 0x00` | R_REGISTER | b & 0x1f | 1 | reg_size |
| `b & 0xe0 == 0x20` | W_REGISTER | b & 0x1f | 1 | reg_size |
| `0x50` | ACTIVATE | None | 1 | 1 |
| `0x61` | R_RX_PAYLOAD | None | 1 | 32 |
| `0x60` | R_RX_PL_WID | None | 1 | 1 |
| `0xA0` | W_TX_PAYLOAD | None | 1 | 32 |
| `0xB0` | W_TX_PAYLOAD_NOACK | None | 1 | 32 |
| `(b & 0xF8) == 0xA8` | W_ACK_PAYLOAD | b & 0x07 | 1 | 32 |
| `0xE1` | FLUSH_TX | None | 0 | 0 |
| `0xE2` | FLUSH_RX | None | 0 | 0 |
| `0xE3` | REUSE_TX_PL | None | 0 | 0 |
| `0xFF` | NOP | None | 0 | 0 |

**xn297扩展命令**：
| `0xFD` | CE_FSPI_ON | None | 1 | 1 |
| `0xFC` | CE_FSPI_OFF | None | 1 | 1 |
| `0x53` | RST_FSPI | None | 1 | 1 |

**寄存器表**（regs字典）：
```c
static const struct { uint8_t addr; const char *name; int size; } nrf24l01_regs[] = {
    {0x00, "CONFIG", 1}, {0x01, "EN_AA", 1}, {0x02, "EN_RXADDR", 1},
    {0x03, "SETUP_AW", 1}, {0x04, "SETUP_RETR", 1}, {0x05, "RF_CH", 1},
    {0x06, "RF_SETUP", 1}, {0x07, "STATUS", 1}, {0x08, "OBSERVE_TX", 1},
    {0x09, "RPD", 1}, {0x0a, "RX_ADDR_P0", 5}, {0x0b, "RX_ADDR_P1", 5},
    {0x0c, "RX_ADDR_P2", 1}, {0x0d, "RX_ADDR_P3", 1}, {0x0e, "RX_ADDR_P4", 1},
    {0x0f, "RX_ADDR_P5", 1}, {0x10, "TX_ADDR", 5}, {0x11, "RX_PW_P0", 1},
    {0x12, "RX_PW_P1", 1}, {0x13, "RX_PW_P2", 1}, {0x14, "RX_PW_P3", 1},
    {0x15, "RX_PW_P4", 1}, {0x16, "RX_PW_P5", 1}, {0x17, "FIFO_STATUS", 1},
    {0x1c, "DYNPD", 1}, {0x1d, "FEATURE", 1},
};
// xn297扩展
static const struct { uint8_t addr; const char *name; int size; } xn297_regs[] = {
    {0x19, "DEMOD_CAL", 1}, {0x1a, "RF_CAL2", 6}, {0x1b, "DEM_CAL2", 3},
    {0x1e, "RF_CAL", 3}, {0x1f, "BB_CAL", 5},
};
```

**finish_command处理**：
- R_REGISTER → 解码MISO字节为寄存器值
- W_REGISTER → 解码MOSI字节为寄存器值，合并命令和数据
- R_RX_PAYLOAD → 解码MISO字节为RX payload
- W_TX_PAYLOAD / W_TX_PAYLOAD_NOACK → 解码MOSI字节为TX payload
- W_ACK_PAYLOAD → 解码MOSI字节为ACK payload for pipe N
- R_RX_PL_WID → MISO第一字节为payload宽度
- ACTIVATE → 检查MOSI字节0x8c/0x73
- RST_FSPI → 检查MOSI字节0x5a/0xa5

**数据格式化**（decode_mb_data）：
- always_hex=True时：每个字节格式化为`%02X`
- always_hex=False时：可打印字符直接显示，不可打印用`\x%02X`
- 输出格式：`label = "{$}"` + `@data`（使用C_ANN_PUT_TYPE双行格式）

#### 3.2.3 C实现规划

**文件**：`nrf24l01_c.c`

**状态结构体**：
```c
#define NRF24_MAX_CMD_BYTES 64

typedef struct {
    int out_ann;
    int chip_type; // 0=nrf24l01, 1=xn297

    // 命令状态
    int first;        // 是否是第一个字节
    int cs_was_released;
    char cmd[32];     // 当前命令名
    int dat;          // 命令附加数据
    int min_bytes;    // 最少后续字节数
    int max_bytes;    // 最多后续字节数

    // 收集的字节
    uint8_t mosi_bytes[NRF24_MAX_CMD_BYTES];
    uint8_t miso_bytes[NRF24_MAX_CMD_BYTES];
    int num_bytes;
    uint64_t mb_ss;   // 第一个数据字节起始
    uint64_t mb_es;   // 最后一个数据字节结束
    uint64_t cmd_ss;  // 命令字节起始
    uint64_t cmd_es;  // 命令字节结束
} nrf24l01_state;
```

**recv_proto逻辑伪代码**：
```
CS-CHANGE → 如果CS释放(上升沿), 处理已收集命令, reset状态
TRANSFER  → 同上
DATA      → 如果cs_was_released:
              如果first: 解析命令字节(MOSI), 解码STATUS寄存器(MISO)
              否则: 收集字节到mosi/miso数组
```

**关键C代码片段** — 命令解析：
```c
static int nrf24l01_parse_command(nrf24l01_state *s, uint8_t b)
{
    int buflen = (s->chip_type == 1) ? 64 : 32;

    if ((b & 0xe0) == 0x00 || (b & 0xe0) == 0x20) {
        snprintf(s->cmd, sizeof(s->cmd), "%s",
                 (b & 0xe0) == 0x00 ? "R_REGISTER" : "W_REGISTER");
        s->dat = b & 0x1f;
        int m = nrf24l01_get_reg_size(s, s->dat);
        s->min_bytes = 1;
        s->max_bytes = m;
        return 0;
    }
    if (b == 0x50) { snprintf(s->cmd, sizeof(s->cmd), "ACTIVATE"); s->dat = -1; s->min_bytes = 1; s->max_bytes = 1; return 0; }
    if (b == 0x61) { snprintf(s->cmd, sizeof(s->cmd), "R_RX_PAYLOAD"); s->dat = -1; s->min_bytes = 1; s->max_bytes = buflen; return 0; }
    // ... 其余命令
    return -1; // unknown
}
```

**关键C代码片段** — 寄存器格式化：
```c
static void nrf24l01_decode_mb_data(struct srd_decoder_inst *di, nrf24l01_state *s,
    uint64_t ss, uint64_t es, int ann, const uint8_t *data, int len,
    const char *label, int always_hex)
{
    char data_str[256];
    int pos = 0;
    for (int i = 0; i < len && pos < (int)sizeof(data_str) - 4; i++) {
        if (always_hex) {
            pos += snprintf(data_str + pos, sizeof(data_str) - pos, "%02X", data[i]);
        } else {
            if (data[i] >= 32 && data[i] <= 126) {
                pos += snprintf(data_str + pos, sizeof(data_str) - pos, "%c", data[i]);
            } else {
                pos += snprintf(data_str + pos, sizeof(data_str) - pos, "\\x%02X", data[i]);
            }
        }
    }
    // 使用双行annotation格式: [label = "{$}", '@' + data]
    char long_str[512], short_str[256];
    snprintf(long_str, sizeof(long_str), "%s = \"{$}\"", label);
    snprintf(short_str, sizeof(short_str), "@%s", data_str);
    C_ANN_PUT(di, ss, es, s->out_ann, ann, long_str, short_str);
}
```

**注意**：Python中`decode_mb_data`使用`self.putp_ann(pos, ann, [label + ' = "{$}"', '@' + data.strip()])`输出双行annotation。C中对应使用`C_ANN_PUT`传入两个文本参数（long和short）。

---

### 3.3 nrf905 — Nordic nRF905 无线收发器

#### 3.3.1 Python元数据

| 属性 | 值 |
|------|-----|
| id | `nrf905` |
| name | `nRF905` |
| longname | `Nordic Semiconductor nRF905` |
| desc | `433/868/933MHz transceiver chip.` |
| inputs | `['spi']` |
| outputs | `[]` |
| tags | `['IC', 'Wireless/RF']` |
| license | `mit` |

**Options**: 无

**Annotations**:
| index | id | name |
|-------|----|------|
| 0 (CMD) | cmd | Command sent to the device |
| 1 (REG_WR) | reg-write | Config register written to the device |
| 2 (REG_RD) | reg-read | Config register read from the device |
| 3 (TX) | tx-data | Payload sent to the device |
| 4 (RX) | rx-data | Payload read from the device |
| 5 (RESP) | resp | Response to commands received from the device |
| 6 (WARN) | warning | Warning |

**Annotation rows**:
| id | name | classes |
|----|------|---------|
| commands | Commands | (0,) |
| responses | Responses | (5,) |
| registers | Registers | (1, 2) |
| tx | Transmitted data | (3,) |
| rx | Received data | (4,) |
| warnings | Warnings | (6,) |

#### 3.3.2 Python解码逻辑分析

**核心状态**：
- `self.mosi_bytes` — 收集的(mosi值, ss, es)元组列表
- `self.miso_bytes` — 收集的(miso值, ss, es)元组列表
- `self.cs_asserted` — CS是否被assert

**处理流程**：
1. `CS-CHANGE`：CS下降沿(assert) → 记录起始；CS上升沿(deassert) → 处理命令
2. `DATA`：仅当CS被assert时收集字节

**命令处理**（process_cmd）：

| 命令字节模式 | 命令名 | 处理函数 |
|-------------|--------|----------|
| `(cmd & 0xF0) == 0x00` | W_CONFIG (WC) | handle_WC |
| `(cmd & 0xF0) == 0x10` | R_CONFIG (RC) | handle_RC |
| `cmd == 0x20` | W_TX_PAYLOAD (WTP) | handle_WTP |
| `cmd == 0x21` | R_TX_PAYLOAD (RTP) | handle_RTP |
| `cmd == 0x22` | W_TX_ADDRESS (WTA) | handle_WTA |
| `cmd == 0x23` | R_TX_ADDRESS (RTA) | handle_RTA |
| `cmd == 0x24` | R_RX_PAYLOAD (RRP) | handle_RRP |
| `(cmd & 0xF0) == 0x80` | CHANNEL_CONFIG (CC) | handle_CC |

**每个命令处理**：
- 先输出命令名annotation（ANN_CMD）
- 再处理STATUS字节（MISO第一字节，ANN_REG_RD）
- 然后处理命令特有数据

**配置寄存器解析**（CFG_REGS字典）：
- 地址0-9，每个寄存器包含多个字段
- 每个字段有name, stbit(起始位), nbits(位数), opts(可选值映射)
- `extract_bits(byte, start_bit, num_bits)` — 从字节中提取指定位
- `extract_vars(reg_vars, reg_value)` — 遍历字段，拼接名称=值(含义)

**CHANNEL_CONFIG特殊处理**：
- 命令字节低1位 + 数据字节 = 9位通道号
- 命令字节包含PA_PWR和HFREQ_PLL字段

#### 3.3.3 C实现规划

**文件**：`nrf905_c.c`

**状态结构体**：
```c
#define NRF905_MAX_BYTES 64

typedef struct {
    int out_ann;
    int cs_asserted;
    uint64_t cmd_ss;
    uint64_t cmd_es;

    uint8_t mosi_bytes[NRF905_MAX_BYTES];
    uint64_t mosi_ss[NRF905_MAX_BYTES];
    uint64_t mosi_es[NRF905_MAX_BYTES];
    uint8_t miso_bytes[NRF905_MAX_BYTES];
    uint64_t miso_ss[NRF905_MAX_BYTES];
    uint64_t miso_es[NRF905_MAX_BYTES];
    int num_bytes;
} nrf905_state;
```

**关键C代码片段** — 配置寄存器字段解析：
```c
typedef struct {
    const char *name;
    int stbit;
    int nbits;
    const char *opts[8]; // 最多8个可选值，NULL结尾
} nrf905_reg_field;

static const nrf905_reg_field cfg_reg_0[] = {
    {"CH_NO", 7, 8, {NULL}},
    {NULL} // 终止符
};

static const nrf905_reg_field cfg_reg_1[] = {
    {"AUTO_RETRAN", 5, 1, {"No retransmission", "Retransmission of data packet", NULL}},
    {"RX_RED_PWR", 4, 1, {"Normal operation", "Reduced power", NULL}},
    {"PA_PWR", 3, 2, {"-10 dBm", "-2 dBm", "+6 dBm", "+10 dBm", NULL}},
    {"HFREQ_PLL", 1, 1, {"433 MHz", "868 / 915 MHz", NULL}},
    {"CH_NO_8", 0, 1, {NULL}},
    {NULL}
};
// ... 类似定义cfg_reg_2到cfg_reg_9

static int nrf905_extract_bits(uint8_t byte, int start_bit, int num_bits)
{
    int begin = 7 - start_bit;
    int end = begin + num_bits;
    if (begin < 0 || end > 8) return 0;
    return (byte >> (8 - end)) & ((1 << num_bits) - 1);
}

static void nrf905_extract_vars(struct srd_decoder_inst *di, nrf905_state *s,
    const nrf905_reg_field *fields, uint8_t reg_value,
    uint64_t ss, uint64_t es, int ann)
{
    char buf[512];
    int pos = 0;
    for (int i = 0; fields[i].name != NULL; i++) {
        int val = nrf905_extract_bits(reg_value, fields[i].stbit, fields[i].nbits);
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s = %d", fields[i].name, val);
        if (fields[i].opts[0] != NULL && val < 8 && fields[i].opts[val] != NULL) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, " (%s)", fields[i].opts[val]);
        }
        if (fields[i + 1].name != NULL) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, " | ");
        }
    }
    C_ANN_PUT(di, ss, es, s->out_ann, ann, buf);
}
```

**recv_proto逻辑**：
```c
static void nrf905_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    nrf905_state *s = (nrf905_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "CS-CHANGE") == 0) {
        if (data_len >= 2) {
            int old_val = data[0];
            int new_val = data[1];
            if (old_val == 0xFF && new_val == 0) {
                // 首次CS assert
                s->cs_asserted = 1;
                s->cmd_ss = start_sample;
                s->num_bytes = 0;
            } else if (old_val == 1 && new_val == 0) {
                // CS assert (下降沿)
                s->cs_asserted = 1;
                s->cmd_ss = start_sample;
                s->num_bytes = 0;
            } else if (old_val == 0 && new_val == 1) {
                // CS deassert (上升沿) → 处理命令
                s->cmd_es = start_sample;
                if (s->num_bytes > 0) {
                    nrf905_process_cmd(di, s);
                }
                s->cs_asserted = 0;
                s->num_bytes = 0;
            }
        }
    } else if (strcmp(cmd, "DATA") == 0 && s->cs_asserted && data_len >= 17) {
        int have_mosi = data[0] & 1;
        int have_miso = (data[0] >> 1) & 1;
        uint64_t mosi_val = 0, miso_val = 0;
        for (int i = 0; i < 8; i++)
            mosi_val |= ((uint64_t)data[1 + i] << (8 * i));
        for (int i = 0; i < 8; i++)
            miso_val |= ((uint64_t)data[9 + i] << (8 * i));

        if (s->num_bytes < NRF905_MAX_BYTES) {
            s->mosi_bytes[s->num_bytes] = (uint8_t)mosi_val;
            s->mosi_ss[s->num_bytes] = start_sample;
            s->mosi_es[s->num_bytes] = end_sample;
            s->miso_bytes[s->num_bytes] = (uint8_t)miso_val;
            s->miso_ss[s->num_bytes] = start_sample;
            s->miso_es[s->num_bytes] = end_sample;
            s->num_bytes++;
        }
    }
}
```

---

### 3.4 rfm12 — HopeRF RFM12 无线收发器

#### 3.4.1 Python元数据

| 属性 | 值 |
|------|-----|
| id | `rfm12` |
| name | `RFM12` |
| longname | `HopeRF RFM12` |
| desc | `HopeRF RFM12 wireless transceiver control protocol.` |
| inputs | `['spi']` |
| outputs | `[]` |
| tags | `['Wireless/RF']` |
| license | `gplv2+` |

**Options**: 无

**Annotations**:
| index | id | name |
|-------|----|------|
| 0 | cmd | Command |
| 1 | params | Command parameters |
| 2 | disabled | Disabled bits |
| 3 | return | Returned values |
| 4 | disabled_return | Disabled returned values |
| 5 | interpretation | Interpretation |

**Annotation rows**:
| id | name | classes |
|----|------|---------|
| commands | Commands | (0, 1, 2) |
| return | Return | (3, 4) |
| interpretation | Interpretation | (5,) |

#### 3.4.2 Python解码逻辑分析

**核心特点**：
- 所有命令由2字节组成
- 同时处理BITS和DATA两种ptype
- BITS用于位级标注（每个bit的ss/es），DATA用于字节值
- 使用`row_pos`数组跟踪每个annotation_row的当前位置
- 使用`ann_to_row`映射annotation class到row索引

**命令分发**（handle_cmd）：

| 命令字节 | 命令处理函数 |
|---------|-------------|
| `0x80` | handle_configuration_cmd |
| `0x82` | handle_power_management_cmd |
| `cmd[0] & 0xF0 == 0xA0` | handle_frequency_setting_cmd |
| `0xC6` | handle_data_rate_cmd |
| `cmd[0] & 0xF8 == 0x90` | handle_receiver_control_cmd |
| `0xC2` | handle_data_filter_cmd |
| `0xCA` | handle_fifo_and_reset_cmd |
| `0xCE` | handle_synchron_pattern_cmd |
| `0xB0` | handle_fifo_read_cmd |
| `0xC4` | handle_afc_cmd |
| `cmd[0] & 0xFE == 0x98` | handle_transceiver_control_cmd |
| `0xCC` | handle_pll_setting_cmd |
| `0xB8` | handle_transmitter_register_cmd |
| `0xFE` | handle_software_reset_cmd |
| `cmd[0] & 0xE0 == 0xE0` | handle_wake_up_timer_cmd |
| `0xC8` | handle_low_duty_cycle_cmd |
| `0xC0` | handle_low_battery_detector_cmd |
| `0x00` | handle_status_read_cmd |

**位级标注机制**：
- `putx(ann, length, description)` — 在当前row_pos位置标注length个bit
- `describe_bits(data, names)` — 根据bit值标注为params(1)或disabled(0)
- `describe_return_bits(data, names)` — 根据bit值标注为return(1)或disabled_return(0)
- `describe_changed_bits(data, old_data, names)` — 标注变化位为interpretation
- `advance_ann(ann, length)` — 推进row_pos但不标注

**状态跟踪**：
- `last_status`, `last_config`, `last_power`, `last_freq`, `last_data_rate`, `last_fifo_and_reset`, `last_afc`, `last_transceiver`, `last_pll`

#### 3.4.3 C实现规划

**文件**：`rfm12_c.c`

**复杂度评估**：**高** — 这是最复杂的解码器，有17种命令处理，大量位级标注和状态跟踪。

**状态结构体**：
```c
#define RFM12_MAX_BYTES 2

typedef struct {
    int out_ann;

    // 收集的字节
    uint8_t mosi_bytes[RFM12_MAX_BYTES];
    uint8_t miso_bytes[RFM12_MAX_BYTES];
    int num_bytes;

    // 位级标注位置跟踪
    int row_pos[3]; // 3个annotation_row的位置
    int ann_to_row[6]; // annotation class到row的映射

    // 状态跟踪
    uint8_t last_status[2];
    uint8_t last_config;
    uint8_t last_power;
    uint16_t last_freq;
    uint8_t last_data_rate;
    uint8_t last_fifo_and_reset;
    uint8_t last_afc;
    uint8_t last_transceiver;
    uint8_t last_pll;

    // 位级数据（BITS ptype）
    uint8_t mosi_bits[16];
    int mosi_bit_ss[16];
    int mosi_bit_es[16];
    int num_mosi_bits;
    uint8_t miso_bits[16];
    int miso_bit_ss[16];
    int miso_bit_es[16];
    int num_miso_bits;
} rfm12_state;
```

**关键简化策略**：
- Python版本使用BITS ptype做位级标注，C版本中SPI的BITS输出格式为BITS v2（per-bit ss/es时间戳，详见c_decoder_utils.h） <!-- Updated: BITS格式已升级为v2 -->
- 由于RFM12的位级标注极其复杂（涉及row_pos跟踪），**建议简化**：只做字节级annotation，不做位级annotation。这样可大幅降低复杂度，同时保留核心功能。
- 如果必须保留位级标注，需要在recv_proto中处理BITS命令，并维护row_pos状态。

**recv_proto逻辑**：
```c
static void rfm12_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    rfm12_state *s = (rfm12_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") == 0 && data_len >= 17) {
        int have_mosi = data[0] & 1;
        int have_miso = (data[0] >> 1) & 1;
        uint64_t mosi_val = 0, miso_val = 0;
        for (int i = 0; i < 8; i++)
            mosi_val |= ((uint64_t)data[1 + i] << (8 * i));
        for (int i = 0; i < 8; i++)
            miso_val |= ((uint64_t)data[9 + i] << (8 * i));

        if (s->num_bytes < RFM12_MAX_BYTES) {
            s->mosi_bytes[s->num_bytes] = (uint8_t)mosi_val;
            s->miso_bytes[s->num_bytes] = (uint8_t)miso_val;
            s->num_bytes++;
        }

        if (s->num_bytes >= 2) {
            s->row_pos[0] = 0; s->row_pos[1] = 8; s->row_pos[2] = 8;
            rfm12_handle_cmd(di, s, start_sample, end_sample);
            s->num_bytes = 0;
        }
    }
}
```

**命令处理示例** — Configuration command：
```c
static void rfm12_handle_configuration_cmd(struct srd_decoder_inst *di,
    rfm12_state *s, uint64_t ss, uint64_t es)
{
    C_ANN_PUT(di, ss, es, s->out_ann, ANN_CMD, "Configuration command", "Configuration");

    uint8_t cmd1 = s->mosi_bytes[1];
    // Frequency
    const char *freqs[] = {"315", "433", "868", "915"};
    int freq_idx = (cmd1 & 0x30) >> 4;
    char buf[64];
    snprintf(buf, sizeof(buf), "Frequency: %sMHz", freqs[freq_idx]);
    C_ANN_PUT(di, ss, es, s->out_ann, ANN_PARAMS, buf);

    // Capacitance
    double cap = 8.5 + (cmd1 & 0xF) * 0.5;
    snprintf(buf, sizeof(buf), "Capacitance: %.1fpF", cap);
    C_ANN_PUT(di, ss, es, s->out_ann, ANN_PARAMS, buf);

    // Changed interpretation
    if ((cmd1 & 0x30) != (s->last_config & 0x30)) {
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_INTERPRETATION, "Changed", "~");
    }
    if ((cmd1 & 0xF) != (s->last_config & 0xF)) {
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_INTERPRETATION, "Changed", "~");
    }

    s->last_config = cmd1;
}
```

---

### 3.5 ssi32 — 同步串行接口(32位)

#### 3.5.1 Python元数据

| 属性 | 值 |
|------|-----|
| id | `ssi32` |
| name | `SSI32` |
| longname | `Synchronous Serial Interface (32bit)` |
| desc | `Synchronous Serial Interface (32bit) protocol.` |
| inputs | `['spi']` |
| outputs | `[]` |
| tags | `['Embedded/industrial']` |
| license | `gplv2+` |

**Options**:
| id | desc | default | idn |
|----|------|---------|-----|
| msgsize | Message size | 64 | dec_ssi32_opt_msgsize |

**Annotations**:
| index | id | name |
|-------|----|------|
| 0 | ctrl-tx | CTRL TX |
| 1 | ack-tx | ACK TX |
| 2 | ctrl-rx | CTRL RX |
| 3 | ack-rx | ACK RX |

**Annotation rows**:
| id | name | classes |
|----|------|---------|
| tx | TX | (0, 1) |
| rx | RX | (2, 3) |

#### 3.5.2 Python解码逻辑分析

**核心逻辑**：
- `CS-CHANGE` → reset数据
- `DATA` → 收集MOSI/MISO字节
- 第一个字节的最高位决定帧类型：
  - bit7=1 → ACK帧（4字节），调用`handle_ack()`
  - bit7=0 → CTRL帧（msgsize字节），调用`handle_ctrl()`

**ACK帧处理**（handle_ack）：
- 仅使用前4字节
- MOSI第一字节 → `> ACK:0x%02x`
- MISO第一字节 → `< ACK:0x%02x`

**CTRL帧处理**（handle_ctrl）：
- MOSI字节0=CTRL, 字节1=LUN, 字节2=SIZE, 字节3=CRC, 字节4..=DATA
- MISO字节0=CTRL, 字节1=LUN, 字节2=SIZE, 字节3=CRC, 字节4..=DATA
- tx_size = mosi_bytes[2], rx_size = miso_bytes[2]
- 输出格式：`> CTRL:0x%02x, LUN:0x%02x, SIZE:0x%02x, CRC:0x%02x, DATA:0x...`

#### 3.5.3 C实现规划

**文件**：`ssi32_c.c`

**状态结构体**：
```c
#define SSI32_MAX_BYTES 128

typedef struct {
    int out_ann;
    int msgsize;

    uint8_t mosi_bytes[SSI32_MAX_BYTES];
    uint8_t miso_bytes[SSI32_MAX_BYTES];
    uint64_t es_array[SSI32_MAX_BYTES]; // 每个字节的结束sample
    int num_bytes;
    uint64_t ss_cmd; // 第一个字节的起始sample
} ssi32_state;
```

**recv_proto逻辑**：
```c
static void ssi32_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ssi32_state *s = (ssi32_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "CS-CHANGE") == 0) {
        // CS变化时重置数据
        s->num_bytes = 0;
        return;
    }

    if (strcmp(cmd, "DATA") != 0 || data_len < 17) return;

    int have_mosi = data[0] & 1;
    int have_miso = (data[0] >> 1) & 1;
    uint64_t mosi_val = 0, miso_val = 0;
    for (int i = 0; i < 8; i++)
        mosi_val |= ((uint64_t)data[1 + i] << (8 * i));
    for (int i = 0; i < 8; i++)
        miso_val |= ((uint64_t)data[9 + i] << (8 * i));

    if (s->num_bytes == 0) s->ss_cmd = start_sample;
    if (s->num_bytes < SSI32_MAX_BYTES) {
        s->mosi_bytes[s->num_bytes] = (uint8_t)mosi_val;
        s->miso_bytes[s->num_bytes] = (uint8_t)miso_val;
        s->es_array[s->num_bytes] = end_sample;
        s->num_bytes++;
    }

    // 判断帧类型
    if (s->mosi_bytes[0] & 0x80) {
        // ACK帧：4字节
        if (s->num_bytes < 4) return;
        ssi32_handle_ack(di, s);
        s->num_bytes = 0;
    } else {
        // CTRL帧：msgsize字节
        if (s->num_bytes < s->msgsize) return;
        ssi32_handle_ctrl(di, s);
        s->num_bytes = 0;
    }
}
```

**ACK处理**：
```c
static void ssi32_handle_ack(struct srd_decoder_inst *di, ssi32_state *s)
{
    char buf[64];
    uint64_t es = s->es_array[0];

    snprintf(buf, sizeof(buf), "> ACK:0x%02x", s->mosi_bytes[0]);
    C_ANN_PUT(di, s->ss_cmd, es, s->out_ann, ANN_ACK_TX, buf);

    snprintf(buf, sizeof(buf), "< ACK:0x%02x", s->miso_bytes[0]);
    C_ANN_PUT(di, s->ss_cmd, es, s->out_ann, ANN_ACK_RX, buf);
}
```

**CTRL处理**：
```c
static void ssi32_handle_ctrl(struct srd_decoder_inst *di, ssi32_state *s)
{
    int tx_size = s->mosi_bytes[2];
    int rx_size = s->miso_bytes[2];
    char buf[512];
    int pos;

    // TX CTRL
    pos = snprintf(buf, sizeof(buf), "> CTRL:0x%02x, LUN:0x%02x, SIZE:0x%02x, CRC:0x%02x",
                   s->mosi_bytes[0], s->mosi_bytes[1], s->mosi_bytes[2], s->mosi_bytes[3]);
    if (tx_size > 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", DATA:0x");
        for (int i = 4; i < tx_size + 4 && i < s->num_bytes; i++)
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%02x", s->mosi_bytes[i]);
    }
    uint64_t tx_es = s->es_array[(tx_size + 3 < s->num_bytes) ? tx_size + 3 : s->num_bytes - 1];
    C_ANN_PUT(di, s->ss_cmd, tx_es, s->out_ann, ANN_CTRL_TX, buf);

    // RX CTRL
    pos = snprintf(buf, sizeof(buf), "< CTRL:0x%02x, LUN:0x%02x, SIZE:0x%02x, CRC:0x%02x",
                   s->miso_bytes[0], s->miso_bytes[1], s->miso_bytes[2], s->miso_bytes[3]);
    if (rx_size > 0) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, ", DATA:0x");
        for (int i = 4; i < rx_size + 4 && i < s->num_bytes; i++)
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%02x", s->miso_bytes[i]);
    }
    uint64_t rx_es = s->es_array[(rx_size + 3 < s->num_bytes) ? rx_size + 3 : s->num_bytes - 1];
    C_ANN_PUT(di, s->ss_cmd, rx_es, s->out_ann, ANN_CTRL_RX, buf);
}
```

---

## 4. 构建集成

### 4.1 CMakeLists.txt修改

在`C_DECODERS`列表中添加5个新解码器：

```cmake
# 在CMakeLists.txt中找到set(C_DECODERS ...)行，添加：
nes_gamepad_c
nrf24l01_c
nrf905_c
rfm12_c
ssi32_c
```

### 4.2 构建命令

```bash
build_incremental.cmd
```

输出DLL位置：`build.dir/decoders/c_decoders/`

---

## 5. 复杂度评估与实现优先级

| 解码器 | 复杂度 | 预估代码行数 | 优先级 | 说明 |
|--------|--------|-------------|--------|------|
| nes_gamepad | ★☆☆☆☆ | ~150行 | P0 | 最简单，仅DATA处理，无状态机 |
| ssi32 | ★★☆☆☆ | ~250行 | P1 | 简单状态机，CS-CHANGE+DATA |
| nrf24l01 | ★★★★☆ | ~600行 | P2 | 复杂命令解析，寄存器表，xn297扩展 |
| nrf905 | ★★★☆☆ | ~450行 | P3 | 配置寄存器字段解析，中等复杂 |
| rfm12 | ★★★★★ | ~800行 | P4 | 最复杂，17种命令，位级标注，状态跟踪 |

---

## 6. 通用工具函数

以下函数可被多个解码器共享（在每个.c文件中独立实现）：

```c
// 从SPI DATA命令的data中提取MOSI/MISO值
static void spi_parse_data(const unsigned char *data, uint64_t data_len,
    uint64_t *mosi_val, uint64_t *miso_val, int *have_mosi, int *have_miso)
{
    *mosi_val = 0; *miso_val = 0; *have_mosi = 0; *have_miso = 0;
    if (data_len < 17) return;
    *have_mosi = data[0] & 1;
    *have_miso = (data[0] >> 1) & 1;
    for (int i = 0; i < 8; i++)
        *mosi_val |= ((uint64_t)data[1 + i] << (8 * i));
    for (int i = 0; i < 8; i++)
        *miso_val |= ((uint64_t)data[9 + i] << (8 * i));
}

// 格式化字节数组为十六进制字符串
static int format_bytes_hex(const uint8_t *bytes, int len, int reversed,
    char *out, int out_size)
{
    int pos = 0;
    if (reversed) {
        for (int i = len - 1; i >= 0 && pos < out_size - 3; i--)
            pos += snprintf(out + pos, out_size - pos, "%02X", bytes[i]);
    } else {
        for (int i = 0; i < len && pos < out_size - 3; i++)
            pos += snprintf(out + pos, out_size - pos, "%02X", bytes[i]);
    }
    return pos;
}
```
