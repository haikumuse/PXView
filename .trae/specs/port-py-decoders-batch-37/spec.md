# Batch 37: Python 解码器移植为 C 解码器 — 详细规格

## 概述

本批次包含 6 个上层 Python 解码器，需要移植为 C 解码器。这些解码器均为上层协议解码器，通过 `recv_proto()` 回调接收下层解码器传递的协议数据。

> **⚠️ 审查结论：本批次全部 6 个解码器均被下层依赖阻塞，当前无法实施。**
> <!-- Updated: 所有下层解码器均无C实现，违反"C解码器只能依赖已有C实现的底层解码器"规则 -->

### 解码器列表

| # | Python ID | C 文件名 | C ID | 输入协议 | 下层C解码器 | 状态 | 复杂度 |
|---|-----------|----------|------|----------|------------|------|--------|
| 1 | `sipi` | `sipi_c.c` | `sipi_c` | `lfast` | lfast_c (❌不存在) | 🔴阻塞 | ★★★ |
| 2 | `pjon` | `pjon_c.c` | `pjon_c` | `pjon_link` | pjdl_c (❌不存在) | 🔴阻塞 | ★★★★ |
| 3 | `tpm_fifo_tis` | `tpm_fifo_tis_c.c` | `tpm_fifo_tis_c` | `tpm-tis` | tpm_tis_spi_c / tpm_tis_i2c_c (❌不存在) | 🔴阻塞 | ★★★★★ |
| 4 | `tm1637` | `tm1637_c.c` | `tm1637_c` | `tmc` | tmc_c (❌不存在) | 🔴阻塞 | ★★★ |
| 5 | `tm1638` | `tm1638_c.c` | `tm1638_c` | `tmc` | tmc_c (❌不存在) | 🔴阻塞 | ★★★ |
| 6 | `ltar_smartdevice_decode` | `ltar_smartdevice_decode_c.c` | `ltar_smartdevice_decode_c` | `ltar_smartdevice` | ltar_smartdevice_c → afsk_c (❌均不存在) | 🔴阻塞 | ★★ |
<!-- Updated: 添加下层C解码器依赖状态列和阻塞标记。经检查，所有6个解码器的下层依赖均无C实现 -->

---
## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |

<!-- Updated: 参考实现文件lm75_c.c和ds1307_c.c均存在且包含recv_proto实现，引用有效。
     但需注意：这两个范本均为I2C上层解码器，其recv_proto接收的cmd为I2C协议命令
     (START/STOP/ADDRESS READ/WRITE/DATA READ/WRITE)。
     本批次的6个解码器分别依赖不同下层协议(lfast/pjdl/tpm-tis/tmc/ltar_smartdevice)，
     其recv_proto的cmd和数据格式与I2C范本差异较大，仅状态机模式和recv_proto框架可参考。 -->

### 下层依赖链完整分析

<!-- Updated: 新增下层依赖链分析，明确每个解码器的完整依赖路径和阻塞原因 -->

| 本批次解码器 | 直接输入协议 | 提供该协议的Python解码器 | 下层Python解码器的输入 | 是否有C实现 |
|-------------|------------|------------------------|----------------------|-----------|
| sipi | lfast | lfast | logic | ❌ lfast_c 不存在 |
| pjon | pjon_link | pjdl | logic | ❌ pjdl_c 不存在 |
| tpm_fifo_tis | tpm-tis | tpm_tis_spi / tpm_tis_i2c | spi / i2c | ❌ 两者均无C实现（spi_c/i2c_c已存在但上层无） |
| tm1637 | tmc | tmc | logic | ❌ tmc_c 不存在 |
| tm1638 | tmc | tmc | logic | ❌ tmc_c 不存在 |
| ltar_smartdevice_decode | ltar_smartdevice | ltar_smartdevice | afsk_bits | ❌ ltar_smartdevice_c 不存在 |
| (ltar_smartdevice的依赖) | afsk_bits | afsk | logic | ❌ afsk_c 不存在 |

## 1. sipi — NXP SIPI (Zipwire) 接口

### 1.1 Python 元数据

```python
id = 'sipi'
name = 'SIPI (Zipwire)'
longname = 'NXP SIPI interface'
desc = 'Serial Inter-Processor Interface (SIPI) aka Zipwire, aka HSSL'
license = 'gplv2+'
inputs = ['lfast']
outputs = []
tags = ['Embedded/industrial']
```

### 1.2 Annotations (7 个)

| 索引 | Python ID | C 宏 | C 标签列 |
|------|-----------|------|----------|
| 0 | `header_tag` | `ANN_HEADER_TAG` | `""`, `"Transaction Tag"` |
| 1 | `header_cmd` | `ANN_HEADER_CMD` | `""`, `"Command Code"` |
| 2 | `header_ch` | `ANN_HEADER_CH` | `""`, `"Channel"` |
| 3 | `address` | `ANN_ADDRESS` | `""`, `"Address"` |
| 4 | `data` | `ANN_DATA` | `""`, `"Data"` |
| 5 | `crc` | `ANN_CRC` | `""`, `"CRC"` |
| 6 | `warning` | `ANN_WARNING` | `""`, `"Warning"` |

### 1.3 Annotation Rows (2 行)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `fields` | Fields | (0, 1, 2, 3, 4, 5) |
| `warnings` | Warnings | (6,) |

### 1.4 解码逻辑分析

