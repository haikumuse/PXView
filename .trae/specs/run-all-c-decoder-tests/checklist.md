# 运行全部C解码器测试 — 验证清单

## decoder_test --python 模式

- [x] decoder_test 接受 --python 命令行参数
- [x] --python 模式能加载 Python 解码器（通过 srd_init(path) + srd_decoder_load_all）
- [x] --python 模式能正确推断 Python 解码器搜索路径
- [x] --python 模式能运行 Python 解码器并输出 actual.json
- [x] --python 模式下 Python 解码器加载失败时以退出码 2 退出

## Python 参考输出生成

- [x] run_python_decoder.py 单解码器模式能正确生成 expected.json
- [x] run_python_decoder.py --all 批量模式能遍历所有有 input.bin 的目录
- [x] C 解码器名称到 Python 解码器名称映射正确（包括特殊映射如 can_fd_c -> can-fd）
- [x] 生成的 expected.json 格式与 C 解码器 actual.json 格式一致

## 批量测试运行

- [x] 全部112个逻辑输入解码器都已测试
- [x] 批量运行 C 解码器测试能输出 PASS/FAIL/ERROR 汇总
- [x] FAIL 测试的差异详情清晰可读（注解数量、类别、范围、文本）
- [x] 非 logic 输入解码器正确标记为 SKIP

## 最终报告

- [x] 汇总报告包含各分类的解码器数量统计
- [x] FAIL 解码器的差异原因已分析
- [x] ERROR 解码器的错误信息已记录
