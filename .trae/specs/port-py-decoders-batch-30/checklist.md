# 移植验证清单 — Batch 30

本清单用于逐项验证每个解码器的 C 移植质量。每项必须通过才能标记为完成。

---

## 通用验证项（适用于所有 5 个解码器）

### 文件结构

- [ ] 文件名格式正确：`{decoder_id}_c.c`
- [ ] 文件位于 `libsigrokdecode/c_decoders/` 目录
- [ ] 包含必要的头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 文件末尾有 `srd_c_decoder_entry()` 和 `srd_c_decoder_api_version()` 导出函数
- [ ] 导出宏使用 `SRD_C_DECODER_EXPORT`

### srd_c_decoder 结构体

- [ ] `.id` 以 `_c` 结尾（如 `j1708_c`）
- [ ] `.name` 格式为 `XXX(C)`
- [ ] `.longname` 包含 `(C)` 后缀
- [ ] `.desc` 描述包含 `C implementation`
- [ ] `.license` 与 Python 版本一致
- [ ] `.channels = NULL`, `.num_channels = 0`（上层解码器无通道）
- [ ] `.optional_channels = NULL`, `.num_optional_channels = 0`
- [ ] `.inputs` 设置为 `{"uart"}`
- [ ] `.decode` 指向空函数（`(void)di;`）
- [ ] `.recv_proto` 指向实现的回调函数
- [ ] `.reset` 指向实现的 reset 函数
- [ ] `.start` 指向实现的 start 函数
- [ ] `.destroy` 指向实现的 destroy 函数

### ann_labels 规范

- [ ] 第一列全部为 `""`
- [ ] 第二列为短 ID
- [ ] 第三列为完整描述
- [ ] 条目数与 `NUM_ANN` 一致
- [ ] 与 Python 版本的 annotations 一一对应

### annotation_rows 规范

- [ ] 每个 row 的 classes 数组以 `-1` 结尾
- [ ] 所有 annotation class 都被映射到某个 row
- [ ] row 名称与 Python 版本一致
- [ ] `num_annotation_rows` 与数组长度一致

### recv_proto 实现

