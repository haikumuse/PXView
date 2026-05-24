# C解码器API完备性终审 Spec

## Why

在开始实现178个C解码器之前，需要对C解码器框架API进行最终审查，确认：API是否完备支持所有功能（含多层解码）、是否存在过时/废弃API需清理、SRD_OUTPUT_PYTHON的设计是否合理。审查发现3个关键问题和若干设计缺陷。

## What Changes

- **修复Python→C桥接崩溃**：type_decoder.c中Python解码器输出到C解码器时对NULL py_inst调用PyObject_CallMethod，会导致段错误
- **重命名SRD_OUTPUT_PYTHON为SRD_OUTPUT_PROTO**：消除"Python输出"的误导性命名，实际语义是"协议层输出用于解码器堆叠"
- **清理未使用API**：标记或移除从未被任何C解码器调用的API函数
- **修复c_decoder_put()的SRD_OUTPUT_PYTHON分支**：错误地将annotation结构体作为协议数据传递
- **添加混合堆叠保护**：srd_inst_stack()中检查C/Python混合堆叠并拒绝或警告

## Impact

- Affected code: `libsigrokdecode/type_decoder.c`, `libsigrokdecode/c_decoder_api.c`, `libsigrokdecode/libsigrokdecode.h`, `libsigrokdecode/instance.c`
- Affected decoders: 所有37个现有C解码器（重命名影响）、所有178个待实现C解码器
- **BREAKING**: SRD_OUTPUT_PYTHON重命名为SRD_OUTPUT_PROTO将影响所有C解码器的outputs数组和register_output调用

***

## ADDED Requirements

### Requirement: Python→C桥接安全处理

当Python解码器堆叠在C解码器之上时（Python→C方向），系统SHALL安全处理而非崩溃。

#### Scenario: Python i2c解码器输出到C lm75_c解码器

* **WHEN** Python i2c解码器通过SRD_OUTPUT_PYTHON输出数据
* **AND** next_di是C解码器实例（is_c_inst=TRUE）
* **THEN** 系统SHALL检查next_di->is_c_inst
* **AND** 如果is_c_inst为TRUE，跳过该next_di并输出警告日志（因为Python→C数据格式转换未实现）
* **AND** 不对NULL的py_inst调用PyObject_CallMethod

#### 当前代码问题

`type_decoder.c:567` 无条件调用 `PyObject_CallMethod(next_di->py_inst, "decode", ...)` — 当next_di是C解码器时，py_inst为NULL，导致段错误。

### Requirement: 混合堆叠保护

srd_inst_stack() SHALL 检查堆叠方向的兼容性，防止不支持的混合堆叠。

#### Scenario: 用户尝试将C解码器堆叠在Python解码器之上

* **WHEN** srd_inst_stack()被调用，di_bottom是Python解码器，di_top是C解码器
* **THEN** 返回SRD_ERR_ARG并输出错误信息"Python→C decoder stacking is not supported"

#### Scenario: 用户尝试将Python解码器堆叠在C解码器之上

* **WHEN** srd_inst_stack()被调用，di_bottom是C解码器，di_top是Python解码器
* **THEN** 返回SRD_ERR_ARG并输出错误信息"C→Python decoder stacking is not supported"

### Requirement: SRD_OUTPUT_PYTHON重命名为SRD_OUTPUT_PROTO

SRD_OUTPUT_PYTHON的命名具有误导性——它不是"输出Python对象"，而是"输出协议数据供上层解码器消费"。C解码器不使用Python对象，此命名造成概念混乱。

#### Scenario: C解码器注册协议输出

* **WHEN** C解码器调用c_decoder_register_output(di, SRD_OUTPUT_PROTO, "i2c")
* **THEN** 输出类型语义清晰——这是协议层输出，用于解码器堆叠

#### 重命名映射

| 旧名称 | 新名称 | 说明 |
|--------|--------|------|
| SRD_OUTPUT_PYTHON | SRD_OUTPUT_PROTO | 协议层输出 |
| c_decoder_put_python | c_decoder_put_proto | 输出协议数据 |

#### 向后兼容

- 保留SRD_OUTPUT_PYTHON作为SRD_OUTPUT_PROTO的别名（#define），确保Python解码器代码不受影响
- c_decoder_put_python保留为c_decoder_put_proto的别名

### Requirement: 修复c_decoder_put()的SRD_OUTPUT_PYTHON分支

c_decoder_put()是annotation输出函数，但其SRD_OUTPUT_PYTHON分支错误地将srd_c_annotation结构体作为数据传递给回调。

#### Scenario: C解码器误用c_decoder_put()输出协议数据

* **WHEN** C解码器通过c_decoder_put()向SRD_OUTPUT_PYTHON/SRD_OUTPUT_PROTO输出发送数据
* **THEN** 应返回SRD_ERR_ARG并输出错误信息，引导使用c_decoder_put_proto()

#### 当前代码问题

`c_decoder_api.c:81-90` 中SRD_OUTPUT_PYTHON分支将`ann`结构体作为pdata.data传递给回调，但回调期望的是协议数据（cmd+binary），不是annotation数据。

## MODIFIED Requirements

### Requirement: 清理未使用API

以下API已在头文件中声明但从未被任何C解码器调用，需评估是否保留：

