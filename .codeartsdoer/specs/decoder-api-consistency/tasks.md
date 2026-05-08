# C Decoder API 一致性对齐 - 编码任务列表

> 基于 spec.md 和 design.md 生成，覆盖 P0 和 P1 级别全部 7 项差异对齐工作。
> 实施分三层推进：L1-数据模型层 → L2-引擎核心层 → L3-Decoder实现层。

---

## 1. L1-数据模型层：srd_c_decoder 结构体扩展（P0-2 + P1-1）

> 为 C decoder 新增 `end()` 和 `metadata()` 生命周期回调函数指针，与 Python decoder 的生命周期对齐。

- [ ] 在 `libsigrokdecode4DSL/libsigrokdecode.h` 的 `srd_c_decoder` 结构体中，在 `decode` 函数指针之后、`destroy` 函数指针之前，新增两个字段：
  - `void (*end)(void *inst);` — 解码结束回调
  - `void (*metadata)(void *inst, int key, uint64_t value);` — 元数据通知回调
- [ ] 为新增的 `end` 和 `metadata` 函数指针添加 Doxygen 注释文档，说明回调语义、参数含义、可选性及与 Python 对应关系
- [ ] 在 `srd_c_decoder` 结构体上方添加注释说明 C decoder 与 Python decoder 的 start() 调用时序差异（P2-1 文档记录）

**修改文件**：`libsigrokdecode4DSL/libsigrokdecode.h`（约第 403-406 行区域）
**依赖**：无
**验证**：编译通过，结构体布局变更正确；现有 C decoder 初始化中未设置新字段，值为 NULL/0

---

## 2. L1-数据模型层：srd_c_annotation 结构体扩展（P1-2）

> 为 C decoder 注解新增 `ann_type` 字段，使 C decoder 能表达注解类型 ID，与 Python annotations 的 3 元组对齐。

- [ ] 在 `libsigrokdecode4DSL/libsigrokdecode.h` 的 `srd_c_annotation` 结构体中，在 `ann_class` 之后新增 `int ann_type;` 字段
- [ ] 为 `ann_type` 字段添加 Doxygen 注释，说明默认值为 0（无特定类型），非零值对应协议特定显示格式标识
- [ ] 更新结构体注释，说明 `ann_type` 与 Python annotations 3 元组第一个元素的对应关系

**修改文件**：`libsigrokdecode4DSL/libsigrokdecode.h`（第 563-566 行区域）
**依赖**：无
**验证**：编译通过；`ann_type` 默认值 0 与现有 memset 初始化行为一致

---

## 3. L1-数据模型层：ann_labels 类型扩展（P1-4）

> 将 C decoder 的 `ann_labels` 从 2 元素数组扩展为 3 元素数组，增加类型 ID 维度，与 Python annotations 的 3 元组对齐。

- [ ] 在 `libsigrokdecode4DSL/libsigrokdecode.h` 的 `srd_c_decoder` 结构体中，将 `const char *(*ann_labels)[2];` 修改为 `const char *(*ann_labels)[3];`
- [ ] 添加注释说明 3 元素格式为 `{type_id, short_name, description}`，type_id 为空字符串 "" 表示默认格式

**修改文件**：`libsigrokdecode4DSL/libsigrokdecode.h`（第 390 行区域）
**依赖**：无
**验证**：编译通过

---

## 4. L1-数据模型层：SRD_C_DECODER_API_VERSION 递增

> 由于 srd_c_decoder 和 srd_c_annotation 结构体布局变更，递增 API 版本号以实现版本管控。

- [ ] 在 `libsigrokdecode4DSL/libsigrokdecode.h` 中将 `#define SRD_C_DECODER_API_VERSION 1` 修改为 `#define SRD_C_DECODER_API_VERSION 2`

**修改文件**：`libsigrokdecode4DSL/libsigrokdecode.h`（第 364 行）
**依赖**：任务 1、2、3（结构体布局变更完成后递增）
**验证**：版本号正确递增；引擎加载旧版 DLL（报告版本 1）时拒绝加载

---

