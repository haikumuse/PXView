# C 解码器 API 对齐 Python API Spec

## Why
C 解码器 API 与 Python 解码器 API 存在多处不一致和缺失，导致 C 解码器无法完整实现 Python 版本的功能。需要以 Python API 为准，补齐 C API 的缺失功能，修复不一致之处。

## What Changes
- 在 `srd_c_annotation` 中添加 `str_number_hex` 和 `numberic_value` 字段，对齐 `srd_proto_data_annotation`
- 修改 `c_decoder_put()` 的 `SRD_OUTPUT_ANN` 路径，填充 `str_number_hex` 和 `numberic_value`
- 修改 `c_decoder_register_output()` 添加 `meta` 参数支持 `SRD_OUTPUT_META` 类型注册
- 修改 `srd_c_decoder_register()` 修复 `ann_type` 丢失问题（当前全部设为 0）
- 在 `srd_c_decoder` 的 `ann_labels` 中添加第 0 列（type 字符串）的处理
- 修改 `c_decoder_put()` 的 `SRD_OUTPUT_PYTHON` 路径，支持 C→Python 堆叠
- 添加 C→C 堆叠支持（C 解码器的 OUTPUT_PYTHON 输出传递给上层 C 解码器）
- 添加 `c_decoder_get_last_samplenum()` API
- 修改 `c_decoder_has_channel()` 修复未使用可选通道返回值不一致（Python 返回 0xff，C 返回 0）
- 添加 `c_decoder_put_meta()` 专用 API 用于输出 META 数据

## Impact
- Affected code:
  - `libsigrokdecode/libsigrokdecode.h` — `srd_c_annotation` 结构体、`srd_c_decoder` 结构体、新增 API 声明
  - `libsigrokdecode/c_decoder_api.c` — `c_decoder_put()`、`c_decoder_register_output()`、新增 API 实现
  - `libsigrokdecode/decoder.c` — `srd_c_decoder_register()` 中 `ann_type` 处理
  - `libsigrokdecode/instance.c` — C→C 堆叠数据传递机制
  - `libsigrokdecode/type_decoder.c` — C→Python 堆叠桥接
  - 所有 C 解码器 `.c` 文件 — `ann_labels` 格式可能需要更新

## ADDED Requirements

### Requirement: C annotation 支持数值字段
C 解码器的 annotation 输出应当支持与 Python 相同的数值字段（`str_number_hex` 和 `numberic_value`），用于 UI 显示十六进制数值和搜索。

#### Scenario: C 解码器输出带数值的 annotation
- **WHEN** C 解码器调用 `C_ANN_PUT_VAL(di, ss, es, out_id, cls, numeric_val, "text1", "text2")`
- **THEN** `srd_proto_data_annotation` 的 `str_number_hex` 被填充为数值的十六进制字符串，`numberic_value` 被填充为数值本身

#### Scenario: C 解码器输出不带数值的 annotation（向后兼容）
- **WHEN** C 解码器调用现有的 `C_ANN_PUT(di, ss, es, out_id, cls, "text1")`
- **THEN** `str_number_hex` 为空字符串，`numberic_value` 为 0，行为与当前一致

### Requirement: C annotation type 保留颜色/显示类型信息
C 解码器的 annotation 应当保留 Python 3 元素 annotation 元组中的 type 信息，用于 UI 区分不同 annotation 的颜色和显示样式。

#### Scenario: C 解码器定义带 type 的 annotation
- **WHEN** C 解码器的 `ann_labels` 第 0 列包含非空 type 字符串（如 `"data"`, `"warning"`）
- **THEN** `srd_c_decoder_register()` 应将该 type 字符串解析为整数，存入 `d->ann_types` 列表，而非统一设为顺序索引

#### Scenario: C 解码器定义不带 type 的 annotation
- **WHEN** C 解码器的 `ann_labels` 第 0 列为空字符串 `""`
- **THEN** `srd_c_decoder_register()` 使用顺序索引作为 `ann_type`，与当前行为一致

### Requirement: C 解码器支持 SRD_OUTPUT_META 类型注册
C 解码器应当支持注册带类型信息的 META 输出，与 Python 的 `self.register(srd.OUTPUT_META, meta=(int, 'name', 'desc'))` 对齐。

#### Scenario: C 解码器注册 META 输出
- **WHEN** C 解码器调用 `c_decoder_register_output_meta(di, "proto_id", SRD_META_INT, "name", "description")`
- **THEN** 创建的 `srd_pd_output` 包含 `meta_type`、`meta_name`、`meta_descr` 字段，与 Python 注册的 META 输出格式一致