**输入格式**：`data` 是一个列表，每个元素为 `[ss, es, value]`（字节级数据）

**核心流程**：

1. **计算 bit_len**：`(data[0][1] - data[0][0]) / 8.0`
2. **解析 Header** (2 bytes)：
   - Tag (3 bits)：bits 15-13
   - Command Code (5 bits)：bits 12-8 → 查 `command_codes` 表获取命令名、地址长度、数据长度
   - Reserved (4 bits)：bits 7-4，应为 0
   - Channel (3 bits)：bits 3-1
   - Reserved (1 bit)：bit 0，应为 0
3. **解析 Payload**：根据 `addr_len` 和 `data_len` 提取地址和数据字节
4. **解析 CRC** (2 bytes)：使用 CRC-CCITT (crc_hqx) 验证

**command_codes 查找表** (13 条有效命令)：

| Code | 名称 | 地址字节 | 数据字节 |
|------|------|----------|----------|
| 0b00000 | Read byte | 4 | 0 |
| 0b00001 | Read 2 byte | 4 | 0 |
| 0b00010 | Read 4 byte | 4 | 0 |
| 0b00100 | Write byte with ACK | 4 | 4 |
| 0b00101 | Write 2 byte with ACK | 4 | 4 |
| 0b00110 | Write 4 byte with ACK | 4 | 4 |
| 0b01000 | ACK | 0 | 0 |
| 0b01001 | NACK (Target Error) | 0 | 0 |
| 0b01010 | Read Answer with ACK | 4 | 4 |
| 0b01100 | Trigger with ACK | 0 | 0 |
| 0b10010 | Read 4-byte JTAG ID | 0 | 0 |
| 0b10111 | Stream 32 byte with ACK | 0 | 32 |

### 1.5 C 实现方案

**文件**：`sipi_c.c`

**command_codes 结构体**：
```c
typedef struct {
    uint8_t code;
    const char *name;
    int addr_len;
    int data_len;
} sipi_command_entry;

static const sipi_command_entry command_table[] = {
    {0x00, "Read byte", 4, 0},
    {0x01, "Read 2 byte", 4, 0},
    {0x02, "Read 4 byte", 4, 0},
    {0x04, "Write byte with ACK", 4, 4},
    {0x05, "Write 2 byte with ACK", 4, 4},
    {0x06, "Write 4 byte with ACK", 4, 4},
    {0x08, "ACK", 0, 0},
    {0x09, "NACK (Target Error)", 0, 0},
    {0x0A, "Read Answer with ACK", 4, 4},
    {0x0C, "Trigger with ACK", 0, 0},
    {0x12, "Read 4-byte JTAG ID", 0, 0},
    {0x17, "Stream 32 byte with ACK", 0, 32},
};
```

**私有结构体**：
```c
typedef struct {
    int out_ann;
    int out_binary;
    double bit_len;
    int addr_len;
    int data_len;
    int frame_len;
} sipi_state;
```

**CRC-CCITT 实现**：
```c
static uint16_t crc_ccitt(const uint8_t *data, int len)
{
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}
```

**recv_proto 回调**：
- `cmd = "DATA"`：data 包含字节列表 `[ss0, es0, val0, ss1, es1, val1, ...]`
- 需要设计 lfast → sipi 的数据传递格式
<!-- Updated: lfast Python解码器使用OUTPUT_PYTHON直接输出payload列表[(ss, es, value), ...]，
     而非recv_proto机制。C版本需要lfast_c通过c_decoder_put_python()输出"DATA"命令，
     sipi_c通过recv_proto接收。数据格式需在lfast_c实现时定义。
     当前lfast_c不存在，此数据传递格式为待定设计。 -->

**关键 C 代码片段**：
```c
static void sipi_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    sipi_state *s = (sipi_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") == 0) {
        // 解析字节列表
        // 计算 bit_len
        // 解析 Header (2 bytes)
        // 解析 Payload
        // 验证 CRC
    }
}
```

### 1.6 难点与注意事项

1. **bit_len 计算**：需要从字节级数据推算 bit 宽度
2. **CRC-CCITT**：Python 使用 `binascii.crc_hqx()`，C 需要手动实现
3. **Header 位域提取**：16-bit header 的位域解析
4. **变长帧**：frame_len 取决于 command code
5. **🔴下层依赖阻塞**：lfast_c 不存在，无法实施。需先完成 lfast → lfast_c 的移植
<!-- Updated: 新增第5点，标注下层依赖阻塞 -->

---

## 2. pjon — PJON 协议

### 2.1 Python 元数据

```python
id = 'pjon'
name = 'PJON'
longname = 'PJON'
desc = 'The PJON protocol.'
license = 'gplv2+'
inputs = ['pjon_link']
outputs = []
tags = ['Embedded/industrial']
```

### 2.2 Annotations (13 个)

