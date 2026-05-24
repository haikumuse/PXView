# Python → C 解码器移植任务清单 — Batch 35

## 任务依赖关系

```
前置修改(ps2_c.c) ──→ ps2_keyboard_c.c
                  └──→ ps2_mouse_c.c
mdio_c.c(已完成)  ──→ cfp_c.c
usb_signalling_c.c(已完成) → usb_packet_c.c → usb_request_c.c
```

---

## Task 0: 前置修改 — ps2_c.c 添加 Python 输出

**优先级**: 阻塞（必须先完成，否则 Task 2/3 无法工作）

**文件**: `libsigrokdecode/c_decoders/ps2_c.c`

### 子任务

- [ ] 0.1 修改 `ps2_outputs` 数组，添加 `"ps2"` 输出类型
- [ ] 0.2 在 `ps2_priv` 结构体中添加 `int out_python` 字段
- [ ] 0.3 在 `ps2_start()` 中注册 python 输出：`s->out_python = c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "ps2");`
- [ ] 0.4 在 `ps2_handle_byte()` 末尾添加 `c_decoder_put_python` 调用，输出格式：
  - cmd = `"BYTE"`
  - data = 4 bytes: `[byte_val, is_host, parity_ok, has_ack]`
- [ ] 0.5 在 `ps2_reset()` 中初始化 `s->out_python = -1`
- [ ] 0.6 更新 `ps2_c_decoder` 结构体：`.outputs = ps2_outputs`, `.num_outputs = 1`
- [ ] 0.7 编译验证 ps2_c.c 无错误
- [ ] 0.8 功能验证：ps2_c 独立使用时输出不受影响

### 关键代码片段

```c
// ps2_handle_byte() 末尾添加:
{
    int ones = 0;
    for (int i = 0; i < 8; i++)
        if (s->byte_val & (1 << i)) ones++;
    ones += s->bits[9];
    int parity_ok = (ones % 2 == 1);

    unsigned char py_data[4];
    py_data[0] = s->byte_val;
    py_data[1] = s->htd ? 1 : 0;
    py_data[2] = parity_ok ? 1 : 0;
    py_data[3] = 0;
    c_decoder_put_python(di, s->bit_ss[0], s->bit_ss[10], s->out_python, "BYTE", py_data, 4);
}
```

### 验证方法
- 编译 ps2_c.dll 成功
- 在 PXView 中加载 ps2_c 解码器，确认 annotation 输出正常
- 在 ps2_c 上堆叠 Python 版 ps2_keyboard，确认数据传递正常

---

## Task 1: CFP 解码器 — `cfp_c.c`

**优先级**: 低复杂度，可先完成
**依赖**: mdio_c.c（已完成）
**文件**: `libsigrokdecode/c_decoders/cfp_c.c`

### 子任务

- [ ] 1.1 创建 `cfp_c.c` 文件，包含标准头文件
- [ ] 1.2 定义 annotation 枚举和标签数组
- [ ] 1.3 定义 annotation rows
- [ ] 1.4 定义 inputs/outputs/tags
- [ ] 1.5 实现 MODULE_ID 查找表（18 条记录）
- [ ] 1.6 定义 `cfp_state` 私有结构体
- [ ] 1.7 实现 `cfp_reset()` — 分配私有状态
- [ ] 1.8 实现 `cfp_start()` — 注册 `out_ann`
- [ ] 1.9 实现 `cfp_decode()` — 空函数
- [ ] 1.10 实现 `cfp_recv_proto()` — 核心逻辑：
  - 仅处理 `cmd == "DATA"` 且 `is_read == 1`
  - 解析 `clause45_addr`，映射到 CFP 寄存器区域
  - 特殊处理 `clause45_addr == 0x8000` 时的 MODULE_ID 解码
- [ ] 1.11 实现 `cfp_destroy()` — 释放私有状态
- [ ] 1.12 定义 `cfp_c_decoder` 结构体，设置 `.recv_proto = cfp_recv_proto`
- [ ] 1.13 实现 `srd_c_decoder_entry()` — 无 options，直接返回
- [ ] 1.14 实现 `srd_c_decoder_api_version()`
- [ ] 1.15 在 CMakeLists.txt 的 `C_DECODERS` 列表中添加 `cfp_c`
- [ ] 1.16 编译验证
- [ ] 1.17 功能验证：在 mdio_c 上堆叠 cfp_c，对比 Python cfp 输出

