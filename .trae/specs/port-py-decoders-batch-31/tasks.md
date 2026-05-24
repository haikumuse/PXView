# 任务分解 — Batch 31 Python→C 解码器移植

## 总体优先级排序

按复杂度从低到高实现，确保简单解码器先验证 recv_proto 基础框架：

1. **scs_c** (★☆☆☆) — 最简单，7 字节固定长度电报
2. **streletz_c** (★★☆☆) — 简单，变长包 + XOR 校验
3. **ufcs_c** (★★★☆) — 中等，SOP 检测 + 变长包 + CRC8 + 多种消息类型
4. **sbus_futaba_c** (★★★★) — 复杂，bit-level 解析 + 多种 UART 事件
5. **amulet_ascii_c** (★★★★★) — 最复杂，41 个命令处理器 + 动态状态机

---

## Task 1: scs_c — SCS 家庭自动化总线

### 1.1 创建文件 `libsigrokdecode/c_decoders/scs_c.c`

**预估代码量**：~120 行

**实现步骤**：

1. 定义 `scs_state` 结构体（`telegram_idx`, `crc`, `out_ann`）
2. 定义 ann labels（1 个：SCS）
3. 定义 ann rows（1 行）
4. 定义 inputs/outputs/tags
5. 实现 `scs_reset()` — 重置 `telegram_idx=0`
6. 实现 `scs_start()` — 注册 `SRD_OUTPUT_ANN`
7. 实现 `scs_decode()` — 空函数
8. 实现 `scs_recv_proto()` — 核心逻辑：
   - 仅处理 `"DATA"` cmd
   - 按 `telegram_idx` 分发：0→init, 1→addr, 2→??, 3→request, 4→??, 5→crc, 6→term
   - CRC 累积 XOR
9. 实现 `scs_destroy()` — g_free
10. 定义 `scs_c_decoder` struct
11. 实现 `srd_c_decoder_entry()` — 无 options，直接返回

**关键代码片段**：

```c
static void scs_recv_proto(struct srd_decoder_inst *di,
    uint64_t ss, uint64_t es,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    scs_state *s = (scs_state *)c_decoder_get_private(di);
    if (!s) return;
    if (strcmp(cmd, "DATA") != 0) return;
    if (data_len < 1) return;

    uint8_t val = data[0];

    switch (s->telegram_idx) {
    case 0:
        if (val == 0xa8)
            C_ANN_PUT(di, ss, es, s->out_ann, ANN_SCS, "init");
        break;
    case 1:
        s->crc = val;
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_SCS, "addr");
        break;
    case 2:
        s->crc ^= val;
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_SCS, "??");
        break;
    case 3:
        s->crc ^= val;
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_SCS, "request");
        break;
    case 4:
        s->crc ^= val;
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_SCS, "??");
        break;
    case 5: {
        const char *crc_text = (s->crc == val) ? "good crc" : "bad crc";
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_SCS, crc_text);
        break;
    }
    case 6:
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_SCS, "term");
        s->telegram_idx = -1;
        break;
    }

    s->telegram_idx++;
}
```

### 1.2 修改 CMakeLists.txt

在 `C_DECODERS` 列表中添加 `scs_c`。

### 1.3 验证

- 编译通过
- 加载 DLL 无错误
- UART 8N1 数据流中检测到 0xA8 开头的 7 字节电报

---

## Task 2: streletz_c — Streletz 安防系统串行协议

### 2.1 创建文件 `libsigrokdecode/c_decoders/streletz_c.c`

**预估代码量**：~250 行

**实现步骤**：

1. 定义常量 `PACKETSIZE_MAX=64`, `PACKETSIZE_MIN=4`
2. 定义 `streletz_state` 结构体
3. 定义 10 个 ann labels
4. 定义 4 个 ann rows（framing, data, warnings, packets）
5. 定义 2 个 options（`header_tx`, `header_rx`）
6. 定义 inputs/outputs/tags
7. 实现 `streletz_reset()` — 清空 accum, 重置 buf_pos
8. 实现 `streletz_start()` — 注册 output, 读取 options
9. 实现 `streletz_decode()` — 空函数
10. 实现 `streletz_handle_byte()` — 核心逻辑：
    - `buf_pos < 1`: 等待 header 匹配
    - `buf_pos == 2`: 解析 DataSize
    - `buf_pos == 3`: 解析 DataType（CMD/ANS）
    - `buf_pos == 4`: 标记 data_ss
    - `buf_pos == packet_size - 1`: 标记 data_es
    - `buf_pos == packet_size`: 校验和验证 + 包注释
