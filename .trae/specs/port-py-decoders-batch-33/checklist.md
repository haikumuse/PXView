# 移植验证清单 (Batch 33)

本清单用于逐项验证每个 C 解码器的正确性。每个解码器分为"代码结构检查"和"功能逻辑检查"两部分。

---

## 通用检查项（适用于所有 5 个解码器）

### 文件结构
- [ ] 文件名格式正确：`{decoder_id}_c.c`
- [ ] 包含标准头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 使用 `#include "libsigrokdecode.h"`（非 `<libsigrokdecode.h>`）

### srd_c_decoder 结构体
- [ ] `.id` 格式为 `xxx_c`（如 `onewire_network_c`）
- [ ] `.name` 格式为 `XXX(C)`（如 `1-Wire network layer(C)`）
- [ ] `.longname` 包含 `(C)` 后缀
- [ ] `.desc` 包含 `C implementation` 说明
- [ ] `.license = "gplv2+"`
- [ ] `.channels = NULL`, `.num_channels = 0`（上层解码器无通道）
- [ ] `.optional_channels = NULL`, `.num_optional_channels = 0`
- [ ] `.inputs` 正确指定下层协议
- [ ] `.decode` 指向空函数（上层解码器不直接解析原始信号）
- [ ] `.recv_proto` 指向实际回调函数
- [ ] `.ann_labels` 第一列为 `""`（空字符串）
- [ ] `.annotation_rows` 中所有 ann class 都映射到某个 row

### 框架函数
- [ ] `reset`：使用 `g_malloc0` 分配 priv（首次），`memset` 清零，初始化默认状态
- [ ] `start`：注册 `SRD_OUTPUT_ANN`，如需输出协议则注册 `SRD_OUTPUT_PYTHON`
- [ ] `decode`：空函数体 `(void)di;`
- [ ] `destroy`：`g_free(priv)` + `c_decoder_set_private(di, NULL)`
- [ ] `srd_c_decoder_entry`：初始化选项默认值（如有），返回解码器指针
- [ ] `srd_c_decoder_api_version`：返回 `SRD_C_DECODER_API_VERSION`

### 编译导出
- [ ] `SRD_C_DECODER_EXPORT` 修饰 `srd_c_decoder_entry` 和 `srd_c_decoder_api_version`
- [ ] 全局解码器变量名格式：`{decoder_id}_decoder`（如 `onewire_network_c_decoder`）

### C_ANN_PUT 使用
- [ ] 所有 `C_ANN_PUT` 调用的 ann class 值在 0 到 NUM_ANN-1 范围内
- [ ] 文本参数使用字面量或栈上缓冲区（不使用已释放的指针）
- [ ] `snprintf` 缓冲区大小参数正确

### c_decoder_put_python 使用
- [ ] 命令字符串与 Python 版本完全一致（大小写敏感）
- [ ] data 缓冲区在函数返回前保持有效
- [ ] data_len 与实际数据长度匹配

