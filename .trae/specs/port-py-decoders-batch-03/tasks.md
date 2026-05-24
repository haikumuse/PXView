# Python解码器移植任务列表 — 第三批

## 总览

| # | 解码器 | C文件 | 复杂度 | 预估工时 | 依赖 |
|---|--------|-------|--------|---------|------|
| 1 | rvswd | rvswd_c.c | ★★☆☆☆ | 2-3h | 无 |
| 2 | swim | swim_c.c | ★★★★☆ | 4-6h | math.h |
| 3 | emmc_sd | emmc_sd_c.c | ★★★★☆ | 5-7h | cmd_names/device_status表 |
| 4 | sdcard_sd | sdcard_sd_c.c | ★★★★★ | 6-8h | cmd_names/acmd_names/accepted_voltages表 |
| 5 | ac97 | ac97_c.c | ★★★★★ | 6-8h | 无 |

**建议实现顺序**: 从简单到复杂，先rvswd再swim，然后emmc_sd，sdcard_sd，最后ac97。

---

## 任务1: rvswd_c.c — RISC-V串行线调试

### 子任务

- [ ] 1.1 创建rvswd_c.c文件骨架（include、结构体、导出函数）
- [ ] 1.2 定义通道数组（CLK=0, DIO=1）
- [ ] 1.3 定义11个注解标签（注意格式化字符串需在运行时生成）
- [ ] 1.4 定义2个注解行
- [ ] 1.5 实现rvswd_priv结构体（bits数组、in_packet标志等）
- [ ] 1.6 实现reset函数
- [ ] 1.7 实现start函数（注册OUTPUT_ANN、OUTPUT_PYTHON、OUTPUT_BINARY）
- [ ] 1.8 实现decode主循环
  - [ ] 1.8.1 等待START条件（CLK=高, DIO=下降沿）
  - [ ] 1.8.2 等待CLK边沿或STOP条件
  - [ ] 1.8.3 位采样逻辑（CLK上升沿push, CLK下降沿terminate）
  - [ ] 1.8.4 STOP条件检测与包处理
- [ ] 1.9 实现process_short_packet（52位）
- [ ] 1.10 实现process_long_packet（84位）
- [ ] 1.11 实现put_annotation_bits（位范围→整数→格式化注解）
- [ ] 1.12 实现destroy函数
- [ ] 1.13 实现srd_c_decoder_entry导出函数
- [ ] 1.14 编译测试

### 关键风险
- c_cond API的OR条件组合可能需要特殊处理
- bit注解的格式化字符串包含数组索引，需要特殊处理

---

## 任务2: swim_c.c — STM8 SWIM总线

### 子任务

- [ ] 2.1 创建swim_c.c文件骨架
- [ ] 2.2 定义通道数组（SWIM=0）
- [ ] 2.3 定义选项数组（debug: yes/no）
- [ ] 2.4 定义16个注解标签
- [ ] 2.5 定义4个注解行
- [ ] 2.6 定义2个二进制输出
- [ ] 2.7 实现swim_priv结构体（浮点时序参数、边沿历史、协议状态等）
- [ ] 2.8 实现reset函数
- [ ] 2.9 实现metadata函数（获取samplerate）
- [ ] 2.10 实现start函数（验证samplerate、计算时序参数、注册输出）
- [ ] 2.11 实现adjust_timings函数
- [ ] 2.12 实现decode主循环
  - [ ] 2.12.1 bit_maxlen机制（逐采样递减）
  - [ ] 2.12.2 边沿检测（eseq_edge, bit_edge历史队列）
  - [ ] 2.12.3 同步帧检测与swim_clock重计算
  - [ ] 2.12.4 进入序列检测（4+4脉冲模式匹配）
  - [ ] 2.12.5 位解码（高低电平比例判断0/1）
- [ ] 2.13 实现bitseq函数（位序列累积、parity、ACK/NACK）
- [ ] 2.14 实现protocol函数（CMD/N/ADDR/DATA状态机）
- [ ] 2.15 实现destroy函数
- [ ] 2.16 实现srd_c_decoder_entry导出函数（含选项值列表）
- [ ] 2.17 编译测试

