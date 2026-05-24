# 实现验证清单 — Batch 08 Python→C 解码器移植

## 通用检查项 (适用于所有5个解码器)

### 文件结构
- [ ] 文件位于 `libsigrokdecode/c_decoders/<name>_c.c`
- [ ] 包含正确的头文件: `libsigrokdecode.h`, `<glib.h>`, `<stdio.h>`, `<stdlib.h>`, `<string.h>`
- [ ] 状态枚举定义完整
- [ ] 注解枚举定义完整，最后一个为 NUM_ANN
- [ ] 私有结构体包含所有必要字段
- [ ] 通道数组定义正确 (id, name, desc, order, type, idn)
- [ ] 选项数组声明为 `static struct srd_decoder_option xxx_options_arr[N]`
- [ ] 注解标签数组格式正确: `static const char* xxx_ann_labels[][3]`
- [ ] 注解行定义正确，classes 数组以 -1 结尾
- [ ] 输入/输出/标签数组以 NULL 结尾

### 回调函数
- [ ] `metadata()` — 正确处理 SRD_CONF_SAMPLERATE
- [ ] `reset()` — 首次分配内存 (g_malloc0)，后续 memset 清零
- [ ] `start()` — 注册输出、读取选项、缓存 samplerate
- [ ] `decode()` — 主解码循环，正确使用条件构建器
- [ ] `destroy()` — 释放私有数据 (g_free)

### 解码器注册
- [ ] `srd_c_decoder` 结构体所有字段正确填充
- [ ] `srd_c_decoder_entry()` 正确初始化选项 (GVariant, GSList)
- [ ] `srd_c_decoder_api_version()` 返回 SRD_C_DECODER_API_VERSION
- [ ] 两个导出函数使用 SRD_C_DECODER_EXPORT 宏

### CMakeLists.txt
- [ ] 解码器名称已添加到 C_DECODERS 列表

### 编译
- [ ] 无编译警告
- [ ] 无编译错误
- [ ] DLL 生成到 build.dir/decoders/c_decoders/

---

## ir_recoil_c 专项检查

### 元数据匹配
- [ ] id = "ir_recoil_c"
- [ ] name = "IR Recoil(C)"
- [ ] longname = "Recoil laser tag IR (C)"
- [ ] desc 包含 "C implementation"
- [ ] license = "unknown"
- [ ] tags = ["Embedded/industrial"]

### 通道
- [ ] 1个必需通道: ir (SRD_CHANNEL_SDATA)
- [ ] 无可选通道

### 选项
- [ ] polarity: default="active-low", values=["active-low", "active-high"]
- [ ] 无 idn (与 Python 源码一致)

### 注解
- [ ] 4个注解类: sync, sync-pause, bit, packet
- [ ] 2个注解行: bits(0,1,2), packets(3)

### 时间参数
- [ ] margin = samplerate * 0.0002 - 1
- [ ] sync = samplerate * 0.0033 - 1
- [ ] syncpause = samplerate * 0.0015 - 1
- [ ] dazero = samplerate * 0.0004 - 1
- [ ] daone = samplerate * 0.0008 - 1
- [ ] dathreshold = samplerate * 0.00059 - 1
- [ ] daminimum = samplerate * 0.0002 - 1
- [ ] damaximum = samplerate * 0.0012 - 1

### 状态机
- [ ] IDLE: 检查 sync 脉冲 (长度+oldpinstate==activeState)
- [ ] SYNCING: 检查 syncpause
- [ ] DATA: 阈值判断位值 [daminimum,dathreshold)=0, [dathreshold,damaximum)=1
- [ ] DATA 等待条件包含 skip: damaximum + margin
- [ ] 位错误或超时 → 输出包注解 → IDLE

### 数据格式
- [ ] 数据用字符串累积 (非位移)
- [ ] 包注解格式: "Packet, %d bits: 0b%s" / "Pack, %d: 0b%s" / "P %d: 0b%s"
- [ ] 包注解范围: packetstartsample 到 oldedgesample + 1

---

## ir_ltto_c 专项检查

### 元数据匹配
- [ ] id = "ir_ltto_c"
- [ ] name = "IR LTTO(C)"
- [ ] longname = "LTTO laser tag IR (C)"
- [ ] license = "unknown"
- [ ] tags = ["Embedded/industrial"]

### 通道
- [ ] 1个必需通道: ir
- [ ] 无可选通道

### 选项
- [ ] polarity: default="active-low", values=["active-low", "active-high"]
- [ ] 无 idn

### 注解
- [ ] 9个注解类: pre-sync, pre-sync-pause, sync, long-sync, bit-pause, bit, signature, long-sync-signature, error
- [ ] 2个注解行: bits(0-5), signatures(6-8)

### 时间参数
- [ ] margin = samplerate * 0.0005 - 1
- [ ] presync = samplerate * 0.003 - 1
- [ ] presyncpause = samplerate * 0.006 - 1
- [ ] sync = samplerate * 0.003 - 1
- [ ] longsync = samplerate * 0.006 - 1
- [ ] bitpause = samplerate * 0.002 - 1
- [ ] dazero = samplerate * 0.001 - 1
- [ ] daone = samplerate * 0.002 - 1

