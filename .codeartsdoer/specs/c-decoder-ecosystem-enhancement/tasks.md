# C Decoder生态体系增强 — 编码任务列表

> 基于spec.md需求规格和design.md实现方案，按S1→S2→S3→S4→S5子系统依赖顺序规划任务。
> 每项任务标注修改文件、修改内容、依赖关系、验证方式。

---

## 1. S1: DLL注册表 — 新增dll_registry.h/c

### 1.1 创建dll_registry.h头文件
- [ ] 新建 `libsigrokdecode4DSL/dll_registry.h`，定义以下内容：
  - 枚举 `enum srd_c_dll_status`：含5种状态（LOADED / VERSION_MISMATCH / ENTRY_MISSING / LOAD_FAILED / UNLOADED）
  - 结构体 `struct srd_c_dll_entry`：含file_path、handle、api_version、status、decoder_id、c_dec、load_time共7个字段
  - 全局注册表声明 `extern GSList *c_dll_registry`
  - 6个注册表操作API声明：add / find_by_path / find_by_id / remove / cleanup / count
- **依赖**：无
- **验证**：头文件可被decoder.c正确include，编译无报错

### 1.2 实现dll_registry.c核心逻辑
- [ ] 新建 `libsigrokdecode4DSL/dll_registry.c`，实现6个API函数：
  - `srd_c_dll_registry_add()`：调用find_by_path去重，重复时返回NULL并记录日志；不重复时分配srd_c_dll_entry、g_strdup各字符串字段、设置load_time=time(NULL)，追加到c_dll_registry
  - `srd_c_dll_registry_find_by_path()`：遍历c_dll_registry，g_strcmp0匹配file_path
  - `srd_c_dll_registry_find_by_id()`：遍历c_dll_registry，g_strcmp0匹配decoder_id
  - `srd_c_dll_registry_remove()`：按decoder_id查找并移除条目，g_free释放字符串，g_slice_free释放条目结构体
  - `srd_c_dll_registry_cleanup()`：遍历所有条目，对status==LOADED的执行FreeLibrary/dlclose释放句柄，然后释放条目和注册表本身
  - `srd_c_dll_registry_count()`：g_slist_length统计
- **依赖**：1.1
- **验证**：单元级验证——调用add添加条目后find_by_path可查到；重复add返回NULL；remove后find_by_id返回NULL；cleanup后count为0

### 1.3 将dll_registry.c加入CMakeLists.txt构建
- [ ] 在 `CMakeLists.txt` 中将 `dll_registry.c` 添加到libsigrokdecode4DSL库的源文件列表中，确保与decoder.c、instance.c等一同编译
- **依赖**：1.1, 1.2
- **验证**：执行增量构建，dll_registry.o/.obj生成成功

---

## 2. S2: 引擎集成改造 — decoder.c改造、热加载/卸载/多路径API

### 2.1 srd_c_decoder_load_all()改造 — 提取公共加载函数
- [ ] 修改 `libsigrokdecode4DSL/decoder.c`：
  - 从`srd_c_decoder_load_all()`（第1314-1423行）提取单个DLL加载逻辑为内部函数 `static int srd_c_decoder_load_single(const char *full_path)`
  - 该函数封装：LoadLibraryA/dlopen → GetProcAddress/dlsym查找入口函数 → API版本校验 → 重复加载防护（调用srd_c_dll_registry_find_by_path） → entry_func()获取srd_c_decoder → srd_c_decoder_register()注册 → srd_c_dll_registry_add()保存元信息
  - 加载成功时，将handle和full_path存入DLL注册表（调用srd_c_dll_registry_add）
  - 版本不匹配时，记录到注册表（状态VERSION_MISMATCH，handle=NULL因为已释放）
  - 重复加载时，释放handle并跳过
  - 改造`srd_c_decoder_load_all()`，循环中改为调用`srd_c_decoder_load_single()`
- **依赖**：1.2
- **验证**：应用启动后，调用srd_c_dll_registry_count()返回已加载DLL数量（应与c_decoders/下的DLL数一致）

