# Batch 06 移植任务列表

## 解码器列表

| # | 解码器 | Python源 | C目标文件 | 优先级 | 复杂度 |
|---|--------|----------|-----------|--------|--------|
| 1 | delta-sigma | `libsigrokdecode/decoders/delta-sigma/pd.py` | `libsigrokdecode/c_decoders/delta-sigma_c.c` | P1 | 低 |
| 2 | em4100 | `libsigrokdecode/decoders/em4100/pd.py` | `libsigrokdecode/c_decoders/em4100_c.c` | P2 | 中 |
| 3 | dsi | `libsigrokdecode/decoders/dsi/pd.py` | `libsigrokdecode/c_decoders/dsi_c.c` | P3 | 中 |
| 4 | em4305 | `libsigrokdecode/decoders/em4305/pd.py` | `libsigrokdecode/c_decoders/em4305_c.c` | P4 | 中高 |
| 5 | dcc | `libsigrokdecode/decoders/dcc/pd.py` | `libsigrokdecode/c_decoders/dcc_c.c` | P5 | 极高 |

## 每个解码器的任务分解

### Task 1: delta-sigma_c（预计 1-2 小时）

- [ ] 1.1 创建 `delta-sigma_c.c` 文件骨架
- [ ] 1.2 定义枚举（注解类：BIT_STREAM=0, FILTERED=1, CONVERTED=2）
- [ ] 1.3 定义私有状态结构体 `delta_sigma_state`
- [ ] 1.4 定义通道数组（DAT, CLK）
- [ ] 1.5 定义选项数组（clock_mode, filter_type, osr, shift, scale）
- [ ] 1.6 定义注解标签和注解行
- [ ] 1.7 实现 `delta_sigma_reset()` 函数
- [ ] 1.8 实现 `delta_sigma_start()` 函数（读取选项，注册输出）
- [ ] 1.9 实现 `delta_sigma_metadata()` 函数（保存 samplerate）
- [ ] 1.10 实现 sinc1 滤波器函数 `run_sinc1()`
- [ ] 1.11 实现 sinc2 滤波器函数 `run_sinc2()`
- [ ] 1.12 实现 sinc3 滤波器函数 `run_sinc3()`
- [ ] 1.13 实现 `delta_sigma_decode()` 主循环（等待 CLK 上升沿，读取 DAT，运行滤波，输出注解）
- [ ] 1.14 定义 `srd_c_decoder` 结构体和导出函数
- [ ] 1.15 编译测试

### Task 2: em4100_c（预计 2-3 小时）

- [ ] 2.1 创建 `em4100_c.c` 文件骨架
- [ ] 2.2 定义枚举（状态机、注解类 0-9）
- [ ] 2.3 定义私有状态结构体 `em4100_state`
- [ ] 2.4 定义通道数组（data）
- [ ] 2.5 定义选项数组（polarity, datarate, coilfreq）
- [ ] 2.6 定义注解标签和注解行
- [ ] 2.7 实现 `em4100_reset()` 函数
- [ ] 2.8 实现 `em4100_start()` 函数
- [ ] 2.9 实现 `em4100_metadata()` 函数（计算 bit_width, halfbit_limit, polarity）
- [ ] 2.10 实现 `manchester_decode()` 函数
- [ ] 2.11 实现 `putbit()` 函数（HEADER/PAYLOAD/TRAILER 状态处理）
- [ ] 2.12 实现 `em4100_decode()` 主循环（等待边沿，调用 manchester_decode）
- [ ] 2.13 定义 `srd_c_decoder` 结构体和导出函数
- [ ] 2.14 编译测试

### Task 3: dsi_c（预计 2-3 小时）

- [ ] 3.1 创建 `dsi_c.c` 文件骨架
- [ ] 3.2 定义枚举（状态机、注解类 0-3）
- [ ] 3.3 定义私有状态结构体 `dsi_state`
- [ ] 3.4 定义通道数组（dsi）
- [ ] 3.5 定义选项数组（polarity）
- [ ] 3.6 定义注解标签和注解行
- [ ] 3.7 实现 `dsi_reset()` 函数
- [ ] 3.8 实现 `dsi_start()` 函数（设置极性）
- [ ] 3.9 实现 `dsi_metadata()` 函数（计算 halfbit）
- [ ] 3.10 实现 `handle_bits()` 函数（位输出和帧解析）
- [ ] 3.11 实现 `dsi_decode()` 主循环（IDLE/PHASE0/PHASE1 状态机）
- [ ] 3.12 处理半位超时检测逻辑
- [ ] 3.13 定义 `srd_c_decoder` 结构体和导出函数
- [ ] 3.14 编译测试

### Task 4: em4305_c（预计 3-4 小时）

