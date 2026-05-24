# Python → C 解码器移植规格 — Batch 10

本规格覆盖 5 个 Python 解码器到 C 的移植：**mvb, mcs48, one_single_wire, ook, opentherm**。

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |

## 通用 C 解码器实现规范

### 文件命名
- 文件名：`{decoder_id}_c.c`，其中 `-` 替换为 `_`
- 例：`one_single_wire` → `one_single_wire_c.c`

### struct srd_c_decoder 约定
- `.id = "xxx_c"`（加 `_c` 后缀）
- `.name = "XXX(C)"`（加 `(C)` 后缀）
- `.longname` 和 `.desc` 末尾加 `(C implementation)`

### ann_labels 约定
- 第一列必须为 `""`（空字符串），API 内部处理 i+7 偏移
- 格式：`{"", "id", "Label"}`

### annotation_rows 约定
- 所有 annotation class 必须映射到某个 annotation_row
- 行定义使用 `struct srd_c_ann_row`，class 数组以 `-1` 结尾

### samplerate timing guard
- 必须实现 `metadata` 回调接收 `SRD_CONF_SAMPLERATE`
- `decode()` 入口处检查 `samplerate == 0` 则直接 return

### Condition Builder API
- `c_cond_new()` → 构建条件 → `c_cond_rise/fall/edge/high/low/skip/or/wait` → `c_cond_wait()` → `c_cond_free()`
- 每次 `c_cond_wait()` 返回后检查 `ret != SRD_OK` 则 return

### 输出 API
- `C_ANN_PUT(di, ss, es, out_ann, class, ...)` — 注解输出，支持 1-5 个字符串参数
- `c_decoder_put_python(di, ss, es, out_python, "TYPE", data, len)` — 上层解码器输出
- `c_decoder_put_binary(di, ss, es, out_binary, class, data, len)` — 二进制输出
- `c_decoder_put_logic(di, ss, es, out_logic, channel_mask, values, num_channels)` — 逻辑信号输出 <!-- Updated: c_decoder_put_logic已实现 -->

### 初始引脚状态获取 <!-- Updated: 新增c_cond_wait_current和c_decoder_get_initial_pin说明 -->
- `c_cond_wait_current(di, &samplenum)` — 等效于 Python 的 `self.wait({})`，获取当前样本位置
- `c_decoder_get_initial_pin(di, ch)` — 直接获取初始引脚状态（不推进样本位置）

### Options 初始化
- 在 `srd_c_decoder_entry()` 中用 `g_variant_new_*()` 初始化
- 字符串选项用 `g_variant_new_string()`
- 整数选项用 `g_variant_new_int64()`
- 浮点选项用 `g_variant_new_double()`
- 枚举值列表用 `GSList` + `g_slist_append()`

### Build 集成
- 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加解码器名称

---

## 1. MVB (Multifunction Vehicle Bus)

### 1.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `mvb` |
| name | `MVB` |
| longname | `Multifunction Vehicle Bus` |
| desc | `Multifunction Vehicle Bus Manchester II with custom preamble.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| channels | `{'id':'mvb', 'name':'MVB', 'desc':'TTL from RS485'}` |
| optional_channels | `()` |
| tags | `['Frame']` |

### 1.2 Annotations (9 个)

| 索引 | id | label |
|------|----|-------|
| 0 | master_preamble | Master preamble |
| 1 | slave_preamble | Slave preamble |
| 2 | master_data | Master data |
| 3 | f_code | Function code |
| 4 | slave_data | Slave data |
| 5 | crc | CRC |
| 6 | crc_error | CRC Error |
| 7 | bit | Bit |
| 8 | addr | Address |

### 1.3 Annotation Rows (5 个)

| row id | label | 包含的 annotation 索引 |
|--------|-------|----------------------|
| bits | Bits | 0, 1, 7 |
| crcs | Check sequence | 5 |
| ma-sl-data | Data | 2, 4 |
| f_codes | Function code | 3, 8 |
| errors | Decoding errors | 6 |

### 1.4 C 实现计划

**文件名**: `mvb_c.c`

**状态机**:
```
FIND_START → 检测 preamble → 解码 Manchester bit → 处理帧
```

**核心算法**:
1. **Preamble 检测**: 滑动窗口匹配 18-bit PREAMBLE_MASTER (0b101100011100010101) 和 PREAMBLE_SLAVE (0b101010100011100011)
2. **Manchester 解码**: 通过 edge 间隔计算 tick 数，交替 phase 产生 tick 值（0 或 1），偶数 tick 对解码：`last_tick=0, current=1 → bit 0`；`last_tick=1, current=0 → bit 1`
3. **CRC 校验**: 使用多项式 `11100101` 的 Modulo-2 除法 + parity + invert
4. **Master 帧**: 4-bit flag + 12-bit address + 8-bit CRC
5. **Slave 帧**: 16/32/64-bit data + 8-bit CRC（可变长度，按 72-bit 段分割）

**状态结构体**:
```c
struct mvb_priv {
    int state;              // FIND_START or DECODING
    uint64_t matching_header_ticks;
    int received_master_header;
    int received_slave_header;
    int last_tick;
    int is_even_tick;
    char decoded_buffer[512]; // Manchester decoded bits as "0"/"1" string
    int decoded_len;
    uint64_t frame_data_begin;
    uint64_t mvb_samples_per_bit;
    uint64_t sample_begin;
    uint64_t sample_end;
    uint64_t samplerate;
    int out_ann;
};
```

**关键常量**:
```c
#define PREAMBLE_MASTER 0x16465   // 0b101100011100010101
#define PREAMBLE_SLAVE  0x15463   // 0b101010100011100011
#define PREAMBLE_LENGTH 18
#define PREAMBLE_MASK   0x3FFFF   // 18-bit mask
#define MVB_CLOCK_RATE  3000000ULL // 3 MHz
```

**F_codes 查找表** (16 项):
```c
static const char *F_codes[] = {
    "PD 2B", "PD 4B", "PD 8B", "PD 16B", "PD 32B",
    "reserved", "reserved", "reserved",
    "Master transfer", "General event",
    "reserved", "reserved",
    "MD", "Group event", "Single event", "Device status"
};
```

**CRC 实现要点**:
- `mod2div()`: Modulo-2 除法，使用字符串操作模拟二进制除法
- `parity()`: 所有位的 XOR
- `invert()`: 所有位取反
- `encode_data()`: 计算 CRC 余数 = `invert(mod2div(data + '0'*(n-1), poly) + parity(data + remainder))`
- `check_check_sequence()`: 比较计算的 CRC 和接收的 CRC

**decode() 逻辑**:
1. 计算 `samples_per_tick = samplerate / MVB_CLOCK_RATE`
2. 计算 `mvb_samples_per_bit = 2 * samples_per_tick`
3. 等待第一个下降沿 `c_cond_fall(cb, 0)`
4. 主循环：等待边沿 `c_cond_edge(cb, 0)`，计算 notch 长度
5. 将 notch 长度转换为 tick 数，对每个 tick 调用 `process_tick()`
6. `process_tick()` 实现 preamble 匹配和 Manchester 解码
7. 帧结束时调用 `process_master_frame()` 或 `process_slave_frame()`

**注意事项**:
- CRC 算法使用字符串操作，C 中需要用位操作实现等效逻辑
- `decoded_buffer` 是字符串形式的二进制位，C 中可用 `uint8_t` 数组 + 位计数替代
- `bits_to_bytes()` 将二进制字符串转为字节数组

### 1.5 C 代码关键片段 — CRC 校验

```c
// CRC-8 with polynomial 0xE5 (11100101)
// Python uses string-based mod2div; C uses bit operations
static uint8_t mvb_crc8(const uint8_t *data, int bit_len)
{
    // data is array of bits (0 or 1), bit_len is number of bits
    // polynomial: 11100101 (8 bits)
    uint8_t crc = 0;
    for (int i = 0; i < bit_len; i++) {
        uint8_t msb = (crc >> 7) & 1;
        crc = (crc << 1) & 0xFF;
        if (msb ^ data[i]) {
            crc ^= 0xE5; // polynomial
        }
    }
    return crc;
}

