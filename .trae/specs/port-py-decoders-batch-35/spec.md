# Python → C 解码器移植规格书 — Batch 35

## 1. 概述

本规格书描述将 5 个 Python 上层协议解码器移植为 C 解码器的完整技术方案。这 5 个解码器均为**上层解码器**（stacked decoder），通过 `recv_proto()` 回调接收下层 C 解码器的输出数据，而非直接读取 logic 信号。

### 1.1 移植清单

| # | 解码器 ID | C 文件名 | 下层依赖 | 下层 C 实现 | 复杂度 |
|---|-----------|----------|----------|------------|--------|
| 1 | `cfp` | `cfp_c.c` | `mdio_c` | ✅ 已有 | 低 |
| 2 | `ps2_keyboard` | `ps2_keyboard_c.c` | `ps2_c` | ✅ 已有 | 中 |
| 3 | `ps2_mouse` | `ps2_mouse_c.c` | `ps2_c` | ✅ 已有 | 中 |
| 4 | `usb_packet` | `usb_packet_c.c` | `usb_signalling_c` | ✅ 已有 | 高 |
| 5 | `usb_request` | `usb_request_c.c` | `usb_packet_c` | ⚠️ 本批次内依赖 | 高 |

<!-- Updated: 添加"下层 C 实现"列，确认所有下层依赖均有 C 实现（mdio_c, ps2_c, usb_signalling_c 已存在）；usb_request_c 依赖 usb_packet_c（本批次内依赖，需先完成 usb_packet_c） -->

### 1.2 依赖关系图

```
mdio_c ──────→ cfp_c
ps2_c  ──────→ ps2_keyboard_c
ps2_c  ──────→ ps2_mouse_c
usb_signalling_c → usb_packet_c → usb_request_c
```

### 1.3 前置条件（已解决）

<!-- Updated: ps2_c.c 已添加 out_python 输出和 "BYTE" 命令，ps2_outputs = {"ps2", NULL}，前置条件已满足 -->

~~**ps2_c.c 当前无 Python 输出**（`outputs = {NULL}`），导致 ps2_keyboard_c 和 ps2_mouse_c 无法堆叠。必须先修改 ps2_c.c 添加 `out_python` 输出和 `"ps2"` output type。~~

**ps2_c.c 已实现 Python 输出**：`out_python` 已注册，`"BYTE"` 命令格式为 `data[0]=byte_val, data[1]=is_host, data[2]=parity_ok, data[3]=has_ack`（4 字节）。ps2_keyboard_c 和 ps2_mouse_c 可正常堆叠。

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |

## 2. 上层 C 解码器架构规范

### 2.1 recv_proto 回调签名

```c
void (*recv_proto)(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

- `cmd`: 下层解码器发送的命令字符串（如 `"DATA"`, `"SOP"`, `"BIT"` 等）
- `data`: 附加二进制数据（可为 NULL）
- `data_len`: data 长度

### 2.2 上层解码器模板结构

上层解码器与底层解码器的关键区别：

| 特征 | 底层解码器 | 上层解码器 |
|------|-----------|-----------|
| `channels` | 定义信号通道 | `NULL, 0` |
| `inputs` | `{"logic"}` | 下层输出类型名 |
| `decode()` | 主循环，使用 `c_cond_wait` | 空函数 `(void)di;` |
| `recv_proto()` | `NULL` | **核心逻辑** |
| `outputs` | 输出类型名 | 可为空或输出类型名 |

### 2.3 srd_c_decoder 结构体关键字段

```c
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",              // 必须以 _c 结尾
    .name = "XXX(C)",           // 显示名加 (C) 后缀
    .longname = "...",
    .desc = "...",
    .license = "...",
    .channels = NULL,           // 上层解码器无通道
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = xxx_options,
    .num_options = N,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = N,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,       // 下层输出类型
    .num_inputs = 1,
    .outputs = xxx_outputs,
    .num_outputs = N,
    .binary = NULL,
    .num_binary = 0,
    .tags = xxx_tags,
    .num_tags = 1,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,       // 空函数
    .destroy = xxx_destroy,
    .recv_proto = xxx_recv_proto, // 核心逻辑
};
```

---

## 3. 各解码器详细规格

---

### 3.1 CFP 解码器 (`cfp_c.c`)

#### 3.1.1 Python 原始元数据

```python
id = 'cfp'
name = 'CFP'
longname = '100 Gigabit C form-factor pluggable'
desc = '100 Gigabit C form-factor pluggable (CFP) protocol.'
license = 'BSD'
inputs = ['mdio']
outputs = []
tags = ['Networking']
annotations = (
    ('register', 'Register'),    # ANN_REGISTER = 0
    ('decode', 'Decode'),        # ANN_DECODE = 1
)
annotation_rows = (
    ('registers', 'Registers', (0,)),
    ('decodes', 'Decodes', (1,)),
)
```

#### 3.1.2 C 元数据映射

```c
#define ANN_REGISTER  0
#define ANN_DECODE    1
#define NUM_ANN       2

static const char *cfp_ann_labels[][3] = {
    {"", "register", "Register"},
    {"", "decode", "Decode"},
};

static const int cfp_row_registers_classes[] = {ANN_REGISTER, -1};
static const int cfp_row_decodes_classes[] = {ANN_DECODE, -1};
static const struct srd_c_ann_row cfp_ann_rows[] = {
    {"registers", "Registers", cfp_row_registers_classes, 1},
    {"decodes", "Decodes", cfp_row_decodes_classes, 1},
};

