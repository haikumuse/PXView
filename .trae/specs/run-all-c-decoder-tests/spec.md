# 运行全部C解码器测试并验证结果 Spec

## Why
项目已有215个C解码器和完整的测试框架（decoder_test.c），但只验证了4个解码器。112个可直接测试的逻辑输入解码器缺少Python参考输出（expected.json），无法判断C解码器解码结果是否正确。需要：1）让decoder_test支持运行Python解码器生成参考输出；2）批量运行全部112个逻辑输入解码器测试；3）对比C与Python输出，发现并报告差异。

## What Changes
- 修改 `decoder_test.c` 添加 `--python` 模式，通过 libsigrokdecode C API 运行 Python 解码器生成 expected.json
- 修改 `run_python_decoder.py` 使其能正确调用 `--python` 模式
- 批量运行全部112个逻辑输入解码器，生成 Python 参考输出并对比
- 生成汇总报告，列出 PASS/FAIL/SKIP/ERROR 的解码器及差异详情

## Impact
- Affected code: `libsigrokdecode/tests/decoder_test.c`（添加--python模式）、`libsigrokdecode/tests/run_python_decoder.py`（修正调用方式）
- Affected specs: `add-c-decoder-test-system`（扩展测试框架功能）
- Dependencies: Python3 运行时（libsigrokdecode内嵌Python解释器已支持）

## ADDED Requirements

### Requirement: decoder_test 支持 --python 模式运行 Python 解码器
系统 SHALL 在 decoder_test 中添加 `--python` 命令行参数，启用时通过 libsigrokdecode C API 加载并运行 Python 解码器（而非 C 解码器 DLL），收集注解输出到 actual.json。

#### Scenario: 使用 --python 运行 Python 解码器
- **WHEN** 执行 `decoder_test --python -d spi -t testdata/spi_c/default --generate-only`
- **THEN** 程序通过 `srd_decoder_load_all()` 加载 Python 解码器，用 Python 解码器 ID（如 "spi"）实例化，送入相同 input.bin 数据，收集注解输出到 actual.json

#### Scenario: Python 解码器搜索路径
- **WHEN** 使用 `--python` 模式
- **THEN** 程序设置 Python 解码器搜索路径为 `libsigrokdecode/decoders/` 目录（通过环境变量或相对路径推断）

#### Scenario: Python 解码器加载失败
- **WHEN** 指定的 Python 解码器不存在或加载失败
- **THEN** 程序输出错误信息并以退出码 2 退出

### Requirement: 批量生成 Python 参考输出
系统 SHALL 能批量运行全部112个逻辑输入解码器对应的 Python 解码器，生成 expected.json 参考文件。

#### Scenario: 批量生成所有 expected.json
- **WHEN** 执行 `python run_python_decoder.py --all -t testdata`
- **THEN** 对每个有 input.bin 的测试数据目录，运行对应 Python 解码器，生成 expected.json

#### Scenario: C 解码器名称到 Python 解码器名称的映射
- **WHEN** 遇到 C 解码器名称（如 "spi_c"、"can_fd_c"）
- **THEN** 自动映射到 Python 解码器名称（如 "spi"、"can-fd"），使用已有的 C_TO_PY_DECODER_MAP 和通用 _c 后缀去除规则

### Requirement: 批量运行全部 C 解码器测试并对比
系统 SHALL 批量运行全部112个逻辑输入 C 解码器测试，与 Python 参考输出对比，生成汇总报告。

#### Scenario: 运行全部测试
- **WHEN** 执行批量测试脚本
- **THEN** 对每个有 expected.json 的解码器运行 C 解码器测试，输出 PASS/FAIL/SKIP/ERROR 汇总

#### Scenario: 差异报告
- **WHEN** C 解码器输出与 Python 参考输出存在差异
- **THEN** 报告差异详情：注解数量差异、类别不匹配、样本范围偏差、文本内容差异

### Requirement: 注解对比需处理 C/Python 输出差异
系统 SHALL 在对比 C 解码器和 Python 解码器输出时，处理已知的系统性差异。

#### Scenario: ATK 颜色注解过滤
- **WHEN** Python 解码器输出 ATK 颜色注解（如 "color:#xxx"），而 C 解码器不输出
- **THEN** 对比时过滤掉纯颜色注解，仅对比协议语义注解

#### Scenario: show_data_point 选项差异
- **WHEN** Python 解码器因 show_data_point 选项产生额外的 ATK 点注解
- **THEN** 对比时统一选项设置，确保 C 和 Python 使用相同的选项值

## MODIFIED Requirements

无修改的需求。

## REMOVED Requirements

无移除的需求。

---

## 技术方案

### decoder_test.c --python 模式实现

核心修改点：
1. 添加 `--python` 命令行参数到 `cmdline_args` 结构和 `parse_args()` 函数
2. 当 `--python` 启用时：
   - 调用 `srd_decoder_searchpath_add()` 添加 Python 解码器搜索路径（而非 `srd_c_decoder_path_add()`）
   - 调用 `srd_decoder_load_all()` 加载 Python 解码器（而非 `srd_c_decoder_load_all()`）
   - 使用 Python 解码器 ID（如 "spi"）调用 `srd_inst_new()`
3. 其余流程（session 创建、数据发送、注解收集、JSON 输出）完全相同

### Python 解码器搜索路径推断

优先级：
1. `PY_DECODERS_PATH` 环境变量
2. 相对于可执行文件的 `../libsigrokdecode/decoders/`
3. 相对于 CWD 的 `libsigrokdecode/decoders/`

### 批量测试流程

```
1. 生成 expected.json：
   python run_python_decoder.py --all -t testdata

2. 运行 C 解码器测试：
   run_tests.cmd --testdata-dir testdata

3. 汇总报告
```

### 预期结果分类

| 分类 | 说明 |
|------|------|
| PASS | C 和 Python 输出注解完全匹配（语义级别） |
| FAIL | C 和 Python 输出存在语义差异 |
| SKIP | 非 logic 输入解码器，无法直接测试 |
| ERROR | 解码器加载失败或运行崩溃 |
