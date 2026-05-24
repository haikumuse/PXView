# 验证清单 — Batch 16: Python→C 解码器移植

---

## 通用验证项（适用于所有5个解码器）

### 编译验证
- [ ] C 文件无编译错误
- [ ] C 文件无编译警告（-Wall -Wextra）
- [ ] CMakeLists.txt 中 C_DECODERS 列表已添加
- [ ] 构建输出 DLL 在 `build.dir/decoders/c_decoders/` 目录下
- [ ] DLL 文件名格式正确：`<decoder_id>_c.dll`

### 结构验证
- [ ] `struct srd_c_decoder` 的 `.id` 字段格式为 `xxx_c`
- [ ] `.name` 字段格式为 `XXX(C)`
- [ ] `ann_labels` 第一列全部为空字符串 `""`
- [ ] 所有 annotation class 都映射到至少一个 annotation_row
- [ ] `num_annotations` 等于 ann_labels 数组长度
- [ ] `num_annotation_rows` 等于 annotation_rows 数组长度
- [ ] `num_channels` + `num_optional_channels` 与通道定义一致
- [ ] `num_options` 与选项定义一致
- [ ] inputs 数组以 NULL 结尾
- [ ] outputs 数组以 NULL 结尾（或包含协议名）
- [ ] tags 数组以 NULL 结尾

### 函数验证
- [ ] `reset` 回调：g_malloc0 分配私有数据，memset 清零
- [ ] `start` 回调：注册输出，读取选项
- [ ] `metadata` 回调：保存 samplerate（如果需要）
- [ ] `decode` 回调：主循环 while(1) + condition builder
- [ ] `destroy` 回调：g_free 释放私有数据
- [ ] `srd_c_decoder_entry()`：初始化选项默认值和 idn
- [ ] `srd_c_decoder_api_version()`：返回 SRD_C_DECODER_API_VERSION

### Condition Builder 验证
- [ ] 每次 c_cond_new() 后都有对应的 c_cond_free()
- [ ] c_cond_wait 返回值检查：`if (ret != SRD_OK) return;`
- [ ] 不使用 c_cond_wait 的旧 API（如 direct pin 读取循环）
- [ ] c_cond_or() 正确使用（多条件组合）

### samplerate 守卫验证
- [ ] decode() 中检查 samplerate 是否已设置
- [ ] 如果 samplerate 为 0，不进行时间计算
- [ ] metadata 回调正确保存 samplerate

### 内存安全验证
- [ ] 无内存泄漏（g_malloc0 对应 g_free）
- [ ] 无缓冲区溢出（snprintf 使用正确 size）
- [ ] 数组索引不越界（bits_pos, pastWords, pastPackets 等）
- [ ] 私有数据指针在 destroy 中置 NULL

---

## tdm_audio_c 专项验证

### 元数据验证
- [ ] id = `tdm_audio_c`
- [ ] name = `TDM audio(C)`
- [ ] 3 个通道：clock(SCLK), frame(SFS), data(SDATA)
- [ ] 4 个选项：bps(uint64, 16), channels(uint64, 1-8), edge(string), sampling_edge(string)
- [ ] 8 个注解：ch0..ch7
- [ ] 8 个 annotation_rows：每通道一行
- [ ] tags 包含 "Audio"

### 功能验证
- [ ] 时钟上升沿采样（edge=rising 默认）
- [ ] 时钟下降沿采样（edge=falling）
- [ ] Frame sync 检测：frame 从 0→1 时重置通道计数
- [ ] sampling_edge="first edge"：bitcount=1, data=pin_val
- [ ] sampling_edge="second edge"：bitcount=0, data=0
- [ ] 8 bit 数据输出 %02x 格式
- [ ] 16 bit 数据输出 %04x 格式
- [ ] 32 bit 数据输出 %08x 格式
- [ ] 通道号正确循环（0..channels-1）
- [ ] 注解包含长/中/短三种格式

---

## timing_c 专项验证