static const char *cfp_inputs[] = {"mdio", NULL};
static const char *cfp_outputs[] = {NULL};
static const char *cfp_tags[] = {"Networking", NULL};
```

#### 3.1.3 下层数据格式（mdio_c → cfp_c）

mdio_c 的 `c_decoder_put_python` 输出格式：
```c
// cmd = "DATA", data = 8 bytes:
// data[0] = clause45 (0 or 1)
// data[1] = clause45_addr >> 8
// data[2] = clause45_addr & 0xFF
// data[3] = is_read (0 or 1)
// data[4] = portad
// data[5] = devad
// data[6] = data_value >> 8
// data[7] = data_value & 0xFF
```

#### 3.1.4 解码逻辑分析

CFP 解码器逻辑非常简单：
1. 仅处理 `is_read == 1` 的 MDIO DATA 包
2. 根据 `clause45_addr` 范围映射到 CFP 寄存器区域
3. 输出寄存器区域标注（ANN_REGISTER）和解码信息（ANN_DECODE）

**寄存器地址映射表**：

| 地址范围 | 寄存器名 | 短名 |
|----------|---------|------|
| 0x8000-0x807F | CFP NVR 1: Basic ID register | NVR1 |
| 0x8080-0x80FF | CFP NVR 2: Extended ID register | NVR2 |
| 0x8100-0x817F | CFP NVR 3: Network lane specific register | NVR3 |
| 0x8180-0x81FF | CFP NVR 4 | NVR4 |
| 0x8400-0x847F | Vendor NVR 1: Vendor data register | V-NVR1 |
| 0x8480-0x84FF | Vendor NVR 2: Vendor data register | V-NVR2 |
| 0x8800-0x887F | User NVR 1: User data register | U-NVR1 |
| 0x8880-0x88FF | User NVR 2: User data register | U-NVR2 |
| 0xA000-0xA07F | CFP Module VR 1: CFP Module level control and DDM register | Mod-VR1 |
| 0xA080-0xA0FF | MLG VR 1: MLG Management Interface register | MLG-VR1 |

**特殊解码**：当 `clause45_addr == 0x8000` 时，额外输出 Module identifier 解码（ANN_DECODE），使用 MODULE_ID 查找表。

#### 3.1.5 MODULE_ID 查找表

```c
static const struct { uint8_t id; const char *name; } module_id_table[] = {
    {0x00, "Unknown or unspecified"},
    {0x01, "GBIC"},
    {0x02, "Module/connector soldered to motherboard"},
    {0x03, "SFP"},
    {0x04, "300 pin XSBI"},
    {0x05, "XENPAK"},
    {0x06, "XFP"},
    {0x07, "XFF"},
    {0x08, "XFP-E"},
    {0x09, "XPAK"},
    {0x0A, "X2"},
    {0x0B, "DWDM-SFP"},
    {0x0C, "QSFP"},
    {0x0D, "QSFP+"},
    {0x0E, "CFP"},
    {0x0F, "CXP (TBD)"},
    {0x11, "CFP2"},
    {0x12, "CFP4"},
};
#define NUM_MODULE_IDS 18

static const char *cfp_lookup_module_id(uint8_t id) {
    for (int i = 0; i < NUM_MODULE_IDS; i++) {
        if (module_id_table[i].id == id)
            return module_id_table[i].name;
    }
    return "Reserved";
}
```

#### 3.1.6 recv_proto 实现

```c
static void cfp_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    cfp_state *s = (cfp_state *)c_decoder_get_private(di);
    if (!s) return;

    // 仅处理 MDIO "DATA" 命令
    if (strcmp(cmd, "DATA") != 0) return;
    if (!data || data_len < 8) return;

    int clause45 = data[0];
    int clause45_addr = (data[1] << 8) | data[2];
    int is_read = data[3];
    // int portad = data[4];  // CFP 不使用
    // int devad = data[5];   // CFP 不使用
    int reg = (data[6] << 8) | data[7];

    if (!is_read) return;

    // 根据 clause45_addr 范围输出寄存器区域标注
    if (clause45_addr >= 0x8000 && clause45_addr <= 0x807F) {
        C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_REGISTER,
                  "CFP NVR 1: Basic ID register", "NVR1");
        if (clause45_addr == 0x8000) {
            const char *mod_name = cfp_lookup_module_id((uint8_t)reg);
            char buf[128];
            snprintf(buf, sizeof(buf), "Module identifier: %s", mod_name);
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_DECODE, buf);
        }
    } else if (clause45_addr >= 0x8080 && clause45_addr <= 0x80FF) {
        C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_REGISTER,
                  "CFP NVR 2: Extended ID register", "NVR2");
    }
    // ... 其余地址范围类似
}
```

#### 3.1.7 私有状态结构

```c
typedef struct {
    int out_ann;
} cfp_state;
```

---

### 3.2 PS/2 Keyboard 解码器 (`ps2_keyboard_c.c`)

#### 3.2.1 Python 原始元数据

```python
id = 'ps2_keyboard'
name = 'PS/2 Keyboard'
longname = 'PS/2 Keyboard'
desc = 'PS/2 keyboard interface.'
license = 'gplv2+'
inputs = ['ps2']
outputs = []
tags = ['PC']
binary = (
    ('Keys', 'Key presses'),   # BINARY_KEYS = 0
)
annotations = (
    ('Press', 'Key pressed'),    # ANN_PRESS = 0
    ('Release', 'Key released'), # ANN_RELEASE = 1
    ('Ack', 'Acknowledge'),      # ANN_ACK = 2
)
annotation_rows = (
    ('keys', 'key presses and releases', (0, 1, 2)),
)
```

#### 3.2.2 C 元数据映射

```c
#define ANN_PRESS    0
#define ANN_RELEASE  1
#define ANN_ACK      2
#define NUM_ANN      3

#define BINARY_KEYS  0

static const char *ps2kb_ann_labels[][3] = {
    {"", "Press", "Key pressed"},
    {"", "Release", "Key released"},
    {"", "Ack", "Acknowledge"},
};

static const int ps2kb_row_keys_classes[] = {ANN_PRESS, ANN_RELEASE, ANN_ACK, -1};
static const struct srd_c_ann_row ps2kb_ann_rows[] = {
    {"keys", "Key presses and releases", ps2kb_row_keys_classes, 3},
};

static const struct srd_decoder_binary ps2kb_binary[] = {
    {0, "keys", "Key presses"},
};

