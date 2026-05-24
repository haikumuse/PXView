# Python→C 解码器移植规格书 — Batch 29

## 参考实现

本批次解码器应参考以下标准范本实现：

| 范本文件 | 角色 | 参考内容 |
|---------|------|---------|
| uart_c.c | 底层协议输出范本 | c_decoder_put_python()输出协议数据、双通道(RX/TX)独立状态 |
| lm75_c.c | 上层recv_proto范本 | recv_proto回调实现、I2C上层解码器状态机 |
| ds1307_c.c | 上层recv_proto范本 | 复杂recv_proto状态机、START REPEAT处理、RTC寄存器解析 |

## 概述

本文档定义将 5 个 UART 上层 Python 解码器移植为 C 解码器的完整规格。所有解码器输入均为 `uart`，采用 `recv_proto()` 回调模式接收 UART 下层数据，而非 `decode()` 采样循环。

### 移植目标解码器

| # | Python ID | C ID | 名称 | 复杂度 | 输出协议 |
|---|-----------|------|------|--------|----------|
| 1 | `arm_itm` | `arm_itm_c` | ARM ITM | ★★★★★ | 无 |
| 2 | `arm_tpiu` | `arm_tpiu_c` | ARM TPIU | ★★★★ | `uart` |
| 3 | `bluetooth_h4` | `bluetooth_h4_c` | Bluetooth H4 | ★★★ | `bluetooth_h4` |
| 4 | `boost` | `boost_c` | LEGO Boost | ★★★ | `boost` |
| 5 | `crsf` | `crsf_c` | CRSF | ★★ | 无 |

---

## 通用架构模式

### recv_proto 回调机制

UART 上层解码器**不实现 `decode()` 函数**（函数体为 `(void)di;`），而是通过 `recv_proto` 回调接收下层数据：

```c
// recv_proto 函数签名
static void xxx_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len);
```

**UART C 解码器发出的 proto 命令：**

| cmd 字符串 | data 内容 | 说明 |
|-----------|----------|------|
| `"DATA"` | data[0]=字节值, data[1]=rxtx(0=RX,1=TX) | UART 数据字节 |
| `"FRAME"` | data[0]=字节值, data[1]=rxtx, data[2]=frame_valid | 完整帧 |
| `"STARTBIT"` | data[0]=start_bit值 | 起始位 |
| `"STOPBIT"` | data[0]=stop_bit值 | 停止位 |
| `"PARITYBIT"` | data[0]=parity_bit值 | 校验位 |
| `"INVALID STARTBIT"` | data[0]=实际值 | 无效起始位 |
| `"INVALID STOPBIT"` | data[0]=实际值 | 无效停止位 |
| `"PARITY ERROR"` | data[0]=期望值, data[1]=实际值 | 校验错误 |
| `"IDLE"` | data[0]=rxtx(0=RX,1=TX) | 空闲检测 <!-- Updated: IDLE已由uart_c.c实现(handle_idle函数)，原spec遗漏此命令 --> |
| `"BREAK"` | data[0]=rxtx(0=RX,1=TX) | Break条件 <!-- Updated: BREAK已由uart_c.c实现(handle_break函数)，原spec遗漏此命令 --> |

**关键：** 上层解码器通常只处理 `cmd=="DATA"` 的消息，忽略其他类型。但 Modbus 等解码器需要 `STARTBIT`/`STOPBIT` 来计算 `bitlength`。 <!-- Updated: 补充说明部分解码器需要非DATA命令 -->

### rxtx 方向常量

```c
#define RX 0
#define TX 1
```

### 标准文件结构

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "libsigrokdecode.h"

// 1. 枚举定义（状态、注解类）
// 2. 私有状态结构体
// 3. 通道/选项/注解标签/注解行 静态数组
// 4. 输入/输出/标签 静态数组
// 5. 辅助函数
// 6. recv_proto 实现
// 7. reset/start/decode/destroy 生命周期函数
// 8. srd_c_decoder 结构体实例
// 9. srd_c_decoder_entry() 导出函数
// 10. srd_c_decoder_api_version() 导出函数
```

### C 解码器命名规范

- 文件名：`{decoder_id}_c.c`（如 `arm_itm_c.c`）
- 结构体 id：`"arm_itm_c"`
- 结构体 name：`"ARM ITM(C)"`
- 结构体 longname：`"ARM Instrumentation Trace Macroblock (C)"`
- 结构体 desc：`"... (C implementation, faster than Python)"`
- 注解标签第一列：`""`（空字符串）

---

## 解码器 1：arm_itm_c

### 1.1 Python 元数据提取

| 属性 | Python 值 | C 映射 |
|------|----------|--------|
| id | `arm_itm` | `arm_itm_c` |
| name | `ARM ITM` | `ARM ITM(C)` |
| longname | `ARM Instrumentation Trace Macroblock` | `ARM Instrumentation Trace Macroblock (C)` |
| desc | `ARM Cortex-M / ARMv7m ITM trace protocol.` | `ARM Cortex-M ITM trace protocol. (C implementation, faster than Python)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['uart']` | `{"uart", NULL}` |
| outputs | `[]` | `NULL, 0` |
| tags | `['Debug/trace']` | `{"Debug/trace", NULL}` |

### 1.2 注解映射

Python 12 个注解类 → C 12 个注解类：

```c
enum arm_itm_ann {
    ANN_TRACE = 0,        // 'trace' - Trace information
    ANN_TIMESTAMP,        // 'timestamp' - Timestamp
    ANN_SOFTWARE,         // 'software' - Software message
    ANN_DWT_EVENT,        // 'dwt_event' - DWT event
    ANN_DWT_WATCHPOINT,   // 'dwt_watchpoint' - DWT watchpoint
    ANN_DWT_EXC,          // 'dwt_exc' - Exception trace
    ANN_DWT_PC,           // 'dwt_pc' - Program counter
    ANN_MODE_THREAD,      // 'mode_thread' - Current mode: thread
    ANN_MODE_IRQ,         // 'mode_irq' - Current mode: IRQ
    ANN_MODE_EXC,         // 'mode_exc' - Current mode: Exception
    ANN_LOCATION,         // 'location' - Current location
    ANN_FUNCTION,         // 'function' - Current function
    NUM_ANN,
};
```

注解标签：

```c
static const char *arm_itm_ann_labels[][3] = {
    {"", "trace", "Trace information"},
    {"", "timestamp", "Timestamp"},
    {"", "software", "Software message"},
    {"", "dwt-event", "DWT event"},
    {"", "dwt-watchpoint", "DWT watchpoint"},
    {"", "dwt-exc", "Exception trace"},
    {"", "dwt-pc", "Program counter"},
    {"", "mode-thread", "Current mode: thread"},
    {"", "mode-irq", "Current mode: IRQ"},
    {"", "mode-exc", "Current mode: Exception"},
    {"", "location", "Current location"},
    {"", "function", "Current function"},
};
```

注解行（9 行）：

```c
static const int arm_itm_row_trace_classes[] = {ANN_TRACE, ANN_TIMESTAMP, -1};
static const int arm_itm_row_software_classes[] = {ANN_SOFTWARE, -1};
static const int arm_itm_row_dwt_event_classes[] = {ANN_DWT_EVENT, -1};
static const int arm_itm_row_dwt_wp_classes[] = {ANN_DWT_WATCHPOINT, -1};
static const int arm_itm_row_dwt_exc_classes[] = {ANN_DWT_EXC, -1};
static const int arm_itm_row_dwt_pc_classes[] = {ANN_DWT_PC, -1};
static const int arm_itm_row_mode_classes[] = {ANN_MODE_THREAD, ANN_MODE_IRQ, ANN_MODE_EXC, -1};
static const int arm_itm_row_location_classes[] = {ANN_LOCATION, -1};
static const int arm_itm_row_function_classes[] = {ANN_FUNCTION, -1};

