# 检查清单：jtag_avr / jtag_ejtag / jtag_stm32 Python→C 解码器移植

## 1. 文件结构检查

### 1.1 源文件存在性

- [ ] `libsigrokdecode/c_decoders/jtag_stm32_c.c` 已创建
- [ ] `libsigrokdecode/c_decoders/jtag_ejtag_c.c` 已创建
- [ ] `libsigrokdecode/c_decoders/jtag_avr_c.c` 已创建

### 1.2 CMakeLists.txt 更新

- [ ] `CMakeLists.txt` 的 `C_DECODERS` 列表中已添加 `jtag_stm32_c`
- [ ] `CMakeLists.txt` 的 `C_DECODERS` 列表中已添加 `jtag_ejtag_c`
- [ ] `CMakeLists.txt` 的 `C_DECODERS` 列表中已添加 `jtag_avr_c`

---

## 2. C 解码器通用规范检查

以下检查项适用于所有 3 个解码器，用 `[S]` `[E]` `[A]` 分别标记 jtag_stm32 / jtag_ejtag / jtag_avr。

### 2.1 头文件

- [ ] `[S][E][A]` 包含 `<stdio.h>`
- [ ] `[S][E][A]` 包含 `<stdlib.h>`
- [ ] `[S][E][A]` 包含 `<string.h>`
- [ ] `[S][E][A]` 包含 `<glib.h>`
- [ ] `[S][E][A]` 包含 `"libsigrokdecode.h"`

### 2.2 srd_c_decoder 结构体

- [ ] `[S][E][A]` `.id` 格式为 `"jtag_xxx_c"`（带 `_c` 后缀）
- [ ] `[S][E][A]` `.name` 格式为 `"JTAG/XXX(C)"`（带 `(C)` 后缀）
- [ ] `[S][E][A]` `.longname` 包含 `(C)` 后缀
- [ ] `[S][E][A]` `.desc` 包含 `C implementation` 说明
- [ ] `[S][E][A]` `.license = "gplv2+"`
- [ ] `[S][E][A]` `.channels = NULL, .num_channels = 0`（上层解码器无通道）
- [ ] `[S][E][A]` `.optional_channels = NULL, .num_optional_channels = 0`
- [ ] `[S][E][A]` `.inputs = {"jtag", NULL}, .num_inputs = 1`
- [ ] `[S][E][A]` `.outputs = NULL, .num_outputs = 0`（上层解码器无输出协议）
- [ ] `[S][E][A]` `.tags = {"Debug/trace", NULL}, .num_tags = 1`
- [ ] `[S][E][A]` `.binary = NULL, .num_binary = 0`
- [ ] `[S][E][A]` `.recv_proto` 指向正确的回调函数
- [ ] `[S][E][A]` `.decode` 指向空函数（上层解码器不直接采样）
- [ ] `[S][E][A]` `.reset` 正确分配和初始化私有数据
- [ ] `[S][E][A]` `.start` 注册 SRD_OUTPUT_ANN 输出
- [ ] `[S][E][A]` `.destroy` 释放私有数据

### 2.3 ann_labels 规范

- [ ] `[S][E][A]` `ann_labels` 第一列为空字符串 `""`（C 解码器标准）
- [ ] `[S][E][A]` `ann_labels` 第二列为小写短标识符（如 `"item"`, `"field"`）
- [ ] `[S][E][A]` `ann_labels` 第三列为可读描述（如 `"Item"`, `"Field"`）
- [ ] `[S][E][A]` `ann_labels` 数量与 `.num_annotations` 一致
- [ ] `[S][E][A]` `ann_labels` 顺序与 Python 版本的 `annotations` 元组顺序一致

### 2.4 annotation_rows 规范

- [ ] `[S][E][A]` 每个 row 的 classes 数组以 `-1` 终止
- [ ] `[S][E][A]` classes 数组中的索引与 `ann_labels` 索引对应
- [ ] `[S][E][A]` row 数量与 `.num_annotation_rows` 一致
- [ ] `[S][E][A]` 所有 ann class 至少出现在一个 row 中

