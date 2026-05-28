# 逆向 Python 状态机生成波形数据修复全部 WARN 解码器 Spec

## Why
当前 215 个 C 解码器测试中，54 个报 WARN（vacuous match），即 Python 和 C 解码器都输出 0 条 annotation。根本原因是这些解码器的测试波形数据为纯随机噪声，无法触发协议状态机的第一步跳转。需要通过逆向分析每个 Python 解码器（`pd.py`）的状态机逻辑，编写专属的 Fuzzer 生成合法波形数据，使状态机完整运行并产生有效输出。

## What Changes
- 为 54 个 WARN 解码器编写/完善 `fuzzers/<decoder>.py` 中的 `generate_testdata()` 方法
- 在 `fuzzers/__init__.py` 中注册新增的 Generator 类
- 在 `generate_testdata.py` 的 `overrides` 字典中添加需要特殊采样率/样本数的解码器配置
- 对不可避免的浮点数/文本偏差，在 `testdata/<decoder>/default/config.json` 中配置 `"expected_deviations": true`
- 不修改 C 解码器源代码
- 不修改 Python 解码器源代码

## Impact
- 修改 `libsigrokdecode/tests/fuzzers/` 下约 54 个 fuzzer 文件
- 修改 `libsigrokdecode/tests/fuzzers/__init__.py` 注册表
- 修改 `libsigrokdecode/tests/generate_testdata.py` 的 overrides 和 elif 分支
- 修改 `libsigrokdecode/tests/testdata/` 下对应 config.json

## WARN 修复方法论

### 步骤 1：逆向分析 Python 状态机
1. 查阅 `decoders/<decoder_name>/pd.py` 的 `__init__` 初始状态
2. 分析 `decode()` 中 `wait()` 条件定义
3. 提炼触发第一个完整帧必须具备的条件（起始字节、前导码、电平持续时间等）
4. 识别状态机的所有分支路径

### 步骤 2：编写专属 Fuzzer 生成器
1. 在 `fuzzers/<decoder>.py` 中实现 `class XxxGenerator(ProtocolFuzzer)` 的 `generate_testdata(self)` 方法
2. 使用 `self.builder.set_level(channel, level, duration_samples)` 构建波形
3. 精确计算采样率换算：`samples_per_bit = int(time_sec * self.samplerate)`

### 步骤 3：全帧序列注入
1. 构建完整有效包：前导码 → 起始位 → 数据位 → 校验和 → 结束位
2. 确保最后给出足够长的 Idle duration，使状态机能跨越帧周期超时阈值并输出最终结果

### 步骤 4：注册与验证
1. 在 `fuzzers/__init__.py` 中注册模块
2. 运行 `python generate_testdata.py --overwrite` 覆写旧数据
3. 运行 `python run_all_tests.py --decoder <id>` 验证
4. 如有浮点数偏差，在 `config.json` 中配置 `"expected_deviations": true`

## 54 个 WARN 解码器分类与逆向分析

### A 类：底层逻辑协议（需要直接生成电平波形）— 38 个

这些解码器直接读取逻辑分析仪的高低电平信号，需要精确的时序波形：

