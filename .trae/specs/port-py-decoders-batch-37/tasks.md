# Batch 37: 任务分解

## 任务总览

| 任务 ID | 解码器 | 文件名 | 优先级 | 预估工时 | 依赖 |
|---------|--------|--------|--------|----------|------|
| T37-1 | ltar_smartdevice_decode | `ltar_smartdevice_decode_c.c` | P0 | 1.5h | T36-1 (ltar_smartdevice_c) |
| T37-2 | sipi | `sipi_c.c` | P0 | 3h | 无 |
| T37-3 | tm1637 | `tm1637_c.c` | P1 | 3h | 无 |
| T37-4 | tm1638 | `tm1638_c.c` | P1 | 3h | T37-3 (复用 TM1637 代码) |
| T37-5 | pjon | `pjon_c.c` | P2 | 5h | 无 |
| T37-6 | tpm_fifo_tis | `tpm_fifo_tis_c.c` | P2 | 8h | 无 |

---

## T37-1: ltar_smartdevice_decode_c.c

### 步骤

1. **创建文件骨架**
   - 文件：`libsigrokdecode/c_decoders/ltar_smartdevice_decode_c.c`
   - 包含头文件、枚举定义、ann_labels、ann_rows

2. **定义 annotation 枚举和标签**
   ```c
   enum {
       ANN_FRAME_NAME = 0,
       ANN_FRAME_ERROR,
       ANN_FRAME_BIT_NAME,
       ANN_FRAME_BITS_DATA,
       ANN_BLOCK_ERROR,
       ANN_BLOCK_DATA,
       NUM_ANN,
   };
   ```

3. **定义查找表**
   ```c
   // Block type 查找表 (8 条)
   static const char *btype_table[] = {
       "UNKNOWN", "REQUEST-STATUS", "TAGGER-STATUS",
       "REQUEST-CONFIG", "SET-CONFIG", "ACK",
       "NACK", "UNKNOWN"
   };

   // Weapon mode 查找表 (2 条)
   static const char *weapmode_table[] = {"SINGLE", "BURST"};

   // Shield status 查找表 (3 条)
   static const char *shieldstatus_table[] = {"OFF", "ON", "RELOADING"};

   // Hunting direction 查找表 (2 条)
   static const char *huntingdirection_table[] = {"OUTWARD", "INWARD"};
   ```

4. **定义私有状态结构体**
   - 简单结构体，仅包含 `out_ann`

5. **实现 reset/start/destroy**
   - `reset`：分配私有结构体
   - `start`：注册 `out_ann` 输出
   - `destroy`：释放私有结构体

6. **实现 checkBlockLength**
   - 检查 block 长度是否合法（根据 block type 判断）

7. **实现 checkBlockCSum**
   - 计算校验和：`0xFF - sum(all_byte_values)`
   - 结果应为 0，否则输出 warning

8. **实现 putBlockType**
   - 输出 block type annotation

9. **实现 putData — TAGGER-STATUS (0x02) 解码**
   - BData0：Player Number (bits 0-2) + Team Number (bits 3-4)
   - BData1：Weapon Mode (bits 0-1) + Shield State (bits 2-3) + Hunting Direction (bit 4)
   - BData2-BData8：各字段直接输出

10. **实现 recv_proto 核心逻辑**
    - 处理 `"BLOCK"` 命令：解析 block 数据
    - 调用 checkBlockLength、checkBlockCSum、putBlockType、putData

11. **实现 decode() 空函数**

12. **定义 srd_c_decoder 结构体**
    - `.id = "ltar_smartdevice_decode_c"`
    - `.name = "LTAR SmartDevice Decode(C)"`
    - `.inputs = {"ltar_smartdevice", NULL}`
    - `.outputs = {"ltar_smartdevice_decode", NULL}`
    - `.recv_proto = ltar_sd_dec_recv_proto`

13. **实现 srd_c_decoder_entry()**
    - 无 options，直接返回

14. **添加到 CMakeLists.txt**

### 验证要点
- [ ] BLOCK 命令正确解析
- [ ] Block type 查找正确
- [ ] TAGGER-STATUS 各 bit 字段提取正确
- [ ] Checksum 验证正确
- [ ] Block 长度检查正确
- [ ] 6 个 annotation 正确输出

