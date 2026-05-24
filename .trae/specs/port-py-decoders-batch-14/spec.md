# Python Decoder 移植到 C 规格 — Batch 14

本规格涵盖 5 个 Python decoder 的 C 移植：**bean**, **ccd**, **cjtag-oscan0**, **rgb_led_ws281x**, **stepper_motor**。

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| spi_c.c | 底层逻辑解码器范本 | 条件构建器(c_cond_new/or/wait/free)、CLK边沿采样、多通道处理 |
| can_fd_c.c | 底层复杂状态机范本 | 位填充、CRC校验、帧结构解析、多状态FSM |


## 1. BEAN — Toyota Body Electronics Area Network

### 1.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `bean` |
| name | `BEAN` |
| longname | `BEAN is a Toyota Body Electronics Area Network` |
| desc | `BEAN is a Toyota Body Electronics Area Network.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Embedded/industrial']` |

**channels:**
| id | name | desc | default |
|----|------|------|---------|
| data | Data | Data line | 0 |

**options:**
| id | desc | default | values |
|----|------|---------|--------|
| bit_annotations | Bit annotations | 'none' | ('none', 'yes') |
| pulse_len | Pulse length | 'none' | ('none', 'yes') |
| command | Command | 'yes' | ('none', 'yes') |
| all_byte | All byte | 'yes' | ('none', 'yes') |

**annotations (9 classes):**
| index | id | label |
|-------|----|-------|
| 0 | bit-0 | Bit 0 |
| 1 | bit-1 | Bit 1 |
| 2 | bite_ann | Bite_ann |
| 3 | byte | Byte |
| 4 | frame | Frame |
| 5 | message | Message |
| 6 | pulse_width | Pulse width |
| 7 | debug | Debug |
| 8 | all_byte | All byte |

**annotation_rows:**
| id | name | classes |
|----|------|---------|
| bits | Bits | (0, 1) |
| bits_ann | Bits_ann | (2,) |
| bytes | Bytes | (3,) |
| frames | Frames | (4, 5) |
| pulse_widths | Pulse_widths | (6,) |
| command | Command | (7,) |
| All_byte | All byte | (8,) |

### 1.2 解码逻辑分析

BEAN 协议是 Toyota 车辆的单线串行总线协议，基于脉宽编码：

**帧结构：**
```
SOF(1bit) | PRI(4bits) | ML(4bits) | DST-ID(8bits) | MES-ID(8bits) | DATA(8~88bits) | CRC(8bits) | EOM(8bits) | RSP(2bits) | EOF(6bits)
```

**脉宽编码规则：**
- 短脉冲 (≤150 samples): 单个 bit 或 SOF/Stuff bit
- 中等脉冲 (150~650 samples): 可能包含多个 bit，每 100 samples 一个 bit
- 长脉冲 (≥650 samples): 帧间隔/EOF

**Bit stuffing:**
- 连续 4 个相同 bit 后插入一个 stuff bit
- `count == 5` 时 `stuff = 1`

**状态机：**
1. 等待边缘跳变 (`self.wait({0: 'e'})`)
2. 根据脉宽分类处理
3. 收集 bits 到 `self.bits[]` 列表
4. 当 `draw == 1` 时（EOM 后或长间隔后），解析完整帧
5. 解析 PRI (bits 0-3), ML (bits 4-7), 然后按字节解析 DST-ID, MES-ID, DATA, CRC, EOM
6. 使用 `lists.py` 中的 `command` 字典查找命令名称

**关键算法：**
- `pinlabels(bit_count)`: 返回 `'Data%i' % (bit_count - 2)` 用于 DATA 字段标注
- 帧验证: `frame_length + 3 <= len(self.bits) // 8`
- CRC: 最后一个 byte 之前的所有 bytes 之和 mod 256（Python 中未显式验证 CRC）
- `noresp` 标志: 长间隔后无 RSP 时设置

### 1.3 C 实现计划

**文件名:** `bean_c.c`

**state struct:**
```c
typedef struct {
    uint64_t samplerate;
    int out_ann;

    // 脉冲跟踪
    uint64_t ss;           // 当前脉冲起始
    uint64_t es;           // 当前脉冲结束
    uint64_t samplenumber_last;
    int pin_last;
    int pin;

    // 状态
    int state;             // IDLE
    int sof;               // SOF 已检测
    int eom;               // EOM 已检测
    int stuff;             // 下一个是 stuff bit
    int draw;              // 帧完成标志
    int noresp;            // 无 RSP 标志

    // 收集的 bits
    int bits[200];         // bit 值 (0/1)
    uint64_t bits_ss[200]; // bit 起始 sample
    uint64_t bits_es[200]; // bit 结束 sample
    int bit_count;

    // bits_ann (SOF/Stuff)
    char *bits_ann_label[100];
    uint64_t bits_ann_ss[100];
    uint64_t bits_ann_es[100];
    int bits_ann_count;

    // 选项
    int opt_bit_annotations;
    int opt_pulse_len;
    int opt_command;
    int opt_all_byte;
} bean_state;
```

**ann_labels (9 classes):**
```c
static const char* bean_ann_labels[][3] = {
    { "", "bit-0", "Bit 0" },
    { "", "bit-1", "Bit 1" },
    { "", "bite-ann", "Bite_ann" },
    { "", "byte", "Byte" },
    { "", "frame", "Frame" },
    { "", "message", "Message" },
    { "", "pulse-width", "Pulse width" },
    { "", "debug", "Debug" },
    { "", "all-byte", "All byte" },
};
```