## 5. L2-引擎核心层：c_decoder_wait() 补充 EITHER_EDGE 和 NO_EDGE 匹配（P0-1）

> 在 `c_decoder_wait()` 的条件匹配循环中补充 `SRD_TERM_EITHER_EDGE` 和 `SRD_TERM_NO_EDGE` 两个分支，修复枚举值已定义但实现缺失的 bug。

- [ ] 在 `libsigrokdecode4DSL/c_decoder_api.c` 的 `c_decoder_wait()` 函数中，在 `SRD_TERM_FALLING_EDGE` 分支的 `}` 之后（约第 191 行），补充两个 `else if` 分支：
  - `SRD_TERM_EITHER_EDGE`：检测 `(old_val==1 && val==0) || (old_val==0 && val==1)`
  - `SRD_TERM_NO_EDGE`：检测 `(old_val==0 && val==0) || (old_val==1 && val==1)`
- [ ] 匹配逻辑必须与 `instance.c:924-930` 中 `sample_matches()` 的实现保持一致
- [ ] 旧值 `old_val` 的获取方式与 `SRD_TERM_RISING_EDGE` 分支一致：`i > 0` 时读取前一采样点 `(i-1)` 的 bit 值，否则为 0

**修改文件**：`libsigrokdecode4DSL/c_decoder_api.c`（第 180-191 行区域之后追加）
**依赖**：无
**验证**：使用 EITHER_EDGE 和 NO_EDGE 条件的 C decoder wait 调用能正确匹配；现有 HIGH/LOW/RISING/FALLING/SKIP 条件匹配逻辑不受影响

---

## 6. L2-引擎核心层：srd_session_end() 调用 C decoder 的 end()（P0-2）

> 修改 `srd_session_end()` 和 `srd_call_sub_decoder_end()`，使其在遍历 C decoder 实例时调用 end() 回调，而非直接跳过。

- [ ] 在 `libsigrokdecode4DSL/session.c` 的 `srd_session_end()` 函数中（第 455-463 行），将 C decoder 的处理逻辑从：
  ```
  if (di->is_c_inst) {
      if (di->next_di != NULL){ ... }
      continue;
  }
  ```
  修改为：
  ```
  if (di->is_c_inst) {
      if (di->c_dec_inst->end) {
          di->c_dec_inst->end(di);
      }
      if (di->next_di != NULL){ ... }
      continue;
  }
  ```
- [ ] 在 `libsigrokdecode4DSL/session.c` 的 `srd_call_sub_decoder_end()` 函数中（第 508-513 行），同样补充 C 子 decoder 的 end() 调用：
  ```
  if (sub_dec->is_c_inst) {
      if (sub_dec->c_dec_inst->end) {
          sub_dec->c_dec_inst->end(sub_dec);
      }
      if (sub_dec->next_di != NULL){ ... }
      continue;
  }
  ```
- [ ] 两处调用均需 NULL 检查保护（`if (di->c_dec_inst->end)`），确保未实现 end 的 decoder 不崩溃

**修改文件**：`libsigrokdecode4DSL/session.c`（第 455-463 行、第 508-513 行）
**依赖**：任务 1（srd_c_decoder 结构体已有 end 函数指针）
**验证**：srd_session_end() 对 C decoder 实例调用 end() 回调；未实现 end() 的 C decoder 不崩溃；Python decoder 的 end() 调用不受影响

---

## 7. L2-引擎核心层：srd_inst_send_meta() 调用 C decoder 的 metadata()（P1-1）

> 修改 `srd_inst_send_meta()`，使其在更新 C decoder 的 samplerate 后调用 metadata() 回调通知 decoder。

- [ ] 在 `libsigrokdecode4DSL/session.c` 的 `srd_inst_send_meta()` 函数中（第 136-144 行），将 C decoder 的处理逻辑修改为：
  ```
  if (di->is_c_inst) {
      if (key == SRD_CONF_SAMPLERATE && data) {
          di->samplerate = g_variant_get_uint64(data);
          if (di->c_dec_inst->metadata) {
              di->c_dec_inst->metadata(di, key, di->samplerate);
          }
      }
      // next_di 递归逻辑保持不变
      ...
  }
  ```
