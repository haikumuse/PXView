# C解码器框架 Spec

## Why
当前所有协议解码器均为Python实现，受Python GIL限制无法真正并行执行，且Python解释执行性能较低。需要引入C解码器框架，使C解码器可以绕过GIL实现真正的多核并行解码，同时大幅提升单解码器的执行速度。C解码器需与Python解码器共存，共享相同的UI和回调机制。

## What Changes
- 在`libsigrokdecode4DSL`中新增C解码器接口定义（`srd_c_decoder`结构体、方法指针）
- 新增C解码器实例结构体（`srd_c_decoder_inst`），复用现有的线程同步机制（GCond/GMutex）和输出回调机制
- 新增C解码器辅助函数：`c_decoder_put()`、`c_decoder_wait()`、`c_decoder_register()`
- 新增C解码器工作线程`c_di_thread()`，无需Python GIL
- 修改`srd_decoder`结构体，添加`is_c_decoder`标志和`c_dec`指针
- 修改`srd_inst_new()`根据解码器类型创建不同实例
- 修改`srd_inst_decode()`根据实例类型调用不同的工作线程
- 修改`srd_decoder_load_all()`同时加载C解码器
- 新增C解码器注册API：`srd_c_decoder_register()`
- 实现SPI C解码器作为首个示例

## Impact
- Affected code:
  - `libsigrokdecode4DSL/libsigrokdecode.h` — 新增`is_c_decoder`字段、`c_dec`指针、`srd_c_decoder`结构体、`srd_c_decoder_inst`结构体、C解码器API声明
  - `libsigrokdecode4DSL/libsigrokdecode-internal.h` — 新增内部函数声明
  - `libsigrokdecode4DSL/instance.c` — 修改`srd_inst_new()`、`srd_inst_decode()`，新增`c_di_thread()`、`create_c_decoder_inst()`
  - `libsigrokdecode4DSL/session.c` — 修改`srd_session_send()`适配C解码器实例
  - `libsigrokdecode4DSL/decoder.c` — 修改`srd_decoder_load_all()`，新增C解码器加载逻辑
  - `libsigrokdecode4DSL/type_decoder.c` — 新增`c_decoder_put()`、`c_decoder_wait()`、`c_decoder_register()`（或独立新文件`c_decoder_api.c`）
  - 新增文件：`libsigrokdecode4DSL/c_decoders/spi_c.c` — SPI C解码器实现
  - `DSView/pv/data/decode/decoder.cpp` — `create_decoder_inst()`适配C解码器（无需Python选项设置）
  - `DSView/pv/data/decoderstack.cpp` — `execute_decode_stack()`适配C解码器实例

## ADDED Requirements

### Requirement: C解码器接口定义
系统应当定义一套C解码器接口，使C语言编写的协议解码器可以与Python解码器使用相同的调度和回调框架。

#### Scenario: C解码器接口包含必要的方法指针
- **WHEN** 定义`srd_c_decoder`结构体
- **THEN** 必须包含`reset()`、`start()`、`decode()`、`destroy()`四个方法指针，以及与Python解码器对齐的元数据（id、name、channels、options、annotations等）

### Requirement: C解码器实例创建
系统应当支持创建C解码器实例，无需启动Python解释器或获取GIL。

#### Scenario: 创建C解码器实例
- **WHEN** 调用`srd_inst_new()`且指定解码器的`is_c_decoder`为TRUE
- **THEN** 应创建`srd_c_decoder_inst`实例，直接调用C函数指针初始化，不经过Python C API

#### Scenario: 创建Python解码器实例（不受影响）
- **WHEN** 调用`srd_inst_new()`且指定解码器的`is_c_decoder`为FALSE
- **THEN** 行为与改动前完全一致，通过Python C API创建实例

### Requirement: C解码器工作线程
C解码器的工作线程不需要Python GIL，可以直接在C层执行解码逻辑。

#### Scenario: C解码器工作线程执行
- **WHEN** C解码器的工作线程`c_di_thread()`启动
- **THEN** 直接调用`decoder->decode(inst)`，不获取Python GIL，不调用`PyGILState_Ensure()`

#### Scenario: C解码器与Python解码器并行
- **WHEN** 一个C解码器和一个Python解码器同时运行
- **THEN** C解码器不受GIL限制独立运行，Python解码器正常获取GIL运行，两者可真正并行

### Requirement: C解码器put()函数
C解码器需要`c_decoder_put()`函数输出解码结果，直接调用注册的回调，无需Python对象转换。

#### Scenario: C解码器输出注解
- **WHEN** C解码器调用`c_decoder_put(di, ss, es, SRD_OUTPUT_ANN, ann_class, ann_text)`
- **THEN** 直接构造`srd_proto_data_annotation`结构体，调用注册的`annotation_callback`，无需经过Python对象转换

