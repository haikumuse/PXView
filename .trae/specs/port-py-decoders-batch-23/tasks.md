# 移植任务分解 — Batch 23

## 任务总览

| 任务ID | 解码器 | 优先级 | 预估工时 | 依赖 |
|--------|--------|--------|----------|------|
| T1 | ad5626_c | P0 | 2h | 无 |
| T2 | ad79x0_c | P1 | 4h | T1 |
| T3 | a7105_c | P1 | 6h | T2 |
| T4 | ade77xx_c | P2 | 6h | T2 |
| T5 | adf435x_c | P2 | 8h | T2 |
| T6 | CMakeLists.txt 注册 | P0 | 0.5h | T1-T5 |

**建议实现顺序**: T1 → T2 → T3 → T4 → T5 → T6

---

## T1: ad5626_c — AD5626 12-bit nanoDAC

### 复杂度: ★★ (最简单)

### 子任务

| # | 子任务 | 详情 |
|---|--------|------|
| T1.1 | 创建文件 | `libsigrokdecode/c_decoders/ad5626_c.c` |
| T1.2 | 定义 enum 和 struct | `ANN_VOLTAGE=0, NUM_ANN=1`; `ad5626_state` 含 `data`, `ss`, `out_ann` |
| T1.3 | 定义 ann_labels | `{"", "voltage", "Voltage"}` |
| T1.4 | 定义 ann_rows | `{"voltages", "Voltages", {0}}` |
| T1.5 | 实现 `ad5626_reset` | `g_malloc0` 初始化 private data |
| T1.6 | 实现 `ad5626_start` | `c_decoder_register_output(di, SRD_OUTPUT_ANN, "ad5626")` |
| T1.7 | 实现 `ad5626_recv_proto` | 处理 CS-CHANGE 和 BITS 消息 |
| T1.8 | 实现 `ad5626_destroy` | `g_free` 释放 private data |
| T1.9 | 定义 `ad5626_c_decoder` struct | 填充所有元数据字段 |
| T1.10 | 实现 `srd_c_decoder_entry` | 返回 decoder 指针 |
| T1.11 | 实现 `srd_c_decoder_api_version` | 返回 `SRD_C_DECODER_API_VERSION` |

### recv_proto 核心逻辑

```
CS-CHANGE (0→1): data >>= 1; voltage = data / 1000.0; 输出 "%.3fV"; data = 0
CS-CHANGE (1→0): ss = start_sample
BITS: 收集 MOSI bits, MSB first: data = data | bit; data <<= 1
```

### 验证要点
- [ ] 12-bit DAC 值正确转换为电压
- [ ] CS 边沿检测正确
- [ ] BITS 消息解析正确

---

## T2: ad79x0_c — AD7910/AD7920 12-bit ADC

### 复杂度: ★★★

### 子任务

| # | 子任务 | 详情 |
|---|--------|------|
| T2.1 | 创建文件 | `libsigrokdecode/c_decoders/ad79x0_c.c` |
| T2.2 | 定义 enum 和 struct | `ANN_MODE=0, ANN_VOLTAGE=1, ANN_VALIDATION=2, NUM_ANN=3`; `ad79x0_state` |
| T2.3 | 定义 ann_labels | 3 个 annotation class |
| T2.4 | 定义 ann_rows | `modes`, `voltages`, `data_validation` |
| T2.5 | 定义 options | `vref` 选项 (默认 1.5V) |
| T2.6 | 实现 `ad79x0_reset` | 初始化 state, `samples_bit = -1`, `ss = -1` |
| T2.7 | 实现 `ad79x0_start` | 注册 output, 读取 vref 选项 |
| T2.8 | 实现 `ad79x0_metadata` | 处理 `SRD_CONF_SAMPLERATE` |
| T2.9 | 实现 `ad79x0_recv_proto` | 处理 CS-CHANGE 和 BITS 消息 |
| T2.10 | 实现 mode/validation/voltage 输出 | 三种模式判断和对应输出 |
| T2.11 | 实现 `ad79x0_destroy` | 释放 private data |
| T2.12 | 定义 `ad79x0_c_decoder` struct | 含 metadata 回调 |
| T2.13 | 实现 `srd_c_decoder_entry` | 含 vref 选项默认值 |

### recv_proto 核心逻辑

