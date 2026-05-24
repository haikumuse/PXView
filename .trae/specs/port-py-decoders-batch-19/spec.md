# Python → C 解码器移植规格书 — Batch 19

## 概述

本规格书涵盖 5 个 I2C 上层协议解码器的 Python → C 移植。所有解码器均以 `inputs=['i2c']` 作为输入，通过 `recv_proto()` 回调接收 I2C 协议数据，而非直接使用 `decode()` 处理原始采样数据。

**目标解码器：**
| # | Python id | C id | 复杂度 | 说明 |
|---|-----------|------|--------|------|
| 1 | `edid` | `edid_c` | ★★★★★ | EDID 显示器识别数据解析，含扩展块、色度、时序描述符 |
| 2 | `i2c_packet` | `i2c_packet_c` | ★★★ | I2C 数据包构建器，将字节流组装为读写包 |
| 3 | `i2cdemux` | `i2cdemux_c` | ★★ | I2C 解复用器，按从设备地址分发数据流 |
| 4 | `i2cfilter` | `i2cfilter_c` | ★★ | I2C 过滤器，按地址/方向过滤数据流 |
| 5 | `ltc26x7` | `ltc26x7_c` | ★★★ | LTC26x7 DAC 解码器，含命令/地址/电压输出 |

---

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |
| i2c_c.c | 底层协议输出范本 | START/STOP条件检测、c_decoder_put_python()输出I2C协议数据 |


## 1. EDID (`edid` → `edid_c`)

### 1.1 Python 元数据

```python
id = 'edid'
name = 'EDID'
longname = 'Extended Display Identification Data'
desc = 'Data structure describing display device capabilities.'
license = 'gplv3+'
inputs = ['i2c']
outputs = []
tags = ['Display', 'Memory', 'PC']
annotations = (
    ('fields', 'EDID structure fields'),      # ANN_FIELDS = 0
    ('sections', 'EDID structure sections'),   # ANN_SECTIONS = 1
)
annotation_rows = (
    ('sections', 'Sections', (1,)),
    ('fields', 'Fields', (0,)),
)
```

**无 options、无 channels、无 optional_channels、无 binary。**

### 1.2 解码逻辑分析

EDID 解码器是本批次最复杂的解码器，核心逻辑如下：

**状态机：**
- `None` / `'header'` — 等待 EDID 头部 (8 字节 `0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00`)
- `'offset'` — 接收写入偏移量
- `'edid'` — 解析 EDID 基础块 (128 字节)
- `'extensions'` — 解析扩展块 (每块 128 字节)

**I2C 交互流程：**
1. `ADDRESS WRITE` + 地址 `0x50` → 进入 `offset` 状态
2. `DATA WRITE` → 记录偏移量，计算 extension 和 cnt
3. `ADDRESS READ` + 地址 `0x50` → 进入 `header` 或 `extensions` 状态
4. `DATA READ` → 逐字节接收数据，根据 cnt 触发各段解析

**数据缓存：**
- `self.cache[]` — 基础块数据缓存 (最多 128 字节)
- `self.sn[]` — 每字节的 [ss, es] 采样号
- `self.ext_cache[]` — 扩展块数据缓存列表
- `self.ext_sn[]` — 扩展块采样号列表

**关键偏移常量：**
```c
#define OFF_VENDOR   8
#define OFF_VERSION  18
#define OFF_BASIC    20
#define OFF_CHROM    25
#define OFF_EST_TIMING 35
#define OFF_STD_TIMING 38
#define OFF_DET_TIMING 54
#define OFF_NUM_EXT  126
#define OFF_CHECKSUM 127
```

**解析子函数：**
- `decode_vid(offset)` — 厂商 ID (PNPID) 解码，需查找 `pnpids.txt`
- `decode_pid(offset)` — 产品 ID 解码
- `decode_serial(offset)` — 序列号解码
- `decode_mfrdate(offset)` — 制造日期解码
- `decode_basicdisplay(offset)` — 基本显示参数 (视频输入、尺寸、gamma、DPMS)
- `decode_chromaticity(offset)` — 色度坐标 (10-bit 转浮点)
- `decode_est_timing(offset)` — 预设时序模式 (17 种)
- `decode_std_timing(offset)` — 标准时序模式 (8 个 2 字节条目)
- `decode_detailed_timing(cache, sn, offset, is_first)` — 详细时序描述符 (18 字节)
- `decode_descriptor(cache, offset)` — 监视器描述符 (序列号/名称/范围限制等)
- `decode_descriptors(offset)` — 4 个连续 18 字节描述符块
- `decode_data_block(tag, cache, sn)` — CEA 扩展数据块
- `decode_data_block_collection(cache, sn)` — 数据块集合

**PNPID 查找：** Python 版使用 `pnpids.txt` 文件（与 `pd.py` 同目录），C 版需将此数据内嵌或提供文件查找机制。