static int check_check_sequence(const uint8_t *frame_bits, int total_bits)
{
    // Calculate parity of data + remainder
    int parity = 0;
    for (int i = 0; i < total_bits - 8; i++)
        parity ^= frame_bits[i];

    // Calculate CRC of data portion
    uint8_t calc_crc = mvb_crc8(frame_bits, total_bits - 8);

    // Invert CRC + parity
    uint8_t check = calc_crc ^ 0xFF; // invert
    check ^= parity; // XOR with parity

    // Compare with received check sequence
    uint8_t received = 0;
    for (int i = 0; i < 8; i++)
        received = (received << 1) | frame_bits[total_bits - 8 + i];

    return check == received;
}
```

### 1.6 C 代码关键片段 — Manchester 解码与 Preamble 匹配

```c
static int process_tick(struct srd_decoder_inst *di, struct mvb_priv *s, int tick_value, uint64_t samplenum)
{
    if (!s->received_master_header && !s->received_slave_header) {
        s->matching_header_ticks = ((s->matching_header_ticks << 1) | tick_value) & PREAMBLE_MASK;
        if (s->matching_header_ticks == PREAMBLE_MASTER) {
            s->received_master_header = 1;
            s->matching_header_ticks = 0;
            C_ANN_PUT(di, samplenum - (PREAMBLE_LENGTH * s->mvb_samples_per_bit / 2),
                      s->sample_end, s->out_ann, ANN_MASTER_PREAMBLE, "Master p");
            s->frame_data_begin = s->sample_end;
        }
        if (s->matching_header_ticks == PREAMBLE_SLAVE) {
            s->received_slave_header = 1;
            s->matching_header_ticks = 0;
            C_ANN_PUT(di, samplenum - (PREAMBLE_LENGTH * s->mvb_samples_per_bit / 2),
                      s->sample_end, s->out_ann, ANN_SLAVE_PREAMBLE, "Slave p");
            s->frame_data_begin = s->sample_end;
        }
        return 1; // continue
    }

    // Manchester decoding
    if (!s->is_even_tick) {
        uint64_t bit_begin = s->sample_end - s->mvb_samples_per_bit;
        if (s->last_tick == 0 && tick_value == 1) {
            s->decoded_buffer[s->decoded_len++] = 0;
            C_ANN_PUT(di, bit_begin, s->sample_end, s->out_ann, ANN_BIT, "0");
        } else if (s->last_tick == 1 && tick_value == 0) {
            s->decoded_buffer[s->decoded_len++] = 1;
            C_ANN_PUT(di, bit_begin, s->sample_end, s->out_ann, ANN_BIT, "1");
        } else {
            // End of frame - same bit twice means transition error = frame boundary
            if (s->received_master_header)
                process_master_frame(di, s);
            if (s->received_slave_header)
                process_slave_frame(di, s);
            reset_frame(s);
            return 0;
        }
    }
    s->is_even_tick = !s->is_even_tick;
    s->last_tick = tick_value;
    return 1;
}
```

---

## 2. MCS-48 (Intel MCS-48)

### 2.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `mcs48` |
| name | `MCS-48` |
| longname | `Intel MCS-48` |
| desc | `Intel MCS-48 external memory access protocol.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| channels | ale(0), psen(1), d0-d7(2-9), a8-a11(10-13) |
| optional_channels | a12(14) |
| tags | `['Retro computing']` |

