# C 解码器 Annotation 缺失修复计划（更新版）

## 当前状态

### 已完成 ✅
- **i2c_c.c** — 已添加 ANN_BIT、ANN_REPEAT_START、ANN_PACKET、ANN_ATK_DATA_POINT、ANN_ATK_RISE，4 行
- **hdlc_c.c** — 已添加 transfers 行
- **spi_c.c** — 已完全重写（10 annotations, 8 rows, 7 options）
- **uart_c.c** — 已完全重写（21 annotations, 13 rows），选项数组已有 9 项但 `num_options = 5`

### 待修复

---

## 步骤 1：修复 uart_c.c — 更新 num_options 和 start() 读取新选项

**现状**：`uart_options[]` 已有 9 项（baudrate, data_bits, stop_bits, parity, bit_order, format, invert_rx, invert_tx, show_data_point），但 `num_options = 5`，`uart_start()` 只读取了前 5 个。

**修改内容**：
- 将 `.num_options` 从 5 改为 9
- 在 `uart_state` 结构体中添加 `format` 字段（int，0=hex, 1=ascii, 2=dec）
- 在 `uart_start()` 中读取 `bit_order`、`format`、`invert_rx`、`invert_tx` 选项
- 在解码逻辑中使用 `bit_order_msb`（已有字段）来控制数据位顺序
- 在 `handle_data_complete()` 中根据 `format` 格式化输出
- 在采样逻辑中根据 `invert_rx`/`invert_tx` 反转信号

---

## 步骤 2：修复 i2s_c.c — 对齐 Python annotation 体系

**现状**：C 版本有 4 个 annotation（WORD, FRAME, WARN, BIT），3 行（words, frames, warnings）
**Python 版本**：3 个 annotation（left, right, warnings），5 个选项（ws_polarity, clk_edge, bit_shift, bit_align, bitorder/wordsize）

**修改内容**：
- 将 annotation 改为：ANN_LEFT(0), ANN_RIGHT(1), ANN_WARN(2), NUM_ANN=3
- 将 annotation 行改为：left-words, right-words, warnings（3 行）
- 添加 4 个选项：ws_polarity, clk_edge, bit_shift, bit_align（保留 bit_depth 和 msb_first）
- 在 `i2s_priv` 中添加选项字段
- 在 `i2s_start()` 中读取新选项
- 在 `i2s_decode()` 中根据 ws_polarity 判断左右声道，输出 ANN_LEFT/ANN_RIGHT
- 移除 ANN_FRAME 和 ANN_BIT（Python 版本没有这些）

---

## 步骤 3：修复 onewire_c.c — 添加 warnings 和 overdrive

**现状**：6 个 annotation（RESET_PRESENCE, PRESENCE, BIT, BYTE, RESET, SLOT），3 行
**Python 版本**（onewire_link）：5 个 annotation（bit, warnings, reset, presence, overdrive）

**修改内容**：
- 添加 ANN_WARN(6), ANN_OVERDRIVE(7)，NUM_ANN=8
- 添加 overdrive 选项
- 添加 warnings 行和 overdrive 行（共 5 行）
- 在解码逻辑中检测 overdrive 模式切换，输出 ANN_OVERDRIVE
- 在异常情况下输出 ANN_WARN

---

## 步骤 4：修复 graycode_c.c — 添加 5 个缺失 annotation

**现状**：2 个 annotation（code, position），2 行，2 选项，decode() 为空壳
**Python 版本**：7 个 annotation（phase, increment, count, turns, interval, average, rpm）

**修改内容**：
- 将 annotation 改为：ANN_PHASE(0), ANN_INCREMENT(1), ANN_COUNT(2), ANN_TURNS(3), ANN_INTERVAL(4), ANN_AVERAGE(5), ANN_RPM(6)，NUM_ANN=7
- 将行改为 7 行（每 annotation 一行）
- 在 `gray_code_state` 中添加计数器字段（prev_gray, prev_bin, edge_count, total_edges, last_time 等）
- 在 `gray_code_start()` 中读取 edges 和 avg_period 选项
- 实现 `gray_code_decode()`：检测边沿 → Gray→Binary 转换 → 计算增量/圈数/间隔/RPM → 输出 annotation

---

## 步骤 5：修复 ds1307_c.c — 添加寄存器级 annotation

**现状**：2 个 annotation（datetime, register），decode() 为空壳，inputs=i2c
**Python 版本**：29 个 annotation（9 reg + 15 bit + 5 action/warning）

**修改内容**：
- 添加核心 annotation：ANN_DATETIME(0), ANN_REGISTER(1), ANN_READ_DATETIME(2), ANN_WRITE_DATETIME(3), ANN_READ_REG(4), ANN_WRITE_REG(5), ANN_WARN(6)，NUM_ANN=7
- 添加行：datetime, registers, actions, warnings（4 行）
- 在 `ds1307_state` 中添加 I2C 解析状态字段
- 实现 `ds1307_decode()`：接收 I2C 上层输入 → 解析地址 0x68 → 识别读/写操作 → 输出寄存器和日期时间 annotation

