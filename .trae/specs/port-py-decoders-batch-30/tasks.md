# 移植任务分解 — Batch 30

## 任务总览

5 个 UART 上层解码器的 Python → C 移植，按复杂度从低到高排序执行。

---

## Task 1：pan1321_c — Panasonic PAN1321 蓝牙模块

**复杂度**：★☆☆☆☆（最低）
**预估工作量**：2-3 小时
**文件**：`libsigrokdecode/c_decoders/pan1321_c.c`

### 1.1 创建文件骨架

- [ ] 创建 `pan1321_c.c` 文件
- [ ] 包含头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 定义 `pan1321_ann` 枚举（3 个：ANN_TEXT_VERBOSE, ANN_TEXT, ANN_WARNINGS）
- [ ] 定义 `pan1321_state` 结构体

### 1.2 实现静态数据

- [ ] `pan1321_ann_labels[3][3]`：第一列 `""`
- [ ] `pan1321_ann_rows[]`：定义 3 个 row（text-verbose, text, warnings）
- [ ] `pan1321_inputs[] = {"uart"}`
- [ ] `pan1321_tags[] = {"Wireless/RF"}`

### 1.3 实现核心函数

- [ ] `pan1321_reset()`：初始化私有状态，清空 cmd 缓冲区
- [ ] `pan1321_start()`：注册 SRD_OUTPUT_ANN 输出
- [ ] `pan1321_decode()`：空函数 `(void)di;`
- [ ] `pan1321_destroy()`：释放私有状态

### 1.4 实现 recv_proto 回调

- [ ] 过滤非 `"DATA"` 命令
- [ ] 提取 `byte_val = data[0]`, `rxtx = data[1]`
- [ ] 按 rxtx 分别缓冲 ASCII 字符到 `s->cmd[rxtx]`
- [ ] 记录 `ss_block`（命令起始位置）
- [ ] 检测 `\r\n` 结束符
- [ ] 完成时调用 `pan1321_handle_host_command()` 或 `pan1321_handle_device_reply()`

### 1.5 实现命令解析

- [ ] `pan1321_handle_host_command()`：解析 AT+JAAC, AT+JPRO, AT+JRES, AT+JSDA, AT+JSEC, AT+JSLN
- [ ] `pan1321_handle_device_reply()`：解析 ROK, OK, ERR, 未知回复
- [ ] 每个命令输出 verbose + short 两级 annotation

### 1.6 实现结构体和入口

- [ ] `pan1321_c_decoder` 结构体定义
- [ ] `srd_c_decoder_entry()` 函数
- [ ] `srd_c_decoder_api_version()` 函数

### 1.7 更新 CMakeLists.txt

- [ ] 在 `C_DECODERS` 列表中添加 `pan1321_c`

---

## Task 2：pn532_c — PN532 NFC 收发器

**复杂度**：★★★☆☆
**预估工作量**：4-5 小时
**文件**：`libsigrokdecode/c_decoders/pn532_c.c`

### 2.1 创建文件骨架

- [ ] 创建 `pn532_c.c` 文件
- [ ] 定义 `pn532_state` 枚举（6 个状态）
- [ ] 定义 `pn532_frame_type` 枚举（5 种帧类型）
- [ ] 定义 `pn532_ann` 枚举（12 个 annotation）
- [ ] 定义 `pn532_byte_data` 结构体（byte_val, ss, es）
- [ ] 定义 `pn532_state` 结构体

### 2.2 实现静态数据

- [ ] `pn532_ann_labels[12][3]`
- [ ] `pn532_ann_rows[]`：4 个 row（data_vals, frame_type, commands, errors）
- [ ] `pn532_inputs[] = {"uart"}`
- [ ] `pn532_outputs[] = {"ISO14443"}`
- [ ] `pn532_tags[] = {"Automotive"}`
- [ ] `pn532_options[]`：4 个选项（preamble, postamble, start_frame, format）
- [ ] 命令查找表：`miscellaneous[]`, `rf_communication[]`, `initiator[]`, `target[]`, `errors[]`