**annotation_rows:**
```c
static const int bean_row_bits[] = { 0, 1, -1 };
static const int bean_row_bits_ann[] = { 2, -1 };
static const int bean_row_bytes[] = { 3, -1 };
static const int bean_row_frames[] = { 4, 5, -1 };
static const int bean_row_pulse_widths[] = { 6, -1 };
static const int bean_row_command[] = { 7, -1 };
static const int bean_row_all_byte[] = { 8, -1 };
static const struct srd_c_ann_row bean_ann_rows[] = {
    { "bits", "Bits", bean_row_bits, 2 },
    { "bits_ann", "Bits_ann", bean_row_bits_ann, 1 },
    { "bytes", "Bytes", bean_row_bytes, 1 },
    { "frames", "Frames", bean_row_frames, 2 },
    { "pulse_widths", "Pulse_widths", bean_row_pulse_widths, 1 },
    { "command", "Command", bean_row_command, 1 },
    { "All_byte", "All byte", bean_row_all_byte, 1 },
};
```

**decode() 核心逻辑:**
```c
while (1) {
    srd_cond_builder *cb = c_cond_new();
    c_cond_edge(cb, 0);  // channel 0 edge
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    if (ret != SRD_OK) return;

    int pin = c_decoder_get_pin(di, 0, samplenum);

    if (!s->samplenumber_last) {
        s->samplenumber_last = samplenum;
        s->pin_last = pin;
        s->ss = samplenum;
        continue;
    }

    s->es = samplenum;
    uint64_t puls = s->es - s->ss;

    // pulse_len 选项
    if (s->opt_pulse_len) {
        char puls_str[32];
        snprintf(puls_str, sizeof(puls_str), " %llu", (unsigned long long)puls);
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, 6, puls_str);
    }

    uint64_t count = puls / 100;

    if (puls > 150 && puls < 650) {
        // 中等脉冲: 拆分为多个 bit
        uint64_t temp_ss = s->ss;
        for (uint64_t i = 0; i < count - 1; i++) {
            if (!s->stuff) {
                if (!s->sof) {
                    s->sof = 1;
                    uint64_t temp_es = temp_ss + puls / count;
                    // 保存 SOF annotation
                    temp_ss = temp_es;
                } else {
                    uint64_t temp_es = temp_ss + puls / count;
                    // 保存 bit
                    s->bits[s->bit_count] = s->pin_last;
                    s->bits_ss[s->bit_count] = temp_ss;
                    s->bits_es[s->bit_count] = temp_es;
                    s->bit_count++;
                    temp_ss = temp_es;
                }
            } else {
                s->stuff = 0;
                uint64_t temp_es = temp_ss + puls / count;
                // 保存 Stuff annotation
                temp_ss = temp_es;
            }
            if (i == 3 && count == 5) {
                s->stuff = 1;
            }
        }
        // 最后一个 bit
        s->bits[s->bit_count] = s->pin_last;
        s->bits_ss[s->bit_count] = temp_ss;
        s->bits_es[s->bit_count] = s->es;
        s->bit_count++;

        if (count == 6) {
            s->eom = 1;
        }
    } else if (puls <= 150) {
        // 短脉冲
        if (!s->sof) {
            s->sof = 1;
            // SOF annotation
        } else if (s->stuff) {
            // Stuff annotation
            s->stuff = 0;
        } else {
            s->bits[s->bit_count] = s->pin_last;
            s->bits_ss[s->bit_count] = s->ss;
            s->bits_es[s->bit_count] = s->es;
            s->bit_count++;
        }
        if (s->eom) {
            // RSP annotation
            s->draw = 1;
        }
    } else { // puls >= 650
        if (s->eom) {
            // noresp
            s->draw = 1;
            s->noresp = 1;
        }
        s->sof = 0;
        s->stuff = 0;
    }

    s->ss = samplenum;
    s->pin_last = pin;

    if (s->draw) {
        // 解析帧...
        bean_parse_frame(di, s);
        if (s->noresp) bean_reset_frame(s);
        else bean_reset(s);
    }
}
```

**command 查找表:** 将 `lists.py` 中的 `command` 字典硬编码为 C 数组：
```c
typedef struct {
    const char *key;
    const char *value;
} bean_command_entry;

static const bean_command_entry bean_commands[] = {
    { "ABA180", "OK - Power Window Master SW" },
    { "AB4080", "OK - ??? 1 ???" },
    { "ABAE0",  "OK - ??? 2 ???" },
    { "ABE00",  "OK - ??? 3 ???" },
    { "ABA80",  "OK - ??? 4 ???" },
    { "ABAB0",  "OK - ??? 5 ???" },
    { "DB01",   "UnBlock Control" },
    { "DB021",  "Block Control" },
    { "DB4C81", "Door Lock" },
    { "DB4C41", "Door UnLock" },
    { "E000400","Window Rear Left - Down" },
    { "E000600","Window Rear Left - Full Down" },
    { "E000800","Window Rear Left - Up" },
    { "E000A00","Window Rear Left - Full Up" },
    { "E004000","Window Rear Right - Down" },
    { "E006000","Window Rear Right - Full Down" },
    { "E008000","Window Rear Right - Up" },
    { "E00A000","Window Rear Right - Full Up" },
    { "E040000","Window Front Right - Down" },
    { "E060000","Window Front Right - Full Down" },
    { "E080000","Window Front Right - Up" },
    { "E0A0000","Window Front Right - Full Up" },
    { NULL, NULL }
};
```