### 1.3 C 实现方案

**文件：** `edid_c.c`

**私有状态结构：**
```c
typedef struct {
    int out_ann;
    int state;              // EDID_IDLE, EDID_OFFSET, EDID_HEADER, EDID_EDID, EDID_EXTENSIONS
    int cnt;                // 当前字节计数
    uint8_t cache[128];     // 基础块数据缓存
    uint64_t sn[128][2];    // 每字节的 [ss, es]
    int offset;             // 随机读偏移
    int extension;          // 当前扩展块号
    uint8_t ext_cache[4][128];  // 扩展块数据缓存 (最多4个扩展)
    uint64_t ext_sn[4][128][2]; // 扩展块采样号
    int have_preferred_timing;
    uint64_t ss;            // 当前包起始采样
    uint64_t es;            // 当前包结束采样
} edid_state;

enum {
    EDID_STATE_IDLE,
    EDID_STATE_OFFSET,
    EDID_STATE_HEADER,
    EDID_STATE_EDID,
    EDID_STATE_EXTENSIONS,
};
```

**Annotation 定义：**
```c
enum {
    ANN_FIELDS = 0,
    ANN_SECTIONS = 1,
    NUM_ANN,
};

static const char *edid_ann_labels[][3] = {
    {"", "fields", "EDID structure fields"},
    {"", "sections", "EDID structure sections"},
};

static const int edid_row_sections_classes[] = {ANN_SECTIONS, -1};
static const int edid_row_fields_classes[] = {ANN_FIELDS, -1};

static const struct srd_c_ann_row edid_ann_rows[] = {
    {"sections", "Sections", edid_row_sections_classes, 1},
    {"fields", "Fields", edid_row_fields_classes, 1},
};
```

**recv_proto 核心逻辑：**
```c
static void edid_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    edid_state *s = (edid_state *)c_decoder_get_private(di);
    if (!s) return;

    s->ss = start_sample;
    s->es = end_sample;
    uint8_t databyte = (data && data_len > 0) ? data[0] : 0;

    // ADDRESS WRITE + 0x50 → offset 状态
    if (strcmp(cmd, "ADDRESS WRITE") == 0 && databyte == 0x50) {
        s->state = EDID_STATE_OFFSET;
        s->ss = start_sample;
        return;
    }

    // ADDRESS READ + 0x50 → header 或 extensions
    if (strcmp(cmd, "ADDRESS READ") == 0 && databyte == 0x50) {
        if (s->extension > 0) {
            s->state = EDID_STATE_EXTENSIONS;
            // 输出扩展段标注
        } else {
            s->state = EDID_STATE_HEADER;
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_SECTIONS, "EDID");
        }
        return;
    }

    // DATA WRITE + offset 状态
    if (strcmp(cmd, "DATA WRITE") == 0 && s->state == EDID_STATE_OFFSET) {
        s->offset = databyte;
        s->extension = s->offset / 128;
        s->cnt = s->offset % 128;
        // 输出偏移标注
        return;
    }

    // 只处理 DATA READ
    if (strcmp(cmd, "DATA READ") != 0) return;

    s->cnt++;
    // 存入 cache 和 sn
    if (s->extension > 0) {
        int ext = s->extension - 1;
        if (ext < 4) {
            s->ext_sn[ext][s->cnt - 1][0] = start_sample;
            s->ext_sn[ext][s->cnt - 1][1] = end_sample;
            s->ext_cache[ext][s->cnt - 1] = databyte;
        }
    } else {
        s->sn[s->cnt - 1][0] = start_sample;
        s->sn[s->cnt - 1][1] = end_sample;
        s->cache[s->cnt - 1] = databyte;
    }

    // 根据 cnt 和 state 触发各段解析
    // ... (详见完整实现)
}
```

**PNPID 处理策略：** C 版将 `pnpids.txt` 编译为静态哈希表或排序数组，使用二分查找。也可简化为仅输出 PNPID 代码而不查找厂商名称。

**复杂度评估：** 约 800-1000 行 C 代码。`decode_detailed_timing` 和 `decode_chromaticity` 是最复杂的子函数。

---

## 2. I2C Packet (`i2c_packet` → `i2c_packet_c`)

### 2.1 Python 元数据

```python
id = 'i2c_packet'
name = 'I²C packet'
longname = 'I²C packet builder'
desc = 'Concatenate I²C data to packets'
license = 'mit'
inputs = ['i2c']
outputs = []  <!-- Updated: Python版outputs=[]但注册了SRD_OUTPUT_PYTHON(无proto_id)，C版需在start()中注册SRD_OUTPUT_PYTHON输出 -->
tags = ['Embedded/industrial']
options = (
    {'id': 'format', 'desc': 'Data format', 'default': 'hex',
        'values': ('ascii', 'dec', 'hex', 'oct', 'bin')},
)
annotations = (
    ('data', 'Data'),   # Ann.DATA = 0
)
annotation_rows = (
    ('packet', 'Packet', (0,)),
)
```

