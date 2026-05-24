# SPI上层协议解码器移植任务分解

## 任务总览

将5个Python SPI上层解码器移植为C解码器，按复杂度从低到高排序实施。

---

## Task 1: nes_gamepad_c — NES游戏手柄解码器

**文件**: `libsigrokdecode/c_decoders/nes_gamepad_c.c`
**复杂度**: ★☆☆☆☆（最简单）
**预估代码行数**: ~150行

### 1.1 创建文件骨架

- [ ] 创建 `nes_gamepad_c.c`
- [ ] 添加标准头文件引用：`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`
- [ ] 定义annotation枚举：`ANN_BUTTON=0, ANN_NO_PRESS, ANN_NOT_CONNECTED, NUM_ANN`

### 1.2 定义静态数据

- [ ] 定义 `nes_gamepad_inputs[] = {"spi", NULL}`
- [ ] 定义 `nes_gamepad_outputs[] = {NULL}`（无输出）
- [ ] 定义 `nes_gamepad_tags[] = {"Retro computing", NULL}`
- [ ] 定义 `nes_gamepad_options[]` — variant选项，id="variant", idn="dec_nes_gamepad_opt_variant"
- [ ] 定义 `nes_gamepad_ann_labels[][3]` — 3个annotation，第一列为空字符串
- [ ] 定义3个 `row_classes[]` 数组，每个以`-1`结尾
- [ ] 定义 `nes_gamepad_ann_rows[]` — 3行：buttons, no-presses, not-connected-vals

### 1.3 定义状态结构体

- [ ] 定义 `nes_gamepad_state`：`int out_ann; int variant;`

### 1.4 实现回调函数

- [ ] `nes_gamepad_reset()` — g_malloc0分配私有数据，memset清零
- [ ] `nes_gamepad_start()` — 注册SRD_OUTPUT_ANN，读取variant选项
- [ ] `nes_gamepad_decode()` — 空函数 `(void)di;`
- [ ] `nes_gamepad_destroy()` — g_free释放私有数据

### 1.5 实现recv_proto

- [ ] 仅处理`strcmp(cmd, "DATA") == 0`
- [ ] 解析data获取MISO值（data[8..15]为MISO uint64 LE）
- [ ] miso==0xFF → `C_ANN_PUT(... ANN_NO_PRESS, "No button is pressed")`
- [ ] miso==0x00 → `C_ANN_PUT(... ANN_NOT_CONNECTED, "Gamepad is not connected")`
- [ ] 其他值 → 按位解析按钮：A/B/Select/Start/North/South/West/East，bit=0为按下
- [ ] 拼接按钮名用" + "连接

### 1.6 定义srd_c_decoder结构体

- [ ] `.id = "nes_gamepad_c"`, `.name = "NES gamepad(C)"`
- [ ] `.inputs = nes_gamepad_inputs`, `.num_inputs = 1`
- [ ] `.outputs = NULL`, `.num_outputs = 0`
- [ ] `.channels = NULL`, `.num_channels = 0`
- [ ] `.optional_channels = NULL`, `.num_optional_channels = 0`
- [ ] `.recv_proto = nes_gamepad_recv_proto`
- [ ] 其余字段按规范填写

### 1.7 实现导出函数

- [ ] `srd_c_decoder_entry()` — 设置variant选项默认值和可选值列表
- [ ] `srd_c_decoder_api_version()` — 返回`SRD_C_DECODER_API_VERSION`

### 1.8 构建集成

- [ ] 在`CMakeLists.txt`的`C_DECODERS`列表中添加`nes_gamepad_c`
- [ ] 运行`build_incremental.cmd`验证编译

---

## Task 2: ssi32_c — 同步串行接口(32位)解码器

**文件**: `libsigrokdecode/c_decoders/ssi32_c.c`
**复杂度**: ★★☆☆☆
**预估代码行数**: ~250行

### 2.1 创建文件骨架

- [ ] 创建 `ssi32_c.c`
- [ ] 添加标准头文件引用
- [ ] 定义annotation枚举：`ANN_CTRL_TX=0, ANN_ACK_TX, ANN_CTRL_RX, ANN_ACK_RX, NUM_ANN`