static const struct srd_c_ann_row arm_itm_ann_rows[] = {
    {"trace", "Trace information", arm_itm_row_trace_classes, 2},
    {"software", "Software trace", arm_itm_row_software_classes, 1},
    {"dwt-event", "DWT event", arm_itm_row_dwt_event_classes, 1},
    {"dwt-watchpoint", "DWT watchpoint", arm_itm_row_dwt_wp_classes, 1},
    {"dwt-exc", "Exception trace", arm_itm_row_dwt_exc_classes, 1},
    {"dwt-pc", "Program counter", arm_itm_row_dwt_pc_classes, 1},
    {"mode", "Current mode", arm_itm_row_mode_classes, 3},
    {"location", "Current location", arm_itm_row_location_classes, 1},
    {"function", "Current function", arm_itm_row_function_classes, 1},
};
```

### 1.3 选项

Python 有 3 个选项（objdump, objdump_opts, elffile），用于调用外部 objdump 工具解析 ELF 文件获取 PC→函数/文件映射。**C 版本不支持 subprocess 调用，因此这 3 个选项全部省略。** C 版本将 `ANN_LOCATION` 和 `ANN_FUNCTION` 注解类保留但不会输出（因为没有 lookup table）。

**C 选项数量：0**

### 1.4 状态机分析

Python `decode()` 方法逐字节接收 UART DATA，维护 `self.buf` 缓冲区，根据首字节判断包类型：

```
get_packet_type(byte):
  - byte & 0x7F == 0        → 'sync'
  - byte == 0x70             → 'overflow'
  - byte & 0x0F == 0 且 byte & 0xF0 != 0  → 'timestamp'
  - byte & 0x0F == 0x08      → 'sw_extension'
  - byte & 0x0F == 0x0C      → 'hw_extension'
  - byte & 0x0F == 0x04      → 'reserved'
  - byte & 0x04 == 0x00      → 'software'
  - else                     → 'hardware'
```

**同步检测：** 维护 `syncbuf`，当最后 6 字节为 `[0, 0, 0, 0, 0, 0x80]` 时强制同步。

**超时重置：** 当字节间隔 > 16 × byte_len 时重置 buf。

### 1.5 包类型处理

| 包类型 | 长度 | 处理逻辑 | 注解类 |
|--------|------|----------|--------|
| sync | 6 字节 (0x00×5 + 0x80) | 仅同步，无输出 | - |
| overflow | 1 字节 (0x70) | 输出 "Overflow" | ANN_TRACE |
| timestamp | 1~5 字节 | 解析 TC/TS，累加时间戳 | ANN_TIMESTAMP |
| software | 1+plen 字节 | plen=(0,1,2,4)[buf[0]&0x03]，pid=buf[0]>>3 | ANN_SOFTWARE |
| hardware | 1+plen 字节 | 多种子类型 (DWT event/exception/PC/watchpoint) | ANN_DWT_* |
| sw_extension | - | 保留 | ANN_TRACE |
| hw_extension | - | 保留 | ANN_TRACE |
| reserved | - | 保留 | ANN_TRACE |

### 1.6 私有状态结构体

```c
typedef struct {
    // 包缓冲区
    uint8_t buf[8];          // 当前包字节缓冲
    int buf_len;             // 缓冲区长度
    uint8_t syncbuf[6];      // 同步检测缓冲
    int syncbuf_len;         // 同步缓冲长度

    // 采样位置追踪
    uint64_t startsample;    // 当前包起始采样
    uint64_t prevsample;     // 上一个字节结束采样
    uint64_t byte_len;       // 一个字节的采样宽度

    // 软件包延迟输出（可打印字符拼接）
    // 简化：C 版本不做字符拼接，每个 software 包直接输出
    // 如果需要完整移植，可添加：
    // char sw_text[2][256];   // RX/TX 各一个 PID→字符串映射
    // uint64_t sw_ss[2][64];  // PID→起始采样
    // 但 PID 范围 0~31，完整映射需要数组

    // DWT 时间戳
    uint64_t dwt_timestamp;

    // 当前模式追踪
    int current_mode;         // -1=none, 0=thread, 1=IRQ, 2=exception
    uint64_t mode_start;      // 模式起始采样
    char mode_name[64];       // 模式名称

    // 输出注册
    int out_ann;
} arm_itm_state;
```

### 1.7 recv_proto 实现要点

```c
static void arm_itm_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    arm_itm_state *s = (arm_itm_state *)c_decoder_get_private(di);
    if (!s) return;

    // 只处理 DATA 命令
    if (strcmp(cmd, "DATA") != 0) return;

    uint8_t byte_val = (data && data_len > 0) ? data[0] : 0;
    // 注意：data[1] 是 rxtx，但 ARM ITM 通常只用 RX 方向
    // Python 版本忽略了 rxtx，C 版本同样忽略

    s->byte_len = end_sample - start_sample;

    // 超时重置
    if (start_sample - s->prevsample > 16 * s->byte_len && s->prevsample > 0) {
        s->buf_len = 0;
    }
    s->prevsample = end_sample;

    // 追加字节到缓冲区
    if (s->buf_len < 8) {
        s->buf[s->buf_len++] = byte_val;
    }

    // 记录包起始
    if (s->buf_len == 1) {
        s->startsample = start_sample;
    }

    // 同步检测
    memmove(s->syncbuf, s->syncbuf + 1, 5);
    s->syncbuf[5] = byte_val;
    if (s->syncbuf[0]==0 && s->syncbuf[1]==0 && s->syncbuf[2]==0 &&
        s->syncbuf[3]==0 && s->syncbuf[4]==0 && s->syncbuf[5]==0x80) {
        // 强制同步
        memcpy(s->buf, s->syncbuf, 6);
        s->buf_len = 6;
        s->startsample = start_sample - 5 * s->byte_len; // 近似
    }

    // 判断包类型并尝试解码
    int ptype = arm_itm_get_packet_type(s->buf[0]);
    int handled = 0;

    switch (ptype) {
    case PKT_SYNC:
        handled = arm_itm_handle_sync(di, s, end_sample);
        break;
    case PKT_OVERFLOW:
        handled = arm_itm_handle_overflow(di, s, end_sample);
        break;
    case PKT_TIMESTAMP:
        handled = arm_itm_handle_timestamp(di, s, end_sample);
        break;
    case PKT_SOFTWARE:
        handled = arm_itm_handle_software(di, s, end_sample);
        break;
    case PKT_HARDWARE:
        handled = arm_itm_handle_hardware(di, s, end_sample);
        break;
    // ... 其他类型
    }

    if (handled) {
        s->buf_len = 0;
    }
}
```

### 1.8 ARM 异常名称映射

```c
static const char *arm_exceptions[] = {
    "Thread",    // 0
    "Reset",     // 1
    "NMI",       // 2
    "HardFault", // 3
    "MemManage", // 4
    "BusFault",  // 5
    "UsageFault",// 6
    NULL, NULL, NULL, NULL, // 7-10
    "SVCall",    // 11
    "Debug Monitor", // 12
    NULL,        // 13
    "PendSV",    // 14
    "SysTick",   // 15
};