### 2.3 实现核心函数

- [ ] `pn532_reset()`：初始化状态机为 START_FRAME
- [ ] `pn532_start()`：注册 SRD_OUTPUT_ANN 和 SRD_OUTPUT_PYTHON 输出；读取选项
- [ ] `pn532_decode()`：空函数
- [ ] `pn532_destroy()`：释放私有状态

### 2.4 实现 recv_proto 回调

- [ ] 过滤非 `"DATA"` 命令
- [ ] 构造 `pn532_byte_data` 结构体
- [ ] 根据 `s->state` 分发到对应 handler

### 2.5 实现状态机 handler

- [ ] `pn532_handle_start_frame()`：缓冲前 3 字节，检测 `00 00 FF` 模式，输出 Preamble + Start Frame annotation
- [ ] `pn532_handle_length()`：读取 2 字节长度，检测 ACK/NACK/Extended/Normal 帧
  - ACK：`00 FF` → 设 frame_type=ACK，转 END_FRAME
  - NACK：`FF 00` → 设 frame_type=NACK，转 END_FRAME
  - Normal：计算 data_size，验证 LCS，转 TFI
- [ ] `pn532_handle_tfi()`：读取 TFI 字节，判断帧方向
  - 0xD4：Host→PN532
  - 0xD5：PN532→Host
  - 0x7F：Error frame
- [ ] `pn532_handle_data()`：缓冲 data_size 个数据字节，完成后转 CHECKSUM
- [ ] `pn532_handle_checksum()`：验证 DCS，输出 OK/Error annotation，转 END_FRAME
- [ ] `pn532_handle_end_frame()`：输出 frame_type annotation，调用命令解码，重置状态机

### 2.6 实现辅助函数

- [ ] `pn532_checksum()`：验证 `(sum(bytes) + checksum) & 0xFF == 0`
- [ ] `pn532_format_value()`：根据 format 选项格式化数值
- [ ] `pn532_handle_command_default()`：根据 TFI 和命令字节查找命令名称

### 2.7 实现结构体和入口

- [ ] `pn532_c_decoder` 结构体定义
- [ ] `srd_c_decoder_entry()`：设置选项默认值和可选值
- [ ] `srd_c_decoder_api_version()`

### 2.8 更新 CMakeLists.txt

- [ ] 在 `C_DECODERS` 列表中添加 `pn532_c`

---

## Task 3：j1708_c — SAE J1708 车辆通信

**复杂度**：★★★☆☆
**预估工作量**：4-5 小时
**文件**：`libsigrokdecode/c_decoders/j1708_c.c`

### 3.1 创建文件骨架

- [ ] 创建 `j1708_c.c` 文件
- [ ] 定义 `j1708_ann` 枚举（6 个）
- [ ] 定义 `j1708_state` 枚举（2 个：IDLE, IN_MESSAGE）
- [ ] 定义 `j1708_state` 结构体（含 data 缓冲区、采样位置记录、bit_width 等）

### 3.2 实现静态数据

- [ ] `j1708_ann_labels[6][3]`
- [ ] `j1708_ann_rows[]`：4 个 row（fields, data, errors, delays）
- [ ] `j1708_binary[]`：3 个 binary 输出（mid, payload, crc）
- [ ] `j1708_inputs[] = {"uart"}`
- [ ] `j1708_tags[] = {"Automotive"}`
- [ ] `j1708_options[]`：1 个选项（message_break）

### 3.3 实现核心函数

- [ ] `j1708_reset()`：初始化状态、清空数据缓冲区
- [ ] `j1708_start()`：注册 SRD_OUTPUT_ANN 和 SRD_OUTPUT_BINARY 输出；读取 message_break 选项
- [ ] `j1708_decode()`：空函数
- [ ] `j1708_destroy()`：释放私有状态
- [ ] `j1708_metadata()`：接收 SRD_CONF_SAMPLERATE，计算 bit_width

