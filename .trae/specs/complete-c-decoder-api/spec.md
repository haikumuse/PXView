# C解码器API完备性补全 Spec

## Why
C解码器应自成体系，拥有完整的API和调用栈，不依赖Python解码器。当前C解码器API在单层解码场景下基本完备，但与Python API相比仍有关键缺失：空条件等待、初始引脚值访问、BITS消息时间戳等。这些缺失导致C解码器无法精确实现部分协议逻辑，必须先补全API再实施178个解码器的C移植。

## What Changes
- 新增 `c_cond_wait_current()` API（等价Python self.wait({})）
- 新增 `c_decoder_get_initial_pin()` API（访问初始引脚状态）
- 扩展 BITS 消息格式增加 bit 级时间戳
- 删除Python→C桥接需求（C解码器不依赖Python解码器）
- 明确C解码器依赖规则：C解码器只能依赖已有C实现的底层解码器

## Impact
- Affected code: `libsigrokdecode/c_decoder_api.c`, `libsigrokdecode/c_decoders/c_decoder_utils.h`, `libsigrokdecode/libsigrokdecode.h`
- Affected specs: `fix-c-decoder-api-gaps` spec中的Python→C桥接需求删除
- Affected decoders: 所有需要初始引脚状态的C解码器、所有SPI/I2C上层C解码器

---

## ADDED Requirements

### Requirement: 空条件等待API（c_cond_wait_current）

C解码器框架 SHALL 提供 `c_cond_wait_current()` API，等价于Python的 `self.wait({})`，用于获取当前采样点的引脚值而不等待任何边沿。

#### Scenario: SPI解码器获取初始CS#状态
- **WHEN** SPI C解码器在decode()开始时需要知道CS#的初始电平
- **THEN** 调用 `c_cond_wait_current(di, &samplenum)` 立即返回当前采样点的所有引脚值
- **AND** 通过 `c_decoder_get_pin(di, CS, samplenum)` 读取CS#初始电平

#### API设计
```c
// 等价于Python self.wait({})，立即返回当前采样位置
// samplenum: 输出当前采样号
// 返回: SRD_OK 或错误码
SRD_API int c_cond_wait_current(struct srd_decoder_inst *di, uint64_t *samplenum);
```

#### 实现方案
在 `c_decoder_api.c` 中实现：设置 `di->skip_count = 0`，调用 `process_samples_until_condition_match()` 使其立即返回当前采样位置。

#### 受影响解码器
- spi_c.c（获取初始CS#状态）
- spi-fast（同上）
- 所有需要在decode()开始时获取初始引脚状态的C解码器

---

### Requirement: 初始引脚值访问API（c_decoder_get_initial_pin）

C解码器框架 SHALL 提供 `c_decoder_get_initial_pin()` API，用于读取用户设置的初始引脚状态。

#### Scenario: UART解码器读取初始线路状态
- **WHEN** UART C解码器需要知道RX/TX线的初始电平（高电平=空闲）
- **THEN** 调用 `c_decoder_get_initial_pin(di, ch)` 返回初始引脚值

#### API设计
```c
// 读取初始引脚值（来自srd_inst_initial_pins_set_all()设置的值）
// ch: 通道索引
// 返回: 引脚值(0/1)，未设置返回0xFF
SRD_API uint8_t c_decoder_get_initial_pin(struct srd_decoder_inst *di, int ch);
```

#### 实现方案
从 `di->old_pins_array[ch]` 读取初始引脚值。

---

### Requirement: BITS消息增加bit级时间戳

C解码器框架 SHALL 在 BITS 协议消息中包含每个bit的起始/结束采样号，使上层C解码器能够实现精确的位级标注。

#### Scenario: SPI上层解码器接收BITS数据
- **WHEN** SPI C解码器输出BITS消息
- **THEN** 上层C解码器通过 `recv_proto()` 接收到的BITS消息data格式包含每个bit的ss/es

#### 当前BITS消息格式（无时间戳）
```
data[0] = have_mosi
data[1..8] = mosi_bits (每字节1bit, 最多64bit)
data[9] = have_miso
data[10..17] = miso_bits (每字节1bit, 最多64bit)
```

#### 新BITS消息格式（含时间戳）
```
data[0] = have_mosi (bit0) | have_miso (bit1)
data[1] = mosi_bit_count (uint8_t)
data[2..2+count*17-1] = 每bit: [value(1B)][ss(8B LE)][es(8B LE)]
data[2+count*17] = have_miso (冗余，与data[0]bit1一致)
data[2+count*17+1] = miso_bit_count (uint8_t)
data[2+count*17+2..] = 每bit: [value(1B)][ss(8B LE)][es(8B LE)]
```

