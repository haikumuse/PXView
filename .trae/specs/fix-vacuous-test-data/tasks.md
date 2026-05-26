# Tasks

- [ ] Task 1: 添加高优先级协议生成器到 protocol_synthesizer.py
  - [ ] SubTask 1.1: JTAGGenerator — 4通道(TCK/TMS/TDI/TDO)，生成TAP状态机序列
  - [ ] SubTask 1.2: MDIOGenerator — 2通道(MDC/MDIO)，生成Clause 22读帧
  - [ ] SubTask 1.3: MicrowireGenerator — 4通道(SK/SI/CS)，生成读帧
  - [ ] SubTask 1.4: HDLCGenerator — 生成HDLC帧(flag 0x7E + 数据 + CRC + flag)
  - [ ] SubTask 1.5: I2SGenerator — 3通道(SCLK/LRCK/SD)，生成I2S音频帧
  - [ ] SubTask 1.6: ISO7816Generator — 2通道(CLK/IO)，生成ATR
  - [ ] SubTask 1.7: SWDGenerator — 2通道(SWCLK/SWDIO)，生成读请求
  - [ ] SubTask 1.8: OneWireGenerator — 1通道，生成reset+presence+读写时序
  - [ ] SubTask 1.9: 修复CANGenerator — can_c/can_fd_c当前0 annotations
  - [ ] SubTask 1.10: 修复PS2Generator — 已存在但test_factory未调用

- [ ] Task 2: 添加中优先级协议生成器到 protocol_synthesizer.py
  - [ ] SubTask 2.1: NRZIGenerator — 1通道NRZI编码
  - [ ] SubTask 2.2: IRGenerator系列 — ir_nec_c, ir_rc5_c, ir_rc6_c, ir_sirc_c, ir_irmp_c, ir_ltto_c, ir_recoil_c
  - [ ] SubTask 2.3: PWMGenerator — 1通道PWM波形
  - [ ] SubTask 2.4: DCCGenerator — 1通道DCC轨道信号
  - [ ] SubTask 2.5: DMX512Generator — 1通道DMX512帧
  - [ ] SubTask 2.6: DALIGenerator — 1通道DALI帧
  - [ ] SubTask 2.7: CECGenerator — 1通道HDMI CEC帧
  - [ ] SubTask 2.8: SPDIFGenerator — 1通道双相标记编码
  - [ ] SubTask 2.9: WiegandGenerator — 2通道(DATA0/DATA1)
  - [ ] SubTask 2.10: OpenthermGenerator — 1通道
  - [ ] SubTask 2.11: SENTGenerator — 1通道
  - [ ] SubTask 2.12: MIPIRFFEGenerator — 2通道(SCLK/SDATA)

- [ ] Task 3: 添加低优先级协议生成器到 protocol_synthesizer.py
  - [ ] SubTask 3.1: J1850VPWGenerator — 1通道
  - [ ] SubTask 3.2: SDCardGenerator — 2通道(CMD/DAT)
  - [ ] SubTask 3.3: SDIOGenerator — 2通道
  - [ ] SubTask 3.4: QSPIGenerator — 2通道
  - [ ] SubTask 3.5: SPIDualQuadGenerator — 3通道
  - [ ] SubTask 3.6: ST7735Generator — 4通道(SPI+DC)
  - [ ] SubTask 3.7: SpaceWireGenerator — 2通道
  - [ ] SubTask 3.8: FlexRayGenerator — 1通道
  - [ ] SubTask 3.9: MapleBusGenerator — 2通道
  - [ ] SubTask 3.10: IECGenerator — 3通道
  - [ ] SubTask 3.11: StepperMotorGenerator — 2通道
  - [ ] SubTask 3.12: XY2_100Generator — 3通道

- [ ] Task 4: 添加特殊协议生成器
  - [ ] SubTask 4.1: Z80Generator — 11通道(地址+数据+控制)
  - [ ] SubTask 4.2: MCS48Generator — 14通道
  - [ ] SubTask 4.3: GPIBGenerator — 16通道
  - [ ] SubTask 4.4: LPCGenerator — 6通道
  - [ ] SubTask 4.5: 其余1通道简单解码器(am230x, bean, dcf77, dsi, lfast, morse, mvb, swi, swim, t55xx, sdq, qi, pxx1, pjdl, rc_encode, rgb_led_ws281x, rinnai_control_panel, carrera, delta_sigma, em4100, em4305, eth_an, fsi, iebus, ieee488, miller, one_single_wire, ook, sony_md, tdm_audio, tlc5620, usb_power_delivery, caliper, c2, avr_pdi, opentherm)

- [ ] Task 5: 修改 test_factory.py 使用新生成器
  - [ ] SubTask 5.1: 扩展INPUT_GENERATORS映射，添加所有新协议类型
  - [ ] SubTask 5.2: 为每个解码器在generate_test_for_decoder中添加协议数据生成逻辑
  - [ ] SubTask 5.3: 完善stack解码器的依赖链配置（jtag→jtag_avr等19个）
  - [ ] SubTask 5.4: 处理0通道解码器（graycode_c, numbers_and_state_c, parallel_c）

- [ ] Task 6: 重新生成测试数据并验证
  - [ ] SubTask 6.1: 运行python test_factory.py重新生成所有input.bin和config.json
  - [ ] SubTask 6.2: 运行python run_all_tests.py --all --jobs 4验证
  - [ ] SubTask 6.3: 确认WARN数量从134降到0（或接近0）

# Task Dependencies
- Task 5 depends on Task 1, 2, 3, 4
- Task 6 depends on Task 5
- Task 1, 2, 3, 4 可以并行执行
