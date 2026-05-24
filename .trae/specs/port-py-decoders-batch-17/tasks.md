# 移植任务分解 — Batch 17

## 任务总览

| 任务编号 | 解码器 | 优先级 | 预估复杂度 |
|----------|--------|--------|-----------|
| T1 | tlc5620_c | 高 | 中等 |
| T2 | xy2-100_c | 高 | 中高 |

---

## T1: tlc5620_c — TI TLC5620 8-bit Quad DAC

### T1.1 创建文件骨架

**文件**: `libsigrokdecode/c_decoders/tlc5620_c.c`

1. 添加标准头文件引用：`libsigrokdecode.h`, `<glib.h>`, `<stdio.h>`, `<stdlib.h>`, `<string.h>`
2. 定义 `enum tlc5620_ann`，包含 10 个注解类型（ANN_DAC_SELECT 到 ANN_INVALID_CMD），末尾 NUM_ANN
3. 定义 `tlc5620_state` 结构体（见 spec.md §1.6.1）
4. 定义通道数组 `tlc5620_channels[2]` 和 `tlc5620_optional_channels[2]`
5. 定义选项数组 `tlc5620_options[4]`（声明为 static struct，在 entry() 中初始化）
6. 定义 `tlc5620_ann_labels[10][3]`（第一列全为 `""`）
7. 定义 annotation_rows 的 class 数组和 `tlc5620_ann_rows[6]`
8. 定义 inputs/outputs/tags 字符串数组
9. 声明所有回调函数：reset/start/decode/destroy/metadata

### T1.2 实现 reset 回调

```c
static void tlc5620_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(tlc5620_state)));
    }
    tlc5620_state *s = (tlc5620_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(tlc5620_state));
    s->ss_dac_first = (uint64_t)-1;
    s->dacval[0] = s->dacval[1] = s->dacval[2] = s->dacval[3] = -1;
    s->gains[0] = s->gains[1] = s->gains[2] = s->gains[3] = -1;
    s->out_ann = -1;
}
```

### T1.3 实现 start 回调

1. 注册输出：`s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "tlc5620")`
2. 读取选项：4 个 vref 值（`c_decoder_get_option_double`）
3. 检查可选通道：`s->have_load = c_decoder_has_channel(di, 2)`, `s->have_ldac = c_decoder_has_channel(di, 3)`
4. 获取 samplerate：`s->samplerate = c_decoder_get_samplerate(di)`

### T1.4 实现 metadata 回调

```c
static void tlc5620_metadata(struct srd_decoder_inst *di, int key, uint64_t value)
{
    tlc5620_state *s = (tlc5620_state *)c_decoder_get_private(di);
    if (key == SRD_CONF_SAMPLERATE) {
        s->samplerate = value;
    }
}
```

### T1.5 实现 handle_11bits 辅助函数

**核心逻辑**（见 spec.md §1.6.4）：
1. 截断超过 11 位的 bits 数组
2. 不足 11 位输出 ANN_INVALID_CMD
3. 解析 DAC 选择（2 bits → 0-3）
4. 解析增益（1 bit → 1 or 2）
5. 解析 DAC 值（8 bits MSB first → 0-255）
6. 输出各字段注解
7. 输出每个 bit 的注解
8. 返回 1（成功）或 0（失败）

**注意事项**：
- `clock_width` 用于估算最后一个 bit 的结束位置
- `ss_dac_first` 在第一次成功解析时设置

### T1.6 实现 handle_load_fall 辅助函数

1. 调用 handle_11bits，失败则返回
2. 输出 ANN_DATA_LATCH 注解
3. 计算电压：`V = Vref * (value / 256.0) * gain`
4. 根据 LDAC 状态选择注解类型：
   - LDAC == 0 → ANN_VOLTAGE_UPDATE（"Setting {DAC} voltage to {V}"）
   - LDAC == 1 → ANN_REG_WRITE（"Setting {DAC} register value to {V}"）
5. 保存 dacval 和 gains

