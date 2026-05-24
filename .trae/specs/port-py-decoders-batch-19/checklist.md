# 验证清单 — Batch 19: I2C 上层解码器移植

## 通用验证项（适用于所有 5 个解码器）

### 文件结构
- [ ] 文件名格式正确：`{decoder_id}_c.c`
- [ ] 文件位于 `libsigrokdecode/c_decoders/` 目录
- [ ] 包含标准头文件：`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<glib.h>`, `"libsigrokdecode.h"`
- [ ] 文件末尾无多余空行

### srd_c_decoder 结构体
- [ ] `.id` 格式为 `"{python_id}_c"` (如 `"edid_c"`, `"i2c_packet_c"`)
- [ ] `.name` 格式为 `"{PythonName}(C)"` (如 `"Edid(C)"`, `"I2c_packet(C)"`)
- [ ] `.longname` 包含 `(C)` 后缀
- [ ] `.desc` 包含 `(C implementation)` 后缀
- [ ] `.license` 与 Python 版一致
- [ ] `.inputs` 为 `{"i2c", NULL}`
- [ ] `.num_inputs` 为 1
- [ ] `.channels` 为 NULL，`.num_channels` 为 0
- [ ] `.optional_channels` 为 NULL，`.num_optional_channels` 为 0
- [ ] `.recv_proto` 指向正确的回调函数
- [ ] `.decode` 指向空函数（仅 `(void)di;`）
- [ ] `.reset` 正确初始化/重置私有状态
- [ ] `.start` 注册输出、读取选项
- [ ] `.destroy` 释放私有状态内存

### Annotation 定义
- [ ] `ann_labels` 首列为 `""` (空字符串)
- [ ] `ann_labels` 第二列为小写英文 id
- [ ] `ann_labels` 第三列为英文描述
- [ ] `NUM_ANN` 枚举值与 `ann_labels` 数组大小一致
- [ ] 所有 annotation class 都映射到某个 row
- [ ] `ann_rows` 中 `ann_classes` 数组以 `-1` 结尾
- [ ] `ann_rows` 中 `num_ann_classes` 计数正确（不含 -1 终止符）

### 导出函数
- [ ] `srd_c_decoder_entry()` 返回 `&xxx_c_decoder` 指针
- [ ] `srd_c_decoder_entry()` 初始化所有 options 的 GVariant 默认值
- [ ] `srd_c_decoder_entry()` 中 `g_variant_new_string()` / `g_variant_new_int64()` 调用正确
- [ ] `srd_c_decoder_api_version()` 返回 `SRD_C_DECODER_API_VERSION`
- [ ] 两个函数都有 `SRD_C_DECODER_EXPORT` 前缀

