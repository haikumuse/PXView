# Python 解码器移植任务 — Batch 02

## 任务概述

将 5 个 Python 协议解码器移植为 C 解码器，按照复杂度从低到高的顺序实现。

---

## 任务 1: SpaceWire 解码器 (spacewire_c)

**优先级**: 高（最简单，建议先实现）  
**预估工时**: 4-6 小时  
**C 文件**: `libsigrokdecode/c_decoders/spacewire_c.c`

### 子任务

- [ ] 1.1 创建 `spacewire_c.c` 文件骨架（头文件、结构体、枚举、通道定义、注释标签）
- [ ] 1.2 实现 `reset()` / `start()` / `destroy()` 函数
- [ ] 1.3 实现 IDLE 状态：Data-Strobe 边沿检测，NULL 码模式匹配 (0b1110100)
- [ ] 1.4 实现 SYNC 状态：index==1 时 DCF 检测和奇偶校验
- [ ] 1.5 实现 SYNC 状态：index==char_len 时控制字符解码（FCT/ESC/EEP/EOP）
- [ ] 1.6 实现 SYNC 状态：数据字符解码（8 位反转）
- [ ] 1.7 实现控制码检测：NULL (ESC+FCT)、Time (ESC+数据字符)
- [ ] 1.8 实现 `srd_c_decoder` 结构体和 `srd_c_decoder_entry()` 导出
- [ ] 1.9 在 CMakeLists.txt 中注册 `spacewire_c`
- [ ] 1.10 编译测试

### 关键依赖

- 无 samplerate 依赖
- 2 通道（Data + Strobe）
- 无选项
- 无 OUTPUT_PYTHON（Python 版本也未注册）

---

## 任务 2: IEBus 解码器 (iebus_c)

**优先级**: 高  
**预估工时**: 6-8 小时  
**C 文件**: `libsigrokdecode/c_decoders/iebus_c.c`

### 子任务

- [ ] 2.1 创建 `iebus_c.c` 文件骨架
- [ ] 2.2 实现 `reset()` / `start()` / `metadata()` / `destroy()` 函数
- [ ] 2.3 实现 `read_bits()` — 核心位读取函数（27µs 采样偏移，33µs 位长度）
- [ ] 2.4 实现 `read_header()` — 开始位检测（>= 100µs 宽度）
- [ ] 2.5 实现 `read_broadcast_bit()` — 广播位读取
- [ ] 2.6 实现 `read_parity_bit()` — 奇偶校验（popcount）
- [ ] 2.7 实现 `read_ack_bit()` — ACK/NAK 检测
- [ ] 2.8 实现主解码循环：header → master addr → slave addr → control → data_len → data_bytes
- [ ] 2.9 实现 Commands 枚举查找和名称输出
- [ ] 2.10 实现 OUTPUT_PYTHON 输出（HEADER/MASTER ADDRESS/SLAVE ADDRESS/CONTROL/DATA LENGTH/DATA/NAK）
- [ ] 2.11 实现总线极性选项（idle-low / idle-high）
- [ ] 2.12 实现 NAK 处理和 ignore_nak 选项
- [ ] 2.13 实现 `srd_c_decoder` 结构体和导出
- [ ] 2.14 在 CMakeLists.txt 中注册 `iebus_c`
- [ ] 2.15 编译测试

### 关键依赖

- 需要 samplerate（27µs 和 33µs 时序计算）
- 1 通道 + 3 选项
- 需要 OUTPUT_PYTHON

---

## 任务 3: FlexRay 解码器 (flexray_c)

**优先级**: 高  
**预估工时**: 8-10 小时  
**C 文件**: `libsigrokdecode/c_decoders/flexray_c.c`

### 子任务

