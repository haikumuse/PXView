# 任务列表 — Batch 09: jitter, lfast, maple_bus, miller, morse

## 解码器 1: jitter_c

### Task 1.1: 创建 `jitter_c.c` 骨架
- [ ] 创建文件 `libsigrokdecode/c_decoders/jitter_c.c`
- [ ] 包含头文件: `libsigrokdecode.h`, `<glib.h>`, `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<math.h>`
- [ ] 定义 `enum jitter_state { STATE_CLK, STATE_SIG }`
- [ ] 定义 `struct jitter_priv`（state, samplerate, oldclk, oldsig, clk_start, sig_start, clk_missed, sig_missed, clk_edge_type, sig_edge_type, out_ann, out_binary）
- [ ] 定义 channels 数组（2 channels: clk, sig）
- [ ] 定义 options 数组（2 options: clk_polarity, sig_polarity — 各含 rising/falling/both）
- [ ] 定义 ann_labels（3 个，第一列为 ""）
- [ ] 定义 annotation_rows（3 行: jitter, clk_missed, sig_missed）
- [ ] 定义 binary（1 个: ascii-float）
- [ ] 定义 inputs/outputs/tags

### Task 1.2: 实现 jitter 回调函数
- [ ] `jitter_reset()`: g_malloc0 私有结构体，初始化 state=STATE_CLK
- [ ] `jitter_start()`: 注册 out_ann 和 out_binary，读取 polarity options 转换为 edge_type
- [ ] `jitter_metadata()`: 获取 samplerate
- [ ] `jitter_destroy()`: g_free 私有结构体

### Task 1.3: 实现 jitter_decode 核心逻辑
- [ ] samplerate guard（metadata + fallback）
- [ ] 主循环：`c_cond_edge(cb, 0); c_cond_or(cb); c_cond_edge(cb, 1);`
- [ ] 读取两个通道 pin 值
- [ ] 内层循环模拟 Python 双步状态推进
- [ ] STATE_CLK: 检测 clock 边沿 → 记录 clk_start → 切换 SIG；检测 missed signal
- [ ] STATE_SIG: 检测 signal 边沿 → 记录 sig_start → 计算 jitter → 切换 CLK；检测 missed clock
- [ ] 实现 `format_jitter()` 辅助函数（自动选择 fs/ps/ns/μs/ms/s）
- [ ] 实现 `is_edge()` 辅助函数（rising/falling/both）
- [ ] Binary 输出：ASCII float + '\n'

### Task 1.4: 实现 srd_c_decoder_entry
- [ ] 初始化 clk_polarity option（string, values: rising/falling/both, default: rising）
- [ ] 初始化 sig_polarity option（string, values: rising/falling/both, default: rising）
- [ ] 返回 decoder 指针

### Task 1.5: 验证
- [ ] 编译通过
- [ ] 与 Python 版本输出对比

---

## 解码器 2: lfast_c

### Task 2.1: 创建 `lfast_c.c` 骨架
- [ ] 创建文件 `libsigrokdecode/c_decoders/lfast_c.c`
- [ ] 定义 `enum lfast_state { STATE_SYNC, STATE_HEADER, STATE_PAYLOAD, STATE_SLEEPBIT }`
- [ ] 定义 `struct lfast_priv`（state, ss, es, ss_bit, es_bit, ss_sync, ss_header, ss_byte, ss_payload, es_payload, bit_len, prev_bit_len, timeout, bits[], bit_count, payload_size, ch_type_id, payload_bytes[], payload_byte_count, out_ann, out_python）
- [ ] 定义 channels（1 channel: data）
- [ ] 定义 ann_labels（9 个）
- [ ] 定义 annotation_rows（3 行: bits, fields, warnings）
- [ ] 定义 payload_sizes/payload_byte_sizes/channel_types/control_payloads 查找表
- [ ] 定义 inputs/outputs/tags

### Task 2.2: 实现 lfast 回调函数
- [ ] `lfast_reset()`: g_malloc0，初始化 state=STATE_SYNC，prev_bit_len=0xFFFFFFFF
- [ ] `lfast_start()`: 注册 out_ann 和 out_python
- [ ] `lfast_destroy()`: g_free