### 2.2 定义静态数据

- [ ] 定义 `ssi32_inputs[] = {"spi", NULL}`
- [ ] 定义 `ssi32_outputs[] = {NULL}`
- [ ] 定义 `ssi32_tags[] = {"Embedded/industrial", NULL}`
- [ ] 定义 `ssi32_options[]` — msgsize选项，id="msgsize", idn="dec_ssi32_opt_msgsize"
- [ ] 定义 `ssi32_ann_labels[][3]` — 4个annotation
- [ ] 定义2个 `row_classes[]` 数组：tx(0,1), rx(2,3)
- [ ] 定义 `ssi32_ann_rows[]` — 2行：tx, rx

### 2.3 定义状态结构体

```c
#define SSI32_MAX_BYTES 128
typedef struct {
    int out_ann;
    int msgsize;
    uint8_t mosi_bytes[SSI32_MAX_BYTES];
    uint8_t miso_bytes[SSI32_MAX_BYTES];
    uint64_t es_array[SSI32_MAX_BYTES];
    int num_bytes;
    uint64_t ss_cmd;
} ssi32_state;
```

### 2.4 实现回调函数

- [ ] `ssi32_reset()` — 分配私有数据，设置默认msgsize=64
- [ ] `ssi32_start()` — 注册SRD_OUTPUT_ANN，读取msgsize选项
- [ ] `ssi32_decode()` — 空函数
- [ ] `ssi32_destroy()` — 释放私有数据

### 2.5 实现recv_proto

- [ ] `CS-CHANGE` → 重置num_bytes=0
- [ ] `DATA` → 解析MOSI/MISO，收集字节
- [ ] 第一个字节时记录ss_cmd=start_sample
- [ ] 判断帧类型：
  - `mosi_bytes[0] & 0x80` → ACK帧，需4字节
  - 否则 → CTRL帧，需msgsize字节

### 2.6 实现handle_ack

- [ ] 输出 `> ACK:0x%02x` (ANN_ACK_TX)
- [ ] 输出 `< ACK:0x%02x` (ANN_ACK_RX)
- [ ] 使用es_array[0]作为结束sample

### 2.7 实现handle_ctrl

- [ ] 解析tx_size = mosi_bytes[2], rx_size = miso_bytes[2]
- [ ] 格式化TX CTRL：`> CTRL:0x%02x, LUN:0x%02x, SIZE:0x%02x, CRC:0x%02x, DATA:0x...`
- [ ] 格式化RX CTRL：`< CTRL:0x%02x, LUN:0x%02x, SIZE:0x%02x, CRC:0x%02x, DATA:0x...`
- [ ] 使用es_array[tx_size+3]和es_array[rx_size+3]作为结束sample

### 2.8 定义srd_c_decoder和导出函数

- [ ] `.id = "ssi32_c"`, `.name = "SSI32(C)"`
- [ ] `.recv_proto = ssi32_recv_proto`
- [ ] `srd_c_decoder_entry()` — 设置msgsize默认值64

### 2.9 构建集成

- [ ] 在`CMakeLists.txt`的`C_DECODERS`列表中添加`ssi32_c`
- [ ] 编译验证

---

## Task 3: nrf24l01_c — Nordic nRF24L01(+)解码器

**文件**: `libsigrokdecode/c_decoders/nrf24l01_c.c`
**复杂度**: ★★★★☆
**预估代码行数**: ~600行

### 3.1 创建文件骨架

- [ ] 创建 `nrf24l01_c.c`
- [ ] 添加标准头文件引用
- [ ] 定义annotation枚举：`ANN_CMD=0, ANN_TX, ANN_REG, ANN_RX, ANN_WARN, NUM_ANN`

### 3.2 定义寄存器表

- [ ] 定义 `nrf24l01_regs[]` 静态数组 — 26个寄存器(addr, name, size)
- [ ] 定义 `xn297_ext_regs[]` — 5个扩展寄存器
- [ ] 实现 `nrf24l01_get_reg_size()` 函数 — 根据chip_type查找寄存器大小

