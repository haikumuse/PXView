# 移植规格：jtag_avr / jtag_ejtag / jtag_stm32 Python→C 解码器

## 1. 概述

本规格描述将 3 个 JTAG 上层 Python 解码器移植为 C 解码器的详细方案。这三个解码器均以 `inputs=['jtag']` 堆叠在底层 JTAG 解码器之上，通过 `recv_proto()` 回调接收 JTAG TAP 状态变迁和数据，而非直接从 logic 信号采样。

### 1.1 移植目标

| Python ID | C ID | 名称 | 功能 |
|-----------|------|------|------|
| `jtag_avr` | `jtag_avr_c` | JTAG/AVR(C) | Atmel AVR PDI over JTAG 协议解码 |
| `jtag_ejtag` | `jtag_ejtag_c` | JTAG/EJTAG(C) | MIPS EJTAG 调试协议解码 |
| `jtag_stm32` | `jtag_stm32_c` | JTAG/STM32(C) | ST STM32 ARM Cortex-M3 JTAG 调试口解码 |

### 1.2 关键架构差异

| 维度 | Python 解码器 | C 解码器 |
|------|-------------|---------|
| 数据接收 | `decode(ss, es, data)` 方法，data 为 `(cmd, val)` 元组 | `recv_proto(di, ss, es, cmd, data, data_len)` 回调 |
| 位串表示 | Python 字符串 `'10110...'` | `unsigned char[]` 字节数组，LSB-first |
| 采样位置 | `val` 包含 `(bitstring, samplenums)` 二元组 | 仅收到 `cmd` + `data[]` + `data_len`，**无逐位采样位置信息** |
| Annotation 输出 | `self.put(ss, es, self.out_ann, [class, [texts]])` | `C_ANN_PUT(di, ss, es, out_ann, class, texts...)` |
| Python 输出 | `self.put(ss, es, self.out_python, ...)` | `c_decoder_put_python(di, ss, es, out_python, cmd, data, len)` |
| 状态管理 | Python class 实例变量 | `struct xxx_priv` + `c_decoder_get_private(di)` |
| 注册 | Python class 属性声明 | `struct srd_c_decoder` 静态结构体 |

### 1.3 recv_proto 机制详解

JTAG 上层 C 解码器**不实现 `decode()` 函数**（或实现为空函数），而是通过 `recv_proto` 回调接收底层 JTAG C 解码器通过 `c_decoder_put_python()` 发送的协议数据。

**调用链路**：
```
jtag_c.decode()
  → c_decoder_put_python(di, ss, es, out_python, "IR TDI", tdi_bytes, byte_count)
  → c_decoder_api.c: 遍历 di->next_di 链表
  → 对每个 C 实例: next_di->c_dec_inst->recv_proto(next_di, ss, es, cmd, data, data_len)
```

**JTAG C 解码器发送的协议命令**（参见 `jtag_c.c` 第272-275行）：

| cmd 字符串 | data 内容 | 触发时机 |
|-----------|----------|---------|
| `"IR TDI"` | IR 寄存器 TDI 位移数据（字节） | SHIFT_IR→EXIT1_IR 时 |
| `"IR TDO"` | IR 寄存器 TDO 位移数据（字节） | SHIFT_IR→EXIT1_IR 时 |
| `"DR TDI"` | DR 寄存器 TDI 位移数据（字节） | SHIFT_DR→EXIT1_DR 时 |
| `"DR TDO"` | DR 寄存器 TDO 位移数据（字节） | SHIFT_DR→EXIT1_DR 时 |
| `"NEW STATE"` | NULL, 0 | TAP 状态变迁时 |

**重要**：JTAG C 解码器**同时发送** `"IR TDI"` 和 `"IR TDO"` 命令（参见 `jtag_c.c` 第272-275行）。同样，`"DR TDI"` 和 `"DR TDO"` 也是同时发送的。Python 上层解码器仅处理 `IR TDI`（不处理 `IR TDO`），但需要处理 `DR TDO` 以获取 TDO 方向的 DR 数据。 <!-- Updated: 原描述"Python上层解码器通常只处理IR TDI"不够精确，补充说明DR TDO是被处理的 -->

**数据字节序**：`data[]` 为 LSB-first 字节数组。例如 4-bit IR 值 `0b0111` 存储为 `data[0] = 0x07`（bit0=1, bit1=1, bit2=1, bit3=0）。

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| jtag_c.c | 底层协议输出范本 | c_decoder_put_python()输出协议数据（IR TDI/TDO、DR TDI/TDO、NEW STATE） <!-- Updated: 补充缺失的底层范本引用 --> |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |

---

## 2. jtag_avr_c 详细移植规格

### 2.1 Python 原始元数据

```python
id = 'jtag_avr'
name = 'JTAG / AVR'
longname = 'Joint Test Action Group / Atmel AVR PDI'
desc = 'Atmel AVR PDI JTAG protocol.'
license = 'gplv2+'
inputs = ['jtag']
outputs = []
tags = ['Debug/trace']
```

### 2.2 C 解码器元数据映射

```c
.id = "jtag_avr_c",
.name = "JTAG/AVR(C)",
.longname = "Joint Test Action Group / Atmel AVR PDI (C)",
.desc = "Atmel AVR PDI JTAG protocol. (C implementation)",
.license = "gplv2+",
.inputs = (const char*[]){"jtag", NULL},
.num_inputs = 1,
.outputs = NULL,
.num_outputs = 0,
.tags = (const char*[]){"Debug/trace", NULL},
.num_tags = 1,
```

### 2.3 Annotations 映射

Python 定义 18 个 annotation classes：

