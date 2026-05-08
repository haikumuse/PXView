# Python 解码器重写为 C 语言指南

## 项目背景

DSView 使用 libsigrokdecode4DSL 库来支持协议解码器。原始解码器全部用 Python 编写，运行在 Python 解释器上。为了提高执行效率，我们将高频使用的底层协议解码器重写为 C 语言 DLL，由主程序在运行时动态加载。

### 当前状态

- **已完成 C 解码器**：24 个（14 个完整协议 + 10 个本轮新增 + 8 个 stub）
- **C 解码器架构**：每个解码器编译为独立 DLL，通过 `srd_c_decoder` 结构体导出
- **运行时加载**：主程序扫描 c_decoders 目录，加载 DLL 并注册解码器

### 编译指令

```bash
# 增量编译
/c:/Users/admin/Downloads/DSView-main_2026_4_27cppnb/build_incremental.cmd

# 完整重新配置
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=../install.dir -DENABLE_DEBUG_HELPER=ON
```

---

## 一、C 解码器文件结构

每个 C 解码器是一个独立的 `.c` 文件，编译为 DLL。文件结构如下：

```
c_decoders/
├── xxx_c.c          # 解码器源码
├── c_decoder_api.c  # 共享的 API 实现（自动链接）
└── ...
```

### 文件模板骨架

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

/* 1. 状态枚举 */
enum xxx_state {
    STATE_IDLE,
    STATE_XXX,
    ...
};

/* 2. 注解枚举 */
enum xxx_ann {
    ANN_XXX = 0,
    ANN_YYY,
    NUM_ANN,
};

/* 3. 私有数据结构 */
typedef struct {
    enum xxx_state state;
    uint64_t samplerate;
    /* ... 其他状态变量 ... */
    int out_ann;
} xxx_priv;

/* 4. 通道定义 */
static struct srd_channel xxx_channels[] = {
    {"id", "Name", "Description", order, SRD_CHANNEL_COMMON, "idn"},
};

/* 5. 选项定义（如需要） */
static struct srd_decoder_option xxx_options[1];

/* 6. 注解标签（每个注解最多3个文本） */
static const char *xxx_ann_labels[][3] = {
    {"短文本", "中文本", "长文本"},
    ...
};

/* 7. 注解行（将注解分组显示） */
static const int row_xxx_classes[] = {ANN_XXX, ANN_YYY, -1};  /* -1 结尾 */
static const struct srd_c_ann_row xxx_ann_rows[] = {
    {"row_id", "Row Name", row_xxx_classes, num_classes},
};

/* 8. 输入/输出/标签 */
static const char *xxx_inputs[] = {"logic", NULL};
static const char *xxx_outputs[] = {"xxx", NULL};
static const char *xxx_tags[] = {"Tag", NULL};

/* 9. 回调函数 */
static void xxx_reset(struct srd_decoder_inst *di) { ... }
static void xxx_start(struct srd_decoder_inst *di) { ... }
static void xxx_decode(struct srd_decoder_inst *di) { ... }
static void xxx_destroy(struct srd_decoder_inst *di) { ... }

/* 10. 导出结构体 */
static struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",
    .name = "XXX",
    .longname = "Full Protocol Name (C)",
    .desc = "Protocol description.",
    .license = "gplv2+",
    .channels = xxx_channels,
    .num_channels = 1,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = xxx_options,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = 1,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,
    .num_inputs = 1,
    .outputs = xxx_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = xxx_tags,
    .num_tags = 1,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,
    .end = NULL,
    .metadata = NULL,
    .destroy = xxx_destroy,
};

/* 11. 导出入口函数 */
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    /* 初始化选项的 GVariant 默认值和枚举值（如需要） */
    return &xxx_c_decoder;
}

SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)
{
    return SRD_C_DECODER_API_VERSION;
}
```

---

## 二、核心 API 详解

### 2.1 条件等待系统（c_cond_*）

这是 C 解码器最核心的机制，替代了 Python 解码器中的 `self.wait()` 调用。

**Python → C 对照表：**

| Python | C |
|--------|---|
| `self.wait({0: 'r'})` | `c_cond_rise(cb, 0); c_cond_wait(cb, di, &sn, &m);` |
| `self.wait({0: 'f'})` | `c_cond_fall(cb, 0); c_cond_wait(cb, di, &sn, &m);` |
| `self.wait({0: 'e'})` | `c_cond_edge(cb, 0); c_cond_wait(cb, di, &sn, &m);` |
| `self.wait({0: 'h'})` | `c_cond_high(cb, 0); c_cond_wait(cb, di, &sn, &m);` |
| `self.wait({0: 'l'})` | `c_cond_low(cb, 0); c_cond_wait(cb, di, &sn, &m);` |
| `self.wait([{'skip': N}])` | `c_cond_skip(cb, N); c_cond_wait(cb, di, &sn, &m);` |
| `self.wait([{0: 'e'}, {1: 'e'}])` | `c_cond_edge(cb, 0); c_cond_or(cb); c_cond_edge(cb, 1); c_cond_wait(...)` |
| `self.wait()` | `c_cond_wait(c_cond_new(), di, &sn, &m);` |

**使用模式：**

```c
srd_cond_builder *cb = c_cond_new();
c_cond_rise(cb, 0);       /* 等待通道0上升沿 */
c_cond_or(cb);             /* OR 条件 */
c_cond_fall(cb, 1);        /* 或通道1下降沿 */

uint64_t samplenum = 0, matched = 0;
int ret = c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);

if (ret != SRD_OK) return;  /* 解码被终止 */

/* matched 的第 N 位表示第 N 个条件是否匹配 */
int cond0_matched = (matched & 1) != 0;
int cond1_matched = (matched & 2) != 0;
```

**⚠️ 重要：** `c_cond_wait` 只能调用一次，之后 builder 自动失效。每次等待都需要 `c_cond_new()` 创建新的 builder。

### 2.2 读取引脚值

```c
uint8_t pin_value = c_decoder_get_pin(di, channel_index, samplenum);
```

- `channel_index`：通道索引，从0开始，按 `channels` 数组顺序
- `samplenum`：采样点编号
- 返回值：0 或 1

### 2.3 注解输出

```c
/* 基本用法：输出注解 */
C_ANN_PUT(di, start_sample, end_sample, out_ann, ann_class, "文本1", "文本2", "文本3");

/* 带类型的注解 */
C_ANN_PUT_TYPE(di, start_sample, end_sample, out_ann, ann_class, ann_type, "文本1", "文本2");
```

- 文本参数数量可变，至少1个，最多3个
- `out_ann`：在 `start()` 中通过 `c_decoder_register_output()` 获取
- `ann_class`：注解类别索引，对应 `ann_labels` 数组

### 2.4 私有数据管理

```c
/* 在 reset() 中分配 */
static void xxx_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(xxx_priv)));
    }
    xxx_priv *s = (xxx_priv *)c_decoder_get_private(di);
    memset(s, 0, sizeof(xxx_priv));
    s->state = STATE_IDLE;
}

/* 在 destroy() 中释放 */
static void xxx_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}
```

### 2.5 选项读取

```c
/* 整数选项 */
int64_t bitrate = c_decoder_get_option_int(di, "baudrate", 115200);

/* 浮点选项 */
double sample_point = c_decoder_get_option_double(di, "sample_point", 70.0);