**channels 详细列表**:
| 索引 | id | name | desc |
|------|----|------|------|
| 0 | ale | ALE | Address latch enable |
| 1 | psen | /PSEN | Program store enable |
| 2 | d0 | D0 | CPU data line 0 |
| 3 | d1 | D1 | CPU data line 1 |
| 4 | d2 | D2 | CPU data line 2 |
| 5 | d3 | D3 | CPU data line 3 |
| 6 | d4 | D4 | CPU data line 4 |
| 7 | d5 | D5 | CPU data line 5 |
| 8 | d6 | D6 | CPU data line 6 |
| 9 | d7 | D7 | CPU data line 7 |
| 10 | a8 | A8 | CPU address line 8 |
| 11 | a9 | A9 | CPU address line 9 |
| 12 | a10 | A10 | CPU address line 10 |
| 13 | a11 | A11 | CPU address line 11 |

**optional_channels**:
| 索引 | id | name | desc |
|------|----|------|------|
| 14 | a12 | A12 | CPU address line 12 |

### 2.2 Annotations (1 个)

| 索引 | id | label |
|------|----|-------|
| 0 | romdata | Address:Data |

### 2.3 Binary (1 个)

| 索引 | id | label |
|------|----|-------|
| 0 | romdata | AAAA:DD |

### 2.4 C 实现计划

**文件名**: `mcs48_c.c`

**状态机**: 非常简单，无显式状态机。等待两个条件之一：
1. ALE 下降沿 → 锁存地址
2. /PSEN 上升沿 → 读取数据

**核心逻辑**:
1. ALE 下降沿时：从 D0-D7 和 A8-A11(可选 A12) 重建地址
   - `addr = (A8-A11 << 8) | (D0-D7)` — 高 4 位地址来自 port P2，低 8 位来自数据总线复用
   - 保存 `addr_s = samplenum`
2. /PSEN 上升沿时：从 D0-D7 读取数据
   - `data = sum(bit << i for i, bit in enumerate(data))`
   - 保存 `data_s = samplenum`
   - 如果 `started`（已收到过 ALE），输出 `AAAA:DD` 格式注解和二进制

**状态结构体**:
```c
struct mcs48_priv {
    uint16_t addr;
    uint64_t addr_s;
    uint8_t data;
    uint64_t data_s;
    int started;        // Flag: have we received an ALE pulse?
    int has_bank;       // Has optional A12 channel
    int out_ann;
    int out_bin;
};
```

**decode() 逻辑**:
```c
while (1) {
    srd_cond_builder *cb = c_cond_new();
    c_cond_fall(cb, 0);   // ALE falling edge
    c_cond_or(cb);
    c_cond_rise(cb, 1);   // /PSEN rising edge
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;

    // Read all channel values at samplenum
    int ale = c_decoder_get_pin(di, 0, samplenum);
    int psen = c_decoder_get_pin(di, 1, samplenum);
    int d[8], a[4], bank;
    for (int i = 0; i < 8; i++)
        d[i] = c_decoder_get_pin(di, 2 + i, samplenum);
    for (int i = 0; i < 4; i++)
        a[i] = c_decoder_get_pin(di, 10 + i, samplenum);
    if (s->has_bank)
        bank = c_decoder_get_pin(di, 14, samplenum);

    if (matched & (1ULL << 0)) { // ALE falling edge
        s->started = 1;
        uint16_t addr = 0;
        for (int i = 0; i < 4; i++)
            addr |= (a[i] << (i + 8));
        for (int i = 0; i < 8; i++)
            addr |= (d[i] << i);
        if (s->has_bank)
            addr |= (bank << 12);
        s->addr = addr;
        s->addr_s = samplenum;
    }
    if (matched & (1ULL << 1)) { // /PSEN rising edge
        uint8_t data = 0;
        for (int i = 0; i < 8; i++)
            data |= (d[i] << i);
        s->data = data;
        s->data_s = samplenum;
        if (s->started) {
            char text[16];
            snprintf(text, sizeof(text), "%04X:%02X", s->addr, s->data);
            C_ANN_PUT(di, s->addr_s, s->data_s, s->out_ann, 0, text);
            // Binary output: 2 bytes addr (big-endian) + 1 byte data
            uint8_t bindata[3];
            bindata[0] = (s->addr >> 8) & 0xFF;
            bindata[1] = s->addr & 0xFF;
            bindata[2] = s->data;
            c_decoder_put_binary(di, s->addr_s, s->data_s, s->out_bin, 0, bindata, 3);
        }
    }
}
```

**注意事项**:
- 14 个必需 channel + 1 个可选 channel，是所有解码器中最多的
- `has_bank` 通过 `c_decoder_has_channel(di, 14)` 检测
- 地址重建：高 4 位(A8-A11) 左移 8 位后与低 8 位(D0-D7) 或运算
- 两个条件可能同时匹配（`self.matched` 可以有多个 bit 置位）

---

## 3. OneSingleWire

### 3.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `OneSingleWire` |
| name | `OneSingleWire custom bus` |
| longname | `OneSingleWire custom bus used in roboSet` |
| desc | `Bidirectional, half-duplex, asynchronous serial bus.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['OneSingleWire']` |
| channels | osw(0), strt(1) |
| optional_channels | `()` |
| tags | `['Custom']` |

**channels 详细列表**:
| 索引 | id | name | desc |
|------|----|------|------|
| 0 | osw | OSW | OSW signal line |
| 1 | strt | Start Pulse | OSW device start pulse signal |

### 3.2 Options (1 个)

| id | desc | default | type |
|----|------|---------|------|
| threshold | Threshold time value (us) | 8 | int |

### 3.3 Annotations (5 个)

| 索引 | id | label |
|------|----|-------|
| 0 | bit | Bit |
| 1 | byte | Byte |
| 2 | sample | Sample |
| 3 | wait | Wait |
| 4 | pb | PB (Parity Bit) |

### 3.4 Annotation Rows (3 个)

| row id | label | 包含的 annotation 索引 |
|--------|-------|----------------------|
| bits | Bits | 0, 3 |
| bytes | Bytes | 1, 4 |
| samples | Samples | 2 |

### 3.5 C 实现计划

**文件名**: `one_single_wire_c.c`

