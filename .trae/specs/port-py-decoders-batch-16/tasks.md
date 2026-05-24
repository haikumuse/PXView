# 任务列表 — Batch 16: Python→C 解码器移植

## 任务概览

| # | 解码器 | 复杂度 | 文件名 | 预计行数 |
|---|--------|--------|--------|----------|
| 1 | tdm_audio | 简单 | `tdm_audio_c.c` | ~250 |
| 2 | timing | 简单 | `timing_c.c` | ~350 |
| 3 | t55xx | 中等 | `t55xx_c.c` | ~600 |
| 4 | spi-fast | 中等 | `spi_fast_c.c` | ~500 |
| 5 | swi | 复杂 | `swi_c.c` | ~900 |

---

## Task 1: tdm_audio_c — TDM 多通道音频解码器

### 1.1 创建文件 `libsigrokdecode/c_decoders/tdm_audio_c.c`

- [ ] 1.1.1 编写文件头注释（GPLv2+ license）
- [ ] 1.1.2 包含头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 1.1.3 定义 `TDM_AUDIO_MAX_CHANNELS = 8`
- [ ] 1.1.4 定义 `enum tdm_audio_ann`（ANN_CH0..ANN_CH7, NUM_ANN=8）
- [ ] 1.1.5 定义通道数组 `tdm_audio_channels[3]`（clock, frame, data），带 idn
- [ ] 1.1.6 定义选项数组 `tdm_audio_options[4]`（bps, channels, edge, sampling_edge）
- [ ] 1.1.7 定义 ann_labels `tdm_audio_ann_labels[8][3]`（第一列空字符串）
- [ ] 1.1.8 定义 annotation_rows `tdm_audio_ann_rows[8]`（每通道一行）
- [ ] 1.1.9 定义 inputs/outputs/tags 数组
- [ ] 1.1.10 定义 `struct tdm_audio_priv`（samplerate, channels, channel, bitdepth, bitcount, samplecount, lastframe, data, ss_block, have_ss_block, edge, sampling_edge, out_ann）
- [ ] 1.1.11 实现 `tdm_audio_reset()`：g_malloc0 分配 + memset 清零
- [ ] 1.1.12 实现 `tdm_audio_start()`：注册 SRD_OUTPUT_ANN，读取选项（bps, channels, edge, sampling_edge）
- [ ] 1.1.13 实现 `tdm_audio_metadata()`：保存 samplerate
- [ ] 1.1.14 实现 `tdm_audio_decode()`：
  - 主循环 while(1)
  - 使用 c_cond_rise/c_cond_fall 等待时钟边沿
  - 采样 data 引脚，移位到 data 寄存器
  - 检查 frame sync 信号（frame != lastframe && frame == 1）
  - 处理 sampling_edge 选项
  - 当 bitcount >= bitdepth 时输出通道注解
  - 根据 bitdepth 选择 %02x/%04x/%08x 格式
- [ ] 1.1.15 实现 `tdm_audio_destroy()`：g_free 释放私有数据
- [ ] 1.1.16 定义 `struct srd_c_decoder tdm_audio_c_decoder`
- [ ] 1.1.17 实现 `srd_c_decoder_entry()`：初始化选项默认值和 idn
- [ ] 1.1.18 实现 `srd_c_decoder_api_version()`

### 1.2 修改 CMakeLists.txt

- [ ] 1.2.1 在 `C_DECODERS` 列表中添加 `tdm_audio_c`

### 1.3 验证

- [ ] 1.3.1 编译通过
- [ ] 1.3.2 在 PXView 中加载解码器，验证通道/选项/注解显示正确

---

## Task 2: timing_c — 时序测量解码器

### 2.1 创建文件 `libsigrokdecode/c_decoders/timing_c.c`

