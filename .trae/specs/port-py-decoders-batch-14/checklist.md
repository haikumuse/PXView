# 验证清单 — Batch 14: Python Decoder 移植到 C

---

## 通用验证项（适用于所有 5 个 decoder）

### 代码结构

- [ ] 文件命名正确: `{decoder_id}_c.c`（`-` 替换为 `_`）
- [ ] 包含正确的版权头
- [ ] 包含 `#include "libsigrokdecode.h"` 和必要的系统头文件
- [ ] 所有 enum/struct 定义在文件内部（static 作用域或文件作用域）
- [ ] 无全局可变状态（所有状态通过 `c_decoder_get_private()` 获取）

### struct srd_c_decoder 规范

- [ ] `.id` 格式为 `"xxx_c"`（与 Python id 对应，加 `_c` 后缀）
- [ ] `.name` 格式为 `"XXX(C)"`
- [ ] `.longname` 和 `.desc` 包含 "(C implementation)" 或类似说明
- [ ] `.license` 正确（ws281x 为 `gplv3+`，其余为 `gplv2+`）
- [ ] `.channels` 数组与 Python 定义一致（id, name, desc, idn）
- [ ] `.optional_channels` 如有，与 Python 定义一致
- [ ] `.num_channels` 和 `.num_optional_channels` 正确
- [ ] `.options` 数组大小与 `.num_options` 一致
- [ ] `.num_annotations` 等于 ann_labels 数组长度
- [ ] `.num_annotation_rows` 正确
- [ ] 所有 annotation class 都映射到某个 annotation_row
- [ ] `.inputs` 包含 `"logic"` 且以 NULL 结尾
- [ ] `.outputs` 正确（ccd 输出 `"ccd"`，cjtag 输出 `"jtag"`，其余无输出）
- [ ] `.tags` 与 Python 定义一致且以 NULL 结尾
- [ ] 所有回调函数指针已设置: `.metadata`, `.reset`, `.start`, `.decode`, `.destroy`

### ann_labels 规范

- [ ] 第一列（index 0）为空字符串 `""`
- [ ] 第二列为 class id（小写，用 `-` 连接）
- [ ] 第三列为 class label（人类可读）
- [ ] 总数与 `.num_annotations` 一致

### annotation_rows 规范

- [ ] 每个 row 的 classes 数组以 `-1` 结尾
- [ ] 每个 row 的 count 值正确（不含终止符 `-1`）
- [ ] 所有 annotation class index 都出现在某个 row 的 classes 中
- [ ] row id 和 name 与 Python 定义一致

### samplerate guard

- [ ] `metadata` 回调处理 `SRD_CONF_SAMPLERATE`
- [ ] `decode()` 入口检查 `samplerate == 0` 并做 fallback (`c_decoder_get_samplerate()`)
- [ ] `samplerate == 0` 时安全退出（不崩溃）

### reset/start/destroy 规范

- [ ] `reset()`: 首次调用时 `g_malloc0()` 分配私有数据
- [ ] `reset()`: 后续调用时 `memset()` 清零
- [ ] `reset()`: 设置正确的初始状态值
- [ ] `start()`: 使用 `c_decoder_register_output()` 注册输出
- [ ] `start()`: 使用 `c_decoder_get_option_string/int/double` 读取选项
- [ ] `destroy()`: `g_free()` 释放私有数据，`c_decoder_set_private(di, NULL)`

### srd_c_decoder_entry() 规范

- [ ] 所有 string 选项使用 `g_variant_new_string()` 创建默认值和候选值
- [ ] 所有 int 选项使用 `g_variant_new_int64()`
- [ ] 所有 double 选项使用 `g_variant_new_double()`
- [ ] 候选值列表使用 `GSList`，通过 `g_slist_append()` 添加
- [ ] 每个 option 的 `.id`, `.idn`, `.desc`, `.def`, `.values` 都已设置
- [ ] 函数返回 `&xxx_c_decoder` 指针
- [ ] 同时实现 `srd_c_decoder_api_version()` 返回 `SRD_C_DECODER_API_VERSION`

### Condition Builder 使用

- [ ] 每次 `c_cond_new()` 后都有对应的 `c_cond_free()`
- [ ] `c_cond_wait()` 返回值检查 `!= SRD_OK` 时安全退出
- [ ] `c_cond_or()` 正确用于多条件组合
- [ ] `c_cond_skip()` 的参数不为负数

