# Python → C Decoder 移植任务列表 (Batch 13)

## 任务概览

按优先级排序，从简单到复杂逐步实现。

---

## Task 1: AUD 解码器移植 (复杂度：低)

### 1.1 创建文件 `libsigrokdecode/c_decoders/aud_c.c`

- [ ] 编写文件头注释（GPLv2+ license）
- [ ] 包含必要头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 定义 annotation 枚举：`ANN_DEST = 0`, `NUM_ANN = 1`
- [ ] 定义 channels 数组 `aud_channels[6]`：
  - audck (SCLK), naudsync (SDATA), audata3/2/1/0 (ADATA)
  - 所有 channel 带 idn
- [ ] 定义 ann_labels `aud_ann_labels[1][3]`：`{"", "dest", "Destination address"}`
- [ ] 定义 annotation_rows `aud_ann_rows[1]`：`{"addresses", "Addresses", classes, 1}`
- [ ] 定义 inputs `["logic", NULL]`、tags `["Debug/trace", NULL]`

### 1.2 实现 state struct 和回调函数

- [ ] 定义 `struct aud_priv`：ncnt, nmax, addr, lastaddr, ss, out_ann
- [ ] 实现 `aud_reset()`：分配 priv，memset 清零
- [ ] 实现 `aud_start()`：注册 SRD_OUTPUT_ANN
- [ ] 实现 `aud_decode()`：
  - while(1) 循环
  - 使用 `c_cond_rise(cb, 0)` 等待 AUDCK 上升沿
  - 读取 6 个 pin 值
  - 重建 nibble：`nib = d0 | (d1<<1) | (d2<<2) | (d3<<3)`
  - sync=1 时：检查 ncnt==nmax，输出地址 annotation，重置计数器，根据 nibble 设置 nmax
  - sync=0 且 nmax>0 时：移入 nibble 到 addr
- [ ] 实现 `aud_destroy()`：释放 priv

### 1.3 定义 srd_c_decoder 结构体

- [ ] `.id = "aud_c"`, `.name = "AUD(C)"`
- [ ] 设置所有字段
- [ ] 实现 `srd_c_decoder_entry()` 返回结构体指针
- [ ] 实现 `srd_c_decoder_api_version()` 返回 `SRD_C_DECODER_API_VERSION`

### 1.4 构建

- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `aud_c`

---

## Task 2: ADAT 解码器移植 (复杂度：中)

### 2.1 创建文件 `libsigrokdecode/c_decoders/adat_c.c`

