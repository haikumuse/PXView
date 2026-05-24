# Python→C 解码器移植任务分解 — Batch 29

## 任务总览

5 个 UART 上层解码器的 Python→C 移植，按依赖关系和复杂度排序。

---

## 阶段一：基础设施（前置条件）

### Task 1.1：确认 C Decoder API recv_proto 机制可用

- **优先级**：高
- **预估时间**：0.5h
- **描述**：验证 libsigrokdecode 的 recv_proto 回调机制对 UART 上层解码器工作正常。确认 uart_c.c 的 `c_decoder_put_python("DATA", ...)` 输出能被上层 C 解码器的 `recv_proto` 正确接收。
- **验证方法**：检查 `c_decoder_api.c` 中 recv_proto 的调用路径，确认 cmd="DATA" 和 data 格式传递正确
- **依赖**：无
- **产出**：确认 recv_proto 对 UART 协议可用，或记录需要修改的地方

### Task 1.2：CMakeLists.txt 添加 5 个新解码器

- **优先级**：高
- **预估时间**：0.2h
- **描述**：在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `arm_itm_c`, `arm_tpiu_c`, `bluetooth_h4_c`, `boost_c`, `crsf_c`
- **验证方法**：运行 `build_incremental.cmd` 确认 CMake 配置正确识别新目标
- **依赖**：无
- **产出**：CMakeLists.txt 修改

---

## 阶段二：简单解码器（先易后难）

### Task 2.1：crsf_c — CRSF 协议解码器

- **优先级**：高
- **预估时间**：3h
- **复杂度**：★★
- **文件**：`libsigrokdecode/c_decoders/crsf_c.c`
- **描述**：实现 CRSF (Crossfire) RC 协议的 C 解码器
- **子任务**：

| 子任务 | 描述 | 预估 |
|--------|------|------|
| 2.1.1 | 创建文件骨架：includes, enum, struct, 静态数组 | 0.3h |
| 2.1.2 | 实现 sync byte / frame type / length 查找表 | 0.3h |
| 2.1.3 | 实现 recv_proto 状态机（4 状态：sync→length→type→payload） | 0.5h |
| 2.1.4 | 实现 RC Channels Packed 解码（16×11bit） | 0.5h |
| 2.1.5 | 实现 Link Statistics 解码 | 0.3h |
| 2.1.6 | 实现通用帧处理（未知帧类型输出 raw hex） | 0.3h |
| 2.1.7 | 实现 reset/start/decode/destroy/entry 函数 | 0.3h |
| 2.1.8 | 编译测试 | 0.5h |

- **关键点**：
  - Python 版本 `RX=0, TX=0`（bug），C 版本需正确处理 rxtx
  - Python 版本 `self.payload + pdata[1]` 有类型错误，C 版本需修正
  - CRC8 校验暂不实现（Python 版本也未实现），仅输出提示文本
- **依赖**：Task 1.1, 1.2
- **验收标准**：编译通过，DLL 加载成功，能解析 CRSF sync/length/type 字节

### Task 2.2：bluetooth_h4_c — Bluetooth H4 协议解码器

- **优先级**：高
- **预估时间**：4h
- **复杂度**：★★★
- **文件**：`libsigrokdecode/c_decoders/bluetooth_h4_c.c`
- **描述**：实现 Bluetooth H4 HCI 传输层协议的 C 解码器
- **子任务**：

| 子任务 | 描述 | 预估 |
|--------|------|------|
| 2.2.1 | 创建文件骨架：includes, enum, struct, 静态数组 | 0.3h |
| 2.2.2 | 实现 HCI 命令名称查找表（~80 条） | 0.5h |
| 2.2.3 | 实现 recv_proto：IDLE→HEADER→PAYLOAD 状态机 | 0.8h |
| 2.2.4 | 实现 CMD 包解析和输出 | 0.5h |
| 2.2.5 | 实现 ACL 包解析和输出 | 0.3h |
| 2.2.6 | 实现 SCO 包解析和输出 | 0.3h |
| 2.2.7 | 实现 EVENT 包解析和输出 | 0.3h |
| 2.2.8 | 实现 Junk 包输出 | 0.2h |
| 2.2.9 | 实现 reset/start/decode/destroy/entry 函数 | 0.3h |
| 2.2.10 | 编译测试 | 0.5h |

- **关键点**：
  - RX/TX 双方向独立状态机，注解类偏移 9（`ann_base = rxtx * 9`）
  - CMD 包长度在字节 3，ACL 包长度在字节 3-4，SCO 包长度在字节 3，EVENT 包长度在字节 2
  - packet_length 初始值 -1 表示未确定
  - 需要注册 out_python 输出