```
CS-CHANGE (0→1):
  data >>= 1
  nb_bits = (ss - start_sample) / samples_bit  // 需 samplerate
  if nb_bits >= 10:
    if data == 0xFFF: Power Up Mode, Invalid
    else: Normal Mode, 计算 vin, Complete/Incomplete
  elif nb_bits < 10: Power Down Mode, Invalid
CS-CHANGE (1→0): start_sample = ss; samples_bit = -1
BITS: 收集 MISO bits, MSB first; 计算 samples_bit
```

### 关键问题: samples_bit 计算

Python 版本从 bit 时间戳获取 `samples_bit`:
```python
self.samples_bit = miso[0][2] - miso[0][1]  # es - ss of first bit
```

C 版本 BITS 消息不含时间戳。**解决方案**:
- 使用 `metadata()` 回调获取 `samplerate`
- 从 CS-CHANGE 和 DATA 消息的时间差推算
- 或简化: 使用 DATA 消息的 byte 级时间来估算

### 验证要点
- [ ] vref 选项正确读取
- [ ] 三种模式 (Normal/PowerDown/PowerUp) 正确判断
- [ ] 电压计算正确: `vin = (data / 4095) * vref`
- [ ] Complete/Incomplete/Invalid 验证正确

---

## T3: a7105_c — AMICCOM A7105 2.4GHz Transceiver

### 复杂度: ★★★★

### 子任务

| # | 子任务 | 详情 |
|---|--------|------|
| T3.1 | 创建文件 | `libsigrokdecode/c_decoders/a7105_c.c` |
| T3.2 | 定义 enum 和 struct | `ANN_CMD=0, ANN_TX=1, ANN_RX=2, ANN_WARN=3, NUM_ANN=4`; `a7105_state` |
| T3.3 | 定义寄存器表 | 52 个寄存器的 name/size 映射 |
| T3.4 | 定义 ann_labels | 4 个 annotation class |
| T3.5 | 定义 ann_rows | `commands`, `warnings` |
| T3.6 | 实现 `a7105_reset` | 初始化 state, `cs_was_released = 0` |
| T3.7 | 实现 `a7105_start` | 注册 output |
| T3.8 | 实现 `a7105_parse_command` | 解析命令字节，返回命令名/寄存器/min/max |
| T3.9 | 实现 `a7105_decode_command` | 处理第一个字节 (命令) |
| T3.10 | 实现 `a7105_finish_command` | CS 上升沿时完成命令解析 |
| T3.11 | 实现 `a7105_decode_register` | 解码寄存器值 |
| T3.12 | 实现 `a7105_decode_mb_data` | 多字节数据解码 |
| T3.13 | 实现 `a7105_recv_proto` | 处理 DATA, CS-CHANGE, TRANSFER |
| T3.14 | 实现 `a7105_destroy` | 释放 private data |
| T3.15 | 定义 `a7105_c_decoder` struct | |
| T3.16 | 实现 `srd_c_decoder_entry` | |

### recv_proto 核心逻辑

```
DATA (first byte): parse_command(mosi)
DATA (subsequent): collect mosi/miso bytes
CS-CHANGE (0→1) or TRANSFER:
  if mb_count < min: warn "missing data bytes"
  elif mb_count > 0: finish_command()
  reset_cmd()
```

### 命令解析表

```c
typedef struct {
    const char *name;
    int reg;       // W/R_REGISTER 的寄存器地址
    int min_bytes; // 最小后续字节数
    int max_bytes; // 最大后续字节数
} a7105_cmd_info;

static int a7105_parse_command(uint8_t b, a7105_cmd_info *info)
{
    if (b == 0x05) { info->name = "W_TX_FIFO"; info->min = 1; info->max = 32; return 1; }
    if (b == 0x45) { info->name = "R_RX_FIFO"; info->min = 1; info->max = 32; return 1; }
    if (b == 0x06) { info->name = "W_ID"; info->min = 1; info->max = 4; return 1; }
    if (b == 0x46) { info->name = "R_ID"; info->min = 1; info->max = 4; return 1; }
    if ((b & 0x80) == 0) {
        if ((b & 0x40) == 0) {
            info->name = "W_REGISTER";
        } else {
            info->name = "R_REGISTER";
        }
        info->reg = b & 0x3F;
        info->min = 1;
        info->max = 1;
        return 1;
    }
    switch (b & 0xF0) {
        case 0x80: info->name = "SLEEP_MODE"; break;
        case 0x90: info->name = "IDLE_MODE"; break;
        case 0xA0: info->name = "STANDBY_MODE"; break;
        case 0xB0: info->name = "PLL_MODE"; break;
        case 0xC0: info->name = "RX_MODE"; break;
        case 0xD0: info->name = "TX_MODE"; break;
        case 0xE0: info->name = "FIFO_WRITE_PTR_RESET"; break;
        case 0xF0: info->name = "FIFO_READ_PTR_RESET"; break;
        default: return 0; // unknown
    }
    info->min = 0;
    info->max = 0;
    return 1;
}
```

