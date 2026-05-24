# 任务列表 — Batch 10 Python→C 解码器移植

## 总览

5 个解码器按复杂度从低到高排序实现。每个解码器的任务分为：创建文件、实现核心逻辑、集成构建、验证。

---

## Task 1: MCS-48 解码器移植

### 1.1 创建 `mcs48_c.c`
- [ ] 创建文件 `libsigrokdecode/c_decoders/mcs48_c.c`
- [ ] 定义 14 个 channels（ale, psen, d0-d7, a8-a11）+ 1 个 optional_channel（a12）
- [ ] 定义 1 个 annotation（romdata）+ 1 个 binary（romdata）
- [ ] 定义 ann_labels（第一列为空字符串）
- [ ] 定义 annotation_rows（1 行包含 ann 0）
- [ ] 定义 `struct mcs48_priv`（addr, addr_s, data, data_s, started, has_bank, out_ann, out_bin）

### 1.2 实现回调函数
- [ ] `mcs48_reset()`: 分配 priv，memset 清零，started=0
- [ ] `mcs48_start()`: 注册 out_ann 和 out_bin，检查 has_bank
- [ ] `mcs48_decode()`: 主循环等待 ALE 下降沿或 /PSEN 上升沿
  - [ ] ALE 下降沿：重建地址（A8-A11 << 8 | D0-D7，可选 A12 << 12）
  - [ ] /PSEN 上升沿：读取 D0-D7 数据，输出 `AAAA:DD` 注解和 3 字节二进制
- [ ] `mcs48_destroy()`: 释放 priv

### 1.3 实现 `srd_c_decoder_entry()`
- [ ] 初始化 decoder struct（id="mcs48_c", name="MCS-48(C)"）
- [ ] 无 options 需要初始化

### 1.4 构建集成
- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `mcs48`

---

## Task 2: OneSingleWire 解码器移植

### 2.1 创建 `one_single_wire_c.c`
- [ ] 创建文件 `libsigrokdecode/c_decoders/one_single_wire_c.c`
- [ ] 定义 2 个 channels（osw, strt）
- [ ] 定义 1 个 option（threshold, int, default=8）
- [ ] 定义 5 个 annotations（bit, byte, sample, wait, pb）
- [ ] 定义 3 个 annotation_rows
- [ ] 定义 `struct osw_priv`（bt_block_ss, by_block_ss, bit_index, decoded_byte, parity_bit, threshold_samples, samplerate, out_ann, out_python）

### 2.2 实现回调函数
- [ ] `osw_reset()`: 分配 priv，memset 清零
- [ ] `osw_start()`: 注册 out_ann 和 out_python，读取 threshold 选项
- [ ] `osw_metadata()`: 接收 samplerate，计算 `threshold_samples = threshold * samplerate / 1000000`
- [ ] `osw_decode()`:
  - [ ] 阶段 1：等待 strt 上升沿
  - [ ] 阶段 2：等待 osw 下降沿
  - [ ] 阶段 3：主循环等待 osw 边沿
    - [ ] 计算 period_range，与 threshold 比较
    - [ ] bit 0-7：数据位（LSB first），bit 8：parity
    - [ ] bit 7 时输出 Byte 注解
    - [ ] bit 8 时输出 PB（parity check）注解
    - [ ] 每个 bit 输出 Bit 和 Sample 注解
    - [ ] bit 9+：输出 Wait 注解，重置状态
- [ ] `osw_destroy()`: 释放 priv

### 2.3 实现 `srd_c_decoder_entry()`
- [ ] 初始化 threshold option（g_variant_new_int64(8)）

### 2.4 输出
- [ ] outputs 包含 `"OneSingleWire"`
- [ ] 注册 out_python 输出

### 2.5 构建集成
- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `one_single_wire`

---

## Task 3: MVB 解码器移植

### 3.1 创建 `mvb_c.c`
- [ ] 创建文件 `libsigrokdecode/c_decoders/mvb_c.c`
- [ ] 定义 1 个 channel（mvb）
- [ ] 定义 9 个 annotations
- [ ] 定义 5 个 annotation_rows
- [ ] 定义常量：PREAMBLE_MASTER, PREAMBLE_SLAVE, PREAMBLE_MASK, MVB_CLOCK_RATE
- [ ] 定义 F_codes 查找表（16 项）
- [ ] 定义 `struct mvb_priv`

