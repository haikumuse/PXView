# Python解码器移植任务清单 — 第5批

## 解码器列表

| # | 解码器 | Python源 | C目标文件 | 复杂度 | 优先级 |
|---|--------|----------|-----------|--------|--------|
| 1 | ADB | `libsigrokdecode/decoders/adb/pd.py` | `libsigrokdecode/c_decoders/adb_c.c` | 中 | 高 |
| 2 | AFSK | `libsigrokdecode/decoders/afsk/pd.py` | `libsigrokdecode/c_decoders/afsk_c.c` | 中 | 高 |
| 3 | AM230x | `libsigrokdecode/decoders/am230x/pd.py` | `libsigrokdecode/c_decoders/am230x_c.c` | 高 | 高 |
| 4 | Caliper | `libsigrokdecode/decoders/caliper/pd.py` | `libsigrokdecode/c_decoders/caliper_c.c` | 中 | 中 |
| 5 | Carrera | `libsigrokdecode/decoders/carrera/pd.py` | `libsigrokdecode/c_decoders/carrera_c.c` | 高 | 中 |

---

## 任务1: ADB解码器

### 1.1 创建文件
- [ ] 创建 `libsigrokdecode/c_decoders/adb_c.c`

### 1.2 元数据实现
- [ ] 定义14个注释枚举（ANN_LO到ANN_UNKNOWN）
- [ ] 定义通道数组（1个通道：data）
- [ ] 定义选项数组（1个选项：format，4个值）
- [ ] 定义注释标签数组（14行x3列）
- [ ] 定义3个注释行（cells, bits, bytes）
- [ ] 定义输入/输出/标签

### 1.3 状态结构体
- [ ] 定义adb_state结构体（samplerate, out_ann, format, cell_s, byte_val, bit_count, attention, byte_s）

### 1.4 核心逻辑
- [ ] 实现adb_reset：分配并清零状态
- [ ] 实现adb_start：注册输出、获取采样率和选项
- [ ] 实现adb_decode主循环：
  - [ ] 初始等待下降沿
  - [ ] 低电平阶段：等待上升沿，判断low时长（<100/100-500/500-1500/>1500）
  - [ ] 高电平阶段：等待下降沿，判断high时长和cell时长
  - [ ] 位判定：low>high为0，否则为1
  - [ ] 字节组装：8位一组
  - [ ] 命令/数据字节区分
- [ ] 实现adb_put_command：解析命令字节（Reset/Flush/Listen/Talk/Unknown）
- [ ] 实现adb_put_data：按格式输出数据字节
- [ ] 实现adb_destroy：释放状态内存

### 1.5 入口函数
- [ ] 实现srd_c_decoder_entry：初始化format选项的值列表
- [ ] 实现srd_c_decoder_api_version

### 1.6 特殊处理
- [ ] 实现自定义二进制格式化函数（%b不可用）
- [ ] 微秒时间转换函数
- [ ] bit_count=-1的处理（命令字节后重置）

---

## 任务2: AFSK解码器

### 2.1 创建文件
- [ ] 创建 `libsigrokdecode/c_decoders/afsk_c.c`

### 2.2 元数据实现
- [ ] 定义3个注释枚举（ANN_BIT_RAW, ANN_BIT_ERROR, ANN_BIT_PHASE）
- [ ] 定义通道数组（1个通道：afsk）
- [ ] 定义选项数组（3个选项：markfreq, spacefreq, marginpct）
- [ ] 定义注释标签数组（3行x3列）
- [ ] 定义2个注释行（raw-bits, errors）
- [ ] 定义输出数组（afsk_bits）

### 2.3 状态结构体
- [ ] 定义afsk_state结构体（samplerate, out_ann, out_python, 频率参数, 边沿历史, 周期类型）

### 2.4 核心逻辑
- [ ] 实现afsk_reset：分配并清零状态
- [ ] 实现afsk_start：
  - [ ] 注册OUTPUT_ANN和OUTPUT_PYTHON输出
  - [ ] 获取采样率
  - [ ] 计算markhalfcycle/markmargin/spacehalfcycle/spacemargin