### 2.2 热加载API — srd_c_decoder_load()
- [ ] 在 `libsigrokdecode4DSL/decoder.c` 中新增函数 `SRD_API int srd_c_decoder_load(const char *dll_path)`：
  - 参数校验：dll_path非NULL且为绝对路径（g_path_is_absolute）
  - 去重检查：调用srd_c_dll_registry_find_by_path，已加载返回SRD_ERR_ARG
  - 调用srd_c_decoder_load_single(dll_path)执行加载
  - 返回SRD_OK或对应错误码
- [ ] 在 `libsigrokdecode4DSL/libsigrokdecode.h` 中添加函数声明：`SRD_API int srd_c_decoder_load(const char *dll_path);`
- **依赖**：2.1
- **验证**：应用运行期间，对一个未加载的DLL调用srd_c_decoder_load()，新decoder出现在srd_decoder_list()中且可创建实例解码；对已加载DLL再次调用返回SRD_ERR_ARG

### 2.3 安全卸载API — srd_c_decoder_unload()
- [ ] 在 `libsigrokdecode4DSL/decoder.c` 中新增函数 `SRD_API int srd_c_decoder_unload(const char *decoder_id)`：
  - 参数校验：decoder_id非NULL
  - 查找注册表条目：srd_c_dll_registry_find_by_id
  - 检查活跃实例：遍历sessions全局变量，检查所有session的di_list中是否有匹配decoder_id且非终止状态的实例；有活跃实例时返回SRD_ERR_ARG
  - 从全局decoder列表(pd_list)移除该decoder
  - 执行FreeLibrary/dlclose卸载DLL
  - 从注册表移除：srd_c_dll_registry_remove
  - 返回SRD_OK
- [ ] 在 `libsigrokdecode4DSL/libsigrokdecode.h` 中添加函数声明：`SRD_API int srd_c_decoder_unload(const char *decoder_id);`
- **依赖**：2.1
- **验证**：无活跃实例时调用srd_c_decoder_unload()成功返回SRD_OK，srd_decoder_list()不再包含该decoder；有活跃实例时返回SRD_ERR_ARG

### 2.4 DLL元信息查询API
- [ ] 在 `libsigrokdecode4DSL/decoder.c` 中新增2个函数：
  - `SRD_API const GSList *srd_c_dll_registry_get(void)`：返回c_dll_registry指针（只读）
  - `SRD_API const struct srd_c_dll_entry *srd_c_dll_info_get(const char *decoder_id)`：调用find_by_id返回条目
- [ ] 在 `libsigrokdecode4DSL/libsigrokdecode.h` 中添加对应函数声明
- **依赖**：1.2
- **验证**：调用srd_c_dll_registry_get()获取非空列表；调用srd_c_dll_info_get("spi_c")返回有效条目且file_path包含"spi_c"

### 2.5 路径配置增强 — 多自定义路径支持
- [ ] 修改 `libsigrokdecode4DSL/decoder.c`：
  - 将 `static char* c_decoder_path`（第1305行）替换为 `static GSList *c_decoder_paths = NULL`
  - 改造 `srd_c_decoder_path_set()`：清空c_decoder_paths列表，若path非NULL则添加单个路径（保持向后兼容）
  - 新增 `SRD_API int srd_c_decoder_path_add(const char *path)`：去重检查后追加到c_decoder_paths
  - 新增 `SRD_API void srd_c_decoder_paths_clear(void)`：g_slist_free_full释放所有路径
  - 改造 `srd_c_decoder_load_all()` 中搜索路径构建（第1320-1321行）：遍历c_decoder_paths追加到search_paths_list
- [ ] 在 `libsigrokdecode4DSL/libsigrokdecode.h` 中添加：
  - `SRD_API int srd_c_decoder_path_add(const char *path);`
  - `SRD_API void srd_c_decoder_paths_clear(void);`
- **依赖**：2.1
- **验证**：调用srd_c_decoder_path_add()添加2个路径后，srd_c_decoder_load_all()能搜索到两个路径下的DLL

