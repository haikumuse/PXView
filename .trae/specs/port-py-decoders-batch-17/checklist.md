# 移植验证清单 — Batch 17

## 通用检查项（适用于所有解码器）

### 文件结构
- [ ] 文件名格式正确：`{decoder_id}_c.c`
- [ ] 文件位于 `libsigrokdecode/c_decoders/` 目录
- [ ] 包含标准头文件：`libsigrokdecode.h`, `<glib.h>`, `<stdio.h>`, `<stdlib.h>`, `<string.h>`
- [ ] 文件末尾有 `SRD_C_DECODER_EXPORT` 的两个导出函数

### srd_c_decoder 结构体
- [ ] `.id` 格式为 `"{python_id}_c"`（如 `"tlc5620_c"`, `"xy2-100_c"`）
- [ ] `.name` 格式为 `"{NAME}(C)"`（如 `"TLC5620(C)"`, `"XY2-100(C)"`）
- [ ] `.longname` 包含 `(C)` 后缀
- [ ] `.desc` 包含 `(C implementation)` 后缀
- [ ] `.license` 与 Python 版本一致（`"gplv2+"`）
- [ ] `.channels` 数组与 Python 一致（id, name, desc, order, type, idn）
- [ ] `.optional_channels` 数组与 Python 一致
- [ ] `.num_channels` 和 `.num_optional_channels` 正确
- [ ] `.options` 数组正确声明（或 NULL）
- [ ] `.num_options` 正确
- [ ] `.num_annotations` 等于 `NUM_ANN`
- [ ] `.ann_labels` 第一列全为 `""`
- [ ] `.ann_labels` 第二列为短标签，第三列为长描述
- [ ] `.annotation_rows` 的 class 数组以 `-1` 结尾
- [ ] `.annotation_rows` 的 `num_ann_classes` 正确计数
- [ ] `.num_annotation_rows` 正确
- [ ] `.inputs` 包含 `"logic"`
- [ ] `.num_inputs = 1`
- [ ] `.outputs` 为 NULL 或正确数组
- [ ] `.num_outputs` 正确
- [ ] `.binary` 和 `.num_binary` 正确
- [ ] `.tags` 与 Python 一致
- [ ] `.num_tags` 正确
- [ ] 所有回调函数指针正确赋值

### 回调函数
- [ ] `reset` — 分配私有数据（`g_malloc0`），`memset` 清零，初始化特殊值
- [ ] `start` — 注册输出（`c_decoder_register_output`），读取选项，检查通道
- [ ] `decode` — 主解码循环，使用 condition builder
- [ ] `destroy` — `g_free` 释放私有数据，`c_decoder_set_private(di, NULL)`
- [ ] `metadata` — 处理 `SRD_CONF_SAMPLERATE`

### Condition Builder 使用
- [ ] 每次循环创建 `c_cond_new()`
- [ ] 正确使用 `c_cond_rise/fall/edge/high/low/skip/or`
- [ ] `c_cond_wait` 返回值检查（`!= SRD_OK` 则 return）
- [ ] `c_cond_free()` 在每次 `c_cond_wait` 后调用
- [ ] `matched` 位掩码正确解析

### Samplerate 守卫
- [ ] `metadata` 回调中保存 samplerate
- [ ] `decode` 入口处尝试获取 samplerate（`c_decoder_get_samplerate`）
- [ ] 如果 samplerate 为 0 且解码器不依赖它，允许继续
- [ ] 如果 samplerate 为 0 且解码器依赖它（如时间计算），提前 return

### 选项初始化
- [ ] 在 `srd_c_decoder_entry()` 中初始化所有选项
- [ ] 选项 `id`, `idn`, `desc` 与 Python 一致
- [ ] 选项 `def` 使用正确的 `g_variant_new_*` 类型
- [ ] 选项 `values` 列表正确（如有枚举值）

### CMakeLists.txt
- [ ] 在 `C_DECODERS` 列表中添加了解码器名称
- [ ] 名称与文件名匹配（不含 `_c.c` 后缀）

---

## TLC5620 专用检查项