| 索引 | Python ID | C 宏 | C 标签列 |
|------|-----------|------|----------|
| 0 | `rx_info` | `ANN_RX_INFO` | `""`, `"Receiver ID"` |
| 1 | `hdr_cfg` | `ANN_HDR_CFG` | `""`, `"Header config"` |
| 2 | `pkt_len` | `ANN_PKT_LEN` | `""`, `"Packet length"` |
| 3 | `meta_crc` | `ANN_META_CRC` | `""`, `"Meta CRC"` |
| 4 | `tx_info` | `ANN_TX_INFO` | `""`, `"Sender ID"` |
| 5 | `port` | `ANN_SVC_ID` | `""`, `"Service ID"` |
| 6 | `pkt_id` | `ANN_PKT_ID` | `""`, `"Packet ID"` |
| 7 | `anon` | `ANN_ANON_DATA` | `""`, `"Anonymous data"` |
| 8 | `payload` | `ANN_PAYLOAD` | `""`, `"Payload"` |
| 9 | `end_crc` | `ANN_END_CRC` | `""`, `"End CRC"` |
| 10 | `syn_rsp` | `ANN_SYN_RSP` | `""`, `"Sync response"` |
| 11 | `relation` | `ANN_RELATION` | `""`, `"Relation"` |
| 12 | `warning` | `ANN_WARN` | `""`, `"Warning"` |

### 2.3 Annotation Rows (3 行)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `fields` | Fields | (0-5, 7-10) |
| `relations` | Relations | (11,) |
| `warnings` | Warnings | (12,) |

### 2.4 解码逻辑分析

**输入格式**：`(ptype, pdata)`
- `ptype` 可以是：`"FRAME_INIT"`, `"IDLE"`, `"FRAME_DATA"`, `"SYNC_RESP_WAIT"`, `"DATA_BYTE"`

**核心流程**：

1. **FRAME_INIT**：开始新帧，重置状态，初始化字段描述
2. **DATA_BYTE**：累积帧字节
   - 根据字段描述（format, width）判断是否收集够一个字段
   - 收集够后调用对应 handler 处理
3. **SYNC_RESP_WAIT**：切换到 ACK 收集模式
4. **IDLE/FRAME_DATA**：刷新帧，输出 relation

**字段描述系统**（Python 动态注册，C 需要静态实现）：
- 固定字段：RX ID (1 byte), Header Config (1 byte)
- 可变字段（取决于 Header Config 的 flags）：
  - Packet Length (1 or 2 bytes)
  - Meta CRC (1 byte)
  - RX Bus ID (4 bytes, if shared)
  - TX Bus ID (4 bytes, if shared+tx_info)
  - TX ID (1 byte, if tx_info)
  - Port/Service ID (2 bytes, if port)
  - Packet ID (2 bytes, if pkt_id)
  - Payload (variable)
  - End CRC (1 or 4 bytes)

**Header Config Flags** (8 bits)：
- bit 0: shared (bus_id)
- bit 1: tx_info (sender address)
- bit 2: sync_ack
- bit 3: async_ack
- bit 4: port (service ID)
- bit 5: crc32
- bit 6: len16
- bit 7: pkt_id

**CRC 算法**：
- CRC-8：多项式 0x97
- CRC-32：标准 Ethernet CRC-32

### 2.5 C 实现方案

**文件**：`pjon_c.c`

**私有结构体**：
```c
#define PJON_MAX_FRAME_BYTES 65536
#define PJON_MAX_FIELDS 16

typedef struct {
    int out_ann;
    // Frame state
    uint8_t *frame_bytes;
    int frame_byte_count;
    uint64_t frame_ss;
    uint64_t frame_es;
    // Config flags
    uint8_t cfg_shared, cfg_tx_info, cfg_sync_ack, cfg_async_ack;
    uint8_t cfg_port, cfg_crc32, cfg_len16, cfg_pkt_id;
    int cfg_overhead;
    // Field scanning
    int field_idx;
    int field_got;
    int field_widths[PJON_MAX_FIELDS];
    int field_ann_classes[PJON_MAX_FIELDS];
    int num_fields;
    // Annotation position
    uint64_t ann_ss;
    uint64_t ann_es;
    // ACK state
    uint8_t *ack_bytes;
    int ack_byte_count;
    // Relation tracking
    int frame_rx_id;
    int frame_tx_id;
    char *frame_payload_text;
    int frame_has_ack;
} pjon_state;
```

**CRC 实现**：
```c
static uint8_t calc_crc8(const uint8_t *data, int len)
{
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            int odd = crc & 1;
            crc >>= 1;
            if (odd) crc ^= 0x97;
        }
    }
    return crc;
}

static uint32_t calc_crc32(const uint8_t *data, int len)
{
    uint32_t crc = 0xffffffff;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            int odd = crc & 1;
            crc >>= 1;
            if (odd) crc ^= 0xedb88320;
        }
    }
    return crc ^ 0xffffffff;
}
```

**recv_proto 回调**：
- `cmd = "FRAME_INIT"`：重置帧状态
- `cmd = "DATA_BYTE"`：累积字节，处理字段
- `cmd = "SYNC_RESP_WAIT"`：切换 ACK 模式
- `cmd = "IDLE"` / `"FRAME_DATA"`：刷新帧

**关键 C 代码片段**：
```c
static void pjon_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    pjon_state *s = (pjon_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "FRAME_INIT") == 0) {
        pjon_frame_flush(di, s);
        pjon_reset_frame(s);
        s->frame_ss = start_sample;
        s->frame_es = end_sample;
    } else if (strcmp(cmd, "DATA_BYTE") == 0) {
        uint8_t b = (data_len > 0) ? data[0] : 0;
        // 累积字节，检查字段完成
        // 调用对应 handler
    } else if (strcmp(cmd, "SYNC_RESP_WAIT") == 0) {
        s->ack_byte_count = 0;
    } else if (strcmp(cmd, "IDLE") == 0 || strcmp(cmd, "FRAME_DATA") == 0) {
        pjon_frame_flush(di, s);
        pjon_reset_frame(s);
    }
}
```
<!-- Updated: strcmp(cmd, "IDLE") == 0 || strcmp(cmd, "FRAME_DATA") == 0 模式正确，
     与Python版 ptype in ('IDLE', 'FRAME_DATA') 逻辑一致，无bug。
     Python版pjon解码器(pjon/pd.py L525)对IDLE和FRAME_DATA执行相同操作(flush+reset)，
     C版使用||合并两个条件是正确的。 -->