---

## T37-2: sipi_c.c

### 步骤

1. **创建文件骨架**

2. **定义 annotation 枚举和标签** (7 个)
   ```c
   enum {
       ANN_HEADER_TAG = 0,
       ANN_HEADER_CMD,
       ANN_HEADER_CH,
       ANN_ADDRESS,
       ANN_DATA,
       ANN_CRC,
       ANN_WARNING,
       NUM_ANN,
   };
   ```

3. **定义 command_codes 查找表** (13 条)
   ```c
   typedef struct {
       uint8_t code;
       const char *name;
       int addr_len;
       int data_len;
   } sipi_command_entry;

   static const sipi_command_entry command_table[] = {
       {0x00, "Read byte", 4, 0},
       {0x01, "Read 2 byte", 4, 0},
       {0x02, "Read 4 byte", 4, 0},
       {0x04, "Write byte with ACK", 4, 4},
       {0x05, "Write 2 byte with ACK", 4, 4},
       {0x06, "Write 4 byte with ACK", 4, 4},
       {0x08, "ACK", 0, 0},
       {0x09, "NACK (Target Error)", 0, 0},
       {0x0A, "Read Answer with ACK", 4, 4},
       {0x0C, "Trigger with ACK", 0, 0},
       {0x12, "Read 4-byte JTAG ID", 0, 0},
       {0x17, "Stream 32 byte with ACK", 0, 32},
   };
   ```

4. **实现 CRC-CCITT**
   ```c
   static uint16_t crc_ccitt(const uint8_t *data, int len)
   {
       uint16_t crc = 0xFFFF;
       for (int i = 0; i < len; i++) {
           crc ^= (uint16_t)data[i] << 8;
           for (int j = 0; j < 8; j++) {
               if (crc & 0x8000)
                   crc = (crc << 1) ^ 0x1021;
               else
                   crc <<= 1;
           }
       }
       return crc;
   }
   ```

5. **定义私有状态结构体**
   - `bit_len`：bit 宽度
   - `addr_len`, `data_len`：当前帧的地址/数据长度
   - `frame_len`：当前帧总长度

6. **实现 reset/start/destroy**

7. **实现 Header 解析**
   - 从 2-byte header 提取 Tag (bits 15-13), Command Code (bits 12-8), Reserved (bits 7-4), Channel (bits 3-1), Reserved (bit 0)
   - 查找 command_table 获取命令名和地址/数据长度

8. **实现 Payload 解析**
   - 根据 addr_len 和 data_len 提取地址和数据字节

9. **实现 CRC 验证**
   - 对整个帧（不含 CRC 本身）计算 CRC-CCITT
   - 与帧末尾 2 字节比较

10. **实现 recv_proto 核心逻辑**
    - 处理 `"DATA"` 命令：解析字节列表
    - 计算 bit_len
    - 调用 Header/Payload/CRC 解析

11. **实现 decode() 空函数**

12. **定义 srd_c_decoder 结构体**
    - `.id = "sipi_c"`
    - `.name = "SIPI (Zipwire)(C)"`
    - `.inputs = {"lfast", NULL}`
    - `.outputs = NULL`
    - `.recv_proto = sipi_recv_proto`

13. **实现 srd_c_decoder_entry()**
    - 无 options，直接返回

14. **添加到 CMakeLists.txt**

### 验证要点
- [ ] bit_len 从字节级数据正确推算
- [ ] Header 位域提取正确 (Tag/Command/Channel)
- [ ] command_codes 查找正确
- [ ] 变长帧长度计算正确 (2 + addr_len + data_len + 2)
- [ ] CRC-CCITT 验证正确
- [ ] Reserved bits 非 0 时输出 warning

---

## T37-3: tm1637_c.c

### 步骤

1. **创建文件骨架**

2. **定义 annotation 枚举和标签** (16 个)

3. **定义 option** (1 个：dpoint)
   - `"dpoint"`：字符串选项，默认 `"Dot"`，可选 `"Dot"` / `"Colon"`

