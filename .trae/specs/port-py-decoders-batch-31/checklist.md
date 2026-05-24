# 移植检查清单 — Batch 31 Python→C 解码器

## 通用检查项（适用于所有 5 个解码器）

### 文件结构

- [ ] 文件名格式正确：`<decoder_id>_c.c`
- [ ] 文件位于 `libsigrokdecode/c_decoders/` 目录
- [ ] 包含必要的头文件：`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<glib.h>`, `"libsigrokdecode.h"`
- [ ] 无需 `<math.h>`（UART 上层解码器不涉及采样点计算）

### struct srd_c_decoder 定义

- [ ] `.id` = `"<name>_c"` 格式
- [ ] `.name` = `"<Name>(C)"` 格式
- [ ] `.longname` 包含 `(C)` 后缀
- [ ] `.desc` 包含 `(C implementation)` 说明
- [ ] `.license` 与 Python 版本一致
- [ ] `.channels = NULL`, `.num_channels = 0`（UART 上层无直接 channel）
- [ ] `.optional_channels = NULL`, `.num_optional_channels = 0`
- [ ] `.inputs = {"uart"}` 或对应上层协议
- [ ] `.outputs` 正确设置（可能为 NULL）
- [ ] `.binary = NULL`, `.num_binary = 0`
- [ ] `.tags` 正确设置
- [ ] `.reset` 指向 reset 函数
- [ ] `.start` 指向 start 函数
- [ ] `.decode` 指向空函数（UART 上层不直接 decode）
- [ ] `.destroy` 指向 destroy 函数
- [ ] **`.recv_proto` 指向 recv_proto 函数**（★ 关键）

### Annotations

- [ ] `ann_labels` 第一列全为 `""`
- [ ] `ann_labels` 每行 3 个字符串（long/short/tiny）
- [ ] `NUM_ANN` 枚举值正确
- [ ] annotation rows 中所有 class 索引 < NUM_ANN
- [ ] annotation rows 的 `ann_classes` 数组以 `-1` 结尾
- [ ] `num_annotation_rows` 与 `annotation_rows` 数组长度一致
- [ ] `num_annotations` = `NUM_ANN`

### Options

- [ ] option 的 `def` 值在 `srd_c_decoder_entry()` 中初始化
- [ ] string option 的 `values` GSList 在 `srd_c_decoder_entry()` 中构建
- [ ] int option 的 `values` GSList 在 `srd_c_decoder_entry()` 中构建
- [ ] option 读取使用正确的 `c_decoder_get_option_*` 函数
- [ ] 默认值与 Python 版本一致

### 私有状态管理

- [ ] `reset()` 中使用 `g_malloc0()` 分配私有结构体（首次）
- [ ] `reset()` 中使用 `memset()` 清零（后续）
- [ ] `destroy()` 中使用 `g_free()` 释放 + `c_decoder_set_private(di, NULL)`
- [ ] 所有 recv_proto 入口检查 `c_decoder_get_private(di)` 非 NULL

### Output 注册

- [ ] `start()` 中注册 `SRD_OUTPUT_ANN`
- [ ] 需要 Python 输出时注册 `SRD_OUTPUT_PYTHON`
- [ ] output ID 与 `outputs` 数组中的字符串一致

### recv_proto 实现