| 索引 | Python ID | Python label | C ann_labels | C enum |
|------|-----------|-------------|-------------|--------|
| 0 | JTAG_ITEM | Item | `{"", "item", "Item"}` | `ANN_JTAG_ITEM` |
| 1 | JTAG_FIELD | Field | `{"", "field", "Field"}` | `ANN_JTAG_FIELD` |
| 2 | JTAG_COMMAND | Command | `{"", "command", "Command"}` | `ANN_JTAG_COMMAND` |
| 3 | JTAG_WARNING | Warning | `{"", "warning", "Warning"}` | `ANN_JTAG_WARNING` |
| 4 | DATA_IN | PDI data in | `{"", "data-in", "PDI data in"}` | `ANN_DATA_IN` |
| 5 | PARITY_IN_OK | Parity OK | `{"", "parity-in-ok", "Parity OK"}` | `ANN_PARITY_IN_OK` |
| 6 | PARITY_IN_ERR | Parity error | `{"", "parity-in-err", "Parity error"}` | `ANN_PARITY_IN_ERR` |
| 7 | DATA_OUT | PDI data out | `{"", "data-out", "PDI data out"}` | `ANN_DATA_OUT` |
| 8 | PARITY_OUT_OK | Parity OK | `{"", "parity-out-ok", "Parity OK"}` | `ANN_PARITY_OUT_OK` |
| 9 | PARITY_OUT_ERR | Parity error | `{"", "parity-out-err", "Parity error"}` | `ANN_PARITY_OUT_ERR` |
| 10 | BREAK | BREAK condition | `{"", "break", "BREAK condition"}` | `ANN_BREAK` |
| 11 | OPCODE | Instruction opcode | `{"", "opcode", "Instruction opcode"}` | `ANN_OPCODE` |
| 12 | DATA_PROG | Programmer data | `{"", "data-prog", "Programmer data"}` | `ANN_DATA_PROG` |
| 13 | DATA_DEV | Device data | `{"", "data-dev", "Device data"}` | `ANN_DATA_DEV` |
| 14 | PDI_BREAK | BREAK at PDI level | `{"", "pdi-break", "BREAK at PDI level"}` | `ANN_PDI_BREAK` |
| 15 | ENABLE | Enable PDI | `{"", "enable", "Enable PDI"}` | `ANN_ENABLE` |
| 16 | DISABLE | Disable PDI | `{"", "disable", "Disable PDI"}` | `ANN_DISABLE` |
| 17 | COMMAND | PDI command with data | `{"", "cmd-data", "PDI command with data"}` | `ANN_COMMAND` |

**annotation_rows 映射**：

| Python row_id | Python label | Python class indices | C classes 数组 |
|--------------|-------------|---------------------|---------------|
| items | Items | (0,) | `{ANN_JTAG_ITEM, -1}` |
| fields | Fields | (1,) | `{ANN_JTAG_FIELD, -1}` |
| commands | Commands | (2,) | `{ANN_JTAG_COMMAND, -1}` |
| warnings | Warnings | (3,) | `{ANN_JTAG_WARNING, -1}` |
| data_in | PDI Data (In) | (4,5,6) | `{ANN_DATA_IN, ANN_PARITY_IN_OK, ANN_PARITY_IN_ERR, -1}` |
| data_out | PDI Data (Out) | (7,8,9) | `{ANN_DATA_OUT, ANN_PARITY_OUT_OK, ANN_PARITY_OUT_ERR, -1}` |
| data_fields | PDI Data Fields | (10,) | `{ANN_BREAK, -1}` |
| pdi_fields | PDI Fields | (14,) | `{ANN_PDI_BREAK, -1}` |
| pdi_prog | PDI Programmer In | (11,12) | `{ANN_OPCODE, ANN_DATA_PROG, -1}` |
| pdi_dev | PDI Device Out | (13,) | `{ANN_DATA_DEV, -1}` |
| pdi_cmds | PDI Commands | (15,16,17) | `{ANN_ENABLE, ANN_DISABLE, ANN_COMMAND, -1}` |

### 2.4 IR 指令映射

Python 中 IR 映射为 4-bit 字符串键：
```python
ir = {
    '0011': ['IDCODE', 32],
    '0111': ['PDICOM', 9],
    '1111': ['BYPASS', 1],
}
```

C 中需要将 `data[]` 字节转为整数后提取低 4 位，再映射：
```c
// data[] 是 LSB-first 字节数组
// IR 值 = data[0] & 0x0F (假设 IR <= 8 bits)
// 映射: 0x3 → IDCODE, 0x7 → PDICOM, 0xF → BYPASS
```

### 2.5 状态机

```
IDLE → (IR TDI) → 根据 IR 值切换到 BYPASS/IDCODE/PDICOM
BYPASS → (DR TDI) → 输出 BYPASS 数据 → IDLE
IDCODE → (DR TDO) → 解码 IDCODE → IDLE
PDICOM → (DR TDI) → PDI 输入处理
PDICOM → (DR TDO) → PDI 输出处理
```

### 2.6 PDI 子协议解码

jtag_avr 的核心复杂度在于内嵌的 PDI (Program and Debug Interface) 子协议。PDI 通过 PDICOM DR 寄存器传输 9-bit 帧（8 data + 1 parity）。

**PDI 指令集**：

| Opcode | 助记符 | 描述 |
|--------|--------|------|
| 0 (0b000) | LDS | Load from data space |
| 1 (0b001) | LD | Load indirect from data space |
| 2 (0b010) | STS | Store to data space |
| 3 (0b011) | ST | Store indirect to data space |
| 4 (0b100) | LDCS | Load from control/status space |
| 5 (0b101) | STCS | Store to control/status space |
| 6 (0b110) | REPEAT | Repeat next instruction |
| 7 (0b111) | KEY | Access key |

**PDI 帧格式**：每帧 9 bits = 1 parity bit (MSB) + 8 data bits。Parity 为偶校验。

**PDI 解码状态**：
```c
struct pdi_state {
    int rep_count;           // REPEAT 计数
    int opcode;              // 当前指令 opcode
    int wr_counts[8];        // 写数据字节数队列
    int wr_count_len;        // 写队列长度
    int rd_counts[8];        // 读数据字节数队列
    int rd_count_len;        // 读队列长度
    uint8_t data_bytes[64];  // 累积数据字节
    int data_count;          // 当前累积字节数
    int data_expected;       // 期望数据字节数
    uint64_t ss_data;        // 数据起始采样号
    char cmd_parts_nice[128]; // 命令文本（详细）
    char cmd_parts_terse[64]; // 命令文本（简略）
    uint64_t ss_cmd;         // 命令起始采样号
};
```

