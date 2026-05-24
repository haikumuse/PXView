# 验证清单 — Batch 10 Python→C 解码器移植

---

## 通用验证项（适用于所有 5 个解码器）

### 代码结构
- [ ] 文件命名正确：`{decoder_id}_c.c`（`-` 替换为 `_`）
- [ ] 包含必要的头文件：`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<glib.h>`, `"libsigrokdecode.h"`
- [ ] `struct srd_c_decoder` 命名正确：`{decoder_id}_c_decoder`
- [ ] `.id` 字段格式正确：`"xxx_c"`（带 `_c` 后缀）
- [ ] `.name` 字段格式正确：`"XXX(C)"`（带 `(C)` 后缀）
- [ ] `.longname` 和 `.desc` 末尾有 `(C implementation)`

### ann_labels
- [ ] 第一列为空字符串 `""`
- [ ] 每行 3 个字符串：`{"", "id", "Label"}`
- [ ] 数量与 `num_annotations` 一致

### annotation_rows
- [ ] 所有 annotation class 都映射到某个 row
- [ ] 每个 row 的 class 数组以 `-1` 结尾
- [ ] `num_annotation_rows` 与实际行数一致
- [ ] row 中的 class 索引不越界

### channels / optional_channels
- [ ] channel 数量与 Python 版本一致
- [ ] 每个 channel 的 id、name、desc 与 Python 版本匹配
- [ ] optional_channels 正确区分
- [ ] `num_channels` 和 `num_optional_channels` 正确

### Options
- [ ] 所有 Python 选项都有对应的 C 定义
- [ ] 默认值类型正确（string → g_variant_new_string, int → g_variant_new_int64, double → g_variant_new_double）
- [ ] 枚举选项的 values 列表完整
- [ ] `srd_c_decoder_entry()` 中正确初始化所有 options

### 回调函数
- [ ] `reset`: 分配 priv（首次调用时 g_malloc0），后续调用 memset 清零
- [ ] `start`: 注册输出（out_ann, out_python, out_binary），读取 options
- [ ] `decode`: 入口检查 samplerate > 0
- [ ] `destroy`: g_free(priv)，设置 private 为 NULL
- [ ] `metadata`: 处理 SRD_CONF_SAMPLERATE（如需要）

### Condition Builder 使用
- [ ] 每次 `c_cond_new()` 后都有对应的 `c_cond_free()`
- [ ] `c_cond_wait()` 返回值检查：`ret != SRD_OK` 则 return
- [ ] `c_cond_or()` 在多个条件之间正确使用
- [ ] `c_cond_skip()` 参数正确（基于 samplerate 计算）

### 输出 API
- [ ] `C_ANN_PUT` 的 ss ≤ es
- [ ] `C_ANN_PUT` 的 class 索引不越界
- [ ] `c_decoder_put_python` 的 type 字符串与 Python 版本一致
- [ ] `c_decoder_put_binary` 数据格式正确

### Build 集成
- [ ] CMakeLists.txt 的 C_DECODERS 列表中已添加解码器名称
- [ ] 编译无 warning
- [ ] DLL 成功生成

---

## MCS-48 特定验证项

### Channels
- [ ] 14 个必需 channel：ale(0), psen(1), d0-d7(2-9), a8-a11(10-13)
- [ ] 1 个可选 channel：a12(14)
- [ ] channel 顺序与 Python 版本一致

### 解码逻辑
- [ ] ALE 下降沿正确触发地址锁存
- [ ] /PSEN 上升沿正确触发数据读取
- [ ] 地址重建：`(A8-A11 << 8) | D0-D7`，可选 `A12 << 12`
- [ ] `has_bank` 通过 `c_decoder_has_channel(di, 14)` 检测
- [ ] 两个条件可能同时匹配（ALE fall + PSEN rise 同一采样点）
- [ ] `started` 标志确保先收到 ALE 再输出数据

### 输出格式
- [ ] 注解格式：`AAAA:DD`（4 位十六进制地址:2 位十六进制数据）
- [ ] 二进制格式：3 字节（2 字节大端地址 + 1 字节数据）
- [ ] 只有 `started==1` 时才输出注解

---

## OneSingleWire 特定验证项

### Channels
- [ ] 2 个 channel：osw(0), strt(1)

### Options
- [ ] threshold 选项默认值 8（微秒）
- [ ] threshold_samples 在 metadata 回调中正确计算：`threshold * samplerate / 1000000`

