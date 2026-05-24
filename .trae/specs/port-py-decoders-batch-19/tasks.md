# 任务分解 — Batch 19: I2C 上层解码器移植

## 任务总览

| 任务ID | 解码器 | 优先级 | 预估工时 | 依赖 |
|--------|--------|--------|----------|------|
| T1 | ltc26x7_c | 高 | 2h | 无 |
| T2 | i2cfilter_c | 高 | 1.5h | 无 |
| T3 | i2cdemux_c | 高 | 1.5h | 无 |
| T4 | i2c_packet_c | 高 | 2.5h | T3 (参考动态输出) |
| T5 | edid_c | 高 | 5h | T1 (参考I2C上层模式) |
| T6 | CMakeLists.txt 更新 | 高 | 0.5h | T1-T5 |
| T7 | 编译验证 | 高 | 1h | T6 |

**建议执行顺序：** T1 → T2 → T3 → T4 → T5 → T6 → T7

---

## T1: ltc26x7_c — LTC26x7 DAC 解码器

### 子任务

| # | 子任务 | 详情 |
|---|--------|------|
| T1.1 | 创建文件 | `libsigrokdecode/c_decoders/ltc26x7_c.c` |
| T1.2 | 定义 annotation enum | ANN_SLAVE_ADDR=0, ANN_COMMAND=1, ANN_ADDRESS=2, ANN_DAC_A_VOLTAGE=3, ANN_DAC_B_VOLTAGE=4 |
| T1.3 | 定义 ann_labels | 5 组标签，首列 `""` |
| T1.4 | 定义 ann_rows | 3 行：addr_cmd(0,1,2), dac_a_voltages(3), dac_b_voltages(4) |
| T1.5 | 定义 options | chip (string, 默认 "ltc2607"), vref (double, 默认 1.5) |
| T1.6 | 定义 tags | `{"IC", "Analog/digital", NULL}` |
| T1.7 | 定义私有状态结构 | state, ss, es, data, dac_val, chip, vref, first_data_byte, data_ss |
| T1.8 | 实现 handle_slave_addr | 三进制地址转换，全局地址 0x73 特殊处理 |
| T1.9 | 实现 handle_cmd_addr | 命令高4位 + 地址低4位解析，查找命令/地址表 |
| T1.10 | 实现 handle_data | 2字节数据累积，chip 选择移位，电压计算 |
| T1.11 | 实现 recv_proto | 4 状态机：IDLE → GET_SLAVE_ADDR → GET_CMD_ADDR → WRITE_DATA |
| T1.12 | 实现 reset/start/decode/destroy | 标准模式 |
| T1.13 | 定义 srd_c_decoder 结构体 | `.id="ltc26x7_c"`, `.name="Ltc26x7(C)"` |
| T1.14 | 实现 srd_c_decoder_entry | 初始化 options 的 GVariant 默认值和值列表 |
| T1.15 | 实现 srd_c_decoder_api_version | 返回 SRD_C_DECODER_API_VERSION |

### 关键代码片段

**命令查找表：**
```c
static const char *ltc26x7_commands[][4] = {
    /* 0x00 */ { "Write Input Register", "Write In Reg", "Wr In Reg", "WIR" },
    /* 0x01 */ { "Update DAC", "Update", "U", "U" },
    /* 0x03 */ { "Write and Power Up DAC", "Write & Power Up", "W&PU", "W&PU" },
    /* 0x04 */ { "Power Down DAC", "Power Down", "PD", "PD" },
    /* 0x0F */ { "No Operation", "NoOp", "NO", "NO" },
};

static const int ltc26x7_cmd_keys[] = { 0x00, 0x01, 0x03, 0x04, 0x0F };
static const int ltc26x7_num_commands = 5;

static const char *ltc26x7_addresses[][2] = {
    /* 0x00 */ { "DAC A", "A" },
    /* 0x01 */ { "DAC B", "B" },
    /* 0x0F */ { "All DACs", "All" },
};
```

