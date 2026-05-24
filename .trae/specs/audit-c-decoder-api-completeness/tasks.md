# Tasks

- [x] Task 1: 修复Python→C桥接崩溃（type_decoder.c）
  - [x] 在type_decoder.c的SRD_OUTPUT_PYTHON分支中添加is_c_inst检查
  - [x] 当next_di->is_c_inst为TRUE时，跳过并输出警告日志
  - [x] 防止对NULL py_inst调用PyObject_CallMethod

- [x] Task 2: 重命名SRD_OUTPUT_PYTHON为SRD_OUTPUT_PROTO
  - [x] 在libsigrokdecode.h中添加SRD_OUTPUT_PROTO枚举值，SRD_OUTPUT_PYTHON改为#define别名
  - [x] 在libsigrokdecode.h中添加c_decoder_put_proto函数声明，c_decoder_put_python改为#define别名
  - [x] 更新c_decoder_api.c中c_decoder_register_output()的警告信息
  - [x] 更新c_decoder_api.c中c_decoder_put()的SRD_OUTPUT_PROTO分支错误信息
  - [x] 保留SRD_OUTPUT_PYTHON和c_decoder_put_python作为向后兼容别名

- [x] Task 3: 修复c_decoder_put()的SRD_OUTPUT_PYTHON分支
  - [x] 将SRD_OUTPUT_PROTO分支改为返回SRD_ERR_ARG
  - [x] 输出错误信息引导使用c_decoder_put_proto()
  - [x] 不再将ann结构体作为pdata.data传递

- [x] Task 4: 添加混合堆叠保护（instance.c）
  - [x] 在srd_inst_stack()中检查di_bottom和di_top的is_c_inst属性
  - [x] Python→C堆叠：返回SRD_ERR_ARG并输出错误
  - [x] C→Python堆叠：返回SRD_ERR_ARG并输出错误
  - [x] 同类型堆叠（C→C或Python→Python）：正常通过

- [x] Task 5: 迁移can_c到c_cond_*模式
  - [x] 将can_c.c中的c_decoder_wait(di, NULL, ...)替换为c_cond_edge(cb, CAN_RX) + c_cond_wait()
  - [x] 对CAN RX通道设置边沿条件

- [x] Task 6: 补充c_decoder_utils.h中未使用API的文档
  - [x] 添加C_ANN_PUT_TYPE使用示例
  - [x] 添加C_ANN_PUT_VAL使用示例
  - [x] 添加c_decoder_get_initial_pin使用示例
  - [x] 添加c_decoder_get_last_samplenum使用示例
  - [x] 添加c_cond_noedge使用示例
  - [x] 添加c_decoder_put_logic使用示例

- [x] Task 7: 更新现有C解码器使用SRD_OUTPUT_PROTO
  - [x] 将18个C解码器的c_decoder_register_output(di, SRD_OUTPUT_PYTHON, ...)改为SRD_OUTPUT_PROTO
  - [x] c_decoder_put_python保留（通过#define别名自动替换为c_decoder_put_proto）

# Task Dependencies
- Task 2 必须在 Task 7 之前完成（先定义新名称再更新引用）✅
- Task 3 依赖 Task 2（错误信息中引用新名称）✅
- Task 1 和 Task 4 可并行执行 ✅
- Task 5 独立执行 ✅
- Task 6 独立执行 ✅
