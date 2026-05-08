# Tasks

- [x] Task 1: 补全srd_c_decoder结构体元数据字段
  - [x] SubTask 1.1: 在`libsigrokdecode.h`的`srd_c_decoder`中新增字段
  - [x] SubTask 1.2: 修改`srd_c_decoder_register()`，将新增字段完整映射到`srd_decoder`的GSList

- [x] Task 2: 定义C解码器DLL标准导出接口
  - [x] SubTask 2.1: 定义`SRD_C_DECODER_API_VERSION`常量和`SRD_C_DECODER_EXPORT`宏
  - [x] SubTask 2.2: 定义标准导出函数签名和函数指针类型
  - [x] SubTask 2.3: 新增`srd_c_decoder_path_set()`声明

- [x] Task 3: 实现DLL动态加载机制
  - [x] SubTask 3.1: 实现`srd_c_decoder_path_set()`
  - [x] SubTask 3.2: 重写`srd_c_decoder_load_all()`为DLL扫描加载
  - [x] SubTask 3.3: 处理加载失败（日志+跳过）
  - [x] SubTask 3.4: 跨平台支持（Windows LoadLibrary + Linux dlopen）

- [x] Task 4: 修复C解码器框架已知问题
  - [x] SubTask 4.1: 修复`c_decoder_wait()`中SRD_TERM_SKIP的处理逻辑
  - [x] SubTask 4.2: 修复`c_decoder_put()`中SRD_OUTPUT_PYTHON分支

- [x] Task 5: 补全SPI C解码器
  - [x] SubTask 5.1: 添加DLL导出函数
  - [x] SubTask 5.2: 补全元数据字段

- [x] Task 6: 实现I2C C解码器DLL
  - [x] SubTask 6.1: 创建`c_decoders/i2c_c.c`
  - [x] SubTask 6.2: I2C C解码器支持所有通道和选项
  - [x] SubTask 6.3: I2C C解码器输出与Python I2C解码器一致
  - [x] SubTask 6.4: 使用SRD_C_DECODER_EXPORT宏导出

- [x] Task 7: 实现UART C解码器DLL
  - [x] SubTask 7.1: 创建`c_decoders/uart_c.c`
  - [x] SubTask 7.2: UART C解码器支持所有通道和选项
  - [x] SubTask 7.3: UART C解码器输出与Python UART解码器一致
  - [x] SubTask 7.4: 使用SRD_C_DECODER_EXPORT宏导出

- [x] Task 8: 实现CAN C解码器DLL
  - [x] SubTask 8.1: 创建`c_decoders/can_c.c`
  - [x] SubTask 8.2: CAN C解码器支持所有通道和选项
  - [x] SubTask 8.3: CAN C解码器输出与Python CAN解码器一致
  - [x] SubTask 8.4: 使用SRD_C_DECODER_EXPORT宏导出

- [x] Task 9: CMake构建规则
  - [x] SubTask 9.1: 每个C解码器编译为独立DLL
  - [x] SubTask 9.2: DLL输出目录为`decoders/c_decoders/`
  - [x] SubTask 9.3: 修复多重定义和符号依赖问题

- [x] Task 10: 测试验证
  - [x] SubTask 10.1: 构建成功，生成DSView.exe + 4个DLL

# Task Dependencies
- [Task 2] depends on [Task 1]
- [Task 3] depends on [Task 2]
- [Task 4] depends on nothing
- [Task 5] depends on [Task 1, Task 2]
- [Task 6] depends on [Task 1, Task 2]
- [Task 7] depends on [Task 1, Task 2]
- [Task 8] depends on [Task 1, Task 2]
- [Task 9] depends on [Task 5, Task 6, Task 7, Task 8]
- [Task 10] depends on [Task 3, Task 9]