### 解码逻辑
- [ ] 首先等待 strt 上升沿
- [ ] 然后等待 osw 下降沿
- [ ] bit 值判断：`period_range < threshold_samples` → 1，否则 → 0
- [ ] 数据位 LSB first（bit 0 是最低位）
- [ ] bit 0-7 是数据位，bit 8 是 parity
- [ ] parity 检查：9 个 bit 的 XOR 应为 0
- [ ] bit 7 时输出 Byte 注解
- [ ] bit 8 时输出 PB（parity check）注解
- [ ] bit 9+ 时输出 Wait 注解并重置

### 输出
- [ ] outputs 包含 `"OneSingleWire"`
- [ ] 注册了 out_python 输出

---

## MVB 特定验证项

### 常量
- [ ] PREAMBLE_MASTER = 0b101100011100010101 (0x16465)
- [ ] PREAMBLE_SLAVE = 0b101010100011100011 (0x15463)
- [ ] PREAMBLE_MASK = 0x3FFFF (18-bit)
- [ ] MVB_CLOCK_RATE = 3000000

### CRC 校验
- [ ] CRC 多项式 0xE5 (11100101)
- [ ] `check_check_sequence()` 正确实现
- [ ] CRC 错误时输出 CRC_ERROR 注解
- [ ] CRC 正确时输出 CRC 注解（十六进制值）

### Manchester 解码
- [ ] samples_per_tick = samplerate / MVB_CLOCK_RATE
- [ ] mvb_samples_per_bit = 2 * samples_per_tick
- [ ] 等待第一个下降沿后开始
- [ ] notch 长度转换为 tick 数（round 到整数）
- [ ] notch_length_mvb >= 4 时重置帧（视为间隔）
- [ ] 交替 phase 产生 tick 值

### Preamble 匹配
- [ ] 滑动窗口匹配 18-bit preamble
- [ ] 匹配后设置 received_master_header 或 received_slave_header
- [ ] 输出 Master/Slave preamble 注解

### 帧处理
- [ ] Master 帧：4-bit flag + 12-bit address + CRC
- [ ] Slave 帧 24-bit：16-bit data + CRC
- [ ] Slave 帧 40-bit：32-bit data + CRC
- [ ] Slave 帧其他长度：按 72-bit 段分割
- [ ] F_code 查找表 16 项完整
- [ ] F_code 0-4 对应的地址输出

### 边界情况
- [ ] decoded_buffer 溢出保护
- [ ] 连续相同 tick 值（00 或 11）正确终止帧
- [ ] reset_frame() 正确清除所有帧状态

---

## OpenTherm 特定验证项

### Options
- [ ] 9 个选项全部正确初始化
- [ ] polarity: "active-low" / "active-high"
- [ ] format: "hex" / "dec" / "oct" / "bin"
- [ ] 所有时间参数默认值与 Python 版本一致

### 时序计算
- [ ] halfbit = t2s(bitlen / 2.0)
- [ ] s_range: [halfbit - t2s(jitter_m), halfbit + t2s(jitter_p)]
- [ ] l_range: [halfbit*2 - t2s(jitter_m), halfbit*2 + t2s(jitter_p)]
- [ ] silence = t2s(min(m2s_silence_min, s2m_silence_min))
- [ ] glitchlen = t2s(ignore_glitches)

### Manchester/Bi-phase-L FSM
- [ ] 6 个状态：IDLE, SYNC, MID1, MID0, START1, START0
- [ ] IDLE → SYNC：根据 polarity 检测起始边沿
- [ ] SYNC → MID1：short 边沿，bit=1
- [ ] MID1 → START1 (short) 或 MID0 (long, bit=0)
- [ ] MID0 → START0 (short) 或 MID1 (long, bit=1)
- [ ] START1 → MID1 (short, bit=1)
- [ ] START0 → MID0 (short, bit=0)
- [ ] Error 边沿：调用 handle_bits() + handle_timing_error()

### 帧解析 (34-bit)
- [ ] bit 0: Start bit（应为 1）
- [ ] bit 1: Parity bit
- [ ] bits 2-4: MSG-TYPE（MSB first，3 bits）
- [ ] bits 5-8: Spare（4 bits）
- [ ] bits 9-16: DATA-ID（MSB first，8 bits）
- [ ] bits 17-32: DATA-VALUE（MSB first，16 bits）
- [ ] bit 33: Stop bit（应为 1）

### Parity 校验
- [ ] XOR of bits 1-33 应为 0
- [ ] Parity 错误时添加 "ParityError" 到 warning

