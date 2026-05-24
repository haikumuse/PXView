# C解码器移植检查清单 — 第5批

每个解码器实现完成后，逐项检查以下内容。

---

## 通用检查项（适用于所有5个解码器）

### 文件结构
- [ ] 文件位于 `libsigrokdecode/c_decoders/<name>_c.c`
- [ ] 包含头文件：`"libsigrokdecode.h"`, `<glib.h>`, `<stdio.h>`, `<stdlib.h>`, `<string.h>`
- [ ] 注释枚举以`NUM_ANN`结尾
- [ ] 状态结构体命名为`<name>_state`
- [ ] 所有静态数组命名为`<name>_channels`, `<name>_options`等
- [ ] 解码器结构体命名为`<name>_c_decoder`
- [ ] 入口函数`srd_c_decoder_entry`和`srd_c_decoder_api_version`正确导出

### 元数据一致性
- [ ] id字段格式为`<name>_c`（如`adb_c`）
- [ ] name字段包含`(C)`后缀（如`ADB(C)`）
- [ ] longname与Python版本一致
- [ ] desc与Python版本一致，末尾可加`(C implementation)`
- [ ] license与Python版本一致
- [ ] 通道id/name/desc与Python版本完全一致
- [ ] 通道idn（如有）与Python版本一致
- [ ] 通道type正确（SCLK/SDATA/ADATA）
- [ ] 选项id/desc/default/values与Python版本一致
- [ ] 选项idn（如有）与Python版本一致
- [ ] 注释数量和顺序与Python版本完全一致
- [ ] 注释行id/label/class_tuple与Python版本一致
- [ ] inputs包含`"logic"`
- [ ] outputs与Python版本一致（空则NULL，有则列出）
- [ ] tags与Python版本一致

### 内存管理
- [ ] reset函数：首次调用时`g_malloc0`分配状态，之后`memset`清零
- [ ] destroy函数：`g_free`释放状态，`c_decoder_set_private(di, NULL)`
- [ ] 无内存泄漏（所有malloc对应free）
- [ ] srd_cond_builder每次wait后都c_cond_free

### 采样率处理
- [ ] 需要采样率的解码器在decode开头检查samplerate
- [ ] samplerate为0时安全返回（不崩溃）
- [ ] 时间转换使用double避免整数溢出

### 条件等待
- [ ] 每次c_cond_wait前创建新的c_cond_builder
- [ ] 每次c_cond_wait后c_cond_free
- [ ] 检查c_cond_wait返回值，非SRD_OK时return
- [ ] 不重用已wait的c_cond_builder

### 注释输出
- [ ] C_ANN_PUT宏参数正确（di, ss, es, out_id, cls, ...）
- [ ] 注释class索引不越界（< NUM_ANN）
- [ ] 格式字符串与Python版本一致（多级缩写）
- [ ] ss <= es（起始采样号不大于结束采样号）

### 编译
- [ ] 无编译警告
- [ ] 无编译错误
- [ ] DLL成功生成到build.dir/decoders/c_decoders/

---

## ADB特定检查项

- [ ] 14个注释枚举完整（LO, HI, ATTN, GRESET, BIT, DATA, START, STOP, SRQ, RESET, FLUSH, LISTEN, TALK, UNKNOWN）
- [ ] format选项4个值（hex, dec, oct, bin）正确初始化
- [ ] to_us时间转换：`sample * 1000000.0 / samplerate`
- [ ] 低电平4种判断阈值正确（<100, 100-500, 500-1500, >1500）
- [ ] 高电平判断：high<100为正常，>=100为停止位
- [ ] cell总长判断：<=130为位单元，>130为停止位/起始位
- [ ] 位判定：low>high为0，否则为1
- [ ] 字节移位：`((byte << 1) & 0xff) | bit`
- [ ] 命令字节解析：addr=(C>>4)&0x0f, cmd=C&0x0f, reg=C&0x03
- [ ] Listen判定：(cmd & 0x0c) == 0x08
- [ ] Talk判定：(cmd & 0x0c) == 0x0c
- [ ] attention标志：收到attention后设1，命令字节后清0
- [ ] bit_count=-1处理：命令字节后设-1，使下一个字节从bit 0开始
- [ ] 二进制格式化自定义函数实现
- [ ] putl/puth输出微秒整数值
- [ ] puta/putr/putQ格式字符串与Python一致

---

## AFSK特定检查项

- [ ] 3个注释枚举完整（BIT_RAW, BIT_ERROR, BIT_PHASE）
- [ ] 3个选项正确初始化（markfreq=2000, spacefreq=4000, marginpct=40）
- [ ] OUTPUT_PYTHON输出注册（proto_id="afsk_bits"）
- [ ] markhalfcycle计算：`(int64_t)(samplerate * (1.0/markfreq) / 2.0) - 1`
- [ ] markmargin计算：`(int64_t)(markhalfcycle * marginpct / 100.0)`
- [ ] spacehalfcycle/spacemargin类似计算
- [ ] 3个边沿采样号历史正确维护
- [ ] 半周期长度范围检查：`>= min && <= max`
- [ ] SPACE+SPACE→bit 0，输出范围twoedgesagosample到currentedgesample
- [ ] MARK+MARK→bit 1，输出范围同上
- [ ] ERROR→puterror，输出范围oneedgeagosample到currentedgesample
- [ ] 混合→putphaseerror，输出范围同上
- [ ] PROCESSED状态：匹配后设cycletype，下一周期不配对
- [ ] c_decoder_put_python调用格式正确（cmd="BIT"/"ERROR", data, data_len）
- [ ] 初始状态：所有边沿采样号为0，cycletype为IDLE

