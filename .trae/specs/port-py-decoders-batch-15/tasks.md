# 任务列表 — Python → C 解码器移植 Batch 15

## 任务依赖关系

```
Task 1 (mipi_dsi_c) ─┐
Task 2 (pxx1_c) ─────┤──→ Task 6 (CMakeLists.txt) ──→ Task 7 (编译验证)
Task 3 (qi_c) ───────┤
Task 4 (rc_encode_c) ─┤
Task 5 (sdq_c) ──────┘
```

Task 1-5 可并行执行，Task 6 依赖全部完成，Task 7 依赖 Task 6。

---

## Task 1: 实现 `mipi_dsi_c` 解码器

**文件**: `libsigrokdecode/c_decoders/mipi_dsi_c.c`
**复杂度**: 复杂
**预计代码量**: ~350 行

### 子任务

- [ ] 1.1 创建文件骨架：includes、channel 定义、ann_labels、annotation_rows、inputs/outputs/tags
- [ ] 1.2 定义状态枚举 `enum mipi_dsi_state`（6 个状态：FIND_START, FIND_MODE_S0/S1/S2, FIND_DATA_EDGE, FIND_DATA_VALID）
- [ ] 1.3 定义注解枚举 `enum mipi_dsi_ann`（13 个注解：LP-00/01/10/11, EscapeMode, BTA, LPDT, DI, ECC, WC, CRC, Stop, Idle）
- [ ] 1.4 定义私有数据结构 `struct mipi_dsi_priv`
- [ ] 1.5 实现 `mipi_dsi_reset()` — 分配并初始化私有数据
- [ ] 1.6 实现 `mipi_dsi_start()` — 注册输出
- [ ] 1.7 实现 `mipi_dsi_decode()` 主循环：
  - [ ] 1.7.1 STATE_FIND_START: `c_cond_fall(cb, 0); c_cond_high(cb, 1)` → handle_start
  - [ ] 1.7.2 STATE_FIND_MODE_S0: `c_cond_low(cb, 0); c_cond_low(cb, 1)`
  - [ ] 1.7.3 STATE_FIND_MODE_S1: OR 条件 `c_cond_high(cb,0);c_cond_low(cb,1)` | `c_cond_low(cb,0);c_cond_high(cb,1)` → 保存 d0n/d0p
  - [ ] 1.7.4 STATE_FIND_MODE_S2: `c_cond_low(cb, 0); c_cond_low(cb, 1)` → handle_esc_bta
  - [ ] 1.7.5 STATE_FIND_DATA_EDGE: OR 条件 → 保存 d0n/d0p
  - [ ] 1.7.6 STATE_FIND_DATA_VALID: OR 条件 → 根据 matched 判断 data/stop
- [ ] 1.8 实现 handle_data 逻辑：LSB first 累积 8 位，输出 DI 注解
- [ ] 1.9 实现 handle_esc_bta 逻辑：根据 d0n 判断 ESC/BTA
- [ ] 1.10 实现 handle_stop 逻辑：输出 Stop 注解，回到 FIND_START
- [ ] 1.11 实现 `mipi_dsi_destroy()` — 释放私有数据
- [ ] 1.12 定义 `struct srd_c_decoder mipi_dsi_c_decoder` 结构体
- [ ] 1.13 实现 `srd_c_decoder_entry()` — 无 options
- [ ] 1.14 **关键注意**：将 Stop(11) 和 Idle(12) 加入 LP annotation_row（Python 中遗漏）

### 验证点
- [ ] 所有 13 个 annotation class 都映射到 annotation_rows
- [ ] OR 条件使用 `c_cond_or()` 正确分隔
- [ ] `c_decoder_get_pin()` 在 matched 后正确读取引脚值
- [ ] samplerate 守卫：decode 入口检查 samplerate

---

## Task 2: 实现 `pxx1_c` 解码器

**文件**: `libsigrokdecode/c_decoders/pxx1_c.c`
**复杂度**: 复杂
**预计代码量**: ~500 行

### 子任务