4. **定义 7-segment fonts 查找表** (~30 条)
   ```c
   static const struct { uint8_t segs; char ch; } font_table[] = {
       {0b0000000, ' '},
       {0b0111111, '0'},
       {0b0000110, '1'},
       {0b1011011, '2'},
       {0b1001111, '3'},
       {0b1100110, '4'},
       {0b1101101, '5'},
       {0b1111101, '6'},
       {0b0000111, '7'},
       {0b1111111, '8'},
       {0b1101111, '9'},
       // ... 字母和特殊字符
   };
   ```

5. **定义 contrasts 数组** (8 级 PWM)
   ```c
   static const char *contrasts[] = {
       "1/16", "2/16", "4/16", "10/16",
       "11/16", "12/16", "13/16", "14/16"
   };
   ```

6. **定义私有状态结构体**
   - 状态枚举：`IDLE`, `REG_CMD`, `REG_DATA`
   - Bit 缓存：`bit_ss[8]`, `bit_es[8]`, `bit_val[8]`
   - 命令状态：`is_write`, `is_auto`, `position`
   - Display 缓冲：`display[16]`, `display_len`
   - Option：`dpoint_is_colon`
   - 传输位置：`ssb`, `ss`, `es`

7. **实现 putd 辅助函数** — 输出 data bit annotation
   - 使用 bit 缓存中的 ss/es/val

8. **实现 putr 辅助函数** — 输出 register bit annotation

9. **实现 handle_command**
   - `0x40`：Data command → 解析 RW (bit 5), ADDR (bits 1-2), MODE (bit 0)
   - `0x80`：Display command → 解析 SWITCH (bit 0), PWM (bits 1-3)
   - `0xC0`：Address command → 解析 DIGIT (bits 0-2)

10. **实现 handle_data**
    - 根据 is_write/is_auto/position 处理数据字节
    - 7-segment 解码 → 更新 display 缓冲
    - position 递增（auto 模式）

11. **实现 handle_info**
    - STOP 时输出完整 display 字符串

12. **实现 recv_proto 核心逻辑**
    - `cmd = "BITS"`：缓存 bit 列表
    - `cmd = "START"`：开始新传输
    - `cmd = "COMMAND"`：处理命令字节
    - `cmd = "DATA"`：处理数据字节
    - `cmd = "STOP"`：输出显示信息

13. **实现 decode() 空函数**

14. **定义 srd_c_decoder 结构体**
    - `.id = "tm1637_c"`
    - `.name = "TM1637(C)"`
    - `.inputs = {"tmc", NULL}`
    - `.outputs = {"tm1637", NULL}`
    - `.recv_proto = tm1637_recv_proto`

15. **实现 srd_c_decoder_entry()**
    - 初始化 `dpoint` option：`g_variant_new_string("Dot")`
    - option values：`g_slist_append(NULL, g_variant_new_string("Dot"))`, `g_variant_new_string("Colon")`

16. **添加到 CMakeLists.txt**

### 验证要点
- [ ] 16 个 annotation 正确定义
- [ ] 3 个 annotation row 正确定义
- [ ] dpoint option 正确初始化和读取
- [ ] 状态机 IDLE→REG_CMD→REG_DATA→IDLE 正确
- [ ] Data command (0x40) 解码正确
- [ ] Display command (0x80) 解码正确
- [ ] Address command (0xC0) 解码正确
- [ ] 7-segment fonts 查找正确
- [ ] Display 缓冲和输出正确
- [ ] Auto 地址递增正确
- [ ] Bit 级标注正确

---

## T37-4: tm1638_c.c

### 步骤

1. **创建文件骨架**
   - 基于 TM1637 代码修改，复用大量逻辑

2. **定义 annotation 枚举和标签** (23 个)
   - 在 TM1637 基础上增加：ANN_POSITION, ANN_LED, ANN_KEY, ANN_RED, ANN_GREEN, ANN_LEDS_INFO, ANN_KEYS_INFO

