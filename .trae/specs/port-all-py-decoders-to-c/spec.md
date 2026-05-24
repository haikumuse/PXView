# Python解码器C语言移植总计划 (母Spec)

## Why
当前项目已有37个C解码器实现，但仍有约178个Python解码器没有C语言版本。Python解码器在运行时需要GIL和Python解释器，性能远低于C实现。将所有Python解码器移植为C版本可大幅提升解码性能，降低内存占用，并消除Python依赖。

本母Spec负责规划所有子Spec的编写，每个子Spec覆盖5个解码器（最后一批可能少于5个），可由独立的子Agent并行执行。

## What Changes
- 为所有尚无C实现的Python解码器编写对应的C语言版本
- 每个C解码器必须与Python版本逻辑完全一致
- 按优先级和依赖关系分批次实施，每批次5个解码器
- 每个批次对应一个独立的子spec，可并行实施
- 新增15个原spec遗漏的解码器：mipi_dsi, pxx1, qi, rc_encode, sdq, spi-fast, swi, t55xx, tdm_audio, timing, tlc5620, xy2-100, streletz, tm1637, tm1638, ltar_smartdevice_decode

## Impact
- Affected code: `libsigrokdecode/c_decoders/` (新增约178个C源文件)
- Affected build: `CMakeLists.txt` (新增约178个DLL构建目标)
- Affected specs: 所有38个子spec

---

## 已有C实现的解码器 (37个，跳过)

4b5b, c2, can, can-fd, cec, counter, dali, dcf77, dmx512, ds1307, ds3231, graycode, hdlc, i2c, i2s, ir_nec, ir_rc5, ir_sirc, iso7816, jtag, lin, lm75, lpc, mdio, microwire, nrzi, numbers_and_state, onewire, ps2, pwm, seven_segment, spdif, spi, swd, uart, usb_signalling, wiegand

## 跳过的解码器 (7个)

| 解码器 | 原因 |
|--------|------|
| 0-i2c, 1-i2c | i2c实例变体，代码与i2c完全相同 |
| 0-spi, 1-spi | spi实例变体，代码与spi完全相同 |
| 0-uart, 1-uart | uart实例变体，代码与uart完全相同 |
| example | 示例解码器，无实际用途 |

## 跳过的目录 (1个)

| 目录 | 原因 |
|------|------|
| common | 工具模块(srdhelper/plugtrx/sdcard)，非解码器 |

---

## 分批计划总览

| 层级 | 批次范围 | 解码器数量 | 批次数 | 依赖 |
|------|----------|-----------|--------|------|
| TIER 1 | Batch 1-4 | 20 | 4 | 无 |
| TIER 2 | Batch 5-17 | 62 | 13 | 无 |
| TIER 3A | Batch 18-22 | 25 | 5 | i2c_c (已完成) |
| TIER 3B | Batch 23-28 | 27 | 6 | spi_c (已完成) |
| TIER 3C | Batch 29-31 | 15 | 3 | uart_c (已完成) |
| TIER 3D | Batch 32 | 3 | 1 | jtag_c (已完成) |
| TIER 3E | Batch 33-37 | 26 | 5 | 各对应底层解码器 |
| **合计** | **Batch 1-37** | **178** | **37** | |

---

## TIER 1: 底层常用协议 (inputs=['logic'], 高频使用)

| 批次 | 子spec目录 | 解码器 | 说明 |
|------|-----------|--------|------|
| Batch 1 | `port-py-decoders-batch-01` | qspi, sdio, spi_dual_quad, uart-fast, cjtag | 存储接口+调试接口 |
| Batch 2 | `port-py-decoders-batch-02` | flexray, mipi_rffe, usb_power_delivery, iebus, spacewire | 汽车总线+移动RF+USB PD |
| Batch 3 | `port-py-decoders-batch-03` | ac97, sdcard_sd, emmc_sd, swim, rvswd | 音频+存储+调试接口 |
| Batch 4 | `port-py-decoders-batch-04` | tmc, sent, sle44xx, pjdl, onewire_link | 仪器+智能卡+1-Wire链路层 |

---

## TIER 2: 底层较少使用协议 (inputs=['logic'])