- [ ] 实现afsk_decode主循环：
  - [ ] 等待任意边沿
  - [ ] 维护3个边沿采样号历史
  - [ ] 计算半周期长度
  - [ ] 判断SPACE/MARK/ERROR
  - [ ] 状态转换：SPACE+SPACE→bit0, MARK+MARK→bit1, ERROR→error, 混合→phase error
  - [ ] 输出注释和Python数据
- [ ] 实现afsk_destroy：释放状态内存

### 2.5 入口函数
- [ ] 实现srd_c_decoder_entry：初始化3个选项
- [ ] 实现srd_c_decoder_api_version

### 2.6 特殊处理
- [ ] OUTPUT_PYTHON注册和c_decoder_put_python调用
- [ ] 半周期采样数计算（注意-1偏移）
- [ ] PROCESSED状态处理

---

## 任务3: AM230x解码器

### 3.1 创建文件
- [ ] 创建 `libsigrokdecode/c_decoders/am230x_c.c`

### 3.2 元数据实现
- [ ] 定义8个注释枚举（ANN_START到ANN_CHECKSUM）
- [ ] 定义通道数组（1个通道：sda，带idn）
- [ ] 定义选项数组（1个选项：device，2个值）
- [ ] 定义注释标签数组（8行x3列）
- [ ] 定义3个注释行（bits, bytes, results）

### 3.3 状态结构体
- [ ] 定义am230x_state结构体（samplerate, out_ann, device, state, fall, rise, bits[40], bit_count, bytepos[5], timing cnt[7]）

### 3.4 核心逻辑
- [ ] 实现am230x_reset：分配并清零状态
- [ ] 实现am230x_start：注册输出、获取采样率和选项、计算时序采样数
- [ ] 实现am230x_decode主循环（8状态状态机）：
  - [ ] WAIT_START_LOW：等待下降沿
  - [ ] WAIT_START_HIGH：等待上升沿，验证START LOW
  - [ ] WAIT_RESPONSE_LOW：等待下降沿，验证START HIGH，输出Start
  - [ ] WAIT_RESPONSE_HIGH：等待上升沿，验证RESPONSE LOW
  - [ ] WAIT_FIRST_BIT：等待下降沿，验证RESPONSE HIGH，输出Response
  - [ ] WAIT_BIT_HIGH：等待上升沿，验证BIT LOW
  - [ ] WAIT_BIT_LOW：等待下降沿，判断BIT 0/1 HIGH，handle_byte
  - [ ] WAIT_END：等待上升沿，输出End
- [ ] 实现handle_byte逻辑：
  - [ ] 追加bit，输出Bit注释
  - [ ] 每8位：计算字节值，输出Byte注释
  - [ ] 16位：计算湿度，输出Humidity注释
  - [ ] 32位：计算温度，输出Temperature注释
  - [ ] 40位：验证校验和，输出Checksum注释
- [ ] 实现is_valid：时序验证
- [ ] 实现bits2num/bits2num16：位转整数
- [ ] 实现am230x_destroy：释放状态内存

### 3.5 入口函数
- [ ] 实现srd_c_decoder_entry：初始化device选项
- [ ] 实现srd_c_decoder_api_version

### 3.6 特殊处理
- [ ] 时序微秒→采样数转换
- [ ] DHT11与AM230x湿度/温度计算差异
- [ ] 温度负值处理
- [ ] °C UTF-8编码
- [ ] putfs/putb/putv的采样范围计算
- [ ] 校验和计算

---

## 任务4: Caliper解码器

### 4.1 创建文件
- [ ] 创建 `libsigrokdecode/c_decoders/caliper_c.c`

### 4.2 元数据实现
- [ ] 定义2个注释枚举（ANN_MEASUREMENT, ANN_WARNING）
- [ ] 定义通道数组（2个通道：clk, data，带idn）
- [ ] 定义选项数组（3个选项：timeout_ms, unit, changes）
- [ ] 定义注释标签数组（2行x3列）
- [ ] 定义2个注释行（measurements, warnings）

