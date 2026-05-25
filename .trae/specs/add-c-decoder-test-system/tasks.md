# Tasks

- [x] Task 1: 创建C测试程序框架 `libsigrokdecode/tests/decoder_test.c`
  - [x] 1.1: 实现命令行参数解析（解码器名、测试数据路径、输出路径、容差参数）
  - [x] 1.2: 实现 config.json 读取（解码器名称、通道映射、选项、采样率）
  - [x] 1.3: 实现 input.bin 读取（位打包逻辑数据，按通道分离）
  - [x] 1.4: 实现 libsigrokdecode API 调用流程（srd_init → srd_session_new → srd_inst_new → srd_inst_channel_set_all → srd_pd_output_callback_add → srd_session_metadata_set → srd_session_start → srd_session_send → srd_session_end）
  - [x] 1.5: 实现注解收集回调，将 srd_proto_data_annotation 结构序列化为内存列表
  - [x] 1.6: 实现注解列表序列化为 actual.json
  - [x] 1.7: 实现 actual.json 与 expected.json 的比对逻辑（类别、样本范围、文本内容、容差）
  - [x] 1.8: 实现比对结果输出（PASS/FAIL + 差异详情 + 退出码）

- [x] Task 2: CMake集成，添加测试程序构建目标
  - [x] 2.1: 在 CMakeLists.txt 中添加 decoder_test 可执行目标
  - [x] 2.2: 链接 libsigrokdecode 和 glib-2.0 依赖
  - [x] 2.3: 验证构建成功

- [x] Task 3: 创建首批4个解码器的手工验证测试数据
  - [x] 3.1: 创建测试数据目录结构 `tests/testdata/{spi_c,i2c_c,uart_c,can_c}/default/`
  - [x] 3.2: 为 spi_c 生成 config.json + input.bin（标准SPI传输：CS低有效、CPOL=0/CPHA=0、8位数据0x53）
  - [x] 3.3: 为 i2c_c 生成 config.json + input.bin（I2C写操作：START → 地址0x50+W → ACK → 数据0x42 → ACK → STOP）
  - [x] 3.4: 为 uart_c 生成 config.json + input.bin（UART传输：8N1、波特率9600、发送0x55）
  - [x] 3.5: 为 can_c 生成 config.json + input.bin（标准CAN帧：ID=0x123、数据0xDE 0xAD）

- [x] Task 4: 创建Python参考输出脚本 `libsigrokdecode/tests/run_python_decoder.py`
  - [x] 4.1: 实现通过 decoder_test --python 运行Python解码器
  - [x] 4.2: 收集注解输出并序列化为 expected.json（与C输出相同JSON格式）
  - [x] 4.3: 为首批4个解码器生成 expected.json 参考文件
  - [x] 4.4: 实现 --all 批量模式，遍历所有有input.bin的目录生成expected.json

- [x] Task 5: 创建自动化测试数据生成脚本 `libsigrokdecode/tests/generate_testdata.py`
  - [x] 5.1: 扫描C解码器源文件，解析元数据（通道、选项、输入类型）
  - [x] 5.2: 提取 channels、optional_channels、options、inputs 定义
  - [x] 5.3: 对 inputs 包含 "logic" 的解码器，自动生成 config.json（默认通道映射+默认选项+1MHz采样率）
  - [x] 5.4: 为逻辑信号解码器生成 input.bin（基本信号模式：交替0/1、随机数据）
  - [x] 5.5: 对 inputs 不包含 "logic" 的解码器，生成标记 needs_upstream=true 的 config.json
  - [x] 5.6: 处理全部215个解码器（112 logic + 102 upstream + 1 no channels）

- [x] Task 6: 创建批量测试运行脚本 `libsigrokdecode/tests/run_tests.sh`
  - [x] 6.1: 实现扫描testdata目录、逐个运行decoder_test
  - [x] 6.2: 实现汇总报告输出（总测试数、PASS/FAIL/SKIP统计 + FAIL差异详情）
  - [x] 6.3: 支持指定单个解码器名称运行

- [x] Task 7: 端到端验证
  - [x] 7.1: 构建测试程序 — ninja decoder_test 成功
  - [x] 7.2: 运行首批4个解码器测试，确认测试流程跑通 — spi_c(47 ann), i2c_c(58 ann), uart_c(119 ann), can_c(22 ann)
  - [x] 7.3: 运行自动化生成脚本，确认能处理全部215个解码器 — 215 processed, 0 errors
  - [x] 7.4: 验证比对逻辑能正确检测匹配 — PASS: 47 annotations match expected output

# Task Dependencies
- Task 2 depends on Task 1
- Task 3 is independent (can parallel with Task 1)
- Task 4 is independent (can parallel with Task 1)
- Task 5 depends on Task 4 (复用Python解码器运行逻辑)
- Task 6 depends on Task 1
- Task 7 depends on Task 1, Task 2, Task 3, Task 4, Task 5, Task 6