/* 字符串选项 */
const char *sig = c_decoder_get_option_string(di, "signalling", "automatic");
```

### 2.6 其他 API

```c
uint64_t samplerate = c_decoder_get_samplerate(di);     /* 获取采样率 */
int has_ch = c_decoder_has_channel(di, channel_index);   /* 检查通道是否连接 */
int out_id = c_decoder_register_output(di, SRD_OUTPUT_ANN, "proto_id");  /* 注册输出 */
```

---

## 三、Python → C 重写步骤

### 步骤1：分析 Python 解码器

1. 读取 `decoders/xxx/__init__.py` 和 `decoders/xxx/pd.py`
2. 识别以下关键信息：
   - `channels` / `optional_channels`：通道定义
   - `options`：选项定义
   - `annotations`：注解定义
   - `annotation_rows`：注解行定义
   - `inputs` / `outputs`：输入输出协议ID
   - `tags`：标签
   - 状态机：`decode()` 中的状态转换逻辑
   - `wait()` 调用：条件等待模式
   - `put()` 调用：注解输出

### 步骤2：映射数据结构

| Python | C |
|--------|---|
| `self.xxx` 实例变量 | `xxx_priv` 结构体字段 |
| `self.state = 'IDLE'` | `s->state = STATE_IDLE` |
| `self.samplenum` | `samplenum` 局部变量（由 c_cond_wait 返回） |
| `self.matched` | `matched` 局部变量（由 c_cond_wait 返回） |
| `self.wait(...)` | `c_cond_new()` + `c_cond_xxx()` + `c_cond_wait()` + `c_cond_free()` |
| `self.put(ss, es, out, [cls, [texts]])` | `C_ANN_PUT(di, ss, es, out, cls, texts...)` |
| `self.register(srd.OUTPUT_ANN)` | `c_decoder_register_output(di, SRD_OUTPUT_ANN, "proto_id")` |
| `self.options['key']` | `c_decoder_get_option_xxx(di, "key", default)` |
| `pins[0]` / `self.wait()[0]` | `c_decoder_get_pin(di, 0, samplenum)` |

### 步骤3：翻译状态机

Python 解码器的 `decode()` 通常是 `while True` 循环，根据 `self.state` 分支处理。C 解码器保持相同结构：

```c
static void xxx_decode(struct srd_decoder_inst *di)
{
    xxx_priv *s = (xxx_priv *)c_decoder_get_private(di);
    uint64_t samplenum = 0, matched = 0;

    while (1) {
        switch (s->state) {
        case STATE_IDLE: {
            srd_cond_builder *cb = c_cond_new();
            c_cond_fall(cb, 0);
            int ret = c_cond_wait(cb, di, &samplenum, &matched);
            c_cond_free(cb);
            if (ret != SRD_OK) return;

            uint8_t pin = c_decoder_get_pin(di, 0, samplenum);
            /* 处理逻辑... */
            s->state = STATE_NEXT;
            break;
        }
        case STATE_NEXT: {
            /* ... */
            break;
        }
        }
    }
}
```

### 步骤4：翻译 wait() 调用

这是最关键的翻译步骤。Python 的 `wait()` 有多种调用模式：

**模式1：等待单个条件**
```python
# Python
(can_rx,) = self.wait({0: 'f'})
```
```c
// C
srd_cond_builder *cb = c_cond_new();
c_cond_fall(cb, 0);
c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
uint8_t can_rx = c_decoder_get_pin(di, 0, samplenum);
```

**模式2：等待多个条件（OR）**
```python
# Python
(dp, dm) = self.wait([{0: 'e'}, {1: 'e'}])
```
```c
// C
srd_cond_builder *cb = c_cond_new();
c_cond_edge(cb, 0);
c_cond_or(cb);
c_cond_edge(cb, 1);
c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
uint8_t dp = c_decoder_get_pin(di, 0, samplenum);
uint8_t dm = c_decoder_get_pin(di, 1, samplenum);
```

**模式3：等待跳过（skip）**
```python
# Python
(pins,) = self.wait([{'skip': N}])
```
```c
// C
srd_cond_builder *cb = c_cond_new();
c_cond_skip(cb, N);
c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

**模式4：混合条件（skip + edge）**
```python
# Python
(can_rx,) = self.wait([{'skip': pos - self.samplenum}, {0: 'f'}])
if (self.matched & (0b1 << 1)):
    self.dom_edge_seen()
if (self.matched & (0b1 << 0)):
    self.handle_bit(can_rx)
```
```c
// C
srd_cond_builder *cb = c_cond_new();
c_cond_skip(cb, pos - samplenum);
c_cond_or(cb);
c_cond_fall(cb, 0);
c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
uint8_t can_rx = c_decoder_get_pin(di, 0, samplenum);
if (matched & 2) dom_edge_seen(s);
if (matched & 1) handle_bit(di, s, can_rx);
```

**模式5：无条件等待（下一采样点）**
```python
# Python
pins = self.wait()
```
```c
// C
srd_cond_builder *cb = c_cond_new();
c_cond_wait(cb, di, &samplenum, &matched);
c_cond_free(cb);
```

### 步骤5：翻译 put() 调用

```python
# Python
self.put(ss, es, self.out_ann, [cls, ['text1', 'text2', 'text3']])
```
```c
// C
C_ANN_PUT(di, ss, es, s->out_ann, cls, "text1", "text2", "text3");
```

### 步骤6：注册到 CMakeLists.txt

在 `CMakeLists.txt` 中将新解码器名添加到 `C_DECODERS` 列表：

```cmake
set(C_DECODERS spi_c i2c_c uart_c can_c ... xxx_c)
```

### 步骤7：编译验证

```bash
cmake --build build --target decoder_xxx_c
```

---

## 四、常见模式与技巧

### 4.1 时钟同步模式

许多协议需要从信号边沿恢复时钟。典型模式：

```c
double bitwidth = (double)samplerate / bitrate;
double samplepos = (double)samplenum;  /* 浮点追踪位置 */

/* 每个比特周期更新 */
samplepos += bitwidth;
uint64_t target = (uint64_t)samplepos;
uint64_t mid_bit = (uint64_t)(samplepos - bitwidth / 2.0);
```

