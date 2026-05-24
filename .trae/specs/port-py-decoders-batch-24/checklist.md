# Python → C 解码器移植检查清单 — Batch 24

## 通用检查项（适用于所有 5 个解码器）

### 文件结构

- [ ] 文件位于 `libsigrokdecode/c_decoders/{id}_c.c`
- [ ] 版权头与 Python 版一致（license 类型匹配）
- [ ] 包含 `#include <stdio.h>`, `#include <stdlib.h>`, `#include <string.h>`, `#include <glib.h>`, `#include "libsigrokdecode.h"`

### srd_c_decoder 结构体

- [ ] `.id` = Python id + "_c"（如 `"adns5020_c"`）
- [ ] `.name` = 显示名 + "(C)"（如 `"ADNS-5020(C)"`）
- [ ] `.longname` = 完整名 + " (C)"（如 `"Avago ADNS-5020 (C)"`）
- [ ] `.desc` = Python desc + " (C implementation)"
- [ ] `.license` 与 Python 版一致
- [ ] `.channels = NULL`, `.num_channels = 0`（SPI 上层无直接通道）
- [ ] `.optional_channels = NULL`, `.num_optional_channels = 0`
- [ ] `.inputs = {"spi", NULL}`, `.num_inputs = 1`
- [ ] `.decode` 指向空函数（SPI 上层不用 decode）
- [ ] `.recv_proto` 指向实际实现
- [ ] `.reset` 正确分配/清零私有状态
- [ ] `.start` 调用 `c_decoder_register_output(di, SRD_OUTPUT_ANN, "xxx")`
- [ ] `.destroy` 调用 `g_free()` 释放私有数据

### ann_labels 规范

- [ ] 每个 label 三元组第一列为 `""`
- [ ] 第二列为短名（与 Python annotation id 一致）
- [ ] 第三列为长描述（与 Python annotation label 一致）
- [ ] `NUM_ANN` 枚举值正确
- [ ] `ann_labels` 数组长度 = `NUM_ANN`

### annotation_rows 规范

- [ ] 每个 row 的 class 数组以 `-1` 结尾
- [ ] `srd_c_ann_row` 的 `num_classes` 字段 = 实际 class 数量（不含 -1）
- [ ] 所有 annotation class 都被映射到某个 row
- [ ] row id 与 Python annotation_rows 的 id 一致
- [ ] row name 与 Python annotation_rows 的 name 一致

### 导出函数

- [ ] `srd_c_decoder_entry()` 返回 `&xxx_c_decoder` 指针
- [ ] `srd_c_decoder_api_version()` 返回 `SRD_C_DECODER_API_VERSION`
- [ ] 两个函数都有 `SRD_C_DECODER_EXPORT` 前缀

### recv_proto 实现

