# 任务分解 — Batch 34: Python→C 解码器移植

## 任务依赖关系

```
Task 1 (ethernet_c) ──┬──→ Task 3 (arp_c)
                       └──→ Task 4 (ipv4_c) ──→ Task 5 (udp_c)

Task 2 (avclan_c) — 独立（依赖 iebus Python 桥接）
Task 0 (CMakeLists) — 所有解码器完成后执行
```

**推荐实现顺序**：ethernet_c → arp_c → ipv4_c → udp_c → avclan_c → CMakeLists

---

## Task 1: ethernet_c — Ethernet II 解码器

### 1.1 创建文件 `libsigrokdecode/c_decoders/ethernet_c.c`

#### 1.1.1 结构定义与常量

- [ ] 定义 `ethernet_state` 结构体，包含：
  - `int state` — 状态机当前状态 (WAITING/DST_MAC/SRC_MAC/ETH_TYPE/PAYLOAD)
  - `uint8_t buffer[2048]` — 数据缓冲区
  - `int buffer_len` — 缓冲区当前长度
  - `uint8_t frame_data[2048]` — 帧二进制数据（用于 CRC 和 pcapng）
  - `int frame_len` — 帧数据长度
  - `uint8_t payload[2048]` — payload 数据
  - `int payload_len` — payload 长度
  - `uint64_t frame_start` — 帧起始采样
  - `uint64_t header_start` — 头部起始采样
  - `uint64_t payload_start` — payload 起始采样
  - `uint64_t ss_block, es_block` — 当前注释块起止
  - `struct { uint64_t ss; uint64_t es; } blocks[2048]` — payload block 位置
  - `int block_count` — block 数量
  - `int out_ann, out_binary, out_python` — 输出注册 ID
- [ ] 定义 annotation 枚举：`ANN_HEADER=0, ANN_DATA, NUM_ANN`
- [ ] 定义 ann_labels 数组（第一列 `""`）
- [ ] 定义 annotation_rows：headers(ANN_HEADER), datas(ANN_DATA)
- [ ] 定义 binary output：pcapng
- [ ] 定义 inputs: `{"4b5b", NULL}`
- [ ] 定义 outputs: `{"ethernet", NULL}`
- [ ] 定义 tags: `{"Networking", "PC", NULL}`

#### 1.1.2 EtherType 查找表

- [ ] 移植 `ethernet/dicts.py` 中的 `ethertype` 字典为 C 结构体数组
- [ ] 实现 `find_ethertype()` 查找函数

#### 1.1.3 CRC32 实现

- [ ] 实现 CRC32 查表初始化函数 `crc32_init()`
- [ ] 实现 CRC32 计算函数 `crc32_calc()`
- [ ] 验证：`crc32_calc(frame, frame_len) == 0x2144DF1C` 表示 FCS 正确

#### 1.1.4 pcapng 二进制输出

- [ ] 实现 `pcap_headers()` — 输出 Section Header Block + Interface Description Block
- [ ] 实现 `pcap_append()` — 输出 Simple Packet Block
- [ ] 在 `start()` 中调用 `pcap_headers()`

#### 1.1.5 状态机实现

- [ ] WAITING 状态：累积字节，检测 SFD (0xD5)
  - 输出 Preamble 注释
  - 输出 SFD 注释
  - 清空 buffer，切换到 DST_MAC
- [ ] DST_MAC 状态：累积 6 字节
  - 格式化 MAC 地址字符串
  - 检测广播 MAC (FF:FF:FF:FF:FF:FF)
  - 输出 Destination MAC 注释
  - 切换到 SRC_MAC
- [ ] SRC_MAC 状态：累积 6 字节
  - 输出 Source MAC 注释
  - 切换到 ETH_TYPE
- [ ] ETH_TYPE 状态：累积 2 字节
  - 查表输出 EtherType 注释
  - 切换到 PAYLOAD
- [ ] PAYLOAD 状态：逐字节处理
  - 追加到 payload
  - 记录 block 位置
  - 输出数据注释 (ANN_DATA)

#### 1.1.6 recv_proto 实现