### 4.2 边沿检测 + 时钟调整

USB、CAN 等协议需要根据信号边沿微调时钟：

```c
if (b == 0) {
    enum usb_sym edgesym = get_symbol(mode, edgepins_dp, edgepins_dm);
    if (edgesym == current_sym) {
        bitwidth -= 0.001 * bitwidth;      /* 缩短0.1% */
        samplepos -= 0.01 * bitwidth;      /* 回调1% */
    } else {
        bitwidth += 0.001 * bitwidth;      /* 延长0.1% */
        samplepos += 0.01 * bitwidth;      /* 前推1% */
    }
}
```

### 4.3 比特填充（Bit Stuffing）

CAN、USB 等协议使用比特填充：

```c
if (consecutive_ones == 6) {
    if (b == 0) {
        /* 填充比特，不作为数据比特 */
        C_ANN_PUT(di, ss, es, out_ann, ANN_STUFFBIT, "0");
        consecutive_ones = 0;
    } else {
        /* 错误：6个1后必须是0 */
        C_ANN_PUT(di, ss, es, out_ann, ANN_ERROR, "Bit stuff error");
        state = STATE_IDLE;
    }
} else {
    if (b == 1) consecutive_ones++;
    else consecutive_ones = 0;
}
```

### 4.4 CRC 校验

CAN、HDLC 等协议需要 CRC 校验。C 语言实现示例：

```c
static uint16_t crc16_ccitt(const uint8_t *data, int len)
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

### 4.5 BCD 解码

DCF77、CEC 等协议使用 BCD 编码：

```c
static int bcd_to_int(int bcd_bits[], int num_bits)
{
    static const int weights[] = {1, 2, 4, 8, 10, 20, 40, 80};
    int value = 0;
    for (int i = 0; i < num_bits && i < 8; i++) {
        if (bcd_bits[i]) value += weights[i];
    }
    return value;
}
```

### 4.6 MSB 优先比特打包

CAN 等协议使用 MSB 优先：

```c
static uint64_t bitpack_msb(int bits[], int count)
{
    uint64_t value = 0;
    for (int i = 0; i < count; i++)
        value = (value << 1) | bits[i];
    return value;
}
```

### 4.7 选项初始化（srd_c_decoder_entry）

选项的 GVariant 值必须在 `srd_c_decoder_entry()` 中初始化，不能在静态初始化中完成：

```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    /* 字符串枚举选项 */
    GVariant *vals[] = {
        g_variant_new_string("automatic"),
        g_variant_new_string("full-speed"),
        g_variant_new_string("low-speed"),
    };
    GSList *val_list = NULL;
    for (int i = 0; i < 3; i++)
        val_list = g_slist_append(val_list, vals[i]);

    static struct srd_decoder_option opts[1];
    opts[0].id = "signalling";
    opts[0].idn = "dec_usb_signalling_opt_signalling";
    opts[0].desc = "Signalling";
    opts[0].def = g_variant_new_string("automatic");
    opts[0].values = val_list;

    xxx_c_decoder.options = opts;
    return &xxx_c_decoder;
}
```

整数选项更简单：

```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    static struct srd_decoder_option opts[1];
    opts[0].id = "baudrate";
    opts[0].idn = NULL;
    opts[0].desc = "Baud rate";
    opts[0].def = g_variant_new_int64(115200);
    opts[0].values = NULL;

    xxx_c_decoder.options = opts;
    return &xxx_c_decoder;
}
```

---

## 五、注意事项

### 5.1 不要使用 C++ 特性

C 解码器必须是纯 C 代码（`.c` 文件），不能使用 C++ 特性：
- ❌ `new`/`delete`，使用 `g_malloc0()`/`g_free()`
- ❌ `class`，使用 `struct`
- ❌ `std::string`，使用 `char[]`/`snprintf()`
- ❌ `// 注释`，使用 `/* 注释 */`（虽然 C99 支持 `//`，但项目风格用 `/* */`）

### 5.2 注解行分类数组必须以 -1 结尾

```c
static const int row_classes[] = {ANN_XXX, ANN_YYY, -1};  /* ✅ 正确 */
static const int row_classes[] = {ANN_XXX, ANN_YYY};       /* ❌ 缺少 -1 */
```

### 5.3 字符串数组必须以 NULL 结尾

