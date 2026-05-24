# 任务列表 — Batch 14: Python Decoder 移植到 C

## 总览

将 5 个 Python decoder 移植为 C decoder，按难度从低到高排序。

---

## Task 1: stepper_motor_c — 步进电机解码器

**优先级:** High | **难度:** ⭐ | **预估时间:** 1h

### 1.1 创建文件 `libsigrokdecode/c_decoders/stepper_motor_c.c`

- [ ] 添加版权头和 includes
- [ ] 定义 `enum stepper_ann` (ANN_SPEED=0, ANN_POSITION=1, NUM_ANN=2)
- [ ] 定义 channel 数组 `stepper_channels[]` (step, dir)
- [ ] 定义 option 数组 `stepper_options_arr[]` (unit, steps_per_mm)
- [ ] 定义 `ann_labels` (2 classes, 第一列为 "")
- [ ] 定义 annotation_rows (speed, position)
- [ ] 定义 inputs/outputs/tags
- [ ] 定义 `stepper_state` struct
- [ ] 实现 `stepper_reset()` — g_malloc0 + memset
- [ ] 实现 `stepper_start()` — 注册 output, 读取 options, 计算 scale
- [ ] 实现 `stepper_metadata()` — 保存 samplerate
- [ ] 实现 `stepper_decode()` — 等待 step 上升沿, 计算 speed/position, 输出 annotation
- [ ] 实现 `stepper_destroy()` — g_free
- [ ] 定义 `stepper_motor_c_decoder` struct
- [ ] 实现 `srd_c_decoder_entry()` — 初始化 options (unit: string enum, steps_per_mm: double)
- [ ] 实现 `srd_c_decoder_api_version()`

### 1.2 构建集成

- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `stepper_motor_c`

### 1.3 验证

- [ ] 编译通过
- [ ] 在 PXView 中加载 decoder，确认 channel 分配正确
- [ ] 使用 step/dir 信号测试，确认 speed 和 position annotation 正确

---

## Task 2: rgb_led_ws281x_c — WS281x RGB LED 解码器

**优先级:** High | **难度:** ⭐⭐⭐ | **预估时间:** 3h

### 2.1 创建文件 `libsigrokdecode/c_decoders/rgb_led_ws281x_c.c`

- [ ] 添加版权头和 includes
- [ ] 定义 `enum ws281x_ann` (ANN_BIT=0, ANN_RESET=1, ANN_RGB=2, NUM_ANN=3)
- [ ] 定义 `enum ws281x_state` (STATE_FIND_RESET, STATE_RESET, STATE_BIT_FALLING, STATE_BIT_RISING)
- [ ] 定义 `enum ws281x_color_mode` (MODE_GRB=0, MODE_RGB, MODE_BRG, MODE_RBG, MODE_BGR, MODE_GRBW, MODE_RGBW, MODE_WRGB, MODE_LBGR, MODE_LGRB, MODE_LRGB, MODE_LRBG, MODE_LGBR, MODE_LBRG)
- [ ] 定义 channel 数组 `ws281x_channels[]` (din)
- [ ] 定义 option 数组 `ws281x_options_arr[]` (colors, polarity)
- [ ] 定义 `ann_labels` (3 classes)
- [ ] 定义 annotation_rows (bit, rgb)
- [ ] 定义 inputs/outputs/tags
- [ ] 定义 `ws281x_state` struct (含 bits[32], colorsize, color_mode, polarity, 时序阈值)
- [ ] 实现 `ws281x_reset()`
- [ ] 实现 `ws281x_start()` — 注册 output, 读取 options, 计算 colorsize
- [ ] 实现 `ws281x_metadata()` — 保存 samplerate, 计算时序阈值 (625ns, 50μs, 3μs)
- [ ] 实现 `check_bit()` — 根据 tH 和 period 判断 bit 值
- [ ] 实现 `ws281x_output_color()` — 根据 color_mode 重排字节并格式化输出
- [ ] 实现 `ws281x_decode()` — 4 状态状态机
  - [ ] STATE_FIND_RESET: 等待低电平, 检查脉宽
  - [ ] STATE_RESET: 输出 RESET annotation
  - [ ] STATE_BIT_FALLING: 检测 bit 值, 判断是否为 RESET
  - [ ] STATE_BIT_RISING: 检测 bit 值, 输出 annotation
