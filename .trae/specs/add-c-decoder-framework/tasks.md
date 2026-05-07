# Tasks

- [x] Task 1: 定义C解码器核心数据结构
  - [x] SubTask 1.1: 在`libsigrokdecode.h`中定义`srd_c_decoder`结构体（id、name、channels、options、annotations等元数据 + reset/start/decode/destroy方法指针）
  - [x] SubTask 1.2: 在`libsigrokdecode.h`中定义`srd_c_decoder_inst`结构体（复用线程同步原语GCond/GMutex、输出回调pd_output、通道映射dec_channelmap、样本缓冲区inbuf/inbuflen等，新增user_data指针和c_dec回指指针）
  - [x] SubTask 1.3: 在`srd_decoder`结构体中新增`gboolean is_c_decoder`和`struct srd_c_decoder *c_dec`字段
  - [x] SubTask 1.4: 在`libsigrokdecode.h`中声明C解码器公开API：`srd_c_decoder_register()`、`srd_c_decoder_load_all()`

- [x] Task 2: 实现C解码器辅助函数
  - [x] SubTask 2.1: 实现`c_decoder_put()` — 根据输出类型（SRD_OUTPUT_ANN/SRD_OUTPUT_PYTHON/SRD_OUTPUT_BINARY/SRD_OUTPUT_META）直接构造C结构体并调用注册回调，无需Python对象转换
  - [x] SubTask 2.2: 实现`c_decoder_wait()` — 在C层直接进行条件匹配（复用现有`find_match()`/`process_samples_until_condition_match()`逻辑），等待新样本时使用GCond/GMutex同步，无需GIL
  - [x] SubTask 2.3: 实现`c_decoder_register()` — C解码器注册输出流，复用现有`srd_pd_output`结构
  - [x] SubTask 2.4: 实现`c_decoder_has_channel()` — 检查通道是否已分配

- [x] Task 3: 实现C解码器工作线程
  - [x] SubTask 3.1: 实现`c_di_thread()` — 不获取GIL，直接调用`c_dec->decode(inst)`，处理错误和终止信号
  - [x] SubTask 3.2: 实现C解码器实例的样本推送逻辑（类似`srd_inst_decode()`但不经过Python）

- [x] Task 4: 修改实例创建逻辑
  - [x] SubTask 4.1: 修改`srd_inst_new()` — 检查`dec->is_c_decoder`，若为TRUE则调用`create_c_decoder_inst()`创建C实例，若为FALSE则走原有Python流程
  - [x] SubTask 4.2: 实现`create_c_decoder_inst()` — 分配`srd_c_decoder_inst`，初始化通道映射、线程同步原语、调用`c_dec->start()`
  - [x] SubTask 4.3: 修改`srd_inst_channel_set_all()`适配C解码器实例
  - [x] SubTask 4.4: 修改`srd_inst_stack()`适配C解码器实例与Python解码器实例的混合堆叠

- [x] Task 5: 修改会话层调度逻辑
  - [x] SubTask 5.1: 修改`srd_session_send()` — 遍历di_list时根据实例类型调用对应的解码函数
  - [x] SubTask 5.2: 修改`srd_session_start()` — 适配C解码器实例的start调用
  - [x] SubTask 5.3: 修改`srd_session_end()` — 适配C解码器实例的结束流程
  - [x] SubTask 5.4: 修改`srd_session_destroy()` — 适配C解码器实例的销毁

- [x] Task 6: 修改解码器加载逻辑
  - [x] SubTask 6.1: 实现`srd_c_decoder_register()` — 将C解码器包装为`srd_decoder`（填充元数据字段，设置is_c_decoder=TRUE），添加到全局pd_list
  - [x] SubTask 6.2: 实现`srd_c_decoder_load_all()` — 遍历内置C解码器列表，调用`srd_c_decoder_register()`
  - [x] SubTask 6.3: 修改`srd_decoder_load_all()` — 在加载Python解码器后调用`srd_c_decoder_load_all()`

- [x] Task 7: C解码器与Python解码器混合堆叠
  - [x] SubTask 7.1: 实现C解码器`SRD_OUTPUT_PYTHON`输出到Python解码器 — 将C数据转换为Python对象后调用上层Python解码器的decode()
  - [x] SubTask 7.2: 实现Python解码器`SRD_OUTPUT_PYTHON`输出到C解码器 — 将Python对象转换为C数据结构后传递给上层C解码器

- [x] Task 8: C++前端适配
  - [x] SubTask 8.1: 修改`Decoder::create_decoder_inst()` — 适配C解码器（无需Python选项设置，直接传递GHashTable选项）
  - [x] SubTask 8.2: 修改`DecoderStack::execute_decode_stack()` — 适配C解码器实例的创建和堆叠
  - [x] SubTask 8.3: 确认`annotation_callback`对C解码器和Python解码器输出结果的处理一致

- [x] Task 9: 实现SPI C解码器
  - [x] SubTask 9.1: 创建`libsigrokdecode4DSL/c_decoders/spi_c.c`，实现SPI协议的C解码器（reset/start/decode/destroy）
  - [x] SubTask 9.2: SPI C解码器支持所有通道（CLK、MISO、MOSI、CS#）和选项（CPOL、CPHA、bit order等）
  - [x] SubTask 9.3: SPI C解码器输出与Python SPI解码器一致的注解格式
  - [x] SubTask 9.4: 注册SPI C解码器到全局列表

- [ ] Task 10: 测试验证
  - [ ] SubTask 10.1: 测试SPI C解码器单独解码功能正确
  - [ ] SubTask 10.2: 测试SPI C解码器与Python SPI解码器输出结果一致
  - [ ] SubTask 10.3: 测试C解码器与Python解码器共存（同时添加C-SPI和Python-I2C解码器）
  - [ ] SubTask 10.4: 测试C解码器与Python解码器堆叠（C-SPI底层 + Python上层解码器）
  - [ ] SubTask 10.5: 测试C解码器在多线程并行解码下的正确性
  - [ ] SubTask 10.6: 测试C解码器删除/停止/清理的正确性

# Task Dependencies
- [Task 2] depends on [Task 1] — 先定义数据结构再实现辅助函数
- [Task 3] depends on [Task 1, Task 2] — 工作线程依赖数据结构和辅助函数
- [Task 4] depends on [Task 1, Task 3] — 实例创建依赖数据结构和工作线程
- [Task 5] depends on [Task 4] — 会话层调度依赖实例创建
- [Task 6] depends on [Task 1] — 加载逻辑依赖数据结构
- [Task 7] depends on [Task 2, Task 4] — 混合堆叠依赖辅助函数和实例创建
- [Task 8] depends on [Task 4, Task 5] — 前端适配依赖后端接口稳定
- [Task 9] depends on [Task 1, Task 2, Task 6] — SPI实现依赖框架完成
- [Task 10] depends on [Task 8, Task 9] — 测试依赖全部实现完成