| 批次 | 子spec目录 | 解码器 | 说明 |
|------|-----------|--------|------|
| Batch 5 | `port-py-decoders-batch-05` | adb, afsk, am230x, caliper, carrera | Apple+调制+温湿度+量具+赛车 |
| Batch 6 | `port-py-decoders-batch-06` | dcc, delta-sigma, dsi, em4100, em4305 | 火车控制+ADC+显示+RFID |
| Batch 7 | `port-py-decoders-batch-07` | eth_an, fsi, gpib, guess_bitrate, iec | 以太网物理+IBM+仪器+工具 |
| Batch 8 | `port-py-decoders-batch-08` | ieee488, ir_irmp, ir_ltto, ir_rc6, ir_recoil | 仪器总线+红外协议族 |
| Batch 9 | `port-py-decoders-batch-09` | jitter, lfast, maple_bus, miller, morse | 抖动+汽车总线+编码+摩尔斯 |
| Batch 10 | `port-py-decoders-batch-10` | mvb, mcs48, one_single_wire, ook, opentherm | 列车总线+MCU+单线+OOK+暖通 |
| Batch 11 | `port-py-decoders-batch-11` | parallel, pcfx-ctrlr, rinnai-control-panel, rpm, sae_j1850_vpw | 并口+游戏手柄+暖控+转速+汽车 |
| Batch 12 | `port-py-decoders-batch-12` | sda2506, signature, sony_md, st7735, st7789 | 存储+签名+索尼+LCD驱动 |
| Batch 13 | `port-py-decoders-batch-13` | z80, adat, arm_etmv3, aud, avr_pdi | CPU+音频+ARM ETM+AUD+AVR |
| Batch 14 | `port-py-decoders-batch-14` | bean, ccd, cjtag-oscan0, rgb_led_ws281x, stepper_motor | BLE+CCD+cJTAG变体+LED+步进 |
| Batch 15 | `port-py-decoders-batch-15` | mipi_dsi, pxx1, qi, rc_encode, sdq | MIPI显示+遥控编码+无线充电+SDQ |
| Batch 16 | `port-py-decoders-batch-16` | spi-fast, swi, t55xx, tdm_audio, timing | SPI快速+单线+RFID+TDM+时序 |
| Batch 17 | `port-py-decoders-batch-17` | tlc5620, xy2-100 | DAC+振镜协议 |

---

## TIER 3A: I2C上层协议 (inputs=['i2c'])

| 批次 | 子spec目录 | 解码器 | 说明 |
|------|-----------|--------|------|
| Batch 18 | `port-py-decoders-batch-18` | ad5593r, adxl345, atsha204a, bh1750, eeprom24xx | ADC+加速度+加密+光照+EEPROM |
| Batch 19 | `port-py-decoders-batch-19` | edid, i2c_packet, i2cdemux, i2cfilter, ltc26x7 | 显示+I2C包+解复用+过滤+DAC |
| Batch 20 | `port-py-decoders-batch-20` | mlx90614, mpu6050, mxc6225xu, nunchuk, pca9571 | 温度+IMU+加速度+Wii+IO扩展 |
| Batch 21 | `port-py-decoders-batch-21` | rtc8564, ssd1306, st25dv, tcs3472x, tpm_tis_i2c | RTC+OLED+NFC+颜色+TPM |
| Batch 22 | `port-py-decoders-batch-22` | xfp, hdcp, hdmi_scdc, tca6408a, tmp102 | 光模块+HDCP+HDMI+IO+温度 |

---

## TIER 3B: SPI上层协议 (inputs=['spi'])

| 批次 | 子spec目录 | 解码器 | 说明 |
|------|-----------|--------|------|
| Batch 23 | `port-py-decoders-batch-23` | a7105, ad5626, ad79x0, ade77xx, adf435x | 无线+DAC+ADC+电能+频率 |
| Batch 24 | `port-py-decoders-batch-24` | adns5020, as5047, avr_isp, cc1101, cyrf6936 | 光学+编码器+AVR编程+无线 |
| Batch 25 | `port-py-decoders-batch-25` | enc28j60, ltc242x, max6954, max7219, mrf24j40 | 以太网+ADC+LED驱动+无线 |
| Batch 26 | `port-py-decoders-batch-26` | nes_gamepad, nrf24l01, nrf905, rfm12, ssi32 | 游戏手柄+无线模块族 |
| Batch 27 | `port-py-decoders-batch-27` | st25r39xx_spi, sdcard_spi, spiflash, spi_tpm, tpm_tis_spi | NFC+SD卡+Flash+TPM |
| Batch 28 | `port-py-decoders-batch-28` | x2444m, rgb_led_spi | EEPROM+LED(SPI版) |

