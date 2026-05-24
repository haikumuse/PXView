# SPI上层协议解码器移植任务分解 (Batch-27)

## 任务总览

将5个SPI上层Python解码器移植为C解码器，按复杂度从低到高排序实施。

---

## 阶段0: 公共基础设施

### Task 0.1: 创建SPI上层解码器公共辅助头文件(可选)

- **描述**: 在 `libsigrokdecode/c_decoders/` 下创建 `spi_proto_helper.h`，定义SPI协议包解析辅助函数
- **文件**: `libsigrokdecode/c_decoders/spi_proto_helper.h` (新建，可选)
- **内容**:
  - `spi_proto_get_mosi()` / `spi_proto_get_miso()` — 从DATA包提取mosi/miso字节值
  - `spi_proto_cs_change_get_values()` — 从CS-CHANGE包提取prev/cur CS值
- **替代方案**: 每个解码器文件内static定义这些辅助函数(更简单，无需修改构建系统)
- **建议**: 采用替代方案，在每个.c文件内重复定义这几个简单的inline函数
- **验证**: 编译通过

---

## 阶段1: tpm_tis_spi_c (最简单，约400行)

### Task 1.1: 创建tpm_tis_spi_c.c骨架

- **文件**: `libsigrokdecode/c_decoders/tpm_tis_spi_c.c`
- **内容**:
  - 头文件include
  - Annotation枚举和labels
  - Annotation rows定义
  - 状态枚举和结构体
  - Inputs/outputs/tags声明
  - 空的reset/start/decode/destroy函数
  - 空的recv_proto函数
  - srd_c_decoder结构体(含.recv_proto字段)
  - srd_c_decoder_entry()和srd_c_decoder_api_version()

### Task 1.2: 实现tpm_tis_spi_c状态机

- **描述**: 将Python协程转为C显式状态机
- **核心逻辑**:
  1. `TIS_GET_RW_LENGTH`: 解析第一个MOSI字节，提取reading/length
  2. `TIS_GET_ADDR_BYTE2/1/0`: 收集3字节地址
  3. `TIS_GET_DATA`: 循环收集length个数据字节
  4. 完成后输出所有annotation(RW/Length, Address, Wait State, Data, Transaction)
- **duplex warning**: Read时检查mosi!=0，Write时检查miso!=0
- **wait_state检测**: addr_byte0的miso==0表示有wait state
- **Python输出**: 注册 `SRD_OUTPUT_PYTHON` 输出 `"tpm-tis"`，Transaction完成时发送

### Task 1.3: 实现tpm_tis_spi_c的_finish_annotations逻辑

- **描述**: Python版本的 `_finish_annotations()` 函数去除冗长的annotation文本
- **C实现**: 在输出annotation时，直接构建从长到短的字符串数组，使用C_ANN_PUT的多参数形式

### Task 1.4: 验证tpm_tis_spi_c

- **验证项**:
  - 编译通过
  - CMakeLists.txt已添加 `tpm_tis_spi_c`
  - srd_c_decoder结构体字段完整正确
  - recv_proto正确处理DATA和CS-CHANGE包

---

## 阶段2: st25r39xx_spi_c (中等，约600行)

### Task 2.1: 创建st25r39xx_spi_c.c骨架

- **文件**: `libsigrokdecode/c_decoders/st25r39xx_spi_c.c`
- **内容**: 同Task 1.1的骨架结构

### Task 2.2: 移植寄存器查找表

- **描述**: 将Python `lists.py` 中的3个字典转为C数组
- **数据结构**:
  ```c
  typedef struct { uint8_t addr; const char *name; } reg_entry;
  static const reg_entry regsSpaceA[] = { {0x00, "IOCFG1"}, ... {0x3F, "ICIDENT"}, ... };
  static const reg_entry regsSpaceB[] = { {0x05, "EMDSUPPRCONF"}, ... };
  static const reg_entry regsTest[] = { {0x01, "ANTSTOBS"} };
  static const reg_entry dirCmd[] = { {0xC0, "SET_DEFAULT"}, ... };
  ```
- **查找函数**: 线性搜索(条目少，性能足够)

### Task 2.3: 实现parse_command()

- **描述**: 解析MOSI命令字节，设置cmd_type/cmd_dat/cmd_min/cmd_max
- **逻辑**: 见spec.md 3.1.4节
- **关键**: Space B(0xFB)和TestAccess(0xFC)后保持first=true

### Task 2.4: 实现finish_command()

- **描述**: CS#释放时调用，根据cmd_type输出对应annotation
- **逻辑**:
  - Write类: 使用mb_mosi[]数据，输出ANN_BURST_WRITE/ANN_BURST_WRITEB/ANN_BURST_WRITET
  - Read类: 使用mb_miso[]数据，输出ANN_BURST_READ/ANN_BURST_READB/ANN_BURST_READT
  - FIFO: 使用对应方向数据，输出ANN_FIFO_WRITE/ANN_FIFO_READ
  - Direct Command: 输出ANN_DIRECTCMD

