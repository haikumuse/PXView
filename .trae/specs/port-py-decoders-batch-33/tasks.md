# 移植任务分解 (Batch 33)

## 任务总览

本批次共 5 个解码器移植任务，按依赖关系排序。onewire_network_c 是 ds2408_c、ds243x_c、ds28ea00_c 的前置依赖，必须先完成。

---

## 任务 1：onewire_network_c

**优先级**：🔴 高（阻塞任务 2/3/4）

**文件**：`libsigrokdecode/c_decoders/onewire_network_c.c`

**预估复杂度**：中等

### 子任务

#### 1.1 创建文件框架
- 创建 `onewire_network_c.c`
- 添加标准头文件 (`stdio.h`, `stdlib.h`, `string.h`, `glib.h`, `libsigrokdecode.h`)
- 定义注解枚举和标签
- 定义 `srd_c_ann_row` 行配置
- 定义 `inputs`/`outputs`/`tags` 数组

#### 1.2 实现私有状态结构
- 定义 `enum own_state`（5 个状态）
- 定义 `enum own_search_phase`（3 个搜索阶段）
- 定义 `struct own_priv`（state, bit_cnt, search, data_p, data_n, data, rom, ss_block, es_block, out_ann, out_python）

#### 1.3 实现 ROM 命令查找表
- 定义 `struct rom_command`（code, name, next_state）
- 填充 10 个 ROM 命令（0x33, 0x0f, 0xcc, 0x55, 0xf0, 0xec, 0x3c, 0x69, 0xa5, 0x96）

#### 1.4 实现 reset 函数
- 分配 `own_priv`（如未分配）
- `memset` 清零
- 初始状态：`STATE_COMMAND`，`bit_cnt = 0`，`search = SEARCH_PHASE_P`

#### 1.5 实现 start 函数
- 注册 `SRD_OUTPUT_ANN`（输出类型 "onewire_network"）
- 注册 `SRD_OUTPUT_PYTHON`（输出类型 "onewire_network"）

#### 1.6 实现 recv_proto 函数
- 处理 `"RESET/PRESENCE"` 命令：输出注解 + 转发协议数据 + 重置状态
- 处理 `"BIT"` 命令：
  - `STATE_COMMAND`：收集 8 bit → 查找 ROM 命令 → 转换状态
  - `STATE_GET_ROM`：收集 64 bit → 输出 ROM 注解 + 协议数据 → 转入 TRANSPORT
  - `STATE_SEARCH_ROM`：三态循环收集 64 bit → 输出 ROM → 转入 TRANSPORT
  - `STATE_TRANSPORT`：收集 8 bit → 输出 DATA 注解 + 协议数据
  - `STATE_COMMAND_ERROR`：收集 8 bit → 输出错误注解

#### 1.7 实现 decode 函数
- 空函数（上层解码器不直接解析原始信号）

#### 1.8 实现 destroy 函数
- 释放 `own_priv`

#### 1.9 定义 srd_c_decoder 结构体
- `.id = "onewire_network_c"`
- `.name = "1-Wire network layer(C)"`
- `.inputs = {"onewire_link"}`
- `.outputs = {"onewire_network"}`
- `.recv_proto = onewire_network_recv_proto`
- `.decode = onewire_network_decode`（空函数）

#### 1.10 实现 srd_c_decoder_entry
- 无选项，直接返回 `&onewire_network_c_decoder`

#### 1.11 实现 srd_c_decoder_api_version
- 返回 `SRD_C_DECODER_API_VERSION`

### 验证要点
- [ ] RESET/PRESENCE 事件正确转发给上层
- [ ] ROM 命令查找正确（10 个已知命令 + 未识别命令）
- [ ] GET ROM 状态下 64 bit LSB first 收集正确
- [ ] SEARCH ROM 三态循环（P→N→D）bit_cnt 只在 D 阶段递增
- [ ] ROM 数据以 8 字节 LSB first 格式输出
- [ ] DATA 数据以 1 字节格式输出

---

## 任务 2：ds2408_c

**优先级**：🟡 中（依赖任务 1）

**文件**：`libsigrokdecode/c_decoders/ds2408_c.c`

**预估复杂度**：中等

### 子任务

#### 2.1 创建文件框架
- 标准头文件 + 注解/行定义
- `inputs = {"onewire_network"}`
- `outputs = NULL`（叶子解码器，无输出）

#### 2.2 实现私有状态结构
- `struct ds2408_priv`：bytes[256], num_bytes, ss, es, ss_block, out_ann

#### 2.3 实现 DS2408 功能命令查找表
- 6 个命令（0xf0, 0xf5, 0x5a, 0xcc, 0xc3, 0x3c）

#### 2.4 实现 recv_proto 函数
- `"RESET/PRESENCE"`：输出注解 + 清空 bytes
- `"ROM"`：提取 family code + 输出注解 + 清空 bytes
- `"DATA"`：累积到 bytes 数组 + 调用 handle_data

