# Python → C 解码器移植检查清单 — Batch 35

## 通用检查项（适用于所有 5 个解码器）

### 文件结构检查
- [ ] 文件名格式正确：`{decoder_id}_c.c`
- [ ] 包含标准头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 文件末尾有换行符

### srd_c_decoder 结构体检查
- [ ] `.id` 以 `_c` 结尾（如 `"cfp_c"`, `"ps2_keyboard_c"`）
- [ ] `.name` 以 `(C)` 后缀结尾（如 `"CFP(C)"`, `"PS/2 Keyboard(C)"`）
- [ ] `.longname` 包含完整描述
- [ ] `.desc` 包含 "C implementation" 字样
- [ ] `.license` 与 Python 版本一致
- [ ] `.channels = NULL`, `.num_channels = 0`（上层解码器无通道）
- [ ] `.optional_channels = NULL`, `.num_optional_channels = 0`
- [ ] `.inputs` 正确设置下层输出类型名
- [ ] `.num_inputs` 正确
- [ ] `.outputs` 正确设置（可为 `{NULL}`）
- [ ] `.num_outputs` 正确
- [ ] `.num_annotations = NUM_ANN`
- [ ] `.ann_labels` 第一列全部为 `""`
- [ ] `.annotation_rows` 所有 ann class 都映射到某个 row
- [ ] `.binary` 正确设置（可为 NULL）
- [ ] `.num_binary` 正确
- [ ] `.tags` 正确设置
- [ ] `.num_tags` 正确
- [ ] `.reset` 函数指针正确
- [ ] `.start` 函数指针正确
- [ ] `.decode` 函数指针正确（空函数）
- [ ] `.destroy` 函数指针正确
- [ ] `.recv_proto` 函数指针正确（核心逻辑）
- [ ] `.metadata` 如需要则设置（仅 usb_request_c 需要）

### 函数实现检查
- [ ] `reset()`: 使用 `g_malloc0` 分配私有状态（首次时），`memset` 清零
- [ ] `start()`: 使用 `c_decoder_register_output()` 注册所有输出
- [ ] `decode()`: 空函数 `(void)di;`
- [ ] `destroy()`: 使用 `g_free` 释放私有状态，`c_decoder_set_private(di, NULL)`
- [ ] `recv_proto()`: 正确解析 cmd 和 data 参数
- [ ] `srd_c_decoder_entry()`: 初始化所有 option 的 `def` 和 `values`
- [ ] `srd_c_decoder_api_version()`: 返回 `SRD_C_DECODER_API_VERSION`

### 编码规范检查
- [ ] 无编译警告
- [ ] 使用 `C_ANN_PUT` 宏输出 annotation
- [ ] 使用 `c_decoder_put_python` 输出 python 数据
- [ ] 使用 `c_decoder_put_binary` 输出 binary 数据
- [ ] 私有状态通过 `c_decoder_get_private` / `c_decoder_set_private` 管理
- [ ] option 值通过 `c_decoder_get_option_string` / `c_decoder_get_option_int` / `c_decoder_get_option_double` 读取

### CMakeLists.txt 检查
- [ ] 解码器名已添加到 `C_DECODERS` 列表

---

## Task 0: ps2_c.c 修改检查

- [ ] `ps2_outputs` 已修改为 `{"ps2", NULL}`
- [ ] `ps2_c_decoder.num_outputs` 已修改为 `1`
- [ ] `ps2_priv` 中添加了 `int out_python` 字段
- [ ] `ps2_start()` 中注册了 python 输出
- [ ] `ps2_reset()` 中初始化 `s->out_python = -1`
- [ ] `ps2_handle_byte()` 中添加了 `c_decoder_put_python` 调用
- [ ] Python 输出格式：cmd="BYTE", data=[val, is_host, parity_ok, has_ack]
- [ ] 编译通过，无警告
- [ ] ps2_c 独立使用功能正常（annotation 输出不变）
- [ ] ps2_c + Python ps2_keyboard 堆叠功能正常

---

## Task 1: cfp_c.c 检查