### 2.5 入口函数

- [ ] `[S][E][A]` `srd_c_decoder_entry()` 使用 `SRD_C_DECODER_EXPORT` 导出
- [ ] `[S][E][A]` `srd_c_decoder_api_version()` 使用 `SRD_C_DECODER_EXPORT` 导出
- [ ] `[S][E][A]` `srd_c_decoder_api_version()` 返回 `SRD_C_DECODER_API_VERSION`
- [ ] `[S][E][A]` 如有 options，在 `srd_c_decoder_entry()` 中初始化默认值

### 2.6 私有数据管理

- [ ] `[S][E][A]` `reset()` 中使用 `g_malloc0()` 分配（首次检查 NULL）
- [ ] `[S][E][A]` `reset()` 中使用 `memset()` 清零
- [ ] `[S][E][A]` `destroy()` 中使用 `g_free()` 释放
- [ ] `[S][E][A]` `destroy()` 中调用 `c_decoder_set_private(di, NULL)`
- [ ] `[S][E][A]` 所有函数通过 `c_decoder_get_private(di)` 获取私有数据
- [ ] `[S][E][A]` 获取私有数据后检查 NULL

---

## 3. recv_proto 实现检查

### 3.1 函数签名

- [ ] `[S][E][A]` 签名为 `void (*)(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] `[S][E][A]` 函数开头获取并检查 `priv` 非 NULL
- [ ] `[S][E][A]` 保存 `ss = start_sample, es = end_sample`

### 3.2 命令处理

- [ ] `[S][E][A]` 处理 `"IR TDI"` 命令
- [ ] `[S][E][A]` 处理 `"DR TDI"` 命令
- [ ] `[S][E][A]` 处理 `"DR TDO"` 命令
- [ ] `[E]` 处理 `"NEW STATE"` 命令（jtag_ejtag 需要）
- [ ] `[S][A]` 不处理 `"NEW STATE"` 命令（jtag_stm32/jtag_avr 不需要）
- [ ] `[S][E][A]` 使用 `strcmp()` 比较命令字符串

### 3.3 数据提取

- [ ] `[S][E][A]` 从 `data[]` LSB-first 字节数组正确提取整数
- [ ] `[S]` 正确提取 9-bit IR（M3 TAP 4-bit + BS TAP 5-bit）
- [ ] `[E]` 正确提取 IR 值（5-bit 或 8-bit）
- [ ] `[A]` 正确提取 4-bit IR
- [ ] `[S]` 正确处理 35-bit DR 数据（DPACC/APACC）
- [ ] `[E]` 正确处理 33-bit FASTDATA 数据
- [ ] `[A]` 正确拆分 9-bit PDI 帧

### 3.4 状态机

- [ ] `[S]` STM32 状态机：IDLE → BYPASS/IDCODE/DPACC/APACC/ABORT/UNKNOWN → IDLE
- [ ] `[E]` EJTAG 状态机：RESET → DEVICE_ID/IMPLEMENTATION/DATA/ADDRESS/CONTROL/FASTDATA/PC_SAMPLE/BYPASS
- [ ] `[A]` AVR 状态机：IDLE → BYPASS/IDCODE/PDICOM → IDLE
- [ ] `[S][E][A]` 状态转换后正确回到 IDLE（或保持当前状态）
- [ ] `[S][E][A]` 未知 IR 值有合理的 fallback 处理

---

## 4. 解码逻辑正确性检查

### 4.1 jtag_stm32_c

- [ ] IDCODE 解码：正确提取 Version/Part/Manufacturer 字段
- [ ] IDCODE 映射：`cm3_idcode_ver`, `cm3_idcode_part`, `jedec_id` 查找正确
- [ ] DPACC 输入解码：DATA[31:0] + A[3:2] + RnW 提取正确
- [ ] DPACC 输出解码：DATA[31:0] + ACK[2:0] 提取正确
- [ ] APACC 解码：与 DPACC 类似但地址含义不同
- [ ] ABORT 解码：DAPABORT 位提取和保留位检查
- [ ] BYPASS 解码：输出位移数据
- [ ] 未知指令：输出警告
- [ ] BS TAP IR 标注：正确分离 5-bit BS TAP IR
- [ ] M3 TAP IR 标注：正确分离 4-bit M3 TAP IR

### 4.2 jtag_ejtag_c

- [ ] IR 指令解码：19 种指令正确映射
- [ ] IR→State 映射：6 种状态映射正确
- [ ] DR TDI 处理：保存 last_data_in，更新 pracc_state
- [ ] DR TDO 处理：保存 last_data_out，更新 pracc_state
- [ ] CONTROL 寄存器解析：13 个字段逐一标注
- [ ] Control Register 字段值描述：读/写方向使用正确的描述数组
- [ ] PrAcc 检测：PrAcc=0(TDI) && PrAcc=1(TDO) 条件正确
- [ ] PrAcc 输出：Store/Load/Fetch + Address + Data 格式正确
- [ ] FASTDATA 解码：32-bit data + SPrAcc 位提取正确
- [ ] FASTDATA SPrAcc 描述：读/写方向使用正确描述
- [ ] 寄存器名称标注：在 UPDATE-DR 时输出
- [ ] UPDATE-DR 检测：正确识别状态变迁（需考虑 C 版本限制）

### 4.3 jtag_avr_c

- [ ] IR 映射：0x3=IDCODE, 0x7=PDICOM, 0xF=BYPASS
- [ ] IDCODE 解码：Version/Part/Manufacturer 字段提取
- [ ] IDCODE 映射：`jedec_id` (0x1f=Atmel), `avr_idcode` (4种芯片)
- [ ] BYPASS 解码：输出位移数据
- [ ] PDI 帧拆分：9-bit 帧从字节数组正确提取
- [ ] PDI 偶校验：parity + ones count % 2 == 0
- [ ] PDI BREAK 检测：parity 错误且 data == 0xBB
- [ ] PDI opcode 解码：(data & 0xE0) >> 5 提取
- [ ] PDI LDS/LD/STS/ST/LDCS/STCS/REPEAT/KEY 指令处理
- [ ] PDI REPEAT 前缀：正确保存和恢复 rep_count
- [ ] PDI 数据累积：多字节数据正确收集和输出
- [ ] PDI 输入/输出方向区分：DR TDI → input, DR TDO → output
- [ ] PDI 指令完成：数据接收完毕后输出命令标注

---

## 5. Annotation 输出检查

### 5.1 C_ANN_PUT 使用

- [ ] `[S][E][A]` 所有 `C_ANN_PUT` 调用的 ann_class 在有效范围内
- [ ] `[S][E][A]` 所有 `C_ANN_PUT` 调用的 ss/es 使用正确的采样号
- [ ] `[S][E][A]` 文本字符串不为 NULL
- [ ] `[S][E][A]` 文本字符串不以换行符结尾（除非有意为之）

### 5.2 与 Python 版本输出一致性

- [ ] `[S]` Item/Field/Command/Warning 行输出与 Python 版本语义一致
- [ ] `[E]` Instruction/Registers/Control fields/PrAcc 行输出与 Python 版本语义一致
- [ ] `[A]` JTAG Item/Field/Command/Warning 行输出与 Python 版本语义一致
- [ ] `[A]` PDI Data In/Out 行输出与 Python 版本语义一致
- [ ] `[A]` PDI Parity 行输出与 Python 版本语义一致
- [ ] `[A]` PDI Opcode/Data Prog/Data Dev 行输出与 Python 版本语义一致
- [ ] `[A]` PDI Commands 行输出与 Python 版本语义一致

---

## 6. 编译检查

### 6.1 编译通过

- [ ] `[S]` `jtag_stm32_c.c` 编译无错误
- [ ] `[E]` `jtag_ejtag_c.c` 编译无错误
- [ ] `[A]` `jtag_avr_c.c` 编译无错误

### 6.2 编译警告

- [ ] `[S]` 无 `-Wall` 警告
- [ ] `[E]` 无 `-Wall` 警告
- [ ] `[A]` 无 `-Wall` 警告

### 6.3 DLL 生成

- [ ] `[S]` `build.dir/decoders/c_decoders/jtag_stm32_c.dll` 存在
- [ ] `[E]` `build.dir/decoders/c_decoders/jtag_ejtag_c.dll` 存在
- [ ] `[A]` `build.dir/decoders/c_decoders/jtag_avr_c.dll` 存在

---

## 7. 运行时验证

### 7.1 解码器加载

- [ ] `[S]` PXView 能识别 jtag_stm32_c 解码器
- [ ] `[E]` PXView 能识别 jtag_ejtag_c 解码器
- [ ] `[A]` PXView 能识别 jtag_avr_c 解码器

### 7.2 解码器堆叠

- [ ] `[S]` jtag_stm32_c 可堆叠在 jtag_c 之上
- [ ] `[E]` jtag_ejtag_c 可堆叠在 jtag_c 之上
- [ ] `[A]` jtag_avr_c 可堆叠在 jtag_c 之上

### 7.3 功能验证

- [ ] `[S]` STM32 IDCODE 正确解码
- [ ] `[S]` STM32 DPACC/APACC 事务正确解码
- [ ] `[S]` STM32 ABORT 正确解码
- [ ] `[E]` EJTAG 指令正确识别
- [ ] `[E]` EJTAG Control Register 字段正确解析
- [ ] `[E]` EJTAG PrAcc 事务正确检测
- [ ] `[E]` EJTAG FASTDATA 正确解码
- [ ] `[A]` AVR IDCODE 正确解码
- [ ] `[A]` AVR PDI 指令正确解码
- [ ] `[A]` AVR PDI 数据正确解码
- [ ] `[A]` AVR PDI 奇偶校验正确工作

### 7.4 与 Python 版本对比

- [ ] `[S]` C 版本 annotation 数量 ≥ Python 版本（允许简化但不可遗漏关键信息）
- [ ] `[E]` C 版本 annotation 数量 ≥ Python 版本
- [ ] `[A]` C 版本 annotation 数量 ≥ Python 版本

---

## 8. 已知限制记录

以下限制是 C 版本的已知差异，不需要修复但需要记录：

- [ ] `[S][E][A]` 无逐位采样位置标注（recv_proto 不提供 samplenums 数组）
- [ ] `[S]` IDCODE 字段（Reserved/Manufacturer/Part/Version）无法精确位级标注
- [ ] `[E]` Control Register 字段无法精确位级标注
- [ ] `[E]` FASTDATA 的 SPrAcc 位无法精确位级标注
- [ ] `[A]` PDI 帧内 Data/Parity 位无法精确位级标注
- [ ] `[A]` PDI 数据字节的精确起止位置无法标注

---

## 9. 代码质量检查

### 9.1 内存安全

- [ ] `[S][E][A]` 无内存泄漏（g_malloc0/g_free 配对）
- [ ] `[S][E][A]` 无缓冲区溢出（snprintf 使用 sizeof 限制）
- [ ] `[S][E][A]` 无使用未初始化的变量
- [ ] `[S][E][A]` 无悬空指针

### 9.2 代码风格

- [ ] `[S][E][A]` 函数命名：`jtag_xxx_` 前缀
- [ ] `[S][E][A]` enum 命名：大写 + 前缀
- [ ] `[S][E][A]` struct 命名：小写 + `_priv` 后缀
- [ ] `[S][E][A]` 缩进：4 空格
- [ ] `[S][E][A]` 行宽：不超过 120 字符

### 9.3 兼容性

- [ ] `[S][E][A]` C11 兼容
- [ ] `[S][E][A]` 无平台特定代码（Windows/Linux 均可编译）
- [ ] `[S][E][A]` 无硬编码路径
- [ ] `[S][E][A]` 无未使用的变量或函数