#### 2.5 实现 ds2408_handle_data 函数
- **0xF0 Read PIO Registers**：bytes[1..2]=地址, bytes[3+]=数据
- **0xF5 Channel Access Read**：bytes[2+]=PIO 采样
- **0x5A Channel Access Write**：bytes[1]=数据, bytes[2]=反码校验, bytes[3+]=0xAA/0xFF
- **0xCC Write Conditional Search Register**：bytes[1..2]=地址, bytes[3+]=数据
- **0xC3 Reset Activity Latches**：bytes[2+]=0xAA=成功/其他=无效

#### 2.6 实现其余框架函数
- reset / start / decode（空）/ destroy / entry / api_version

### 验证要点
- [ ] 0x5A Channel Access Write 的反码校验逻辑正确
- [ ] 0xF0/0xCC 的目标地址解析（小端序）正确
- [ ] 0xC3 的成功/失败判断正确
- [ ] RESET/PRESENCE 和 ROM 事件正确清空 bytes 数组

---

## 任务 3：ds243x_c

**优先级**：🟡 中（依赖任务 1）

**文件**：`libsigrokdecode/c_decoders/ds243x_c.c`

**预估复杂度**：🔴 高（命令最多，含 CRC-16，数据累积最复杂）

### 子任务

#### 3.1 创建文件框架
- 标准头文件 + 注解/行定义 + binary 定义
- `inputs = {"onewire_network"}`
- `outputs = NULL`
- binary：`{0, "mem_read", "Data read from memory"}`

#### 3.2 实现 CRC-16 函数
- 初始值 0x0000，反转多项式 0xA001，异或输出 0xFFFF
- 与 Python 版本 `crc16()` 完全一致

#### 3.3 实现私有状态结构
- `struct ds243x_priv`：bytes[64], num_bytes, family_code, family[16], out_ann, out_binary, ss, es, ss_block

#### 3.4 实现家族代码识别
- 0x33 → DS2432（7 个命令）
- 0x23 → DS2433（4 个命令）
- 其他 → 未知

#### 3.5 实现 recv_proto 函数
- `"RESET/PRESENCE"`：输出注解 + 清空 bytes
- `"ROM"`：提取 family_code + 识别设备 + 选择命令集 + 清空 bytes
- `"DATA"`：累积 + 根据命令解析

#### 3.6 实现各命令解析逻辑

**0x0F Write scratchpad**：
- bytes[1..2]：目标地址
- bytes[3..10]：数据（8 字节）
- bytes[11..12]：CRC-16 校验

**0xAA Read scratchpad**：
- bytes[1..2]：目标地址
- bytes[3]：E/S 状态
- bytes[4..11]：数据（8 字节）
- bytes[12..13]：CRC-16 校验

**0x55 Copy scratchpad**：
- bytes[1..3]：授权模式 (TA1, TA2, E/S)
- bytes[4..23]：MAC（20 字节）
- 后续：0xAA/0x55=成功，0x00=失败

**0xF0 Read memory**：
- bytes[1..2]：目标地址
- bytes[3+]：数据，输出 binary

**0x5A Load first secret**：
- bytes[1..3]：授权模式
- 后续：0xAA/0x55=结束

**0x33 Compute next secret**：
- bytes[1..2]：目标地址
- 后续：0xAA/0x55=结束

**0xA5 Read authenticated page**：
- bytes[1..2]：目标地址
- bytes[3..34]：数据（32 字节）
- bytes[35]：padding（0xFF=正确）
- bytes[36..37]：CRC-16
- bytes[38..57]：MAC（20 字节）
- bytes[58..59]：MAC CRC-16
- 后续：0xAA/0x55=完成

#### 3.7 实现其余框架函数
- reset / start / decode（空）/ destroy / entry / api_version

### 验证要点
- [ ] CRC-16 计算与 Python 版本一致
- [ ] DS2432 vs DS2433 命令集切换正确
- [ ] 0xA5 Read authenticated page 的多段数据解析正确（最多 60+ 字节）
- [ ] 0xF0 Read memory 的 binary 输出正确
- [ ] MAC 数据格式化输出正确

---

## 任务 4：ds28ea00_c

**优先级**：🟢 低（依赖任务 1，逻辑最简单）

**文件**：`libsigrokdecode/c_decoders/ds28ea00_c.c`

**预估复杂度**：🟢 低

### 子任务

#### 4.1 创建文件框架
- 标准头文件 + 注解/行定义
- `inputs = {"onewire_network"}`
- `outputs = NULL`

#### 4.2 实现私有状态结构
- `struct ds28ea00_priv`：state, rom, ss, es, out_ann

#### 4.3 实现 DS28EA00 功能命令查找表
- 9 个命令（0x4e, 0xbe, 0x48, 0x44, 0xb4, 0xb8, 0xf5, 0xa5, 0x99）