### 3.2 实现 CRC 算法
- [ ] `mvb_crc8()`: 使用位操作实现 Modulo-2 除法（多项式 0xE5）
- [ ] `check_check_sequence()`: 计算 CRC 并与接收值比较
- [ ] 注意：Python 版本使用字符串操作，C 版本需要用位数组实现等效逻辑

### 3.3 实现 Manchester 解码
- [ ] `process_tick()`: preamble 匹配 + Manchester bit 解码
- [ ] `reset_frame()`: 重置帧状态

### 3.4 实现帧处理
- [ ] `process_master_frame()`: 解析 4-bit flag + 12-bit address + CRC
- [ ] `process_slave_frame()`: 处理 24-bit / 40-bit / 可变长度 slave 帧
  - [ ] 24-bit：16-bit data + 8-bit CRC
  - [ ] 40-bit：32-bit data + 8-bit CRC
  - [ ] 其他：按 72-bit（64+8）段分割

### 3.5 实现回调函数
- [ ] `mvb_reset()`: 分配 priv，memset 清零
- [ ] `mvb_start()`: 注册 out_ann
- [ ] `mvb_metadata()`: 接收 samplerate
- [ ] `mvb_decode()`:
  - [ ] 计算 samples_per_tick 和 mvb_samples_per_bit
  - [ ] 等待第一个下降沿
  - [ ] 主循环：等待边沿，计算 notch 长度
  - [ ] 将 notch 转换为 tick 数，调用 process_tick()
- [ ] `mvb_destroy()`: 释放 priv

### 3.6 实现 `srd_c_decoder_entry()`
- [ ] 无 options 需要初始化

### 3.7 构建集成
- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `mvb`

---

## Task 4: OpenTherm 解码器移植

### 4.1 创建 `opentherm_c.c`
- [ ] 创建文件 `libsigrokdecode/c_decoders/opentherm_c.c`
- [ ] 定义 1 个 channel（ot）
- [ ] 定义 9 个 options（polarity, bitlen, jitter_m, jitter_p, m2s_silence_min, m2s_silence_max, s2m_silence_min, m2m_act_max, ignore_glitches, format）
- [ ] 定义 14 个 annotations
- [ ] 定义 7 个 annotation_rows
- [ ] 定义 msg_type_table[8] 查找表
- [ ] 定义 `struct ot_priv`

### 4.2 实现时序计算
- [ ] `setup_calc()`: 从 options 和 samplerate 计算 halfbit, s_range, l_range, silence, glitchlen
- [ ] `s2t()`: samples → microseconds
- [ ] `t2s()`: microseconds → samples
- [ ] `edge_type()`: 判断边沿间隔类型（short/long/error）

### 4.3 实现 Manchester/Bi-phase-L FSM
- [ ] 状态：IDLE, SYNC, MID1, MID0, START1, START0
- [ ] IDLE：根据 polarity 检测起始边沿
- [ ] SYNC：检测 short 边沿进入 MID1
- [ ] MID1/MID0/START1/START0：根据边沿类型转换状态并解码 bit

### 4.4 实现帧解析
- [ ] `handle_bits()`: 解析 34-bit 帧
  - [ ] bit 0: Start bit
  - [ ] bit 1: Parity bit（XOR of bits 1-33 应为 0）
  - [ ] bits 2-4: MSG-TYPE（MSB first）
  - [ ] bits 5-8: Spare
  - [ ] bits 9-16: DATA-ID（MSB first）
  - [ ] bits 17-32: DATA-VALUE（MSB first）
  - [ ] bit 33: Stop bit
- [ ] `handle_timing_error()`: 输出 timing error 注解并重置
- [ ] `reset_decoder_state()`: 重置 FSM 状态

### 4.5 实现 Glitch 过滤
- [ ] 如果 `ignore_glitches > 0`，过滤短于 glitchlen 的脉冲

### 4.6 实现回调函数
- [ ] `ot_reset()`: 分配 priv，memset 清零
- [ ] `ot_start()`: 注册 out_ann，读取所有 options
- [ ] `ot_metadata()`: 接收 samplerate，调用 setup_calc()
- [ ] `ot_decode()`: 主循环（等待边沿 → glitch 过滤 → FSM 处理 → 帧解析）
- [ ] `ot_destroy()`: 释放 priv

### 4.7 实现 `srd_c_decoder_entry()`
- [ ] 初始化所有 9 个 options
  - polarity: string, default "active-low", values ["active-low", "active-high"]
  - bitlen: int64, default 1000
  - jitter_m: int64, default 100
  - jitter_p: int64, default 150
  - m2s_silence_min: int64, default 20000
  - m2s_silence_max: int64, default 800000
  - s2m_silence_min: int64, default 100000
  - m2m_act_max: int64, default 1150000
  - ignore_glitches: int64, default 0
  - format: string, default "dec", values ["hex", "dec", "oct", "bin"]

