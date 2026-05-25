# Tasks

- [x] Task 1: 修改 decoder_test.c 添加 --python 模式
  - [x] 1.1: 在 cmdline_args 结构中添加 python_mode 字段，在 parse_args() 中添加 --python 参数解析
  - [x] 1.2: 实现 Python 解码器搜索路径推断逻辑（环境变量 > 相对路径 > CWD）
  - [x] 1.3: 当 --python 启用时，调用 srd_init(path) + srd_decoder_load_all() 加载 Python 解码器（而非 srd_c_decoder_path_add() + srd_c_decoder_load_all()）
  - [x] 1.4: 验证 --python 模式能正确运行 Python 解码器并输出 actual.json

- [x] Task 2: 修改 run_python_decoder.py 适配 --python 模式
  - [x] 2.1: 确认脚本调用 decoder_test --python 的命令行参数正确
  - [x] 2.2: 添加 Python 解码器搜索路径的自动推断（传递给 decoder_test 或设置环境变量）
  - [x] 2.3: 验证单解码器模式能正确生成 expected.json

- [x] Task 3: 构建并验证 decoder_test --python 模式
  - [x] 3.1: 增量构建 decoder_test（ninja decoder_test）
  - [x] 3.2: 用 spi 解码器验证 --python 模式能正常运行并生成 actual.json
  - [x] 3.3: 对比 Python spi 输出与 C spi_c 输出，确认注解格式一致

- [x] Task 4: 批量生成全部112个逻辑输入解码器的 Python 参考输出
  - [x] 4.1: 创建 run_all_tests.py 批量测试脚本
  - [x] 4.2: 运行全部112个解码器测试
  - [x] 4.3: 统计生成结果（PASS=61, FAIL=20, ERROR=31）

- [x] Task 5: 批量运行全部 C 解码器测试并生成汇总报告
  - [x] 5.1: 运行 run_all_tests.py --all 对所有逻辑输入解码器执行 C/Python 对比测试
  - [x] 5.2: 收集测试结果，统计 PASS/FAIL/ERROR 数量
  - [x] 5.3: 对 FAIL 的解码器分析差异原因（注解数量、类别、范围、文本）
  - [x] 5.4: 生成最终汇总报告（test_results.csv）

# Task Dependencies
- Task 2 depends on Task 1
- Task 3 depends on Task 1
- Task 4 depends on Task 2, Task 3
- Task 5 depends on Task 4