### recv_proto 实现
- [ ] 函数签名正确：`(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 从 `c_decoder_get_private(di)` 获取私有状态，检查 NULL
- [ ] 使用 `strcmp()` 比较 cmd 字符串
- [ ] 从 data 提取 databyte 时检查 `data && data_len > 0`
- [ ] 状态机完整，所有 I2C 事件都有处理
- [ ] `STOP` 事件正确重置状态

### 内存安全
- [ ] `reset()` 中使用 `g_malloc0()` 分配私有状态（首次）
- [ ] `destroy()` 中使用 `g_free()` 释放私有状态
- [ ] `destroy()` 中调用 `c_decoder_set_private(di, NULL)`
- [ ] 所有 `snprintf()` 调用检查缓冲区大小
- [ ] 无栈上大数组（>1KB 的数组使用堆分配或确保足够栈空间）

---

## ltc26x7_c 专项验证

### 元数据一致性
- [ ] `.id = "ltc26x7_c"`
- [ ] `.name = "Ltc26x7(C)"`
- [ ] `.longname = "Linear Technology LTC26x7 (C)"`
- [ ] `.desc` 包含 "C implementation"
- [ ] `.license = "gplv2+"`
- [ ] `.tags = {"IC", "Analog/digital", NULL}`，`.num_tags = 2`
- [ ] `.num_annotations = 5`

### Annotation 映射
- [ ] ANN_SLAVE_ADDR=0, ANN_COMMAND=1, ANN_ADDRESS=2, ANN_DAC_A_VOLTAGE=3, ANN_DAC_B_VOLTAGE=4
- [ ] addr_cmd row 包含 class 0,1,2
- [ ] dac_a_voltages row 包含 class 3
- [ ] dac_b_voltages row 包含 class 4

### Options
- [ ] chip option: string 类型，默认 "ltc2607"，值列表 ("ltc2607","ltc2617","ltc2627")
- [ ] vref option: double 类型，默认 1.5

### 解码逻辑
- [ ] 状态机：IDLE → GET_SLAVE_ADDR → GET_CMD_ADDR → WRITE_DATA
- [ ] 全局地址 0x73 特殊处理
- [ ] 三进制地址转换正确（CA2/CA1/CA0 → GND/FLOAT/VCC）
- [ ] 命令高4位解析：0x00/0x01/0x03/0x04/0x0F
- [ ] 地址低4位解析：0x00=DAC A, 0x01=DAC B, 0x0F=All DACs
- [ ] 2字节数据累积：先收高字节，后收低字节
- [ ] ltc2607: 16-bit, 除以 0xFFFF
- [ ] ltc2617: 14-bit (右移2), 除以 0x3FFF
- [ ] ltc2627: 12-bit (右移4), 除以 0x0FFF
- [ ] All DACs (0x0F) 时同时输出 DAC A 和 DAC B 电压
- [ ] 电压格式：`"%.6fV"` 和 `"%.2fV"`

---

## i2cfilter_c 专项验证

### 元数据一致性
- [ ] `.id = "i2cfilter_c"`
- [ ] `.name = "I2cfilter(C)"`
- [ ] `.outputs = {"i2c", NULL}`，`.num_outputs = 1`
- [ ] `.tags = {"Util", NULL}`，`.num_tags = 1`
- [ ] `.num_annotations = 0`，`.ann_labels = NULL`
- [ ] `.num_annotation_rows = 0`，`.annotation_rows = NULL`

### Options
- [ ] address option: int 类型，默认 0
- [ ] direction option: string 类型，默认 "both"，值列表 ("read","write","both")

### 解码逻辑
- [ ] 缓存所有 I2C 包直到 STOP/START REPEAT
- [ ] ADDRESS READ/WRITE 时记录 curslave 和 curdirection
- [ ] address=0 时不过滤地址
- [ ] address!=0 时只转发匹配地址的包
- [ ] direction="both" 时不过滤方向
- [ ] direction="read" 时只转发 ADDRESS READ 段
- [ ] direction="write" 时只转发 ADDRESS WRITE 段
- [ ] 不匹配时清空缓存（不输出任何数据）
- [ ] 匹配时通过 `c_decoder_put_python()` 转发所有缓存包
- [ ] 转发后清空缓存

### Python 输出
- [ ] `out_python` 使用 `c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "i2c")` 注册
- [ ] proto_id 为 `"i2c"`（与 Python 版 `proto_id='i2c'` 一致）

---

## i2cdemux_c 专项验证

### 元数据一致性
- [ ] `.id = "i2cdemux_c"`
- [ ] `.name = "I2cdemux(C)"`
- [ ] `.outputs = NULL`，`.num_outputs = 0`（动态创建）
- [ ] `.tags = {"Util", NULL}`，`.num_tags = 1`
- [ ] `.num_annotations = 0`，`.ann_labels = NULL`
- [ ] `.num_annotation_rows = 0`，`.annotation_rows = NULL`

### 解码逻辑
- [ ] 缓存所有 I2C 包直到 STOP
- [ ] ADDRESS READ/WRITE 时查找或创建输出流
- [ ] 新地址时调用 `c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "i2c-0xNN")`
- [ ] proto_id 格式为 `"i2c-0x%02x"`（与 Python 版 `hex(databyte)` 一致）
- [ ] STOP 时将所有缓存包转发到对应输出流
- [ ] 转发后清空缓存和 stream 索引
- [ ] MAX_SLAVES 限制合理（建议 128）

### 动态输出流安全
- [ ] 验证 `c_decoder_register_output()` 在 `recv_proto()` 回调中调用是否安全
- [ ] 如不安全，改为在 `start()` 中预注册或使用其他机制

---

## i2c_packet_c 专项验证

### 元数据一致性
- [ ] `.id = "i2c_packet_c"`
- [ ] `.name = "I2c_packet(C)"`
- [ ] `.license = "mit"`（注意：与 Python 版一致，不是 gplv2+）
- [ ] `.tags = {"Embedded/industrial", NULL}`，`.num_tags = 1`
- [ ] `.num_annotations = 1`

### Annotation 映射
- [ ] ANN_DATA=0
- [ ] packet row 包含 class 0

### Options
- [ ] format option: string 类型，默认 "hex"，值列表 ("ascii","dec","hex","oct","bin")

### 解码逻辑
- [ ] DATA READ/DATA WRITE → 追加到 packet_data
- [ ] START → 先输出当前包（如有），记录新包起始
- [ ] START REPEAT → 输出当前包（start_repeat=true），合并字符串
- [ ] ADDRESS READ/WRITE → 记录地址和方向
- [ ] *ACK → 更新 packet_es
- [ ] STOP → 输出最终包

### Annotation 格式
- [ ] 单段包：`"0x50 RD: 48 65 6C 6C 6F"` (hex 格式)
- [ ] 合并包：`"0x50 WR: 00 [SR] 0x50 RD: 48 65 6C 6C 6F"`
- [ ] 短格式：去除 "0x" 前缀

### Python 输出
- [ ] 注册 `SRD_OUTPUT_ANN` + `SRD_OUTPUT_PYTHON`
- [ ] PACKET READ/WRITE 格式正确
- [ ] TRANSACTION END 格式正确
- [ ] 数据格式化根据 format 选项正确

---

## edid_c 专项验证

### 元数据一致性
- [ ] `.id = "edid_c"`
- [ ] `.name = "Edid(C)"`
- [ ] `.license = "gplv3+"`（注意：不是 gplv2+）
- [ ] `.tags = {"Display", "Memory", "PC", NULL}`，`.num_tags = 3`
- [ ] `.num_annotations = 2`

### Annotation 映射
- [ ] ANN_FIELDS=0, ANN_SECTIONS=1
- [ ] sections row 包含 class 1
- [ ] fields row 包含 class 0

### 常量定义
- [ ] EDID_HEADER = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00}
- [ ] OFF_VENDOR=8, OFF_VERSION=18, OFF_BASIC=20, OFF_CHROM=25
- [ ] OFF_EST_TIMING=35, OFF_STD_TIMING=38, OFF_DET_TIMING=54
- [ ] OFF_NUM_EXT=126, OFF_CHECKSUM=127

### est_modes 数组
- [ ] 17 种预设时序模式字符串完整
- [ ] 顺序与 Python 版一致

### xy_ratio 数组
- [ ] 4 种宽高比：(16,10), (4,3), (5,4), (16,9)

### 解码逻辑
- [ ] ADDRESS WRITE + 0x50 → offset 状态
- [ ] DATA WRITE + offset → 记录偏移，计算 extension 和 cnt
- [ ] ADDRESS READ + 0x50 → header 或 extensions 状态
- [ ] DATA READ → 逐字节接收，触发各段解析
- [ ] EDID 头部检测：8 字节匹配 EDID_HEADER
- [ ] 头部前垃圾数据丢弃
- [ ] cnt=18 时解析 Vendor/product
- [ ] cnt=20 时解析 EDID Version
- [ ] cnt=25 时解析 Basic display
- [ ] cnt=35 时解析 Color characteristics
- [ ] cnt=38 时解析 Established timings
- [ ] cnt=54 时解析 Standard timings
- [ ] cnt=126 时解析 Detailed timing descriptors
- [ ] cnt=127 时输出 Extensions present
- [ ] cnt=128 时校验 Checksum

### 扩展块处理
- [ ] ext_cache 最多支持 4 个扩展块
- [ ] 扩展块 cnt=1 时检查 Tag
- [ ] 扩展块 cnt=2 时输出 Version
- [ ] 扩展块 cnt=3 时输出 DTD offset
- [ ] 扩展块 cnt=4 时输出 Format support | DTD count
- [ ] 扩展块数据块集合解析
- [ ] 扩展块 DTD 解析
- [ ] 扩展块 Padding 和 Checksum

### PNPID 处理
- [ ] PNPID 3 字符解码算法正确
- [ ] 厂商名称查找（内嵌或文件）
- [ ] 找不到厂商名时仅输出 PNPID 代码

### 子函数验证
- [ ] `decode_vid()` — 位操作正确提取 3 个 5-bit 字符
- [ ] `decode_pid()` — 2 字节产品 ID 格式化
- [ ] `decode_serial()` — 4 字节序列号，字母数字检测
- [ ] `decode_mfrdate()` — 周+年格式化
- [ ] `decode_basicdisplay()` — 数字/模拟输入、尺寸、gamma、DPMS、特征
- [ ] `decode_chromaticity()` — 10-bit 色度值转浮点，红绿蓝白 4 组
- [ ] `decode_est_timing()` — 17 位 bitmap 解码
- [ ] `decode_std_timing()` — 8 个 2 字节条目，宽高比和刷新率
- [ ] `decode_detailed_timing()` — 18 字节详细时序（像素时钟、水平/垂直、同步、尺寸、边框、特征）
- [ ] `decode_descriptor()` — tag 0xFF/0xFE/0xFC/0xFD/0xFB/0xFA 处理
- [ ] `decode_descriptors()` — 4 个连续 18 字节描述符块
- [ ] `decode_data_block()` — CEA 扩展 tag 0-7 处理
- [ ] `decode_data_block_collection()` — 数据块集合遍历

---

## 编译验证

### 增量编译
- [ ] `build_incremental.cmd` 执行成功
- [ ] 无编译错误
- [ ] 无编译警告（或仅有可接受的警告）

### DLL 生成
- [ ] `build.dir/decoders/c_decoders/edid_c.dll` 存在
- [ ] `build.dir/decoders/c_decoders/i2c_packet_c.dll` 存在
- [ ] `build.dir/decoders/c_decoders/i2cdemux_c.dll` 存在
- [ ] `build.dir/decoders/c_decoders/i2cfilter_c.dll` 存在
- [ ] `build.dir/decoders/c_decoders/ltc26x7_c.dll` 存在

### 运行时验证
- [ ] PXView.exe 正常启动
- [ ] 新解码器出现在解码器列表中
- [ ] 解码器名称显示正确（含 "(C)" 后缀）
- [ ] 选择 I2C 上层解码器时能正确堆叠在 i2c_c 之上

### 功能验证（如有测试数据）
- [ ] ltc26x7_c: 能正确解析 LTC2607/LTC2617/LTC2627 的 I2C 通信
- [ ] i2cfilter_c: 能正确过滤指定地址/方向的数据
- [ ] i2cdemux_c: 能正确按从设备地址分发数据流
- [ ] i2c_packet_c: 能正确组装数据包并格式化输出
- [ ] edid_c: 能正确解析 EDID 基础块和扩展块

---

## CMakeLists.txt 验证

- [ ] C_DECODERS 列表中添加了 `edid_c`
- [ ] C_DECODERS 列表中添加了 `i2c_packet_c`
- [ ] C_DECODERS 列表中添加了 `i2cdemux_c`
- [ ] C_DECODERS 列表中添加了 `i2cfilter_c`
- [ ] C_DECODERS 列表中添加了 `ltc26x7_c`
- [ ] 添加位置在列表末尾，格式与现有条目一致