### 预估代码量
- 约 200-250 行

### 关键风险
- mdio_c 的 python 输出数据格式必须与 CFP 解码器期望的格式一致（已确认一致：8 bytes DATA 格式）

---

## Task 2: PS/2 Keyboard 解码器 — `ps2_keyboard_c.c`

**优先级**: 中等复杂度
**依赖**: Task 0（ps2_c.c 添加 Python 输出）
**文件**: `libsigrokdecode/c_decoders/ps2_keyboard_c.c`

### 子任务

- [ ] 2.1 创建 `ps2_keyboard_c.c` 文件
- [ ] 2.2 定义 annotation 枚举（ANN_PRESS/ANN_RELEASE/ANN_ACK）和标签
- [ ] 2.3 定义 binary 输出（BINARY_KEYS）
- [ ] 2.4 定义 annotation rows
- [ ] 2.5 定义 inputs/outputs/tags
- [ ] 2.6 实现标准扫描码查找表（`std_keys` 数组，约 48 条记录）
- [ ] 2.7 实现扩展扫描码查找表（`ext_keys` 数组，约 19 条记录）
- [ ] 2.8 实现 `ps2kb_lookup_key()` 函数
- [ ] 2.9 定义 `ps2kb_state` 私有结构体
- [ ] 2.10 实现 `ps2kb_reset()` — 分配私有状态，初始化 `sw=0, ann=ANN_PRESS, extended=0`
- [ ] 2.11 实现 `ps2kb_start()` — 注册 `out_ann` 和 `out_binary`
- [ ] 2.12 实现 `ps2kb_decode()` — 空函数
- [ ] 2.13 实现 `ps2kb_recv_proto()` — 核心状态机：
  - 处理 `cmd == "BYTE"`
  - 解析 4 字节数据：`[val, is_host, parity_ok, has_ack]`
  - 主机发送时重置状态
  - 0xF0 → 设置 Release 标记
  - 0xE0 → 设置 Extended 标记
  - 0xFA → ACK 标注
  - 其他 → 查表输出按键名
- [ ] 2.14 实现 `ps2kb_destroy()`
- [ ] 2.15 定义 `ps2_keyboard_c_decoder` 结构体
- [ ] 2.16 实现 `srd_c_decoder_entry()` — 无 options
- [ ] 2.17 实现 `srd_c_decoder_api_version()`
- [ ] 2.18 在 CMakeLists.txt 添加 `ps2_keyboard_c`
- [ ] 2.19 编译验证
- [ ] 2.20 功能验证：在 ps2_c 上堆叠 ps2_keyboard_c

### 预估代码量
- 约 300-350 行（含查找表）

### 关键风险
- ps2_c.c 的 python 输出格式必须与 ps2_keyboard_c 期望的格式匹配
- 扫描码查找表必须完整覆盖 Python 版本的所有条目

---

## Task 3: PS/2 Mouse 解码器 — `ps2_mouse_c.c`

**优先级**: 中等复杂度
**依赖**: Task 0（ps2_c.c 添加 Python 输出）
**文件**: `libsigrokdecode/c_decoders/ps2_mouse_c.c`

### 子任务

- [ ] 3.1 创建 `ps2_mouse_c.c` 文件
- [ ] 3.2 定义 annotation 枚举（ANN_MOVEMENT）和标签
- [ ] 3.3 定义 binary 输出（BINARY_BYTES/BINARY_MOVEMENT）
- [ ] 3.4 定义 annotation rows
- [ ] 3.5 定义 inputs/outputs/tags
- [ ] 3.6 定义 `ps2mouse_packet_entry` 和 `ps2mouse_state` 结构体
- [ ] 3.7 实现 `ps2mouse_reset()` — 分配私有状态，清空 packets
- [ ] 3.8 实现 `ps2mouse_start()` — 注册 `out_ann` 和 `out_binary`
- [ ] 3.9 实现 `ps2mouse_decode()` — 空函数
- [ ] 3.10 实现 `ps2mouse_mouse_movement()` — 解码 3 字节鼠标数据包：
  - 解析 flags 字节的按键位（L/M/R）
  - 解析 X/Y 位移（有符号，符号扩展）
  - 检测溢出标志
  - 输出 Movement 标注和 binary
