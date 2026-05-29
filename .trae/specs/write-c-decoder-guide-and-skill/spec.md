# C 解码器开发指南与 LLM Skill Spec

## Why

项目已有 215 个 C 解码器通过 API v4 完成从 Python 的迁移，测试结果达到 204 PASS / 0 FAIL / 0 ERROR。但当前缺少：
1. 面向人类开发者的 C 解码器编写指南文档
2. 面向大模型的 C 解码器自动生成 Skill（提示词模板 + 工作流）
3. 单独为 C 解码器生成测试数据并验证的独立流程

这些基础设施将使后续新增 C 解码器的工作效率大幅提升，并确保新解码器从编写到验证的全流程可追溯。

## What Changes

- 新增 `doc/c-decoder-guide.md` — C 解码器开发完整指南（含 API 参考、Python→C 翻译规则、常见陷阱、调试技巧）
- 新增 `doc/c-decoder-testing.md` — C 解码器独立测试流程文档（含测试数据生成、单解码器验证、批量验证）
- 新增 `.trae/skills/c-decoder-generator.md` — LLM Skill 定义文件，指导大模型从 Python 解码器自动生成 C 解码器

## Impact

- Affected code: 无代码变更，纯文档和 Skill 定义
- Affected specs: `redesign-c-decoder-api-v4`（API v4 参考文档化）、`add-c-decoder-test-system`（测试流程文档化）

---

## ADDED Requirements

### Requirement: C 解码器开发指南文档

系统 SHALL 提供一份完整的 C 解码器开发指南文档 `doc/c-decoder-guide.md`，涵盖以下内容：

#### Scenario: 文档包含 API v4 完整参考
- **WHEN** 开发者查阅文档
- **THEN** 文档包含所有 v4 API 函数/宏的签名、语义、Python 等价表达，包括：
  - `c_wait()` 变参声明式等待（含 `CW_H/L/R/F/E/N/SKIP/OR/END` 宏）
  - `c_put()` / `c_put_v()` / `c_put_t()` 注解输出（含多文本变体）
  - `c_proto()` / `C_END` / `C_U8/U16/U32/U64/I8/I16/I32/I64/F64/STR/BYTES` 协议输出
  - `c_pin()` / `di_samplenum()` / `di_matched()` 快捷访问
  - `c_opt_int/str/dbl/bool()` 选项读取
  - `c_has_ch()` / `c_samplerate()` / `c_last_samplenum()` / `c_init_pin()` 辅助函数
  - `c_reg_out()` / `c_reg_meta()` 输出注册
  - `C_DECODER_STATE` / `C_DECODER_DEFINE` 宏
  - `decode_upper` 回调函数签名

#### Scenario: 文档包含 Python→C 翻译规则表
- **WHEN** 开发者需要将 Python 解码器翻译为 C
- **THEN** 文档包含完整的 Python→C 翻译对照表，覆盖：
  - `self.wait({ch: 'r'})` → `c_wait(di, CW_R(ch), CW_END)`
  - `self.wait([{0:'r'}, {0:'h',1:'f'}])` → `c_wait(di, CW_R(0), CW_OR, CW_H(0), CW_F(1), CW_END)`
  - `self.wait({'skip': n})` → `c_wait(di, CW_SKIP(n), CW_END)`
  - `self.wait({})` → `c_wait(di, CW_END)` 或 `c_wait(di, CW_SKIP(0), CW_END)`
  - `self.samplenum` → `di_samplenum(di)`
  - `self.matched` → `di_matched(di)` （整数位掩码，用 `& (1 << N)` 检查）
  - `self.put(ss, es, self.out_ann, [cls, ['text1', 'text2']])` → `c_put(di, ss, es, out_ann, cls, "text1", "text2")`
  - `self.putp(ss, es, self.out_python, ['CMD', data])` → `c_proto(di, ss, es, out_python, "CMD", C_U8(data), C_END)`
  - `self.has_channel(ch)` → `c_has_ch(di, ch)`
  - `self.option['key']` → `c_opt_int(di, "key", default)` / `c_opt_str(di, "key", default)`
  - `self.saved_samplenum` → `c_last_samplenum(di)`
  - `self.initial_pin` → `c_init_pin(di, ch)`

