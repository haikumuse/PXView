# 移植检查清单 — Batch 22

> 解码器：xfp, hdcp, hdmi_scdc, tca6408a, tmp102
> 每个 C 解码器必须通过以下所有检查项

---

## 通用检查项（适用于所有 5 个解码器）

### 文件结构

- [ ] 文件位于 `libsigrokdecode/c_decoders/<name>_c.c`
- [ ] 包含正确的头文件：`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<glib.h>`, `"libsigrokdecode.h"`
- [ ] 文件末尾有换行符

### 元数据完整性

- [ ] `srd_c_decoder.id` 格式为 `"<name>_c"`（如 `"tca6408a_c"`）
- [ ] `srd_c_decoder.name` 格式为 `"<NAME>(C)"`（如 `"TCA6408A(C)"`）
- [ ] `srd_c_decoder.longname` 包含完整名称 + `(C implementation)`
- [ ] `srd_c_decoder.desc` 与 Python 版本一致
- [ ] `srd_c_decoder.license` 与 Python 版本一致
- [ ] `srd_c_decoder.inputs` 为 `{"i2c", NULL}`
- [ ] `srd_c_decoder.channels` 为 `NULL`，`num_channels` 为 `0`
- [ ] `srd_c_decoder.optional_channels` 为 `NULL`，`num_optional_channels` 为 `0`

### 注解标签

- [ ] `ann_labels` 第一列（短标签）为 `""`（空字符串）
- [ ] `ann_labels` 第二列为 Python 版本 annotations 的 id
- [ ] `ann_labels` 第三列为 Python 版本 annotations 的描述
- [ ] 注解数量 `num_annotations` 与 `ann_labels` 数组长度一致
- [ ] 所有注解类都映射到至少一个 annotation_row

### 注解行

- [ ] `annotation_rows` 数量正确
- [ ] 每行的 `classes` 数组包含正确的注解类索引
- [ ] 每行的 `num_classes` 与数组长度一致
- [ ] 行 id 和行名称与 Python 版本一致

### recv_proto() 实现

