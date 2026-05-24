# Tasks

- [x] Task 1: 实现c_cond_wait_current() API
  - [x] 在libsigrokdecode.h中添加函数声明
  - [x] 在c_decoder_api.c中实现：SKIP(0)条件立即返回当前采样位置
  - [x] 在spi_c.c中使用c_cond_wait_current()替代first_sample变通方案
  - [x] 编译验证通过

- [x] Task 2: 实现c_decoder_get_initial_pin() API
  - [x] 在libsigrokdecode.h中添加函数声明
  - [x] 在c_decoder_api.c中实现：从di->old_pins_array->data[ch]读取初始引脚值
  - [x] 编译验证通过

- [x] Task 3: 扩展BITS消息格式增加bit级时间戳
  - [x] 修改spi_c.c的BITS输出，为每个bit添加[value(1B)][ss(8B LE)][es(8B LE)]
  - [x] 修改i2c_c.c新增BITS输出（原来没有BITS输出）
  - [x] 更新c_decoder_utils.h中BITS v2格式文档注释
  - [ ] 更新Batch-20/21/22/23/26/27/32的spec中BITS解析代码（待后续实施）
  - [x] 编译验证通过

- [x] Task 4: 修改ps2_c.c添加Python输出
  - [x] 在ps2_c.c的start()中注册SRD_OUTPUT_PYTHON输出
  - [x] 在ps2_handle_byte()中添加c_decoder_put_python()调用，输出"BYTE"命令
  - [x] 输出格式: cmd="BYTE", data=[byte_val, is_host, parity_ok, has_ack]
  - [x] 编译验证通过

- [x] Task 5: 确认usb_signalling_c.c Python输出完整性
  - [x] 读取usb_signalling_c.c确认已有完整Python输出（10处c_decoder_put_python调用）
  - [x] 无需修改

- [x] Task 6: 更新fix-c-decoder-api-gaps spec删除Python→C桥接需求
  - [x] 删除spec中Python→C proto桥接的Requirement
  - [x] 添加C解码器依赖规则说明
  - [x] 更新阻塞解码器清单

# Task Dependencies
- Task 3 依赖 spi_c.c 和 i2c_c.c 的BITS输出逻辑理解（已完成）
- Task 4 是 Batch-35 ps2_keyboard_c/ps2_mouse_c的前置条件（已完成）
- Task 1/2/4/5 已并行执行完成