### 元数据验证
- [ ] id = `timing_c`
- [ ] name = `Timing(C)`
- [ ] 1 个通道：data(SDATA)
- [ ] 4 个选项：avg_period, edge, delta, format
- [ ] 4 个注解：TIME, TERSE, AVG, DELTA
- [ ] 3 个 annotation_rows：times(TIME+TERSE), averages(AVG), deltas(DELTA)
- [ ] tags 包含 "Clock/timing", "Util"

### 功能验证
- [ ] edge="any"：等待任意边沿
- [ ] edge="rising"：等待上升沿
- [ ] edge="falling"：等待下降沿
- [ ] format="full"：输出到 ANN_TIME，格式如 "1.234 ms (0.810 kHz)"
- [ ] format="terse-auto"：输出到 ANN_TERSE，自动选择单位
- [ ] format="terse-us"：输出到 ANN_TERSE，固定微秒单位
- [ ] format="samples"：输出到 ANN_TERSE，样本数
- [ ] avg_period=100：滑动窗口100个间隔的平均值
- [ ] avg_period=0：不输出平均值
- [ ] delta="yes"：输出与上一次间隔的差值
- [ ] delta="no"：不输出差值
- [ ] 时间格式化正确：s/ms/us/ns + Hz/kHz/MHz
- [ ] terse 格式化正确：带单位和不带单位两种
- [ ] 第一个边沿不输出（无前一个边沿参考）

---

## t55xx_c 专项验证

### 元数据验证
- [ ] id = `t55xx_c`
- [ ] name = `T55xx(C)`
- [ ] 1 个通道：data(SDATA)
- [ ] 8 个选项：coilfreq, start_gap, w_gap, w_one_min, w_one_max, w_zero_min, w_zero_max, em4100_decode
- [ ] 11 个注解
- [ ] 4 个 annotation_rows：bits, structure, fields, decode
- [ ] tags 包含 "IC", "RFID"

### 功能验证
- [ ] START_GAP 状态：检测到 start_gap 后切换到 WRITE_GAP
- [ ] WRITE_GAP 状态：检测到 write_gap 后标记 gap_detected
- [ ] Write zero 检测：间隔在 w_zero_min..w_zero_max 范围内
- [ ] Write one 检测：间隔在 w_one_min..w_one_max 范围内
- [ ] nogap 超时：64个 field clock 后退出写入模式
- [ ] 70-bit 模式：Opcode(2) + Password(32) + Lock(1) + Data(32) + Addr(3)
- [ ] 38-bit 模式：Opcode(2) + Lock(1) + Data(32) + Addr(3)
- [ ] 2-bit 模式：仅 Opcode
- [ ] 配置寄存器解码（addr=0）：Safer Key, Bit Rate, Modulation, PSK-CF, AOR, Max-Block, PWD, ST, POR delay
- [ ] EM4100 解码（addr=1,2）：header + nibbles + partial nibble + trailer
- [ ] 密码寄存器（addr=7）：Data 字段显示为 Password
- [ ] 阈值计算正确：field_clock = samplerate / coilfreq
- [ ] 位位置记录正确：bit_val, ss, es

---

## spi_fast_c 专项验证

### 元数据验证
- [ ] id = `spi_fast_c`
- [ ] name = `SPI-Fast(C)`
- [ ] 1 个必选通道：CLK(SCLK)
- [ ] 3 个可选通道：MISO(SDATA), MOSI(SDATA), CS(SCS)
- [ ] 7 个选项：cs_polarity, cpol, cpha, bitorder, wordsize, format, show_data_point
- [ ] 5 个注解：MISO_DATA, MOSI_DATA, ATK_DATA_POINT, ATK_RISING_EDGE, ATK_FALLING_EDGE
- [ ] 3 个 annotation_rows：miso, mosi, atk
- [ ] 2 个 binary：miso, mosi
- [ ] outputs 包含 "spi"
- [ ] tags 包含 "Embedded/industrial"