---

## TIER 3C: UART上层协议 (inputs=['uart'])

| 批次 | 子spec目录 | 解码器 | 说明 |
|------|-----------|--------|------|
| Batch 29 | `port-py-decoders-batch-29` | arm_itm, arm_tpiu, bluetooth_h4, boost, crsf | ARM调试+蓝牙+协议 |
| Batch 30 | `port-py-decoders-batch-30` | j1708, midi, modbus, pan1321, pn532 | 卡车+音乐+工业+蓝牙+NFC |
| Batch 31 | `port-py-decoders-batch-31` | sbus_futaba, scs, ufcs, amulet_ascii, streletz | 遥控+舵机+快充+显示+安防 |

---

## TIER 3D: JTAG上层协议 (inputs=['jtag'])

| 批次 | 子spec目录 | 解码器 | 说明 |
|------|-----------|--------|------|
| Batch 32 | `port-py-decoders-batch-32` | jtag_avr, jtag_ejtag, jtag_stm32 | AVR+EJTAG+STM32调试 |

---

## TIER 3E: 其他上层协议 (依赖各种解码器)

| 批次 | 子spec目录 | 解码器 | 依赖 | 说明 |
|------|-----------|--------|------|------|
| Batch 33 | `port-py-decoders-batch-33` | onewire_network, ds2408, ds243x, ds28ea00, eeprom93xx | onewire_link_c, onewire_c, microwire_c | 1-Wire网络层+1-Wire设备+EEPROM |
| Batch 34 | `port-py-decoders-batch-34` | avclan, ethernet, arp, ipv4, udp | iebus_c, 4b5b_c | 汽车总线+网络协议栈 |
| Batch 35 | `port-py-decoders-batch-35` | cfp, ps2_keyboard, ps2_mouse, usb_packet, usb_request | mdio_c, ps2_c, usb_signalling_c | MDIO上层+PS2设备+USB上层 |
| Batch 36 | `port-py-decoders-batch-36` | ook_oregon, ook_vis, ltar_smartdevice, ir_ltto_decode, sony_md_decode | ook_c, afsk_c, ir_ltto_c, sony_md_c | OOK解码+设备解码层 |
| Batch 37 | `port-py-decoders-batch-37` | sipi, pjon, tpm_fifo_tis, tm1637, tm1638, ltar_smartdevice_decode | lfast_c, pjdl_c, tpm_tis_spi_c/tpm_tis_i2c_c, tmc_c, ltar_smartdevice_c | LFAST上层+PJON上层+TPM FIFO+LED驱动+LTAR解码 |

---

## 子Spec编写规范

每个子Spec必须包含以下内容，且足够详细，使子Agent能独立完成实现：

### 1. 子Spec目录结构
```
.trae/specs/port-py-decoders-batch-NN/
├── spec.md        # 详细规格书
├── tasks.md       # 任务列表
└── checklist.md   # 验证清单
```

### 2. 子Spec spec.md 必须包含的内容

对于每个解码器，必须提供：

#### 2.1 Python解码器元数据
- id, name, longname, desc, license
- inputs, outputs, tags
- channels (id, name, desc, idn)
- optional_channels (id, name, desc, idn)
- options (id, desc, default, values, idn)
- annotations (索引, id, 描述)
- annotation_rows (id, 描述, 包含的注释类索引)
- binary (如有)

#### 2.2 Python解码逻辑分析
- 状态机定义（所有状态及转换条件）
- self.wait() 条件映射
- 关键算法（CRC、校验、编码等）
- 边界条件处理
- Python特有构造的C等价实现方案

#### 2.3 C实现计划
- 状态结构体定义
- 通道/选项/注解的C数组定义
- 各回调函数的实现策略
- samplerate时序防护方案
- 条件构建器使用方案

#### 2.4 关键代码片段
- 状态机转换的C伪代码
- 复杂算法的C实现
- 上层解码器的recv_proto实现

### 3. 子Spec tasks.md 格式
```
# Tasks
- [ ] Task 1: 实现 xxx_c.c 解码器
  - [ ] 1.1 定义通道、选项、注解数组
  - [ ] 1.2 实现状态结构体和reset回调
  - [ ] 1.3 实现start/metadata回调
  - [ ] 1.4 实现decode/recv_proto回调
  - [ ] 1.5 实现destroy回调
  - [ ] 1.6 在CMakeLists.txt中注册
  - [ ] 1.7 编译验证
- [ ] Task 2: 实现 yyy_c.c 解码器
  ...
```

