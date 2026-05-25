# C解码器全面对齐Python版本 — 验证清单

## 严重问题验证

- [ ] can_c.c: annotation范围包含位宽扩展（left/right偏移）
- [ ] can_c.c: 协议输出格式与Python兼容（元组格式）
- [ ] can_c.c: 采样率无效时安全退出
- [ ] jtag_c.c: "NEW STATE"协议输出包含状态名称字符串
- [ ] jtag_c.c: "DR TDI"/"DR TDO"/"IR TDI"/"IR TDO"协议输出包含bitstring和per-bit ss/es
- [ ] jtag_c.c: 最后一位的bit annotation输出时机与Python一致
- [ ] swd_c.c: 请求奇偶校验检查已实现
- [ ] swd_c.c: 协议输出包含ACK值
- [ ] i2s_c.c: 二进制输出包含WAV文件头
- [ ] i2s_c.c: annotation行定义与Python一致（1行）
- [ ] i2s_c.c: 协议输出格式与Python兼容
- [ ] onewire_c.c: 完整时序检查已实现（RSTL/RSTH/PDH/PDL/SLOT/REC/LOWR）
- [ ] onewire_c.c: 采样率检查已实现
- [ ] onewire_c.c: 存在检测超时机制已实现
- [ ] onewire_c.c: "无存在检测"情况已处理
- [ ] nrzi_c.c: 协议输出格式与Python兼容（裸整数0/1）
- [ ] 4b5b_c.c: inputs=['nrzi']，从NRZI解码器接收bit数据
- [ ] 4b5b_c.c: 内嵌NRZI解码逻辑已移除
- [ ] 4b5b_c.c: 协议输出格式与Python兼容（value, is_control_symbol）
- [ ] lin_c.c: inputs=['uart']，从UART解码器接收字节
- [ ] lin_c.c: 内嵌read_uart_byte()已移除
- [ ] lin_c.c: 帧中断处理已实现
- [ ] graycode_c.c: bits选项支持任意位数
- [ ] numbers_and_state_c.c: enum映射功能已实现
- [ ] iso7816_c.c: PCAP二进制输出已实现

## 中等问题验证

- [ ] hdlc_c.c: CS-CHANGE输出已添加
- [ ] hdlc_c.c: 协议输出包含per-byte ss/es信息
- [ ] microwire_c.c: 协议输出格式与Python兼容
- [ ] mdio_c.c: 协议输出格式与Python兼容（元组格式）
- [ ] mdio_c.c: ta_invalid/op_invalid使用字符串表示
- [x] ps2_c.c: ATK颜色/点注解已添加（与Python一致的颜色值）
- [x] ps2_c.c: Word注解范围仅包含8个数据位（不含奇偶校验位）
- [x] ir_nec_c.c: 设备名/按键名查找表已硬编码
- [x] ir_nec_c.c: REMOTE annotation包含设备名和按键名
- [x] ir_rc5_c.c: 系统名/命令名查找表已硬编码
- [x] ir_rc5_c.c: Address/Command annotation包含系统名和命令名
- [ ] ir_sirc_c.c: 设备名查找表已硬编码
- [ ] ir_sirc_c.c: REMOTE annotation包含设备名
- [ ] spdif_c.c: 重检逻辑已实现（避免丢失初始数据）
- [ ] usb_signalling_c.c: 协议输出格式与Python兼容

## 轻微问题验证

- [ ] can_fd_c.c: bitpack_msb支持超过32位
- [ ] can_fd_c.c: 采样率检查已实现

## 编译验证

- [ ] 所有修改的C解码器编译通过
- [ ] 无编译错误和警告

## 审查回归Bug验证

- [x] lin_c.c: calc_parity()返回值位位置正确（p0在bit0, p1在bit1）
- [x] ir_nec_c.c: putpause()/putd()/handle_bit()使用实际samplenum作为end sample
- [x] swd_c.c: 协议输出字符串使用下划线（AP_READ/DP_READ/AP_WRITE/DP_WRITE）
- [x] i2s_c.c: channels和options的idn字段已填充
- [x] jtag_c.c: 使用c_decoder_has_channel() API替代直接结构体访问
- [x] iso7816_c.c: PCAP输出包含GSMTAP over UDP/IP/Ethernet封装
- [x] onewire_c.c: 过驱动模式short_thresh为2us