### T1.7 实现 handle_ldac_fall 辅助函数

1. 输出 ANN_LDAC_FALL 注解
2. 如果 ss_dac_first 未设置，直接返回
3. 遍历 4 个 DAC，计算电压（未知的显示 "?"）
4. 输出 ANN_VOLTAGE_UPDATE_ALL 注解
5. 重置 ss_dac_first

### T1.8 实现 decode 主循环

1. Samplerate 守卫（尝试获取，不阻塞）
2. 主循环：
   - 构建 condition builder：CLK 下降沿 + (可选) LOAD 下降沿 + (可选) LDAC 下降沿
   - `c_cond_wait` 等待
   - 读取各通道当前电平
   - 根据 matched 位掩码分发处理：
     - CLK 下降沿匹配 → 采样 DATA 引脚，追加到 bits 数组
     - LOAD 下降沿匹配 → 调用 handle_load_fall
     - LDAC 下降沿匹配 → 调用 handle_ldac_fall
   - **注意**：多个条件可能同时匹配，需要全部处理

**条件索引计算**：
```
cond_idx_clk = 0
cond_idx_load = have_load ? 1 : -1
cond_idx_ldac = have_ldac ? (have_load ? 2 : 1) : -1
```

**CLK 下降沿时采样 DATA**：
```c
int data = c_decoder_get_pin(di, 1, samplenum);
if (s->bits_count < TLC5620_MAX_BITS) {
    s->bits_value[s->bits_count] = data;
    s->bits_ss[s->bits_count] = samplenum;
    s->bits_es[s->bits_count] = samplenum;
}
s->bits_count++;
```

**LOAD 下降沿时读取 LDAC 状态**：
```c
if (s->have_ldac) {
    s->ldac = c_decoder_get_pin(di, 3, samplenum);
}
```

### T1.9 实现 destroy 回调

```c
static void tlc5620_destroy(struct srd_decoder_inst *di)
{
    void *priv = c_decoder_get_private(di);
    if (priv) {
        g_free(priv);
        c_decoder_set_private(di, NULL);
    }
}
```

### T1.10 定义 srd_c_decoder 结构体

```c
struct srd_c_decoder tlc5620_c_decoder = {
    .id = "tlc5620_c",
    .name = "TLC5620(C)",
    .longname = "Texas Instruments TLC5620 (C)",
    .desc = "Texas Instruments TLC5620 8-bit quad DAC. (C implementation)",
    .license = "gplv2+",
    .channels = tlc5620_channels,
    .num_channels = 2,
    .optional_channels = tlc5620_optional_channels,
    .num_optional_channels = 2,
    .options = tlc5620_options,
    .num_options = 4,
    .num_annotations = NUM_ANN,
    .ann_labels = tlc5620_ann_labels,
    .num_annotation_rows = 6,
    .annotation_rows = tlc5620_ann_rows,
    .inputs = tlc5620_inputs,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = tlc5620_tags,
    .num_tags = 2,
    .reset = tlc5620_reset,
    .start = tlc5620_start,
    .decode = tlc5620_decode,
    .destroy = tlc5620_destroy,
    .metadata = tlc5620_metadata,
};
```

### T1.11 实现 srd_c_decoder_entry()

初始化 4 个 vref 选项（double 类型，默认 3.3V）：
```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    const char *vref_ids[] = {"vref_a", "vref_b", "vref_c", "vref_d"};
    const char *vref_idns[] = {
        "dec_tlc5620_opt_vref_a", "dec_tlc5620_opt_vref_b",
        "dec_tlc5620_opt_vref_c", "dec_tlc5620_opt_vref_d"
    };
    const char *vref_descs[] = {
        "Reference voltage DACA (V)", "Reference voltage DACB (V)",
        "Reference voltage DACC (V)", "Reference voltage DACD (V)"
    };
    for (int i = 0; i < 4; i++) {
        tlc5620_options[i].id = vref_ids[i];
        tlc5620_options[i].idn = vref_idns[i];
        tlc5620_options[i].desc = vref_descs[i];
        tlc5620_options[i].def = g_variant_new_double(3.3);
        tlc5620_options[i].values = NULL;
    }
    return &tlc5620_c_decoder;
}
```