### 4. 子Spec checklist.md 格式
```
- [ ] xxx_c.c 编译通过无警告
- [ ] xxx_c.c 通道定义与Python一致
- [ ] xxx_c.c 选项定义与Python一致
- [ ] xxx_c.c 注解定义与Python一致
- [ ] xxx_c.c 解码逻辑与Python一致
- [ ] xxx_c.c samplerate时序防护已实现
- [ ] xxx_c.c 已在CMakeLists.txt中注册
...
```

---

## C解码器实现标准 (所有子spec必须遵循)

### 1. 文件命名
- 文件名: `{decoder_id}_c.c` (如 `qspi_c.c`)
- Python的 `-` 替换为 `_` (如 `can-fd` → `can_fd_c.c`)

### 2. 结构体定义
```c
struct srd_c_decoder xxx_c_decoder = {
    .id = "xxx_c",           // Python id + "_c" 后缀
    .name = "XXX(C)",        // Python name + "(C)" 后缀
    .longname = "...",       // 与Python longname一致 + " (C implementation)"
    .desc = "...",           // 与Python desc一致
    .license = "gplv2+",
    .channels = xxx_channels,
    .num_channels = N,
    .optional_channels = xxx_optional_channels,
    .num_optional_channels = M,
    .options = xxx_options,
    .num_options = K,
    .num_annotations = NUM_ANN,
    .ann_labels = xxx_ann_labels,
    .num_annotation_rows = R,
    .annotation_rows = xxx_ann_rows,
    .inputs = xxx_inputs,
    .num_inputs = 1,
    .outputs = xxx_outputs,
    .num_outputs = O,
    .binary = xxx_binary,
    .num_binary = B,
    .tags = xxx_tags,
    .num_tags = T,
    .reset = xxx_reset,
    .start = xxx_start,
    .decode = xxx_decode,
    .metadata = xxx_metadata,   // 如果使用samplerate则必须实现
    .destroy = xxx_destroy,
};
```

### 3. ann_labels 规则
- 第一列必须为 `""` (空字符串)，API层自动处理 i+7 偏移
- 第二列为Python annotations的id
- 第三列为Python annotations的描述

### 4. annotation_rows 规则
- 所有注释类(enum值)必须映射到annotation_rows
- 遗漏映射会导致运行时断言失败

### 5. 输出函数映射
| Python | C | 用途 |
|--------|---|------|
| `self.put(ss, es, out_ann, [N, [text]])` | `C_ANN_PUT(di, ss, es, out_ann, N, text)` | 显示注释 |
| `self.put(ss, es, out_ann, [N, tp, [text]])` | `C_ANN_PUT_TYPE(di, ss, es, out_ann, N, tp, text)` | 带类型注释 |
| `self.put(ss, es, out_python, [cmd, data])` | `c_decoder_put_proto(di, ss, es, out_proto, cmd, data, len)` | 解码器间通信 |
| `self.put(ss, es, out_binary, [0, bytes(...)])` | `c_decoder_put_binary(di, ss, es, out_binary, 0, len, data)` | 二进制输出 |
| `self.put(ss, es, out_meta, value)` | `c_decoder_put_meta_double(di, ss, es, out_meta, value)` | 元数据输出 |
| `self.wait({})` | `c_cond_wait_current(di, &samplenum)` | 获取当前采样位置 |
| `self.initial_pins` | `c_decoder_get_initial_pin(di, ch)` | 获取初始引脚状态 |
| `self.register(srd.OUTPUT_LOGIC)` | `c_decoder_register_output(di, SRD_OUTPUT_LOGIC, "xxx")` | 逻辑输出 |

### 6. samplerate时序防护 (关键!)
- `start()` 中获取samplerate可能为0
- 必须实现 `metadata` 回调以在samplerate设置时更新
- 必须在 `decode()` 入口处添加fallback获取:
```c
if (!s->samplerate) {
    s->samplerate = c_decoder_get_samplerate(di);
    // 重新计算所有依赖samplerate的派生值
}
if (!s->samplerate)
    return;
```

