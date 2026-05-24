# 移植检查清单 — Batch 34: Python→C 解码器移植

## 通用检查项（适用于所有 5 个解码器）

### 文件结构

- [ ] 文件名格式正确：`{decoder_id}_c.c`
- [ ] 文件位于 `libsigrokdecode/c_decoders/` 目录
- [ ] 包含标准头文件：`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<glib.h>`, `"libsigrokdecode.h"`
- [ ] 无未使用的 include

### srd_c_decoder 结构体

- [ ] `.id` 格式为 `"{name}_c"`（如 `"ethernet_c"`）
- [ ] `.name` 格式为 `"{Name}(C)"`（如 `"Ethernet(C)"`）
- [ ] `.longname` 包含 `(C)` 后缀
- [ ] `.desc` 包含 `C implementation` 说明
- [ ] `.license` 正确设置
- [ ] `.channels = NULL, .num_channels = 0`（上层解码器无通道）
- [ ] `.optional_channels = NULL, .num_optional_channels = 0`
- [ ] `.inputs` 正确声明输入协议名
- [ ] `.outputs` 正确声明输出协议名（无输出则为 NULL）
- [ ] `.tags` 正确设置
- [ ] `.recv_proto` 指向实现函数
- [ ] `.decode` 指向空实现函数

### Annotations

- [ ] ann_labels 第一列全部为 `""`
- [ ] ann_labels 行数 = NUM_ANN
- [ ] 所有 annotation class 都被映射到某个 annotation_row
- [ ] annotation_row 的 class 数组末尾无 `-1` 哨兵（上层解码器使用 count 参数）
- [ ] annotation_row 的 count 参数与 class 数组实际长度一致

### 生命周期函数

- [ ] `reset()` — 使用 `c_decoder_get_private()` + `g_malloc0()` 模式
- [ ] `start()` — 使用 `c_decoder_register_output()` 注册输出
- [ ] `decode()` — 空实现 `(void)di;`
- [ ] `destroy()` — 使用 `g_free()` + `c_decoder_set_private(di, NULL)` 模式
- [ ] `srd_c_decoder_entry()` — 初始化 options 的 `def` 和 `values`
- [ ] `srd_c_decoder_api_version()` — 返回 `SRD_C_DECODER_API_VERSION`

### recv_proto 实现