**recv_proto 状态机：**
```c
static void ltc26x7_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    ltc26x7_state *s = (ltc26x7_state *)c_decoder_get_private(di);
    if (!s) return;

    s->es = end_sample;
    uint8_t databyte = (data && data_len > 0) ? data[0] : 0;

    if (s->state == LTC_IDLE) {
        if (strcmp(cmd, "START") != 0) return;
        s->state = LTC_GET_SLAVE_ADDR;
    } else if (s->state == LTC_GET_SLAVE_ADDR) {
        if (strcmp(cmd, "ADDRESS WRITE") != 0) return;
        s->ss = start_sample;
        ltc26x7_handle_slave_addr(di, s, databyte);
        s->ss = (uint64_t)-1;
        s->state = LTC_GET_CMD_ADDR;
    } else if (s->state == LTC_GET_CMD_ADDR) {
        if (strcmp(cmd, "DATA WRITE") != 0) return;
        s->ss = start_sample;
        ltc26x7_handle_cmd_addr(di, s, databyte);
        s->ss = (uint64_t)-1;
        s->state = LTC_WRITE_DATA;
        s->first_data_byte = 1;
        s->data = 0;
    } else if (s->state == LTC_WRITE_DATA) {
        if (strcmp(cmd, "DATA WRITE") == 0) {
            if (s->first_data_byte) {
                s->data_ss = start_sample;
                s->data = databyte;
                s->first_data_byte = 0;
                return;
            }
            ltc26x7_handle_data(di, s, databyte);
            s->ss = (uint64_t)-1;
        } else if (strcmp(cmd, "STOP") == 0) {
            s->state = LTC_IDLE;
        } else {
            return;
        }
    }
}
```

---

## T2: i2cfilter_c — I2C 过滤器

### 子任务

| # | 子任务 | 详情 |
|---|--------|------|
| T2.1 | 创建文件 | `libsigrokdecode/c_decoders/i2cfilter_c.c` |
| T2.2 | 定义 annotation | 无 annotation（num_annotations=0） |
| T2.3 | 定义 options | address (int, 默认 0), direction (string, 默认 "both") |
| T2.4 | 定义 outputs | `{"i2c", NULL}` |
| T2.5 | 定义 tags | `{"Util", NULL}` |
| T2.6 | 定义私有状态结构 | out_python, curslave, curdirection, filter_address, filter_direction, packets[] |
| T2.7 | 实现 recv_proto | 缓存包 → 地址/方向过滤 → 转发或丢弃 |
| T2.8 | 实现 reset/start/decode/destroy | start 中注册 OUTPUT_PYTHON (proto_id="i2c") |
| T2.9 | 定义 srd_c_decoder 结构体 | `.id="i2cfilter_c"`, `.name="I2cfilter(C)"` |
| T2.10 | 实现 srd_c_decoder_entry | 初始化 options |

### 关键代码片段

**start() 中注册 Python 输出：**
```c
static void i2cfilter_start(struct srd_decoder_inst *di)
{
    i2cfilter_state *s = (i2cfilter_state *)c_decoder_get_private(di);
    s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "i2c");

    // 读取选项
    s->filter_address = (int)c_decoder_get_option_int(di, "address", 0);
    const char *dir = c_decoder_get_option_string(di, "direction", "both");
    if (strcmp(dir, "read") == 0) s->filter_direction = 1;
    else if (strcmp(dir, "write") == 0) s->filter_direction = 2;
    else s->filter_direction = 0;
}
```

**过滤逻辑：**
```c
// 在 STOP 或 START REPEAT 时
if (s->filter_address != 0 && s->curslave != s->filter_address) {
    s->num_packets = 0;
    return;
}
if (s->filter_direction != 0 && s->curdirection != s->filter_direction) {
    s->num_packets = 0;
    return;
}
// 通过过滤，转发所有缓存包
for (int i = 0; i < s->num_packets; i++) {
    c_decoder_put_python(di, s->packets[i].ss, s->packets[i].es,
        s->out_python, s->packets[i].cmd,
        s->packets[i].data, s->packets[i].data_len);
}
s->num_packets = 0;
```

---

## T3: i2cdemux_c — I2C 解复用器

### 子任务