- [ ] 2.1.1 编写文件头注释（GPLv2+ license）
- [ ] 2.1.2 包含头文件：`stdio.h`, `stdlib.h`, `string.h`, `math.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 2.1.3 定义 `TIMING_MAX_AVG = 10000`
- [ ] 2.1.4 定义 `enum timing_ann`（ANN_TIME, ANN_TERSE, ANN_AVG, ANN_DELTA, NUM_ANN=4）
- [ ] 2.1.5 定义通道数组 `timing_channels[1]`（data），带 idn
- [ ] 2.1.6 定义选项数组 `timing_options[4]`（avg_period, edge, delta, format）
- [ ] 2.1.7 定义 ann_labels `timing_ann_labels[4][3]`
- [ ] 2.1.8 定义 annotation_rows `timing_ann_rows[3]`（times, averages, deltas）
- [ ] 2.1.9 定义 inputs/outputs/tags 数组
- [ ] 2.1.10 定义 `struct timing_priv`（samplerate, avg_period, edge, delta, format, avg_buffer[], avg_count, avg_head, avg_sum, last_t, ss, have_ss, out_ann）
- [ ] 2.1.11 实现 `timing_normalize_time()`：时间格式化函数（s/ms/us/ns + Hz/kHz/MHz）
- [ ] 2.1.12 实现 `timing_terse_time()`：紧凑格式化函数（terse-auto/terse-s/ms/us/ns/ps/samples）
- [ ] 2.1.13 实现 `timing_reset()`：g_malloc0 分配 + memset 清零
- [ ] 2.1.14 实现 `timing_start()`：注册 SRD_OUTPUT_ANN，读取选项
- [ ] 2.1.15 实现 `timing_metadata()`：保存 samplerate
- [ ] 2.1.16 实现 `timing_decode()`：
  - samplerate 守卫检查
  - 主循环 while(1)
  - 根据 edge 选项使用 c_cond_rise/c_cond_fall/c_cond_edge
  - 计算间隔 sa = es - ss, t = sa / samplerate
  - 根据 format 选项选择 ANN_TIME 或 ANN_TERSE 输出
  - 滑动窗口平均：维护环形缓冲区，计算平均值
  - delta 计算：t - last_t
  - 更新 last_t, ss
- [ ] 2.1.17 实现 `timing_destroy()`：g_free 释放私有数据
- [ ] 2.1.18 定义 `struct srd_c_decoder timing_c_decoder`
- [ ] 2.1.19 实现 `srd_c_decoder_entry()`：初始化选项默认值和 idn
- [ ] 2.1.20 实现 `srd_c_decoder_api_version()`

### 2.2 修改 CMakeLists.txt

- [ ] 2.2.1 在 `C_DECODERS` 列表中添加 `timing_c`

### 2.3 验证

- [ ] 2.3.1 编译通过
- [ ] 2.3.2 在 PXView 中加载解码器，验证时间/频率/平均/差值显示正确

---

## Task 3: t55xx_c — T55xx RFID 解码器

### 3.1 创建文件 `libsigrokdecode/c_decoders/t55xx_c.c`

- [ ] 3.1.1 编写文件头注释（GPLv2+ license）
- [ ] 3.1.2 包含头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 3.1.3 定义 `T55XX_MAX_BITS = 70`
- [ ] 3.1.4 定义状态枚举：`STATE_START_GAP = 0`, `STATE_WRITE_GAP = 1`
- [ ] 3.1.5 定义 `enum t55xx_ann`（11个注解，NUM_ANN=11）
- [ ] 3.1.6 定义通道数组 `t55xx_channels[1]`（data），带 idn
- [ ] 3.1.7 定义选项数组 `t55xx_options[8]`（coilfreq, start_gap, w_gap, w_one_min, w_one_max, w_zero_min, w_zero_max, em4100_decode）
- [ ] 3.1.8 定义 ann_labels `t55xx_ann_labels[11][3]`
- [ ] 3.1.9 定义 annotation_rows `t55xx_ann_rows[4]`（bits, structure, fields, decode）
- [ ] 3.1.10 定义 inputs/outputs/tags 数组
- [ ] 3.1.11 定义位位置结构 `struct t55xx_bit_pos { int bit_val; uint64_t ss; uint64_t es; }`
- [ ] 3.1.12 定义 `struct t55xx_priv`（samplerate, state, bits_pos[70], bit_nr, field_clock, 阈值, 边沿追踪, em4100状态, out_ann）
- [ ] 3.1.13 定义静态字符串表：br_string[8], mod_str1[4], mod_str2[8], pskcf_str[4]
- [ ] 3.1.14 实现 `t55xx_reset()`：g_malloc0 分配 + memset 清零
- [ ] 3.1.15 实现 `t55xx_start()`：注册 SRD_OUTPUT_ANN，读取 em4100_decode 选项
- [ ] 3.1.16 实现 `t55xx_metadata()`：保存 samplerate，计算所有阈值（field_clock, wzmax, wzmin, womax, womin, startgap, writegap, nogap）
- [ ] 3.1.17 实现 `t55xx_add_bits_pos()`：添加位位置记录
- [ ] 3.1.18 实现 `t55xx_get_32_bits()`：从 bits_pos 获取32位值
- [ ] 3.1.19 实现 `t55xx_get_3_bits()`：从 bits_pos 获取3位值
- [ ] 3.1.20 实现 `t55xx_put4bits()`：输出4位十六进制
- [ ] 3.1.21 实现 `t55xx_decode_config()`：解码配置寄存器（Safer Key, Bit Rate, Modulation, PSK-CF, AOR, Max-Block, PWD, ST, POR delay）
- [ ] 3.1.22 实现 `t55xx_em4100_decode1()`：EM4100 第一部分解码
- [ ] 3.1.23 实现 `t55xx_em4100_decode2()`：EM4100 第二部分解码
- [ ] 3.1.24 实现 `t55xx_put_fields()`：根据 bit_nr (70/38/2) 输出字段（Opcode, Password, Lock, Data, Address），调用 decode_config 和 em4100_decode
- [ ] 3.1.25 实现 `t55xx_decode()`：
  - samplerate 守卫检查
  - 初始化所有追踪变量
  - 主循环 while(1)
  - 使用 c_cond_edge 等待任意边沿
  - 计算脉冲长度 pl
  - WRITE_GAP 状态：检查 pl > writegap，标记 gap_detected
  - START_GAP 状态：检查 pl > startgap，标记 gap_detected，切换到 WRITE_GAP
  - gap_detected 处理：检查 write zero/one 范围，添加位位置
  - 检查 nogap 超时：退出写入模式，调用 put_fields
- [ ] 3.1.26 实现 `t55xx_destroy()`：g_free 释放私有数据
- [ ] 3.1.27 定义 `struct srd_c_decoder t55xx_c_decoder`
- [ ] 3.1.28 实现 `srd_c_decoder_entry()`：初始化选项默认值（8个选项的 g_variant_new）和 idn
- [ ] 3.1.29 实现 `srd_c_decoder_api_version()`

### 3.2 修改 CMakeLists.txt

- [ ] 3.2.1 在 `C_DECODERS` 列表中添加 `t55xx_c`

### 3.3 验证

- [ ] 3.3.1 编译通过
- [ ] 3.3.2 在 PXView 中加载解码器，验证 gap 检测和配置寄存器解码

---

## Task 4: spi_fast_c — SPI Ultra-Fast 解码器

### 4.1 创建文件 `libsigrokdecode/c_decoders/spi_fast_c.c`

- [ ] 4.1.1 编写文件头注释（GPLv2+ license）
- [ ] 4.1.2 包含头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 4.1.3 定义 `enum spi_fast_ann`（5个注解，NUM_ANN=5）
- [ ] 4.1.4 定义通道数组 `spi_fast_channels[1]`（CLK），带 idn
- [ ] 4.1.5 定义可选通道数组 `spi_fast_optional_channels[3]`（MISO, MOSI, CS），带 idn
- [ ] 4.1.6 定义选项数组 `spi_fast_options[7]`（cs_polarity, cpol, cpha, bitorder, wordsize, format, show_data_point）
- [ ] 4.1.7 定义 ann_labels `spi_fast_ann_labels[5][3]`
- [ ] 4.1.8 定义 annotation_rows `spi_fast_ann_rows[3]`（miso, mosi, atk）
- [ ] 4.1.9 定义 binary `spi_fast_binary[2]`（miso, mosi）
- [ ] 4.1.10 定义 inputs/outputs/tags 数组
- [ ] 4.1.11 定义 `struct spi_fast_priv`（samplerate, have_miso/mosi/cs, cs_active, 选项, 位收集状态, 输出句柄）
- [ ] 4.1.12 实现 `spi_fast_cs_asserted()`：CS 极性判定
- [ ] 4.1.13 实现 `spi_fast_format_value()`：数据格式化（ascii/dec/hex/oct/bin）
- [ ] 4.1.14 实现 `spi_fast_reset()`：g_malloc0 分配 + memset 清零
- [ ] 4.1.15 实现 `spi_fast_start()`：
  - 注册 SRD_OUTPUT_ANN, SRD_OUTPUT_PYTHON, SRD_OUTPUT_BINARY
  - 读取所有选项
  - 检查通道可用性（have_miso, have_mosi, have_cs）
  - 如果没有 CS，发送 CS-CHANGE(None, None)
- [ ] 4.1.16 实现 `spi_fast_metadata()`：保存 samplerate
- [ ] 4.1.17 实现 `spi_fast_decode()`：
  - 输出 ATK 颜色注解
  - 如果没有 CS，发送 CS-CHANGE python 输出
  - 主循环 while(1)
  - 构建 condition：CLK 边沿 + CS 边沿(可选)
  - 处理 CS 变化事件
  - 处理时钟边沿：根据 CPOL/CPHA 确定采样边沿
  - 位收集：按 bitorder 移位
  - 达到 wordsize 后输出数据
  - 输出 ATK data point 注解（如果 show_data_point）
  - 输出 binary 数据
  - 输出 python DATA 数据
- [ ] 4.1.18 实现 `spi_fast_destroy()`：g_free 释放私有数据
- [ ] 4.1.19 定义 `struct srd_c_decoder spi_fast_c_decoder`
- [ ] 4.1.20 实现 `srd_c_decoder_entry()`：初始化7个选项的默认值和 idn
- [ ] 4.1.21 实现 `srd_c_decoder_api_version()`

### 4.2 修改 CMakeLists.txt

- [ ] 4.2.1 在 `C_DECODERS` 列表中添加 `spi_fast_c`

### 4.3 验证

- [ ] 4.3.1 编译通过
- [ ] 4.3.2 在 PXView 中加载解码器，验证 SPI 四种模式解码正确

---

## Task 5: swi_c — Infineon SWI 解码器

### 5.1 创建文件 `libsigrokdecode/c_decoders/swi_c.c`

- [ ] 5.1.1 编写文件头注释（GPLv2+ license）
- [ ] 5.1.2 包含头文件：`stdio.h`, `stdlib.h`, `string.h`, `math.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 5.1.3 定义 `SWI_MAX_WORDS = 256`, `SWI_MAX_PACKETS = 256`, `SWI_MAX_UID_DATA = 64`, `SWI_LOG_SIZE = 4096`
- [ ] 5.1.4 定义 `enum swi_ann`（7个注解，NUM_ANN=7）
- [ ] 5.1.5 定义通道数组 `swi_channels[1]`（swi），带 idn
- [ ] 5.1.6 定义 ann_labels `swi_ann_labels[7][3]`
- [ ] 5.1.7 定义 annotation_rows `swi_ann_rows[7]`
- [ ] 5.1.8 定义 inputs/outputs/tags 数组
- [ ] 5.1.9 定义 `struct swi_word`（startN, endN, type_int, data_int, bit_string[16], inverted）
- [ ] 5.1.10 定义 `struct swi_packet`（startN, endN, recieve, first_two_bytes, last_byte, packetClass, recieve2）
- [ ] 5.1.11 定义 `struct swi_priv`（samplerate, strt, halfRate, pastNs/pastVs ring buffer, pastWords[], word_count, lastHdrIdx, packetClass, recieveData, pastPackets[], packet_count, readPacketSeq, polling, readOdcNumber, pastBits[], pastUidData[], startUidByte, bitsIdx, enumIdx, out_ann）
- [ ] 5.1.12 实现 `swi_calculate_bauds()`：计算 baud 间隔（4.47μs 基准，halfRate 检测）
- [ ] 5.1.13 实现 `swi_calculate_bit()`：从 baud 值计算 bit 值
- [ ] 5.1.14 实现 `swi_save_log()`：保存边沿日志
- [ ] 5.1.15 实现 `swi_parse_enumerate()`：枚举阶段解析
- [ ] 5.1.16 实现 `swi_parse_broadcast()`：广播消息解析
- [ ] 5.1.17 实现 `swi_parse_unicast()`：单播消息解析
- [ ] 5.1.18 实现 `swi_parse_packet()`：数据包解析（p0/p1/ecce）
- [ ] 5.1.19 实现 `swi_parse_packet_p0()`：Class 0 数据包解析
- [ ] 5.1.20 实现 `swi_parse_packet_ecce()`：ECCE 认证解析
- [ ] 5.1.21 实现 `swi_reset()`：g_malloc0 分配 + memset 清零
- [ ] 5.1.22 实现 `swi_start()`：注册 SRD_OUTPUT_ANN
- [ ] 5.1.23 实现 `swi_metadata()`：保存 samplerate
- [ ] 5.1.24 实现 `swi_decode()`：
  - samplerate 守卫检查
  - 主循环 while(1)
  - 使用 c_cond_edge 等待任意边沿
  - strt 标记处理
  - 计算 bauds
  - 检查有效间隔（1或3 baud）
  - 检查 5 baud 间隔（word 分隔）
  - 收集 13 个 baud 组成 word
  - 解析 word：training bits, data bits, invert bit
  - 根据 word_type 分发到 parse_broadcast/parse_unicast
  - 保存 word 到历史