11. 实现 `streletz_recv_proto()` — 仅处理 `"FRAME"` cmd
12. 实现 `streletz_destroy()`
13. 定义 `streletz_c_decoder` struct
14. 实现 `srd_c_decoder_entry()` — 初始化 options 默认值

**关键代码片段**：

```c
// 包完成时的校验和验证和注释
if (s->buf_pos == s->packet_size) {
    char cs_str[16];
    snprintf(cs_str, sizeof(cs_str), "CS: 0x%02X", byte_val);

    const char *rxtx_str = (rxtx == 1) ? "TX" : "RX";
    s->packet_es = es;

    if (s->checksum == 0) {
        C_ANN_PUT(di, ss, es, s->out_ann, ANN_CHECKSUM, cs_str, "CS");
        int pkt_ann = (rxtx == 1) ? ANN_PACKET_TX : ANN_PACKET_RX;
        // 构建包 hex 字符串
        char pkt_str[256];
        int pos = 0;
        for (int i = 0; i < s->accum_count && pos < 200; i++)
            pos += snprintf(pkt_str + pos, sizeof(pkt_str) - pos,
                           "%s%02X", (i > 0) ? " " : "", s->accum_bytes[i]);
        char long_str[300];
        snprintf(long_str, sizeof(long_str), "%s PACKET: %s", rxtx_str, pkt_str);
        C_ANN_PUT(di, s->packet_ss, s->packet_es, s->out_ann, pkt_ann,
                  long_str, rxtx_str, "P");

        // 数据区域注释
        if (s->packet_size > PACKETSIZE_MIN) {
            // ... 数据 hex 字符串
        }
    } else {
        char warn_str[300];
        snprintf(warn_str, sizeof(warn_str), "Err %s PACKET: ...", rxtx_str);
        C_ANN_PUT(di, s->packet_ss, s->packet_es, s->out_ann, ANN_WARN,
                  warn_str, "EP");
    }
    streletz_reset_state(s);
}
```

### 2.2 修改 CMakeLists.txt

添加 `streletz_c`。

### 2.3 验证

- 编译通过
- 检测 header 字节（0xD9/0x9D）
- XOR 校验和正确/错误分别标注

---

## Task 3: ufcs_c — UFCS 统一快充协议

### 3.1 创建文件 `libsigrokdecode/c_decoders/ufcs_c.c`

**预估代码量**：~600 行

**实现步骤**：

1. 定义常量 `UFCS_MAX_PKT=136`
2. 定义 CTRL_TYPES / DATA_TYPES 字符串查找表
3. 定义 12 个 ann labels
4. 定义 5 个 ann rows
5. 定义 1 个 option（`fulltext`）
6. 定义 `ufcs_state` 结构体
7. 实现 `ufcs_reset()` — 清空 datapkt, dataidx=0, plen=0
8. 实现 `ufcs_start()` — 注册 output, 读取 fulltext option
9. 实现 `ufcs_decode()` — 空函数
10. 实现 `ufcs_compute_crc8()` — 多项式 0x29
11. 实现 `ufcs_recv_proto()` — 核心入口：
    - 仅处理 `"DATA"` cmd
    - SOP 检测 (0xAA)
    - 字节累积
    - idx==3 时确定包长度
    - 包完成时调用 `ufcs_decode_pkt()`
12. 实现 `ufcs_decode_pkt()` — 包解析：
    - 解析 head[0..3]
    - 调用 `ufcs_puthead()` — power_role/消息类型注释
    - 调用 `ufcs_decode_data_msg()` — 数据消息解析
    - CRC 校验
    - fulltext 输出
13. 实现各数据消息解析函数：
    - `ufcs_get_source_cap()` — PDO 解析
    - `ufcs_get_request()` — 请求解析
    - `ufcs_get_src_info()` / `ufcs_get_snk_info()`
    - `ufcs_get_cable_info()` / `ufcs_get_device_info()`
    - `ufcs_get_error_info()` — 16 位错误标志
    - `ufcs_config_watchdog()` / `ufcs_refuse()`