**状态机**: 简单线性状态
1. 等待 strt 上升沿
2. 等待 osw 下降沿（开始位）
3. 循环：等待 osw 边沿，测量周期，根据阈值判断 bit 值
4. 9 个 bit（8 数据 + 1 校验）后输出 Wait，然后重新开始

**核心算法**:
- 每个 bit 的值由当前边沿到上一个边沿的采样数决定
- 如果 `period_range < threshold_samples_num` → bit = 1（短周期）
- 如果 `period_range >= threshold_samples_num` → bit = 0（长周期）
- `threshold_samples_num = threshold_us * samplerate / 1000000`
- 8 个数据位 LSB-first，第 9 位是 parity（所有位的 XOR 应为 0）

**状态结构体**:
```c
struct osw_priv {
    uint64_t bt_block_ss;   // 当前 bit 块起始 sample
    uint64_t by_block_ss;   // 当前 byte 块起始 sample
    int bit_index;           // 当前 bit 索引 (0-8 = data+parity, 9+ = wait)
    uint8_t decoded_byte;   // 解码中的字节值
    int parity_bit;         // 校验位累计
    uint64_t threshold_samples; // 阈值（采样数）
    uint64_t samplerate;
    int out_ann;
    // int out_python;  <!-- Updated: Python源码未注册OUTPUT_PYTHON，暂不需要 -->
};
```

**decode() 逻辑**:
```c
// Phase 1: Wait for start pulse
srd_cond_builder *cb = c_cond_new();
c_cond_rise(cb, 1);  // strt rising edge
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);

// Phase 2: Wait for signal falling edge
cb = c_cond_new();
c_cond_fall(cb, 0);  // osw falling edge
ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);

s->bt_block_ss = samplenum;
s->by_block_ss = samplenum;

// Phase 3: Main decode loop
while (1) {
    cb = c_cond_new();
    c_cond_edge(cb, 0);  // osw edge
    ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;

    uint64_t period_range = samplenum - s->bt_block_ss;
    if (s->bit_index < 9) {
        int osw = (period_range < s->threshold_samples) ? 1 : 0;
        s->decoded_byte |= (osw << s->bit_index);
        s->parity_bit ^= osw;
        if (s->bit_index == 7) {
            char byte_str[32];
            snprintf(byte_str, sizeof(byte_str), "Byte: %d", s->decoded_byte);
            char val_str[8];
            snprintf(val_str, sizeof(val_str), "%d", s->decoded_byte);
            C_ANN_PUT(di, s->by_block_ss, samplenum, s->out_ann, ANN_BYTE, byte_str, val_str);
        } else if (s->bit_index == 8) {
            const char *pstr = (s->parity_bit == 0) ? "OK" : "ERR";
            char pb_str[32];
            snprintf(pb_str, sizeof(pb_str), "Parity check: %s", pstr);
            C_ANN_PUT(di, s->bt_block_ss, samplenum, s->out_ann, ANN_PB, pb_str, pstr);
        }
        char bit_str[16], bit_short[4];
        snprintf(bit_str, sizeof(bit_str), "Bit: %d", osw);
        snprintf(bit_short, sizeof(bit_short), "%d", osw);
        C_ANN_PUT(di, s->bt_block_ss, samplenum, s->out_ann, ANN_BIT, bit_str, bit_short);
        char samp_str[32], samp_short[16];
        snprintf(samp_str, sizeof(samp_str), "Samples: %d", (int)period_range);
        snprintf(samp_short, sizeof(samp_short), "%d", (int)period_range);
        C_ANN_PUT(di, s->bt_block_ss, samplenum, s->out_ann, ANN_SAMPLE, samp_str, samp_short);
        s->bit_index++;
    } else {
        C_ANN_PUT(di, s->bt_block_ss, samplenum, s->out_ann, ANN_WAIT, "Wait", "w");
        char samp_str[32], samp_short[16];
        snprintf(samp_str, sizeof(samp_str), "Samples: %d", (int)period_range);
        snprintf(samp_short, sizeof(samp_short), "%d", (int)period_range);
        C_ANN_PUT(di, s->bt_block_ss, samplenum, s->out_ann, ANN_SAMPLE, samp_str, samp_short);
        s->by_block_ss = samplenum;
        s->decoded_byte = 0;
        s->parity_bit = 0;
        s->bit_index = 0;
    }
    s->bt_block_ss = samplenum;
}
```

**注意事项**:
- Python 版本虽然声明 `outputs = ['OneSingleWire']`，但实际代码中只注册了 `OUTPUT_ANN`，未注册 `OUTPUT_PYTHON`。C 版本可暂不实现 Python 输出，仅实现注解输出 <!-- Updated: 修正了错误的out_python描述，Python源码实际未注册OUTPUT_PYTHON -->
- `threshold` 选项在 `metadata()` 回调中计算为采样数
- Python 中 `i` 从 0 开始，bit 0-7 是数据位（LSB first），bit 8 是 parity
- Parity 检查：所有 9 个 bit 的 XOR 应为 0

---

## 4. OOK (On-Off Keying)

### 4.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `ook` |
| name | `OOK` |
| longname | `On-off keying` |
| desc | `On-off keying protocol.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['ook']` |
| channels | data(0) |
| optional_channels | `()` |
| tags | `['Encoding']` |

### 4.2 Options (5 个)

| id | desc | default | values |
|----|------|---------|--------|
| invert | Invert data | 'no' | ('no', 'yes') |
| decodeas | Decode type | 'Manchester' | ('NRZ', 'Manchester', 'Diff Manchester') |
| preamble | Preamble | 'auto' | ('auto', '1010', '1111') |
| preamlen | Filter length | '7' | ('0','3','4','5','6','7','8','9','10') |
| diffmanvar | Transition at start | '1' | ('1', '0') |

### 4.3 Annotations (6 个)

| 索引 | id | label |
|------|----|-------|
| 0 | frame | Frame |
| 1 | info | Info |
| 2 | 1111 | 1111 |
| 3 | 1010 | 1010 |
| 4 | diffman | Diff man |
| 5 | nrz | NRZ |