3. **定义 switches 查找表** (24 条)
   ```c
   static const struct { const char *key_tag; const char *switch_name; } switch_table[] = {
       {"K3-KS1", "S1"}, {"K3-KS2", "S5"}, {"K3-KS3", "S9"},
       {"K3-KS4", "S13"}, {"K3-KS5", "S17"}, {"K3-KS6", "S21"},
       {"K2-KS1", "S2"}, {"K2-KS2", "S6"}, {"K2-KS3", "S10"},
       {"K2-KS4", "S14"}, {"K2-KS5", "S18"}, {"K2-KS6", "S22"},
       {"K1-KS1", "S3"}, {"K1-KS2", "S7"}, {"K1-KS3", "S11"},
       {"K1-KS4", "S15"}, {"K1-KS5", "S19"}, {"K1-KS6", "S23"},
       {"K1-KS1", "S4"}, {"K1-KS2", "S8"}, {"K1-KS3", "S12"},
       {"K1-KS4", "S16"}, {"K1-KS5", "S20"}, {"K1-KS6", "S24"},
   };
   ```

4. **定义私有状态结构体**
   - 在 TM1637 基础上增加：
     - LED 缓冲：`leds[16]`, `leds_len`
     - Key 缓冲：`keys[24][8]`, `keys_len`

5. **实现 handle_data_digit**
   - 复用 TM1637 的 7-segment 解码逻辑

6. **实现 handle_data_led**
   - 奇数地址为 LED
   - bit 0 = Red, bit 1 = Green

7. **实现 handle_data_keyboard**
   - Read 模式下读取按键数据
   - 通过 switches 查找表映射按键名称

8. **实现 handle_info**
   - 输出 Display + LEDs + Keys 信息

9. **修改 handle_command**
   - Address bits 为 4 bits (0-3) vs TM1637 的 3 bits (0-2)
   - 偶数地址 = 数字管，奇数地址 = LED

10. **修改 recv_proto**
    - 与 TM1637 类似，但增加 LED 和键盘处理

11. **定义 srd_c_decoder 结构体**
    - `.id = "tm1638_c"`
    - `.name = "TM1638(C)"`
    - `.inputs = {"tmc", NULL}`
    - `.outputs = {"tm1638", NULL}`
    - `.recv_proto = tm1638_recv_proto`

12. **实现 srd_c_decoder_entry()**
    - 同 TM1637 的 dpoint option

13. **添加到 CMakeLists.txt**

### 验证要点
- [ ] 23 个 annotation 正确定义
- [ ] 5 个 annotation row 正确定义
- [ ] Address 4 bits 正确处理
- [ ] 偶数地址 = 数字管，奇数地址 = LED
- [ ] LED Red/Green 解码正确
- [ ] 键盘扫描解码正确
- [ ] switches 查找表完整 (24 条)
- [ ] Display + LEDs + Keys info 输出正确
- [ ] 7-segment fonts 与 TM1637 一致

---

## T37-5: pjon_c.c

### 步骤

1. **创建文件骨架**

2. **定义 annotation 枚举和标签** (13 个)

3. **实现 CRC-8 算法**
   ```c
   static uint8_t calc_crc8(const uint8_t *data, int len)
   {
       uint8_t crc = 0;
       for (int i = 0; i < len; i++) {
           crc ^= data[i];
           for (int j = 0; j < 8; j++) {
               int odd = crc & 1;
               crc >>= 1;
               if (odd) crc ^= 0x97;
           }
       }
       return crc;
   }
   ```

4. **实现 CRC-32 算法**
   ```c
   static uint32_t calc_crc32(const uint8_t *data, int len)
   {
       uint32_t crc = 0xffffffff;
       for (int i = 0; i < len; i++) {
           crc ^= data[i];
           for (int j = 0; j < 8; j++) {
               int odd = crc & 1;
               crc >>= 1;
               if (odd) crc ^= 0xedb88320;
           }
       }
       return crc ^ 0xffffffff;
   }
   ```

5. **定义私有状态结构体**
   - Frame 状态：`frame_bytes`, `frame_byte_count`, `frame_ss`, `frame_es`
   - Config flags：8 个 flag 位
   - 字段扫描：`field_idx`, `field_got`, `field_widths[]`, `field_ann_classes[]`
   - ACK 状态：`ack_bytes`, `ack_byte_count`
   - Relation 跟踪：`frame_rx_id`, `frame_tx_id`, `frame_payload_text`

6. **实现字段描述系统**（静态实现替代 Python 动态注册）
   - 固定字段表：RX ID (1 byte, ANN_RX_INFO), Header Config (1 byte, ANN_HDR_CFG)
   - 条件字段表：根据 cfg flags 决定是否包含
   - `pjon_build_field_table()`：根据 header config 构建字段表