- **依赖**：Task 1.1, 1.2
- **验收标准**：编译通过，能解析 HCI Command/Event/ACL/SCO 包

### Task 2.3：boost_c — LEGO Boost 协议解码器

- **优先级**：高
- **预估时间**：4h
- **复杂度**：★★★
- **文件**：`libsigrokdecode/c_decoders/boost_c.c`
- **描述**：实现 LEGO Boost Hub 通信协议的 C 解码器
- **子任务**：

| 子任务 | 描述 | 预估 |
|--------|------|------|
| 2.3.1 | 创建文件骨架：includes, enum, struct, 静态数组 | 0.3h |
| 2.3.2 | 实现消息类型/长度/校验查找表 | 0.3h |
| 2.3.3 | 实现 XOR 校验和函数 | 0.2h |
| 2.3.4 | 实现 LEGO 颜色/传感器模式查找表 | 0.2h |
| 2.3.5 | 实现 recv_proto：逐字节构建消息 | 0.5h |
| 2.3.6 | 实现消息处理器分发（15 种消息类型） | 1.5h |
| 2.3.7 | 实现各消息类型的格式化输出 | 0.5h |
| 2.3.8 | 实现 show_errors/show_bytes 选项过滤 | 0.2h |
| 2.3.9 | 实现 reset/start/decode/destroy/entry 函数 | 0.3h |
| 2.3.10 | 编译测试 | 0.5h |

- **关键点**：
  - Python handlers.py 的 `handle_message_CF` 有语法错误 (`msg[1]. msg[2]`)，C 版本需修正
  - 消息长度由首字节（消息类型）决定，不是由长度字段指定
  - 校验和 = XOR 所有字节，结果应为 0
  - Motor Init (0x54) 不用校验和，而是与固定字节序列比较
  - RX/TX 双方向独立缓冲区
- **依赖**：Task 1.1, 1.2
- **验收标准**：编译通过，能解析颜色/距离/电机状态消息

---

## 阶段三：复杂解码器

### Task 3.1：arm_tpiu_c — ARM TPIU 协议解码器

- **优先级**：高
- **预估时间**：5h
- **复杂度**：★★★★
- **文件**：`libsigrokdecode/c_decoders/arm_tpiu_c.c`
- **描述**：实现 ARM Trace Port Interface Unit 帧格式解码器，将 TPIU 流解复用为独立 UART 流
- **子任务**：

| 子任务 | 描述 | 预估 |
|--------|------|------|
| 3.1.1 | 创建文件骨架：includes, enum, struct, 静态数组 | 0.3h |
| 3.1.2 | 实现 16 字节帧缓冲区管理 | 0.5h |
| 3.1.3 | 实现同步检测（0xFF×3 + 0x7F） | 0.3h |
| 3.1.4 | 实现帧处理核心：lowbits 提取、stream ID/data 判断 | 1.0h |
| 3.1.5 | 实现流切换逻辑（含延迟切换） | 0.8h |
| 3.1.6 | 实现 emit_byte：过滤流 + 输出注解 + 输出 Python | 0.5h |
| 3.1.7 | 实现 stream_changed：流切换注解输出 | 0.3h |
| 3.1.8 | 实现 sync_offset 选项 | 0.2h |
| 3.1.9 | 实现 reset/start/decode/destroy/entry 函数 | 0.3h |
| 3.1.10 | 编译测试 | 0.5h |
| 3.1.11 | 与 arm_itm_c 堆叠测试 | 0.5h |

- **关键点**：
  - **必须输出 uart 协议**，以便 arm_itm_c 可堆叠
  - Python 输出格式 `['DATA', 0, (byte, [])]` 需要映射为 C 的 `c_decoder_put_python(di, ss, es, out_python, "DATA", data, 2)`
  - data[0]=byte_value, data[1]=rxtx(0)
  - 帧中 Byte 15 的低 4 位是 Byte 0,2,4,6 的最低位
  - 流切换可延迟到下一个偶数位字节之后
  - sync_offset 选项用于跳过初始字节以快速同步
- **依赖**：Task 1.1, 1.2
- **验收标准**：编译通过，能解复用 TPIU 帧，arm_itm_c 可堆叠在其上方

### Task 3.2：arm_itm_c — ARM ITM 协议解码器

- **优先级**：高
- **预估时间**：6h
- **复杂度**：★★★★★
- **文件**：`libsigrokdecode/c_decoders/arm_itm_c.c`
- **描述**：实现 ARM Instrumentation Trace Macroblock 协议解码器
- **子任务**：