### Task 2.5: 实现decode_reg()和decode_mb_data()

- **描述**: 格式化寄存器名+数据值的annotation
- **格式**: `"Write: IOCFG1 (00) = 01 02"` / `"@01 02"`

### Task 2.6: 实现recv_proto()

- **描述**: 主回调函数
- **逻辑**:
  1. CS-CHANGE: CS#上升沿→finish_command()→next()
  2. DATA: first字节→parse_command()，后续字节→收集到mb_mosi[]/mb_miso[]

### Task 2.7: 验证st25r39xx_spi_c

- **验证项**: 编译通过，寄存器查找表完整，两级命令解析(Space B/TestAccess)正确

---

## 阶段3: spi_tpm_c (中高，约700行)

### Task 3.1: 创建spi_tpm_c.c骨架

- **文件**: `libsigrokdecode/c_decoders/spi_tpm_c.c`
- **内容**: 骨架+options定义(tpm_version)

### Task 3.2: 移植FIFO寄存器查找表

- **描述**: 将Python `lists.py` 中的RangeDict转为C数组
- **数据结构**:
  ```c
  typedef struct { uint16_t start; uint16_t end; const char *name; } fifo_reg_range;
  static const fifo_reg_range tpm2_fifo_regs[] = { {0x0000, 0x0000, "TPM_ACCESS_0"}, ..., {0, 0, NULL} };
  static const fifo_reg_range tpm1_fifo_regs[] = { ... };
  ```
- **查找函数**: 线性搜索，匹配addr >= start && addr <= end

### Task 3.3: 实现事务状态机

- **描述**: 将Python TransactionState转为C枚举和状态机
- **状态**: NONE → READ/WRITE → READ_ADDRESS → WAIT → TRANSFER_DATA
- **Transaction完成时**: 输出Read/Write/Address/Wait/Data annotation

### Task 3.4: 实现VMK提取

- **描述**: 将Python VMK提取逻辑转为C
- **核心逻辑**:
  1. 维护12字节环形缓冲区 `vmk_queue[]`
  2. 仅在 `_is_vmk_transaction()` (地址匹配TPM_DATA_FIFO_0) 时收集MISO数据
  3. 检测VMK header: `2c 00 0[0-6] 00 0[1-9] 00 0[0-1] 00 0[0-5] 20 00 00`
  4. 检测到header后收集32字节VMK
- **C实现要点**:
  - 环形缓冲区用模运算实现
  - 正则匹配改为逐字节比较
  - VMK header检测函数:
    ```c
    static int check_vmk_header(const uint8_t *queue) {
        if (queue[0] != 0x2c) return 0;
        if (queue[1] != 0x00) return 0;
        if ((queue[2] & 0xF0) != 0x00 || (queue[2] & 0x0F) > 6) return 0;
        // ... 逐字节检查
        return 1;
    }
    ```

### Task 3.5: 实现recv_proto()

- **描述**: 主回调
- **逻辑**:
  1. CS-CHANGE: end_current_transaction()
  2. DATA: 根据state调用对应handler

### Task 3.6: 验证spi_tpm_c

- **验证项**: 编译通过，TPM 2.0/1.2版本切换正确，VMK提取逻辑完整

---

## 阶段4: spiflash_c (高复杂度，约900行)

### Task 4.1: 创建spiflash_c.c骨架

- **文件**: `libsigrokdecode/c_decoders/spiflash_c.c`
- **内容**: 骨架+options(chip, format)+31个annotation定义

### Task 4.2: 移植芯片信息查找表

- **描述**: 将Python `lists.py` 中的chips/device_name字典转为C数组
- **数据结构**:
  ```c
  typedef struct {
      const char *key; const char *vendor; const char *model;
      uint32_t rdid_id; uint16_t rems_id; int page_size; int sector_size; int block_size;
  } spiflash_chip_info;
  static const spiflash_chip_info spiflash_chips[] = { ... };
  ```

### Task 4.3: 移植命令查找表

- **描述**: 将Python `lists.py` 中的cmds OrderedDict转为C数组
- **数据结构**:
  ```c
  typedef struct { uint8_t cmd; const char *shortname; const char *desc; } spiflash_cmd_entry;
  static const spiflash_cmd_entry spiflash_cmds[] = {
      {0x01, "WRSR", "Write status register"},
      {0x02, "PP", "Page program"},
      // ...
  };
  ```

### Task 4.4: 实现命令handler函数

- **描述**: 为28个命令各实现handler函数
- **优先级**: 先实现常用命令(READ/WRITE/PP/RDSR/RDID/WREN/WRDI/SE/FAST_READ)，其余可简化
- **关键handler**:
  - `handle_read()`: cmd→3字节addr→N字节miso数据
  - `handle_pp()`: cmd→3字节addr→N字节mosi数据
  - `handle_rdid()`: cmd→mfg_id→mem_type→device_id
  - `handle_rdsr()`: cmd→循环读取status register
  - `handle_se()`: cmd→3字节addr，检查WREN和4K对齐

