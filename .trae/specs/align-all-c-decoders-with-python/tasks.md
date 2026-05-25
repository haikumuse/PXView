# Tasks

## 第一批：严重问题修复（影响解码正确性或下游兼容性）

- [x] Task 1: 修复 can_c.c — annotation范围位宽扩展和协议输出格式
  - [x] SubTask 1.1: 添加putg辅助函数实现annotation范围的left/right偏移扩展
  - [x] SubTask 1.2: 修改协议输出格式，使用与Python兼容的元组格式替代逗号分隔字符串
  - [x] SubTask 1.3: 添加采样率检查（无效时安全退出）

- [x] Task 2: 修复 jtag_c.c — 协议输出格式
  - [x] SubTask 2.1: 修改"NEW STATE"协议输出，附带状态名称字符串
  - [x] SubTask 2.2: 修改"DR TDI"/"DR TDO"/"IR TDI"/"IR TDO"协议输出，包含bitstring和per-bit ss/es列表
  - [x] SubTask 2.3: 修正SHIFT->EXIT1时最后一位的bit annotation输出时机

- [x] Task 3: 修复 swd_c.c — 请求奇偶校验和协议输出
  - [x] SubTask 3.1: 添加请求奇偶校验检查（calc_parity），校验失败时输出警告
  - [x] SubTask 3.2: 修改协议输出格式，包含ACK值

- [x] Task 4: 修复 i2s_c.c — WAV文件头和annotation行
  - [x] SubTask 4.1: 在首次二进制输出前添加WAV文件头（44字节RIFF/WAVE头）
  - [x] SubTask 4.2: 修改annotation行定义为1行（与Python版本一致，无annotation_rows）
  - [x] SubTask 4.3: 修改协议输出格式，使用与Python兼容的格式

- [x] Task 5: 修复 onewire_c.c — 时序检查和存在检测
  - [x] SubTask 5.1: 添加完整的timing字典和时序验证（RSTL/RSTH/PDH/PDL/SLOT/REC/LOWR）
  - [x] SubTask 5.2: 添加采样率检查（正常模式>400kHz，过驱动>2MHz）
  - [x] SubTask 5.3: 添加存在检测超时机制，防止永久阻塞
  - [x] SubTask 5.4: 添加"无存在检测"处理，输出['RESET/PRESENCE', False]
  - [x] SubTask 5.5: 添加过驱动复位脉冲上限检查

- [x] Task 6: 修复 nrzi_c.c — 协议输出格式
  - [x] SubTask 6.1: 修改协议输出格式，输出裸整数（0或1）而非命名命令，与Python版本兼容

- [x] Task 7: 修复 4b5b_c.c — 架构对齐
  - [x] SubTask 7.1: 修改inputs为['nrzi']，从NRZI解码器接收已解码的bit数据
  - [x] SubTask 7.2: 移除内嵌的NRZI解码逻辑（SYNC/DECODE状态机）
  - [x] SubTask 7.3: 修改协议输出格式为(value, is_control_symbol)元组格式
  - [x] SubTask 7.4: 添加recv_proto回调处理来自NRZI解码器的bit数据

- [x] Task 8: 修复 lin_c.c — 输入源对齐
  - [x] SubTask 8.1: 修改inputs为['uart']，从UART解码器接收已解码的字节
  - [x] SubTask 8.2: 移除内嵌的read_uart_byte()函数
  - [x] SubTask 8.3: 添加recv_proto回调处理来自UART解码器的DATA/BREAK包
  - [x] SubTask 8.4: 添加帧中断处理（wipe_break_null_byte逻辑）
  - [x] SubTask 8.5: 添加end()方法处理未完成帧

- [x] Task 9: 修复 graycode_c.c — 位数支持
  - [x] SubTask 9.1: 添加bits选项支持任意位数（替代硬编码2位）
  - [x] SubTask 9.2: 添加SI前缀选项
  - [x] SubTask 9.3: 添加滑动窗口逻辑

- [x] Task 10: 修复 numbers_and_state_c.c — enum映射
  - [x] SubTask 10.1: 添加enum选项和映射功能
  - [x] SubTask 10.2: 修改协议输出格式与Python兼容

- [x] Task 11: 修复 iso7816_c.c — PCAP输出和T=1
  - [x] SubTask 11.1: 添加PCAP二进制输出
  - [x] SubTask 11.2: 添加T=1多块/APDU处理逻辑

## 第二批：中等问题修复（影响功能完整性或用户体验）

- [x] Task 12: 修复 hdlc_c.c — CS-CHANGE和协议输出
  - [x] SubTask 12.1: 添加CS-CHANGE输出（无EN通道时输出['CS-CHANGE', None, None]）
  - [x] SubTask 12.2: 修改协议输出格式，包含per-byte ss/es信息

- [x] Task 13: 修复 microwire_c.c — 协议输出格式
  - [x] SubTask 13.1: 修改协议输出格式，使用与Python兼容的PyPacket格式