### 4.4 Annotation Rows (6 个)

| row id | label | 包含的 annotation 索引 |
|--------|-------|----------------------|
| frames | Framing | 0 |
| info-vals | Info | 1 |
| man1111 | Man 1111 | 2 |
| man1010 | Man 1010 | 3 |
| diffmans | Diff man | 4 |
| nrz-vals | NRZ | 5 |

### 4.5 Binary (1 个)

| 索引 | id | label |
|------|----|-------|
| 0 | pulse-lengths | Pulse lengths |

### 4.6 C 实现计划

**文件名**: `ook_c.c`

**状态机**:
```
IDLE → WAITING_FOR_PREAMBLE → DECODING → DECODE_TIMEOUT → IDLE
```

**核心算法**:
1. **Preamble 检测** (`lock_onto_preamble`): 收集前 N 个脉冲，过滤噪声（长/短比 > 5:1 视为垃圾），确定 `sample_high` 和 `sample_low`
2. **NRZ 解码**: 根据 `sample_high`/`sample_low` 将脉冲宽度转换为 bit
3. **Manchester 解码**: 支持两种 preamble（1111 和 1010），同时尝试两种，选错误少的
4. **Diff Manchester 解码**: 基于边沿间转换方向解码
5. **Timeout 检测**: 5 倍 `sample_first` 无边沿则触发 timeout

**状态结构体**:
```c
/* <!-- Updated: typedef移到struct外部，C语言不允许typedef嵌套在struct内 --> */
typedef struct {
    uint64_t ss;
    uint64_t es;
    char state;
} ook_bit_t;

struct ook_priv {
    int state;              // IDLE, WAITING_FOR_PREAMBLE, DECODING, DECODE_TIMEOUT
    uint64_t ss, es;
    uint64_t ss_1111, ss_1010;
    uint64_t samplenumber_last;
    uint64_t sample_first;
    uint64_t sample_high;
    uint64_t sample_low;
    int edge_count;
    uint64_t word_first;
    int word_count;
    int insync;

    // Manchester state
    int lstate_1111;        // Last state for 1111 preamble
    int lstate_1010;        // Last state for 1010 preamble
    int half_time;          // Half time counter for 1111
    int half_time_1010;     // Half time counter for 1010
    int man_errors;
    int man_errors_1010;
    int max_errors;

    // Diff Manchester state
    char diff_man_trans;    // Current transition state
    int diff_man_len;       // Length of previous pulse in half clock periods

    // Preamble buffer
    struct {
        uint64_t start;
        uint64_t samples;
        char state;
    } preamble[10];
    int preamble_count;

    // Decoded stream
    /* <!-- Updated: typedef不能嵌套在struct内部，已移到外部 --> */
    ook_bit_t decoded[1024];
    int decoded_count;
    ook_bit_t decoded_1010[1024];
    int decoded_1010_count;

    // Pulse lengths for binary output
    uint64_t pulse_lengths[1024];
    int pulse_count;

    // Options
    int invert;
    int decodeas;           // 0=NRZ, 1=Manchester, 2=Diff Manchester
    int preamble_val;       // 0=auto, 1=1010, 2=1111
    int preamble_len;
    int diffmanvar;

    uint64_t samplerate;
    int out_ann;
    int out_python;
    int out_binary;
};
```

**decode() 主循环**:
```c
while (1) {
    srd_cond_builder *cb;
    if (s->edge_count == 0) {
        cb = c_cond_new();
        c_cond_edge(cb, 0);
    } else {
        cb = c_cond_new();
        c_cond_edge(cb, 0);
        c_cond_or(cb);
        c_cond_skip(cb, 5 * s->sample_first);
    }
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;

    // Check for timeout (no edges for 5 pulses)
    if (s->edge_count > 0 && (matched & (1ULL << 1)) && !(matched & (1ULL << 0))) {
        s->state = DECODE_TIMEOUT;
    }

    // Calculate samples since last edge
    uint64_t samples = samplenum - s->samplenumber_last;

    // Determine pin state
    int pinstate = c_decoder_get_pin(di, 0, samplenum);
    if (s->state == DECODE_TIMEOUT)
        pinstate = !pinstate;
    if (s->invert)
        pinstate = !pinstate;
    char state = pinstate ? '1' : '0';

    if (!s->insync) {
        lock_onto_preamble(di, s, samples, state);
    } else {
        switch (s->decodeas) {
        case 0: decode_nrz(di, s, s->samplenumber_last, samples, state); break;
        case 1: decode_manchester(di, s, s->samplenumber_last, samples, state); break;
        case 2: decode_diff_manchester(di, s, s->samplenumber_last, samples, state); break;
        }
    }
    s->samplenumber_last = samplenum;
}
```

**注意事项**:
- 这是最复杂的解码器，需要实现 3 种编码方式
- Manchester 解码同时尝试 1111 和 1010 preamble，最后选错误少的
- `decoded` 和 `decoded_1010` 数组需要预分配足够空间
- `pulse_lengths` 用于二进制输出
- Python 版本在 `end()` 中也会输出 decoded 数据
- `preamlen` 选项是字符串类型（'0'-'10'），需要转换为整数
- Timeout 后需要调用 `decode_timeout()` 重置状态

### 4.7 关键 C 代码片段 — Manchester 解码核心

