# Tasks

## 第一批：严重问题修复（影响解码正确性或下游兼容性）

- [ ] Task 1: 修复 can_c.c — annotation范围位宽扩展和协议输出格式
  - [ ] SubTask 1.1: 添加putg辅助函数实现annotation范围的left/right偏移扩展
  - [ ] SubTask 1.2: 修改协议输出格式，使用与Python兼容的元组格式替代逗号分隔字符串
  - [ ] SubTask 1.3: 添加采样率检查（无效时安全退出）

- [ ] Task 2: 修复 jtag_c.c — 协议输出格式
  - [ ] SubTask 2.1: 修改"NEW STATE"协议输出，附带状态名称字符串
  - [ ] SubTask 2.2: 修改"DR TDI"/"DR TDO"/"IR TDI"/"IR TDO"协议输出，包含bitstring和per-bit ss/es列表
  - [ ] SubTask 2.3: 修正SHIFT->EXIT1时最后一位的bit annotation输出时机

- [ ] Task 3: 修复 swd_c.c — 请求奇偶校验和协议输出
  - [ ] SubTask 3.1: 添加请求奇偶校验检查（calc_parity），校验失败时输出警告
  - [ ] SubTask 3.2: 修改协议输出格式，包含ACK值

- [ ] Task 4: 修复 i2s_c.c — WAV文件头和annotation行
  - [ ] SubTask 4.1: 在首次二进制输出前添加WAV文件头（44字节RIFF/WAVE头）
  - [ ] SubTask 4.2: 修改annotation行定义为1行（与Python版本一致，无annotation_rows）
  - [ ] SubTask 4.3: 修改协议输出格式，使用与Python兼容的格式

- [ ] Task 5: 修复 onewire_c.c — 时序检查和存在检测
  - [ ] SubTask 5.1: 添加完整的timing字典和时序验证（RSTL/RSTH/PDH/PDL/SLOT/REC/LOWR）
  - [ ] SubTask 5.2: 添加采样率检查（正常模式>400kHz，过驱动>2MHz）
  - [ ] SubTask 5.3: 添加存在检测超时机制，防止永久阻塞
  - [ ] SubTask 5.4: 添加"无存在检测"处理，输出['RESET/PRESENCE', False]
  - [ ] SubTask 5.5: 添加过驱动复位脉冲上限检查

- [ ] Task 6: 修复 nrzi_c.c — 协议输出格式
  - [ ] SubTask 6.1: 修改协议输出格式，输出裸整数（0或1）而非命名命令，与Python版本兼容

- [ ] Task 7: 修复 4b5b_c.c — 架构对齐
  - [ ] SubTask 7.1: 修改inputs为['nrzi']，从NRZI解码器接收已解码的bit数据
  - [ ] SubTask 7.2: 移除内嵌的NRZI解码逻辑（SYNC/DECODE状态机）
  - [ ] SubTask 7.3: 修改协议输出格式为(value, is_control_symbol)元组格式
  - [ ] SubTask 7.4: 添加recv_proto回调处理来自NRZI解码器的bit数据

- [ ] Task 8: 修复 lin_c.c — 输入源对齐
  - [ ] SubTask 8.1: 修改inputs为['uart']，从UART解码器接收已解码的字节
  - [ ] SubTask 8.2: 移除内嵌的read_uart_byte()函数
  - [ ] SubTask 8.3: 添加recv_proto回调处理来自UART解码器的DATA/BREAK包
  - [ ] SubTask 8.4: 添加帧中断处理（wipe_break_null_byte逻辑）
  - [ ] SubTask 8.5: 添加end()方法处理未完成帧

- [ ] Task 9: 修复 graycode_c.c — 位数支持
  - [ ] SubTask 9.1: 添加bits选项支持任意位数（替代硬编码2位）
  - [ ] SubTask 9.2: 添加SI前缀选项
  - [ ] SubTask 9.3: 添加滑动窗口逻辑