### 2.6 srd_exit()集成DLL注册表清理
- [ ] 修改 `libsigrokdecode4DSL/srd.c` 中的 `srd_exit()` 函数（第337行后）：
  - 在 `srd_decoder_unload_all()` 之后添加 `srd_c_dll_registry_cleanup()` 调用
  - 确保srd_exit时所有DLL句柄被正确释放
- [ ] 在 `libsigrokdecode4DSL/srd.c` 顶部添加 `#include "dll_registry.h"`
- **依赖**：1.2, 2.1
- **验证**：应用退出时，日志中可见所有DLL被FreeLibrary/dlclose释放的记录

### 2.7 安全约束 — 热加载路径验证
- [ ] 在 `srd_c_decoder_load()` 实现中添加路径安全检查：
  - 校验dll_path必须为绝对路径（g_path_is_absolute），相对路径返回SRD_ERR_ARG并记录警告
  - 不从工作目录./、PATH/LD_LIBRARY_PATH环境变量搜索DLL
  - 此约束已在srd_c_decoder_load_all()中隐式保证，仅需在热加载API中显式校验
- **依赖**：2.2
- **验证**：传入相对路径"./test.dll"时返回SRD_ERR_ARG，日志包含"must be absolute"警告

---

## 3. S3: CMake构建修复 — C_DECODERS补全至12项

### 3.1 C_DECODERS列表补全
- [ ] 修改 `CMakeLists.txt:774`，将：
  ```cmake
  set(C_DECODERS spi_c i2c_c uart_c can_c)
  ```
  改为：
  ```cmake
  set(C_DECODERS spi_c i2c_c uart_c can_c counter_c ds1307_c ds3231_c graycode_c lm75_c numbers_and_state_c pwm_c seven_segment_c)
  ```
  补全当前c_decoders/目录下已有的12个C decoder源文件
- **依赖**：无（可与S1并行）
- **验证**：执行完整构建，build.dir/decoders/c_decoders/目录下生成12个DLL/SO文件

### 3.2 Windows平台符号导出控制 — .def文件
- [ ] 新建 `libsigrokdecode4DSL/c_decoders/c_decoder.def`：
  ```
  LIBRARY
  EXPORTS
      srd_c_decoder_entry
      srd_c_decoder_api_version
  ```
- [ ] 修改 `CMakeLists.txt` 的C decoder foreach循环中，为Windows平台添加def文件：
  ```cmake
  if(WIN32)
      set_target_properties(decoder_${dec} PROPERTIES
          LINK_FLAGS "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/libsigrokdecode4DSL/c_decoders/c_decoder.def"
      )
  endif()
  ```
- **依赖**：3.1
- **验证**：Windows构建后，用dumpbin /exports检查DLL仅导出2个符号

---

## 4. S4: Tier-2 C Decoder重写 — 首批实现

### 4.1 分析Python usb_signalling decoder实现
- [ ] 阅读 `libsigrokdecode4DSL/decoders/usb_signalling/pd.py`，提取：
  - 通道定义（DP, DM）
  - 可配置选项（speed: low/full）
  - 注解类型及标签（SYNC, PID, DATA, CRC, ERROR等）
  - 注解行分组
  - 协议状态机逻辑（NRZI解码、bit-stuffing、PID校验、CRC5/16）
- **依赖**：无
- **验证**：产出Python decoder行为摘要文档（可在代码注释中体现）

### 4.2 实现usb_signalling_c — C Decoder骨架
- [ ] 新建 `libsigrokdecode4DSL/c_decoders/usb_signalling_c.c`，包含：
  - user_data结构体 `struct usb_sig_context`：状态机变量（state, speed, nrzi_state, bit_stuff_count, packet数据缓冲区等）
  - 通道定义：DP(D+, order=0), DM(D-, order=1)
  - 可选通道：无
  - 注解标签 `ann_labels[][3]`：对齐Python版本的SYNC/PID/DATA/CRC/ERROR等
  - annotation_rows定义：对齐Python版本
  - options定义：speed选项（low/full）
  - srd_c_decoder结构体完整初始化
  - DLL导出函数：srd_c_decoder_entry()、srd_c_decoder_api_version()