### recv_proto 实现
- [ ] 函数签名正确：`void func(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 首先获取 priv 指针并检查 NULL
- [ ] 使用 `strcmp` 匹配命令字符串
- [ ] 访问 data 前检查 `data && data_len > 0`

---

## 解码器 1：onewire_network_c

### 元数据检查
- [ ] `.id = "onewire_network_c"`
- [ ] `.name = "1-Wire network layer(C)"`
- [ ] `.inputs = {"onewire_link"}`
- [ ] `.outputs = {"onewire_network"}`
- [ ] `.num_outputs = 1`

### 状态机检查
- [ ] 5 个状态定义：COMMAND, GET_ROM, SEARCH_ROM, TRANSPORT, COMMAND_ERROR
- [ ] 3 个搜索阶段：SEARCH_PHASE_P, SEARCH_PHASE_N, SEARCH_PHASE_D
- [ ] RESET/PRESENCE 事件正确重置状态为 COMMAND

### ROM 命令检查
- [ ] 10 个 ROM 命令完整定义
- [ ] 0x33 → Read ROM → GET_ROM
- [ ] 0x0f → Conditional read ROM → GET_ROM
- [ ] 0xcc → Skip ROM → TRANSPORT
- [ ] 0x55 → Match ROM → GET_ROM
- [ ] 0xf0 → Search ROM → SEARCH_ROM
- [ ] 0xec → Conditional search ROM → SEARCH_ROM
- [ ] 0x3c → Overdrive skip ROM → TRANSPORT
- [ ] 0x69 → Overdrive match ROM → GET_ROM
- [ ] 0xa5 → Resume → TRANSPORT
- [ ] 0x96 → DS2408: Disable Test Mode → GET_ROM
- [ ] 未识别命令 → COMMAND_ERROR

### BIT 收集逻辑检查
- [ ] COMMAND 状态：收集 8 bit → 查找命令
- [ ] GET_ROM 状态：收集 64 bit → 输出 ROM
- [ ] SEARCH_ROM 状态：三态循环 P→N→D，bit_cnt 仅在 D 阶段递增
- [ ] TRANSPORT 状态：收集 8 bit → 输出 DATA
- [ ] COMMAND_ERROR 状态：收集 8 bit → 输出错误

### 数据格式检查
- [ ] BIT 数据：1 字节（0x00 或 0x01）
- [ ] RESET/PRESENCE 数据：1 字节（0x00 或 0x01），正确转发
- [ ] ROM 数据：8 字节 LSB first 格式输出
- [ ] DATA 数据：1 字节格式输出

### 搜索 ROM 三态循环详细检查
- [ ] P 阶段：data_p 收集原始位，转入 N
- [ ] N 阶段：data_n 收集补码位，转入 D
- [ ] D 阶段：data 收集方向位，转入 P，bit_cnt++
- [ ] bit_cnt == 64 时：data_p, data_n, data 均掩码为 64 位
- [ ] 搜索完成后：search 重置为 P，bit_cnt 重置为 0

### 注解输出检查
- [ ] RESET/PRESENCE：`"Reset/presence: true/false"`
- [ ] ROM 命令：`"ROM command: 0xXX 'name'"` 或 `"ROM command: 0xXX 'unrecognized'"`
- [ ] ROM 地址：`"ROM: 0xXXXXXXXXXXXXXXXX"`（16 位十六进制）
- [ ] DATA：`"Data: 0xXX"`
- [ ] 错误数据：`"ROM error data: 0xXX"`

---

## 解码器 2：ds2408_c

### 元数据检查
- [ ] `.id = "ds2408_c"`
- [ ] `.name = "DS2408(C)"`
- [ ] `.inputs = {"onewire_network"}`
- [ ] `.outputs = NULL`, `.num_outputs = 0`

### 命令解析检查
- [ ] 6 个功能命令完整定义
- [ ] 0xf0 → Read PIO Registers
- [ ] 0xf5 → Channel Access Read
- [ ] 0x5a → Channel Access Write
- [ ] 0xcc → Write Conditional Search Register
- [ ] 0xc3 → Reset Activity Latches
- [ ] 0x3c → Disable Test Mode

### 0xF0 Read PIO Registers 检查
- [ ] bytes[1..2] 目标地址解析（小端序）：`(bytes[2] << 8) + bytes[1]`
- [ ] bytes[3+] 数据输出

### 0xF5 Channel Access Read 检查
- [ ] bytes[2+] PIO 采样输出

### 0x5A Channel Access Write 检查
- [ ] bytes[1] 数据字节
- [ ] bytes[2] 反码校验：`bytes[2] == bytes[1] ^ 0xFF`
- [ ] 校验通过：`"Data: 0xXX (bit-inversion correct: 0xXX)"`
- [ ] 校验失败：`"Data error: second byte (0xXX) is not bit-inverse of first (0xXX)"`
- [ ] bytes[3+]：0xAA → Success, 0xFF → Fail New State

### 0xCC Write Conditional Search Register 检查
- [ ] bytes[1..2] 目标地址解析
- [ ] bytes[3+] 数据输出

### 0xC3 Reset Activity Latches 检查
- [ ] 0xAA → Success
- [ ] 其他 → Invalid byte

### 事件处理检查
- [ ] RESET/PRESENCE → 清空 bytes
- [ ] ROM → 提取 family code + 清空 bytes
- [ ] 未识别命令 → `"Unrecognized command: 0xXX"`

---

## 解码器 3：ds243x_c

### 元数据检查
- [ ] `.id = "ds243x_c"`
- [ ] `.name = "DS243x(C)"`
- [ ] `.inputs = {"onewire_network"}`
- [ ] `.outputs = NULL`, `.num_outputs = 0`
- [ ] binary 定义：`{0, "mem_read", "Data read from memory"}`

### CRC-16 检查
- [ ] 初始值 0x0000
- [ ] 反转多项式 0xA001
- [ ] 异或输出 0xFFFF
- [ ] 与 Python 版本 `crc16()` 结果一致（需用测试向量验证）

### 家族代码识别检查
- [ ] 0x33 → DS2432（7 个命令）
- [ ] 0x23 → DS2433（4 个命令）
- [ ] 未知代码 → `"family code 0xXX unknown"`

### 0x0F Write scratchpad 检查
- [ ] bytes[1..2]：目标地址（小端序）
- [ ] bytes[3..10]：8 字节数据，逗号分隔格式化
- [ ] bytes[11..12]：CRC-16 校验，`crc16(bytes[0:11]) == bytes[11] + (bytes[12] << 8)`

### 0xAA Read scratchpad 检查
- [ ] bytes[1..2]：目标地址
- [ ] bytes[3]：E/S 状态
- [ ] bytes[4..11]：8 字节数据
- [ ] bytes[12..13]：CRC-16 校验

### 0x55 Copy scratchpad 检查
- [ ] bytes[1..3]：授权模式 (TA1, TA2, E/S)
- [ ] bytes[4..23]：MAC（20 字节），逗号分隔格式化
- [ ] 后续：0xAA/0x55 → Operation succeeded, 0x00 → Operation failed

### 0xF0 Read memory 检查
- [ ] bytes[1..2]：目标地址
- [ ] bytes[3+]：数据 + binary 输出

### 0x5A Load first secret 检查
- [ ] bytes[1..3]：授权模式
- [ ] 后续：0xAA/0x55 → End of operation

### 0x33 Compute next secret 检查
- [ ] bytes[1..2]：目标地址
- [ ] 后续：0xAA/0x55 → End of operation

### 0xA5 Read authenticated page 检查
- [ ] bytes[1..2]：目标地址
- [ ] bytes[3..34]：32 字节数据
- [ ] bytes[35]：padding（0xFF=ok, else=error）
- [ ] bytes[36..37]：CRC-16
- [ ] bytes[38..57]：MAC（20 字节）
- [ ] bytes[58..59]：MAC CRC-16
- [ ] 后续：0xAA/0x55 → Operation completed

### 缓冲区大小检查
- [ ] bytes 数组至少 64 字节（0xA5 命令最多 60+ 字节）

---

## 解码器 4：ds28ea00_c

### 元数据检查
- [ ] `.id = "ds28ea00_c"`
- [ ] `.name = "DS28EA00(C)"`
- [ ] `.inputs = {"onewire_network"}`
- [ ] `.outputs = NULL`, `.num_outputs = 0`

### 命令检查
- [ ] 9 个功能命令完整定义
- [ ] 0x4e → Write scratchpad
- [ ] 0xbe → Read scratchpad
- [ ] 0x48 → Copy scratchpad
- [ ] 0x44 → Convert temperature
- [ ] 0xb4 → Read power mode
- [ ] 0xb8 → Recall EEPROM
- [ ] 0xf5 → PIO access read
- [ ] 0xa5 → PIO access write
- [ ] 0x99 → Chain

### 状态机检查
- [ ] ROM → COMMAND（收到 ROM 事件后）
- [ ] COMMAND → 各命令状态（收到 DATA 事件后）
- [ ] RESET/PRESENCE → ROM（重置）

### DATA 处理检查
- [ ] READ_SCRATCHPAD 状态：`"Scratchpad data: 0xXX"`
- [ ] CONVERT_TEMPERATURE 状态：`"Temperature conversion status: 0xXX"`
- [ ] 其他状态：`"TODO 'state_name': 0xXX"`
- [ ] 未识别命令：`"Unrecognized command: 0xXX"`

---

## 解码器 5：eeprom93xx_c

### 元数据检查
- [ ] `.id = "eeprom93xx_c"`
- [ ] `.name = "93xx EEPROM(C)"`
- [ ] `.inputs = {"microwire"}`
- [ ] `.outputs = NULL`, `.num_outputs = 0`
- [ ] 3 个选项定义
- [ ] 2 个 binary 输出定义

### mw_py_entry 结构检查
- [ ] 字段顺序与 microwire_c.c 完全一致：ss, es, si, so
- [ ] 字段类型一致：uint64_t, uint64_t, int, int
- [ ] 结构体大小一致（可用 `sizeof` 验证）

### 选项检查
- [ ] addresssize 默认值 8
- [ ] wordsize 默认值 16
- [ ] format 默认值 "hex"，可选值 "ascii"/"hex"
- [ ] `srd_c_decoder_entry()` 中正确初始化选项的 `def` 和 `values`

### 指令解析检查
- [ ] opcode 提取：`(entries[0].si << 1) | entries[1].si`
- [ ] opcode=2 (READ)：输出地址 + 读取 SO 数据字
- [ ] opcode=1 (WRITE)：输出地址 + 写入 SI 数据字
- [ ] opcode=3 (ERASE)：输出地址
- [ ] opcode=0 + SI[2]=1,SI[3]=1 → WEN
- [ ] opcode=0 + SI[2]=0,SI[3]=0 → WDS
- [ ] opcode=0 + SI[2]=1,SI[3]=0 → ERAL
- [ ] opcode=0 + SI[2]=0,SI[3]=1 → WRAL + 写入字

### 地址提取检查
- [ ] MSB first：`addr |= entries[i].si << (addresssize - i - 1)`
- [ ] 注解格式：`"Address: 0xXXXX"`
- [ ] Binary 输出：1 字节地址

### 字数据提取检查
- [ ] MSB first：`word |= d << (wordsize - b - 1)`
- [ ] SI 数据使用 `entries[b].si`，SO 数据使用 `entries[b].so`
- [ ] hex 格式：`"Data: 0xXXXX"` + 2 字节 binary
- [ ] ascii 格式：按 8 位一组显示字符，不可打印字符显示 `[XX]`

### 错误处理检查
- [ ] 数据不足（< 2 + addresssize）：`"Not enough packet bits"`
- [ ] READ 时字位不足：`"Not enough word bits"`
- [ ] WRITE/WRAL 时字位不足：`"Not enough word bits"`

---

## 构建验证

### 编译检查
- [ ] `build_incremental.cmd` 成功完成
- [ ] 无编译错误
- [ ] 无编译警告（或仅有已知的无害警告）

### DLL 生成检查
- [ ] `build.dir/decoders/c_decoders/onewire_network_c.dll` 存在
- [ ] `build.dir/decoders/c_decoders/ds2408_c.dll` 存在
- [ ] `build.dir/decoders/c_decoders/ds243x_c.dll` 存在
- [ ] `build.dir/decoders/c_decoders/ds28ea00_c.dll` 存在
- [ ] `build.dir/decoders/c_decoders/eeprom93xx_c.dll` 存在

### 运行时验证
- [ ] PXView 启动无崩溃
- [ ] 解码器列表中显示 5 个新 C 解码器
- [ ] 解码器可被选中并堆叠到正确的下层解码器上
- [ ] onewire_network_c 可堆叠在 onewire_c 之上
- [ ] ds2408_c / ds243x_c / ds28ea00_c 可堆叠在 onewire_network_c 之上
- [ ] eeprom93xx_c 可堆叠在 microwire_c 之上

---

## 回归测试

### 现有解码器不受影响
- [ ] onewire_c 功能正常
- [ ] microwire_c 功能正常
- [ ] i2c_c + lm75_c / ds1307_c 堆叠功能正常
- [ ] 其他已有 C 解码器功能正常

### Python 解码器兼容性
- [ ] onewire_network (Python) 仍可堆叠在 onewire_c (C) 之上
- [ ] ds2408 (Python) 仍可堆叠在 onewire_network (Python) 之上
- [ ] eeprom93xx (Python) 仍可堆叠在 microwire_c (C) 之上