14. 实现 `ufcs_destroy()`
15. 定义 `ufcs_c_decoder` struct
16. 实现 `srd_c_decoder_entry()` — fulltext option 默认值 + values

**关键代码片段**：

```c
// CRC8 计算
static uint8_t ufcs_compute_crc8(const uint8_t *data, int len)
{
    const uint8_t POLY = 0x29;
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ POLY;
            else
                crc <<= 1;
        }
    }
    return crc & 0xFF;
}

// 包头解析
static void ufcs_puthead(struct srd_decoder_inst *di, ufcs_state *s)
{
    int pwr_role = (s->datapkt[0] >> 5) & 7;
    int ann_type;
    const char *role;
    if (pwr_role == 1)      { ann_type = ANN_SRC; role = "SRC"; }
    else if (pwr_role == 2) { ann_type = ANN_SNK; role = "SNK"; }
    else if (pwr_role == 3) { ann_type = ANN_CABLE; role = "Cable"; }
    else                    { ann_type = ANN_RESERVED; role = "Reserved"; }

    int msg_type = s->datapkt[2];
    int data_len = ((s->datapkt[1] & 7) == 1) ? s->datapkt[3] : 0;

    const char *shortm;
    char shortm_buf[32];
    if (data_len == 0) {
        if (msg_type >= 0 && msg_type <= 15)
            shortm = ufcs_ctrl_type_names[msg_type];
        else
            shortm = "reserved cmd";
    } else {
        if (msg_type == 1) shortm = "OUTPUT CAP";
        else if (msg_type == 2) shortm = "REQUEST";
        // ... 其他数据消息类型
        else { snprintf(shortm_buf, sizeof(shortm_buf), "DAT%d", msg_type); shortm = shortm_buf; }
    }

    int rev = (s->datapkt[1] >> 3) & 31;
    int msg_id = (s->datapkt[0] >> 1) & 15;
    char longm[128];
    snprintf(longm, sizeof(longm), "(r%d) %s[%d]: %s", rev, role, msg_id, shortm);

    C_ANN_PUT(di, s->bytepos_ss[0], s->bytepos_es[2],
              s->out_ann, ann_type, longm, shortm);
}
```

### 3.2 修改 CMakeLists.txt

添加 `ufcs_c`。

### 3.3 验证

- 编译通过
- 检测 0xAA SOP
- 控制消息/数据消息正确区分
- CRC8 校验正确

---

## Task 4: sbus_futaba_c — Futaba SBUS 遥控协议

### 4.1 创建文件 `libsigrokdecode/c_decoders/sbus_futaba_c.c`

**预估代码量**：~400 行

**实现步骤**：

1. 定义 `sbus_state` 结构体（bits accumulator + 解析状态）
2. 定义 7 个 ann labels
3. 定义 3 个 ann rows
4. 定义 2 个 options（`prop_val_min`, `prop_val_max`）
5. 定义 inputs/outputs/tags
6. 实现 `sbus_reset()` — 清空 bits_accum, sent_fields=0
7. 实现 `sbus_start()` — 注册 outputs, 读取 options
8. 实现 `sbus_decode()` — 空函数
9. 实现 `bitpack_lsb()` — LSB-first bit 打包
10. 实现 `sbus_recv_proto()` — 核心入口：
    - `"DATA"` → 将字节拆为 8 bits 存入 accum
    - `"FRAME"` → 调用 `flush_accum_bits()`
    - `"IDLE"` → 调用 `handle_idle()`
    - `"BREAK"` → 调用 `handle_break()`
11. 实现 `sbus_flush_accum_bits()` — 字段解析：
    - Header (8 bits)
    - 16 × Proportional (11 bits each)
    - 2 × Digital (1 bit each)
    - 2 × Flags (1 bit each: framelost, failsafe)
    - 4 × MSB flags padding
    - Footer (8 bits)
    - 完成后检查多余 bits
12. 实现 `sbus_handle_idle()` — 刷新未处理 bits + 重置
13. 实现 `sbus_handle_break()` — BREAK 警告 + 重置
14. 实现 `sbus_destroy()`
15. 定义 `sbus_futaba_c_decoder` struct
16. 实现 `srd_c_decoder_entry()` — options 默认值