- [ ] 2.1 创建文件骨架：includes、channel 定义、ann_labels(20个)、annotation_rows(3个)、inputs/outputs/tags、binary
- [ ] 2.2 定义状态枚举 `enum pxx1_state`（19 个状态）
- [ ] 2.3 定义注解枚举 `enum pxx1_ann`（20 个注解）
- [ ] 2.4 定义私有数据结构 `struct pxx1_priv`
- [ ] 2.5 实现 `pxx1_reset()` — 分配并初始化私有数据
- [ ] 2.6 实现 `pxx1_start()` — 注册输出（ann + binary）
- [ ] 2.7 实现 `pxx1_decode()` 主循环：
  - [ ] 2.7.1 等待第一个下降沿
  - [ ] 2.7.2 主循环：等待上升沿 → 等待下降沿 → 计算 period_t
  - [ ] 2.7.3 根据 period_t 判断 bit(1/0) 或 break
- [ ] 2.8 实现 `pxx1_add_bit()` 函数：
  - [ ] 2.8.1 bit stuffing 检测（bit_one_cnt >= 6）
  - [ ] 2.8.2 byte 累积和 addByte
  - [ ] 2.8.3 state_word 累积
  - [ ] 2.8.4 调用 pxx1_process_state()
- [ ] 2.9 实现 `pxx1_add_byte()` 函数
- [ ] 2.10 实现 `pxx1_process_state()` 函数：
  - [ ] 2.10.1 wait_header: 8 bit → 检查 0x7E
  - [ ] 2.10.2 rx_model_id: 8 bit → Model ID
  - [ ] 2.10.3 rx_type: 2 bit → FCC/EU/EU+/AU+
  - [ ] 2.10.4 rx_range_check: 1 bit
  - [ ] 2.10.5 rx_fail_safe: 1 bit
  - [ ] 2.10.6 rx_country_code: 3 bit
  - [ ] 2.10.7 rx_bind: 1 bit
  - [ ] 2.10.8 rx_flag2: 8 bit
  - [ ] 2.10.9 rx_channels: 96 bit → nibble 累积 + 通道解码
  - [ ] 2.10.10 rx_rsrv2: 1 bit
  - [ ] 2.10.11 rx_euplus: 1 bit
  - [ ] 2.10.12 rx_disable_sport: 1 bit
  - [ ] 2.10.13 rx_powerlevel: 2 bit
  - [ ] 2.10.14 rx_highchan: 1 bit
  - [ ] 2.10.15 rx_telemetry_off: 1 bit
  - [ ] 2.10.16 rx_external_antena: 1 bit
  - [ ] 2.10.17 rx_crc: 16 bit
  - [ ] 2.10.18 rx_stop: 8 bit → 检查 0x7E
  - [ ] 2.10.19 error: 保持错误状态
- [ ] 2.11 实现 `pxx1_break_rx()` 函数
- [ ] 2.12 实现 `pxx1_destroy()` — 释放私有数据
- [ ] 2.13 定义 `struct srd_c_decoder pxx1_c_decoder` 结构体
- [ ] 2.14 实现 `srd_c_decoder_entry()` — 无 options

### 验证点
- [ ] bit stuffing 逻辑：`bit_one_cnt >= 6` 时跳过，`value == 0` 时重置计数
- [ ] `byte_cnt > 18` 时关闭 bit_stuffing
- [ ] channels 解码：nibble 累积排除 stuffing 位，96 bit = 24 nibbles
- [ ] period_t 阈值：23-25us(1), 15-17us(0), >=40us(break)
- [ ] samplerate 守卫

---

## Task 3: 实现 `qi_c` 解码器

**文件**: `libsigrokdecode/c_decoders/qi_c.c`
**复杂度**: 中等
**预计代码量**: ~400 行

### 子任务

