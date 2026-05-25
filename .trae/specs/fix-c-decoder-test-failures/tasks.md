# Tasks

- [x] Task 1: 修改 decoder_test.c — stack解码器自动通道映射
  - [x] SubTask 1.1: 在stack解码器创建后，当没有显式channels字段时，自动构建通道映射（必需+可选通道）
  - [x] SubTask 1.2: 对stack解码器调用 srd_inst_channel_set_all() 设置映射
  - [x] SubTask 1.3: 编译验证 decoder_test 构建通过

- [x] Task 2: 修改 test_factory.py — 为stack解码器生成channels字段
  - [x] SubTask 2.1: 在生成config.json时，为stack中的解码器自动生成channels映射
  - [x] SubTask 2.2: UART堆叠解码器只映射rx通道（因为TX数据全0）
  - [x] SubTask 2.3: 修复parse_decoder_metadata的ID提取BUG
  - [x] SubTask 2.4: 重新生成所有测试数据

- [x] Task 3: 修复 parallel_c 测试数据
  - [x] SubTask 3.1: 为parallel_c生成包含CLK+8个数据通道的测试数据

- [x] Task 4: 运行全量自动化验证
  - [x] SubTask 4.1: 运行 `python run_all_tests.py --all --jobs 4`
  - [x] SubTask 4.2: 结果: 216 total, 197 PASS, 18 FAIL, 1 ERROR
  - [x] SubTask 4.3: UART堆叠deviations从~622降到8-24

- [x] Task 5: 修复delta_sigma_c ID映射
  - [x] SubTask 5.1: 添加'delta-sigma': 'delta_sigma'到HARDCODED_ID_MAP

# Task Dependencies

- Task 2 depends on Task 1
- Task 4 depends on Task 1, 2, 3
- Task 1 和 Task 3 可以并行执行

# 剩余18个FAIL的根因

## UART堆叠解码器（16个）— 细微逻辑差异
deviations已从~622降到8-24：
1. 时序偏差：C uart_c的停止位end_sample与Python差5个样本
2. 文本格式差异：如"Slave ID {} is invalid" vs "Slave ID 0 is invalid"
3. 注解数量差异：个别解码器有额外的MISSED/EXTRA注解

## ccd_c（1个）— Python解码器BUG
Python ccd/pd.py的self.wait()调用触发SystemError

## delta_sigma_c（1个）— ID映射已添加，待验证