**关键代码片段**：

```c
// DATA 事件处理：字节拆 bits
if (strcmp(cmd, "DATA") == 0) {
    if (data_len < 1) return;
    uint8_t byte_val = data[0];
    // LSB-first 拆分为 8 个 bits
    for (int i = 0; i < 8; i++) {
        if (s->num_bits < 256) {
            s->bit_vals[s->num_bits] = (byte_val >> i) & 1;
            s->bit_ss[s->num_bits] = ss;
            s->bit_es[s->num_bits] = es;
            s->num_bits++;
        }
    }
}

// 比例通道解析
while (s->sent_fields < upto_proportional) {
    if (s->num_bits - s->consumed_bits < 11) return;
    uint32_t value = bitpack_lsb(&s->bit_vals[s->consumed_bits], 11);
    s->consumed_bits += 11;

    char val_str[16];
    snprintf(val_str, sizeof(val_str), "%d", value);
    C_ANN_PUT(di, s->bit_ss[s->consumed_bits - 11],
              s->bit_es[s->consumed_bits - 1],
              s->out_ann, ANN_PROPORTIONAL, val_str);

    if ((int)value < s->prop_val_min) {
        C_ANN_PUT(di, s->bit_ss[s->consumed_bits - 11],
                  s->bit_es[s->consumed_bits - 1],
                  s->out_ann, ANN_WARN,
                  "Low proportional value", "Low value", "Low");
    }
    if ((int)value > s->prop_val_max) {
        C_ANN_PUT(di, s->bit_ss[s->consumed_bits - 11],
                  s->bit_es[s->consumed_bits - 1],
                  s->out_ann, ANN_WARN,
                  "High proportional value", "High value", "High");
    }

    s->sent_fields++;
}
```

### 4.2 修改 CMakeLists.txt

添加 `sbus_futaba_c`。

### 4.3 验证

- 编译通过
- 25 字节 SBUS 消息正确解析为 16 通道 + 2 数字 + 标志
- IDLE/BREAK 事件正确触发重置
- 比例通道值范围检查

---

## Task 5: amulet_ascii_c — Amulet LCD ASCII 控制协议

### 5.1 创建文件 `libsigrokdecode/c_decoders/amulet_ascii_c.c`

**预估代码量**：~1200 行（最大解码器）

**实现步骤**：

1. 定义命令码查找表（41 条命令）
2. 定义 44 个 ann labels（41 命令 + BIT + FIELD + WARN）
3. 定义 4 个 ann rows
4. 定义 2 个 options（`ms_chan`, `sm_chan`）
5. 定义 `amulet_state` 结构体
6. 实现 `amulet_reset()` — state=0, cmdstate=0
7. 实现 `amulet_start()` — 注册 output, 读取 options
8. 实现 `amulet_decode()` — 空函数
9. 实现 `amulet_recv_proto()` — 核心入口：
    - 仅处理 `"DATA"` cmd
    - 命令中止检测（0xD0-0xF7 范围字节中断当前命令）
    - 新命令检测
    - 分发到命令处理器
10. 实现命令处理器（分阶段）：

**Phase 1 — 核心命令**（~400 行）：
- `amulet_handle_page()` — 页面跳转（5 字节：0xA0, 0x02, idx_hi, idx_lo, checksum）
- `amulet_handle_gbv()` — 读取字节变量
- `amulet_handle_sbv()` — 设置字节变量
- `amulet_handle_ack()` — ACK
- `amulet_handle_nack()` — NACK
- `amulet_handle_read()` — 通用读取逻辑
- `amulet_handle_set_common()` — 通用设置逻辑

**Phase 2 — 扩展命令**（~400 行）：
- `amulet_handle_gwv()` / `amulet_handle_swv()` — 字变量读写
- `amulet_handle_gsv()` / `amulet_handle_ssv()` — 字符串变量读写
- `amulet_handle_glv()` — 标签变量
- `amulet_handle_rpc()` / `amulet_handle_grpc()` — RPC

**Phase 3 — 绘图与应答**（~400 行）：
- `amulet_handle_line()` / `amulet_handle_rect()` / `amulet_handle_frect()`
- `amulet_handle_pixel()`
- 所有 reply 命令（GBVR/GWVR/GSVR/GLVR/GRPCR/SBVR/SWVR/SSVR/RPCR）
- 数组命令（GBVA/GWVA/SBVA/SWVA 及其 reply）
- 颜色变量（GCV/GCVR/SCV/SCVR）