| 子任务 | 描述 | 预估 |
|--------|------|------|
| 3.2.1 | 创建文件骨架：includes, enum, struct, 静态数组 | 0.3h |
| 3.2.2 | 实现 ARM 异常名称查找表 | 0.2h |
| 3.2.3 | 实现 get_packet_type 函数 | 0.3h |
| 3.2.4 | 实现同步检测（0x00×5 + 0x80） | 0.3h |
| 3.2.5 | 实现超时重置（16×byte_len） | 0.2h |
| 3.2.6 | 实现 handle_sync | 0.2h |
| 3.2.7 | 实现 handle_overflow | 0.2h |
| 3.2.8 | 实现 handle_timestamp（1~5 字节变长解析） | 0.8h |
| 3.2.9 | 实现 handle_software（plen 解析 + PID 输出） | 0.5h |
| 3.2.10 | 实现 handle_hardware：DWT events (pid=0) | 0.3h |
| 3.2.11 | 实现 handle_hardware：Exception trace (pid=1) | 0.5h |
| 3.2.12 | 实现 handle_hardware：PC sample (pid=2) | 0.3h |
| 3.2.13 | 实现 handle_hardware：Watchpoint (0x84/0x47/0x4E) | 0.5h |
| 3.2.14 | 实现 mode_change 追踪 | 0.3h |
| 3.2.15 | 实现 fallback（未处理包类型） | 0.2h |
| 3.2.16 | 实现 reset/start/decode/destroy/entry 函数 | 0.3h |
| 3.2.17 | 编译测试 | 0.5h |
| 3.2.18 | 与 arm_tpiu_c 堆叠测试 | 0.5h |

- **关键点**：
  - **不实现 objdump/ELF 功能**：省略 3 个选项，ANN_LOCATION 和 ANN_FUNCTION 保留但不输出
  - **简化 software 字符拼接**：Python 版本将可打印字符按 PID 拼接，C 版本每个 software 包直接输出
  - 包长度由首字节低 2 位决定：plen = (0,1,2,4)[buf[0] & 0x03]
  - 硬件包 PID = buf[0] >> 3
  - 时间戳包使用 7-bit 变长编码
  - 模式追踪：thread/IRQ/exception，需要记住起始采样位置
- **依赖**：Task 1.1, 1.2, Task 3.1（堆叠测试）
- **验收标准**：编译通过，能解析 ITM sync/overflow/timestamp/software/hardware 包

---

## 阶段四：集成测试

### Task 4.1：全量编译测试

- **优先级**：高
- **预估时间**：0.5h
- **描述**：运行 `build_incremental.cmd` 确认所有 5 个新解码器编译成功，且不影响现有解码器
- **依赖**：Task 2.1~3.2 全部完成
- **验收标准**：编译 0 error, 0 warning

### Task 4.2：DLL 加载测试

- **优先级**：高
- **预估时间**：1h
- **描述**：启动 PXView，确认 5 个新 C 解码器出现在解码器列表中，且能正确加载
- **依赖**：Task 4.1
- **验收标准**：5 个解码器均可选、可配置、可运行

### Task 4.3：功能对比测试

- **优先级**：中
- **预估时间**：2h
- **描述**：对每个解码器，使用相同的 UART 捕获数据，对比 Python 版本和 C 版本的输出注解
- **依赖**：Task 4.2
- **验收标准**：C 版本输出的注解内容与 Python 版本基本一致（允许简化差异）

---

## 时间估算汇总

| 阶段 | 任务 | 预估时间 |
|------|------|----------|
| 一 | 基础设施 | 0.7h |
| 二 | crsf_c | 3h |
| 二 | bluetooth_h4_c | 4h |
| 二 | boost_c | 4h |
| 三 | arm_tpiu_c | 5h |
| 三 | arm_itm_c | 6h |
| 四 | 集成测试 | 3.5h |
| **总计** | | **26.2h** |

---

## 并行化建议

```
时间线:
T0 ─── T1 ─── T2 ─── T3 ─── T4 ─── T5 ─── T6 ─── T7

线程1: [1.1+1.2] [crsf_c    ] [arm_tpiu_c        ] [4.1+4.2]
线程2:           [bt_h4_c   ] [arm_itm_c          ] [4.3    ]
线程3:           [boost_c   ]
```

- Task 2.1/2.2/2.3 可完全并行
- Task 3.1 和 3.2 可并行开发，但堆叠测试需串行
- 集成测试在所有开发完成后进行