### 7. 上层解码器 (inputs != ['logic'])
- 不实现 `decode()` 回调 (或留空)
- 实现 `recv_proto()` 回调接收下层解码器输出
- `recv_proto` 签名: `void (*recv_proto)(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- 通过 `c_decoder_register_output(di, SRD_OUTPUT_PROTO, "xxx")` 注册协议输出
- **注意**: SRD_OUTPUT_PYTHON 已重命名为 SRD_OUTPUT_PROTO，c_decoder_put_python 已重命名为 c_decoder_put_proto
- **C解码器只能依赖C解码器**：srd_inst_stack() 会拒绝 C/Python 混合堆叠

### 8. 条件等待API
| Python | C |
|--------|---|
| `self.wait({0: 'r'})` | `c_cond_rise(cb, 0); c_cond_wait(cb, di, &samplenum, &matched);` |
| `self.wait({0: 'f'})` | `c_cond_fall(cb, 0); c_cond_wait(cb, di, &samplenum, &matched);` |
| `self.wait({0: 'e'})` | `c_cond_edge(cb, 0); c_cond_wait(cb, di, &samplenum, &matched);` |
| `self.wait({0: 'h'})` | `c_cond_high(cb, 0); c_cond_wait(cb, di, &samplenum, &matched);` |
| `self.wait({0: 'l'})` | `c_cond_low(cb, 0); c_cond_wait(cb, di, &samplenum, &matched);` |
| `self.wait([{0:'r'},{1:'f'}])` | `c_cond_rise(cb_or1, 0); c_cond_or(cb); c_cond_fall(cb_or2, 1); c_cond_wait(cb, di, ...)` |
| `self.wait({0: 'n'})` | `c_cond_noedge(cb, 0); c_cond_wait(cb, di, ...)` |
| `self.wait({'skip': N})` | `c_cond_skip(cb, N); c_cond_wait(cb, di, ...)` |

### 9. 选项初始化
- 静态数组声明时只初始化id字段
- 在 `srd_c_decoder_entry()` 中完整初始化所有字段(id, idn, desc, def, values)
- 字符串选项用 `g_variant_new_string()`
- 整数选项用 `g_variant_new_int64()`
- 浮点选项用 `g_variant_new_double()`
- values列表用 `g_slist_append()` + GVariant

### 10. 编译集成
- 每个C解码器编译为独立DLL
- 在 `CMakeLists.txt` 中 `C_DECODERS` 列表添加解码器名
- 导出符号: `SRD_C_DECODER_EXPORT`

---

## 子Spec依赖关系

```
TIER 1 (Batch 1-4): 无依赖，可立即开始
TIER 2 (Batch 5-17): 无依赖，可立即开始
TIER 3A (Batch 18-22): 依赖 i2c_c (已完成)
TIER 3B (Batch 23-28): 依赖 spi_c (已完成)
TIER 3C (Batch 29-31): 依赖 uart_c (已完成)
TIER 3D (Batch 32): 依赖 jtag_c (已完成)
TIER 3E (Batch 33): 依赖 onewire_c + onewire_link_c + microwire_c
TIER 3E (Batch 34): 依赖 iebus_c + 4b5b_c
TIER 3E (Batch 35): 依赖 mdio_c + ps2_c + usb_signalling_c
TIER 3E (Batch 36): 依赖 ook_c + afsk_c + ir_ltto_c + sony_md_c
TIER 3E (Batch 37): 依赖 lfast_c + pjdl_c + tpm_tis_spi_c/tpm_tis_i2c_c + tmc_c + ltar_smartdevice_c
```

## 并行执行建议

- TIER 1 和 TIER 2 的所有批次 (Batch 1-17) 可完全并行
- TIER 3A-3D (Batch 18-32) 依赖的底层解码器均已完成，可立即开始
- TIER 3E (Batch 33-37) 需等待对应底层解码器完成:
  - Batch 33: 需 onewire_link_c (Batch 4) + microwire_c (已完成)
  - Batch 34: 需 iebus_c (Batch 2) + 4b5b_c (已完成)
  - Batch 35: 需 mdio_c (已完成) + ps2_c (已完成) + usb_signalling_c (已完成)
  - Batch 36: 需 ook_c (Batch 10) + afsk_c (Batch 5) + ir_ltto_c (Batch 8) + sony_md_c (Batch 12)
  - Batch 37: 需 lfast_c (Batch 9) + pjdl_c (Batch 4) + tmc_c (Batch 4) + tpm_tis_spi_c/tpm_tis_i2c_c (Batch 27/21) + ltar_smartdevice_c (Batch 36)
- 建议同时启动 4-8 个子agent，每个处理一个批次

---

## 已完成的子Spec

| 批次 | 子spec目录 | 状态 | 备注 |
|------|-----------|------|------|
| Batch 1 | `port-py-decoders-batch-01` | 已完成 | qspi, sdio, spi_dual_quad, uart-fast, cjtag |
| Batch 2 | `port-py-decoders-batch-02` | 已完成 | flexray, mipi_rffe, usb_power_delivery, iebus, spacewire |
| Batch 3 | `port-py-decoders-batch-03` | 已完成 | ac97, sdcard_sd, emmc_sd, swim, rvswd |
| Batch 4 | `port-py-decoders-batch-04` | 已完成 | tmc, sent, sle44xx, pjdl, onewire_link |
| Batch 5 | `port-py-decoders-batch-05` | 已完成 | adb, afsk, am230x, caliper, carrera |
| Batch 6 | `port-py-decoders-batch-06` | 已完成 | dcc, delta-sigma, dsi, em4100, em4305 |
| Batch 7 | `port-py-decoders-batch-07` | 已完成 | eth_an, fsi, gpib, guess_bitrate, iec |
| Batch 8 | `port-py-decoders-batch-08` | 已完成 | ieee488, ir_irmp, ir_ltto, ir_rc6, ir_recoil |
| Batch 9 | `port-py-decoders-batch-09` | 已完成 | jitter, lfast, maple_bus, miller, morse |
| Batch 10 | `port-py-decoders-batch-10` | 已完成 | mvb, mcs48, one_single_wire, ook, opentherm |
| Batch 11 | `port-py-decoders-batch-11` | 已完成 | parallel, pcfx-ctrlr, rinnai-control-panel, rpm, sae_j1850_vpw |
| Batch 12 | `port-py-decoders-batch-12` | 已完成 | sda2506, signature, sony_md, st7735, st7789 |
| Batch 13 | `port-py-decoders-batch-13` | 已完成 | z80, adat, arm_etmv3, aud, avr_pdi |
| Batch 14 | `port-py-decoders-batch-14` | 已完成 | bean, ccd, cjtag-oscan0, rgb_led_ws281x, stepper_motor |
| Batch 15 | `port-py-decoders-batch-15` | 已完成 | mipi_dsi, pxx1, qi, rc_encode, sdq |
| Batch 16 | `port-py-decoders-batch-16` | 已完成 | spi-fast, swi, t55xx, tdm_audio, timing |
| Batch 17 | `port-py-decoders-batch-17` | 已完成 | tlc5620, xy2-100 |
| Batch 18 | `port-py-decoders-batch-18` | 已完成 | ad5593r, adxl345, atsha204a, bh1750, eeprom24xx |
| Batch 19 | `port-py-decoders-batch-19` | 已完成 | edid, i2c_packet, i2cdemux, i2cfilter, ltc26x7 |
| Batch 20 | `port-py-decoders-batch-20` | 已完成 | mlx90614, mpu6050, mxc6225xu, nunchuk, pca9571 |
| Batch 21 | `port-py-decoders-batch-21` | 已完成 | rtc8564, ssd1306, st25dv, tcs3472x, tpm_tis_i2c |
| Batch 22 | `port-py-decoders-batch-22` | 已完成 | xfp, hdcp, hdmi_scdc, tca6408a, tmp102 |
| Batch 23 | `port-py-decoders-batch-23` | 已完成 | a7105, ad5626, ad79x0, ade77xx, adf435x |
| Batch 24 | `port-py-decoders-batch-24` | 已完成 | adns5020, as5047, avr_isp, cc1101, cyrf6936 |
| Batch 25 | `port-py-decoders-batch-25` | 已完成 | enc28j60, ltc242x, max6954, max7219, mrf24j40 |
| Batch 26 | `port-py-decoders-batch-26` | 已完成 | nes_gamepad, nrf24l01, nrf905, rfm12, ssi32 |
| Batch 27 | `port-py-decoders-batch-27` | 已完成 | st25r39xx_spi, sdcard_spi, spiflash, spi_tpm, tpm_tis_spi |
| Batch 28 | `port-py-decoders-batch-28` | 已完成 | x2444m, rgb_led_spi |
| Batch 29 | `port-py-decoders-batch-29` | 已完成 | arm_itm, arm_tpiu, bluetooth_h4, boost, crsf |
| Batch 30 | `port-py-decoders-batch-30` | 已完成 | j1708, midi, modbus, pan1321, pn532 |
| Batch 31 | `port-py-decoders-batch-31` | 已完成 | sbus_futaba, scs, ufcs, amulet_ascii, streletz |
| Batch 32 | `port-py-decoders-batch-32` | 已完成 | jtag_avr, jtag_ejtag, jtag_stm32 |
| Batch 33 | `port-py-decoders-batch-33` | 已完成 | onewire_network, ds2408, ds243x, ds28ea00, eeprom93xx |
| Batch 34 | `port-py-decoders-batch-34` | 已完成 | avclan, ethernet, arp, ipv4, udp |
| Batch 35 | `port-py-decoders-batch-35` | 已完成 | cfp, ps2_keyboard, ps2_mouse, usb_packet, usb_request |
| Batch 36 | `port-py-decoders-batch-36` | 已完成 | ook_oregon, ook_vis, ltar_smartdevice, ir_ltto_decode, sony_md_decode |
| Batch 37 | `port-py-decoders-batch-37` | 已完成 | sipi, pjon, tpm_fifo_tis, tm1637, tm1638, ltar_smartdevice_decode |

**全部37个子Spec已完成编写！**

---

## C解码器实现阻塞状态（API终审后更新）

### 被阻塞的批次

以下批次的解码器因底层C解码器未实现而被阻塞，需先完成对应底层解码器：

| 批次 | 被阻塞解码器 | 缺失的底层C解码器 | 阻塞原因 |
|------|------------|-----------------|---------|
| Batch 34 (部分) | avclan | iebus_c (Batch 2) | iebus仅有Python实现 |
| Batch 36 (全部) | ook_oregon, ook_vis, ltar_smartdevice, ir_ltto_decode, sony_md_decode | ook_c (Batch 10), afsk_c (Batch 5), ir_ltto_c (Batch 8), sony_md_c (Batch 12) | 下层均为Python解码器 |
| Batch 37 (全部) | sipi, pjon, tpm_fifo_tis, tm1637, tm1638, ltar_smartdevice_decode | lfast_c (Batch 9), pjdl_c (Batch 4), tmc_c (Batch 4), ltar_smartdevice_c (Batch 36) | 下层C解码器未实现 |

### 实施优先级（解锁最多下游解码器）

| 优先级 | 底层解码器 | 所属批次 | 解锁的下游解码器数 |
|--------|----------|---------|------------------|
| P0 | ethernet_c | Batch 34 | 3 (arp, ipv4, udp) |
| P0 | usb_packet_c | Batch 35 | 1 (usb_request) |
| P1 | tmc_c | Batch 4 | 2 (tm1637, tm1638) |
| P1 | pjdl_c | Batch 4 | 1 (pjon) |
| P1 | iebus_c | Batch 2 | 1 (avclan) |
| P2 | afsk_c | Batch 5 | 1 (ltar_smartdevice) → 1 (ltar_smartdevice_decode) |
| P2 | ook_c | Batch 10 | 2 (ook_oregon, ook_vis) |
| P2 | lfast_c | Batch 9 | 1 (sipi) |
| P2 | ir_ltto_c | Batch 8 | 1 (ir_ltto_decode) |
| P2 | sony_md_c | Batch 12 | 1 (sony_md_decode) |

### API变更记录（影响所有子Spec）

| 变更 | 旧名称 | 新名称 | 说明 |
|------|--------|--------|------|
| 枚举重命名 | SRD_OUTPUT_PYTHON | SRD_OUTPUT_PROTO | 旧名保留为#define别名 |
| 函数重命名 | c_decoder_put_python | c_decoder_put_proto | 旧名保留为#define别名 |
| 新增API | — | c_cond_wait_current | 等效Python self.wait({}) |
| 新增API | — | c_decoder_get_initial_pin | 等效Python self.initial_pins |
| 新增API | — | c_decoder_put_logic | SRD_OUTPUT_LOGIC输出 |
| 混合堆叠保护 | — | srd_inst_stack()拒绝C/Python混合 | 防止运行时崩溃 |
| BITS格式升级 | v1 (无时间戳) | v2 (per-bit ss/es) | spi_c和i2c_c已实现 |