### MSG-TYPE 查找表
- [ ] 8 项完整：READ-DATA, WRITE-DATA, INVALID-DATA, RESERVED, READ-ACK, WRITE-ACK, DATA-INVALID, UNKNOWN-DATAID
- [ ] 方向标注正确：M2S (0-3) / S2M (4-7)

### Glitch 过滤
- [ ] ignore_glitches > 0 时启用
- [ ] 短于 glitchlen 的脉冲被过滤
- [ ] 过滤时输出 Glitch warning 注解

### 不完整帧处理
- [ ] < 2 bits：输出 "Incomplete packet" warning
- [ ] < 5 bits：输出 warning
- [ ] < 9 bits：输出 warning
- [ ] < 17 bits：输出 warning
- [ ] < 33 bits：输出 warning
- [ ] < 34 bits：输出 warning

### DATA-VALUE 格式
- [ ] 根据 format 选项选择显示格式
- [ ] hex: %2X
- [ ] dec: %d
- [ ] oct: %03o
- [ ] bin: %08b（需要手动实现，C 没有 %b）

---

## OOK 特定验证项

### Options
- [ ] 5 个选项全部正确初始化
- [ ] invert: "no" / "yes"
- [ ] decodeas: "NRZ" / "Manchester" / "Diff Manchester"
- [ ] preamble: "auto" / "1010" / "1111"
- [ ] preamlen: "0"-"10"（字符串类型）
- [ ] diffmanvar: "1" / "0"

### Preamble 检测
- [ ] 收集前 N 个脉冲（N = preamlen）
- [ ] 噪声过滤：长/短比 > 5:1 视为垃圾，清空 preamble
- [ ] 确定 sample_high 和 sample_low
- [ ] 达到 preamble_len 后设置 insync=1

### NRZ 解码
- [ ] 根据 sample_high/sample_low 分割脉冲为多个 bit
- [ ] 宽度 > 1.5 倍基准时输出多个 bit
- [ ] DECODE_TIMEOUT 时输出 decoded 数据

### Manchester 解码
- [ ] 同时维护 1111 和 1010 两套解码状态
- [ ] Long pulse（0.75-1.5 倍基准）处理正确
- [ ] Short pulse（0.25-0.75 倍基准）处理正确
- [ ] Error pulse 处理正确
- [ ] Timeout 时选择错误少的版本
- [ ] preamble="auto" 时两种都显示

### Diff Manchester 解码
- [ ] diff_man_trans 初始为 '1'
- [ ] diff_man_len 初始为 1
- [ ] 1+1 组合：输出当前 trans，trans 翻转为 '1'
- [ ] 1+2 组合：输出当前 trans（半周期），trans 翻转为 '0'，增加虚拟 edge
- [ ] 2+1 组合：输出当前 trans，trans 翻转为 '1'
- [ ] 2+2 组合：输出两个 'E'（错误），trans 翻转为 '1'
- [ ] diffmanvar='0' 时 trans 初始翻转逻辑不同

### Timeout 处理
- [ ] 5 * sample_first 无边沿触发 timeout
- [ ] timeout 时输出 decoded 数据（通过 out_python）
- [ ] 错误数 < max_errors 时输出，否则输出 error message
- [ ] decode_timeout() 正确重置所有状态

### 二进制输出
- [ ] dump_pulse_lengths() 将脉冲长度转为字符串
- [ ] 格式：`Pulses(us)=val1,val2,...\n`
- [ ] 最后一个脉冲长度修正为 sample_first

### 数组边界
- [ ] decoded 数组不溢出（建议 1024 或更大）
- [ ] decoded_1010 数组不溢出
- [ ] pulse_lengths 数组不溢出
- [ ] preamble 数组不溢出（最大 10）

### 输出
- [ ] outputs 包含 `"ook"`
- [ ] 注册了 out_python 和 out_binary 输出

---

## 构建验证

### 编译
- [ ] 5 个 C 文件均无编译错误
- [ ] 无编译警告（-Wall -Wextra）
- [ ] DLL 成功生成到 `build.dir/decoders/c_decoders/`

### CMakeLists.txt
- [ ] C_DECODERS 列表包含：mvb, mcs48, one_single_wire, ook, opentherm
- [ ] 每个名称与文件名匹配（去掉 `_c` 后缀）

### 运行时
- [ ] PXView 启动无崩溃
- [ ] 解码器列表中出现 5 个 C 版本解码器
- [ ] 选择 C 解码器后无崩溃
- [ ] 解码器选项 UI 正确显示