---

## AM230x特定检查项

- [ ] 8个注释枚举完整（START, RESPONSE, BIT, END, BYTE, HUMIDITY, TEMPERATURE, CHECKSUM）
- [ ] device选项2个值（am230x/rht, dht11）正确初始化
- [ ] 7种时序的微秒→采样数转换正确
- [ ] 8个状态的状态机完整实现
- [ ] is_valid函数：LOW时序用fall，HIGH时序用rise
- [ ] bits2num：LSB在数组末尾，`bits[count-1-i] * (1<<i)`
- [ ] 湿度计算：
  - DHT11：`bits2num(bits[0:8])`
  - AM230x：`bits2num16(bits[0:16]) / 10.0`
- [ ] 温度计算：
  - DHT11：`bits2num(bits[16:24])`
  - AM230x：`bits2num16(bits[17:32]) / 10.0`，bits[16]为1则取负
- [ ] 校验和：4字节相加模256，与第5字节比较
- [ ] putfs：从fall到samplenum
- [ ] putb：从bytepos[-1]到samplenum
- [ ] putv：从bytepos[-2]到samplenum
- [ ] bytepos管理：WAIT_FIRST_BIT时追加第一个，每8位追加一个
- [ ] °C符号使用UTF-8编码"\xc2\xb0C"
- [ ] 状态验证失败时reset_variables（不是reset整个解码器）
- [ ] WAIT_BIT_LOW中先判断BIT_0_HIGH再判断BIT_1_HIGH

---

## Caliper特定检查项

- [ ] 2个注释枚举完整（MEASUREMENT, WARNING）
- [ ] 2个通道正确（clk=SCLK, data=SDATA）
- [ ] 3个选项正确初始化（timeout_ms=10, unit=keep/keep/mm/inch, changes=no/yes）
- [ ] 通道idn与Python一致（dec_caliper_chan_clk, dec_caliper_chan_data）
- [ ] 选项idn与Python一致（dec_caliper_opt_timeout_ms, dec_caliper_opt_unit, dec_caliper_opt_changes）
- [ ] 超时条件：c_cond_rise + c_cond_or + c_cond_skip
- [ ] matched位掩码：bit 0为CLK上升沿，bit 1为超时
- [ ] 超时时输出Warning注释，格式与Python一致
- [ ] DATA引脚采样：`c_decoder_get_pin(di, 1, samplenum)`
- [ ] bitpack：MSB优先，`number = (number << 1) | bit`
- [ ] 负数处理：使用int32_t
- [ ] 英寸模式：number /= 2000
- [ ] 毫米模式：number /= 100
- [ ] 单位转换：mm→inch用`round(value/25.4*10000)/10000`
- [ ] 单位转换：inch→mm用`value * 25.4`
- [ ] changes_only模式：比较当前值和上次值
- [ ] ss初始值0判断
- [ ] 24位全部接收后才处理

---

## Carrera特定检查项

- [ ] 27个注释枚举完整（CONTROLLER_0到PROG_PERFORMANCE）
- [ ] 2个选项正确初始化（invert=nein/ja/nein, format=hex/hex/dec/oct/bin）
- [ ] dataWord初始值为1（不是0）
- [ ] 微秒时间计算：`samplenum * 1000000.0 / samplerate`
- [ ] 有效位判定：75 <= intervalMicros <= 125
- [ ] 字间隔判定：intervalMicros > 6000
- [ ] intervalMicros < 200时更新endDataWord
- [ ] invert选项：ja时bit=1，nein时bit=0
- [ ] 位判定：pin == bit时dataWord |= 1
- [ ] 控制器字：dataWord < 1024
  - [ ] regler_id = (dataWord >> 6) & 0x07
  - [ ] regler_id==2或7时设next_could_be_active_data_word
  - [ ] regler_id==7时为SC，提取5个标志位
  - [ ] 其他ID提取gas和wt
  - [ ] 注释索引=regler_id（0-6）
- [ ] 活动数据字：127 < dataWord < 256（需next_could_be_active_data_word）
  - [ ] 提取ie和r0-r5
  - [ ] 注释索引=8
- [ ] 确认字：dataWord < 512（需next_could_be_active_data_word）
  - [ ] 提取s0-s7
  - [ ] 注释索引=10
- [ ] 编程数据字：dataWord >= 1024
  - [ ] flip_bits正确实现
  - [ ] wert = flip_bits((dataWord>>8)&0x0f, 4)
  - [ ] befehl = flip_bits((dataWord>>3)&0x1f, 5)
  - [ ] regler = flip_bits((dataWord>>0)&0x07, 3)
  - [ ] 注释索引=11+befehl，越界保护
- [ ] format_data函数支持hex/dec/oct/bin
- [ ] 二进制格式化自定义实现
- [ ] prog_performance拼写修正（Python中有空格笔误）

---

## 构建集成检查

- [ ] CMakeLists.txt的C_DECODERS列表已添加5个新名称
- [ ] 增量构建成功（无编译错误/警告）
- [ ] 5个DLL文件生成到build.dir/decoders/c_decoders/
- [ ] DLL文件名格式正确（如adb_c.dll, afsk_c.dll等）

---

## 功能验证（如有测试信号）

- [ ] ADB：能识别Attention、Global Reset、命令字节、数据字节
- [ ] AFSK：能检测Mark/Space频率，正确输出bit 0/1
- [ ] AM230x：能完成完整的40位数据接收，正确计算湿度/温度/校验和
- [ ] Caliper：能接收24位数据，正确显示测量值和单位
- [ ] Carrera：能识别控制器字、活动数据字、确认字、编程数据字