### C_ANN_PUT 使用

- [ ] annotation class index 在有效范围内
- [ ] 字符串参数在 `C_ANN_PUT` 调用期间保持有效（栈变量或字面量）
- [ ] ss ≤ es（起始 ≤ 结束）

### CMakeLists.txt

- [ ] decoder 名称已添加到 `C_DECODERS` 列表
- [ ] 名称格式: `bean_c`, `ccd_c`, `cjtag_oscan0_c`, `rgb_led_ws281x_c`, `stepper_motor_c`

---

## stepper_motor_c 专项验证

- [ ] channels: step (index 0), dir (index 1)
- [ ] options: unit (string: "steps"/"mm"), steps_per_mm (double: 100.0)
- [ ] decode: 等待 step 上升沿 (`c_cond_rise(cb, 0)`)
- [ ] speed 计算: `samplerate / delta / scale`
- [ ] position 更新: `pos += direction ? 1 : -1`
- [ ] unit=steps 时: format="%.0f", scale=1
- [ ] unit=mm 时: format="%.2f", scale=steps_per_mm
- [ ] annotation 输出包含单位后缀 ("steps/s", "mm/s", "steps", "mm")
- [ ] 首次 step 脉冲不输出 annotation（无前一个脉冲可比较）

---

## rgb_led_ws281x_c 专项验证

- [ ] channels: din (index 0)
- [ ] options: colors (14 string 值), polarity (2 string 值)
- [ ] samplerate 必须有效，否则无法计算时序阈值
- [ ] 时序阈值计算:
  - [ ] tH_threshold = samplerate * 625e-9
  - [ ] reset_threshold = samplerate * 50e-6
  - [ ] bit_threshold = samplerate * 3e-6
- [ ] bit 判定逻辑:
  - [ ] tH ≥ 625ns → bit 1
  - [ ] tH/period > 0.5 → bit 1
  - [ ] 否则 → bit 0
- [ ] polarity=normal: 等待低电平开始
- [ ] polarity=inverted: 等待高电平开始
- [ ] colorsize=24 (3-char colors: GRB/RGB/BRG/RBG/BGR)
- [ ] colorsize=32 (4-char colors: GRBW/RGBW/WRGB/LBGR/LGRB/LRGB/LRBG/LGBR/LBRG)
- [ ] 所有 14 种颜色模式的字节重排正确
- [ ] RESET annotation 输出 ("RESET", "RST", "R")
- [ ] RGB annotation 格式: "XXX#%06x" 或 "XXX#%06x W#%02x"
- [ ] 状态机: FIND_RESET → RESET/BIT_FALLING → BIT_RISING → BIT_FALLING → ...

---

## bean_c 专项验证

- [ ] channels: data (index 0)
- [ ] options: bit_annotations, pulse_len, command, all_byte (各为 yes/no)
- [ ] 脉宽分类:
  - [ ] ≤150: 短脉冲
  - [ ] 150~650: 中等脉冲
  - [ ] ≥650: 长脉冲
- [ ] 中等脉冲 bit 拆分: puls/100 个 bit
- [ ] bit stuffing: 连续 4 个相同 bit 后插入 stuff bit
  - [ ] count==5 时 stuff=1
- [ ] SOF 检测: 第一个脉冲
- [ ] EOM 检测: count==6 的中等脉冲
- [ ] RSP 检测: EOM 后的短脉冲
- [ ] 帧解析:
  - [ ] PRI: bits[0..3]
  - [ ] ML: bits[4..7]
  - [ ] frame_length + 3 ≤ bit_count / 8
  - [ ] 按字节解析: DST-ID, MES-ID, DATA, CRC, EOM
- [ ] 命令查找表: 22 个条目，key 为 hex string
- [ ] byte_ann 分类: PRI/ML → message row (class 5), DST-ID/MES-ID/DATA → frame row (class 4)
- [ ] 选项控制:
  - [ ] bit_annotations=yes 时输出 SOF/Stuff annotation (class 2)
  - [ ] pulse_len=yes 时输出脉宽 annotation (class 6)
  - [ ] command=yes 时输出命令 annotation (class 7)
  - [ ] all_byte=yes 时输出全字节 annotation (class 8)
- [ ] noresp 标志: 长间隔后 reset_frame 而非完全 reset

---

## ccd_c 专项验证

