# 验证清单 — Batch 18: 5 个 I2C 上层解码器移植

## 通用验证项（每个解码器都必须通过）

### 文件结构
- [ ] 文件名格式正确: `{id}_c.c` (如 `bh1750_c.c`)
- [ ] 文件位于 `libsigrokdecode/c_decoders/` 目录
- [ ] 包含正确的头文件: `stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`

### srd_c_decoder 结构体
- [ ] `.id` 格式: `"{python_id}_c"` (如 `"bh1750_c"`)
- [ ] `.name` 格式: `"{PythonName}(C)"` (如 `"BH1750(C)"`)
- [ ] `.longname` 包含 `(C)` 后缀
- [ ] `.desc` 末尾包含 `(C implementation)`
- [ ] `.channels = NULL`, `.num_channels = 0`
- [ ] `.optional_channels = NULL`, `.num_optional_channels = 0`
- [ ] `.inputs` 包含 `{"i2c", NULL}`, `.num_inputs = 1`
- [ ] `.outputs = NULL`, `.num_outputs = 0` (除非 Python 版本有 output)
- [ ] `.decode` 指向空函数: `static void xxx_decode(struct srd_decoder_inst *di) { (void)di; }`
- [ ] `.recv_proto` 指向实现的回调函数
- [ ] `.reset` 正确分配/清零私有数据
- [ ] `.start` 注册输出并读取选项
- [ ] `.destroy` 释放私有数据 (`g_free` + `c_decoder_set_private(di, NULL)`)

### 注释标签
- [ ] `ann_labels` 第一列为 `""` (空字符串)
- [ ] `ann_labels` 第二列为短名 (如 `"celsius"`)
- [ ] `ann_labels` 第三列为长描述 (如 `"Temperature in degrees Celsius"`)
- [ ] `NUM_ANN` 枚举值正确，等于注释总数
- [ ] 所有注释 class 都映射到某个 annotation_row

### annotation_rows
- [ ] 每个 row 的 `classes` 数组包含正确的 class index
- [ ] `num_annotation_rows` 与数组长度一致
- [ ] row id 和 label 与 Python 版本一致

### 选项
- [ ] 选项 `id` 与 Python 版本一致
- [ ] 选项 `desc` / `idn` 正确设置
- [ ] `srd_c_decoder_entry()` 中初始化了所有选项的 `def` (GVariant)
- [ ] `srd_c_decoder_entry()` 中初始化了所有选项的 `values` (GSList)
- [ ] `start()` 中通过 `c_decoder_get_option_string/int` 读取选项