---

## 步骤 6：修复 ds3231_c.c — 添加寄存器级 annotation

**现状**：2 个 annotation（datetime, register），decode() 为空壳，1 选项，inputs=i2c
**Python 版本**：20 个 annotation（6 reg + 7 bit + 7 action/warning）

**修改内容**：
- 添加核心 annotation：ANN_DATETIME(0), ANN_REGISTER(1), ANN_ALARM(2), ANN_CONTROL(3), ANN_TEMPERATURE(4), ANN_READ_DATETIME(5), ANN_WRITE_DATETIME(6), ANN_READ_ALARM(7), ANN_WRITE_ALARM(8), ANN_READ_TEMP(9), ANN_WARN(10)，NUM_ANN=11
- 添加行：datetime, registers, alarms, control, temperature, actions, warnings（7 行）
- 实现 `ds3231_decode()`：接收 I2C 上层输入 → 解析地址 0x68 → 识别寄存器区域 → 输出 annotation

---

## 步骤 7：修复 lm75_c.c — 添加 3 个缺失 annotation

**现状**：2 个 annotation（temp, register），decode() 为空壳，inputs=i2c
**Python 版本**：5 个 annotation（celsius, kelvin, text-verbose, text, warnings）

**修改内容**：
- 将 annotation 改为：ANN_CELSIUS(0), ANN_KELVIN(1), ANN_TEXT_VERBOSE(2), ANN_TEXT(3), ANN_WARN(4)，NUM_ANN=5
- 将行改为：celsius, kelvin, text, warnings（4 行）
- 在 `lm75_state` 中添加 I2C 解析状态
- 实现 `lm75_decode()`：接收 I2C 上层输入 → 解析地址 0x48/0x49/0x4A/0x4B → 读取温度寄存器 → 转换为摄氏/开尔文 → 输出 annotation

---

## 步骤 8：修复 counter_c.c — 添加 word_reset annotation 和选项

**现状**：2 个 annotation（count, edge），2 行，0 选项，decode() 为空壳
**Python 版本**：3 个 annotation（edge_count, word_count, word_reset），7 选项

**修改内容**：
- 将 annotation 改为：ANN_EDGE_COUNT(0), ANN_WORD_COUNT(1), ANN_WORD_RESET(2)，NUM_ANN=3
- 将行改为：edge_counts, word_counts, word_resets（3 行）
- 添加 7 个选项：data_edge, divider, reset_edge, edge_off, word_off, dead_cycles, start_with_reset
- 在 `counter_state` 中添加计数器字段
- 实现 `counter_decode()`：检测边沿 → 计数 → 检测 reset → 输出 annotation

---

## 步骤 9：修复 numbers_and_state_c.c — 添加 raw/enum/warning annotation 和选项

**现状**：2 个 annotation（number, state），2 行，0 选项，decode() 为空壳
**Python 版本**：35 个 annotation（raw, number, enum0-31, enumovr, warning），6 选项

**修改内容**：
- 简化 annotation 为：ANN_RAW(0), ANN_NUMBER(1), ANN_WARN(2)，NUM_ANN=3（enum slot 太多，暂不实现完整 35 个）
- 将行改为：raws, numbers, warnings（3 行）
- 添加 4 个核心选项：clkedge, count, interp, format
- 实现 `numbers_and_state_decode()`：在时钟边沿采样 → 位打包 → 格式化输出

---

## 步骤 10：修复 lin_c.c — 添加 version 选项

**现状**：11 个 annotation，2 行，1 选项（baudrate），decode() 已完整实现
**Python 版本**：4 个 annotation（data, control, error, inline_error），1 选项（version）

**修改内容**：
- 添加 version 选项（值：1 或 2，默认 2）
- 在 `lin_priv` 中添加 version 字段
- 在 `lin_start()` 中读取 version 选项
- 在 checksum 计算中根据 version 选择 classic/enhanced 校验
- annotation 体系保持不变（C 版本的字段级 annotation 比 Python 的语义分类更实用）

---

## 步骤 11：编译验证

- 增量构建所有修改的 C 解码器
- 确保无编译错误和警告

---

## 优先级排序

1. **uart_c.c** — 只需改 num_options 和读取逻辑，最小改动
2. **i2s_c.c** — annotation 体系需重构，但解码逻辑已有基础
3. **onewire_c.c** — 添加 2 个 annotation + 1 个选项，中等改动
4. **lin_c.c** — 添加 1 个选项，最小改动
5. **graycode_c.c** — 需要实现完整解码逻辑
6. **counter_c.c** — 需要实现完整解码逻辑
7. **lm75_c.c** — 需要实现 I2C 上层解码
8. **ds1307_c.c** — 需要实现 I2C 上层解码
9. **ds3231_c.c** — 需要实现 I2C 上层解码
10. **numbers_and_state_c.c** — 最复杂，需要实现位打包和格式化
