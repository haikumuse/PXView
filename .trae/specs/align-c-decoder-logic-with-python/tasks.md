# Tasks

- [x] Task 1: 修复 I2S C 解码器 WS 极性反转 bug
  - [x] SubTask 1.1: 将 `ws_is_left = ws_polarity_left_high ? (ws == 0) : (ws == 1)` 改为 `ws_is_left = ws_polarity_left_high ? (ws == 1) : (ws == 0)`
  - [x] SubTask 1.2: 实现 bit_shift="right-shifted by one" 选项的处理逻辑

- [x] Task 2: 修复 JTAG C 解码器初始状态和 SHIFT 逻辑
  - [x] SubTask 2.1: 将初始状态从 TEST_LOGIC_RESET 改为 RUN_TEST_IDLE
  - [x] SubTask 2.2: 在 SHIFT-DR/SHIFT-IR 状态中跳过第一个 bit
  - [x] SubTask 2.3: 修改 bitstring annotation 格式，添加十六进制值和 bit 数

- [x] Task 3: 修复 SPI C 解码器 format 选项和 CS-CHANGE
  - [x] SubTask 3.1: 在 spi_put_data() 中读取 format 选项而非硬编码 "hex"
  - [x] SubTask 3.2: 在 spi_start() 中读取 format 选项到 state 结构体
  - [x] SubTask 3.3: 添加 CS-CHANGE annotation 输出

- [x] Task 4: 扩展 UART C 解码器 parity 和 format 类型
  - [x] SubTask 4.1: 在 uart_parity 枚举中添加 PARITY_ZERO, PARITY_ONE, PARITY_IGNORE
  - [x] SubTask 4.2: 在 uart_start() 中解析 "zero"/"one"/"ignore" parity 选项
  - [x] SubTask 4.3: 在 parity_ok() 函数中处理新的 parity 类型
  - [x] SubTask 4.4: 在 format 选项中添加 "oct" 和 "bin" 格式支持

- [x] Task 5: 为 I2C C 解码器添加 OUTPUT_PYTHON 输出
  - [x] SubTask 5.1: 在 i2c_start() 中注册 SRD_OUTPUT_PYTHON 输出
  - [x] SubTask 5.2: 在各协议事件处调用 c_decoder_put_python() 输出协议数据

- [x] Task 6: 为 SPI C 解码器添加 OUTPUT_PYTHON 输出
  - [x] SubTask 6.1: 在 spi_start() 中注册 SRD_OUTPUT_PYTHON 输出
  - [x] SubTask 6.2: 在数据完成、CS 变化、传输完成处调用 c_decoder_put_python()

- [x] Task 7: 为 UART C 解码器添加 OUTPUT_PYTHON 输出
  - [x] SubTask 7.1: 在 uart_start() 中注册 SRD_OUTPUT_PYTHON 输出
  - [x] SubTask 7.2: 在各帧事件处调用 c_decoder_put_python()

- [x] Task 8: 为 JTAG C 解码器添加 OUTPUT_PYTHON 输出
  - [x] SubTask 8.1: 在 jtag_start() 中注册 SRD_OUTPUT_PYTHON 输出
  - [x] SubTask 8.2: 在状态变化和 bitstring 完成处调用 c_decoder_put_python()

- [x] Task 9: 为 SWD C 解码器添加 OUTPUT_PYTHON 输出
  - [x] SubTask 9.1: 在 swd_start() 中注册 SRD_OUTPUT_PYTHON 输出
  - [x] SubTask 9.2: 在读/写操作完成处调用 c_decoder_put_python()

- [x] Task 10: 为 HDLC C 解码器添加 OUTPUT_PYTHON 输出
  - [x] SubTask 10.1: 在 hdlc_start() 中注册 SRD_OUTPUT_PYTHON 输出
  - [x] SubTask 10.2: 在帧完成处调用 c_decoder_put_python()

- [x] Task 11: 为 I2S C 解码器添加 OUTPUT_PYTHON 输出
  - [x] SubTask 11.1: 在 i2s_start() 中注册 SRD_OUTPUT_PYTHON 输出
  - [x] SubTask 11.2: 在左右声道字完成处调用 c_decoder_put_python()

- [x] Task 12: 编译验证
  - [x] SubTask 12.1: 增量构建所有修改的 C 解码器
  - [x] SubTask 12.2: 确保无编译错误

# Task Dependencies
- [Task 5-11] 依赖 [Task 1-4]（先修复逻辑 bug，再添加输出）
- [Task 5-11] 之间可并行
- [Task 12] 依赖所有其他 tasks