static const char *ps2kb_inputs[] = {"ps2", NULL};
static const char *ps2kb_outputs[] = {NULL};
static const char *ps2kb_tags[] = {"PC", NULL};
```

#### 3.2.3 前置条件：ps2_c.c Python 输出（已实现）

<!-- Updated: ps2_c.c 已完成 Python 输出添加，无需再修改 -->

**已实现**：ps2_c.c 已添加 `out_python` 输出，具体修改如下：

1. ~~修改 `ps2_outputs`~~ → 已修改为：
```c
static const char *ps2_outputs[] = {"ps2", NULL};
```

2. ~~在 `ps2_priv` 中添加字段~~ → 已添加：
```c
int out_python;
```

3. ~~在 `ps2_start()` 中注册 python 输出~~ → 已添加：
```c
s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "ps2");
```

4. ~~在 `ps2_handle_byte()` 末尾添加 python 输出~~ → 已添加，格式为：
```c
// cmd = "BYTE", data = 4 bytes:
// data[0] = byte_val
// data[1] = is_host (0 or 1)
// data[2] = parity_ok (0 or 1)
// data[3] = has_ack (0 or 1)
c_decoder_put_python(di, s->bit_ss[0], samplenum, s->out_python, "BYTE", py_data, 4);
```

5. ~~更新 `srd_c_decoder` 结构体~~ → 已更新：
```c
.outputs = ps2_outputs,
.num_outputs = 1,
```

#### 3.2.4 解码逻辑分析

PS/2 Keyboard 解码器是一个简单的状态机：

**状态**：
- `sw == 0`: 初始状态，等待第一个字节
- `sw == 1`: 已收到第一个字节，等待后续
- `sw == 4`: ACK 状态

**字节处理**：
1. 如果 `host == 1`（主机发送），重置状态
2. 如果 `val == 0xF0`：下一个字节是释放码（Release），设置 `ann = ANN_RELEASE`
3. 如果 `val == 0xE0`：扩展字符标记，设置 `extended = True`
4. 如果 `val == 0xFA`：ACK 确认码
5. 否则：查表解码按键名

**按键查表**：需要将 `sc.py` 中的 `std` 和 `ext` 字典转换为 C 数组。

#### 3.2.5 扫描码查找表

```c
// 标准扫描码表
static const struct { uint8_t code; const char *name; } std_keys[] = {
    {0x1C, "A"}, {0x32, "B"}, {0x21, "C"}, {0x23, "D"}, {0x24, "E"},
    {0x2B, "F"}, {0x34, "G"}, {0x33, "H"}, {0x43, "I"}, {0x3B, "J"},
    {0x42, "K"}, {0x4B, "L"}, {0x3A, "M"}, {0x31, "N"}, {0x44, "O"},
    {0x4D, "P"}, {0x15, "Q"}, {0x2D, "R"}, {0x1B, "S"}, {0x2C, "T"},
    {0x3C, "U"}, {0x2A, "V"}, {0x1D, "W"}, {0x22, "X"}, {0x35, "Y"},
    {0x1A, "Z"}, {0x45, "0)"}, {0x16, "1!"}, {0x1E, "2@"}, {0x26, "3#"},
    {0x25, "4$"}, {0x2E, "5%"}, {0x36, "6^"}, {0x3D, "7&"}, {0x3E, "8*"},
    {0x46, "9("}, {0x0E, "`~"}, {0x4E, "-_"}, {0x55, "=+"}, {0x5D, "\\|"},
    {0x66, "Backsp"}, {0x29, "Space"}, {0x0D, "Tab"}, {0x58, "CapsLk"},
    {0x12, "L Shft"}, {0x14, "L Ctrl"}, {0x11, "L Alt"}, {0x59, "R Shft"},
    {0x5A, "Enter"}, {0x76, "Esc"}, {0x05, "F1"}, {0x06, "F2"},
    {0x04, "F3"}, {0x0C, "F4"}, {0x03, "F5"}, {0x0B, "F6"},
    {0x83, "F7"}, {0x0A, "F8"}, {0x01, "F9"}, {0x09, "F10"},
    {0x78, "F11"}, {0x07, "F12"}, {0x7E, "ScrLck"},
};
#define NUM_STD_KEYS (sizeof(std_keys) / sizeof(std_keys[0]))

// 扩展扫描码表
static const struct { uint8_t code; const char *name; } ext_keys[] = {
    {0x1F, "L Sup"}, {0x14, "R Ctrl"}, {0x27, "R Sup"}, {0x11, "R Alt"},
    {0x2F, "Menu"}, {0x12, "PrtScr"}, {0x7C, "SysRq"}, {0x70, "Insert"},
    {0x6C, "Home"}, {0x7D, "Pg Up"}, {0x71, "Delete"}, {0x69, "End"},
    {0x7A, "Pg Dn"}, {0x75, "Up arrow"}, {0x6B, "Left arrow"},
    {0x74, "Right arrow"}, {0x72, "Down arrow"}, {0x4A, "KP /"},
    {0x5A, "KP Ent"},
};
#define NUM_EXT_KEYS (sizeof(ext_keys) / sizeof(ext_keys[0]))

