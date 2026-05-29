# 从零创建 C 解码器工作流与 LLM Skill Spec

## Why

项目已有 Python→C 翻译的工作流和 Skill，但当需要支持一个全新协议（无现有 Python 解码器）时，开发者/LLM 需要从协议规范/数据手册出发，独立设计并实现 C 解码器，然后生成测试数据验证其正确性。当前缺少这条"从零创建"的工作流。

## What Changes

- 新增 `.trae/skills/c-decoder-from-spec.md` — LLM Skill：从协议规范出发创建 C 解码器
- 新增 `doc/c-decoder-from-spec-guide.md` — 从零创建 C 解码器的完整指南

## Impact

- Affected docs: 新增 2 个文件，不修改任何现有文件
- Affected specs: 无（独立于 `write-c-decoder-guide-and-skill`）

---

## ADDED Requirements

### Requirement: 从零创建 C 解码器的 LLM Skill

系统 SHALL 新增一个 LLM Skill 文件 `.trae/skills/c-decoder-from-spec.md`，指导大模型从协议规范/数据手册出发，独立创建 C 解码器。

#### Scenario: Skill 触发条件
- **WHEN** 用户要求支持新协议、从协议规范创建解码器、或提供数据手册要求实现解码器
- **THEN** Skill 被触发，启动从零创建工作流（而非 Python→C 翻译工作流）

#### Scenario: Skill 包含 5 阶段完整工作流
- **WHEN** Skill 被触发
- **THEN** 指导按以下 5 个阶段执行：
  1. **协议分析** — 从规范提取信号线/帧格式/时序/校验/状态机/选项
  2. **解码器设计** — 设计决策树 + 组件设计（通道/选项/注解/状态结构体/状态机/输出/堆叠）
  3. **解码器实现** — 骨架→start→decode→校验→堆叠→CMakeLists
  4. **测试数据生成** — BitstreamBuilder + 帧生成 + config.json
  5. **验证与调试** — 独立验证 + Python 对比验证（如有）+ 修复循环

#### Scenario: Skill 包含协议分析模板
- **WHEN** LLM 执行协议分析步骤
- **THEN** Skill 提供结构化模板：
  - 信号线表格（名称、方向、有效电平、通道类型 SCLK/SDATA/COMMON）
  - 帧格式图（ASCII 时序图 + 字段表：名称、位宽、字节序、描述）
  - 状态机图（ASCII 状态转换图 + 转换条件表）
  - 校验算法描述（CRC 多项式/初始值/反转/异或输出）
  - 可配置选项列表（名称、类型、默认值、可选值）

#### Scenario: Skill 包含解码器设计决策树
- **WHEN** LLM 执行解码器设计步骤
- **THEN** Skill 提供决策树：
  - **有时钟线？** → `c_wait(di, CW_R(CLK), CW_END)` 在时钟沿采样
  - **无时钟线？** → `c_wait(di, CW_E(DATA), CW_END)` 在数据边沿触发
  - **有片选线？** → CS 有效时处理数据，CS 无效时 `c_wait(di, CW_L(CS), CW_END)`
  - **有超时条件？** → `CW_SKIP(max_samples)` 添加超时
  - **需要向上堆叠？** → 定义 `decode_upper` 回调 + `c_proto()` 输出
  - **需要向下堆叠？** → 注册 `SRD_OUTPUT_PROTO` 输出
  - **多条件组合？** → `CW_OR` 合并条件组

#### Scenario: Skill 包含测试数据生成模板
- **WHEN** LLM 执行测试数据生成步骤
- **THEN** Skill 提供 Python 脚本模板：
  - `BitstreamBuilder` 类：按位构建 `input.bin`（set_bit/set_bits/add_samples/save）
  - 通道映射函数：将协议信号映射到 bit 位置
  - 协议帧生成函数：根据协议规范生成帧数据
  - `config.json` 生成函数
  - 覆盖场景：正常帧、错误帧、边界条件

#### Scenario: Skill 包含独立验证指导
- **WHEN** 没有 Python 参考解码器可用
- **THEN** Skill 提供独立验证方法：
  1. 人工审查注解输出是否与协议规范描述一致
  2. 使用已知协议捕获数据（逻辑分析仪真实数据）验证
  3. 对比其他工具（Wireshark/PulseView）的解码结果
  4. 编写协议帧级别验证脚本，检查注解中字段值是否符合预期

#### Scenario: Skill 包含质量检查清单
- **WHEN** LLM 完成 C 解码器生成
- **THEN** Skill 提供至少 13 项检查：
  - 通道定义与协议规范一致
  - 选项定义覆盖协议变体
  - 注解覆盖协议所有字段和状态
  - 注解文本包含长/中/短三级变体
  - `c_wait()` 条件与协议时序一致
  - `c_wait()` 返回值已检查
  - 无顺序 `c_wait()` 调用
  - `c_proto()` 以 `C_END` 结尾
  - CRC/校验计算与协议规范一致
  - 状态机覆盖协议所有状态和转换
  - 构建通过
  - 测试数据覆盖正常/错误/边界场景
  - 解码器输出与协议规范一致

#### Scenario: Skill 包含完整示例
- **WHEN** LLM 需要参考如何使用此工作流
- **THEN** Skill 包含从 I2C 协议规范出发创建 i2c_c 解码器的全流程演示

---

### Requirement: 从零创建 C 解码器指南文档

系统 SHALL 新增一份指南文档 `doc/c-decoder-from-spec-guide.md`，面向人类开发者。

#### Scenario: 文档包含协议分析方法论
- **WHEN** 开发者需要分析协议规范
- **THEN** 文档提供系统化的分析方法：
  - 信号线提取方法（如何从时序图识别时钟/数据/控制线）
  - 帧格式分析方法（如何从帧结构图提取字段定义）
  - 时序约束提取方法（如何确定采样时机和等待条件）
  - 校验机制提取方法（如何从规范获取 CRC 参数）
  - 状态机建模方法（如何从协议描述构建状态转换图）

#### Scenario: 文档包含解码器设计决策树
- **WHEN** 开发者需要设计解码器架构
- **THEN** 文档提供与 Skill 一致的设计决策树

#### Scenario: 文档包含从规范到代码的映射规则
- **WHEN** 开发者需要将协议规范转化为代码
- **THEN** 文档提供映射规则：
  - 规范中的时序图 → `c_wait()` 条件
  - 规范中的字段表 → 注解枚举 + `c_put()` 调用
  - 规范中的状态图 → `enum state` + `switch/case`
  - 规范中的校验算法 → CRC/校验和 C 实现
  - 规范中的可配置参数 → `srd_decoder_option` 定义

#### Scenario: 文档包含测试数据设计方法
- **WHEN** 开发者需要设计测试数据
- **THEN** 文档提供方法：
  - 如何根据协议规范确定正常帧的测试向量
  - 如何设计错误帧测试（CRC 错误、格式错误、超时）
  - 如何设计边界条件测试（最小/最大帧长度、全 0/全 1 数据）
  - 如何使用 BitstreamBuilder 生成 `input.bin`

#### Scenario: 文档包含 API v4 速查
- **WHEN** 开发者需要查阅 API
- **THEN** 文档包含 API v4 速查表（精简版，详细版参考 `c-decoder-guide.md`）