**samplerate guard:** BEAN 不严格需要 samplerate（基于固定 sample 计数阈值 150/650），但应在 metadata 中记录 samplerate，decode 入口做 fallback。

**难度:** ⭐⭐⭐⭐ — 复杂的脉宽编码 + bit stuffing + 帧解析 + 命令查找表

---

## 2. CCD — Chrysler Collision Detection Data Bus

### 2.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `ccd` |
| name | `CCD` |
| longname | `CCD (Crysler Collision Detection) Data Bus` |
| desc | `CCD (Crysler Collision Detection) Data Bus.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['ccd']` |
| tags | `['Automotive']` |

**channels:**
| id | name | desc |
|----|------|------|
| bus | bus | CCD bidirectional shared data bus |

**options:**
| id | desc | default | values |
|----|------|---------|--------|
| ignoreerrors | Ignore checksum and frame errors | 'no' | ('yes', 'no') |
| invert_bus | Invert bus? | 'no' | ('yes', 'no') |
| units | Show metric/imperial/both/native units | 'native' | ('metric', 'imperial', 'both', 'native') |

**annotations (7 classes):**
| index | id | label |
|-------|----|-------|
| 0 | bus-bits | Bus data bits |
| 1 | bus-bytes | Bus data bytes |
| 2 | idle | Bus idle |
| 3 | frame-error | Frame errors |
| 4 | checksum | Message checksum errors |
| 5 | bus-decoded | Decoded bus message |
| 6 | bus-message | Message bytes |

**annotation_rows:**
| id | name | classes |
|----|------|---------|
| a-bus-bits | Bus bits | (0,) |
| a-idle | Idle | (2,) |
| a-bus-warnings | Bus warnings | (3, 4) |
| a-bus-data | Bus bytes | (1,) |
| a-bus-message | Message bytes | (6,) |
| a-bus-decoded | Message decoded | (5,) |

### 2.2 解码逻辑分析

CCD 是 Chrysler 车辆的单线串行总线协议，固定波特率 7812.5 bps。

**双层状态机：**

**层1: IDLE/BUSY 检测**
- IDLE → BUSY: bus 信号变化时
- BUSY → IDLE: bus 保持高电平超过 10 个 bit 宽度
- 使用动态 `waitidle` 条件：`{ 'skip': bit_width*10 }`

**层2: UART 解码（在 BUSY 期间）**
- `WAIT FOR START BIT`: 检测 1→0 跳变（start bit）
- `GET DATA BITS`: 在 1.5*bit_width 后采样第一个 bit，然后每 bit_width 采样一次，共 8 bits（LSB first）
- `GET STOP BIT`: 检测 stop bit（应为高电平），否则 frame error

**消息处理：**
- BUSY→IDLE 转换时，验证 checksum（所有 bytes 之和 mod 256，最后一个 byte 为 checksum）
- 调用 `decode_ccd_message()` 解析消息内容
- 支持多种消息 ID 的解码（速度、RPM、VIN、门锁、温度等）

**关键参数：**
- `bit_width = ceil(samplerate / 7812.5)` — 固定波特率
- 需要 samplerate

**动态 wait 条件:**
- `waituart`: UART 定时采样，使用 `{ 'skip': N }`
- `waitidle`: IDLE 检测定时，使用 `{ 'skip': N }` 或 `{ 0: 'e' }`
- 主循环: `self.wait([{0: 'e'}, self.waituart, self.waitidle])`

### 2.3 C 实现计划

**文件名:** `ccd_c.c`

**state struct:**
```c
typedef struct {
    uint64_t samplerate;
    uint64_t bit_width;
    int out_ann;
    int out_python;        // <!-- Updated: CCD outputs='ccd'，需要 out_python 输出以供上层解码器使用 -->

    // IDLE/BUSY 状态
    int idle;              // 0=IDLE, 1=BUSY
    uint64_t idlestart;
    uint64_t busystart;
    int oldbus;

    // UART 状态
    int uart_state;        // WAIT_START, GET_DATA, GET_STOP
    int databit;
    uint8_t databyte;
    uint64_t framestart;
    uint64_t waitfortime;

    // 消息收集
    uint8_t ccd_message[256];
    int ccd_msg_len;
    int errors;

    // VIN 缓存
    char vin[18];

    // 选项
    int opt_ignoreerrors;
    int opt_invert_bus;
    int opt_units;         // 0=metric, 1=imperial, 2=both, 3=native
} ccd_state;
```

**decode() 核心逻辑 — 使用 c_cond_wait 实现多条件等待:**

Python 中 `self.wait([{0: 'e'}, self.waituart, self.waitidle])` 需要在 C 中用 `c_cond_or` + `c_cond_edge`/`c_cond_skip` 实现：

