# Python 解码器移植到 C — Batch 04 任务分解

## 阶段 1：OneWire Link 解码器（优先级最高，有现有参考）

### 任务 1.1：创建 onewire_link_c.c 基本框架
- [ ] 创建文件 `libsigrokdecode/c_decoders/onewire_link_c.c`
- [ ] 实现通道定义（1 个必需通道 owr）
- [ ] 实现选项定义（overdrive: yes/no）
- [ ] 实现注解定义（5 个注解类：BIT, WARN, RESET, PRESENCE, OVERDRIVE）
- [ ] 实现注解行定义（bits, info, warnings）
- [ ] 实现 reset/start/destroy 函数
- [ ] 实现 srd_c_decoder_entry 和 srd_c_decoder_api_version

### 任务 1.2：实现 onewire_link 状态机
- [ ] 实现 STATE_INITIAL：等待高电平
- [ ] 实现 STATE_IDLE：等待下降沿，检查恢复时间
- [ ] 实现 STATE_LOW：测量低电平时间，判断复位/时间槽
- [ ] 实现 STATE_PRESENCE_DETECT_HIGH：等待存在信号下降沿或超时
- [ ] 实现 STATE_PRESENCE_DETECT_LOW：等待存在信号上升沿
- [ ] 实现 STATE_SLOT：等待时间槽结束
- [ ] 实现 STATE_PRESENCE_DETECT：等待存在检测结束

### 任务 1.3：实现 onewire_link 辅助功能
- [ ] 实现时序阈值计算（正常/过驱动模式）
- [ ] 实现采样率检查和警告
- [ ] 实现过驱动模式动态切换（ROM 命令 0x3C/0x69）
- [ ] 实现 Python 输出（BIT, RESET/PRESENCE）
- [ ] 实现所有警告注解

### 任务 1.4：注册和测试 onewire_link_c
- [ ] 在 CMakeLists.txt 的 C_DECODERS 列表中添加 onewire_link_c
- [ ] 编译验证
- [ ] 功能测试

---

## 阶段 2：TMC 解码器

### 任务 2.1：创建 tmc_c.c 基本框架
- [ ] 创建文件 `libsigrokdecode/c_decoders/tmc_c.c`
- [ ] 实现通道定义（2 个必需：CLK, DIO；1 个可选：STB）
- [ ] 实现选项定义（radix: Hex/Dec/Oct/Bin）
- [ ] 实现注解定义（8 个注解类：START, STOP, ACK, NACK, COMMAND, DATA, BIT, WARN）
- [ ] 实现注解行定义（bits, data, warnings）
- [ ] 实现二进制输出定义（DATA）
- [ ] 实现 reset/start/destroy/metadata 函数

### 任务 2.2：实现 TMC 状态机
- [ ] 实现 STATE_FIND_START：检测 START 条件，自动判断 wire2/wire3
- [ ] 实现 STATE_FIND_DATA：检测 CLK 上升沿或 STOP 条件
- [ ] 实现 STATE_FIND_ACK：检测 CLK 下降沿（仅 wire2）
- [ ] 实现 STATE_FIND_STOP：检测 STOP 条件

### 任务 2.3：实现 TMC 数据处理
- [ ] 实现 wire2 数据位处理（LSB-first，8+1 位）
- [ ] 实现 wire3 数据位处理（8 位，无 ACK）
- [ ] 实现位注解延迟输出
- [ ] 实现 COMMAND/DATA 字节注解
- [ ] 实现 ACK/NACK 注解
- [ ] 实现 bitrate 计算和 META 输出
- [ ] 实现 radix 格式化（Hex/Dec/Oct/Bin）
- [ ] 实现 Python 输出（START, COMMAND, DATA, STOP, ACK, NACK, BITS）
- [ ] 实现 binary 输出

### 任务 2.4：注册和测试 tmc_c
- [ ] 在 CMakeLists.txt 的 C_DECODERS 列表中添加 tmc_c
- [ ] 编译验证
- [ ] 功能测试

---

## 阶段 3：SLE44xx 解码器

### 任务 3.1：创建 sle44xx_c.c 基本框架
- [ ] 创建文件 `libsigrokdecode/c_decoders/sle44xx_c.c`
- [ ] 实现通道定义（3 个必需：RST, CLK, IO）
- [ ] 实现注解定义（13 个注解类）
- [ ] 实现注解行定义（symbols, fields, operations）
- [ ] 实现二进制输出定义（bytes）
- [ ] 实现 reset/start/destroy/metadata 函数

### 任务 3.2：实现 SLE44xx 主循环和条件等待
- [ ] 实现 9 个等待条件的组合
- [ ] 实现 RST 处理（RESET/INTERRUPT 判断）
- [ ] 实现 RST+CLK 处理
- [ ] 实现 DATA 位边界处理（CLK 上升沿/下降沿）
- [ ] 实现 CMD START/STOP 条件处理
- [ ] 实现 PROC IO 高电平检测

### 任务 3.3：实现 SLE44xx 数据处理
- [ ] 实现位收集和字节组装（bitpack_lsb）
- [ ] 实现 ATR 状态处理（4 字节收集）
- [ ] 实现 CMD 状态处理（3 字节收集和命令解析）
- [ ] 实现命令码表查找和格式化
- [ ] 实现 OUT 状态处理（数据字节收集）
- [ ] 实现 PROC 状态处理（CLK 计数和 IO 监控）
- [ ] 实现 flush_queued（ATR/CMD/OUT/PROC 数据注解输出）
- [ ] 实现时间计算（samplerate → 微秒/毫秒）

