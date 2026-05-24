# 验证清单 — Python → C 解码器移植 Batch 15

## 通用验证项（适用于所有 5 个解码器）

### 文件结构
- [ ] 文件命名正确：`{decoder_id}_c.c`（`-` 替换为 `_`）
- [ ] 文件位于 `libsigrokdecode/c_decoders/` 目录
- [ ] 包含正确的头文件：`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<glib.h>`, `"libsigrokdecode.h"`

### struct srd_c_decoder 验证
- [ ] `.id` 格式为 `"{python_id}_c"`
- [ ] `.name` 格式为 `"{PythonName}(C)"`
- [ ] `.longname` 为 Python longname + `" (C)"`
- [ ] `.desc` 为 Python desc + `" (C implementation)"`
- [ ] `.license` 与 Python 一致
- [ ] `.channels` 数组与 Python channels 一致（id, name, desc, order, type, idn）
- [ ] `.num_channels` 正确
- [ ] `.optional_channels` 为 NULL（如无可选通道）
- [ ] `.num_optional_channels` 正确
- [ ] `.options` 数组正确（如无选项则为 NULL + num=0）
- [ ] `.num_options` 正确
- [ ] 所有回调函数已实现：reset, start, decode, destroy
- [ ] metadata 回调已实现（如需要 samplerate）

### ann_labels 验证
- [ ] 第一列为空字符串 `""`
- [ ] 第二列为 row id
- [ ] 第三列为显示名称
- [ ] 注解数量与 Python annotations 一致
- [ ] `NUM_ANN` 宏定义正确

### annotation_rows 验证
- [ ] **所有** annotation class 都映射到某个 row（Python 中遗漏的也必须映射）
- [ ] 每行的 `ann_classes` 数组以正确的 class index 结尾
- [ ] `num_ann_classes` 与数组长度一致
- [ ] row id 和 desc 与 Python 一致
- [ ] `num_annotation_rows` 正确

### samplerate 守卫
- [ ] `metadata` 回调保存 samplerate
- [ ] `decode()` 入口检查 samplerate，为 0 时 return

### Condition Builder 使用
- [ ] 每次使用 `c_cond_new()` 后都有对应的 `c_cond_free()`
- [ ] `c_cond_wait()` 返回值检查 `ret != SRD_OK` 时 return
- [ ] OR 条件使用 `c_cond_or()` 正确分隔
- [ ] `c_cond_skip()` 参数类型为 `uint64_t`

### 输出 API 使用
- [ ] `C_ANN_PUT` 参数正确：di, ss, es, out_id, cls, ...text
- [ ] `C_ANN_PUT_VAL` 用于需要数值显示的注解
- [ ] `c_decoder_put_python()` 用于 Python 协议输出（如有 outputs）
- [ ] `c_decoder_put_binary()` 用于二进制输出（如有 binary）

### 内存管理
- [ ] `reset` 中使用 `g_malloc0()` 分配私有数据
- [ ] `destroy` 中使用 `g_free()` 释放私有数据
- [ ] `destroy` 中将 private 设为 NULL

### srd_c_decoder_entry() 验证
- [ ] 导出宏 `SRD_C_DECODER_EXPORT` 正确使用
- [ ] 返回 `&xxx_c_decoder` 指针
- [ ] Options 的 `idn` 在 entry 中设置
- [ ] Options 的 `def` 使用 `g_variant_new_*()` 初始化
- [ ] 枚举选项的 `values` 使用 `GSList` + `g_variant_new_*()` 初始化
- [ ] `srd_c_decoder_api_version()` 函数返回 `SRD_C_DECODER_API_VERSION`

---

## mipi_dsi_c 专项验证

### 元数据一致性
- [ ] 2 个 channels：D0N(type=8/SDATA), D0P(type=108/ADATA)
- [ ] 13 个 annotations
- [ ] 2 个 annotation_rows：LPData(0-3), LP(4-10+11+12)
- [ ] 1 个 output：`"mipi_dsi"`
- [ ] 无 options

### 状态机验证
- [ ] 6 个状态正确实现
- [ ] FIND_START：`c_cond_fall(cb,0); c_cond_high(cb,1)` — AND 条件
- [ ] FIND_MODE_S0：`c_cond_low(cb,0); c_cond_low(cb,1)` — AND 条件
- [ ] FIND_MODE_S1：OR 条件，两个分支
- [ ] FIND_MODE_S2：`c_cond_low(cb,0); c_cond_low(cb,1)` — AND 条件
- [ ] FIND_DATA_EDGE：OR 条件，保存引脚值
- [ ] FIND_DATA_VALID：OR 条件，根据 matched 判断

