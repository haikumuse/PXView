# 修复所有C解码器测试至全PASS Spec

## Why
当前215个C解码器测试中有11个FAIL和79个WARN（空真），需要全部修复至PASS，确保C解码器与Python解码器输出完全一致。

## What Changes
- 修复11个FAIL解码器的C解码器Bug或测试数据生成器
- 修复79个WARN解码器的测试数据生成器，使其产生有效协议数据
- 修复测试比较逻辑中的已知不一致（@-prefix处理等）

## Impact
- Affected code: `libsigrokdecode/c_decoders/*.c` (C解码器Bug修复)
- Affected code: `libsigrokdecode/tests/protocol_synthesizer.py` (协议生成器)
- Affected code: `libsigrokdecode/tests/test_factory.py` (测试数据配置)
- Affected code: `libsigrokdecode/tests/decoder_test.c` (测试框架)
- Affected code: `libsigrokdecode/decoders/*/pd.py` (Python解码器Bug)

## 当前状态

### 11个FAIL解码器
| 解码器 | 原因 | 修复方向 |
|--------|------|----------|
| ccd_c | 测试数据不符合CCD协议 | 改进CCD生成器 |
| jtag_c | 测试数据不符合JTAG TAP状态机 | 改进JTAG生成器 |
| lfast_c | NRZ编码/C解码器位赋值差异 | 修复lfast_c.c位赋值+生成器 |
| maple_bus_c | 测试数据不符合Maple Bus协议 | 改进Maple Bus生成器 |
| ps2_c | 测试数据不符合PS/2帧格式 | 改进PS/2生成器 |
| ps2_keyboard_c | 依赖ps2_c | 随ps2_c修复 |
| ps2_mouse_c | 依赖ps2_c | 随ps2_c修复 |
| qi_c | 测试数据不符合Qi协议 | 改进Qi生成器 |
| rvswd_c | 测试数据不符合RVSWD协议 | 改进RVSWD生成器 |
| sdio_c | 测试数据不符合SDIO协议 | 改进SDIO生成器 |
| sipi_c | 依赖lfast_c | 随lfast_c修复 |
| usb_power_delivery_c | 测试数据不符合USB PD协议 | 改进USB PD生成器 |

### 79个WARN解码器
这些解码器的Python和C都产生0个注解，测试数据无效。需要为每个生成有效的协议信号数据。

## ADDED Requirements

### Requirement: 所有C解码器测试必须PASS
系统 SHALL 确保所有215个C解码器测试结果为PASS（0 FAIL, 0 WARN）。

#### Scenario: 全量测试通过
- **WHEN** 运行 `python run_all_tests.py --all --jobs 4`
- **THEN** 输出 215 PASS, 0 FAIL, 0 WARN, 0 ERROR

### Requirement: FAIL解码器修复
每个FAIL解码器 SHALL 通过以下方式之一修复：
1. 修复C解码器Bug（当C解码器逻辑与Python不一致时）
2. 修复测试数据生成器（当测试数据不符合协议规范时）
3. 修复Python解码器Bug（当Python解码器有已知Bug时）

### Requirement: WARN解码器修复
每个WARN解码器 SHALL 通过改进测试数据生成器使其产生有效协议数据，让Python和C解码器都能输出注解。

## MODIFIED Requirements
无

## REMOVED Requirements
无