### 关键风险
- 浮点时序计算精度
- math.h的ceil/floor链接问题
- 单线协议的边沿检测逻辑复杂

---

## 任务3: emmc_sd_c.c — eMMC (SD模式)

### 子任务

- [ ] 3.1 创建emmc_sd_c.c文件骨架
- [ ] 3.2 定义通道数组（CMD=0, CLK=1）
- [ ] 3.3 定义73个注解标签
- [ ] 3.4 定义5个注解行
- [ ] 3.5 内联cmd_names查找表（来自emmc_sd/mod.py，63个条目）
- [ ] 3.6 内联device_status查找表（来自emmc_sd/mod.py，32个条目）
- [ ] 3.7 实现emmc_sd_priv结构体
- [ ] 3.8 实现reset函数
- [ ] 3.9 实现start函数
- [ ] 3.10 实现decode主循环
  - [ ] 3.10.1 CLK上升沿等待
  - [ ] 3.10.2 起始位检测（CMD=低）
  - [ ] 3.10.3 状态机分发
- [ ] 3.11 实现get_token_bits函数
- [ ] 3.12 实现handle_common_token_fields函数
- [ ] 3.13 实现get_command_token函数
- [ ] 3.14 实现各CMD处理函数（约30个）
  - [ ] 3.14.1 CMD0,1,2,3,4,5
  - [ ] 3.14.2 CMD6,7,8,9,10
  - [ ] 3.14.3 CMD12,13,14,15,16
  - [ ] 3.14.4 CMD17,18,19,21,23
  - [ ] 3.14.5 CMD24,25,26,27,28,29,30,31
  - [ ] 3.14.6 CMD35,36,38,39,40,42
  - [ ] 3.14.7 CMD44,45,46,47,48,49
  - [ ] 3.14.8 CMD53,54,55,56
  - [ ] 3.14.9 CMD999（未知命令）
- [ ] 3.15 实现各响应处理函数
  - [ ] 3.15.1 R1 (48位)
  - [ ] 3.15.2 R1b (48位)
  - [ ] 3.15.3 R2 (136位)
  - [ ] 3.15.4 R3 (48位)
  - [ ] 3.15.5 R4 (39位)
  - [ ] 3.15.6 R5 (40位)
- [ ] 3.16 实现putbit/putf/puta/putc/putr辅助函数
- [ ] 3.17 实现destroy函数
- [ ] 3.18 实现srd_c_decoder_entry导出函数
- [ ] 3.19 编译测试

### 关键风险
- Python代码中注解索引不一致（硬编码128-136 vs 定义0-72）
- CMD23条件参数解析
- R4/R5非标准token长度

---

## 任务4: sdcard_sd_c.c — SD卡 (SD模式)

### 子任务

- [ ] 4.1 创建sdcard_sd_c.c文件骨架
- [ ] 4.2 定义通道数组（CMD=0, CLK=1）+ 可选通道（DAT0-3）
- [ ] 4.3 定义217个注解标签（最大的注解集）
- [ ] 4.4 定义5个注解行
- [ ] 4.5 内联cmd_names查找表（来自common/sdcard/mod.py，64个条目）
- [ ] 4.6 内联acmd_names查找表（来自common/sdcard/mod.py，64个条目）
- [ ] 4.7 内联accepted_voltages查找表（4个条目）
- [ ] 4.8 实现sdcard_sd_priv结构体
- [ ] 4.9 实现reset函数
- [ ] 4.10 实现start函数
- [ ] 4.11 实现decode主循环
  - [ ] 4.11.1 CLK上升沿 + 可选CMD低等待
  - [ ] 4.11.2 状态机分发
  - [ ] 4.11.3 ACMD标志管理
- [ ] 4.12 实现get_token_bits函数
- [ ] 4.13 实现handle_common_token_fields函数
- [ ] 4.14 实现get_command_token函数
- [ ] 4.15 实现CMD处理函数
  - [ ] 4.15.1 CMD0,2,3,6,7,8
  - [ ] 4.15.2 CMD9,10,13,16,55
  - [ ] 4.15.3 ACMD6,13,41,51
  - [ ] 4.15.4 CMD999, ACMD999