- [ ] 实现 `ws281x_destroy()`
- [ ] 定义 `rgb_led_ws281x_c_decoder` struct
- [ ] 实现 `srd_c_decoder_entry()` — 初始化 options
  - [ ] colors: 14 个 string 值的 GSList
  - [ ] polarity: 2 个 string 值的 GSList
- [ ] 实现 `srd_c_decoder_api_version()`

### 2.2 颜色格式化实现

- [ ] 24-bit 模式: GRB, RGB, BRG, RBG, BGR (5 种)
- [ ] 32-bit 模式: GRBW, RGBW, WRGB, LBGR, LGRB, LRGB, LRBG, LGBR, LBRG (9 种)
- [ ] 每种模式的字节重排逻辑

### 2.3 构建集成

- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `rgb_led_ws281x_c`

### 2.4 验证

- [ ] 编译通过
- [ ] 使用 WS2812 信号测试，确认 bit/RESET/RGB annotation 正确
- [ ] 测试不同 colors 选项
- [ ] 测试 inverted polarity

---

## Task 3: bean_c — Toyota BEAN 协议解码器

**优先级:** High | **难度:** ⭐⭐⭐⭐ | **预估时间:** 4h

### 3.1 创建文件 `libsigrokdecode/c_decoders/bean_c.c`

- [ ] 添加版权头和 includes
- [ ] 定义 `enum bean_ann` (9 classes: ANN_BIT_0..ANN_ALL_BYTE)
- [ ] 定义 channel 数组 `bean_channels[]` (data)
- [ ] 定义 option 数组 `bean_options_arr[]` (4 options)
- [ ] 定义 `ann_labels` (9 classes)
- [ ] 定义 annotation_rows (7 rows)
- [ ] 定义 inputs/outputs/tags
- [ ] 定义 `bean_state` struct (含 bits[], bits_ann[], 选项标志)
- [ ] 硬编码 `bean_commands[]` 命令查找表
- [ ] 实现 `bean_reset()` — 完全重置
- [ ] 实现 `bean_reset_frame()` — 仅重置帧数据
- [ ] 实现 `bean_start()` — 注册 output, 读取 options
- [ ] 实现 `bean_metadata()` — 保存 samplerate
- [ ] 实现 `pinlabels()` — 返回 DATA 字段标签
- [ ] 实现 `bean_lookup_command()` — 在查找表中搜索命令
- [ ] 实现 `bean_parse_frame()` — 解析完整帧
  - [ ] 提取 PRI (bits 0-3)
  - [ ] 提取 ML (bits 4-7)
  - [ ] 按字节提取 DST-ID, MES-ID, DATA, CRC, EOM
  - [ ] 输出 bit annotations
  - [ ] 输出 bit_ann annotations (SOF/Stuff, 受选项控制)
  - [ ] 输出 byte annotations
  - [ ] 输出 byte_ann annotations (PRI/ML/DST-ID/MES-ID/DATA/CRC/EOM)
  - [ ] 输出 all_byte annotations (受选项控制)
  - [ ] 输出 command annotation (受选项控制)
- [ ] 实现 `bean_decode()` — 脉宽解码主循环
  - [ ] 等待 data edge
  - [ ] 计算脉宽
  - [ ] 短脉冲 (≤150): 单 bit / SOF / stuff
  - [ ] 中等脉冲 (150-650): 多 bit 拆分 + bit stuffing
  - [ ] 长脉冲 (≥650): EOF / 帧间隔
  - [ ] EOM 检测 (count==6)
  - [ ] RSP 检测
  - [ ] draw 标志触发帧解析
- [ ] 实现 `bean_destroy()`
- [ ] 定义 `bean_c_decoder` struct
- [ ] 实现 `srd_c_decoder_entry()` — 初始化 4 个 options
- [ ] 实现 `srd_c_decoder_api_version()`

### 3.2 构建集成

- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `bean_c`

### 3.3 验证

- [ ] 编译通过
- [ ] 使用 BEAN 信号测试帧解码
- [ ] 验证 bit stuffing 逻辑
- [ ] 验证命令查找
- [ ] 测试各选项开关