```c
while (1) {
    srd_cond_builder *cb = c_cond_new();
    c_cond_edge(cb, 0);  // 条件0: bus edge

    // 条件1: UART 定时
    if (s->uart_state != UART_WAIT_START || s->idle == IDLE_IDLE) {
        // 不需要 UART 定时
    } else {
        c_cond_or(cb);
        c_cond_skip(cb, skip_uart);
    }

    // 条件2: IDLE 定时
    if (s->idle == IDLE_BUSY) {
        c_cond_or(cb);
        c_cond_skip(cb, skip_idle);
    }

    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);
    // ... 处理
}
```

**注意:** CCD 的 `waituart` 和 `waitidle` 是动态变化的 skip 值，每次循环需要重新计算。这在 C decoder API 中需要特别处理——每次迭代构建新的 condition builder。

**samplerate guard:** 必须！CCD 需要 samplerate 计算 `bit_width`。在 `metadata` 回调中计算，`decode()` 入口检查。

**难度:** ⭐⭐⭐⭐⭐ — 双层状态机 + 动态 wait 条件 + 复杂消息解码 + UART 解码

<!-- Updated: CCD outputs='ccd'，C 实现需要在 start() 中注册 SRD_OUTPUT_PYTHON 输出，
     并在帧完成时使用 c_decoder_put_python() 输出 ccd 协议数据（帧字节），
     以便上层解码器（如 ccd_msg）可以堆叠在 CCD 之上。
     参考 uart_c.c 的 IDLE/BREAK 输出模式。 -->

---

## 3. CJTAG-OSCAN0 — Compact JTAG OScan1

### 3.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `cjtag_oscan1` |
| name | `cJTAG OScan1` |
| longname | `Compact Joint Test Action Group (IEEE 1149.7)` |
| desc | `Protocol for testing, debugging, and flashing ICs, Now this plugin has no ZBS support, it only supports Oscan1 format.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `['jtag']` |
| tags | `['Debug/trace']` |

**channels (4):**
| id | name | desc | idn |
|----|------|------|-----|
| tdi | TDI | Test data input | dec_cjtag_oscan0_chan_tdi |
| tdo | TDO | Test data output | dec_cjtag_oscan0_chan_tdo |
| tck | TCK | Test clock | dec_cjtag_oscan0_chan_tck |
| tms | TMS | Test mode select | dec_cjtag_oscan0_chan_tms |

**optional_channels (3):**
| id | name | desc | idn |
|----|------|------|-----|
| trst | TRST# | Test reset | dec_cjtag_oscan0_opt_chan_trst |
| srst | SRST# | System reset | dec_cjtag_oscan0_opt_chan_srst |
| rtck | RTCK | Return clock signal | dec_cjtag_oscan0_opt_chan_rtck |

**annotations (22 classes):**
- 0-15: JTAG states (TEST-LOGIC-RESET, RUN-TEST/IDLE, ..., EXIT2-IR)
- 16: bit-tdi
- 17: bit-tdo
- 18: bitstring-tdi
- 19: bitstring-tdo
- 20: bit-tms
- 21: state-tapc (TAPC State)

**annotation_rows:**
| id | name | classes |
|----|------|---------|
| bits-tdi | Bits (TDI) | (16,) |
| bits-tdo | Bits (TDO) | (17,) |
| bitstrings-tdi | Bitstring (TDI) | (18,) |
| bitstrings-tdo | Bitstring (TDO) | (19,) |
| bit-tms | Bit (TMS) | (20,) |
| state-tapc | TAPC State | (21,) |
| states | States | (0..15) |

### 3.2 解码逻辑分析

CJTAG OScan1 是标准 JTAG 的 2-pin 压缩版本。核心逻辑：

**cJTAG 状态机：**
1. `4-WIRE`: 标准 4 线 JTAG 模式
2. `CJTAG-OAC`: Online Activation Code 检测（6 个 TMS edge 后进入）
3. `CJTAG-EC/SPARE/TPDEL/TPREV/TPST/RDYC/DLYC/SCNFMT/CP`: OAC 阶段子状态
4. `OSCAN1`: 激活后进入 OScan1 模式

**OSCAN1 模式 (3-cycle):**
- cycle 0 (nTDI): TMS=0 → TDI=1, TMS=1 → TDI=0
- cycle 1 (TMS): TMS 值直接使用
- cycle 2 (TDO): TMS 值作为 TDO

**Escape 检测:**
- 在 TCK 高电平期间检测 TMS 变化
- 连续 6 个 TMS edge → 进入 CJTAG-OAC
- 连续 8 个 TMS edge → 回到 4-WIRE

**JTAG 状态机:** 与标准 JTAG 相同的 16 状态 TAP controller。

**关键：** 已有 `jtag_c.c` C decoder，cjtag-oscan0 可以大量复用其 JTAG 状态机逻辑。

### 3.3 C 实现计划

**文件名:** `cjtag_oscan0_c.c`

**state struct:**
```c
typedef struct {
    int jtag_state;
    int old_jtag_state;
    int cjtag_state;       // 0=4-WIRE, 1=CJTAG-OAC, 2=CJTAG-EC, etc.
    int old_cjtag_state;
    int escape_edges;
    int oaclen;
    int oacp;
    int oscan1cycle;
    int oldtms;

    // TDI/TDO bit 收集
    int bits_tdi[256];
    int bits_tdo[256];
    uint64_t bits_ss_tdi[256];
    uint64_t bits_es_tdi[256];
    uint64_t bits_ss_tdo[256];
    uint64_t bits_es_tdo[256];
    int bits_cnt;
    int data_ready;

    // 采样位置
    uint64_t ss_item;
    uint64_t es_item;
    uint64_t ss_bitstring;
    uint64_t es_bitstring;
    int first;

    int out_ann;
    int out_python;
} cjtag_state;
```

**decode() 核心逻辑:**
```c
while (1) {
    // 等待 TCK 上升沿
    srd_cond_builder *cb = c_cond_new();
    c_cond_rise(cb, 2);  // TCK rising
    int ret = c_cond_wait(cb, di, &samplenum, &matched);
    c_cond_free(cb);

    int tdi = c_decoder_get_pin(di, 0, samplenum);
    int tdo = c_decoder_get_pin(di, 1, samplenum);
    int tck = c_decoder_get_pin(di, 2, samplenum);
    int tms = c_decoder_get_pin(di, 3, samplenum);

    handle_tapc_state(s, tms);

    if (s->cjtag_state == CJTAG_OSCAN1) {
        if (s->oscan1cycle == 0) {
            tdi_real = (tms == 0) ? 1 : 0;
            s->oscan1cycle = 1;
        } else if (s->oscan1cycle == 1) {
            tms_real = tms;
            s->oscan1cycle = 2;
        } else {
            tdo_real = tms;
            handle_rising_tck_edge(di, s, tdi_real, tdo_real, tck, tms_real);
            s->oscan1cycle = 0;
        }
    } else {
        handle_rising_tck_edge(di, s, tdi, tdo, tck, tms);
    }

    // 在 TCK 高电平期间检测 TMS 变化
    // 需要额外 wait: TCK falling 或 TMS edge
}
```

**TCK 高电平期间的 TMS 变化检测:**
```c
// 等待 TCK 下降沿或 TMS 变化
cb = c_cond_new();
c_cond_fall(cb, 2);   // TCK falling
c_cond_or(cb);
c_cond_edge(cb, 3);   // TMS edge
ret = c_cond_wait(cb, di, &samplenum2, &matched2);
c_cond_free(cb);
```

**samplerate guard:** 不严格需要，但建议实现 metadata 回调。

**难度:** ⭐⭐⭐⭐⭐ — 复杂的 cJTAG 状态机 + OScan1 3-cycle 解码 + 标准 JTAG 状态机 + TMS escape 检测

<!-- Updated: cjtag-oscan0 outputs='jtag'，需要使用 c_decoder_put_python() 输出 jtag 协议数据。
     已有 jtag_c.c C decoder，cjtag-oscan0 的 Python 输出格式必须与 jtag_c.c 的
     c_decoder_put_python() 输出格式兼容，以便上层解码器（如 jtag_stm32）可以堆叠。
     参考 jtag_c.c 的 Python 输出格式。 -->

---

## 4. RGB LED WS281x

### 4.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `rgb_led_ws281x` |
| name | `RGB LED WS2812+` |
| longname | `RGB LED color decoder` |
| desc | `Decodes colors from bus pulses for single wire RGB leds like APA106, WS2811, WS2812, WS2813, SK6812, TM1829, TM1814, and TX1812.` |
| license | `gplv3+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Display', 'IC']` |