### T1.12 修改 CMakeLists.txt

在 `C_DECODERS` 列表中添加 `tlc5620_c`。

---

## T2: xy2-100_c — XY2-100 振镜定位协议

### T2.1 创建文件骨架

**文件**: `libsigrokdecode/c_decoders/xy2-100_c.c`

1. 添加标准头文件引用
2. 定义 `enum xy2100_ann`，包含 9 个注解类型（ANN_BIT 到 ANN_WARNING），末尾 NUM_ANN
3. 定义 `enum xy2100_frame_type`（NONE/COMMAND/16BIT_POS/18BIT_POS）
4. 定义 `xy2100_state` 结构体（见 spec.md §2.6.1）
5. 定义通道数组 `xy2100_channels[3]` 和 `xy2100_optional_channels[1]`
6. 无选项（`.num_options = 0, .options = NULL`）
7. 定义 `xy2100_ann_labels[9][3]`
8. 定义 annotation_rows 的 class 数组和 `xy2100_ann_rows[6]`
9. 定义 inputs/outputs/tags 字符串数组
10. 声明所有回调函数：reset/start/decode/destroy/metadata

### T2.2 实现 reset 回调

```c
static void xy2100_reset(struct srd_decoder_inst *di)
{
    if (!c_decoder_get_private(di)) {
        c_decoder_set_private(di, g_malloc0(sizeof(xy2100_state)));
    }
    xy2100_state *s = (xy2100_state *)c_decoder_get_private(di);
    memset(s, 0, sizeof(xy2100_state));
    s->stat_skip_bit = 1;
    s->out_ann = -1;
}
```

**注意**：需要额外的 `xy2100_reset_state()` 函数，仅重置 bits/stat_bits 采集状态（不重置 samplerate/out_ann 等持久状态），用于帧处理完成后：

```c
static void xy2100_reset_state(xy2100_state *s)
{
    s->bits_count = 0;
    s->stat_bits_count = 0;
    s->stat_skip_bit = 1;
}
```

### T2.3 实现 start 回调

1. 注册输出：`s->out_ann = c_decoder_register_output(di, SRD_OUTPUT_ANN, "xy2-100")`
2. 检查可选通道：`s->has_stat = c_decoder_has_channel(di, 3)`
3. 获取 samplerate

### T2.4 实现 metadata 回调

同 tlc5620，保存 SRD_CONF_SAMPLERATE。

### T2.5 实现 process_bit 辅助函数

**核心逻辑**（见 spec.md §2.6.4）：

1. 输出 bit 注解
2. 追加到 bits 数组
3. 当 sync == 0 时：
   a. 检查 bits 数量（< 20 报错）
   b. 计算奇偶校验
   c. 判断帧类型（18-bit/16-bit/命令/未知）
   d. 输出帧类型注解
   e. 输出校验注解
   f. 解析位置值或命令/参数
   g. 输出相应注解
   h. 调用 xy2100_reset_state()

**帧类型判断逻辑**（严格按 Python 顺序）：
```
if (type_1_value == 1 && parity_odd == 1) → 18-bit 位置帧
else if (type_3_value == 1) → 16-bit 位置帧
else if (type_3_value == 7 && parity_even == 1) → 命令帧
else → 错误
```

**注意**：18-bit 位置帧的 type_es 是 bits[0] 的 es，而非 bits[2] 的 es。

### T2.6 实现 process_stat_bit 辅助函数

1. 跳过第一个 stat bit
2. 输出 stat bit 注解
3. 追加到 stat_bits 数组
4. 当 sync == 0 且 stat_bits == 19 时，计算并输出状态值

### T2.7 实现 decode 主循环

