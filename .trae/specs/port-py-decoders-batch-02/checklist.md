# Python 解码器移植检查清单 — Batch 02

## 通用检查项（适用于所有 5 个解码器）

### 文件结构

- [ ] C 文件位于 `libsigrokdecode/c_decoders/<name>_c.c`
- [ ] 包含必要头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 导出变量名格式：`<name>_c_decoder`
- [ ] 导出函数：`SRD_C_DECODER_EXPORT struct srd_c_decoder *srd_c_decoder_entry(void)`

### 元数据一致性

- [ ] `id` 字段格式为 `<python_id>_c`（如 `flexray_c`）
- [ ] `name` 字段包含 `(C)` 后缀（如 `FlexRay(C)`）
- [ ] `longname` 字段包含 `(C)` 后缀
- [ ] `desc` 字段末尾包含 `(C implementation)`
- [ ] `license` 与 Python 版本一致
- [ ] `inputs` 为 `{"logic", NULL}`
- [ ] `outputs` 与 Python 版本一致（如有）
- [ ] `tags` 与 Python 版本一致

### 通道定义

- [ ] 通道 `id` 与 Python 版本一致
- [ ] 通道 `name` 与 Python 版本一致
- [ ] 通道 `desc` 与 Python 版本一致
- [ ] 通道 `order` 按索引顺序设置
- [ ] 通道 `type` 正确设置（SCLK=8, SDATA=108, 其他=0）
- [ ] 通道 `idn` 与 Python 版本一致（如有）
- [ ] 可选通道正确设置为 `optional_channels`

### 选项定义

- [ ] 选项 `id` 与 Python 版本一致
- [ ] 选项 `desc` 与 Python 版本一致
- [ ] 选项 `idn` 与 Python 版本一致（如有）
- [ ] 选项 `def` 默认值正确（GVariant 类型匹配）
- [ ] 选项 `values` 列表完整（GVariant 列表）
- [ ] 在 `srd_c_decoder_entry()` 中正确创建 GVariant 值列表

### 注释定义

- [ ] 注释数量 (`num_annotations`) 与 Python 版本一致
- [ ] 每个注释的 3 个标签 [long, short, shortest] 正确
- [ ] 注释行数量 (`num_annotation_rows`) 与 Python 版本一致
- [ ] 每个注释行的 `id` / `desc` / `ann_classes` / `num_ann_classes` 正确
- [ ] `ann_classes` 数组包含正确的注释类索引

### 生命周期函数

- [ ] `reset()` — 分配私有数据（g_malloc0），初始化所有字段
- [ ] `start()` — 注册输出，获取 samplerate，读取选项
- [ ] `decode()` — 主解码循环
- [ ] `metadata()` — 处理 SRD_CONF_SAMPLERATE（如需要）
- [ ] `destroy()` — 释放私有数据（g_free）

### 解码逻辑

- [ ] 状态机与 Python 版本一致
- [ ] 所有 wait 条件正确转换为 c_cond_xxx 调用
- [ ] matched 位掩码检查正确
- [ ] 所有注释输出使用 C_ANN_PUT 宏
- [ ] 注释格式字符串与 Python 版本一致（long/short/shortest 三级）
- [ ] OUTPUT_PYTHON 输出格式与 Python 版本一致（如需要）

### 内存安全

- [ ] 私有数据结构使用固定大小数组（非动态分配）
- [ ] 数组大小足够容纳最大帧（参考 spec 中的建议大小）
- [ ] 无缓冲区溢出风险（位计数检查）
- [ ] g_malloc0 / g_free 配对使用
- [ ] 无内存泄漏

---

## SpaceWire (spacewire_c) 专项检查

- [ ] 2 通道定义：Data (order=0) + Strobe (order=1)
- [ ] 无选项定义
- [ ] 无 metadata 回调（不需要 samplerate）
- [ ] IDLE 状态 NULL 码检测：data_val 低 7 位 == 0b1110100
- [ ] SYNC 状态 DCF 检测：data_val & 1 → 控制字符(1) / 数据字符(0)
- [ ] 奇偶校验：对 last_data_val 所有位（除 parity/DCF）+ 当前 DCF 异或后取反
- [ ] 控制字符位反转正确（3 位反转）
- [ ] 数据字符位反转正确（8 位反转）
- [ ] FCT/ESC/EEP/EOP 字符值正确映射
- [ ] NULL 控制码检测：ESC + FCT
- [ ] Time 控制码检测：ESC + 数据字符
- [ ] last_samplenums 数组管理正确（环形缓冲区或移位）
- [ ] 注释位置使用 last_samplenums 正确范围
- [ ] 8 个注释类全部定义
- [ ] 4 个注释行定义正确