### 3.4 实现 recv_proto 回调

- [ ] 过滤非 `"DATA"` 命令
- [ ] 过滤非 RX 数据（`rxtx != 0`）
- [ ] 检查消息间隔：`(start_sample - prev_stopbit_es) / bit_width > message_break`
- [ ] 间隔超限时调用 `j1708_flush_message()` 刷新前一条消息
- [ ] 记录第一个字节的 start_sample
- [ ] 缓冲数据字节
- [ ] 更新 `prev_stopbit_es`

### 3.5 实现消息刷新

- [ ] `j1708_flush_message()`：
  - 验证 checksum（调用 `j1708_checksum()`）
  - checksum 正确：输出 MID + Payload + CRC 字段 annotation + binary
  - checksum 错误：输出 inline_error + error annotation
- [ ] `j1708_checksum()`：实现二进制补码校验和
  ```c
  uint8_t sum = 0;
  for (int i = 0; i < len; i++) sum = (sum + msg[i]) & 0xFF;
  return (~sum + 1) & 0xFF;
  ```

### 3.6 实现延迟测量

- [ ] `j1708_flush_delay_measurement()`：输出消息间延迟 annotation
- [ ] 检查总线访问时间违规（< MIN_BUS_ACCESS_BIT_TIMES = 12）

### 3.7 实现结构体和入口

- [ ] `j1708_c_decoder` 结构体定义（含 `.metadata = j1708_metadata`）
- [ ] `srd_c_decoder_entry()`：设置 message_break 默认值 2 和可选值 (2, 10, 12)
- [ ] `srd_c_decoder_api_version()`

### 3.8 更新 CMakeLists.txt

- [ ] 在 `C_DECODERS` 列表中添加 `j1708_c`

---

## Task 4：modbus_c — Modbus RTU 工业协议

**复杂度**：★★★★☆
**预估工作量**：6-8 小时
**文件**：`libsigrokdecode/c_decoders/modbus_c.c`

### 4.1 创建文件骨架

- [ ] 创建 `modbus_c.c` 文件
- [ ] 定义 `modbus_ann` 枚举（15 个）
- [ ] 定义 `modbus_adu` 结构体（数据缓冲区、采样位置数组、帧状态）
- [ ] 定义 `modbus_state` 结构体（双 ADU + bitlength + 选项）

### 4.2 实现静态数据

- [ ] `modbus_ann_labels[15][3]`
- [ ] `modbus_ann_rows[]`：3 个 row（sc, cs, error-indicators）
- [ ] `modbus_inputs[] = {"uart"}`
- [ ] `modbus_outputs[] = {"modbus"}`
- [ ] `modbus_tags[] = {"Embedded/industrial"}`
- [ ] `modbus_options[]`：3 个选项（scchannel, cschannel, framegap）

### 4.3 实现核心函数

- [ ] `modbus_reset()`：初始化双 ADU
- [ ] `modbus_start()`：注册 SRD_OUTPUT_ANN；读取选项
- [ ] `modbus_decode()`：空函数
- [ ] `modbus_destroy()`：释放私有状态

### 4.4 实现 CRC 算法

- [ ] `modbus_calc_crc()`：Modbus CRC-16
  ```c
  uint16_t crc = 0xFFFF;
  for (int i = 0; i < len; i++) {
      crc ^= data[i];
      for (int j = 0; j < 8; j++) {
          if (crc & 1) crc = (crc >> 1) ^ 0xA001;
          else crc >>= 1;
      }
  }
  return crc;
  ```

### 4.5 实现 recv_proto 回调

- [ ] 首次收到 STARTBIT/STOPBIT 时推算 bitlength
- [ ] 过滤非 `"DATA"` 命令
- [ ] 根据 rxtx 分发到 SC 或 CS ADU
- [ ] 检查帧间隔：`(start_sample - adu->last_read) > bitlength * framegap`
- [ ] 帧间隔超限时关闭当前 ADU，开始新帧