- [ ] 调用顺序：先更新 `di->samplerate`，再调用 `metadata()`，确保回调内部可通过 `c_decoder_get_samplerate()` 获取已更新值
- [ ] NULL 检查保护：`if (di->c_dec_inst->metadata)`

**修改文件**：`libsigrokdecode4DSL/session.c`（第 136-144 行）
**依赖**：任务 1（srd_c_decoder 结构体已有 metadata 函数指针）
**验证**：srd_inst_send_meta() 对 C decoder 实例在更新 samplerate 后调用 metadata()；未实现 metadata() 的 C decoder 不崩溃

---

## 8. L2-引擎核心层：c_decoder_put() 传递 ann_type 并添加警告日志（P1-2 + P0-3）

> 修改 `c_decoder_put()` 的 SRD_OUTPUT_ANN 分支传递 ann_type 字段；对 SRD_OUTPUT_PYTHON 和 SRD_OUTPUT_BINARY 分支添加警告日志。

- [ ] 在 `libsigrokdecode4DSL/c_decoder_api.c` 的 `c_decoder_put()` 函数中，SRD_OUTPUT_ANN 分支（第 68-69 行）补充 `pda.ann_type = ann->ann_type;`
- [ ] 在 SRD_OUTPUT_PYTHON 分支（第 74 行之前）添加警告日志：
  ```
  _srd_err("C decoder %s: SRD_OUTPUT_PYTHON output is not fully "
           "compatible with Python decoder stack. Consider using "
           "SRD_OUTPUT_ANN instead.", di->c_dec_inst->name);
  ```
- [ ] 在 SRD_OUTPUT_BINARY 分支（第 88 行之前）添加弃用警告日志：
  ```
  _srd_err("C decoder %s: Use c_decoder_put_binary() for BINARY output "
           "instead of c_decoder_put().", di->c_dec_inst->name);
  ```

**修改文件**：`libsigrokdecode4DSL/c_decoder_api.c`（第 64-93 行区域）
**依赖**：任务 2（srd_c_annotation 结构体已有 ann_type 字段）
**验证**：c_decoder_put() 输出 SRD_OUTPUT_ANN 时正确传递 ann_type；SRD_OUTPUT_PYTHON/BINARY 使用时输出警告日志

---

## 9. L2-引擎核心层：新增 c_decoder_put_binary() 函数（P1-3）

> 新增专用二进制输出 API 函数，使用标准 `srd_proto_data_binary` 结构体传递数据，与 Python 二进制输出语义一致。

- [ ] 在 `libsigrokdecode4DSL/libsigrokdecode.h` 中 `c_decoder_put()` 声明之后，新增 `c_decoder_put_binary()` 函数声明：
  ```c
  SRD_API int c_decoder_put_binary(struct srd_decoder_inst *di,
      uint64_t start_sample, uint64_t end_sample,
      int output_id, int bin_class, uint64_t size, const unsigned char *data);
  ```
- [ ] 在 `libsigrokdecode4DSL/c_decoder_api.c` 中 `c_decoder_put()` 函数之后，新增 `c_decoder_put_binary()` 实现，使用 `srd_proto_data_binary` 结构体
- [ ] 添加 Doxygen 注释文档

**修改文件**：`libsigrokdecode4DSL/libsigrokdecode.h`、`libsigrokdecode4DSL/c_decoder_api.c`
**依赖**：任务 4（API 版本已递增）
**验证**：编译通过；c_decoder_put_binary() 对非 BINARY 类型的 output_id 返回 SRD_ERR_ARG

---

## 10. L2-引擎核心层：c_decoder_register_output() 添加 PYTHON 输出警告（P0-3）

> 对 C decoder 注册 SRD_OUTPUT_PYTHON 输出时添加警告，提示该输出无法被上层 Python decoder 正确消费。