### 2.2 解码逻辑分析

I2C Packet 解码器将 I2C 字节流组装为逻辑数据包，并输出 Python 协议数据供上层解码器使用。

**状态机（隐式）：**
- 通过 `packet_data` deque 缓存数据字节
- 通过 `address` 和 `read_sign` 跟踪当前从设备地址和读写方向
- 支持 START REPEAT 合并前后两段数据

**I2C 事件处理：**
1. `DATA READ` / `DATA WRITE` → 追加到 `packet_data`，更新 `packet_es`
2. `START` / `START REPEAT` → 如果有缓存数据，先输出当前包；记录新包起始
3. `ADDRESS READ` / `ADDRESS WRITE` → 记录地址和方向
4. `*ACK` → 更新 `packet_es`
5. `STOP` → 输出完整包

**Python 输出格式：**
```python
# PACKET READ / PACKET WRITE
(ptype, (address, (data_byte1, data_byte2, ...)))
# TRANSACTION END
('TRANSACTION END', None)
```

**Annotation 格式：**
```
0x50 RD: 48 65 6C 6C 6F
0x50 WR: 00 [SR] 0x50 RD: 48 65 6C 6C 6F
```

**数据格式化选项：** `ascii` / `dec` / `hex` / `oct` / `bin`

### 2.3 C 实现方案

**文件：** `i2c_packet_c.c`

**私有状态结构：**
```c
#define MAX_PACKET_DATA 4096

typedef struct {
    int out_ann;
    int out_py;  <!-- Updated: 需在start()中通过c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "i2c_packet")注册 -->
    uint8_t packet_data[MAX_PACKET_DATA];
    int packet_data_len;
    char packet_str[8192];      // 累积的包字符串
    char packet_str_short[4096];
    uint64_t packet_ss;
    uint64_t packet_part_ss;
    uint64_t packet_es;
    int read_sign;              // 0=write, 1=read
    uint8_t address;
    int format;                 // 0=ascii, 1=dec, 2=hex, 3=oct, 4=bin
} i2c_packet_state;
```

**Annotation 定义：**
```c
enum {
    ANN_DATA = 0,
    NUM_ANN,
};

static const char *i2c_packet_ann_labels[][3] = {
    {"", "data", "Data"},
};

static const int i2c_packet_row_packet_classes[] = {ANN_DATA, -1};

static const struct srd_c_ann_row i2c_packet_ann_rows[] = {
    {"packet", "Packet", i2c_packet_row_packet_classes, 1},
};
```

**Options：**
```c
static struct srd_decoder_option i2c_packet_options[] = {
    {"format", "dec_i2c_packet_opt_format", "Data format", NULL, NULL},
};
```

**关键函数 — handle_packet：**
```c
static void i2c_packet_handle_packet(struct srd_decoder_inst *di,
    i2c_packet_state *s, int start_repeat)
{
    if (s->packet_data_len == 0) {
        if (!start_repeat) i2c_packet_reset_state(s);
        return;
    }

    // 格式化包字符串
    char cur_str[8192], cur_short[4096];
    i2c_packet_format_current(di, s, cur_str, sizeof(cur_str),
                              cur_short, sizeof(cur_short));

    // 输出 Python 协议数据
    const char *ptype = s->read_sign ? "PACKET READ" : "PACKET WRITE";
    c_decoder_put_python(di, s->packet_part_ss, s->packet_es,
                         s->out_py, ptype, ...);

    if (start_repeat) {
        // 合并到 packet_str
        strcat(s->packet_str, " [SR] ");
        strcat(s->packet_str, cur_str);
        s->packet_data_len = 0;
    } else {
        // 最终输出 annotation
        char final_str[16384];
        if (s->packet_str[0]) {
            snprintf(final_str, sizeof(final_str), "%s [SR] %s",
                     s->packet_str, cur_str);
        } else {
            strncpy(final_str, cur_str, sizeof(final_str));
        }
        C_ANN_PUT(di, s->packet_ss, s->packet_es, s->out_ann, ANN_DATA, final_str);
        i2c_packet_reset_state(s);
    }
}
```

**注意：** `c_decoder_put_python()` 的 API 签名为 `(di, ss, es, output_id, cmd, data, data_len)`，需要将 packet 数据打包为 `cmd + data` 格式。对于 `PACKET READ/WRITE`，cmd 为类型字符串，data 包含地址和数据字节；对于 `TRANSACTION END`，cmd 为字符串，data 为空。

<!-- Updated: c_decoder_put_python()输出SRD_OUTPUT_PYTHON类型数据，注册时c_decoder_register_output()会输出警告"cannot be properly consumed by upper-layer Python decoders"，这意味着i2c_packet_c的Python输出只能被C解码器消费，不能被Python解码器消费。这是C解码器框架的已知限制。 -->