- [ ] Task 10: 修复 numbers_and_state_c.c — enum映射
  - [ ] SubTask 10.1: 添加enum选项和映射功能
  - [ ] SubTask 10.2: 修改协议输出格式与Python兼容

- [ ] Task 11: 修复 iso7816_c.c — PCAP输出和T=1
  - [ ] SubTask 11.1: 添加PCAP二进制输出
  - [ ] SubTask 11.2: 添加T=1多块/APDU处理逻辑

## 第二批：中等问题修复（影响功能完整性或用户体验）

- [ ] Task 12: 修复 hdlc_c.c — CS-CHANGE和协议输出
  - [ ] SubTask 12.1: 添加CS-CHANGE输出（无EN通道时输出['CS-CHANGE', None, None]）
  - [ ] SubTask 12.2: 修改协议输出格式，包含per-byte ss/es信息

- [ ] Task 13: 修复 microwire_c.c — 协议输出格式
  - [ ] SubTask 13.1: 修改协议输出格式，使用与Python兼容的PyPacket格式

- [ ] Task 14: 修复 mdio_c.c — 协议输出格式
  - [ ] SubTask 14.1: 修改协议输出格式，使用与Python兼容的元组格式
  - [ ] SubTask 14.2: 修改ta_invalid/op_invalid为字符串表示

- [ ] Task 15: 修复 ps2_c.c — ATK颜色和注解范围
  - [ ] SubTask 15.1: 添加ATK颜色/点注解（与Python版本一致的颜色值）
  - [ ] SubTask 15.2: 修正Word注解范围（仅包含8个数据位，不含奇偶校验位）

- [ ] Task 16: 修复 ir_nec_c.c — 设备名/按键名查找
  - [ ] SubTask 16.1: 将ir_nec/lists.py中的address和command字典硬编码到C代码中
  - [ ] SubTask 16.2: 修改REMOTE annotation输出，包含设备名和按键名

- [ ] Task 17: 修复 ir_rc5_c.c — 系统名/命令名查找
  - [ ] SubTask 17.1: 将ir_rc5/lists.py中的system和command字典硬编码到C代码中
  - [ ] SubTask 17.2: 修改Address/Command annotation输出，包含系统名和命令名

- [ ] Task 18: 修复 ir_sirc_c.c — 设备名查找
  - [ ] SubTask 18.1: 将ir_sirc/lists.py中的ADDRESSES字典硬编码到C代码中
  - [ ] SubTask 18.2: 修改REMOTE annotation输出，包含设备名

- [ ] Task 19: 修复 spdif_c.c — 重检逻辑
  - [ ] SubTask 19.1: 添加find_third_pulse_width后的重检逻辑，避免丢失初始数据
  - [ ] SubTask 19.2: 修正Sample annotation使用正确的类（ANN_SAMPLES而非ANN_AUX）— 注意Python也有此bug，保持一致即可

- [ ] Task 20: 修复 usb_signalling_c.c — 协议输出格式
  - [ ] SubTask 20.1: 修改协议输出格式，确保与Python版本兼容

## 第三批：轻微问题修复

- [ ] Task 21: 修复 can_fd_c.c — bitpack_msb和采样率检查
  - [ ] SubTask 21.1: 扩展bitpack_msb支持超过32位
  - [ ] SubTask 21.2: 添加采样率检查（无效时安全退出）

- [ ] Task 22: 修复 dcf77_c.c — 星期/月份名称
  - [ ] SubTask 22.1: 保持硬编码英文名称（与locale无关，功能等价）

## 第四批：编译验证

- [ ] Task 23: 编译验证所有修改的C解码器
  - [ ] SubTask 23.1: 增量构建所有修改的C解码器
  - [ ] SubTask 23.2: 确保无编译错误和警告

# Task Dependencies
- [Task 7] 依赖 [Task 6]（4b5b需要NRZI先修复协议输出格式）
- [Task 8] 依赖 uart_c.c已正确（LIN依赖UART解码器的协议输出）
- [Task 23] 依赖所有其他tasks
- [Task 1-5] 可并行
- [Task 6-11] 可并行
- [Task 12-22] 可并行
