# 修复C解码器测试问题 — 验证清单

## 测试基础设施

- [ ] Python DLL加载环境问题已修复，18个之前ERROR的解码器能运行Python参考输出
- [ ] hdlc_c不再崩溃，能正常输出注解

## C解码器超时修复

- [ ] dali_c不再超时
- [ ] maple_bus_c不再超时
- [ ] ook_c不再超时
- [ ] usb_signalling_c不再超时
- [ ] wiegand_c不再超时

## C解码器输出0注解修复

- [ ] ieee488_c在仅DATA通道时输出位注解
- [ ] miller_c超时后继续解码，不再输出0注解
- [ ] swi_c无效数据时输出错误注解

## 约2倍注解计数差异修复

- [ ] nrzi_c位注解范围正确（每个位覆盖1个symbol_len）
- [ ] opentherm_c输出sync error注解
- [ ] sent_c每次迭代消耗1个下降沿

## 重大注解计数差异修复

- [ ] can_c为每个位输出ANN_BIT注解
- [ ] microwire_c边沿检测时机正确
- [ ] morse_c不在process_symbol中输出SYMBOL注解
- [ ] timing_c在format=full时不输出TERSE注解
- [ ] swim_c位检测逻辑与Python一致
- [ ] ps2_c位检测起始位置与Python一致
- [ ] z80_c end_sample计算正确，包含初始状态警告

## 文本格式差异修复

- [ ] caliper_c测量值始终包含小数点
- [ ] dcc_c时序注解包含类型后缀
- [ ] pwm_c数值和单位之间有空格
- [ ] seven_segment_c包含小数点标记

## 其他差异修复

- [ ] graycode_c起始采样位置正确
- [ ] numbers_and_state_c起始采样位置正确
- [ ] i2c_c包含ATK颜色注解
- [ ] spi_c位采样起始位置与Python一致
- [ ] jitter_c输出多个文本变体
- [ ] onewire_link_c输出多个文本变体
- [ ] pjdl_c不在每个无效边沿输出错误

## 编译和回归测试

- [ ] 所有修改的C解码器编译通过，无错误和警告
- [ ] 重新运行全部测试，FAIL数量显著减少