### 元数据一致性
- [ ] 2 个必需通道：CLK (order=0, SRD_CHANNEL_SCLK), DATA (order=1, SRD_CHANNEL_SDATA)
- [ ] 2 个可选通道：LOAD (order=2, SRD_CHANNEL_COMMON), LDAC (order=3, SRD_CHANNEL_COMMON)
- [ ] 4 个选项：vref_a/b/c/d（double 类型，默认 3.3）
- [ ] 10 个注解类型（ANN_DAC_SELECT 到 ANN_INVALID_CMD）
- [ ] 6 个注解行（bits, fields, registers, voltage-updates, events, errors）
- [ ] 2 个标签（"IC", "Analog/digital"）

### 解码逻辑
- [ ] 等待条件：CLK 下降沿 OR LOAD 下降沿 OR LDAC 下降沿
- [ ] 可选通道 LOAD/LDAC 不存在时，不添加对应等待条件
- [ ] CLK 下降沿时：采样 DATA 引脚，追加到 bits 数组
- [ ] LOAD 下降沿时：调用 handle_11bits，处理锁存逻辑
- [ ] LDAC 下降沿时：调用 handle_ldac_fall，更新所有 DAC 电压
- [ ] 多个条件同时匹配时全部处理

### handle_11bits
- [ ] bits > 11 时截断为最后 11 位
- [ ] bits < 11 时输出 ANN_INVALID_CMD "Command too short"
- [ ] DAC 选择：bit[0:2] → 00=A, 01=B, 10=C, 11=D
- [ ] 增益：bit[2] → 0=x1, 1=x2
- [ ] DAC 值：bit[3:11] → 8-bit MSB first
- [ ] 输出 ANN_DAC_SELECT 注解（5 个短格式变体）
- [ ] 输出 ANN_GAIN 注解（3 个短格式变体）
- [ ] 输出 ANN_VALUE 注解（5 个短格式变体）
- [ ] 输出每个 bit 的 ANN_BIT 注解
- [ ] clock_width 正确计算（es_gain - ss_gain）
- [ ] 最后一个 bit 的 es 使用 clock_width 估算

### handle_load_fall
- [ ] 调用 handle_11bits，失败则返回
- [ ] 输出 ANN_DATA_LATCH 注解
- [ ] 电压计算：`V = Vref * (value / 256.0) * gain`
- [ ] LDAC == 0 → ANN_VOLTAGE_UPDATE
- [ ] LDAC == 1 → ANN_REG_WRITE
- [ ] 保存 dacval[dac_select] 和 gains[dac_select]

### handle_ldac_fall
- [ ] 输出 ANN_LDAC_FALL 注解
- [ ] ss_dac_first 未设置时直接返回
- [ ] 遍历 4 个 DAC 计算电压
- [ ] 未知 DAC 值显示 "?"
- [ ] 输出 ANN_VOLTAGE_UPDATE_ALL 注解（3 个短格式变体）
- [ ] 重置 ss_dac_first

### 边界情况
- [ ] 无 LOAD 通道时：bits 永远不会被锁存（仅 CLK 采样）
- [ ] 无 LDAC 通道时：不处理 LDAC 下降沿
- [ ] bits 数组最大容量 TLC5620_MAX_BITS ≥ 16（允许超过 11 位后截断）
- [ ] ss_dac_first 初始化为 `(uint64_t)-1`

---

## XY2-100 专用检查项

### 元数据一致性
- [ ] 3 个必需通道：CLK (order=0, SRD_CHANNEL_SCLK), SYNC (order=1, SRD_CHANNEL_COMMON), DATA (order=2, SRD_CHANNEL_SDATA)
- [ ] 1 个可选通道：STAT (order=3, SRD_CHANNEL_SDATA)
- [ ] 无选项
- [ ] 9 个注解类型（ANN_BIT 到 ANN_WARNING）
- [ ] 6 个注解行（bits, stat_bits, data, positions, statuses, warnings）
- [ ] 1 个标签（"Embedded/industrial"）