### 2.6 难点与注意事项

1. **动态字段系统**：Python 使用动态注册字段描述，C 需要改为静态字段表 + 条件跳过
2. **Header Config 解析**：8 个 flag 位决定帧格式
3. **Payload 长度计算**：需要从 packet length 减去 overhead
4. **两种 CRC**：CRC-8 和 CRC-32
5. **Relation 输出**：帧结束后输出通信关系
6. **ACK 处理**：SYNC_RESP_WAIT 后的字节作为 ACK
7. **🔴下层依赖阻塞**：pjdl_c 不存在，无法实施。pjdl是pjon_link协议的唯一提供者，需先完成 pjdl → pjdl_c 的移植
<!-- Updated: 新增第7点，标注下层依赖阻塞。pjdl(Padded Jittering Data Link)是pjon的链路层，
     inputs=['logic'], outputs=['pjon_link']，当前无C实现 -->

---

## 3. tpm_fifo_tis — TPM FIFO TIS 接口

### 3.1 Python 元数据

```python
id = 'tpm_fifo_tis'
name = 'TPM FIFO'
longname = 'Trusted Platform Module Commands over TIS 2.0 interface'
desc = 'Trusted Platform Module Commands over TIS 2.0 interface'
license = 'gplv3+'
inputs = ['tpm-tis']
outputs = ['tpm']
tags = ['TPM']
```

### 3.2 Annotations (6 个)

| 索引 | Python ID | C 宏 | C 标签列 |
|------|-----------|------|----------|
| 0 | `register-read` | `ANN_REG_READ` | `""`, `"Register Read"` |
| 1 | `register-write` | `ANN_REG_WRITE` | `""`, `"Register Write"` |
| 2 | `tpm-command` | `ANN_TPM_CMD` | `""`, `"TPM Command"` |
| 3 | `tpm-response` | `ANN_TPM_RSP` | `""`, `"TPM Response"` |
| 4 | `warning` | `ANN_WARN` | `""`, `"Warning"` |
| 5 | `state` | `ANN_STATE` | `""`, `"State"` |

### 3.3 Annotation Rows (4 行)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `register` | Register Transaction | (0, 1) |
| `tpm` | TPM Command/Response | (2, 3) |
| `warnings` | Warnings | (4,) |
| `states` | TPM States | (5,) |

### 3.4 解码逻辑分析

**输入格式**：`(ptype, xfer)`
- `ptype = "TRANSACTION"`
- `xfer` 包含：`addr`, `data`, `reading` (bool)

**核心流程**：基于状态机的 TPM TIS 协议解码

**TPM 状态**：
- Unknown → Idle → Ready → Reception → Execution → Completion → Idle

**关键寄存器**：
- `TPM_STS_X (0x0018)`：状态寄存器
  - `commandReady (0x40)`, `tpmGo (0x20)`, `dataAvail (0x10)`, `Expect (0x08)`, `selfTestDone (0x04)`, `responseRetry (0x02)`, `stsValid (0x80)`
- `TPM_DATA_FIFO_X (0x0024)`：数据 FIFO

**命令/响应缓冲**：
- 写入 FIFO 的数据累积为 `command_buffer`
- 读取 FIFO 的数据累积为 `response_buffer`
- 命令完成时输出 TPM Command annotation
- 响应完成时输出 TPM Response annotation

**TPM 命令码查找表**：~80 条命令名称（`TPM_COMMAND_CODE_NAMES`）
**TPM 响应码查找表**：~70 条响应名称（`TPM_RESPONSE_CODE_NAMES`）

### 3.5 C 实现方案

**文件**：`tpm_fifo_tis_c.c`

**私有结构体**：
```c
#define TPM_MAX_BUFFER 4096

typedef struct {
    int out_ann;
    int out_py;
    // TPM State
    int state;  // enum TpmState
    int state_finished;
    uint64_t state_start;
    // Command buffer
    uint8_t command_buffer[TPM_MAX_BUFFER];
    int command_len;
    uint64_t command_start;
    // Response buffer
    uint8_t response_buffer[TPM_MAX_BUFFER];
    int response_len;
    uint64_t response_start;
} tpm_state;
```

**recv_proto 回调**：
- `cmd = "TRANSACTION"`：data 包含 addr, data, reading flag
<!-- Updated: TRANSACTION的数据格式需要在tpm_tis_spi_c或tpm_tis_i2c_c实现时定义。
     Python版tpm_tis_spi接收SPI DATA(mosi/miso)，输出('TRANSACTION', xfer)。
     xfer包含addr(int), data(bytes), reading(bool)。
     C版需要将这些序列化为二进制格式通过c_decoder_put_python传递。
     当前tpm_tis_spi_c和tpm_tis_i2c_c均不存在，此格式为待定设计。 -->

