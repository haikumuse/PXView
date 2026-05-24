# Python → C 解码器移植任务分解 — Batch 24

## 任务总览

| 任务 ID | 解码器 | 预估工时 | 依赖 |
|---------|--------|----------|------|
| T1 | adns5020_c | 2h | 无 |
| T2 | as5047_c | 3h | 无 |
| T3 | avr_isp_c | 4h | 无 |
| T4 | cc1101_c | 6h | 无 |
| T5 | cyrf6936_c | 10h | 无 |
| T6 | CMakeLists.txt 集成 | 0.5h | T1-T5 |
| T7 | 编译验证 | 1h | T6 |
| T8 | 功能验证 | 2h | T7 |

**建议执行顺序**：T1 → T2 → T3 → T4 → T5 → T6 → T7 → T8

---

## T1: adns5020_c — ADNS-5020 光学鼠标传感器

### T1.1 创建文件骨架

- [ ] 创建 `libsigrokdecode/c_decoders/adns5020_c.c`
- [ ] 添加版权头（GPLv2+，与 Python 版一致）
- [ ] 包含标准头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`

### T1.2 定义 annotation 枚举和标签

- [ ] 定义 `enum { ANN_READ=0, ANN_WRITE, ANN_WARN, NUM_ANN }`
- [ ] 定义 `adns5020_ann_labels[][3]`，第一列为 `""`

### T1.3 定义 annotation rows

- [ ] 定义 3 个 row 的 class 数组：`read_row={0}`, `write_row={1}`, `warnings_row={2}`
- [ ] 定义 `adns5020_ann_rows[]`

### T1.4 定义寄存器映射表

- [ ] 定义 `adns5020_regs[]` 结构体数组（16 个寄存器 + 哨兵）
- [ ] 实现 `adns5020_reg_name(int reg)` 查找函数

### T1.5 定义私有状态结构体

- [ ] `adns5020_state`：mosi_bytes[2], byte_count, ss_cmd, es_cmd, out_ann

### T1.6 实现回调函数

- [ ] `adns5020_reset()`：首次 g_malloc0，后续 memset
- [ ] `adns5020_start()`：注册 SRD_OUTPUT_ANN 输出
- [ ] `adns5020_decode()`：空函数
- [ ] `adns5020_destroy()`：g_free 释放私有数据

### T1.7 实现 recv_proto 核心逻辑

- [ ] CS-CHANGE 处理：上升沿时检查 byte_count 是否为 0 或 2
- [ ] DATA 处理：收集 2 字节，解析命令/数据，输出 annotation
- [ ] 忽略 BITS 和 TRANSFER 包

### T1.8 定义 srd_c_decoder 结构体

- [ ] `.id = "adns5020_c"`, `.name = "ADNS-5020(C)"`
- [ ] `.inputs = {"spi", NULL}`
- [ ] `.tags = {"IC", "PC", "Sensor", NULL}`
- [ ] `.recv_proto = adns5020_recv_proto`

### T1.9 实现导出函数

- [ ] `srd_c_decoder_entry()`：返回 `&adns5020_c_decoder`
- [ ] `srd_c_decoder_api_version()`：返回 `SRD_C_DECODER_API_VERSION`

---

## T2: as5047_c — AS5047 磁编码器

### T2.1 创建文件骨架

- [ ] 创建 `libsigrokdecode/c_decoders/as5047_c.c`
- [ ] 添加版权头、标准包含

### T2.2 定义 annotation 枚举和标签

- [ ] 定义 7 个 annotation class：ANN_COMMANDFRAME, ANN_READDATAFRAME, ANN_WRITEDATAFRAME, ANN_REGISTERREAD, ANN_REGISTERWRITE, ANN_WARNING, ANN_FIELD
- [ ] 定义 `as5047_ann_labels[][3]`

### T2.3 定义 annotation rows

- [ ] 4 个 row：fields{6}, frames{0,1,2}, transactions{3,4}, warnings{5}

### T2.4 定义寄存器映射表

- [ ] 定义 `as5047_regs[]`（11 个寄存器 + 哨兵）
- [ ] 实现 `as5047_reg_name(uint16_t addr)` 查找函数

### T2.5 定义私有状态结构体

- [ ] `as5047_state`：state(INIT/READ/WRITE), transaction_start, current_reg, byte_phase(0/1), mosi_word, miso_word, out_ann

### T2.6 实现 16-bit 帧组装

- [ ] AS5047 使用 16-bit SPI 帧，需要处理两种情况：
  - SPI wordsize=16：每次 DATA 回调直接获取 16-bit 值
  - SPI wordsize=8：需要两次 DATA 回调组装 16-bit 值
- [ ] 实现 `byte_phase` 计数器：phase=0 收高字节，phase=1 收低字节，组装完成

### T2.7 实现奇偶校验

- [ ] `popcount_parity(uint16_t v)` 函数

### T2.8 实现 recv_proto 核心逻辑

- [ ] CS-CHANGE：上升沿重置状态
- [ ] DATA + INIT 状态：
  - 组装 16-bit MOSI 命令帧
  - 检查 MOSI 奇偶校验
  - 解析 bit14=R/W, bits13:0=register
  - 输出 ANN_COMMANDFRAME
  - 切换到 READ/WRITE 状态
- [ ] DATA + READ 状态：
  - 组装 16-bit MISO 数据帧
  - 检查 MISO 奇偶校验
  - 检查 error flag (bit14)
  - 输出 ANN_READDATAFRAME
  - 输出 ANN_REGISTERREAD（跨帧范围）
  - 回到 INIT
- [ ] DATA + WRITE 状态：
  - 组装 16-bit MOSI 数据帧
  - 输出 ANN_WRITEDATAFRAME
  - 输出 ANN_REGISTERWRITE（跨帧范围）
  - 回到 INIT

### T2.9 定义 srd_c_decoder 结构体和导出函数

- [ ] `.id = "as5047_c"`, `.name = "AS5047(C)"`
- [ ] `.inputs = {"spi", NULL}`
- [ ] `.tags = {"Embedded/industrial", NULL}`

---

## T3: avr_isp_c — AVR ISP 编程协议

### T3.1 创建文件骨架

- [ ] 创建 `libsigrokdecode/c_decoders/avr_isp_c.c`

### T3.2 定义 annotation 枚举和标签

- [ ] 15 个 annotation class：ANN_PE, ANN_RSB0, ANN_RSB1, ANN_RSB2, ANN_CE, ANN_RFB, ANN_RHFB, ANN_REFB, ANN_RLB, ANN_REEM, ANN_RP, ANN_LPMP, ANN_WP, ANN_WARN, ANN_DEV
- [ ] 定义 `avr_isp_ann_labels[][3]`

### T3.3 定义 annotation rows

- [ ] 3 个 row：commands{0-12}, warnings{13}, devs{14}

### T3.4 定义设备查找表

- [ ] `avr_devices[]`：9 个设备条目 + 哨兵
- [ ] `vendor_code_name()` 函数

### T3.5 定义私有状态结构体

- [ ] `avr_isp_state`：mosi_bytes[4], miso_bytes[4], byte_count, ss_cmd, es_cmd, ss_device, xx/yy/zz/mm, vendor_code, part_fam_flash_size, part_number, out_ann

### T3.6 实现命令分发

- [ ] `avr_isp_handle_command()`：根据 cmd[0:2] 模式匹配
- [ ] 13 个命令处理函数：
  - handle_cmd_programming_enable
  - handle_cmd_read_signature_byte_0x00/0x01/0x02
  - handle_cmd_chip_erase
  - handle_cmd_read_fuse_bits
  - handle_cmd_read_fuse_high_bits
  - handle_cmd_read_extended_fuse_bits
  - handle_cmd_read_lock_bits
  - handle_cmd_read_eeprom_memory
  - handle_cmd_read_program_memory
  - handle_cmd_load_program_memory_page
  - handle_cmd_write_program_memory_page

### T3.7 实现 recv_proto 核心逻辑

- [ ] CS-CHANGE：上升沿时重置 byte_count
- [ ] BITS：忽略（不中断状态机）
- [ ] DATA：
  - byte_count==0 时记录 ss_cmd
  - 收集 MOSI/MISO 字节对
  - byte_count==4 时调用 handle_command()
  - 重置 byte_count

### T3.8 定义 srd_c_decoder 结构体和导出函数

- [ ] `.id = "avr_isp_c"`, `.name = "AVR ISP(C)"`
- [ ] `.inputs = {"spi", NULL}`
- [ ] `.tags = {"Debug/trace", NULL}`

---

## T4: cc1101_c — TI CC1101 RF 收发器

### T4.1 创建文件骨架

- [ ] 创建 `libsigrokdecode/c_decoders/cc1101_c.c`

### T4.2 定义 annotation 枚举和标签

- [ ] 8 个 annotation class：ANN_STROBE, ANN_SINGLE_READ, ANN_SINGLE_WRITE, ANN_BURST_READ, ANN_BURST_WRITE, ANN_STATUS_READ, ANN_STATUS, ANN_WARN
- [ ] 定义 `cc1101_ann_labels[][3]`

### T4.3 定义 annotation rows

- [ ] 4 个 row：cmd{0}, data{1-5}, status{6}, warnings{7}

### T4.4 定义寄存器/命令映射表

- [ ] `cc1101_regs[]`：47 个配置寄存器
- [ ] `cc1101_status_regs[]`：16 个状态寄存器
- [ ] `cc1101_strobes[]`：14 个命令选通
- [ ] `cc1101_status_states[]`：8 个状态名

### T4.5 实现命令字节解析

- [ ] `cc1101_parse_command()`：返回命令类型、地址、min/max 字节数

### T4.6 定义私有状态结构体

- [ ] `cc1101_state`：first, cmd_type, addr, min/max, mb[] (MOSI/MISO 对), ss_mb/es_mb, cs_was_released, out_ann

### T4.7 实现 Status 寄存器解码

- [ ] `cc1101_decode_status()`：解析 CHIP_RDYn, STATE, FIFO_BYTES_AVAILABLE

### T4.8 实现寄存器解码

- [ ] `cc1101_decode_reg()`：根据 regid 查找寄存器名，格式化输出
- [ ] `cc1101_decode_mb_data()`：多字节数据格式化

### T4.9 实现 finish_command

- [ ] 根据 cmd_type 选择对应 annotation class
- [ ] Write → ANN_SINGLE_WRITE, Burst write → ANN_BURST_WRITE
- [ ] Read → ANN_SINGLE_READ, Burst read → ANN_BURST_READ
- [ ] Status read → ANN_STATUS_READ

### T4.10 实现 recv_proto 核心逻辑

- [ ] CS-CHANGE：
  - 上升沿：finish_command()，重置状态
  - 首次 CS 释放标记
- [ ] DATA + first=true：
  - 解析命令字节
  - 解码 MISO 状态寄存器
  - Strobe 命令立即输出
- [ ] DATA + first=false：
  - 收集数据字节
  - 检查 max 限制
  - excess byte warning

### T4.11 定义 srd_c_decoder 结构体和导出函数

- [ ] `.id = "cc1101_c"`, `.name = "CC1101(C)"`
- [ ] `.inputs = {"spi", NULL}`
- [ ] `.tags = {"IC", "Wireless/RF", NULL}`

---

## T5: cyrf6936_c — Cypress CYRF6936 RF SoC

### T5.1 创建文件骨架

- [ ] 创建 `libsigrokdecode/c_decoders/cyrf6936_c.c`

### T5.2 定义 annotation 枚举和标签

- [ ] 7 个 annotation class：ANN_WRITE, ANN_READ, ANN_TX_DATA, ANN_RX_DATA, ANN_STATE, ANN_WARN, ANN_WAIT
- [ ] 定义 `cyrf6936_ann_labels[][3]`

### T5.3 定义 annotation rows

- [ ] 3 个 row：cmd{0-3}, warnings{4-5}, delays{6}

### T5.4 定义 options

- [ ] 4 个选项：spi3pin, delaysplit, invert_mosi, invert_miso
- [ ] `srd_c_decoder_entry()` 中设置默认值和可选值列表

### T5.5 定义寄存器映射表

- [ ] `cyrf6936_regs[]`：43 个寄存器（含宽度信息）

### T5.6 实现命令字节解析

- [ ] `cyrf6936_parse_command()`：解析 addr, dir_wr, inc

### T5.7 定义私有状态结构体

- [ ] `cyrf6936_state`：first, addr, dir_wr, inc, min/max, mb[], mb_s/mb_e, cs_was_released, spi3pin, samplerate, delaysplit, wait_s/wait_e, out_ann

### T5.8 实现寄存器解码函数（分层）

**基础层（必须实现）**：
- [ ] `cyrf6936_reg_name()` 查找函数
- [ ] `cyrf6936_format_command()` 格式化命令输出
- [ ] 通用寄存器输出：`"read/write(_inc)(REG_NAME) = 0xHH"`

**增强层（优先实现）**：
- [ ] `cyrf6936_decode_reg_0x00()` — CHANNEL_ADR：频率计算、速度类型
- [ ] `cyrf6936_decode_reg_0x03()` — TX_CFG_ADR：数据模式、PA 功率
- [ ] `cyrf6936_decode_reg_0x0D()` — IO_CFG_ADR：SPI 模式检测
- [ ] `cyrf6936_decode_reg_0x0F()` — XACT_CFG_ADR：事务结束状态
- [ ] `cyrf6936_decode_reg_0x10()` — FRAMING_CFG_ADR：SOP 配置

**完整层（后续迭代）**：
- [ ] 其余 25+ 寄存器的位域解码
- [ ] 可在基础层验证通过后逐步添加

### T5.9 实现 finish_command

- [ ] 收集 MOSI/MISO 字节
- [ ] 调用对应寄存器解码函数
- [ ] 输出 annotation
- [ ] 处理 TX_BUFFER_ADR / RX_BUFFER_ADR 的 binary 输出（如支持）

### T5.10 实现 recv_proto 核心逻辑

- [ ] CS-CHANGE：
  - 上升沿：finish_command()，重置状态，记录 wait_s
  - 下降沿：记录 wait_e，计算 delay（如 delaysplit > 0）
  - 首次 CS 释放标记
- [ ] DATA + first=true：
  - 解析命令字节
  - 检查 MISO 第一个字节（应为 0xFF 或 0x00）
- [ ] DATA + first=false：
  - 收集数据字节
  - 达到 max 时 finish_command() + next()
  - IO_CFG_ADR 特殊处理：检测 SPI 3/4-pin 模式
  - inc=1 时地址自增
- [ ] invert_mosi / invert_miso 选项处理

### T5.11 实现 metadata 回调

- [ ] `cyrf6936_metadata()`：接收 SRD_CONF_SAMPLERATE

### T5.12 定义 srd_c_decoder 结构体和导出函数

- [ ] `.id = "cyrf6936_c"`, `.name = "CYRF6936(C)"`
- [ ] `.inputs = {"spi", NULL}`
- [ ] `.outputs = {"cyrf6936", NULL}`
- [ ] `.tags = {"Embedded/industrial", NULL}`
- [ ] `.options = cyrf6936_options`, `.num_options = 4`
- [ ] `.metadata = cyrf6936_metadata`（如 API 支持）

---

## T6: CMakeLists.txt 集成

- [ ] 在 `C_DECODERS` 列表中添加 `adns5020_c`, `as5047_c`, `avr_isp_c`, `cc1101_c`, `cyrf6936_c`
- [ ] 确认 CMake 的 C decoder 构建规则能正确处理新文件

---

## T7: 编译验证

- [ ] 运行 `build_incremental.cmd`
- [ ] 确认 5 个 DLL 成功编译输出到 `build.dir/decoders/c_decoders/`
- [ ] 检查编译警告，修复所有 warning

---

## T8: 功能验证

### T8.1 ADNS-5020 验证

- [ ] 在 PXView 中加载 SPI + ADNS-5020(C) 解码器栈
- [ ] 使用 demo 数据或实际捕获验证：
  - 读寄存器显示在 Read row
  - 写寄存器显示在 Write row
  - CS# 中断时显示 warning

### T8.2 AS5047 验证

- [ ] 加载 SPI + AS5047(C) 解码器栈
- [ ] 验证：
  - 命令帧正确显示（read from/write to）
  - 数据帧正确显示
  - 跨帧事务（register read/write）范围正确
  - 奇偶校验错误时显示 warning
  - Error flag 时显示 warning

### T8.3 AVR ISP 验证

- [ ] 加载 SPI + AVR ISP(C) 解码器栈
- [ ] 验证：
  - Programming Enable 命令识别
  - 签名字节读取和设备识别
  - Chip Erase 命令
  - Fuse bits 读取
  - EEPROM/Flash 读写
  - 未知命令 warning

### T8.4 CC1101 验证

- [ ] 加载 SPI + CC1101(C) 解码器栈
- [ ] 验证：
  - Strobe 命令识别
  - 单字节/突发 读写
  - Status 寄存器解码
  - CS# 上升沿正确结束事务

### T8.5 CYRF6936 验证

- [ ] 加载 SPI + CYRF6936(C) 解码器栈
- [ ] 验证：
  - 读/写命令识别
  - 寄存器名正确显示
  - 基础层：所有寄存器输出十六进制值
  - 增强层：CHANNEL_ADR 频率计算
  - 增强层：IO_CFG_ADR SPI 模式检测
  - SPI 3-pin 模式选项
  - Delay 标注（设置 delaysplit > 0）

### T8.6 对比验证

- [ ] 对每个解码器，使用相同输入数据对比 Python 版和 C 版的 annotation 输出
- [ ] 确保关键 annotation 内容一致（允许格式微小差异）