### Task 2.3: 实现 lfast_decode 核心逻辑
- [ ] 主循环：`c_cond_edge(cb, 0); c_cond_or(cb); c_cond_skip(cb, timeout);`
- [ ] 超时检测：检查 matched 第二位
- [ ] bit_len 自动检测（第一个边沿间隔）
- [ ] bit_count 计算：`round((es-ss)/bit_len)`
- [ ] bit_value 判断：rising edge → '0', falling edge → '1'
- [ ] handle_sync: 收集 16 bits，bitpack 检查 0xA84B
- [ ] handle_header: 收集 8 bits，解析 size_id(3)/ch_type_id(4)/cts(1)
- [ ] handle_payload: 收集 payload_size*8 bits，区分数据/控制通道
- [ ] handle_sleepbit: 检查 1 bit
- [ ] 实现 bitpack 辅助函数
- [ ] OUTPUT_PYTHON 输出：`c_decoder_put_python()`

### Task 2.4: 实现 srd_c_decoder_entry
- [ ] 无 options 需要初始化
- [ ] 返回 decoder 指针

### Task 2.5: 验证
- [ ] 编译通过
- [ ] 与 Python 版本输出对比

---

## 解码器 3: maple_bus_c

### Task 3.1: 创建 `maple_bus_c.c` 骨架
- [ ] 创建文件 `libsigrokdecode/c_decoders/maple_bus_c.c`
- [ ] 定义 `struct maple_priv`（ss, es, data, length, expected_length, checksum, pending_bit, pending_bit_pos, out_ann, out_binary）
- [ ] 定义 channels（2 channels: sdcka, sdckb）
- [ ] 定义 ann_labels（15 个）
- [ ] 定义 annotation_rows（3 行: bits, fields, warnings）
- [ ] 定义 binary（6 个: size, source, dest, command, data, checksum）
- [ ] 定义 inputs/outputs/tags

### Task 3.2: 实现 maple_bus 回调函数
- [ ] `maple_bus_reset()`: g_malloc0
- [ ] `maple_bus_start()`: 注册 out_ann 和 out_binary
- [ ] `maple_bus_destroy()`: g_free

### Task 3.3: 实现 handle_start 函数
- [ ] 等待 SDCKA=low, SDCKB=high（使用 c_cond_low + c_cond_high + c_cond_or）
- [ ] 计数循环：等待 SDCKB 下降沿或 SDCKA 上升沿
- [ ] count=4 → Start, count=6 → Start with CRC, count=8 → Occupancy, count≥14 → Reset
- [ ] 输出对应 annotation

### Task 3.4: 实现 handle_byte_or_stop 函数
- [ ] 4 个 bit 对解码循环
- [ ] 等待 SDCKA 下降沿或 SDCKB 下降沿
- [ ] SDCKA 下降沿：读取 SDCKB 值作为 bit
- [ ] SDCKB 下降沿：读取 SDCKA 值作为 bit
- [ ] End pattern 检测：counta=1, countb=0, data=0, sdckb=0
- [ ] 4 bits 组成 1 byte
- [ ] 实现 got_bit / got_byte / byte_annotation 辅助函数

### Task 3.5: 实现 maple_bus_decode 主循环
- [ ] 外层循环：handle_start → handle_byte_or_stop 循环
- [ ] 帧初始化：length=0, expected_length=4, checksum=0
- [ ] Byte 0: expected_length = 4 * (data + 1)
- [ ] 最后 byte: checksum 验证
- [ ] Size error / Checksum error 检测
- [ ] Binary 输出

### Task 3.6: 实现 srd_c_decoder_entry
- [ ] 无 options
- [ ] 返回 decoder 指针

### Task 3.7: 验证
- [ ] 编译通过
- [ ] 与 Python 版本输出对比

---

## 解码器 4: miller_c

### Task 4.1: 创建 `miller_c.c` 骨架
- [ ] 创建文件 `libsigrokdecode/c_decoders/miller_c.c`
- [ ] 定义 `struct miller_priv`（samplerate, timeunit, edge_type, prevbit, prevedge, expectedstart, bits[], numbits, bitvalue, stringstart, stringend, out_ann, out_binary）
- [ ] 定义 channels（1 channel: data）
- [ ] 定义 options（2: baudrate=int, edge=string with values）
- [ ] 定义 ann_labels（2 个）
- [ ] 定义 annotation_rows（2 行: bit, bitstring）
- [ ] 定义 binary（1 个: raw）
- [ ] 定义 inputs/outputs/tags