**关键 C 代码片段**：
```c
static void tpm_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    tpm_state *s = (tpm_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "TRANSACTION") != 0) return;

    // 解析 xfer: addr, data, reading
    uint32_t addr = ...;
    int reading = ...;
    const uint8_t *xfer_data = ...;
    int xfer_len = ...;

    if (reading)
        tpm_on_read(di, s, addr, xfer_data, xfer_len, start_sample, end_sample);
    else
        tpm_on_write(di, s, addr, xfer_data, xfer_len, start_sample, end_sample);
}
```

### 3.6 难点与注意事项

1. **极其复杂**：状态机有 6 个状态，每个状态对读写操作有不同响应
2. **大量查找表**：~80 条命令码 + ~70 条响应码
3. **辅助模块**：Python 版本使用 `tpm_tis_registers.py` 和 `tpm_fifo_tis.py` 两个辅助模块
4. **Generator 模式**：Python 使用 generator 协程，C 需要改为直接函数调用
5. **TPM 命令/响应格式化**：需要 hex 格式化输出
6. **状态转换逻辑**：每个状态对 STS 寄存器的读写有复杂判断
7. **建议**：先实现核心状态机框架，再逐步完善各状态的处理逻辑
8. **🔴下层依赖阻塞**：tpm_tis_spi_c 和 tpm_tis_i2c_c 均不存在，无法实施。tpm-tis协议由这两个解码器提供，它们分别依赖spi_c(已存在)和i2c_c(已存在)，但自身尚未移植为C
<!-- Updated: 新增第8点，标注下层依赖阻塞。tpm_tis_spi(inputs=['spi'])和tpm_tis_i2c(inputs=['i2c'])
     的底层spi_c/i2c_c已存在，但这两个中间层解码器本身尚未有C实现 -->

---

## 4. tm1637 — TM1637 LED 驱动控制

### 4.1 Python 元数据

```python
id = "tm1637"
name = "TM1637"
longname = "LED drive control special circuit"
desc = "Titan Micro Electronics LED drive control special circuit..."
license = "gplv2+"
inputs = ["tmc"]
outputs = ["tm1637"]
tags = ['Embedded/industrial']
```

### 4.2 Annotations (16 个)

| 索引 | Python ID | C 宏 | C 标签列 |
|------|-----------|------|----------|
| 0 | `reserved` | `ANN_RESERVED` | `""`, `"Reserved"` |
| 1 | `write` | `ANN_WRITE` | `""`, `"Write"` |
| 2 | `read` | `ANN_READ` | `""`, `"Read"` |
| 3 | `data` | `ANN_DATA` | `""`, `"Data command"` |
| 4 | `display` | `ANN_DISPLAY` | `""`, `"Display command"` |
| 5 | `address` | `ANN_ADDRESS` | `""`, `"Address command"` |
| 6 | `auto` | `ANN_AUTO` | `""`, `"AutoAddr"` |
| 7 | `fixed` | `ANN_FIXED` | `""`, `"FixedAddr"` |
| 8 | `normal` | `ANN_NORMAL` | `""`, `"Normal"` |
| 9 | `test` | `ANN_TEST` | `""`, `"Test"` |
| 10 | `digit` | `ANN_DIGIT` | `""`, `"Digit"` |
| 11 | `contrast` | `ANN_CONTRAST` | `""`, `"Contrast"` |
| 12 | `off` | `ANN_OFF` | `""`, `"OFF"` |
| 13 | `on` | `ANN_ON` | `""`, `"ON"` |
| 14 | `warn` | `ANN_WARN` | `""`, `"Warnings"` |
| 15 | `display-info` | `ANN_DISPLAY_INFO` | `""`, `"Tubes"` |

### 4.3 Annotation Rows (3 行)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `bits` | Bits | (0-13) |
| `display` | Display | (15,) |
| `warnings` | Warnings | (14,) |

### 4.4 Options (1 个)

| ID | 描述 | 默认值 | 可选值 |
|----|------|--------|--------|
| `dpoint` | Decimal point | `"Dot"` | `"Dot"`, `"Colon"` |

### 4.5 解码逻辑分析

**输入格式**：`(cmd, databyte)`
- `cmd = "BITS"`：bit 列表 `[[bitval, ss, es], ...]`
- `cmd = "START"`：传输开始
- `cmd = "COMMAND"`：命令字节
- `cmd = "DATA"`：数据字节
- `cmd = "STOP"`：传输结束
<!-- Updated: BITS输入格式说明——Python版tmc解码器输出BITS时使用Python列表[[bitval, ss, es], ...]，
     但C版tmc_c(待实现)应使用BITS v2二进制格式(见c_decoder_utils.h)：
     data[0] = flags (have_mosi|have_miso)
     data[1] = bit_count
     data[2..2+count*17-1] = per bit: [value(1B)][ss(8B LE)][es(8B LE)]
     tm1637_c/tm1638_c的recv_proto在收到cmd="BITS"时需按此v2格式解析。
     注意：TMC是2/3线总线(非SPI/I2C)，BITS v2格式中只有miso段(无mosi/miso区分)，
     类似I2C的做法：flags=0x02, mosi_count=0, reserved=0x00, miso_count=N。 -->

**状态机**：
- `IDLE` → `REGISTER COMMAND` (on START)
- `REGISTER COMMAND` → `REGISTER DATA` (on COMMAND)
- `REGISTER DATA` → `IDLE` (on STOP)