- [ ] 函数签名正确：`void (*recv_proto)(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 正确处理 UART 推送的 cmd 字符串
- [ ] `data` 参数正确解析（注意 `data_len` 检查）
- [ ] 不处理的 cmd 类型安全忽略（return 而非崩溃）

### C_ANN_PUT 使用

- [ ] 使用正确的 `out_ann` output ID
- [ ] ss/es 参数正确（来自 recv_proto 的 start_sample/end_sample 或缓存的值）
- [ ] ann class 索引 < NUM_ANN
- [ ] 文本字符串为字面量或栈上缓冲区（非悬空指针）

### srd_c_decoder_entry()

- [ ] 函数签名为 `SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)`
- [ ] 返回 `&xxx_c_decoder`
- [ ] `srd_c_decoder_api_version()` 函数也存在并返回 `SRD_C_DECODER_API_VERSION`

### CMakeLists.txt

- [ ] 解码器名称添加到 `C_DECODERS` 列表

---

## 解码器特定检查项

### scs_c

- [ ] 仅处理 `"DATA"` cmd
- [ ] `telegram_idx` 从 0 递增到 6
- [ ] `telegram_idx == 0` 时检查 `val == 0xA8`
- [ ] CRC XOR 累积正确（idx 1-4）
- [ ] `telegram_idx == 5` 时 CRC 比较正确
- [ ] `telegram_idx == 6` 后重置为 -1（然后 ++ 变为 0）
- [ ] 无 options
- [ ] 无 outputs

### streletz_c

- [ ] 仅处理 `"FRAME"` cmd（不是 DATA）
- [ ] `data[0]` = byte_val, `data[1]` = rxtx
- [ ] header 匹配逻辑区分 RX/TX（可配置 header 字节）
- [ ] `PACKETSIZE_MIN = 4`, `PACKETSIZE_MAX = 64`
- [ ] `buf_pos` 驱动状态机
- [ ] rxtx 方向切换时重置状态
- [ ] XOR 校验和：全包 XOR 应为 0
- [ ] 校验正确时标注 PACKET_RX/PACKET_TX
- [ ] 校验错误时标注 WARN
- [ ] 数据区域单独标注 DATA_RX/DATA_TX
- [ ] options `header_tx` 默认 217, `header_rx` 默认 157

### ufcs_c

- [ ] 仅处理 `"DATA"` cmd
- [ ] SOP 检测：`0xAA` 触发重置
- [ ] `dataidx == 3` 时确定包长度（ctrl=4, data=DataLen+5）
- [ ] CRC8 多项式 `0x29` 计算正确
- [ ] CRC 校验：`datapkt[0..plen-2]` 计算 vs `datapkt[plen-1]`
- [ ] power_role 解析：1=SRC, 2=SNK, 3=Cable, other=Reserved
- [ ] 控制消息类型名称查找正确（0-15）
- [ ] 数据消息类型名称查找正确
- [ ] SOURCE_CAP PDO 解析：64-bit double word, mode/step_ma/step_mv/max_mv/min_mv/max_ma/min_ma
- [ ] REQUEST 解析：mode/volt/curr
- [ ] SOURCE_INFO/SINK_INFO 解析：温度偏移 -50
- [ ] ERROR_INFO 解析：16 位错误标志
- [ ] `fulltext` option 正确实现
- [ ] `bytepos` 数组记录每个字节的 ss/es

### sbus_futaba_c

- [ ] 处理 4 种 UART 事件：DATA, FRAME, IDLE, BREAK
- [ ] DATA 事件：字节拆分为 8 bits（LSB-first）存入 accum
- [ ] FRAME 事件：检查 `data[2]` (valid) 标志
- [ ] bits accumulator 正确管理（consumed_bits 指针）
- [ ] Header 解析：8 bits, 期望 `0x0F`
- [ ] 16 个比例通道：每个 11 bits, LSB-first bitpack
- [ ] 2 个数字通道：每个 1 bit
- [ ] 标志位：framelost(1b) + failsafe(1b) + MSB_padding(4b)
- [ ] Footer 解析：8 bits, 期望 `0x00`
- [ ] 比例通道值范围检查（prop_val_min/prop_val_max）
- [ ] MSB flags padding 非零时发 WARN
- [ ] 消息完成后多余 bits 发 WARN
- [ ] IDLE 事件：未处理 bits 发 WARN + 重置
- [ ] BREAK 事件：BREAK 警告 + 重置
- [ ] `bitpack_lsb()` 函数正确实现
- [ ] options `prop_val_min` 默认 0, `prop_val_max` 默认 2047

### amulet_ascii_c

- [ ] 仅处理 `"DATA"` cmd
- [ ] 44 个 annotations 正确定义（41 命令 + BIT + FIELD + WARN）
- [ ] 命令码查找表 41 条完整
- [ ] `cmds_with_high_bytes` 检查：0xA0, 0xD7, 0xE7, 0xE2, 0xE3
- [ ] 命令中止逻辑：0xD0-0xF7 范围字节中断非 high_byte 命令
- [ ] 命令分发 switch-case 覆盖所有 41 个命令码
- [ ] 未知命令码发 WARN + 重置 state
- [ ] PAGE 命令：0xA0 + 0x02 + idx_hi + idx_lo + checksum
- [ ] PAGE checksum: (0xA0 + 0x02 + idx_hi + idx_lo) & 0xFF == 0
- [ ] 读取类命令（GBV/GWV/GSV/GLV/GRPC/GBVA/GWVA/GCV/RPC）共享逻辑
- [ ] 设置类命令（SBV/SWV/SSV/SBVA/SWVA/SCV）共享逻辑
- [ ] 字符串处理（SSV/GSVR/GLVR/SSVR）：null 终止
- [ ] 地址解析：2 个 hex nibble → 8-bit 地址
- [ ] 字变量解析：4 个 hex nibble → 16-bit 值
- [ ] 绘图命令坐标解析：4 × 4 nibble → 4 × 16-bit 坐标
- [ ] ACK/NACK 立即响应（无后续字节）
- [ ] 颜色变量命令（GCV/GCVR/SCV/SCVR）：8 字节，标记为 not implemented
- [ ] options `ms_chan` 默认 "RX", `sm_chan` 默认 "TX"
- [ ] `emit_cmd_byte()` / `emit_cmd_end()` / `emit_addr_bytes()` 辅助函数正确
- [ ] ss_cmd/es_cmd/ss_field/es_field 时间戳正确传递

---

## 编译与运行验证

### 编译

- [ ] `build_incremental.cmd` 无错误
- [ ] 无编译警告（特别是 format-truncation 等）
- [ ] 5 个 DLL 生成到 `build.dir/decoders/c_decoders/`

### 运行时

- [ ] PXView 启动无崩溃
- [ ] 解码器列表中可见 5 个新解码器
- [ ] 选择 UART 解码器后可叠加新解码器
- [ ] 解码器选项 UI 正确显示

### 功能验证

- [ ] **scs_c**: 0xA8 开头的 7 字节电报正确解析
- [ ] **streletz_c**: header 匹配 + XOR 校验 + 包注释
- [ ] **ufcs_c**: 0xAA SOP + 控制消息/数据消息 + CRC8
- [ ] **sbus_futaba_c**: 25 字节消息 → 16 通道 + 标志
- [ ] **amulet_ascii_c**: 命令字节触发状态机 + PAGE/ACK/NACK

---

## Python 版本兼容性

### 输出格式一致性

- [ ] annotation class 数量与 Python 版本一致
- [ ] annotation row 布局与 Python 版本一致
- [ ] option 数量和默认值与 Python 版本一致
- [ ] 关键 annotation 文本内容与 Python 版本一致（允许 C 版本更简洁）

### 已知 Python Bug 不需复制

- [ ] UFCS `get_device_info()` 中 `emk`/`imp` 变量名错误 → C 版本修正
- [ ] UFCS `refuse()` 中 `$s` 格式化错误 → C 版本修正
- [ ] UFCS `verify_request()`/`verify_response()` 中 `dhl`/`id` 变量错误 → C 版本修正
- [ ] amulet_ascii `handle_sbva()`/`handle_gbvar()` 中 `stage` 变量应为 `nibble` → C 版本修正
