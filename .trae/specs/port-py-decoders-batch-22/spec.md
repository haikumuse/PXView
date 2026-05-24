# Python → C 解码器移植规格书 — Batch 22

> 目标：将 5 个 I2C 上层 Python 解码器移植为 C 实现
> 解码器列表：xfp, hdcp, hdmi_scdc, tca6408a, tmp102
> 所有解码器均为 I2C 上层协议（`inputs=['i2c']`），使用 `recv_proto()` 回调

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| i2c_c.c | 底层协议输出范本 | START/STOP条件检测、c_decoder_put_python()输出I2C协议数据、BITS v2格式 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机、7位地址检查 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |

---

## 1. 总体架构

### 1.1 I2C 上层解码器的 C 实现模式

I2C 上层解码器**不直接处理原始信号**，而是接收来自 `i2c_c` 解码器的协议数据。因此在 C 实现中：

- **不实现 `decode()` 回调**（留空即可）
- **实现 `recv_proto()` 回调**，接收 i2c 解码器发出的协议包
- 在 `start()` 中调用 `c_decoder_register_output(di, SRD_OUTPUT_ANN, "xxx")` 注册输出
- 不需要 `channels` / `optional_channels`（信号通道由下层 i2c 解码器处理）

### 1.2 recv_proto() 函数签名

```c
static void xxx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

参数说明：
- `di`：解码器实例
- `start_sample` / `end_sample`：当前 I2C 包的采样范围
- `cmd`：I2C 命令字符串，可能的值：
  - `"START"` / `"START REPEAT"` / `"STOP"`
  - `"ADDRESS READ"` / `"ADDRESS WRITE"`
  - `"DATA READ"` / `"DATA WRITE"`
  - `"ACK"` / `"NACK"`
  - `"BITS"`
- `data`：数据字节缓冲区。**注意**：I2C C 解码器默认使用 `address_format=shifted`，因此 ADDRESS WRITE/READ 的 data 为 **7 位地址**（如 0x20 而非 0x40）。上层解码器地址检查应使用7位地址 <!-- Updated: I2C C 解码器默认 address_shifted=1，发送7位地址 -->
- `data_len`：数据长度
- **BITS v2 格式**：BITS 消息现已包含 per-bit 时间戳，格式为 `data[0]=flags, data[1]=mosi_count, data[2..]=per-bit [value(1B)][ss(8B LE)][es(8B LE)], 0x00, miso_count, ...`。I2C 上层解码器通常忽略 BITS <!-- Updated: BITS v2 格式已实现 -->

### 1.3 状态机模式

所有 I2C 上层解码器遵循统一的状态机模式：

```
IDLE → GET_SLAVE_ADDR → GET_REG_ADDR → READ_REGS / WRITE_REGS → IDLE
```

在 `recv_proto()` 中使用 `strcmp(cmd, ...)` 匹配命令，驱动状态转换。

### 1.4 C 解码器标准结构

```c
// 1. 头文件
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

// 2. 注解枚举
enum { ANN_XXX = 0, ..., NUM_ANN };

// 3. 状态枚举
enum xxx_state { XXX_IDLE, XXX_GET_SLAVE_ADDR, ... };

// 4. 私有数据结构
typedef struct {
    enum xxx_state state;
    // ... 其他字段
    int out_ann;
} xxx_state;

// 5. 输入/标签/选项声明
static const char *xxx_inputs[] = {"i2c", NULL};
static const char *xxx_tags[] = {..., NULL};

// 6. 注解标签（第一列必须为 ""）
static const char *xxx_ann_labels[][3] = {
    {"", "ann-id", "Description"},
    ...
};

// 7. 注解行
static const int xxx_row_xxx_classes[] = {...};
static const struct srd_c_ann_row xxx_ann_rows[] = {...};

// 8. 辅助函数
// 9. recv_proto() 实现
// 10. reset/start/decode/destroy 回调
// 11. srd_c_decoder 结构体
// 12. 导出入口函数
```

---

## 2. 解码器详细规格

---

### 2.1 XFP 解码器

#### 2.1.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `xfp` |
| name | `XFP` |
| longname | `10 Gigabit Small Form Factor Pluggable Module (XFP)` |
| desc | `XFP I²C management interface structures/protocol` |
| license | `gplv3+` |
| inputs | `['i2c']` |
| outputs | `[]` |
| tags | `['Networking']` |
| channels | 无 |
| optional_channels | 无 |
| options | 无 |
| binary | 无 |

#### 2.1.2 注解定义

Python 原始定义：
```python
annotations = (
    ('field-name-and-val', 'Field name and value'),  # 0
    ('field-val', 'Field value'),                     # 1
)
annotation_rows = (
    ('field-names-and-vals', 'Field names and values', (0,)),
    ('field-vals', 'Field values', (1,)),
)
```

C 映射：
```c
enum {
    ANN_FIELD_NAME_VAL = 0,
    ANN_FIELD_VAL = 1,
    NUM_ANN,
};

static const char *xfp_ann_labels[][3] = {
    {"", "field-name-and-val", "Field name and value"},
    {"", "field-val", "Field value"},
};

