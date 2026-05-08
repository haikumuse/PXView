# C解码器DLL动态加载 Spec

## Why
当前C解码器框架仅支持静态编译链接（硬编码`extern`声明+`srd_c_decoder_load_all()`注册），无法像Python解码器那样通过放置文件来扩展。需要实现DLL动态加载机制，使C解码器可以编译为独立DLL文件，放入指定目录后自动被发现和加载，同时将高频使用的Python解码器逐步转换为C DLL以提升性能。

## What Changes
- 实现C解码器DLL动态加载机制（Windows: LoadLibrary, Linux: dlopen）
- 定义C解码器DLL的标准导出接口（`srd_c_decoder_entry`导出函数）
- 修改`srd_c_decoder_load_all()`从DLL目录扫描加载，替代硬编码注册
- 新增DLL搜索路径配置（复用现有decoders/目录或新增c_decoders/子目录）
- 补全`srd_c_decoder`结构体缺失的元数据字段（options、annotations、inputs、outputs）
- 修改`srd_c_decoder_register()`映射完整元数据到`srd_decoder`
- 将高频Python解码器（I2C、SPI、UART、CAN）转换为C DLL实现
- 修复已知问题：`c_decoder_wait()`的SKIP逻辑、`c_decoder_put()`的PYTHON输出

## Impact
- Affected code:
  - `libsigrokdecode4DSL/decoder.c` — 重写`srd_c_decoder_load_all()`为DLL扫描加载
  - `libsigrokdecode4DSL/libsigrokdecode.h` — 补全`srd_c_decoder`元数据字段、新增DLL加载API
  - `libsigrokdecode4DSL/c_decoder_api.c` — 修复`c_decoder_wait()`和`c_decoder_put()`已知问题
  - `libsigrokdecode4DSL/c_decoders/` — 新增I2C/UART/CAN C解码器DLL源码
  - `libsigrokdecode4DSL/c_decoders/spi_c.c` — 补全选项和注解元数据
  - `CMakeLists.txt` — C解码器DLL的构建规则
  - `DSView/pv/appcontrol.cpp` — 可能需要设置C解码器DLL搜索路径

## ADDED Requirements

### Requirement: C解码器DLL动态加载
系统应当支持从指定目录扫描并动态加载C解码器DLL文件。

#### Scenario: 启动时自动扫描C解码器DLL目录
- **WHEN** 程序启动并调用`srd_decoder_load_all()`
- **THEN** 系统扫描C解码器DLL目录（如`decoders/c_decoders/`），对每个`.dll`（Windows）或`.so`（Linux）文件调用`LoadLibrary`/`dlopen`加载，查找`srd_c_decoder_entry`导出符号，调用该函数获取`srd_c_decoder*`并注册

#### Scenario: 无DLL文件时正常降级
- **WHEN** C解码器DLL目录不存在或为空
- **THEN** 程序正常运行，仅使用Python解码器和静态注册的C解码器

#### Scenario: DLL加载失败时跳过并记录日志
- **WHEN** 某个DLL文件无法加载（如缺少依赖、导出符号不存在）
- **THEN** 跳过该DLL，记录错误日志，不影响其他DLL和Python解码器的加载

### Requirement: C解码器DLL标准导出接口
每个C解码器DLL必须导出标准入口函数。

#### Scenario: DLL导出srd_c_decoder_entry函数
- **WHEN** C解码器DLL被加载
- **THEN** 系统查找导出符号`srd_c_decoder_entry`，该函数签名为`struct srd_c_decoder* srd_c_decoder_entry(void)`，返回指向`srd_c_decoder`结构体的指针

#### Scenario: DLL同时导出版本检查函数
- **WHEN** C解码器DLL被加载
- **THEN** 系统先查找`srd_c_decoder_api_version`导出符号，返回API版本号（当前为1），若版本不匹配则拒绝加载并记录日志

### Requirement: C解码器DLL搜索路径
系统应当支持配置C解码器DLL的搜索路径。