- [ ] 4.16 实现响应处理函数
  - [ ] 4.16.1 R1 (48位, 含状态寄存器)
  - [ ] 4.16.2 R1b (48位)
  - [ ] 4.16.3 R2 (136位, 含CID/CSD寄存器)
  - [ ] 4.16.4 R3 (48位, OCR寄存器)
  - [ ] 4.16.5 R6 (48位, RCA响应)
  - [ ] 4.16.6 R7 (48位, 接口条件)
- [ ] 4.17 实现handle_reg_status函数（30个状态位）
- [ ] 4.18 实现handle_reg_cid函数（9个CID字段）
- [ ] 4.19 实现handle_reg_csd函数（34个CSD字段）
- [ ] 4.20 实现putf/puta/putc/putr辅助函数
- [ ] 4.21 实现AssertionError恢复逻辑（传输位检查）
- [ ] 4.22 实现destroy函数
- [ ] 4.23 实现srd_c_decoder_entry导出函数
- [ ] 4.24 编译测试

### 关键风险
- 217个注解标签数组编写工作量大
- 状态寄存器/CID/CSD字段注解数量多
- ACMD切换逻辑
- AssertionError恢复逻辑

---

## 任务5: ac97_c.c — Audio Codec '97

### 子任务

- [ ] 5.1 创建ac97_c.c文件骨架
- [ ] 5.2 定义通道数组（SYNC=0, CLK=1）+ 可选通道（OUT=2, IN=3, RST=4）
- [ ] 5.3 定义32个注解标签
- [ ] 5.4 定义8个注解行
- [ ] 5.5 定义4个二进制输出
- [ ] 5.6 实现ac97_priv结构体（frame_ss_list动态数组、位序列、slot数据等）
- [ ] 5.7 实现reset函数
- [ ] 5.8 实现metadata函数（获取samplerate）
- [ ] 5.9 实现start函数（检查SDATA_OUT/SDATA_IN通道、注册输出）
- [ ] 5.10 实现decode主循环
  - [ ] 5.10.1 BIT_CLK边沿等待序列（e→r→f→r循环）
  - [ ] 5.10.2 SYNC帧起始检测（3元素历史队列）
  - [ ] 5.10.3 位采样与注解输出
- [ ] 5.11 实现start_frame函数
- [ ] 5.12 实现flush_frame_bits函数
- [ ] 5.13 实现handle_bits函数（位累积、slot边界检测、slot数据处理）
- [ ] 5.14 实现handle_slot_00函数（TAG: READY, VALID, RSV, CODEC）
- [ ] 5.15 实现handle_slot_01函数（命令地址: R/W, ADDR, REQ, RSV）
- [ ] 5.16 实现handle_slot_02函数（命令数据: DATA, RSV）
- [ ] 5.17 实现handle_slot_dummy函数（slot 3-12默认处理）
- [ ] 5.18 实现bits_to_int辅助函数
- [ ] 5.19 实现bits_to_bin_ann辅助函数
- [ ] 5.20 实现int_to_nibble_text辅助函数
- [ ] 5.21 实现get_bit_field辅助函数
- [ ] 5.22 实现destroy函数（释放动态内存）
- [ ] 5.23 实现srd_c_decoder_entry导出函数
- [ ] 5.24 编译测试

### 关键风险
- frame_ss_list动态内存管理
- 双数据线（OUT/IN）同时处理
- 二进制输出（位数组→字节数组转换）
- 帧起始检测的3元素SYNC历史队列

---

## 通用任务

- [ ] G.1 在CMakeLists.txt的C_DECODERS列表中添加5个新解码器名称
- [ ] G.2 全量编译测试
- [ ] G.3 与Python解码器对比测试（相同输入数据，验证注解输出一致性）
- [ ] G.4 边界条件测试（无数据、部分帧、错误帧、缺失可选通道）