### 逻辑验证
- [ ] ESC/BTA 判断：`d0n == 1` → ESC Mode, 否则 → BTA
- [ ] 数据位 LSB first：`databyte >>= 1; if d0p: databyte |= 0x80`
- [ ] 8 位累积后输出 DI 注解，格式 `"0x%02X"`
- [ ] Stop 注解后回到 FIND_START
- [ ] Python 中 Stop(11) 和 Idle(12) **已加入** LP annotation_row

---

## pxx1_c 专项验证

### 元数据一致性
- [ ] 1 个 channel：data
- [ ] 20 个 annotations
- [ ] 3 个 annotation_rows：bytes(0), bits(1,2), desc(3-19)
- [ ] 1 个 binary class：raw
- [ ] 无 options（Python 中 options 为空元组）
- [ ] 无 outputs

### 状态机验证
- [ ] 19 个状态正确实现
- [ ] 主循环：下降沿 → 上升沿 → 下降沿，计算 period_t
- [ ] period_t 阈值：23-25us(1), 15-17us(0), >=40us(break+0)
- [ ] break 时修正 es_block：`ss_block + samplerate/1000000*16`

### Bit Stuffing 验证
- [ ] `bit_one_cnt` 在每次 addBit 时递增
- [ ] `bit_stuffing == True` 且 `bit_one_cnt >= 6` 时跳过（输出 "S"）
- [ ] `value == 0` 时重置 `bit_one_cnt = 0`
- [ ] Header(0x7E) 时重置 `bit_one_cnt = 0`
- [ ] `byte_cnt > 18` 时关闭 `bit_stuffing = False`

### Channels 解码验证
- [ ] nibble 累积排除 bit stuffing 位（`bstuff == 0` 检查）
- [ ] 每 4 bit 保存一个 nibble
- [ ] 96 bit = 24 nibbles
- [ ] 通道值计算：`ch1 = nibble[3]<<8 | nibble[1]<<4 | nibble[0]`
- [ ] 通道值计算：`ch2 = nibble[4]<<8 | nibble[5]<<4 | nibble[2]`
- [ ] ch1 > 2048 标记为 (9-16)

### transmit_type 数组
- [ ] 硬编码 `{"FCC", "EU", "EU+", "AU+"}`

---

## qi_c 专项验证

### 元数据一致性
- [ ] 1 个 channel：qi
- [ ] 8 个 annotations
- [ ] 3 个 annotation_rows：bits(0), bytes(1-4), packets(5-7)
- [ ] 无 options
- [ ] 无 outputs

### 差分编码验证
- [ ] `bit_width = samplerate / 2000` 在 metadata 中计算
- [ ] 容差范围：0.75 * bit_width 到 1.25 * bit_width
- [ ] deque 替代：固定数组 deq[2] + deq_len 计数器
- [ ] bit 1 判断条件1：`deq[-1] + deq[-2]` 在容差内
- [ ] bit 1 判断条件2：htl 且 `l * 2` 在容差内且 `deq[-2] > 1.25 * bit_width`
- [ ] bit 0 判断：`l` 在容差内
- [ ] IDLE 判断：`l > 1.25 * bit_width`

### 前导码验证
- [ ] IDLE 状态检测 [1,1,1,1,0] 序列
- [ ] 检测后转入 DATA 状态
- [ ] 设置 start bit = 0

### 字节处理验证
- [ ] 11 位：start(1) + data(8) + parity(1) + stop(1)
- [ ] start bit == 0 → Start bit 注解，否则 Start error 注解
- [ ] data bits LSB first 转换
- [ ] parity 计算：data bits 中 1 的个数 + parity bit 应为奇数
- [ ] stop bit == 1 → Stop bit 注解，否则 Stop error 注解

### 包处理验证
- [ ] `packet_len()` 四段分段函数正确
- [ ] 完整包 = packet_len(header) + 2 字节
- [ ] Signal Strength (0x01)：1 数据字节
- [ ] End Power Transfer (0x02)：end_codes 查表（9 个条目）
- [ ] Control Error (0x03)：有符号转换
- [ ] Configuration (0x51)：5 数据字节，解析各字段
- [ ] Identification (0x71)：7 数据字节
- [ ] Extended Identification (0x81)：8 数据字节
- [ ] Checksum：XOR 所有字节（不含最后一个），与最后一个比较
- [ ] Checksum OK → class 6，Checksum ERR → class 7