### 元数据一致性
- [ ] `id = "cfp_c"`
- [ ] `name = "CFP(C)"`
- [ ] `inputs = {"mdio", NULL}`
- [ ] `outputs = {NULL}`
- [ ] `tags = {"Networking", NULL}`
- [ ] `license = "bsd"`（与 Python 版本一致）
- [ ] 2 个 annotations: REGISTER, DECODE
- [ ] 2 个 annotation rows: registers, decodes

### 解码逻辑检查
- [ ] 仅处理 `cmd == "DATA"` 的 recv_proto 回调
- [ ] 正确解析 8 字节 mdio python 数据格式
- [ ] 仅处理 `is_read == 1` 的数据包
- [ ] clause45_addr 范围映射完整（10 个地址范围）
- [ ] MODULE_ID 查找表完整（18 条记录）
- [ ] `clause45_addr == 0x8000` 时额外输出 MODULE_ID 解码
- [ ] 未识别的 MODULE_ID 返回 "Reserved"

### Python 输出格式验证
- [ ] mdio_c 的 python 输出 data 格式：`[clause45, addr_hi, addr_lo, is_read, portad, devad, data_hi, data_lo]`
- [ ] cfp_c 正确解析 `clause45_addr = (data[1] << 8) | data[2]`
- [ ] cfp_c 正确解析 `reg = (data[6] << 8) | data[7]`
- [ ] cfp_c 正确解析 `is_read = data[3]`

---

## Task 2: ps2_keyboard_c.c 检查

### 元数据一致性
- [ ] `id = "ps2_keyboard_c"`
- [ ] `name = "PS/2 Keyboard(C)"`
- [ ] `inputs = {"ps2", NULL}`
- [ ] `outputs = {NULL}`
- [ ] `tags = {"PC", NULL}`
- [ ] `license = "gplv2+"`
- [ ] 3 个 annotations: PRESS, RELEASE, ACK
- [ ] 1 个 annotation row: keys
- [ ] 1 个 binary: Keys