**复杂度评估：** 约 300-400 行 C 代码。主要难点在字符串格式化和 Python 输出数据打包。

---

## 3. I2C Demux (`i2cdemux` → `i2cdemux_c`)

### 3.1 Python 元数据

```python
id = 'i2cdemux'
name = 'I²C demux'
longname = 'I²C demultiplexer'
desc = 'Demux I²C packets into per-slave-address streams.'
license = 'gplv2+'
inputs = ['i2c']
outputs = []  # 运行时动态创建 <!-- Updated: C版在recv_proto中通过c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "i2c-0xNN")动态注册，已确认安全 -->
tags = ['Util']
```

**无 options、无 channels、无 annotations、无 annotation_rows。**

### 3.2 解码逻辑分析

I2C Demux 是一个纯路由解码器，无 annotation 输出，仅输出 Python 协议数据。

**核心逻辑：**
1. 缓存从 START 到 STOP 之间的所有 I2C 包
2. 在 `ADDRESS READ` / `ADDRESS WRITE` 时确定从设备地址
3. 如果是新地址，动态注册一个新的 `OUTPUT_PYTHON` 输出流（proto_id=`i2c-0xNN`）
4. 在 `STOP` 时，将整段缓存数据发送到对应地址的输出流

**动态输出流：** Python 版使用 `self.register(srd.OUTPUT_PYTHON, proto_id='i2c-%s' % hex(databyte))` 动态注册输出。C 版需使用 `c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "i2c-0xNN")` 实现相同功能。

### 3.3 C 实现方案

**文件：** `i2cdemux_c.c`

**私有状态结构：**
```c
#define MAX_SLAVES 128
#define MAX_PACKETS 1024
// <!-- Updated: i2cdemux_state结构体大小约 (8+8+32+8+8)*1024 + 128 + 128*4 = ~66KB，需确认不会导致栈溢出，建议使用g_malloc0动态分配 -->

typedef struct {
    int num_slaves;
    uint8_t slaves[MAX_SLAVES];       // 已知从设备地址列表
    int out_python[MAX_SLAVES];       // 每个从设备的输出流 ID
    int stream;                       // 当前输出流索引
    int streamcount;                  // 已创建的输出流数量

    // 包缓存
    struct {
        uint64_t ss;
        uint64_t es;
        char cmd[32];
        uint8_t data[8];  // <!-- Updated: I2C ADDRESS/DATA命令data仅1字节，8字节足够；但BITS包数据可达144字节，i2cdemux不转发BITS所以无需更大缓冲区 -->
        uint64_t data_len;
    } packets[MAX_PACKETS];
    int num_packets;
} i2cdemux_state;
```

**注意：** 由于此解码器无 annotation，`num_annotations = 0`，`ann_labels = NULL`，`annotation_rows = NULL`。

**recv_proto 核心逻辑：**
```c
static void i2cdemux_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    i2cdemux_state *s = (i2cdemux_state *)c_decoder_get_private(di);
    if (!s) return;

    // 缓存包
    if (s->num_packets < MAX_PACKETS) {
        int i = s->num_packets++;
        s->packets[i].ss = start_sample;
        s->packets[i].es = end_sample;
        strncpy(s->packets[i].cmd, cmd, sizeof(s->packets[i].cmd) - 1);
        uint64_t copy_len = data_len < sizeof(s->packets[i].data) ? data_len : sizeof(s->packets[i].data);
        if (data && copy_len > 0) memcpy(s->packets[i].data, data, copy_len);
        s->packets[i].data_len = copy_len;
    }
    // <!-- Updated: 当num_packets >= MAX_PACKETS时包被静默丢弃，应考虑输出警告注解或增大缓冲区 -->

    if (strcmp(cmd, "ADDRESS READ") == 0 || strcmp(cmd, "ADDRESS WRITE") == 0) {
        uint8_t addr = (data && data_len > 0) ? data[0] : 0;
        // 查找或创建输出流
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
    } else if (strcmp(cmd, "STOP") == 0) {
        // 发送所有缓存包到目标流
        if (s->stream >= 0 && s->stream < s->num_slaves) {
            for (int i = 0; i < s->num_packets; i++) {
                c_decoder_put_python(di, s->packets[i].ss, s->packets[i].es,
                    s->out_python[s->stream],
                    s->packets[i].cmd, s->packets[i].data, s->packets[i].data_len);
            }
        }
        s->num_packets = 0;
        s->stream = -1;
    }
}
```

**复杂度评估：** 约 150-200 行 C 代码。逻辑简单，但动态输出流注册是关键。

---

## 4. I2C Filter (`i2cfilter` → `i2cfilter_c`)

### 4.1 Python 元数据