1. Samplerate 守卫
2. 初始化局部变量：`bit_ss = -1`, `bit_value = 0`, `stat_ss = -1`, `stat_value = 0`, `sync_value = 0`
3. 主循环：
   - 等待 CLK 任意边沿（`c_cond_edge(cb, 0)`）
   - 读取所有通道电平
   - CLK == 1（上升沿）：保存 stat_value，处理 data bit，更新 bit_ss
   - CLK == 0（下降沿）：采样 DATA/SYNC，处理 stat bit，更新 stat_ss

**关键**：DATA 在下降沿采样但 bit 区间在上升沿结束。SYNC 也在下降沿采样。STAT 在上升沿采样但 stat bit 区间在下降沿结束。

### T2.8 实现 destroy 回调

同 tlc5620。

### T2.9 定义 srd_c_decoder 结构体

```c
struct srd_c_decoder xy2100_c_decoder = {
    .id = "xy2-100_c",
    .name = "XY2-100(C)",
    .longname = "XY2-100(E) and XY-200(E) galvanometer protocol (C)",
    .desc = "Serial protocol for galvanometer positioning in laser systems (C implementation)",
    .license = "gplv2+",
    .channels = xy2100_channels,
    .num_channels = 3,
    .optional_channels = xy2100_optional_channels,
    .num_optional_channels = 1,
    .options = NULL,
    .num_options = 0,
    .num_annotations = NUM_ANN,
    .ann_labels = xy2100_ann_labels,
    .num_annotation_rows = 6,
    .annotation_rows = xy2100_ann_rows,
    .inputs = xy2100_inputs,
    .num_inputs = 1,
    .outputs = NULL,
    .num_outputs = 0,
    .binary = NULL,
    .num_binary = 0,
    .tags = xy2100_tags,
    .num_tags = 1,
    .reset = xy2100_reset,
    .start = xy2100_start,
    .decode = xy2100_decode,
    .destroy = xy2100_destroy,
    .metadata = xy2100_metadata,
};
```

### T2.10 实现 srd_c_decoder_entry()

```c
SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)
{
    return &xy2100_c_decoder;
}
```

无选项需要初始化。

### T2.11 修改 CMakeLists.txt

在 `C_DECODERS` 列表中添加 `xy2-100_c`。

---

## 执行顺序

1. **T1.1-T1.11**：完成 tlc5620_c.c 全部代码
2. **T2.1-T2.10**：完成 xy2-100_c.c 全部代码
3. **T1.12 + T2.11**：修改 CMakeLists.txt
4. **编译验证**：运行 `build_incremental.cmd`
5. **功能验证**：在 PXView 中加载解码器测试

---

## 风险与注意事项

### tlc5620_c
- **多条件同时匹配**：Python 的 `self.matched` 可以同时有多个位被设置，C 的 `c_cond_wait` 也支持。必须在同一次循环迭代中处理所有匹配条件
- **LDAC 状态读取时机**：LOAD 下降沿时需要知道 LDAC 的当前电平。应在 LOAD 匹配时通过 `c_decoder_get_pin` 读取
- **bits 数组截断**：超过 11 位时需要移位操作，确保数组索引正确
- **ss_dac_first 初始化**：使用 `(uint64_t)-1` 表示未设置

### xy2-100_c
- **CLK 上升/下降沿双重处理**：与大多数解码器不同，XY2-100 在 CLK 的两个边沿都有操作
- **DATA 采样时机**：DATA 在 CLK 下降沿采样，但 bit 区间在 CLK 上升沿结束
- **SYNC 采样时机**：SYNC 在 CLK 下降沿采样，用于 process_bit 判断帧结束
- **STAT 采样时机**：STAT 在 CLK 上升沿采样，stat bit 区间在 CLK 下降沿结束
- **stat_skip_bit**：第一个 stat bit 被跳过，但只在 reset 时重置
- **18-bit vs 16-bit 帧区分**：依赖奇偶校验，存在歧义（代码中有警告）
- **有符号数转换**：16-bit 使用 `pos >= 32768 ? pos - 65536`，18-bit 使用 `pos >= 131072 ? pos - 262144`