### 4.6 实现 ADU 解析

- [ ] `modbus_decode_adu()`：添加数据字节到 ADU，调用 parse
- [ ] `modbus_adu_sc_parse()`：Server→Client 帧解析
  - 功能码 1-2：Read Bits
  - 功能码 3-4, 23：Read Registers
  - 功能码 5：Write Single Coil
  - 功能码 6：Write Single Register
  - 功能码 7：Read Exception Status
  - 功能码 8：Diagnostics
  - 功能码 11：Get Comm Event Counter
  - 功能码 12：Get Comm Event Log
  - 功能码 15-16：Write Multiple
  - 功能码 17：Report Server ID
  - 功能码 22：Mask Write Register
  - 功能码 >0x80：Error Response
- [ ] `modbus_adu_cs_parse()`：Client→Server 帧解析
  - 功能码 1-4：Read Data Command
  - 功能码 5：Write Single Coil
  - 功能码 6：Write Single Register
  - 功能码 7, 11, 12, 17：Single Byte Request
  - 功能码 8：Diagnostics
  - 功能码 15-16：Write Multiple
  - 功能码 22：Mask Write Register
  - 功能码 23：Read/Write Registers

### 4.7 实现 annotation 输出

- [ ] `modbus_puta()`：根据 annotation 字符串名称输出 annotation
- [ ] `modbus_puti()`：输出指定字节范围的 annotation
- [ ] `modbus_check_crc()`：验证 CRC 并输出结果

### 4.8 实现结构体和入口

- [ ] `modbus_c_decoder` 结构体定义
- [ ] `srd_c_decoder_entry()`：设置选项默认值
  - scchannel: "RX", values: ("RX", "TX")
  - cschannel: "TX", values: ("RX", "TX")
  - framegap: 28
- [ ] `srd_c_decoder_api_version()`

### 4.9 更新 CMakeLists.txt

- [ ] 在 `C_DECODERS` 列表中添加 `modbus_c`

---

## Task 5：midi_c — MIDI 音乐设备数字接口

**复杂度**：★★★★★（最高）
**预估工作量**：10-12 小时
**文件**：`libsigrokdecode/c_decoders/midi_c.c`

### 5.1 创建文件骨架

- [ ] 创建 `midi_c.c` 文件
- [ ] 定义 `midi_state` 枚举（6 个状态）
- [ ] 定义 `midi_ann` 枚举（3 个）
- [ ] 定义 `midi_state` 结构体（status_byte, cmd 缓冲区, 采样位置等）

### 5.2 实现查找表（最大工作量）

- [ ] `status_bytes[256][3]`：状态字节名称（仅 0x80-0xFF 有效条目）
- [ ] `control_functions[128][3]`：控制器功能名称
- [ ] `channel_mode_names[8][3]`：通道模式名称（0x78-0x7F）
- [ ] `chromatic_notes[128]`：音名（C-1 到 G9）
- [ ] `percussion_notes[128]`：打击乐名称（仅 channel 10）
- [ ] `gm_instruments[128]`：通用 MIDI 乐器名
- [ ] `quarter_frame_type[8][2]`：MIDI Time Code 四帧类型
- [ ] `smpte_type[4]`：SMPTE 类型
- [ ] `sysex_manufacturer_ids[]`：SysEx 厂商 ID 查找表（结构体数组 + 线性搜索）

### 5.3 实现静态数据

- [ ] `midi_ann_labels[3][3]`
- [ ] `midi_ann_rows[]`：2 个 row（normal, sys-real）
- [ ] `midi_inputs[] = {"uart"}`
- [ ] `midi_tags[] = {"Audio", "PC"}`

### 5.4 实现核心函数

- [ ] `midi_reset()`：初始化状态为 IDLE，清空 status_byte 和 cmd
- [ ] `midi_start()`：注册 SRD_OUTPUT_ANN 输出
- [ ] `midi_decode()`：空函数
- [ ] `midi_destroy()`：释放私有状态