#### Scenario: 文档包含常见陷阱与修复方法
- **WHEN** 开发者遇到 C 解码器测试偏差
- **THEN** 文档列出已知常见陷阱及修复方法：
  - **顺序 c_wait 陷阱**：多个 `c_wait()` 调用顺序执行应合并为带 `CW_OR` 的单次调用
  - **self.matched 陷阱**：Python `self.matched` 是整数位掩码，不是元组；必须用 `& (1 << N)` 而非 `== (True, False)`
  - **CW_SKIP(0) 语义**：`c_wait(di, CW_END)` 等价 Python `self.wait({})`，立即返回当前采样
  - **c_proto 哨兵**：必须用 `C_END` 结尾，不能用裸 `NULL`
  - **hex 大小写**：Python `%x` 产生小写，C `%X` 产生大写；对齐时用 `%x`/`%llx`
  - **hex 位序**：Python `int('0b0' + bitstring, 2)` 是 MSB-first；C `val |= bit << i` 是 LSB-first；需用 `<< (cnt-1-i)` 匹配
  - **reduce_bus 位序**：Python `reversed(bus)` 使 D7=MSB；C 必须从高索引向低索引迭代
  - **注解类号**：C enum 值必须与 Python 代码中 `put()` 调用的类号完全一致
  - **多文本变体**：Python `['Long', 'Mid', 'Short']` 对应 C `c_put(di, ..., cls, "Long", "Mid", "Short")`
  - **CW_END vs CW_SKIP(0)**：`CW_END` 等价 `self.wait({})`；`CW_SKIP(0)` 也返回当前采样但语义不同

#### Scenario: 文档包含完整解码器骨架代码
- **WHEN** 开发者需要创建新的 C 解码器
- **THEN** 文档提供完整的骨架代码模板，包含：
  - 头文件包含
  - 注解枚举定义
  - 状态结构体定义
  - 通道/选项/标签/行定义
  - reset/start/decode/destroy 函数
  - `srd_c_decoder` 结构体初始化
  - DLL 入口函数

---

### Requirement: C 解码器独立测试流程文档

系统 SHALL 提供一份 C 解码器独立测试流程文档 `doc/c-decoder-testing.md`，涵盖以下内容：

#### Scenario: 文档包含测试数据生成流程
- **WHEN** 开发者需要为新 C 解码器生成测试数据
- **THEN** 文档描述完整的测试数据生成流程：
  1. 运行 `python libsigrokdecode/tests/generate_testdata.py --decoder <name>` 自动生成 `config.json` 和 `input.bin`
  2. 手动创建/修改测试数据目录结构：`testdata/<decoder_name>/default/config.json` + `input.bin`
  3. `config.json` 格式说明（decoder、samplerate、channels、options、stack 字段）
  4. `input.bin` 格式说明（按位打包的逻辑信号原始数据，每字节 8 个通道，LSB-first）

#### Scenario: 文档包含单解码器验证流程
- **WHEN** 开发者需要验证单个 C 解码器的正确性
- **THEN** 文档描述完整的验证流程：
  1. 构建：`build_incremental.cmd`（确保 DLL 已编译）
  2. 单解码器测试：`python run_all_tests.py --decoder <name>`
  3. 仅生成 C 输出：`decoder_test.exe -d <name> -t <testdata_dir> -f actual_c.json --generate-only`
  4. 仅生成 Python 参考：`decoder_test.exe -d <py_name> -t <testdata_dir> -f expected_py.json --python --generate-only`
  5. 手动对比：`python -c "import json; ..."` 对比两个 JSON 文件
  6. 全量测试：`python run_all_tests.py --all`