```python
id = 'i2cfilter'
name = 'I²C filter'
longname = 'I²C filter'
desc = 'Filter out addresses/directions in an I²C stream.'
license = 'gplv3+'
inputs = ['i2c']
outputs = ['i2c']
tags = ['Util']
options = (
    {'id': 'address', 'desc': 'Address to filter out of the I²C stream',
        'default': 0, 'idn':'dec_i2cfilter_opt_address'},
    {'id': 'direction', 'desc': 'Direction to filter', 'default': 'both',
        'values': ('read', 'write', 'both'), 'idn':'dec_i2cfilter_opt_direction'}
)
```

**无 channels、无 annotations、无 annotation_rows。**

### 4.2 解码逻辑分析

I2C Filter 与 I2C Demux 类似，但只保留匹配指定地址和方向的数据。

**核心逻辑：**
1. 缓存从 START 到 STOP/START REPEAT 之间的所有 I2C 包
2. 在 `ADDRESS READ` / `ADDRESS WRITE` 时记录当前从设备地址和方向
3. 在 `STOP` / `START REPEAT` 时判断是否匹配过滤条件
4. 如果匹配，将缓存数据通过 `OUTPUT_PYTHON` (proto_id='i2c') 转发
5. 如果不匹配，丢弃缓存

**过滤条件：**
- `address == 0` → 不过滤地址（通过所有地址）
- `address != 0` → 只转发指定地址的数据
- `direction == 'both'` → 不过滤方向
- `direction == 'read'` → 只转发 ADDRESS READ 段
- `direction == 'write'` → 只转发 ADDRESS WRITE 段

### 4.3 C 实现方案

**文件：** `i2cfilter_c.c`

**私有状态结构：**
```c
#define I2CFILTER_MAX_PACKETS 1024

typedef struct {
    int out_python;
    int curslave;
    int curdirection;       // 0=none, 1=read, 2=write
    int filter_address;
    int filter_direction;   // 0=both, 1=read, 2=write

    struct {
        uint64_t ss;
        uint64_t es;
        char cmd[32];
        uint8_t data[8];  // <!-- Updated: 同i2cdemux，I2C ADDRESS/DATA仅1字节，8字节足够 -->
        uint64_t data_len;
    } packets[I2CFILTER_MAX_PACKETS];
    int num_packets;
} i2cfilter_state;
```

**Options：**
```c
static struct srd_decoder_option i2cfilter_options[] = {
    {"address", "dec_i2cfilter_opt_address", "Address to filter out of the I²C stream", NULL, NULL},
    {"direction", "dec_i2cfilter_opt_direction", "Direction to filter", NULL, NULL},
};
```

**recv_proto 核心逻辑：**
```c
static void i2cfilter_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    i2cfilter_state *s = (i2cfilter_state *)c_decoder_get_private(di);
    if (!s) return;

    // 缓存包
    if (s->num_packets < I2CFILTER_MAX_PACKETS) {
        int i = s->num_packets++;
        s->packets[i].ss = start_sample;
        s->packets[i].es = end_sample;
        strncpy(s->packets[i].cmd, cmd, sizeof(s->packets[i].cmd) - 1);
        uint64_t copy_len = data_len < sizeof(s->packets[i].data) ? data_len : sizeof(s->packets[i].data);
        if (data && copy_len > 0) memcpy(s->packets[i].data, data, copy_len);
        s->packets[i].data_len = copy_len;
    }

    if (strcmp(cmd, "ADDRESS READ") == 0 || strcmp(cmd, "ADDRESS WRITE") == 0) {
        s->curslave = (data && data_len > 0) ? data[0] : 0;
        s->curdirection = (strcmp(cmd, "ADDRESS READ") == 0) ? 1 : 2;
    } else if (strcmp(cmd, "STOP") == 0 || strcmp(cmd, "START REPEAT") == 0) {
        // 地址过滤
        if (s->filter_address != 0 && s->curslave != s->filter_address) {
            s->num_packets = 0;
            return;
        }
        // 方向过滤
        if (s->filter_direction != 0 && s->curdirection != s->filter_direction) {
            s->num_packets = 0;
            return;
        }
        // 转发
        for (int i = 0; i < s->num_packets; i++) {
            c_decoder_put_python(di, s->packets[i].ss, s->packets[i].es,
                s->out_python, s->packets[i].cmd,
                s->packets[i].data, s->packets[i].data_len);
        }
        s->num_packets = 0;
    }
}
```

**复杂度评估：** 约 150-200 行 C 代码。与 i2cdemux 非常相似，增加过滤逻辑。

---

## 5. LTC26x7 (`ltc26x7` → `ltc26x7_c`)

### 5.1 Python 元数据