#### Scenario: C 解码器输出 META 数据
- **WHEN** C 解码器调用 `c_decoder_put_meta_int(di, ss, es, out_id, 42)` 或 `c_decoder_put_meta_double(di, ss, es, out_id, 3.14)`
- **THEN** META 数据被正确传递给注册的回调函数

### Requirement: C→Python 堆叠支持
C 解码器的 `SRD_OUTPUT_PYTHON` 输出应当能正确传递给上层 Python 解码器。

#### Scenario: C 解码器输出被 Python 上层解码器接收
- **WHEN** C 解码器（如 I2C C）调用 `c_decoder_put_python(di, ss, es, out_id, cmd_str, data_byte)` 输出协议数据
- **THEN** 上层 Python 解码器（如 ds1307 Python）的 `decode(self, ss, es, data)` 方法被调用，`data` 为正确的 Python 元组

### Requirement: C→C 堆叠支持
C 解码器应当支持将 `SRD_OUTPUT_PYTHON` 输出传递给上层 C 解码器。

#### Scenario: C 解码器输出被上层 C 解码器接收
- **WHEN** C 解码器（如 I2C C）输出协议数据，上层堆叠了 C 解码器（如 ds1307 C）
- **THEN** 上层 C 解码器的 `decode()` 方法被调用，接收到协议数据

### Requirement: C 解码器获取 last_samplenum
C 解码器的 `end()` 回调应当能获取 `last_samplenum`，与 Python 的 `self.last_samplenum` 对齐。

#### Scenario: C 解码器在 end() 中获取 last_samplenum
- **WHEN** C 解码器的 `end()` 被调用
- **THEN** 可通过 `c_decoder_get_last_samplenum(di)` 获取最后一个样本号

### Requirement: 未使用可选通道的返回值对齐 Python
C 解码器的 `c_decoder_get_pin()` 对未使用的可选通道应当返回与 Python 一致的值。

#### Scenario: 查询未使用的可选通道
- **WHEN** C 解码器调用 `c_decoder_get_pin(di, ch, samplenum)` 且通道 `ch` 未被用户分配
- **THEN** 返回 `0xFF`（与 Python 一致），而非当前的 `0`

## MODIFIED Requirements

### Requirement: srd_c_annotation 结构体
原定义：`srd_c_annotation` 仅包含 `ann_class`、`ann_type`、`ann_text` 三个字段。
修改为：新增 `str_number_hex[DECODE_NUM_HEX_MAX_LEN]` 和 `numberic_value` 两个字段，对齐 `srd_proto_data_annotation`。

### Requirement: c_decoder_put() SRD_OUTPUT_ANN 路径
原逻辑：仅填充 `ann_class`、`ann_type`、`ann_text`，`str_number_hex` 和 `numberic_value` 始终为 0。
修改为：从 `srd_c_annotation` 中读取 `str_number_hex` 和 `numberic_value`，填充到 `srd_proto_data_annotation`。

### Requirement: c_decoder_register_output() 
原逻辑：仅接受 `output_type` 和 `proto_id` 两个参数，不支持 META 类型信息。
修改为：新增 `c_decoder_register_output_meta()` 函数，接受 `meta_type`、`meta_name`、`meta_descr` 参数。原 `c_decoder_register_output()` 保持不变以保持向后兼容。

### Requirement: srd_c_decoder_register() ann_type 处理
原逻辑：所有 `ann_type` 统一设为顺序索引 `GINT_TO_POINTER(i)`，丢弃 `ann_labels[i][0]` 中的 type 信息。
修改为：检查 `ann_labels[i][0]` 是否为非空字符串，如果是则解析为整数作为 `ann_type`；如果为空则使用顺序索引。

### Requirement: c_decoder_put() SRD_OUTPUT_PYTHON 路径
原逻辑：仅打印警告，将数据降级为 ANN 输出，不传递给堆叠的上层解码器。
修改为：将 C 协议数据转换为 Python 对象后，调用上层 Python 解码器的 `decode()` 方法；同时支持传递给上层 C 解码器。

### Requirement: c_decoder_has_channel() / c_decoder_get_pin() 未使用通道处理
原逻辑：`c_decoder_get_pin()` 对未映射通道返回 `0`。
修改为：`c_decoder_get_pin()` 对未映射的可选通道返回 `0xFF`，对未映射的必需通道返回 `0`。

## REMOVED Requirements

无移除的需求。