- [ ] 3.1 创建 `flexray_c.c` 文件骨架
- [ ] 3.2 实现 `reset()` / `start()` / `metadata()` / `destroy()` 函数
- [ ] 3.3 实现 CRC 算法（通用，支持 11 位和 24 位）
- [ ] 3.4 实现 IDLE 状态：TSS 检测（低→高转换）
- [ ] 3.5 实现 GET BITS 状态：采样点计算和时钟同步
- [ ] 3.6 实现 `is_bss_sequence()` — BSS 检测
- [ ] 3.7 实现 `handle_bit()` — 按位号处理各字段
  - [ ] 3.7.1 bitnum 0-1: FSS + 保留位 + TSS/CAS 检测
  - [ ] 3.7.2 bitnum 2-5: PPI/NF/Sync/Startup
  - [ ] 3.7.3 bitnum 6-16: Frame ID
  - [ ] 3.7.4 bitnum 17-23: Payload Length
  - [ ] 3.7.5 bitnum 24-34: Header CRC（含验证）
  - [ ] 3.7.6 bitnum 35-40: Cycle Code
  - [ ] 3.7.7 bitnum 41-last_databit: Data Bytes
  - [ ] 3.7.8 bitnum last_databit+23: Frame CRC（含验证）
  - [ ] 3.7.9 bitnum last_databit+24-25: FES
  - [ ] 3.7.10 bitnum last_databit+26+: DTS/CID
- [ ] 3.8 实现 `putg()` / `putx()` / `putb()` 注释辅助函数
- [ ] 3.9 实现 `dom_edge_seen()` 和 `get_sample_point()` 时钟同步
- [ ] 3.10 实现通道类型选项（A/B）影响 Frame CRC 初始值
- [ ] 3.11 实现比特率选项（10M/5M/2.5M）
- [ ] 3.12 实现 `srd_c_decoder` 结构体和导出
- [ ] 3.13 在 CMakeLists.txt 中注册 `flexray_c`
- [ ] 3.14 编译测试

### 关键依赖

- 需要 samplerate（bit_width 和 sample_point 计算）
- 1 通道 + 2 选项
- 无 OUTPUT_PYTHON
- 参考 can_c.c 的类似架构（同为汽车总线，有 dom_edge 同步机制）

---

## 任务 4: MIPI RFFE 解码器 (mipi_rffe_c)

**优先级**: 中  
**预估工时**: 10-12 小时  
**C 文件**: `libsigrokdecode/c_decoders/mipi_rffe_c.c`

### 子任务

- [ ] 4.1 创建 `mipi_rffe_c.c` 文件骨架
- [ ] 4.2 实现 `reset()` / `start()` / `metadata()` / `destroy()` 函数
- [ ] 4.3 实现 FIND SSC 状态：SCLK/SDATA 时序检测
- [ ] 4.4 实现 `handle()` — 通用数据读取函数（含 IJE 检测）
- [ ] 4.5 实现 `handle_CMD()` — 命令类型解码
  - [ ] 4.5.1 R0W 检测
  - [ ] 4.5.2 基本/扩展命令区分
  - [ ] 4.5.3 ERW/ERR/ERWL/ERRL/RW/RR 命令确定
- [ ] 4.6 实现 `Parity()` — 奇偶校验（含 Pdata 调整逻辑）
- [ ] 4.7 实现 FIND PARITY 状态：根据 cmdkey 和 Pcount 决定下一状态
- [ ] 4.8 实现 FIND ADDRESS 状态：根据 cmdkey 和 ADDcount 确定位数
- [ ] 4.9 实现 FIND DATA 状态：根据 cmdkey 和 bits 确定位数
- [ ] 4.10 实现 FIND BUS_PARK 状态：读/写操作的不同处理
- [ ] 4.11 实现 `cmdset()` / `handle_BP()` / `init()` / `initBP()` 辅助函数
- [ ] 4.12 实现 error_display 选项
- [ ] 4.13 实现 OUTPUT_PYTHON 输出
- [ ] 4.14 实现 `srd_c_decoder` 结构体和导出
- [ ] 4.15 在 CMakeLists.txt 中注册 `mipi_rffe_c`
- [ ] 4.16 编译测试

### 关键依赖

- 需要 samplerate（但解码逻辑不直接使用）
- 2 通道（SCLK + SDATA）+ 1 选项
- 需要 OUTPUT_PYTHON
- 命令类型用枚举替代字符串

---

## 任务 5: USB Power Delivery 解码器 (usb_power_delivery_c)