**channels:**
| id | name | desc | idn |
|----|------|------|-----|
| din | DIN | DIN data line | dec_rgb_led_ws281x_chan_din |

**options:**
| id | desc | default | values |
|----|------|---------|--------|
| colors | Colors | 'GRB' | ('GRB','RGB','BRG','RBG','BGR','GRBW','RGBW','WRGB','LBGR','LGRB','LRGB','LRBG','LGBR','LBRG') |
| polarity | Polarity | 'normal' | ('normal', 'inverted') |

**annotations (3 classes):**
| index | id | label |
|-------|----|-------|
| 0 | bit | Bit |
| 1 | reset | RESET |
| 2 | rgb | RGB |

**annotation_rows:**
| id | name | classes |
|----|------|---------|
| bit | Bits | (0, 1) |
| rgb | RGB | (2,) |

### 4.2 解码逻辑分析

WS281x 是单线 RGB LED 协议，基于脉宽编码：

**编码规则：**
- Bit 1: 高电平时间 ≥ 625ns，或占空比 > 50%
- Bit 0: 高电平时间 < 625ns 且占空比 ≤ 50%
- RESET: 低电平时间 > 50μs

**状态机：**
1. `FIND RESET`: 等待低电平（normal polarity）或高电平（inverted）
2. `RESET`: 检测到 >50μs 低电平后，输出 RESET annotation
3. `BIT FALLING`: 等待下一个 edge，判断是 bit 还是 RESET
4. `BIT RISING`: 检测 bit 值，输出 annotation

**颜色解析：**
- 24-bit 模式 (GRB/RGB/BRG/RBG/BGR): 3 bytes
- 32-bit 模式 (GRBW/RGBW/WRGB/LBGR/LGRB/LRGB/LRBG/LGBR/LBRG): 4 bytes
- 根据 colors 选项重排字节顺序

**关键时序参数：**
- tH ≥ 625ns → bit 1
- tH < 625ns 且 tH/period > 0.5 → bit 1
- tH/period ≤ 0.5 → bit 0
- 低电平 > 50μs → RESET
- 3μs < 低电平 < 50μs → 可能是 bit（不是 RESET）

### 4.3 C 实现计划

**文件名:** `rgb_led_ws281x_c.c`