### 2.7 关键 C 代码片段

#### recv_proto 入口

```c
static void jtag_avr_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    struct jtag_avr_priv *priv = (struct jtag_avr_priv *)c_decoder_get_private(di);
    if (!priv) return;

    priv->ss = start_sample;
    priv->es = end_sample;

    if (strcmp(cmd, "IR TDI") == 0) {
        // 从 data[] 提取 IR 值（4-bit, LSB-first）
        uint8_t ir_val = 0;
        if (data_len > 0) ir_val = data[0] & 0x0F;
        handle_ir_tdi(di, priv, ir_val);
    }
    else if (strcmp(cmd, "IR TDO") == 0) {
        // JTAG C解码器同时发送IR TDO（jtag_c.c第272-275行） <!-- Updated: 原行号"274-275"修正为"272-275"以覆盖完整put_python调用块 -->
        // Python上层解码器通常忽略IR TDO，但C版本可选择性处理
        // AVR PDI协议中IR TDO一般不使用，此处预留
        (void)data; (void)data_len;
    }
    else if (strcmp(cmd, "DR TDI") == 0) {
        handle_dr_tdi(di, priv, data, data_len);
    }
    else if (strcmp(cmd, "DR TDO") == 0) {
        handle_dr_tdo(di, priv, data, data_len);
    }
    // "NEW STATE" 不需要处理
}
```

#### IR TDI 处理

```c
static void handle_ir_tdi(struct srd_decoder_inst *di,
    struct jtag_avr_priv *priv, uint8_t ir_val)
{
    // IR 映射: 0x3=IDCODE, 0x7=PDICOM, 0xF=BYPASS
    if (ir_val == 0x3) {
        priv->state = AVR_STATE_IDCODE;
        C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ANN_JTAG_COMMAND, "IR: IDCODE");
    } else if (ir_val == 0x7) {
        priv->state = AVR_STATE_PDICOM;
        C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ANN_JTAG_COMMAND, "IR: PDICOM");
    } else if (ir_val == 0xF) {
        priv->state = AVR_STATE_BYPASS;
        C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ANN_JTAG_COMMAND, "IR: BYPASS");
    } else {
        priv->state = AVR_STATE_IDLE;
        char buf[32];
        snprintf(buf, sizeof(buf), "IR: UNKNOWN (0x%x)", ir_val);
        C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ANN_JTAG_COMMAND, buf);
    }
}
```

#### IDCODE 解码

```c
static void handle_reg_idcode(struct srd_decoder_inst *di,
    struct jtag_avr_priv *priv, const unsigned char *data, uint64_t data_len)
{
    // data[] 是 LSB-first 4 字节 IDCODE
    uint32_t idcode = 0;
    for (uint64_t i = 0; i < data_len && i < 4; i++)
        idcode |= ((uint32_t)data[i]) << (i * 8);

    uint8_t version = (idcode >> 28) & 0xF;
    uint16_t part = (idcode >> 12) & 0xFFFF;
    uint16_t manuf_raw = (idcode >> 1) & 0x7FF;

    const char *manuf = "INVALID";
    if (manuf_raw == 0x1f) manuf = "Atmel";

    char part_str[32];
    switch (part) {
        case 0x9642: snprintf(part_str, sizeof(part_str), "ATXMega64A3U"); break;
        case 0x9742: snprintf(part_str, sizeof(part_str), "ATXMega128A3U"); break;
        case 0x9744: snprintf(part_str, sizeof(part_str), "ATXMega192A3U"); break;
        case 0x9842: snprintf(part_str, sizeof(part_str), "ATXMega256A3U"); break;
        default: snprintf(part_str, sizeof(part_str), "0x%x", part); break;
    }

    char buf[128];
    snprintf(buf, sizeof(buf), "IDCODE: 0x%x (%s: %s@r%d)", idcode, manuf, part_str, version);
    C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ANN_JTAG_COMMAND, buf);
    C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ANN_JTAG_ITEM, buf);

    priv->state = AVR_STATE_IDLE;
}
```

#### PDI 奇偶校验

```c
// 检查 9-bit PDI 帧的偶校验
// frame_bits: LSB-first 字节数组中的 9 bits
// 返回: data (8-bit), parity_ok
static int pdi_check_parity(const unsigned char *frame_bits, int bit_count,
                            uint8_t *out_data, int *parity_ok)
{
    if (bit_count != 9) {
        *out_data = 0;
        *parity_ok = 0;
        return -1;
    }
    // bit0 = parity, bits[1:8] = data
    int parity = frame_bits[0] & 1;
    int ones = 0;
    uint8_t data_val = 0;
    for (int i = 1; i < 9; i++) {
        int bit = (frame_bits[i/8] >> (i%8)) & 1;
        data_val |= (bit << (i - 1));
        ones += bit;
    }
    *out_data = data_val;
    *parity_ok = ((ones + parity) % 2 == 0);
    return 0;
}
```

### 2.8 关键移植难点

1. **逐位采样位置缺失**：Python 版本通过 `samplenums` 数组可对每一位标注精确的 ss/es 范围。C 版本的 `recv_proto` 仅收到整个 DR/IR 位移区间的 ss/es，**无法实现 `putf(s, e, ...)` 精确位级标注**。解决方案：对 IDCODE 等寄存器的字段标注，使用整个区间的 ss/es，不做位级细分；或按比例估算位区间。

2. **PDI 帧边界**：Python 版本中 PDICOM DR 数据为 9-bit 帧，通过 `samplenums` 可精确定位每帧边界。C 版本中需根据 DR 数据长度推断帧数（每帧 9 bits），但无法获得帧间精确采样位置。