**命令类型** (bits 6-7)：
- `0x40`：Data command → 解析 RW/ADDR/MODE bits
- `0x80`：Display command → 解析 SWITCH/PWM bits
- `0xC0`：Address command → 解析 DIGIT bits

**数据解码**：
- 7-segment fonts 查找表（~30 个字符映射）
- segments 名称：a, b, c, d, e, f, g, dp
- Decimal point 处理（Dot 或 Colon）

**Display 输出**：STOP 时输出完整显示字符串

### 4.6 C 实现方案

**文件**：`tm1637_c.c`

**私有结构体**：
```c
#define TM1637_MAX_DISPLAY 8
#define TM1637_MAX_BITS 8

typedef struct {
    int out_ann;
    // State
    int state;  // IDLE, REG_CMD, REG_DATA
    // Bit cache
    uint64_t bit_ss[TM1637_MAX_BITS];
    uint64_t bit_es[TM1637_MAX_BITS];
    int bit_val[TM1637_MAX_BITS];
    int num_bits;
    // Command state
    int is_write;
    int is_auto;
    int position;
    // Display buffer
    char display[TM1637_MAX_DISPLAY * 2];  // char + dp
    int display_len;
    // Options
    int dpoint_is_colon;
    // Transmission start
    uint64_t ssb;
    uint64_t ss;
    uint64_t es;
} tm1637_state;
```

**fonts 查找表**：
```c
static const struct { uint8_t segs; char ch; } font_table[] = {
    {0b0000000, ' '},
    {0b0111111, '0'},
    {0b0000110, '1'},
    // ... ~30 条
};
```

**recv_proto 回调**：
- `cmd = "BITS"`：缓存 bit 列表
- `cmd = "START"`：开始新传输
- `cmd = "COMMAND"`：处理命令字节
- `cmd = "DATA"`：处理数据字节
- `cmd = "STOP"`：输出显示信息

**关键 C 代码片段**：
```c
static void tm1637_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    tm1637_state *s = (tm1637_state *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;

    if (strcmp(cmd, "BITS") == 0) {
        // 缓存 bit 列表
        // <!-- Updated: 需按BITS v2格式解析，参见c_decoder_utils.h -->
    } else if (strcmp(cmd, "START") == 0) {
        s->ssb = start_sample;
        s->state = 1; // REG_CMD
    } else if (strcmp(cmd, "COMMAND") == 0) {
        tm1637_handle_command(di, s, data[0]);
        s->state = 2; // REG_DATA
    } else if (strcmp(cmd, "DATA") == 0) {
        tm1637_handle_data(di, s, data[0]);
    } else if (strcmp(cmd, "STOP") == 0) {
        tm1637_handle_info(di, s);
        s->state = 0; // IDLE
    }
}
```

### 4.7 难点与注意事项

1. **Bit 级标注**：需要使用 bit 缓存实现 `putd()` 和 `putr()` 辅助函数
2. **7-segment fonts**：~30 个字符映射
3. **contrasts 数组**：8 级 PWM 对比度
4. **Decimal point 选项**：Dot 或 Colon
5. **Address 自动递增**：auto 模式下 position 自增
6. **🔴下层依赖阻塞**：tmc_c 不存在，无法实施。tmc(inputs=['logic'], outputs=['tmc'])是TM1637/TM1638的底层协议解码器，需先完成 tmc → tmc_c 的移植
<!-- Updated: 新增第6点，标注下层依赖阻塞 -->

---

## 5. tm1638 — TM1638 LED 驱动控制

### 5.1 Python 元数据

```python
id = "tm1638"
name = "TM1638"
longname = "Special circuit for LED driver control"
desc = "Titan Micro Electronics LED drive control special circuit..."
license = "gplv2+"
inputs = ["tmc"]
outputs = ["tm1638"]
tags = ['Embedded/industrial']
```

### 5.2 Annotations (23 个)

| 索引 | Python ID | C 宏 | C 标签列 |
|------|-----------|------|----------|
| 0 | `reserved` | `ANN_RESERVED` | `""`, `"Reserved"` |
| 1 | `write` | `ANN_WRITE` | `""`, `"Write"` |
| 2 | `read` | `ANN_READ` | `""`, `"Read"` |
| 3 | `data` | `ANN_DATA` | `""`, `"Data command"` |
| 4 | `display` | `ANN_DISPLAY` | `""`, `"Display command"` |
| 5 | `address` | `ANN_ADDRESS` | `""`, `"Address command"` |
| 6 | `auto` | `ANN_AUTO` | `""`, `"AutoAddr"` |
| 7 | `fixed` | `ANN_FIXED` | `""`, `"FixedAddr"` |
| 8 | `normal` | `ANN_NORMAL` | `""`, `"Normal"` |
| 9 | `test` | `ANN_TEST` | `""`, `"Test"` |
| 10 | `position` | `ANN_POSITION` | `""`, `"Position"` |
| 11 | `digit` | `ANN_DIGIT` | `""`, `"Digit"` |
| 12 | `led` | `ANN_LED` | `""`, `"LED"` |
| 13 | `key` | `ANN_KEY` | `""`, `"Key"` |
| 14 | `red` | `ANN_RED` | `""`, `"Red LED"` |
| 15 | `green` | `ANN_GREEN` | `""`, `"Green LED"` |
| 16 | `contrast` | `ANN_CONTRAST` | `""`, `"Contrast"` |
| 17 | `off` | `ANN_OFF` | `""`, `"OFF"` |
| 18 | `on` | `ANN_ON` | `""`, `"ON"` |
| 19 | `warn` | `ANN_WARN` | `""`, `"Warnings"` |
| 20 | `display-info` | `ANN_DISPLAY_INFO` | `""`, `"Tubes"` |
| 21 | `leds-info` | `ANN_LEDS_INFO` | `""`, `"LEDs"` |
| 22 | `keys-info` | `ANN_KEYS_INFO` | `""`, `"Keys"` |

