# Python→C 解码器移植验收清单 — Batch 29

## 通用验收标准（适用于所有 5 个解码器）

### 文件规范

- [ ] 文件名格式正确：`{decoder_id}_c.c`
- [ ] 文件位于 `libsigrokdecode/c_decoders/` 目录
- [ ] 包含标准头文件：`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<glib.h>`, `"libsigrokdecode.h"`
- [ ] 无多余头文件包含

### srd_c_decoder 结构体

- [ ] `.id` 格式为 `"{python_id}_c"`（如 `"arm_itm_c"`）
- [ ] `.name` 格式为 `"{Python Name}(C)"`（如 `"ARM ITM(C)"`）
- [ ] `.longname` 包含 `(C)` 后缀
- [ ] `.desc` 包含 `"(C implementation, faster than Python)"`
- [ ] `.license` = `"gplv2+"`
- [ ] `.channels` = `NULL`, `.num_channels` = `0`（UART 上层无直接通道）
- [ ] `.optional_channels` = `NULL`, `.num_optional_channels` = `0`
- [ ] `.inputs` = `{"uart", NULL}`, `.num_inputs` = `1`
- [ ] `.outputs` 正确设置（arm_tpiu_c 输出 `{"uart",NULL}`，其他按需）
- [ ] `.tags` 正确设置
- [ ] `.num_annotations` = `NUM_ANN`
- [ ] `.ann_labels` 第一列全部为 `""`
- [ ] `.annotation_rows` 中所有 ann_classes 数组以 `-1` 结尾
- [ ] `.binary` = `NULL`, `.num_binary` = `0`（除非特别需要）
- [ ] `.decode` 函数体为 `(void)di;`
- [ ] `.recv_proto` 函数指针已设置
- [ ] `.metadata` = `NULL`（除非特别需要）

### 导出函数

- [ ] `srd_c_decoder_entry()` 函数存在且返回 `&xxx_c_decoder`
- [ ] `srd_c_decoder_api_version()` 函数存在且返回 `SRD_C_DECODER_API_VERSION`
- [ ] 两个函数均有 `SRD_C_DECODER_EXPORT` 前缀
- [ ] 选项默认值在 `srd_c_decoder_entry()` 中初始化（使用 `g_variant_new_*`）
- [ ] 选项枚举值列表在 `srd_c_decoder_entry()` 中构建（使用 `g_slist_append`）

### 生命周期函数

- [ ] `reset`：使用 `g_malloc0` 分配私有状态（首次），`memset` 清零
- [ ] `start`：注册输出（`c_decoder_register_output`），读取选项
- [ ] `decode`：空函数体 `(void)di;`
- [ ] `destroy`：`g_free` 释放私有状态，`c_decoder_set_private(di, NULL)`

### recv_proto 函数

- [ ] 函数签名正确：`void xxx_recv_proto(struct srd_decoder_inst *di, uint64_t start_sample, uint64_t end_sample, const char *cmd, const unsigned char *data, uint64_t data_len)`
- [ ] 首先获取私有状态：`xxx_state *s = (xxx_state *)c_decoder_get_private(di);`
- [ ] 检查 `s` 非 NULL
- [ ] 只处理 `strcmp(cmd, "DATA") == 0` 的消息
- [ ] 正确提取 `data[0]`（字节值）和 `data[1]`（rxtx 方向）
- [ ] 处理 `data_len` 边界情况

### 注解输出

- [ ] 使用 `C_ANN_PUT` 宏输出注解
- [ ] 注解类使用 enum 常量而非魔法数字
- [ ] 字符串使用局部变量（栈上），不使用动态分配

### Python 输出（如有）

- [ ] 使用 `c_decoder_put_python` 输出
- [ ] cmd 字符串与 uart_c.c 输出格式一致（`"DATA"` 等）
- [ ] data 字节数组正确编码

### 编译

- [ ] 无编译错误
- [ ] 无编译警告（`-Wall -Wextra`）
- [ ] DLL 成功生成到 `build.dir/decoders/c_decoders/`

---

## crsf_c 专项验收

