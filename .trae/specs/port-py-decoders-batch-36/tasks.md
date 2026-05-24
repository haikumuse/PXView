# Batch 36: 任务分解

## 任务总览

| 任务 ID | 解码器 | 文件名 | 优先级 | 预估工时 | 依赖 |
|---------|--------|--------|--------|----------|------|
| T36-1 | ltar_smartdevice | `ltar_smartdevice_c.c` | P0 | 2h | 无 |
| T36-2 | ir_ltto_decode | `ir_ltto_decode_c.c` | P0 | 3h | 无 |
| T36-3 | ook_vis | `ook_vis_c.c` | P1 | 4h | 无 |
| T36-4 | ook_oregon | `ook_oregon_c.c` | P1 | 6h | T36-3 (共享 ook 输入) |
| T36-5 | sony_md_decode | `sony_md_decode_c.c` | P2 | 8h | 无 |

---

## T36-1: ltar_smartdevice_c.c

### 步骤

1. **创建文件骨架**
   - 文件：`libsigrokdecode/c_decoders/ltar_smartdevice_c.c`
   - 包含头文件、枚举定义、ann_labels、ann_rows

2. **定义 annotation 枚举和标签**
   ```c
   enum {
       ANN_BIT_START = 0,
       ANN_BIT_DATA,
       ANN_BIT_STOP,
       ANN_BIT_SPACER,
       ANN_BIT_BLOCKEND,
       ANN_FRAME,
       ANN_FRAME_ERROR,
       ANN_BLOCK,
       ANN_BLOCK_ERROR,
       NUM_ANN,
   };
   ```

3. **定义私有状态结构体**
   - 状态枚举：`IDLE`, `DATA`, `FRAMESTOP`, `WAITINGFORBLOCKEND`
   - Frame bit 缓冲：`frame_bits_ss[10]`, `frame_bits_es[10]`, `frame_bits_val[10]`
   - Block 数据：`block_frame_count`, `block_start_ss`
   - Spacer 计数器

4. **实现 reset/start/destroy**
   - `reset`：分配私有结构体，初始化状态为 IDLE
   - `start`：注册 `out_ann` 和 `out_python` 输出
   - `destroy`：释放私有结构体

5. **实现 recv_proto 核心逻辑**
   - 处理 `"BIT"` 命令：根据状态机处理 start/data/stop/spacer bits
   - 处理 `"ERROR"` 命令：重置状态机
   - Frame 完成：bit-swap 8 data bits 为 MSB first，输出 frame annotation
   - Block 完成：输出 block annotation 和 python output

6. **实现 decode() 空函数**

7. **定义 srd_c_decoder 结构体**
   - `.id = "ltar_smartdevice_c"`
   - `.name = "LTAR SmartDevice(C)"`
   - `.inputs = {"afsk_bits", NULL}`
   - `.outputs = {"ltar_smartdevice", NULL}`
   - `.recv_proto = ltar_sd_recv_proto`

8. **实现 srd_c_decoder_entry()**
   - 无 options，直接返回

9. **添加到 CMakeLists.txt**

### 验证要点
- [ ] Start bit (0) 正确触发 DATA 状态
- [ ] 8 data bits 收集后进入 FRAMESTOP
- [ ] Stop bit (1) 完成 frame，(0) 触发 framing error
- [ ] 15+ spacer bits 标记 block 结束
- [ ] 10+ spacer bits 后拒绝新 frame start
- [ ] ERROR 命令正确重置

---

## T36-2: ir_ltto_decode_c.c

### 步骤

1. **创建文件骨架**

2. **定义 annotation 枚举和标签**
   ```c
   enum {
       ANN_SIG_TYPE = 0,
       ANN_SIG_ERROR,
       ANN_SIG_DATA,
       ANN_PKT_TYPE,
       ANN_PKT_ERROR,
       ANN_PKT_DATA,
       NUM_ANN,
   };
   ```

3. **定义 ptype 查找表**
   ```c
   typedef struct { uint16_t code; const char *name; } ltto_ptype_entry;
   static const ltto_ptype_entry ptype_table[] = {
       {0x00, "GAME START"},
       {0x01, "JOIN CONFIRMED"},
       // ... ~50 条
   };
   ```