### 3.3 定义静态数据

- [ ] 定义 `nrf24l01_inputs[] = {"spi", NULL}`
- [ ] 定义 `nrf24l01_outputs[] = {NULL}`
- [ ] 定义 `nrf24l01_tags[] = {"IC", "Wireless/RF", NULL}`
- [ ] 定义 `nrf24l01_options[]` — chip选项，id="chip", idn="dec_nrf24l01_opt_chip"
- [ ] 定义 `nrf24l01_ann_labels[][3]` — 5个annotation
- [ ] 定义3个 `row_classes[]` 数组：commands(0,1), responses(2,3), warnings(4)
- [ ] 定义 `nrf24l01_ann_rows[]` — 3行

### 3.4 定义状态结构体

```c
#define NRF24_MAX_CMD_BYTES 64
typedef struct {
    int out_ann;
    int chip_type; // 0=nrf24l01, 1=xn297
    int first;
    int cs_was_released;
    char cmd[32];
    int dat;
    int min_bytes;
    int max_bytes;
    uint8_t mosi_bytes[NRF24_MAX_CMD_BYTES];
    uint8_t miso_bytes[NRF24_MAX_CMD_BYTES];
    int num_bytes;
    uint64_t mb_ss;
    uint64_t mb_es;
    uint64_t cmd_ss;
    uint64_t cmd_es;
} nrf24l01_state;
```

### 3.5 实现回调函数

- [ ] `nrf24l01_reset()` — 分配私有数据，调用next()重置状态
- [ ] `nrf24l01_start()` — 注册SRD_OUTPUT_ANN，读取chip选项
- [ ] `nrf24l01_decode()` — 空函数
- [ ] `nrf24l01_destroy()` — 释放私有数据

### 3.6 实现命令解析

- [ ] `nrf24l01_parse_command()` — 解析MOSI命令字节
  - R_REGISTER (0x00-0x1F)
  - W_REGISTER (0x20-0x3F)
  - ACTIVATE (0x50)
  - R_RX_PAYLOAD (0x61)
  - R_RX_PL_WID (0x60)
  - W_TX_PAYLOAD (0xA0)
  - W_TX_PAYLOAD_NOACK (0xB0)
  - W_ACK_PAYLOAD (0xA8-0xAF)
  - FLUSH_TX (0xE1)
  - FLUSH_RX (0xE2)
  - REUSE_TX_PL (0xE3)
  - NOP (0xFF)
  - xn297扩展: CE_FSPI_ON (0xFD), CE_FSPI_OFF (0xFC), RST_FSPI (0x53)

### 3.7 实现命令格式化

- [ ] `nrf24l01_format_command()` — 返回命令标签字符串
  - R_REGISTER → `Cmd R_REGISTER "REG_NAME"`
  - 其他 → `Cmd COMMAND_NAME`

### 3.8 实现数据格式化

- [ ] `nrf24l01_decode_mb_data()` — 格式化多字节数据
  - always_hex模式：每个字节`%02X`
  - 非hex模式：可打印字符直接显示，不可打印`\x%02X`
  - 输出格式：`label = "{$}"` + `@data`（双行annotation）

### 3.9 实现寄存器解码

- [ ] `nrf24l01_decode_register()` — 解码寄存器值
  - 查找寄存器名
  - 多字节寄存器LSByte first（需reversed）
  - W_REGISTER合并命令和寄存器名

### 3.10 实现finish_command

- [ ] R_REGISTER → decode_register(ANN_REG, miso_bytes)
- [ ] W_REGISTER → decode_register(ANN_CMD, mosi_bytes)
- [ ] R_RX_PAYLOAD → decode_mb_data(ANN_RX, miso_bytes, "RX payload")
- [ ] W_TX_PAYLOAD → decode_mb_data(ANN_TX, mosi_bytes, "TX payload")
- [ ] W_TX_PAYLOAD_NOACK → decode_mb_data(ANN_TX, mosi_bytes, "TX payload")
- [ ] W_ACK_PAYLOAD → decode_mb_data(ANN_TX, mosi_bytes, "ACK payload for pipe N")
- [ ] R_RX_PL_WID → 输出payload宽度
- [ ] ACTIVATE → 检查数据字节(0x8c/0x73)
- [ ] RST_FSPI → 检查数据字节(0x5a/0xa5)

