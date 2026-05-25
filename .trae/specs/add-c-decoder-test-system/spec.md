# C解码器形式化测试系统 Spec

## Why
项目中有215个从Python移植的C解码器（对应222个Python解码器），目前没有任何自动化测试手段来验证C解码器的解码结果是否与Python版本一致。现有的`align-all-c-decoders-with-python` spec仅通过人工审查源码对齐，无法保证运行时行为正确。需要一个形式化测试系统，自动运行C解码器和Python解码器处理相同的输入数据，比对输出注解，发现差异。215个解码器的规模要求测试系统必须面向自动化和可扩展性设计，不能依赖手动创建测试数据。

## What Changes
- 新增独立C测试程序 `libsigrokdecode/tests/decoder_test.c`，通过 libsigrokdecode API 加载C解码器DLL、送入测试数据、收集注解输出
- 新增Python测试数据生成脚本 `libsigrokdecode/tests/generate_testdata.py`，自动从Python解码器元数据生成测试配置和输入数据
- 新增Python参考输出脚本 `libsigrokdecode/tests/run_python_decoder.py`，运行Python解码器获取参考输出
- 新增批量测试运行脚本 `libsigrokdecode/tests/run_tests.sh`，批量执行所有解码器测试并输出报告
- CMake中添加测试构建目标
- 新增首批4个解码器的手工验证测试数据，用于验证测试框架本身

## Impact
- Affected code: `libsigrokdecode/` (新增测试文件，不修改现有代码)
- Affected specs: `align-all-c-decoders-with-python` (测试系统验证该spec的对齐结果)
- Dependencies: libsigrokdecode API、glib-2.0、Python3 (用于生成参考输出和测试数据)

## ADDED Requirements

### Requirement: 独立C测试程序
系统 SHALL 提供一个独立C程序，能够加载C解码器DLL、送入位打包逻辑数据、收集SRD_OUTPUT_ANN注解输出到结构化列表。

#### Scenario: 加载C解码器并解码
- **WHEN** 测试程序指定C解码器名称和测试数据文件
- **THEN** 程序加载对应DLL、创建解码器实例、送入数据、收集所有注解到JSON文件

#### Scenario: 解码器加载失败
- **WHEN** 指定的C解码器DLL不存在或API版本不匹配
- **THEN** 程序输出错误信息并以非零退出码退出

### Requirement: 测试数据格式
系统 SHALL 定义标准测试数据格式，包含输入信号描述和解码器配置。

#### Scenario: 测试数据文件格式
- **WHEN** 测试程序读取测试数据目录
- **THEN** 解析 `config.json`（解码器名称、通道映射、选项、采样率）和 `input.bin`（按位打包的逻辑信号原始数据）

#### Scenario: 多测试用例支持
- **WHEN** 同一解码器有多个测试场景（如SPI不同CPOL/CPHA组合）
- **THEN** 每个场景存放在独立子目录中（如 `spi_c/cpol0_cpha0/`、`spi_c/cpol1_cpha1/`）

### Requirement: 自动化测试数据生成
系统 SHALL 提供Python脚本，自动为所有215个C解码器生成测试配置和输入数据。

#### Scenario: 从解码器元数据自动生成config.json
- **WHEN** 运行 `generate_testdata.py`
- **THEN** 脚本扫描所有C解码器DLL，读取其通道定义和选项定义，自动生成 `config.json`（包含默认通道映射和默认选项值）

#### Scenario: 为直接读取逻辑信号的解码器生成input.bin
- **WHEN** C解码器的inputs包含"logic"
- **THEN** 脚本生成包含基本信号模式的 `input.bin`（如交替0/1、全0、全1、随机数据）

#### Scenario: 跳过需要上游解码器输入的解码器
- **WHEN** C解码器的inputs不包含"logic"（如inputs=["spi"]的解码器）
- **THEN** 脚本标记该解码器为"需要堆叠测试"，生成占位config但不生成input.bin

### Requirement: Python参考输出
系统 SHALL 提供Python脚本，对相同输入数据运行Python解码器并收集注解输出到相同JSON格式。