---

## Task 4: ccd_c — Chrysler CCD 协议解码器

**优先级:** High | **难度:** ⭐⭐⭐⭐⭐ | **预估时间:** 5h

### 4.1 创建文件 `libsigrokdecode/c_decoders/ccd_c.c`

- [ ] 添加版权头和 includes
- [ ] 定义 `enum ccd_ann` (7 classes: ANN_BUS_BITS..ANN_BUS_MESSAGE)
- [ ] 定义 `enum ccd_uart_state` (UART_WAIT_START, UART_GET_DATA, UART_GET_STOP)
- [ ] 定义 `enum ccd_idle_state` (IDLE_IDLE, IDLE_BUSY)
- [ ] 定义 channel 数组 `ccd_channels[]` (bus)
- [ ] 定义 option 数组 `ccd_options_arr[]` (3 options)
- [ ] 定义 `ann_labels` (7 classes)
- [ ] 定义 annotation_rows (6 rows)
- [ ] 定义 inputs/outputs/tags
- [ ] 定义 `ccd_state` struct (含 UART 状态, IDLE 状态, 消息缓冲, VIN)
- [ ] 实现 `ccd_reset()`
- [ ] 实现 `ccd_start()` — 注册 output, 读取 options
- [ ] 实现 `ccd_metadata()` — 保存 samplerate, 计算 bit_width
- [ ] 实现 `ccd_decode_message()` — 解码 CCD 消息内容
  - [ ] 0x24: Speed
  - [ ] 0xE4: RPM + MAP
  - [ ] 0x6D: VIN
  - [ ] 0x86: Door lock/alarm
  - [ ] 0x42: TPS/Cruise
  - [ ] 0x35: Ignition switch
  - [ ] 0xA4: Instrument cluster lamps
  - [ ] 0x8C: Temperatures
  - [ ] 0x84: Increment odometer
  - [ ] 0x7B: Ambient temperature
  - [ ] 0x82: Steering wheel
  - [ ] 0x8E: Doors
  - [ ] 0xFE: PWM lamp dim
  - [ ] 0xEE: Trip distance
  - [ ] 0x50: Airbag lamp
  - [ ] 0x25: Fuel level
  - [ ] 0x0c: Voltage/temperatures/oil
  - [ ] 0xDA: Check engine lamp
  - [ ] 0xCE: Odometer
  - [ ] 0x62: Electric doors/mirrors
  - [ ] 其他: Unknown message
  - [ ] 日志: 输出完整消息 bytes
- [ ] 实现 `ccd_decode()` — 双层状态机
  - [ ] 构建多条件 wait (edge + UART skip + IDLE skip)
  - [ ] IDLE/BUSY 状态管理
  - [ ] UART 解码 (start bit, 8 data bits LSB first, stop bit)
  - [ ] Checksum 验证
  - [ ] Frame error 检测
- [ ] 实现 `ccd_destroy()`
- [ ] 定义 `ccd_c_decoder` struct
- [ ] 实现 `srd_c_decoder_entry()` — 初始化 3 个 options
- [ ] 实现 `srd_c_decoder_api_version()`

### 4.2 关键实现细节

- [ ] 动态 skip 值: 每次循环重新计算 `waituart` 和 `waitidle` 的 skip 值
- [ ] 多条件 wait: 使用 `c_cond_or()` 组合 edge + skip 条件
- [ ] bit_width 计算: `ceil(samplerate / 7812.5)`
- [ ] UART 采样: start bit 后 1.5*bit_width，然后每 bit_width
- [ ] BUSY→IDLE: bus 高电平持续 > 10*bit_width

### 4.3 构建集成

- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `ccd_c`

### 4.4 验证

- [ ] 编译通过
- [ ] 使用 CCD 信号测试 UART 解码
- [ ] 验证 IDLE/BUSY 状态转换
- [ ] 验证 checksum
- [ ] 验证消息解码（至少 speed 和 RPM）
- [ ] 测试 invert_bus 选项

---

## Task 5: cjtag_oscan0_c — cJTAG OScan1 解码器

**优先级:** High | **难度:** ⭐⭐⭐⭐⭐ | **预估时间:** 6h

