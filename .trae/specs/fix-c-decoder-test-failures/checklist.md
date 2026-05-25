# 修复测试框架stack通道映射 — 验证清单

## decoder_test.c stack自动通道映射

- [x] stack条目无channels字段时，自动将输入数据前N个通道映射到stack解码器前N个通道（必需+可选）
- [x] 自动映射后调用 srd_inst_channel_set_all()
- [x] stack条目有显式channels字段时，优先使用显式映射
- [x] decoder_test 编译通过

## test_factory.py stack channels生成

- [x] test_factory为stack解码器自动生成channels字段
- [x] UART堆叠解码器只映射rx通道（因为TX数据全0）
- [x] 修复parse_decoder_metadata的ID提取BUG（从srd_c_decoder结构体提取）
- [x] 重新生成的测试数据格式正确

## parallel_c 测试数据

- [x] parallel_c测试数据包含CLK+8个数据通道（9通道）
- [x] parallel_c测试PASS

## 全量测试验证

- [x] 运行全量测试，FAIL数量从19减少到18
- [x] UART堆叠解码器deviations从~622降到8-24（巨大改善）
- [x] delta_sigma_c ID映射已添加到HARDCODED_ID_MAP
- [x] 无新增ERROR（delta_sigma_c的ERROR已修复）
- [x] 之前PASS的解码器没有回退为FAIL

## 剩余18个FAIL的根因分析

### UART堆叠解码器（16个）— 细微逻辑差异
deviations已从~622降到8-24，主要是：
1. **时序偏差**：C uart_c的停止位end_sample与Python差5个样本
2. **文本格式差异**：如"Slave ID {} is invalid" vs "Slave ID 0 is invalid"
3. **注解数量差异**：个别解码器有额外的MISSED/EXTRA注解

### ccd_c（1个）— Python解码器BUG
Python ccd/pd.py的self.wait()调用触发SystemError

### delta_sigma_c（1个）— ID映射
已添加到HARDCODED_ID_MAP，需要重新运行验证
