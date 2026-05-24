# Python→C 解码器移植任务列表 — Batch 07

## 概述

将 5 个 Python 解码器移植为 C 解码器。按复杂度从低到高排序实施。

---

## 任务 1: guess_bitrate_c — 猜测比特率

**优先级**: 高（最简单，先完成验证流程）
**预估难度**: ★☆☆☆☆
**源文件**: `libsigrokdecode/decoders/guess_bitrate/pd.py`
**目标文件**: `libsigrokdecode/c_decoders/guess_bitrate_c.c`

### 子任务

- [ ] 1.1 创建 `guess_bitrate_c.c` 文件框架
- [ ] 1.2 定义 ANN_BITRATE 枚举和 `guess_bitrate_priv` 结构体
- [ ] 1.3 定义通道（1个：data）
- [ ] 1.4 定义注解标签和行
- [ ] 1.5 实现 `guess_bitrate_reset()`
- [ ] 1.6 实现 `guess_bitrate_start()` — 注册输出、获取 samplerate
- [ ] 1.7 实现 `guess_bitrate_metadata()` — 接收 samplerate
- [ ] 1.8 实现 `guess_bitrate_decode()`:
  - [ ] 检查 samplerate 是否可用
  - [ ] 等待第一个边沿
  - [ ] 主循环：等待边沿、计算最小间距、输出比特率
- [ ] 1.9 实现 `guess_bitrate_destroy()`
- [ ] 1.10 定义 `guess_bitrate_c_decoder` 结构体
- [ ] 1.11 实现 `srd_c_decoder_entry()` 和 `srd_c_decoder_api_version()`
- [ ] 1.12 在 CMakeLists.txt 的 C_DECODERS 列表中添加 `guess_bitrate_c`
- [ ] 1.13 编译验证

---

## 任务 2: iec_c — Commodore IEC 总线

**优先级**: 高
**预估难度**: ★★★☆☆
**源文件**: `libsigrokdecode/decoders/iec/pd.py`
**目标文件**: `libsigrokdecode/c_decoders/iec_c.c`

### 子任务

- [ ] 2.1 创建 `iec_c.c` 文件框架
- [ ] 2.2 定义注解枚举（ANN_ITEMS, ANN_GPIB, ANN_EOI）
- [ ] 2.3 定义 `iec_priv` 结构体（step, saved_ATN, saved_EOI, bits, numbits 等）
- [ ] 2.4 定义通道（3个：data, clk, atn）和可选通道（1个：srq）
- [ ] 2.5 定义注解标签和行
- [ ] 2.6 实现 `iec_reset()`
- [ ] 2.7 实现 `iec_start()`
- [ ] 2.8 实现 `iec_handle_bits()` — 字节解码和命令/数据输出
  - [ ] ATN 命令解码（GTL, SDC, PPC, GET, TCT, LLO, DCL, PPU, SPE, SPD, UNL, UNT）
  - [ ] Listener/Talker 地址解码
  - [ ] IEC 特有命令（Channel reopen/close/open）
  - [ ] 数据模式 ASCII 解码
  - [ ] EOI 输出
- [ ] 2.9 实现 `iec_decode()`:
  - [ ] step 0: 等待 ATN fall 或 (DATA low + CLK high)
  - [ ] step 1: 等待 ATN fall 或 (DATA high + CLK high) 或 CLK low
  - [ ] step 2: 等待 ATN fall 或 DATA fall 或 CLK low
  - [ ] step 3: 等待 ATN fall 或 CLK edge
  - [ ] ATN 下降沿始终重置 step
  - [ ] 位累积和字节完成检测
- [ ] 2.10 实现 `iec_destroy()`
- [ ] 2.11 定义 `iec_c_decoder` 结构体
- [ ] 2.12 实现入口函数
- [ ] 2.13 在 CMakeLists.txt 中添加 `iec_c`
- [ ] 2.14 编译验证

---

## 任务 3: eth_an_c — ETH 自动协商

**优先级**: 中
**预估难度**: ★★★☆☆
**源文件**: `libsigrokdecode/decoders/eth_an/pd.py`
**目标文件**: `libsigrokdecode/c_decoders/eth_an_c.c`

### 子任务

- [ ] 3.1 创建 `eth_an_c.c` 文件框架
- [ ] 3.2 定义注解枚举（ANN_DATA, ANN_FORMAT, ANN_BITD, ANN_BIT, ANN_NLP）
- [ ] 3.3 定义状态枚举（STATE_BASE_PAGE, STATE_BASE_PAGE_ACK, STATE_NEXT_PAGE, STATE_NEXT_PAGE_ACK）
- [ ] 3.4 定义 `eth_an_priv` 结构体（samplerate, hex, pre_hex, index, state, data_list 数组等）
- [ ] 3.5 定义通道（1个：dp/TX+）
- [ ] 3.6 定义注解标签和行（5行）
- [ ] 3.7 实现 `eth_an_reset()`
- [ ] 3.8 实现 `eth_an_start()` — 注册输出、获取 samplerate
- [ ] 3.9 实现 `eth_an_metadata()` — 接收 samplerate
- [ ] 3.10 实现 `eth_an_change_state()` — 状态转换逻辑
- [ ] 3.11 实现 `eth_an_decode_base_page()`:
  - [ ] Selector field 解码
  - [ ] Technology ability field 解码
  - [ ] Other fields 解码
  - [ ] 各位注解输出
- [ ] 3.12 实现 `eth_an_decode_next_page()`:
  - [ ] MP 位检测
  - [ ] Technology ability / Unformatted code field 解码
  - [ ] Other fields 解码
  - [ ] Master-Slave seed value 注解
  - [ ] 各位注解输出