### 状态机
- [ ] IDLE: 检查 presync 脉冲 (长度+oldpinstate==activeState)
- [ ] PSP: 检查 presyncpause
- [ ] SYNC: 检查 sync 或 longsync
- [ ] BITPAUSE: 检查 bitpause; 超时时根据 count/waslongsync 输出
- [ ] BIT: 精确范围匹配 [dazero-margin, dazero+margin]=0, [daone-margin, daone+margin]=1
- [ ] BIT/BITPAUSE 等待条件包含 skip: bitpause + margin + margin

### 数据格式
- [ ] 数据用位移累积: data = (data << 1) | bit
- [ ] 签名格式: "Signature, %d bits: 0x%03X" (3位十六进制)
- [ ] Long SYNC 签名格式: "Signature, long SYNC, %d bits: 0x%03X"
- [ ] 错误注解: "Error" / "Err" / "E"

### Python 输出
- [ ] 注册 SRD_OUTPUT_PYTHON 输出
- [ ] SHORT 签名: c_decoder_put_python(di, ss, es, out_python, "SHORT", data_bytes, len)
- [ ] LONG 签名: c_decoder_put_python(di, ss, es, out_python, "LONG", data_bytes, len)

---

## ir_rc6_c 专项检查

### 元数据匹配
- [ ] id = "ir_rc6_c"
- [ ] name = "IR RC-6(C)"
- [ ] longname = "IR RC-6(C)"
- [ ] license = "gplv2+"
- [ ] tags = ["IR"]

### 通道
- [ ] 1个必需通道: ir (idn: dec_ir_rc6_chan_ir)
- [ ] 无可选通道

### 选项
- [ ] polarity: default="auto", values=["auto", "active-low", "active-high"]
- [ ] idn: dec_ir_rc6_opt_polarity

### 注解
- [ ] 7个注解类: bit, sync, startbit, field, togglebit, address, command
- [ ] 2个注解行: bits(0), fields(1-6)

### 时间参数
- [ ] halfbit = (uint64_t)((double)samplerate * 0.000889 / 2.0)

### 同步检测
- [ ] deltas 最后两个值 == [6, 2]
- [ ] 同步位: width=8, value=1 (必须)
- [ ] 起始位: value=1 (必须)
- [ ] auto 极性: invert = (ir == 0)

### 位处理
- [ ] 位元组: (start_sample, end_sample, width_in_halfbits, value)
- [ ] 边界插入: deltas[-2] != deltas[-1] 时拆分
- [ ] DATA 状态超时: skip = halfbit * 6
- [ ] delta 四舍五入: (uint64_t)(delta_double + 0.5)

### 包处理
- [ ] Mode 0: 22位, 8位地址 + 8位命令, 格式 Address: %0.2X, Data: %0.2X
- [ ] Mode 6A: bits[6]==0, 8位地址 + 可变数据, 格式 Address: %0.2X, Data: %X
- [ ] Mode 6B: bits[6]==1, 16位地址 + 可变数据, 格式 Address: %0.2X, Data: %X
- [ ] 同步位和起始位值为0时跳过包处理

### 极性
- [ ] auto: 同步检测后确定 invert
- [ ] 数据阶段: value = ir if invert else 1-ir
- [ ] 非 auto: value = ir if active-low else 1-ir

---

## ir_irmp_c 专项检查

### 元数据匹配
- [ ] id = "ir_irmp_c"
- [ ] name = "IR IRMP(C)"
- [ ] longname = "IR IRMP(C)"
- [ ] license = "gplv2+"
- [ ] tags = ["IR"]

### 通道
- [ ] 1个必需通道: ir (idn: dec_ir_irmp_chan_ir)
- [ ] 无可选通道

### 选项
- [ ] polarity: default="active-low", values=["active-low", "active-high"]
- [ ] idn: dec_ir_irmp_opt_polarity

### 注解
- [ ] 1个注解类: packet
- [ ] 1个注解行: packets(0)

### 外部库
- [ ] 动态加载 irmp.dll (Windows) / libirmp.so (Linux)
- [ ] 所有函数指针正确获取 (GetProcAddress / dlsym)
- [ ] ResultData 结构体与库兼容
- [ ] 库不可用时优雅失败 (不崩溃)
- [ ] 实例分配和释放正确

### 采样率
- [ ] 获取库采样率: irmp_get_sample_rate()
- [ ] 验证: samplerate % lib_rate == 0
- [ ] rate_factor = samplerate / lib_rate
- [ ] start_sample/end_sample 乘以 rate_factor

### 解码循环
- [ ] 逐样本送入: c_cond_skip(rate_factor)
- [ ] 极性取反: active-low 时 ir = 1 - ir
- [ ] 检测到帧时输出注解

### 注解格式
- [ ] 5个缩放级别
- [ ] Protocol name, Address (0x%04x), Command (0x%04x), Flags
- [ ] Flags: repeat='rep'/'r', release='rel'/'R', 无='-'

---

## ieee488_c 专项检查

