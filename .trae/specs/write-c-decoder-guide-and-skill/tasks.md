# Tasks

- [x] Task 1: 编写 C 解码器开发指南文档 `doc/c-decoder-guide.md`
  - [x] 1.1: 编写 API v4 完整参考（c_wait/c_put/c_proto/c_pin/di_samplenum/di_matched/c_opt_*/c_has_ch/c_samplerate/C_DECODER_STATE/C_DECODER_DEFINE/decode_upper）
  - [x] 1.2: 编写 Python→C 翻译规则对照表（wait/put/putp/matched/option/channel/samplenum 等全部映射）
  - [x] 1.3: 编写常见陷阱与修复方法（顺序c_wait/matched位掩码/hex位序/hex大小写/注解类号/多文本/C_END哨兵/reduce_bus位序等 — 实际 12 个）
  - [x] 1.4: 编写完整解码器骨架代码模板（含通道/选项/注解/状态/reset/start/decode/destroy/DLL入口）
  - [x] 1.5: 编写堆叠解码器示例（decode_upper 回调 + c_proto 发送 — 含三层堆叠示例）

- [x] Task 2: 编写 C 解码器测试流程文档 `doc/c-decoder-testing.md`
  - [x] 2.1: 编写测试数据生成流程（generate_testdata.py 用法 + 手动创建 config.json/input.bin）
  - [x] 2.2: 编写单解码器验证流程（构建→单测试→生成参考→对比→全量测试）
  - [x] 2.3: 编写测试结果解读（PASS/DEVIATION/WARN/FAIL/ERROR/SKIP 含义）
  - [x] 2.4: 编写自定义测试数据创建指南（Python 脚本示例生成 input.bin 位流 — 含 SPI 和 UART 示例）

- [x] Task 3: 编写 LLM C 解码器生成 Skill `.trae/skills/c-decoder-generator.md`
  - [x] 3.1: 编写 Skill 元数据和触发条件
  - [x] 3.2: 编写完整生成工作流（读Python→提取元数据→翻译→生成C→构建→测试→修复）
  - [x] 3.3: 编写 Python→C 翻译规则（含状态机/列表/字典/字符串/异常翻译模式）
  - [x] 3.4: 编写质量检查清单（12 项）
  - [x] 3.5: 编写调试指导（偏差定位→代码定位→条件检查→matched检查→修复循环 — 8 步）

# Task Dependencies
- Task 2 依赖 Task 1（测试文档引用 API 参考）✅
- Task 3 依赖 Task 1 和 Task 2（Skill 引用翻译规则和测试流程）✅