- [ ] 函数签名：`void xxx_recv_proto(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 首先获取私有状态：`xxx_state *s = (xxx_state *)c_decoder_get_private(di);`
- [ ] 检查 `s` 非 NULL
- [ ] 正确解析 SPI "DATA" 包格式：flags + mosi(8字节LE) + miso(8字节LE)
- [ ] 正确解析 SPI "CS-CHANGE" 包格式：old_cs + new_cs
- [ ] 忽略不相关的 cmd 类型（BITS, TRANSFER 等）
- [ ] 状态机转换正确，无死锁/遗漏状态

### 私有状态管理

- [ ] `reset()` 首次调用时 `g_malloc0()` 分配
- [ ] `reset()` 后续调用时 `memset()` 清零
- [ ] `destroy()` 调用 `g_free()` 并 `c_decoder_set_private(di, NULL)`
- [ ] 状态结构体无内存泄漏

### CMakeLists.txt

- [ ] 在 `C_DECODERS` 列表中添加了解码器名（不含 `_c` 后缀）

---

## ADNS-5020 专用检查项

### 元数据

- [ ] `.id = "adns5020_c"`, `.name = "ADNS-5020(C)"`
- [ ] `.tags = {"IC", "PC", "Sensor", NULL}`, `.num_tags = 3`
- [ ] `.outputs = NULL`, `.num_outputs = 0`

### Annotations

- [ ] ANN_READ=0, ANN_WRITE=1, ANN_WARN=2, NUM_ANN=3
- [ ] 3 个 row：read{0}, write{1}, warnings{2}

### 寄存器表

- [ ] 16 个寄存器条目：0x00-0x0B, 0x0D, 0x3A, 0x3F, 0x63
- [ ] addr > 0x63 返回 "Unknown"
- [ ] 未在表中的 addr 返回 "Reserved 0xXX"

### 解码逻辑

- [ ] CS-CHANGE 上升沿：byte_count 不为 0 或 2 时输出 warning
- [ ] DATA 收集 2 字节后处理
- [ ] cmd & 0x80 判断读/写
- [ ] cmd & 0x7f 提取寄存器地址
- [ ] 读/写均输出 `"{reg_desc}: {arg:02X}"` 格式

---

## AS5047 专用检查项

### 元数据

- [ ] `.id = "as5047_c"`, `.name = "AS5047(C)"`
- [ ] `.tags = {"Embedded/industrial", NULL}`, `.num_tags = 1`
- [ ] `.outputs = NULL`, `.num_outputs = 0`

### Annotations

- [ ] 7 个 class：commandframe(0), readdataframe(1), writedataframe(2), registerread(3), registerwrite(4), warning(5), field(6)
- [ ] 4 个 row：fields{6}, frames{0,1,2}, transactions{3,4}, warnings{5}

### 16-bit 帧组装

- [ ] 正确处理 SPI wordsize=8 时的双字节组装
- [ ] 高字节先收（phase=0），低字节后收（phase=1）
- [ ] SPI wordsize=16 时直接使用完整值
- [ ] 组装完成后才进行解码

### 奇偶校验

- [ ] MOSI 命令帧奇偶校验（INIT 状态）
- [ ] MISO 数据帧奇偶校验（READ 状态）
- [ ] 校验失败输出 ANN_WARNING

### Error Flag

- [ ] MISO 读数据帧 bit14=1 时输出 warning "error flag set"

### 跨帧事务

- [ ] ANN_REGISTERREAD 范围从 transaction_start 到当前 es
- [ ] ANN_REGISTERWRITE 范围从 transaction_start 到当前 es
- [ ] transaction_start 在 INIT 状态收到第一帧时记录

---

## AVR ISP 专用检查项

### 元数据

- [ ] `.id = "avr_isp_c"`, `.name = "AVR ISP(C)"`
- [ ] `.tags = {"Debug/trace", NULL}`, `.num_tags = 1`
- [ ] `.outputs = NULL`, `.num_outputs = 0`

### Annotations

- [ ] 15 个 class：PE(0), RSB0(1), RSB1(2), RSB2(3), CE(4), RFB(5), RHFB(6), REFB(7), RLB(8), REEM(9), RP(10), LPMP(11), WP(12), WARN(13), DEV(14)
- [ ] 3 个 row：commands{0-12}, warnings{13}, devs{14}

### 4 字节命令收集

- [ ] 同时收集 MOSI 和 MISO 字节
- [ ] byte_count==0 时记录 ss_cmd
- [ ] byte_count==4 时调用 handle_command()

### 命令识别

- [ ] Programming Enable: cmd[0]==0xAC && cmd[1]==0x53
- [ ] Chip Erase: cmd[0]==0xAC && (cmd[1] & 0x80)
- [ ] Read Fuse Bits: cmd[0:3]==[0x50,0x00,0x00]
- [ ] Read Fuse High Bits: cmd[0:3]==[0x58,0x08,0x00]
- [ ] Read Extended Fuse Bits: cmd[0:3]==[0x50,0x08,0x00]
- [ ] Read Signature Byte 0: cmd[0]==0x30 && cmd[2]==0x00
- [ ] Read Signature Byte 1: cmd[0]==0x30 && cmd[2]==0x01
- [ ] Read Signature Byte 2: cmd[0]==0x30 && cmd[2]==0x02
- [ ] Read Lock Bits: cmd[0:2]==[0x58,0x00]
- [ ] Read EEPROM: cmd[0]==0xA0 && (cmd[1] & 0xC0)==0x00
- [ ] Read Program Memory: (cmd[0]==0x20||0x28) && (cmd[1] & 0xF0)==0x00
- [ ] Load Program Memory Page: (cmd[0]==0x40||0x48) && (cmd[1] & 0xF0)==0x00
- [ ] Write Program Memory Page: cmd[0]==0x4C && (cmd[1] & 0xF0)==0x00
- [ ] Unknown command: 输出 warning

### 设备识别

- [ ] Signature Byte 0 → vendor_code → "Atmel" / "Device locked"
- [ ] Signature Byte 1 → part_fam_flash_size
- [ ] Signature Byte 2 → part_number → 查找设备名
- [ ] 设备 annotation 范围从 ss_device 到 es_cmd
- [ ] 非 Atmel vendor code (0x1E) 时输出 warning

### Sanity Checks

- [ ] Programming Enable 回复检查：ret[1:4] != [0xAC, 0x53, cmd[2]]
- [ ] Signature Byte 0 回复检查
- [ ] Signature Byte 1 回复检查
- [ ] Signature Byte 2 回复检查
- [ ] Chip Erase 回复检查

### BITS 包处理

- [ ] 收到 BITS 包时不中断状态机
- [ ] BITS 包数据可忽略（C 版不需要位级数据）

---

## CC1101 专用检查项

### 元数据

- [ ] `.id = "cc1101_c"`, `.name = "CC1101(C)"`
- [ ] `.tags = {"IC", "Wireless/RF", NULL}`, `.num_tags = 2`
- [ ] `.outputs = NULL`, `.num_outputs = 0`

### Annotations

- [ ] 8 个 class：strobe(0), single_read(1), single_write(2), burst_read(3), burst_write(4), status_read(5), status(6), warning(7)
- [ ] 4 个 row：cmd{0}, data{1-5}, status{6}, warnings{7}

### 命令字节解析

- [ ] addr < 0x30 或 addr==0x3E/0x3F：配置寄存器
  - [ ] 0x00 → Write (1 byte)
  - [ ] 0x40 → Burst write (1+ bytes)
  - [ ] 0x80 → Read (1 byte)
  - [ ] 0xC0 → Burst read (1+ bytes)
- [ ] addr >= 0x30（非 0x3E/0x3F）：命令/状态
  - [ ] 0x00-0x3F → Strobe (0 data bytes)
  - [ ] 0xC0 → Status read (1+ bytes)
- [ ] 未知命令输出 warning

### Status 寄存器解码

- [ ] bit7: CHIP_RDYn（=1 时标注 "CHIP_RDYn is high!"）
- [ ] bits6:4: STATE（查表 cc1101_status_states）
- [ ] bits3:0: FIFO_BYTES_AVAILABLE
- [ ] 读操作 → "available in RX FIFO"
- [ ] 写操作 → "free in TX FIFO"

### 事务处理

- [ ] 第一个 MISO 字节始终解码为 Status 寄存器
- [ ] Strobe 命令立即输出，不等待数据
- [ ] CS 上升沿结束当前事务
- [ ] 不足 min 字节时输出 warning "missing data bytes"
- [ ] 超出 max 字节时输出 warning "excess byte"

### 寄存器名查找

- [ ] 配置寄存器（0x00-0x2E）查 cc1101_regs 表
- [ ] 状态寄存器（0x30-0x3F）查 cc1101_status_regs 表
- [ ] PATABLE(0x3E) 和 FIFO(0x3F) 特殊处理
- [ ] 格式：`"{REG_NAME} ({ADDR:02X})"`

---

## CYRF6936 专用检查项

### 元数据

- [ ] `.id = "cyrf6936_c"`, `.name = "CYRF6936(C)"`
- [ ] `.tags = {"Embedded/industrial", NULL}`, `.num_tags = 1`
- [ ] `.outputs = {"cyrf6936", NULL}`, `.num_outputs = 1`
- [ ] `.license = "gplv3+"`（注意与 gplv2+ 不同）

### Annotations

- [ ] 7 个 class：write(0), read(1), tx-data(2), rx-data(3), state(4), warning(5), wait(6)
- [ ] 3 个 row：cmd{0-3}, warnings{4-5}, delays{6}

### Options

- [ ] 4 个选项正确定义
- [ ] `spi3pin`：默认 "no"，可选 "yes"/"no"
- [ ] `delaysplit`：默认 0（double 类型）
- [ ] `invert_mosi`：默认 "no"，可选 "yes"/"no"
- [ ] `invert_miso`：默认 "no"，可选 "yes"/"no"
- [ ] `srd_c_decoder_entry()` 中正确设置 GVariant 默认值和可选值列表

### 命令字节解析

- [ ] bits5:0 = address
- [ ] bit7 = direction (1=write, 0=read)
- [ ] bit6 = increment (1=auto address increment)

### 寄存器宽度

- [ ] 每个寄存器有独立的 width（1-16 字节）
- [ ] TX_BUFFER_ADR(0x20) width=16
- [ ] RX_BUFFER_ADR(0x21) width=16
- [ ] SOP_CODE_ADR(0x22) width=8
- [ ] DATA_CODE_ADR(0x23) width=16
- [ ] PREAMBLE_ADR(0x24) width=3
- [ ] MFG_ID_ADR(0x25) width=6
- [ ] 其余 width=1

### SPI 3-pin 模式

- [ ] spi3pin=1 时，读操作从 MOSI 线取数据（而非 MISO）
- [ ] IO_CFG_ADR(0x0D) bit1 检测 SPI 模式切换
- [ ] 模式切换时输出 ANN_STATE annotation

### Delay 标注

- [ ] 需要 samplerate（通过 metadata 回调获取）
- [ ] CS 上升沿记录 wait_s
- [ ] CS 下降沿记录 wait_e
- [ ] 计算延迟：`dt_us = (wait_e - wait_s) * 1000000 / samplerate`
- [ ] dt_us >= delaysplit 时输出 ANN_WAIT `"delay_us({dt_us})"`

### 寄存器解码（基础层）

- [ ] 所有寄存器至少输出 `"read/write(_inc)(REG_NAME) = 0xHH"` 格式
- [ ] 未知寄存器输出 warning
- [ ] 多字节寄存器输出空格分隔的十六进制值

### 寄存器解码（增强层 — 关键寄存器）

- [ ] 0x00 CHANNEL_ADR：频率计算、速度类型、通道号范围检查
- [ ] 0x03 TX_CFG_ADR：数据模式、PA 功率等级
- [ ] 0x0D IO_CFG_ADR：SPI 3/4-pin 模式检测
- [ ] 0x0F XACT_CFG_ADR：事务结束状态、ACK 超时
- [ ] 0x10 FRAMING_CFG_ADR：SOP 配置

### 增量地址模式

- [ ] inc=1 时，finish_command() 后 addr 自增
- [ ] 自增后重置 mb 缓冲
- [ ] 自增后重新查找寄存器宽度

### 第一个 MISO 字节

- [ ] 命令字节后的第一个 MISO 字节被丢弃
- [ ] 若 MISO 非 0xFF 且非 0x00，输出 warning "unrequested data"

### Binary 输出（如支持）

- [ ] TX_BUFFER_ADR 写操作时输出 binary class 0
- [ ] RX_BUFFER_ADR 读操作时输出 binary class 1
- [ ] 数据 slice/pad 到寄存器宽度

---

## 编译检查

- [ ] `build_incremental.cmd` 成功完成
- [ ] 5 个 DLL 文件存在于 `build.dir/decoders/c_decoders/`
- [ ] 无编译错误
- [ ] 无编译警告（或仅有可接受的警告）

## 运行时检查

- [ ] PXView 能正确加载每个 C 解码器
- [ ] C 解码器能与 SPI C 解码器正确堆叠
- [ ] Annotation 在 UI 中正确显示
- [ ] 无崩溃或内存泄漏
- [ ] 与 Python 版输出对比，关键内容一致