```c
static const char *inputs[] = {"logic", NULL};   /* ✅ 正确 */
static const char *inputs[] = {"logic"};          /* ❌ 缺少 NULL */
```

### 5.4 c_cond_wait 只能调用一次

```c
srd_cond_builder *cb = c_cond_new();
c_cond_rise(cb, 0);
c_cond_wait(cb, di, &sn, &m);  /* ✅ 第一次调用 */
c_cond_wait(cb, di, &sn, &m);  /* ❌ 第二次调用会失败 */
c_cond_free(cb);
```

每次等待后必须 `c_cond_free()` 并重新 `c_cond_new()`。

### 5.5 decode() 函数是无限循环

`decode()` 函数在 `while(1)` 中运行，框架会在适当时机终止。当 `c_cond_wait()` 返回非 `SRD_OK` 时，应立即 `return`。

### 5.6 采样点编号是绝对值

`samplenum` 是绝对采样点编号，不是相对偏移。`c_cond_skip()` 的参数是相对当前采样点的偏移量。

### 5.7 通道索引

通道索引按定义顺序：`channels[0]` = 索引0，`channels[1]` = 索引1，`optional_channels[0]` = 索引 `num_channels`，以此类推。

---

## 六、已完成的 C 解码器清单

### 完整协议实现（14个）

| 解码器 | 文件 | 协议 | 通道数 |
|--------|------|------|--------|
| spi_c | spi_c.c | SPI | 4 (SCLK, MOSI, MISO, CS) |
| i2c_c | i2c.c | I2C | 2 (SCL, SDA) |
| uart_c | uart_c.c | UART | 2 opt (RX, TX) |
| can_c | can_c.c | CAN 2.0 | 1 (CAN_RX) |
| jtag_c | jtag_c.c | JTAG | 4+3 opt (TCK,TMS,TDI,TDO) |
| swd_c | swd_c.c | SWD | 2 (SWCLK, SWDIO) |
| onewire_c | onewire_c.c | 1-Wire | 1 (OWR) |
| i2s_c | i2s_c.c | I2S | 3 (SCLK, WS, SD) |
| lin_c | lin_c.c | LIN | 1 (LIN) |
| hdlc_c | hdlc_c.c | HDLC | 1 (DATA) |
| microwire_c | microwire_c.c | Microwire | 4 (CS, SK, SI, SO) |
| mdio_c | mdio_c.c | MDIO | 2 (MDC, MDIO) |
| ps2_c | ps2_c.c | PS/2 | 2 (CLK, DATA) |
| dmx512_c | dmx512_c.c | DMX512 | 1 (DATA) |

### 本轮新增（10个）

| 解码器 | 文件 | 协议 | 通道数 |
|--------|------|------|--------|
| nrzi_c | nrzi_c.c | NRZ-I 编码 | 1 (DATA) |
| ir_nec_c | ir_nec_c.c | NEC 红外 | 1 (IR) |
| dcf77_c | dcf77_c.c | DCF77 时钟 | 1 (DATA) |
| cec_c | cec_c.c | HDMI CEC | 1 (CEC) |
| spdif_c | spdif_c.c | S/PDIF | 1 (DATA) |
| usb_signalling_c | usb_signalling_c.c | USB LS/FS | 2 (DP, DM) |
| 4b5b_c | 4b5b_c.c | 4B/5B+NRZI | 1 (DATA) |
| can_fd_c | can_fd_c.c | CAN-FD | 1 (CAN_RX) |
| iso7816_c | iso7816_c.c | ISO 7816 | 2 (CLK, DATA) |
| lpc_c | lpc_c.c | LPC | 6+7 opt (LFRAME,LCLK,LAD[0-3]) |

### Stub 解码器（8个）

pwm_c, counter_c, graycode_c, numbers_and_state_c, seven_segment_c, ds1307_c, ds3231_c, lm75_c

---

## 七、待重写的 Python 解码器优先级

### 第一优先级：`inputs=['logic']` 底层协议（性能收益最大）