- **依赖**：4.1
- **验证**：编译为DLL后，srd_c_decoder_load()可成功加载，srd_decoder_list()中出现usb_signalling_c

### 4.3 实现usb_signalling_c — 回调函数逻辑
- [ ] 在 `usb_signalling_c.c` 中实现6个回调函数：
  - `usb_sig_reset()`：初始化状态机为IDLE，清空数据缓冲区
  - `usb_sig_start()`：读取speed选项，注册输出
  - `usb_sig_decode()`：主解码逻辑——
    - IDLE→SOP：检测K/J状态转换（低速与全速阈值不同）
    - SOP→PACKET：NRZI解码 + bit-stuffing去除
    - PACKET：累积数据字节，PID校验（高4位=取反低4位）
    - PACKET→EOP：检测SE0状态
    - CRC5校验（token包）、CRC16校验（数据包）
    - 通过c_decoder_put()输出注解
  - `usb_sig_end()`：可为NULL
  - `usb_sig_metadata()`：可为NULL
  - `usb_sig_destroy()`：释放user_data
- **依赖**：4.2
- **验证**：使用USB低速/全速采样数据运行解码，输出注解与Python版本语义等价

### 4.4 分析Python sdcard_sd decoder实现
- [ ] 阅读 `libsigrokdecode4DSL/decoders/sdcard_sd/pd.py`，提取：
  - 通道定义（CMD, DAT0-DAT3）
  - 可配置选项
  - 注解类型及标签（CMD, RSP, DATA, CRC, ERROR等）
  - 注解行分组
  - 协议状态机逻辑（命令索引、参数解析、CRC7/CRC16校验、响应类型）
- **依赖**：无
- **验证**：产出Python decoder行为摘要文档

### 4.5 实现sdcard_sd_c — C Decoder骨架
- [ ] 新建 `libsigrokdecode4DSL/c_decoders/sdcard_sd_c.c`，包含：
  - user_data结构体 `struct sdcard_sd_context`：状态机变量（state, cmd_index, cmd_arg, response_type, 数据缓冲区等）
  - 通道定义：CMD(order=0), DAT0(order=1)
  - 可选通道：DAT1(order=2), DAT2(order=3), DAT3(order=4)（宽总线模式）
  - 注解标签 `ann_labels[][3]`：对齐Python版本的CMD/RSP/DATA/CRC/ERROR等
  - annotation_rows定义：对齐Python版本
  - options定义：对齐Python版本选项
  - srd_c_decoder结构体完整初始化
  - DLL导出函数
- **依赖**：4.4
- **验证**：编译为DLL后可成功加载

### 4.6 实现sdcard_sd_c — 回调函数逻辑
- [ ] 在 `sdcard_sd_c.c` 中实现6个回调函数：
  - `sdcard_sd_reset()`：初始化状态机为IDLE
  - `sdcard_sd_start()`：注册输出
  - `sdcard_sd_decode()`：主解码逻辑——
    - IDLE→CMD：检测命令起始位，累积48位命令包（cmd_index + arg + crc7）
    - CMD→RSP：等待响应，根据cmd_index确定响应类型（R1/R2/R3/R6/R7）
    - RSP→DATA：数据传输阶段，累积数据块+CRC16
    - CRC7校验（命令/响应）、CRC16校验（数据块）
    - 命令参数解析与注解输出
  - `sdcard_sd_end()`：可为NULL
  - `sdcard_sd_metadata()`：可为NULL
  - `sdcard_sd_destroy()`：释放user_data
- **依赖**：4.5
- **验证**：使用SD卡命令/响应采样数据运行解码，输出注解与Python版本语义等价

### 4.7 新增C Decoder注册到CMake构建
- [ ] 修改 `CMakeLists.txt:774`，在C_DECODERS列表末尾追加：`usb_signalling_c sdcard_sd_c`
- **依赖**：3.1, 4.3, 4.6
- **验证**：完整构建后，build.dir/decoders/c_decoders/目录下新增usb_signalling_c.dll和sdcard_sd_c.dll

