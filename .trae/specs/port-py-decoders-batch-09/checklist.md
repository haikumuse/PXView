# 验证清单 — Batch 09: jitter, lfast, maple_bus, miller, morse

## 通用验证项（适用于所有 5 个解码器）

### 结构合规性
- [ ] 文件命名正确：`{id}_c.c`（jitter_c.c, lfast_c.c, maple_bus_c.c, miller_c.c, morse_c.c）
- [ ] `.id` 字段格式正确：`"xxx_c"`（如 `"jitter_c"`）
- [ ] `.name` 字段格式正确：`"XXX(C)"`（如 `"Jitter(C)"`）
- [ ] `ann_labels` 第一列全部为 `""`（空字符串）
- [ ] `ann_labels` 第二列为 id 字符串，第三列为显示标签
- [ ] 所有 annotation class 都映射到 annotation_rows
- [ ] annotation_rows 的 classes 数组以 `-1` 结尾
- [ ] annotation_rows 中 `count` 字段等于 classes 数组中有效元素数（不含 -1）
- [ ] `SRD_C_DECODER_EXPORT` 宏用于 `srd_c_decoder_entry()` 和 `srd_c_decoder_api_version()`
- [ ] `srd_c_decoder_api_version()` 返回 `SRD_C_DECODER_API_VERSION`
- [ ] inputs 包含 `"logic"` 且以 `NULL` 结尾
- [ ] channels 数组中每个 channel 有 `idn` 字段
- [ ] options 数组中每个 option 有 `idn` 字段

### 内存管理
- [ ] `reset()` 中使用 `g_malloc0()` 分配私有结构体（首次检查 NULL）
- [ ] `destroy()` 中使用 `g_free()` 释放私有结构体
- [ ] `destroy()` 中将 private 设为 NULL
- [ ] 无内存泄漏（所有 g_malloc 对应 g_free，所有 g_slist_append 的值在必要时释放）

### Samplerate 处理
- [ ] 实现 `metadata` 回调获取 samplerate
- [ ] `decode()` 开头有 samplerate fallback：`if (!s->samplerate) s->samplerate = c_decoder_get_samplerate(di);`
- [ ] samplerate 为 0 时安全退出（return，不崩溃）
- [ ] morse 特殊：samplerate 为 0 时 fallback 到 1.0（与 Python 行为一致）

### Option 初始化
- [ ] 所有 option 在 `srd_c_decoder_entry()` 中初始化
- [ ] String option 使用 `g_variant_new_string()`
- [ ] Int option 使用 `g_variant_new_int64()`
- [ ] Double option 使用 `g_variant_new_double()`
- [ ] String option 的 values 列表使用 `GSList` + `g_variant_new_string()`
- [ ] Option 的 `idn` 字段与 Python 版本一致

### Condition Builder 使用
- [ ] 每次 `c_cond_new()` 后都有对应的 `c_cond_free()`
- [ ] `c_cond_wait()` 返回值检查：`if (ret != SRD_OK) return;`
- [ ] `c_cond_or()` 正确用于组合多个条件
- [ ] `c_cond_skip()` 用于超时等待
- [ ] `c_cond_edge/rise/fall` 正确用于边沿检测

### Annotation 输出
- [ ] `C_ANN_PUT` 宏使用正确：`(di, ss, es, out_ann, class, ...)`
- [ ] ss ≤ es（start sample ≤ end sample）
- [ ] es=0 时由 API 自动填充为当前 samplenum
- [ ] 每个 annotation class 至少有一个字符串参数

---

## jitter_c 专项验证

### 元数据匹配
- [ ] 2 channels: clk(0), sig(1)
- [ ] 2 options: clk_polarity(rising/falling/both), sig_polarity(rising/falling/both)
- [ ] 3 annotations: jitter(0), clk_missed(1), sig_missed(2)
- [ ] 3 annotation_rows: jitter(0), clk_missed(1), sig_missed(2)
- [ ] 1 binary: ascii-float(0)