### 解码逻辑
- [ ] 等待条件：CLK 任意边沿（`c_cond_edge`）
- [ ] CLK == 1（上升沿）：结束 data bit 区间，处理上一个 data bit
- [ ] CLK == 0（下降沿）：采样 DATA 和 SYNC，处理 stat bit
- [ ] DATA 在下降沿采样，bit 区间在上升沿结束
- [ ] SYNC 在下降沿采样
- [ ] STAT 在上升沿采样，stat bit 区间在下降沿结束

### process_bit
- [ ] 输出 ANN_BIT 注解
- [ ] 追加到 bits 数组（最多 20 位）
- [ ] sync == 0 时处理帧
- [ ] bits < 20 时输出 ANN_WARNING "Not enough data bits"
- [ ] 奇偶校验计算正确（XOR bits[0:19]，不含 parity bit）
- [ ] 帧类型判断顺序正确：18-bit → 16-bit → 命令 → 错误
- [ ] 18-bit 位置帧：type_1_value == 1 && parity_odd
- [ ] 16-bit 位置帧：type_3_value == 1（不含奇偶校验检查，仅偶校验标记）
- [ ] 命令帧：type_3_value == 7 && parity_even
- [ ] 18-bit 帧的 type_es 使用 bits[0] 的 es（特殊处理）
- [ ] 16-bit 和命令帧的 type_es 使用 bits[2] 的 es
- [ ] 16-bit 位置值：bits[3:19]，有符号，范围 [-32768, 32767]
- [ ] 18-bit 位置值：bits[3:19]（注意 Python 代码实际用 bits[3:19]），有符号，范围 [-131072, 131071]
- [ ] 命令值：bits[3:11]，8-bit MSB first
- [ ] 参数值：bits[11:19]，8-bit MSB first
- [ ] 校验注解输出（"OK" / "NOK" / "X"）
- [ ] 帧类型注解输出（3 个短格式变体）
- [ ] 位置值注解输出
- [ ] 命令注解输出（3 个短格式变体）
- [ ] 参数注解输出（3 个短格式变体）
- [ ] 歧义警告输出（18-bit 和命令帧的奇偶校验歧义）
- [ ] 处理完成后调用 xy2100_reset_state()

### process_stat_bit
- [ ] 第一个 stat bit 被跳过（stat_skip_bit）
- [ ] 输出 ANN_STAT_BIT 注解
- [ ] 追加到 stat_bits 数组（最多 19 位）
- [ ] sync == 0 且 stat_bits == 19 时输出 ANN_STATUS
- [ ] 状态值计算：19-bit MSB first
- [ ] 输出格式："Status 0x%X" / "0x%X"

### 边界情况
- [ ] bit_ss 初始值为 `(uint64_t)-1`，第一次上升沿不处理
- [ ] stat_ss 初始值为 `(uint64_t)-1`，第一次下降沿不处理
- [ ] 无 STAT 通道时跳过所有 stat 处理
- [ ] stat_skip_bit 在 reset 时重置为 1
- [ ] stat_skip_bit 在 xy2100_reset_state() 中也重置为 1
- [ ] 18-bit 位置帧和命令帧的歧义警告正确输出
- [ ] 16-bit 位置帧奇偶校验错误时输出 "Parity error" / "PE"
- [ ] 未知帧类型时输出 "Error" / "Unknown command or parity error"

---

## 编译验证

- [ ] `build_incremental.cmd` 编译成功，无错误
- [ ] 无编译警告（或仅有可接受的警告）
- [ ] DLL 文件生成在 `build.dir/decoders/c_decoders/` 目录
- [ ] `tlc5620_c.dll`（或 `.so`）存在
- [ ] `xy2-100_c.dll`（或 `.so`）存在

## 功能验证（如有测试数据）

- [ ] PXView 中可选择 tlc5620_c 解码器
- [ ] PXView 中可选择 xy2-100_c 解码器
- [ ] 通道分配界面正确显示所有通道
- [ ] 选项界面正确显示所有选项（tlc5620: 4 个 vref）
- [ ] 解码输出注解与 Python 版本一致