### 5.3 Annotation Rows (5 行)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `bits` | Bits | (0-18) |
| `display` | Display | (20,) |
| `leds` | LEDchain | (21,) |
| `keys` | Keyboard | (22,) |
| `warnings` | Warnings | (19,) |

### 5.4 解码逻辑分析

**与 TM1637 的主要区别**：

1. **Address bits**：4 bits (0-3) vs TM1637 的 3 bits (0-2)
2. **LED 支持**：偶数地址 = 数字管，奇数地址 = LED
3. **键盘扫描**：Read 模式下读取按键数据
4. **LED 颜色**：bit 0 = Red, bit 1 = Green
5. **按键映射**：`switches` 查找表（24 个按键）

**数据解码**：
- `handle_data_digit()`：同 TM1637 的 7-segment 解码
- `handle_data_led()`：Red/Green LED 解码
- `handle_data_keyboard()`：按键扫描解码

**Info 输出**：
- Display：数字管显示字符串
- LEDs：LED 颜色字符串
- Keys：按键名称字符串（通过 switches 查找表映射）

### 5.5 C 实现方案

**文件**：`tm1638_c.c`

**私有结构体**：
```c
#define TM1638_MAX_DISPLAY 8
#define TM1638_MAX_LEDS 8
#define TM1638_MAX_KEYS 24

typedef struct {
    int out_ann;
    int state;
    // Bit cache
    uint64_t bit_ss[8];
    uint64_t bit_es[8];
    int bit_val[8];
    // Command state
    int is_write;
    int is_auto;
    int position;
    // Display buffer
    char display[TM1638_MAX_DISPLAY * 2];
    int display_len;
    // LED buffer
    char leds[TM1638_MAX_LEDS * 2];
    int leds_len;
    // Key buffer
    char keys[TM1638_MAX_KEYS][8];
    int keys_len;
    // Transmission
    uint64_t ssb, ss, es;
} tm1638_state;
```

**switches 查找表**（24 条）：
```c
static const struct { const char *key_tag; const char *switch_name; } switch_table[] = {
    {"K3-KS1", "S1"}, {"K3-KS2", "S5"}, ...
};
```

**recv_proto 回调**：与 TM1637 类似，但增加 LED 和键盘处理

### 5.6 难点与注意事项

1. **与 TM1637 高度相似**：可复用大量代码
2. **LED 解码**：奇数地址为 LED，bit 0=Red, bit 1=Green
3. **键盘扫描**：Read 模式下的按键数据解码
4. **switches 查找表**：24 个按键映射
5. **Address 4 bits**：比 TM1637 多 1 bit
6. **🔴下层依赖阻塞**：同TM1637，tmc_c 不存在，无法实施
<!-- Updated: 新增第6点，标注下层依赖阻塞 -->

---

## 6. ltar_smartdevice_decode — LTAR SmartDevice 解码

### 6.1 Python 元数据

```python
id = 'ltar_smartdevice_decode'
name = 'LTAR SmartDevice Decode'
longname = 'LTAR SmartDevice Decode'
desc = 'A decoder for the LTAR SmartDevice protocol'
license = 'unknown'
inputs = ['ltar_smartdevice']
outputs = ['ltar_smartdevice_decode']
tags = ['Embedded/industrial']
```

### 6.2 Annotations (6 个)

| 索引 | Python ID | C 宏 | C 标签列 |
|------|-----------|------|----------|
| 0 | `frame-name` | `ANN_FRAME_NAME` | `""`, `"Frame Name"` |
| 1 | `frame-error` | `ANN_FRAME_ERROR` | `""`, `"Frame Error"` |
| 2 | `frame-bit-name` | `ANN_FRAME_BIT_NAME` | `""`, `"Frame Bit Name"` |
| 3 | `frame-bits-data` | `ANN_FRAME_BITS_DATA` | `""`, `"Frame Bits Data"` |
| 4 | `block-error` | `ANN_BLOCK_ERROR` | `""`, `"Block Errors"` |
| 5 | `block-data` | `ANN_BLOCK_DATA` | `""`, `"Block Data"` |

### 6.3 Annotation Rows (5 行)

| Row ID | 名称 | 包含的 annotation 索引 |
|--------|------|----------------------|
| `frame-names` | Frame name | (0,) |
| `frame-errors` | Frame errors | (1,) |
| `frame-bit-names` | Frame bit names | (2,) |
| `frame-bits-datas` | Frame bits data | (3,) |
| `block-errors` | Block errors | (4,) |

### 6.4 解码逻辑分析

**输入格式**：`(garbage, data)`
- `data` 是 `ltar_smartdevice` 输出的 block 数据
- 每个 frame = `[bit_data_list, byte_value]`
- `bit_data_list` = `[[ss, es, bit_val], ...]` × 10

**核心流程**：

