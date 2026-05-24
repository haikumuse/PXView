# Python → C Decoder 移植验证清单 (Batch 13)

## 通用验证项（适用于所有 5 个解码器）

### 文件结构

- [ ] 文件位于 `libsigrokdecode/c_decoders/<name>_c.c`
- [ ] 文件名中 `-` 替换为 `_`（如 `arm_etmv3_c.c`）
- [ ] 包含正确的 license 头部注释
- [ ] 包含必要头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 无编译 warning

### srd_c_decoder 结构体

- [ ] `.id` 格式为 `"<name>_c"`（如 `"aud_c"`, `"adat_c"`, `"arm_etmv3_c"`, `"avr_pdi_c"`, `"z80_c"`）
- [ ] `.name` 格式为 `"<NAME>(C)"`（如 `"AUD(C)"`, `"ADAT(C)"`, `"ARM ETMv3(C)"`, `"AVR PDI(C)"`, `"Z80(C)"`）
- [ ] `.longname` 包含完整描述 + `(C)` 后缀
- [ ] `.desc` 与 Python 版本一致 + `(C implementation)` 后缀
- [ ] `.license` 与 Python 版本一致
- [ ] `.channels` / `.optional_channels` 与 Python 版本一致，包含正确的 idn
- [ ] `.num_channels` / `.num_optional_channels` 数值正确
- [ ] `.options` 与 Python 版本一致（如有）
- [ ] `.num_options` 数值正确
- [ ] `.num_annotations` 等于 annotation class 总数
- [ ] `.ann_labels` 第一列为空字符串 `""`
- [ ] `.ann_labels` 第二列为 annotation id，第三列为 annotation name
- [ ] 所有 annotation classes 都映射到 annotation_rows
- [ ] `.annotation_rows` 中 class 数组以 `-1` 结尾
- [ ] `.annotation_rows` 中 class_count 不包含末尾 `-1`
- [ ] `.inputs` 与 Python 版本一致
- [ ] `.outputs` 与 Python 版本一致（如有）
- [ ] `.tags` 与 Python 版本一致
- [ ] `.binary` 定义正确（仅 avr_pdi）
- [ ] `.num_binary` 正确（仅 avr_pdi）
- [ ] `.reset` 回调函数已实现
- [ ] `.start` 回调函数已实现
- [ ] `.decode` 回调函数已实现
- [ ] `.destroy` 回调函数已实现
- [ ] `.metadata` 回调函数已实现（需要 samplerate 的解码器）
- [ ] `.recv_proto` 回调函数已实现（仅 arm_etmv3）

### srd_c_decoder_entry()

- [ ] 函数签名正确：`SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)`
- [ ] 所有 options 的 `idn` 字段已设置
- [ ] 所有 options 的 `def` 字段已用 `g_variant_new_*()` 初始化
- [ ] 字符串 option 的 `values` GSList 已创建（如有枚举值）
- [ ] 整数 option 使用 `g_variant_new_int64()`
- [ ] 双精度 option 使用 `g_variant_new_double()`
- [ ] 字符串 option 使用 `g_variant_new_string()`
- [ ] 返回结构体指针

### srd_c_decoder_api_version()

- [ ] 函数签名正确：`SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)`
- [ ] 返回 `SRD_C_DECODER_API_VERSION`

### 内存管理

- [ ] `reset()` 中使用 `g_malloc0()` 分配 priv
- [ ] `destroy()` 中使用 `g_free()` 释放 priv
- [ ] `destroy()` 中调用 `c_decoder_set_private(di, NULL)`
- [ ] 无内存泄漏

### Condition Builder 使用

- [ ] 使用 `c_cond_new()` 创建 builder
- [ ] 使用 `c_cond_free()` 释放 builder
- [ ] `c_cond_wait()` 返回值检查 `ret != SRD_OK` 时 return
- [ ] 正确使用 `c_cond_rise/fall/edge/high/low/skip/or/wait/free`