- [ ] 3.11 实现 `ps2mouse_print_packets()` — 输出当前数据包组
- [ ] 3.12 实现 `ps2mouse_recv_proto()` — 核心逻辑：
  - 处理 `cmd == "BYTE"`
  - 维护 packets 列表
  - 检测 host/mouse 方向切换
  - 特殊处理 ACK 字节（0xFA）
  - 每 3 个字节触发一次输出
- [ ] 3.13 实现 `ps2mouse_destroy()`
- [ ] 3.14 定义 `ps2_mouse_c_decoder` 结构体
- [ ] 3.15 实现 `srd_c_decoder_entry()` — 无 options
- [ ] 3.16 实现 `srd_c_decoder_api_version()`
- [ ] 3.17 在 CMakeLists.txt 添加 `ps2_mouse_c`
- [ ] 3.18 编译验证
- [ ] 3.19 功能验证：在 ps2_c 上堆叠 ps2_mouse_c

### 预估代码量
- 约 250-300 行

### 关键风险
- 鼠标数据包的 3 字节分组逻辑需要正确处理方向切换
- ACK 字节的特殊处理逻辑

---

## Task 4: USB Packet 解码器 — `usb_packet_c.c`

**优先级**: 高复杂度，核心解码器
**依赖**: usb_signalling_c.c（已完成）
**文件**: `libsigrokdecode/c_decoders/usb_packet_c.c`

### 子任务

- [ ] 4.1 创建 `usb_packet_c.c` 文件
- [ ] 4.2 定义 29 个 annotation 枚举和标签数组
- [ ] 4.3 定义 annotation rows（fields + packets）
- [ ] 4.4 定义 inputs/outputs/tags
- [ ] 4.5 定义 option（signalling）
- [ ] 4.6 实现 PID 查找表（16 条记录）
- [ ] 4.7 实现 `get_category()` 函数
- [ ] 4.8 实现 `ann_index()` 函数
- [ ] 4.9 实现 `bitstr_to_num()` 函数
- [ ] 4.10 实现 `calc_crc5()` 函数
- [ ] 4.11 实现 `calc_crc16()` 函数
- [ ] 4.12 定义 `usb_pkt_state` 私有结构体（含 bits 数组和采样点数组）
- [ ] 4.13 实现 `usb_pkt_reset()` — 分配私有状态
- [ ] 4.14 实现 `usb_pkt_start()` — 注册 `out_ann` 和 `out_python`
- [ ] 4.15 实现 `usb_pkt_decode()` — 空函数
- [ ] 4.16 实现 `usb_pkt_handle_packet()` — 核心包解析逻辑：
  - SYNC 字段验证
  - PID 解析和查找
  - Token 包处理（ADDR + EP + CRC5）
  - Data 包处理（数据字节 + CRC16）
  - Handshake 包处理
  - Special 包处理
  - 包摘要输出
- [ ] 4.17 实现 `usb_pkt_recv_proto()` — 状态机：
  - SOP → 开始收集位
  - BIT → 添加到位数组
  - EOP/ERR → 触发 handle_packet
- [ ] 4.18 实现 Python 输出格式（PACKET 命令，供 usb_request_c 使用）
- [ ] 4.19 实现 `usb_pkt_destroy()`
- [ ] 4.20 定义 `usb_packet_c_decoder` 结构体
- [ ] 4.21 实现 `srd_c_decoder_entry()` — 初始化 signalling option
- [ ] 4.22 实现 `srd_c_decoder_api_version()`
- [ ] 4.23 在 CMakeLists.txt 添加 `usb_packet_c`
- [ ] 4.24 编译验证
- [ ] 4.25 功能验证：在 usb_signalling_c 上堆叠 usb_packet_c

### 预估代码量
- 约 600-800 行