**state struct:**
```c
typedef struct {
    uint64_t samplerate;
    int out_ann;

    int state;             // FIND_RESET, RESET, BIT_FALLING, BIT_RISING
    uint64_t ss_packet;    // 当前 packet 起始
    uint64_t ss;           // 当前脉冲起始
    uint64_t es;           // 当前脉冲结束
    int bits[32];          // 收集的 bits
    int bit_count;
    int colorsize;         // 24 或 32
    int bit_val;           // 当前 bit 值

    // 选项
    int color_mode;        // 0=GRB, 1=RGB, etc.
    int polarity;          // 0=normal, 1=inverted
} ws281x_state;
```

**时序计算（需要 samplerate）：**
```c
// 625ns in samples
uint64_t tH_threshold = (uint64_t)(s->samplerate * 625e-9);
// 50μs in samples
uint64_t reset_threshold = (uint64_t)(s->samplerate * 50e-6);
// 3μs in samples
uint64_t bit_threshold = (uint64_t)(s->samplerate * 3e-6);
```

**decode() 核心逻辑:**
```c
while (1) {
    switch (s->state) {
    case STATE_FIND_RESET:
        // 等待低电平（normal）或高电平（inverted）
        cb = c_cond_new();
        if (s->polarity == 0) c_cond_low(cb, 0);
        else c_cond_high(cb, 0);
        c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        s->ss = samplenum;

        // 等待下一个 edge
        cb = c_cond_new();
        c_cond_edge(cb, 0);
        c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        s->es = samplenum;

        if ((s->es - s->ss) > reset_threshold) {
            s->state = STATE_RESET;
        } else if ((s->es - s->ss) > bit_threshold) {
            // 不是 RESET，可能是 bit
            s->bit_count = 0;
            s->ss = samplenum;
            s->ss_packet = samplenum;
            cb = c_cond_new();
            c_cond_edge(cb, 0);
            c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            s->state = STATE_BIT_FALLING;
        }
        break;

    case STATE_RESET:
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, 1, "RESET", "RST", "R");
        s->bit_count = 0;
        s->ss = samplenum;
        s->ss_packet = samplenum;
        cb = c_cond_new();
        c_cond_edge(cb, 0);
        c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        s->state = STATE_BIT_FALLING;
        break;

    case STATE_BIT_FALLING:
        s->es = samplenum;
        cb = c_cond_new();
        c_cond_edge(cb, 0);
        c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);

        if ((samplenum - s->es) > reset_threshold) {
            // 检测 bit 值
            check_bit(s, samplenum);
            // 输出 bit
            // 添加到 bits
            // 检查完整颜色
            s->ss = s->es;
            s->es = samplenum;
            s->state = STATE_RESET;
        } else {
            s->state = STATE_BIT_RISING;
        }
        break;

    case STATE_BIT_RISING:
        check_bit(s, samplenum);
        // 输出 bit
        // 添加到 bits
        // 检查完整颜色
        s->ss = samplenum;
        cb = c_cond_new();
        c_cond_edge(cb, 0);
        c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        s->state = STATE_BIT_FALLING;
        break;
    }
}
```

**颜色格式化关键代码:**
```c
static void ws281x_output_color(struct srd_decoder_inst *di, ws281x_state *s)
{
    uint32_t elems = 0;
    for (int i = 0; i < s->colorsize; i++) {
        elems = (elems << 1) | s->bits[i];
    }

    char color_str[64];
    if (s->colorsize == 24) {
        uint32_t rgb;
        switch (s->color_mode) {
        case 0: // GRB
            rgb = (elems & 0xff0000) >> 8 | (elems & 0x00ff00) << 8 | (elems & 0x0000ff);
            snprintf(color_str, sizeof(color_str), "GRB#%06x", rgb);
            break;
        case 1: // RGB
            rgb = elems;
            snprintf(color_str, sizeof(color_str), "RGB#%06x", rgb);
            break;
        // ... 其他模式
        }
    } else { // 32-bit
        // ... GRBW, RGBW, WRGB, LBGR, etc.
    }

    C_ANN_PUT(di, s->ss_packet, s->es, s->out_ann, 2, color_str);
}
```

**samplerate guard:** 必须！需要 samplerate 计算时序阈值。metadata 回调 + decode() 入口 fallback。

**难度:** ⭐⭐⭐ — 相对简单的状态机，但需要精确的时序计算和多种颜色格式支持

---

## 5. STEPPER MOTOR

### 5.1 Python 元数据

| 字段 | 值 |
|------|-----|
| id | `stepper_motor` |
| name | `Stepper motor` |
| longname | `Stepper motor position / speed` |
| desc | `Absolute position and movement speed from step/dir.` |
| license | `gplv2+` |
| inputs | `['logic']` |
| outputs | `[]` |
| tags | `['Embedded/industrial']` |

**channels (2):**
| id | name | desc | idn |
|----|------|------|-----|
| step | Step | Step pulse | dec_stepper_motor_chan_step |
| dir | Direction | Direction select | dec_stepper_motor_chan_dir |

**options:**
| id | desc | default | idn |
|----|------|---------|-----|
| unit | Unit | 'steps' | dec_stepper_motor_opt_unit |
| steps_per_mm | Steps per mm | 100.0 | dec_stepper_motor_opt_steps_per_mm |

