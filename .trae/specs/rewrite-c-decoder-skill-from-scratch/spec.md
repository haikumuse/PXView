# 从零创建 C 解码器工作流与 LLM Skill Spec

## Why

项目已有 215 个 C 解码器通过 API v4 完成迁移，但当前的工作流文档和 LLM Skill 都围绕"Python→C 翻译"设计。实际需求是：当需要支持一个新协议时，开发者/LLM 应该能够**从协议规范/数据手册出发**，独立设计并实现 C 解码器，然后生成测试数据验证其正确性——无需依赖 Python 解码器作为参考。

## What Changes

- **重写** `.trae/skills/c-decoder-generator.md` — 从"Python→C 翻译"改为"协议规范→C 解码器"的完整工作流
- **重写** `doc/c-decoder-guide.md` — 从"Python→C 翻译指南"改为"C 解码器独立开发指南"，聚焦从协议规范出发的设计方法
- **保留** `doc/c-decoder-testing.md` — 测试流程文档基本适用，但需补充"无 Python 参考时的独立验证"章节

## Impact

- Affected docs: `doc/c-decoder-guide.md`, `doc/c-decoder-testing.md`, `.trae/skills/c-decoder-generator.md`
- Affected specs: `write-c-decoder-guide-and-skill`（前一轮已完成但方向错误，需修正）

---

## ADDED Requirements

### Requirement: 从零创建 C 解码器的完整工作流

系统 SHALL 提供一个从协议规范到可验证 C 解码器的完整工作流，包含以下阶段：

#### Scenario: 阶段 1 — 协议分析
- **WHEN** 开发者/LLM 获得一份协议规范（PDF/文档/描述）
- **THEN** 工作流指导提取以下信息：
  1. **信号线定义**：协议使用哪些信号线（时钟、数据、片选等），每条线的方向和有效电平
  2. **帧格式**：帧的起始/结束条件、字段划分、字段位宽和字节序
  3. **时序约束**：采样时机（上升沿/下降沿）、建立/保持时间、超时条件
  4. **校验机制**：CRC/校验和/奇偶校验的计算方法
  5. **状态机**：协议的状态转换逻辑（空闲→寻址→数据→确认等）
  6. **变体和选项**：协议的可配置参数（波特率、位序、地址模式等）

#### Scenario: 阶段 2 — 解码器设计
- **WHEN** 协议分析完成
- **THEN** 工作流指导设计解码器的以下组件：
  1. **通道定义**：根据信号线定义 `srd_channel` 数组，选择正确的通道类型（SCLK/SDATA/COMMON）
  2. **选项定义**：根据协议变体定义 `srd_decoder_option` 数组
  3. **注解定义**：根据帧格式设计注解枚举和标签（长/中/短三级文本）
  4. **注解行定义**：设计注解行的分组布局
  5. **状态结构体**：设计 `C_DECODER_STATE` 结构体，包含所有解码过程中需要跟踪的变量
  6. **状态机设计**：设计 `enum state` 和 `switch/case` 结构
  7. **输出设计**：确定 `c_reg_out()` 注册的输出类型（ANN/PROTO/BINARY/META）
  8. **堆叠设计**：确定是否需要 `decode_upper` 回调来接收下层解码器的协议数据

#### Scenario: 阶段 3 — 解码器实现
- **WHEN** 解码器设计完成
- **THEN** 工作流指导按以下顺序实现代码：
  1. 编写骨架代码（通道/选项/注解/行/状态结构体/reset/start/decode/destroy/DLL入口）
  2. 实现 `start()` 回调：注册输出、读取选项、获取采样率
  3. 实现 `decode()` 回调：使用 `c_wait()` 构建状态机，用 `c_put()` 输出注解
  4. 实现校验计算（CRC/校验和）
  5. 实现 `decode_upper()` 回调（如果是堆叠解码器）
  6. 添加到 `CMakeLists.txt` 的 `C_DECODERS` 列表

#### Scenario: 阶段 4 — 测试数据生成
- **WHEN** 解码器代码编写完成
- **THEN** 工作流指导生成测试数据：
  1. 根据协议规范编写 Python 脚本生成 `input.bin` 位流
  2. 编写 `config.json` 配置文件（解码器名称、采样率、通道映射、选项）
  3. 生成覆盖协议主要场景的测试数据（正常帧、错误帧、边界条件）
  4. 如果有 Python 参考解码器，可同时生成 Python 参考输出用于对比