### 5.5 实现 recv_proto 回调

- [ ] 过滤非 `"DATA"` 命令
- [ ] 实现状态路由逻辑（与 Python 版 decode() 一致）
- [ ] 处理 Running Status 机制
- [ ] 处理 SysRealtime 中断（保存/恢复状态）

### 5.6 实现通道消息处理

- [ ] `midi_handle_channel_msg_0x80()`：Note Off
- [ ] `midi_handle_channel_msg_0x90()`：Note On（velocity=0 时为 Note Off）
- [ ] `midi_handle_channel_msg_0xa0()`：Polyphonic Key Pressure
- [ ] `midi_handle_channel_msg_0xb0()`：Control Change / Channel Mode
  - 特殊处理 cc 0x44（Legato Footswitch）
  - 特殊处理 cc 0x54（Portamento Control）
  - cc 0x78-0x7F：Channel Mode Messages
- [ ] `midi_handle_channel_msg_0xc0()`：Program Change
- [ ] `midi_handle_channel_msg_0xd0()`：Channel Pressure
- [ ] `midi_handle_channel_msg_0xe0()`：Pitch Bend Change

### 5.7 实现系统消息处理

- [ ] `midi_handle_sysex_msg()`：System Exclusive
  - 提取厂商 ID（1 字节或 3 字节）
  - 查找厂商名称
  - 输出 payload
- [ ] `midi_handle_syscommon_msg()`：System Common
  - 0xF1：MIDI Time Code Quarter Frame
  - 0xF2：Song Position Pointer
  - 0xF3：Song Select
  - 0xF4/0xF5/0xF6：Undefined/Tune Request
- [ ] `midi_handle_sysrealtime_msg()`：System Real-Time
  - 保存/恢复 ss_block, es_block
  - 不重置 cmd 和 state

### 5.8 实现辅助函数

- [ ] `midi_get_next_state()`：根据字节值确定下一个状态
- [ ] `midi_handle_state()`：状态分发
- [ ] `midi_handle_garbage_msg()`：处理未识别数据
- [ ] `midi_get_note_name()`：获取音名（区分普通通道和打击乐通道）
- [ ] `midi_check_for_garbage_flush()`：检查是否需要刷新垃圾数据

### 5.9 实现结构体和入口

- [ ] `midi_c_decoder` 结构体定义
- [ ] `srd_c_decoder_entry()`：无选项
- [ ] `srd_c_decoder_api_version()`

### 5.10 更新 CMakeLists.txt

- [ ] 在 `C_DECODERS` 列表中添加 `midi_c`

---

## 构建与测试

### Task 6：编译验证

- [ ] 运行 `build_incremental.cmd`
- [ ] 确认 5 个新 DLL 成功编译到 `build.dir/decoders/c_decoders/`
- [ ] 检查无编译警告

### Task 7：功能验证

- [ ] 启动 PXView，加载含 UART 信号的捕获文件
- [ ] 添加 UART C 解码器
- [ ] 在 UART 上层分别添加 5 个新 C 解码器
- [ ] 对比 C 版本与 Python 版本的 annotation 输出
- [ ] 验证关键场景：
  - J1708：消息分隔、checksum 验证
  - MIDI：Running Status、SysRealtime 中断、SysEx
  - Modbus：CRC 验证、功能码解析、双通道
  - PAN1321：AT 命令解析、\r\n 分隔
  - PN532：帧格式、ACK/NACK、checksum 验证

---

## 依赖关系

```
pan1321_c ──→ (无依赖，可独立开始)
pn532_c   ──→ (无依赖，可独立开始)
j1708_c   ──→ (需要 metadata 回调模式参考)
modbus_c  ──→ (需要 j1708_c 的 bitlength 推算模式参考)
midi_c    ──→ (最复杂，建议最后实现)
```

建议执行顺序：pan1321_c → pn532_c → j1708_c → modbus_c → midi_c