### 扫描码查找表检查
- [ ] 标准扫描码表（std_keys）包含所有 Python 版本的条目
  - [ ] 字母 A-Z（26 条）
  - [ ] 数字 0-9（10 条）
  - [ ] 特殊键：`~, -_, =+, \\|, Backsp, Space, Tab, Enter, Esc
  - [ ] 修饰键：L Shft, R Shft, L Ctrl, L Alt, CapsLk
  - [ ] 功能键：F1-F12, ScrLck
- [ ] 扩展扫描码表（ext_keys）包含所有 Python 版本的条目
  - [ ] L Sup, R Ctrl, R Sup, R Alt, Menu
  - [ ] PrtScr, SysRq
  - [ ] Insert, Home, Pg Up, Delete, End, Pg Dn
  - [ ] 方向键：Up, Left, Right, Down
  - [ ] KP /, KP Ent
- [ ] 未知按键格式：`[XX]`（标准）或 `[E0XX]`（扩展）

### 状态机逻辑检查
- [ ] 主机发送时（is_host==1）重置状态
- [ ] 0xF0 正确设置 Release 标记
- [ ] 0xE0 正确设置 Extended 标记
- [ ] 0xFA 正确输出 ACK 标注
- [ ] 普通按键正确查表并输出
- [ ] Press 时输出 binary 数据
- [ ] 状态重置在每次输出后执行

### recv_proto 数据格式验证
- [ ] 正确处理 cmd="BYTE"
- [ ] 正确解析 4 字节数据：`[val, is_host, parity_ok, has_ack]`

---

## Task 3: ps2_mouse_c.c 检查

### 元数据一致性
- [ ] `id = "ps2_mouse_c"`
- [ ] `name = "PS/2 Mouse(C)"`
- [ ] `inputs = {"ps2", NULL}`
- [ ] `outputs = {NULL}`
- [ ] `tags = {"PC", NULL}`
- [ ] `license = "gplv2+"`
- [ ] 1 个 annotation: MOVEMENT
- [ ] 1 个 annotation row: mov
- [ ] 2 个 binary: bytes, movement

### 鼠标数据包解码检查
- [ ] 正确解析 3 字节数据包
- [ ] Byte 0 flags 位解析：
  - [ ] bit0: L（左键）
  - [ ] bit1: M（中键）
  - [ ] bit2: R（右键）
  - [ ] bit4: X sign（X 符号位）
  - [ ] bit5: Y sign（Y 符号位）
  - [ ] bit6: X overflow
  - [ ] bit7: Y overflow
- [ ] X 位移符号扩展：`if (flags & 0x10) x -= 256`
- [ ] Y 位移符号扩展：`if (flags & 0x20) y -= 256`
- [ ] 溢出警告：`!!` 标记
- [ ] 无移动时输出 "No Movement"

### 数据包分组逻辑检查
- [ ] 正确维护 packets 列表
- [ ] host/mouse 方向切换时输出当前数据包组
- [ ] ACK 字节（0xFA, 非主机）特殊处理
- [ ] 每 3 个字节触发一次 mouse_movement 输出
- [ ] binary bytes 输出包含 "Host:" / "Mouse:" 前缀

---

## Task 4: usb_packet_c.c 检查

### 元数据一致性
- [ ] `id = "usb_packet_c"`
- [ ] `name = "USB packet(C)"`
- [ ] `inputs = {"usb_signalling", NULL}`
- [ ] `outputs = {"usb_packet", NULL}`
- [ ] `tags = {"PC", NULL}`
- [ ] `license = "gplv2+"`
- [ ] 29 个 annotations（0-28）
- [ ] 2 个 annotation rows: fields, packets
- [ ] 1 个 option: signalling

### PID 查找表检查
- [ ] Token PIDs: OUT, IN, SOF, SETUP
- [ ] Data PIDs: DATA0, DATA1, DATA2, MDATA
- [ ] Handshake PIDs: ACK, NAK, STALL, NYET
- [ ] Special PIDs: PRE, SPLIT, PING, Reserved
- [ ] 共 16 条记录

### CRC 算法检查
- [ ] `calc_crc5()` 实现正确
  - [ ] 多项式 0x25
  - [ ] 初始值 0x1F
  - [ ] 最终异或 0x1F
  - [ ] 位序反转
- [ ] `calc_crc16()` 实现正确
  - [ ] 多项式 0x18005
  - [ ] 初始值 0xFFFF
  - [ ] 最终异或 0xFFFF
  - [ ] 位序反转
- [ ] CRC 计算结果与 Python 版本一致

### 包解析逻辑检查
- [ ] SYNC 字段验证（必须为 "00000001"）
- [ ] SYNC 错误时输出 ANN_SYNC_ERR
- [ ] PID 解析正确（8 位 bitstring → 名称）
- [ ] Token 包解析：
  - [ ] SOF: FRAMENUM(11bit) + CRC5(5bit)
  - [ ] OUT/IN/SETUP/PING: ADDR(7bit) + EP(4bit) + CRC5(5bit)
  - [ ] CRC5 校验正确
- [ ] Data 包解析：
  - [ ] 数据字节按 8 位分组
  - [ ] CRC16 校验正确
- [ ] Handshake 包：仅 SYNC+PID
- [ ] Special 包：PRE 仅 SYNC+PID
- [ ] 包摘要输出正确

### recv_proto 状态机检查
- [ ] SOP → 开始收集位
- [ ] BIT → 添加到位数组（包含 ss/es 采样点）
- [ ] EOP → 触发 handle_packet
- [ ] ERR → 触发 handle_packet
- [ ] 忽略 STUFF BIT, SYM, RESET, KEEP ALIVE

### Python 输出格式检查
- [ ] PACKET 命令格式设计合理
- [ ] 包含 pcategory, pname, pinfo 数据
- [ ] usb_request_c 能正确解析

### 位序处理检查
- [ ] `bitstr_to_num()` 正确实现 LSB-first 转换
- [ ] ADDR 字段：7 位，LSB-first
- [ ] EP 字段：4 位，LSB-first
- [ ] FRAMENUM 字段：11 位，LSB-first
- [ ] CRC5 字段：5 位
- [ ] CRC16 字段：16 位

---

## Task 5: usb_request_c.c 检查

### 元数据一致性
- [ ] `id = "usb_request_c"`
- [ ] `name = "USB request(C)"`
- [ ] `inputs = {"usb_packet", NULL}`
- [ ] `outputs = {"usb_request", NULL}`
- [ ] `tags = {"PC", NULL}`
- [ ] `license = "gplv2+"`
- [ ] 5 个 annotations: SETUP_READ, SETUP_WRITE, BULK_READ, BULK_WRITE, ERROR
- [ ] 4 个 annotation rows: request-setup, request-in, request-out, errors
- [ ] 1 个 binary: pcap
- [ ] 1 个 option: in_request_start
- [ ] `.metadata` 函数指针设置（处理 samplerate）

### 事务状态机检查
- [ ] IDLE → TOKEN RECEIVED（收到 TOKEN 包）
- [ ] TOKEN RECEIVED → DATA RECEIVED（收到 DATA 包）
- [ ] DATA RECEIVED → IDLE（收到 HANDSHAKE 包）
- [ ] TOKEN RECEIVED → IDLE（收到 HANDSHAKE 包，无数据阶段）
- [ ] 事务超时检测正确

### 请求跟踪检查
- [ ] 按 (addr, ep) 对跟踪请求
- [ ] CONTROL SETUP 阶段：
  - [ ] 解析 SETUP 数据（8 字节）
  - [ ] 根据 bmRequestType 判断方向（IN/OUT）
  - [ ] 解析 wLength
- [ ] CONTROL DATA 阶段：
  - [ ] SETUP IN + IN → 收集数据
  - [ ] SETUP OUT + OUT → 收集数据（仅 ACK 时）
- [ ] CONTROL STATUS 阶段：
  - [ ] SETUP IN + OUT → 完成
  - [ ] SETUP OUT + IN → 完成
- [ ] BULK IN 处理
- [ ] BULK OUT 处理
- [ ] 协议 STALL 清除（新 SETUP 时）

### PCAP 输出检查
- [ ] PCAP 全局头格式正确
  - [ ] Magic number: 0xA1B2C3D4
  - [ ] 版本: 2.4
  - [ ] Link layer: LINKTYPE_USB_LINUX_MMAPPED (220)
- [ ] PCAP 记录头格式正确
- [ ] PCAP USB 包格式正确（64 字节头 + 数据）
- [ ] SUBMIT 记录在请求开始时输出
- [ ] COMPLETE 记录在请求完成时输出
- [ ] 时间戳从采样点正确转换

### recv_proto 数据格式验证
- [ ] 正确处理 cmd="PACKET"
- [ ] 正确解析 pcategory（TOKEN/DATA/HANDSHAKE/SPECIAL）
- [ ] 正确解析 pname（OUT/IN/SOF/SETUP/DATA0/DATA1/ACK/NAK/STALL/NYET/PRE/SPLIT/PING/Reserved）
- [ ] 正确解析 pinfo 数据

---

## 集成测试检查

### 编译测试
- [ ] 全部 5 个新 C 解码器编译为 DLL
- [ ] 修改后的 ps2_c.c 编译为 DLL
- [ ] 无编译警告
- [ ] 无链接错误

### 加载测试
- [ ] PXView 启动无错误
- [ ] 所有新解码器出现在解码器列表中
- [ ] 解码器可正确选中

### 堆叠测试
- [ ] mdio_c → cfp_c 堆叠正常
- [ ] ps2_c → ps2_keyboard_c 堆叠正常
- [ ] ps2_c → ps2_mouse_c 堆叠正常
- [ ] usb_signalling_c → usb_packet_c 堆叠正常
- [ ] usb_packet_c → usb_request_c 堆叠正常

### 输出一致性测试
- [ ] cfp_c annotation 输出与 Python cfp 一致
- [ ] ps2_keyboard_c annotation 输出与 Python ps2_keyboard 一致
- [ ] ps2_mouse_c annotation 输出与 Python ps2_mouse 一致
- [ ] usb_packet_c annotation 输出与 Python usb_packet 一致
- [ ] usb_request_c annotation 输出与 Python usb_request 一致

### 回归测试
- [ ] ps2_c 独立使用功能正常
- [ ] mdio_c 独立使用功能正常
- [ ] usb_signalling_c 独立使用功能正常
- [ ] 现有 C 解码器（i2c_c, spi_c, uart_c, can_fd_c 等）不受影响