- [ ] Sync byte 查找表包含 4 种：0xEE, 0xEA, 0xC8, 0xEC
- [ ] Frame type 查找表包含 Python 中定义的所有类型
- [ ] 状态机 4 个状态正确实现：WAIT_SYNC → WAIT_LENGTH → WAIT_TYPE → RECV_PAYLOAD
- [ ] Length byte 范围验证：2 ≤ len ≤ 62
- [ ] RC Channels Packed 解码：16 通道 × 11 位
- [ ] Link Statistics 解码：10 字节 payload
- [ ] 未知帧类型输出 raw hex 而非崩溃
- [ ] 无选项（`.num_options = 0`）
- [ ] 输出为空（`.outputs = NULL, .num_outputs = 0`）
- [ ] Tags 包含 `"radio"`, `"control"`, `"RC"`
- [ ] Python 版本 `RX=0, TX=0` bug 已修正（C 版本正确处理 rxtx）

---

## bluetooth_h4_c 专项验收

- [ ] 18 个注解类（9 RX + 9 TX）全部定义
- [ ] 4 个注解行：rx, rx-bins, tx, tx-bins
- [ ] RX 注解类偏移 0，TX 注解类偏移 9
- [ ] HCI 命令名称查找表完整（~80 条）
- [ ] 4 种包类型解析正确：
  - [ ] CMD (0x01)：4 字节头（type+OGF/OCF+length）
  - [ ] ACL (0x02)：5 字节头（type+handle+length×2）
  - [ ] SCO (0x03)：4 字节头（type+handle+length）
  - [ ] EVENT (0x04)：3 字节头（type+event+length）
- [ ] Junk 字节（< 0x01 或 > 0x04）正确处理
- [ ] packet_length 递减逻辑正确
- [ ] CMD 包输出包含 HCI 命令名称
- [ ] 输出协议为 `"bluetooth_h4"`
- [ ] 注册了 out_python 输出
- [ ] 无选项（`.num_options = 0`）
- [ ] Tags 包含 `"Embedded/bluetooth"`

---

## boost_c 专项验收

- [ ] 3 个注解类：ANN_MESSAGE, ANN_ERROR, ANN_BYTES
- [ ] 3 个注解行：messages, errors, bytes
- [ ] 2 个选项：show_errors (默认 "no"), show_bytes (默认 "no")
- [ ] 选项枚举值包含 "yes"/"no"
- [ ] XOR 校验和函数正确实现
- [ ] 消息类型/长度查找表包含 15 种消息
- [ ] LEGO 颜色查找表（11 种颜色）
- [ ] LEGO 传感器模式查找表
- [ ] RX/TX 双方向独立缓冲区
- [ ] show_errors 选项过滤 ANN_ERROR 输出
- [ ] show_bytes 选项过滤 ANN_BYTES 输出
- [ ] Motor Init (0x54) 特殊处理：与固定序列比较而非校验和
- [ ] Python handlers.py 的 `handle_message_CF` 语法错误已修正
- [ ] 输出协议为 `"boost"`
- [ ] Tags 包含 `"Embedded/industrial"`

---

## arm_tpiu_c 专项验收

- [ ] 2 个注解类：ANN_STREAM, ANN_DATA
- [ ] 2 个注解行：stream, data
- [ ] 2 个选项：stream (默认 1), sync_offset (默认 0)
- [ ] **输出协议为 `"uart"`**（关键！arm_itm_c 需要堆叠）
- [ ] 注册了 out_python 输出，协议 ID 为 `"uart"`
- [ ] 16 字节帧缓冲区正确管理
- [ ] 同步检测：0xFF, 0xFF, 0xFF, 0x7F
- [ ] 帧处理核心逻辑：
  - [ ] Byte 15 低 4 位提取 lowbits
  - [ ] 偶索引字节 bit0=1 → stream ID
  - [ ] 偶索引字节 bit0=0 → data byte
  - [ ] 奇索引字节始终为 data
  - [ ] lowbit 正确合并到 data byte
- [ ] 延迟流切换逻辑正确
- [ ] stream_changed 注解输出
- [ ] emit_byte 过滤当前 stream
- [ ] emit_byte 输出 Python 格式：`c_decoder_put_python(di, ss, es, out_python, "DATA", py_data, 2)` 其中 py_data[0]=byte, py_data[1]=0
- [ ] sync_offset 选项跳过前 N 字节
- [ ] 超时重置（字节间隔 > byte_len）
- [ ] Tags 包含 `"Debug/trace"`

---

## arm_itm_c 专项验收