### 逻辑验证
- [ ] 双通道边沿等待：`c_cond_edge(cb, 0); c_cond_or(cb); c_cond_edge(cb, 1);`
- [ ] CLK→SIG 状态转换正确
- [ ] SIG→CLK 状态转换正确
- [ ] Jitter 计算公式：`(sig_start - clk_start) / samplerate`
- [ ] 时间格式化：自动选择 fs/ps/ns/μs/ms/s
- [ ] Missed clock 检测：在 SIG 状态等待时检测到 CLK 边沿
- [ ] Missed signal 检测：在 CLK 状态等待时检测到 SIG 边沿
- [ ] 同一样本不重复处理（clk_start/sig_start == samplenum 检查）
- [ ] Binary 输出：delta 格式化为 ASCII float + '\n'
- [ ] edge_type 根据 polarity option 正确设置

---

## lfast_c 专项验证

### 元数据匹配
- [ ] 1 channel: data(0)
- [ ] 0 options
- [ ] 9 annotations: bit(0), sync(1), header_pl_size(2), header_ch_type(3), header_cts(4), payload(5), ctrl_data(6), sleep(7), warning(8)
- [ ] 3 annotation_rows: bits(0), fields(1-7), warnings(8)
- [ ] outputs 包含 `"lfast"`

### 逻辑验证
- [ ] 超时等待：`c_cond_edge(cb, 0); c_cond_or(cb); c_cond_skip(cb, timeout);`
- [ ] Sync 检测：16 bits bitpack == 0xA84B
- [ ] Header 解析：size_id(3 bits), ch_type_id(4 bits), cts(1 bit)
- [ ] Payload size 查找：payload_byte_sizes[size_id] 正确
- [ ] Channel type 查找：channel_types[ch_type_id] 正确
- [ ] 数据通道(0b0100-0b1011) vs 控制通道区分正确
- [ ] Control payload 查找：control_payloads[value] 正确
- [ ] Sleepbit 检测：bit=1 → sleep, bit=0 → no sleep
- [ ] OUTPUT_PYTHON 输出：数据通道的 payload
- [ ] bit_len 自动检测：第一个边沿间隔
- [ ] prev_bit_len 保存：reset 后使用上次的 bit_len
- [ ] 超时处理：各状态超时值正确
  - sync: 16.2 * bit_len
  - header: 9.4 * bit_len
  - payload: (payload_size - len) * 8 * bit_len
  - sleepbit: 1.4 * bit_len
- [ ] bit_count=0 时重置（bit time too short warning）

---

## maple_bus_c 专项验证

### 元数据匹配
- [ ] 2 channels: sdcka(0), sdckb(1)
- [ ] 0 options
- [ ] 15 annotations: start(0), end(1), start-with-crc(2), occupancy(3), reset(4), bit(5), size(6), source(7), dest(8), command(9), data(10), checksum(11), frame-error(12), checksum-error(13), size-error(14)
- [ ] 3 annotation_rows: bits(0-5), fields(6-11), warnings(12-14)
- [ ] 6 binary classes: size(0), source(1), dest(2), command(3), data(4), checksum(5)

### 逻辑验证
- [ ] handle_start: SDCKA low + SDCKB high 等待
- [ ] handle_start: SDCKB 下降沿计数正确
- [ ] Start pattern: count=4
- [ ] Start with CRC: count=6
- [ ] Occupancy: count=8
- [ ] Reset: count≥14
- [ ] Frame error: 其他 count 值
- [ ] handle_byte_or_stop: 4 bit 对解码
- [ ] SDCKA 下降沿时读取 SDCKB 值
- [ ] SDCKB 下降沿时读取 SDCKA 值
- [ ] End pattern: counta=1, countb=0, data=0, sdckb=0
- [ ] Byte 累加：`data = data * 2 + n`（MSB-first）
- [ ] expected_length = 4 * (data + 1)（从 byte 0 计算）
- [ ] Checksum: XOR 所有 bytes
- [ ] Checksum error: 最后 byte != checksum
- [ ] Size error: length != expected_length + 1
- [ ] Binary 输出：每个 byte 输出对应 bintype
- [ ] pending_bit 延迟输出机制正确