### 4.8 后续Tier-2 C Decoder重写任务（待首批验证后展开）
- [ ] **sdcard_spi_c**（优先级3）：SPI模式SD卡协议，通道CS/CLK/MOSI/MISO，状态机IDLE→CMD→RSP→DATA
- [ ] **swd_c**（优先级4）：Serial Wire Debug协议，通道SWCLK/SWDIO，状态机IDLE→LINE_RESET→PACKET→ACK→DATA
- [ ] **jtag_c**（优先级5）：JTAG协议，通道TCK/TMS/TDI/TDO，TAP 16状态机
- [ ] **nrf_c**（优先级6）：Nordic nRF SPI协议，通道CE/CSN/SCK/MOSI/MISO，Enhanced ShockBurst
- [ ] **onewire_c**（优先级7）：1-Wire协议，通道OWRX，复位/存在检测+ROM搜索+CRC8

---

## 5. S5: 版本兼容与规范

### 5.1 新增SRD_C_DECODER_API_MIN_VERSION宏定义
- [ ] 修改 `libsigrokdecode4DSL/libsigrokdecode.h`：
  - 在 `SRD_C_DECODER_API_VERSION` 定义（第364行）后新增：
    ```c
    #define SRD_C_DECODER_API_MIN_VERSION 2  /* 最小兼容API版本 */
    ```
- **依赖**：无
- **验证**：C decoder源文件include libsigrokdecode.h后可使用SRD_C_DECODER_API_MIN_VERSION宏

### 5.2 实现版本兼容校验函数
- [ ] 修改 `libsigrokdecode4DSL/decoder.c`，新增内部函数：
  ```c
  static int srd_c_decoder_check_version(int dll_version)
  ```
  逻辑：
  - dll_version == SRD_C_DECODER_API_VERSION → 返回SRD_OK（完全匹配）
  - SRD_C_DECODER_API_MIN_VERSION <= dll_version < SRD_C_DECODER_API_VERSION → 返回SRD_OK（兼容模式，记录info日志）
  - 其他情况 → 返回SRD_ERR（不兼容，记录warn日志）
- [ ] 修改 `srd_c_decoder_load_single()` 中的版本校验逻辑，将原有的 `version_func() != SRD_C_DECODER_API_VERSION` 严格匹配替换为调用 `srd_c_decoder_check_version()`
- **依赖**：5.1, 2.1
- **验证**：构建一个报告API_VERSION=2的DLL在API_VERSION=2、MIN_VERSION=2环境下正常加载；若将MIN_VERSION改为3，报告版本2的DLL被拒绝并输出兼容性日志

### 5.3 编写C Decoder实现规范文档
- [ ] 新建 `libsigrokdecode4DSL/c_decoders/C_DECODER_SPEC.md`，内容包含：
  - **必须实现项**（8项）：srd_c_decoder结构体完整初始化、reset()、start()、decode()、end()、metadata()、destroy()、DLL导出函数
  - **推荐实现项**（3项）：options定义、annotation_rows定义、optional_channels定义
  - **命名规则**：C decoder id = Python版本id + "_c"后缀；变体通过options而非单独decoder支持
  - **ann_labels[3]规范**：类型ID + 短名 + 描述三元素格式
  - **版本规范**：srd_c_decoder_api_version()必须返回SRD_C_DECODER_API_VERSION
  - **禁止项**：禁止decode()中调用Python C API；禁止DLL隐式链接主程序符号；禁止破坏输出兼容性
- **依赖**：5.1
- **验证**：文档内容覆盖8项必须实现+3项推荐实现+命名规则+禁止项，新开发者参照此文档可独立实现C decoder

---

## 6. 集成验证与测试

### 6.1 DLL注册表功能集成测试
- [ ] 启动应用后验证：
  - 调用srd_c_dll_registry_count()，返回值应等于C_DECODERS列表中的decoder数量（12 + 新增数量）
  - 调用srd_c_dll_info_get("spi_c")，验证file_path、api_version、status字段正确
  - 检查日志中无重复加载警告（去重功能生效）
- **依赖**：2.1, 2.4, 3.1
- **验证**：所有已注册C decoder的DLL信息完整可查