```c
static void decode_manchester_sim(struct srd_decoder_inst *di, struct ook_priv *s,
    uint64_t start, uint64_t samples, char state, uint64_t dsamples,
    int *half_time, char *lstate, uint64_t *ss, int pream,
    ook_bit_t *decoded, int *decoded_count, int *errors)
{
    ook_bit_t bit = {0, 0, 0};
    *errors = 0;

    if (s->edge_count == 0)
        (*half_time)++;

    if (samples > 0.75 * dsamples && samples <= 1.5 * dsamples) { // Long pulse
        *half_time += 2;
        uint64_t es;
        if (*half_time % 2 == 0) { // Transition
            es = start;
        } else {
            es = start + samples / 2;
        }
        if (*ss == start) {
            *lstate = 'E';
            es = start + samples;
        }
        if (!(s->edge_count == 0 && pream == 1010)) { // Skip first pulse for 1010
            bit.ss = *ss;
            bit.es = es;
            bit.state = *lstate;
        }
        *lstate = state;
        *ss = es;
    } else if (samples > 0.25 * dsamples && samples <= 0.75 * dsamples) { // Short pulse
        *half_time += 1;
        if (*half_time % 2 == 0) { // Transition
            uint64_t es = start + samples;
            bit.ss = *ss;
            bit.es = es;
            bit.state = *lstate;
            *lstate = state;
            *ss = es;
        } else { // 1st half
            *ss = start;
            *lstate = state;
        }
    } else { // Error
        *errors = 1;
        if (s->state != DECODE_TIMEOUT) {
            *lstate = 'E';
            bit.ss = *ss;
            bit.es = *ss + samples;
            bit.state = 'E';
        } else {
            bit.ss = *ss;
            bit.es = *ss + s->sample_first;
            bit.state = *lstate;
        }
        *ss = bit.es;
    }

    if (bit.state != 0 && *decoded_count < 1024) {
        decoded[(*decoded_count)++] = bit;
    }
}
```

---

## 5. OpenTherm

### 5.1 Python 元数据

| 属性 | 值 |
|------|-----|
| id | `opentherm` |
| name | `OpenTherm` |
| longname | `OpenTherm` |
| desc | `OpenTherm protocol.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| channels | ot(0) |
| optional_channels | `()` |
| tags | `['OT']` |

### 5.2 Options (9 个)

| id | desc | default | type/values |
|----|------|---------|-------------|
| polarity | Polarity | 'active-low' | ('active-low', 'active-high') |
| bitlen | Single bit period (us) | 1000 | int |
| jitter_m | Edge jitter minus (us) | 100 | int |
| jitter_p | Edge jitter plus (us) | 150 | int |
| m2s_silence_min | Master to Slave min silence (us) | 20000 | int |
| m2s_silence_max | Master to Slave max silence (us) | 800000 | int |
| s2m_silence_min | Slave to Master min silence (us) | 100000 | int |
| m2m_act_max | Master req to req max period (us) | 1150000 | int |
| ignore_glitches | Ignore glitches up to (us) | 0 | int |
| format | Data format | 'dec' | ('hex', 'dec', 'oct', 'bin') |

### 5.3 Annotations (14 个)

| 索引 | id | label |
|------|----|-------|
| 0 | bit | Bit |
| 1 | startbit | Startbit |
| 2 | stopbit | Stopbit |
| 3 | paritybit | Paritybit |
| 4 | msgtype | MSG-TYPE |
| 5 | spare | Spare |
| 6 | dataid | DATA-ID |
| 7 | datavalue | DATA-VALUE |
| 8 | m2s | MasterToSlave |
| 9 | s2m | SlaveToMaster |
| 10 | frame | OpenThermFrame |
| 11 | timing | Timing error |
| 12 | warning | Warning |
| 13 | otx | OpenThermExchange |

### 5.4 Annotation Rows (7 个)

| row id | label | 包含的 annotation 索引 |
|--------|-------|----------------------|
| bits | Bits | 0 |
| fields | Fields | 1, 2, 3, 4, 5, 6, 7 |
| direction | Direction | 8, 9 |
| frames | Frame | 10 |
| otxs | Description | 13 |
| warnings | Warnings | 12 |
| timings | Timing errors | 11 |

### 5.5 外部数据 — lists.py

```python
msg_type = {
    0: ['M2S', 'READ-DATA', 'RD'],
    1: ['M2S', 'WRITE-DATA', 'WD'],
    2: ['M2S', 'INVALID-DATA', 'INV'],
    3: ['M2S', 'RESERVED', 'RSV'],
    4: ['S2M', 'READ-ACK', 'RACK'],
    5: ['S2M', 'WRITE-ACK', 'WACK'],
    6: ['S2M', 'DATA-INVALID', 'INV'],
    7: ['S2M', 'UNKNOWN-DATAID', 'UNK'],
}
```

**C 中的等效实现**:
```c
static const struct {
    const char *dir;    // "M2S" or "S2M"
    const char *name;   // Full name
    const char *short_name; // Short name
} msg_type_table[8] = {
    {"M2S", "READ-DATA", "RD"},
    {"M2S", "WRITE-DATA", "WD"},
    {"M2S", "INVALID-DATA", "INV"},
    {"M2S", "RESERVED", "RSV"},
    {"S2M", "READ-ACK", "RACK"},
    {"S2M", "WRITE-ACK", "WACK"},
    {"S2M", "DATA-INVALID", "INV"},
    {"S2M", "UNKNOWN-DATAID", "UNK"},
};
```

### 5.6 外部数据 — otdecoder.py

Python 版本使用 `OTDecoder` 类来描述参数含义（如 Data-ID=0 对应 "Master status" 等）。C 版本**不需要**移植完整的 `OTDecoder`，因为：
1. 它是一个大型查找表（数百行）
2. 它仅用于 `ann_descr` (索引 13) 注解
3. C 版本可以简化为只输出基本帧信息，不输出参数描述

**简化方案**: C 版本不实现 `ann_descr` 注解（索引 13），但保留 annotation 定义以保持索引一致。或者实现一个精简版的参数描述表。

### 5.7 C 实现计划

**文件名**: `opentherm_c.c`

**状态机** (Manchester/Bi-phase-L 解码):
```
IDLE → SYNC → MID1/START1 → MID0/START0 → ... → 34 bits → handle_bits → IDLE
```

**核心算法**:
1. **Manchester/Bi-phase-L 解码**: 边沿间隔分类为 short (s) 或 long (l)
2. **FSM 状态**:
   - `IDLE`: 等待起始边沿（根据 polarity 设置）
   - `SYNC`: 检测 short 边沿（半位周期）→ 进入 `MID1`
   - `MID1`: short → `START1`，long → `MID0` (bit=0)
   - `MID0`: short → `START0`，long → `MID1` (bit=1)
   - `START1`: short → `MID1` (bit=1)
   - `START0`: short → `MID0` (bit=0)
3. **边沿分类**: `edge_type()` 判断边沿间隔属于 short range 还是 long range
4. **34-bit 帧**: bit 0=start, bit 1=parity, bits 2-4=msg type, bits 5-8=spare, bits 9-16=DATA-ID, bits 17-32=DATA-VALUE, bit 33=stop
5. **Glitch 过滤**: 可选，忽略短于 `ignore_glitches` 的脉冲

**状态结构体**:
```c
struct ot_priv {
    // Decoder state
    int state;              // IDLE, SYNC, MID1, MID0, START1, START0

