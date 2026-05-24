# 实现任务分解 — Batch 08 Python→C 解码器移植

## 任务总览

按实现难度从低到高排序，建议按此顺序执行。

---

## 任务 1: ir_recoil_c — Recoil 激光标签红外协议

**文件**: `libsigrokdecode/c_decoders/ir_recoil_c.c`
**难度**: 低
**预估时间**: 2-3 小时

### 子任务

- [ ] 1.1 创建文件骨架：头文件包含、枚举定义、结构体定义
- [ ] 1.2 定义通道数组 (1个必需通道: ir)
- [ ] 1.3 定义选项数组 (1个选项: polarity, active-low/active-high)
- [ ] 1.4 定义注解标签 (4个: sync, sync-pause, bit, packet)
- [ ] 1.5 定义注解行 (2行: bits, packets)
- [ ] 1.6 实现 calc_rate() — 计算8个时间参数
- [ ] 1.7 实现 metadata() 回调
- [ ] 1.8 实现 reset() 回调
- [ ] 1.9 实现 start() 回调 — 读取选项、注册输出、计算时间参数
- [ ] 1.10 实现 handle_bit() — 阈值区间判断 [daminimum,dathreshold)=0, [dathreshold,damaximum)=1
- [ ] 1.11 实现 decode() — 3状态机 (IDLE→SYNCING→DATA)
- [ ] 1.12 实现 destroy() 回调
- [ ] 1.13 实现 srd_c_decoder_entry() — 选项初始化
- [ ] 1.14 实现 srd_c_decoder_api_version()
- [ ] 1.15 在 CMakeLists.txt 的 C_DECODERS 列表中添加 ir_recoil_c
- [ ] 1.16 编译验证

### 关键细节

- 数据累积用字符串拼接 (data += str(bit))，输出 "0b10110010" 格式
- DATA 状态等待条件: `[{0: 'e'}, {'skip': damaximum + margin}]`
- 包注解范围: packetstartsample 到 oldedgesample + 1
- SYNC 脉冲需同时检查长度和 oldpinstate == activeState
- 无 Python 输出，无二进制输出
- license 为 "unknown"

---

## 任务 2: ir_ltto_c — LTTO 激光标签红外协议

**文件**: `libsigrokdecode/c_decoders/ir_ltto_c.c`
**难度**: 中
**预估时间**: 3-4 小时

### 子任务

- [ ] 2.1 创建文件骨架：头文件包含、枚举定义、结构体定义
- [ ] 2.2 定义通道数组 (1个必需通道: ir)
- [ ] 2.3 定义选项数组 (1个选项: polarity, active-low/active-high)
- [ ] 2.4 定义注解标签 (9个: pre-sync, pre-sync-pause, sync, long-sync, bit-pause, bit, signature, long-sync-signature, error)
- [ ] 2.5 定义注解行 (2行: bits, signatures)
- [ ] 2.6 实现 calc_rate() — 计算8个时间参数
- [ ] 2.7 实现 metadata() 回调
- [ ] 2.8 实现 reset() 回调
- [ ] 2.9 实现 start() 回调 — 读取选项、注册输出(ANN+PYTHON)、计算时间参数
- [ ] 2.10 实现 handle_bit() — 精确匹配 [dazero±margin]=0, [daone±margin]=1
- [ ] 2.11 实现 decode() — 5状态机 (IDLE→PSP→SYNC→BITPAUSE→BIT)
- [ ] 2.12 实现签名输出 — SHORT/LONG 签名注解和 Python 输出
- [ ] 2.13 实现错误注解输出
- [ ] 2.14 实现 destroy() 回调
- [ ] 2.15 实现 srd_c_decoder_entry() — 选项初始化
- [ ] 2.16 实现 srd_c_decoder_api_version()
- [ ] 2.17 在 CMakeLists.txt 的 C_DECODERS 列表中添加 ir_ltto_c
- [ ] 2.18 编译验证

### 关键细节

- 数据累积用位移: `data = (data << 1) | bit`，data 为 uint32_t
- 签名格式: `%d bits: 0x%03X` — 3位十六进制
- BIT/BITPAUSE 状态等待条件: `[{0: 'e'}, {'skip': bitpause + margin + margin}]`
- 需要 Python 输出: `['SHORT'/'LONG', count, data]`
- BITPAUSE 超时时根据 count 和 waslongsync 决定输出类型
- 位判断用精确范围匹配 (dazero±margin, daone±margin)，非阈值区间

---

## 任务 3: ir_rc6_c — RC-6 红外遥控协议

**文件**: `libsigrokdecode/c_decoders/ir_rc6_c.c`
**难度**: 高
**预估时间**: 4-6 小时

### 子任务