**优先级**: 中  
**预估工时**: 14-18 小时  
**C 文件**: `libsigrokdecode/c_decoders/usb_power_delivery_c.c`

### 子任务

- [ ] 5.1 创建 `usb_power_delivery_c.c` 文件骨架
- [ ] 5.2 实现 `reset()` / `start()` / `metadata()` / `destroy()` 函数
- [ ] 5.3 实现 BMC 解码（decode() 主循环）
  - [ ] 5.3.1 边沿等待和 diff 计算
  - [ ] 5.3.2 包间空闲检测（diff > maxbit）
  - [ ] 5.3.3 BMC 位解码（half_one 状态机）
- [ ] 5.4 实现 4b5b 解码表和符号解码
- [ ] 5.5 实现 `scan_eop()` — SOP 序列扫描
  - [ ] 5.5.1 7 种 SOP 序列匹配
  - [ ] 5.5.2 3/4 容错匹配
  - [ ] 5.5.3 Hard Reset / Cable Reset 检测
- [ ] 5.6 实现 `get_short()` / `get_word()` — 16/32 位数据读取
- [ ] 5.7 实现包头解码（head_ext/count/id/power_role/data_role/rev/type）
- [ ] 5.8 实现控制消息解码（CTRL_TYPES 查找）
- [ ] 5.9 实现数据消息解码
  - [ ] 5.9.1 SOURCE CAP / SINK CAP（PDO 解码：Fixed/Battery/Variable/PPS）
  - [ ] 5.9.2 REQUEST（RDO 解码）
  - [ ] 5.9.3 BIST
  - [ ] 5.9.4 VDM（结构化/非结构化）
  - [ ] 5.9.5 EPR Mode
- [ ] 5.10 实现扩展消息解码（分块传输）
- [ ] 5.11 实现 CRC32 计算
- [ ] 5.12 实现 EOP 检测
- [ ] 5.13 实现 PDO 存储和引用
- [ ] 5.14 实现 OUTPUT_BINARY 输出
- [ ] 5.15 实现 OUTPUT_META 输出（bitrate）
- [ ] 5.16 实现 fulltext 选项
- [ ] 5.17 实现可选通道 CC2 支持
- [ ] 5.18 实现 `srd_c_decoder` 结构体和导出
- [ ] 5.19 在 CMakeLists.txt 中注册 `usb_power_delivery_c`
- [ ] 5.20 编译测试

### 关键依赖

- 需要 samplerate（maxbit 和 threshold 计算）
- 1 通道 + 1 可选通道 + 1 选项
- 需要 OUTPUT_PYTHON + OUTPUT_BINARY + OUTPUT_META
- 需实现 CRC32
- 最复杂的解码器

---

## 通用任务

- [ ] G.1 在 CMakeLists.txt 的 C_DECODERS 列表中添加所有 5 个解码器
- [ ] G.2 全量编译测试（build_incremental.cmd）
- [ ] G.3 验证所有 5 个 DLL 正确生成到 build.dir/decoders/c_decoders/
- [ ] G.4 运行时加载测试（PXView 启动无报错）

---

## 依赖关系

```
spacewire_c (无依赖) ──┐
iebus_c (无依赖) ──────┤
flexray_c (参考 can_c) ┤── 可并行开发
mipi_rffe_c (无依赖) ──┤
usb_power_delivery_c ──┘ (参考 4b5b_c)
```

所有 5 个解码器之间无依赖关系，可并行开发。但建议按复杂度顺序实现，先从 spacewire 获取经验。

---

## 风险与注意事项

1. **usb_power_delivery 的 CRC32**：需确认系统是否提供 CRC32 库，否则需自行实现
2. **mipi_rffe 的奇偶校验调整**：Pdata 调整逻辑复杂，需仔细对照 Python 实现
3. **flexray 的 BSS 检测**：基于 rawbits 长度的模运算，需确保 C 版本与 Python 行为一致
4. **iebus 的时序精度**：27µs 和 33µs 的采样偏移需精确计算，避免累积误差
5. **spacewire 的位反转**：需验证反转逻辑与 Python 版本一致