### 3.11 实现recv_proto

- [ ] `CS-CHANGE` → CS上升沿时处理已收集命令，调用finish_command，reset状态
- [ ] `TRANSFER` → 同上
- [ ] `DATA` + cs_was_released：
  - first字节：解析命令(MOSI)，解码STATUS寄存器(MISO)
  - 后续字节：收集到mosi/miso数组
  - 超过max：输出warning

### 3.12 定义srd_c_decoder和导出函数

- [ ] `.id = "nrf24l01_c"`, `.name = "nRF24L01(+)(C)"`
- [ ] `.recv_proto = nrf24l01_recv_proto`
- [ ] `srd_c_decoder_entry()` — 设置chip选项默认值和可选值列表

### 3.13 构建集成

- [ ] 在`CMakeLists.txt`的`C_DECODERS`列表中添加`nrf24l01_c`
- [ ] 编译验证

---

## Task 4: nrf905_c — Nordic nRF905解码器

**文件**: `libsigrokdecode/c_decoders/nrf905_c.c`
**复杂度**: ★★★☆☆
**预估代码行数**: ~450行

### 4.1 创建文件骨架

- [ ] 创建 `nrf905_c.c`
- [ ] 添加标准头文件引用
- [ ] 定义annotation枚举：`ANN_CMD=0, ANN_REG_WR, ANN_REG_RD, ANN_TX, ANN_RX, ANN_RESP, ANN_WARN, NUM_ANN`

### 4.2 定义配置寄存器字段表

- [ ] 定义 `nrf905_reg_field` 结构体：`{name, stbit, nbits, opts[8]}`
- [ ] 定义 `cfg_reg_0[]` 到 `cfg_reg_9[]` — 每个寄存器的字段数组
- [ ] 定义 `chn_cfg[]` — CHANNEL_CONFIG命令字段
- [ ] 定义 `stat_reg[]` — STATUS寄存器字段
- [ ] 定义 `cfg_reg_fields[10]` — 指向cfg_reg_0..9的指针数组

### 4.3 定义静态数据

- [ ] 定义 `nrf905_inputs[] = {"spi", NULL}`
- [ ] 定义 `nrf905_outputs[] = {NULL}`
- [ ] 定义 `nrf905_tags[] = {"IC", "Wireless/RF", NULL}`
- [ ] 无options
- [ ] 定义 `nrf905_ann_labels[][3]` — 7个annotation
- [ ] 定义6个 `row_classes[]` 数组
- [ ] 定义 `nrf905_ann_rows[]` — 6行

### 4.4 定义状态结构体

```c
#define NRF905_MAX_BYTES 64
typedef struct {
    int out_ann;
    int cs_asserted;
    uint64_t cmd_ss;
    uint64_t cmd_es;
    uint8_t mosi_bytes[NRF905_MAX_BYTES];
    uint64_t mosi_ss[NRF905_MAX_BYTES];
    uint64_t mosi_es[NRF905_MAX_BYTES];
    uint8_t miso_bytes[NRF905_MAX_BYTES];
    uint64_t miso_ss[NRF905_MAX_BYTES];
    uint64_t miso_es[NRF905_MAX_BYTES];
    int num_bytes;
} nrf905_state;
```

### 4.5 实现工具函数

- [ ] `nrf905_extract_bits()` — 从字节中提取指定位段
- [ ] `nrf905_extract_vars()` — 遍历字段数组，拼接名称=值(含义)字符串

### 4.6 实现命令处理函数