static const char *arm_itm_get_exception_name(int excnum) {
    if (excnum >= 0 && excnum <= 15 && arm_exceptions[excnum])
        return arm_exceptions[excnum];
    static char buf[32];
    snprintf(buf, sizeof(buf), "IRQ %d", excnum - 16);
    return buf;
}
```

### 1.9 关键简化决策

| Python 特性 | C 处理方式 | 原因 |
|-------------|-----------|------|
| `subprocess` 调用 objdump | **省略** | C 解码器不能调用外部进程 |
| ELF 文件解析/PC→函数映射 | **省略** | 依赖 objdump 输出 |
| `ANN_LOCATION`/`ANN_FUNCTION` | 保留枚举但不输出 | 结构完整性 |
| 可打印字符拼接 (`add_delayed_sw`) | **简化**：每个 software 包直接输出 | 避免复杂缓冲管理 |
| `swpackets` 字典 | **省略** | 简化实现 |
| 3 个选项 | **省略** | 依赖 objdump |

### 1.10 srd_c_decoder 结构体

```c
struct srd_c_decoder arm_itm_c_decoder = {
    .id = "arm_itm_c",
    .name = "ARM ITM(C)",
    .longname = "ARM Instrumentation Trace Macroblock (C)",
    .desc = "ARM Cortex-M ITM trace protocol. (C implementation, faster than Python)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = arm_itm_ann_labels,
    .num_annotation_rows = 9,
    .annotation_rows = arm_itm_ann_rows,
    .inputs = arm_itm_inputs,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = arm_itm_tags,
    .num_tags = 1,
    .reset = arm_itm_reset,
    .start = arm_itm_start,
    .decode = arm_itm_decode,
    .destroy = arm_itm_destroy,
    .recv_proto = arm_itm_recv_proto,
};
```

---

## 解码器 2：arm_tpiu_c

### 2.1 Python 元数据提取

| 属性 | Python 值 | C 映射 |
|------|----------|--------|
| id | `arm_tpiu` | `arm_tpiu_c` |
| name | `ARM TPIU` | `ARM TPIU(C)` |
| longname | `ARM Trace Port Interface Unit` | `ARM Trace Port Interface Unit (C)` |
| desc | `Filter TPIU formatted trace data into separate streams.` | `Filter TPIU formatted trace data into separate streams. (C implementation, faster than Python)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['uart']` | `{"uart", NULL}` |
| outputs | `['uart']` | `{"uart", NULL}` |
| tags | `['Debug/trace']` | `{"Debug/trace", NULL}` |

### 2.2 注解映射

```c
enum arm_tpiu_ann {
    ANN_STREAM = 0,  // 'stream' - Current stream
    ANN_DATA,        // 'data' - Stream data
    NUM_ANN,
};

static const char *arm_tpiu_ann_labels[][3] = {
    {"", "stream", "Current stream"},
    {"", "data", "Stream data"},
};

static const int arm_tpiu_row_stream_classes[] = {ANN_STREAM, -1};
static const int arm_tpiu_row_data_classes[] = {ANN_DATA, -1};

static const struct srd_c_ann_row arm_tpiu_ann_rows[] = {
    {"stream", "Current stream", arm_tpiu_row_stream_classes, 1},
    {"data", "Stream data", arm_tpiu_row_data_classes, 1},
};
```

### 2.3 选项

```c
static struct srd_decoder_option arm_tpiu_options[] = {
    {"stream", NULL, "Stream index(流索引)", NULL, NULL},
    {"sync_offset", NULL, "Initial sync offset(初始同步偏移)", NULL, NULL},
};
```

- `stream`：整数，默认 1，选择要输出的流编号
- `sync_offset`：整数，默认 0，跳过前 N 个字节

### 2.4 状态机分析

ARM TPIU 将 UART 字节流组织为 16 字节帧：

```
帧结构 (16 字节):
  Byte 0, 2, 4, 6, 8, 10, 12, 14  → 奇数位(偶索引)：stream ID 或 data
  Byte 1, 3, 5, 7, 9, 11, 13, 15  → 偶数位(奇索引)：始终是 data
  Byte 15 的低 4 位包含 Byte 0, 2, 4, 6 的最低位