3. **PDI 状态机复杂度**：PDI 子协议有 8 种指令，每种有不同的读写数据字节数，加上 REPEAT 前缀修饰。需要完整实现 PDI 状态机。

---

## 3. jtag_ejtag_c 详细移植规格

### 3.1 Python 原始元数据

```python
id = 'jtag_ejtag'
name = 'JTAG / EJTAG'
longname = 'Joint Test Action Group / EJTAG (MIPS)'
desc = 'MIPS EJTAG protocol.'
license = 'gplv2+'
inputs = ['jtag']
outputs = []
tags = ['Debug/trace']
```

### 3.2 C 解码器元数据映射

```c
.id = "jtag_ejtag_c",
.name = "JTAG/EJTAG(C)",
.longname = "Joint Test Action Group / EJTAG (MIPS) (C)",
.desc = "MIPS EJTAG protocol. (C implementation)",
.license = "gplv2+",
.inputs = (const char*[]){"jtag", NULL},
.num_inputs = 1,
.outputs = NULL,
.num_outputs = 0,
.tags = (const char*[]){"Debug/trace", NULL},
.num_tags = 1,
```

### 3.3 Annotations 映射

Python 定义 13 个 annotation classes（1 instruction + 9 register + 2 control field + 1 pracc）：

| 索引 | Python ID | Python label | C ann_labels | C enum |
|------|-----------|-------------|-------------|--------|
| 0 | INSTRUCTION | Instruction | `{"", "instruction", "Instruction"}` | `ANN_INSTRUCTION` |
| 1 | reset | RESET | `{"", "reset", "RESET"}` | `ANN_REG_RESET` |
| 2 | device_id | DEVICE_ID | `{"", "device-id", "DEVICE_ID"}` | `ANN_REG_DEVICE_ID` |
| 3 | implementation | IMPLEMENTATION | `{"", "implementation", "IMPLEMENTATION"}` | `ANN_REG_IMPLEMENTATION` |
| 4 | data | DATA | `{"", "data", "DATA"}` | `ANN_REG_DATA` |
| 5 | address | ADDRESS | `{"", "address", "ADDRESS"}` | `ANN_REG_ADDRESS` |
| 6 | control | CONTROL | `{"", "control", "CONTROL"}` | `ANN_REG_CONTROL` |
| 7 | fastdata | FASTDATA | `{"", "fastdata", "FASTDATA"}` | `ANN_REG_FASTDATA` |
| 8 | pc_sample | PC_SAMPLE | `{"", "pc-sample", "PC_SAMPLE"}` | `ANN_REG_PC_SAMPLE` |
| 9 | bypass | BYPASS | `{"", "bypass", "BYPASS"}` | `ANN_REG_BYPASS` |
| 10 | CONTROL_FIELD_IN | Control field in | `{"", "control-field-in", "Control field in"}` | `ANN_CTRL_FIELD_IN` |
| 11 | CONTROL_FIELD_OUT | Control field out | `{"", "control-field-out", "Control field out"}` | `ANN_CTRL_FIELD_OUT` |
| 12 | PRACC | PrAcc | `{"", "pracc", "PrAcc"}` | `ANN_PRACC` |

**annotation_rows 映射**：

| Python row_id | Python label | C classes 数组 |
|--------------|-------------|---------------|
| instructions | Instructions | `{ANN_INSTRUCTION, -1}` |
| regs | Registers | `{ANN_REG_RESET, ANN_REG_DEVICE_ID, ANN_REG_IMPLEMENTATION, ANN_REG_DATA, ANN_REG_ADDRESS, ANN_REG_CONTROL, ANN_REG_FASTDATA, ANN_REG_PC_SAMPLE, ANN_REG_BYPASS, -1}` |
| control_fields_in | Control fields in | `{ANN_CTRL_FIELD_IN, -1}` |
| control_fields_out | Control fields out | `{ANN_CTRL_FIELD_OUT, -1}` |
| pracc | PrAcc | `{ANN_PRACC, -1}` |

### 3.4 EJTAG 指令映射

```c
// EJTAG 指令码到名称/描述
static const struct {
    uint8_t code;
    const char *name;
    const char *desc;
} ejtag_insn[] = {
    {0x00, "Free",        "Boundary scan"},
    {0x01, "IDCODE",      "Select Device Identification (ID) register"},
    {0x03, "IMPCODE",     "Select Implementation register"},
    {0x08, "ADDRESS",     "Select Address register"},
    {0x09, "DATA",        "Select Data register"},
    {0x0A, "CONTROL",     "Select EJTAG Control register"},
    {0x0B, "ALL",         "Select the Address, Data and EJTAG Control registers"},
    {0x0C, "EJTAGBOOT",   "Fetch code from the debug exception vector after reset"},
    {0x0D, "NORMALBOOT",  "Execute the reset handler after reset"},
    {0x0E, "FASTDATA",    "Select the Data and Fastdata registers"},
    {0x10, "TCBCONTROLA", "Select the control register TCBTraceControl"},
    {0x11, "TCBCONTROLB", "Selects trace control block register B"},
    {0x12, "TCBDATA",     "Access the registers specified by TCBCONTROLB"},
    {0x13, "TCBCONTROLC", "Select trace control block register C"},
    {0x14, "PCSAMPLE",    "Select the PCsample register"},
    {0x15, "TCBCONTROLD", "Select trace control block register D"},
    {0x16, "TCBCONTROLE", "Select trace control block register E"},
    {0x17, "FDC",         "Select Fast Debug Channel"},
    {0x1C, "Free",        "Boundary scan"},
    {0, NULL, NULL} // 终止符
};
```

### 3.5 状态机

```c
enum ejtag_state {
    EJTAG_STATE_RESET = 0,
    EJTAG_STATE_DEVICE_ID = 1,
    EJTAG_STATE_IMPLEMENTATION = 2,
    EJTAG_STATE_DATA = 3,
    EJTAG_STATE_ADDRESS = 4,
    EJTAG_STATE_CONTROL = 5,
    EJTAG_STATE_FASTDATA = 6,
    EJTAG_STATE_PC_SAMPLE = 7,
    EJTAG_STATE_BYPASS = 8,
};
```

