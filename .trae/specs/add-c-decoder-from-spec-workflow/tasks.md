# Tasks

- [x] Task 1: 创建 LLM Skill `.trae/skills/c-decoder-from-spec.md`
  - [x] 1.1: 编写 Skill 元数据和触发条件
  - [x] 1.2: 编写阶段 1 — 协议分析模板（信号线表/帧格式图/状态机图/校验算法/选项列表）
  - [x] 1.3: 编写阶段 2 — 解码器设计决策树（时钟/无时钟/片选/超时/堆叠/多条件）
  - [x] 1.4: 编写阶段 3 — 解码器实现工作流（骨架→start→decode→校验→堆叠→CMakeLists）
  - [x] 1.5: 编写阶段 4 — 测试数据生成模板（BitstreamBuilder/帧生成/config.json/场景覆盖）
  - [x] 1.6: 编写阶段 5 — 验证与调试（独立验证 + Python对比验证 + 修复循环）
  - [x] 1.7: 编写质量检查清单（13 项）
  - [x] 1.8: 编写完整示例 — I2C 协议规范→i2c_c 解码器全流程演示

- [x] Task 2: 创建指南文档 `doc/c-decoder-from-spec-guide.md`
  - [x] 2.1: 编写协议分析方法论（信号线提取/帧格式分析/时序约束/校验机制/状态机建模）
  - [x] 2.2: 编写解码器设计决策树（与 Skill 一致）
  - [x] 2.3: 编写从规范到代码的映射规则（时序图→c_wait/字段表→注解/状态图→switch/校验→CRC/参数→option）
  - [x] 2.4: 编写测试数据设计方法（正常帧/错误帧/边界条件/BitstreamBuilder 用法）
  - [x] 2.5: 编写 API v4 速查表（精简版）

# Task Dependencies
- Task 2 依赖 Task 1（指南的设计决策树和映射规则与 Skill 一致）✅