- [ ] 处理 `"START"` cmd → 记录 frame_start
- [ ] 处理 `"DATA"` cmd → 调用状态机
- [ ] 处理 `"TERMINATE"` cmd → FCS 校验 + pcapng 输出 + 向上层发送 PAYLOAD
- [ ] 处理 `"RESET"` cmd → 重置状态

#### 1.1.7 生命周期函数

- [ ] `ethernet_reset()` — 分配/清零私有数据
- [ ] `ethernet_start()` — 注册输出 (ANN, BINARY, PYTHON)，调用 pcap_headers
- [ ] `ethernet_decode()` — 空实现
- [ ] `ethernet_destroy()` — 释放私有数据

#### 1.1.8 srd_c_decoder 结构体与入口

- [ ] 填充 `ethernet_c_decoder` 结构体
- [ ] 实现 `srd_c_decoder_entry()` — 无 options 需要初始化
- [ ] 实现 `srd_c_decoder_api_version()`

---

## Task 2: avclan_c — AVC-LAN 解码器

### 2.1 创建文件 `libsigrokdecode/c_decoders/avclan_c.c`

#### 2.1.1 查找表移植（lists.py → C）

- [ ] 移植 `HWAddresses` 枚举 → `hw_addresses[]` name_entry 数组
- [ ] 移植 `FunctionIDs` 枚举 → `function_ids[]` name_entry 数组
- [ ] 移植 `CommCtrlOpcodes` 枚举 → `comm_ctrl_opcodes[]` name_entry 数组
- [ ] 移植 `CDOpcodes` 枚举 → `cd_opcodes[]` name_entry 数组
- [ ] 移植 `CDSlots` IntFlag → `cd_slots` 位标志定义 + `cd_slots_str()` 函数
- [ ] 移植 `CDStateCodes` IntFlag → `cd_state_codes` 位标志 + `cd_state_str()` 函数
- [ ] 移植 `CDFlags` IntFlag → `cd_flags` 位标志 + `cd_flags_str()` 函数
- [ ] 移植 `CmdSwOpcodes` 枚举 → `cmd_sw_opcodes[]` name_entry 数组
- [ ] 移植 `AudioAmpOpcodes` 枚举 → `audio_amp_opcodes[]` name_entry 数组
- [ ] 移植 `AudioAmpFlags` IntFlag → `audio_amp_flags` 位标志 + `audio_amp_flags_str()` 函数
- [ ] 移植 `TunerOpcodes` 枚举 → `tuner_opcodes[]` name_entry 数组
- [ ] 移植 `TunerFlags` IntFlag → `tuner_flags` 位标志 + `tuner_flags_str()` 函数
- [ ] 移植 `TunerState` 枚举 → `tuner_states[]` name_entry 数组
- [ ] 移植 `TunerModes` 枚举 → `tuner_modes[]` name_entry 数组
- [ ] 实现通用查找函数 `find_name()`

#### 2.1.2 结构定义与常量

- [ ] 定义 `avclan_state` 结构体，包含：
  - `int state` — 状态机 (IDLE/MASTER_ADDRESS/SLAVE_ADDRESS/CONTROL/DATA_LENGTH/DATA)
  - `int broadcast_bit`
  - `uint16_t master_addr, slave_addr`
  - `int control`
  - `int data_length`
  - `struct { uint8_t b; uint64_t ss; uint64_t es; } data_bytes[256]`
  - `int data_byte_count`
  - `int from_function, to_function` — 解析后的功能 ID
  - `int out_ann` — 输出注册 ID
- [ ] 定义 31 个 annotation 枚举值
- [ ] 定义 ann_labels 数组
- [ ] 定义 7 个 annotation_rows
- [ ] 定义 inputs: `{"iebus", NULL}`
- [ ] 定义 outputs: `{NULL}` (无输出)
- [ ] 定义 tags: `{"Automotive", NULL}`

#### 2.1.3 状态机实现