#### Scenario: 文档包含测试结果解读
- **WHEN** 开发者查看测试结果
- **THEN** 文档解释所有测试状态：
  - **PASS**：C 和 Python 输出完全匹配（允许 2 采样点容差和数值语义等价）
  - **DEVIATION**：有偏差但 `config.json` 中 `expected_deviations=true`
  - **WARN**：两个解码器都产生 0 注解（空输出匹配）
  - **FAIL**：注解不匹配
  - **ERROR**：解码器崩溃或超时
  - **SKIP**：`config.json` 中 `needs_upstream=true`

#### Scenario: 文档包含自定义测试数据创建指南
- **WHEN** 开发者需要为特定协议场景创建自定义测试数据
- **THEN** 文档提供 Python 脚本示例，展示如何：
  1. 根据协议时序生成 `input.bin` 位流
  2. 设置正确的 `samplerate` 和通道映射
  3. 配置解码器选项
  4. 配置堆叠解码器（stack 字段）

---

### Requirement: LLM C 解码器生成 Skill

系统 SHALL 提供一个 LLM Skill 定义文件 `.trae/skills/c-decoder-generator.md`，指导大模型从 Python 解码器自动生成 C 解码器。

#### Scenario: Skill 包含完整的生成工作流
- **WHEN** 大模型调用此 Skill
- **THEN** Skill 指导大模型按以下步骤工作：
  1. 读取 Python 解码器源码（`pd.py` + `__init__.py`）
  2. 提取元数据（通道、选项、注解、标签、行）
  3. 将 Python 解码逻辑逐行翻译为 C（遵循翻译规则表）
  4. 生成完整的 C 解码器源文件
  5. 将解码器名称添加到 `CMakeLists.txt` 的 `C_DECODERS` 列表
  6. 构建并运行单解码器测试验证
  7. 分析偏差并修复

#### Scenario: Skill 包含 Python→C 翻译规则
- **WHEN** 大模型执行翻译步骤
- **THEN** Skill 提供完整的翻译规则表（与开发指南文档中的规则一致），并额外包含：
  - 状态机翻译模式（Python `if/elif` 状态枚举 → C `switch/case`）
  - 列表操作翻译（Python `list.insert(0, x)` → C 数组 `memmove` + 赋值）
  - 字典操作翻译（Python `dict[key]` → C 结构体字段或查找函数）
  - 字符串格式化翻译（Python f-string / % → C `snprintf`）
  - 异常处理翻译（Python `try/except` → C 返回值检查）

#### Scenario: Skill 包含质量检查清单
- **WHEN** 大模型完成 C 解码器生成
- **THEN** Skill 提供质量检查清单：
  - [ ] 所有 Python `self.wait()` 调用已翻译为 `c_wait()`
  - [ ] 所有 Python `self.put()` 调用已翻译为 `c_put()`
  - [ ] 所有 Python `self.putp()` 调用已翻译为 `c_proto()` + `C_END`
  - [ ] `self.matched` 使用位掩码操作（`& (1 << N)`）而非元组比较
  - [ ] 注解类号与 Python 一致
  - [ ] 多文本变体与 Python 一致
  - [ ] hex 格式使用小写（`%x`/`%llx`）
  - [ ] hex 位序使用 MSB-first（`<< (cnt-1-i)`）
  - [ ] 无顺序 `c_wait()` 调用（应合并为 `CW_OR`）
  - [ ] `c_proto()` 以 `C_END` 结尾
  - [ ] 构建通过（无编译错误/警告）
  - [ ] 单解码器测试通过

#### Scenario: Skill 包含调试指导
- **WHEN** 大模型遇到测试偏差
- **THEN** Skill 提供系统化调试流程：
  1. 对比 `actual_c.json` 和 `expected_py.json` 找到偏差位置
  2. 根据 `(start_sample, ann_class)` 定位到 C 代码中的 `c_put()` 调用
  3. 检查文本格式、采样位置、类号是否匹配
  4. 检查 `c_wait()` 条件是否等价于 Python 的 `self.wait()` 条件
  5. 检查 `self.matched` 使用是否正确
  6. 修复后重新构建并测试