- [ ] 函数签名正确：`static void xxx_recv_proto(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 通过 `c_decoder_get_private(di)` 获取私有数据，检查非 NULL
- [ ] 使用 `strcmp(cmd, ...)` 匹配 I2C 命令
- [ ] 从 `data[0]` 获取数据字节（需检查 `data_len > 0`）
- [ ] 状态机覆盖所有 Python 版本的状态转换
- [ ] `START` / `START REPEAT` / `STOP` 命令正确处理
- [ ] `ADDRESS READ` / `ADDRESS WRITE` 命令检查从地址
- [ ] `DATA READ` / `DATA WRITE` 命令处理数据字节

### 回调函数

- [ ] `reset()` 使用 `g_malloc0()` 分配私有数据（首次）或 `memset()` 重置
- [ ] `start()` 调用 `c_decoder_register_output(di, SRD_OUTPUT_ANN, "xxx")` 注册输出
- [ ] `decode()` 为空函数（I2C 上层解码器不使用）
- [ ] `destroy()` 使用 `g_free()` 释放私有数据，设置 `c_decoder_set_private(di, NULL)`

### 导出入口

- [ ] `srd_c_decoder_entry()` 函数存在，返回 `&xxx_c_decoder`
- [ ] `srd_c_decoder_api_version()` 函数存在，返回 `SRD_C_DECODER_API_VERSION`
- [ ] 两个函数都有 `SRD_C_DECODER_EXPORT` 前缀
- [ ] 选项的 `def` 和 `values` 在 `srd_c_decoder_entry()` 中初始化

### 注解输出

- [ ] 使用 `C_ANN_PUT(di, ss, es, out_ann, cls, text)` 宏输出注解
- [ ] 注解类索引在合法范围内（0 到 NUM_ANN-1）
- [ ] 文本内容与 Python 版本语义一致

### CMakeLists.txt

- [ ] `C_DECODERS` 列表中添加了 `<name>_c`

---

## tca6408a_c 专项检查

- [ ] 从地址检查：0x20 和 0x21 为合法地址，其他输出警告
- [ ] 4 个寄存器处理函数完整：0x00(Input), 0x01(Output), 0x02(Polarity), 0x03(Config)
- [ ] 寄存器名称输出正确：Input port / Output port / Polarity inversion register / Configuration register
- [ ] 寄存器值格式：`"State of inputs: %02X"`, `"Outputs set: %02X"`, `"Polarity inverted: %02X"`, `"Configuration: %02X"`
- [ ] Repeated Start 后的读取流程正确（WRITE_IO_REGS → READ_IO_REGS → READ_IO_REGS2）
- [ ] 3 个注解类正确映射到 2 个注解行

---

## hdcp_c 专项检查

- [ ] 从地址检查：仅处理 0x3A
- [ ] 20 个注解类正确映射到 3 个注解行（messages 18 个, summaries 1 个, warnings 1 个）
- [ ] `msg_ids` 查找表完整（12 条记录：ID 2-17）
- [ ] `write_items` 查找表完整（17 条记录：偏移 0x00-0x80）
- [ ] RxStatus 解析正确：2 字节组合，bit11=reauth, bit10=ready, bit[9:0]=length
- [ ] Bstatus 解析正确：2 字节组合，bit[6:0]=device_count, bit7=max_devs, bit[10:8]=depth, bit11=max_cascade, bit12=hdmi_mode
- [ ] Read_Message / Write_Message：首字节为消息 ID，查找 msg_ids 输出到对应注解类
- [ ] HDCP2Version：bit2 判断是否 HDCP2
- [ ] WRITE_OFFSET 状态下，偏移 0x10/0x15/0x18/0x60 进入 BUFFER_DATA
- [ ] 空缓冲区时仅输出 type 名称

---

## hdmi_scdc_c 专项检查

- [ ] 从地址检查：0xA8（写）和 0xA9（读）
- [ ] 4 个注解类正确映射到 2 个注解行
- [ ] `verbosity` 选项正确读取（short/long/debug）
- [ ] SCDC 寄存器查找表包含所有 16 个寄存器定义
- [ ] 字段解释表覆盖所有有 fields 定义的寄存器（0x01, 0x02, 0x10, 0x11, 0x20, 0x21, 0x30, 0x40, 0x41）
- [ ] CED 寄存器特殊处理：
  - [ ] 0x50/0x52/0x54 保存低 7 位到 err_det_lower
  - [ ] 0x51/0x53/0x55 组合 2 字节计算 error_counter，通道号 = (offset-0x51)/2
  - [ ] 0x56 输出 Checksum
  - [ ] CED 寄存器后自动 offset += 1
- [ ] 8 状态状态机完整实现
- [ ] Debug 注解在 verbosity=debug 时输出

---

## tmp102_c 专项检查

- [ ] 35 个注解类正确映射到 4 个注解行（bits, regs, info, warnings）
- [ ] `radix` 选项正确读取（Hex/Dec/Oct/Bin）
- [ ] `units` 选项正确读取（Celsius/Fahrenheit/Kelvin）
- [ ] 从地址检查：0x48-0x4B 为合法地址，0x00 为 General Call
- [ ] 温度计算正确：
  - [ ] 12-bit 模式：rawdata >>= 4，>0x07ff 时补码扩展
  - [ ] 13-bit 模式：rawdata >>= 3，>0x0fff 时补码扩展
  - [ ] 除以 16 得到摄氏度
  - [ ] 华氏转换：*9/5+32
  - [ ] 开尔文转换：+273.15
- [ ] 配置寄存器位解析完整：OS, R0/R1, F0/F1, POL, TM, SD, CR0/CR1, AL, EM
- [ ] 4 个数据寄存器处理函数：0x00(Temp), 0x01(Conf), 0x02(TLOW), 0x03(THIGH)
- [ ] General Call Reset (0x06) 处理
- [ ] 2 字节数据收集：首字节保存到 bytes[0]，次字节保存到 bytes[1]（注意字节序）
- [ ] ADDRESS READ 时直接进入 REGISTER_DATA（跳过 REGISTER_ADDRESS）

---

## xfp_c 专项检查

- [ ] 2 个注解类正确映射到 2 个注解行
- [ ] 从地址：0x50（0xA0 写，0xA1 读）
- [ ] 字节计数器 cnt 从 -1 开始，DATA READ 时递增
- [ ] 采样位置数组 sn[256][2] 正确记录每个字节的 [start, end]
- [ ] Lower Memory 映射（0x00-0x7F）处理函数完整
- [ ] High Table 1 映射（0x80-0xFF，仅当 cur_highmem_page==0x01）处理函数完整
- [ ] page_select (offset 0x7F) 更新 cur_highmem_page
- [ ] plugtrx 查找表完整移植：
  - [ ] MODULE_ID
  - [ ] ALARM_THRESHOLDS
  - [ ] AD_READOUTS
  - [ ] GCS_BITS
  - [ ] CONNECTOR
  - [ ] TRANSCEIVER（8×8 二维）
  - [ ] SERIAL_ENCODING
  - [ ] XMIT_TECH
  - [ ] CDR
  - [ ] DEVICE_TECH（4×2 二维）
  - [ ] ENHANCED_OPTS
  - [ ] AUX_TYPES
- [ ] 换算函数正确：
  - [ ] to_temp：16-bit 二进制补码，1/256°C 精度
  - [ ] to_current：0.2μA 精度，输出 mA
  - [ ] to_power：0.1μW 精度，输出 mW
  - [ ] to_wavelength：0.05nm 精度，输出 nm
  - [ ] to_wavelength_tolerance：0.005nm 精度，输出 nm
- [ ] ASCII 字段处理：vendor, vendor_pn, vendor_rev, vendor_sn, maybe_ascii
- [ ] 多字节字段在正确的偏移量触发处理
- [ ] ignore() 处理函数清空缓冲区但不输出注解

---

## 编译与运行时检查

### 编译

- [ ] `build_incremental.cmd` 执行无错误
- [ ] 无编译警告（-Wall -Wextra）
- [ ] 5 个 DLL 文件生成在 `build.dir/decoders/c_decoders/`
  - [ ] `xfp_c.dll`
  - [ ] `hdcp_c.dll`
  - [ ] `hdmi_scdc_c.dll`
  - [ ] `tca6408a_c.dll`
  - [ ] `tmp102_c.dll`

### 运行时

- [ ] PXView 启动无崩溃
- [ ] C 解码器出现在解码器选择列表中
- [ ] 选择 C 解码器后无崩溃
- [ ] 注解输出与 Python 版本语义一致
- [ ] 无内存泄漏（可选：使用 Valgrind/ASan 检查）

### CMakeLists.txt

- [ ] `C_DECODERS` 列表包含所有 5 个新解码器
- [ ] 列表顺序按字母序或按添加时间序
- [ ] 无重复项