static const char *ps2kb_lookup_key(uint8_t code, int extended) {
    const struct { uint8_t code; const char *name; } *table;
    int count;
    if (extended) {
        table = ext_keys;
        count = NUM_EXT_KEYS;
    } else {
        table = std_keys;
        count = NUM_STD_KEYS;
    }
    for (int i = 0; i < count; i++) {
        if (table[i].code == code)
            return table[i].name;
    }
    return NULL; // 未知按键
}
```

#### 3.2.6 私有状态结构

```c
typedef struct {
    int out_ann;
    int out_binary;
    int sw;           // 状态机: 0=初始, 1=已收到首字节, 4=ACK
    int ann;          // 当前标注类型: ANN_PRESS / ANN_RELEASE / ANN_ACK
    int extended;     // 扩展字符标记
    uint64_t ss;      // 起始采样点
} ps2kb_state;
```

#### 3.2.7 recv_proto 实现

```c
static void ps2kb_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ps2kb_state *s = (ps2kb_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "BYTE") != 0) return;
    if (!data || data_len < 4) return;

    uint8_t val = data[0];
    int is_host = data[1];
    // int parity_ok = data[2]; // 当前未使用
    // int has_ack = data[3];   // 当前未使用

    // 主机发送时重置
    if (is_host) {
        s->sw = 0;
        s->ann = ANN_PRESS;
        s->extended = 0;
        return;
    }

    if (s->sw < 1) {
        s->ss = start_sample;
        s->sw = 1;
    }

    if (s->sw < 2) {
        if (val == 0xF0) {
            s->ann = ANN_RELEASE;
            return;
        } else if (val == 0xE0) {
            s->extended = 1;
            return;
        } else if (val == 0xFA) {
            s->ann = ANN_ACK;
            s->sw = 4;
        }
    }

    if (s->sw < 3) {
        const char *key_name = ps2kb_lookup_key(val, s->extended);
        if (key_name) {
            C_ANN_PUT(di, s->ss, end_sample, s->out_ann, s->ann, key_name);
        } else {
            char buf[16];
            if (s->extended)
                snprintf(buf, sizeof(buf), "[E0%02X]", val);
            else
                snprintf(buf, sizeof(buf), "[%02X]", val);
            C_ANN_PUT(di, s->ss, end_sample, s->out_ann, s->ann, buf);
        }
    }

    // Press 时输出 binary
    if (s->ann == ANN_PRESS && s->sw < 3) {
        const char *key_name = ps2kb_lookup_key(val, s->extended);
        if (key_name) {
            c_decoder_put_binary(di, s->ss, end_sample, s->out_binary,
                                 BINARY_KEYS, strlen(key_name), (const uint8_t *)key_name);
        }
    }

    // 重置状态
    s->sw = 0;
    s->ann = ANN_PRESS;
    s->extended = 0;
}
```

---

### 3.3 PS/2 Mouse 解码器 (`ps2_mouse_c.c`)

#### 3.3.1 Python 原始元数据

```python
id = 'ps2_mouse'
name = 'PS/2 Mouse'
longname = 'PS/2 Mouse'
desc = 'PS/2 mouse interface.'
license = 'gplv2+'
inputs = ['ps2']
outputs = []
tags = ['PC']
binary = (
    ('bytes', 'Bytes without explanation'),      # BINARY_BYTES = 0
    ('movement', 'Explanation of mouse movement'), # BINARY_MOVEMENT = 1
)
annotations = (
    ('Movement', 'Mouse movement packets'),  # ANN_MOVEMENT = 0
)
annotation_rows = (
    ('mov', 'Mouse Movement', (0,)),
)
```

#### 3.3.2 C 元数据映射

```c
#define ANN_MOVEMENT  0
#define NUM_ANN       1

#define BINARY_BYTES     0
#define BINARY_MOVEMENT  1

static const char *ps2mouse_ann_labels[][3] = {
    {"", "Movement", "Mouse movement packets"},
};

static const int ps2mouse_row_mov_classes[] = {ANN_MOVEMENT, -1};
static const struct srd_c_ann_row ps2mouse_ann_rows[] = {
    {"mov", "Mouse Movement", ps2mouse_row_mov_classes, 1},
};

static const struct srd_decoder_binary ps2mouse_binary[] = {
    {0, "bytes", "Bytes without explanation"},
    {1, "movement", "Explanation of mouse movement"},
};

static const char *ps2mouse_inputs[] = {"ps2", NULL};
static const char *ps2mouse_outputs[] = {NULL};
static const char *ps2mouse_tags[] = {"PC", NULL};
```

#### 3.3.3 解码逻辑分析

PS/2 Mouse 解码器维护一个数据包列表，按 3 字节一组处理鼠标移动数据：

**数据包结构**（3 字节）：
- Byte 0 (flags): Y overflow(bit7), X overflow(bit6), Y sign(bit5), X sign(bit4), M button(bit2), R button(bit1), L button(bit0)
- Byte 1 (x): X 位移值（有符号，受 bit4 符号扩展）
- Byte 2 (y): Y 位移值（有符号，受 bit5 符号扩展）

**状态机**：
1. 收集字节到 `packets` 列表
2. 当 `host` 方向改变时，输出当前收集的数据包
3. 特殊处理 ACK 字节（0xFA，非主机发送）
4. 每 3 个字节输出一次鼠标移动信息

**鼠标移动解码**：
- 按键状态：L(左键), M(中键), R(右键)
- X/Y 位移：有符号值，带溢出警告
- 无移动时输出 "No Movement"

#### 3.3.4 私有状态结构

```c
#define PS2MOUSE_MAX_PACKETS 16

typedef struct {
    uint8_t val;
    int is_host;
} ps2mouse_packet_entry;

typedef struct {
    int out_ann;
    int out_binary;
    ps2mouse_packet_entry packets[PS2MOUSE_MAX_PACKETS];
    int num_packets;
    uint64_t ss;
    uint64_t es;
} ps2mouse_state;
```

#### 3.3.5 recv_proto 实现（关键逻辑）

```c
static void ps2mouse_mouse_movement(struct srd_decoder_inst *di, ps2mouse_state *s)
{
    if (s->num_packets < 3) return;
    if (s->packets[0].is_host) return;

    uint8_t flags = s->packets[0].val;
    int x = s->packets[1].val;
    int y = s->packets[2].val;

    char msg[128];
    int pos = 0;

    if (flags & 1) pos += snprintf(msg + pos, sizeof(msg) - pos, "L");
    if (flags & 2) pos += snprintf(msg + pos, sizeof(msg) - pos, "M");
    if (flags & 4) pos += snprintf(msg + pos, sizeof(msg) - pos, "R");

    if (flags & 0x10) x -= 256;
    if (flags & 0x20) y -= 256;

    if (x != 0) pos += snprintf(msg + pos, sizeof(msg) - pos, " X%+d", x);
    if (flags & 0x40) pos += snprintf(msg + pos, sizeof(msg) - pos, "!!");
    if (y != 0) pos += snprintf(msg + pos, sizeof(msg) - pos, " Y%+d", y);
    if (flags & 0x80) pos += snprintf(msg + pos, sizeof(msg) - pos, "!!");

    if (pos == 0) snprintf(msg, sizeof(msg), "No Movement");

    C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_MOVEMENT, msg);

    // Binary movement output
    char bin_msg[256];
    int bpos = snprintf(bin_msg, sizeof(bin_msg), "\n%s", msg);
    c_decoder_put_binary(di, s->ss, s->es, s->out_binary,
                         BINARY_MOVEMENT, bpos, (const uint8_t *)bin_msg);
}