- [ ] 3.1 创建文件骨架：头文件包含、枚举定义、结构体定义
- [ ] 3.2 定义通道数组 (1个必需通道: ir)
- [ ] 3.3 定义选项数组 (1个选项: polarity, auto/active-low/active-high)
- [ ] 3.4 定义注解标签 (7个: bit, sync, startbit, field, togglebit, address, command)
- [ ] 3.5 定义注解行 (2行: bits, fields)
- [ ] 3.6 实现 metadata() 回调 — 计算 halfbit
- [ ] 3.7 实现 reset() 回调
- [ ] 3.8 实现 start() 回调 — 读取选项、注册输出
- [ ] 3.9 实现 handle_bit() — 处理前6位 (sync, startbit, field, togglebit)
- [ ] 3.10 实现 handle_package() — Mode 0, Mode 6A, Mode 6B 处理
- [ ] 3.11 实现 decode() — 3状态机 (IDLE→SYNC→DATA)
  - [ ] 3.11.1 边沿和增量跟踪
  - [ ] 3.11.2 同步模式检测 (deltas[-2:] == [6, 2])
  - [ ] 3.11.3 auto 极性处理
  - [ ] 3.11.4 DATA 状态位边界插入逻辑
  - [ ] 3.11.5 DATA 状态超时 (skip = halfbit * 6)
- [ ] 3.12 实现 destroy() 回调
- [ ] 3.13 实现 srd_c_decoder_entry() — 选项初始化 (含 auto 选项)
- [ ] 3.14 实现 srd_c_decoder_api_version()
- [ ] 3.15 在 CMakeLists.txt 的 C_DECODERS 列表中添加 ir_rc6_c
- [ ] 3.16 编译验证

### 关键细节

- 曼彻斯特编码，半位周期 = samplerate * 0.000889 / 2
- delta 四舍五入: `delta = int(delta + 0.5)`
- 同步模式: deltas [6, 2] (6个半位低 + 2个半位高)
- 位表示: (start_sample, end_sample, width_in_halfbits, value)
- 位边界插入: 当 deltas[-2] != deltas[-1] 时在 edges 列表中插入点
- auto 极性: 同步检测时 invert = (ir == 0)
- Mode 0: 22位 (8位地址 + 8位命令)
- Mode 6A: bits[6]==0, 8位地址 + 可变数据
- Mode 6B: bits[6]==1, 16位地址 + 可变数据
- 无 Python 输出，无二进制输出

---

## 任务 4: ir_irmp_c — IRMP 多协议红外遥控

**文件**: `libsigrokdecode/c_decoders/ir_irmp_c.c`
**难度**: 特殊 (依赖外部库)
**预估时间**: 4-6 小时

### 子任务

- [ ] 4.1 创建文件骨架：头文件包含、枚举定义、结构体定义
- [ ] 4.2 定义通道数组 (1个必需通道: ir)
- [ ] 4.3 定义选项数组 (1个选项: polarity, active-low/active-high)
- [ ] 4.4 定义注解标签 (1个: packet)
- [ ] 4.5 定义注解行 (1行: packets)
- [ ] 4.6 实现 irmp_load_library() — 动态加载 irmp.dll/libirmp.so
- [ ] 4.7 定义 ResultData 兼容结构体
- [ ] 4.8 实现 metadata() 回调
- [ ] 4.9 实现 reset() 回调
- [ ] 4.10 实现 start() 回调 — 加载库、验证采样率、计算 rate_factor
- [ ] 4.11 实现 irmp_putframe() — 格式化5级缩放注解
- [ ] 4.12 实现 decode() — 逐样本送入 IRMP 库
- [ ] 4.13 实现 destroy() 回调 — 释放库实例和句柄
- [ ] 4.14 实现 srd_c_decoder_entry() — 选项初始化
- [ ] 4.15 实现 srd_c_decoder_api_version()
- [ ] 4.16 在 CMakeLists.txt 的 C_DECODERS 列表中添加 ir_irmp_c
- [ ] 4.17 编译验证 (注意: 运行时需要 irmp.dll)

### 关键细节

- 动态加载外部库: Windows 用 LoadLibrary/GetProcAddress, Linux 用 dlopen/dlsym
- 采样率验证: 捕获采样率必须是库采样率的整数倍
- rate_factor = samplerate / lib_rate
- 逐样本送入: 使用 c_cond_skip(rate_factor)
- 极性处理: active-low 时 IR 值取反后送入库
- 注解格式: 5个缩放级别，含 Protocol/Address/Command/Flags
- Flags: repeat='rep'/'r', release='rel'/'R', 无标志='-'
- 库不可用时应优雅失败，不崩溃
- start_sample/end_sample 需乘以 rate_factor 转换

---

