# Batch 36: 移植检查清单

## 通用检查项（适用于所有 5 个解码器）

### 文件结构
- [ ] 文件名格式正确：`{decoder_id}_c.c`（`-` 替换为 `_`）
- [ ] 包含必要头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 无编译警告（`-Wall -Wextra`）
- [ ] 无内存泄漏（`destroy` 正确释放所有动态内存）

### srd_c_decoder 结构体
- [ ] `.id` 格式为 `"xxx_c"`（下划线，非连字符）
- [ ] `.name` 格式为 `"XXX(C)"`（英文括号）
- [ ] `.longname` 包含 `(C)` 后缀
- [ ] `.desc` 注明 C implementation
- [ ] `.license` 与 Python 版本一致
- [ ] `.channels = NULL`, `.num_channels = 0`（上层解码器无直接通道）
- [ ] `.optional_channels = NULL`, `.num_optional_channels = 0`
- [ ] `.inputs` 正确声明输入协议
- [ ] `.outputs` 正确声明输出协议（或 NULL）
- [ ] `.tags` 正确声明
- [ ] `.num_annotations = NUM_ANN`
- [ ] `.ann_labels` 第一列为 `""`
- [ ] `.annotation_rows` 所有 ann class 都映射到某行
- [ ] `.binary` 正确声明（或 NULL）
- [ ] `.recv_proto` 函数指针正确赋值
- [ ] `.decode` 函数体为空 `(void)di;`

### srd_c_decoder_entry()
- [ ] 所有 option 默认值在此初始化
- [ ] 字符串 option 使用 `g_variant_new_string()`
- [ ] 整数 option 使用 `g_variant_new_int64()`
- [ ] 浮点 option 使用 `g_variant_new_double()`
- [ ] option values 列表使用 `g_slist_append()`
- [ ] 返回 `&xxx_c_decoder`

### srd_c_decoder_api_version()
- [ ] 返回 `SRD_C_DECODER_API_VERSION`

