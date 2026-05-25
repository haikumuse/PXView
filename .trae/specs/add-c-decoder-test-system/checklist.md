# C解码器形式化测试系统 — 验证清单

## 测试程序功能验证

- [x] decoder_test 可成功编译并链接 libsigrokdecode
- [x] decoder_test 能加载指定C解码器DLL并创建解码器实例
- [x] decoder_test 能读取 config.json 并正确设置通道映射和选项
- [x] decoder_test 能读取 input.bin 并正确送入位打包逻辑数据
- [x] decoder_test 能通过 SRD_OUTPUT_ANN 回调收集所有注解
- [x] decoder_test 能将注解序列化为 actual.json
- [x] decoder_test 能正确比对 actual.json 与 expected.json
- [x] 比对结果为 PASS 时输出正确
- [x] 比对结果为 FAIL 时输出差异详情（样本号、期望值、实际值）
- [x] 解码器加载失败时以非零退出码退出
- [x] 容差模式（--tolerance N）正确工作

## 手工测试数据验证（首批4个）

- [x] spi_c 测试数据目录包含 config.json、input.bin、expected.json
- [x] i2c_c 测试数据目录包含 config.json、input.bin、expected.json
- [x] uart_c 测试数据目录包含 config.json、input.bin、expected.json
- [x] can_c 测试数据目录包含 config.json、input.bin、expected.json
- [x] config.json 格式正确，包含通道映射、选项、采样率
- [x] input.bin 为有效的位打包逻辑数据

## Python参考输出验证

- [x] run_python_decoder.py 能运行Python解码器并收集注解
- [x] 生成的 expected.json 格式与 actual.json 一致
- [x] Python参考输出包含正确的注解类别、样本范围和文本
- [x] --all 批量模式能遍历所有目录生成参考输出

## 自动化生成脚本验证

- [x] generate_testdata.py 能扫描C解码器源文件目录
- [x] 能从C源文件解析提取通道和选项定义
- [x] 对 inputs=["logic"] 的解码器生成 config.json + input.bin
- [x] 对 inputs 不含 "logic" 的解码器生成标记 needs_upstream=true 的 config.json
- [x] 能处理全部215个C解码器（不崩溃、不遗漏）

## 批量测试验证

- [x] run_tests.sh / run_tests.cmd 能扫描testdata目录并逐个运行测试
- [x] 输出汇总报告（PASS/FAIL/SKIP统计）
- [x] 支持指定单个解码器运行
- [x] FAIL测试的差异详情清晰可读

## 端到端验证

- [x] 首批4个解码器测试流程完整跑通（spi_c: 47 ann, i2c_c: 58 ann, uart_c: 119 ann, can_c: 22 ann）
- [x] 比对逻辑能正确检测匹配（PASS: 47 annotations match expected output）
- [x] 自动化生成脚本能处理全部215个解码器（215 processed, 0 errors）