```python
id = 'ltc26x7'
name = 'LTC26x7'
longname = 'Linear Technology LTC26x7'
desc = 'Linear Technology LTC26x7 16-/14-/12-bit rail-to-rail DACs.'
license = 'gplv2+'
inputs = ['i2c']
outputs = []
tags = ['IC', 'Analog/digital']
options = (
    {'id': 'chip', 'desc': 'Chip', 'default': 'ltc2607',
        'values': ('ltc2607', 'ltc2617', 'ltc2627'), 'idn':'dec_ltc26x7_opt_chip'},
    {'id': 'vref', 'desc': 'Reference voltage (V)', 'default': 1.5, 'idn':'dec_ltc26x7_opt_vref'},
)
annotations = (
    ('slave_addr', 'Slave address'),       # 0
    ('command', 'Command'),                # 1
    ('address', 'Address'),                # 2
    ('dac_a_voltage', 'DAC A voltage'),    # 3
    ('dac_b_voltage', 'DAC B voltage'),    # 4
)
annotation_rows = (
    ('addr_cmd', 'Address/command', (0, 1, 2)),
    ('dac_a_voltages', 'DAC A voltages', (3,)),
    ('dac_b_voltages', 'DAC B voltages', (4,)),
)
```

### 5.2 解码逻辑分析

LTC26x7 解码器解析 Linear Technology DAC 芯片的 I2C 通信。

**状态机：**
- `IDLE` → 等待 START
- `GET SLAVE ADDR` → 等待 ADDRESS WRITE
- `GET CMD ADDR` → 等待 DATA WRITE (命令+地址字节)
- `WRITE DATA` → 等待 2 个 DATA WRITE (数据高字节+低字节)

**从设备地址解析：**
- 地址位 CA2/CA1/CA0 由引脚电平决定 (GND/FLOAT/VCC)
- 特殊地址 0x73 为全局地址
- 需要将 I2C 地址转换为三进制表示

**命令/地址字节格式：**
- 高 4 位 = 命令 (0x00=Write Input Reg, 0x01=Update DAC, 0x03=Write&Power Up, 0x04=Power Down, 0x0F=No Op)
- 低 4 位 = DAC 地址 (0x00=DAC A, 0x01=DAC B, 0x0F=All DACs)

**数据字节：**
- 2 字节组成 16-bit 数据值
- 根据 chip 选项进行移位和电压计算：
  - `ltc2607`: 16-bit, `V = vref * data / 0xFFFF`
  - `ltc2617`: 14-bit, `data >>= 2`, `V = vref * data / 0x3FFF`
  - `ltc2627`: 12-bit, `data >>= 4`, `V = vref * data / 0x0FFF`

### 5.3 C 实现方案

**文件：** `ltc26x7_c.c`

**私有状态结构：**
```c
typedef struct {
    int out_ann;
    int state;              // LTC_IDLE, LTC_GET_SLAVE_ADDR, LTC_GET_CMD_ADDR, LTC_WRITE_DATA
    uint64_t ss;
    uint64_t es;
    int data;               // 累积的数据值
    int dac_val;            // DAC 地址值
    int chip;               // 0=ltc2607, 1=ltc2617, 2=ltc2627
    double vref;
    int first_data_byte;    // 是否是第一个数据字节
    uint64_t data_ss;       // 数据起始采样号
} ltc26x7_state;

enum {
    LTC_IDLE,
    LTC_GET_SLAVE_ADDR,
    LTC_GET_CMD_ADDR,
    LTC_WRITE_DATA,
};
```

**Annotation 定义：**
```c
enum {
    ANN_SLAVE_ADDR = 0,
    ANN_COMMAND = 1,
    ANN_ADDRESS = 2,
    ANN_DAC_A_VOLTAGE = 3,
    ANN_DAC_B_VOLTAGE = 4,
    NUM_ANN,
};

static const char *ltc26x7_ann_labels[][3] = {
    {"", "slave_addr", "Slave address"},
    {"", "command", "Command"},
    {"", "address", "Address"},
    {"", "dac_a_voltage", "DAC A voltage"},
    {"", "dac_b_voltage", "DAC B voltage"},
};

static const int ltc26x7_row_addr_cmd_classes[] = {ANN_SLAVE_ADDR, ANN_COMMAND, ANN_ADDRESS, -1};
static const int ltc26x7_row_dac_a_classes[] = {ANN_DAC_A_VOLTAGE, -1};
static const int ltc26x7_row_dac_b_classes[] = {ANN_DAC_B_VOLTAGE, -1};

static const struct srd_c_ann_row ltc26x7_ann_rows[] = {
    {"addr_cmd", "Address/command", ltc26x7_row_addr_cmd_classes, 3},
    {"dac_a_voltages", "DAC A voltages", ltc26x7_row_dac_a_classes, 1},
    {"dac_b_voltages", "DAC B voltages", ltc26x7_row_dac_b_classes, 1},
};
```