### 5.1 创建文件 `libsigrokdecode/c_decoders/cjtag_oscan0_c.c`

- [ ] 添加版权头和 includes
- [ ] 定义 `enum jtag_state` (16 states, 复用 jtag_c.c 的定义)
- [ ] 定义 `enum cjtag_ann` (22 classes: 16 JTAG states + bit-tdi/tdo + bitstring-tdi/tdo + bit-tms + state-tapc)
- [ ] 定义 `enum cjtag_mode` (MODE_4WIRE, MODE_CJTAG_OAC, MODE_CJTAG_EC, ..., MODE_OSCAN1)
- [ ] 定义 channel 数组 `cjtag_channels[]` (tdi, tdo, tck, tms)
- [ ] 定义 optional_channels 数组 `cjtag_optional_channels[]` (trst, srst, rtck)
- [ ] 定义 `ann_labels` (22 classes)
- [ ] 定义 annotation_rows (7 rows)
- [ ] 定义 inputs/outputs/tags
- [ ] 定义 `cjtag_state` struct
- [ ] 定义 JTAG 状态转换表 `next_state[16][2]` (复用 jtag_c.c)
- [ ] 实现 `cjtag_reset()`
- [ ] 实现 `cjtag_start()` — 注册 output (ann + python)
- [ ] 实现 `advance_state_machine()` — JTAG TAP controller 状态转换
- [ ] 实现 `handle_rising_tck_edge()` — TCK 上升沿处理
  - [ ] 状态转换
  - [ ] 输出旧状态 annotation
  - [ ] 输出 TAPC state annotation
  - [ ] SHIFT-* 期间收集 TDI/TDO bits
  - [ ] EXIT1-* 时输出 bitstring
- [ ] 实现 `handle_tms_edge()` — TMS edge 计数
- [ ] 实现 `handle_tapc_state()` — cJTAG 模式检测
  - [ ] 6 edges → CJTAG-OAC
  - [ ] 8 edges → 4-WIRE
- [ ] 实现 `cjtag_decode()` — 主解码循环
  - [ ] 等待 TCK 上升沿
  - [ ] OSCAN1 模式: 3-cycle 解码 (nTDI/TMS/TDO)
  - [ ] 4-WIRE 模式: 直接使用 TDI/TDO
  - [ ] TCK 高电平期间: 等待 TCK 下降沿或 TMS edge
- [ ] 实现 `cjtag_destroy()`
- [ ] 定义 `cjtag_oscan0_c_decoder` struct
- [ ] 实现 `srd_c_decoder_entry()` — 无 options
- [ ] 实现 `srd_c_decoder_api_version()`

### 5.2 关键实现细节

- [ ] OScan1 3-cycle 解码: nTDI(cycle 0) → TMS(cycle 1) → TDO(cycle 2)
- [ ] cJTAG OAC 阶段: oacp 计数器驱动的子状态机
- [ ] Escape 检测: TCK 高电平期间的 TMS 变化计数
- [ ] TDI/TDO bit 收集: insert(0, val) 模式 → 在 C 中用数组前移实现
- [ ] Bitstring 输出: 格式化 "DR TDI: (0xNN), N bits"
- [ ] Python output: 使用 `c_decoder_put_python()` 输出 JTAG 状态和 bitstring

### 5.3 构建集成

- [ ] 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 `cjtag_oscan0_c`

### 5.4 验证

- [ ] 编译通过
- [ ] 使用标准 JTAG 信号测试 4-WIRE 模式
- [ ] 使用 cJTAG 信号测试 OScan1 模式
- [ ] 验证 TDI/TDO bitstring 输出
- [ ] 验证 JTAG 状态转换
- [ ] 验证 escape 检测

---

## Task 6: 最终集成与测试

**优先级:** Medium | **预估时间:** 2h

- [ ] 确认所有 5 个 decoder 在 CMakeLists.txt 中
- [ ] 完整构建 (build_incremental.cmd)
- [ ] 确认所有 DLL 生成到 build.dir/decoders/c_decoders/
- [ ] 在 PXView 中加载每个 decoder 确认无崩溃
- [ ] 验证 annotation_rows 显示正确
- [ ] 验证 options 显示和生效
