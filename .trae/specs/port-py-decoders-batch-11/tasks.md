# Python Decoder 移植任务列表 — Batch 11

## 总览

将 5 个 Python decoder 移植为 C decoder，按难度从低到高排序实施。

---

## Task 1: rpm_c — RPM 转速计算（难度：低）

### 1.1 创建文件
- [ ] 创建 `libsigrokdecode/c_decoders/rpm_c.c`

### 1.2 元数据实现
- [ ] 定义 `enum rpm_ann`：`ANN_RPM = 0, NUM_ANN = 1`
- [ ] 定义 `struct rpm_priv`：samplerate, last_samplenum, edge_num, edge_type, num_pulses, out_ann
- [ ] 定义 `rpm_channels[]`：1 个 channel `{"data", "Data", "Data line", 0, SRD_CHANNEL_SDATA, NULL}`
- [ ] 定义 `rpm_options[]`：2 个选项 `num_pulses`(int64, default=2) 和 `edge`(string, default="falling", values=["rising","falling"])
- [ ] 定义 `rpm_ann_labels[][3]`：`{"", "rpm", "RPM"}`
- [ ] 定义 `rpm_ann_rows[]`：1 行 `{"rpms", "RPM", {ANN_RPM}, 1}`
- [ ] 定义 inputs/tags/outputs

### 1.3 回调函数实现
- [ ] `rpm_reset()`: g_malloc0 priv, memset, 初始化 edge_num=0, last_samplenum=0
- [ ] `rpm_start()`: 注册 out_ann, 读取 edge 选项, 读取 num_pulses 选项
- [ ] `rpm_metadata()`: 接收 SRD_CONF_SAMPLERATE
- [ ] `rpm_decode()`: 主循环 — 等待 edge, 计算 RPM, 输出 annotation
- [ ] `rpm_destroy()`: g_free priv

### 1.4 srd_c_decoder_entry() 实现
- [ ] 初始化 `num_pulses` 选项：`g_variant_new_int64(2)`
- [ ] 初始化 `edge` 选项：`g_variant_new_string("falling")`, values = ["rising", "falling"]

### 1.5 验证
- [ ] 编译通过
- [ ] RPM 计算公式正确：`rpm = 60 / t`（t 为 num_pulses 个 edge 之间的秒数）
- [ ] t >= 0.5 秒时重置计数器
- [ ] edge 选项正确切换 rising/falling

---

## Task 2: rinnai_control_panel_c — Rinnai 控制面板脉冲编码（难度：中）

### 2.1 创建文件
- [ ] 创建 `libsigrokdecode/c_decoders/rinnai_control_panel_c.c`

### 2.2 元数据实现
- [ ] 定义 `enum rinnai_ann`：`ANN_BIT=0, ANN_WARNING=1, ANN_RESET=2, ANN_BYTE=3, ANN_PACKET=4, NUM_ANN=5`
- [ ] 定义 `struct rinnai_priv`：samplerate, state, fall, rise, invert, lsb_first, bit_count, byte_val, byte_start, bytes[], byte_count, packet_start, out_ann, out_python
- [ ] 定义 `rinnai_channels[]`：1 个 channel `{"data", "Data", "Pulse length signal line", 0, SRD_CHANNEL_SDATA, NULL}`
- [ ] 定义 `rinnai_options[]`：2 个选项 `invert`(string, "no"/"yes") 和 `bit_numbering`(string, "lsb"/"msb")
- [ ] 定义 `rinnai_ann_labels[][3]`：5 个 annotation
- [ ] 定义 `rinnai_ann_rows[]`：4 行 bits/warnings/bytes/packets
- [ ] 定义 inputs/tags/outputs

### 2.3 常量定义
- [ ] `SYMBOL_DURATION_US = 600`
- [ ] `SHORT_RATIO_MIN/MAX = 0.15/0.35`
- [ ] `LONG_RATIO_MIN/MAX = 0.65/0.85`
- [ ] `RESET_RATIO_MIN/MAX = 1.0/2.0`
- [ ] `MAX_PACKET_BYTES = 64`