---

## rc_encode_c 专项验证

### 元数据一致性
- [ ] 1 个 channel：data
- [ ] 8 个 annotations
- [ ] 3 个 annotation_rows：bits(0-4), pins(5), code-words(6,7)
- [ ] 1 个 option：remote（字符串枚举：none, maplin_l95ar）
- [ ] 无 outputs

### decode_bit 验证
- [ ] 逻辑 0：edges[1] ≈ edges[0]*3, edges[2] ≈ edges[0], edges[3] ≈ edges[0]*3
- [ ] 逻辑 1：edges[0] ≈ edges[1]*3, edges[0] ≈ edges[2], edges[0] ≈ edges[3]*3
- [ ] 逻辑 F：edges[1] ≈ edges[0]*3, edges[2] ≈ edges[0]*3, edges[3] ≈ edges[0]
- [ ] 比较倍数：lmin=2, lmax=5, eqmin=0.5, eqmax=1.5

### 位收集验证
- [ ] 每个逻辑位由 4 个脉冲组成
- [ ] 12 个逻辑位（A0-A11）
- [ ] 第 13 个"位"为同步位

### pinlabels 验证
- [ ] bit_count 1-6：`A{bit_count-1}`（A0-A5）
- [ ] bit_count 7-12：`A{bit_count-1}/D{12-bit_count}`（A6/D5 - A11/D0）

### 同步位验证
- [ ] 使用 `c_cond_skip(8 * samples)` 等待
- [ ] 输出 Sync 注解（class 4）

### Option 初始化验证
- [ ] `remote` option：`g_variant_new_string("none")`
- [ ] values 列表：`"none"`, `"maplin_l95ar"`
- [ ] `c_decoder_get_option_string()` 读取

### maplin_l95ar 模型验证
- [ ] 地址解析：A0-A5，0=on，1/f=off
- [ ] 按钮解析：A6/D5-A11/D0 组合
- [ ] 输出 code-word-addr(6) 和 code-word-data(7) 注解

---

## sdq_c 专项验证

### 元数据一致性
- [ ] 1 个 channel：sdq
- [ ] 3 个 annotations：BIT, BYTE, BREAK
- [ ] 3 个 annotation_rows：bits(0), bytes(1), breaks(2)
- [ ] 1 个 option：bitrate（整数，默认 98425）
- [ ] 无 outputs

### 时间参数验证
- [ ] `bit_width = samplerate / bitrate`
- [ ] `half_bit_width = bit_width / 2`
- [ ] `break_threshold = bit_width * 1.2`

### 位解码验证
- [ ] 等待下降沿 → 等待上升沿 → 测量 delta
- [ ] `delta > break_threshold` → BREAK
- [ ] `delta > half_bit_width` → bit 0
- [ ] 否则 → bit 1

### bitpack 验证
- [ ] LSB first：`byte = bits[0] | (bits[1]<<1) | ... | (bits[7]<<7)`
- [ ] 8 位后输出 byte 注解

### 注解时间范围验证
- [ ] bit 注解：startsample 到 startsample + bit_width
- [ ] byte 注解：bytepos 到 startsample + bit_width
- [ ] break 注解：startsample 到 samplenum（上升沿）

### Option 初始化验证
- [ ] `bitrate` option：`g_variant_new_int64(98425)`
- [ ] `c_decoder_get_option_int()` 读取

---

## 编译验证

### CMakeLists.txt
- [ ] `C_DECODERS` 列表包含 `mipi_dsi_c pxx1_c qi_c rc_encode_c sdq_c`
- [ ] 列表位于现有最后一个解码器 `ir_sirc_c` 之后

### 编译输出
- [ ] `build_incremental.cmd` 执行成功
- [ ] 5 个 DLL 文件生成于 `build.dir/decoders/c_decoders/`
  - [ ] `decoder_mipi_dsi_c.dll`
  - [ ] `decoder_pxx1_c.dll`
  - [ ] `decoder_qi_c.dll`
  - [ ] `decoder_rc_encode_c.dll`
  - [ ] `decoder_sdq_c.dll`
- [ ] 无编译错误
- [ ] 无编译警告（或仅有可接受的警告）

### 运行时验证
- [ ] PXView 启动正常
- [ ] C 解码器列表中可见 5 个新解码器
- [ ] 解码器 ID 正确显示（`mipi_dsi_c`, `pxx1_c`, `qi_c`, `rc_encode_c`, `sdq_c`）
