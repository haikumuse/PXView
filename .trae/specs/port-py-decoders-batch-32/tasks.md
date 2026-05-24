# 任务分解：jtag_avr / jtag_ejtag / jtag_stm32 Python→C 解码器移植

## 任务总览

| 任务 ID | 解码器 | 复杂度 | 预估代码行数 | 优先级 |
|---------|--------|--------|-------------|--------|
| T1 | jtag_stm32_c | ⭐ 低 | ~300行 | P0 |
| T2 | jtag_ejtag_c | ⭐⭐⭐ 高 | ~600行 | P1 |
| T3 | jtag_avr_c | ⭐⭐⭐⭐ 极高 | ~700行 | P2 |
| T4 | CMakeLists.txt 更新 | ⭐ 低 | 3行 | P0 |
| T5 | 编译验证 | ⭐ 低 | - | P0 |

**建议执行顺序**：T1 → T4 → T5(验证T1) → T2 → T5(验证T2) → T3 → T5(验证T3)

---

## T1: jtag_stm32_c（最简单，先做）

### T1.1 创建文件 `libsigrokdecode/c_decoders/jtag_stm32_c.c`

### T1.2 实现步骤

| 步骤 | 内容 | 详细说明 |
|------|------|---------|
| T1.2.1 | 头文件和 enum 定义 | 包含 `stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`；定义 `enum stm32_ann`（4个：ANN_ITEM=0, ANN_FIELD, ANN_COMMAND, ANN_WARNING, NUM_ANN）；定义 `enum stm32_state`（7个状态） |
| T1.2.2 | 私有数据结构 | `struct stm32_priv { int state; uint64_t ss; uint64_t es; int out_ann; }` |
| T1.2.3 | 静态数据表 | `stm32_ir_map[]`（5项：BYPASS/IDCODE/DPACC/APACC/ABORT）；`cm3_idcode_ver` 映射；`cm3_idcode_part` 映射；`jedec_id` 映射；`jtag_idcode` 映射；`ack_val` 映射；`dp_reg` 映射 |
| T1.2.4 | ann_labels 定义 | 4项 `{"", "item", "Item"}` 等 |
| T1.2.5 | annotation_rows 定义 | 4行：items, fields, commands, warnings |
| T1.2.6 | channels/options | `channels=NULL, num_channels=0, options=NULL, num_options=0` |
| T1.2.7 | inputs/outputs/tags | `inputs={"jtag", NULL}, outputs=NULL, tags={"Debug/trace", NULL}` |
| T1.2.8 | reset 函数 | `g_malloc0` 分配 priv，`memset` 清零，`state=STM32_STATE_IDLE` |
| T1.2.9 | start 函数 | `c_decoder_register_output(di, SRD_OUTPUT_ANN, "jtag_stm32")` |
| T1.2.10 | decode 函数 | 空函数 `(void)di;` |
| T1.2.11 | destroy 函数 | `g_free(priv); c_decoder_set_private(di, NULL);` |
| T1.2.12 | recv_proto 函数 | 核心逻辑，见下方详细分解 |
| T1.2.13 | srd_c_decoder 结构体 | 填充所有字段，`.recv_proto = jtag_stm32_recv_proto` |
| T1.2.14 | srd_c_decoder_entry | `return &jtag_stm32_c_decoder;` |
| T1.2.15 | srd_c_decoder_api_version | `return SRD_C_DECODER_API_VERSION;` |

### T1.2.12 recv_proto 详细分解

```
recv_proto(di, ss, es, cmd, data, data_len):
  priv->ss = ss, priv->es = es

  if cmd == "IR TDI":
    // 提取 9-bit IR
    m3_ir = data[0] & 0x0F
    bs_ir = (data[0] >> 4) | ((data_len > 1 ? data[1] : 0) << 4)
    // 映射 M3 TAP IR
    switch m3_ir:
      0xF → state = BYPASS
      0xE → state = IDCODE
      0xA → state = DPACC
      0xB → state = APACC
      0x8 → state = ABORT
      default → state = UNKNOWN
    // 输出 IR 标注
    C_ANN_PUT(ANN_FIELD, "IR (BS TAP): ...")
    C_ANN_PUT(ANN_FIELD, "IR (M3 TAP): ...")
    C_ANN_PUT(ANN_COMMAND, "IR: <state>")

  elif cmd == "DR TDI":
    switch state:
      BYPASS → handle_reg_bypass(cmd, data, data_len); state = IDLE
      DPACC → handle_reg_dpacc(cmd, data, data_len, 0)
      APACC → handle_reg_apacc(cmd, data, data_len, 1)

  elif cmd == "DR TDO":
    switch state:
      IDCODE → handle_reg_idcode(data, data_len); state = IDLE
      ABORT → handle_reg_abort(data, data_len); state = IDLE
      UNKNOWN → handle_reg_unknown(data, data_len); state = IDLE
      DPACC → handle_reg_dpacc(cmd, data, data_len, 0); state = IDLE
      APACC → handle_reg_apacc(cmd, data, data_len, 1); state = IDLE

  elif cmd == "NEW STATE":
    // 不需要处理
```