**IR→State 映射**：
```c
static const struct { uint8_t ir; enum ejtag_state state; } ejtag_state_map[] = {
    {0x01, EJTAG_STATE_DEVICE_ID},
    {0x03, EJTAG_STATE_IMPLEMENTATION},
    {0x09, EJTAG_STATE_DATA},
    {0x08, EJTAG_STATE_ADDRESS},
    {0x0A, EJTAG_STATE_CONTROL},
    {0x0E, EJTAG_STATE_FASTDATA},
    {0, EJTAG_STATE_RESET},
};
```

### 3.6 Control Register 字段解析

EJTAG Control Register 是 32-bit 寄存器，Python 版本定义了 `ejtag_control_reg` 数组逐字段标注。C 版本需要完整移植：

```c
static const struct {
    int start_bit;  // 原始 bit 位（31-based，Python 风格）
    int end_bit;
    const char *name;
    const char *read_desc[4];  // 最多4个值描述
    int read_desc_count;
    const char *write_desc[4];
    int write_desc_count;
} ejtag_ctrl_fields[] = {
    {31, 31, "Rocc",     {"No reset occurred", "Reset occurred"}, 2,
                       {"Acknowledge reset", "No effect"}, 2},
    {30, 29, "Psz",      {"Access: byte", "Access: halfword", "Access: word", "Access: triple"}, 4,
                       {}, 0},
    {23, 23, "VPED",     {"VPE disabled", "VPE enabled"}, 2, {}, 0},
    {22, 22, "Doze",     {"Processor is not in low-power mode", "Processor is in low-power mode"}, 2, {}, 0},
    {21, 21, "Halt",     {"Internal system bus clock is running", "Internal system bus clock is stopped"}, 2, {}, 0},
    {20, 20, "Per Rst",  {"No peripheral reset applied", "Peripheral reset applied"}, 2,
                       {"Deassert peripheral reset", "Assert peripheral reset"}, 2},
    {19, 19, "PRn W",    {"Read processor access", "Write processor access"}, 2, {}, 0},
    {18, 18, "Pr Acc",   {"No pending processor access", "Pending processor access"}, 2,
                       {"Finish processor access", "Don't finish processor access"}, 2},
    {16, 16, "Pr Rst",   {"No processor reset applied", "Processor reset applied"}, 2,
                       {"Deassert processor reset", "Assert system reset"}, 2},
    {15, 15, "Prob En",  {"Probe will not serve processor accesses", "Probe will service processor accesses"}, 2, {}, 0},
    {14, 14, "Prob Trap", {"Default location", "DMSEG fetch"}, 2,
                       {"Set to default location", "Set to DMSEG fetch"}, 2},
    {13, 13, "ISA On Debug", {"MIPS32/MIPS64 ISA", "microMIPS ISA"}, 2,
                       {"Set to MIPS32/MIPS64 ISA", "Set to microMIPS ISA"}, 2},
    {12, 12, "EJTAG Brk", {"No pending debug interrupt", "Pending debug interrupt"}, 2,
                       {"No effect", "Request debug interrupt"}, 2},
    {3,  3,  "DM",       {"Not in debug mode", "In debug mode"}, 2, {}, 0},
};
```

### 3.7 PrAcc (Processor Access) 协议

PrAcc 是 EJTAG 的核心调试协议，通过 CONTROL 寄存器的 PrAcc 和 PRnW 位协调调试器与处理器之间的数据传输。

```c
struct pracc_state {
    uint32_t address_in;    // TDI 写入的地址
    uint32_t address_out;   // TDO 读出的地址
    uint32_t data_in;       // TDI 写入的数据
    uint32_t data_out;      // TDO 读出的数据
    int is_write;           // PRnW 位
    uint64_t ss;            // 起始采样号
    uint64_t es;            // 结束采样号
};
```

**PrAcc 检测逻辑**（Python `parse_pracc()` 翻译）：
```c
static void parse_pracc(struct srd_decoder_inst *di, struct ejtag_priv *priv)
{
    uint32_t ctrl_in = priv->last_data_in;
    uint32_t ctrl_out = priv->last_data_out;

    // 检查: TDI 中 PrAcc=0（调试器确认）且 TDO 中 PrAcc=1（处理器挂起）
    if (!((!(ctrl_in & (1 << 18))) && (ctrl_out & (1 << 18))))
        return;

    int pracc_write = (ctrl_out & (1 << 19)) != 0;
    char buf[256];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos, "PrAcc: %s",
                    pracc_write ? "Store" : "Load/Fetch");

    if (pracc_write) {
        if (priv->pracc.address_out != 0)
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", A: 0x%08X", priv->pracc.address_out);
        if (priv->pracc.data_out != 0)
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", D: 0x%08X", priv->pracc.data_out);
    } else {
        if (priv->pracc.address_out != 0)
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", A: 0x%08X", priv->pracc.address_out);
        if (priv->pracc.data_in != 0)
            pos += snprintf(buf + pos, sizeof(buf) - pos, ", D: 0x%08X", priv->pracc.data_in);
    }

    C_ANN_PUT(di, priv->pracc.ss, priv->pracc.es, priv->out_ann, ANN_PRACC, buf);
    memset(&priv->pracc, 0, sizeof(priv->pracc));
}
```

### 3.8 FASTDATA 处理

FASTDATA 寄存器为 33-bit（32-bit data + 1-bit SPrAcc）。Python 版本通过 `bitstring[32]` 获取 SPrAcc 位。C 版本需要从 `data[]` 中提取第 33 位。