### Annotation 输出

- [ ] 使用 `C_ANN_PUT` 宏输出 annotation
- [ ] ss < es（起始 sample < 结束 sample）
- [ ] annotation class 索引在有效范围内
- [ ] 字符串参数有效（非 NULL）

---

## AUD 解码器专项验证

- [ ] 6 个 channels 定义正确：audck(SCLK), naudsync(SDATA), audata3/2/1/0(ADATA)
- [ ] 所有 channel 的 idn 与 Python 版本一致
- [ ] 仅 1 个 annotation class (ANN_DEST)
- [ ] 仅 1 个 annotation row
- [ ] 无 options
- [ ] 使用 `c_cond_rise(cb, 0)` 等待 AUDCK 上升沿
- [ ] Nibble 重建逻辑正确：`d0 | (d1<<1) | (d2<<2) | (d3<<3)`
- [ ] sync=1 时地址输出和计数器重置逻辑正确
- [ ] 命令 nibble (0x08/0x09/0x0A/0x0B) 正确映射到 nmax (1/2/4/8)
- [ ] sync=0 时 nibble 移位逻辑正确
- [ ] 地址格式化为 `"0x%08X"`

---

## ADAT 解码器专项验证

- [ ] 1 个 channel 定义正确：adat(SDATA)
- [ ] 15 个 annotation classes 全部定义
- [ ] 12 个 annotation rows 全部定义
- [ ] 3 个 options 定义正确：
  - samplerate: int, default 48000
  - sample_display: string, "decimal"/"hexadecimal"
  - annotations: string, "intra-frame"/"per-frame"/"both"
- [ ] samplerate guard：metadata 回调 + decode 中 fallback
- [ ] 最低采样率检查：`samplerate >= 2.5 * 256 * audio_samplerate`
- [ ] bit_time 计算：`samplerate / (256 * audio_samplerate)`
- [ ] NRZI 解码逻辑正确：edge 处根据时间差填充 bits
- [ ] SYNC pad 检测：`[1,0,0,0,0,0,0,0,0,0,0]` (11 bits)
- [ ] 4b/5b 验证：首 bit 必须为 1
- [ ] User bits 解码：5 bits → 4-bit user data
- [ ] Channel data 解码：6 nibble/channel × 8 channels
- [ ] Sign extend 24-bit 正确
- [ ] annotations_mode 控制输出级别正确
- [ ] Signal buffer 溢出保护

---

## ARM ETMv3 解码器专项验证

- [ ] 无 channels（stack 在 uart 之上）
- [ ] 11 个 annotation classes 全部定义
- [ ] 8 个 annotation rows 全部定义
- [ ] 4 个 options 定义正确（objdump 相关 3 个 + branch_enc）
- [ ] inputs 为 `["uart", NULL]`
- [ ] `recv_proto` 回调已实现
- [ ] 只处理 `"DATA"` 类型的 proto 消息
- [ ] 长间隔重置 buf 逻辑：`ss - prevsample > 16 * byte_len`
- [ ] syncbuf 维护：`[0x00, 0x00, 0x00, 0x00, 0x80]` 检测
- [ ] get_packet_type() 所有分支正确
- [ ] handle_a_sync() 正确
- [ ] handle_i_sync() 正确：
  - contextid_bytes = 0
  - cyclecount 解析
  - info byte 解析
  - PC 更新
  - CPU 状态更新 (arm/thumb/jazelle)
- [ ] handle_branch() 正确：
  - varint 解析
  - branch_enc alternative/original 两种模式
  - exc_info 解析
  - CPU 状态变更
- [ ] handle_p_header() 正确：
  - 非 cycle-accurate 模式
  - E/N 计数解析
  - instructions_executed() 调用
- [ ] handle_exception_entry/exit() 正确
- [ ] objdump 功能已跳过（C 版本不支持）
- [ ] branch_enc option 正确读取