### Task 4.2: 实现 miller 回调函数
- [ ] `miller_reset()`: g_malloc0
- [ ] `miller_start()`: 注册 out_ann 和 out_binary，读取 baudrate 和 edge option
- [ ] `miller_metadata()`: 获取 samplerate，计算 timeunit = samplerate / baudrate
- [ ] `miller_destroy()`: g_free

### Task 4.3: 实现 miller_decode 核心逻辑
- [ ] samplerate guard
- [ ] 等待第一个边沿（根据 edge option 选择 rise/fall/edge）
- [ ] 初始 bit=0，输出
- [ ] 主循环：等待边沿或超时（3*timeunit）
- [ ] 计算 timedelta = round(sampledelta / timeunit, 0.5)
- [ ] Miller 解码规则：
  - prevbit=0: 1.0→0, 1.5→1, ≥2.0→end
  - prevbit=1: 1.0→1, 1.5→0+0, 2.0→0+1, >2.0→0+end
- [ ] 超时 → end of message
- [ ] decode_run: 收集 bits → bitstring → binary 输出

### Task 4.4: 实现 srd_c_decoder_entry
- [ ] 初始化 baudrate option（int64, default: 106000）
- [ ] 初始化 edge option（string, values: rising/falling/either, default: falling）
- [ ] 返回 decoder 指针

### Task 4.5: 验证
- [ ] 编译通过
- [ ] 与 Python 版本输出对比

---

## 解码器 5: morse_c

### Task 5.1: 创建 `morse_c.c` 骨架
- [ ] 创建文件 `libsigrokdecode/c_decoders/morse_c.c`
- [ ] 定义 `struct morse_priv`（samplerate, timeunit, out_ann, out_binary, prev_val, prev_time, sequence[], seq_len, letter_ss, letter_es, word[], word_len, word_ss, word_es）
- [ ] 定义 channels（1 channel: data）
- [ ] 定义 options（1: timeunit=double, default 0.1）
- [ ] 定义 ann_labels（5 个）
- [ ] 定义 annotation_rows（5 行，每个 annotation 独占一行）
- [ ] 定义 morse_alphabet 查找表（字母+数字+标点）
- [ ] 定义 inputs/outputs/tags

### Task 5.2: 实现 morse 回调函数
- [ ] `morse_reset()`: g_malloc0
- [ ] `morse_start()`: 注册 out_ann 和 out_binary，读取 timeunit option
- [ ] `morse_metadata()`: 获取 samplerate（fallback 到 1.0）
- [ ] `morse_destroy()`: g_free

### Task 5.3: 实现 morse_decode 核心逻辑
- [ ] samplerate guard（fallback 到 1.0）
- [ ] 等待上升沿开始
- [ ] 符号层循环：等待边沿或超时（5*samplerate*timeunit）
- [ ] 计算电平持续时间 → units → iunits
- [ ] 符号映射：(1,1)=dit, (1,3)=dah, (0,1)=gap, (0,3)=letter_sep, (0,7)=word_sep
- [ ] 自适应 timeunit: `timeunit += (thisunit - timeunit) * 0.2 * max(0, 1 - 2*error)`
- [ ] 超时 → flush word
- [ ] 字母层：收集 sequence → 查找 alphabet → 输出 letter annotation
- [ ] 单词层：收集 letters → 输出 word annotation
- [ ] 实现 lookup_morse() 辅助函数
- [ ] 实现 encode_ditdah() 辅助函数（用于未知字母回退）

### Task 5.4: 实现 srd_c_decoder_entry
- [ ] 初始化 timeunit option（double, default: 0.1）
- [ ] 返回 decoder 指针

### Task 5.5: 验证
- [ ] 编译通过
- [ ] 与 Python 版本输出对比

---

## 通用任务

### Task 6.1: 修改 CMakeLists.txt
- [ ] 在 `C_DECODERS` 列表中添加: `jitter_c lfast_c maple_bus_c miller_c morse_c`

### Task 6.2: 编译验证
- [ ] 运行 `build_incremental.cmd`
- [ ] 确认 5 个 DLL 成功生成到 `build.dir/decoders/c_decoders/`

### Task 6.3: 运行时验证
- [ ] 在 PXView 中加载各解码器，确认无崩溃
- [ ] 使用对应协议的采样数据验证解码输出