#### 受影响解码器
- spi_c.c, i2c_c.c（BITS输出方，需修改输出格式）
- Batch-20/21/22/23/26/27/32 的上层解码器（BITS消费方，需更新解析逻辑）

---

## MODIFIED Requirements

### Requirement: C解码器依赖规则

C解码器 SHALL 只能依赖已有C实现的底层解码器。如果C解码器的inputs指定的协议尚无C实现，则该C解码器必须等待底层C解码器先实现。

#### 规则
1. C解码器的 `inputs` 字段只能引用已有C解码器实现的协议
2. 如果底层协议仅有Python实现，C上层解码器标记为"阻塞"，等待底层C解码器实现后解除
3. 不实现Python→C桥接，C解码器完全独立于Python

#### 阻塞解码器清单（底层仅有Python实现）

| 上层C解码器 | 依赖的Python底层 | 所在批次 |
|------------|-----------------|---------|
| avclan_c | iebus | Batch-34 |
| ook_oregon_c | ook | Batch-36 |
| ook_vis_c | ook | Batch-36 |
| ltar_smartdevice_c | afsk_bits | Batch-36 |
| ir_ltto_decode_c | ir_ltto | Batch-36 |
| sony_md_decode_c | sony_md | Batch-36 |
| sipi_c | lfast | Batch-37 |
| pjon_c | pjon_link | Batch-37 |
| tpm_fifo_tis_c | tpm-tis | Batch-37 |
| tm1637_c | tmc | Batch-37 |
| tm1638_c | tmc | Batch-37 |
| ltar_smartdevice_decode_c | ltar_smartdevice | Batch-37 |
| ps2_keyboard_c | ps2 | Batch-35 |
| ps2_mouse_c | ps2 | Batch-35 |
| usb_packet_c | usb_signalling | Batch-35 |
| usb_request_c | usb_packet | Batch-35 |

注：ps2_c和usb_signalling_c已有C实现但缺少Python输出，需先修改添加输出。

---

## REMOVED Requirements

### Requirement: Python→C proto桥接
**Reason**: C解码器应自成体系，不依赖Python解码器。如果C解码器需要Python底层解码器的输出，应先将Python底层解码器移植为C实现。
**Migration**: 上述阻塞解码器清单中的解码器需等待底层C解码器实现后再移植。

---

## API差距完整对照表

| Python API | C API | 对等性 | 缺失 | 优先级 |
|-----------|-------|--------|------|--------|
| self.wait({ch:'r'/'f'/'h'/'l'/'e'}) | c_cond_rise/fall/high/low/edge | 完全对等 | 无 | - |
| self.wait([{cond1}, {cond2}]) | c_cond_or + c_cond_wait | 完全对等 | 无 | - |
| self.wait({}) | **缺失** | 不对等 | c_cond_wait_current | P1 |
| self.matched | c_cond_wait matched参数 | 完全对等 | 无 | - |
| {'skip': N} | c_cond_skip | 基本对等 | 无 | - |
| self.put(ss,es,out_ann,[cls,[texts]]) | C_ANN_PUT | 完全对等 | 无 | - |
| self.put(ss,es,out_python,[cmd,data]) | c_decoder_put_python | 功能对等 | 无 | - |
| self.put(ss,es,out_binary,data) | c_decoder_put_binary | 完全对等 | 无 | - |
| self.put(ss,es,out_meta,id,val) | c_decoder_put_meta_int/double | 完全对等 | 无 | - |
| self.register(output_type) | c_decoder_register_output | 完全对等 | 无 | - |
| self.options['key'] | c_decoder_get_option_int/double/string | 完全对等 | 无 | - |
| self.has_channel(ch) | c_decoder_has_channel | 完全对等 | 无 | - |
| self.samplenum | c_decoder_get_last_samplenum | 完全对等 | 无 | - |
| self.samplerate | c_decoder_get_samplerate | 完全对等 | 无 | - |
| self.initial_pins | **缺失** | 不对等 | c_decoder_get_initial_pin | P2 |
| common.bitpack/bcd2int | **缺失** | 不对等 | 可在各解码器内联实现 | P3 |
| BITS消息含bit时间戳 | **缺失** | 不对等 | 扩展BITS格式 | P1 |