### 2.4 辅助函数实现
- [ ] `rinnai_samples_to_us()`: samples → 微秒转换
- [ ] `rinnai_bit_append()`: 输出 bit annotation, 按 lsb/msb 组装 byte
- [ ] `rinnai_bits_reset()`: 重置 bit 计数器
- [ ] `rinnai_byte_append()`: 输出 byte annotation + Python output
- [ ] `rinnai_bytes_flush()`: 输出 packet annotation
- [ ] `rinnai_bytes_reset()`: 重置 byte 数组

### 2.5 回调函数实现
- [ ] `rinnai_reset()`: 初始化状态为 STATE_INITIAL
- [ ] `rinnai_start()`: 注册 out_ann + out_python, 读取 invert/bit_numbering 选项
- [ ] `rinnai_metadata()`: 接收 SRD_CONF_SAMPLERATE
- [ ] `rinnai_decode()`: 4 状态 FSM (INITIAL/IDLE/PRE/SYMBOL)
  - [ ] STATE_INITIAL: 等待 data 低电平
  - [ ] STATE_IDLE: 等待 data 上升沿
  - [ ] STATE_PRE: 等待 data 下降沿, 判断 reset vs bad pre
  - [ ] STATE_SYMBOL: 等待上升沿+下降沿, 计算 timeA/timeB, 判断 bit/reset/bad
- [ ] `rinnai_destroy()`: g_free priv

### 2.6 srd_c_decoder_entry() 实现
- [ ] 初始化 `invert` 选项
- [ ] 初始化 `bit_numbering` 选项

### 2.7 验证
- [ ] 编译通过
- [ ] Reset pulse 正确检测（600-1200us 高电平）
- [ ] 短/长脉冲比例正确判断 bit 值
- [ ] invert 选项正确反转 bit
- [ ] lsb/msb bit_numbering 正确
- [ ] Byte/Packet annotation 正确输出

---

## Task 3: sae_j1850_vpw_c — SAE J1850 VPW 汽车总线（难度：中）

### 3.1 创建文件
- [ ] 创建 `libsigrokdecode/c_decoders/sae_j1850_vpw_c.c`

### 3.2 元数据实现
- [ ] 定义 `enum vpw_ann`：`ANN_RAW=0, ANN_SOF=1, ANN_IFS=2, ANN_DATA=3, ANN_PACKET=4, NUM_ANN=5`
- [ ] 定义 `struct vpw_priv`：samplerate, state, active, spd, byte_val, bit_count, datastart, byte_count, mode, csa, csb, out_ann
- [ ] 定义 `vpw_channels[]`：1 个 channel `{"data", "Data", "Data line", 0, SRD_CHANNEL_SDATA, "dec_sae_j1850_vpw_chan_data"}`
- [ ] 定义 `vpw_ann_labels[][3]`：5 个 annotation
- [ ] 定义 `vpw_ann_rows[]`：3 行 raws/bytes/packets
- [ ] 定义 inputs/tags/outputs

### 3.3 常量定义
- [ ] VPW 时序常量（SOF/LONG/SHORT/IFS 及其 min/max）

### 3.4 辅助函数实现
- [ ] `vpw_us_to_samples()`: 微秒 → samples
- [ ] `vpw_samples_to_us()`: samples → 微秒
- [ ] `vpw_handle_bit()`: MSB-first bit 组装, 8 bits → byte annotation + packet field annotation

### 3.5 回调函数实现
- [ ] `vpw_reset()`: 初始化状态为 STATE_IDLE, active=0, spd=1
- [ ] `vpw_start()`: 注册 out_ann
- [ ] `vpw_metadata()`: 接收 SRD_CONF_SAMPLERATE
- [ ] `vpw_decode()`: 2 状态 FSM (IDLE/DATA)
  - [ ] 初始等待第一个边沿
  - [ ] STATE_IDLE: 检测 1X/4X SOF
  - [ ] STATE_DATA: 检测 EOF/IFS, short/long pulse → bit
  - [ ] EOF 时回溯标记 Checksum