```c
static void handle_fastdata(struct srd_decoder_inst *di,
    struct ejtag_priv *priv, const unsigned char *data, uint64_t data_len,
    int ann_class)
{
    // data[] 为 LSB-first, 33 bits = 5 bytes (仅使用低33位)
    uint32_t fastdata_val = 0;
    for (int i = 0; i < 4 && i < (int)data_len; i++)
        fastdata_val |= ((uint32_t)data[i]) << (i * 8);

    int spracc = (data_len > 4) ? (data[4] & 1) : 0;

    char buf[64];
    snprintf(buf, sizeof(buf), "0x%08X", fastdata_val);
    C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ann_class, buf);

    // SPrAcc 标注
    if (ann_class == ANN_CTRL_FIELD_IN) {
        // 写方向
        const char *spracc_desc = spracc ? "SPrAcc: 1, No effect" : "SPrAcc: 0, Request completion";
        C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ann_class, spracc_desc);
    } else {
        // 读方向
        const char *spracc_desc = spracc ? "SPrAcc: 1, Successful completion" : "SPrAcc: 0, Fastdata access failure";
        C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ann_class, spracc_desc);
    }
}
```

### 3.9 关键移植难点

1. **Control Register 逐位标注**：Python 版本通过 `samplenums` 可精确标注每个字段的 ss/es。C 版本无法实现，只能使用整个 DR 区间的 ss/es。

2. **last_data 机制**：Python 版本维护 `last_data['in']` 和 `last_data['out']`，在 `UPDATE-DR` 状态时触发 CONTROL 寄存器解析。C 版本需在 `NEW STATE` 命令中检测 `UPDATE-DR`。

3. **FASTDATA 的 33-bit 数据**：需要正确处理 5 字节数据中的第 33 位。

4. **IR TDO 和 DR TDO 处理**：JTAG C 解码器同时发送 `"IR TDO"` 和 `"DR TDO"` 命令（jtag_c.c 第272-275行）。EJTAG 协议需要 DR TDO 来获取 TDO 方向的数据（如 IDCODE 读出、CONTROL 寄存器读出），IR TDO 在 EJTAG 中一般不使用但应在 recv_proto 中预留处理分支。参考 `lm75_c.c` 和 `ds1307_c.c` 的 recv_proto 模式实现标准。 <!-- Updated: 行号"274-275"修正为"272-275" -->

---

## 4. jtag_stm32_c 详细移植规格

### 4.1 Python 原始元数据

```python
id = 'jtag_stm32'
name = 'JTAG / STM32'
longname = 'Joint Test Action Group / ST STM32'
desc = 'ST STM32-specific JTAG protocol.'
license = 'gplv2+'
inputs = ['jtag']
outputs = []
tags = ['Debug/trace']
```

### 4.2 C 解码器元数据映射

```c
.id = "jtag_stm32_c",
.name = "JTAG/STM32(C)",
.longname = "Joint Test Action Group / ST STM32 (C)",
.desc = "ST STM32-specific JTAG protocol. (C implementation)",
.license = "gplv2+",
.inputs = (const char*[]){"jtag", NULL},
.num_inputs = 1,
.outputs = NULL,
.num_outputs = 0,
.tags = (const char*[]){"Debug/trace", NULL},
.num_tags = 1,
```

### 4.3 Annotations 映射

Python 定义 4 个 annotation classes（最简单的 JTAG 上层解码器）：

| 索引 | Python ID | Python label | C ann_labels | C enum |
|------|-----------|-------------|-------------|--------|
| 0 | item | Item | `{"", "item", "Item"}` | `ANN_ITEM` |
| 1 | field | Field | `{"", "field", "Field"}` | `ANN_FIELD` |
| 2 | command | Command | `{"", "command", "Command"}` | `ANN_COMMAND` |
| 3 | warning | Warning | `{"", "warning", "Warning"}` | `ANN_WARNING` |

**annotation_rows 映射**：

| Python row_id | Python label | C classes 数组 |
|--------------|-------------|---------------|
| items | Items | `{ANN_ITEM, -1}` |
| fields | Fields | `{ANN_FIELD, -1}` |
| commands | Commands | `{ANN_COMMAND, -1}` |
| warnings | Warnings | `{ANN_WARNING, -1}` |

### 4.4 IR 指令映射

STM32 有两个串联 TAP：Boundary Scan TAP（5-bit IR）和 Cortex-M3 TAP（4-bit IR），共 9-bit IR。

```c
// M3 TAP IR 映射（IR[3:0]）
static const struct {
    uint8_t ir;
    const char *name;
    int reg_size;
} stm32_ir_map[] = {
    {0xF, "BYPASS", 1},
    {0xE, "IDCODE", 32},
    {0xA, "DPACC",  35},
    {0xB, "APACC",  35},
    {0x8, "ABORT",  35},
};

// BS TAP IR 映射（IR[8:4]）
// 目前仅 BYPASS (0x1F)
```

**IR 值提取**：
```c
// data[] 为 LSB-first, 9 bits = 2 bytes
// IR[3:0] = M3 TAP = data[0] & 0x0F
// IR[8:4] = BS TAP = (data[0] >> 4) | ((data[1] & 1) << 4)
uint8_t m3_ir = data[0] & 0x0F;
uint8_t bs_ir = (data[0] >> 4) | ((data_len > 1 ? data[1] : 0) << 4);
```

### 4.5 状态机

```c
enum stm32_state {
    STM32_STATE_IDLE = 0,
    STM32_STATE_BYPASS,
    STM32_STATE_IDCODE,
    STM32_STATE_DPACC,
    STM32_STATE_APACC,
    STM32_STATE_ABORT,
    STM32_STATE_UNKNOWN,
};
```

**状态转换逻辑**：
```
IDLE → (IR TDI) → 根据 M3 TAP IR 值切换
BYPASS → (DR TDI) → 输出 BYPASS → IDLE
IDCODE → (DR TDO) → 解码 IDCODE → IDLE
ABORT → (DR TDO) → 解码 ABORT → IDLE
UNKNOWN → (DR TDO) → 输出未知指令 → IDLE
DPACC → (DR TDI/DR TDO) → 解码 DPACC → (DR TDO后) IDLE
APACC → (DR TDI/DR TDO) → 解码 APACC → (DR TDO后) IDLE
```