### Task 4.5: 实现延迟输出机制(on_end_transaction)

- **描述**: Python版本通过lambda回调在CS#释放时输出数据块
- **C实现**:
  - 在state中保存 `pending_output` 标志和相关信息
  - 在recv_proto()收到CS-CHANGE时检查并执行延迟输出
  - 或直接在DATA包处理中累积数据，CS-CHANGE时统一输出

### Task 4.6: 实现decode_status_reg()

- **描述**: 将Python的status register解码逻辑转为C
- **输出**: 多行文本annotation，包含WIP/WEL/BP/CP/SRWD位解析

### Task 4.7: 实现recv_proto()

- **描述**: 主回调
- **逻辑**:
  1. CS-CHANGE: end_current_transaction() (执行延迟输出)
  2. DATA: state==NULL时设置命令state，否则调用对应handler

### Task 4.8: 验证spiflash_c

- **验证项**: 编译通过，所有28个命令handler存在，芯片选项正确

---

## 阶段5: sdcard_spi_c (最高复杂度，约800行)

### Task 5.1: 创建sdcard_spi_c.c骨架

- **文件**: `libsigrokdecode/c_decoders/sdcard_spi_c.c`
- **内容**: 骨架+135个annotation定义(使用宏生成)

### Task 5.2: 移植命令名查找表

- **描述**: 将Python `common/sdcard.py` 中的cmd_names/acmd_names转为C数组
- **需先读取**: `libsigrokdecode/decoders/common/sdcard.py`

### Task 5.3: 实现命令token解析

- **描述**: handle_command_token() — 收集6字节命令token并解析
- **格式**: [start_bit(0)][transmitter(1)][cmd_index(6bit)][argument(32bit)][CRC7(7bit)][end_bit(1)]
- **C实现**:
  - 收集6个MOSI字节到cmd_token[]
  - 解析各字段
  - 输出bit级annotation

### Task 5.4: 实现命令处理函数

- **描述**: handle_cmd0/1/9/16/17/24/55/59, handle_acmd41, handle_cmd999
- **关键**:
  - CMD55: 设置is_acmd标志
  - CMD17: READ_SINGLE_BLOCK，需处理数据块
  - CMD24: WRITE_BLOCK，需处理数据块和响应

### Task 5.5: 实现响应处理函数

- **描述**: handle_response_r1/r1b/r2/r3/r7
- **R1响应**: 1字节，8个状态位解析
- **简化**: R1B/R2/R3/R7可先输出原始数据，后续完善

### Task 5.6: 实现数据块传输处理

- **描述**: CMD17/CMD24的数据块读写
- **CMD17**: R1→0xFF填充→Start Block(0xFE)→blocklen字节数据→2字节CRC
- **CMD24**: R1→Start Block(0xFE)→blocklen字节数据→Data Response→Busy

### Task 5.7: 实现recv_proto()

- **描述**: 主回调
- **逻辑**:
  1. CS-CHANGE: 重置状态
  2. DATA: 根据state分发到对应handler

### Task 5.8: 验证sdcard_spi_c

- **验证项**: 编译通过，CMD/ACMD切换正确，数据块传输完整

---

## 阶段6: 构建集成与最终验证

### Task 6.1: 修改CMakeLists.txt

- **文件**: `CMakeLists.txt`
- **修改**: 在C_DECODERS列表末尾追加 `st25r39xx_spi_c sdcard_spi_c spiflash_c spi_tpm_c tpm_tis_spi_c`

### Task 6.2: 全量编译验证

- **命令**: `build_incremental.cmd`
- **验证**: 所有5个新解码器DLL成功编译

### Task 6.3: 运行时验证

- **验证**: PXView.exe启动后，在解码器列表中能看到5个新的C解码器
- **测试**: 选择SPI+C解码器组合，验证上层解码器能正确接收SPI数据

---

## 实施顺序建议

```
阶段0(公共) → 阶段1(tpm_tis_spi) → 阶段2(st25r39xx_spi) → 阶段3(spi_tpm) → 阶段4(spiflash) → 阶段5(sdcard_spi) → 阶段6(集成)
```

每个阶段完成后独立编译验证，确保增量开发不引入回归问题。

---

## 工时估算

| 阶段 | 任务 | 预估工时 |
|------|------|---------|
| 0 | 公共基础设施 | 0.5h |
| 1 | tpm_tis_spi_c | 2h |
| 2 | st25r39xx_spi_c | 3h |
| 3 | spi_tpm_c | 3.5h |
| 4 | spiflash_c | 4.5h |
| 5 | sdcard_spi_c | 4h |
| 6 | 集成验证 | 1h |
| **总计** | | **~18.5h** |