**annotations (2 classes):**
| index | id | label |
|-------|----|-------|
| 0 | speed | Speed |
| 1 | position | Position |

**annotation_rows:**
| id | name | classes |
|----|------|---------|
| speed | Speed | (0,) |
| position | Position | (1,) |

### 5.2 解码逻辑分析

步进电机解码器非常简单：

1. 等待 Step 信号上升沿: `self.wait({0: 'r'})`
2. 读取 Direction 信号
3. 计算速度: `speed = samplerate / (ss - ss_prev_step) / scale`
4. 更新位置: `pos += (1 if direction else -1)`
5. 输出速度和位置 annotation

**关键参数：**
- `scale`: 1 (steps) 或 `steps_per_mm` (mm)
- `format`: '%0.0f' (steps) 或 '%0.2f' (mm)

### 5.3 C 实现计划

**文件名:** `stepper_motor_c.c`

**state struct:**
```c
typedef struct {
    uint64_t samplerate;
    int out_ann;

    uint64_t ss_prev_step;
    int64_t pos;
    double scale;
    int is_mm;             // 0=steps, 1=mm

    // 选项
    double steps_per_mm;
} stepper_state;
```

**decode() 核心逻辑:**
```c
static void stepper_decode(struct srd_decoder_inst *di)
{
    stepper_state *s = (stepper_state *)c_decoder_get_private(di);
    uint64_t samplenum;
    uint64_t matched;

    if (!s->samplerate) {
        s->samplerate = c_decoder_get_samplerate(di);
    }
    if (s->samplerate == 0) return;

    while (1) {
        srd_cond_builder *cb = c_cond_new();
        c_cond_rise(cb, 0);  // Step 上升沿
        int ret = c_cond_wait(cb, di, &samplenum, &matched);
        c_cond_free(cb);
        if (ret != SRD_OK) return;

        int direction = c_decoder_get_pin(di, 1, samplenum);

        if (s->ss_prev_step != 0) {
            uint64_t delta = samplenum - s->ss_prev_step;

            // 速度
            double speed = (double)s->samplerate / (double)delta / s->scale;
            char speed_str[64];
            if (s->is_mm)
                snprintf(speed_str, sizeof(speed_str), "%.2f mm/s", speed);
            else
                snprintf(speed_str, sizeof(speed_str), "%.0f steps/s", speed);

            char speed_short[32];
            if (s->is_mm)
                snprintf(speed_short, sizeof(speed_short), "%.2f", speed);
            else
                snprintf(speed_short, sizeof(speed_short), "%.0f", speed);

            C_ANN_PUT(di, s->ss_prev_step, samplenum, s->out_ann, 0,
                      speed_str, speed_short);

            // 位置
            char pos_str[64];
            if (s->is_mm)
                snprintf(pos_str, sizeof(pos_str), "%.2f mm", (double)s->pos / s->scale);
            else
                snprintf(pos_str, sizeof(pos_str), "%.0f steps", (double)s->pos / s->scale);

            char pos_short[32];
            if (s->is_mm)
                snprintf(pos_short, sizeof(pos_short), "%.2f", (double)s->pos / s->scale);
            else
                snprintf(pos_short, sizeof(pos_short), "%.0f", (double)s->pos / s->scale);

            C_ANN_PUT(di, s->ss_prev_step, samplenum, s->out_ann, 1,
                      pos_str, pos_short);
        }

        s->pos += direction ? 1 : -1;
        s->ss_prev_step = samplenum;
    }
}
```

**samplerate guard:** 必须！需要 samplerate 计算速度。metadata 回调 + decode() 入口 fallback。

**难度:** ⭐ — 最简单的 decoder 之一

---

## 6. 通用实现规范

### 6.1 文件命名

| Python id | C 文件名 | C decoder id |
|-----------|----------|-------------|
| bean | `bean_c.c` | `bean_c` |
| ccd | `ccd_c.c` | `ccd_c` |
| cjtag-oscan0 | `cjtag_oscan0_c.c` | `cjtag_oscan0_c` |
| rgb_led_ws281x | `rgb_led_ws281x_c.c` | `rgb_led_ws281x_c` |
| stepper_motor | `stepper_motor_c.c` | `stepper_motor_c` |

### 6.2 struct srd_c_decoder 规范

每个 decoder 必须包含：
```c
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX(C)",
    .longname = "...",
    .desc = "... (C implementation)",
    .license = "gplv2+",   // 或 gplv3+ for ws281x
    .channels = xxx_channels,
    .num_channels = N,
    .optional_channels = NULL,  // 或 xxx_optional_channels
    .num_optional_channels = 0,
    .options = xxx_options_arr,
    .num_options = N,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = N,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,
    .num_inputs = 1,
    .outputs = xxx_outputs,
    .num_outputs = N,
    .binary = NULL,
    .num_binary = 0,
    .tags = xxx_tags,
    .num_tags = 1,
    .metadata = xxx_metadata,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,
    .destroy = xxx_destroy,
};
```

### 6.3 ann_labels 规范

第一列必须为空字符串 `""`：
```c
static const char* xxx_ann_labels[][3] = {
    { "", "class-id", "Class Label" },
    // ...
};
```

### 6.4 annotation_rows 规范

所有 annotation classes 必须映射到 annotation_rows：
```c
static const int xxx_row_xxx_classes[] = { ANN_XXX, -1 };
static const struct srd_c_ann_row xxx_ann_rows[] = {
    { "row-id", "Row Label", xxx_row_xxx_classes, count },
};
```