### 4.6 IDCODE 解码

```c
static void handle_reg_idcode(struct srd_decoder_inst *di,
    struct stm32_priv *priv, const unsigned char *data, uint64_t data_len)
{
    // STM32 IDCODE 为 33-bit (1-bit BS TAP BYPASS + 32-bit M3 IDCODE)
    // 但 Python 版本先 bits[1:] 去掉最高位
    // data[] LSB-first: 32-bit M3 IDCODE + 1-bit BS BYPASS
    uint32_t idcode = 0;
    int bit_count = (data_len > 4) ? 32 : (int)(data_len * 8);
    for (int i = 0; i < 4 && i < (int)data_len; i++)
        idcode |= ((uint32_t)data[i]) << (i * 8);

    uint8_t version = (idcode >> 28) & 0xF;
    uint16_t part = (idcode >> 12) & 0xFFFF;
    uint8_t cc = (idcode >> 8) & 0xF;
    uint8_t ic = (idcode >> 1) & 0x7F;

    const char *ver_str = "UNKNOWN";
    if (version == 0x3) ver_str = "JTAG-DP";
    else if (version == 0x2) ver_str = "SW-DP";

    const char *part_str = "UNKNOWN";
    if (part == 0xba00) part_str = "JTAG-DP";
    else if (part == 0xba10) part_str = "SW-DP";

    const char *manuf_str = "UNKNOWN";
    if (cc == 4 && ic == 0x3b) manuf_str = "ARM Ltd.";

    char buf[256];
    snprintf(buf, sizeof(buf), "IDCODE: 0x%x (%s: %s/%s)", idcode, manuf_str, ver_str, part_str);
    C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ANN_COMMAND, buf);
}
```

### 4.7 DPACC/APACC 解码

DPACC 和 APACC 寄存器为 35-bit，格式如下：

**TDI 方向（data_in）**：
- Bits[34:3] = DATA[31:0]：32-bit 数据
- Bits[2:1] = A[3:2]：2-bit 地址
- Bits[0:0] = RnW：读(1)/写(0) 请求

**TDO 方向（data_out）**：
- Bits[34:3] = DATA[31:0]：32-bit 读出数据
- Bits[2:0] = ACK[2:0]：3-bit 应答

```c
static void handle_reg_dpacc_apacc(struct srd_decoder_inst *di,
    struct stm32_priv *priv, const char *cmd,
    const unsigned char *data, uint64_t data_len, int is_apacc)
{
    // data[] LSB-first, 35 bits = 5 bytes
    // 提取 35-bit 值
    uint64_t val = 0;
    for (int i = 0; i < 5 && i < (int)data_len; i++)
        val |= ((uint64_t)data[i]) << (i * 8);
    val &= 0x7FFFFFFFFULL; // 35-bit mask

    const char *reg_name = is_apacc ? "APACC" : "DPACC";
    char buf[256];

    if (strcmp(cmd, "DR TDI") == 0) {
        // 输入方向
        uint32_t data_val = (uint32_t)((val >> 3) & 0xFFFFFFFF);
        uint8_t a = (uint8_t)((val >> 1) & 0x3);
        int rnw = (int)(val & 1);

        const char *reg;
        char a_str[32];
        if (!is_apacc) {
            // DP 寄存器映射
            switch (a) {
                case 0: reg = "Reserved"; break;
                case 1: reg = "DP CTRL/STAT"; break;
                case 2: reg = "DP SELECT"; break;
                case 3: reg = "DP RDBUFF"; break;
                default: reg = "?"; break;
            }
        } else {
            snprintf(a_str, sizeof(a_str), "0x%x", a);
            reg = a_str;
        }

        snprintf(buf, sizeof(buf), "New transaction: DATA: 0x%x, A: %s, RnW: %s",
                 data_val, reg, rnw ? "Read request" : "Write request");
    } else {
        // 输出方向
        uint32_t data_val = (uint32_t)((val >> 3) & 0xFFFFFFFF);
        uint8_t ack = (uint8_t)(val & 0x7);

        const char *ack_str = "Reserved";
        if (ack == 1) ack_str = "WAIT";
        else if (ack == 2) ack_str = "OK/FAULT";

        snprintf(buf, sizeof(buf), "Previous transaction result: DATA: 0x%x, ACK: %s",
                 data_val, ack_str);
    }

    C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ANN_COMMAND, buf);
}
```

### 4.8 ABORT 寄存器解码

```c
static void handle_reg_abort(struct srd_decoder_inst *di,
    struct stm32_priv *priv, const unsigned char *data, uint64_t data_len)
{
    uint64_t val = 0;
    for (int i = 0; i < 5 && i < (int)data_len; i++)
        val |= ((uint64_t)data[i]) << (i * 8);

    int dapabort = (int)(val & 1);
    char buf[128];
    snprintf(buf, sizeof(buf), "DAPABORT = %d: %sDAP abort generated",
             dapabort, dapabort ? "" : "No ");
    C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ANN_COMMAND, buf);

    // 检查保留位
    if ((val >> 1) != 0) {
        C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ANN_WARNING,
                  "WARNING: DAPABORT[31:1] reserved!");
    }
}
```

### 4.9 关键移植难点

1. **9-bit IR 拆分**：STM32 有两个串联 TAP，IR 为 9-bit，需拆分为 BS TAP（5-bit）和 M3 TAP（4-bit）。

2. **35-bit DR 数据**：DPACC/APACC 为 35-bit 数据，需要正确处理 5 字节 LSB-first 数据。

3. **IDCODE 33-bit**：IDCODE DR 为 33-bit（1-bit BS BYPASS + 32-bit M3 IDCODE），Python 版本用 `bits[1:]` 去掉 BS BYPASS 位。

4. **逐位标注缺失**：Python 版本对 IDCODE 字段（Reserved, Manufacturer, Part, Version）有精确的位级标注。C 版本无法实现，需简化为整体标注。