**Options：**
```c
static struct srd_decoder_option ltc26x7_options[] = {
    {"chip", "dec_ltc26x7_opt_chip", "Chip", NULL, NULL},
    {"vref", "dec_ltc26x7_opt_vref", "Reference voltage (V)", NULL, NULL},
};
```

**关键函数 — handle_slave_addr：**
```c
static const char *slave_address_str[][4] = {
    { "GND", "GND", "GND", "G" },   // 0
    { "FLOAT", "FLOAT", "FLOAT", "F" }, // 1
    { "VCC", "VCC", "VCC", "V" },   // 2
};

static void convert_ternary(int n, int result[3]) {
    for (int i = 2; i >= 0; i--) {
        result[i] = n % 3;
        n /= 3;
    }
}

static void ltc26x7_handle_slave_addr(struct srd_decoder_inst *di,
    ltc26x7_state *s, uint8_t data)
{
    if (data == 0x73) {
        C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_SLAVE_ADDR,
                  "Global address", "Global addr", "Glob addr", "GA");
        return;
    }
    // 提取 CA2/CA1/CA0 位
    int addr = 0;
    for (int i = 0; i < 7; i++) {
        if (i == 2 || i == 3) continue;
        int offset = i;
        if (i > 3) offset -= 2;
        if (data & (1 << i)) addr |= (1 << offset);
    }
    addr -= 0x04;
    int ternary[3];
    convert_ternary(addr, ternary);

    char buf[128];
    snprintf(buf, sizeof(buf), "CA2=%s CA1=%s CA0=%s",
             slave_address_str[ternary[0]][0],
             slave_address_str[ternary[1]][0],
             slave_address_str[ternary[2]][0]);
    C_ANN_PUT(di, s->ss, s->es, s->out_ann, ANN_SLAVE_ADDR, buf);
}
```

**关键函数 — handle_data：**
```c
static const char *commands[][4] = {
    { "Write Input Register", "Write In Reg", "Wr In Reg", "WIR" },  // 0x00
    { "Update DAC", "Update", "U", "U" },                            // 0x01
    { "Write and Power Up DAC", "Write & Power Up", "W&PU", "W&PU" },// 0x03
    { "Power Down DAC", "Power Down", "PD", "PD" },                  // 0x04
    { "No Operation", "NoOp", "NO", "NO" },                          // 0x0F
};

static const char *addresses[][2] = {
    { "DAC A", "A" },    // 0x00
    { "DAC B", "B" },    // 0x01
    { "All DACs", "All" }, // 0x0F
};

static void ltc26x7_handle_data(struct srd_decoder_inst *di,
    ltc26x7_state *s, uint8_t databyte)
{
    s->data = (s->data << 8) & 0xFF00;
    s->data += databyte;

    double voltage;
    switch (s->chip) {
    case 1: // ltc2617 (14-bit)
        s->data >>= 2;
        voltage = s->vref * s->data / 0x3FFF;
        break;
    case 2: // ltc2627 (12-bit)
        s->data >>= 4;
        voltage = s->vref * s->data / 0x0FFF;
        break;
    default: // ltc2607 (16-bit)
        voltage = s->vref * s->data / 0xFFFF;
        break;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "%.6fV", voltage);
    char buf2[32];
    snprintf(buf2, sizeof(buf2), "%.2fV", voltage);

    s->data = 0;
    if (s->dac_val == 0x0F) {
        // All DACs
        C_ANN_PUT(di, s->data_ss, s->es, s->out_ann, ANN_DAC_A_VOLTAGE, buf, buf2);
        C_ANN_PUT(di, s->data_ss, s->es, s->out_ann, ANN_DAC_B_VOLTAGE, buf, buf2);
    } else {
        C_ANN_PUT(di, s->data_ss, s->es, s->out_ann, ANN_DAC_A_VOLTAGE + s->dac_val, buf, buf2);
    }
}
```

**复杂度评估：** 约 250-350 行 C 代码。

---

## 通用 C 解码器框架模板

所有 5 个解码器均遵循以下模板：

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