### T1.3 handle 函数清单

| 函数 | 功能 | 复杂度 |
|------|------|--------|
| `handle_reg_bypass()` | 输出 BYPASS 数据 | 低 |
| `handle_reg_idcode()` | 解码 33-bit IDCODE，输出 Manufacturer/Part/Version | 中 |
| `handle_reg_dpacc()` | 解码 35-bit DPACC（TDI/TDO 双向） | 中 |
| `handle_reg_apacc()` | 解码 35-bit APACC（TDI/TDO 双向） | 中 |
| `handle_reg_abort()` | 解码 ABORT 寄存器，检查保留位 | 低 |
| `handle_reg_unknown()` | 输出未知指令警告 | 低 |
| `bytes_to_uint64()` | LSB-first 字节数组转整数 | 低（通用辅助） |

---

## T2: jtag_ejtag_c（中等复杂度）

### T2.1 创建文件 `libsigrokdecode/c_decoders/jtag_ejtag_c.c`

### T2.2 实现步骤

| 步骤 | 内容 | 详细说明 |
|------|------|---------|
| T2.2.1 | 头文件和 enum 定义 | 定义 `enum ejtag_ann`（13个annotation）；定义 `enum ejtag_state`（9个状态） |
| T2.2.2 | 私有数据结构 | `struct ejtag_priv` 包含：state, ss, es, out_ann, last_data_in(32-bit), last_data_out(32-bit), last_data_in_ss/es, last_data_out_ss/es, pracc_state, has_last_data 标志 |
| T2.2.3 | 静态数据表 | `ejtag_insn[]`（19项指令映射）；`ejtag_state_map[]`（6项 IR→State 映射）；`ejtag_ctrl_fields[]`（13项 Control Register 字段定义） |
| T2.2.4 | ann_labels 定义 | 13项 |
| T2.2.5 | annotation_rows 定义 | 5行 |
| T2.2.6 | channels/options | 无通道，无选项 |
| T2.2.7 | inputs/outputs/tags | `inputs={"jtag", NULL}, tags={"Debug/trace", NULL}` |
| T2.2.8 | reset 函数 | 分配 priv，初始化 state=RESET，清零 pracc_state |
| T2.2.9 | start 函数 | 注册 SRD_OUTPUT_ANN |
| T2.2.10 | decode 函数 | 空函数 |
| T2.2.11 | destroy 函数 | 释放 priv |
| T2.2.12 | recv_proto 函数 | 核心逻辑，见下方详细分解 |
| T2.2.13 | srd_c_decoder 结构体 | `.recv_proto = jtag_ejtag_recv_proto` |
| T2.2.14 | entry/api_version 函数 | 标准模板 |

### T2.2.12 recv_proto 详细分解

```
recv_proto(di, ss, es, cmd, data, data_len):
  priv->ss = ss, priv->es = es

  if cmd == "IR TDI":
    code = bytes_to_uint64(data, data_len) & 0xFF  // EJTAG IR 通常 5-bit 或 8-bit
    // 查找指令名称
    name = lookup_ejtag_insn(code)
    C_ANN_PUT(ANN_INSTRUCTION, "name: desc (0xHH)")
    // 切换状态
    priv->state = lookup_ejtag_state(code)  // 默认 RESET

  elif cmd == "DR TDI":
    value = bytes_to_uint64(data, data_len)
    priv->last_data_in = value
    priv->last_data_in_ss = ss
    priv->last_data_in_es = es
    priv->has_last_data = 1
    priv->pracc.ss = ss
    priv->pracc.es = es
    switch state:
      ADDRESS → priv->pracc.address_in = value
      DATA → priv->pracc.data_in = value
      FASTDATA → handle_fastdata(di, priv, data, data_len, ANN_CTRL_FIELD_IN)

  elif cmd == "DR TDO":
    value = bytes_to_uint64(data, data_len)
    priv->last_data_out = value
    priv->last_data_out_ss = ss
    priv->last_data_out_es = es
    switch state:
      ADDRESS → priv->pracc.address_out = value
      DATA → priv->pracc.data_out = value
      FASTDATA → handle_fastdata(di, priv, data, data_len, ANN_CTRL_FIELD_OUT)

  elif cmd == "NEW STATE":
    if val == "UPDATE-DR" && has_last_data && state != RESET:
      // 输出寄存器名称
      reg_name = ejtag_reg_names[state]
      C_ANN_PUT(ANN_REG + state, reg_name)
      // CONTROL 寄存器特殊处理
      if state == CONTROL:
        if data_len == 32:  // 验证长度
          parse_control_reg(di, priv, ANN_CTRL_FIELD_IN)
          parse_control_reg(di, priv, ANN_CTRL_FIELD_OUT)
          parse_pracc(di, priv)
        else:
          C_ANN_PUT(ANN_REG, "Error: length != 32")
```