static void ps2mouse_print_packets(struct srd_decoder_inst *di, ps2mouse_state *s)
{
    ps2mouse_mouse_movement(di, s);

    // Binary bytes output
    const char *tag = s->packets[s->num_packets - 1].is_host ? "Host: " : "Mouse:";
    char octets[128];
    int opos = 0;
    for (int i = 0; i < s->num_packets && opos < 100; i++) {
        opos += snprintf(octets + opos, sizeof(octets) - opos, "%s%02X",
                         (i > 0) ? " " : "", s->packets[i].val);
    }
    char bin_out[256];
    int bpos = snprintf(bin_out, sizeof(bin_out), "\n%s %s", tag, octets);
    c_decoder_put_binary(di, s->ss, s->es, s->out_binary,
                         BINARY_BYTES, bpos, (const uint8_t *)bin_out);

    // 重置
    s->num_packets = 0;
}

static void ps2mouse_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ps2mouse_state *s = (ps2mouse_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "BYTE") != 0) return;
    if (!data || data_len < 4) return;

    uint8_t val = data[0];
    int is_host = data[1];

    if (s->num_packets == 0) {
        s->ss = start_sample;
    } else if (is_host != s->packets[s->num_packets - 1].is_host) {
        ps2mouse_print_packets(di, s);
        s->ss = start_sample;
        // 特殊处理 ACK
        if (val == 0xFA && !is_host) {
            char ack_bin[] = "\n ACK";
            c_decoder_put_binary(di, start_sample, end_sample, s->out_binary,
                                 BINARY_BYTES, sizeof(ack_bin) - 1, (const uint8_t *)ack_bin);
            s->num_packets = 0;
            return;
        }
    }

    if (s->num_packets < PS2MOUSE_MAX_PACKETS) {
        s->packets[s->num_packets].val = val;
        s->packets[s->num_packets].is_host = is_host;
        s->num_packets++;
    }
    s->es = end_sample;

    if (s->num_packets > 2) {
        ps2mouse_print_packets(di, s);
    }
}
```

---

### 3.4 USB Packet 解码器 (`usb_packet_c.c`)

#### 3.4.1 Python 原始元数据

```python
id = 'usb_packet'
name = 'USB packet'
longname = 'Universal Serial Bus (LS/FS) packet'
desc = 'USB (low-speed and full-speed) packet protocol.'
license = 'gplv2+'
inputs = ['usb_signalling']
outputs = ['usb_packet']
tags = ['PC']
options = (
    {'id': 'signalling', 'desc': 'Signalling',
     'default': 'full-speed', 'values': ('full-speed', 'low-speed'),
     'idn': 'dec_usb_packet_opt_signalling'},
)
annotations = (
    ('sync-ok', 'SYNC'),           # 0
    ('sync-err', 'SYNC (error)'),  # 1
    ('pid', 'PID'),                # 2
    ('framenum', 'FRAMENUM'),      # 3
    ('addr', 'ADDR'),              # 4
    ('ep', 'EP'),                  # 5
    ('crc5-ok', 'CRC5'),           # 6
    ('crc5-err', 'CRC5 (error)'),  # 7
    ('data', 'DATA'),              # 8
    ('crc16-ok', 'CRC16'),         # 9
    ('crc16-err', 'CRC16 (error)'),# 10
    ('packet-out', 'Packet: OUT'),    # 11
    ('packet-in', 'Packet: IN'),      # 12
    ('packet-sof', 'Packet: SOF'),    # 13
    ('packet-setup', 'Packet: SETUP'),# 14
    ('packet-data0', 'Packet: DATA0'),# 15
    ('packet-data1', 'Packet: DATA1'),# 16
    ('packet-data2', 'Packet: DATA2'),# 17
    ('packet-mdata', 'Packet: MDATA'),# 18
    ('packet-ack', 'Packet: ACK'),    # 19
    ('packet-nak', 'Packet: NAK'),    # 20
    ('packet-stall', 'Packet: STALL'),# 21
    ('packet-nyet', 'Packet: NYET'),  # 22
    ('packet-pre', 'Packet: PRE'),    # 23
    ('packet-err', 'Packet: ERR'),    # 24
    ('packet-split', 'Packet: SPLIT'),# 25
    ('packet-ping', 'Packet: PING'),  # 26
    ('packet-reserved', 'Packet: Reserved'), # 27
    ('packet-invalid', 'Packet: Invalid'),   # 28
)
annotation_rows = (
    ('fields', 'Packet fields', tuple(range(10 + 1))),
    ('packet', 'Packets', tuple(range(11, 28 + 1))),
)
```

#### 3.4.2 C 元数据映射

```c
#define ANN_SYNC_OK       0
#define ANN_SYNC_ERR      1
#define ANN_PID           2
#define ANN_FRAMENUM      3
#define ANN_ADDR          4
#define ANN_EP            5
#define ANN_CRC5_OK       6
#define ANN_CRC5_ERR      7
#define ANN_DATA          8
#define ANN_CRC16_OK      9
#define ANN_CRC16_ERR     10
#define ANN_PKT_OUT       11
#define ANN_PKT_IN        12
#define ANN_PKT_SOF       13
#define ANN_PKT_SETUP     14
#define ANN_PKT_DATA0     15
#define ANN_PKT_DATA1     16
#define ANN_PKT_DATA2     17
#define ANN_PKT_MDATA     18
#define ANN_PKT_ACK       19
#define ANN_PKT_NAK       20
#define ANN_PKT_STALL     21
#define ANN_PKT_NYET      22
#define ANN_PKT_PRE       23
#define ANN_PKT_ERR       24
#define ANN_PKT_SPLIT     25
#define ANN_PKT_PING      26
#define ANN_PKT_RESERVED  27
#define ANN_PKT_INVALID   28
#define NUM_ANN           29