| # | 解码器 | 传输层 | 协议特征 | 逆向关键点 |
|---|--------|--------|----------|-----------|
| 1 | arp_c | NRZI→4b5b→Ethernet 上层 | 以太网 ARP 包 | 需先触发 ethernet_c 底层 |
| 2 | aud_c | 自定义串行 | AUD 音频协议 | 查 pd.py 的起始条件 |
| 3 | can_fd_c | CAN FD 总线 | CAN FD 帧（BRS/ESI 位） | 需发送 FD 格式帧 |
| 4 | carrera_c | OOK 无线 | Carrera 赛道控制 | 已有 Fuzzer 但仍 WARN，需检查 |
| 5 | cjtag_c | JTAG 衍生 | cJTAG 协议 | 需 TCK/TMS 信号 |
| 6 | cjtag_oscan0_c | cJTAG OSCAN0 | cJTAG OSCAN0 模式 | 需特定 TMS 序列 |
| 7 | ds2408_c | 1-Wire 上层 | DS2408 8位地址开关 | 需 1-Wire ROM 命令序列 |
| 8 | ds243x_c | 1-Wire 上层 | DS243x 电池监控 | 需 1-Wire ROM 命令序列 |
| 9 | ds28ea00_c | 1-Wire 上层 | DS28EA00 温度传感器 | 需 1-Wire ROM 命令序列 |
| 10 | dsi_c | DSI 串行 | Display Serial Interface | 需 DSI 帧结构 |
| 11 | emmc_sd_c | eMMC/SD 总线 | eMMC SD 模式 | 需 CMD+DAT 信号 |
| 12 | ethernet_c | MII/RMII/NRZI→4b5b | 以太网 MAC 帧 | 需前导码+SFD+帧数据 |
| 13 | flexray_c | FlexRay 总线 | FlexRay 帧 | 需 TSS+帧头+CRC |
| 14 | fsi_c | FSI 串行 | IBM FSI 协议 | 需 BREAK+命令帧 |
| 15 | guess_bitrate_c | 逻辑分析 | 波特率猜测 | 需规律性电平变化 |
| 16 | iec_c | IEC 串行 | IEC 协议 | 查 pd.py 起始条件 |
| 17 | ieee488_c | GPIB 并行 | IEEE-488/GPIB | 需 DAV+NRFD+NDAC 握手 |
| 18 | ipv4_c | Ethernet 上层 | IPv4 包 | 需先触发 ethernet_c |
| 19 | ir_irmp_c | IR 红外 | IRMP 多协议红外 | 需特定红外编码 |
| 20 | ir_ltto_c | IR 红外 | LTTO 红外标签 | 需 LTTO 编码波形 |
| 21 | ir_nec_c | IR 红外 | NEC 红外协议 | 需 NEC 引导码+数据 |
| 22 | ir_rc5_c | IR 红外 | RC5 红外协议 | 需 RC5 Manchester 编码 |
| 23 | ir_recoil_c | IR 红外 | Recoil 红外 | 需 Recoil 编码 |
| 24 | ir_sirc_c | IR 红外 | SIRC 红外协议 | 需 SIRC 编码 |
| 25 | jitter_c | 逻辑分析 | 抖动测量 | 需带抖动的时钟信号 |
| 26 | maple_bus_c | Maple Bus | Sega Maple 总线 | 需 Maple 帧结构 |
| 27 | mipi_dsi_c | MIPI DSI | MIPI 显示串行接口 | 需 DSI LP/HS 模式切换 |
| 28 | mipi_rffe_c | MIPI RFFE | MIPI RF 前端接口 | 需 SSC 起始条件 |
| 29 | mvb_c | MVB 总线 | Multifunction Vehicle Bus | 需 Manchester 编码帧 |
| 30 | numbers_and_state_c | 逻辑分析 | 数值/状态显示 | 需任意有效电平 |
| 31 | pcfx_ctrlr_c | PC-FX 控制器 | PC-FX 手柄协议 | 需特定时序 |
| 32 | qspi_c | QSPI 总线 | Quad SPI | 需 SPI 基础信号 |
| 33 | rinnai_control_panel_c | Rinnai 面板 | Rinnai 控制面板 | 需自定义串行协议 |
| 34 | rpm_c | 逻辑分析 | RPM 测量 | 需周期性脉冲 |
| 35 | sae_j1850_vpw_c | SAE J1850 VPW | 汽车诊断协议 | 需 VPW 编码帧 |
| 36 | sda2506_c | SDA2506 | SDA2506 EEPROM | 需 I2C-like 通信 |
| 37 | sdcard_sd_c | SD 卡 SD 模式 | SD 卡协议 | 需 CMD+DAT 信号 |
| 38 | seven_segment_c | 逻辑分析 | 七段数码管 | 需段码信号 |

### B 类：上层协议解码器（依赖底层解码器输出）— 16 个

这些解码器不直接读取电平信号，而是接收底层解码器的协议输出。需要确保底层解码器先正确解码：

| # | 解码器 | 依赖底层 | 协议特征 |
|---|--------|----------|----------|
| 39 | signature_c | 任意解码器 | 签名/统计 |
| 40 | sle44xx_c | ISO7816 | SLE44xx 智能卡 |
| 41 | spacewire_c | SpaceWire | SpaceWire 链路 |
| 42 | spi_dual_quad_c | SPI | 双/四线 SPI |
| 43 | st7735_c | SPI | ST7735 LCD 驱动 |
| 44 | st7789_c | SPI | ST7789 LCD 驱动 |
| 45 | stepper_motor_c | 逻辑分析 | 步进电机控制 |
| 46 | tm1637_c | I2C-like | TM1637 LED 驱动 |
| 47 | tm1638_c | TMC | TM1638 LED 驱动 |
| 48 | tmc_c | SPI-like | TMC 步进电机驱动 |
| 49 | tpm_fifo_tis_c | LPC/SP | TPM FIFO TIS 接口 |
| 50 | udp_c | Ethernet→IPv4 | UDP 数据包 |
| 51 | usb_request_c | USB Packet | USB 请求解码 |
| 52 | usb_signalling_c | USB 信号 | USB 信令层 |
| 53 | usb_request_c | USB Packet | USB 请求 |
| 54 | xy2_100_c | XY2-100 | XY2-100 光栅尺协议 |

## ADDED Requirements

### Requirement: 逆向 Python 状态机生成合法波形数据
系统 SHALL 为每个 WARN 解码器提供基于 Python 解码器状态机逆向的合法波形生成器。

#### Scenario: WARN 解码器获得合法波形后产生非零输出
- **WHEN** 为 WARN 解码器编写了基于 Python 状态机逆向的 Fuzzer
- **AND** 运行 `python generate_testdata.py --overwrite` 重新生成测试数据
- **THEN** 该解码器的 Python 和 C 版本都应产生大于 0 条 annotation
- **AND** 测试结果从 WARN 变为 PASS 或 DEVIATION

#### Scenario: 浮点数/文本偏差合法降级
- **WHEN** C 解码器与 Python 解码器存在不可避免的浮点数或文本格式差异
- **AND** 在 `config.json` 中配置了 `"expected_deviations": true`
- **THEN** 测试结果应标记为 DEVIATION 而非 FAIL

### Requirement: Fuzzer 必须覆盖 Python 状态机的主要路径
每个 Fuzzer 生成的波形 SHALL 至少触发 Python 解码器的以下状态路径：
1. 初始状态 → 第一个有效帧的完整解析
2. 数据接收和输出
3. 帧结束/超时后的状态重置

### Requirement: 分批执行与验证
所有 54 个 WARN 解码器 SHALL 按每批 5 个的节奏分批修复，每批完成后立即验证。

## MODIFIED Requirements
无

## REMOVED Requirements
无
