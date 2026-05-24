# 移植检查清单 — Batch 21

## 通用检查项（适用于所有 5 个解码器）

### 文件结构
- [ ] 文件位于 `libsigrokdecode/c_decoders/{id}_c.c`
- [ ] 包含标准头文件: `<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<glib.h>`, `"libsigrokdecode.h"`
- [ ] 版权声明和 License 头部正确

### srd_c_decoder 结构体
- [ ] `.id` 格式为 `"{python_id}_c"`（如 `"rtc8564_c"`）
- [ ] `.name` 格式为 `"{PythonName}(C)"`（如 `"RTC-8564(C)"`）
- [ ] `.longname` 与 Python 版本一致，可加 ` (C)` 后缀
- [ ] `.desc` 与 Python 版本一致，可加 ` (C implementation)` 后缀
- [ ] `.license` 与 Python 版本一致
- [ ] `.channels = NULL`, `.num_channels = 0`
- [ ] `.optional_channels = NULL`, `.num_optional_channels = 0`
- [ ] `.inputs = {"i2c", NULL}`, `.num_inputs = 1`
- [ ] `.outputs` 与 Python 版本一致（有则定义，无则 NULL/0）
- [ ] `.binary = NULL`, `.num_binary = 0`
- [ ] `.tags` 与 Python 版本一致
- [ ] `.num_annotations = NUM_ANN`（枚举最后一个值）

### ann_labels
- [ ] 每个条目第一列为 `""`（空字符串）
- [ ] 第二列为 annotation ID（小写，连字符分隔）
- [ ] 第三列为 annotation 描述
- [ ] 总数与 `NUM_ANN` 一致

### annotation_rows
- [ ] 每个 row 的 `classes` 数组包含正确的 annotation index
- [ ] `num_annotation_rows` 与实际行数一致
- [ ] 所有 annotation classes 都被映射到某个 row（无遗漏）
- [ ] row ID 和名称与 Python 版本一致