- [ ] `nrf905_handle_WC()` — W_CONFIG命令
- [ ] `nrf905_handle_RC()` — R_CONFIG命令
- [ ] `nrf905_handle_WTP()` — Write TX payload
- [ ] `nrf905_handle_RTP()` — Read TX payload
- [ ] `nrf905_handle_WTA()` — Write TX address
- [ ] `nrf905_handle_RTA()` — Read TX address
- [ ] `nrf905_handle_RRP()` — Read RX payload
- [ ] `nrf905_handle_CC()` — CHANNEL_CONFIG
- [ ] `nrf905_handle_STAT()` — STATUS寄存器

### 4.7 实现process_cmd

- [ ] 解析第一个MOSI字节确定命令类型
- [ ] 输出命令名annotation (ANN_CMD)
- [ ] 处理STATUS字节 (MISO第一字节)
- [ ] 调用对应的命令处理函数

### 4.8 实现回调函数

- [ ] `nrf905_reset()` — 分配私有数据，清零
- [ ] `nrf905_start()` — 注册SRD_OUTPUT_ANN
- [ ] `nrf905_decode()` — 空函数
- [ ] `nrf905_destroy()` — 释放私有数据

### 4.9 实现recv_proto

- [ ] `CS-CHANGE` → 跟踪CS状态，上升沿时处理命令
- [ ] `DATA` → 仅当CS asserted时收集字节

### 4.10 定义srd_c_decoder和导出函数

- [ ] `.id = "nrf905_c"`, `.name = "nRF905(C)"`
- [ ] `.recv_proto = nrf905_recv_proto`
- [ ] `srd_c_decoder_entry()` — 无options，直接返回

### 4.11 构建集成

- [ ] 在`CMakeLists.txt`的`C_DECODERS`列表中添加`nrf905_c`
- [ ] 编译验证

---

## Task 5: rfm12_c — HopeRF RFM12解码器

**文件**: `libsigrokdecode/c_decoders/rfm12_c.c`
**复杂度**: ★★★★★（最复杂）
**预估代码行数**: ~800行

### 5.1 创建文件骨架

- [ ] 创建 `rfm12_c.c`
- [ ] 添加标准头文件引用，额外添加`math.h`（pow函数）
- [ ] 定义annotation枚举：`ANN_CMD=0, ANN_PARAMS, ANN_DISABLED, ANN_RETURN, ANN_DISABLED_RETURN, ANN_INTERPRETATION, NUM_ANN`

### 5.2 定义静态数据

- [ ] 定义 `rfm12_inputs[] = {"spi", NULL}`
- [ ] 定义 `rfm12_outputs[] = {NULL}`
- [ ] 定义 `rfm12_tags[] = {"Wireless/RF", NULL}`
- [ ] 无options
- [ ] 定义 `rfm12_ann_labels[][3]` — 6个annotation
- [ ] 定义3个 `row_classes[]` 数组：commands(0,1,2), return(3,4), interpretation(5)
- [ ] 定义 `rfm12_ann_rows[]` — 3行

### 5.3 定义状态结构体

```c
typedef struct {
    int out_ann;
    uint8_t mosi_bytes[2];
    uint8_t miso_bytes[2];
    int num_bytes;

    // 位级标注位置跟踪（简化版可省略）
    int row_pos[3];
    int ann_to_row[6];

    // 状态跟踪
    uint8_t last_status[2];
    uint8_t last_config;
    uint8_t last_power;
    uint16_t last_freq;
    uint8_t last_data_rate;
    uint8_t last_fifo_and_reset;
    uint8_t last_afc;
    uint8_t last_transceiver;
    uint8_t last_pll;
} rfm12_state;
```

### 5.4 实现命令处理函数（17个）