- [x] Task 14: 修复 mdio_c.c — 协议输出格式
  - [x] SubTask 14.1: 修改协议输出格式，使用与Python兼容的元组格式
  - [x] SubTask 14.2: 修改ta_invalid/op_invalid为字符串表示

- [x] Task 15: 修复 ps2_c.c — ATK颜色和注解范围
  - [x] SubTask 15.1: 添加ATK颜色/点注解（与Python版本一致的颜色值）
  - [x] SubTask 15.2: 修正Word注解范围（仅包含8个数据位，不含奇偶校验位）

- [x] Task 16: 修复 ir_nec_c.c — 设备名/按键名查找
  - [x] SubTask 16.1: 将ir_nec/lists.py中的address和command字典硬编码到C代码中
  - [x] SubTask 16.2: 修改REMOTE annotation输出，包含设备名和按键名

- [x] Task 17: 修复 ir_rc5_c.c — 系统名/命令名查找
  - [x] SubTask 17.1: 将ir_rc5/lists.py中的system和command字典硬编码到C代码中
  - [x] SubTask 17.2: 修改Address/Command annotation输出，包含系统名和命令名

- [x] Task 18: 修复 ir_sirc_c.c — 设备名查找
  - [x] SubTask 18.1: 将ir_sirc/lists.py中的ADDRESSES字典硬编码到C代码中
  - [x] SubTask 18.2: 修改REMOTE annotation输出，包含设备名

- [x] Task 19: 修复 spdif_c.c — 重检逻辑
  - [x] SubTask 19.1: 添加find_third_pulse_width后的重检逻辑，避免丢失初始数据
  - [x] SubTask 19.2: 修正Sample annotation使用正确的类（ANN_SAMPLES而非ANN_AUX）— 注意Python也有此bug，保持一致即可

- [x] Task 20: 修复 usb_signalling_c.c — 协议输出格式
  - [x] SubTask 20.1: 修改协议输出格式，确保与Python版本兼容

## 第三批：轻微问题修复

- [x] Task 21: 修复 can_fd_c.c — bitpack_msb和采样率检查
  - [x] SubTask 21.1: 扩展bitpack_msb支持超过32位
  - [x] SubTask 21.2: 添加采样率检查（无效时安全退出）

- [x] Task 22: 修复 dcf77_c.c — 星期/月份名称
  - [x] SubTask 22.1: 保持硬编码英文名称（与locale无关，功能等价）

## 第四批：编译验证

- [x] Task 23: 编译验证所有修改的C解码器
  - [x] SubTask 23.1: 增量构建所有修改的C解码器
  - [x] SubTask 23.2: 确保无编译错误和警告

## 第五批：审查发现的回归Bug修复

- [x] Task 24: 修复 lin_c.c — calc_parity()返回值位位置错误
  - [x] SubTask 24.1: 将calc_parity()返回值从 `(p0 << 6) | (p1 << 7)` 改为 `(p0 << 0) | (p1 << 1)`

- [x] Task 25: 修复 ir_nec_c.c — 结束采样点为0
  - [x] SubTask 25.1: 修改putpause()函数，使用实际samplenum替代0作为end sample
  - [x] SubTask 25.2: 修改putd()函数，使用实际samplenum替代0作为end sample
  - [x] SubTask 25.3: 修改handle_bit()函数，使用实际samplenum替代0作为end sample

- [x] Task 26: 修复 swd_c.c — 协议输出字符串使用空格而非下划线
  - [x] SubTask 26.1: 将"AP READ"/"DP READ"/"AP WRITE"/"DP WRITE"改为"AP_READ"/"DP_READ"/"AP_WRITE"/"DP_WRITE"

- [x] Task 27: 修复 i2s_c.c — 缺失channel/option的idn字段
  - [x] SubTask 27.1: 为channels和options添加idn字段（国际化标识符）

- [x] Task 28: 修复 jtag_c.c — 使用直接结构体访问而非API
  - [x] SubTask 28.1: 将di->dec_num_channels和di->dec_channelmap替换为c_decoder_has_channel() API调用

- [x] Task 29: 修复 iso7816_c.c — PCAP输出缺少GSMTAP封装
  - [x] SubTask 29.1: 添加GSMTAP over UDP/IP/Ethernet封装层到PCAP输出

- [x] Task 30: 修复 onewire_c.c — 过驱动模式时序阈值差异
  - [x] SubTask 30.1: 将过驱动模式short_thresh从1us调整为2us（与Python LOWR max一致）

## 第六批：编译验证

- [x] Task 31: 编译验证所有修复
  - [x] SubTask 31.1: 增量构建所有修改的C解码器
  - [x] SubTask 31.2: 确保无编译错误和警告

# Task Dependencies
- [Task 7] 依赖 [Task 6]（4b5b需要NRZI先修复协议输出格式）
- [Task 8] 依赖 uart_c.c已正确（LIN依赖UART解码器的协议输出）
- [Task 23] 依赖所有其他tasks
- [Task 1-5] 可并行
- [Task 6-11] 可并行
- [Task 12-22] 可并行
- [Task 24-30] 可并行
- [Task 31] 依赖 [Task 24-30]