static const char *usb_pkt_ann_labels[][3] = {
    {"", "sync-ok", "SYNC"},
    {"", "sync-err", "SYNC (error)"},
    {"", "pid", "PID"},
    {"", "framenum", "FRAMENUM"},
    {"", "addr", "ADDR"},
    {"", "ep", "EP"},
    {"", "crc5-ok", "CRC5"},
    {"", "crc5-err", "CRC5 (error)"},
    {"", "data", "DATA"},
    {"", "crc16-ok", "CRC16"},
    {"", "crc16-err", "CRC16 (error)"},
    {"", "packet-out", "Packet: OUT"},
    {"", "packet-in", "Packet: IN"},
    {"", "packet-sof", "Packet: SOF"},
    {"", "packet-setup", "Packet: SETUP"},
    {"", "packet-data0", "Packet: DATA0"},
    {"", "packet-data1", "Packet: DATA1"},
    {"", "packet-data2", "Packet: DATA2"},
    {"", "packet-mdata", "Packet: MDATA"},
    {"", "packet-ack", "Packet: ACK"},
    {"", "packet-nak", "Packet: NAK"},
    {"", "packet-stall", "Packet: STALL"},
    {"", "packet-nyet", "Packet: NYET"},
    {"", "packet-pre", "Packet: PRE"},
    {"", "packet-err", "Packet: ERR"},
    {"", "packet-split", "Packet: SPLIT"},
    {"", "packet-ping", "Packet: PING"},
    {"", "packet-reserved", "Packet: Reserved"},
    {"", "packet-invalid", "Packet: Invalid"},
};

static const int usb_pkt_row_fields_classes[] = {
    ANN_SYNC_OK, ANN_SYNC_ERR, ANN_PID, ANN_FRAMENUM, ANN_ADDR, ANN_EP,
    ANN_CRC5_OK, ANN_CRC5_ERR, ANN_DATA, ANN_CRC16_OK, ANN_CRC16_ERR, -1
};
static const int usb_pkt_row_packet_classes[] = {
    ANN_PKT_OUT, ANN_PKT_IN, ANN_PKT_SOF, ANN_PKT_SETUP,
    ANN_PKT_DATA0, ANN_PKT_DATA1, ANN_PKT_DATA2, ANN_PKT_MDATA,
    ANN_PKT_ACK, ANN_PKT_NAK, ANN_PKT_STALL, ANN_PKT_NYET,
    ANN_PKT_PRE, ANN_PKT_ERR, ANN_PKT_SPLIT, ANN_PKT_PING,
    ANN_PKT_RESERVED, ANN_PKT_INVALID, -1
};
static const struct srd_c_ann_row usb_pkt_ann_rows[] = {
    {"fields", "Packet fields", usb_pkt_row_fields_classes, 11},
    {"packet", "Packets", usb_pkt_row_packet_classes, 18},
};
```

#### 3.4.3 下层数据格式（usb_signalling_c → usb_packet_c）

usb_signalling_c 的 python 输出命令：
- `"SOP"` — 包开始，data = NULL
- `"BIT"` — 数据位，data = 1 byte ('0' or '1')
- `"STUFF BIT"` — 填充位，data = NULL
- `"EOP"` — 包结束，data = NULL
- `"ERR"` — 错误，data = NULL
- `"SYM"` — 符号，data = 符号名字符串
- `"RESET"` — 复位，data = NULL
- `"KEEP ALIVE"` — 保持活动，data = NULL

#### 3.4.4 解码逻辑分析

USB Packet 解码器是本批次最复杂的解码器。核心状态机：

**状态**：
- `WAIT_FOR_SOP`: 等待 SOP
- `GET_BIT`: 收集数据位

**数据位收集**：
- 收到 `BIT` 命令时，将位值和采样范围存入 `bits` 数组
- 收到 `EOP` 或 `ERR` 时，处理已收集的位

**包处理（handle_packet）**：
1. 检查 SYNC 字段（前 8 位，应为 `00000001`）
2. 解析 PID（8 位），查 PID 表获取包类型名
3. 根据 PID 类型处理不同字段：
   - **Token 包**（OUT/IN/SOF/SETUP/PING）：解析 ADDR(7bit) + EP(4bit) + CRC5(5bit)
   - **Data 包**（DATA0/DATA1/DATA2/MDATA）：解析数据字节 + CRC16(16bit)
   - **Handshake 包**（ACK/NAK/STALL/NYET/ERR）：仅 SYNC+PID
   - **Special 包**（PRE/SPLIT/Reserved）：特殊处理
4. CRC5/CRC16 校验
5. 输出包摘要标注

**CRC5 计算**：
```c
static uint8_t calc_crc5(const char *bitstr, int len) {
    uint8_t poly5 = 0x25;
    uint8_t crc5 = 0x1f;
    for (int i = 0; i < len; i++) {
        crc5 <<= 1;
        if ((bitstr[i] - '0') != (crc5 >> 5))
            crc5 ^= poly5;
        crc5 &= 0x1f;
    }
    crc5 ^= 0x1f;
    // 反转位序
    uint8_t out = 0;
    for (int i = 0; i < 5; i++) {
        if (crc5 >> i & 1)
            out |= (1 << (4 - i));
    }
    return out;
}
```

**CRC16 计算**：
```c
static uint16_t calc_crc16(const char *bitstr, int len) {
    uint32_t poly16 = 0x18005;
    uint32_t crc16 = 0xffff;
    for (int i = 0; i < len; i++) {
        crc16 <<= 1;
        if ((bitstr[i] - '0') != (crc16 >> 16))
            crc16 ^= poly16;
        crc16 &= 0xffff;
    }
    crc16 ^= 0xffff;
    // 反转位序
    uint16_t out = 0;
    for (int i = 0; i < 16; i++) {
        if (crc16 >> i & 1)
            out |= (1 << (15 - i));
    }
    return out;
}
```

**PID 查找表**：
```c
static const struct { const char *bitstr; const char *name; const char *desc; } pid_table[] = {
    // Token
    {"10000111", "OUT", "Address & EP number in host-to-function transaction"},
    {"10010110", "IN", "Address & EP number in function-to-host transaction"},
    {"10100101", "SOF", "Start-Of-Frame marker & frame number"},
    {"10110100", "SETUP", "Address & EP number in host-to-function SETUP"},
    // Data
    {"11000011", "DATA0", "Data packet PID even"},
    {"11010010", "DATA1", "Data packet PID odd"},
    {"11100001", "DATA2", "Data packet PID HS"},
    {"11110000", "MDATA", "Data packet PID HS for split"},
    // Handshake
    {"01001011", "ACK", "Receiver accepts error-free packet"},
    {"01011010", "NAK", "Receiver cannot accept"},
    {"01111000", "STALL", "EP halted"},
    {"01101001", "NYET", "No response yet"},
    // Special
    {"00111100", "PRE", "Host-issued preamble"},
    {"00011110", "SPLIT", "HS split transaction token"},
    {"00101101", "PING", "HS flow control probe"},
    {"00001111", "Reserved", "Reserved PID"},
};
```

#### 3.4.5 私有状态结构

```c
#define USB_PKT_MAX_BITS 4096