### 6.2 热加载/卸载功能端到端测试
- [ ] 验证热加载流程：
  - 应用运行中，将一个新编译的C decoder DLL复制到非搜索路径
  - 调用srd_c_decoder_load(绝对路径)，验证新decoder出现在srd_decoder_list()
  - 创建该decoder的实例并运行解码，验证功能正常
- [ ] 验证安全卸载流程：
  - 销毁所有使用该decoder的实例
  - 调用srd_c_decoder_unload(decoder_id)，验证返回SRD_OK
  - 验证srd_decoder_list()不再包含该decoder
  - 验证再次加载同一DLL成功
- [ ] 验证卸载保护：
  - 创建decoder实例后，调用srd_c_decoder_unload()，验证返回SRD_ERR_ARG
- **依赖**：2.2, 2.3
- **验证**：热加载→创建实例→解码→销毁实例→卸载 全流程无崩溃

### 6.3 CMake构建完整性验证
- [ ] 执行 `build_full.cmd` 完整构建，验证：
  - build.dir/decoders/c_decoders/目录下DLL数量 = C_DECODERS列表长度
  - 每个DLL可被srd_c_decoder_load()成功加载
  - Windows平台下DLL仅导出2个符号（srd_c_decoder_entry + srd_c_decoder_api_version）
- **依赖**：3.1, 3.2, 4.7
- **验证**：构建成功且所有DLL功能正常

### 6.4 usb_signalling_c功能对齐验证
- [ ] 使用同一组USB低速采样数据，分别运行Python usb_signalling和C usb_signalling_c decoder
- [ ] 对比两者的注解输出ann_class序列，应完全一致
- [ ] 对比ann_text内容，语义应等价（格式允许差异）
- [ ] 使用全速采样数据重复上述验证
- **依赖**：4.3
- **验证**：C与Python版本输出一致性≥95%（核心注解类型100%一致）

### 6.5 sdcard_sd_c功能对齐验证
- [ ] 使用同一组SD卡命令/响应采样数据，分别运行Python sdcard_sd和C sdcard_sd_c decoder
- [ ] 对比注解输出ann_class序列
- [ ] 对比CMD/RSP注解中的命令索引和参数解析结果
- **依赖**：4.6
- **验证**：C与Python版本输出一致性≥95%

### 6.6 版本兼容性验证
- [ ] 构建一个SRD_C_DECODER_API_VERSION=2的DLL，在当前引擎（API_VERSION=2, MIN_VERSION=2）下加载成功
- [ ] 修改引擎SRD_C_DECODER_API_VERSION=3, MIN_VERSION=2，验证版本2的DLL在兼容模式下加载成功并记录info日志
- [ ] 修改引擎SRD_C_DECODER_API_MIN_VERSION=3，验证版本2的DLL被拒绝并输出warn日志
- **依赖**：5.2
- **验证**：版本协商逻辑三种场景（完全匹配/兼容/不兼容）行为正确

### 6.7 应用退出资源释放验证
- [ ] 启动应用后正常退出，检查日志中包含所有DLL的FreeLibrary/dlclose记录
- [ ] 使用内存检测工具（如Valgrind/ASAN）验证无DLL相关内存泄漏
- **依赖**：2.6
- **验证**：退出时无资源泄漏，无崩溃

---

## 任务依赖关系总览

```
S1(1.1→1.2→1.3) ──┐
                    ├── S2(2.1→2.2→2.3→2.4→2.5→2.6→2.7)
                    │
S3(3.1→3.2) ───────┤
                    │
S5(5.1→5.2→5.3) ──┤
                    │
S4(4.1→4.2→4.3, 4.4→4.5→4.6, 4.7) ──┘
                    │
                集成验证(6.1→6.2→6.3→6.4→6.5→6.6→6.7)
```

**关键路径**：S1 → S2.1(提取公共加载函数) → S2.2/S2.3(热加载/卸载) → 集成验证
**并行路径**：S3(CMake补全) ∥ S5.1(版本宏) ∥ S4.1/S4.4(Python分析)