### T2.3 handle 函数清单

| 函数 | 功能 | 复杂度 |
|------|------|--------|
| `handle_ir_tdi()` | 解码 IR 指令，切换状态 | 低 |
| `handle_dr_tdi()` | 保存 TDI 数据，更新 pracc_state | 低 |
| `handle_dr_tdo()` | 保存 TDO 数据，更新 pracc_state | 低 |
| `handle_new_state()` | UPDATE-DR 时触发寄存器解析 | 中 |
| `handle_fastdata()` | 解码 33-bit FASTDATA | 中 |
| `parse_control_reg()` | 逐字段解析 Control Register | 高 |
| `parse_pracc()` | 检测并输出 PrAcc 事务 | 中 |
| `lookup_ejtag_insn()` | IR 码→指令名称查找 | 低 |
| `lookup_ejtag_state()` | IR 码→状态映射 | 低 |

### T2.4 难点：parse_control_reg

这是 jtag_ejtag 最复杂的部分。Python 版本通过 `control_bit_positions` 数组获取每个 bit 的精确 ss/es。C 版本中**没有逐位位置信息**，需要替代方案：

**方案 A（推荐）**：使用整个 DR 区间的 ss/es，对每个字段输出标注，不区分位级位置。
```c
// 对每个字段，使用 priv->last_data_in_ss 和 priv->last_data_in_es
// 输出字段名称和值描述
```

**方案 B**：按比例估算位区间（不推荐，可能不准确）。

### T2.5 难点：NEW STATE 处理

Python 版本中 `handle_new_state()` 在 `NEW STATE` 命令中检查是否为 `UPDATE-DR`。但 C 版本的 `recv_proto` 收到的 `cmd` 是 `"NEW STATE"`，**data 为 NULL**。需要确认 JTAG C 解码器是否发送了状态名称。

查看 `jtag_c.c` 第288行：
```c
c_decoder_put_python(di, ss_state, samplenum, priv->out_python, "NEW STATE", NULL, 0);
```

**问题**：JTAG C 解码器发送 `"NEW STATE"` 时 data 为 NULL，不包含具体状态名称。Python 版本的 JTAG 解码器发送的是 `("NEW STATE", new_state_name)` 元组。

**解决方案**：在 C 版本中，需要在 `recv_proto` 内部跟踪 JTAG TAP 状态变迁。由于 JTAG C 解码器不发送状态名称，上层解码器需要自行推断。最简单的方案是：**在 DR TDI/TDO 后自动触发寄存器解析**，而不依赖 `NEW STATE` 命令。

**替代方案**：修改 `jtag_c.c`，在 `NEW STATE` 的 data 中包含状态名称字符串。但这需要修改底层解码器，影响范围更大。

**最终方案**：对于 jtag_ejtag_c，在每次 DR TDI 后保存数据，在 DR TDO 后（如果当前状态需要 TDO）完成寄存器解析。对于 CONTROL 寄存器，在 DR TDO 后同时解析 TDI 和 TDO 方向的字段。

---

## T3: jtag_avr_c（最复杂）

### T3.1 创建文件 `libsigrokdecode/c_decoders/jtag_avr_c.c`

### T3.2 实现步骤