- [ ] 3.1 创建文件骨架：includes、channel 定义、ann_labels(8个)、annotation_rows(3个)、inputs/outputs/tags
- [ ] 3.2 定义状态枚举 `enum qi_state`（IDLE, DATA）
- [ ] 3.3 定义注解枚举 `enum qi_ann`（8 个注解）
- [ ] 3.4 定义私有数据结构 `struct qi_priv`
- [ ] 3.5 实现 `qi_reset()` — 分配并初始化私有数据
- [ ] 3.6 实现 `qi_start()` — 注册输出
- [ ] 3.7 实现 `qi_metadata()` — 保存 samplerate，计算 bit_width
- [ ] 3.8 实现 `qi_decode()` 主循环：
  - [ ] 3.8.1 读取初始引脚值
  - [ ] 3.8.2 主循环：等待边沿 `{0: 'e'}` → 计算 l → handle_transition
- [ ] 3.9 实现 `qi_handle_transition()` 函数：
  - [ ] 3.9.1 deque 替代（固定数组 + 计数器）
  - [ ] 3.9.2 bit 1 判断：sum of last two in tolerance OR htl + 2*l in tolerance
  - [ ] 3.9.3 bit 0 判断：l in tolerance
  - [ ] 3.9.4 IDLE 判断：l > 1.25 * bit_width
- [ ] 3.10 实现 `qi_add_bit()` 函数：
  - [ ] 3.10.1 IDLE 状态：检测前导码 [1,1,1,1,0]
  - [ ] 3.10.2 DATA 状态：累积 11 位 → process_byte
- [ ] 3.11 实现 `qi_process_byte()` 函数：
  - [ ] 3.11.1 start bit 检查
  - [ ] 3.11.2 data bits 提取（LSB first）
  - [ ] 3.11.3 parity 检查
  - [ ] 3.11.4 stop bit 检查
  - [ ] 3.11.5 加入 packet
- [ ] 3.12 实现 `qi_packet_len()` 函数
- [ ] 3.13 实现 `qi_process_packet()` 函数：
  - [ ] 3.13.1 Signal Strength (0x01)
  - [ ] 3.13.2 End Power Transfer (0x02) + end_codes 查表
  - [ ] 3.13.3 Control Error (0x03) — 有符号
  - [ ] 3.13.4 Received Power (0x04)
  - [ ] 3.13.5 Charge Status (0x05)
  - [ ] 3.13.6 Power Control Hold-off (0x06)
  - [ ] 3.13.7 Configuration (0x51)
  - [ ] 3.13.8 Identification (0x71)
  - [ ] 3.13.9 Extended Identification (0x81)
  - [ ] 3.13.10 Proprietary 和 Unknown
  - [ ] 3.13.11 Checksum 验证
- [ ] 3.14 实现 `qi_calc_checksum()` 函数
- [ ] 3.15 实现 `qi_destroy()` — 释放私有数据
- [ ] 3.16 定义 `struct srd_c_decoder qi_c_decoder` 结构体
- [ ] 3.17 实现 `srd_c_decoder_entry()` — 无 options

### 验证点
- [ ] `bit_width = samplerate / 2000` 在 metadata 中计算
- [ ] 前导码检测 [1,1,1,1,0] 正确
- [ ] `bits_to_uint` LSB first 转换正确
- [ ] `packet_len()` 四段分段函数正确
- [ ] checksum XOR 计算正确
- [ ] samplerate 守卫

---

## Task 4: 实现 `rc_encode_c` 解码器

**文件**: `libsigrokdecode/c_decoders/rc_encode_c.c`
**复杂度**: 中等
**预计代码量**: ~350 行

### 子任务

- [ ] 4.1 创建文件骨架：includes、channel 定义、ann_labels(8个)、annotation_rows(3个)、inputs/outputs/tags
- [ ] 4.2 定义注解枚举 `enum rc_encode_ann`（8 个注解）
- [ ] 4.3 定义私有数据结构 `struct rc_encode_priv`
- [ ] 4.4 实现 `rc_encode_reset()` — 分配并初始化私有数据
- [ ] 4.5 实现 `rc_encode_start()` — 注册输出，读取 remote option
- [ ] 4.6 实现 `rc_encode_decode()` 主循环：
  - [ ] 4.6.1 等待边沿 `{0: 'e'}`
  - [ ] 4.6.2 首次边沿初始化
  - [ ] 4.6.3 bit_count < 12：收集 4 个脉冲 → decode_bit
  - [ ] 4.6.4 bit_count >= 12：同步位处理