```

**同步检测：** 当 syncbuf 最后 4 字节为 `[0xFF, 0xFF, 0xFF, 0x7F]` 时重置。

**流切换逻辑：**
- 奇数位字节 bit0=1 → stream ID（byte >> 1 为流编号）
- 奇数位字节 bit0=0 → data（byte | lowbit 为数据值）
- stream ID 切换可以延迟到下一个偶数位字节之后

### 2.5 私有状态结构体

```c
typedef struct {
    // 帧缓冲区：每个条目 (ss, es, byte_value)
    uint64_t frame_ss[16];
    uint64_t frame_es[16];
    uint8_t frame_data[16];
    int frame_len;            // 当前帧已收集字节数

    // 同步缓冲
    uint8_t syncbuf[4];
    int syncbuf_len;

    // 采样追踪
    uint64_t prevsample;
    uint64_t byte_len;

    // 流状态
    int current_stream;       // 当前流编号
    uint64_t ss_stream;       // 当前流起始采样
    int bytenum;              // 总字节计数

    // 选项
    int stream_filter;        // 要输出的流编号
    int sync_offset;          // 初始同步偏移

    // 输出注册
    int out_ann;
    int out_python;
} arm_tpiu_state;
```

### 2.6 recv_proto 实现要点

```c
static void arm_tpiu_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    arm_tpiu_state *s = (arm_tpiu_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") != 0) return;

    uint8_t byte_val = (data && data_len > 0) ? data[0] : 0;

    s->byte_len = end_sample - start_sample;

    // 超时重置
    if (start_sample - s->prevsample > s->byte_len && s->prevsample > 0) {
        s->frame_len = 0;
    }
    s->prevsample = end_sample;

    // 存储帧字节
    if (s->frame_len < 16) {
        s->frame_ss[s->frame_len] = start_sample;
        s->frame_es[s->frame_len] = end_sample;
        s->frame_data[s->frame_len] = byte_val;
        s->frame_len++;
    }
    s->bytenum++;

    // 同步偏移
    if (s->bytenum < s->sync_offset) {
        s->frame_len = 0;
        return;
    }

    // 同步检测
    memmove(s->syncbuf, s->syncbuf + 1, 3);
    s->syncbuf[3] = byte_val;
    if (s->syncbuf[0]==0xFF && s->syncbuf[1]==0xFF &&
        s->syncbuf[2]==0xFF && s->syncbuf[3]==0x7F) {
        s->frame_len = 0;
        return;
    }

    // 帧完成
    if (s->frame_len == 16) {
        arm_tpiu_process_frame(di, s);
        s->frame_len = 0;
    }
}
```

### 2.7 帧处理核心逻辑

```c
static void arm_tpiu_process_frame(struct srd_decoder_inst *di, arm_tpiu_state *s)
{
    uint8_t lowbits = s->frame_data[15];

    for (int i = 0; i < 15; i += 2) {
        int delayed_stream_change = -1;
        int lowbit = (lowbits >> (i / 2)) & 0x01;

        // 奇数位字节（偶索引 i）
        if (s->frame_data[i] & 0x01) {
            // Stream ID
            if (lowbit) {
                delayed_stream_change = s->frame_data[i] >> 1;
            } else {
                arm_tpiu_stream_changed(di, s, s->frame_ss[i], s->frame_data[i] >> 1);
            }
        } else {
            // Data byte
            uint8_t byte_out = s->frame_data[i] | lowbit;
            arm_tpiu_emit_byte(di, s, s->frame_ss[i], s->frame_es[i], byte_out);
        }

        // 偶数位字节（奇索引 i+1）
        if (i < 14) {
            arm_tpiu_emit_byte(di, s, s->frame_ss[i+1], s->frame_es[i+1], s->frame_data[i+1]);
        }

        // 延迟流切换
        if (delayed_stream_change >= 0) {
            arm_tpiu_stream_changed(di, s, s->frame_es[i+1], delayed_stream_change);
        }
    }
}
```

### 2.8 输出转发

arm_tpiu 需要输出 `uart` 协议数据，以便 arm_itm 可以堆叠在其上方：

<!-- Updated: Python版本使用self.put(ss, es, self.out_python, ['DATA', 0, (byte, [])])格式，
     C版本使用c_decoder_put_python(di, ss, es, out_python, "DATA", py_data, 2)格式，
     其中py_data[0]=byte_val, py_data[1]=0(rxtx=RX)，与uart_c的DATA输出格式一致 -->

```c
static void arm_tpiu_emit_byte(struct srd_decoder_inst *di, arm_tpiu_state *s,
    uint64_t ss, uint64_t es, uint8_t byte_val)
{
    if (s->current_stream != s->stream_filter) return;

    // 注解输出
    char val_str[8];
    snprintf(val_str, sizeof(val_str), "0x%02x", byte_val);
    C_ANN_PUT(di, ss, es, s->out_ann, ANN_DATA, val_str);

    // Python 输出（模拟 UART DATA 格式）
    unsigned char py_data[2];
    py_data[0] = byte_val;
    py_data[1] = 0; // rxtx=0 (RX)
    c_decoder_put_python(di, ss, es, s->out_python, "DATA", py_data, 2);
}
```

### 2.9 srd_c_decoder 结构体

```c
struct srd_c_decoder arm_tpiu_c_decoder = {
    .id = "arm_tpiu_c",
    .name = "ARM TPIU(C)",
    .longname = "ARM Trace Port Interface Unit (C)",
    .desc = "Filter TPIU formatted trace data into separate streams. (C implementation, faster than Python)",
    .license = "gplv2+",
    .channels = NULL,
    .num_channels = 0,
    .optional_channels = NULL,
    .num_optional_channels = 0,
    .options = arm_tpiu_options,
    .num_options = 2,
    .num_annotations = NUM_ANN,
    .ann_labels = arm_tpiu_ann_labels,
    .num_annotation_rows = 2,
    .annotation_rows = arm_tpiu_ann_rows,
    .inputs = arm_tpiu_inputs,
    .num_inputs = 1,
    .outputs = arm_tpiu_outputs,
    .num_outputs = 1,
    .binary = NULL,
    .num_binary = 0,
    .tags = arm_tpiu_tags,
    .num_tags = 1,
    .reset = arm_tpiu_reset,
    .start = arm_tpiu_start,
    .decode = arm_tpiu_decode,
    .destroy = arm_tpiu_destroy,
    .recv_proto = arm_tpiu_recv_proto,
};
```

### 2.10 srd_c_decoder_entry 实现

```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    arm_tpiu_options[0].def = g_variant_new_int64(1);
    arm_tpiu_options[1].def = g_variant_new_int64(0);
    return &arm_tpiu_c_decoder;
}
```

---

## 解码器 3：bluetooth_h4_c

### 3.1 Python 元数据提取

| 属性 | Python 值 | C 映射 |
|------|----------|--------|
| id | `bluetooth_h4` | `bluetooth_h4_c` |
| name | `bluetooth_h4` | `Bluetooth H4(C)` |
| longname | `Blueetooth H4 packet decoder` | `Bluetooth H4 packet decoder (C)` |
| desc | `Bluetooth H4 packet decoder.` | `Bluetooth H4 packet decoder. (C implementation, faster than Python)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['uart']` | `{"uart", NULL}` |
| outputs | `['bluetooth_h4']` | `{"bluetooth_h4", NULL}` |
| tags | `['Embedded/bluetooth']` | `{"Embedded/bluetooth", NULL}` |

### 3.2 注解映射

Python 18 个注解类（9 RX + 9 TX）→ C 18 个注解类：

```c
enum bt_h4_ann {
    // RX (rxtx=0)
    ANN_RX_CMD = 0,     // 'rx-cmd' - RX Command packet
    ANN_RX_ACL,         // 'rx-acl' - RX ACL data packet
    ANN_RX_SCO,         // 'rx-sco' - RX SCO data packet
    ANN_RX_EVENT,       // 'rx-event' - RX Event data packet
    ANN_RX_ERROR,       // 'rx-error' - RX Error message packet
    ANN_RX_NEGO,        // 'rx-nego' - RX Negotiation packet
    ANN_RX_JUNK,        // 'rx-junk' - RX Garbages
    ANN_RX_DESC,        // 'rx-desc' - RX packet description
    ANN_RX_BIN,         // 'rx-bin' - RX packet binary

    // TX (rxtx=1)
    ANN_TX_CMD,         // 'tx-cmd' - TX Command packet
    ANN_TX_ACL,         // 'tx-acl' - TX ACL data packet
    ANN_TX_SCO,         // 'tx-sco' - TX SCO data packet
    ANN_TX_EVENT,       // 'tx-event' - TX Event data packet
    ANN_TX_ERROR,       // 'tx-error' - TX Error message packet
    ANN_TX_NEGO,        // 'tx-nego' - TX Negotiation packet
    ANN_TX_JUNK,        // 'tx-junk' - TX Garbages
    ANN_TX_DESC,        // 'tx-desc' - TX packet description
    ANN_TX_BIN,         // 'tx-bin' - TX packet binary
    NUM_ANN,
};
```

注解行（4 行）：

```c
static const int bt_h4_row_rx_classes[] = {
    ANN_RX_CMD, ANN_RX_ACL, ANN_RX_SCO, ANN_RX_EVENT,
    ANN_RX_ERROR, ANN_RX_NEGO, ANN_RX_JUNK, -1
};
static const int bt_h4_row_rx_bins_classes[] = {ANN_RX_BIN, -1};
static const int bt_h4_row_tx_classes[] = {
    ANN_TX_CMD, ANN_TX_ACL, ANN_TX_SCO, ANN_TX_EVENT,
    ANN_TX_ERROR, ANN_TX_NEGO, ANN_TX_JUNK, -1
};
static const int bt_h4_row_tx_bins_classes[] = {ANN_TX_BIN, -1};

static const struct srd_c_ann_row bt_h4_ann_rows[] = {
    {"rx", "RX", bt_h4_row_rx_classes, 7},
    {"rx-bins", "RX binary", bt_h4_row_rx_bins_classes, 1},
    {"tx", "TX", bt_h4_row_tx_classes, 7},
    {"tx-bins", "TX binary", bt_h4_row_tx_bins_classes, 1},
};
```

### 3.3 选项

无选项。

### 3.4 状态机分析

Bluetooth H4 协议基于 UART，包格式：

| 包类型 | 指示字节 | 结构 |
|--------|---------|------|
| HCI Command | 0x01 | [0x01][OGF/OCF 2B][Length 1B][Data nB] |
| ACL Data | 0x02 | [0x02][Handle 2B][Length 2B][Data nB] |
| SCO Data | 0x03 | [0x03][Handle 2B][Length 1B][Data nB] |
| HCI Event | 0x04 | [0x04][Event 1B][Length 1B][Data nB] |

**状态机（每个 rxtx 方向独立）：**

```
IDLE → 收到字节
  ├─ 字节 < 0x01 或 > 0x04 → 输出 JUNK，回到 IDLE
  └─ 字节 0x01~0x04 → 记录 ss_block，进入 HEADER