| # | 子任务 | 详情 |
|---|--------|------|
| T3.1 | 创建文件 | `libsigrokdecode/c_decoders/i2cdemux_c.c` |
| T3.2 | 定义 annotation | 无 annotation（num_annotations=0） |
| T3.3 | 定义 outputs | `NULL`（动态创建） |
| T3.4 | 定义 tags | `{"Util", NULL}` |
| T3.5 | 定义私有状态结构 | slaves[], out_python[], stream, streamcount, packets[] |
| T3.6 | 实现 recv_proto | 缓存包 → 查找/创建输出流 → STOP 时转发 |
| T3.7 | 实现 reset/start/destroy | start 中初始化 out_python 数组 |
| T3.8 | 定义 srd_c_decoder 结构体 | `.id="i2cdemux_c"`, `.name="I2cdemux(C)"` |
| T3.9 | 实现 srd_c_decoder_entry | 无 options |

### 关键代码片段

**动态输出流注册：**
```c
if (strcmp(cmd, "ADDRESS READ") == 0 || strcmp(cmd, "ADDRESS WRITE") == 0) {
    uint8_t addr = (data && data_len > 0) ? data[0] : 0;
    int found = -1;
    for (int j = 0; j < s->num_slaves; j++) {
        if (s->slaves[j] == addr) { found = j; break; }
    }
    if (found < 0 && s->num_slaves < MAX_SLAVES) {
        char proto_id[32];
        snprintf(proto_id, sizeof(proto_id), "i2c-0x%02x", addr);
        s->slaves[s->num_slaves] = addr;
        s->out_python[s->num_slaves] = c_decoder_register_output(
            di, SRD_OUTPUT_PYTHON, proto_id);
        found = s->num_slaves;
        s->num_slaves++;
    }
    s->stream = found;
}
```

---

## T4: i2c_packet_c — I2C 数据包构建器

### 子任务

| # | 子任务 | 详情 |
|---|--------|------|
| T4.1 | 创建文件 | `libsigrokdecode/c_decoders/i2c_packet_c.c` |
| T4.2 | 定义 annotation | ANN_DATA=0, 1 个 annotation |
| T4.3 | 定义 ann_labels | `{"", "data", "Data"}` |
| T4.4 | 定义 ann_rows | 1 行：packet(0) |
| T4.5 | 定义 options | format (string, 默认 "hex", 值: ascii/dec/hex/oct/bin) |
| T4.6 | 定义 tags | `{"Embedded/industrial", NULL}` |
| T4.7 | 定义私有状态结构 | out_ann, out_py, packet_data[], packet_str, address, read_sign, format |
| T4.8 | 实现 format_data_value | 根据 format 选项格式化单字节 |
| T4.9 | 实现 data_array_to_str | 将字节数组格式化为字符串 |
| T4.10 | 实现 format_packet | 格式化为 "0xNN RD/WR: XX XX XX" |
| T4.11 | 实现 handle_packet | 输出 Python 数据 + annotation，处理 START REPEAT 合并 |
| T4.12 | 实现 recv_proto | 处理 DATA/START/ADDRESS/ACK/STOP 事件 |
| T4.13 | 实现 reset/start/decode/destroy | start 中注册 OUTPUT_ANN + OUTPUT_PYTHON |
| T4.14 | 定义 srd_c_decoder 结构体 | `.id="i2c_packet_c"`, `.name="I2c_packet(C)"` |
| T4.15 | 实现 srd_c_decoder_entry | 初始化 format option |

### 关键代码片段

**Python 输出数据打包：**
```c
// PACKET READ/WRITE 输出
static void i2c_packet_put_python_packet(struct srd_decoder_inst *di,
    i2c_packet_state *s, uint64_t ss, uint64_t es)
{
    // 构造 Python 输出数据
    // 格式: cmd="PACKET READ" 或 "PACKET WRITE"
    // data: [address, data_byte_0, data_byte_1, ...]
    int total_len = 1 + s->packet_data_len;
    unsigned char py_data[257];
    py_data[0] = s->address;
    memcpy(py_data + 1, s->packet_data, s->packet_data_len);

    const char *ptype = s->read_sign ? "PACKET READ" : "PACKET WRITE";
    c_decoder_put_python(di, ss, es, s->out_py, ptype, py_data, total_len);

    // TRANSACTION END
    if (!start_repeat) {
        c_decoder_put_python(di, es, es, s->out_py, "TRANSACTION END", NULL, 0);
    }
}
```