### recv_proto 回调
- [ ] 函数签名正确: `void (*recv_proto)(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 开头获取私有数据: `xxx_state *s = (xxx_state *)c_decoder_get_private(di);`
- [ ] 检查 `s` 非 NULL
- [ ] 更新 `s->ss = start_sample; s->es = end_sample;`
- [ ] 忽略 `"BITS"` cmd (C 上层解码器不处理)
- [ ] 正确处理所有 I2C cmd: `"START"`, `"START REPEAT"`, `"ADDRESS READ"`, `"ADDRESS WRITE"`, `"DATA READ"`, `"DATA WRITE"`, `"STOP"`, `"ACK"`, `"NACK"`
- [ ] 状态机完整，所有状态都有处理
- [ ] STOP 条件正确重置状态机

### 注册输出
- [ ] `c_decoder_register_output(di, SRD_OUTPUT_ANN, "{id}")` 在 `start()` 中调用
- [ ] 如有 binary 输出: `c_decoder_register_output(di, SRD_OUTPUT_BINARY, "binary")`
- [ ] 如有 python 输出: `c_decoder_register_output(di, SRD_OUTPUT_PYTHON, "{id}")`

### 导出函数
- [ ] `SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)` 存在
- [ ] `SRD_C_DECODER_EXPORT int srd_c_decoder_api_version(void)` 存在，返回 `SRD_C_DECODER_API_VERSION`

---

## bh1750_c 专项验证

- [ ] Slave 地址验证: 0x23 (GND) 和 0x5C (VCC)
- [ ] MTreg 默认值: 69
- [ ] MTreg 高位更新: `mtreg = (reg << 5) | (mtreg & 0x1F)` (清除高位后设置)
- [ ] MTreg 低位更新: `mtreg = (mtreg & 0xE0) | (reg & 0x1F)` (清除低位后设置)
- [ ] 光照计算: `light = rawdata * sensitivity`
- [ ] 灵敏度计算: `sensitivity = (1/accuracy) * 69 / mtreg`
- [ ] 双分辨率模式 (0x11/0x21) 灵敏度减半
- [ ] 读取数据为大端: `regword = (bytes[1] << 8) + bytes[0]` (注意 Python 中 bytes[0] 是后收到的字节)
- [ ] 选项 `radix` 正确读取并用于格式化
- [ ] 选项 `params` 正确读取并影响精度参数
- [ ] 测量模式寄存器 (0x10-0x23) 更新 `mode` 变量
- [ ] START REPEAT 正确处理（继续传输）
- [ ] 无数据传输（仅地址）输出 "Slave presence check"

---

## atsha204a_c 专项验证

- [ ] TX 帧解析: Word Address + Count + Opcode + Param1 + Param2[2] + Data[N] + CRC[2]
- [ ] RX 帧解析: Count + Data/CRC
- [ ] Word Address 映射: 0x00=RESET, 0x01=SLEEP, 0x02=IDLE, 0x03=COMMAND
- [ ] 帧长度验证: `len(bytes) - 1 != count` 时输出 Warning
- [ ] 所有 20 个 Opcode 正确映射到名称
- [ ] Param1 格式化: 12 种不同格式（Mode/Zone/Random/Selector/Encrypted 等）
- [ ] Param2 格式化: 9 种不同格式（TargetKey/KeyID/SlotID/Address/Zero/Summary/NewValue 等）
- [ ] Data 格式化: 8 种不同格式
  - CheckMac: ClientChal[32] + ClientResp[32] + OtherData[13]
  - DeriveKey: MAC[32]
  - ECDH: Pub X[32] + Pub Y[32]
  - GenDig/GenKey: OtherData[4]
  - MAC: Challenge[32]
  - PrivWrite: Value + MAC (条件判断)
  - Verify: ECDSA R[32] + S[32] + 可选 OtherData[19] 或 Pub X/Y[32+32]
  - Write: Value + MAC (条件判断)
- [ ] CRC 格式化: 2 字节十六进制
- [ ] Status 解析: 6 种状态码
- [ ] RX 时重置 opcode 为 -1（避免影响响应显示）
- [ ] 字节缓冲区足够大 (256 字节)
- [ ] STOP 时正确清空缓冲区

---

## ad5593r_c 专项验证

- [ ] Slave 地址验证: 0x10 或 0x11
- [ ] Pointer Byte 解析: opcode 在 bits[4:7]
- [ ] Pointer Byte opcode 映射: 0x00=CONFIG_MODE, 0x01=DAC_WR, 0x04=ADC_RD, 0x05=DAC_RD, 0x06=GPIO_RD, 0x07=REG_RD
- [ ] `databyte_register` 正确设置: 根据 Pointer Byte 类型确定后续数据解析方式
- [ ] 16-bit 数据: 先高字节后低字节
- [ ] 数据连续传输: DATA HIGH → DATA LOW → DATA HIGH → ...
- [ ] 所有 CONFIG_MODE_BITS 映射正确 (14 个)
- [ ] 所有 REG_SEL_RD 映射正确 (13 个)
- [ ] 寄存器字段解析:
  - bit_indices(): 提取置位位索引
  - disabled_enabled(): 0=Disabled, 1=Enabled
  - vref_range(): 0V-Vref / 0V-2xVref
  - dac_chn()/adc_chn(): DAC0/ADC0 格式
- [ ] Vref 选项正确读取 (double, default 2.5)
- [ ] GPIO_RD_POINTER 根据 R/W 方向选择 GPIO_INPUT/GPIO_OUTPUT
- [ ] ADDRESS READ 时跳过 Pointer Byte 直接进入 DATA HIGH

---

## adxl345_c 专项验证 (SPI→I2C 适配)

- [ ] I2C 地址: 0x53 (7-bit)
- [ ] Register Address Byte 格式: bit7=R/W, bit6=MB, bit5:0=Address
- [ ] 写操作: ADDRESS WRITE → Reg Addr Byte → Data Byte(s)
- [ ] 读操作: ADDRESS WRITE → Reg Addr Byte → START REPEAT → ADDRESS READ → Data Byte(s)
- [ ] R/W 位和 MB 位正确注释
- [ ] 寄存器地址自动递增 (MB=1 时)
- [ ] 缩放因子正确:
  - THRESH_TAP: 62.5 mg/LSB
  - OFSX/Y/Z: 15.6 mg/LSB
  - DUR: 0.625 ms/LSB
  - Latent: 1.25 ms/LSB
  - Window: 1.25 ms/LSB
  - TIME_INACT: 1 s/LSB
  - TIME_FF: 5 ms/LSB
- [ ] 16-bit 轴数据 (0x32-0x37): 两字节组合，低字节在前
- [ ] BW_RATE 速率码映射 (16 个值)
- [ ] FIFO 模式映射 (4 个值)
- [ ] 位域解码 (ACT_INACT_CTL, TAP_AXES 等)
- [ ] DATA_FORMAT 中的 g 量程计算: `2^(range_g+1)`
- [ ] POWER_CTL 中的唤醒频率: `2^(~wakeup & 0x03)`
- [ ] 错误消息在条件满足时输出

---

## eeprom24xx_c 专项验证

- [ ] 19 个状态全部实现
- [ ] ACK/NACK 包正确处理
- [ ] 控制字注释: 控制码位 + 地址引脚位 + R/W 位
- [ ] 地址引脚数量根据芯片配置动态调整 (`chip->addr_pins`)
- [ ] 字地址: 支持 1 字节和 2 字节地址模式 (`chip->addr_bytes`)
- [ ] 地址计数器: 每次数据字节后自增
- [ ] 5 种操作类型正确识别和注释:
  - Byte Write: 1 个数据字节
  - Page Write: 2+ 个数据字节，检查 page_size 和 page boundary
  - Current Address Read: 无字地址，1 个数据字节
  - Random Read: 有字地址，1 个数据字节
  - Sequential Random Read: 有字地址，2+ 个数据字节
- [ ] Page Write 警告:
  - 超过 page_size 时输出警告
  - 跨页边界时输出警告
- [ ] 芯片配置表: 15 个芯片配置完整
- [ ] `chip` 选项: 默认 "generic"，所有芯片 key 作为 values
- [ ] `addr_counter` 选项: 默认 0
- [ ] Binary 输出: 使用 `c_decoder_put_binary()` 输出 EEPROM 数据
- [ ] `hexbytes()` 格式化正确
- [ ] `addr_and_len()` 格式化正确
- [ ] START REPEAT 正确触发随机读流程
- [ ] NACK 正确终止读操作
- [ ] STOP 后正确重置所有标志

---

## 编译验证

- [ ] `build_incremental.cmd` 无编译错误
- [ ] 无编译警告 (warning)
- [ ] 5 个 DLL 文件生成在 `build.dir/decoders/c_decoders/`:
  - [ ] `bh1750_c.dll`
  - [ ] `atsha204a_c.dll`
  - [ ] `ad5593r_c.dll`
  - [ ] `adxl345_c.dll`
  - [ ] `eeprom24xx_c.dll`
- [ ] `install.dir/bin/PXView.exe` 正常运行
- [ ] 在 PXView 中可以选择并使用新的 C 解码器

---

## 功能验证（如有测试信号）

### bh1750
- [ ] 写入 Power Down 命令 (0x00) → 正确注释
- [ ] 写入 Continuous H-Res 模式 (0x10) → 正确注释
- [ ] 读取光照数据 → 正确计算 lux 值
- [ ] 修改 MTreg → 灵敏度正确更新

### atsha204a
- [ ] 发送 Command (Word Addr=0x03) → 正确解析帧
- [ ] Read 命令 → Param1/Param2 正确格式化
- [ ] 接收 Status 响应 → 正确显示状态
- [ ] 帧长度不匹配 → 输出 Warning

### ad5593r
- [ ] 写入 Pointer Byte → 正确识别操作类型
- [ ] 写入 16-bit DAC 数据 → 正确解析
- [ ] 读取 ADC 结果 → 正确解析 ADC_DATA + ADC_ADDR
- [ ] 无效 Slave 地址 → 输出 Warning

### adxl345
- [ ] I2C 写寄存器 → 正确解析 Reg Addr Byte
- [ ] I2C 读寄存器 → START REPEAT 流程正确
- [ ] 读取 XYZ 数据 → 16-bit 组合正确
- [ ] BW_RATE 寄存器 → 速率码正确映射

### eeprom24xx
- [ ] Byte Write → 正确识别和注释
- [ ] Page Write → 正确识别，page boundary 检查
- [ ] Random Read → 两阶段流程正确
- [ ] Current Address Read → 正确识别
- [ ] Binary 输出 → 数据正确