- [ ] IDLE + `"HEADER"` → 保存 broadcast_bit，转 MASTER_ADDRESS
- [ ] MASTER_ADDRESS + `"MASTER ADDRESS"` → 保存 master_addr，查表输出
- [ ] SLAVE_ADDRESS + `"SLAVE ADDRESS"` → 保存 slave_addr，查表输出
- [ ] CONTROL + `"CONTROL"` → 保存 control
- [ ] DATA_LENGTH + `"DATA LENGTH"` → 保存 data_length
- [ ] DATA + `"DATA"` → 解析数据字节序列
  - 根据 broadcast_bit 区分单播/广播
  - 提取 from_function 和 to_function
  - 输出功能注释
  - 分派到具体处理函数
- [ ] 任意状态 + `"NAK"` → 重置

#### 2.1.4 协议处理函数

- [ ] `avclan_pkt_comm_ctrl()` — 处理通信控制协议
  - 解析 opcode
  - ADVERTISE_FUNCTION: 输出功能名
  - PING_REQ/PING_RESP: 输出序列号
  - LIST_FUNCTIONS_RESP: 遍历输出功能列表
- [ ] `avclan_pkt_from_25()` — 处理 CMD_SW
  - 解析 opcode，输出命令名
- [ ] `avclan_pkt_from_60()` — 处理 TUNER
  - 解析 opcode
  - REPORT: 解析 state, mode, band, freq, channel, flags
  - 频率计算（FM/AM 波段）
- [ ] `avclan_pkt_from_cd_player()` — 处理 CD 播放器
  - REPORT_PLAYBACK: state, disc, track, time, flags
  - REPORT_TRACK_NAME: disc, track, title
  - REPORT_LOADER: slots 信息
  - REPORT_TOC: disc, track, count, time
- [ ] `avclan_pkt_to_cd_player()` — 处理 CD 命令
  - REQUEST_TRACK_NAME: disc, track
- [ ] `avclan_pkt_74()` — 处理 AUDIO_AMP
  - REPORT: volume, balance, fade, bass, treble, flags

#### 2.1.5 辅助函数

- [ ] `bcd2dec(b)` — BCD 转十进制
- [ ] `map_left_right(value, center, neg_tag, pos_tag)` — 左右映射
- [ ] 位标志转字符串函数（CDFlags, TunerFlags 等）

#### 2.1.6 函数分派机制

- [ ] 实现优先级分派表
- [ ] 优先级：`pkt_from_{from}_to_{to}` > `pkt_to_{to}` > `pkt_from_{from}` > `pkt_{id}`

#### 2.1.7 生命周期函数

- [ ] `avclan_reset()` — 分配/清零私有数据
- [ ] `avclan_start()` — 注册 ANN 输出
- [ ] `avclan_decode()` — 空实现
- [ ] `avclan_destroy()` — 释放私有数据

#### 2.1.8 srd_c_decoder 结构体与入口

- [ ] 填充 `avclan_c_decoder` 结构体
- [ ] 实现 `srd_c_decoder_entry()` — 无 options
- [ ] 实现 `srd_c_decoder_api_version()`

---

## Task 3: arp_c — ARP 解码器

### 3.1 创建文件 `libsigrokdecode/c_decoders/arp_c.c`

#### 3.1.1 结构定义与常量

- [ ] 定义 `arp_state` 结构体，包含：
  - `int out_ann, out_python` — 输出注册 ID
  - `uint64_t ss_block, es_block` — 当前注释块
- [ ] 定义 annotation 枚举：`ANN_DATA=0, ANN_MSG, NUM_ANN`
- [ ] 定义 ann_labels 数组
- [ ] 定义 annotation_rows：datas(ANN_DATA), msgs(ANN_MSG)
- [ ] 定义 inputs: `{"ethernet", NULL}`
- [ ] 定义 outputs: `{NULL}` (无输出)
- [ ] 定义 tags: `{"Networking", "PC", NULL}`

#### 3.1.2 EtherType 查找表（精简版）

- [ ] 移植 ARP 所需的 EtherType 条目（仅需 IPv4 和 ARP 相关的少数条目）
- [ ] 或复用完整表

#### 3.1.3 recv_proto 实现

- [ ] 处理 `"PAYLOAD"` cmd
- [ ] 解析 payload_len, payload, block_count, blocks
- [ ] 验证 payload 长度 >= 28 字节