HEADER → 继续接收
  ├─ CMD(0x01): 收到 4 字节后解析长度
  ├─ ACL(0x02): 收到 5 字节后解析长度
  ├─ SCO(0x03): 收到 4 字节后解析长度
  └─ EVENT(0x04): 收到 3 字节后解析长度

PAYLOAD → packet_length 递减
  └─ packet_length == 0 → 输出完整包，回到 IDLE
```

### 3.5 私有状态结构体

```c
#define BT_H4_MAX_PACKET 512

typedef struct {
    // 每个方向独立
    uint8_t datavalues[2][BT_H4_MAX_PACKET];
    int data_len[2];
    uint64_t ss_block[2];
    int packet_length[2];    // -1 表示未确定

    // 输出注册
    int out_ann;
    int out_python;
} bt_h4_state;
```

### 3.6 HCI 命令名称查找表

Python 版本有一个约 80 条的 `hcicmds` 字典。C 版本使用结构体数组：

```c
typedef struct {
    uint16_t opcode;
    const char *name;
} hci_cmd_entry;

static const hci_cmd_entry hci_cmds[] = {
    {0x0000, "NOP"},
    {0x0401, "Inquiry"},
    {0x0402, "Inquiry_Cancel"},
    // ... 完整列表从 Python 提取
    {0x1009, "Read_BD_ADDR"},
    {0xFFFF, NULL}  // 哨兵
};

static const char *hci_cmd_name(uint16_t opcode) {
    for (int i = 0; hci_cmds[i].name; i++) {
        if (hci_cmds[i].opcode == opcode)
            return hci_cmds[i].name;
    }
    return "*UNKNOWN*";
}
```

### 3.7 recv_proto 实现要点

```c
static void bt_h4_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    bt_h4_state *s = (bt_h4_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") != 0) return;

    uint8_t byte_val = (data && data_len > 0) ? data[0] : 0;
    int rxtx = (data_len > 1) ? data[1] : 0;
    if (rxtx < 0 || rxtx > 1) rxtx = 0;

    // 追加字节
    if (s->data_len[rxtx] < BT_H4_MAX_PACKET) {
        s->datavalues[rxtx][s->data_len[rxtx]++] = byte_val;
    }

    if (s->ss_block[rxtx] == 0 && s->packet_length[rxtx] < 0) {
        // IDLE 状态
        if (byte_val < 0x01 || byte_val > 0x04) {
            // Junk
            char junk_str[16];
            snprintf(junk_str, sizeof(junk_str), "%02X ", byte_val);
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann,
                      ANN_RX_JUNK + rxtx * 9, junk_str);
            s->data_len[rxtx] = 0;
        } else {
            s->ss_block[rxtx] = start_sample;
        }
    } else if (s->packet_length[rxtx] < 0) {
        // HEADER 阶段：尝试解析包长度
        bt_h4_try_parse_header(di, s, rxtx, end_sample);
    } else {
        // PAYLOAD 阶段
        s->packet_length[rxtx]--;
        if (s->packet_length[rxtx] == 0) {
            bt_h4_output_packet(di, s, rxtx, end_sample);
            s->data_len[rxtx] = 0;
            s->ss_block[rxtx] = 0;
            s->packet_length[rxtx] = -1;
        }
    }
}
```

### 3.8 包输出逻辑

```c
static void bt_h4_output_packet(struct srd_decoder_inst *di, bt_h4_state *s,
    int rxtx, uint64_t es)
{
    uint8_t *dv = s->datavalues[rxtx];
    int len = s->data_len[rxtx];
    int pkt_type = dv[0];
    int ann_base = rxtx * 9;
    const char *dir = rxtx ? "TX" : "RX";
    char cmd_str[1024];

    switch (pkt_type) {
    case 0x01: { // CMD
        uint16_t opcd = dv[2] * 256 + dv[1];
        const char *name = hci_cmd_name(opcd);
        snprintf(cmd_str, sizeof(cmd_str),
            "%s: CMD=%02X%02X [%s] LEN=%d(%02Xh) D=",
            dir, dv[2], dv[1], name, dv[3], dv[3]);
        // 追加 data hex
        for (int i = 4; i < len; i++) {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "%02X", dv[i]);
            // 注意：Windows无strlcat，使用snprintf拼接替代 <!-- Updated: 修正strlcat为跨平台安全方案 -->
            int cur_len = (int)strlen(cmd_str);
            if (cur_len < (int)sizeof(cmd_str) - 4)
                snprintf(cmd_str + cur_len, sizeof(cmd_str) - cur_len, "%s", tmp);
        }
        C_ANN_PUT(di, s->ss_block[rxtx], es, s->out_ann,
                  ann_base + 0, cmd_str);  // ANN_RX_CMD or ANN_TX_CMD
        break;
    }
    case 0x02: { // ACL
        int pkt_len = dv[3] + dv[4] * 256;
        snprintf(cmd_str, sizeof(cmd_str),
            "%s: ACL H=%02X%02X LEN=%d(%02X%02Xh) D=",
            dir, dv[2], dv[1], pkt_len, dv[4], dv[3]);
        for (int i = 5; i < len; i++) {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "%02X", dv[i]);
            // 注意：Windows无strlcat，使用snprintf拼接替代 <!-- Updated: 修正strlcat为跨平台安全方案 -->
            int cur_len = (int)strlen(cmd_str);
            if (cur_len < (int)sizeof(cmd_str) - 4)
                snprintf(cmd_str + cur_len, sizeof(cmd_str) - cur_len, "%s", tmp);
        }
        C_ANN_PUT(di, s->ss_block[rxtx], es, s->out_ann,
                  ann_base + 1, cmd_str);
        break;
    }
    case 0x03: { // SCO
        snprintf(cmd_str, sizeof(cmd_str),
            "%s: SCO H=%02X%02X LEN=%d(%02Xh) D=",
            dir, dv[2], dv[1], dv[3], dv[3]);
        for (int i = 4; i < len; i++) {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "%02X", dv[i]);
            // 注意：Windows无strlcat，使用snprintf拼接替代 <!-- Updated: 修正strlcat为跨平台安全方案 -->
            int cur_len = (int)strlen(cmd_str);
            if (cur_len < (int)sizeof(cmd_str) - 4)
                snprintf(cmd_str + cur_len, sizeof(cmd_str) - cur_len, "%s", tmp);
        }
        C_ANN_PUT(di, s->ss_block[rxtx], es, s->out_ann,
                  ann_base + 2, cmd_str);
        break;
    }
    case 0x04: { // EVENT
        snprintf(cmd_str, sizeof(cmd_str),
            "%s: EVENT=%02X LEN=%d(%02Xh) D=",
            dir, dv[1], dv[2], dv[2]);
        for (int i = 3; i < len; i++) {
            char tmp[8];
            snprintf(tmp, sizeof(tmp), "%02X", dv[i]);
            // 注意：Windows无strlcat，使用snprintf拼接替代 <!-- Updated: 修正strlcat为跨平台安全方案 -->
            int cur_len = (int)strlen(cmd_str);
            if (cur_len < (int)sizeof(cmd_str) - 4)
                snprintf(cmd_str + cur_len, sizeof(cmd_str) - cur_len, "%s", tmp);
        }
        C_ANN_PUT(di, s->ss_block[rxtx], es, s->out_ann,
                  ann_base + 3, cmd_str);
        break;
    }
    }

    // Python 输出（模拟 bluetooth_h4 Python 协议格式）
    // Python 版本使用: self.put(ss, es, self.out_python, [pkt_type - 1 + rxtx * 9, datavalues])
    // C 版本使用 c_decoder_put_python 传递包类型和数据
    unsigned char py_data[BT_H4_MAX_PACKET + 2];
    py_data[0] = (unsigned char)(pkt_type - 1 + rxtx * 9); // ann_class 映射
    py_data[1] = (unsigned char)len;
    memcpy(py_data + 2, dv, len);
    c_decoder_put_python(di, s->ss_block[rxtx], es, s->out_python,
        "PACKET", py_data, 2 + len);
}
```

---

## 解码器 4：boost_c

### 4.1 Python 元数据提取

| 属性 | Python 值 | C 映射 |
|------|----------|--------|
| id | `boost` | `boost_c` |
| name | `Boost` | `Boost(C)` |
| longname | `LEGO Boost` | `LEGO Boost (C)` |
| desc | `LEGO Boost Hub and Peripherals.` | `LEGO Boost Hub and Peripherals. (C implementation, faster than Python)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['uart']` | `{"uart", NULL}` |
| outputs | `['boost']` | `{"boost", NULL}` |
| tags | `['Embedded/industrial']` | `{"Embedded/industrial", NULL}` |

### 4.2 注解映射

```c
enum boost_ann {
    ANN_MESSAGE = 0,  // 'message' - Valid messages that pass checksum
    ANN_ERROR,        // 'error' - Invalid/malformed messages
    ANN_BYTES,        // 'byte' - Each individual byte
    NUM_ANN,
};