7. **实现字段处理 handlers**
   - `pjon_handle_rx_id()`：接收方 ID
   - `pjon_handle_hdr_cfg()`：Header Config，解析 8 个 flag 位
   - `pjon_handle_pkt_len()`：包长度 (1 or 2 bytes)
   - `pjon_handle_meta_crc()`：Meta CRC
   - `pjon_handle_tx_info()`：发送方信息
   - `pjon_handle_port()`：服务端口
   - `pjon_handle_pkt_id()`：包 ID
   - `pjon_handle_payload()`：负载数据
   - `pjon_handle_end_crc()`：End CRC (1 or 4 bytes)

8. **实现 pjon_frame_flush**
   - 输出 relation annotation
   - 重置帧状态

9. **实现 pjon_reset_frame**
   - 清零帧缓冲和字段状态

10. **实现 recv_proto 核心逻辑**
    - `cmd = "FRAME_INIT"`：刷新旧帧，重置状态
    - `cmd = "DATA_BYTE"`：累积字节，检查字段完成
    - `cmd = "SYNC_RESP_WAIT"`：切换 ACK 模式
    - `cmd = "IDLE"` / `"FRAME_DATA"`：刷新帧

11. **实现 decode() 空函数**

12. **定义 srd_c_decoder 结构体**
    - `.id = "pjon_c"`
    - `.name = "PJON(C)"`
    - `.inputs = {"pjon_link", NULL}`
    - `.outputs = NULL`
    - `.recv_proto = pjon_recv_proto`

13. **实现 srd_c_decoder_entry()**
    - 无 options，直接返回

14. **添加到 CMakeLists.txt**

### 验证要点
- [ ] 13 个 annotation 正确定义
- [ ] 3 个 annotation row 正确定义
- [ ] Header Config 8 个 flag 位正确解析
- [ ] 字段描述系统正确构建
- [ ] CRC-8 验证正确
- [ ] CRC-32 验证正确
- [ ] Payload 长度计算正确 (pkt_len - overhead)
- [ ] ACK 处理正确
- [ ] Relation 输出正确
- [ ] 帧刷新和重置逻辑正确

---

## T37-6: tpm_fifo_tis_c.c

### 步骤

1. **创建文件骨架**

2. **定义 annotation 枚举和标签** (6 个)

3. **定义 TPM 命令码查找表** (~80 条)
   ```c
   typedef struct { uint32_t code; const char *name; } tpm_cmd_entry;
   static const tpm_cmd_entry tpm_command_codes[] = {
       {0x00000000, "TPM_NV_UndefineSpaceSpecial"},
       {0x0000011C, "TPM_EvictControl"},
       {0x00000120, "TPM_HierarchyControl"},
       // ... ~80 条
   };
   ```

4. **定义 TPM 响应码查找表** (~70 条)
   ```c
   static const tpm_cmd_entry tpm_response_codes[] = {
       {0x00000100, "TPM_RC_SUCCESS"},
       {0x00000101, "TPM_RC_BAD_TAG"},
       // ... ~70 条
   };
   ```

5. **定义 TPM 寄存器常量**
   ```c
   #define TPM_ACCESS_X        0x0000
   #define TPM_INT_ENABLE_X    0x0008
   #define TPM_INT_VECTOR_X    0x000C
   #define TPM_INT_STATUS_X    0x0010
   #define TPM_INTF_CAPABILITY_X 0x0014
   #define TPM_STS_X           0x0018
   #define TPM_DATA_FIFO_X     0x0024
   #define TPM_XID_X           0x0028
   ```

6. **定义 TPM 状态枚举**
   ```c
   enum TpmState {
       TPM_STATE_UNKNOWN = 0,
       TPM_STATE_IDLE,
       TPM_STATE_READY,
       TPM_STATE_RECEPTION,
       TPM_STATE_EXECUTION,
       TPM_STATE_COMPLETION,
   };
   ```

7. **定义私有状态结构体**
   - TPM 状态：`state`, `state_finished`, `state_start`
   - Command 缓冲：`command_buffer[4096]`, `command_len`, `command_start`
   - Response 缓冲：`response_buffer[4096]`, `response_len`, `response_start`

