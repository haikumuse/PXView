# 修复C解码器自动化测试发现的所有问题 Spec

## Why
自动化测试发现112个逻辑输入C解码器中，20个FAIL（C与Python输出不一致）、31个ERROR（Python DLL加载失败、C解码器超时/崩溃）。需要修复这些问题，使C解码器与Python参考实现对齐。

## What Changes
- 修复 Python DLL 加载环境问题（影响18个解码器无法测试）
- 修复5个C解码器超时问题（dali_c, maple_bus_c, ook_c, usb_signalling_c, wiegand_c）
- 修复1个C解码器崩溃问题（hdlc_c）
- 修复20个C解码器逻辑问题（FAIL解码器）
- 重新运行全部测试验证修复效果

## Impact
- Affected code: `libsigrokdecode/c_decoders/` 中约26个C解码器源文件
- Affected code: `libsigrokdecode/tests/run_all_tests.py`（Python DLL路径修复）
- Affected specs: `align-all-c-decoders-with-python`（补充自动化测试发现的遗漏问题）

## ADDED Requirements

### Requirement: 修复Python DLL加载环境问题
系统 SHALL 在测试脚本中正确设置Python DLL搜索路径，使`srd_decoder_load_all()`能成功加载所有Python解码器。

#### Scenario: Python DLL加载成功
- **WHEN** 运行 `run_all_tests.py --all`
- **THEN** 18个之前因binascii DLL加载失败而ERROR的解码器能成功运行Python参考输出

### Requirement: 修复C解码器超时问题
5个C解码器（dali_c, maple_bus_c, ook_c, usb_signalling_c, wiegand_c）在处理不匹配的输入数据时进入无限等待。系统 SHALL 为这些解码器添加基于数据总长度的退出条件。

#### Scenario: 输入数据不匹配时正常退出
- **WHEN** C解码器处理完所有输入数据但未找到目标信号模式
- **THEN** 解码器正常返回而非无限等待

### Requirement: 修复C解码器崩溃问题
hdlc_c解码器在start()中调用`c_decoder_put_python()`时传入NULL数据指针导致崩溃。系统 SHALL 修复此空指针问题。

#### Scenario: hdlc_c正常启动
- **WHEN** 运行hdlc_c解码器
- **THEN** 不崩溃，正常输出注解

### Requirement: 修复C解码器输出0注解的严重Bug
3个C解码器（ieee488_c, miller_c, swi_c）在Python产生注解时C产生0注解。系统 SHALL 修复这些解码器的核心逻辑。

#### Scenario: ieee488_c仅DATA通道时输出位注解
- **WHEN** ieee488_c仅提供DATA通道（无CLK/DAV）
- **THEN** 输出位级注解（与Python一致）

#### Scenario: miller_c超时后继续解码
- **WHEN** miller_c检测到idle符号超时
- **THEN** flush当前bitstring后继续寻找下一条消息，而非退出

#### Scenario: swi_c无效数据时输出错误注解
- **WHEN** swi_c检测到无效波特间隔或不完整字
- **THEN** 输出错误注解而非静默跳过

### Requirement: 修复约2倍注解计数差异
3个C解码器（nrzi_c, opentherm_c, sent_c）的注解数量约为Python的一半。系统 SHALL 修复位范围计算和注解输出逻辑。

#### Scenario: nrzi_c位注解范围正确
- **WHEN** nrzi_c输出位注解
- **THEN** 每个位注解覆盖1个symbol_len（而非2个）

#### Scenario: opentherm_c输出sync error注解
- **WHEN** opentherm_c在IDLE状态检测到静默间隔不足
- **THEN** 输出"Sync error: silence too short"注解

#### Scenario: sent_c每次迭代消耗1个下降沿
- **WHEN** sent_c处理下降沿
- **THEN** 每1个下降沿输出1个tick注解（而非每2个）

### Requirement: 修复重大注解计数差异
7个C解码器（can_c, microwire_c, morse_c, swim_c, timing_c, ps2_c, z80_c）存在显著注解计数差异。系统 SHALL 修复注解输出逻辑。

#### Scenario: can_c为每个位输出ANN_BIT注解
- **WHEN** can_c解码CAN帧
- **THEN** 为帧中每个位输出位注解

#### Scenario: morse_c不在process_symbol中输出SYMBOL注解
- **WHEN** morse_c处理符号
- **THEN** process_symbol只输出TIME和UNITS，SYMBOL由更高层处理

#### Scenario: timing_c在format=full时不输出TERSE注解
- **WHEN** timing_c的format选项为full
- **THEN** 只输出TIME注解，不额外输出TERSE注解

### Requirement: 修复文本格式差异
4个C解码器（caliper_c, dcc_c, pwm_c, seven_segment_c）的注解文本格式与Python不一致。系统 SHALL 对齐文本格式。

#### Scenario: caliper_c始终包含小数点
- **WHEN** caliper_c输出测量值
- **THEN** 使用"0.0mm"格式（非"0mm"）

#### Scenario: pwm_c数值和单位之间有空格
- **WHEN** pwm_c输出时间值
- **THEN** 使用"2.0 μs"格式（非"2.0μs"）

#### Scenario: seven_segment_c包含小数点标记
- **WHEN** seven_segment_c的DP通道激活
- **THEN** 在数字字符后追加"."（如"B."而非"B"）

#### Scenario: dcc_c包含时序类型后缀
- **WHEN** dcc_c输出时序注解
- **THEN** 包含类型后缀如"(sync)"

### Requirement: 修复其他差异
6个C解码器（graycode_c, i2c_c, numbers_and_state_c, spi_c, jitter_c, onewire_link_c, pjdl_c）存在起始偏移、缺少文本变体、过多错误注解等问题。

#### Scenario: spi_c位采样起始位置对齐
- **WHEN** spi_c解码SPI数据
- **THEN** 位采样起始位置与Python一致

#### Scenario: jitter_c/onewire_link_c输出多个文本变体
- **WHEN** jitter_c/onewire_link_c输出注解
- **THEN** 包含与Python一致的多个文本变体

#### Scenario: pjdl_c不在每个无效边沿输出错误
- **WHEN** pjdl_c检测到无效脉冲
- **THEN** 不在每个无效边沿都输出错误注解

## MODIFIED Requirements

无修改的需求。

## REMOVED Requirements

无移除的需求。