4. **定义私有状态结构体**
   - Multibyte 跟踪：`in_multibyte`, `multibyte_start_ss`, `multibyte_data[]`

5. **实现 Tag 签名解码** `ltto_put_tag_signature()`
   - 提取 team (bits 5-6), player (bits 1-4,8,10,11,13,14), megatag (bits 0-1)
   - 特殊处理：team=0 时的各种 player 角色

6. **实现 LTTO Beacon 解码** `ltto_put_ltto_beacon()`
   - 提取 team (bits 3-4), hitflag (bit 2), extra (bits 0-1)
   - Area Beacon 特殊逻辑

7. **实现 LTAR Beacon 解码** `ltto_put_ltar_beacon()`
   - 提取 hitflag (bit 8), shields (bit 7), health (bits 5-6), team (bits 3-4), player (bits 0-2)

8. **实现 Multibyte Packet 解码**
   - Start frame (bit8=0)：初始化 multibyte 状态
   - Data frame (8 bits)：累积数据
   - End frame (bit8=1)：输出完整 packet

9. **实现 recv_proto**
   - 解析 `"SHORT"` / `"LONG"` 命令
   - 提取 bitcount 和 bitdata
   - 根据 synclength + bitcount 分发到对应处理函数

10. **添加到 CMakeLists.txt**

### 验证要点
- [ ] SHORT+7bit → Tag 正确解码 team/player/megatag
- [ ] SHORT+9bit → Multibyte start/end 正确区分
- [ ] SHORT+8bit → Multibyte data 累积
- [ ] LONG+5bit → LTTO Beacon 正确解码
- [ ] LONG+9bit → LTAR Beacon 正确解码
- [ ] Multibyte packet 完整输出
- [ ] ptype 查找表完整

---

## T36-3: ook_vis_c.c

### 步骤

1. **创建文件骨架**

2. **定义 annotation 枚举和标签** (6 个)

3. **定义 options** (4 个)
   - `displayas`：字符串选项，8 种值
   - `synclen`：整数选项，0-10
   - `syncoffset`：整数选项，-4~4
   - `refsample`：字符串选项，off/show numbers/1~30

4. **实现 bcd2int 辅助函数**

5. **实现 display_all 逻辑**
   - 构建 ookstring
   - 根据 displayas 确定分组宽度 (4 or 8 bits)
   - 逐段输出 field annotation
   - 调用 display_level2

6. **实现 display_level2 逻辑**
   - Preamble 检测（1111 或 1010 模式）
   - Sync 标注
   - 剩余 nibble/byte 输出

7. **实现参考比较逻辑**
   - Cache 管理
   - display_ref

8. **实现 recv_proto**
   - 接收 `"DATA"` 命令
   - 执行 display_all
   - 透传 c_decoder_put_python

9. **添加到 CMakeLists.txt**

### 验证要点
- [ ] Nibble/Byte 分组正确
- [ ] Hex/BCD 格式化正确
- [ ] rev 选项正确反转 bit 序
- [ ] Preamble 自动检测
- [ ] Sync 标注位置正确
- [ ] 参考比较功能
- [ ] 透传输出正确

---

## T36-4: ook_oregon_c.c

### 步骤

1. **创建文件骨架**

2. **定义 annotation 枚举和标签** (9 个)

3. **移植 sensor 查找表** (从 lists.py)
   ```c
   typedef struct {
       const char *id;
       const char *models;
       const char *type;
   } oregon_sensor_entry;

   static const oregon_sensor_entry sensor_table[] = {
       {"1984", "WGR800", "Wind"},
       {"1A2D", "THGR228N", "Temp_Hum1"},
       // ... ~20 条
   };
   ```

4. **移植 sensor_checksum 查找表**

5. **移植 dir_table 风向表**

6. **实现 options** (1 个：unknown type)

7. **实现版本检测逻辑**
   - v2.1：ookstring 前 40 位含 `"10011001"`
   - v1：ookstring 前 17 位含 `"E1100"`
   - v3：ookstring 前 28 位含 `"0101"`

