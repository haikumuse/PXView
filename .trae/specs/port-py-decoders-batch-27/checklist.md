# SPI上层协议解码器移植验收清单 (Batch-27)

## 通用验收项 (所有5个解码器)

### G1. 文件命名与位置
- [ ] 文件位于 `libsigrokdecode/c_decoders/` 目录
- [ ] 文件名格式: `{decoder_id}_c.c` (将`-`替换为`_`)
- [ ] 文件编码: UTF-8

### G2. 编译通过
- [ ] `build_incremental.cmd` 编译无error
- [ ] 无compiler warning (或仅有已知的无害warning)
- [ ] 生成DLL文件在 `build.dir/decoders/c_decoders/` 目录

### G3. srd_c_decoder结构体完整性
- [ ] `.id` 格式为 `xxx_c` (与Python的`xxx`对应，加`_c`后缀)
- [ ] `.name` 格式为 `XXX(C)` (中英文均可，但须标注C实现)
- [ ] `.longname` 包含完整描述
- [ ] `.desc` 描述C实现
- [ ] `.license` 与Python原版一致
- [ ] `.channels` / `.num_channels` — 上层解码器均为 `NULL, 0`
- [ ] `.optional_channels` / `.num_optional_channels` — 均为 `NULL, 0`
- [ ] `.options` / `.num_options` — 与Python原版一致
- [ ] `.num_annotations` = `NUM_ANN`
- [ ] `.ann_labels` 第一列为空字符串 `""`
- [ ] `.num_annotation_rows` 正确
- [ ] `.annotation_rows` — 所有annotation class必须映射到某个row
- [ ] `.inputs` = `{"spi", NULL}`, `.num_inputs` = 1
- [ ] `.outputs` — 与Python原版一致(可能为NULL)
- [ ] `.tags` — 与Python原版一致，以NULL结尾
- [ ] `.binary` / `.num_binary` — 与Python原版一致
- [ ] `.reset` 函数指针
- [ ] `.start` 函数指针
- [ ] `.decode` 函数指针 (空函数体)
- [ ] `.destroy` 函数指针
- [ ] `.recv_proto` 函数指针 (核心!)
- [ ] `.metadata` 可选，上层解码器通常不需要

### G4. 入口函数
- [ ] `srd_c_decoder_entry()` 返回解码器结构体指针
- [ ] `srd_c_decoder_api_version()` 返回 `SRD_C_DECODER_API_VERSION`
- [ ] 两个函数均有 `SRD_C_DECODER_EXPORT` 前缀
- [ ] Options的def值在 `srd_c_decoder_entry()` 中初始化
- [ ] Options的values列表在 `srd_c_decoder_entry()` 中初始化