### 验证要点
- [ ] 所有 14 种命令正确解析
- [ ] W_REGISTER/R_REGISTER 寄存器名正确映射
- [ ] 多字节命令 (FIFO/ID) 数据正确收集
- [ ] CS-CHANGE 和 TRANSFER 都能正确触发 finish_command
- [ ] 输出格式与 Python 版本兼容

---

## T4: ade77xx_c — ADE77xx Poly Phase Energy Metering IC

### 复杂度: ★★★★

### 子任务

| # | 子任务 | 详情 |
|---|--------|------|
| T4.1 | 创建文件 | `libsigrokdecode/c_decoders/ade77xx_c.c` |
| T4.2 | 定义 enum 和 struct | `ANN_READ=0, ANN_WRITE=1, ANN_WARN=2, NUM_ANN=3`; `ade77xx_state` |
| T4.3 | 转换寄存器表 | 从 `lists.py` 的 OrderedDict 转换为 C 数组 (57 个寄存器) |
| T4.4 | 定义 ann_labels | 3 个 annotation class |
| T4.5 | 定义 ann_rows | `read`, `write`, `warnings` |
| T4.6 | 实现 `ade77xx_reset` | 初始化 state |
| T4.7 | 实现 `ade77xx_start` | 注册 output |
| T4.8 | 实现 `ade77xx_recv_proto` | 处理 DATA 和 CS-CHANGE |
| T4.9 | 实现命令解析 | cmd byte: bit7=write, bit0-6=reg addr |
| T4.10 | 实现多字节数据组装 | 1-3 字节值组装 |
| T4.11 | 实现短传输警告 | CS 上升沿时数据不完整 |
| T4.12 | 实现 `ade77xx_destroy` | 释放 private data |
| T4.13 | 定义 `ade77xx_c_decoder` struct | |
| T4.14 | 实现 `srd_c_decoder_entry` | |

### 寄存器表转换工作量

`lists.py` 中有 57 个寄存器条目，格式为:
```python
(addr, (name, desc, access, bits, signed, default))
```

需要转换为 C 结构体数组:
```c
static const ade77xx_reg_info ade77xx_regs[0x80] = {
    [0x01] = {"AWATTHR", "Watt-Hour Accumulation...", "R", 16, 1, 1, 0x0},
    // ... 57 个条目
};
```

### 验证要点
- [ ] 57 个寄存器完整转换
- [ ] 1/2/3 字节寄存器值正确组装
- [ ] 短传输警告正确触发
- [ ] 未知寄存器警告正确触发
- [ ] 读/写 annotation 正确分类

---

## T5: adf435x_c — ADF4350/1 Wideband Synthesizer

### 复杂度: ★★★★★ (最复杂)

### 子任务

| # | 子任务 | 详情 |
|---|--------|------|
| T5.1 | 创建文件 | `libsigrokdecode/c_decoders/adf435x_c.c` |
| T5.2 | 定义 enum 和 struct | `ANN_REG=0, ANN_WARN=1, NUM_ANN=2`; `adf435x_state` |
| T5.3 | 定义字段描述结构体 | `adf435x_field_desc` 含 offset, width, name, parser, checker |
| T5.4 | 实现 Reg0 字段解析 | FRAC[14:3], INT[30:15] + INT checker |
| T5.5 | 实现 Reg1 字段解析 | MOD, Phase, Prescalar, Phase Adjust |
| T5.6 | 实现 Reg2 字段解析 | 13 个字段 (最复杂) |
| T5.7 | 实现 Reg3 字段解析 | 6 个字段 |
| T5.8 | 实现 Reg4 字段解析 | 11 个字段 |
| T5.9 | 实现 Reg5 字段解析 | 1 个字段 |
| T5.10 | 定义 ann_labels | 2 个 annotation class |
| T5.11 | 定义 ann_rows | `writes`, `warnings` |
| T5.12 | 实现 `adf435x_reset` | 初始化 state, `bit_count = 0` |
| T5.13 | 实现 `adf435x_start` | 注册 output |
| T5.14 | 实现 32-bit 字解码 | `adf435x_decode_word` |
| T5.15 | 实现字段值提取 | `adf435x_extract_field` |
| T5.16 | 实现 `adf435x_recv_proto` | 处理 BITS 和 TRANSFER |
| T5.17 | 实现所有 parser 函数 | disabled_enabled, output_power, cp_current, muxout, etc. |
| T5.18 | 实现所有 checker 函数 | INT range check |
| T5.19 | 实现 `adf435x_destroy` | 释放 private data |
| T5.20 | 定义 `adf435x_c_decoder` struct | |
| T5.21 | 实现 `srd_c_decoder_entry` | |