- [ ] `rfm12_handle_configuration_cmd()` — 配置命令(0x80)
- [ ] `rfm12_handle_power_management_cmd()` — 电源管理(0x82)
- [ ] `rfm12_handle_frequency_setting_cmd()` — 频率设置(0xA0-0xAF)
- [ ] `rfm12_handle_data_rate_cmd()` — 数据速率(0xC6)
- [ ] `rfm12_handle_receiver_control_cmd()` — 接收控制(0x90-0x97)
- [ ] `rfm12_handle_data_filter_cmd()` — 数据滤波(0xC2)
- [ ] `rfm12_handle_fifo_and_reset_cmd()` — FIFO和复位(0xCA)
- [ ] `rfm12_handle_synchron_pattern_cmd()` — 同步模式(0xCE)
- [ ] `rfm12_handle_fifo_read_cmd()` — FIFO读取(0xB0)
- [ ] `rfm12_handle_afc_cmd()` — AFC(0xC4)
- [ ] `rfm12_handle_transceiver_control_cmd()` — 收发控制(0x98-0x99)
- [ ] `rfm12_handle_pll_setting_cmd()` — PLL设置(0xCC)
- [ ] `rfm12_handle_transmitter_register_cmd()` — 发送寄存器(0xB8)
- [ ] `rfm12_handle_software_reset_cmd()` — 软件复位(0xFE)
- [ ] `rfm12_handle_wake_up_timer_cmd()` — 唤醒定时器(0xE0-0xFF)
- [ ] `rfm12_handle_low_duty_cycle_cmd()` — 低占空比(0xC8)
- [ ] `rfm12_handle_low_battery_detector_cmd()` — 低电池检测(0xC0)
- [ ] `rfm12_handle_status_read_cmd()` — 状态读取(0x00)

### 5.5 实现辅助函数

- [ ] `rfm12_advance_ann()` — 推进row_pos
- [ ] `rfm12_putx()` — 在当前row_pos位置标注
- [ ] `rfm12_describe_bits()` — 位级标注（启用/禁用）
- [ ] `rfm12_describe_return_bits()` — 返回值位级标注
- [ ] `rfm12_describe_changed_bits()` — 变化位标注

**注意**：位级标注功能需要处理BITS协议命令。如果简化实现，可以只做字节级annotation，跳过BITS处理。建议先实现字节级版本，验证基本功能后再添加位级标注。

### 5.6 实现命令分发

- [ ] `rfm12_handle_cmd()` — 根据命令字节分发到对应处理函数

### 5.7 实现回调函数

- [ ] `rfm12_reset()` — 分配私有数据，初始化Power-On-Reset默认值
- [ ] `rfm12_start()` — 注册SRD_OUTPUT_ANN
- [ ] `rfm12_decode()` — 空函数
- [ ] `rfm12_destroy()` — 释放私有数据

### 5.8 实现recv_proto

- [ ] `DATA` → 收集2字节后处理命令
- [ ] `BITS` → 可选：收集位级数据用于位级标注（简化版可跳过）

### 5.9 定义srd_c_decoder和导出函数

- [ ] `.id = "rfm12_c"`, `.name = "RFM12(C)"`
- [ ] `.recv_proto = rfm12_recv_proto`
- [ ] `srd_c_decoder_entry()` — 无options，直接返回

### 5.10 构建集成

- [ ] 在`CMakeLists.txt`的`C_DECODERS`列表中添加`rfm12_c`
- [ ] 编译验证

---

## Task 6: 最终验证

### 6.1 编译验证

- [ ] 运行 `build_incremental.cmd` 确认所有5个解码器编译成功
- [ ] 检查输出DLL：`build.dir/decoders/c_decoders/nes_gamepad_c.dll` 等5个文件

### 6.2 运行时验证

- [ ] 启动PXView，在解码器列表中确认5个新C解码器可见
- [ ] 每个解码器能正确堆叠在SPI解码器之上
- [ ] 选项配置正确显示和保存

---

## 依赖关系

```
Task 1 (nes_gamepad_c) ─── 无依赖，可独立开始
Task 2 (ssi32_c)        ─── 无依赖，可独立开始
Task 3 (nrf24l01_c)     ─── 建议Task 1完成后开始（参考简单模式）
Task 4 (nrf905_c)       ─── 建议Task 3完成后开始（参考寄存器模式）
Task 5 (rfm12_c)        ─── 建议Task 4完成后开始（最复杂）
Task 6 (最终验证)        ─── 依赖Task 1-5全部完成
```

Task 1和Task 2可以并行实施。