#### Scenario: 生成Python参考输出
- **WHEN** 运行 `run_python_decoder.py` 指定解码器名称和测试数据
- **THEN** 脚本通过sigrok Python API运行Python解码器，输出 `expected.json`

#### Scenario: 批量生成所有参考输出
- **WHEN** 运行 `run_python_decoder.py --all`
- **THEN** 脚本遍历所有有input.bin的测试数据目录，批量生成expected.json

### Requirement: 注解比对
系统 SHALL 比较C解码器输出与Python参考输出，报告差异。

#### Scenario: 注解完全匹配
- **WHEN** C解码器输出的注解类别、样本范围、文本内容与Python参考输出完全一致
- **THEN** 报告 PASS

#### Scenario: 注解存在差异
- **WHEN** C解码器输出的注解与Python参考输出在类别、样本范围或文本内容上存在差异
- **THEN** 报告 FAIL，列出每个差异的详细信息（样本号、期望值、实际值）

#### Scenario: 注解数量不同
- **WHEN** C解码器输出的注解数量与Python参考输出不同
- **THEN** 报告 FAIL，标明缺少或多余的注解

#### Scenario: 容差比对模式
- **WHEN** 使用 `--tolerance N` 参数
- **THEN** 样本范围差异在N个采样点以内视为匹配（处理C/Python解码器在边沿检测上的微小差异）

### Requirement: 批量测试运行
系统 SHALL 提供脚本批量运行所有解码器测试并生成汇总报告。

#### Scenario: 运行全部测试
- **WHEN** 执行 `run_tests.sh`
- **THEN** 对每个有测试数据的解码器运行C解码器测试，与参考输出比对，输出 PASS/FAIL 汇总表

#### Scenario: 运行单个解码器测试
- **WHEN** 执行 `run_tests.sh spi_c`
- **THEN** 仅运行spi_c解码器的测试

#### Scenario: 汇总报告
- **WHEN** 所有测试运行完毕
- **THEN** 输出统计信息：总测试数、PASS数、FAIL数、SKIP数（无测试数据的解码器），以及FAIL测试的差异详情

### Requirement: CMake集成
系统 SHALL 在CMake中添加测试构建目标，使测试程序可随项目一起构建。

#### Scenario: 构建测试程序
- **WHEN** 执行 `cmake --build . --target decoder_test`
- **THEN** 编译 `decoder_test.c` 并链接 libsigrokdecode，生成可执行文件

## MODIFIED Requirements

无修改的需求。

## REMOVED Requirements

无移除的需求。

---

## 技术方案概述

### 规模挑战与应对策略

215个C解码器的规模意味着：
1. **不能手动创建所有测试数据** — 需要自动化生成
2. **不能假设所有解码器结构相同** — 需要从DLL元数据自动提取通道/选项定义
3. **部分解码器有不同输入源** — C版本直接读逻辑信号，Python版本从上游解码器接收（如4b5b_c、lin_c），需分类处理
4. **测试数据质量参差** — 自动生成的通用输入数据只能验证解码器不崩溃，高质量测试需逐步补充

### 解码器分类

| 类别 | 数量(估) | 输入源 | 测试策略 |
|------|---------|--------|---------|
| 直接读逻辑信号 | ~150+ | inputs=["logic"] | 自动生成input.bin + Python参考 |
| 需上游解码器 | ~50+ | inputs=["spi"/"i2c"/"uart"等] | 标记为SKIP，后续支持堆叠测试 |
| 纯直通/转换 | ~10+ | inputs=["logic"] | 简单输入即可验证 |

### 测试程序核心流程

```
1. 解析命令行参数（解码器名、测试数据路径、容差）
2. 读取 config.json（通道映射、选项、采样率）
3. 读取 input.bin（位打包逻辑数据）
4. srd_init() + srd_c_decoder_load()
5. srd_session_new()
6. srd_inst_new(sess, decoder_name, options)
7. srd_inst_channel_set_all(di, channel_map)
8. srd_pd_output_callback_add(sess, SRD_OUTPUT_ANN, collect_callback, &ann_list)
9. srd_session_metadata_set(sess, SRD_CONF_SAMPLERATE, ...)
10. srd_session_start(sess)
11. srd_session_send(sess, 0, sample_count, inbuf, inbuf_const, sample_count, &error)
12. srd_session_end(sess)
13. 将 ann_list 序列化为 actual.json
14. 与 expected.json 比对，输出结果
15. srd_session_destroy(sess) + srd_exit()
```