- [ ] 函数签名正确：`void xxx_recv_proto(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 开头获取私有数据：`xxx_state *s = (xxx_state *)c_decoder_get_private(di);`
- [ ] 检查 `s` 非 NULL
- [ ] 使用 `strcmp(cmd, "...")` 匹配命令
- [ ] 从 `data` 读取时检查 `data_len` 是否足够

### 向上层传递数据

- [ ] 使用 `c_decoder_put_python(di, ss, es, out_python, cmd, data, data_len)` 传递
- [ ] 动态分配的 data 缓冲区在调用后 `g_free()` 释放
- [ ] 数据布局使用 little-endian 字节序
- [ ] `uint16_t` 和 `uint64_t` 使用 `memcpy()` 读写（避免对齐问题）

### 内存安全

- [ ] 无内存泄漏：所有 `g_malloc()`/`g_malloc0()` 都有对应的 `g_free()`
- [ ] 缓冲区溢出保护：数组访问前检查边界
- [ ] 字符串操作使用 `snprintf()` 而非 `sprintf()`
- [ ] 无 use-after-free

---

## ethernet_c 专项检查

### 协议解析

- [ ] SFD 检测：0xD5 字节触发从 WAITING → DST_MAC 转换
- [ ] MAC 地址格式化：`XX:XX:XX:XX:XX:XX` 格式
- [ ] 广播 MAC 检测：`FF:FF:FF:FF:FF:FF`
- [ ] EtherType 查表：已知类型显示名称，未知显示 "UNKNOWN"
- [ ] FCS 校验：CRC32 计算结果与 `0x2144DF1C` 比较
- [ ] FCS 注释位置计算正确

### 二进制输出

- [ ] pcapng Section Header Block 格式正确
- [ ] pcapng Interface Description Block 格式正确
- [ ] pcapng Simple Packet Block 格式正确（含 padding 对齐）
- [ ] 在 `start()` 中输出 pcapng headers

### 向上层传递

- [ ] `"PAYLOAD"` cmd 发送 payload（不含 FCS 的 4 字节）
- [ ] PAYLOAD data 布局：`payload_len(2) + payload(N) + block_count(2) + blocks(M*16)`
- [ ] blocks 数组与 payload 字节一一对应

---

## avclan_c 专项检查

### 查找表完整性

- [ ] HWAddresses：30 个条目全部移植
- [ ] FunctionIDs：25 个条目全部移植
- [ ] CommCtrlOpcodes：15 个条目全部移植
- [ ] CDOpcodes：10 个条目全部移植
- [ ] CmdSwOpcodes：12 个条目全部移植
- [ ] AudioAmpOpcodes：1 个条目
- [ ] TunerOpcodes：1 个条目
- [ ] TunerState：2 个条目
- [ ] TunerModes：6 个条目
- [ ] 位标志类型（CDSlots, CDStateCodes, CDFlags, AudioAmpFlags, TunerFlags）有对应的字符串转换函数

### 状态机

- [ ] IDLE → MASTER_ADDRESS → SLAVE_ADDRESS → CONTROL → DATA_LENGTH → DATA → IDLE
- [ ] NAK 在任意状态重置为 IDLE
- [ ] DATA 状态后自动重置为 IDLE

### 数据解析

- [ ] 单播 (broadcast_bit=1): data_bytes[1]=from, data_bytes[2]=to, data_bytes[3:]=payload
- [ ] 广播 (broadcast_bit=0): data_bytes[0]=from, data_bytes[1]=to, data_bytes[2:]=payload
- [ ] 功能 ID 查表正确
- [ ] 函数分派优先级正确：from_to > to_only > from_only > both

### 协议处理

- [ ] COMM_CTRL: ADVERTISE_FUNCTION, PING_REQ/RESP, LIST_FUNCTIONS_RESP
- [ ] CMD_SW: opcode 解析
- [ ] TUNER: REPORT 解析（state, mode, band, freq, channel, flags）
- [ ] CD Player: REPORT_PLAYBACK, REPORT_TRACK_NAME, REPORT_LOADER, REPORT_TOC
- [ ] CD To: REQUEST_TRACK_NAME
- [ ] AUDIO_AMP: REPORT（volume, balance, fade, bass, treble, flags）
- [ ] BCD 转换正确
- [ ] 左右/前后映射正确

### iebus 桥接

- [ ] ⚠️ 确认 Python iebus → C avclan_c 的桥接机制可用
- [ ] 如果桥接不可用，在代码注释中标注限制

---

## arp_c 专项检查

### 包解析

- [ ] ARP 包最小长度 28 字节检查
- [ ] struct unpack 格式 `>2H2BH6s4s6s4s` 对应的 C 字段解析正确
- [ ] htype, ptype, hlen, plen, oper 字段解析正确
- [ ] MAC 地址格式化：`XX:XX:XX:XX:XX:XX`
- [ ] IP 地址格式化：`X.X.X.X`

### 注释输出

- [ ] Hardware Type 注释
- [ ] Protocol Type 注释（查 EtherType 表）
- [ ] Hardware Address Length 注释
- [ ] Protocol Address Length 注释
- [ ] Operation 注释（Request/Reply）
- [ ] Source MAC 注释
- [ ] Source IP 注释
- [ ] Destination MAC 注释
- [ ] Destination IP 注释
- [ ] Message 注释（ANN_MSG）：
  - [ ] ARP Announcement 检测（spa == tpa）
  - [ ] ARP Probe 检测（spa == "0.0.0.0"）
  - [ ] Request 消息格式："Who has {tpa}? Tell {spa} ({sha})"
  - [ ] Reply 消息格式："{spa} is at {sha}"

### blocks 定位

- [ ] 每个字段的 ss/es 使用正确的 block 索引
- [ ] msg_start = SHA 的 ss, msg_end = TPA 的 es

---

## ipv4_c 专项检查

### 包解析

- [ ] IHL 检查：`(payload[0] & 0x0F) * 4`，仅支持 IHL=20
- [ ] IHL != 20 时跳过（不报错，直接 return）
- [ ] Version + Header Length 注释
- [ ] DSCP + ECN 注释
- [ ] Total Length 注释
- [ ] Identification 注释
- [ ] Flags 注释（DF, MF）
- [ ] Fragment Offset 注释
- [ ] TTL 注释
- [ ] Protocol 注释（查 ip_protocol 表）
- [ ] Header Checksum 注释（校验结果 OK/FAILED）
- [ ] Source IP 注释
- [ ] Destination IP 注释
- [ ] Payload 逐字节注释（ANN_DATA）

### Checksum 校验

- [ ] 16 位反码和算法正确
- [ ] 结果 == 0xFFFF 表示校验通过

### 向上层传递

- [ ] `"IP_PAYLOAD"` cmd 发送 payload + blocks + src_ip + dst_ip
- [ ] IP_PAYLOAD data 布局：`payload_len(2) + payload(N) + block_count(2) + blocks(M*16) + src_ip(4) + dst_ip(4)`
- [ ] blocks 从 index 20 开始（跳过 IP header）
- [ ] src_ip = payload[12..15], dst_ip = payload[16..19]

### Flags 字段定位

- [ ] Flags 的 ss = blocks[6].ss
- [ ] Flags 的 es = blocks[6].ss + (blocks[6].es - blocks[6].ss) * 3/8
- [ ] Fragment Offset 的 ss = blocks[6].ss + (blocks[6].es - blocks[6].ss) * 3/8
- [ ] Fragment Offset 的 es = blocks[7].es

---

## udp_c 专项检查

### Options

- [ ] format 选项默认值 "hex"
- [ ] format 选项可选值：ascii, dec, hex, oct, bin
- [ ] `srd_c_decoder_entry()` 中初始化 option 默认值和 values 列表

### 包解析

- [ ] Source Port 注释
- [ ] Destination Port 注释
- [ ] Length 注释
- [ ] Checksum 注释（校验结果 OK/FAILED）

### Checksum 校验

- [ ] 伪头部构造：src_ip(4) + dst_ip(4) + 0x00 + 0x11(UDP) + udp_length(2)
- [ ] 16 位反码和算法正确
- [ ] 结果 == 0xFFFF 表示校验通过

### Payload 格式化

- [ ] hex: `0xXX`
- [ ] dec: `N`
- [ ] ascii: 尝试 UTF-8，失败显示 `[0xXX]`
- [ ] oct: `N`（八进制）
- [ ] bin: `0bXXXXXXXX`

### 向上层传递

- [ ] `"UDP_PAYLOAD"` cmd 发送 payload + blocks
- [ ] UDP_PAYLOAD data 布局：`payload_len(2) + payload(N) + block_count(2) + blocks(M*16)`
- [ ] blocks 从 index 8 开始（跳过 UDP header）

### Binary 输出

- [ ] Raw UDP payload 通过 `c_decoder_put_binary()` 输出

---

## 构建验证

- [ ] `build_incremental.cmd` 成功编译所有 5 个解码器 DLL
- [ ] DLL 输出路径：`build.dir/decoders/c_decoders/{name}_c.dll`
- [ ] 无编译警告（特别是 `-Wformat-truncation` 相关）
- [ ] PXView.exe 可正常启动
- [ ] 在 PXView 中可添加 C 版本解码器到信号
- [ ] C 解码器与对应 Python 解码器的注释输出一致

## 功能验证

### ethernet_c

- [ ] 可堆叠在 4b5b_c 之上
- [ ] Preamble + SFD 注释正确
- [ ] MAC 地址注释正确
- [ ] EtherType 注释正确
- [ ] FCS 校验结果正确
- [ ] pcapng 二进制输出可被 Wireshark 打开

### arp_c

- [ ] 可堆叠在 ethernet_c 之上
- [ ] ARP Request/Reply 解析正确
- [ ] 消息注释正确（Who has / is at）

### ipv4_c

- [ ] 可堆叠在 ethernet_c 之上
- [ ] IP 头部各字段注释正确
- [ ] Checksum 校验结果正确
- [ ] Payload 可传递给上层

### udp_c

- [ ] 可堆叠在 ipv4_c 之上
- [ ] UDP 头部各字段注释正确
- [ ] Checksum 校验结果正确
- [ ] Payload 格式化输出正确
- [ ] Binary 输出正确

### avclan_c

- [ ] ⚠️ 需要 iebus C 版本或 Python→C 桥接才能测试
- [ ] 如果 iebus 桥接可用：状态机转换正确
- [ ] 功能 ID 查表正确
- [ ] 各设备类型处理函数正确