static const char *boost_ann_labels[][3] = {
    {"", "message", "Valid messages that pass checksum"},
    {"", "error", "Invalid/malformed messages"},
    {"", "byte", "Each individual byte"},
};

static const int boost_row_messages_classes[] = {ANN_MESSAGE, -1};
static const int boost_row_errors_classes[] = {ANN_ERROR, -1};
static const int boost_row_bytes_classes[] = {ANN_BYTES, -1};

static const struct srd_c_ann_row boost_ann_rows[] = {
    {"messages", "Messages", boost_row_messages_classes, 1},
    {"errors", "Errors", boost_row_errors_classes, 1},
    {"bytes", "Bytes", boost_row_bytes_classes, 1},
};
```

### 4.3 选项

```c
static struct srd_decoder_option boost_options[] = {
    {"show_errors", NULL, "Show errors?(显示错误)", NULL, NULL},
    {"show_bytes", NULL, "Show message bytes?(显示消息字节)", NULL, NULL},
};
```

### 4.4 状态机分析

LEGO Boost 协议基于 UART，消息格式：

- 每条消息以消息类型字节（首字节）开始
- 消息长度由类型决定（通过 `expectedLength` 装饰器）
- 校验和：XOR 所有字节，结果应为 0

**Python 的 handlers.py 定义了约 15 个消息处理器：**

| 消息类型 | 长度 | 说明 | 校验 |
|---------|------|------|------|
| 0x46 | 3 | C/D Sensor Mode | ✓ |
| 0xC1 | 3 | Distance (mode 01) | ✓ |
| 0xD2 | 6 | Mode 02 response | ✓ |
| 0xC3 | 3 | CD Mode 03 | ✓ |
| 0xC4 | 3 | CD Mode 04 | ✓ |
| 0xC5 | 3 | CD Mode 05 | ✓ |
| 0xCF | 3 | CD Mode 07 | ✓ |
| 0xE2 | 15 | CD Mode 0A | ✓ |
| 0xD1 | 6 | Luminosity (mode 09) | ✓ |
| 0xDE | 10 | RGB (mode 06) | ✓ |
| 0x43 | 4 | Sensor Mode Change | ✓ |
| 0x54 | 6 | Motor Init | ✗ (固定匹配) |
| 0xC0 | 3 | Color | ✓ |
| 0xD0 | 6 | Color+Distance | ✓ |
| 0xD8 | 10 | Motor Status | ✓ |

### 4.5 私有状态结构体

```c
#define BOOST_MAX_MSG 20

typedef struct {
    // 每个方向独立的消息缓冲
    uint8_t message[2][BOOST_MAX_MSG];
    int msg_len[2];
    uint64_t ss_block[2];
    uint64_t es_block[2];

    // 选项
    int show_errors;
    int show_bytes;

    // 输出注册
    int out_ann;
} boost_state;
```

### 4.6 校验和与消息长度查找

```c
static int boost_valid_checksum(const uint8_t *msg, int len)
{
    uint8_t b = 0xFF;
    for (int i = 0; i < len; i++)
        b ^= msg[i];
    return (b == 0) ? 1 : 0;
}

typedef struct {
    uint8_t msg_type;
    int expected_len;
    int has_checksum;  // 1=需要校验和, 0=不需要
} boost_msg_spec;

static const boost_msg_spec boost_msg_specs[] = {
    {0x46, 3, 1}, {0xC1, 3, 1}, {0xD2, 6, 1}, {0xC3, 3, 1},
    {0xC4, 3, 1}, {0xC5, 3, 1}, {0xCF, 3, 1}, {0xE2, 15, 1},
    {0xD1, 6, 1}, {0xDE, 10, 1}, {0x43, 4, 1}, {0x54, 6, 0},
    {0xC0, 3, 1}, {0xD0, 6, 1}, {0xD8, 10, 1},
    {0x00, 0, 0}  // 哨兵
};

static const boost_msg_spec *boost_find_spec(uint8_t msg_type)
{
    for (int i = 0; boost_msg_specs[i].expected_len > 0; i++) {
        if (boost_msg_specs[i].msg_type == msg_type)
            return &boost_msg_specs[i];
    }
    return NULL;
}
```

### 4.7 recv_proto 实现要点

```c
static void boost_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    boost_state *s = (boost_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") != 0) return;

    uint8_t byte_val = (data && data_len > 0) ? data[0] : 0;
    int rxtx = (data_len > 1) ? data[1] : 0;
    if (rxtx < 0 || rxtx > 1) rxtx = 0;

    // 消息开始
    if (s->msg_len[rxtx] == 0) {
        s->ss_block[rxtx] = start_sample;
    }

    // 追加字节
    if (s->msg_len[rxtx] < BOOST_MAX_MSG) {
        s->message[rxtx][s->msg_len[rxtx]++] = byte_val;
    }

    // 输出单字节注解
    if (s->show_bytes) {
        char byte_str[8];
        snprintf(byte_str, sizeof(byte_str), "%02X", byte_val);
        C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_BYTES, byte_str);
    }

    s->es_block[rxtx] = end_sample;

    // 尝试处理消息
    if (boost_handle_message(di, s, rxtx)) {
        s->msg_len[rxtx] = 0;
    }
}
```

### 4.8 消息处理器分发

```c
static int boost_handle_message(struct srd_decoder_inst *di, boost_state *s, int rxtx)
{
    uint8_t *msg = s->message[rxtx];
    int len = s->msg_len[rxtx];

    const boost_msg_spec *spec = boost_find_spec(msg[0]);
    if (!spec) {
        // 未知消息类型，丢弃
        return 1;
    }

    // 等待更多字节
    if (len < spec->expected_len)
        return 0;

    // 校验和检查
    if (spec->has_checksum && !boost_valid_checksum(msg, len)) {
        if (s->show_errors) {
            char err_str[128];
            // 生成 hex 字符串
            int pos = 0;
            pos += snprintf(err_str + pos, sizeof(err_str) - pos, "Failed checksum: ");
            for (int i = 0; i < len && pos < 100; i++)
                pos += snprintf(err_str + pos, sizeof(err_str) - pos, "%02X", msg[i]);
            C_ANN_PUT(di, s->ss_block[rxtx], s->es_block[rxtx], s->out_ann, ANN_ERROR, err_str);
        }
        return 1;
    }

    // 特殊处理：Motor Init (0x54)
    if (msg[0] == 0x54) {
        static const uint8_t expected[] = {0x54, 0x22, 0x00, 0x10, 0x20, 0xB9};
        if (memcmp(msg, expected, 6) != 0) {
            if (s->show_errors) {
                C_ANN_PUT(di, s->ss_block[rxtx], s->es_block[rxtx],
                          s->out_ann, ANN_ERROR, "Malformed Motor Initialization message");
            }
            return 1;
        }
        C_ANN_PUT(di, s->ss_block[rxtx], s->es_block[rxtx],
                  s->out_ann, ANN_MESSAGE, "Motor Initialization", "Motor Init", "MI");
        return 1;
    }

    // 根据消息类型输出
    boost_output_message(di, s, rxtx, spec);
    return 1;
}
```

### 4.9 LEGO 颜色与传感器模式查找表

```c
static const char *lego_colors[] = {
    "Black", "Pink", "Purple", "Blue", "LightBlue",
    "Cyan", "Green", "Yellow", "Orange", "Red", "White"
};