### G5. recv_proto()实现
- [ ] 函数签名: `void (*recv_proto)(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 正确处理 `"DATA"` 包: 提取mosi/miso字节值
- [ ] 正确处理 `"CS-CHANGE"` 包: 检测CS#上升沿(释放)
- [ ] 忽略 `"BITS"` 包(上层解码器通常不需要bit级信息)
- [ ] 忽略 `"TRANSFER"` 包(除非解码器需要)

### G6. 内存管理
- [ ] `reset()` 中使用 `g_malloc0()` 分配私有状态
- [ ] `destroy()` 中使用 `g_free()` 释放私有状态
- [ ] 无内存泄漏
- [ ] 无use-after-free

### G7. Annotation输出规范
- [ ] 使用 `C_ANN_PUT()` 宏输出annotation
- [ ] 每个annotation class至少有一个对应的row
- [ ] annotation文本不包含emoji
- [ ] ss/es参数合理(start_sample <= end_sample)

### G8. CMakeLists.txt集成
- [ ] 解码器名已添加到 `C_DECODERS` 列表
- [ ] DLL安装目标正确

---

## tpm_tis_spi_c 专项验收

### T1. 元数据正确性
- [ ] `.id` = `"tpm_tis_spi_c"`
- [ ] `.name` = `"TPM TIS 2.0 SPI(C)"`
- [ ] `.outputs` = `{"tpm-tis", NULL}`, `.num_outputs` = 1
- [ ] `.license` = `"gplv3+"` (注意与Python原版一致，不是gplv2+)
- [ ] 6个annotation: RW_LENGTH, ADDRESS, WAIT_STATE, DATA, TRANSACTION, WARNING

### T2. 状态机完整性
- [ ] GET_RW_LENGTH → GET_ADDR_BYTE2 → GET_ADDR_BYTE1 → GET_ADDR_BYTE0 → GET_DATA → (循环GET_DATA) → 回到GET_RW_LENGTH
- [ ] reading标志正确: `mosi & 0x80 == 0x80`
- [ ] length计算: `(mosi & 0x7F) + 1`
- [ ] 地址组装: `addr = byte2<<16 | byte1<<8 | byte0`
- [ ] wait_state检测: `addr_byte0的miso == 0`

### T3. Python输出
- [ ] 注册 `SRD_OUTPUT_PYTHON` 输出 `"tpm-tis"`
- [ ] Transaction完成时发送 `"TRANSACTION"` python包

### T4. Duplex Warning
- [ ] Read时: mosi != 0 → 输出ANN_WARNING "unexpected duplex operation"
- [ ] Write时: miso != 0 → 输出ANN_WARNING (addr0的miso==1是允许的例外)

### T5. _finish_annotations逻辑
- [ ] 按从长到短排序annotation文本
- [ ] 去除比前一个更长的后续annotation

---

## st25r39xx_spi_c 专项验收

### S1. 元数据正确性
- [ ] `.id` = `"st25r39xx_spi_c"`
- [ ] `.name` = `"ST25R39xx(C)"`
- [ ] `.outputs` = `NULL`, `.num_outputs` = 0
- [ ] 11个annotation: BURST_READ/WRITE/READB/WRITEB/READT/WRITET, DIRECTCMD, FIFO_WRITE/READ, STATUS, WARN
- [ ] 5个annotation rows: regs, cmds, data, status, warnings

### S2. 寄存器查找表
- [ ] regsSpaceA: 至少包含0x00-0x3F共64个条目 + 4个特殊地址(0xA0/0xA8/0xAC/0xBF)
- [ ] regsSpaceB: 至少包含14个条目
- [ ] regsTest: 至少包含1个条目(0x01: ANTSTOBS)
- [ ] dirCmd: 至少包含35个条目

### S3. 命令解析
- [ ] Space A Write: `0x00-0x3F` → CMD_WRITE, addr=mosi&0x3F
- [ ] Space A Read: `0x40-0x7F` → CMD_READ, addr=mosi&0x3F
- [ ] FIFO Write: `0x80` → CMD_FIFO_WRITE
- [ ] FIFO Read: `0x9F` → CMD_FIFO_READ
- [ ] Special Write: `0xA0/0xA8/0xAC` → CMD_WRITE
- [ ] Special Read: `0xBF` → CMD_READ
- [ ] Direct Command: `0xC0-0xE8` → CMD_DIRECT
- [ ] Space B: `0xFB` → CMD_SPACE_B (保持first=true)
- [ ] TestAccess: `0xFC` → CMD_TEST_ACCESS (保持first=true)

### S4. CS#释放处理
- [ ] CS#上升沿时调用finish_command()
- [ ] 检查mb_count >= cmd_min
- [ ] mb_count < cmd_min时输出warning "Missing data bytes"

### S5. 数据输出格式
- [ ] Write类: 使用mb_mosi[]数据
- [ ] Read类: 使用mb_miso[]数据
- [ ] 格式: `"Write: IOCFG1 (00) = 01 02"` / `"@01 02"`

---

## spi_tpm_c 专项验收

### P1. 元数据正确性
- [ ] `.id` = `"spi_tpm_c"`
- [ ] `.name` = `"SPI TPM(C)"`
- [ ] `.tags` = `{"IC", "TPM", "BitLocker", NULL}`
- [ ] 6个annotation: READ, WRITE, ADDRESS, WAIT, DATA, VMK
- [ ] 2个annotation rows: Transactions, B-VMK
- [ ] 1个option: tpm_version (默认"2.0", 可选"2.0"/"1.2")

### P2. Options初始化
- [ ] tpm_version默认值: `"2.0"`
- [ ] tpm_version可选值: `"2.0"`, `"1.2"`
- [ ] start()中根据tpm_version设置wait_mask和fifo_registers

### P3. 事务状态机
- [ ] NONE → READ/WRITE (根据mosi bit7)
- [ ] READ/WRITE → READ_ADDRESS (收集3字节地址)
- [ ] READ_ADDRESS → WAIT (miso == wait_mask时) 或 TRANSFER_DATA
- [ ] WAIT → TRANSFER_DATA (miso == end_wait时)
- [ ] TRANSFER_DATA → NONE (数据收集完成)

### P4. FIFO寄存器查找
- [ ] TPM 2.0: 至少包含TPM_ACCESS_0, TPM_STS_0, TPM_DATA_FIFO_0等关键寄存器
- [ ] TPM 1.2: 同上
- [ ] 地址范围匹配正确
- [ ] 未匹配地址返回"Unknown"

### P5. VMK提取
- [ ] 仅在READ事务且地址匹配TPM_DATA_FIFO_0时收集MISO数据
- [ ] 环形缓冲区大小12字节
- [ ] VMK header检测: `2c 00 0[0-6] 00 0[1-9] 00 0[0-1] 00 0[0-5] 20 00 00`
- [ ] 检测到header后输出ANN_VMK "VMK header: ..."
- [ ] 收集32字节VMK后输出ANN_VMK "VMK: ..."
- [ ] VMK提取期间其他TPM事务不干扰(saving_vmk标志)

### P6. Transaction annotation输出
- [ ] 有wait_count时: 输出4个annotation (Read/Write, Address, Wait, Data)
- [ ] 无wait_count时: 输出3个annotation (Read/Write, Address, Data)
- [ ] 寄存器名正确显示

---

## spiflash_c 专项验收

### F1. 元数据正确性
- [ ] `.id` = `"spiflash_c"`
- [ ] `.name` = `"SPI Flash(C)"`
- [ ] `.tags` = `{"IC", "Memory", NULL}`
- [ ] 31个annotation: 28个命令 + BIT + FIELD + WARN
- [ ] 4个annotation rows: bits, fields, commands, warnings
- [ ] 2个options: chip, format

### F2. Options初始化
- [ ] chip默认值: `"macronix_mx25l1605d"`
- [ ] chip可选值: 6个芯片型号
- [ ] format默认值: `"hex"`
- [ ] format可选值: `"hex"`, `"ascii"`

### F3. 命令查找表
- [ ] 28个命令完整: 0x01-0xEF
- [ ] 每个命令有shortname和desc
- [ ] 未知命令输出ANN_BIT "Unknown command"

### F4. 关键命令handler
- [ ] handle_wren(): 输出ANN_WREN, 设置writestate=1
- [ ] handle_wrdi(): 输出ANN_WRDI, 设置writestate=0
- [ ] handle_rdid(): 4字节序列(cmd+mfg+type+device_id)
- [ ] handle_rdsr(): cmd+循环status register
- [ ] handle_read(): cmd+3字节addr+N字节miso数据
- [ ] handle_pp(): cmd+3字节addr+N字节mosi数据
- [ ] handle_se(): cmd+3字节addr, 检查WREN和4K对齐
- [ ] handle_fast_read(): cmd+3字节addr+1字节dummy+N字节miso数据

### F5. 延迟输出机制
- [ ] READ/PP/FAST_READ等命令的数据块在CS#释放时输出
- [ ] end_current_transaction()在CS-CHANGE时调用
- [ ] 数据块格式: `"Read data (addr 001234, 16 bytes): 01 02 03 ..."`

### F6. WREN检查
- [ ] PP/SE/CE等写命令: 若writestate==0，输出ANN_WARN "Warning: WREN might be missing"

### F7. Status Register解码
- [ ] decode_status_reg(): 解析WIP/WEL/BP/CP/SRWD位
- [ ] 输出多行文本annotation

### F8. 芯片信息
- [ ] 6个芯片型号信息完整
- [ ] device_name查找表正确
- [ ] RDID/REMS后显示vendor+device名

---

## sdcard_spi_c 专项验收

### D1. 元数据正确性
- [ ] `.id` = `"sdcard_spi_c"`
- [ ] `.name` = `"SD Card SPI(C)"`
- [ ] `.tags` = `{"Memory", NULL}`
- [ ] 135个annotation: CMD0-63 + ACMD0-63 + R1/R1B/R2/R3/R7 + BIT + BIT_WARNING
- [ ] 2个annotation rows: bits, commands-replies

### D2. 命令名查找
- [ ] cmd_names[]: 64个CMD名称
- [ ] acmd_names[]: 64个ACMD名称
- [ ] CMD32/33名称后缀添加"_ADDR"

### D3. 命令Token解析
- [ ] 6字节token收集完整
- [ ] Start bit检查(必须为0)
- [ ] Transmitter bit检查(必须为1)
- [ ] Command index提取: `token[0] & 0x3F`
- [ ] Argument提取: `(token[1]<<24)|(token[2]<<16)|(token[3]<<8)|token[4]`
- [ ] CRC7提取: `token[5] >> 1`
- [ ] End bit检查(必须为1)

### D4. CMD/ACMD切换
- [ ] CMD55设置is_acmd=true
- [ ] 非CMD55的ACMD处理后重置is_acmd=false
- [ ] CMD0忽略0xFF字节

### D5. 响应处理
- [ ] R1: 1字节，8个状态位解析
- [ ] R1 bit0: "In idle state"
- [ ] R1 bit1: "Erase reset"
- [ ] R1 bit2: "Illegal command"
- [ ] R1 bit3: "Communication CRC error"
- [ ] R1 bit4: "Erase sequence error"
- [ ] R1 bit5: "Address error"
- [ ] R1 bit6: "Parameter error"
- [ ] R1 bit7: "Always 0"
- [ ] R1B/R2/R3/R7: 至少输出原始数据(可后续完善)

### D6. CMD17数据块读取
- [ ] R1响应后等待Start Block(0xFE)
- [ ] 读取blocklen字节数据
- [ ] 读取2字节CRC
- [ ] blocklen默认512(若未通过CMD16设置)

### D7. CMD24数据块写入
- [ ] R1响应后发送Start Block(0xFE)
- [ ] 写入blocklen字节数据
- [ ] Data Response token解析
- [ ] Busy等待(miso==0x00)

### D8. 状态机完整性
- [ ] IDLE → GET_CMD_TOKEN → HANDLE_CMD* → GET_RESPONSE_R* → (HANDLE_DATA_BLOCK | IDLE)
- [ ] 所有20+状态均有处理逻辑
- [ ] 未知状态回退到IDLE

---

## 最终集成验收

### I1. CMakeLists.txt
- [ ] C_DECODERS列表包含: `st25r39xx_spi_c sdcard_spi_c spiflash_c spi_tpm_c tpm_tis_spi_c`
- [ ] 编译成功，5个DLL生成

### I2. 运行时加载
- [ ] PXView.exe启动无崩溃
- [ ] 解码器列表中可见5个新C解码器
- [ ] 解码器名称/描述正确显示

### I3. 协议栈验证
- [ ] SPI(C) → tpm_tis_spi_c 堆叠正常
- [ ] SPI(C) → st25r39xx_spi_c 堆叠正常
- [ ] SPI(C) → spi_tpm_c 堆叠正常
- [ ] SPI(C) → spiflash_c 堆叠正常
- [ ] SPI(C) → sdcard_spi_c 堆叠正常

### I4. 与Python原版功能对比
- [ ] 相同输入数据，C版本annotation数量 >= Python版本(简化项可少)
- [ ] 关键annotation内容与Python版本一致
- [ ] 无崩溃、无内存泄漏