static const int xfp_row_name_val_classes[] = {ANN_FIELD_NAME_VAL};
static const int xfp_row_val_classes[] = {ANN_FIELD_VAL};
static const struct srd_c_ann_row xfp_ann_rows[] = {
    {"field-names-and-vals", "Field names and values", xfp_row_name_val_classes, 1},
    {"field-vals", "Field values", xfp_row_val_classes, 1},
};
```

#### 2.1.3 解码逻辑分析

**复杂度：★★★★★（最高）**

XFP 解码器的特点：
1. **只处理 DATA READ**：Python 版本仅处理 `cmd == 'DATA READ'`，忽略所有写操作
2. **顺序字节计数**：使用 `self.cnt` 从 0 递增，根据偏移量映射到不同的处理函数
3. **双内存映射**：
   - Lower Memory（0x00-0x7F）：128 字节标准结构
   - High Table 1（0x80-0xFF）：256 字节扩展结构（当 `cur_highmem_page == 0x01` 时）
4. **多字节字段**：许多字段跨越多个字节（如温度 2 字节、告警阈值 56 字节等）
5. **依赖外部模块**：使用 `common.plugtrx` 中的大量查找表（MODULE_ID, ALARM_THRESHOLDS, AD_READOUTS, GCS_BITS, CONNECTOR, TRANSCEIVER, SERIAL_ENCODING, XMIT_TECH, CDR, DEVICE_TECH, ENHANCED_OPTS, AUX_TYPES）。**注意**：这是 Python 辅助模块而非解码器依赖，C 版本需要将所有查找表内联为 C 数据结构，不涉及 C 解码器依赖规则问题 <!-- Updated: plugtrx 是 Python 辅助模块而非解码器，不阻塞 C 移植 -->

**状态机**（简化版，Python 原版无显式状态机）：
```
IDLE → (START) → 等待 DATA READ → 累积字节 → 根据偏移量调用处理函数
```

**C 实现策略**：
- 需要在 C 中内联所有 `plugtrx` 查找表
- 使用 `recv_proto()` 接收 I2C 协议数据
- 维护 `cnt` 计数器和 `buf` 缓冲区
- 实现 `MAP_LOWER_MEMORY` 和 `MAP_HIGH_TABLE_1` 对应的处理函数数组
- 使用 `sn` 数组记录每个字节的采样范围

#### 2.1.4 关键 C 代码片段

```c
enum xfp_state {
    XFP_IDLE,
    XFP_GET_SLAVE_ADDR,
    XFP_READ_REGS,
};

typedef struct {
    enum xfp_state state;
    int cnt;                    // 字节计数器
    uint8_t buf[256];           // 多字节缓冲区
    int buf_len;                // 缓冲区当前长度
    uint64_t sn[256][2];        // 每个字节的 [start, end] 采样
    int cur_highmem_page;       // 当前高内存页
    int have_clei;              // CLEI 标志
    uint64_t ss, es;
    int out_ann;
} xfp_state;

static void xfp_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    xfp_state *s = (xfp_state *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    if (s->state == XFP_IDLE) {
        if (strcmp(cmd, "START") == 0)
            s->state = XFP_GET_SLAVE_ADDR;
    } else if (s->state == XFP_GET_SLAVE_ADDR) {
        if (strcmp(cmd, "ADDRESS READ") == 0) {
            // XFP slave address = 0x50 (0xa0)
            s->state = XFP_READ_REGS;
        } else if (strcmp(cmd, "ADDRESS WRITE") == 0) {
            // 写操作用于设置寄存器偏移
            s->state = XFP_READ_REGS;
        }
    } else if (s->state == XFP_READ_REGS) {
        if (strcmp(cmd, "DATA READ") == 0) {
            uint8_t databyte = (data_len > 0) ? data[0] : 0;
            s->cnt++;
            s->sn[s->cnt][0] = start_sample;
            s->sn[s->cnt][1] = end_sample;
            s->buf[s->buf_len++] = databyte;

            // 检查是否到达某个处理边界
            if (s->cnt < 0x80) {
                xfp_handle_lower_memory(di, s);
            } else if (s->cnt < 0x100 && s->cur_highmem_page == 0x01) {
                xfp_handle_high_table_1(di, s);
            }
        } else if (strcmp(cmd, "STOP") == 0) {
            s->state = XFP_IDLE;
        }
    }
}
```

#### 2.1.5 移植难点

1. **plugtrx 查找表**：需要将所有 Python 字典转换为 C 结构体数组，工作量极大
2. **多字节字段处理**：需要精确管理缓冲区，在正确的偏移量触发处理
3. **温度/电流/功率换算**：需要实现 `to_temp()`, `to_current()`, `to_power()`, `to_wavelength()` 等换算函数
4. **ASCII 字段**：`maybe_ascii()`, `vendor()` 等需要处理 ASCII 解码
5. **建议简化**：C 版本可先实现核心字段解析，复杂查找表可后续补充

---

### 2.2 HDCP 解码器

#### 2.2.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `hdcp` |
| name | `HDCP` |
| longname | `HDCP over HDMI` |
| desc | `HDCP protocol over HDMI.` |
| license | `gplv2+` |
| inputs | `['i2c']` |
| outputs | `['hdcp']` |
| tags | `['PC', 'Security/crypto']` |
| channels | 无 |
| optional_channels | 无 |
| options | 无 |
| binary | 无 |

#### 2.2.2 注解定义

Python 原始定义：
```python
annotations = \
    tuple(('message-0x%02X' % i, 'Message 0x%02X' % i) for i in range(18)) + (
    ('summary', 'Summary'),       # 18
    ('warning', 'Warning'),       # 19
)
annotation_rows = (
    ('messages', 'Messages', tuple(range(18))),
    ('summaries', 'Summaries', (18,)),
    ('warnings', 'Warnings', (19,)),
)
```

C 映射（20 个注解类）：
```c
enum {
    ANN_MSG_0x00 = 0,  ANN_MSG_0x01,  ANN_MSG_0x02,  ANN_MSG_0x03,
    ANN_MSG_0x04,  ANN_MSG_0x05,  ANN_MSG_0x06,  ANN_MSG_0x07,
    ANN_MSG_0x08,  ANN_MSG_0x09,  ANN_MSG_0x0A,  ANN_MSG_0x0B,
    ANN_MSG_0x0C,  ANN_MSG_0x0D,  ANN_MSG_0x0E,  ANN_MSG_0x0F,
    ANN_MSG_0x10,  ANN_MSG_0x11,
    ANN_SUMMARY = 18,
    ANN_WARNING = 19,
    NUM_ANN,
};

static const char *hdcp_ann_labels[][3] = {
    {"", "message-0x00", "Message 0x00"},
    {"", "message-0x01", "Message 0x01"},
    // ... 0x02-0x11
    {"", "summary", "Summary"},
    {"", "warning", "Warning"},
};