1. **Block Type**：第一个 frame 的 byte_value 为 block type
2. **Block Checksum**：最后一个 frame 为 checksum
3. **Block Data**：中间的 frames 为数据

**查找表**：
- `btype`：8 种 block type 名称
- `weapmode`：2 种武器模式
- `shieldstatus`：3 种盾牌状态
- `huntingdirection`：2 种狩猎方向

**TAGGER-STATUS (0x02) 解码**：
- BData0：Player Number (3 bits) + Team Number (2 bits)
- BData1：Weapon Mode (2 bits) + Shield State (2 bits) + Hunting Direction (1 bit)
- BData2：Health Remaining
- BData3：Loaded Ammo
- BData4：Remaining Ammo Low Byte
- BData5：Remaining Ammo High Byte
- BData6：Shield Time
- BData7：Game Time Minutes
- BData8：Game Time Seconds

**Checksum 验证**：`0xFF - sum(all_byte_values)`，结果应为 0

### 6.5 C 实现方案

**文件**：`ltar_smartdevice_decode_c.c`

**私有结构体**：
```c
#define LTAR_SD_DEC_MAX_FRAMES 16

typedef struct {
    int out_ann;
} ltar_sd_dec_state;
```

**recv_proto 回调**：
- `cmd = "BLOCK"`：data 包含 block 数据
<!-- Updated: BLOCK的数据格式需要在ltar_smartdevice_c实现时定义。
     Python版ltar_smartdevice输出['BLOCK', currentblockdata]，其中currentblockdata是
     嵌套列表：[[bit_data_list, byte_value], ...]，bit_data_list = [[ss, es, bit_val], ...] × 10。
     C版需要将此嵌套结构扁平化为二进制格式。建议格式：
     data[0] = frame_count (uint8_t)
     每个frame: [byte_value(1B)][bit_count(1B)][per bit: value(1B)+ss(8B LE)+es(8B LE)]
     当前ltar_smartdevice_c和afsk_c均不存在，此格式为待定设计。 -->

**关键 C 代码片段**：
```c
static void ltar_sd_dec_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ltar_sd_dec_state *s = (ltar_sd_dec_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "BLOCK") == 0) {
        // 解析 block 数据
        // checkBlockLength
        // checkBlockCSum
        // putBlockType
        // putData for each frame
        // putCSum
    }
}
```

### 6.6 难点与注意事项

1. **相对简单**：本批次最简单的解码器
2. **输入格式**：需要确认 `ltar_smartdevice` C 解码器的 output 格式
3. **Checksum 验证**：简单的减法校验
4. **TAGGER-STATUS 解码**：bit 级别的字段提取
5. **查找表**：4 个小型查找表
6. **🔴下层依赖阻塞**：ltar_smartdevice_c 和 afsk_c 均不存在，无法实施。依赖链为 afsk(inputs=['logic']) → ltar_smartdevice(inputs=['afsk_bits']) → ltar_smartdevice_decode，需先完成整条链路的移植
<!-- Updated: 新增第6点，标注下层依赖阻塞。ltar_smartdevice_decode依赖ltar_smartdevice，
     而ltar_smartdevice又依赖afsk(输出afsk_bits协议)，两层下层解码器均无C实现 -->

---

## CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加：
```
sipi_c
pjon_c
tpm_fifo_tis_c
tm1637_c
tm1638_c
ltar_smartdevice_decode_c
```

> **⚠️ 注意：由于所有6个解码器均被下层依赖阻塞，CMakeLists.txt的修改应在下层C解码器完成后再执行。**
<!-- Updated: 添加阻塞提醒。需先完成以下下层C解码器才能添加本批次：
     lfast_c (sipi依赖)
     pjdl_c (pjon依赖)
     tpm_tis_spi_c 或 tpm_tis_i2c_c (tpm_fifo_tis依赖)
     tmc_c (tm1637/tm1638依赖)
     afsk_c + ltar_smartdevice_c (ltar_smartdevice_decode依赖) -->

## 审查变更记录

<!-- Updated: 以下为本轮审查新增的变更记录 -->

### 2026-05-24 审查

**审查范围**：检查spec中标注是否过时、BITS/SPI DATA格式一致性、下层依赖状态、已知bug

**关键发现**：

1. **🔴 全部阻塞**：所有6个解码器的下层依赖均无C实现，违反"C解码器只能依赖已有C实现的底层解码器"规则
2. **pjon strcmp 无bug**：`strcmp(cmd, "IDLE") == 0 || strcmp(cmd, "FRAME_DATA") == 0` 与Python版逻辑一致，正确
3. **BITS格式需更新**：tm1637/tm1638的BITS输入应使用BITS v2二进制格式(见c_decoder_utils.h)，非Python列表格式
4. **SPI DATA格式已确认**：17字节(1B flags + 8B mosi + 8B miso)，spi_c.c已实现(sipi不直接依赖SPI)
5. **参考实现有效**：lm75_c.c和ds1307_c.c均存在且有recv_proto实现，但协议差异较大
6. **C框架能力已确认**：SRD_OUTPUT_LOGIC、c_decoder_put_logic()、c_cond_wait_current()、c_decoder_get_initial_pin()均已实现
7. **数据传递格式待定**：sipi(lfast→DATA)、tpm_fifo_tis(tpm-tis→TRANSACTION)、ltar_smartdevice_decode(BLOCK)的recv_proto数据格式需在下层C解码器实现时定义