## 任务 5: ieee488_c — IEEE-488 GPIB/HPIB/IEC 总线

**文件**: `libsigrokdecode/c_decoders/ieee488_c.c`
**难度**: 极高
**预估时间**: 8-12 小时

### 子任务

- [ ] 5.1 创建文件骨架：头文件包含、枚举定义、结构体定义
- [ ] 5.2 定义通道数组 (1个必需通道: dio1)
- [ ] 5.3 定义可选通道数组 (16个: dio2-dio8, eoi, dav, nrfd, ndac, ifc, srq, atn, ren, clk)
- [ ] 5.4 定义选项数组 (2个: iec_periph, delim)
- [ ] 5.5 定义注解标签 (11个)
- [ ] 5.6 定义注解行 (7行)
- [ ] 5.7 定义二进制输出 (2个: raw, data)
- [ ] 5.8 实现辅助函数:
  - [ ] 5.8.1 bitpack() — 位数组打包为字节
  - [ ] 5.8.2 is_command(), is_listen_addr(), is_talk_addr(), is_secondary_addr(), is_msb_set()
  - [ ] 5.8.3 invert_pins() — 低电平有效取反
  - [ ] 5.8.4 get_data_text() — ASCII 控制码表 + 格式化
  - [ ] 5.8.5 get_command_texts() — 命令表查找
  - [ ] 5.8.6 get_address_texts() — 地址类型格式化
- [ ] 5.9 实现 metadata() 回调 (空，不需要 samplerate)
- [ ] 5.10 实现 reset() 回调
- [ ] 5.11 实现 start() 回调 — 读取选项、注册3种输出(ANN+BIN+PYTHON)、检测通道可用性
- [ ] 5.12 实现串行解码:
  - [ ] 5.12.1 decode_serial() — 4状态机
  - [ ] 5.12.2 串行等待条件构建
  - [ ] 5.12.3 ATN 下降沿重置逻辑
  - [ ] 5.12.4 位累积和字节组装
- [ ] 5.13 实现并行解码:
  - [ ] 5.13.1 decode_parallel() — 动态等待条件
  - [ ] 5.13.2 首次 'l' 触发 → 后续 'e' 触发
  - [ ] 5.13.3 处理顺序 (IFC→EOI→ATN→DAV→ATN→EOI→IFC)
- [ ] 5.14 实现数据字节处理:
  - [ ] 5.14.1 handle_data_byte() — ATN 激活/非激活分支
  - [ ] 5.14.2 handle_dav_change() — DAV 边沿处理
  - [ ] 5.14.3 inject_dav_phase() — 串行模式的 DAV 模拟
- [ ] 5.15 实现控制线处理:
  - [ ] 5.15.1 handle_ifc_change()
  - [ ] 5.15.2 handle_eoi_change()
  - [ ] 5.15.3 handle_atn_change()
- [ ] 5.16 实现文本累积器:
  - [ ] 5.16.1 flush_bytes_text_accu()
  - [ ] 5.16.2 check_extra_flush() — EOL 分隔逻辑
- [ ] 5.17 实现 IEC 外设处理 (可选):
  - [ ] 5.17.1 handle_iec_periph() — Commodore 磁盘地址映射
- [ ] 5.18 实现 Python 输出 (11种 ptype)
- [ ] 5.19 实现 decode() 入口 — 判断串行/并行模式
- [ ] 5.20 实现 destroy() 回调
- [ ] 5.21 实现 srd_c_decoder_entry() — 选项初始化
- [ ] 5.22 实现 srd_c_decoder_api_version()
- [ ] 5.23 在 CMakeLists.txt 的 C_DECODERS 列表中添加 ieee488_c
- [ ] 5.24 编译验证

### 关键细节

- 17 个通道是所有 C 解码器中最多的
- 双模式: has_clk 决定串行/并行
- 需要3种输出类型: ANN + BIN + PYTHON
- 所有信号低电平有效，需取反 (1-p)，但未连接引脚不取反
- 并行模式首次用 'l' 触发，后续改为 'e'
- 命令表需要完整映射 (10个已知命令 + UNL/UNT + 未知命令格式)
- ASCII 控制码表 (0x00-0x1f) 需要完整移植
- 文本累积器需要动态缓冲区管理
- IEC 外设仅在选项启用时工作
- 行终止符处理: CR(13)/LF(10) 后跟非终止符时刷新

---

## 任务 6: 集成验证

- [ ] 6.1 确认所有5个解码器在 CMakeLists.txt 中注册
- [ ] 6.2 执行完整编译 (build_incremental.cmd)
- [ ] 6.3 确认所有 DLL 生成到 build.dir/decoders/c_decoders/
- [ ] 6.4 在 PXView 中测试每个解码器的基本功能