- [ ] `vpw_destroy()`: g_free priv

### 3.6 验证
- [ ] 编译通过
- [ ] 1X SOF 正确检测（164-245us active 电平脉冲）
- [ ] 4X SOF 正确检测（41-61us active 电平脉冲）
- [ ] Short/Long pulse 正确解码为 bit
- [ ] active 电平概念正确
- [ ] Checksum 回溯 annotation 正确
- [ ] Packet field annotation 正确（Priority/Dest/Source/Mode/Pid）

---

## Task 4: pcfx_ctrlr_c — PC-FX 控制器协议（难度：中高）

### 4.1 创建文件
- [ ] 创建 `libsigrokdecode/c_decoders/pcfx_ctrlr_c.c`

### 4.2 元数据实现
- [ ] 定义 `enum pcfx_ann`：12 个 annotation (start/reset/bit/outbits/byte/word/ctrldata/ctrlpad/ctrltap/ctrlmouse/ctrlunkn/warning)
- [ ] 定义 `struct pcfx_priv`：samplerate, state, startsamplenum, triggertype, startbit, bitvalue, have_direction, dir, bitcount, bits_value[32], bits_start[32], bits_end[32], bitvals, out_ann
- [ ] 定义 `pcfx_channels[]`：3 个 channel (TRG/CLK/DATA)
- [ ] 定义 `pcfx_optional_channels[]`：1 个 channel (DIR)
- [ ] 定义 `pcfx_options[]`：1 个选项 `bitvals`(string, "electrical"/"internal")
- [ ] 定义 `pcfx_ann_labels[][3]`：12 个 annotation
- [ ] 定义 `pcfx_ann_rows[]`：6 行
- [ ] 定义 inputs/tags/outputs

### 4.3 辅助函数实现
- [ ] `pcfx_get_bitfield()`: 从 bits_value 数组提取位域值
- [ ] `pcfx_put_button()`: 输出单个按钮 annotation
- [ ] `pcfx_handle_complete()`: 32 bits 完成后的解析和输出
  - [ ] 输出 4 个 byte annotation
  - [ ] 输出 1 个 word annotation
  - [ ] 控制器类型判断：Joypad(15)/Multitap(14)/Mouse(13)/Unknown
  - [ ] Joypad: 16 个按钮状态输出
  - [ ] Mouse: X/Y 坐标解析（补码处理）+ 左右键

### 4.4 回调函数实现
- [ ] `pcfx_reset()`: 初始化状态为 STATE_FIND_START
- [ ] `pcfx_start()`: 注册 out_ann, 读取 bitvals 选项, 检查 DIR channel
- [ ] `pcfx_metadata()`: 接收 SRD_CONF_SAMPLERATE
- [ ] `pcfx_decode()`: 4 状态 FSM
  - [ ] STATE_FIND_START: 等待 TRG 下降沿
  - [ ] STATE_CHECK_RESET: 等待 (TRG低+CLK下降) OR (TRG上升)
  - [ ] STATE_START_BIT: 等待 CLK 下降沿 OR TRG 下降沿
  - [ ] STATE_END_BIT: 等待 CLK 上升沿 OR TRG 下降沿, 32 bits 完成后调用 handle_complete
- [ ] `pcfx_destroy()`: g_free priv

### 4.5 srd_c_decoder_entry() 实现
- [ ] 初始化 `bitvals` 选项

### 4.6 验证
- [ ] 编译通过
- [ ] Trigger/Reset 正确区分
- [ ] 32 bits 正确收集（内部值取反）
- [ ] Byte/Word annotation 正确
- [ ] Joypad 按钮状态正确输出
- [ ] Mouse X/Y 坐标补码处理正确
- [ ] DIR channel 影响 bit annotation class

