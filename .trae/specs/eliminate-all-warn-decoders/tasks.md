# Tasks

## Phase 1: 基础设施准备
- [ ] Task 1: 确认 `generate_testdata.py` 动态分发机制正常工作
  - [ ] 验证 `synthesize_input_bin()` 的 P1 动态反射调用路径正确
  - [ ] 确认所有空壳 Fuzzer 的 `generate_testdata()` 方法不会产生有效波形（确保 fallback 到随机噪声）

## Phase 2: Batch 5 — 修复第一批 5 个 WARN 解码器
- [ ] Task 2: 修复 `can_fd_c`
  - [ ] 逆向 `decoders/can_fd/pd.py` 状态机：分析 CAN FD 帧格式（BRS 位、ESI 位、DLC、CRC）
  - [ ] 在 `fuzzers/can_fd.py` 中实现 `CanFdGenerator.generate_testdata()`：基于 CANGenerator 扩展，发送 FD 格式帧
  - [ ] 注册到 `fuzzers/__init__.py`
  - [ ] 运行 `python generate_testdata.py --overwrite && python run_all_tests.py --decoder can_fd_c` 验证

- [ ] Task 3: 修复 `ethernet_c`
  - [ ] 逆向 `decoders/ethernet/pd.py` 状态机：分析以太网帧前导码+SFD+MAC 头
  - [ ] 在 `fuzzers/ethernet.py` 中实现 `EthernetGenerator.generate_testdata()`：生成 MII/RMII 信号或 NRZI→4b5b 编码的以太网帧
  - [ ] 注册到 `fuzzers/__init__.py`
  - [ ] 运行验证

- [ ] Task 4: 修复 `ir_nec_c`
  - [ ] 逆向 `decoders/ir_nec/pd.py` 状态机：NEC 引导码(9ms+4.5ms) + 32 位数据
  - [ ] 在 `fuzzers/ir_nec.py` 中实现 `IrNecGenerator.generate_testdata()`：生成完整 NEC 帧
  - [ ] 注册到 `fuzzers/__init__.py`
  - [ ] 运行验证

- [ ] Task 5: 修复 `mipi_rffe_c`
  - [ ] 逆向 `decoders/mipi_rffe/pd.py` 状态机：SSC 起始条件 + 命令帧
  - [ ] 在 `fuzzers/mipi_rffe.py` 中实现 `MipiRffeGenerator.generate_testdata()`：生成 SSC 起始 + Register Write 帧
  - [ ] 注册到 `fuzzers/__init__.py`
  - [ ] 运行验证

- [ ] Task 6: 修复 `usb_signalling_c`
  - [ ] 逆向 `decoders/usb_signalling/pd.py` 状态机：USB SYNC+PID+数据+EOP
  - [ ] 在 `fuzzers/usb_signalling.py` 中实现 `UsbSignallingGenerator.generate_testdata()`：生成 USB NRZI 编码的信令帧
  - [ ] 注册到 `fuzzers/__init__.py`
  - [ ] 运行验证

## Phase 3: Batch 6 — 修复第二批 5 个 WARN 解码器
- [ ] Task 7: 修复 `cjtag_c`
  - [ ] 逆向 `decoders/cjtag/pd.py` 状态机
  - [ ] 实现 `CjtagGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 8: 修复 `ds2408_c`
  - [ ] 逆向 `decoders/ds2408/pd.py` 状态机：1-Wire ROM 命令序列
  - [ ] 实现 `Ds2408Generator.generate_testdata()`：基于 OneWireGenerator 扩展
  - [ ] 注册并验证

- [ ] Task 9: 修复 `ds243x_c`
  - [ ] 逆向 `decoders/ds243x/pd.py` 状态机
  - [ ] 实现 `Ds243xGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 10: 修复 `ds28ea00_c`
  - [ ] 逆向 `decoders/ds28ea00/pd.py` 状态机
  - [ ] 实现 `Ds28ea00Generator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 11: 修复 `flexray_c`
  - [ ] 逆向 `decoders/flexray/pd.py` 状态机
  - [ ] 实现 `FlexrayGenerator.generate_testdata()`
  - [ ] 注册并验证

## Phase 4: Batch 7 — 修复第三批 5 个 WARN 解码器
- [ ] Task 12: 修复 `fsi_c`
  - [ ] 逆向 `decoders/fsi/pd.py` 状态机：BREAK 检测 + ABS_ADR 命令
  - [ ] 实现 `FSiGenerator.generate_testdata()`：256 个时钟周期的 BREAK + 完整命令帧
  - [ ] 注册并验证

- [ ] Task 13: 修复 `ieee488_c`
  - [ ] 逆向 `decoders/ieee488/pd.py` 状态机：GPIB 握手协议
  - [ ] 实现 `Ieee488Generator.generate_testdata()`：DAV+NRFD+NDAC 三线握手
  - [ ] 注册并验证

- [ ] Task 14: 修复 `maple_bus_c`
  - [ ] 逆向 `decoders/maple_bus/pd.py` 状态机
  - [ ] 实现 `MapleBusGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 15: 修复 `mvb_c`
  - [ ] 逆向 `decoders/mvb/pd.py` 状态机：Manchester II 编码
  - [ ] 实现 `MvbGenerator.generate_testdata()`：正确 CRC 计算
  - [ ] 注册并验证

