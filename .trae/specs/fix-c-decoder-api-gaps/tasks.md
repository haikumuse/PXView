# Tasks

## Phase 1: API缺口补全（代码修改）

- [x] Task 1: 实现SRD_OUTPUT_LOGIC输出类型及c_decoder_put_logic() API
  - [x] 在c_decoder_utils.h中添加SRD_OUTPUT_LOGIC枚举值和c_decoder_put_logic()声明
  - [x] 在c_decoder_api.c中实现c_decoder_put_logic()函数
  - [x] 在c_decoder_api.c的c_decoder_register_output()中处理SRD_OUTPUT_LOGIC类型

- [x] Task 2: 扩展BITS消息格式增加bit级时间戳
  - [x] 修改spi_c.c的BITS输出格式，为每个bit添加ss/es时间戳
  - [x] 修改i2c_c.c的BITS输出格式（8位，MSB first，含时间戳）
  - [x] 更新c_decoder_utils.h中BITS格式文档注释

- [x] Task 3: 验证c_decoder_register_output()在recv_proto()中可安全调用
  - [x] 审查c_decoder_register_output()实现，确认线程安全性和内存安全性（g_malloc0+g_slist_append，在di_thread中同步调用，无并发问题）
  - [x] 验证recv_proto中动态注册输出流的场景（下游解码器无独立线程，架构安全）

- [x] Task 4: 补充uart_c.c的IDLE/BREAK输出
  - [x] 在uart_c.c中添加"IDLE"和"BREAK"命令的c_decoder_put_python()输出
  - [x] 确保与Python uart解码器的输出格式一致

## Phase 2: 严重协议格式错误修正（子Spec修改）

- [x] Task 6: 修正Batch-26/27/28的SPI DATA包格式
  - [x] Batch-26: 修正SPI DATA格式为17字节（data[0]=flags, data[1..8]=mosi, data[9..16]=miso）
  - [x] Batch-26: 修正CS-CHANGE处理逻辑（data[1]判断CS状态）
  - [x] Batch-27: 修正SPI DATA格式和辅助函数spi_data_get_mosi/miso
  - [x] Batch-27: 修正have_mosi/have_miso判断为位操作（data[0] & 1, (data[0]>>1) & 1）
  - [x] Batch-28: 修正SPI DATA格式和have_mosi/have_miso提取方式

- [x] Task 7: 修正Batch-34的4b5b_c输出格式
  - [x] 将4b5b_c输出格式从"START"/"TERMINATE"/"RESET"修正为"J"/"K"/"T"/"R"/"Q"/"H"/"L"/"IDLE"/"SET"
  - [x] 重新设计ethernet_c的recv_proto状态机（检测JK序列作为帧开始）

- [x] Task 8: 修正Batch-31的UART输出格式描述
  - [x] 标注"IDLE"/"BREAK"命令为"C版本暂不支持"
  - [x] 明确C UART DATA输出不含databits的限制

- [x] Task 9: 修正Batch-32的JTAG IR TDO描述
  - [x] 修正"JTAG C解码器未发送IR TDO"为"JTAG C解码器同时发送IR TDI和IR TDO"
  - [x] 添加对"IR TDO"和"DR TDO"的处理逻辑

- [x] Task 10: 修正Batch-13的arm_etmv3数据读取错误
  - [x] 将`uint8_t byte = data[0]`修正为`uint8_t rxtx = data[0]; uint8_t byte = data[1]`
  - [x] 补充uart_c.c完整输出格式文档

- [x] Task 11: 修正Batch-37的pjon strcmp bug
  - [x] 将`strcmp(cmd, "IDLE") || strcmp(cmd, "FRAME_DATA") == 0`修正为`strcmp(cmd, "IDLE") == 0 || strcmp(cmd, "FRAME_DATA") == 0`

## Phase 3: 中等/轻微错误修正（子Spec修改）

- [x] Task 12: 修正Batch-20的mpu6050 annotation枚举命名
  - [x] 将DS1307风格命名替换为MPU6050相关命名

- [x] Task 13: 修正Batch-25的%b格式和license
  - [x] 实现自定义二进制格式化辅助函数替代%b
  - [x] 修正enc28j60 license为"mit"

- [x] Task 14: 修正Batch-17的xy2-100 idn命名
  - [x] 将连字符改为下划线：dec_xy2_100_chan_clk

- [x] Task 15: 修正Batch-03的c_cond_or错误描述
  - [x] 删除"c_cond_wait不支持OR条件列表"的错误声明
  - [x] 使用标准c_cond_or模式替代变通方案

## Phase 4: 参考范本引用补全（全部37批子Spec）

- [x] Task 16: 为Batch 01-10添加标准范本引用
  - [x] 每个batch在spec开头添加"参考实现"章节
  - [x] 底层解码器引用spi_c.c/can_fd_c.c
  - [x] 上层解码器引用lm75_c.c/ds1307_c.c + 对应数据源格式参考

- [x] Task 17: 为Batch 11-20添加标准范本引用
  - [x] 同上规则

- [x] Task 18: 为Batch 21-30添加标准范本引用
  - [x] 同上规则

- [x] Task 19: 为Batch 31-37添加标准范本引用
  - [x] 同上规则

# Task Dependencies
- Task 6-9 依赖 Task 2（BITS格式修改后需同步更新spec中的BITS解析代码）
- Task 7 依赖确认4b5b_c.c实际输出格式（已验证）
- Task 8 依赖 Task 4（uart_c.c添加IDLE/BREAK输出已完成）
- Task 16-19 可并行执行（已完成）
- Task 6/7/10/11 为最优先修正项（P0级别，已完成）
