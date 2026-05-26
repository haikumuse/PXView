# 修复空真测试 — 为134个WARN解码器生成有效协议数据

## Why
当前215个C解码器测试中，134个（62%）是"空真"（vacuous truth）——Python和C解码器都产生0个annotations，因为 `test_factory.py` 生成的随机数据不包含有效协议帧。测试框架虽然PASS了，但完全没有验证解码逻辑的正确性。

## What Changes
- 在 `protocol_synthesizer.py` 中添加新的协议信号生成器，覆盖89个直接输入logic的根解码器
- 修改 `test_factory.py` 的 `generate_test_for_decoder()` 函数，为每个解码器生成对应协议的有效信号数据
- 完善依赖链：为19个依赖上游解码器输出的stack解码器正确配置stack和channels
- `run_all_tests.py` 的WARN状态已实现（上一步完成），无需修改

### 需要添加的协议生成器（按优先级分组）

#### 高优先级 — 常见协议（覆盖~40个解码器）
- **JTAGGenerator**: 4通道(TCK/TMS/TDI/TDO)，生成TAP状态机序列 → jtag_c, jtag_avr_c, jtag_ejtag_c, jtag_stm32_c
- **MDIOGenerator**: 2通道(MDC/MDIO)，生成Clause 22/45帧 → mdio_c, cfp_c
- **MicrowireGenerator**: 4通道(SK/SI/SO/CS)，生成读/写帧 → microwire_c, eeprom93xx_c
- **HDLCGenerator**: 2通道(RX/TX)，生成HDLC帧（flag+数据+CRC+flag）→ hdlc_c
- **I2SGenerator**: 3通道(SCLK/LRCK/SD)，生成I2S音频帧 → i2s_c
- **ISO7816Generator**: 2通道(CLK/IO)，生成智能卡ATR和TPU → iso7816_c
- **SWDGenerator**: 2通道(SWCLK/SWDIO)，生成SWD读/写请求 → swd_c, rvswd_c
- **OneWireGenerator**: 1通道，生成reset+presence+读写时序 → onewire_c, onewire_link_c, onewire_network_c, ds2408_c, ds243x_c, ds28ea00_c
- **PS2Generator**: 已存在，但test_factory未使用 → ps2_c, ps2_keyboard_c, ps2_mouse_c
- **CANGenerator**: 已存在，但test_factory对can_c/can_fd_c生成数据无效 → can_c, can_fd_c

#### 中优先级 — 特殊协议（覆盖~30个解码器）
- **NRZIGenerator**: 1通道，NRZI编码 → nrzi_c, 4b5b_c, ethernet_c, arp_c, ipv4_c, udp_c
- **ManchesterEncoder**: 已存在(ManchesterGenerator)，用于IR协议 → ir_nec_c, ir_rc5_c, ir_rc6_c, ir_sirc_c, ir_irmp_c, ir_ltto_c, ir_ltto_decode_c, ir_recoil_c
- **PWMGenerator**: 1通道，生成PWM波形 → pwm_c, counter_c, guess_bitrate_c, rpm_c, timing_c
- **DCCGenerator**: 1通道，生成DCC轨道信号 → dcc_c
- **DMX512Generator**: 1通道，生成DMX512 break+MAB+数据 → dmx512_c
- **DALIGenerator**: 1通道，生成DALI帧 → dali_c
- **CECGenerator**: 1通道，生成HDMI CEC帧 → cec_c
- **SPDIFGenerator**: 1通道，生成SPDIF双相标记编码 → spdif_c
- **WiegandGenerator**: 2通道(DATA0/DATA1)，生成Wiegand数据 → wiegand_c
- **OpenthermGenerator**: 1通道，生成Opentherm帧 → opentherm_c
- **SENTGenerator**: 1通道，生成SENT帧 → sent_c
- **MIPIRFFEGenerator**: 2通道(SCLK/SDATA)，生成RFFE命令 → mipi_rffe_c

#### 低优先级 — 罕见/复杂协议（覆盖~20个解码器）
- **J1850VPWGenerator**: 1通道 → sae_j1850_vpw_c
- **MCS48Generator**: 14通道(地址+数据总线) → mcs48_c
- **GPIBGenerator**: 16通道 → gpib_c
- **Z80Generator**: 11通道 → z80_c
- **LPCGenerator**: 6通道 → lpc_c
- **SDCardGenerator**: 2通道(CMD/DAT) → sdcard_sd_c
- **SDIOGenerator**: 2通道(CLK/CMD) → sdio_c
- **QSPIGenerator**: 2通道(CLK/DQ0) → qspi_c
- **SPIDualQuadGenerator**: 3通道 → spi_dual_quad_c
- **ST7735/ST7789Generator**: 4通道(SPI+DC+RST) → st7735_c, st7789_c
- **SpaceWireGenerator**: 2通道(DIN/SIN) → spacewire_c
- **FlexRayGenerator**: 1通道 → flexray_c
- **MapleBusGenerator**: 2通道 → maple_bus_c
- **IECGenerator**: 3通道 → iec_c
- **StepperMotorGenerator**: 2通道 → stepper_motor_c
- **XY2_100Generator**: 3通道 → xy2_100_c

#### 特殊处理 — 无法生成有效数据的解码器
- **graycode_c, numbers_and_state_c**: 0通道，纯元数据解码器，需要特殊处理
- **parallel_c**: 已有测试数据（9通道），但0 annotations，可能需要检查
- **dsi_c, dcf77_c, am230x_c, bean_c, lfast_c, morse_c, mvb_c, swi_c, swim_c, t55xx_c, sdq_c, qi_c, pxx1_c, pjdl_c, pjon_c, rc_encode_c, rgb_led_ws281x_c, rinnai_control_panel_c, carrerra_c, c2_c, avr_pdi_c, caliper_c, delta_sigma_c, em4100_c, em4305_c, eth_an_c, fsi_c, iebus_c, ieee488_c, miller_c, one_single_wire_c, ook_c, sony_md_c, tdm_audio_c, tlc5620_c, usb_power_delivery_c**: 各自需要专用生成器

## Impact
- Affected code: `libsigrokdecode/tests/protocol_synthesizer.py`, `libsigrokdecode/tests/test_factory.py`
- 测试结果将从 81 PASS + 134 WARN 变为 215 PASS + 0 WARN
- 不影响任何C解码器源代码

## ADDED Requirements

### Requirement: 协议信号生成器
系统 SHALL 在 `protocol_synthesizer.py` 中提供协议信号生成器，为每个直接输入logic的解码器生成包含至少一个完整协议帧的bitstream数据。

#### Scenario: JTAG解码器获得有效数据
- **WHEN** test_factory为jtag_c生成测试数据
- **THEN** input.bin包含至少一个完整的JTAG TAP序列（Test-Reset → Run-Test-Idle → Shift-DR → ...）

#### Scenario: 依赖链解码器获得有效数据
- **WHEN** test_factory为jtag_avr_c生成测试数据
- **THEN** config.json中stack包含jtag_c，且jtag_c的输入数据包含有效JTAG序列

### Requirement: 空真检测
系统 SHALL 在 `run_all_tests.py` 中将双方0 annotations的情况标记为WARN状态（已实现）。

### Requirement: 最小有效输出
系统 SHALL 确保每个非元数据解码器的测试数据能触发至少1个annotation输出。