- [ ] 4.7 实现 `rc_decode_bit()` 函数：
  - [ ] 4.7.1 逻辑 0 判断（短长短长）
  - [ ] 4.7.2 逻辑 1 判断（长短长短）
  - [ ] 4.7.3 逻辑 F 判断（短长长短）
  - [ ] 4.7.4 Unknown 判断
- [ ] 4.8 实现 `rc_pinlabels()` 函数
- [ ] 4.9 实现 `rc_decode_model()` 函数（maplin_l95ar）
- [ ] 4.10 实现 `rc_encode_destroy()` — 释放私有数据
- [ ] 4.11 定义 `struct srd_c_decoder rc_encode_c_decoder` 结构体
- [ ] 4.12 实现 `srd_c_decoder_entry()` — 初始化 remote option

### 验证点
- [ ] decode_bit 浮点比较正确（lmin=2, lmax=5, eqmin=0.5, eqmax=1.5）
- [ ] pinlabels 正确处理 bit_count 1-12
- [ ] 同步位使用 `c_cond_skip(8 * samples)` 等待
- [ ] remote option 字符串枚举正确初始化
- [ ] decode_model 仅在 model == maplin_l95ar 时调用

---

## Task 5: 实现 `sdq_c` 解码器

**文件**: `libsigrokdecode/c_decoders/sdq_c.c`
**复杂度**: 简单
**预计代码量**: ~200 行

### 子任务

- [ ] 5.1 创建文件骨架：includes、channel 定义、ann_labels(3个)、annotation_rows(3个)、inputs/outputs/tags
- [ ] 5.2 定义注解枚举 `enum sdq_ann`（3 个：BIT, BYTE, BREAK）
- [ ] 5.3 定义私有数据结构 `struct sdq_priv`
- [ ] 5.4 实现 `sdq_reset()` — 分配并初始化私有数据
- [ ] 5.5 实现 `sdq_start()` — 注册输出
- [ ] 5.6 实现 `sdq_metadata()` — 保存 samplerate
- [ ] 5.7 实现 `sdq_decode()` 主循环：
  - [ ] 5.7.1 计算 bit_width, half_bit_width, break_threshold
  - [ ] 5.7.2 等待线路变高
  - [ ] 5.7.3 主循环：等待下降沿 → 等待上升沿 → 判断 delta
- [ ] 5.8 实现 `sdq_handle_bit()` 函数：bitpack + 注解输出
- [ ] 5.9 实现 `sdq_destroy()` — 释放私有数据
- [ ] 5.10 定义 `struct srd_c_decoder sdq_c_decoder` 结构体
- [ ] 5.11 实现 `srd_c_decoder_entry()` — 初始化 bitrate option

### 验证点
- [ ] `bit_width = samplerate / bitrate`（bitrate 默认 98425）
- [ ] bitpack LSB first 正确
- [ ] bit 注解时间范围：startsample 到 startsample + bit_width
- [ ] byte 注解时间范围：bytepos 到 startsample + bit_width
- [ ] break_threshold = bit_width * 1.2
- [ ] samplerate 守卫

---

## Task 6: 修改 CMakeLists.txt

**文件**: `CMakeLists.txt`
**行号**: 837

### 子任务
- [ ] 6.1 在 `C_DECODERS` 列表末尾添加 `mipi_dsi_c pxx1_c qi_c rc_encode_c sdq_c`

---

## Task 7: 编译验证

### 子任务
- [ ] 7.1 运行 `build_incremental.cmd` 执行增量编译
- [ ] 7.2 检查编译输出，确保 5 个新 DLL 成功生成
- [ ] 7.3 验证 DLL 文件位于 `build.dir/decoders/c_decoders/` 目录
- [ ] 7.4 检查无编译警告或错误