### 字段解析函数清单

| 函数名 | 用途 | 寄存器 |
|--------|------|--------|
| `adf435x_parse_disabled_enabled` | 0→"Disabled", 1→"Enabled" | Reg2,3,4 多处 |
| `adf435x_parse_output_power` | 0→"-4dBm", 1→"-1dBm", 2→"+2dBm", 3→"+5dBm" | Reg4 |
| `adf435x_parse_prescalar` | 0→"4/5", 1→"8/9" | Reg1 |
| `adf435x_parse_phase_adjust` | 0→"Off", 1→"On" | Reg1 |
| `adf435x_parse_pd_polarity` | 0→"Negative", 1→"Positive" | Reg2 |
| `adf435x_parse_ldp` | 0→"10ns", 1→"6ns" | Reg2 |
| `adf435x_parse_ldf` | 0→"FRAC-N", 1→"INT-N" | Reg2 |
| `adf435x_parse_cp_current` | 16 级电流值 | Reg2 |
| `adf435x_parse_muxout` | 8 种 MUXOUT 模式 | Reg2 |
| `adf435x_parse_low_noise_spur` | 4 种模式 | Reg2 |
| `adf435x_parse_clock_div_mode` | 4 种模式 | Reg3 |
| `adf435x_parse_abp` | 0→"6ns (FRAC-N)", 1→"3ns (INT-N)" | Reg3 |
| `adf435x_parse_band_select_clk` | 0→"Low", 1→"High" | Reg3 |
| `adf435x_parse_aux_output_select` | 0→"Divided Output", 1→"Fundamental" | Reg4 |
| `adf435x_parse_vco_powerdown` | 0→"VCO Powered Up", 1→"VCO Powered Down" | Reg4 |
| `adf435x_parse_rf_divider` | 2^v 格式 | Reg4 |
| `adf435x_parse_feedback_select` | 0→"Divided", 1→"Fundamental" | Reg4 |
| `adf435x_parse_ld_pin_mode` | 4 种模式 | Reg5 |
| `adf435x_check_int` | v < 23 → "Not Allowed" | Reg0 |

### 验证要点
- [ ] 32-bit 字正确从 BITS 消息组装
- [ ] 6 个寄存器所有字段正确解析
- [ ] 所有 parser 函数输出正确
- [ ] INT checker 正确触发警告
- [ ] 非 32-bit 传输正确报错
- [ ] 未知寄存器地址正确处理

---

## T6: CMakeLists.txt 注册

### 子任务

| # | 子任务 | 详情 |
|---|--------|------|
| T6.1 | 定位 C_DECODERS 列表 | 在 `CMakeLists.txt` 中找到 `set(C_DECODERS ...)` |
| T6.2 | 添加 5 个解码器 | 在列表末尾添加 `a7105_c ad5626_c ad79x0_c ade77xx_c adf435x_c` |
| T6.3 | 验证构建 | 运行 `build_incremental.cmd` 确认编译通过 |

---

## 通用实现模式

### 每个 C 解码器必须包含的函数

1. **`xxx_reset`**: 分配/清零 private data
2. **`xxx_start`**: 注册 output (`c_decoder_register_output`)
3. **`xxx_decode`**: 空函数体 `(void)di;`
4. **`xxx_destroy`**: 释放 private data (`g_free`)
5. **`xxx_recv_proto`**: 核心协议解析逻辑
6. **`srd_c_decoder_entry`**: 返回 decoder 指针，设置选项默认值
7. **`srd_c_decoder_api_version`**: 返回 `SRD_C_DECODER_API_VERSION`

### ann_labels 格式

```c
static const char *xxx_ann_labels[][3] = {
    {"", "id", "Full Label"},  // 第一列必须为 ""
    // ...
};
```

### annotation_rows 格式

```c
static const int xxx_row_xxx_classes[] = {ANN_XXX, -1};  // -1 终止
static const struct srd_c_ann_row xxx_ann_rows[] = {
    {"row_id", "Row Label", xxx_row_xxx_classes, count},
    // ...
};
```

### inputs 格式

```c
static const char *xxx_inputs[] = {"spi", NULL};
```