**数据格式化：**
```c
static void i2c_packet_format_data(i2c_packet_state *s,
    const uint8_t *data, int len, char *out, int out_size)
{
    int pos = 0;
    for (int i = 0; i < len && pos < out_size - 20; i++) {
        if (i > 0) out[pos++] = ' ';
        switch (s->format) {
        case 0: // ascii
            if (data[i] >= 32 && data[i] <= 126) {
                out[pos++] = data[i];
            } else {
                pos += snprintf(out + pos, out_size - pos, "[%02X]", data[i]);
            }
            break;
        case 1: // dec
            pos += snprintf(out + pos, out_size - pos, "%d", data[i]);
            break;
        case 2: // hex
            pos += snprintf(out + pos, out_size - pos, "%02X", data[i]);
            break;
        case 3: // oct
            pos += snprintf(out + pos, out_size - pos, "%03o", data[i]);
            break;
        case 4: // bin
            for (int b = 7; b >= 0; b--)
                out[pos++] = (data[i] & (1 << b)) ? '1' : '0';
            break;
        }
    }
    out[pos] = '\0';
}
```

---

## T5: edid_c — EDID 显示器识别数据

### 子任务

| # | 子任务 | 详情 |
|---|--------|------|
| T5.1 | 创建文件 | `libsigrokdecode/c_decoders/edid_c.c` |
| T5.2 | 定义 annotation | ANN_FIELDS=0, ANN_SECTIONS=1 |
| T5.3 | 定义 ann_labels | 2 组标签 |
| T5.4 | 定义 ann_rows | 2 行：sections(1), fields(0) |
| T5.5 | 定义 tags | `{"Display", "Memory", "PC", NULL}` |
| T5.6 | 定义常量 | EDID_HEADER, OFF_VENDOR 等 |
| T5.7 | 定义 est_modes 数组 | 17 种预设时序模式字符串 |
| T5.8 | 定义 xy_ratio 数组 | 4 种宽高比 |
| T5.9 | 定义私有状态结构 | state, cnt, cache[128], sn[128][2], offset, extension, ext_cache[4][128], ext_sn[4][128][2], have_preferred_timing |
| T5.10 | 实现 ann_field | 跨偏移量范围的 annotation 输出 |
| T5.11 | 实现 convert_color | 10-bit 色度值转浮点 |
| T5.12 | 实现 decode_vid | PNPID 解码 (3 字符) |
| T5.13 | 实现 decode_pid | 产品 ID 解码 |
| T5.14 | 实现 decode_serial | 序列号解码 |
| T5.15 | 实现 decode_mfrdate | 制造日期解码 |
| T5.16 | 实现 decode_basicdisplay | 基本显示参数 (视频输入/尺寸/gamma/DPMS/特征) |
| T5.17 | 实现 decode_chromaticity | 色度坐标 (红绿蓝白 4 组) |
| T5.18 | 实现 decode_est_timing | 预设时序模式 (17 位 bitmap) |
| T5.19 | 实现 decode_std_timing | 标准时序 (8 个 2 字节条目) |
| T5.20 | 实现 decode_detailed_timing | 详细时序描述符 (18 字节) |
| T5.21 | 实现 decode_descriptor | 监视器描述符 (tag 0xFF/0xFE/0xFC/0xFD/0xFB/0xFA) |
| T5.22 | 实现 decode_descriptors | 4 个连续 18 字节描述符块 |
| T5.23 | 实现 decode_data_block | CEA 扩展数据块 (tag 0-7) |
| T5.24 | 实现 decode_data_block_collection | 数据块集合解析 |
| T5.25 | 实现 recv_proto | 5 状态机：IDLE/OFFSET/HEADER/EDID/EXTENSIONS |
| T5.26 | 实现 reset/start/decode/destroy | 标准 + cache/sn 初始化 |
| T5.27 | 定义 srd_c_decoder 结构体 | `.id="edid_c"`, `.name="Edid(C)"` |
| T5.28 | 实现 srd_c_decoder_entry | 无 options |
| T5.29 | PNPID 处理 | 内嵌常用 PNP ID 或简化输出 |