- [ ] 编写文件头注释（GPLv2+ license）
- [ ] 包含必要头文件：`stdio.h`, `stdlib.h`, `string.h`, `math.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 定义 annotation 枚举：ANN_BIT=0 到 ANN_CHANNEL_7=14, NUM_ANN=15
- [ ] 定义 channels 数组 `adat_channels[1]`：adat (SDATA)
- [ ] 定义 ann_labels `adat_ann_labels[15][3]`
- [ ] 定义 annotation_rows `adat_ann_rows[12]`（bits, nibbles, fields, user-data, channel0-7）
- [ ] 定义 options 数组 `adat_options[3]`：
  - samplerate (int, default 48000)
  - sample_display (string, "decimal"/"hexadecimal")
  - annotations (string, "intra-frame"/"per-frame"/"both")
- [ ] 定义 inputs/tags

### 2.2 实现 state struct 和辅助函数

- [ ] 定义 `struct adat_priv`：samplerate, bit_time, bit_time_int, signal buffer, times buffer, state, channel_no, nibble_no, channel_data, all_channels_data[8], frame_start_time, frame_user_data, sample_display_hex, annotations_mode, out_ann
- [ ] 实现 `sign_extend_24bit()` 辅助函数
- [ ] 实现 `bits_to_int()` 辅助函数

### 2.3 实现核心解码函数

- [ ] 实现 `adat_reset()`：分配 priv，memset 清零
- [ ] 实现 `adat_start()`：注册 SRD_OUTPUT_ANN，读取 options
- [ ] 实现 `adat_metadata()`：处理 SRD_CONF_SAMPLERATE，计算 bit_time，检查最低采样率
- [ ] 实现 `look_for_sync_pad()`：
  - 检查 signal[:11] == [1,0,0,0,0,0,0,0,0,0,0]
  - 切换到 USER BITS 状态
  - 输出 SYNC annotation
- [ ] 实现 `decode_user_bits()`：
  - 累积 5 bits
  - 验证首 bit=1（4b/5b）
  - 提取 4-bit user data
  - 输出 USER/NIBBLE annotations
  - 切换到 CHANNEL DATA 状态
- [ ] 实现 `decode_channel_data()`：
  - 累积 5 bits/nibble
  - 6 nibble/channel，8 channels/frame
  - 输出 CHANNEL/NIBBLE annotations
  - 完成所有 channel 后输出 per-frame annotations

### 2.4 实现 decode 主循环

- [ ] 实现 `adat_decode()`：
  - samplerate guard
  - while(1) 循环
  - 使用 `c_cond_edge(cb, 0)` 等待 ADAT 信号 edge
  - NRZI 解码：根据时间差计算 bit 数，填充 signal buffer
  - 根据 state 调用对应的解码函数
  - 更新 last_time

### 2.5 定义 srd_c_decoder 结构体

- [ ] `.id = "adat_c"`, `.name = "ADAT(C)"`
- [ ] 设置 `.metadata = adat_metadata`
- [ ] 实现 `srd_c_decoder_entry()`：初始化 options 的 GVariant 默认值和 values 列表
- [ ] 实现 `srd_c_decoder_api_version()`

### 2.6 构建

- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `adat_c`

---

## Task 3: AVR PDI 解码器移植 (复杂度：中高)

### 3.1 创建文件 `libsigrokdecode/c_decoders/avr_pdi_c.c`

- [ ] 编写文件头注释（GPLv2+ license）
- [ ] 包含必要头文件
- [ ] 定义 annotation 枚举：ANN_BIT=0 到 ANN_COMMAND=14, NUM_ANN=15
- [ ] 定义 binary 枚举：BIN_BYTES=0, NUM_BIN=1
- [ ] 定义 channels 数组 `avr_pdi_channels[2]`：reset(SCLK), data(SDATA)，带 idn
- [ ] 定义 ann_labels `avr_pdi_ann_labels[15][3]`
- [ ] 定义 binary_labels `avr_pdi_binary_labels[1][3]`
- [ ] 定义 annotation_rows `avr_pdi_ann_rows[4]`
- [ ] 定义 inputs/tags

### 3.2 实现 PDI 指令相关常量和辅助函数

- [ ] 定义 PDI opcode 枚举：OP_LDS=0 到 OP_KEY=7
- [ ] 定义 pointer_format_nice/terse 字符串数组
- [ ] 实现 `ctrl_reg_name()` 函数
- [ ] 实现 `count_ones()` 和 `parity_even_ok()` 函数

### 3.3 实现 state struct

- [ ] 定义 `struct pdi_bit`：val, ss, es
- [ ] 定义 `struct avr_pdi_priv`：
  - Clock edge tracking: ss_last_fall, ss_curr_fall, data_sample
  - UART frame: bits[12], bit_count
  - BREAK detection: zero_count, zero_ss, break_ss, break_es
  - PDI instruction: insn_rep_count, insn_opcode, insn_dat_bytes[8], insn_dat_count, insn_ss_data
  - Command tracking: cmd_ss, cmd_parts_nice[256], cmd_parts_terse[256]
  - Operand info: insn_write_counts, insn_read_counts, width_addr, width_data
  - Pointer/register text: ptr_txt, ptr_txt_terse, reg_num, reg_txt, reg_txt_terse
  - Output: out_ann, out_binary

### 3.4 实现 UART 层处理

- [ ] 实现 `avr_pdi_reset()`：分配 priv，调用 clear_state
- [ ] 实现 `clear_state()`：清零所有 UART 和 PDI 状态
- [ ] 实现 `clear_insn()`：清零指令状态（保留 insn_rep_count）
- [ ] 实现 `avr_pdi_start()`：注册 SRD_OUTPUT_ANN 和 SRD_OUTPUT_BINARY
- [ ] 实现 `avr_pdi_metadata()`：处理 SRD_CONF_SAMPLERATE
- [ ] 实现 `handle_bits()`：
  - BREAK 检测（连续 11+ 个 0 bit）
  - 累积 11 bits 组成帧
  - 解析 start/data/parity/stop
  - 输出 UART 层 annotations
  - 有效帧时调用 handle_byte()
- [ ] 实现 `handle_clk_edge()`：
  - 上升沿采样 DATA
  - 下降沿处理 bit slot

### 3.5 实现 PDI 指令层处理

- [ ] 实现 `handle_byte()`：
  - BREAK 处理（byteval=NULL）
  - Opcode 解码（8 种 opcode）
  - 操作数大小计算
  - 多字节数据累积
  - Little-endian 数据重组
  - REPEAT 计数管理
  - Command annotation 输出
  - Binary 输出

### 3.6 实现 decode 主循环

- [ ] 实现 `avr_pdi_decode()`：
  - while(1) 循环
  - 使用 `c_cond_edge(cb, 0)` 等待 RESET pin edge
  - 调用 handle_clk_edge()

### 3.7 定义 srd_c_decoder 结构体

- [ ] `.id = "avr_pdi_c"`, `.name = "AVR PDI(C)"`
- [ ] 设置 `.metadata = avr_pdi_metadata`
- [ ] 设置 `.binary = avr_pdi_binary_labels`, `.num_binary = 1`
- [ ] 实现 `srd_c_decoder_entry()` 和 `srd_c_decoder_api_version()`

### 3.8 构建

- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `avr_pdi_c`

---

## Task 4: ARM ETMv3 解码器移植 (复杂度：高)

### 4.1 创建文件 `libsigrokdecode/c_decoders/arm_etmv3_c.c`

- [ ] 编写文件头注释（GPLv2+ license）
- [ ] 包含必要头文件
- [ ] 定义 annotation 枚举：ANN_TRACE=0 到 ANN_FUNCTION=10, NUM_ANN=11
- [ ] 定义 ann_labels `arm_etmv3_ann_labels[11][3]`
- [ ] 定义 annotation_rows `arm_etmv3_ann_rows[8]`
- [ ] 定义 options 数组 `arm_etmv3_options[4]`：
  - objdump (string, default "arm-none-eabi-objdump")
  - objdump_opts (string, default "-lSC")
  - elffile (string, default "")
  - branch_enc (string, "alternative"/"original")
- [ ] 定义 inputs `["uart", NULL]`、outputs `[]`、tags `["Debug/trace", NULL]`

### 4.2 实现辅助函数

- [ ] 定义 exception 名称数组 `exc_names[]`
- [ ] 实现 `parse_varint()`：解析变长整数
- [ ] 实现 `parse_uint()`：解析小端整数
- [ ] 实现 `parse_exc_info()`：解析异常信息
- [ ] 实现 `parse_branch_addr()`：解析分支地址
- [ ] 实现 `get_packet_type()`：根据首字节识别包类型

### 4.3 实现包处理函数

- [ ] 实现 `handle_a_sync()`：检测同步包
- [ ] 实现 `handle_i_sync()`：解析 I-Sync 包
- [ ] 实现 `handle_trigger()`：触发事件
- [ ] 实现 `handle_p_header()`：指令执行状态
- [ ] 实现 `handle_branch()`：分支地址
- [ ] 实现 `handle_exception_entry/exit()`：异常处理
- [ ] 实现 `fallback()`：未处理的包类型

### 4.4 实现 state struct 和回调

- [ ] 定义 `struct etmv3_priv`：buf, syncbuf, prevsample, startsample, last_branch, cpu_state, current_pc, branch_enc_alt, out_ann
- [ ] 实现 `arm_etmv3_reset()`：分配 priv，memset 清零
- [ ] 实现 `arm_etmv3_start()`：注册 SRD_OUTPUT_ANN，读取 branch_enc option
- [ ] 实现 `arm_etmv3_recv_proto()`：
  - 只处理 "DATA" 类型
  - 检测长间隔重置 buf
  - 维护 syncbuf 检测同步
  - 调用对应的 handle 函数
  - 输出 annotations

### 4.5 定义 srd_c_decoder 结构体

- [ ] `.id = "arm_etmv3_c"`, `.name = "ARM ETMv3(C)"`
- [ ] 设置 `.recv_proto = arm_etmv3_recv_proto`
- [ ] 实现 `srd_c_decoder_entry()`：初始化 options
- [ ] 实现 `srd_c_decoder_api_version()`

### 4.6 构建

- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `arm_etmv3_c`

---

## Task 5: Z80 解码器移植 (复杂度：极高)

### 5.1 创建文件 `libsigrokdecode/c_decoders/z80_c.c`

- [ ] 编写文件头注释（GPLv3+ license）
- [ ] 包含必要头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 定义 Cycle 枚举、Ann 枚举、Row 枚举、Pin 枚举
- [ ] 定义 channels 数组 `z80_channels[11]`：d0-d7, m1, rd, wr
- [ ] 定义 optional_channels 数组 `z80_optional_channels[18]`：mreq, iorq, a0-a15
- [ ] 定义 ann_labels `z80_ann_labels[9][3]`
- [ ] 定义 annotation_rows `z80_ann_rows[5]`
- [ ] 定义 inputs/tags

### 5.2 翻译指令表

- [ ] 定义 `struct z80_instr`：want_dis, want_imm, want_read, want_write, op_repeat, mnemonic
- [ ] 翻译 `main_instructions` → `main_instr_table[256]`
- [ ] 翻译 `extended_instructions` → `extended_instr_table[256]`
- [ ] 翻译 `bit_instructions` → `bit_instr_table[256]`
- [ ] 翻译 `index_instructions` → `index_instr_table[256]`
- [ ] 翻译 `index_bit_instructions` → `index_bit_instr_table[256]`
- [ ] 定义 `instr_table_by_prefix` 查找表

### 5.3 实现辅助函数

- [ ] 实现 `reduce_bus()`：从 pin 数组重建总线值
- [ ] 实现 `signed_byte()`：有符号字节转换
- [ ] 实现 `detect_cycle()`：根据控制信号检测 bus cycle 类型
- [ ] 实现 `format_hex_H()`：自定义十六进制格式化（{value:04H}h 格式）
- [ ] 实现 `format_mnemonic()`：格式化指令助记符

### 5.4 实现状态机

- [ ] 定义 `struct z80_priv`：所有状态变量
- [ ] 实现 `z80_reset()`：分配 priv，初始化状态
- [ ] 实现 `z80_start()`：注册 SRD_OUTPUT_ANN
- [ ] 实现 cycle 事件处理：
  - `on_cycle_begin()`：开始新 cycle
  - `on_cycle_end()`：结束 cycle，推进状态机
  - `on_cycle_trans()`：非法转换警告
- [ ] 实现所有状态函数：
  - `state_IDLE()`
  - `state_PRE1()`
  - `state_PRE2()`
  - `state_PREDIS()`
  - `state_OPCODE()`
  - `state_POSTDIS()`
  - `state_IMM1()`
  - `state_IMM2()`
  - `state_ROP1()`
  - `state_ROP2()`
  - `state_WOP1()`
  - `state_WOP2()`
  - `state_RESTART()`
- [ ] 实现 annotation 输出函数：
  - `put_text()`
  - `put_disasm()`

### 5.5 实现 decode 主循环

- [ ] 实现 `z80_decode()`：
  - while(1) 循环
  - 使用条件构建器等待控制信号变化
  - 读取所有 pin 值
  - 检测 cycle 类型
  - 处理 cycle 转换

### 5.6 定义 srd_c_decoder 结构体

- [ ] `.id = "z80_c"`, `.name = "Z80(C)"`
- [ ] 实现 `srd_c_decoder_entry()` 和 `srd_c_decoder_api_version()`

### 5.7 构建

- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `z80_c`

---

## Task 6: 集成测试

### 6.1 编译验证

- [ ] 运行 `build_incremental.cmd` 确保所有 5 个 C decoder 编译成功
- [ ] 检查生成的 DLL 文件在 `build.dir/decoders/c_decoders/` 目录中

### 6.2 运行时验证

- [ ] 在 PXView 中加载各解码器，确认无崩溃
- [ ] 验证 annotation classes 和 rows 正确显示
- [ ] 验证 options 正确加载和保存
- [ ] 使用已知信号数据验证解码输出正确性

### 6.3 CMakeLists.txt 最终确认

- [ ] 确认 `C_DECODERS` 列表包含所有 5 个新解码器
- [ ] 确认编译和链接无错误