static const char *lego_sensor_modes[] = {
    "ColorOnly", "CoarseDist", NULL, NULL, "FineDist",
    NULL, "RGB", NULL, "Color+Dist", "Luminosity"
};
```

---

## 解码器 5：crsf_c

### 5.1 Python 元数据提取

| 属性 | Python 值 | C 映射 |
|------|----------|--------|
| id | `crsf` | `crsf_c` |
| name | `CRSF` | `CRSF(C)` |
| longname | `Crossfire rc protocol` | `Crossfire RC protocol (C)` |
| desc | `A protocol for radio control systems.` | `A protocol for radio control systems. (C implementation, faster than Python)` |
| license | `gplv2+` | `gplv2+` |
| inputs | `['uart']` | `{"uart", NULL}` |
| outputs | `[]` | `NULL, 0` |
| tags | `['radio','control', 'RC']` | `{"radio", "control", "RC", NULL}` |

### 5.2 注解映射

```c
enum crsf_ann {
    ANN_TEXT_VERBOSE = 0,  // 'text-verbose' - Human-readable text (verbose)
    ANN_TEXT_ERROR,        // 'text-error' - Human-readable Error text
    NUM_ANN,
};

static const char *crsf_ann_labels[][3] = {
    {"", "text-verbose", "Human-readable text (verbose)"},
    {"", "text-error", "Human-readable Error text"},
};

static const int crsf_row_normal_classes[] = {ANN_TEXT_VERBOSE, ANN_TEXT_ERROR, -1};

static const struct srd_c_ann_row crsf_ann_rows[] = {
    {"normal", "Normal", crsf_row_normal_classes, 2},
};
```

### 5.3 选项

无选项。

### 5.4 CRSF 协议帧结构

```
[Sync Byte][Length Byte][Frame Type Byte][Payload nB][CRC8 1B]

Sync Byte:
  0xEE = To Transmitter Module
  0xEA = To Handset
  0xC8 = To Flight Controller
  0xEC = To Receiver

Length Byte: 2~62 (payload 长度 + 2，不含 sync 和 CRC)
Frame Type: 见下表
CRC8: poly 0xD5
```

### 5.5 帧类型查找表

```c
typedef struct {
    uint8_t type;
    const char *name;
    const char *short_name;
} crsf_frame_type;

static const crsf_frame_type crsf_frame_types[] = {
    {0x16, "CRSF_FRAMETYPE_RC_CHANNELS_PACKED", "RC_CH"},
    {0x28, "CRSF_FRAMETYPE_DEVICE_PING", "DEV_PING"},
    {0x29, "CRSF_FRAMETYPE_DEVICE_INFO", "DEV_INFO"},
    {0x2B, "CRSF_FRAMETYPE_PARAMETER_SETTINGS_ENTRY", "PARAM_SET"},
    {0x2C, "CRSF_FRAMETYPE_PARAMETER_READ", "PARAM_RD"},
    {0x2D, "CRSF_FRAMETYPE_PARAMETER_WRITE", "PARAM_WR"},
    {0x32, "CRSF_FRAMETYPE_COMMAND", "CMD"},
    {0x02, "GPS", "GPS"},
    {0x07, "Vario", "VARIO"},
    {0x08, "Battery sensor", "BAT"},
    {0x09, "Baro altitude", "BARO"},
    {0x10, "OpenTX sync", "OTX_SYNC"},
    {0x14, "LINK_STATISTICS", "LINK_STAT"},
    {0x1E, "Attitude", "ATT"},
    {0x21, "Flight mode", "FLT_MODE"},
    {0x2A, "Request settings", "REQ_SET"},
    {0x3A, "Radio", "RADIO"},
    {0x00, NULL, NULL}  // 哨兵
};
```

### 5.6 Sync Byte 查找表

```c
typedef struct {
    uint8_t sync;
    const char *name;
    const char *short_name;
} crsf_sync_byte;

static const crsf_sync_byte crsf_sync_bytes[] = {
    {0xEE, "To Transmitter Module", "To TX Module"},
    {0xEA, "To Handset", "To HS"},
    {0xC8, "To Flight Controller", "To FC"},
    {0xEC, "To Receiver", "To RX"},
    {0x00, NULL, NULL}
};
```

### 5.7 私有状态结构体

```c
#define CRSF_MAX_PAYLOAD 64