- [ ] Task 16: 修复 `sae_j1850_vpw_c`
  - [ ] 逆向 `decoders/sae_j1850_vpw/pd.py` 状态机：VPW 编码
  - [ ] 实现 `SaeJ1850VpwGenerator.generate_testdata()`
  - [ ] 注册并验证

## Phase 5: Batch 8 — 修复第四批 5 个 WARN 解码器
- [ ] Task 17: 修复 `sdcard_sd_c`
  - [ ] 逆向 `decoders/sdcard_sd/pd.py` 状态机
  - [ ] 实现 `SdcardSdGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 18: 修复 `emmc_sd_c`
  - [ ] 逆向 `decoders/emmc_sd/pd.py` 状态机
  - [ ] 实现 `EmmcSdGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 19: 修复 `spacewire_c`
  - [ ] 逆向 `decoders/spacewire/pd.py` 状态机
  - [ ] 实现 `SpacewireGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 20: 修复 `qspi_c`
  - [ ] 逆向 `decoders/qspi/pd.py` 状态机
  - [ ] 实现 `QspiGenerator.generate_testdata()`：基于 SPIGenerator 扩展
  - [ ] 注册并验证

- [ ] Task 21: 修复 `spi_dual_quad_c`
  - [ ] 逆向 `decoders/spi_dual_quad/pd.py` 状态机
  - [ ] 实现 `SpiDualQuadGenerator.generate_testdata()`
  - [ ] 注册并验证

## Phase 6: Batch 9 — 修复第五批 5 个 WARN 解码器
- [ ] Task 22: 修复 `ir_rc5_c`
  - [ ] 逆向 `decoders/ir_rc5/pd.py` 状态机
  - [ ] 实现 `IrRc5Generator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 23: 修复 `ir_sirc_c`
  - [ ] 逆向 `decoders/ir_sirc/pd.py` 状态机
  - [ ] 实现 `IrSircGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 24: 修复 `ir_irmp_c`
  - [ ] 逆向 `decoders/ir_irmp/pd.py` 状态机
  - [ ] 实现 `IrIrmpGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 25: 修复 `ir_ltto_c`
  - [ ] 逆向 `decoders/ir_ltto/pd.py` 状态机
  - [ ] 实现 `IrLttoGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 26: 修复 `ir_recoil_c`
  - [ ] 逆向 `decoders/ir_recoil/pd.py` 状态机
  - [ ] 实现 `IrRecoilGenerator.generate_testdata()`
  - [ ] 注册并验证

## Phase 7: Batch 10 — 修复第六批 5 个 WARN 解码器
- [ ] Task 27: 修复 `dsi_c`
  - [ ] 逆向 `decoders/dsi/pd.py` 状态机
  - [ ] 实现 `DsiGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 28: 修复 `mipi_dsi_c`
  - [ ] 逆向 `decoders/mipi_dsi/pd.py` 状态机
  - [ ] 实现 `MipiDsiGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 29: 修复 `pcfx_ctrlr_c`
  - [ ] 逆向 `decoders/pcfx_ctrlr/pd.py` 状态机
  - [ ] 实现 `PcfxCtrlrGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 30: 修复 `rinnai_control_panel_c`
  - [ ] 逆向 `decoders/rinnai_control_panel/pd.py` 状态机
  - [ ] 实现 `RinnaiControlPanelGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 31: 修复 `xy2_100_c`
  - [ ] 逆向 `decoders/xy2_100/pd.py` 状态机
  - [ ] 实现 `Xy2100Generator.generate_testdata()`
  - [ ] 注册并验证

## Phase 8: Batch 11 — 修复第七批 5 个 WARN 解码器
- [ ] Task 32: 修复 `arp_c`
  - [ ] 逆向 `decoders/arp/pd.py` 状态机（上层协议，依赖 ethernet_c）
  - [ ] 实现 `ArpGenerator.generate_testdata()`：确保底层 ethernet 先解码
  - [ ] 注册并验证

- [ ] Task 33: 修复 `ipv4_c`
  - [ ] 逆向 `decoders/ipv4/pd.py` 状态机（上层协议，依赖 ethernet_c）
  - [ ] 实现 `Ipv4Generator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 34: 修复 `udp_c`
  - [ ] 逆向 `decoders/udp/pd.py` 状态机（上层协议，依赖 ipv4_c）
  - [ ] 实现 `UdpGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 35: 修复 `usb_request_c`
  - [ ] 逆向 `decoders/usb_request/pd.py` 状态机（上层协议，依赖 usb_packet_c）
  - [ ] 实现 `UsbRequestGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 36: 修复 `tpm_fifo_tis_c`
  - [ ] 逆向 `decoders/tpm_fifo_tis/pd.py` 状态机
  - [ ] 实现 `TpmFifoTisGenerator.generate_testdata()`
  - [ ] 注册并验证