---

## IEBus (iebus_c) 专项检查

- [ ] 1 通道定义：BUS (order=0)
- [ ] 3 个选项：mode / bus_polarity / ignore_nak
- [ ] metadata 回调保存 samplerate
- [ ] read_bits() 使用 27µs 采样偏移：`skip = (uint64_t)(27e-6 * samplerate)`
- [ ] read_bits() 位结束计算：`bit_end = bit_start + (uint64_t)(33e-6 * samplerate)`
- [ ] 总线极性处理：idle-low 等待上升沿，idle-high 等待下降沿
- [ ] 位值取反：idle-low 时 `(pins[0]+1)%2`，idle-high 时再次取反
- [ ] 开始位宽度检查：`(es-ss)/samplerate >= 100e-6`
- [ ] 广播位：1=Unicast, 0=Broadcast
- [ ] 奇偶校验：popcount(value) % 2
- [ ] ACK/NAK：非广播时 ack=0 为 ACK，ack=1 为 NAK
- [ ] ignore_nak 选项：强制 ack_bit=0
- [ ] 数据长度 0 → 256 字节
- [ ] 数据长度 > 128 输出警告
- [ ] NAK 时 continue 重新搜索开始位
- [ ] Commands 枚举查找和名称输出
- [ ] OUTPUT_PYTHON 格式正确
- [ ] 11 个注释类全部定义
- [ ] 3 个注释行定义正确

---

## FlexRay (flexray_c) 专项检查

- [ ] 1 通道定义：Channel (order=0, idn=dec_flexray_chan_channel)
- [ ] 2 个选项：channel_type / bitrate
- [ ] metadata 回调计算 bit_width 和 sample_point
- [ ] bit_width = samplerate / bitrate（浮点）
- [ ] sample_point = (bit_width / 100.0) * 50（固定 50%）
- [ ] CRC 算法与 Python 版本完全一致
  - [ ] Header CRC：bits[4:24], 多项式 0x385, 初始值 0x01A, 11 位
  - [ ] Frame CRC：bits[1:-24], 多项式 0x5D6DCB, 初始值 0xFEDCBA(A)/0xABCDEF(B), 24 位
- [ ] IDLE 状态：等待低→高转换
- [ ] GET BITS 状态：采样点计算 + 下降沿时钟同步
- [ ] BSS 检测：`(num_rawbits-2)%10==0` 或 `(num_rawbits-3)%10==0`
- [ ] BSS 检测在 end_of_frame 时禁用
- [ ] bitnum==1 时 CAS 检测：rawbits[:3]==[1,1,1]
- [ ] putg() 扩展注释范围正确
- [ ] 21 个注释类全部定义
- [ ] 3 个注释行定义正确
- [ ] 参考 can_c.c 的类似架构

---

## MIPI RFFE (mipi_rffe_c) 专项检查

- [ ] 2 通道定义：SCLK (order=0, type=8) + SDATA (order=1, type=108)
- [ ] 1 个选项：error_display
- [ ] metadata 回调保存 samplerate
- [ ] FIND SSC 状态：SCLK 低 + SDATA 上升沿 → SCLK 低 + SDATA 下降沿 → SCLK 高
- [ ] handle() 函数：通用数据读取
  - [ ] _display 模式下 IJE 检测
  - [ ] SCLK 上升沿读取 SDATA
  - [ ] SCLK 下降沿结束
- [ ] handle_CMD() 函数：命令解码
  - [ ] R0W：bitcount==0 时 sdata=1
  - [ ] 基本/扩展区分
  - [ ] ERW/ERR/ERWL/ERRL/RW/RR 命令确定
- [ ] Parity() 函数：Pdata 调整逻辑
  - [ ] ERW: Pdata += 0*(2^(Pkey+1))
  - [ ] ERR: Pdata += 2*(2^(Pkey+1))
  - [ ] ERWL: Pdata += 6*(2^(Pkey+1))
  - [ ] ERRL: Pdata += 7*(2^(Pkey+1))
  - [ ] RW: Pdata += 2*(2^(Pkey+1))
  - [ ] RR: Pdata += 3*(2^(Pkey+1))
  - [ ] R0W: Pdata += 1*(2^(Pkey+1))
  - [ ] Popcount 奇偶校验