### 注解JSON格式

```json
{
  "decoder": "spi_c",
  "input_file": "input.bin",
  "samplerate": 1000000,
  "annotations": [
    {
      "sample_range": [100, 200],
      "ann_class": 0,
      "ann_type": 7,
      "texts": ["0x53", "S", "MOSI: 0x53"]
    }
  ]
}
```

### 测试数据目录结构

```
libsigrokdecode/tests/testdata/
├── spi_c/
│   ├── default/              # 默认选项测试
│   │   ├── config.json       # 通道映射、选项、采样率
│   │   ├── input.bin         # 位打包逻辑数据
│   │   └── expected.json     # Python参考输出
│   └── cpol1_cpha1/          # 不同选项组合测试
│       ├── config.json
│       ├── input.bin
│       └── expected.json
├── i2c_c/
│   └── default/
│       ├── config.json
│       ├── input.bin
│       └── expected.json
├── uart_c/
│   └── default/
│       ├── config.json
│       ├── input.bin
│       └── expected.json
├── can_c/
│   └── default/
│       ├── config.json
│       ├── input.bin
│       └── expected.json
└── ...（215个解码器目录）
```

### 首批手工验证测试数据（4个）

选择4个已有标准模板的解码器作为首批测试目标，手工构造高质量测试数据用于验证测试框架本身：
1. **spi_c** — 10个annotation类，CS-CHANGE，ATK颜色
2. **i2c_c** — 13个annotation类，START/STOP/ACK/NACK
3. **uart_c** — 21个annotation类，多级文本，ATK点注解
4. **can_c** — 严重问题清单中的重点修复对象

### 自动化测试数据生成流程

```
1. 扫描 build.dir/decoders/c_decoders/ 下所有DLL
2. 加载每个DLL，读取 srd_c_decoder 结构体元数据
3. 提取 channels、optional_channels、options、inputs
4. 对 inputs 包含 "logic" 的解码器：
   a. 生成 config.json（默认通道映射 + 默认选项 + 1MHz采样率）
   b. 生成 input.bin（基本信号模式：交替0/1、随机数据等）
   c. 运行Python解码器生成 expected.json
5. 对 inputs 不包含 "logic" 的解码器：
   a. 生成 config.json（标记 needs_upstream=true）
   b. 不生成 input.bin 和 expected.json
```

### 可行性评估结论

**完全可行**，理由如下：

1. **API层面**：libsigrokdecode 提供了完整的C API（`srd_session_send`、`srd_pd_output_callback_add`等），无需GUI即可运行解码器
2. **数据格式**：位打包逻辑数据格式简单明确，可直接程序生成
3. **C解码器独立加载**：`srd_c_decoder_load()` 可加载单个DLL，不依赖Python解释器
4. **注解收集**：`SRD_OUTPUT_ANN` 回调可获取完整的注解数据（类别、范围、文本、数值）
5. **Python参考**：sigrok的Python解码器可通过Python API独立运行，生成参考输出
6. **无侵入性**：测试系统完全独立于现有代码，不修改任何生产代码
7. **可扩展性**：自动化生成脚本可处理215个解码器的规模，手工数据仅用于框架验证

**限制**：
- C解码器和Python解码器不能混合堆叠，因此只能测试顶层解码器（直接读取逻辑信号的解码器）
- 协议输出（SRD_OUTPUT_PROTO/SRD_OUTPUT_PYTHON）格式在C和Python间不同，比对时需注意格式转换
- 部分解码器（如4b5b_c、lin_c）的输入源不同（C直接读逻辑信号，Python从上游解码器接收），需为C版本生成独立的测试数据
- 自动生成的通用输入数据只能验证解码器不崩溃和基本输出格式，协议语义正确性需要高质量手工数据逐步补充