### 4.8 简化决策
- [ ] 决定是否实现 `otdecoder.py` 的参数描述表（建议不实现，保留 ann_descr 索引但不输出）

### 4.9 构建集成
- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `opentherm`

---

## Task 5: OOK 解码器移植

### 5.1 创建 `ook_c.c`
- [ ] 创建文件 `libsigrokdecode/c_decoders/ook_c.c`
- [ ] 定义 1 个 channel（data）
- [ ] 定义 5 个 options（invert, decodeas, preamble, preamlen, diffmanvar）
- [ ] 定义 6 个 annotations
- [ ] 定义 6 个 annotation_rows
- [ ] 定义 1 个 binary（pulse-lengths）
- [ ] 定义 `struct ook_priv`（包含 decoded/decoded_1010 数组、preamble 缓冲、pulse_lengths 等）

### 5.2 实现 Preamble 检测
- [ ] `lock_onto_preamble()`: 收集前 N 个脉冲
  - [ ] 过滤噪声（长/短比 > 5:1 视为垃圾）
  - [ ] 确定 sample_high 和 sample_low
  - [ ] 达到 preamble_len 后设置 insync

### 5.3 实现 NRZ 解码
- [ ] `decode_nrz()`: 根据 sample_high/sample_low 将脉冲宽度转换为 bit
  - [ ] 处理多位脉冲（宽度 > 1.5 倍基准）
  - [ ] DECODE_TIMEOUT 时输出 decoded 数据

### 5.4 实现 Manchester 解码
- [ ] `decode_manchester_sim()`: 核心 Manchester 解码逻辑
  - [ ] Long pulse（0.75-1.5 倍基准）
  - [ ] Short pulse（0.25-0.75 倍基准）
  - [ ] Error（过长或过短）
- [ ] `decode_manchester()`: 同时尝试 1111 和 1010 preamble
  - [ ] 维护两套解码状态（half_time, lstate, ss, errors）
  - [ ] Timeout 时选择错误少的版本输出

### 5.5 实现 Diff Manchester 解码
- [ ] `decode_diff_manchester()`: 基于边沿间转换方向解码
  - [ ] 维护 diff_man_trans 和 diff_man_len
  - [ ] 处理 1+1, 1+2, 2+1, 2+2 等组合
  - [ ] Error 情况处理

### 5.6 实现 Timeout 处理
- [ ] `decode_timeout()`: 重置所有状态
  - [ ] 清空 preamble、decoded、pulse_lengths
  - [ ] 重置 edge_count、man_errors
  - [ ] 状态回到 IDLE

### 5.7 实现二进制输出
- [ ] `dump_pulse_lengths()`: 将脉冲长度转为字符串输出

### 5.8 实现回调函数
- [ ] `ook_reset()`: 分配 priv，memset 清零
- [ ] `ook_start()`: 注册 out_ann, out_python, out_binary，读取 options
- [ ] `ook_metadata()`: 接收 samplerate
- [ ] `ook_decode()`: 主循环
  - [ ] edge_count==0 时等待边沿
  - [ ] edge_count>0 时等待边沿或 timeout（5 * sample_first）
  - [ ] 检测 timeout → DECODE_TIMEOUT
  - [ ] insync==0 时调用 lock_onto_preamble()
  - [ ] insync==1 时根据 decodeas 调用对应解码函数
- [ ] `ook_destroy()`: 释放 priv

### 5.9 实现 `srd_c_decoder_entry()`
- [ ] 初始化所有 5 个 options
  - invert: string, default "no", values ["no", "yes"]
  - decodeas: string, default "Manchester", values ["NRZ", "Manchester", "Diff Manchester"]
  - preamble: string, default "auto", values ["auto", "1010", "1111"]
  - preamlen: string, default "7", values ["0","3","4","5","6","7","8","9","10"]
  - diffmanvar: string, default "1", values ["1", "0"]

### 5.10 构建集成
- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `ook`

---

## Task 6: 构建验证

### 6.1 增量构建
- [ ] 运行 `build_incremental.cmd`
- [ ] 确认 5 个 DLL 成功生成到 `build.dir/decoders/c_decoders/`

### 6.2 运行时验证
- [ ] 启动 PXView，确认 5 个 C 解码器出现在解码器列表中
- [ ] 对每个解码器进行基本功能测试（如有对应信号文件）