| 优先级 | 解码器 | 协议 | 复杂度 |
|--------|--------|------|--------|
| ★★★★ | usb_power_delivery | USB PD | 高 |
| ★★★★ | ethernet | 以太网 | 高（依赖4b5b） |
| ★★★★ | flexray | FlexRay | 高 |
| ★★★★ | z80 | Z80 CPU | 高 |
| ★★★ | lpc | ✅ 已完成 | - |
| ★★★ | spdif | ✅ 已完成 | - |
| ★★★ | cec | ✅ 已完成 | - |
| ★★★ | iso7816 | ✅ 已完成 | - |
| ★★★ | spacewire | SpaceWire | 中 |
| ★★★ | iebus | IEBus | 中 |
| ★★★ | sdcard_sd | SD卡(SD模式) | 中 |
| ★★★ | qspi | QSPI | 中 |
| ★★★ | ac97 | AC97 | 中 |
| ★★★ | tmc | TMC步进驱动 | 中 |
| ★★★ | sent | SENT传感器 | 中 |
| ★★★ | mipi_rffe | MIPI RFFE | 中 |
| ★★★ | avr_pdi | AVR PDI | 中 |
| ★★★ | fsi | FSI | 中 |
| ★★★ | gpib | GPIB | 中 |
| ★★ | parallel | 并口 | 低 |
| ★★ | dali | DALI照明 | 低 |
| ★★ | dcc | DCC模型火车 | 低 |
| ★★ | wiegand | Wiegand门禁 | 低 |
| ★★ | c2 | C2协议 | 低 |
| ★★ | swim | SWIM | 低 |
| ★★ | rgb_led_ws281x | WS281x LED | 低 |
| ★★ | ir_rc5 | RC5红外 | 低 |
| ★★ | ir_sirc | SIRC红外 | 低 |
| ★★ | opentherm | OpenTherm | 低 |
| ★★ | dcf77 | ✅ 已完成 | - |
| ★★ | ir_nec | ✅ 已完成 | - |
| ★★ | nrzi | ✅ 已完成 | - |

### 第二优先级：上层解码器（依赖其他解码器输出）

| 解码器 | 依赖 | 备注 |
|--------|------|------|
| eeprom93xx | microwire | ✅ microwire_c 已完成 |
| spiflash | spi | ✅ spi_c 已完成 |
| eeprom24xx | i2c | ✅ i2c_c 已完成 |
| i2c_packet | i2c | ✅ i2c_c 已完成 |
| ps2_keyboard | ps2 | ✅ ps2_c 已完成 |
| ps2_mouse | ps2 | ✅ ps2_c 已完成 |
| midi | uart | ✅ uart_c 已完成 |
| modbus | uart | ✅ uart_c 已完成 |
| usb_packet | usb_signalling | ✅ usb_signalling_c 已完成 |
| jtag_avr/stm32/ejtag | jtag | ✅ jtag_c 已完成 |

**注意**：上层解码器需要 C 解码器支持堆叠（stacked）模式，目前架构尚未完全支持。上层解码器暂时仍使用 Python 实现。

---

## 八、架构说明

### 8.1 运行时函数指针表（srd_decoder_runtime）

C 解码器 DLL 不直接访问 `srd_decoder_inst` 的内部字段，而是通过 `di->runtime` 函数指针表调用：

```c
struct srd_decoder_runtime {
    int (*wait)(struct srd_decoder_inst *di, GSList *cond, uint64_t *sn, uint64_t *m);
    uint8_t (*get_pin)(struct srd_decoder_inst *di, int ch, uint64_t sn);
    void *(*get_private)(struct srd_decoder_inst *di);
    void (*set_private)(struct srd_decoder_inst *di, void *data);
};
```

DLL 中的 `c_decoder_wait()`、`c_decoder_get_pin()` 等函数内部委托到 `di->runtime` 的对应函数指针。这样：
- DLL 不需要知道主程序的内部数据结构
- 主程序可以自由修改内部实现而不影响 DLL 兼容性
- API 版本通过 `SRD_C_DECODER_API_VERSION` 控制

### 8.2 DLL 加载流程

1. 主程序扫描 `c_decoders` 目录
2. 对每个 DLL 调用 `LoadLibrary()` / `dlopen()`
3. 获取 `srd_c_decoder_entry` 和 `srd_c_decoder_api_version` 符号
4. 检查 API 版本是否兼容
5. 调用 `srd_c_decoder_entry()` 获取 `srd_c_decoder` 结构体
6. 注册到解码器列表

### 8.3 解码执行流程

1. 用户选择 C 解码器并配置通道/选项
2. 主程序调用 `reset()` → `start()` → `metadata()` → `decode()`
3. `decode()` 在独立线程中运行 `while(1)` 循环
4. 每次循环通过 `c_cond_wait()` 等待条件
5. 条件匹配后读取引脚值、处理逻辑、输出注解
6. 当数据流结束，`c_cond_wait()` 返回非 `SRD_OK`，`decode()` 退出
7. 主程序调用 `destroy()` 释放资源