#### Scenario: C解码器输出Python对象给堆叠的上层Python解码器
- **WHEN** C解码器调用`c_decoder_put(di, ss, es, SRD_OUTPUT_PYTHON, ...)`且上层堆叠了Python解码器
- **THEN** 需将C数据转换为Python对象后传递给上层Python解码器的`decode()`方法

### Requirement: C解码器wait()函数
C解码器需要`c_decoder_wait()`函数等待条件匹配，直接在C层操作样本数据，无需GIL。

#### Scenario: C解码器等待条件匹配
- **WHEN** C解码器调用`c_decoder_wait(di, conditions, &samplenum, &matched)`
- **THEN** 在C层直接进行条件匹配（边沿检测、电平匹配），等待新样本时释放`data_mutex`让推送线程继续，无需获取/释放Python GIL

#### Scenario: C解码器wait()性能
- **WHEN** C解码器使用`c_decoder_wait()`等待条件
- **THEN** 整个等待和匹配过程在纯C层完成，无Python解释器开销，性能显著优于Python的`Decoder_wait()`

### Requirement: C解码器与Python解码器共存
C解码器和Python解码器在UI上统一展示，用户无需区分类型。

#### Scenario: 解码器列表统一展示
- **WHEN** 用户打开协议面板查看可用解码器列表
- **THEN** C解码器和Python解码器出现在同一个列表中，C解码器可标注"(C)"以示区分，但不影响使用方式

#### Scenario: C解码器与Python解码器堆叠
- **WHEN** 用户将C解码器（如SPI）作为底层，Python解码器（如SPI闪存）堆叠在上层
- **THEN** C解码器的`SRD_OUTPUT_PYTHON`输出应正确转换为Python对象传递给上层Python解码器

#### Scenario: Python解码器与C解码器堆叠
- **WHEN** 用户将Python解码器作为底层，C解码器堆叠在上层
- **THEN** Python解码器的`SRD_OUTPUT_PYTHON`输出应正确转换为C数据结构传递给上层C解码器

### Requirement: C解码器注册与加载
系统应当支持C解码器的注册和加载。

#### Scenario: 静态注册C解码器
- **WHEN** 在程序初始化时调用`srd_c_decoder_register(&spi_c_decoder)`
- **THEN** C解码器被添加到全局解码器列表`pd_list`中，与Python解码器统一管理

#### Scenario: 加载所有解码器
- **WHEN** 调用`srd_decoder_load_all()`
- **THEN** 同时加载Python解码器（扫描decoders/目录）和C解码器（调用已注册的C解码器列表）

### Requirement: SPI C解码器实现
实现SPI协议的C解码器作为首个C解码器示例。

#### Scenario: SPI C解码器功能正确性
- **WHEN** 使用SPI C解码器解码SPI信号
- **THEN** 解码结果与Python SPI解码器完全一致（相同的注解输出、相同的通道配置、相同的选项）

#### Scenario: SPI C解码器性能
- **WHEN** 对比SPI C解码器与SPI Python解码器的解码速度
- **THEN** C解码器速度应显著快于Python解码器（预期5x-10x提升）

## MODIFIED Requirements

### Requirement: srd_decoder结构体
原定义：`srd_decoder`仅包含Python相关字段（`py_mod`、`py_dec`）。
修改为：新增`gboolean is_c_decoder`标志和`struct srd_c_decoder *c_dec`指针。Python解码器的`is_c_decoder`为FALSE，`c_dec`为NULL；C解码器的`is_c_decoder`为TRUE，`py_mod`和`py_dec`为NULL。

### Requirement: srd_inst_new()实例创建
原逻辑：始终通过`PyObject_CallObject(dec->py_dec, NULL)`创建Python实例。
修改为：先检查`dec->is_c_decoder`，若为TRUE则创建C解码器实例（分配`srd_c_decoder_inst`，调用`c_dec->reset()`和`c_dec->start()`），若为FALSE则走原有Python流程。

### Requirement: srd_inst_decode()解码执行
原逻辑：始终创建`di_thread()`工作线程（持有GIL执行Python decode()）。
修改为：根据实例类型，C解码器创建`c_di_thread()`工作线程（无GIL），Python解码器仍使用`di_thread()`。

### Requirement: srd_session_send()数据推送
原逻辑：遍历`sess->di_list`，对每个`srd_decoder_inst`调用`srd_inst_decode()`。
修改为：遍历`sess->di_list`，根据实例类型（C或Python）调用对应的解码函数。C解码器实例和Python解码器实例可以共存于同一个session中。