typedef struct {
    // 帧解析状态
    int state;                // 0=等待sync, 1=等待length, 2=等待type, 3=接收payload
    uint8_t sync_byte;        // 当前帧的 sync byte
    uint8_t len_byte;         // 当前帧的 length byte
    uint8_t frame_type;       // 当前帧的 type byte
    const char *frame_type_name;  // 帧类型名称
    uint8_t payload[CRSF_MAX_PAYLOAD];
    int payload_len;          // 已接收 payload 长度
    int payload_expected;     // 期望 payload 长度 (len_byte - 2)
    uint64_t ss_frame;        // 帧起始采样

    // 输出注册
    int out_ann;
} crsf_state;
```

### 5.8 recv_proto 实现要点

```c
static void crsf_recv_proto(struct srd_decoder_inst *di,
    uint64_t start_sample, uint64_t end_sample,
    const char *cmd, const unsigned char *data, uint64_t data_len)
{
    crsf_state *s = (crsf_state *)c_decoder_get_private(di);
    if (!s) return;

    if (strcmp(cmd, "DATA") != 0) return;

    uint8_t byte_val = (data && data_len > 0) ? data[0] : 0;

    switch (s->state) {
    case 0: // 等待 sync byte
    {
        const crsf_sync_byte *sb = crsf_find_sync(byte_val);
        if (sb) {
            s->sync_byte = byte_val;
            s->frame_type_name = NULL;
            s->ss_frame = start_sample;
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann,
                      ANN_TEXT_VERBOSE, sb->name, sb->short_name);
            s->state = 1;
        } else {
            char err[64];
            snprintf(err, sizeof(err), "Unknown sync: 0x%02X", byte_val);
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_TEXT_ERROR, err);
        }
        break;
    }

    case 1: // 等待 length byte
    {
        if (byte_val >= 2 && byte_val <= 62) {
            s->len_byte = byte_val;
            s->payload_expected = byte_val - 2;  // 减去 type + CRC
            char len_str[64];
            snprintf(len_str, sizeof(len_str),
                     "Num of bytes succeeding: %d", byte_val - 2);
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann,
                      ANN_TEXT_VERBOSE, len_str);
            s->state = 2;
        } else {
            char err[64];
            snprintf(err, sizeof(err), "Invalid length: %d", byte_val);
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_TEXT_ERROR, err);
            crsf_reset_state(s);
        }
        break;
    }

    case 2: // 等待 frame type byte
    {
        const crsf_frame_type *ft = crsf_find_frame_type(byte_val);
        if (ft) {
            s->frame_type = byte_val;
            s->frame_type_name = ft->name;
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann,
                      ANN_TEXT_VERBOSE, ft->name, ft->short_name);
            s->payload_len = 0;
            s->state = 3;
        } else {
            char err[64];
            snprintf(err, sizeof(err), "Unknown frame type: 0x%02X", byte_val);
            C_ANN_PUT(di, start_sample, end_sample, s->out_ann, ANN_TEXT_ERROR, err);
            crsf_reset_state(s);
        }
        break;
    }

    case 3: // 接收 payload + CRC
    {
        if (s->payload_len < CRSF_MAX_PAYLOAD) {
            s->payload[s->payload_len++] = byte_val;
        }

        // len_byte 包含 type(1) + payload(n) + CRC(1)
        // 所以总字节数 = len_byte + 1 (不含 sync)
        // 已接收: type(1) + payload_len
        // 还需要: len_byte - 1 - payload_len (包含 CRC)
        if (s->payload_len >= s->payload_expected + 1) {
            // 帧完成，payload_len - 1 是实际 payload，最后一个是 CRC
            crsf_handle_frame(di, s, start_sample, end_sample);
            crsf_reset_state(s);
        }
        break;
    }
    }
}
```

### 5.9 RC Channels 解码

```c
static void crsf_decode_rc_channels(struct srd_decoder_inst *di, crsf_state *s,
    uint64_t es)
{
    // RC_CHANNELS_PACKED: 16 channels × 11 bits = 176 bits = 22 bytes payload
    // payload[0..21] = channel data, payload[22] = CRC
    int num_channels = 16;
    int bits_per_channel = 11;

    for (int chan = 0; chan < num_channels; chan++) {
        int bit_offset = chan * bits_per_channel;
        uint32_t value = 0;
        for (int bit = 0; bit < bits_per_channel; bit++) {
            int byte_idx = (bit_offset + bit) / 8;
            int bit_idx = 7 - ((bit_offset + bit) % 8);  // MSB first
            if (byte_idx < s->payload_len - 1) {  // 排除 CRC
                int b = (s->payload[byte_idx] >> bit_idx) & 1;
                value = (value << 1) | b;
            }
        }
        char chan_str[64];
        snprintf(chan_str, sizeof(chan_str), "Chan:%d Value:%d", chan, value);
        C_ANN_PUT(di, s->ss_frame, es, s->out_ann, ANN_TEXT_VERBOSE, chan_str);
    }
    C_ANN_PUT(di, s->ss_frame, es, s->out_ann,
              ANN_TEXT_VERBOSE, "Checksum crc8 poly 0xD5.");
}
```

### 5.10 Link Statistics 解码

```c
static void crsf_decode_link_stats(struct srd_decoder_inst *di, crsf_state *s,
    uint64_t es)
{
    // LINK_STATISTICS payload: 10 bytes + CRC
    if (s->payload_len < 11) return;
    uint8_t *p = s->payload;

    char stat_str[128];
    snprintf(stat_str, sizeof(stat_str),
             "Uplink RSSI 1: -%ddB", p[0]);
    C_ANN_PUT(di, s->ss_frame, es, s->out_ann, ANN_TEXT_VERBOSE, stat_str);

    snprintf(stat_str, sizeof(stat_str),
             "Uplink RSSI 2: -%ddB", p[2]);
    C_ANN_PUT(di, s->ss_frame, es, s->out_ann, ANN_TEXT_VERBOSE, stat_str);

    snprintf(stat_str, sizeof(stat_str),
             "Uplink Link Quality: %d", p[4]);
    C_ANN_PUT(di, s->ss_frame, es, s->out_ann, ANN_TEXT_VERBOSE, stat_str);

    snprintf(stat_str, sizeof(stat_str),
             "Uplink SNR: %ddB", p[6]);
    C_ANN_PUT(di, s->ss_frame, es, s->out_ann, ANN_TEXT_VERBOSE, stat_str);

    snprintf(stat_str, sizeof(stat_str),
             "Active Antenna: %d", p[8]);
    C_ANN_PUT(di, s->ss_frame, es, s->out_ann, ANN_TEXT_VERBOSE, stat_str);

    snprintf(stat_str, sizeof(stat_str),
             "RF Mode: %d", p[9]);
    C_ANN_PUT(di, s->ss_frame, es, s->out_ann, ANN_TEXT_VERBOSE, stat_str);

    C_ANN_PUT(di, s->ss_frame, es, s->out_ann,
              ANN_TEXT_VERBOSE, "Checksum crc8 poly 0xD5.");
}
```

---

## CMakeLists.txt 修改

在 `C_DECODERS` 列表中添加 5 个新解码器：

```cmake
# 在 C_DECODERS 列表末尾添加：
arm_itm_c
arm_tpiu_c
bluetooth_h4_c
boost_c
crsf_c
```

---

## 测试策略

### 单元测试方法

1. **编译测试**：确保所有 5 个 C 文件编译无错误
2. **加载测试**：确保 DLL 能被 libsigrokdecode 正确加载
3. **功能测试**：使用已知 UART 捕获数据验证解码输出

### 测试数据建议

| 解码器 | 测试数据来源 |
|--------|------------|
| arm_itm_c | ARM Cortex-M ITM trace 输出 |
| arm_tpiu_c | ARM TPIU 帧格式数据 |
| bluetooth_h4_c | HCI Command/Event 帧序列 |
| boost_c | LEGO Boost Hub 通信数据 |
| crsf_c | CRSF RC 控制帧 |

---

## 风险与注意事项

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| arm_itm 的 objdump 功能无法移植 | PC→函数映射丢失 | 保留注解类但不输出，文档说明 |
| arm_tpiu 需要输出 uart 协议 | 下层堆叠依赖 | 确保 c_decoder_put_python 格式与 uart_c 一致 <!-- Updated: 已验证arm_tpiu的DATA输出格式与uart_c.c的DATA输出格式一致 --> |
| bluetooth_h4 的 HCI 命令表很大 | 代码体积 | 使用静态数组 + 线性搜索 |
| boost 的 handlers.py 有语法错误 (msg[1]. msg[2]) | 0xCF 处理器有 bug | C 版本修正此 bug |
| crsf Python 版本不完整 | 部分帧类型未实现 | C 版本实现 RC Channels + Link Statistics，其他帧类型输出原始数据 |
| arm_itm 依赖 arm_tpiu 输出 | 两个解码器在同一批次 | arm_tpiu_c 必须先完成，arm_itm_c 才能测试 <!-- Updated: 新增依赖关系说明 --> |
| bluetooth_h4 Python输出格式 | Python使用[ann_class, datavalues]格式 | C版本需使用c_decoder_put_python正确传递包类型和数据 <!-- Updated: 新增Python输出格式说明 --> |
| strlcat 不可用 | Windows平台无strlcat | 使用snprintf拼接替代 <!-- Updated: 新增跨平台兼容性问题 --> |
| SRD_OUTPUT_PYTHON兼容性警告 | c_decoder_api.c注册SRD_OUTPUT_PYTHON时会输出警告 | arm_tpiu_c和bluetooth_h4_c需注册此输出，警告可忽略，C→C堆叠不受影响 <!-- Updated: 新增API兼容性说明 --> |