#### Scenario: 默认搜索路径
- **WHEN** 未显式设置C解码器DLL搜索路径
- **THEN** 默认搜索Python解码器目录下的`c_decoders/`子目录（如`decoders/c_decoders/`）

#### Scenario: 自定义搜索路径
- **WHEN** 调用`srd_c_decoder_path_set(path)`设置自定义路径
- **THEN** 后续加载C解码器DLL时从指定路径搜索

### Requirement: 补全C解码器元数据
`srd_c_decoder`结构体应当包含与Python解码器对齐的完整元数据。

#### Scenario: C解码器声明选项
- **WHEN** C解码器定义了`options`字段
- **THEN** 选项应正确映射到`srd_decoder`的`options` GSList，UI上可配置

#### Scenario: C解码器声明注解标签
- **WHEN** C解码器定义了`annotations`和`annotation_rows`字段
- **THEN** 注解标签应正确映射到`srd_decoder`的`annotations`/`annotation_rows` GSList，UI上正确显示

#### Scenario: C解码器声明输入输出协议
- **WHEN** C解码器定义了`inputs`和`outputs`字段
- **THEN** 输入输出协议应正确映射到`srd_decoder`，堆叠匹配检查可正常工作

### Requirement: 高频Python解码器转换为C DLL
将I2C、SPI、UART、CAN四个高频解码器转换为C DLL实现。

#### Scenario: I2C C解码器功能正确性
- **WHEN** 使用I2C C解码器解码I2C信号
- **THEN** 解码结果与Python I2C解码器一致（START/STOP/READ/WRITE/ACK/NACK等注解）

#### Scenario: UART C解码器功能正确性
- **WHEN** 使用UART C解码器解码UART信号
- **THEN** 解码结果与Python UART解码器一致（数据帧、波特率等注解）

#### Scenario: CAN C解码器功能正确性
- **WHEN** 使用CAN C解码器解码CAN信号
- **THEN** 解码结果与Python CAN解码器一致（帧ID、数据、ACK等注解）

#### Scenario: SPI C解码器补全选项
- **WHEN** 使用SPI C解码器
- **THEN** 支持CPOL、CPHA、bit_order、cs_polarity选项，与Python SPI解码器选项一致

### Requirement: 修复C解码器框架已知问题

#### Scenario: c_decoder_wait()正确处理SKIP条件
- **WHEN** C解码器使用SKIP条件等待
- **THEN** 应正确跳过指定数量的样本，而非直接忽略

#### Scenario: c_decoder_put()的PYTHON输出包含协议数据
- **WHEN** C解码器通过SRD_OUTPUT_PYTHON输出给上层Python解码器
- **THEN** 应将C数据转换为Python对象（如SPI字节值），使上层Python解码器可正确解析

## MODIFIED Requirements

### Requirement: srd_c_decoder结构体
原定义：缺少options/annotations/inputs/outputs/tags/binary字段。
修改为：新增以下字段以对齐Python解码器元数据：
```c
struct srd_c_decoder {
    // ... 现有字段 ...
    const char **inputs;
    int num_inputs;
    const char **outputs;
    int num_outputs;
    const struct srd_decoder_option *options;
    int num_options;
    const char *(*annotations)[2];
    int num_annotations;
    const struct srd_decoder_annotation_row *annotation_rows;
    int num_annotation_rows;
    const struct srd_decoder_binary *binary;
    int num_binary;
    const char **tags;
    int num_tags;
};
```

### Requirement: srd_c_decoder_load_all()
原逻辑：硬编码`extern`声明+直接注册。
修改为：扫描DLL目录，动态加载每个DLL，查找导出符号并注册。保留静态注册作为后备。

### Requirement: srd_c_decoder_register()
原逻辑：options/annotations/inputs/outputs等字段设为NULL。
修改为：从`srd_c_decoder`完整映射所有元数据字段到`srd_decoder`的GSList。