### 4.3 状态结构体
- [ ] 定义caliper_state结构体（samplerate, out_ann, timeout_ms, unit, changes_only, ss, es, number_bits[16], number_count, flags_bits[8], flags_count, last_number, last_is_inch, has_last）

### 4.4 核心逻辑
- [ ] 实现caliper_reset：分配并清零状态
- [ ] 实现caliper_start：注册输出、获取采样率和选项
- [ ] 实现caliper_decode主循环：
  - [ ] 等待CLK上升沿（可选超时）
  - [ ] 超时处理：输出Warning，reset
  - [ ] 采样DATA引脚
  - [ ] 收集16位数值+8位标志
  - [ ] 数据处理：bitpack、负号、单位转换
  - [ ] 变化检测
  - [ ] 输出Measurement注释
- [ ] 实现caliper_destroy：释放状态内存

### 4.5 入口函数
- [ ] 实现srd_c_decoder_entry：初始化3个选项
- [ ] 实现srd_c_decoder_api_version

### 4.6 特殊处理
- [ ] 超时条件：c_cond_or + c_cond_skip
- [ ] matched位掩码判断超时
- [ ] bitpack（MSB优先）
- [ ] 负数处理（int32_t）
- [ ] 单位转换（mm↔inch，mm_per_inch=25.4）
- [ ] round函数（4位小数精度）

---

## 任务5: Carrera解码器

### 5.1 创建文件
- [ ] 创建 `libsigrokdecode/c_decoders/carrera_c.c`

### 5.2 元数据实现
- [ ] 定义27个注释枚举（ANN_CONTROLLER_0到ANN_PROG_PERFORMANCE）
- [ ] 定义通道数组（1个通道：data）
- [ ] 定义选项数组（2个选项：invert, format）
- [ ] 定义注释标签数组（27行x3列）
- [ ] 定义4个注释行（bits, Reglerwort, Aktiv-/Quittierungswort, Programmierdatenwort）

### 5.3 状态结构体
- [ ] 定义carrera_state结构体（samplerate, out_ann, invert, format, 时间变量, dataWord, 位置变量, next_could_be_active_data_word）

### 5.4 核心逻辑
- [ ] 实现carrera_reset：分配并清零状态
- [ ] 实现carrera_start：注册输出、获取采样率和选项
- [ ] 实现carrera_decode主循环：
  - [ ] 等待任意边沿
  - [ ] 计算微秒时间间隔
  - [ ] 有效位判定（75-125us）
  - [ ] 字间隔判定（>6000us）
  - [ ] 数据字分类和输出
- [ ] 实现carrera_print_reglerdatenwort：控制器字输出
- [ ] 实现carrera_print_aktivdatenwort：活动数据字输出
- [ ] 实现carrera_print_quittierungswort：确认字输出
- [ ] 实现carrera_print_programmierdatenwort：编程数据字输出
- [ ] 实现carrera_flip_bits：位反转
- [ ] 实现carrera_get_value：位域提取
- [ ] 实现carrera_format_data：hex/dec/oct/bin格式化
- [ ] 实现carrera_destroy：释放状态内存

### 5.5 入口函数
- [ ] 实现srd_c_decoder_entry：初始化2个选项
- [ ] 实现srd_c_decoder_api_version

### 5.6 特殊处理
- [ ] dataWord初始值为1（不是0）
- [ ] invert选项影响位判定逻辑
- [ ] flip_bits位反转
- [ ] 编程字注释索引=11+befehl，需越界保护
- [ ] 二进制格式化需自定义
- [ ] 微秒时间计算
- [ ] prog_performance拼写修正（Python中有空格笔误）

---

## 任务6: 构建集成

- [ ] 在CMakeLists.txt的C_DECODERS列表中添加：adb_c, afsk_c, am230x_c, caliper_c, carrera_c
- [ ] 执行增量构建验证编译
- [ ] 验证DLL生成到build.dir/decoders/c_decoders/

---

## 依赖关系

- 任务1-5相互独立，可并行实现
- 任务6依赖任务1-5全部完成
- 建议实现顺序：Caliper(最简单) → ADB → AFSK → AM230x → Carrera(最复杂)