#### 3.1.4 ARP 包解析

- [ ] 解析 htype (2 bytes) — Hardware Type
- [ ] 解析 ptype (2 bytes) — Protocol Type（查 EtherType 表）
- [ ] 解析 hlen (1 byte) — Hardware Address Length
- [ ] 解析 plen (1 byte) — Protocol Address Length
- [ ] 解析 oper (2 bytes) — Operation (1=Request, 2=Reply)
- [ ] 解析 sha (6 bytes) — Sender MAC
- [ ] 解析 spa (4 bytes) — Sender IP
- [ ] 解析 tha (6 bytes) — Target MAC
- [ ] 解析 tpa (4 bytes) — Target IP

#### 3.1.5 注释输出

- [ ] 逐字段输出 ANN_DATA 注释，使用 blocks 定位
- [ ] 输出 ANN_MSG 消息注释：
  - Request + spa==tpa → ARP Announcement
  - Request + spa=="0.0.0.0" → ARP Probe
  - Request → "Who has X? Tell Y (Z)"
  - Reply → "X is at Y"

#### 3.1.6 辅助函数

- [ ] `format_mac(const uint8_t *mac, char *out)` — 格式化 MAC 地址
- [ ] `format_ip(const uint8_t *ip, char *out)` — 格式化 IP 地址

#### 3.1.7 生命周期函数

- [ ] `arp_reset()`, `arp_start()`, `arp_decode()`, `arp_destroy()`

#### 3.1.8 srd_c_decoder 结构体与入口

- [ ] 填充 `arp_c_decoder` 结构体
- [ ] 实现 `srd_c_decoder_entry()`, `srd_c_decoder_api_version()`

---

## Task 4: ipv4_c — IPv4 解码器

### 4.1 创建文件 `libsigrokdecode/c_decoders/ipv4_c.c`

#### 4.1.1 结构定义与常量

- [ ] 定义 `ipv4_state` 结构体，包含：
  - `int out_ann, out_python` — 输出注册 ID
  - `uint64_t ss_block, es_block` — 当前注释块
  - `uint64_t payload_start` — payload 起始采样
  - `int ihl` — IP 头部长度
- [ ] 定义 annotation 枚举：`ANN_HEADER=0, ANN_DATA, NUM_ANN`
- [ ] 定义 ann_labels, annotation_rows
- [ ] 定义 inputs: `{"ethernet", NULL}`
- [ ] 定义 outputs: `{"ipv4", NULL}`
- [ ] 定义 tags: `{"Networking", "PC", NULL}`

#### 4.1.2 IP Protocol 查找表

- [ ] 移植 `ipv4/dicts.py` 中的 `ip_protocol` 字典为 C 结构体数组
- [ ] 实现 `find_ip_protocol()` 查找函数

#### 4.1.3 Header Checksum 校验

- [ ] 实现 `ip_checksum_verify()` 函数
- [ ] 算法：16 位反码和，结果应为 0xFFFF

#### 4.1.4 recv_proto 实现

- [ ] 处理 `"PAYLOAD"` cmd
- [ ] 解析 payload 和 blocks

#### 4.1.5 IPv4 包解析

- [ ] Version + Header Length (byte 0)
- [ ] DSCP + ECN (byte 1)
- [ ] Total Length (bytes 2-3)
- [ ] Identification (bytes 4-5)
- [ ] Flags + Fragment Offset (bytes 6-7)
  - DF = (byte[6] & 0x40) >> 6
  - MF = (byte[6] & 0x20) >> 5
  - Offset = ((byte[6] & 0x1F) << 8) | byte[7]
- [ ] TTL (byte 8)
- [ ] Protocol (byte 9) — 查表
- [ ] Header Checksum (bytes 10-11)
- [ ] Source IP (bytes 12-15)
- [ ] Destination IP (bytes 16-19)
- [ ] Payload (bytes 20+) — 逐字节输出 ANN_DATA

#### 4.1.6 向上层传递

- [ ] 构造 `"IP_PAYLOAD"` 数据包
- [ ] 包含：payload + blocks + src_ip + dst_ip
- [ ] 调用 `c_decoder_put_python()`