### 任务 3.4：注册和测试 sle44xx_c
- [ ] 在 CMakeLists.txt 的 C_DECODERS 列表中添加 sle44xx_c
- [ ] 编译验证
- [ ] 功能测试

---

## 阶段 4：Modbus RTU 解码器

### 任务 4.1：创建 modbus_c.c 基本框架
- [ ] 创建文件 `libsigrokdecode/c_decoders/modbus_c.c`
- [ ] 实现选项定义（scchannel, cschannel, framegap）
- [ ] 实现注解定义（15 个注解类：SC 7 个 + CS 7 个 + error-indication）
- [ ] 实现注解行定义（sc, cs, error-indicators）
- [ ] 实现 reset/start/destroy 函数
- [ ] 实现 recv_proto 回调（接收 UART 解码器输出）

### 任务 4.2：实现 Modbus ADU 管理
- [ ] 实现 ADU 数据结构
- [ ] 实现帧间隔检测（bitlength * framegap）
- [ ] 实现 bitlength 推导（从 STARTBIT/STOPBIT）
- [ ] 实现 ADU 创建和关闭
- [ ] 实现双向解码（SC/CS 通道分离）

### 任务 4.3：实现 Modbus CRC-16
- [ ] 实现 Modbus CRC-16 计算（初始值 0xFFFF，多项式 0xA001）
- [ ] 实现 CRC 校验和注解

### 任务 4.4：实现 Modbus 功能码解析
- [ ] 实现 puti 机制（C 版本用返回值替代异常）
- [ ] 实现 putl 机制
- [ ] 实现 half_word 读取
- [ ] 实现 SC 方向解析（功能码 1-4, 5, 6, 7, 8, 11, 12, 15, 16, 17, 22, 0x80+）
- [ ] 实现 CS 方向解析（功能码 1-4, 5, 6, 7/11/12/17, 8, 15/16, 22, 23）
- [ ] 实现错误响应解析
- [ ] 实现所有格式化字符串

### 任务 4.5：注册和测试 modbus_c
- [ ] 在 CMakeLists.txt 的 C_DECODERS 列表中添加 modbus_c
- [ ] 编译验证
- [ ] 功能测试（需要 UART 解码器堆叠）

---

## 阶段 5：PJDL 解码器

### 任务 5.1：创建 pjdl_c.c 基本框架
- [ ] 创建文件 `libsigrokdecode/c_decoders/pjdl_c.c`
- [ ] 实现通道定义（1 个必需：DATA）
- [ ] 实现选项定义（mode: 1/2/3/4, idle_add_us）
- [ ] 实现注解定义（11 个注解类）
- [ ] 实现注解行定义（carriers, bits, bytes, frames, warns）
- [ ] 实现 reset/start/destroy/metadata 函数

### 任务 5.2：实现 PJDL 时序计算
- [ ] 实现 span_prepare（根据模式和采样率计算所有时序参数）
- [ ] 实现位宽度范围计算（含容差）
- [ ] 实现 span_is_pad/span_is_data/span_is_short

### 任务 5.3：实现 PJDL 载波检测
- [ ] 实现 carrier_check（HIGH→BUSY, LOW→IDLE）
- [ ] 实现 carrier_set_idle/carrier_set_busy
- [ ] 实现 carrier_flush

### 任务 5.4：实现 PJDL 符号处理
- [ ] 实现符号列表管理（append, get_last, has_prev, collapse）
- [ ] 实现 PAD_BIT/SHORT_BIT/DATA_BIT 符号
- [ ] 实现 SYNC_PAD 合并（PAD_BIT + ZERO_BIT）
- [ ] 实现 FRAME_INIT 合并（3 × SYNC_PAD）
- [ ] 实现 WAIT_ACK 合并（SHORT_BIT + SYNC_PAD，含挤压）
- [ ] 实现 DATA_BYTE 合并（SYNC_PAD + 8 × DATA_BIT）

### 任务 5.5：实现 PJDL 数据位采样
- [ ] 实现 wait_until（保持载波检测的同时等待到指定样本位置）
- [ ] 实现 8 位数据位采样（固定间隔，从 SYNC_PAD 下降沿开始）
- [ ] 实现最后一个 DATA 位后的 HIGH 保持检测
- [ ] 实现 bitpack 位打包

### 任务 5.6：实现 PJDL 帧处理
- [ ] 实现 frame_flush（构建帧文本和 Python 输出）
- [ ] 实现帧刷新触发（FRAME_INIT, IDLE, WAIT_ACK+DATA_BYTE）
- [ ] 实现 Python 输出（所有符号类型）

### 任务 5.7：注册和测试 pjdl_c
- [ ] 在 CMakeLists.txt 的 C_DECODERS 列表中添加 pjdl_c
- [ ] 编译验证
- [ ] 功能测试

---

## 阶段 6：最终验证

### 任务 6.1：全量编译
- [ ] 确保所有 5 个解码器编译无错误无警告
- [ ] 确保增量构建正常

### 任务 6.2：集成测试
- [ ] 在 PXView 中加载每个解码器
- [ ] 使用对应协议的捕获文件验证
- [ ] 与 Python 版本输出对比
