# Python Decoder 移植验证清单 — Batch 11

## 通用验证项（适用于所有 5 个 decoder）

### 文件结构
- [ ] C 文件位于 `libsigrokdecode/c_decoders/{name}_c.c`
- [ ] 文件名中 `-` 已替换为 `_`
- [ ] 包含正确的头文件：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`

### struct srd_c_decoder 字段
- [ ] `.id` 以 `_c` 结尾
- [ ] `.name` 以 `(C)` 结尾
- [ ] `.longname` 和 `.desc` 与 Python 版本一致
- [ ] `.license = "gplv2+"`
- [ ] `.channels` 正确定义（order 从 0 开始）
- [ ] `.optional_channels` 正确定义（如有）
- [ ] `.num_channels` / `.num_optional_channels` 计数正确
- [ ] `.options` 正确定义
- [ ] `.num_options` 计数正确

### ann_labels 规范
- [ ] 每个条目的第一列为 `""`（空字符串）
- [ ] 第二列为短标签，第三列为长标签
- [ ] `num_annotations` 与 ann_labels 数组长度一致

### annotation_rows 规范
- [ ] 所有 annotation class 都被映射到某一行
- [ ] 每行使用 `static const int xxx_row_xxx_classes[]` 定义 class 列表
- [ ] `num_annotation_rows` 与 ann_rows 数组长度一致
- [ ] `srd_c_ann_row` 的 `num_ann_classes` 字段正确

### 回调函数
- [ ] `reset()`: 使用 `g_malloc0` 分配 priv，`memset` 清零，设置初始状态
- [ ] `start()`: 注册 `out_ann`（和 `out_python` 如有 Python 输出），读取选项
- [ ] `metadata()`: 接收 `SRD_CONF_SAMPLERATE`，存入 priv
- [ ] `decode()`: samplerate guard（检查 samplerate 是否已设置）
- [ ] `destroy()`: `g_free(priv)`, `c_decoder_set_private(di, NULL)`

### Condition Builder 使用
- [ ] 每次 `c_cond_new()` 后都有对应的 `c_cond_free()`
- [ ] `c_cond_wait()` 返回值检查：`if (ret != SRD_OK) return;`
- [ ] `c_cond_or()` 正确使用（在两组条件之间）
- [ ] channel index 与 channels/optional_channels 定义一致

### srd_c_decoder_entry() 函数
- [ ] 所有选项的 `.id`, `.idn`, `.desc`, `.def` 都已初始化
- [ ] 字符串选项使用 `g_variant_new_string()`
- [ ] 整数选项使用 `g_variant_new_int64()`
- [ ] 枚举选项使用 `GSList` + `g_variant_new_string()` 构建可选值列表
- [ ] 函数返回 `&xxx_c_decoder` 指针

### srd_c_decoder_api_version() 函数
- [ ] 返回 `SRD_C_DECODER_API_VERSION`

### CMakeLists.txt
- [ ] decoder 名已添加到 `C_DECODERS` 列表

---

## rpm_c 专项验证

### 元数据一致性
- [ ] id = `"rpm_c"`, name = `"RPM(C)"`
- [ ] 1 个 channel: data
- [ ] 2 个 options: num_pulses(int64, default=2), edge(string, default="falling")
- [ ] 1 个 annotation: RPM
- [ ] 1 个 annotation_row: rpms
- [ ] tags = `["Util"]`

### 逻辑验证
- [ ] RPM 计算公式：`rpm = (int)(60.0 / t)`，其中 `t = (samplenum - last_samplenum) / samplerate`
- [ ] 首次 edge 只记录位置不计算
- [ ] `t >= 0.5` 秒时重置 edge_num 和 last_samplenum
- [ ] `edge_num == num_pulses` 时输出 RPM annotation 并重置
- [ ] edge 选项正确切换 `c_cond_rise` / `c_cond_fall`
- [ ] `C_ANN_PUT_VAL` 使用，传入 rpm 数值

---

## rinnai_control_panel_c 专项验证

### 元数据一致性
- [ ] id = `"rinnai_control_panel_c"`, name = `"Rinnai Control Panel(C)"`
- [ ] 1 个 channel: data
- [ ] 2 个 options: invert(string, "yes"/"no"), bit_numbering(string, "lsb"/"msb")
- [ ] 5 个 annotations: bit, warning, reset, byte, packet
- [ ] 4 个 annotation_rows: bits(0,2), warnings(1), bytes(3), packets(4)
- [ ] outputs = `["rinnai"]`
- [ ] tags = `["Embedded/industrial"]`

### 常量验证
- [ ] `SYMBOL_DURATION_US = 600`
- [ ] Short ratio: 0.15 ~ 0.35 (90us ~ 210us)
- [ ] Long ratio: 0.65 ~ 0.85 (390us ~ 510us)
- [ ] Reset ratio: 1.0 ~ 2.0 (600us ~ 1200us)

### 逻辑验证
- [ ] STATE_INITIAL: 等待 data 低电平
- [ ] STATE_IDLE: 等待 data 上升沿
- [ ] STATE_PRE: 等待 data 下降沿，判断 reset vs bad pre
- [ ] STATE_SYMBOL: 等待上升沿 + 下降沿，计算 timeA/timeB
- [ ] timeA 短 + timeB 长 → bit=1（或 invert 时 bit=0）
- [ ] timeA 长 + timeB 短 → bit=0（或 invert 时 bit=1）
- [ ] timeB 在 reset 范围 → 新 reset
- [ ] 其他 → bad bit, 回到 IDLE
- [ ] lsb_first 时 `byte |= (bit << bit_count)`
- [ ] msb_first 时 `byte = 2 * byte + bit`
- [ ] 8 bits 完成 → byte annotation + Python output
- [ ] bytes_flush 输出 packet annotation（hex 逗号分隔）
- [ ] out_python 注册为 `"rinnai"`

---

## sae_j1850_vpw_c 专项验证

### 元数据一致性
- [ ] id = `"sae_j1850_vpw_c"`, name = `"SAE J1850 VPW(C)"`
- [ ] 1 个 channel: data（idn = `"dec_sae_j1850_vpw_chan_data"`）
- [ ] 无 options
- [ ] 5 个 annotations: raw, sof, ifs, data, packet
- [ ] 3 个 annotation_rows: raws(0,1,2), bytes(3), packets(4)
- [ ] tags = `["Automotive"]`

### 时序常量验证
- [ ] SOF: 200us (164-245us)
- [ ] Long: 128us (97-170us)
- [ ] Short: 64us (24-97us)
- [ ] IFS: 240us

### 逻辑验证
- [ ] STATE_IDLE: 检测 SOF
  - [ ] 1X SOF: active 电平, 164-245us → spd=1
  - [ ] 4X SOF: active 电平, 41-61us → spd=4
- [ ] STATE_DATA: 检测 bit
  - [ ] t >= IFS/spd → EOF/IFS, 回到 IDLE
  - [ ] short range (24-97us / spd):
    - active 电平 → bit=1
    - 非active 电平 → bit=0
  - [ ] long range (97-170us / spd):
    - active 电平 → bit=0
    - 非active 电平 → bit=1
- [ ] `active = 0` 初始值
- [ ] MSB-first bit 组装: `byte_val |= (bit << (7 - bit_count))`
- [ ] 8 bits 完成 → Data annotation
- [ ] Packet field annotation: Priority/Dest/Source/Mode/Pid
- [ ] EOF 时回溯标记 Checksum（使用保存的 csa/csb）
- [ ] `spd` 变量正确影响时序判断
- [ ] 初始等待第一个边沿获取 es 基准

---

## pcfx_ctrlr_c 专项验证

### 元数据一致性
- [ ] id = `"pcfx_ctrlr_c"`, name = `"PCFX Cntrlr(C)"`
- [ ] 3 个 channels: TRG(0), CLK(1), DATA(2)
- [ ] 1 个 optional_channel: DIR(3)
- [ ] 1 个 option: bitvals(string, "electrical"/"internal")
- [ ] 12 个 annotations
- [ ] 6 个 annotation_rows: starts(0,1), bits(2,3), bytes(4), words(5), controller(6,7,8,9,10), warnings(11)
- [ ] tags = `["Retro computing"]`

### 逻辑验证
- [ ] STATE_FIND_START: 等待 TRG 下降沿
- [ ] STATE_CHECK_RESET: 等待 (TRG低+CLK下降) OR (TRG上升)
  - [ ] matched bit 0 → triggertype=1 (Reset)
  - [ ] 两种匹配都进入 START_BIT
  - [ ] triggertype=0 → Start annotation (class 0)
  - [ ] triggertype=1 → Reset annotation (class 1)
- [ ] STATE_START_BIT: 等待 CLK下降 OR TRG下降
  - [ ] CLK下降匹配 → 读取 DATA, bits_value 取反 (1-value)
- [ ] STATE_END_BIT: 等待 CLK上升 OR TRG下降
  - [ ] CLK上升匹配 → 记录 bits_end, 输出 bit annotation
  - [ ] bitvals=electrical 时显示原始电平值
  - [ ] bitvals=internal 时显示取反后的值
  - [ ] DIR=1 时使用 class 3 (outbits), 否则 class 2 (bit)
- [ ] 32 bits 完成后:
  - [ ] 4 个 byte annotation (每 8 bits)
  - [ ] 1 个 word annotation (32 bits)
  - [ ] 控制器类型: bits[28:31]
    - [ ] 15 → Joypad (class 7): 14 个按钮
    - [ ] 14 → Multitap (class 8)
    - [ ] 13 → Mouse (class 9): X/Y 坐标 + 左右键
    - [ ] 其他 → Unknown (class 10)
- [ ] Mouse Y 坐标: `value & 0x80` → `value = 0 - value`, "Y=%d (Up)"
- [ ] Mouse X 坐标: `value & 0x80` → `value = 0 - value`, "X=%d (Left)"
- [ ] `get_bitfield(start_bit, field_size)`: LSB-first 提取位域
- [ ] `putbit()`: 检查 `value & (1 << bitnum)` 输出按钮

---

## parallel_c 专项验证

### 元数据一致性
- [ ] id = `"parallel_c"`, name = `"Parallel(C)"`
- [ ] 0 个 channels
- [ ] 33 个 optional_channels: CLK(0) + D0-D31(1-32)
- [ ] 3 个 options: clock_edge, wordsize, endianness
- [ ] 2 个 annotations: items(0), words(1)
- [ ] 2 个 annotation_rows: items(0), words(1)
- [ ] outputs = `["parallel"]`
- [ ] tags = `["Util"]`

### Channel 初始化验证
- [ ] 33 个 optional_channels 在 `srd_c_decoder_entry()` 中动态初始化
- [ ] CLK: id="clk", name="CLK", desc="Clock line", type=SRD_CHANNEL_SCLK
- [ ] D0-D31: id="d0"-"d31", name="D0"-"D31", desc="Data line 0-31", type=SRD_CHANNEL_SDATA
- [ ] order 字段正确（0-32）

### 逻辑验证
- [ ] `start()` 中初始化 channel 映射:
  - [ ] `idx_channels[i]` = 已连接的 channel 索引, -1=未连接
  - [ ] `has_channels[]` = 已连接 channel 列表
  - [ ] 至少 1 个 channel 已连接
  - [ ] `max_connected` = 最大已连接索引
  - [ ] `have_clock` = channel 0 是否连接
- [ ] 有 clock 模式:
  - [ ] clock_edge=rising → `c_cond_rise(cb, 0)`
  - [ ] clock_edge=falling → `c_cond_fall(cb, 0)`
- [ ] 无 clock 模式:
  - [ ] 首次: 获取初始值（wait edge 或直接读当前值）
  - [ ] 后续: 等待任意已连接数据线的边沿（多条件 OR）
- [ ] `bitpack()`: 将 D0-Dn 打包为整数，未连接视为 0
- [ ] `num_item_bits = max_connected`（数据线数量）
- [ ] Item annotation 格式: `"@%0NX"` (N = (num_item_bits+3)/4)
- [ ] Word annotation 格式: `"@%0NX"` (N = (num_word_bits+3)/4)
- [ ] 延迟输出: 当前 item 在下一个采样点输出
- [ ] Word 组装:
  - [ ] wordsize > 0 时收集 items
  - [ ] little endian: items 直接拼接
  - [ ] big endian: items 反转后拼接
  - [ ] `word = sum(items[i] << (i * num_item_bits))`
- [ ] `end()` 回调: 输出最后一个 saved_item 和 saved_word
- [ ] Python output: ITEM 和 WORD 格式

---

## 编译验证

### 增量构建
- [ ] 执行 `build_incremental.cmd`
- [ ] 无编译错误
- [ ] 无编译警告（或仅有可接受的警告）

### DLL 生成
- [ ] `build.dir/decoders/c_decoders/rpm_c.dll` (或 `.so`)
- [ ] `build.dir/decoders/c_decoders/rinnai_control_panel_c.dll`
- [ ] `build.dir/decoders/c_decoders/sae_j1850_vpw_c.dll`
- [ ] `build.dir/decoders/c_decoders/pcfx_ctrlr_c.dll`
- [ ] `build.dir/decoders/c_decoders/parallel_c.dll`

### 运行时验证
- [ ] PXView 启动无崩溃
- [ ] Decoder 列表中可见 5 个新 C decoder
- [ ] 每个 C decoder 可正常添加到信号上
- [ ] 与对应 Python decoder 的 annotation 输出一致（对比测试）

---

## 回归测试

### 现有 C decoder 不受影响
- [ ] 原有 37 个 C decoder 仍可正常工作
- [ ] CMakeLists.txt 修改未破坏现有构建

### Python decoder 不受影响
- [ ] 对应的 5 个 Python decoder 仍可正常使用
- [ ] C decoder 和 Python decoder 可共存（id 不同：`xxx_c` vs `xxx`）
