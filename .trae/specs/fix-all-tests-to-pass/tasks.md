# Tasks

- [ ] Task 1: 修复11个FAIL解码器
  - [ ] SubTask 1.1: 修复lfast_c — C解码器位赋值和sleep bit逻辑 + NRZ生成器改进
  - [ ] SubTask 1.2: 修复ccd_c — 改进CCD/UART测试数据生成器
  - [ ] SubTask 1.3: 修复jtag_c — 改进JTAG TAP状态机测试数据生成器
  - [ ] SubTask 1.4: 修复ps2_c/ps2_keyboard_c/ps2_mouse_c — 改进PS/2帧格式生成器
  - [ ] SubTask 1.5: 修复maple_bus_c — 改进Maple Bus协议生成器
  - [ ] SubTask 1.6: 修复qi_c — 改进Qi差分双相编码生成器
  - [ ] SubTask 1.7: 修复rvswd_c — 改进RVSWD协议生成器
  - [ ] SubTask 1.8: 修复sdio_c — 改进SDIO命令生成器
  - [ ] SubTask 1.9: 修复usb_power_delivery_c — 改进USB PD BMC编码生成器
  - [ ] SubTask 1.10: 修复sipi_c — 随lfast_c修复后验证

- [ ] Task 2: 修复79个WARN解码器（按批次）
  - [ ] SubTask 2.1: 分析79个WARN解码器，按协议复杂度分类
  - [ ] SubTask 2.2: 修复简单1通道协议（pwm_c, counter_c, graycode_c等）— 约20个
  - [ ] SubTask 2.3: 修复串行协议（uart_c衍生, ir系列, dcf77_c等）— 约15个
  - [ ] SubTask 2.4: 修复总线协议（i2c衍生, spi衍生, can衍生等）— 约15个
  - [ ] SubTask 2.5: 修复特殊/复杂协议（z80_c, mcs48_c, gpib_c, lpc_c等）— 约15个
  - [ ] SubTask 2.6: 修复stack解码器（依赖上游解码器的）— 约14个

- [ ] Task 3: 编译验证和全量测试
  - [ ] SubTask 3.1: ninja -C build 编译所有修改的C解码器
  - [ ] SubTask 3.2: python test_factory.py 重新生成测试数据
  - [ ] SubTask 3.3: python run_all_tests.py --all --jobs 4 验证215 PASS

# Task Dependencies
- Task 1 和 Task 2 可以并行执行
- SubTask 1.10 (sipi_c) 依赖 SubTask 1.1 (lfast_c)
- SubTask 2.6 (stack解码器) 依赖 SubTask 2.2-2.5 (上游解码器)
- Task 3 依赖 Task 1 和 Task 2