### recv_proto 实现
- [ ] 函数签名：`void (*recv_proto)(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 获取私有数据前检查 NULL
- [ ] 使用 `strcmp()` 匹配 cmd 字符串
- [ ] 使用 `C_ANN_PUT` 宏输出 annotation
- [ ] 使用 `c_decoder_put_python()` 输出 python 数据
- [ ] 使用 `c_decoder_put_binary()` 输出 binary 数据

### reset/start/destroy
- [ ] `reset`：首次调用时 `g_malloc0` 分配私有结构体
- [ ] `reset`：使用 `memset` 清零，然后初始化特定字段
- [ ] `start`：注册输出端口 `c_decoder_register_output()`
- [ ] `start`：读取 option 值
- [ ] `destroy`：释放私有结构体，设置 `c_decoder_set_private(di, NULL)`

---

## T36-1: ltar_smartdevice_c.c 专项检查

- [ ] 9 个 annotation 正确定义
- [ ] 3 个 annotation row 正确定义
- [ ] 状态机 4 个状态正确实现
- [ ] Start bit (0) → DATA 状态转换
- [ ] 8 data bits 收集后 → FRAMESTOP
- [ ] Stop bit (1) → frame 完成，(0) → framing error
- [ ] Frame data bit-swap (LSB→MSB) 正确
- [ ] Spacer bit 计数逻辑正确
- [ ] 15+ spacer → block 结束
- [ ] 10+ spacer → 拒绝新 frame
- [ ] ERROR/PHASE 和 ERROR/INVALID 处理
- [ ] Block 输出包含 frame 数量
- [ ] Python output 格式：`["BLOCK", blockdata]`
- [ ] inputs = `{"afsk_bits", NULL}`
- [ ] outputs = `{"ltar_smartdevice", NULL}`

---

## T36-2: ir_ltto_decode_c.c 专项检查

- [ ] 6 个 annotation 正确定义
- [ ] 4 个 annotation row 正确定义
- [ ] ptype 查找表完整（~50 条）
- [ ] healthtext 数组（4 条）
- [ ] SHORT+7bit → Tag 签名
  - [ ] team 提取 (bits 5-6)
  - [ ] player 提取 (bits 1-4,8,10,11,13,14)
  - [ ] megatag 提取 (bits 0-1)
  - [ ] team=0 特殊 player 角色名称
- [ ] SHORT+9bit → Multibyte start/end
  - [ ] bit8=0 → start
  - [ ] bit8=1 → end + packet 输出
- [ ] SHORT+8bit → Multibyte data
- [ ] LONG+5bit → LTTO Beacon
  - [ ] team (bits 3-4)
  - [ ] hitflag (bit 2)
  - [ ] extra (bits 0-1)
  - [ ] Area Beacon 特殊逻辑
- [ ] LONG+9bit → LTAR Beacon
  - [ ] hitflag (bit 8)
  - [ ] shields (bit 7)
  - [ ] health (bits 5-6)
  - [ ] team (bits 3-4)
  - [ ] player (bits 0-2)
- [ ] inputs = `{"ir_ltto", NULL}`
- [ ] outputs = `{"ir_ltto_decode", NULL}`

---

## T36-3: ook_vis_c.c 专项检查

- [ ] 6 个 annotation 正确定义
- [ ] 6 个 annotation row 正确定义
- [ ] 4 个 option 正确定义
  - [ ] `displayas`：8 种字符串值
  - [ ] `synclen`：0-10
  - [ ] `syncoffset`：-4~4
  - [ ] `refsample`：off/show numbers/1~30
- [ ] bcd2int 辅助函数实现
- [ ] Nibble (4-bit) 分组正确
- [ ] Byte (8-bit) 分组正确
- [ ] Hex 格式化正确
- [ ] BCD 格式化正确
- [ ] rev 选项反转 bit 序正确
- [ ] Preamble 检测（1111 或 1010 模式）
- [ ] Sync 标注位置正确
- [ ] 参考比较功能（cache 管理）
- [ ] 透传输出 `c_decoder_put_python()` 正确
- [ ] inputs = `{"ook", NULL}`
- [ ] outputs = `{"ook", NULL}`（透传）

---

## T36-4: ook_oregon_c.c 专项检查

- [ ] 9 个 annotation 正确定义
- [ ] 3 个 annotation row 正确定义
- [ ] 1 个 option（unknown type，11 种值）
- [ ] Binary output（1 个：data-hex）
- [ ] sensor 查找表完整（~20 条）
- [ ] sensor_checksum 查找表完整（3 条）
- [ ] dir_table 风向表完整（17 条）
- [ ] 版本检测正确
  - [ ] v2.1：ookstring 前 40 位含 `"10011001"`
  - [ ] v1：ookstring 前 17 位含 `"E1100"`（preamble ≤ 12）
  - [ ] v3：ookstring 前 28 位含 `"0101"`（preamble > 12）
- [ ] v2.1 奇数位丢弃正确
- [ ] Nibble 反转正确（从右到左）
- [ ] oregon_v3 解码
  - [ ] SensorID (16 bit)
  - [ ] Ch (4 bit)
  - [ ] RollingCode (8 bit)
  - [ ] Flags1 (4 bit)
  - [ ] 剩余 nibble 输出
- [ ] oregon_v1 解码
  - [ ] RollingCode, Ch, Temp, Checksum
- [ ] Level 2 解码
  - [ ] oregon_temp：含负温度和小数
  - [ ] oregon_channel：v2/v3 不同解码方式
  - [ ] oregon_battery：bit2 判断
  - [ ] oregon_baro：+856 偏移
  - [ ] oregon_wind_dir：22.5° 步进
  - [ ] oregon_put_l2_param：通用参数输出
- [ ] Checksum 验证
  - [ ] v2/v3：累加反转 nibble，模 255，v2 减 10
  - [ ] v1：累加字节，模 255
- [ ] inputs = `{"ook", NULL}`
- [ ] outputs = `NULL`

---

## T36-5: sony_md_decode_c.c 专项检查

- [ ] 16 个 annotation 正确定义
- [ ] 11 个 annotation row 正确定义
- [ ] characters 查找表（7 条特殊字符）
- [ ] putValueLSBFirst 实现
- [ ] putValueMSBFirst 实现
- [ ] Remote Header 解码
  - [ ] bit0: unused
  - [ ] bit1: ready for text
  - [ ] bit2: done scrolling
  - [ ] bit3: unused
  - [ ] bit4: has data to send
  - [ ] bit5: unused
  - [ ] bit6: Kanji capable
  - [ ] bit7: remote present
- [ ] Player Header 解码
  - [ ] bit0: has data to send
  - [ ] bit1-3: unused
  - [ ] bit4: cede bus
  - [ ] bit5-6: unused
  - [ ] bit7: player present
- [ ] Player Data Block
  - [ ] 10 bytes data + 1 byte XOR checksum
  - [ ] Checksum 验证正确
- [ ] expandPlayerDataBlock 子协议
  - [ ] 0x01: Request Remote Capabilities
  - [ ] 0x03: Scroll Control
  - [ ] 0x05: LCD Backlight Control
  - [ ] 0x40: Volume Level
  - [ ] 0x41: Playback Mode (8 种模式)
  - [ ] 0x42: Recording Indicator
  - [ ] 0x43: Battery Level Indicator (8 种状态)
  - [ ] 0x46: EQ/Sound Indicator
  - [ ] 0x47: Alarm Indicator
  - [ ] 0xA0: Track number
  - [ ] 0xA1: LCD Disc Icon Control
  - [ ] 0xC8: LCD Text (含 7 个字符位置)
- [ ] Remote Data Block
  - [ ] 9-bit frame 格式
  - [ ] 10 × 9-bit data + 1 × 9-bit checksum
- [ ] expandRemoteDataBlock
  - [ ] 0x83: Serial number
  - [ ] 0xC0: Remote capabilities
- [ ] Shift-JIS 字符解码
  - [ ] ASCII 可打印字符
  - [ ] 双字节 SJIS
  - [ ] carryover 处理
- [ ] Static/Unused/Unknown byte 标注
- [ ] inputs = `{"sony_md", NULL}`
- [ ] outputs = `{"sony_md_decode", NULL}`

---

## 构建验证

- [ ] `build_incremental.cmd` 执行成功
- [ ] 5 个 DLL 生成到 `build.dir/decoders/c_decoders/`
- [ ] PXView.exe 可正常启动
- [ ] 解码器列表中可见 5 个新 C 解码器
- [ ] 选择对应下层解码器后可正常堆叠

## 功能验证（如有测试数据）

- [ ] ltar_smartdevice_c：与 Python 版本输出对比
- [ ] ir_ltto_decode_c：与 Python 版本输出对比
- [ ] ook_vis_c：与 Python 版本输出对比
- [ ] ook_oregon_c：与 Python 版本输出对比
- [ ] sony_md_decode_c：与 Python 版本输出对比