#### 4.1.7 生命周期函数

- [ ] `ipv4_reset()`, `ipv4_start()`, `ipv4_decode()`, `ipv4_destroy()`

#### 4.1.8 srd_c_decoder 结构体与入口

- [ ] 填充 `ipv4_c_decoder` 结构体
- [ ] 实现 `srd_c_decoder_entry()`, `srd_c_decoder_api_version()`

---

## Task 5: udp_c — UDP 解码器

### 5.1 创建文件 `libsigrokdecode/c_decoders/udp_c.c`

#### 5.1.1 结构定义与常量

- [ ] 定义 `udp_state` 结构体，包含：
  - `int out_ann, out_binary, out_python` — 输出注册 ID
  - `uint64_t ss_block, es_block` — 当前注释块
  - `uint64_t payload_start` — payload 起始采样
  - `int format` — 数据格式选项 (0=hex, 1=ascii, 2=dec, 3=oct, 4=bin)
- [ ] 定义 annotation 枚举：`ANN_HEADER=0, ANN_DATA, NUM_ANN`
- [ ] 定义 ann_labels, annotation_rows
- [ ] 定义 binary output：raw
- [ ] 定义 options：format
- [ ] 定义 inputs: `{"ipv4", NULL}`
- [ ] 定义 outputs: `{"udp", NULL}`
- [ ] 定义 tags: `{"Networking", "PC", NULL}`

#### 5.1.2 Checksum 校验

- [ ] 实现 UDP checksum 验证（使用 IPv4 伪头部）
- [ ] 伪头部 = src_ip(4) + dst_ip(4) + 0x00 + 0x11 + udp_length(2)
- [ ] 校验和计算同 IP header checksum 算法

#### 5.1.3 recv_proto 实现

- [ ] 处理 `"IP_PAYLOAD"` cmd
- [ ] 解析 payload, blocks, src_ip, dst_ip

#### 5.1.4 UDP 包解析

- [ ] Source Port (2 bytes)
- [ ] Destination Port (2 bytes)
- [ ] Length (2 bytes)
- [ ] Checksum (2 bytes) — 校验
- [ ] Payload (bytes 8+) — 根据 format 选项格式化输出

#### 5.1.5 数据格式化

- [ ] ascii: 尝试 UTF-8 解码，失败则显示 [0xXX]
- [ ] dec: 十进制
- [ ] hex: 0xXX（默认）
- [ ] oct: 八进制
- [ ] bin: 0bXXXXXXXX

#### 5.1.6 向上层传递

- [ ] 构造 `"UDP_PAYLOAD"` 数据包
- [ ] 调用 `c_decoder_put_python()`
- [ ] 输出 binary 数据（raw UDP payload）

#### 5.1.7 生命周期函数

- [ ] `udp_reset()`, `udp_start()`, `udp_decode()`, `udp_destroy()`

#### 5.1.8 srd_c_decoder 结构体与入口

- [ ] 填充 `udp_c_decoder` 结构体
- [ ] 实现 `srd_c_decoder_entry()` — 初始化 format option 默认值 "hex"
- [ ] 实现 `srd_c_decoder_api_version()`

---

## Task 0: CMakeLists.txt 更新

### 0.1 修改 `CMakeLists.txt`

- [ ] 在 `C_DECODERS` 列表中添加：`avclan_c`, `ethernet_c`, `arp_c`, `ipv4_c`, `udp_c`
- [ ] 验证构建：`build_incremental.cmd`

---

## 工作量估算

| Task | 解码器 | 预估行数 | 预估时间 | 难度 |
|------|--------|---------|---------|------|
| 1 | ethernet_c | ~550 | 2h | ★★★ |
| 2 | avclan_c | ~900 | 3h | ★★★★★ |
| 3 | arp_c | ~350 | 1h | ★★ |
| 4 | ipv4_c | ~500 | 1.5h | ★★★ |
| 5 | udp_c | ~350 | 1h | ★★ |
| 0 | CMakeLists | ~5 | 0.1h | ★ |
| **合计** | | **~2655** | **~8.6h** | |