### 关键代码片段

**EDID 头部检测：**
```c
static const uint8_t EDID_HEADER[] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

// 在 DATA READ 处理中
if (s->state == EDID_STATE_HEADER || s->state == EDID_STATE_IDLE) {
    if (s->cnt >= OFF_VENDOR) {
        int match = 1;
        for (int i = 0; i < 8; i++) {
            if (s->cache[s->cnt - 8 + i] != EDID_HEADER[i]) {
                match = 0;
                break;
            }
        }
        if (match) {
            // 丢弃头部之前的垃圾数据
            // 重置 cnt=8, state=EDID_STATE_EDID
            C_ANN_PUT(di, s->sn[0][0], s->es, s->out_ann, ANN_SECTIONS, "Header");
            C_ANN_PUT(di, s->sn[0][0], s->es, s->out_ann, ANN_FIELDS, "Header pattern");
        }
    }
}
```

**色度坐标转换：**
```c
static double edid_convert_color(uint16_t value)
{
    double outval = 0.0;
    for (int i = 0; i < 10; i++) {
        if (value & 0x01)
            outval += pow(2.0, -(10 - i));
        value >>= 1;
    }
    return outval;
}
```

**详细时序描述符：**
```c
static void edid_decode_detailed_timing(struct srd_decoder_inst *di,
    edid_state *s, const uint8_t *cache, int offset, int is_first)
{
    const char *section = (is_first && s->have_preferred_timing)
                          ? "Preferred timing descriptor"
                          : "Detailed timing descriptor";
    C_ANN_PUT(di, s->sn[offset][0], s->sn[offset + 17][1],
              s->out_ann, ANN_SECTIONS, section);

    double pixclock = (double)((cache[1] << 8) + cache[0]) / 100.0;
    char buf[128];
    snprintf(buf, sizeof(buf), "Pixel clock: %.2f MHz", pixclock);
    edid_ann_field(di, s, offset, offset + 1, buf);

    int horiz_active = ((cache[4] & 0xf0) << 4) + cache[2];
    int horiz_blank = ((cache[4] & 0x0f) << 8) + cache[3];
    snprintf(buf, sizeof(buf), "Horizontal active: %d, blanking: %d",
             horiz_active, horiz_blank);
    edid_ann_field(di, s, offset + 2, offset + 4, buf);

    // ... 继续解析 vert_active, sync, size, border, features
}
```

---

## T6: CMakeLists.txt 更新

### 子任务

| # | 子任务 | 详情 |
|---|--------|------|
| T6.1 | 定位 C_DECODERS 列表 | 在 CMakeLists.txt 中找到 `set(C_DECODERS ...)` |
| T6.2 | 添加 5 个解码器 | 在列表末尾添加 `edid_c i2c_packet_c i2cdemux_c i2cfilter_c ltc26x7_c` |
| T6.3 | 验证格式 | 确保与现有条目格式一致 |

---

## T7: 编译验证

### 子任务

| # | 子任务 | 详情 |
|---|--------|------|
| T7.1 | 增量编译 | 运行 `build_incremental.cmd` |
| T7.2 | 检查编译错误 | 修复所有编译警告和错误 |
| T7.3 | 验证 DLL 生成 | 确认 `build.dir/decoders/c_decoders/` 下生成 5 个新 DLL |
| T7.4 | 验证 PXView 启动 | 确认 PXView.exe 正常启动，新解码器出现在列表中 |

---

## 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| `c_decoder_register_output()` 在 `recv_proto()` 中调用不安全 | i2cdemux 无法动态注册输出流 | 改为在 start() 中预注册多个输出流，或验证 API 是否支持 |
| EDID PNPID 查找性能 | 大文件加载影响启动速度 | 使用编译时静态数组，或延迟加载 |
| i2c_packet Python 输出格式不兼容 | 上层解码器无法正确接收数据 | 严格匹配 Python 版输出格式，编写测试用例验证 |
| 缓冲区溢出 | 固定大小数组越界 | 使用足够大的缓冲区 + 边界检查 |
| EDID 扩展块数量不确定 | ext_cache 大小不足 | 限制最大扩展块数为 4 (与 Python 版一致) |