- [ ] 在 `libsigrokdecode4DSL/c_decoder_api.c` 的 `c_decoder_register_output()` 函数中（pdo 创建之后、return 之前），添加：
  ```c
  if (output_type == SRD_OUTPUT_PYTHON) {
      _srd_err("C decoder %s: Registering SRD_OUTPUT_PYTHON output. "
               "This output type cannot be properly consumed by "
               "upper-layer Python decoders.", di->c_dec_inst->name);
  }
  ```

**修改文件**：`libsigrokdecode4DSL/c_decoder_api.c`（第 246-249 行区域）
**依赖**：无
**验证**：C decoder 注册 SRD_OUTPUT_PYTHON 输出时输出警告日志

---

## 11. L3-Decoder实现层：更新 spi_c.c 的 ann_labels 和 srd_c_annotation 初始化（P1-4 + P1-2）

> 适配 ann_labels 从 [2] 到 [3] 的类型变更，并确保 srd_c_annotation 初始化兼容新增 ann_type 字段。

- [ ] 将 `spi_ann_labels[][2]` 修改为 `spi_ann_labels[][3]`，每行在首位添加空字符串 `""` 作为类型 ID
- [ ] 检查 spi_c.c 中 srd_c_annotation 的初始化方式：当前为逐字段赋值方式（`ann.ann_class = 0; ann.ann_text = ann_texts;`），需补充 `ann.ann_type = 0;` 赋值
- [ ] 确认 `.ann_labels` 赋值无需修改（类型兼容）

**修改文件**：`libsigrokdecode4DSL/c_decoders/spi_c.c`
**依赖**：任务 2、3（结构体和 ann_labels 类型已变更）
**验证**：编译通过；spi_c 解码输出注解格式不变

---

## 12. L3-Decoder实现层：更新 i2c_c.c 的 ann_labels 和 srd_c_annotation 初始化（P1-4 + P1-2）

- [ ] 将 `i2c_ann_labels[][2]` 修改为 `i2c_ann_labels[][3]`，每行在首位添加空字符串 `""`
- [ ] 将所有 `struct srd_c_annotation ann = {ann_class, texts};` 花括号初始化修改为 C99 指定初始化器：`struct srd_c_annotation ann = {.ann_class = ann_class, .ann_text = texts};`，或改为逐字段赋值
- [ ] 确认 `.ann_labels` 赋值无需修改

**修改文件**：`libsigrokdecode4DSL/c_decoders/i2c_c.c`
**依赖**：任务 2、3
**验证**：编译通过；i2c_c 解码输出注解格式不变

---

## 13. L3-Decoder实现层：更新 uart_c.c 的 ann_labels 和 srd_c_annotation 初始化（P1-4 + P1-2）

- [ ] 将 `uart_ann_labels[][2]` 修改为 `uart_ann_labels[][3]`，每行在首位添加空字符串 `""`
- [ ] 检查 uart_c.c 中 srd_c_annotation 的初始化方式（逐字段赋值），补充 `ann.ann_type = 0;`
- [ ] 确认 `.ann_labels` 赋值无需修改

**修改文件**：`libsigrokdecode4DSL/c_decoders/uart_c.c`
**依赖**：任务 2、3
**验证**：编译通过；uart_c 解码输出注解格式不变

---

## 14. L3-Decoder实现层：更新 can_c.c 的 ann_labels 和 srd_c_annotation 初始化（P1-4 + P1-2）

- [ ] 将 `can_ann_labels[][2]` 修改为 `can_ann_labels[][3]`，每行在首位添加空字符串 `""`
- [ ] 将所有 `struct srd_c_annotation ann = {ann_class, texts};` 花括号初始化修改为 C99 指定初始化器或逐字段赋值方式
- [ ] 确认 `.ann_labels` 赋值无需修改

**修改文件**：`libsigrokdecode4DSL/c_decoders/can_c.c`
**依赖**：任务 2、3
**验证**：编译通过；can_c 解码输出注解格式不变

---

## 15. L3-Decoder实现层：更新其余 C decoder 的 ann_labels 适配（P1-4）

> 其余 C decoder（counter_c, ds1307_c, ds3231_c, graycode_c, lm75_c, numbers_and_state_c, pwm_c, seven_segment_c）的 ann_labels 均为 NULL，无需修改数组定义，但需确认编译兼容。