- [ ] 函数签名正确：`void xxx_recv_proto(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 获取私有状态前检查 NULL
- [ ] 正确过滤 `cmd` 类型（通常只处理 `"DATA"`）
- [ ] 正确提取 `data[0]`（byte_value）和 `data[1]`（rxtx）
- [ ] 正确处理 RX/TX 方向

### 内存管理

- [ ] `reset()` 中使用 `g_malloc0` 分配私有状态（首次）
- [ ] `reset()` 中使用 `memset` 清零（后续调用）
- [ ] `destroy()` 中使用 `g_free` 释放私有状态
- [ ] `destroy()` 中将 private 设为 NULL
- [ ] 无内存泄漏

### 选项处理

- [ ] 选项默认值在 `srd_c_decoder_entry()` 中设置
- [ ] 选项可选值列表在 `srd_c_decoder_entry()` 中设置
- [ ] `start()` 中使用 `c_decoder_get_option_int/string/double` 读取选项
- [ ] 选项 ID 与 Python 版本一致

### CMakeLists.txt

- [ ] `C_DECODERS` 列表中添加了新解码器名称

### 编译

- [ ] 无编译错误
- [ ] 无编译警告
- [ ] DLL 成功生成到 `build.dir/decoders/c_decoders/`

---

## pan1321_c 专项验证

### 元数据一致性

- [ ] `id = "pan1321_c"`, `name = "PAN1321(C)"`
- [ ] `inputs = {"uart"}`
- [ ] `outputs = {}`（空，与 Python 一致）
- [ ] `tags = {"Wireless/RF"}`
- [ ] 3 个 annotation 与 Python 一致
- [ ] 无 options（Python 版本无选项）

### 功能验证

- [ ] 正确缓冲 RX 和 TX 方向的 ASCII 字符
- [ ] 正确检测 `\r\n` 命令结束符
- [ ] RX 方向正确解析设备回复：
  - [ ] `ROK` → "Device initialized correctly"
  - [ ] `OK` → "Device acknowledged last command"
  - [ ] `ERR=xx` → "Device sent error code xx"
  - [ ] 未知回复 → "Device sent an unknown reply"
- [ ] TX 方向正确解析主机命令：
  - [ ] `AT+JAAC=0/1` → Auto-accept 解析
  - [ ] `AT+JPRO=0/1` → Production mode 解析
  - [ ] `AT+JRES` → Software reset 解析
  - [ ] `AT+JSDA=len,data` → Send data 解析
  - [ ] `AT+JSEC=...` → Security settings 解析（至少 PIN 提取）
  - [ ] `AT+JSLN=len,name` → Bluetooth name 解析
  - [ ] 其他命令 → "Unsupported command"
- [ ] 无效参数输出 warning annotation
- [ ] ss_block 正确记录命令起始位置

---

## pn532_c 专项验证

### 元数据一致性

- [ ] `id = "pn532_c"`, `name = "PN532(C)"`
- [ ] `inputs = {"uart"}`
- [ ] `outputs = {"ISO14443"}`
- [ ] `tags = {"Automotive"}`
- [ ] 12 个 annotation 与 Python 一致
- [ ] 4 个 annotation_rows 与 Python 一致
- [ ] 4 个 options 与 Python 一致（preamble, postamble, start_frame, format）

### 功能验证

- [ ] START_FRAME 状态：正确检测 `00 00 FF` 前导码
  - [ ] 输出 Preamble annotation
  - [ ] 输出 Start Frame annotation
- [ ] LENGTH 状态：
  - [ ] ACK 帧（`00 FF`）正确识别
  - [ ] NACK 帧（`FF 00`）正确识别
  - [ ] Normal 帧：正确计算 data_size，验证 LCS
  - [ ] LCS 校验失败输出 Error annotation
- [ ] TFI 状态：
  - [ ] 0xD4 → HOST_TO_PN532
  - [ ] 0xD5 → PN532_TO_HOST
  - [ ] 0x7F → ERROR
- [ ] DATA 状态：正确缓冲 data_size 个字节
- [ ] CHECKSUM 状态：
  - [ ] DCS 校验正确输出 "OK"
  - [ ] DCS 校验失败输出 Error annotation
- [ ] END_FRAME 状态：
  - [ ] 输出 frame_type annotation（Host to PN532 / PN532 to Host / ACK / NACK / Error）
  - [ ] 输出 Postamble annotation
  - [ ] 调用命令解码
- [ ] 命令解码：
  - [ ] TFI=0xD4 时查找命令名称
  - [ ] miscellaneous 命令正确识别
  - [ ] rf_communication 命令正确识别
  - [ ] initiator 命令正确识别
  - [ ] target 命令正确识别
- [ ] format 选项正确应用（hex/dec/oct/bin/ascii）
- [ ] 状态机在帧结束后正确重置

---

## j1708_c 专项验证

### 元数据一致性

- [ ] `id = "j1708_c"`, `name = "J1708(C)"`
- [ ] `inputs = {"uart"}`
- [ ] `outputs = {}`（空，与 Python 一致）
- [ ] `tags = {"Automotive"}`
- [ ] 6 个 annotation 与 Python 一致
- [ ] 4 个 annotation_rows 与 Python 一致
- [ ] 3 个 binary 输出与 Python 一致
- [ ] 1 个 option（message_break）与 Python 一致

### 功能验证

- [ ] 仅处理 RX 数据（`rxtx == 0`），忽略 TX
- [ ] 忽略 FRAME、BREAK、INVALID STOPBIT 类型
- [ ] metadata 回调正确接收 samplerate 并计算 bit_width
- [ ] 消息间隔检测：
  - [ ] 间隔 > message_break bit times 时刷新前一条消息
  - [ ] message_break 默认值为 2
  - [ ] message_break 可选值为 (2, 10, 12)
- [ ] Checksum 验证：
  - [ ] 正确实现二进制补码校验和
  - [ ] 校验通过：输出 MID + Payload + CRC 字段
  - [ ] 校验失败：输出 inline_error + error annotation
- [ ] 字段输出：
  - [ ] MID 字段：正确输出 hex 格式
  - [ ] Payload 字段：正确输出 hex 格式
  - [ ] CRC 字段：正确输出 hex 格式
- [ ] Binary 输出：
  - [ ] BINARY_MID：输出 MID 字节
  - [ ] BINARY_PAYLOAD：输出 payload 字节
  - [ ] BINARY_CRC：输出 checksum 字节
- [ ] 延迟测量：
  - [ ] 正确输出消息间延迟（bit times 格式）
  - [ ] 总线访问时间违规（< 12 bit times）输出 bus_access annotation
- [ ] 空消息不导致崩溃

---

## modbus_c 专项验证

### 元数据一致性

- [ ] `id = "modbus_c"`, `name = "Modbus(C)"`
- [ ] `inputs = {"uart"}`
- [ ] `outputs = {"modbus"}`
- [ ] `tags = {"Embedded/industrial"}`
- [ ] 15 个 annotation 与 Python 一致
- [ ] 3 个 annotation_rows 与 Python 一致
- [ ] 3 个 options 与 Python 一致

### 功能验证

- [ ] bitlength 推算：从 STARTBIT/STOPBIT 的采样范围计算
- [ ] 双通道解码：
  - [ ] SC（Server→Client）通道正确映射
  - [ ] CS（Client→Server）通道正确映射
  - [ ] 同一 UART 通道可同时映射为 SC 和 CS
- [ ] 帧分隔：framegap（默认 28 bit times）正确判断帧边界
- [ ] CRC-16 验证：
  - [ ] Modbus CRC 算法正确（初始值 0xFFFF，多项式 0xA001）
  - [ ] CRC 正确输出 "CRC correct"
  - [ ] CRC 错误输出期望值
- [ ] SC 帧解析（Server→Client 响应）：
  - [ ] 功能码 1-2：Read Bits（byte count + data）
  - [ ] 功能码 3-4：Read Registers（byte count + register data）
  - [ ] 功能码 5：Write Single Coil（address + ON/OFF）
  - [ ] 功能码 6：Write Single Register（address + value）
  - [ ] 功能码 7：Read Exception Status
  - [ ] 功能码 8：Diagnostics（subfunction + data）
  - [ ] 功能码 11：Get Comm Event Counter
  - [ ] 功能码 12：Get Comm Event Log
  - [ ] 功能码 15-16：Write Multiple（address + count）
  - [ ] 功能码 17：Report Server ID
  - [ ] 功能码 22：Mask Write Register
  - [ ] 功能码 >0x80：Error Response（error code 解析）
- [ ] CS 帧解析（Client→Server 请求）：
  - [ ] 功能码 1-4：Read Data Command（address + quantity）
  - [ ] 功能码 5：Write Single Coil
  - [ ] 功能码 6：Write Single Register
  - [ ] 功能码 7, 11, 12, 17：Single Byte Request
  - [ ] 功能码 8：Diagnostics
  - [ ] 功能码 15-16：Write Multiple（含 byte count + data）
  - [ ] 功能码 22：Mask Write Register
  - [ ] 功能码 23：Read/Write Multiple Registers
- [ ] Slave ID 验证：
  - [ ] ID 0：Broadcast message
  - [ ] ID 1-247：正常
  - [ ] ID 248-255：Reserved address
- [ ] 错误指示：
  - [ ] 帧过短输出 error
  - [ ] 帧含错误时输出 error-indication
  - [ ] 帧超过 256 字节输出 error
- [ ] 每个字节的 start_sample/end_sample 正确记录

---

## midi_c 专项验证

### 元数据一致性

- [ ] `id = "midi_c"`, `name = "MIDI(C)"`
- [ ] `inputs = {"uart"}`
- [ ] `outputs = {}`（空，与 Python 一致）
- [ ] `tags = {"Audio", "PC"}`
- [ ] 3 个 annotation 与 Python 一致
- [ ] 2 个 annotation_rows 与 Python 一致
- [ ] 无 options（与 Python 一致）

### 查找表完整性

- [ ] `status_bytes`：覆盖 0x80-0xFF 所有状态字节
- [ ] `control_functions`：覆盖 0x00-0x7F 所有控制器
- [ ] `chromatic_notes`：覆盖 0-127 所有音名
- [ ] `percussion_notes`：覆盖常用打击乐（35-81）
- [ ] `gm_instruments`：覆盖 1-128 所有通用 MIDI 乐器
- [ ] `quarter_frame_type`：覆盖 0-7
- [ ] `smpte_type`：覆盖 0-3
- [ ] `sysex_manufacturer_ids`：至少包含 Python 版本中的主要厂商

### 功能验证

- [ ] Channel Voice Messages：
  - [ ] 0x80 Note Off：channel + note + velocity
  - [ ] 0x90 Note On：velocity=0 时显示为 Note Off
  - [ ] 0xA0 Polyphonic Key Pressure
  - [ ] 0xB0 Control Change：cc 0x00-0x77 正确查找名称
  - [ ] 0xB0 Channel Mode：cc 0x78-0x7F 正确解析
  - [ ] 0xC0 Program Change：乐器名查找
  - [ ] 0xD0 Channel Pressure
  - [ ] 0xE0 Pitch Bend：LSB+MSB 合并为 14 位值
- [ ] Running Status：
  - [ ] 连续相同类型消息省略 status byte 时正确处理
  - [ ] status byte 被缓存
  - [ ] 新 status byte 替换旧缓存
- [ ] System Exclusive：
  - [ ] 0xF0 开始，0xF7 或其他 status byte 结束
  - [ ] 1 字节厂商 ID 正确查找
  - [ ] 3 字节厂商 ID（首字节 0x00）正确查找
  - [ ] 未找到厂商 ID 时显示 "undefined"
  - [ ] payload 正确输出
  - [ ] 截断的厂商 ID 输出错误
- [ ] System Common：
  - [ ] 0xF1 MIDI Time Code Quarter Frame：nn + dd 解析
  - [ ] 0xF2 Song Position Pointer：14 位值
  - [ ] 0xF3 Song Select
  - [ ] 0xF4/0xF5 Undefined
  - [ ] 0xF6 Tune Request
- [ ] System Real-Time：
  - [ ] 0xF8-0xFF 所有实时消息正确识别
  - [ ] 实时消息不重置 cmd 和 state
  - [ ] 实时消息输出到 SysReal row（ANN_TEXT_SYSREAL_VERBOSE）
  - [ ] 实时消息处理完后恢复之前的状态
- [ ] Garbage 处理：
  - [ ] 0xF7 作为首字节 → BUFFER GARBAGE
  - [ ] 无效 status byte → BUFFER GARBAGE
  - [ ] 未处理数据正确输出 UNHANDLED DATA
- [ ] 消息中断处理：
  - [ ] 新 status byte 到达时正确刷新前一条消息
  - [ ] 刷新时不丢失数据

---

## 回归测试

### 与 Python 版本对比

- [ ] 同一捕获文件，C 版本与 Python 版本的 annotation 数量一致
- [ ] C 版本与 Python 版本的 annotation 文本内容一致（允许格式微小差异）
- [ ] C 版本与 Python 版本的 annotation 时间范围一致
- [ ] C 版本与 Python 版本的 binary 输出一致（J1708）

### 性能

- [ ] C 版本解码速度显著快于 Python 版本（预期 5-10x）
- [ ] 大文件（>10MB 采样数据）不导致内存溢出
- [ ] 无内存泄漏（长时间运行稳定）

### 边界情况

- [ ] 空输入不崩溃
- [ ] 损坏的 UART 数据不崩溃
- [ ] 超长消息不导致缓冲区溢出
- [ ] 采样率为 0 时不崩溃（J1708）
- [ ] bitlength 为 0 时不崩溃（Modbus）