- [ ] 4.1 创建 `em4305_c.c` 文件骨架
- [ ] 4.2 定义枚举（状态机、注解类 0-10）
- [ ] 4.3 定义私有状态结构体 `em4305_state`
- [ ] 4.4 定义通道数组（data）
- [ ] 4.5 定义选项数组（coilfreq, first_field_stop, w_gap, w_one_max, w_zero_on_min, w_zero_off_max, em4100_decode）
- [ ] 4.6 定义注解标签和注解行
- [ ] 4.7 实现 `em4305_reset()` 函数
- [ ] 4.8 实现 `em4305_start()` 函数
- [ ] 4.9 实现 `em4305_metadata()` 函数（计算 field_clock, wzmax, wzmin, womax, ffs, writegap, nogap）
- [ ] 4.10 实现辅助函数：`get_8_bits()`, `get_32_bits()`, `get_3_bits()`, `get_4_bits()`
- [ ] 4.11 实现辅助函数：`print_row_parity()`, `print_col_parity()`, `print_8bit_data()`
- [ ] 4.12 实现 `decode_config()` 函数
- [ ] 4.13 实现 `em4100_decode1()` 和 `em4100_decode2()` 函数
- [ ] 4.14 实现 `put_fields()` 函数（50 位帧和 57 位帧解析）
- [ ] 4.15 实现 `add_bits_pos()` 函数
- [ ] 4.16 实现 `em4305_decode()` 主循环（FFS_SEARCH/FFS_DETECTED/SKIP 状态机）
- [ ] 4.17 定义 `srd_c_decoder` 结构体和导出函数
- [ ] 4.18 编译测试

### Task 5: dcc_c（预计 5-8 小时）

- [ ] 5.1 创建 `dcc_c.c` 文件骨架
- [ ] 5.2 定义枚举（状态机、注解类 0-13）
- [ ] 5.3 定义私有状态结构体 `dcc_state`
- [ ] 5.4 定义通道数组（data）
- [ ] 5.5 定义选项数组（CV_29_1, Mode_112_127, Addr_offset, Search_acc_addr, Search_dec_addr, Search_cv, Search_byte, Ignore_short_pulse）
- [ ] 5.6 定义注解标签和注解行
- [ ] 5.7 定义静态查找表（weekday, weekday_short, month）
- [ ] 5.8 实现 `dcc_reset()` 函数
- [ ] 5.9 实现 `dcc_start()` 函数（解析所有选项，特别是搜索值的多种进制解析）
- [ ] 5.10 实现 `dcc_metadata()` 函数（保存 samplerate）
- [ ] 5.11 实现辅助函数：`putx()`, `put_signal()`, `put_packetbyte()`, `put_packetbytes()`
- [ ] 5.12 实现 `incPos()` 辅助函数
- [ ] 5.13 实现 `collectDataBytes()` 函数（WAITINGFORPREAMBLE/PREAMBLE/ADDRESSDATABYTE 状态机）
- [ ] 5.14 实现 `handleDecodedBytes()` — 服务模式部分（Register/Page Mode, Service Mode）
- [ ] 5.15 实现 `handleDecodedBytes()` — 多功能解码器地址解析（0-127, 192-231）
- [ ] 5.16 实现 `handleDecodedBytes()` — Decoder Control 命令（cmd=000）
- [ ] 5.17 实现 `handleDecodedBytes()` — Advanced Operations（cmd=001）
- [ ] 5.18 实现 `handleDecodedBytes()` — Speed/Direction（cmd=010/011）
- [ ] 5.19 实现 `handleDecodedBytes()` — Function Group One（cmd=100）
- [ ] 5.20 实现 `handleDecodedBytes()` — Function Group Two（cmd=101）
- [ ] 5.21 实现 `handleDecodedBytes()` — Future Expansion（cmd=110）
- [ ] 5.22 实现 `handleDecodedBytes()` — CV Access Short Form（cmd=111, subcmd&0b10000）
- [ ] 5.23 实现 `handleDecodedBytes()` — CV Access Long Form POM（cmd=111, 5/6 字节包）
- [ ] 5.24 实现 `handleDecodedBytes()` — XPOM（cmd=111, ≥6/7 字节包）
- [ ] 5.25 实现 `handleDecodedBytes()` — Accessory Decoder（128-191）
- [ ] 5.26 实现 `handleDecodedBytes()` — 保留和空闲（232-254, 255）
- [ ] 5.27 实现 `handleDecodedBytes()` — 剩余字节和校验和
- [ ] 5.28 实现 `handleDecodedBytes()` — 搜索功能
- [ ] 5.29 实现 `dcc_decode()` 主循环（边沿检测、位值判定、短脉冲过滤、Railcom cutout 检测）
- [ ] 5.30 实现边沿检测方向切换逻辑
- [ ] 5.31 定义 `srd_c_decoder` 结构体和导出函数
- [ ] 5.32 编译测试

### Task 6: 集成

- [ ] 6.1 在 `CMakeLists.txt` 的 `C_DECODERS` 列表中添加 5 个新解码器
- [ ] 6.2 执行增量构建 `build_incremental.cmd`
- [ ] 6.3 验证所有 5 个 DLL 正确生成
- [ ] 6.4 在 PXView 中测试每个解码器的基本功能

## 依赖关系

- Task 1-4 之间无依赖，可并行实现
- Task 5（DCC）建议最后实现，因为最复杂
- Task 6 依赖 Task 1-5 全部完成