// Messages 行包含 18 个注解类
static const int hdcp_row_messages_classes[] = {
    ANN_MSG_0x00, ANN_MSG_0x01, ..., ANN_MSG_0x11
};
static const int hdcp_row_summaries_classes[] = {ANN_SUMMARY};
static const int hdcp_row_warnings_classes[] = {ANN_WARNING};
static const struct srd_c_ann_row hdcp_ann_rows[] = {
    {"messages", "Messages", hdcp_row_messages_classes, 18},
    {"summaries", "Summaries", hdcp_row_summaries_classes, 1},
    {"warnings", "Warnings", hdcp_row_warnings_classes, 1},
};
```

#### 2.2.3 解码逻辑分析

**复杂度：★★★☆☆**

HDCP 解码器的特点：
1. **I2C 从地址 0x3A**：只处理地址为 0x3A 的 I2C 通信（7位地址，与 I2C C 解码器默认 shifted 格式一致）
2. **状态机**：`IDLE → GET_SLAVE_ADDR → WRITE_OFFSET → BUFFER_DATA`
3. **消息类型映射**：`msg_ids` 字典（2-17 映射到 HDCP 2.2 消息名称）
4. **写偏移映射**：`write_items` 字典（0x00-0x80 映射到 HDCP 1.4/2.x 寄存器名称）
5. **数据缓冲**：使用 `self.stack` 累积数据字节
6. **特殊处理**：
   - `RxStatus`：2 字节，解析 reauth/ready/length
   - `1.4 Bstatus`：2 字节，解析 device_count/depth/hdmi_mode
   - `Read_Message` / `Write_Message`：首字节为消息 ID
   - `HDCP2Version`：检查 bit2 判断是否 HDCP2

#### 2.2.4 关键 C 代码片段

```c
enum hdcp_state {
    HDCP_IDLE,
    HDCP_GET_SLAVE_ADDR,
    HDCP_WRITE_OFFSET,
    HDCP_BUFFER_DATA,
};

typedef struct {
    enum hdcp_state state;
    uint8_t stack[256];
    int stack_len;
    char type[64];          // 当前传输类型
    uint64_t ss, es;
    uint64_t ss_block, es_block;
    int out_ann;
} hdcp_state;

// HDCP 2.2 消息 ID 映射
static const struct { int id; const char *name; } hdcp_msg_ids[] = {
    {2,  "AKE_Init"},
    {3,  "AKE_Send_Cert"},
    {4,  "AKE_No_stored_km"},
    {5,  "AKE_Stored_km"},
    {7,  "AKE_Send_H_prime"},
    {8,  "AKE_Send_Pairing_Info"},
    {9,  "LC_Init"},
    {10, "LC_Send_L_prime"},
    {11, "SKE_Send_Eks"},
    {12, "RepeaterAuth_Send_ReceiverID_List"},
    {15, "RepeaterAuth_Send_Ack"},
    {16, "RepeaterAuth_Stream_Manage"},
    {17, "RepeaterAuth_Stream_Ready"},
    {-1, NULL},
};

// 写偏移映射
static const struct { int offset; const char *name; } hdcp_write_items[] = {
    {0x00, "1.4 Bksv - Receiver KSV"},
    {0x08, "1.4 Ri' - Link Verification"},
    // ... 完整列表
    {0x80, "Read_Message"},
    {-1, NULL},
};

static void hdcp_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    hdcp_state *s = (hdcp_state *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    if (s->state == HDCP_IDLE) {
        if (strcmp(cmd, "START") == 0) {
            s->stack_len = 0;
            s->type[0] = '\0';
            s->ss_block = start_sample;
            s->state = HDCP_GET_SLAVE_ADDR;
        } else if (strcmp(cmd, "START REPEAT") != 0) {
            return;
        }
        s->state = HDCP_GET_SLAVE_ADDR;
    } else if (s->state == HDCP_GET_SLAVE_ADDR) {
        if (strcmp(cmd, "ADDRESS READ") == 0) {
            uint8_t addr = (data_len > 0) ? data[0] : 0;
            if (addr != 0x3a) { s->state = HDCP_IDLE; return; }
            s->state = HDCP_BUFFER_DATA;
        } else if (strcmp(cmd, "ADDRESS WRITE") == 0) {
            uint8_t addr = (data_len > 0) ? data[0] : 0;
            if (addr != 0x3a) { s->state = HDCP_IDLE; return; }
            s->state = HDCP_WRITE_OFFSET;
        }
    } else if (s->state == HDCP_WRITE_OFFSET) {
        if (strcmp(cmd, "DATA WRITE") == 0) {
            uint8_t databyte = (data_len > 0) ? data[0] : 0;
            // 查找 write_items
            const char *type_name = hdcp_lookup_write_item(databyte);
            if (type_name) strncpy(s->type, type_name, sizeof(s->type) - 1);
            // 判断是否需要缓冲数据
            if (databyte == 0x10 || databyte == 0x15 || databyte == 0x18 || databyte == 0x60)
                s->state = HDCP_BUFFER_DATA;
            else if (s->type[0] != '\0')
                s->state = HDCP_IDLE;
        }
    } else if (s->state == HDCP_BUFFER_DATA) {
        if (strcmp(cmd, "STOP") == 0 || strcmp(cmd, "NACK") == 0) {
            s->es_block = end_sample;
            hdcp_process_buffer(di, s);
            s->state = HDCP_IDLE;
        } else if (strcmp(cmd, "DATA READ") == 0 || strcmp(cmd, "DATA WRITE") == 0) {
            uint8_t databyte = (data_len > 0) ? data[0] : 0;
            s->stack[s->stack_len++] = databyte;
        }
    }
}
```

#### 2.2.5 移植难点

1. **动态注解类**：Python 版本使用 `self.putb([msg, ...])` 其中 `msg` 是动态索引，C 版本需要确保消息 ID 在合法范围内
2. **write_items 查找表**：需要实现为 C 数组+线性搜索
3. **RxStatus/Bstatus 解析**：需要位操作和格式化输出

---

### 2.3 HDMI_SCDC 解码器

#### 2.3.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `hdmi_scdc` |
| name | `HMDI_SCDC` |
| longname | `Status and Control Data Channel` |
| desc | `Status and Control Data Channel: SCDC for HDMI2.0` |
| license | `gplv2+` |
| inputs | `['i2c']` |
| outputs | `['scdc']` |
| tags | `['Embedded/industrial']` |
| channels | 无 |
| optional_channels | 无 |
| options | `{'id': 'verbosity', 'desc': 'Verbosity', 'default': 'short', 'values': ('short', 'long', 'debug')}` |
| binary | 无 |

#### 2.3.2 注解定义

Python 原始定义：
```python
annotations = (
    ('Address', 'I²C address'),      # 0
    ('Register', 'Register name and offset'),  # 1
    ('Fields', 'Readable register interpretation'),  # 2
    ('Debug', 'Debug messages'),     # 3
)
annotation_rows = (
    ('scdc', 'SCDC', (0, 1, 2)),
    ('debug', 'Debug', (3,)),
)
```

C 映射：
```c
enum {
    ANN_ADDRESS = 0,
    ANN_REGISTER = 1,
    ANN_FIELDS = 2,
    ANN_DEBUG = 3,
    NUM_ANN,
};