## Phase 9: Batch 12 — 修复第八批 5 个 WARN 解码器
- [ ] Task 37: 修复 `cjtag_oscan0_c`
  - [ ] 逆向 `decoders/cjtag_oscan0/pd.py` 状态机
  - [ ] 实现 `CjtagOscan0Generator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 38: 修复 `guess_bitrate_c`
  - [ ] 逆向 `decoders/guess_bitrate/pd.py` 状态机
  - [ ] 实现 `GuessBitrateGenerator.generate_testdata()`：生成规律性电平变化
  - [ ] 注册并验证

- [ ] Task 39: 修复 `iec_c`
  - [ ] 逆向 `decoders/iec/pd.py` 状态机
  - [ ] 实现 `IecGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 40: 修复 `jitter_c`
  - [ ] 逆向 `decoders/jitter/pd.py` 状态机
  - [ ] 实现 `JitterGenerator.generate_testdata()`：生成带抖动的时钟
  - [ ] 注册并验证

- [ ] Task 41: 修复 `numbers_and_state_c`
  - [ ] 逆向 `decoders/numbers_and_state/pd.py` 状态机
  - [ ] 实现 `NumbersAndStateGenerator.generate_testdata()`
  - [ ] 注册并验证

## Phase 10: Batch 13 — 修复第九批 5 个 WARN 解码器
- [ ] Task 42: 修复 `rpm_c`
  - [ ] 逆向 `decoders/rpm/pd.py` 状态机
  - [ ] 实现 `RpmGenerator.generate_testdata()`：生成周期性脉冲
  - [ ] 注册并验证

- [ ] Task 43: 修复 `sda2506_c`
  - [ ] 逆向 `decoders/sda2506/pd.py` 状态机
  - [ ] 实现 `Sda2506Generator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 44: 修复 `seven_segment_c`
  - [ ] 逆向 `decoders/seven_segment/pd.py` 状态机
  - [ ] 实现 `SevenSegmentGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 45: 修复 `signature_c`
  - [ ] 逆向 `decoders/signature/pd.py` 状态机
  - [ ] 实现 `SignatureGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 46: 修复 `sle44xx_c`
  - [ ] 逆向 `decoders/sle44xx/pd.py` 状态机
  - [ ] 实现 `Sle44xxGenerator.generate_testdata()`
  - [ ] 注册并验证

## Phase 11: Batch 14 — 修复第十批 5 个 WARN 解码器
- [ ] Task 47: 修复 `st7735_c`
  - [ ] 逆向 `decoders/st7735/pd.py` 状态机
  - [ ] 实现 `St7735Generator.generate_testdata()`：手动生成 CS+CLK+MOSI+DC 信号
  - [ ] 注册并验证

- [ ] Task 48: 修复 `st7789_c`
  - [ ] 逆向 `decoders/st7789/pd.py` 状态机
  - [ ] 实现 `St7789Generator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 49: 修复 `stepper_motor_c`
  - [ ] 逆向 `decoders/stepper_motor/pd.py` 状态机
  - [ ] 实现 `StepperMotorGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 50: 修复 `tm1637_c`
  - [ ] 逆向 `decoders/tm1637/pd.py` 状态机
  - [ ] 实现 `Tm1637Generator.generate_testdata()`：I2C-like CLK/DIO 协议
  - [ ] 注册并验证

- [ ] Task 51: 修复 `tmc_c`
  - [ ] 逆向 `decoders/tmc/pd.py` 状态机
  - [ ] 实现 `TmcGenerator.generate_testdata()`：CLK+DIO+STB 协议
  - [ ] 注册并验证

## Phase 12: Batch 15 — 修复最后 3 个 WARN 解码器
- [ ] Task 52: 修复 `aud_c`
  - [ ] 逆向 `decoders/aud/pd.py` 状态机
  - [ ] 实现 `AudGenerator.generate_testdata()`
  - [ ] 注册并验证

- [ ] Task 53: 修复 `carrera_c`
  - [ ] 检查现有 Fuzzer 为何仍 WARN，修复波形生成逻辑
  - [ ] 注册并验证

- [ ] Task 54: 修复 `tm1638_c`
  - [ ] 逆向 `decoders/tm1638/pd.py` 状态机
  - [ ] 实现 `Tm1638Generator.generate_testdata()`
  - [ ] 注册并验证

## Phase 13: 全量验证
- [ ] Task 55: 运行全量测试确认所有 WARN 已消除
  - [ ] 运行 `python run_all_tests.py --all`
  - [ ] 确认 WARN 数量为 0
  - [ ] 确认之前 PASS 的解码器无回归

# Task Dependencies
- Task 1 必须最先完成（基础设施）
- Phase 2-12 中的各 Batch 可按顺序执行，每个 Batch 内的 5 个 Task 可并行
- Phase 6-8 中的上层协议（arp_c, ipv4_c, udp_c, usb_request_c）依赖对应底层协议先修复
- Task 55 依赖所有前置任务完成