- [ ] 逐一检查 8 个 C decoder 的 `.ann_labels = NULL` 赋值，确认 `const char *(*)[3]` 类型与 NULL 赋值兼容
- [ ] 检查这些 decoder 中是否存在 `srd_c_annotation` 的花括号初始化，如有则改为 C99 指定初始化器或逐字段赋值

**修改文件**：`libsigrokdecode4DSL/c_decoders/counter_c.c`、`ds1307_c.c`、`ds3231_c.c`、`graycode_c.c`、`lm75_c.c`、`numbers_and_state_c.c`、`pwm_c.c`、`seven_segment_c.c`
**依赖**：任务 2、3
**验证**：所有 C decoder 编译通过

---

## 16. 编译验证与回归测试

> 全量编译验证，确保所有修改不引入编译错误或运行时回归。

- [ ] 执行全量编译：运行 `build_full.cmd`（Windows）或对应的 CMake+Ninja 构建命令，确认主程序和所有 C decoder DLL 编译通过
- [ ] 验证 C decoder DLL 版本检查：确认引擎加载新版 DLL（报告 API 版本 2）成功，旧版 DLL（报告 API 版本 1）被拒绝
- [ ] 验证现有 C decoder 功能回归：使用 DSView 分别加载 spi_c、i2c_c、uart_c、can_c decoder 进行解码测试，确认输出结果与修改前一致
- [ ] 验证 end() 回调：确认未实现 end() 的 C decoder 在 srd_session_end() 中不崩溃
- [ ] 验证 metadata() 回调：确认未实现 metadata() 的 C decoder 在 srd_inst_send_meta() 中不崩溃
- [ ] 验证 EITHER_EDGE/NO_EDGE 条件匹配：构造测试数据验证 c_decoder_wait() 对双边沿和无沿条件的匹配结果与 instance.c 中 sample_matches() 一致
- [ ] 验证 ann_type 传递：确认 c_decoder_put() 输出 SRD_OUTPUT_ANN 时 pda.ann_type 正确传递（默认值 0）
- [ ] 验证警告日志：确认 C decoder 使用 SRD_OUTPUT_PYTHON 或 SRD_OUTPUT_BINARY 时输出警告日志

---

## 任务依赖关系图

```
任务1 ─┬─→ 任务4（API版本递增）
任务2 ─┤
任务3 ─┘

任务1 ──→ 任务6（srd_session_end调用end）
任务1 ──→ 任务7（srd_inst_send_meta调用metadata）

任务5（c_decoder_wait补条件）— 独立，无依赖

任务2 ──→ 任务8（c_decoder_put传ann_type+警告）
任务4 ──→ 任务9（c_decoder_put_binary新增函数）

任务10（register_output PYTHON警告）— 独立，无依赖

任务2+3 ──→ 任务11（spi_c适配）
任务2+3 ──→ 任务12（i2c_c适配）
任务2+3 ──→ 任务13（uart_c适配）
任务2+3 ──→ 任务14（can_c适配）
任务2+3 ──→ 任务15（其余decoder适配）

任务1-15 ──→ 任务16（编译验证与回归测试）
```

## 覆盖情况

| 差异编号 | 优先级 | 差异点 | 覆盖任务 |
|----------|--------|--------|----------|
| P0-1 | 必须对齐 | c_decoder_wait()缺EITHER_EDGE/NO_EDGE | 任务5 |
| P0-2 | 必须对齐 | C decoder缺end()回调 | 任务1、6 |
| P0-3 | 必须对齐 | C→Python栈SRD_OUTPUT_PYTHON类型不匹配 | 任务8、10 |
| P1-1 | 建议对齐 | C decoder缺metadata()回调 | 任务1、7 |
| P1-2 | 建议对齐 | srd_c_annotation缺ann_type字段 | 任务2、8、11-15 |
| P1-3 | 建议对齐 | SRD_OUTPUT_BINARY输出结构不标准 | 任务9 |
| P1-4 | 建议对齐 | 注解标签缺类型ID | 任务3、11-15 |