- [ ] FIND PARITY 状态转换表完整（7 种 cmdkey × 多种 Pcount）
- [ ] FIND ADDRESS 状态：根据 cmdkey 和 ADDcount 确定位数
- [ ] FIND DATA 状态：根据 cmdkey 和 bits 确定位数
- [ ] FIND BUS_PARK 状态：读/写操作不同处理
- [ ] BC 范围检查：ERW/ERR 1-16，其他 1-8
- [ ] cmdkey 使用枚举替代字符串
- [ ] 19 个注释类全部定义
- [ ] 2 个注释行定义正确
- [ ] OUTPUT_PYTHON 输出格式正确

---

## USB Power Delivery (usb_power_delivery_c) 专项检查

- [ ] 1 通道 + 1 可选通道：CC1 (order=0) + CC2 (optional, order=0)
- [ ] 1 个选项：fulltext
- [ ] metadata 回调计算 maxbit 和 threshold
  - [ ] maxbit = us2samples(3 * UI_US)
  - [ ] threshold = us2samples(THRESHOLD_US)
- [ ] BMC 解码正确
  - [ ] diff > maxbit → 包结束
  - [ ] diff > threshold + !half_one → bit=0
  - [ ] diff <= threshold + half_one → bit=1, half_one=False
  - [ ] diff <= threshold + !half_one → half_one=True
  - [ ] 其他 → 无效 BMC
- [ ] 4b5b 解码表 DEC4B5B[32] 完整正确
- [ ] SOP 序列检测
  - [ ] 7 种 SOP 序列完整
  - [ ] 3/4 容错匹配
  - [ ] Hard Reset / Cable Reset 返回 -1
- [ ] 包头解码
  - [ ] head_ext / head_count / head_id / head_power_role / head_rev / head_data_role / head_type
- [ ] 控制消息类型 CTRL_TYPES[25] 完整
- [ ] 数据消息类型 DATA_TYPES 完整
- [ ] 扩展消息类型 EXTENDED_TYPES 完整
- [ ] PDO 解码
  - [ ] Fixed PDO：mv/ma 计算，Source/Sink 标志不同
  - [ ] Battery PDO：minv/maxv/mw
  - [ ] Variable PDO：minv/maxv/ma
  - [ ] PPS APDO：minv/maxv/ma, power_limited 标志
- [ ] RDO 解码：pos/op_ma/max_ma/flags
- [ ] VDM 解码：结构化/非结构化
- [ ] BIST 解码
- [ ] EPR Mode 解码
- [ ] 扩展消息解码（分块传输）
  - [ ] chunk_num==0 首块处理
  - [ ] chunk_num>0 后续块处理
  - [ ] 扩展头字段：chunked/chunk_num/req_chunk/data_size
- [ ] CRC32 实现正确
  - [ ] 打包格式：小端 <H + <I*n
  - [ ] 与 zlib.crc32 结果一致
- [ ] EOP 检测
- [ ] PDO 存储（stored_pdos）和引用
- [ ] OUTPUT_BINARY 输出
- [ ] OUTPUT_META 输出（bitrate）
- [ ] fulltext 选项处理
- [ ] 可选通道 CC2 支持
- [ ] 超时检测：skip = samplerate/1000
- [ ] 13 个注释类全部定义
- [ ] 6 个注释行定义正确
- [ ] 1 个 binary 输出定义

---

## 编译与集成检查

- [ ] CMakeLists.txt 中 C_DECODERS 列表已添加所有 5 个解码器名
- [ ] `build_incremental.cmd` 编译成功
- [ ] 5 个 DLL 文件生成在 `build.dir/decoders/c_decoders/`
  - [ ] `flexray_c.dll` (或 `.so`)
  - [ ] `mipi_rffe_c.dll`
  - [ ] `usb_power_delivery_c.dll`
  - [ ] `iebus_c.dll`
  - [ ] `spacewire_c.dll`
- [ ] PXView 启动时无解码器加载错误
- [ ] 解码器在 UI 中正确显示名称和描述
- [ ] 通道/选项配置界面正确

---

## 回归测试检查

- [ ] 现有 37 个 C 解码器仍正常工作
- [ ] Python 版本的同名解码器仍可使用（C 版本为独立 ID）
- [ ] 无编译警告（-Wall -Wextra）
- [ ] 无内存泄漏（Valgrind/ASan 检查，如可用）