- [ ] 3.13 实现 `eth_an_decode()`:
  - [ ] samplerate 检查
  - [ ] decodeTiming 逻辑：等待上升沿→等待边沿→计算脉冲宽度
  - [ ] NLP 检测（1-10μs）
  - [ ] 逻辑1/0 间隔检测（60-70μs / 120-140μs）
  - [ ] 16位收集完成→状态转换→页面解码→重置
- [ ] 3.14 实现 `eth_an_destroy()`
- [ ] 3.15 定义 `eth_an_c_decoder` 结构体
- [ ] 3.16 实现入口函数
- [ ] 3.17 在 CMakeLists.txt 中添加 `eth_an_c`
- [ ] 3.18 编译验证

---

## 任务 4: gpib_c — 通用接口总线

**优先级**: 中
**预估难度**: ★★★★☆
**源文件**: `libsigrokdecode/decoders/gpib/pd.py`
**目标文件**: `libsigrokdecode/c_decoders/gpib_c.c`

### 子任务

- [ ] 4.1 创建 `gpib_c.c` 文件框架
- [ ] 4.2 定义注解枚举（ANN_ITEMS, ANN_GPIB, ANN_EOI）
- [ ] 4.3 定义 `gpib_priv` 结构体（items数组, itemcount, saved_*, first标志, sample_total等）
- [ ] 4.4 定义16个通道（dio1-dio8, eoi, dav, nrfd, ndac, ifc, srq, atn, ren）
- [ ] 4.5 定义选项（sample_total）
- [ ] 4.6 定义注解标签和行
- [ ] 4.7 实现 `gpib_reset()`
- [ ] 4.8 实现 `gpib_start()` — 注册输出、获取选项
- [ ] 4.9 实现 `gpib_handle_bits()`:
  - [ ] 8位数据读取和反转
  - [ ] ATN/EOI 检测
  - [ ] 延迟一拍输出逻辑
  - [ ] GPIB 命令解码
  - [ ] Listener/Talker 地址格式化
  - [ ] 数据模式 ASCII 解码
  - [ ] EOI 输出
- [ ] 4.10 实现 `gpib_decode()`:
  - [ ] 构建 wait 条件（DAV low/fall + 可选 skip）
  - [ ] 读取16个通道值
  - [ ] 调用 handle_bits
  - [ ] 动态更新 skip 值
- [ ] 4.11 实现 `gpib_destroy()`
- [ ] 4.12 定义 `gpib_c_decoder` 结构体
- [ ] 4.13 实现入口函数（含选项默认值设置）
- [ ] 4.14 在 CMakeLists.txt 中添加 `gpib_c`
- [ ] 4.15 编译验证

---

## 任务 5: fsi_c — 灵活服务接口

**优先级**: 低（最复杂，最后实现）
**预估难度**: ★★★★★
**源文件**: `libsigrokdecode/decoders/fsi/pd.py`
**目标文件**: `libsigrokdecode/c_decoders/fsi_c.c`

### 子任务

- [ ] 5.1 创建 `fsi_c.c` 文件框架
- [ ] 5.2 定义注解枚举（9个：WARNINGS 到 TAR）
- [ ] 5.3 定义状态枚举（17个状态）
- [ ] 5.4 定义 `fsi_priv` 结构体（所有状态变量、CRC、地址追踪等）
- [ ] 5.5 定义通道（2个：data, clock）
- [ ] 5.6 定义注解标签和行
- [ ] 5.7 实现 `fsi_reset()`
- [ ] 5.8 实现 `fsi_start()`
- [ ] 5.9 实现 `fsi_decode()`:
  - [ ] 主循环：等待时钟边沿
  - [ ] 数据反相处理
  - [ ] BREAK 检测（仅上升沿，256个连续1）
  - [ ] 主从边沿选择逻辑
  - [ ] IDLE 状态：检测 START 位
  - [ ] TX_SLAVE_ID 状态：2位 slave ID
  - [ ] COMMAND 状态：2-3位命令码解码
  - [ ] DIRECTION 状态：1位方向
  - [ ] REL_ADDRESS_SIGN 状态：1位符号
  - [ ] ADDRESS 状态：2/8/21位地址
  - [ ] DATA_SIZE 状态：数据大小检测 + TERM 命令检测
  - [ ] TX_DATA 状态：8/16/32位发送数据
  - [ ] CRC 状态：4位 CRC 接收和验证
  - [ ] TAR 状态：turn-around 周期
  - [ ] RX_SLAVE_ID 状态：2位响应 slave ID
  - [ ] RESPONSE 状态：1-2位响应码解码
  - [ ] RX_DATA 状态：8/16/32位接收数据
  - [ ] RX_IPOLL_INTERRUPT_FIELD 状态：2位中断字段
  - [ ] RX_IPOLL_DMA_CONTROL_FIELD 状态：3位 DMA 控制字段
  - [ ] BREAK_TAR_QUEUED/BREAK_TAR 状态
  - [ ] CRC LFSR 计算（循环末尾）
  - [ ] fsi_data_prev 更新（循环末尾）
- [ ] 5.10 实现 `fsi_destroy()`
- [ ] 5.11 定义 `fsi_c_decoder` 结构体
- [ ] 5.12 实现入口函数
- [ ] 5.13 在 CMakeLists.txt 中添加 `fsi_c`
- [ ] 5.14 编译验证

---

## 全局任务

- [ ] G.1 确认所有5个解码器编译通过
- [ ] G.2 确认 CMakeLists.txt 中 C_DECODERS 列表已更新
- [ ] G.3 运行 `build_incremental.cmd` 验证完整构建
