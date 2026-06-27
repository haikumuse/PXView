# 协议状态机全量逆向与修复计划 (Detailed Roadmap)

## 1. 项目上下文总结 (Context Summary)
本项目旨在将 215 个基于 Python 的协议解码器（`libsigrokdecode`）迁移至高性能的 C 语言引擎，并确保两者输出的语义完全对齐（Semantic Parity）。

目前我们构建了一套基于虚拟波形注入（Fuzzing）的自动化测试框架 `run_all_tests.py`。最新的全量测试（包含 215 个解码器）状态分布如下：
- 🟢 **PASS / DEVIATION (141个，约 65.6%)**：已完美对齐。这部分协议使用了官方的真实抓包数据（非逻辑层协议）或我们在前几个 Batch 中手工编写的高保真波形生成器（如 I2C, SPI, 乃至 Batch 4 刚解决的 `bean_c`, `carrera_c`）。
- 🟠 **WARN (54个)**：**空载输出（Vacuous）**。因为它们处于“缺省测试”状态，被喂入了随机的二进制噪声。协议严格的状态机（例如要求特定的起始字节 SOF、前导码 Preamble）直接将随机噪声丢弃，导致 Python 和 C 都输出 `0` 个标注。
- 🔴 **FAIL (15个)**：**语义偏离**。随机噪声意外触发了协议的某些边缘状态，但 C 版本在处理浮点数容差（Tolerance）或内存越界判断时，与 Python 结果不一致。
- 💥 **ERROR (5个)**：**引擎崩溃/超时**。例如 `wiegand_c`, `ook_c` 等 5 个协议，在接收不规则波形时，C 语言版本的解析引擎陷入了死循环，导致测试脚本超时（>30s）并强制终止。

---

## 2. WARN 修复策略与方法论 (Methodology for Fixing WARN)
前 4 个 Batch 中，我们将多个 WARN 状态的解码器成功转化为 PASS 或合法的 DEVIATION。我们将复用这套被验证的“波形注入”方法论来消灭剩余的 54 个 WARN：

1. **逆向分析状态机 (Reverse Engineering)**
   - 查阅原始的 Python 解码器源码（`decoders/<decoder_name>/pd.py`）。
   - 寻找 `__init__` 中定义的初始状态，并详细分析 `decode()` 中对于 `wait()` 条件的定义。
   - 提炼出触发第一个完整帧必须具备的条件（例如：电平持续特定个采样点、特定的寻址位等）。

2. **编写专属 Fuzzer 生成器**
   - 在 `tests/fuzzers/<decoder>.py` 中新建专属的生成器（继承自 `ProtocolFuzzer`）。
   - 利用框架提供的底座构建 API：`self.builder.set_level(channel, level, duration_samples)`。
   - 必须通过精确计算采样率来换算持续时间：`samples_per_bit = int(time_sec * self.samplerate)`。

3. **全帧序列注入 (Frame Injection)**
   - 构建完整的有效包：包含 前导码 (Preamble) -> 起始位 (Start) -> 数据位 (Data) -> 校验和 (CRC) -> 结束位 (EOM)。
   - 必须确保最后给出一个足够长的延迟（Idle duration），使状态机能跨越帧周期的超时阈值并输出最终结果。

4. **测试与白名单容忍 (Tolerance & Whitelisting)**
   - 在 `fuzzers/__init__.py` 中注册模块，运行 `python generate_testdata.py --overwrite` 覆写旧的随机噪声，随后运行 `python run_all_tests.py --decoder <id>` 验证。
   - 如果遇到不可避免的浮点数偏差，在 `config.json` 中配置 `"expected_deviations": true` 将其合法降级为 `DEVIATION`，从而清零 WARN。

---

## 3. 分步执行计划 (Execution Roadmap)

### Phase 1: 致命引擎修复 (Fix ERROR) - 🌟 最高优先级
必须首先解决死循环问题，否则会严重拖慢批量测试的速度。
- **目标解码器 (5个)**：`hdlc_c`, `ook_c`, `ook_oregon_c`, `ook_vis_c`, `wiegand_c`
- **操作**：直接检查上述 5 个 C 语言解码器的源码（`libsigrokdecode/c_decoders/`），重点排查 `while` 循环内部缺少包指针推进（`inc`）或者在异常分支下没有退出状态机导致的 Infinite Loop。修复后重新编译测试。

### Phase 2: 语义平齐攻坚 (Fix FAIL) - 🔴 高优先级
这 15 个解码器已经有输出，但逻辑存在偏差。
- **目标解码器 (15个)**：包括 `miller_c`, `nrzi_c`, `spi_fast_c`, `ac97_c`, `ccd_c` 等。
- **操作**：使用现有的随机数据（或稍加改造），对比 `actual_c.json` 与 `expected_py.json`。分析是否是 C 语言中处理异常包头（例如半个周期）时导致状态机错位。修复 C 源码以 100% 对齐 Python。

### Phase 3: 消除空载 (Eliminate WARN) - 🟠 中优先级
采用上述 **“WARN 修复方法论”**。54 个协议按每次 5 个（Batch）进行推进，预计需要 11 个 Batch 循环。
- **操作**：针对如 `can_fd_c`, `ethernet_c`, `mipi_dsi_c` 等协议，逐个解剖其 Python 状态机。在 `fuzzers/` 下撰写针对性的状态翻转逻辑，直到它不仅通过，还能生成成百上千个合法标注。

### Phase 4: 全量覆盖率审查 (Full Coverage Audit)
当 `run_all_tests.py --all` 不再有 WARN/FAIL/ERROR 时执行。
- 盘点那 140 个原本就是 PASS 的解码器，筛查是否有“侥幸通过”（仅发送了一个错误帧引发了极短的异常流）的情况。
- 补全这些解码器的 Fuzzer 逻辑，刻意注入（Error Injection），确保测试流覆盖了 Python 版本的所有 `if/else` 分支。

## User Review Required

> [!IMPORTANT]
> **请确认接下来的行动：**
> 计划已非常详尽并涵盖了上下文和我们前几个 Batch 积累的宝贵方法论。
> 是否立刻授权启动 **Phase 1**，由我帮您揪出导致 `wiegand_c` 等 5 个协议在 C 端死循环的底层代码 Bug？