---

## Task 5: parallel_c — 通用并行同步总线（难度：高）

### 5.1 创建文件
- [ ] 创建 `libsigrokdecode/c_decoders/parallel_c.c`

### 5.2 元数据实现
- [ ] 定义 `enum parallel_ann`：`ANN_ITEMS=0, ANN_WORDS=1, NUM_ANN=2`
- [ ] 定义 `struct parallel_priv`：samplerate, have_clock, clock_edge, wordsize, endianness, num_item_bits, max_connected, idx_channels[33], has_channels[33], num_has_channels, prv_dex, saved_item, has_saved_item, items[], item_count, saved_word, has_saved_word, ss_word, es_word, first, is_first_wait, out_ann, out_python
- [ ] 定义 `parallel_optional_channels[33]`：CLK + D0-D31（在 entry 中动态初始化）
- [ ] 定义 `parallel_options[]`：3 个选项
- [ ] 定义 `parallel_ann_labels[][3]`：2 个 annotation
- [ ] 定义 `parallel_ann_rows[]`：2 行
- [ ] 定义 inputs/tags/outputs

### 5.3 辅助函数实现
- [ ] `parallel_bitpack()`: 将连接的数据线打包为整数值
- [ ] `parallel_handle_word()`: Word 组装逻辑
  - [ ] 输出保存的 word annotation
  - [ ] 收集 items 到 wordsize
  - [ ] 按 endianness 组装 word
- [ ] `parallel_handle_bits()`: Item 延迟输出逻辑
- [ ] `parallel_init_channels()`: 在 start 中初始化 channel 映射

### 5.4 回调函数实现
- [ ] `parallel_reset()`: 初始化 first=1, prv_dex=0
- [ ] `parallel_start()`: 注册 out_ann + out_python, 读取选项, 初始化 channel 映射
- [ ] `parallel_metadata()`: 接收 SRD_CONF_SAMPLERATE
- [ ] `parallel_decode()`: 主循环
  - [ ] 有 clock 模式：等待 clock 配置边沿
  - [ ] 无 clock 模式：首次 wait(None) 获取初始值，之后等待任意数据线边沿
  - [ ] bitpack → handle_bits → handle_word
- [ ] `parallel_end()`: 输出最后一个保存的 item 和 word
- [ ] `parallel_destroy()`: g_free priv

### 5.5 srd_c_decoder_entry() 实现
- [ ] 动态初始化 33 个 optional_channels 的 id/name/desc
- [ ] 初始化 3 个选项

### 5.6 验证
- [ ] 编译通过
- [ ] 有 clock 模式正确采样
- [ ] 无 clock 模式正确检测数据线边沿
- [ ] bitpack 正确（未连接 channel 视为 0）
- [ ] Word 组装正确（endianness）
- [ ] 延迟输出逻辑正确
- [ ] end() 回调输出最后的 item/word

---

## Task 6: 构建集成

### 6.1 CMakeLists.txt 修改
- [ ] 在 `C_DECODERS` 列表末尾添加：`rpm_c rinnai_control_panel_c sae_j1850_vpw_c pcfx_ctrlr_c parallel_c`

### 6.2 编译验证
- [ ] 执行 `build_incremental.cmd` 编译通过
- [ ] 确认 5 个 DLL 生成在 `build.dir/decoders/c_decoders/` 目录

---

## 依赖关系

```
Task 1 (rpm_c)          — 无依赖，可独立开始
Task 2 (rinnai_c)       — 无依赖，可独立开始
Task 3 (vpw_c)          — 无依赖，可独立开始
Task 4 (pcfx_c)         — 无依赖，可独立开始
Task 5 (parallel_c)     — 无依赖，可独立开始
Task 6 (build)          — 依赖 Task 1-5 全部完成
```

建议实施顺序：Task 1 → Task 2 → Task 3 → Task 4 → Task 5 → Task 6