---

## AVR PDI 解码器专项验证

- [ ] 2 个 channels 定义正确：reset(SCLK), data(SDATA)
- [ ] 所有 channel 的 idn 与 Python 版本一致
- [ ] 15 个 annotation classes 全部定义
- [ ] 4 个 annotation rows 全部定义
- [ ] 1 个 binary class 定义
- [ ] 无 options
- [ ] samplerate guard：metadata 回调
- [ ] Clock edge 处理：
  - 上升沿采样 DATA
  - 下降沿处理 bit slot
- [ ] 使用 `c_cond_edge(cb, 0)` 等待 RESET pin edge
- [ ] UART 帧格式：1 start + 8 data + 1 parity + 1 stop = 11 bits
- [ ] BREAK 检测：
  - 连续 11+ 个 0 bit
  - 在 line 恢复 high 时输出 annotation
  - 传递 NULL 给 handle_byte
- [ ] Parity 检查：even parity
- [ ] Stop bit 检查
- [ ] 8 种 opcode 解码正确：
  - LDS: arg32=addr width, arg10=data width, write=1, read=1
  - LD: arg32=pointer format, arg10=data width, write=0, read=1
  - STS: arg32=addr width, arg10=data width, write=2, read=0
  - ST: arg32=pointer format, arg10=data width, write=1, read=0
  - LDCS: arg30=reg, write=0, read=1
  - STCS: arg30=reg, write=1, read=0
  - REPEAT: arg10=data width, write=1, read=0
  - KEY: fixed 8 bytes, write=1, read=0
- [ ] REPEAT 前缀正确处理：
  - REPEAT N → 下一条指令执行 N+1 次
  - insn_rep_count 在 clear_insn 时保留
  - LD/ST 消费 rep_count
- [ ] Little-endian 多字节数据重组
- [ ] Binary 输出使用 `c_decoder_put_binary()`
- [ ] Pointer format 文本正确：`*(ptr)`, `*(ptr++)`, `ptr`, `ptr++ (rsv)`
- [ ] 控制寄存器名称：status(0), reset(1), ctrl(2)

---

## Z80 解码器专项验证

- [ ] 11 个 required channels 定义正确：d0-d7, m1, rd, wr
- [ ] 18 个 optional channels 定义正确：mreq, iorq, a0-a15
- [ ] 所有 channel 的 idn 与 Python 版本一致
- [ ] 9 个 annotation classes 全部定义
- [ ] 5 个 annotation rows 全部定义
- [ ] 无 options
- [ ] 无 samplerate 依赖
- [ ] Cycle 检测逻辑正确：
  - MREQ=0, RD=0, M1=0 → FETCH
  - MREQ=0, RD=0, M1=1 → MEMRD
  - MREQ=0, WR=0 → MEMWR
  - IORQ=0, M1=0 → INTACK
  - IORQ=0, RD=0 → IORD
  - IORQ=0, WR=0 → IOWR
  - Optional channel 默认值：MREQ=1(asserted), IORQ=1(not asserted)
- [ ] Bus 数据读取：reduce_bus() 正确处理未分配 channel (0xFF → -1)
- [ ] 指令表完整翻译：
  - main_instructions (256 条)
  - extended_instructions (部分有效)
  - bit_instructions (256 条)
  - index_instructions (部分有效)
  - index_bit_instructions (256 条)
- [ ] 指令表查找：`instr_table_by_prefix` 正确映射
  - 0 → main
  - 0xED → extended
  - 0xCB → bit
  - 0xDD → index (reg="IX")
  - 0xFD → index (reg="IY")
  - 0xDDCB → index_bit (reg="IX")
  - 0xFDCB → index_bit (reg="IY")