11. 实现辅助函数：
    - `amulet_emit_cmd_byte()` — 命令字节注释
    - `amulet_emit_addr_bytes()` — 地址解析
    - `amulet_emit_cmd_end()` — 命令结束
    - `amulet_handle_string()` — 通用字符串处理
    - `amulet_decode_coords()` — 坐标解析
    - `amulet_cmd_ann_list()` — 命令注释文本生成
    - `amulet_is_high_byte_cmd()` — 判断是否为允许高字节的命令
12. 实现 `amulet_destroy()`
13. 定义 `amulet_ascii_c_decoder` struct
14. 实现 `srd_c_decoder_entry()` — options 默认值 + values

**关键代码片段**：

```c
// 命令码查找表
static const struct {
    uint8_t code;
    const char *shortname;
    const char *desc;
} amulet_cmds[] = {
    {0xA0, "PAGE", "Jump to page"},
    {0xD0, "GBV", "Get byte variable"},
    {0xD1, "GWV", "Get word variable"},
    {0xD2, "GSV", "Get string variable"},
    {0xD3, "GLV", "Get label variable"},
    {0xD4, "GRPC", "Get RPC buffer"},
    {0xD5, "SBV", "Set byte variable"},
    {0xD6, "SWV", "Set word variable"},
    {0xD7, "SSV", "Set string variable"},
    {0xD8, "RPC", "Invoke RPC"},
    {0xD9, "LINE", "Draw line"},
    {0xDA, "RECT", "Draw rectangle"},
    {0xDB, "FRECT", "Draw filled rectangle"},
    {0xDC, "PIXEL", "Draw pixel"},
    {0xDD, "GBVA", "Get byte variable array"},
    {0xDE, "GWVA", "Get word variable array"},
    {0xDF, "SBVA", "Set byte variable array"},
    {0xE0, "GBVR", "Get byte variable reply"},
    {0xE1, "GWVR", "Get word variable reply"},
    {0xE2, "GSVR", "Get string variable reply"},
    {0xE3, "GLVR", "Get label variable reply"},
    {0xE4, "GRPCR", "Get RPC buffer reply"},
    {0xE5, "SBVR", "Set byte variable reply"},
    {0xE6, "SWVR", "Set word variable reply"},
    {0xE7, "SSVR", "Set string variable reply"},
    {0xE8, "RPCR", "Invoke RPC reply"},
    {0xE9, "LINER", "Draw line reply"},
    {0xEA, "RECTR", "Draw rectangle reply"},
    {0xEB, "FRECTR", "Draw filled rectangle reply"},
    {0xEC, "PIXELR", "Draw pixel reply"},
    {0xED, "GBVAR", "Get byte variable array reply"},
    {0xEE, "GWVAR", "Get word variable array reply"},
    {0xEF, "SBVAR", "Set byte variable array reply"},
    {0xF0, "ACK", "Acknowledgment"},
    {0xF1, "NACK", "Negative acknowledgment"},
    {0xF2, "SWVA", "Set word variable array"},
    {0xF3, "SWVAR", "Set word variable array reply"},
    {0xF4, "GCV", "Get color variable"},
    {0xF5, "GCVR", "Get color variable reply"},
    {0xF6, "SCV", "Set color variable"},
    {0xF7, "SCVR", "Set color variable reply"},
};
#define NUM_AMULET_CMDS 41

// cmds_with_high_bytes
static const uint8_t amulet_high_byte_cmds[] = {
    0xA0, 0xD7, 0xE7, 0xE2, 0xE3
};

static int amulet_is_high_byte_cmd(uint8_t cmd)
{
    for (int i = 0; i < (int)(sizeof(amulet_high_byte_cmds)/sizeof(amulet_high_byte_cmds[0])); i++)
        if (amulet_high_byte_cmds[i] == cmd) return 1;
    return 0;
}

// 命令分发
static void amulet_handle_command(struct srd_decoder_inst *di, amulet_state *s, uint8_t pdata)
{
    switch (s->state) {
    case 0xA0: amulet_handle_page(di, s, pdata); break;
    case 0xD0: amulet_handle_read_cmd(di, s, pdata, ANN_GBV); break;
    case 0xD1: amulet_handle_read_cmd(di, s, pdata, ANN_GWV); break;
    case 0xD2: amulet_handle_read_cmd(di, s, pdata, ANN_GSV); break;
    case 0xD3: amulet_handle_read_cmd(di, s, pdata, ANN_GLV); break;
    case 0xD4: amulet_handle_grpc(di, s, pdata); break;
    case 0xD5: amulet_handle_sbv(di, s, pdata); break;
    case 0xD6: amulet_handle_swv(di, s, pdata); break;
    case 0xD7: amulet_handle_ssv(di, s, pdata); break;
    case 0xD8: amulet_handle_read_cmd(di, s, pdata, ANN_RPC); break;
    case 0xD9: amulet_handle_line(di, s, pdata); break;
    case 0xDA: amulet_handle_rect(di, s, pdata); break;
    case 0xDB: amulet_handle_frect(di, s, pdata); break;
    case 0xDC: amulet_handle_pixel(di, s, pdata); break;
    case 0xDD: amulet_handle_read_cmd(di, s, pdata, ANN_GBVA); break;
    case 0xDE: amulet_handle_read_cmd(di, s, pdata, ANN_GWVA); break;
    case 0xDF: amulet_handle_sbva(di, s, pdata); break;
    case 0xE0: amulet_handle_gbvr(di, s, pdata); break;
    case 0xE1: amulet_handle_gwvr(di, s, pdata); break;
    case 0xE2: amulet_handle_string(di, s, pdata, ANN_GSVR); break;
    case 0xE3: amulet_handle_string(di, s, pdata, ANN_GLVR); break;
    case 0xE4: amulet_handle_grpcr(di, s, pdata); break;
    case 0xE5: amulet_handle_sbvr(di, s, pdata); break;
    case 0xE6: amulet_handle_swvr(di, s, pdata); break;
    case 0xE7: amulet_handle_string(di, s, pdata, ANN_SSVR); break;
    case 0xE8: amulet_handle_read_cmd(di, s, pdata, ANN_RPCR); break;
    case 0xE9: amulet_handle_liner(di, s, pdata); break;
    case 0xEA: amulet_handle_rectr(di, s, pdata); break;
    case 0xEB: amulet_handle_frectr(di, s, pdata); break;
    case 0xEC: amulet_handle_pixelr(di, s, pdata); break;
    case 0xED: amulet_handle_gbvar(di, s, pdata); break;
    case 0xEE: amulet_handle_gwvar(di, s, pdata); break;
    case 0xEF: amulet_handle_sbvar(di, s, pdata); break;
    case 0xF0: amulet_handle_ack(di, s, pdata); break;
    case 0xF1: amulet_handle_nack(di, s, pdata); break;
    case 0xF2: amulet_handle_swva(di, s, pdata); break;
    case 0xF3: amulet_handle_swvar(di, s, pdata); break;
    case 0xF4: /* GCV - not implemented */ break;
    case 0xF5: /* GCVR - not implemented */ break;
    case 0xF6: /* SCV - not implemented */ break;
    case 0xF7: /* SCVR - not implemented */ break;
    default:
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_WARN,
                  "Unknown command", "Unknown");
        s->state = 0;
        break;
    }
}
```

### 5.2 修改 CMakeLists.txt

添加 `amulet_ascii_c`。

### 5.3 验证

- 编译通过
- 命令字节 0xA0/0xD0-0xF7 正确触发状态机
- PAGE 命令（0xA0, 0x02, idx_hi, idx_lo, checksum）正确解析
- ACK/NACK 立即响应
- 字符串命令 null 终止正确处理
- 命令中止逻辑正确

---

## Task 6: CMakeLists.txt 更新

在 `CMakeLists.txt` 的 `C_DECODERS` 列表中一次性添加全部 5 个解码器：

```cmake
scs_c
streletz_c
ufcs_c
sbus_futaba_c
amulet_ascii_c
```

---

## Task 7: 编译验证

对所有 5 个解码器执行增量编译：

```bash
build_incremental.cmd
```

验证：
- 编译无错误/警告
- 5 个 DLL 正确生成到 `build.dir/decoders/c_decoders/`
- PXView 启动后可在解码器列表中看到新解码器