static const char *hdmi_scdc_ann_labels[][3] = {
    {"", "address", "I²C address"},
    {"", "register", "Register name and offset"},
    {"", "fields", "Readable register interpretation"},
    {"", "debug", "Debug messages"},
};

static const int hdmi_scdc_row_scdc_classes[] = {ANN_ADDRESS, ANN_REGISTER, ANN_FIELDS};
static const int hdmi_scdc_row_debug_classes[] = {ANN_DEBUG};
static const struct srd_c_ann_row hdmi_scdc_ann_rows[] = {
    {"scdc", "SCDC", hdmi_scdc_row_scdc_classes, 3},
    {"debug", "Debug", hdmi_scdc_row_debug_classes, 1},
};
```

#### 2.3.3 解码逻辑分析

**复杂度：★★★★☆**

HDMI_SCDC 解码器的特点：
1. **I2C 从地址 0x54**：SCDC 专用地址。注意：I2C C 解码器默认 `address_shifted=1`，发送7位地址 0x54（对应8位写地址 0xA8 / 读地址 0xA9）。Python 版本使用8位地址 0xA8/0xA9 检查，C 版本应使用7位地址 0x54 <!-- Updated: I2C C 解码器默认发送7位地址，HDMI_SCDC 地址检查需用0x54而非0xA8/0xA9 -->
2. **8 状态状态机**：`IDLE, GET_SLAVE_ADDR, READ_REGISTER, WRITE_REGISTER, GET_OFFSET, OFFSET_RECEIVED, READ_REGISTER_WOFFSET, WRITE_REGISTER_WOFFSET`
3. **SCDC 寄存器查找表**：`SCDC_REG_LOOKUP` 字典，包含 13 个寄存器定义
4. **选项支持**：`verbosity` 选项（short/long/debug）
5. **错误检测寄存器**：0x50-0x56 为 Character Error Detection 寄存器，需要特殊处理（2 字节组合、通道计算）
6. **字段解释**：每个寄存器有 `fields` 数组，每个字段有 `mask` 和 `interpretation` 字典

#### 2.3.4 关键 C 代码片段

```c
enum hdmi_scdc_state {
    SCDC_IDLE,
    SCDC_GET_SLAVE_ADDR,
    SCDC_READ_REGISTER,
    SCDC_WRITE_REGISTER,
    SCDC_GET_OFFSET,
    SCDC_OFFSET_RECEIVED,
    SCDC_READ_REGISTER_WOFFSET,
    SCDC_WRITE_REGISTER_WOFFSET,
};

typedef struct {
    enum hdmi_scdc_state state;
    int reg;                // 当前寄存器地址
    int offset;             // 寄存器偏移
    int protocol;           // 0=none, 1=scdc
    uint8_t databytes[16];
    int databytes_len;
    int err_det_lower;      // CED 低字节
    uint64_t block_s, block_e;
    uint64_t ss, es;
    int out_ann;
    int verbosity;          // 0=short, 1=long, 2=debug
} hdmi_scdc_state;

// SCDC 寄存器定义
typedef struct {
    uint8_t offset;
    const char *name;
    const char *type;       // "R" or "RW"
} scdc_reg_def;

static const scdc_reg_def scdc_regs[] = {
    {0x01, "Sink version", "R"},
    {0x02, "Source version", "RW"},
    {0x10, "Update_0", "R"},
    {0x11, "Update_1", "RW"},
    {0x20, "TMDS_Config", "RW"},
    {0x21, "Scrambler status", "R"},
    {0x30, "Config_0", "RW"},
    {0x40, "Status_Flags_0", "R"},
    {0x41, "Status_Flags_1", "R"},
    {0x50, "Err_Det_0_L", "R"},
    {0x51, "Err_Det_0_H", "R"},
    {0x52, "Err_Det_1_L", "R"},
    {0x53, "Err_Det_1_H", "R"},
    {0x54, "Err_Det_2_L", "R"},
    {0x55, "Err_Det_2_H", "R"},
    {0x56, "Err_Det_Checksum", "R"},
    {0, NULL, NULL},
};

// 字段解释查找表（简化版，完整版需要为每个寄存器定义）
typedef struct {
    uint8_t mask;
    uint8_t value;
    const char *short_text;
    const char *long_text;
} scdc_field_interp;

static void hdmi_scdc_handle_scdc(struct srd_decoder_inst *di, hdmi_scdc_state *s)
{
    uint8_t reg_val = s->databytes[s->databytes_len - 1];
    char messages[512] = "";
    // 根据 offset 查找寄存器定义并解释字段
    // ... 字段解析逻辑
    C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_FIELDS, messages);
}