typedef struct {
    int state;           // WAIT_FOR_SOP / GET_BIT
    char bits[USB_PKT_MAX_BITS];  // 位字符串
    int bits_len;
    uint64_t bit_ss[USB_PKT_MAX_BITS]; // 每位的起始采样点
    uint64_t bit_es[USB_PKT_MAX_BITS]; // 每位的结束采样点
    uint64_t ss_packet;
    uint64_t es_packet;
    char packet_summary[256];
    int out_ann;
    int out_python;
} usb_pkt_state;
```

#### 3.4.6 recv_proto 实现（框架）

```c
static void usb_pkt_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    usb_pkt_state *s = (usb_pkt_state *)c_decoder_get_private(di);
    if (!s) return;

    // 仅处理 SOP, BIT, EOP, ERR
    if (strcmp(cmd, "SOP") == 0) {
        if (s->state != 0) return; // WAIT_FOR_SOP
        s->ss_packet = start_sample;
        s->bits_len = 0;
        s->packet_summary[0] = '\0';
        s->state = 1; // GET_BIT
    } else if (strcmp(cmd, "BIT") == 0) {
        if (s->state != 1) return;
        if (s->bits_len < USB_PKT_MAX_BITS && data && data_len > 0) {
            s->bits[s->bits_len] = (char)data[0]; // '0' or '1'
            s->bit_ss[s->bits_len] = start_sample;
            s->bit_es[s->bits_len] = end_sample;
            s->bits_len++;
        }
    } else if (strcmp(cmd, "EOP") == 0 || strcmp(cmd, "ERR") == 0) {
        if (s->state != 1) return;
        s->es_packet = end_sample;
        usb_pkt_handle_packet(di, s);
        s->bits_len = 0;
        s->packet_summary[0] = '\0';
        s->state = 0; // WAIT_FOR_SOP
    }
    // 忽略 STUFF BIT, SYM, RESET, KEEP ALIVE
}
```

#### 3.4.7 Python 输出格式

usb_packet_c 需要向 usb_request_c 输出数据，格式：

```c
// cmd = "PACKET", data 格式:
// data[0] = pcategory (0=TOKEN, 1=DATA, 2=HANDSHAKE, 3=SPECIAL)
// data[1] = pname_len
// data[2..2+pname_len-1] = pname 字符串
// 后续: pinfo 数据
```

或使用更简单的格式：
```c
// cmd = "PACKET", data = 结构化二进制
// pcategory 用字符串: "TOKEN", "DATA", "HANDSHAKE", "SPECIAL"
```

**建议使用 c_decoder_put_python 发送 PACKET 数据**，格式与 Python 版本对齐：
```c
// 对于 TOKEN 包:
c_decoder_put_python(di, ss, es, s->out_python, "PACKET", pkt_data, pkt_data_len);
// pkt_data 编码: pcategory_str\0pname_str\0[pinfo_bytes]
```

---

### 3.5 USB Request 解码器 (`usb_request_c.c`)

#### 3.5.1 Python 原始元数据

```python
id = 'usb_request'
name = 'USB request'
longname = 'Universal Serial Bus (LS/FS) transaction/request'
desc = 'USB (low-speed/full-speed) transaction/request protocol.'
license = 'gplv2+'
inputs = ['usb_packet']
outputs = ['usb_request']
options = (
    {'id': 'in_request_start', 'desc': 'Start IN requests on',
     'default': 'submit', 'values': ('submit', 'first-ack'),
     'idn': 'dec_usb_request_opt_in_request_start'},
)
tags = ['PC']
annotations = (
    ('request-setup-read', 'Setup: Device-to-host'),   # 0
    ('request-setup-write', 'Setup: Host-to-device'),   # 1
    ('request-bulk-read', 'Bulk: Device-to-host'),      # 2
    ('request-bulk-write', 'Bulk: Host-to-device'),     # 3
    ('error', 'Unexpected packet'),                      # 4
)
annotation_rows = (
    ('request-setup', 'USB SETUP', (0, 1)),
    ('request-in', 'USB BULK IN', (2,)),
    ('request-out', 'USB BULK OUT', (3,)),
    ('errors', 'Errors', (4,)),
)
binary = (
    ('pcap', 'PCAP format'),  # BINARY_PCAP = 0
)
```

#### 3.5.2 C 元数据映射

```c
#define ANN_SETUP_READ   0
#define ANN_SETUP_WRITE  1
#define ANN_BULK_READ    2
#define ANN_BULK_WRITE   3
#define ANN_ERROR        4
#define NUM_ANN          5

#define BINARY_PCAP      0

static const char *usb_req_ann_labels[][3] = {
    {"", "request-setup-read", "Setup: Device-to-host"},
    {"", "request-setup-write", "Setup: Host-to-device"},
    {"", "request-bulk-read", "Bulk: Device-to-host"},
    {"", "request-bulk-write", "Bulk: Host-to-device"},
    {"", "error", "Unexpected packet"},
};

static const int usb_req_row_setup_classes[] = {ANN_SETUP_READ, ANN_SETUP_WRITE, -1};
static const int usb_req_row_in_classes[] = {ANN_BULK_READ, -1};
static const int usb_req_row_out_classes[] = {ANN_BULK_WRITE, -1};
static const int usb_req_row_errors_classes[] = {ANN_ERROR, -1};
static const struct srd_c_ann_row usb_req_ann_rows[] = {
    {"request-setup", "USB SETUP", usb_req_row_setup_classes, 2},
    {"request-in", "USB BULK IN", usb_req_row_in_classes, 1},
    {"request-out", "USB BULK OUT", usb_req_row_out_classes, 1},
    {"errors", "Errors", usb_req_row_errors_classes, 1},
};

