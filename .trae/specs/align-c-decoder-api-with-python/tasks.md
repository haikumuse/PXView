# Tasks

- [x] Task 1: 在 `srd_c_annotation` 中添加 `str_number_hex` 和 `numberic_value` 字段，修改 `c_decoder_put()` ANN 路径填充这些字段
  - [x] SubTask 1.1: 在 `libsigrokdecode.h` 的 `srd_c_annotation` 结构体中添加 `char str_number_hex[DECODE_NUM_HEX_MAX_LEN]` 和 `long long numberic_value` 字段
  - [x] SubTask 1.2: 在 `c_decoder_api.c` 的 `c_decoder_put()` SRD_OUTPUT_ANN 路径中，将 `srd_c_annotation` 的 `str_number_hex` 和 `numberic_value` 复制到 `srd_proto_data_annotation`
  - [x] SubTask 1.3: 添加 `C_ANN_PUT_VAL` 宏，支持传入数值参数
  - [x] SubTask 1.4: 更新现有 `C_ANN_PUT` 和 `C_ANN_PUT_TYPE` 宏，初始化新字段为 0

- [x] Task 2: 修复 `srd_c_decoder_register()` 中 `ann_type` 丢失问题
  - [x] SubTask 2.1: 修改 `decoder.c` 中 `srd_c_decoder_register()` 的 annotation 注册逻辑，检查 `ann_labels[i][0]` 是否为非空字符串
  - [x] SubTask 2.2: 如果 `ann_labels[i][0]` 非空，将其解析为整数作为 `ann_type`；如果为空，使用顺序索引
  - [x] SubTask 2.3: 验证现有 C 解码器的 `ann_labels` 第 0 列均为空字符串，确保向后兼容

- [x] Task 3: 添加 `c_decoder_register_output_meta()` API
  - [x] SubTask 3.1: 在 `libsigrokdecode.h` 中声明 `c_decoder_register_output_meta(di, output_type, proto_id, meta_type, meta_name, meta_descr)`
  - [x] SubTask 3.2: 在 `c_decoder_api.c` 中实现该函数，填充 `srd_pd_output` 的 `meta_type`、`meta_name`、`meta_descr` 字段
  - [x] SubTask 3.3: 添加 `c_decoder_put_meta_int()` 和 `c_decoder_put_meta_double()` 便捷函数

- [x] Task 4: 添加 `c_decoder_get_last_samplenum()` API
  - [x] SubTask 4.1: 在 `libsigrokdecode.h` 中声明该函数
  - [x] SubTask 4.2: 在 `c_decoder_api.c` 中实现，返回 `di->last_samplenum`
  - [x] SubTask 4.3: 在 `session.c` 的 C 解码器 `end()` 调用前设置 `di->last_samplenum`

- [x] Task 5: 修复 `c_decoder_get_pin()` 未使用可选通道返回值
  - [x] SubTask 5.1: 修改 `instance.c` 中 `c_decoder_get_pin_impl()`，对未映射的可选通道返回 `0xFF` 而非 `0`
  - [x] SubTask 5.2: 修改 pin cache 逻辑，对未映射通道缓存 `0xFF`

- [x] Task 6: 实现 C→Python 堆叠桥接
  - [x] SubTask 6.1: 添加 `c_decoder_put_python(di, ss, es, out_id, cmd, data, data_len)` API
  - [x] SubTask 6.2: 在 `c_decoder_api.c` 中实现 C→Python 转换（当前实现 C→C 和 fallback ANN）

- [x] Task 7: 实现 C→C 堆叠支持
  - [x] SubTask 7.1: 在 `srd_c_decoder` 中添加 `recv_proto` 回调
  - [x] SubTask 7.2: 在 `c_decoder_put_python()` 中检查 `di->next_di` 是否有 C 解码器实例并调用 `recv_proto`

- [x] Task 8: 编译验证
  - [x] SubTask 8.1: 增量构建所有修改的文件
  - [x] SubTask 8.2: 确保所有 37 个 C 解码器编译通过
  - [x] SubTask 8.3: 确保现有 C 解码器行为不受影响（向后兼容）

# Task Dependencies
- [Task 2] depends on [Task 1] (ann_type 修改可能影响 C_ANN_PUT 宏)
- [Task 6] depends on [Task 1] (C→Python 桥接需要 annotation 字段完整)
- [Task 7] depends on [Task 6] (C→C 堆叠基于 C→Python 的数据结构)
- [Task 8] depends on all other tasks