static void hdmi_scdc_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    hdmi_scdc_state *s = (hdmi_scdc_state *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    if (strcmp(cmd, "STOP") == 0) {
        memset(s, 0, sizeof(hdmi_scdc_state));
        s->state = SCDC_IDLE;
        return;
    }

    switch (s->state) {
    case SCDC_IDLE:
        if (strcmp(cmd, "START") == 0)
            s->state = SCDC_GET_SLAVE_ADDR;
        break;
    case SCDC_GET_SLAVE_ADDR:
        if (strcmp(cmd, "ADDRESS WRITE") == 0) {
            uint8_t addr = (data_len > 0) ? data[0] : 0;
            if (addr == 0x54) { /* 7-bit address (0xA8 >> 1) */
                C_ANN_PUT(di, s->ss, v->es, s->out_ann, ANN_ADDRESS,
                          "SCDC write - Address : 0xA8");
                s->protocol = 1;
                s->state = SCDC_GET_OFFSET;
            }
        } else if (strcmp(cmd, "ADDRESS READ") == 0) {
            uint8_t addr = (data_len > 0) ? data[0] : 0;
            if (addr == 0x54) { /* 7-bit address (0xA9 >> 1) */
                C_ANN_PUT(di, s->ss, v->es, s->out_ann, ANN_ADDRESS,
                          "SCDC read - Address : 0xA9");
                s->protocol = 1;
                s->state = SCDC_READ_REGISTER;
            }
        }
        break;
    case SCDC_GET_OFFSET:
        if (strcmp(cmd, "DATA WRITE") == 0) {
            s->offset = (data_len > 0) ? data[0] : 0;
            // 输出寄存器名称
            const char *reg_name = hdmi_scdc_lookup_reg(s->offset);
            char buf[128];
            if (reg_name)
                snprintf(buf, sizeof(buf), "Register: %s (0x%02x)", reg_name, s->offset);
            else
                snprintf(buf, sizeof(buf), "Unknown Register (0x%02x)", s->offset);
            C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_REGISTER, buf);
            s->state = SCDC_OFFSET_RECEIVED;
        }
        break;
    case SCDC_OFFSET_RECEIVED:
        if (strcmp(cmd, "START REPEAT") == 0) {
            s->state = SCDC_GET_SLAVE_ADDR;
        } else if (strcmp(cmd, "DATA WRITE") == 0) {
            s->databytes[s->databytes_len++] = (data_len > 0) ? data[0] : 0;
            s->state = SCDC_WRITE_REGISTER;
            hdmi_scdc_handle_scdc(di, s);
        }
        break;
    case SCDC_READ_REGISTER:
    case SCDC_WRITE_REGISTER:
        if (strcmp(cmd, "DATA READ") == 0 || strcmp(cmd, "DATA WRITE") == 0) {
            s->databytes[s->databytes_len++] = (data_len > 0) ? data[0] : 0;
            hdmi_scdc_handle_scdc(di, s);
        } else if (strcmp(cmd, "STOP") == 0 || strcmp(cmd, "START REPEAT") == 0) {
            memset(s, 0, sizeof(hdmi_scdc_state));
            s->state = SCDC_IDLE;
        }
        break;
    default:
        break;
    }
}
```

#### 2.3.5 移植难点

1. **SCDC_REG_LOOKUP 嵌套结构**：Python 版本使用嵌套字典，C 版本需要展平为结构体数组
2. **字段解释**：每个寄存器有多个字段，每个字段有 mask + interpretation 字典，需要大量 C 数据结构
3. **verbosity 选项**：需要从 `c_decoder_get_option_string()` 读取并影响输出格式
4. **CED 寄存器**：0x50-0x55 需要跨 2 字节组合，自动递增 offset
5. **建议**：先实现寄存器名称输出，字段解释可分阶段完成

---

### 2.4 TCA6408A 解码器

#### 2.4.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `tca6408a` |
| name | `TI TCA6408A` |
| longname | `Texas Instruments TCA6408A` |
| desc | `Texas Instruments TCA6408A 8-bit I²C I/O expander.` |
| license | `gplv2+` |
| inputs | `['i2c']` |
| outputs | `[]` |
| tags | `['Embedded/industrial', 'IC']` |
| channels | 无 |
| optional_channels | 无 |
| options | 无 |
| binary | 无 |
| logic_output_channels | `p0`-`p7`（8 个逻辑输出通道） |

#### 2.4.2 注解定义

Python 原始定义：
```python
annotations = (
    ('register', 'Register type'),    # 0
    ('value', 'Register value'),      # 1
    ('warning', 'Warning'),           # 2
)
annotation_rows = (
    ('regs', 'Registers', (0, 1)),
    ('warnings', 'Warnings', (2,)),
)
```

C 映射：
```c
enum {
    ANN_REGISTER = 0,
    ANN_VALUE = 1,
    ANN_WARNING = 2,
    NUM_ANN,
};

static const char *tca6408a_ann_labels[][3] = {
    {"", "register", "Register type"},
    {"", "value", "Register value"},
    {"", "warning", "Warning"},
};

static const int tca6408a_row_regs_classes[] = {ANN_REGISTER, ANN_VALUE};
static const int tca6408a_row_warnings_classes[] = {ANN_WARNING};
static const struct srd_c_ann_row tca6408a_ann_rows[] = {
    {"regs", "Registers", tca6408a_row_regs_classes, 2},
    {"warnings", "Warnings", tca6408a_row_warnings_classes, 1},
};
```

#### 2.4.3 解码逻辑分析

**复杂度：★★☆☆☆（最低）**

TCA6408A 解码器的特点：
1. **I2C 从地址 0x20/0x21**：检查地址合法性
2. **4 个寄存器**：
   - 0x00：Input port（只读）
   - 0x01：Output port（读写）
   - 0x02：Polarity inversion register（读写）
   - 0x03：Configuration register（读写）
3. **状态机**：`IDLE → GET SLAVE ADDR → GET REG ADDR → WRITE IO REGS / READ IO REGS → IDLE`
4. **逻辑输出**：注册 `SRD_OUTPUT_LOGIC` 输出，8 个通道（p0-p7），使用 `c_decoder_put_logic()` 输出逻辑状态 <!-- Updated: SRD_OUTPUT_LOGIC 和 c_decoder_put_logic() 已实现 -->
5. **简单直接**：每个寄存器只有一个字节，处理逻辑非常简单

#### 2.4.4 关键 C 代码片段

```c
enum tca6408a_state {
    TCA6408A_IDLE,
    TCA6408A_GET_SLAVE_ADDR,
    TCA6408A_GET_REG_ADDR,
    TCA6408A_WRITE_IO_REGS,
    TCA6408A_READ_IO_REGS,
    TCA6408A_READ_IO_REGS2,
};

