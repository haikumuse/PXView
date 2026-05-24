# Python 解码器移植到 C — Batch 04 验证清单

## 通用验证项（每个解码器）

### 编译验证
- [ ] 文件创建在正确路径 `libsigrokdecode/c_decoders/<name>_c.c`
- [ ] 包含正确的头文件（stdio.h, stdlib.h, string.h, glib.h, libsigrokdecode.h）
- [ ] 无编译错误
- [ ] 无编译警告（-Wall -Wextra）
- [ ] CMakeLists.txt 中 C_DECODERS 列表已添加
- [ ] 增量构建成功

### 元数据验证
- [ ] id 格式为 `<name>_c`（如 `tmc_c`, `modbus_c`）
- [ ] name 以 `(C)` 结尾
- [ ] longname 以 `(C)` 结尾
- [ ] desc 以 `(C implementation)` 结尾
- [ ] license 与 Python 版本一致
- [ ] inputs 与 Python 版本一致
- [ ] outputs 与 Python 版本一致
- [ ] tags 与 Python 版本一致

### 通道验证
- [ ] 通道数量与 Python 版本一致
- [ ] 每个通道的 id/name/desc/idn 与 Python 版本一致
- [ ] 必需通道和可选通道正确分类
- [ ] 通道类型（SRD_CHANNEL_SCLK/SDATA/COMMON）正确
- [ ] 通道顺序与 Python 版本一致

### 选项验证
- [ ] 选项数量与 Python 版本一致
- [ ] 每个选项的 id/desc/idn 与 Python 版本一致
- [ ] 默认值正确设置（在 srd_c_decoder_entry 中）
- [ ] 值列表正确设置（在 srd_c_decoder_entry 中）
- [ ] 选项读取使用正确的 API（get_option_string/get_option_int/get_option_double）

### 注解验证
- [ ] 注解数量（num_annotations）与 Python 版本一致
- [ ] 每个注解的标签文本与 Python 版本一致
- [ ] ann_labels 数组格式正确（每行最多 3 个字符串）
- [ ] 注解行数量和内容与 Python 版本一致
- [ ] 注解行的 ann_classes 数组以 -1 结尾（或使用 num_ann_classes）
- [ ] 注解行中的类索引正确

### 二进制输出验证
- [ ] binary 定义与 Python 版本一致（如有）
- [ ] bin_class, id, desc 正确

### 函数实现验证
- [ ] reset 函数正确初始化私有数据
- [ ] start 函数注册输出（ANN, PYTHON, BINARY, META）
- [ ] decode 函数使用正确的条件等待 API
- [ ] destroy 函数释放私有数据
- [ ] metadata 函数处理 SRD_CONF_SAMPLERATE（如需要）
- [ ] recv_proto 函数正确处理上游数据（如需要）
- [ ] srd_c_decoder_entry 函数正确初始化选项
- [ ] srd_c_decoder_api_version 返回 SRD_C_DECODER_API_VERSION

### 条件等待验证
- [ ] c_cond_new/c_cond_free 配对使用
- [ ] c_cond_wait 返回值检查（SRD_OK）
- [ ] matched 位掩码正确解析
- [ ] 多条件 OR 正确使用 c_cond_or

### 注解输出验证
- [ ] C_ANN_PUT 参数正确（di, ss, es, out_id, ann_class, texts...）
- [ ] ss < es（起始样本 < 结束样本）
- [ ] ann_class 在有效范围内
- [ ] 字符串格式化正确（snprintf 无截断问题）

### 私有数据验证
- [ ] 使用 g_malloc0 分配
- [ ] 使用 g_free 释放
- [ ] memset 初始化
- [ ] 无内存泄漏

---

## OneWire Link 解码器专项验证

- [ ] 7 个状态完整实现（INITIAL, IDLE, LOW, PRESENCE_DETECT_HIGH, PRESENCE_DETECT_LOW, SLOT, PRESENCE_DETECT）
- [ ] 正常模式时序阈值正确（RSTL: 480-960us, PDH: 15-60us, PDL: 60-240us, SLOT: 60-120us, LOWR: 1-15us, REC: 1us）
- [ ] 过驱动模式时序阈值正确（RSTL: 48-80us, PDH: 2-6us, PDL: 8-24us, SLOT: 6-16us, LOWR: 1-2us）
- [ ] wait_falling_timeout 正确实现（c_cond_fall + c_cond_skip + c_cond_or）
- [ ] 过驱动模式切换正确（ROM 命令 0x3C/0x69 进入，正常复位退出）
- [ ] 采样率检查和警告正确
- [ ] 位判断逻辑正确（短脉冲=1，长脉冲=0）
- [ ] Python 输出格式正确（BIT, RESET/PRESENCE）
- [ ] 存在检测完整（有/无存在信号两种情况）
- [ ] 与现有 onewire_c.c 功能对比（新版本更完整）