// 1. Annotation enum
// 2. 私有状态结构
// 3. 静态元数据 (inputs, tags, ann_labels, ann_rows, options)
// 4. 辅助函数
// 5. recv_proto() 回调
// 6. reset() / start() / decode() / destroy()
// 7. srd_c_decoder 结构体
// 8. srd_c_decoder_entry() + srd_c_decoder_api_version()
```

**关键 API 函数：**
| 函数 | 用途 |
|------|------|
| `c_decoder_register_output(di, SRD_OUTPUT_ANN, "xxx")` | 注册 annotation 输出 |
| `c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "i2c")` | 注册 Python 协议输出 |
| `C_ANN_PUT(di, ss, es, out_id, cls, ...)` | 输出 annotation |
| `c_decoder_put_python(di, ss, es, out_id, cmd, data, len)` | 输出 Python 协议数据 |
| `c_decoder_put_binary(di, ss, es, out_id, cls, size, data)` | 输出 binary 数据 |
| `c_decoder_get_private(di)` | 获取私有状态 |
| `c_decoder_set_private(di, ptr)` | 设置私有状态 |
| `c_decoder_get_option_int(di, key, def)` | 获取整数选项 |
| `c_decoder_get_option_double(di, key, def)` | 获取浮点选项 |
| `c_decoder_get_option_string(di, key, def)` | 获取字符串选项 |

**recv_proto 签名：**
```c
void (*recv_proto)(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

**I2C 协议 cmd 值：**
- `"START"`, `"START REPEAT"`, `"STOP"`
- `"ADDRESS READ"`, `"ADDRESS WRITE"`
- `"DATA READ"`, `"DATA WRITE"`
- `"ACK"`, `"NACK"`

---

## CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加：
```cmake
edid_c
i2c_packet_c
i2cdemux_c
i2cfilter_c
ltc26x7_c
```

---

## 特殊注意事项

### EDID 的 pnpids.txt 处理
Python 版使用 `os.path.join(os.path.dirname(__file__), 'pnpids.txt')` 加载 PNP ID 映射。C 版有两种方案：
1. **内嵌方案**：将常用 PNP ID 编译为静态数组（推荐，约 500 条常用条目）
2. **文件查找方案**：运行时读取 pnpids.txt 文件（需处理路径问题）
3. **简化方案**：仅输出 PNPID 代码，不查找厂商名称

<!-- Updated: 推荐使用内嵌方案（方案1），将pnpids.txt编译为静态排序数组+二分查找。pnpids.txt位于libsigrokdecode/decoders/edid/pnpids.txt，C版需将此文件内容转换为C常量数组。 -->

### i2c_packet 的 Python 输出
`c_decoder_put_python()` 的 data 参数为 `const unsigned char *`，需要将 `(address, (data_bytes...))` 元组序列化为字节流。建议格式：
- data[0] = address
- data[1..] = 数据字节

### i2cdemux / i2cfilter 的动态输出流
这两个解码器在运行时动态注册输出流。需确认 `c_decoder_register_output()` 在 `recv_proto()` 回调中调用是否安全。参考 `lm75_c.c` 和 `ds1307_c.c`，它们在 `start()` 中注册输出，但 i2cdemux 需要在 `recv_proto()` 中动态注册。

<!-- Updated: 经审查c_decoder_register_output()实现(c_decoder_api.c:268-292)，该函数仅执行g_malloc0分配和g_slist_append追加到di->pd_output链表，无全局状态修改，无线程安全问题。在recv_proto()中调用是安全的。但需注意：动态注册的输出流可能无法被上层Python解码器消费（c_decoder_register_output对SRD_OUTPUT_PYTHON会输出警告），仅能被C解码器通过recv_proto消费。 -->

### i2cfilter 的 outputs
Python 版 `outputs = ['i2c']`，C 版需在 `srd_c_decoder` 中设置 `outputs` 数组，并在 `start()` 中注册 `SRD_OUTPUT_PYTHON` 输出（proto_id="i2c"）。

<!-- Updated: i2cfilter的Python版在__init__中注册self.register(srd.OUTPUT_PYTHON, proto_id='i2c')，C版需在start()中对应注册 -->

---

## 复杂度总结

| 解码器 | 预估行数 | 难点 | 风险 |
|--------|----------|------|------|
| edid_c | 800-1000 | 大量解析子函数、PNPID 查找、扩展块处理 | 高 |
| i2c_packet_c | 300-400 | 字符串格式化、Python 输出打包 | 中 |
| i2cdemux_c | 150-200 | 动态输出流注册 | 中 | <!-- Updated: c_decoder_register_output()在recv_proto中调用已确认安全，风险降为低 -->
| i2cfilter_c | 150-200 | 动态输出流、过滤逻辑 | 低 |
| ltc26x7_c | 250-350 | 三进制地址转换、电压计算 | 低 |

---

## 解码器依赖关系

<!-- Updated: 新增依赖关系说明，明确C解码器只能依赖已有C实现的底层解码器 -->

| 解码器 | 输入类型 | 依赖的底层C解码器 | 状态 |
|--------|----------|-------------------|------|
| edid_c | i2c | i2c_c.c ✅ | 可实现 |
| i2c_packet_c | i2c | i2c_c.c ✅ | 可实现 |
| i2cdemux_c | i2c | i2c_c.c ✅ | 可实现（动态输出流已确认安全） |
| i2cfilter_c | i2c | i2c_c.c ✅ | 可实现 |
| ltc26x7_c | i2c | i2c_c.c ✅ | 可实现 |

**注意**: i2cdemux_c 和 i2cfilter_c 输出 SRD_OUTPUT_PYTHON，仅能被C解码器消费（上层Python解码器无法消费C解码器的Python输出）。