- [ ] 状态机所有 13 个状态正确实现
- [ ] 前缀处理正确：
  - DD/FD 后跟 CB → PRE2 状态
  - DD/FD 后跟 DD/ED/FD → 重新进入 PRE1
  - ED 后跟 CB → 作为普通 opcode 处理
- [ ] Displacement 处理：signed_byte() 转换
- [ ] 立即数处理：1 字节和 2 字节
- [ ] 读写操作数处理：1 字节和 2 字节
- [ ] Big-endian 写操作数处理（want_write < 0）
- [ ] 重复指令标记处理
- [ ] 格式化输出：
  - `{i:04H}h` 格式正确（前导0 + H后缀）
  - `{i:02H}h` 格式正确
  - `{d:+d}` displacement 格式
  - `{j:+d}` relative jump 格式
  - `{r}` register name 替换
- [ ] 地址 annotation 输出正确
- [ ] 数据 annotation 输出正确（MEMRD/MEMWR/IORD/IOWR）
- [ ] 指令 annotation 输出正确
- [ ] 操作数 annotation 输出正确（ROP/WOP）
- [ ] 警告 annotation 输出正确（非法转换、无效指令等）
- [ ] Cycle 转换处理正确：
  - NONE → 有 cycle：on_cycle_begin
  - 有 cycle → NONE：on_cycle_end
  - 有 cycle → 另一个 cycle：on_cycle_trans（警告）

---

## CMakeLists.txt 验证

- [ ] `C_DECODERS` 列表包含 `aud_c`
- [ ] `C_DECODERS` 列表包含 `adat_c`
- [ ] `C_DECODERS` 列表包含 `arm_etmv3_c`
- [ ] `C_DECODERS` 列表包含 `avr_pdi_c`
- [ ] `C_DECODERS` 列表包含 `z80_c`
- [ ] 编译无错误
- [ ] 编译无 warning
- [ ] 生成的 DLL 文件位于 `build.dir/decoders/c_decoders/`

---

## 运行时验证

### AUD

- [ ] 解码器可加载，无崩溃
- [ ] 6 个 channel 正确显示
- [ ] 地址 annotation 正确显示
- [ ] 不同命令 nibble (0x08-0x0B) 正确设置地址长度

### ADAT

- [ ] 解码器可加载，无崩溃
- [ ] 1 个 channel 正确显示
- [ ] 3 个 options 可设置
- [ ] 采样率不足时显示错误
- [ ] SYNC pad 正确检测
- [ ] 8 个 channel 数据正确解码
- [ ] User data 正确显示
- [ ] 十进制/十六进制显示切换正确

### ARM ETMv3

- [ ] 解码器可加载，无崩溃
- [ ] 可 stack 在 uart_c 之上
- [ ] 4 个 options 可设置
- [ ] 接收 UART 字节流正确
- [ ] 同步包正确检测
- [ ] I-Sync 包正确解析
- [ ] Branch 包正确解析
- [ ] P-header 包正确解析
- [ ] branch_enc 两种模式正确

### AVR PDI

- [ ] 解码器可加载，无崩溃
- [ ] 2 个 channel 正确显示
- [ ] UART 帧正确解码
- [ ] BREAK 条件正确检测
- [ ] 8 种 opcode 正确解码
- [ ] REPEAT 前缀正确处理
- [ ] Binary 输出正确
- [ ] Parity/Stop 错误正确标注

### Z80

- [ ] 解码器可加载，无崩溃
- [ ] 11 个 required channel + 18 个 optional channel 正确显示
- [ ] Bus cycle 正确检测
- [ ] 无前缀指令正确反汇编
- [ ] ED 前缀指令正确反汇编
- [ ] CB 前缀指令正确反汇编
- [ ] DD/FD 前缀指令正确反汇编
- [ ] DD CB / FD CB 前缀指令正确反汇编
- [ ] 地址/数据总线 annotation 正确
- [ ] 指令 annotation 正确
- [ ] 操作数 annotation 正确
- [ ] 警告 annotation 正确