### 功能验证
- [ ] 至少需要 MISO 或 MOSI 其中一个
- [ ] CPOL=0, CPHA=0 (Mode 0)：上升沿采样
- [ ] CPOL=0, CPHA=1 (Mode 1)：下降沿采样
- [ ] CPOL=1, CPHA=0 (Mode 2)：下降沿采样
- [ ] CPOL=1, CPHA=1 (Mode 3)：上升沿采样
- [ ] cs_polarity="active-low"：CS=0 时有效
- [ ] cs_polarity="active-high"：CS=1 时有效
- [ ] 无 CS 时：所有时钟边沿都采样
- [ ] bitorder="msb-first"：高位先移入
- [ ] bitorder="lsb-first"：低位先移入
- [ ] wordsize 可配置（默认8）
- [ ] format="hex"：输出 %02X
- [ ] format="dec"：输出十进制
- [ ] format="ascii"：可打印字符直接输出，否则十六进制
- [ ] show_data_point="yes"：输出 ATK 注解标记采样点
- [ ] CS-CHANGE python 输出：CS 变化时发送
- [ ] DATA python 输出：word 完成时发送
- [ ] Binary 输出：MISO/MOSI 字节流
- [ ] ATK 颜色注解：初始化时输出 "color:#F32FDC"

---

## swi_c 专项验证

### 元数据验证
- [ ] id = `swi_c`
- [ ] name = `SWI(C)`
- [ ] 1 个通道：swi(SDATA)
- [ ] 0 个选项
- [ ] 7 个注解：BAUD_RATE, BITS, BYTES, ERR, MEAN, PBYTES, NMBR
- [ ] 7 个 annotation_rows：bauds, bits_a, data, errors, meanings, meanings_data, numbs
- [ ] tags 包含 "Clock/timing", "Util"

### 功能验证
- [ ] Baud 计算正确：bauds = round(时间差 / 4.47μs)
- [ ] Half-rate 检测：偶数 bauds 除以2
- [ ] 有效间隔检测：bauds == 1 或 bauds == 3
- [ ] Word 分隔检测：前一个间隔 >= 5 bauds
- [ ] 13-baud word 收集
- [ ] Training bits 解析：前2位确定 word_type
- [ ] word_type=1 (unicast)：单播消息
- [ ] word_type=2 (broadcast)：广播消息
- [ ] Invert bit 解析：第13个 baud == 3 表示 inverted
- [ ] calculate_bit 正确：(baud==3 && !invert) || (baud==1 && invert)
- [ ] Broadcast 解析：Initialize, Enumerate/Select, Packet Header, Packet Class, Selected Device
- [ ] Enumerate 解析：Enum Start, Request 0s/1s, Sel 0/1, UID 组装
- [ ] Unicast 解析：3个 word 组成 packet
- [ ] Packet Class 0 解析：UID, Polling
- [ ] Packet Class 1 解析：Read, Request, ECCE
- [ ] ECCE 解析：C/Z/X challenge
- [ ] ODC 读取序列：48+17+18 步
- [ ] START 标记输出
- [ ] ACK 标记输出
- [ ] B1/B3 baud 标记输出
- [ ] 错误注解输出

### 内存验证
- [ ] pastNs/pastVs ring buffer 不溢出
- [ ] pastWords 数组不越界（SWI_MAX_WORDS）
- [ ] pastPackets 数组不越界（SWI_MAX_PACKETS）
- [ ] pastUidData 数组不越界（SWI_MAX_UID_DATA）
- [ ] pastBits 字符串缓冲区足够大

---

## 集成验证

### 构建验证
- [ ] `build_incremental.cmd` 执行成功
- [ ] 所有5个 DLL 生成在 `build.dir/decoders/c_decoders/`
- [ ] PXView.exe 启动无错误

### 运行时验证
- [ ] 在 PXView 解码器列表中可以看到所有5个 C 解码器
- [ ] C 解码器名称带 "(C)" 后缀
- [ ] 选择 C 解码器后通道配置界面正确
- [ ] 选项配置界面正确（类型、默认值、可选值）
- [ ] 解码器运行无崩溃
- [ ] 注解显示正确（行分配、颜色、文本）
- [ ] 与 Python 版本输出对比一致（相同输入数据）

### 回归验证
- [ ] 原有 C 解码器（spi_c, i2c_c 等）仍正常工作
- [ ] 原有 Python 解码器仍正常工作
- [ ] CMakeLists.txt 修改不影响其他构建目标