| 步骤 | 内容 | 详细说明 |
|------|------|---------|
| T3.2.1 | 头文件和 enum 定义 | 定义 `enum avr_ann`（18个annotation）；定义 `enum avr_state`（4个状态：IDLE, BYPASS, IDCODE, PDICOM）；定义 `enum pdi_opcode`（8个 PDI 指令） |
| T3.2.2 | 私有数据结构 | `struct jtag_avr_priv` + 内嵌 `struct pdi_state`（见 spec.md 2.7） |
| T3.2.3 | 静态数据表 | `avr_ir_map[]`（3项）；`jedec_id` 映射；`avr_idcode` 映射；PDI 指令处理函数表 |
| T3.2.4 | ann_labels 定义 | 18项 |
| T3.2.5 | annotation_rows 定义 | 11行 |
| T3.2.6 | channels/options | 无通道，无选项 |
| T3.2.7 | inputs/outputs/tags | `inputs={"jtag", NULL}, tags={"Debug/trace", NULL}` |
| T3.2.8 | reset 函数 | 分配 priv，初始化 state=IDLE，清零 PDI 状态 |
| T3.2.9 | start 函数 | 注册 SRD_OUTPUT_ANN |
| T3.2.10 | decode 函数 | 空函数 |
| T3.2.11 | destroy 函数 | 释放 priv |
| T3.2.12 | recv_proto 函数 | 核心逻辑，见下方详细分解 |
| T3.2.13 | srd_c_decoder 结构体 | `.recv_proto = jtag_avr_recv_proto` |
| T3.2.14 | entry/api_version 函数 | 标准模板 |

### T3.2.12 recv_proto 详细分解

```
recv_proto(di, ss, es, cmd, data, data_len):
  priv->ss = ss, priv->es = es

  if cmd == "IR TDI":
    ir_val = data[0] & 0x0F
    switch ir_val:
      0x3 → state = IDCODE
      0x7 → state = PDICOM
      0xF → state = BYPASS
      default → state = IDLE
    C_ANN_PUT(ANN_JTAG_COMMAND, "IR: <name>")

  elif cmd == "DR TDI":
    switch state:
      BYPASS → handle_reg_bypass(data, data_len); state = IDLE
      PDICOM → pdi_handle_input(di, priv, data, data_len)

  elif cmd == "DR TDO":
    switch state:
      IDCODE → handle_reg_idcode(data, data_len); state = IDLE
      PDICOM → pdi_handle_output(di, priv, data, data_len)
```

### T3.3 PDI 子协议实现

PDI 是 jtag_avr 的核心复杂度。PDICOM DR 数据以 9-bit 帧（8 data + 1 parity）为单位传输。

**关键问题**：C 版本中 `recv_proto` 收到的 DR 数据是整个位移区间的字节数据，需要自行拆分为 9-bit 帧。

**帧拆分逻辑**：
```c
// DR 数据长度（bits）= data_len * 8
// 帧数 = total_bits / 9
// 每帧: bit[0] = parity, bits[1:8] = data (LSB-first)
static void pdi_handle_input(struct srd_decoder_inst *di,
    struct jtag_avr_priv *priv, const unsigned char *data, uint64_t data_len)
{
    int total_bits = (int)(data_len * 8);
    int frame_count = total_bits / 9;

    for (int f = 0; f < frame_count; f++) {
        // 提取 9-bit 帧
        uint8_t frame_bits[2] = {0, 0};
        for (int b = 0; b < 9; b++) {
            int global_bit = f * 9 + b;
            int byte_idx = global_bit / 8;
            int bit_idx = global_bit % 8;
            if (byte_idx < (int)data_len) {
                frame_bits[b / 8] |= ((data[byte_idx] >> bit_idx) & 1) << (b % 8);
            }
        }

        // 校验
        int parity = frame_bits[0] & 1;
        uint8_t data_val = 0;
        int ones = 0;
        for (int b = 1; b < 9; b++) {
            int bit = (frame_bits[b / 8] >> (b % 8)) & 1;
            data_val |= (bit << (b - 1));
            ones += bit;
        }
        int parity_ok = ((ones + parity) % 2 == 0);

        // 标注
        char data_str[16];
        snprintf(data_str, sizeof(data_str), "D: 0x%02x", data_val);
        C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ANN_DATA_IN, data_str);

        if (parity_ok)
            C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ANN_PARITY_IN_OK, "P");
        else
            C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ANN_PARITY_IN_ERR, "PE");

        // 处理 PDI 指令
        pdi_process_frame(di, priv, data_val, parity_ok, 0 /* input */);
    }
}
```

### T3.4 PDI 指令处理状态机