## TMC 解码器专项验证

- [ ] Wire2/Wire3 自动检测正确
- [ ] Wire2 START 条件：CLK=高, DIO=下降沿
- [ ] Wire3 START 条件：STB=下降沿
- [ ] Wire2 STOP 条件：CLK=高, DIO=上升沿
- [ ] Wire3 STOP 条件：STB=上升沿
- [ ] LSB-first 数据传输正确（databyte >>= 1; databyte |= (dio << 7)）
- [ ] Wire2 ACK/NACK 处理正确（第 9 个 CLK 脉冲）
- [ ] Wire3 无 ACK 处理
- [ ] 第一个字节标记为 COMMAND，后续标记为 DATA
- [ ] 位注解延迟输出正确
- [ ] Radix 格式化正确（Hex/Dec/Oct/Bin）
- [ ] Bitrate 计算和 META 输出正确
- [ ] Python 输出格式正确（START, COMMAND, DATA, STOP, ACK, NACK, BITS）
- [ ] Binary 输出正确

## SLE44xx 解码器专项验证

- [ ] 9 个等待条件正确组合
- [ ] RST 优先级最高（先检查 RST 条件）
- [ ] RESET vs INTERRUPT 判断正确（有 CLK 脉冲=RESET，无=INTERRUPT）
- [ ] ATR 状态：4 字节收集后 flush
- [ ] CMD 状态：3 字节收集后解析命令
- [ ] 命令码表完整（0x30, 0x31, 0x33, 0x34, 0x38, 0x39, 0x3c）
- [ ] DATA→OUT/PROC 切换正确
- [ ] OUT 状态：达到 out_len 后 flush
- [ ] PROC 状态：IO 变高时结束
- [ ] bitpack_lsb 实现正确
- [ ] 位双重调用处理正确（上升沿记录 ss，下降沿记录 es）
- [ ] CMD/STOP 仅在非 OUT/PROC 状态处理
- [ ] flush_queued 在所有正确位置调用
- [ ] 时间计算正确（samplerate → 微秒/毫秒）
- [ ] Binary 输出正确

## Modbus 解码器专项验证

- [ ] recv_proto 回调正确接收 UART 数据
- [ ] UART 数据格式正确解析（ptype, rxtx, pdata）
- [ ] bitlength 从 STARTBIT/STOPBIT 正确推导
- [ ] 帧间隔检测正确（bitlength * framegap）
- [ ] 双向解码正确（SC/CS 通道分离）
- [ ] CRC-16 Modbus 计算正确（初始值 0xFFFF，多项式 0xA001）
- [ ] puti 机制用返回值替代异常
- [ ] half_word 大端序读取正确
- [ ] SC 方向所有功能码解析正确
- [ ] CS 方向所有功能码解析正确
- [ ] 错误响应解析正确（功能码 0x80+）
- [ ] 错误码文本正确
- [ ] 帧过短/CRC 错误/未知功能码处理
- [ ] 注解前缀映射正确（sc- → ANN_SC_*, cs- → ANN_CS_*）
- [ ] error-indication 注解正确

## PJDL 解码器专项验证

- [ ] 4 种通信模式时序正确
- [ ] 容差计算正确（±10% 和 ±1.5us）
- [ ] 位宽度范围计算正确
- [ ] span_is_pad/span_is_data/span_is_short 正确
- [ ] 载波检测正确（HIGH→BUSY, LOW 持续 byte_width→结束 BUSY, LOW 持续 idle_width→IDLE）
- [ ] 符号列表管理正确（append, get_last, has_prev, collapse）
- [ ] SYNC_PAD 合并正确（PAD_BIT + ZERO_BIT）
- [ ] FRAME_INIT 合并正确（3 × SYNC_PAD）
- [ ] WAIT_ACK 合并正确（SHORT_BIT + SYNC_PAD，含挤压）
- [ ] DATA_BYTE 合并正确（SYNC_PAD + 8 × DATA_BIT）
- [ ] 数据位采样正确（固定间隔，从 SYNC_PAD 下降沿开始）
- [ ] wait_until 实现正确（保持载波检测）
- [ ] 最后 DATA 位 HIGH 保持检测
- [ ] bitpack 实现正确
- [ ] frame_flush 文本构建正确
- [ ] 帧刷新触发正确
- [ ] Python 输出格式正确
- [ ] 采样率检查（至少 1MSa/s）

---

## 集成验证

- [ ] 所有 5 个解码器在 CMakeLists.txt 中注册
- [ ] 全量编译成功
- [ ] 增量构建成功
- [ ] PXView 启动不崩溃
- [ ] 每个解码器可以在 PXView 中选择
- [ ] 每个解码器可以正确解码对应协议的捕获数据
- [ ] C 版本注解输出与 Python 版本基本一致