static const struct srd_decoder_binary usb_req_binary[] = {
    {0, "pcap", "PCAP format"},
};

static const char *usb_req_inputs[] = {"usb_packet", NULL};
static const char *usb_req_outputs[] = {"usb_request", NULL};
static const char *usb_req_tags[] = {"PC", NULL};

static struct srd_decoder_option usb_req_options[] = {
    {"in_request_start", "dec_usb_request_opt_in_request_start",
     "Start IN requests on", NULL, NULL},
};
```

#### 3.5.3 解码逻辑分析

USB Request 解码器跟踪 USB 事务和请求，按设备地址和端点组织。

**事务状态机**：
- `IDLE`: 空闲
- `TOKEN RECEIVED`: 已收到 TOKEN 包
- `DATA RECEIVED`: 已收到 DATA 包

**请求跟踪**：
- 按 `(addr, ep)` 对跟踪请求
- 支持 CONTROL（SETUP IN/OUT）、BULK IN/OUT 传输
- CONTROL 传输三阶段：SETUP → DATA → STATUS
- BULK 传输：IN/OUT + ACK

**PCAP 二进制输出**：
- 写入 PCAP 全局头
- 每个请求生成 SUBMIT 和 COMPLETE 记录
- 使用 Linux usbmon 格式

#### 3.5.4 私有状态结构

```c
#define USB_REQ_MAX_DATA 2048
#define USB_REQ_MAX_PENDING 64

typedef struct {
    int type;               // 0=None, 1=BULK IN, 2=BULK OUT, 3=SETUP IN, 4=SETUP OUT
    uint8_t setup_data[8];
    int setup_data_len;
    uint8_t data[USB_REQ_MAX_DATA];
    int data_len;
    uint16_t wLength;
    int handshake;          // 0=none, 1=ACK, 2=NAK, 3=STALL, 4=NYET, 5=timeout
    uint64_t ss;
    uint64_t es;
    uint64_t ss_data;
    int id;
    int addr;
    int ep;
} usb_req_request;

typedef struct {
    int out_ann;
    int out_binary;
    int transaction_state;   // 0=IDLE, 1=TOKEN_RECEIVED, 2=DATA_RECEIVED
    int transaction_type;    // 0=IN, 1=OUT, 2=SETUP
    int transaction_ep;
    int transaction_addr;
    uint8_t transaction_data[USB_REQ_MAX_DATA];
    int transaction_data_len;
    int handshake;
    uint64_t ss_transaction;
    uint64_t es_transaction;
    int request_id;
    int wrote_pcap_header;
    uint64_t samplerate;
    double secs_per_sample;
    int in_request_start;    // 0=submit, 1=first-ack
    usb_req_request requests[USB_REQ_MAX_PENDING];
} usb_req_state;
```

#### 3.5.5 recv_proto 实现（框架）

```c
static void usb_req_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    usb_req_state *s = (usb_req_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "PACKET") != 0) return;
    if (!data || data_len < 2) return;

    // 解析 pcategory 和 pname
    // pcategory: "TOKEN", "DATA", "HANDSHAKE", "SPECIAL"
    // pname: "OUT", "IN", "SOF", "SETUP", "DATA0", "DATA1", "ACK", "NAK", etc.

    // ... 根据 pcategory 和 pname 处理事务状态机
}
```

---

## 4. 前置修改：ps2_c.c 添加 Python 输出（已完成）

<!-- Updated: ps2_c.c 的 Python 输出已全部实现，本章节内容仅供参考，无需再执行 -->

**状态：已完成** ✅

ps2_c.c 已完成以下所有修改：

1. **`ps2_outputs`** → 已修改为 `{"ps2", NULL}`
2. **`ps2_priv.out_python`** → 已添加
3. **`ps2_start()`** → 已注册 `c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "ps2")`
4. **`ps2_handle_byte()`** → 已添加 `"BYTE"` 命令输出（4 字节：byte_val, is_host, parity_ok, has_ack）
5. **`srd_c_decoder`** → `.outputs = ps2_outputs, .num_outputs = 1`

---

## 5. CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加 5 个新解码器：

```cmake
cfp_c
ps2_keyboard_c
ps2_mouse_c
usb_packet_c
usb_request_c
```

---

## 6. 编码规范

### 6.1 文件命名
- `{decoder_id}_c.c`

### 6.2 ID 命名
- `.id = "xxx_c"` — 必须以 `_c` 结尾

### 6.3 Name 命名
- `.name = "XXX(C)"` — 显示名加 `(C)` 后缀

### 6.4 ann_labels 第一列
- 第一列必须为空字符串 `""`

### 6.5 所有 ann class 必须映射到 row
- 每个 annotation class 必须出现在某个 `annotation_rows` 的 `ann_classes` 数组中

### 6.6 recv_proto 使用
- 上层解码器的 `decode()` 函数为空：`static void xxx_decode(struct srd_decoder_inst *di) { (void)di; }`
- 核心逻辑在 `recv_proto()` 中

### 6.7 Options 初始化
- 所有 option 的 `def` 和 `values` 必须在 `srd_c_decoder_entry()` 中初始化

### 6.8 内存管理
- `reset()` 中使用 `g_malloc0` 分配私有状态
- `destroy()` 中使用 `g_free` 释放

### 6.9 输出注册
- `start()` 中使用 `c_decoder_register_output()` 注册所有输出
- 返回值为 output ID，存入私有状态

---

## 7. 测试验证策略

### 7.1 编译验证
- 确保每个 `.c` 文件能通过 CMake 编译为 DLL
- 确保无编译警告

### 7.2 加载验证
- 启动 PXView，确认解码器出现在解码器列表中
- 确认解码器可正确堆叠在下层解码器之上

### 7.3 功能验证
- 使用对应的测试信号文件验证解码输出
- 对比 Python 版本和 C 版本的输出一致性

### 7.4 回归验证
- ~~确认修改 ps2_c.c 后，ps2_c 本身功能不受影响~~ <!-- Updated: ps2_c.c 修改已完成且已验证，此项不再需要 -->
- 确认现有 C 解码器不受影响