8. **实现 reset/start/destroy**
   - `reset`：分配私有结构体，初始化状态为 UNKNOWN
   - `start`：注册 `out_ann` 和 `out_python` 输出
   - `destroy`：释放私有结构体

9. **实现 tpm_lookup_command**
   - 在 tpm_command_codes 表中查找命令名

10. **实现 tpm_lookup_response**
    - 在 tpm_response_codes 表中查找响应名

11. **实现 tpm_state_transition**
    - 根据 STS 寄存器值和当前状态进行状态转换
    - 输出 state annotation

12. **实现 tpm_on_write**
    - 写入 ACCESS 寄存器：处理 activeLocality 等
    - 写入 STS 寄存器：处理 commandReady, tpmGo 等
    - 写入 FIFO：累积 command_buffer 数据
    - tpmGo 时触发状态转换 Reception → Execution

13. **实现 tpm_on_read**
    - 读取 ACCESS 寄存器
    - 读取 STS 寄存器：处理 dataAvail 等
    - 读取 FIFO：累积 response_buffer 数据
    - dataAvail 时触发状态转换 Execution → Completion

14. **实现 tpm_output_command**
    - 格式化 command_buffer 为 hex 字符串
    - 查找命令码名称
    - 输出 TPM Command annotation

15. **实现 tpm_output_response**
    - 格式化 response_buffer 为 hex 字符串
    - 查找响应码名称
    - 输出 TPM Response annotation

16. **实现 recv_proto 核心逻辑**
    - `cmd = "TRANSACTION"`：解析 addr, data, reading flag
    - 分发到 tpm_on_read 或 tpm_on_write

17. **实现 decode() 空函数**

18. **定义 srd_c_decoder 结构体**
    - `.id = "tpm_fifo_tis_c"`
    - `.name = "TPM FIFO(C)"`
    - `.inputs = {"tpm-tis", NULL}`
    - `.outputs = {"tpm", NULL}`
    - `.recv_proto = tpm_recv_proto`

19. **实现 srd_c_decoder_entry()**
    - 无 options，直接返回

20. **添加到 CMakeLists.txt**

### 验证要点
- [ ] 6 个 annotation 正确定义
- [ ] 4 个 annotation row 正确定义
- [ ] 6 个 TPM 状态正确实现
- [ ] 状态转换逻辑正确
  - [ ] Unknown → Idle (requestUse)
  - [ ] Idle → Ready (commandReady=1)
  - [ ] Ready → Reception (FIFO write)
  - [ ] Reception → Execution (tpmGo=1)
  - [ ] Execution → Completion (dataAvail=1)
  - [ ] Completion → Idle (commandReady=1)
- [ ] TPM 命令码查找表完整 (~80 条)
- [ ] TPM 响应码查找表完整 (~70 条)
- [ ] Command buffer 累积和输出正确
- [ ] Response buffer 累积和输出正确
- [ ] STS 寄存器位域解析正确
- [ ] Register Read/Write annotation 正确
- [ ] Warning 输出正确（异常状态转换等）

---

## CMakeLists.txt 修改

在 `C_DECODERS` 列表中按顺序添加：
```
sipi_c
pjon_c
tpm_fifo_tis_c
tm1637_c
tm1638_c
ltar_smartdevice_decode_c
```

## 构建验证

完成所有文件后执行 `build_incremental.cmd`，确认：
1. 编译无错误
2. 6 个 DLL 生成到 `build.dir/decoders/c_decoders/`
3. PXView.exe 可正常加载解码器

## 任务依赖关系

```
T37-1 (ltar_sd_decode) ──→ 依赖 T36-1 (ltar_smartdevice_c) 的 python output 格式
T37-3 (tm1637) ──→ 无依赖
T37-4 (tm1638) ──→ 依赖 T37-3 (复用代码，建议先完成 TM1637)
T37-2 (sipi) ──→ 无依赖
T37-5 (pjon) ──→ 无依赖
T37-6 (tpm_fifo_tis) ──→ 无依赖
```

**可并行执行**：T37-1, T37-2, T37-3, T37-5, T37-6
**需串行执行**：T37-4 在 T37-3 之后