5. **IR TDO 和 DR TDO 处理**：JTAG C 解码器同时发送 `"IR TDO"` 和 `"DR TDO"` 命令（jtag_c.c 第272-275行）。STM32 协议需要 DR TDO 来获取 TDO 方向的数据（如 IDCODE 读出、DPACC/APACK 读出），IR TDO 在 STM32 中一般不使用但应在 recv_proto 中预留处理分支。参考 `lm75_c.c` 和 `ds1307_c.c` 的 recv_proto 模式实现标准。 <!-- Updated: 行号"274-275"修正为"272-275" -->

---

## 5. 通用 C 解码器结构模板

### 5.1 私有数据结构模板

```c
struct jtag_xxx_priv {
    int state;              // 当前状态机状态
    uint64_t ss;            // 当前区间起始采样号
    uint64_t es;            // 当前区间结束采样号
    int out_ann;            // annotation 输出 ID
    // 解码器特有字段...
};
```

### 5.2 函数模板

```c
// reset: 分配并初始化私有数据
static void jtag_xxx_reset(struct srd_decoder_inst *di);

// start: 注册输出
static void jtag_xxx_start(struct srd_decoder_inst *di);

// decode: 空函数（上层解码器不直接采样）
static void jtag_xxx_decode(struct srd_decoder_inst *di) { (void)di; }

// recv_proto: 接收底层 JTAG 协议数据
static void jtag_xxx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    struct jtag_xxx_priv *priv = (struct jtag_xxx_priv *)c_decoder_get_private(di);
    if (!priv) return;

    priv->ss = start_sample;
    priv->es = end_sample;

    if (strcmp(cmd, "IR TDI") == 0) {
        // 从 data[] 提取 IR 值，切换状态机
        handle_ir_tdi(di, priv, data, data_len);
    }
    else if (strcmp(cmd, "IR TDO") == 0) {
        // JTAG C解码器同时发送IR TDO（jtag_c.c第272-275行） <!-- Updated: 原行号"274-275"修正为"272-275"以覆盖完整put_python调用块 -->
        // 可选择性处理：某些协议需要IR TDO方向数据
        handle_ir_tdo(di, priv, data, data_len);
    }
    else if (strcmp(cmd, "DR TDI") == 0) {
        handle_dr_tdi(di, priv, data, data_len);
    }
    else if (strcmp(cmd, "DR TDO") == 0) {
        handle_dr_tdo(di, priv, data, data_len);
    }
    else if (strcmp(cmd, "NEW STATE") == 0) {
        // TAP状态变迁通知，可用于检测UPDATE-DR等
        handle_new_state(di, priv);
    }
}

// destroy: 释放私有数据
static void jtag_xxx_destroy(struct srd_decoder_inst *di);
```

### 5.3 srd_c_decoder 结构体模板

```c
struct srd_c_decoder jtag_xxx_c_decoder = {
    .id = "jtag_xxx_c",
    .name = "JTAG/XXX(C)",
    .longname = "... (C)",
    .desc = "... (C implementation)",
    .license = "gplv2+",
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
    .inputs = (const char*[]){"jtag", NULL},
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = (const char*[]){"Debug/trace", NULL},
    .num_tags = 1,
    .reset = jtag_xxx_reset,
    .start = jtag_xxx_start,
    .decode = jtag_xxx_decode,
    .destroy = jtag_xxx_destroy,
    .recv_proto = jtag_xxx_recv_proto,
};
```

### 5.4 入口函数模板

```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    // 初始化 options 默认值（如果有）
    return &jtag_xxx_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
```

---

## 6. 辅助函数

### 6.1 LSB-first 字节数组转整数

```c
// 将 LSB-first 字节数组转为最多 64-bit 整数
static uint64_t bytes_to_uint64(const unsigned char *data, uint64_t data_len)
{
    uint64_t val = 0;
    for (uint64_t i = 0; i < data_len && i < 8; i++)
        val |= ((uint64_t)data[i]) << (i * 8);
    return val;
}
```

### 6.2 LSB-first 字节数组转 bit 字符串

```c
// 将 LSB-first 字节数组转为 bit 字符串（MSB-first 显示）
// 用于调试和兼容 Python 版本的 bit string 操作
static void bytes_to_bitstring(const unsigned char *data, uint64_t data_len,
                               int bit_count, char *out, int out_size)
{
    int pos = 0;
    for (int i = bit_count - 1; i >= 0 && pos < out_size - 1; i--) {
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        if (byte_idx < (int)data_len)
            out[pos++] = ((data[byte_idx] >> bit_idx) & 1) ? '1' : '0';
    }
    out[pos] = '\0';
}
```

---

## 7. CMakeLists.txt 修改

在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加：

```cmake
jtag_avr_c
jtag_ejtag_c
jtag_stm32_c
```

每个解码器会自动编译为 `build.dir/decoders/c_decoders/jtag_avr_c.dll` 等。

---

## 8. 与 Python 版本的功能对比

| 功能 | Python 版本 | C 版本 | 差异说明 |
|------|-----------|--------|---------|
| IR 指令解码 | ✅ | ✅ | 完全等价 |
| DR 数据解码 | ✅ | ✅ | 完全等价 |
| 逐位采样位置标注 | ✅ `putf(s,e)` | ❌ | C recv_proto 无逐位信息，简化为区间标注 |
| IDCODE 字段细分 | ✅ 每字段独立 ss/es | ⚠️ 简化 | 使用整体区间或按比例估算 |
| Control Register 字段标注 | ✅ 精确位级 | ⚠️ 简化 | 使用整体区间 |
| PrAcc 协议解析 | ✅ | ✅ | 完全等价 |
| FASTDATA 解码 | ✅ | ✅ | 完全等价 |
| PDI 子协议 | ✅ | ✅ | 完全等价（jtag_avr） |
| DPACC/APACC 解码 | ✅ | ✅ | 完全等价（jtag_stm32） |
| ABORT 保留位警告 | ✅ | ✅ | 完全等价 |
| NEW STATE 处理 | ✅ | ✅ | 完全等价 |