```c
static void pdi_process_frame(struct srd_decoder_inst *di,
    struct jtag_avr_priv *priv, uint8_t data_val, int parity_ok, int is_output)
{
    struct pdi_state *pdi = &priv->pdi;

    if (!parity_ok) {
        // 检查 BREAK 条件
        if (data_val == 0xBB) {
            C_ANN_PUT(di, priv->ss, priv->es, priv->out_ann, ANN_BREAK, "BREAK");
            int saved_rep = pdi->rep_count;
            pdi_clear_insn(pdi);
            pdi->rep_count = saved_rep;
        }
        return;
    }

    if (is_output) {
        pdi_handle_data_output(di, priv, data_val);
        return;
    }

    // Input 方向
    if (pdi->opcode < 0) {
        // 第一个帧：opcode
        pdi->opcode = (data_val & 0xE0) >> 5;
        pdi->ss_cmd = priv->ss;
        int args = data_val & 0x0F;
        pdi_handle_opcode(di, priv, pdi->opcode, args);
    } else {
        // 后续帧：数据
        pdi_handle_data_input(di, priv, data_val);
    }
}
```

### T3.5 handle 函数清单

| 函数 | 功能 | 复杂度 |
|------|------|--------|
| `handle_ir_tdi()` | 解码 IR，切换状态 | 低 |
| `handle_reg_bypass()` | 输出 BYPASS 数据 | 低 |
| `handle_reg_idcode()` | 解码 32-bit IDCODE | 中 |
| `pdi_handle_input()` | 拆分 9-bit 帧，校验，分派 | 高 |
| `pdi_handle_output()` | 拆分 9-bit 帧，校验，分派 | 高 |
| `pdi_process_frame()` | PDI 帧级状态机 | 高 |
| `pdi_handle_opcode()` | 处理 PDI 指令 opcode | 中 |
| `pdi_handle_data_input()` | 处理 PDI 输入数据 | 高 |
| `pdi_handle_data_output()` | 处理 PDI 输出数据 | 中 |
| `pdi_clear_insn()` | 清除 PDI 指令状态 | 低 |
| `pdi_check_parity()` | 9-bit 帧偶校验 | 低 |

---

## T4: CMakeLists.txt 更新

### T4.1 修改位置

文件：`CMakeLists.txt`
搜索：`set(C_DECODERS` 列表
添加：
```cmake
jtag_avr_c
jtag_ejtag_c
jtag_stm32_c
```

### T4.2 验证

确认 `C_DECODERS` 列表中每个名称对应 `libsigrokdecode/c_decoders/<name>.c` 文件存在。

---

## T5: 编译验证

### T5.1 增量编译

```bash
build_incremental.cmd
```

### T5.2 检查项

| 检查项 | 预期结果 |
|--------|---------|
| 编译无错误 | ✅ |
| 编译无警告 | ✅ |
| DLL 生成 | `build.dir/decoders/c_decoders/jtag_avr_c.dll` 等 |
| DLL 安装 | `install.dir/lib/sigrokdecode/decoders/c_decoders/jtag_avr_c.dll` 等 |

### T5.3 运行时验证

1. 启动 PXView
2. 加载包含 JTAG 信号的 .sr 会话文件
3. 添加 JTAG 解码器（jtag_c）
4. 在 JTAG 解码器上堆叠 jtag_stm32_c / jtag_ejtag_c / jtag_avr_c
5. 验证 annotation 输出与 Python 版本一致

---

## 依赖关系图

```
T1 (jtag_stm32_c) ──→ T4 (CMake) ──→ T5 (编译验证)
T2 (jtag_ejtag_c)  ──→ T4          ──→ T5
T3 (jtag_avr_c)    ──→ T4          ──→ T5
```

T1/T2/T3 可并行开发，但建议按复杂度递增顺序逐个完成并验证。

---

## 代码行数估算

| 解码器 | ann_labels | annotation_rows | 静态数据 | 核心逻辑 | 辅助函数 | 结构体/entry | 总计 |
|--------|-----------|----------------|---------|---------|---------|-------------|------|
| jtag_stm32_c | ~20行 | ~20行 | ~60行 | ~150行 | ~30行 | ~50行 | ~330行 |
| jtag_ejtag_c | ~40行 | ~30行 | ~120行 | ~250行 | ~60行 | ~50行 | ~550行 |
| jtag_avr_c | ~55行 | ~60行 | ~50行 | ~350行 | ~80行 | ~55行 | ~650行 |
| **总计** | | | | | | | **~1530行** |