#### Scenario: 阶段 5 — 验证与调试
- **WHEN** 测试数据生成完成
- **THEN** 工作流指导验证解码器正确性：
  1. 构建并运行解码器
  2. 检查输出注解是否符合协议规范（帧边界、字段值、校验结果）
  3. 如果有 Python 参考解码器，运行对比测试
  4. 如果没有 Python 参考解码器，手动验证注解输出
  5. 修复偏差并重新验证

---

### Requirement: LLM Skill — 从协议规范生成 C 解码器

系统 SHALL 提供一个 LLM Skill 定义文件，指导大模型从协议规范/数据手册出发，独立创建 C 解码器。

#### Scenario: Skill 触发条件
- **WHEN** 用户要求支持新协议、创建新解码器、或提供协议规范要求实现解码器
- **THEN** Skill 被触发，启动从零创建工作流

#### Scenario: Skill 包含协议分析模板
- **WHEN** LLM 执行协议分析步骤
- **THEN** Skill 提供结构化的协议分析模板，包含：
  - 信号线表格（名称、方向、有效电平、通道类型）
  - 帧格式图（ASCII 时序图 + 字段表）
  - 状态机图（ASCII 状态转换图）
  - 校验算法描述
  - 可配置选项列表

#### Scenario: Skill 包含解码器设计决策树
- **WHEN** LLM 执行解码器设计步骤
- **THEN** Skill 提供设计决策树：
  - **有时钟线？** → 使用 `c_wait(di, CW_R(CLK), CW_END)` 在时钟沿采样
  - **无时钟线？** → 使用 `c_wait(di, CW_E(DATA), CW_END)` 在数据边沿触发
  - **有片选线？** → 在 CS 有效时才处理数据，CS 无效时 `c_wait(di, CW_L(CS), CW_END)`
  - **有超时条件？** → 使用 `CW_SKIP(max_samples)` 添加超时
  - **需要堆叠？** → 定义 `decode_upper` 回调 + `c_proto()` 输出
  - **需要向下堆叠？** → 注册 `SRD_OUTPUT_PROTO` 输出

#### Scenario: Skill 包含测试数据生成模板
- **WHEN** LLM 执行测试数据生成步骤
- **THEN** Skill 提供 Python 脚本模板，包含：
  - `BitstreamBuilder` 类：按位构建 `input.bin` 位流
  - 通道映射函数：将协议信号映射到 bit 位置
  - 协议帧生成函数：根据协议规范生成帧数据
  - `config.json` 生成函数

#### Scenario: Skill 包含质量检查清单
- **WHEN** LLM 完成 C 解码器生成
- **THEN** Skill 提供质量检查清单：
  - [ ] 通道定义与协议规范一致（名称、类型、顺序）
  - [ ] 选项定义覆盖协议变体
  - [ ] 注解覆盖协议所有字段和状态
  - [ ] 注解文本包含长/中/短三级变体
  - [ ] `c_wait()` 条件与协议时序一致
  - [ ] `c_wait()` 返回值已检查
  - [ ] 无顺序 `c_wait()` 调用（应合并 `CW_OR`）
  - [ ] `c_proto()` 以 `C_END` 结尾
  - [ ] CRC/校验计算与协议规范一致
  - [ ] 状态机覆盖协议所有状态和转换
  - [ ] 构建通过（无编译错误/警告）
  - [ ] 测试数据覆盖正常/错误/边界场景
  - [ ] 解码器输出与协议规范一致

#### Scenario: Skill 包含独立验证指导（无 Python 参考）
- **WHEN** 没有 Python 参考解码器可用
- **THEN** Skill 提供独立验证方法：
  1. 人工审查注解输出是否与协议规范描述一致
  2. 使用已知的协议捕获数据（如逻辑分析仪抓取的真实数据）验证
  3. 对比其他工具（如 Wireshark、Sigrok PulseView）的解码结果
  4. 编写协议帧级别的验证脚本，检查注解中的字段值是否符合预期

---

## MODIFIED Requirements

### Requirement: C 解码器开发指南文档（方向修正）

前一轮的 `doc/c-decoder-guide.md` 以"Python→C 翻译"为核心，需修正为"从协议规范出发的独立开发"：

- **保留**：API v4 完整参考、常见陷阱与修复方法、骨架代码模板、堆叠解码器示例
- **删除**：Python→C 翻译规则对照表（改为附录参考）
- **新增**：协议分析方法论、解码器设计决策树、从规范到代码的映射规则、测试数据设计方法

### Requirement: C 解码器测试流程文档（补充独立验证）

前一轮的 `doc/c-decoder-testing.md` 以"与 Python 对比"为核心，需补充：

- **新增**：无 Python 参考时的独立验证方法
- **新增**：基于协议规范的注解正确性检查清单
- **新增**：使用真实捕获数据验证的方法