typedef struct {
    enum tca6408a_state state;
    int reg;
    int chip;
    uint64_t ss, es;
    uint64_t logic_output_es;
    uint8_t logic_value;
    int out_ann;
    int out_logic;
} tca6408a_state;

static void tca6408a_handle_reg(struct srd_decoder_inst *di, tca6408a_state *s,
    int reg, uint8_t b)
{
    char buf[64];
    switch (reg) {
    case 0x00:
        snprintf(buf, sizeof(buf), "State of inputs: %02X", b);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_VALUE, buf);
        break;
    case 0x01:
        snprintf(buf, sizeof(buf), "Outputs set: %02X", b);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_VALUE, buf);
        // 更新逻辑输出 <!-- Updated: 使用 c_decoder_put_logic() 输出逻辑状态 -->
        s->logic_value = b;
        if (s->es > s->logic_output_es) {
            uint8_t logic_data[1] = {b};
            c_decoder_put_logic(di, s->logic_output_es, s->es,
                                s->out_logic, 0xFF, logic_data, 8);
            s->logic_output_es = s->es;
        }
        break;
    case 0x02:
        snprintf(buf, sizeof(buf), "Polarity inverted: %02X", b);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_VALUE, buf);
        break;
    case 0x03:
        snprintf(buf, sizeof(buf), "Configuration: %02X", b);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_VALUE, buf);
        break;
    }
}

static void tca6408a_handle_write_reg(struct srd_decoder_inst *di, tca6408a_state *s, int reg)
{
    switch (reg) {
    case 0: C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_REGISTER, "Input port"); break;
    case 1: C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_REGISTER, "Output port"); break;
    case 2: C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_REGISTER, "Polarity inversion register"); break;
    case 3: C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_REGISTER, "Configuration register"); break;
    }
}

static void tca6408a_check_correct_chip(struct srd_decoder_inst *di, tca6408a_state *s, int addr)
{
    if (addr != 0x20 && addr != 0x21) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Warning: I²C slave 0x%02X not a TCA6408A compatible chip.", addr);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_WARNING, buf);
        s->state = TCA6408A_IDLE;
    }
}