    // Edge tracking
    uint64_t edges[64];     // Edge sample numbers
    int edge_count;

    // Bit tracking
    struct {
        uint64_t sample;    // Sample where bit starts
        int value;          // Bit value (0 or 1)
    } bits[34];
    int bit_count;

    // ss/es for each bit (computed in handle_bits)
    struct {
        uint64_t ss;
        uint64_t es;
    } ss_es_bits[34];

    // Previous edge info
    uint64_t prev_samplenum;
    int prev_lvl;
    uint64_t c_samplenum;
    int c_lvl;

    // Timing parameters (computed from options + samplerate)
    uint64_t halfbit;       // Half bit period in samples
    uint64_t s_range_min;   // Short period range min
    uint64_t s_range_max;   // Short period range max
    uint64_t l_range_min;   // Long period range min
    uint64_t l_range_max;   // Long period range max
    uint64_t silence;       // Min silence period
    uint64_t glitchlen;     // Glitch filter length

    uint64_t last_frame_edge;

    // Options
    int polarity;           // 0=active-low, 1=active-high
    int format;             // 0=hex, 1=dec, 2=oct, 3=bin

    uint64_t samplerate;
    int out_ann;
};
```

**decode() 主循环**:
```c
while (1) {
    if (s->silence == 0)
        setup_calc(di, s); // Compute timing ranges from options

    srd_cond_builder *cb = c_cond_new();
    c_cond_edge(cb, 0);  // Any edge on OT line
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;

    int lvl = c_decoder_get_pin(di, 0, samplenum);

    if (s->prev_samplenum == 0) {
        s->prev_samplenum = samplenum;
        s->prev_lvl = lvl;
        continue;
    }

    // Glitch filtering
    if (s->glitchlen > 0 && (samplenum - s->prev_samplenum) <= s->glitchlen) {
        char glitch_str[64];
        snprintf(glitch_str, sizeof(glitch_str), "Glitch (%d us)", s2t(s, samplenum - s->prev_samplenum));
        C_ANN_PUT(di, s->prev_samplenum, samplenum, s->out_ann, ANN_WARNING, glitch_str, "Glitch", "G");
        s->prev_samplenum = samplenum;
        s->prev_lvl = lvl;
        continue;
    }

    s->c_samplenum = s->prev_samplenum;
    s->c_lvl = s->prev_lvl;
    s->prev_samplenum = samplenum;
    s->prev_lvl = lvl;

    s->edges[s->edge_count++] = s->c_samplenum;

    // FSM processing
    int bit = -1;
    uint64_t bitpos = 0;

    if (s->state == STATE_IDLE) {
        // Check silence duration
        if (s->last_frame_edge != 0 && (s->c_samplenum - s->last_frame_edge) < s->halfbit * 4) {
            C_ANN_PUT(di, s->last_frame_edge, s->c_samplenum, s->out_ann, ANN_WARNING,
                      "Sync error: silence too short", "Sync err", "S");
            s->last_frame_edge = s->c_samplenum;
            continue;
        }
        if ((s->polarity == 0 && s->c_lvl == 1) || (s->polarity == 1 && s->c_lvl == 0)) {
            s->state = STATE_SYNC;
        }
        continue;
    }

    // Classify edge
    char edge = edge_type(s, s->edges[s->edge_count - 2], s->edges[s->edge_count - 1]);

    switch (s->state) {
    case STATE_SYNC:
        if (edge == 's') { s->state = STATE_MID1; bit = 1; bitpos = s->edges[s->edge_count - 2]; }
        else { handle_timing_error(di, s); bit = -1; }
        break;
    case STATE_MID1:
        if (edge == 's') { s->state = STATE_START1; bit = -1; }
        else if (edge == 'l') { s->state = STATE_MID0; bit = 0; bitpos = s->c_samplenum - s->halfbit; }
        else { handle_bits(di, s); handle_timing_error(di, s); bit = -1; }
        break;
    case STATE_MID0:
        if (edge == 's') { s->state = STATE_START0; bit = -1; }
        else if (edge == 'l') { s->state = STATE_MID1; bit = 1; bitpos = s->c_samplenum - s->halfbit; }
        else { handle_bits(di, s); handle_timing_error(di, s); bit = -1; }
        break;
    case STATE_START1:
        if (edge == 's') { s->state = STATE_MID1; bit = 1; bitpos = s->edges[s->edge_count - 2]; }
        else { handle_bits(di, s); handle_timing_error(di, s); bit = -1; }
        break;
    case STATE_START0:
        if (edge == 's') { s->state = STATE_MID0; bit = 0; bitpos = s->edges[s->edge_count - 2]; }
        else { handle_bits(di, s); handle_timing_error(di, s); bit = -1; }
        break;
    }

    if (bit >= 0 && s->bit_count < 34) {
        s->bits[s->bit_count].sample = bitpos;
        s->bits[s->bit_count].value = bit;
        s->bit_count++;
    }

    if (s->bit_count == 34) {
        handle_bits(di, s);
        reset_decoder_state(s);
        s->last_frame_edge = s->c_samplenum;
    }
}
```

**handle_bits() 实现**:
```c
static void handle_bits(struct srd_decoder_inst *di, struct ot_priv *s)
{
    if (s->bit_count < 1) return;

    // Compute ss/es for each bit
    for (int i = 0; i < s->bit_count; i++) {
        s->ss_es_bits[i].ss = s->bits[i].sample;
        if (i < s->bit_count - 1)
            s->ss_es_bits[i].es = s->bits[i + 1].sample;
        else
            s->ss_es_bits[i].es = s->bits[i].sample + s->halfbit * 2;
        // Announce individual bit
        char bit_str[4];
        snprintf(bit_str, sizeof(bit_str), "%d", s->bits[i].value);
        C_ANN_PUT(di, s->ss_es_bits[i].ss, s->ss_es_bits[i].es, s->out_ann, ANN_BIT, bit_str);
    }

    // Start bit (bit 0)
    char start_str[32];
    snprintf(start_str, sizeof(start_str), "Startbit: %d", s->bits[0].value);
    C_ANN_PUT(di, s->ss_es_bits[0].ss, s->ss_es_bits[0].es, s->out_ann, ANN_STARTBIT,
              start_str, "STRB", "S");

    if (s->bit_count < 2) { /* incomplete warning */ return; }

    // Parity bit (bit 1)
    char par_str[32];
    snprintf(par_str, sizeof(par_str), "Paritybit: %d", s->bits[1].value);
    C_ANN_PUT(di, s->ss_es_bits[1].ss, s->ss_es_bits[1].es, s->out_ann, ANN_PARITYBIT,
              par_str, "PB", "P");

    // Check parity (XOR of bits 1-33 should be 0)
    int parity = 0;
    if (s->bit_count == 34) {
        for (int i = 0; i < 32; i++)
            parity ^= s->bits[i + 1].value;
    }
    char wrn[256] = "";
    if (parity == 1 && s->bit_count == 34)
        strcat(wrn, "ParityError");

    if (s->bit_count < 5) { /* incomplete warning */ return; }

    // MSG-TYPE (bits 2-4, MSB first)
    int msg_type_val = 0;
    for (int i = 0; i < 3; i++)
        msg_type_val |= (s->bits[2 + i].value << (2 - i));

    const char *mt_dir = msg_type_table[msg_type_val].dir;
    const char *mt_name = msg_type_table[msg_type_val].name;
    const char *mt_short = msg_type_table[msg_type_val].short_name;

    char mt_str[64];
    snprintf(mt_str, sizeof(mt_str), "MSG-TYPE: %s (%d)", mt_name, msg_type_val);
    C_ANN_PUT(di, s->ss_es_bits[2].ss, s->ss_es_bits[4].es, s->out_ann, ANN_MSGTYPE,
              mt_str, mt_short, mt_short);

    // Direction annotation
    if (strcmp(mt_dir, "M2S") == 0)
        C_ANN_PUT(di, s->ss_es_bits[0].ss, s->ss_es_bits[s->bit_count-1].es, s->out_ann, ANN_M2S, "MasterToSlave", mt_dir);
    else if (strcmp(mt_dir, "S2M") == 0)
        C_ANN_PUT(di, s->ss_es_bits[0].ss, s->ss_es_bits[s->bit_count-1].es, s->out_ann, ANN_S2M, "SlaveToMaster", mt_dir);

    // ... (SPARE, DATA-ID, DATA-VALUE, STOP bit — similar pattern)

    // Frame annotation
    if (s->bit_count == 34) {
        char frame_str[128];
        snprintf(frame_str, sizeof(frame_str), "Frame %s %s(%d) %d/0x%x %d/0x%x",
                 mt_dir, mt_name, msg_type_val, data_id, data_id, data_value, data_value);
        C_ANN_PUT(di, s->ss_es_bits[0].ss, s->ss_es_bits[33].es, s->out_ann, ANN_FRAME,
                  frame_str, mt_name, "F");
    }
}
```

**注意事项**:
- `edge_type()` 使用 `s_range` 和 `l_range` 判断边沿类型，C 中用范围比较替代 Python 的 `range()` 对象
- `setup_calc()` 需要在 `start()` 和 `metadata()` 中调用，计算所有时序参数
- `s2t()` 和 `t2s()` 辅助函数用于 samples ↔ microseconds 转换
- Glitch 过滤是可选功能（`ignore_glitches` 默认为 0）
- `otdecoder.py` 中的参数描述表非常庞大，C 版本建议不实现或只实现最常用的参数
- 34-bit 帧结构固定：1 start + 1 parity + 3 msg type + 4 spare + 8 data ID + 16 data value + 1 stop

---

## 复杂度排序

| 排名 | 解码器 | 复杂度 | 原因 |
|------|--------|--------|------|
| 1 | OOK | ★★★★★ | 3 种编码方式、preamble 检测、时钟恢复、timeout 处理、双路 Manchester 并行解码 |
| 2 | OpenTherm | ★★★★☆ | Manchester/Bi-phase-L FSM、34-bit 帧解析、多选项、glitch 过滤、参数描述表 |
| 3 | MVB | ★★★☆☆ | Manchester 解码、CRC-8、可变长度 slave 帧、preamble 匹配 |
| 4 | OneSingleWire | ★★☆☆☆ | 简单阈值判断、9-bit 帧、parity 校验 |
| 5 | MCS-48 | ★☆☆☆☆ | 最简单：两个条件等待、地址/数据锁存 |

## 建议实现顺序

1. **MCS-48** — 最简单，验证基本框架
2. **OneSingleWire** — 简单阈值解码，验证 options 和 metadata
3. **MVB** — Manchester 解码 + CRC，中等复杂度
4. **OpenTherm** — Manchester FSM + 帧解析
5. **OOK** — 最复杂，最后实现