- [ ] 12 个注解类全部定义
- [ ] 9 个注解行全部定义
- [ ] 无选项（`.num_options = 0`）— objdump/ELF 功能省略
- [ ] 无输出协议（`.outputs = NULL, .num_outputs = 0`）
- [ ] ARM 异常名称查找表（16 条）
- [ ] get_packet_type 函数 8 种返回值
- [ ] 同步检测：0x00×5 + 0x80
- [ ] 超时重置：16 × byte_len
- [ ] 包类型处理：
  - [ ] sync：检测到后清空 buf
  - [ ] overflow：输出 "Overflow"
  - [ ] timestamp：1~5 字节变长，7-bit 编码，TC 字段解析
  - [ ] software：plen=(0,1,2,4)[buf[0]&0x03]，pid=buf[0]>>3
  - [ ] hardware：多子类型
    - [ ] pid=0：DWT events（Cyc/Fold/LSU/Sleep/Exc/CPI）
    - [ ] pid=1：Exception trace（Enter/Exit/Resume）
    - [ ] pid=2：PC sample（32-bit 值）
    - [ ] 0x84：Data watchpoint
    - [ ] 0x47：PC watchpoint
    - [ ] 0x4E：Address watchpoint offset
  - [ ] sw_extension/hw_extension/reserved：fallback 输出
- [ ] 模式追踪：thread/IRQ/exception，正确输出 ANN_MODE_*
- [ ] fallback 函数输出 "Unhandled {type}: hex..."
- [ ] 简化决策文档化：
  - [ ] objdump/ELF 功能省略
  - [ ] software 字符拼接简化
  - [ ] ANN_LOCATION/ANN_FUNCTION 保留但不输出
- [ ] Tags 包含 `"Debug/trace"`

---

## CMakeLists.txt 修改验收

- [ ] `C_DECODERS` 列表包含 `arm_itm_c`
- [ ] `C_DECODERS` 列表包含 `arm_tpiu_c`
- [ ] `C_DECODERS` 列表包含 `bluetooth_h4_c`
- [ ] `C_DECODERS` 列表包含 `boost_c`
- [ ] `C_DECODERS` 列表包含 `crsf_c`
- [ ] 修改不影响现有解码器编译

---

## 集成测试验收

### 编译测试

- [ ] `build_incremental.cmd` 执行成功
- [ ] 5 个 DLL 文件生成到 `build.dir/decoders/c_decoders/`
- [ ] 无编译错误
- [ ] 无编译警告

### 加载测试

- [ ] PXView 启动无崩溃
- [ ] 解码器列表中出现 `ARM ITM(C)`
- [ ] 解码器列表中出现 `ARM TPIU(C)`
- [ ] 解码器列表中出现 `Bluetooth H4(C)`
- [ ] 解码器列表中出现 `Boost(C)`
- [ ] 解码器列表中出现 `CRSF(C)`

### 堆叠测试

- [ ] UART → arm_tpiu_c → arm_itm_c 堆叠可创建
- [ ] UART → bluetooth_h4_c 可运行
- [ ] UART → boost_c 可运行
- [ ] UART → crsf_c 可运行

### 功能对比测试

- [ ] crsf_c：与 Python crsf 输出基本一致
- [ ] bluetooth_h4_c：与 Python bluetooth_h4 输出基本一致
- [ ] boost_c：与 Python boost 输出基本一致
- [ ] arm_tpiu_c：与 Python arm_tpiu 输出基本一致
- [ ] arm_itm_c：与 Python arm_itm 输出基本一致（允许 objdump 简化差异）

---

## 已知简化与偏差记录

| 解码器 | 简化项 | 原因 | 影响 |
|--------|--------|------|------|
| arm_itm_c | 省略 objdump/ELF 功能 | C 不能调用 subprocess | 无 PC→函数/文件映射 |
| arm_itm_c | 省略 software 字符拼接 | 简化实现 | 每个 software 包独立输出 |
| arm_itm_c | ANN_LOCATION/ANN_FUNCTION 不输出 | 依赖 objdump | 这两个注解行始终为空 |
| crsf_c | CRC8 校验暂不实现 | Python 版本也未实现 | 不验证数据完整性 |
| crsf_c | 修正 Python RX=0,TX=0 bug | Python 代码错误 | C 版本正确处理方向 |
| boost_c | 修正 handlers.py CF 语法错误 | Python 代码错误 | C 版本正确处理 0xCF 消息 |
| bluetooth_h4_c | HCI 命令表线性搜索 | 简化实现 | 性能影响可忽略（表小） |