---

## miller_c 专项验证

### 元数据匹配
- [ ] 1 channel: data(0)
- [ ] 2 options: baudrate(int, default=106000), edge(string, rising/falling/either, default=falling)
- [ ] 2 annotations: bit(0), bitstring(1)
- [ ] 2 annotation_rows: bit(0), bitstring(1)
- [ ] 1 binary: raw(0)

### 逻辑验证
- [ ] timeunit = samplerate / baudrate
- [ ] Edge type 条件构建正确（rising/falling/either）
- [ ] 第一个边沿后初始 bit=0
- [ ] timedelta 计算：`round(sampledelta / timeunit, 0.5)` → 可得 1.0, 1.5, 2.0
- [ ] Miller 解码规则完整：
  - prevbit=0, timedelta=1.0 → bit 0
  - prevbit=0, timedelta=1.5 → bit 1
  - prevbit=0, timedelta≥2.0 → end
  - prevbit=1, timedelta=1.0 → bit 1
  - prevbit=1, timedelta=1.5 → bit 0 + bit 0
  - prevbit=1, timedelta=2.0 → bit 0 + bit 1
  - prevbit=1, timedelta>2.0 → bit 0 + end
- [ ] timedelta≤0.5 → error
- [ ] 超时（3*timeunit 无边沿）→ end of message
- [ ] Bitstring 格式化：每 4 bits 加空格
- [ ] Binary 输出：bitvalue → little-endian bytes
- [ ] decode_run 循环：decode_bits → decode_run → decode

---

## morse_c 专项验证

### 元数据匹配
- [ ] 1 channel: data(0)
- [ ] 1 option: timeunit(double, default=0.1)
- [ ] 5 annotations: time(0), units(1), symbol(2), letter(3), word(4)
- [ ] 5 annotation_rows: 每个 annotation 独占一行
- [ ] morse_alphabet 查找表完整（26字母+10数字+标点）

### 逻辑验证
- [ ] samplerate fallback 到 1.0（与 Python 一致）
- [ ] 超时等待：5 * samplerate * timeunit
- [ ] 符号检测：
  - (1,1) = dit (.)
  - (1,3) = dah (-)
  - (0,1) = intra-character gap
  - (0,3) = letter separator
  - (0,7) = word separator
- [ ] 自适应 timeunit: `timeunit += (thisunit - timeunit) * 0.2 * max(0, 1 - 2*error)`
- [ ] 超时 → flush word
- [ ] Sequence 收集：连续 mark 符号组成 tuple
- [ ] Letter flush: space ≥ 3 单位
- [ ] Alphabet 查找：sequence → letter
- [ ] 未知字母回退：输出 encode_ditdah 字符串
- [ ] Word 收集和 flush
- [ ] iunits 最小值为 1
- [ ] Time annotation 格式：`{:.3g}`
- [ ] Units annotation 格式：`{:.1f}*{:.3g}` 或 `!! {:.1f}*{:.3g} !!`

---

## 编译验证

- [ ] CMakeLists.txt 中 C_DECODERS 列表已添加 5 个解码器
- [ ] `build_incremental.cmd` 执行成功
- [ ] 5 个 DLL 文件生成到 `build.dir/decoders/c_decoders/`
- [ ] 无编译警告（或仅有可接受的警告）
- [ ] PXView 启动无崩溃
- [ ] 解码器列表中可见 5 个新 C 解码器
- [ ] 加载解码器到对应通道无崩溃

## 与 Python 版本对比验证

- [ ] jitter: 使用双通道信号数据，对比 jitter 值输出
- [ ] lfast: 使用 LFAST 信号数据，对比 sync/header/payload 解码
- [ ] maple_bus: 使用 Maple bus 信号数据，对比帧解码
- [ ] miller: 使用 Miller 编码信号数据，对比 bit/bitstring 输出
- [ ] morse: 使用 Morse 码信号数据，对比 letter/word 解码