| API | 调用次数 | 建议 |
|-----|---------|------|
| c_decoder_put_logic | 0 | **保留** — 新解码器(PCA9571/TCA6408A)将使用 |
| c_decoder_get_initial_pin | 0 | **保留** — 新解码器将使用 |
| c_decoder_get_last_samplenum | 0 | **保留** — 新解码器将使用 |
| c_cond_noedge | 0 | **保留** — Python的self.wait({'sdio': 'n'})等价功能 |
| C_ANN_PUT_TYPE | 0 | **保留** — ann_type字段已实现，新解码器可能需要 |
| C_ANN_PUT_VAL | 0 | **保留** — 数值标注功能已实现，新解码器可能需要 |

**结论**：所有未使用API均保留，因为它们对应Python解码器的实际功能，新实现的C解码器将需要这些API。但在c_decoder_utils.h中补充使用文档和示例。

### Requirement: can_c迁移到c_cond_*模式

can_c是唯一仍使用旧式c_decoder_wait()的C解码器，应迁移到c_cond_*构建器模式以保持一致性。

#### 当前代码

can_c直接调用 `c_decoder_wait(di, NULL, &samplenum, &matched)`，传入NULL条件列表表示"等待任意变化"。

#### 修正方案

使用 `c_cond_edge(b, ch)` 替代NULL条件列表，对CAN RX和TX通道分别设置边沿条件。

## REMOVED Requirements

### Requirement: Python→C数据格式转换桥接

**Reason**: C解码器依赖规则已确定——C解码器只能依赖C解码器，不依赖Python解码器。Python→C和C→Python的桥接不需要实现，只需安全拒绝混合堆叠即可。
**Migration**: 通过srd_inst_stack()中的混合堆叠保护，从架构层面杜绝混合堆叠。

***

## API完备性审查结论

### 已完备的功能（无需修改）

| 功能 | API | 状态 |
|------|-----|------|
| 注解输出 | C_ANN_PUT, C_ANN_PUT_TYPE, C_ANN_PUT_VAL | ✅ 完备 |
| 条件等待 | c_cond_new/rise/fall/high/low/edge/skip/or/wait/free | ✅ 完备 |
| 当前采样获取 | c_cond_wait_current | ✅ 完备 |
| 引脚读取 | c_decoder_get_pin, c_decoder_get_initial_pin | ✅ 完备 |
| 通道检查 | c_decoder_has_channel | ✅ 完备 |
| 采样率获取 | c_decoder_get_samplerate | ✅ 完备 |
| 选项读取 | c_decoder_get_option_int/double/string | ✅ 完备 |
| 私有数据 | c_decoder_get_private, c_decoder_set_private | ✅ 完备 |
| 二进制输出 | c_decoder_put_binary | ✅ 完备 |
| 逻辑输出 | c_decoder_put_logic | ✅ 完备 |
| 元数据输出 | c_decoder_put_meta_int/double, c_decoder_register_output_meta | ✅ 完备 |
| 输出注册 | c_decoder_register_output | ✅ 完备 |
| C→C堆叠 | c_decoder_put_python → recv_proto | ✅ 完备 |
| BITS v2格式 | per-bit ss/es时间戳 | ✅ 完备 |
| SPI DATA格式 | 17字节 | ✅ 完备 |

### 需修改的功能

| 功能 | 问题 | 修改方案 |
|------|------|---------|
| SRD_OUTPUT_PYTHON命名 | 误导性 | 重命名为SRD_OUTPUT_PROTO |
| c_decoder_put_python命名 | 误导性 | 重命名为c_decoder_put_proto |
| Python→C堆叠 | 崩溃风险 | 添加is_c_inst检查+跳过+警告 |
| C→Python堆叠 | 静默丢数据 | 添加警告日志 |
| 混合堆叠保护 | 无限制 | srd_inst_stack()拒绝混合堆叠 |
| c_decoder_put() PYTHON分支 | 数据类型错误 | 返回错误+引导使用put_proto |
| can_c旧式wait | 不一致 | 迁移到c_cond_*模式 |

### SRD_OUTPUT_PYTHON（PROTO）存在的必要性

**为什么C解码器需要SRD_OUTPUT_PROTO（原SRD_OUTPUT_PYTHON）？**

这是C解码器多层堆叠的核心机制：

1. **底层解码器输出协议数据**：i2c_c输出"START"/"STOP"/"ADDRESS READ"等命令，spi_c输出"DATA"/"CS-CHANGE"/"BITS"等命令
2. **上层解码器通过recv_proto接收**：lm75_c的recv_proto()接收i2c_c的协议数据，解析LM75温度寄存器
3. **这是C→C堆叠的唯一数据通道**：c_decoder_put_proto() → next_di->recv_proto()

**与Python解码器的对比**：
- Python解码器：`self.put(ss, es, self.output_python, data)` → Python对象 → 上层Python解码器的`decode()`
- C解码器：`c_decoder_put_proto(di, ss, es, out_id, cmd, data, len)` → 二进制数据 → 上层C解码器的`recv_proto()`

两者功能等价，但数据格式不同（Python对象 vs cmd+binary）。因此SRD_OUTPUT_PROTO是C解码器框架的必要组成部分，只是命名需要修正。

## 受影响的现有C解码器修改清单

| 解码器 | 修改内容 |
|--------|---------|
| 全部18个有SRD_OUTPUT_PYTHON的解码器 | outputs数组中"python"引用不变（字符串匹配），register_output调用改用SRD_OUTPUT_PROTO |
| c_decoder_api.c | c_decoder_put_python重命名为c_decoder_put_proto，保留别名 |
| libsigrokdecode.h | 添加SRD_OUTPUT_PROTO枚举值，SRD_OUTPUT_PYTHON改为别名 |
| type_decoder.c | 添加is_c_inst检查 |
| instance.c | srd_inst_stack()添加混合堆叠保护 |
| can_c.c | 迁移到c_cond_*模式 |