### recv_proto 实现
- [ ] 函数签名正确: `void xxx_recv_proto(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 开头获取私有数据: `xxx_state *s = (xxx_state *)c_decoder_get_private(di);`
- [ ] 检查 `s` 非 NULL
- [ ] 处理 `"BITS"` 命令: 直接 return（I2C 上层解码器忽略 BITS 包）
- [ ] 正确提取 `databyte`: `uint8_t databyte = (data_len > 0) ? data[0] : 0;`
- [ ] 状态机覆盖所有 I2C 命令: START, START REPEAT, ADDRESS WRITE, ADDRESS READ, DATA WRITE, DATA READ, ACK, NACK, STOP
- [ ] 使用 `strcmp()` 比较 `cmd` 字符串

### 生命周期回调
- [ ] `reset()`: 分配私有数据（首次）或清零（后续），使用 `g_malloc0()` + `c_decoder_set_private()`
- [ ] `start()`: 调用 `c_decoder_register_output(di, SRD_OUTPUT_ANN, "xxx")` 注册输出
- [ ] `start()`: 读取选项（如有）
- [ ] `decode()`: 空实现 `(void)di;`
- [ ] `destroy()`: `g_free()` 释放私有数据，`c_decoder_set_private(di, NULL)`

### 导出函数
- [ ] `srd_c_decoder_entry()`: 返回 `&xxx_c_decoder` 指针
- [ ] `srd_c_decoder_entry()`: 设置选项默认值（如有选项）
- [ ] `srd_c_decoder_api_version()`: 返回 `SRD_C_DECODER_API_VERSION`
- [ ] 两个函数都有 `SRD_C_DECODER_EXPORT` 前缀

### CMakeLists.txt
- [ ] 在 `C_DECODERS` 列表中添加了解码器名称（不含 `_c` 后缀）

---

## tcs3472x_c 专项检查

- [ ] 定义了 `device_address` 选项，默认值 `"0x29"`，可选值 `"0x29"` 和 `"0x39"`
- [ ] I2C 地址过滤: 使用选项值过滤 ADDRESS WRITE/READ
- [ ] 命令字节解析: bit7=R/W, bit6=自动递增, bit5-0=寄存器地址
- [ ] 寄存器表包含 20 个寄存器
- [ ] 多字节寄存器（16 位值）正确累积低字节 + 高字节
- [ ] ENABLE 寄存器 (0x00) 显示 PON/AEN/WEN/AIEN 位
- [ ] CONTROL 寄存器 (0x0F) 显示 AGAIN 增益 (1x/4x/16x/60x)
- [ ] PERS 寄存器 (0x0C) 显示持久性滤波器值
- [ ] 颜色数据寄存器正确显示 16 位值
- [ ] NUM_ANN = 1
- [ ] 1 个 annotation_row: `registers`

---

## rtc8564_c 专项检查

- [ ] 实现了 `bcd2int()` 辅助函数
- [ ] 6 个状态: IDLE, GET_SLAVE_ADDR, GET_REG_ADDR, WRITE_RTC_REGS, READ_RTC_REGS, READ_RTC_REGS2
- [ ] 寄存器自动递增: 每处理一个字节后 `reg++`
- [ ] handle_reg_0x02: 正确解析 VL bit (bit7) 和 BCD 秒数 (bit6-0)
- [ ] handle_reg_0x03: 正确解析 BCD 分钟
- [ ] handle_reg_0x04: 正确解析 BCD 小时 (bit5-0)
- [ ] handle_reg_0x05: 正确解析 BCD 日 (bit5-0)
- [ ] handle_reg_0x06: 正确解析 BCD 星期 (bit2-0)
- [ ] handle_reg_0x07: 正确解析 Century bit (bit7) 和 BCD 月 (bit4-0)
- [ ] handle_reg_0x08: 正确解析 BCD 年
- [ ] handle_reg_0x01: 输出 TI/TP, AF, TF, AIE, TIE 位描述
- [ ] STOP 时输出完整日期时间: `DD.MM.YY HH:MM:SS`
- [ ] 区分读操作和写操作的日期时间注解
- [ ] NUM_ANN = 16
- [ ] 3 个 annotation_rows: `bits`, `regs`, `date-time`
- [ ] `bits` row 包含 indices 0-8, 11, 12, 13
- [ ] `regs` row 包含 indices 14, 15
- [ ] `date-time` row 包含 indices 9, 10

---

## tpm_tis_i2c_c 专项检查

- [ ] outputs 包含 `"tpm-tis"`（与 Python 一致）
- [ ] 10 个状态覆盖完整 TIS 事务流程
- [ ] TIS 寄存器地址为 1 字节
- [ ] 读路径: START REPEAT → ADDRESS READ → 多个 DATA READ → NACK → STOP
- [ ] 写路径: 多个 DATA WRITE → STOP
- [ ] 数据累积缓冲区足够大（建议 256 字节）
- [ ] hex 格式化正确: 大写，无分隔符
- [ ] Address 注解: `"%02X"` 格式
- [ ] Data Read 注解: hex 数据字符串
- [ ] Data Write 注解: hex 数据字符串
- [ ] Transaction 注解: 包含地址 + 操作方向 + 数据
- [ ] Warning 注解: 意外 I2C 命令时输出
- [ ] 错误恢复: 遇到意外命令重置状态机
- [ ] NUM_ANN = 5
- [ ] 3 个 annotation_rows: `protocol`, `transactions`, `warnings`

---

## st25dv_c 专项检查

- [ ] outputs 包含 `"st25dv"`（与 Python 一致）
- [ ] license 为 `"mit"`（与 Python 一致）
- [ ] 9 个 step 覆盖完整事务流程
- [ ] 2 字节寄存器地址: 先 MSB 后 LSB
- [ ] I2C 地址识别: 0x53 → DATA, 0x57 → SYSTEM
- [ ] 未知 I2C 地址 → error 注解
- [ ] 寄存器定义表包含 30+ 个寄存器
- [ ] 单字节寄存器: 立即格式化字段值
- [ ] 多字节寄存器: 累积数据后格式化
- [ ] I2CPASSWD (0x0900): 17 字节
- [ ] MAILBOX_RAM (0x2008): 256 字节
- [ ] 字段提取: `(value & mask) >> shift`
- [ ] Register address 注解: `"Read/Write XXXX: RegName"`
- [ ] Register value 注解: 字段级描述
- [ ] NUM_ANN = 5
- [ ] 1 个 annotation_row: `regs`

---

## ssd1306_c 专项检查

- [ ] NUM_ANN 精确计算: 51 个 annotation classes
- [ ] 51 个 ann_labels 条目，第一列均为 `""`
- [ ] 4 个 annotation_rows: `bits`, `cmds`, `blockdata`, `warnings`
- [ ] `bits` row: indices 0-9
- [ ] `cmds` row: indices 10-48
- [ ] `blockdata` row: index 49
- [ ] `warnings` row: index 50
- [ ] I2C 地址过滤: 仅接受 0x3C 和 0x3D
- [ ] 控制字节处理: 0x80 → 命令, 0x40 → 数据
- [ ] 命令范围归一化:
  - 0x00-0x0F → 归一化为 0x00
  - 0x10-0x1F → 归一化为 0x10
  - 0x40-0x7F → 归一化为 0x40
  - 0xB0-0xB7 → 归一化为 0xB0
- [ ] 子状态: COMMAND / PARAMETER / PARAMETER2
- [ ] 命令表包含所有 30+ 条命令
- [ ] 每条命令的 has_param 标志正确
- [ ] 参数处理函数覆盖所有需要参数的命令:
  - [ ] 0x00: 低列起始地址
  - [ ] 0x10: 高列起始地址
  - [ ] 0x20: 显示模式
  - [ ] 0x21: 设置列地址
  - [ ] 0x22: 设置页地址
  - [ ] 0x23: 淡出闪烁
  - [ ] 0x26/0x27: 水平滚动
  - [ ] 0x29/0x2A: 垂直水平滚动
  - [ ] 0x81: 对比度
  - [ ] 0x8D: 充电泵
  - [ ] 0xA3: 垂直滚动区域（2 个参数）
  - [ ] 0xA8: 多路复用比
  - [ ] 0xB0: 页起始地址
  - [ ] 0xD3: 垂直偏移
  - [ ] 0xD5: 显示时钟比
  - [ ] 0xD6: 缩放
  - [ ] 0xD9: 预充电周期
  - [ ] 0xDA: COM 引脚
  - [ ] 0xDB: Vcomh 取消选择
- [ ] blockstring 累积和输出
- [ ] GDDRAM 数据写入注解
- [ ] 无效参数的 warning 注解（如无效多路复用比 < 16）
- [ ] tags 包含 `"Display"` 和 `"IC"`

---

## 编译验证

- [ ] `build_incremental.cmd` 成功完成
- [ ] 5 个 DLL 文件生成在 `build.dir/decoders/c_decoders/` 目录
- [ ] PXView 启动无错误
- [ ] 解码器列表中显示 5 个新的 C 解码器

## 运行时验证

### tcs3472x_c
- [ ] I2C + tcs3472x_c 堆叠加载成功
- [ ] 寄存器读写注解正确显示
- [ ] device_address 选项可切换

### rtc8564_c
- [ ] I2C + rtc8564_c 堆叠加载成功
- [ ] 时间寄存器解析正确
- [ ] 日期时间注解格式正确
- [ ] 读/写操作区分正确

### tpm_tis_i2c_c
- [ ] I2C + tpm_tis_i2c_c 堆叠加载成功
- [ ] TIS 事务注解正确
- [ ] 读/写数据 hex 格式化正确
- [ ] Transaction 注解包含完整信息

### st25dv_c
- [ ] I2C + st25dv_c 堆叠加载成功
- [ ] 2 字节寄存器地址解析正确
- [ ] DATA/SYSTEM 地址区分正确
- [ ] 寄存器字段解析正确

### ssd1306_c
- [ ] I2C + ssd1306_c 堆叠加载成功
- [ ] 控制字节解析正确
- [ ] 命令解析和参数处理正确
- [ ] GDDRAM 数据注解正确
- [ ] 无效参数 warning 正确显示