8. **实现 oregon_put_nib** — Nibble 提取和标注

9. **实现 oregon_v3** — v3 协议解码

10. **实现 oregon_v2** — v2.1 协议解码（丢弃奇数位后调用 v3）

11. **实现 oregon_v1** — v1 协议解码

12. **实现 Level 2 解码**
    - oregon_level2：根据 sensor type 分发
    - oregon_temp：温度解码（含负温度）
    - oregon_channel：通道解码
    - oregon_battery：电池状态
    - oregon_baro：气压解码
    - oregon_wind_dir：风向解码
    - oregon_put_l2_param：通用参数输出

13. **实现 Checksum 验证**
    - oregon_checksum：v2/v3 checksum
    - oregon_checksum_v1：v1 checksum

14. **实现 recv_proto**
    - 接收 `"DATA"` 命令
    - 构建 ookstring
    - 版本检测和分发

15. **添加到 CMakeLists.txt**

### 验证要点
- [ ] v1/v2.1/v3 版本正确检测
- [ ] v2.1 奇数位丢弃正确
- [ ] Nibble 反转正确
- [ ] Sensor ID 查找正确
- [ ] 温度解码（含负温度和小数）
- [ ] 湿度/气压/风速/雨量解码
- [ ] Checksum 验证正确
- [ ] Binary output 格式正确

---

## T36-5: sony_md_decode_c.c

### 步骤

1. **创建文件骨架**

2. **定义 annotation 枚举和标签** (16 个)

3. **定义 characters 查找表**

4. **实现 LSB-first/MSB-first 值提取**
   - `putValueLSBFirst()`
   - `putValueMSBFirst()`

5. **实现 Remote Header 解码** `putRemoteHeader()`
   - 8 bits：ready for text, data to send, Kanji, present

6. **实现 Player Header 解码** `putPlayerHeader()`
   - 8 bits：data to send, cede bus, present

7. **实现 Player Data Block 解码** `putPlayerDataBlock()`
   - 10 bytes data + 1 byte checksum
   - XOR checksum 验证

8. **实现 expandPlayerDataBlock** — 子协议分发
   - 优先实现：0x40 Volume, 0x41 Playback Mode, 0xC8 LCD Text
   - 次要实现：0x01, 0x03, 0x05, 0x42, 0x43, 0x46, 0x47, 0xA0, 0xA1
   - 可延后：0x02, 0x06, 0x08, 0x09, 0x18, 0xA2, 0xA3, 0xA5, 0xC0

9. **实现 Remote Data Block 解码** `putRemoteDataBlock()`
   - 9-bit frame 格式
   - Checksum 验证

10. **实现 expandRemoteDataBlock**
    - 0x83: Serial number
    - 0xC0: Remote capabilities

11. **实现 Shift-JIS 字符解码** `putLCDCharacter()`
    - ASCII 可打印字符
    - 半角片假名
    - 双字节 SJIS

12. **实现 Static/Unused/Unknown byte 标注**

13. **实现 recv_proto**
    - 接收 `"MESSAGE"` 命令
    - 解析 bitData
    - expandMessage

14. **添加到 CMakeLists.txt**

### 验证要点
- [ ] Remote Header 正确解码
- [ ] Player Header 正确解码
- [ ] Player Data Block checksum 正确
- [ ] Volume Level (0x40) 解码
- [ ] Playback Mode (0x41) 解码
- [ ] LCD Text (0xC8) 解码含 SJIS
- [ ] Remote Data Block 9-bit 格式正确
- [ ] Static/Unused/Unknown byte 标注
- [ ] Message Start/End 标注

---

## CMakeLists.txt 修改

在 `C_DECODERS` 列表中按顺序添加：
```
ook_oregon_c
ook_vis_c
ltar_smartdevice_c
ir_ltto_decode_c
sony_md_decode_c
```

## 构建验证

完成所有文件后执行 `build_incremental.cmd`，确认：
1. 编译无错误
2. 5 个 DLL 生成到 `build.dir/decoders/c_decoders/`
3. PXView.exe 可正常加载解码器