### 元数据匹配
- [ ] id = "ieee488_c"
- [ ] name = "IEEE-488(C)"
- [ ] longname = "IEEE-488 GPIB/HPIB/IEC (C)"
- [ ] license = "gplv2+"
- [ ] tags = ["PC", "Retro computing"]

### 通道
- [ ] 1个必需通道: dio1 (idn: dec_ieee488_chan_dio1)
- [ ] 16个可选通道: dio2-dio8, eoi, dav, nrfd, ndac, ifc, srq, atn, ren, clk
- [ ] 所有可选通道的 idn 正确

### 选项
- [ ] iec_periph: default="no", values=["no", "yes"], idn: dec_ieee488_opt_iec_periph
- [ ] delim: default="eol", values=["none", "eol"], idn: dec_ieee488_opt_delim

### 注解
- [ ] 11个注解类
- [ ] 7个注解行
- [ ] 2个二进制输出

### 输出注册
- [ ] SRD_OUTPUT_ANN
- [ ] SRD_OUTPUT_BINARY
- [ ] SRD_OUTPUT_PYTHON

### 信号取反
- [ ] 所有引脚值取反 (1-p)
- [ ] 未连接引脚 (值不在0/1范围) 不取反

### 串行模式
- [ ] 4状态机: WAIT_READY_TO_SEND → WAIT_READY_FOR_DATA → PREP_DATA_TEST_EOI → CLOCK_DATA_BITS
- [ ] ATN 下降沿重置状态
- [ ] 8位累积后调用 inject_dav_phase
- [ ] 位注解输出 (IEC bit)
- [ ] EOI 在串行模式中的处理

### 并行模式
- [ ] 动态等待条件构建 (DAV + ATN + EOI + IFC)
- [ ] 首次 'l' 触发 → 后续 'e' 触发
- [ ] 处理顺序: IFC↑ → EOI↑ → ATN↑ → DAV → ATN↓ → EOI↓ → IFC↓
- [ ] bitpack() 正确实现

### 数据字节处理
- [ ] ATN 激活: 命令/地址分类和注解
- [ ] ATN 非激活: 数据累积和文本格式化
- [ ] 命令表完整 (10个已知命令 + UNL/UNT + 未知命令)
- [ ] 地址格式化 (Listen/Talk/Secondary/MSB)

### 文本累积器
- [ ] accu_bytes 和 accu_text 正确管理
- [ ] 刷新条件: EOI↓, ATN↑, IFC↑, EOL 分隔
- [ ] BIN_DATA 二进制输出
- [ ] TEXT 注解输出

### Python 输出 (11种)
- [ ] IEC_BIT
- [ ] GPIB_RAW
- [ ] COMMAND
- [ ] LISTEN
- [ ] TALK
- [ ] SECONDARY
- [ ] MSB_SET
- [ ] DATA_BYTE
- [ ] TALK_LISTEN
- [ ] TALKER_BYTES
- [ ] TALKER_TEXT

### IEC 外设 (可选)
- [ ] 仅在 iec_periph=yes 时激活
- [ ] Commodore 磁盘地址映射 (8=Disk 0, 9=Disk 1)
- [ ] 副地址子命令 (0x60=Reopen, 0xe0=Close, 0xf0=Open)

### ASCII 控制码表
- [ ] 0x00-0x1f 完整映射 (NUL, SOH, STX, ..., US)
- [ ] 可打印字符 (0x20-0x7e, 排除 [ 和 ]) 直接输出
- [ ] 其他: [xx] 格式

---

## 功能测试检查

### ir_recoil_c
- [ ] 能检测 SYNC 脉冲 (3.3ms)
- [ ] 能检测 SYNC PAUSE (1.5ms)
- [ ] 能正确判断 0/1 位
- [ ] 包注解正确显示位数和二进制数据
- [ ] 位错误时正确终止包

### ir_ltto_c
- [ ] 能检测 PRE-SYNC 脉冲 (3ms)
- [ ] 能检测 PRE-SYNC PAUSE (6ms)
- [ ] 能区分 SYNC (3ms) 和 LONG-SYNC (6ms)
- [ ] 能正确判断 0/1 位
- [ ] SHORT 签名和 LONG 签名正确输出
- [ ] Python 输出格式正确

### ir_rc6_c
- [ ] 能检测同步模式 (deltas [6, 2])
- [ ] auto 极性正确工作
- [ ] Mode 0 包正确解析 (8位地址 + 8位命令)
- [ ] Mode 6A/6B 包正确解析
- [ ] 位注解和字段注解正确

### ir_irmp_c
- [ ] 能加载 irmp.dll
- [ ] 采样率验证正确
- [ ] 能检测红外协议帧
- [ ] 注解格式正确 (Protocol/Address/Command/Flags)
- [ ] 库不可用时优雅失败

### ieee488_c
- [ ] 串行模式 (IEC) 正确解码
- [ ] 并行模式 (GPIB) 正确解码
- [ ] 命令/地址/数据正确分类
- [ ] EOI 注解正确
- [ ] 文本累积和刷新正确
- [ ] Python 输出格式正确
- [ ] IEC 外设选项正确工作