#### 4.4 实现 recv_proto 函数
- `"RESET/PRESENCE"`：输出注解 + 状态→ROM
- `"ROM"`：存储 ROM + 输出注解 + 状态→COMMAND
- `"DATA"` + `STATE_COMMAND`：查找命令 + 转换状态
- `"DATA"` + `STATE_READ_SCRATCHPAD`：输出 scratchpad 数据
- `"DATA"` + `STATE_CONVERT_TEMPERATURE`：输出温度转换状态
- `"DATA"` + 其他状态：输出 TODO 注解

#### 4.5 实现状态名辅助函数
- `ds28ea00_state_name()`：将枚举值转为字符串

#### 4.6 实现其余框架函数
- reset / start / decode（空）/ destroy / entry / api_version

### 验证要点
- [ ] 9 个功能命令正确识别
- [ ] Read scratchpad 和 Convert temperature 状态有专门处理
- [ ] 其他命令状态输出 TODO 标记
- [ ] RESET/PRESENCE 正确重置状态到 ROM

---

## 任务 5：eeprom93xx_c

**优先级**：🟡 中（独立于任务 1-4，依赖 microwire_c）

**文件**：`libsigrokdecode/c_decoders/eeprom93xx_c.c`

**预估复杂度**：中等

### 子任务

#### 5.1 创建文件框架
- 标准头文件 + 注解/行定义 + binary 定义
- `inputs = {"microwire"}`
- `outputs = NULL`
- 3 个选项：addresssize(8), wordsize(16), format("hex"/"ascii")

#### 5.2 定义 mw_py_entry 结构
- 必须与 microwire_c.c 中的定义完全一致
- 字段：ss(uint64_t), es(uint64_t), si(int), so(int)

#### 5.3 实现私有状态结构
- `struct eeprom93xx_priv`：addresssize, wordsize, format, out_ann, out_binary

#### 5.4 实现 recv_proto 函数
- 只处理 `"microwire"` 命令
- 解析 mw_py_entry 数组
- 提取 opcode（前 2 个 SI bit）
- 根据 opcode 分发到各指令处理

#### 5.5 实现指令解析逻辑
- **opcode=2 (READ)**：输出地址 + 读取所有字（SO 数据）
- **opcode=1 (WRITE)**：输出地址 + 写入字（SI 数据）
- **opcode=3 (ERASE)**：输出地址
- **opcode=0**：根据 SI[2]/SI[3] 判断 WEN/WDS/ERAL/WRAL

#### 5.6 实现辅助函数
- `eeprom93xx_put_address()`：提取地址 + 输出注解 + 输出 binary
- `eeprom93xx_put_word()`：提取字数据 + hex/ascii 格式化 + 输出注解 + 输出 binary

#### 5.7 实现选项初始化
- `srd_c_decoder_entry()` 中初始化 3 个选项的默认值和可选值列表

#### 5.8 实现其余框架函数
- reset / start / decode（空）/ destroy / api_version

### 验证要点
- [ ] mw_py_entry 结构体与 microwire_c 完全一致（字段顺序、类型、对齐）
- [ ] 地址提取 MSB first 逻辑正确
- [ ] 字数据提取 MSB first 逻辑正确
- [ ] hex/ascii 格式化输出正确
- [ ] binary 输出（地址 1 字节 + 数据 2 字节）正确
- [ ] 3 个选项的默认值和可选值正确
- [ ] READ 指令支持连续读取多个字

---

## 任务 6：CMakeLists.txt 更新

**优先级**：🔴 高（所有解码器完成后执行）

**文件**：`CMakeLists.txt`

### 子任务

#### 6.1 在 C_DECODERS 列表中添加 5 个新解码器
```cmake
onewire_network_c
ds2408_c
ds243x_c
ds28ea00_c
eeprom93xx_c
```

#### 6.2 验证构建
- 运行 `build_incremental.cmd`
- 确认 5 个 DLL 成功生成到 `build.dir/decoders/c_decoders/`

---

## 执行顺序建议

```
任务 1 (onewire_network_c) ─── 必须 first，阻塞 2/3/4
  ├─ 任务 2 (ds2408_c)     ─── 可与 3/4 并行
  ├─ 任务 3 (ds243x_c)     ─── 最复杂，建议分配最多时间
  └─ 任务 4 (ds28ea00_c)   ─── 最简单，可快速完成
任务 5 (eeprom93xx_c)       ─── 独立，可与 1 并行
任务 6 (CMakeLists.txt)     ─── 最后执行
```

## 工时估算

| 任务 | 预估工时 | 复杂度 |
|---|---|---|
| 1. onewire_network_c | 2-3h | 中等 |
| 2. ds2408_c | 1.5-2h | 中等 |
| 3. ds243x_c | 3-4h | 高 |
| 4. ds28ea00_c | 1h | 低 |
| 5. eeprom93xx_c | 2-3h | 中等 |
| 6. CMakeLists.txt | 0.5h | 低 |
| **合计** | **10-13.5h** | |