### 关键风险
- CRC5/CRC16 算法必须与 Python 版本完全一致
- 位序处理（LSB-first）必须正确
- Python 输出格式需要设计，确保 usb_request_c 能正确解析
- bits 数组大小需要合理设置（USB 包最大 1024 字节数据 = 8192+ 位）

---

## Task 5: USB Request 解码器 — `usb_request_c.c`

**优先级**: 高复杂度
**依赖**: Task 4（usb_packet_c.c）
**文件**: `libsigrokdecode/c_decoders/usb_request_c.c`

### 子任务

- [ ] 5.1 创建 `usb_request_c.c` 文件
- [ ] 5.2 定义 5 个 annotation 枚举和标签数组
- [ ] 5.3 定义 binary 输出（BINARY_PCAP）
- [ ] 5.4 定义 annotation rows（4 行）
- [ ] 5.5 定义 inputs/outputs/tags
- [ ] 5.6 定义 option（in_request_start）
- [ ] 5.7 定义 `usb_req_request` 和 `usb_req_state` 结构体
- [ ] 5.8 实现 PCAP 全局头生成函数
- [ ] 5.9 实现 PCAP USB 包结构体和序列化
- [ ] 5.10 实现 `usb_req_reset()` — 分配私有状态
- [ ] 5.11 实现 `usb_req_start()` — 注册 `out_ann` 和 `out_binary`，读取 option
- [ ] 5.12 实现 `usb_req_decode()` — 空函数
- [ ] 5.13 实现 `usb_req_metadata()` — 处理 samplerate
- [ ] 5.14 实现 `usb_req_handle_transfer()` — 事务完成处理：
  - CONTROL SETUP 阶段
  - CONTROL DATA 阶段
  - CONTROL STATUS 阶段
  - BULK IN/OUT 处理
  - 请求超时处理
- [ ] 5.15 实现 `usb_req_handle_request()` — 请求完成处理：
  - 生成 annotation
  - 生成 PCAP SUBMIT/COMPLETE 记录
- [ ] 5.16 实现 `usb_req_recv_proto()` — 核心状态机：
  - 处理 `cmd == "PACKET"`
  - 解析 pcategory 和 pname
  - TOKEN 包 → 设置事务参数
  - DATA 包 → 收集事务数据
  - HANDSHAKE 包 → 完成事务
  - 事务超时检测
- [ ] 5.17 实现 `usb_req_destroy()`
- [ ] 5.18 定义 `usb_request_c_decoder` 结构体，设置 `.metadata = usb_req_metadata`
- [ ] 5.19 实现 `srd_c_decoder_entry()` — 初始化 in_request_start option
- [ ] 5.20 实现 `srd_c_decoder_api_version()`
- [ ] 5.21 在 CMakeLists.txt 添加 `usb_request_c`
- [ ] 5.22 编译验证
- [ ] 5.23 功能验证：在 usb_packet_c 上堆叠 usb_request_c

### 预估代码量
- 约 500-700 行

### 关键风险
- PCAP 二进制输出格式复杂，需要精确实现
- 请求跟踪逻辑涉及多个并发请求（按 addr+ep 索引）
- 事务超时检测逻辑
- usb_packet_c 的 Python 输出格式必须与 usb_request_c 期望的格式匹配

---

## 执行顺序建议

```
Phase 1: Task 0 (ps2_c.c 修改) + Task 1 (cfp_c.c)  — 可并行
Phase 2: Task 2 (ps2_keyboard_c.c) + Task 3 (ps2_mouse_c.c)  — 依赖 Task 0
Phase 3: Task 4 (usb_packet_c.c)  — 独立链
Phase 4: Task 5 (usb_request_c.c)  — 依赖 Task 4
```

### 总预估代码量
| 解码器 | 预估行数 |
|--------|---------|
| ps2_c.c 修改 | ~20 行新增 |
| cfp_c.c | ~200-250 行 |
| ps2_keyboard_c.c | ~300-350 行 |
| ps2_mouse_c.c | ~250-300 行 |
| usb_packet_c.c | ~600-800 行 |
| usb_request_c.c | ~500-700 行 |
| **总计** | **~1900-2400 行** |