- [ ] 5.1.25 实现 `swi_destroy()`：g_free 释放私有数据
- [ ] 5.1.26 定义 `struct srd_c_decoder swi_c_decoder`
- [ ] 5.1.27 实现 `srd_c_decoder_entry()`：无选项，直接返回 decoder 指针
- [ ] 5.1.28 实现 `srd_c_decoder_api_version()`

### 5.2 修改 CMakeLists.txt

- [ ] 5.2.1 在 `C_DECODERS` 列表中添加 `swi_c`

### 5.3 验证

- [ ] 5.3.1 编译通过
- [ ] 5.3.2 在 PXView 中加载解码器，验证 word 检测和协议解析

---

## 依赖关系

- Task 1 (tdm_audio) 和 Task 2 (timing) 互相独立，可并行
- Task 3 (t55xx) 和 Task 4 (spi-fast) 互相独立，可并行
- Task 5 (swi) 最复杂，建议最后实现
- 所有 Task 完成后统一修改 CMakeLists.txt 并编译验证

## 建议实现顺序

1. **tdm_audio_c** (最简单，无状态机，~250行) — 验证基本框架
2. **timing_c** (简单，滑动窗口，~350行) — 验证时间格式化
3. **t55xx_c** (中等，2状态机+配置解码，~600行) — 验证阈值计算
4. **spi_fast_c** (中等，多条件等待，~500行) — 参考 spi_c.c 已有实现
5. **swi_c** (最复杂，多层协议解析，~900行) — 最后实现