- [ ] channels: bus (index 0)
- [ ] options: ignoreerrors (yes/no), invert_bus (yes/no), units (metric/imperial/both/native)
- [ ] bit_width 计算: `ceil(samplerate / 7812.5)`
- [ ] IDLE/BUSY 状态:
  - [ ] IDLE → BUSY: bus 信号变化
  - [ ] BUSY → IDLE: bus 高电平持续 > 10*bit_width
- [ ] UART 解码:
  - [ ] Start bit: 1→0 跳变
  - [ ] Data bits: 8 bits, LSB first, databyte = databyte//2 + (bus?128:0)
  - [ ] Stop bit: 应为高电平，否则 frame error
  - [ ] 采样点: start bit 后 1.5*bit_width，然后每 bit_width
- [ ] Checksum: 所有 bytes 之和 mod 256，最后一个 byte 为 checksum
- [ ] 消息解码: 至少实现以下 ID:
  - [ ] 0x24: Speed
  - [ ] 0xE4: RPM + MAP
  - [ ] 0x6D: VIN (累积拼接)
  - [ ] 0x86: Door lock/alarm
  - [ ] 0x8E: Doors
  - [ ] 0x0c: Voltage/temperatures/oil
  - [ ] 0xCE: Odometer
  - [ ] 0x62: Electric doors/mirrors
  - [ ] 其他: Unknown message
- [ ] invert_bus 选项: bus = 1 - bus
- [ ] ignoreerrors 选项: errors==0 或 ignoreerrors==yes 时才解码
- [ ] 动态 wait 条件: 每次循环重新构建 condition builder
- [ ] 消息日志: class 6 输出完整消息 bytes

---

## cjtag_oscan0_c 专项验证

- [ ] channels: tdi(0), tdo(1), tck(2), tms(3)
- [ ] optional_channels: trst(4), srst(5), rtck(6)
- [ ] 22 个 annotation classes (16 JTAG states + 6 extra)
- [ ] 7 个 annotation_rows
- [ ] JTAG 状态转换表: next_state[16][2] 正确
- [ ] cJTAG 模式状态机:
  - [ ] 4-WIRE: 标准 JTAG
  - [ ] CJTAG-OAC: 6 个 TMS edge 后进入, oaclen=12
  - [ ] CJTAG-EC/SPARE/TPDEL/TPREV/TPST/RDYC/DLYC/SCNFMT: OAC 子阶段
  - [ ] CJTAG-CP: oacp > 8 (oaclen=12) 或 oacp > 32 (oaclen=36)
  - [ ] OSCAN1: oacp > oaclen 后进入
  - [ ] 8 个 escape edges → 回到 4-WIRE
- [ ] OScan1 3-cycle 解码:
  - [ ] cycle 0 (nTDI): TMS=0→TDI=1, TMS=1→TDI=0
  - [ ] cycle 1 (TMS): 直接使用 TMS
  - [ ] cycle 2 (TDO): TMS 作为 TDO
- [ ] TDI/TDO bit 收集:
  - [ ] SHIFT-* 期间收集
  - [ ] bits_tdi/bits_tdo 使用 insert(0, val) 模式
  - [ ] EXIT1-* 时标记 data_ready
  - [ ] EXIT→PAUSE 时输出 bitstring
- [ ] Bitstring 格式: "DR TDI: (0xNN), N bits"
- [ ] Python output: 使用 `c_decoder_put_python()` 输出
- [ ] TCK 高电平期间: 等待 TCK 下降沿或 TMS edge
- [ ] Escape edge 计数: TCK 高电平期间 TMS 变化

---

## 编译验证

- [ ] `build_incremental.cmd` 执行成功
- [ ] 无编译警告（-Wall -Wextra）
- [ ] 5 个 DLL 文件生成到 `build.dir/decoders/c_decoders/`:
  - [ ] `bean_c.dll`
  - [ ] `ccd_c.dll`
  - [ ] `cjtag_oscan0_c.dll`
  - [ ] `rgb_led_ws281x_c.dll`
  - [ ] `stepper_motor_c.dll`

## 运行时验证

- [ ] PXView 启动无崩溃
- [ ] 每个 decoder 可在 decoder 列表中找到（显示 "XXX(C)" 名称）
- [ ] 选择 decoder 后 channel 分配界面正确
- [ ] options 界面显示正确（类型、默认值、候选值）
- [ ] 运行解码不崩溃
- [ ] annotation 在波形上正确显示
- [ ] annotation_rows 布局正确