static void tca6408a_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    tca6408a_state *s = (tca6408a_state *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    if (s->state == TCA6408A_IDLE) {
        if (strcmp(cmd, "START") == 0)
            s->state = TCA6408A_GET_SLAVE_ADDR;
    } else if (s->state == TCA6408A_GET_SLAVE_ADDR) {
        s->chip = (data_len > 0) ? data[0] : 0;
        s->state = TCA6408A_GET_REG_ADDR;
    } else if (s->state == TCA6408A_GET_REG_ADDR) {
        if (strcmp(cmd, "ADDRESS READ") == 0 || strcmp(cmd, "ADDRESS WRITE") == 0) {
            tca6408a_check_correct_chip(di, s, (data_len > 0) ? data[0] : 0);
        }
        if (strcmp(cmd, "DATA WRITE") != 0) return;
        s->reg = (data_len > 0) ? data[0] : 0;
        tca6408a_handle_write_reg(di, s, s->reg);
        s->state = TCA6408A_WRITE_IO_REGS;
    } else if (s->state == TCA6408A_WRITE_IO_REGS) {
        if (strcmp(cmd, "START REPEAT") == 0) {
            s->state = TCA6408A_READ_IO_REGS;
        } else if (strcmp(cmd, "DATA WRITE") == 0) {
            tca6408a_handle_reg(di, s, s->reg, (data_len > 0) ? data[0] : 0);
        } else if (strcmp(cmd, "STOP") == 0) {
            s->state = TCA6408A_IDLE;
            s->chip = -1;
        }
    } else if (s->state == TCA6408A_READ_IO_REGS) {
        if (strcmp(cmd, "ADDRESS READ") == 0) {
            s->state = TCA6408A_READ_IO_REGS2;
            s->chip = (data_len > 0) ? data[0] : 0;
        }
    } else if (s->state == TCA6408A_READ_IO_REGS2) {
        if (strcmp(cmd, "DATA READ") == 0) {
            tca6408a_handle_reg(di, s, s->reg, (data_len > 0) ? data[0] : 0);
        } else if (strcmp(cmd, "STOP") == 0) {
            s->state = TCA6408A_IDLE;
        }
    }
}
```

#### 2.4.5 移植难点

1. **逻辑输出通道**：Python 版本注册了 `OUTPUT_LOGIC`，C 版本需要使用 `c_decoder_register_output(di, SRD_OUTPUT_LOGIC, "tca6408a")` 注册，并使用 `c_decoder_put_logic(di, start_sample, end_sample, out_logic, channel_mask, values, num_channels)` 输出逻辑状态。API 已实现 <!-- Updated: c_decoder_put_logic() API 已实现，签名: (di, start_sample, end_sample, output_id, channel_mask, values, num_channels) -->
2. **flush 回调**：Python 版本有 `flush()` 方法输出逻辑状态，C 版本需要在 STOP 时处理
3. **整体简单**：这是 5 个解码器中最简单的，建议优先实现

---

### 2.5 TMP102 解码器

#### 2.5.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `tmp102` |
| name | `TMP102` |
| longname | `Digital temperature sensor TMP102` |
| desc | `Low power digital temperature sensor.` |
| license | `gplv2+` |
| inputs | `['i2c']` |
| outputs | `['tmp102']` |
| tags | `['Embedded/industrial']` |
| channels | 无 |
| optional_channels | 无 |
| options | `{'id': 'radix', 'desc': 'Number format', 'default': 'Hex', 'values': ('Hex', 'Dec', 'Oct', 'Bin')}`, `{'id': 'units', 'desc': 'Temperature unit', 'default': 'Celsius', 'values': ('Celsius', 'Fahrenheit', 'Kelvin')}` |
| binary | 无 |

#### 2.5.2 注解定义

Python 版本使用 `hlp.create_annots()` 辅助函数动态生成注解，涉及多个枚举类：

```python
# 地址注解
AnnAddrs: GC, GND, VCC, SDA, SCL
# 寄存器注解
AnnRegs: RESET, CONF, TEMP, TLOW, THIGH
# 位注解
AnnBits: RESERVED, DATA, EM, AL, CR0, SD, TM, POL, F0, R0, OS
# 信息注解
AnnInfo: WARN, BADADD, GRST, CHECK, WRITE, READ, SELECT, CUSTOM, PWRUP, CONF, TEMP, TLOW, THIGH
```

C 映射（需要展平所有注解类为一个连续枚举）：
```c
enum {
    // AnnAddrs (0-4)
    ANN_ADDR_GC = 0,
    ANN_ADDR_GND,
    ANN_ADDR_VCC,
    ANN_ADDR_SDA,
    ANN_ADDR_SCL,
    // AnnRegs (5-9)
    ANN_REG_RESET,
    ANN_REG_CONF,
    ANN_REG_TEMP,
    ANN_REG_TLOW,
    ANN_REG_THIGH,
    // AnnBits (10-20)
    ANN_BIT_RESERVED,
    ANN_BIT_DATA,
    ANN_BIT_EM,
    ANN_BIT_AL,
    ANN_BIT_CR0,
    ANN_BIT_SD,
    ANN_BIT_TM,
    ANN_BIT_POL,
    ANN_BIT_F0,
    ANN_BIT_R0,
    ANN_BIT_OS,
    // AnnInfo (21-34)
    ANN_INFO_WARN,
    ANN_INFO_BADADD,
    ANN_INFO_GRST,
    ANN_INFO_CHECK,
    ANN_INFO_WRITE,
    ANN_INFO_READ,
    ANN_INFO_SELECT,
    ANN_INFO_CUSTOM,
    ANN_INFO_PWRUP,
    ANN_INFO_CONF,
    ANN_INFO_TEMP,
    ANN_INFO_TLOW,
    ANN_INFO_THIGH,
    NUM_ANN,
};
```

#### 2.5.3 解码逻辑分析

**复杂度：★★★★★（最高之一）**

TMP102 解码器的特点：
1. **4 个寄存器**：Configuration (0x01), Temperature (0x00), TLOW (0x02), THIGH (0x03)，加上 General Call (0x06)
2. **General Call 支持**：地址 0x00 为通用呼叫地址
3. **4 个从地址**：ADD0 引脚决定地址（GND=0x48, VCC=0x49, SDA=0x4A, SCL=0x4B）
4. **Extended Mode**：12-bit（正常）或 13-bit（扩展）温度分辨率
5. **状态机**：`IDLE → ADDRESS SLAVE → REGISTER ADDRESS → REGISTER DATA`
6. **BITS 包处理**：需要累积 BITS 包用于位级注解
7. **温度计算**：支持 12/13-bit 二进制补码，支持摄氏/华氏/开尔文转换
8. **选项支持**：`radix`（数制格式）和 `units`（温度单位）
9. **使用辅助模块**：`common.srdhelper` 中的 `hlp` 模块

#### 2.5.4 关键 C 代码片段

```c
enum tmp102_state {
    TMP102_IDLE,
    TMP102_ADDRESS_SLAVE,
    TMP102_REGISTER_ADDRESS,
    TMP102_REGISTER_DATA,
};

typedef struct {
    enum tmp102_state state;
    int addr;               // 当前从地址
    int reg;                // 当前寄存器
    int em;                 // Extended mode 标志
    int write;              // 写操作标志
    uint8_t bytes[4];       // 数据字节缓冲
    int bytes_len;
    uint64_t ssd;           // 数据块起始采样
    uint64_t ssb;           // 块起始采样
    uint64_t ss, es;
    int out_ann;
    int radix;              // 0=Hex, 1=Dec, 2=Oct, 3=Bin
    int units;              // 0=Celsius, 1=Fahrenheit, 2=Kelvin
} tmp102_state;

static double tmp102_calculate_temperature(tmp102_state *s, int rawdata)
{
    if (rawdata & (1 << 0))  // EM bit in temperature register
        s->em = 1;

    if (s->em) {
        // Extended mode (13-bit)
        rawdata >>= 3;
        if (rawdata > 0x0fff)
            rawdata |= 0xe000;  // 2's complement
    } else {
        // Normal mode (12-bit)
        rawdata >>= 4;
        if (rawdata > 0x07ff)
            rawdata |= 0xf000;  // 2's complement
    }

    double temperature = (double)rawdata / 16.0;  // Celsius

    if (s->units == 1) {  // Fahrenheit
        temperature = temperature * 9.0 / 5.0 + 32.0;
    } else if (s->units == 2) {  // Kelvin
        temperature += 273.15;
    }

    return temperature;
}

static void tmp102_handle_datareg_0x00(struct srd_decoder_inst *di, tmp102_state *s, int dataword)
{
    double temp = tmp102_calculate_temperature(s, dataword);
    const char *unit_str = (s->units == 0) ? "°C" : (s->units == 1) ? "°F" : "K";
    char buf[64];
    snprintf(buf, sizeof(buf), "Temperature: %.2f %s", temp, unit_str);
    C_ANN_PUT(di, s->ssb, s->es, s->out_ann, ANN_INFO_TEMP, buf);
}