### 6.5 samplerate guard 模式

```c
static void xxx_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    xxx_state *s = (xxx_state *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->samplerate = value;
        // 计算时序参数
    }
}

static void xxx_decode(struct srd_decoder_inst *di)
{
    xxx_state *s = (xxx_state *)c_decoder_get_private(di);
    if (!s->samplerate) {
        s->samplerate = c_decoder_get_samplerate(di);
    }
    if (s->samplerate == 0) return;  // 无法解码
    // ...
}
```

### 6.6 Options 初始化模式

在 `srd_c_decoder_entry()` 中初始化：
```c
SRD_C_DECODER_EXPORT struct srd_c_decoder* srd_c_decoder_entry(void)
{
    // 字符串选项
    GVariant *vals[] = { g_variant_new_string("val1"), g_variant_new_string("val2") };
    GSList *list = NULL;
    list = g_slist_append(list, vals[0]);
    list = g_slist_append(list, vals[1]);
    xxx_options_arr[0].id = "opt_id";
    xxx_options_arr[0].idn = "dec_xxx_opt_opt_id";
    xxx_options_arr[0].desc = "Option description";
    xxx_options_arr[0].def = g_variant_new_string("val1");
    xxx_options_arr[0].values = list;

    // 整数选项
    xxx_options_arr[1].id = "opt_int";
    xxx_options_arr[1].idn = "dec_xxx_opt_opt_int";
    xxx_options_arr[1].desc = "Integer option";
    xxx_options_arr[1].def = g_variant_new_int64(100);
    xxx_options_arr[1].values = NULL;

    // 浮点选项
    xxx_options_arr[2].id = "opt_double";
    xxx_options_arr[2].idn = "dec_xxx_opt_opt_double";
    xxx_options_arr[2].desc = "Double option";
    xxx_options_arr[2].def = g_variant_new_double(100.0);
    xxx_options_arr[2].values = NULL;

    return &xxx_c_decoder;
}
```

### 6.7 Condition Builder API 使用

```c
srd_cond_builder *cb = c_cond_new();
c_cond_rise(cb, channel);     // 上升沿
c_cond_fall(cb, channel);     // 下降沿
c_cond_edge(cb, channel);     // 任意边沿
c_cond_high(cb, channel);     // 高电平
c_cond_low(cb, channel);      // 低电平
c_cond_skip(cb, count);       // 跳过 N samples
c_cond_or(cb);                // 或条件
c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

<!-- Updated: 以下 API 已实现，可在本批次解码器中使用：
     - c_cond_wait_current(di, &samplenum) — 等效于 Python self.wait({})，
       获取当前采样位置而不前进。适用于初始化时读取引脚状态。
       参考 spi_c.c 中的使用方式。
     - c_decoder_get_initial_pin(di, ch) — 获取初始引脚值（old_pins_array），
       等效于 Python self.oldpin。适用于解码器启动时需要知道初始信号状态。
       注意：当前所有已有 C 解码器均未使用此 API，但在需要初始引脚值的
       场景下（如 CCD 的初始 bus 状态）可以使用。 -->

### 6.8 CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加：
```
bean_c
ccd_c
cjtag_oscan0_c
rgb_led_ws281x_c
stepper_motor_c
```

### 6.9 难度排序（推荐实现顺序）

1. **stepper_motor** ⭐ — 最简单，适合验证框架
2. **rgb_led_ws281x** ⭐⭐⭐ — 中等复杂度，时序解码
3. **bean** ⭐⭐⭐⭐ — 复杂脉宽编码 + bit stuffing
4. **ccd** ⭐⭐⭐⭐⭐ — 双层状态机 + 动态 wait + UART 解码
5. **cjtag_oscan0** ⭐⭐⭐⭐⭐ — 复杂 cJTAG 状态机 + OScan1 解码

<!-- Updated: 已实现的关键 API 补充说明：
     1. SRD_OUTPUT_LOGIC + c_decoder_put_logic() — 已实现。如果解码器需要输出
        逻辑信号给其他解码器（如 cjtag-oscan0 输出 jtag 信号），可以使用此 API。
        签名：c_decoder_put_logic(di, ss, es, out_id, channel_mask, values, num_channels)
        注意：cjtag-oscan0 当前设计使用 SRD_OUTPUT_PYTHON 输出 jtag 协议数据，
        而非 SRD_OUTPUT_LOGIC 输出原始逻辑信号。如果上层解码器需要原始信号，
        则需额外注册 SRD_OUTPUT_LOGIC 输出。
     2. BITS v2 格式（per-bit ss/es 时间戳）— 已在 spi_c.c 和 i2c_c.c 中实现。
        本批次解码器均不输出 spi/i2c 协议，因此不需要使用 BITS v2 格式。
        详见 c_decoder_utils.h 中的格式说明。
     3. SPI DATA 格式（17字节）— 已在 spi_c.c 中实现。本批次解码器不涉及。
     4. C解码器依赖规则 — C解码器只能依赖已有C实现的底层解码器。
        本批次所有解码器 inputs=['logic']，无依赖阻塞问题。
        CCD outputs='ccd' 和 cjtag-oscan0 outputs='jtag' 需要确保 Python 输出
        格式与对应 Python 解码器兼容。 -->