static void tmp102_handle_datareg_0x01(struct srd_decoder_inst *di, tmp102_state *s, int dataword)
{
    // Configuration register - 解析各个字段
    // OS bit
    int os = (dataword >> 15) & 1;
    // R0/R1 bits - converter resolution
    int res = (dataword >> 13) & 3;
    // F0/F1 bits - fault queue
    int flt = (dataword >> 11) & 3;
    // POL bit - polarity
    int pol = (dataword >> 10) & 1;
    // TM bit - thermostat mode
    int tm = (dataword >> 9) & 1;
    // SD bit - shutdown mode
    int sd = (dataword >> 8) & 1;
    // CR0/CR1 bits - conversion rate
    int cr = (dataword >> 6) & 3;
    // AL bit - alert
    int al = (dataword >> 5) & 1;
    // EM bit - extended mode
    int em = (dataword >> 4) & 1;
    s->em = em;

    char buf[256];
    snprintf(buf, sizeof(buf),
             "Configuration: OS=%d R=%d F=%d POL=%d TM=%d SD=%d CR=%d AL=%d EM=%d",
             os, res, flt, pol, tm, sd, cr, al, em);
    C_ANN_PUT(di, s->ssb, s->es, s->out_ann, ANN_INFO_CONF, buf);
}

static void tmp102_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    tmp102_state *s = (tmp102_state *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    if (strcmp(cmd, "BITS") == 0) {
        // Python 版本累积 BITS 包用于位级注解
        // C 版本可简化：不实现位级注解，只实现寄存器级和信息级
        return;
    }

    if (s->state == TMP102_IDLE) {
        if (strcmp(cmd, "START") == 0) {
            s->ssb = start_sample;
            s->state = TMP102_ADDRESS_SLAVE;
        }
    } else if (s->state == TMP102_ADDRESS_SLAVE) {
        if (strcmp(cmd, "ADDRESS WRITE") == 0 || strcmp(cmd, "ADDRESS READ") == 0) {
            uint8_t addr = (data_len > 0) ? data[0] : 0;
            if (tmp102_check_addr(addr, 1)) {
                s->addr = addr;
                s->write = (strcmp(cmd, "ADDRESS WRITE") == 0);
                if (!s->write)
                    s->state = TMP102_REGISTER_DATA;
                else
                    s->state = TMP102_REGISTER_ADDRESS;
            } else {
                s->state = TMP102_IDLE;
            }
        }
    } else if (s->state == TMP102_REGISTER_ADDRESS) {
        if (strcmp(cmd, "DATA WRITE") == 0 || strcmp(cmd, "DATA READ") == 0) {
            s->reg = (data_len > 0) ? data[0] : 0;
            s->state = TMP102_REGISTER_DATA;
        } else if (strcmp(cmd, "STOP") == 0 || strcmp(cmd, "START REPEAT") == 0) {
            s->state = TMP102_IDLE;
        }
    } else if (s->state == TMP102_REGISTER_DATA) {
        if (strcmp(cmd, "DATA WRITE") == 0 || strcmp(cmd, "DATA READ") == 0) {
            uint8_t databyte = (data_len > 0) ? data[0] : 0;
            if (s->bytes_len == 0) {
                s->ssd = start_sample;
                s->bytes[s->bytes_len++] = databyte;
            } else {
                s->bytes[s->bytes_len++] = databyte;
            }
        } else if (strcmp(cmd, "START REPEAT") == 0) {
            s->state = TMP102_ADDRESS_SLAVE;
        } else if (strcmp(cmd, "STOP") == 0) {
            tmp102_handle_data(di, s);
            s->state = TMP102_IDLE;
        }
    }
}
```

#### 2.5.5 移植难点

1. **注解数量庞大**：35 个注解类，需要逐一映射
2. **BITS 包处理**：Python 版本使用 BITS 包做位级注解，C 版本可简化为只做寄存器级和信息级。如需实现位级注解，BITS v2 格式已提供 per-bit 时间戳 <!-- Updated: BITS v2 已提供 per-bit ss/es -->
3. **hlp 辅助模块**：Python 版本大量使用 `hlp.compose_annot()` 等辅助函数，C 版本需要手动格式化
4. **General Call 支持**：需要处理地址 0x00 的通用呼叫
5. **选项处理**：`radix` 和 `units` 选项影响输出格式
6. **温度计算**：12/13-bit 二进制补码 + 单位转换
7. **建议**：先实现核心温度读取和配置寄存器解析，位级注解和 General Call 可后续补充

---

## 3. CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加 5 个新解码器：

```cmake
set(C_DECODERS
    # ... 现有解码器 ...
    xfp_c
    hdcp_c
    hdmi_scdc_c
    tca6408a_c
    tmp102_c
)
```

---

## 4. 文件命名与结构体命名规范

| 解码器 | 文件名 | 结构体 id | 结构体 name | 全局变量名 |
|--------|--------|-----------|-------------|-----------|
| xfp | `xfp_c.c` | `xfp_c` | `XFP(C)` | `xfp_c_decoder` |
| hdcp | `hdcp_c.c` | `hdcp_c` | `HDCP(C)` | `hdcp_c_decoder` |
| hdmi_scdc | `hdmi_scdc_c.c` | `hdmi_scdc_c` | `HDMI_SCDC(C)` | `hdmi_scdc_c_decoder` |
| tca6408a | `tca6408a_c.c` | `tca6408a_c` | `TCA6408A(C)` | `tca6408a_c_decoder` |
| tmp102 | `tmp102_c.c` | `tmp102_c` | `TMP102(C)` | `tmp102_c_decoder` |

---

## 5. 复杂度排序与实现建议

| 优先级 | 解码器 | 复杂度 | 预估代码行数 | 建议 |
|--------|--------|--------|-------------|------|
| 1 | tca6408a | ★★☆☆☆ | ~200 行 | 最简单，4 个寄存器，无选项，优先实现 |
| 2 | hdcp | ★★★☆☆ | ~350 行 | 中等，状态机清晰，查找表可控 |
| 3 | hdmi_scdc | ★★★★☆ | ~500 行 | 较复杂，寄存器解释表大，有选项 |
| 4 | tmp102 | ★★★★★ | ~600 行 | 复杂，注解类多，BITS 处理，选项 |
| 5 | xfp | ★★★★★ | ~800+ 行 | 最复杂，依赖外部查找表，多字节字段 |

**建议实现顺序**：tca6408a → hdcp → hdmi_scdc → tmp102 → xfp
