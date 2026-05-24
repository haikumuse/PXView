# 任务列表 — Batch 12: sda2506, signature, sony_md, st7735, st7789

## 任务 1：SDA2506 C 解码器

- [ ] 1.1 创建 `libsigrokdecode/c_decoders/sda2506_c.c`
- [ ] 1.2 定义通道数组（CLK=0, DATA=1, CE#=2），含 idn
- [ ] 1.3 定义 5 个 annotation labels（cmdbit, databit, cmd, data, warnings）
- [ ] 1.4 定义 4 个 annotation rows（bits, commands, data, warnings）
- [ ] 1.5 定义状态结构体 sda_state（cmdbits 数组、databits 数组、samplerate 等）
- [ ] 1.6 实现 reset 回调（g_malloc0 + memset）
- [ ] 1.7 实现 start 回调（注册 OUTPUT_ANN）
- [ ] 1.8 实现 metadata 回调（获取 samplerate）
- [ ] 1.9 实现 decode 回调：
  - [ ] 1.9.1 samplerate 守卫
  - [ ] 1.9.2 主循环：等待 CLK 边沿或 CE 边沿
  - [ ] 1.9.3 命令模式（CE=1 + CLK 上升沿）：采样 DATA，等 CLK 下降沿，cmdbits 头部插入
  - [ ] 1.9.4 数据模式（CE=0 + CLK 下降沿）：25μs skip 等待，采样 DATA，databits 头部插入，8 bit 输出
  - [ ] 1.9.5 CE 下降沿：解析 addr/CB，区分 Read/Write/Erase 命令
  - [ ] 1.9.6 异常处理：解析失败时 reset
- [ ] 1.10 实现 destroy 回调（g_free）
- [ ] 1.11 定义 srd_c_decoder 结构体（id="sda2506_c", name="SDA2506(C)"）
- [ ] 1.12 实现 srd_c_decoder_entry() 和 srd_c_decoder_api_version()
- [ ] 1.13 在 CMakeLists.txt 的 C_DECODERS 列表添加 `sda2506`

## 任务 2：Signature C 解码器

- [ ] 2.1 创建 `libsigrokdecode/c_decoders/signature_c.c`
- [ ] 2.2 定义通道数组（START=0, STOP=1, CLOCK=2, DATA=3），含 idn
- [ ] 2.3 定义 5 个 annotation labels（bit0, bit1, start, stop, signature）
- [ ] 2.4 定义 2 个 annotation rows（bits, signatures）
- [ ] 2.5 定义 4 个 options（start_edge, stop_edge, clk_edge, annbits），含 idn
- [ ] 2.6 定义状态结构体 sig_state（gate_is_open, shiftreg, prev_start/stop 等）
- [ ] 2.7 实现 reset 回调
- [ ] 2.8 实现 start 回调（注册 OUTPUT_ANN，读取 options）
- [ ] 2.9 实现 metadata 回调
- [ ] 2.10 实现 decode 回调：
  - [ ] 2.10.1 samplerate 守卫
  - [ ] 2.10.2 根据 clk_edge option 选择等待上升/下降沿
  - [ ] 2.10.3 START 边沿检测（根据 start_edge option）
  - [ ] 2.10.4 STOP 边沿检测（根据 stop_edge option）
  - [ ] 2.10.5 门打开时 LFSR 更新：popcount(shiftreg & 0x0291) + data
  - [ ] 2.10.6 门关闭时输出签名（4 nibble → symbol_map 查表）
  - [ ] 2.10.7 位级注解（annbits=yes 时）
- [ ] 2.11 实现 destroy 回调
- [ ] 2.12 定义 srd_c_decoder 结构体（id="signature_c", name="Signature(C)"）
- [ ] 2.13 实现 srd_c_decoder_entry()（初始化 4 个 string options）和 srd_c_decoder_api_version()
- [ ] 2.14 在 CMakeLists.txt 的 C_DECODERS 列表添加 `signature`

## 任务 3：Sony MD Remote C 解码器

- [ ] 3.1 创建 `libsigrokdecode/c_decoders/sony_md_c.c`
- [ ] 3.2 定义通道数组（data=0）
- [ ] 3.3 定义 8 个 annotation labels（signals, bit-zero, bit-one, bit-error, state-error, byte, bit-count, bit-count-error）
- [ ] 3.4 定义 5 个 annotation rows（signalling, raw-bits, byte-values, Messages, errors）
- [ ] 3.5 定义 1 个 option（marginpct），含 idn
- [ ] 3.6 定义状态结构体 sony_md_state（5 个状态、时序参数、位计数等）
- [ ] 3.7 实现 reset 回调
- [ ] 3.8 实现 start 回调（注册 OUTPUT_ANN 和 OUTPUT_PYTHON，计算时序参数）
- [ ] 3.9 实现 metadata 回调
- [ ] 3.10 实现 decode 回调：
  - [ ] 3.10.1 samplerate 守卫
  - [ ] 3.10.2 主循环：等待 DATA 边沿
  - [ ] 3.10.3 IDLE 状态：检测 Reset/Presync 脉冲
  - [ ] 3.10.4 PRESYNC 状态：检测 Presync Delay
  - [ ] 3.10.5 SYNC 状态：检测 Sync 脉冲
  - [ ] 3.10.6 DATA-BIT-HIGH 状态：记录 databitstart
  - [ ] 3.10.7 DATA-BIT-LOW 状态：区分 0/1 bit，检查特殊位（5/9/13）
  - [ ] 3.10.8 消息完成：输出 bit count，发送 Python 输出
  - [ ] 3.10.9 错误处理：脉冲宽度不匹配时输出错误并 returnToIdle
- [ ] 3.11 实现 destroy 回调
- [ ] 3.12 定义 srd_c_decoder 结构体（id="sony_md_c", name="Sony MD Remote(C)"）
- [ ] 3.13 实现 srd_c_decoder_entry()（初始化 marginpct option）和 srd_c_decoder_api_version()
- [ ] 3.14 在 CMakeLists.txt 的 C_DECODERS 列表添加 `sony_md`

## 任务 4：ST7735 C 解码器

- [ ] 4.1 创建 `libsigrokdecode/c_decoders/st7735_c.c`
- [ ] 4.2 定义通道数组（CS#=0, CLK=1, MOSI=2, DC=3），含 idn
- [ ] 4.3 定义 4 个 annotation labels（bit, command, data, description）
- [ ] 4.4 定义 3 个 annotation rows（bits, fields, description）
- [ ] 4.5 定义命令查找表 st7735_cmd_table（约 30 个条目）
- [ ] 4.6 定义状态结构体 st7735_state（accum_byte, current_cmd, current_data 等）
- [ ] 4.7 实现 reset 回调
- [ ] 4.8 实现 start 回调（注册 OUTPUT_ANN）
- [ ] 4.9 实现 decode 回调：
  - [ ] 4.9.1 主循环：等待 CLK 边沿
  - [ ] 4.9.2 CS 高电平：reset 状态
  - [ ] 4.9.3 CLK 上升沿：采样 MOSI 位
  - [ ] 4.9.4 CLK 下降沿：累积 bit → 字节
  - [ ] 4.9.5 字节完成：根据 DC 区分 Command/Data
  - [ ] 4.9.6 新 Command 时输出上一个 Command 的 description
  - [ ] 4.9.7 Data 累积（最多 128 字节）
- [ ] 4.10 实现 destroy 回调
- [ ] 4.11 定义 srd_c_decoder 结构体（id="st7735_c", name="ST7735(C)"）
- [ ] 4.12 实现 srd_c_decoder_entry() 和 srd_c_decoder_api_version()
- [ ] 4.13 在 CMakeLists.txt 的 C_DECODERS 列表添加 `st7735`

## 任务 5：ST7789 C 解码器

- [ ] 5.1 创建 `libsigrokdecode/c_decoders/st7789_c.c`
- [ ] 5.2 定义通道数组（CSX=0, DCX=1, SDO=2, WRX=3），含 idn
- [ ] 5.3 定义 5 个 annotation labels（bit, command, data, cmd_data, asserted）
- [ ] 5.4 定义 4 个 annotation rows（bits, bytes, cmd_data, asserted）
- [ ] 5.5 定义命令查找表 st7789_cmd_table（约 60 个条目）
- [ ] 5.6 定义状态结构体 st7789_state（bit, bit_count, byte_val, last_cmd 等）
- [ ] 5.7 实现 reset 回调
- [ ] 5.8 实现 start 回调（注册 OUTPUT_ANN）
- [ ] 5.9 实现 decode 回调：
  - [ ] 5.9.1 外层循环：等待 CSX 下降沿
  - [ ] 5.9.2 内层循环：等待 CSX 上升沿或 DCX 边沿
  - [ ] 5.9.3 CSX=1：输出 Asserted 注解，输出 cmd_data 组合，跳出内层
  - [ ] 5.9.4 DCX=1 + bit 未设置：采样 SDO 位
  - [ ] 5.9.5 DCX=0 + bit 已设置：完成一个 bit
  - [ ] 5.9.6 字节完成：WRX=1→Data, WRX=0→Command
  - [ ] 5.9.7 新 Command 时输出上一个 cmd_data 组合
- [ ] 5.10 实现 destroy 回调
- [ ] 5.11 定义 srd_c_decoder 结构体（id="st7789_c", name="ST7789(C)"）
- [ ] 5.12 实现 srd_c_decoder_entry() 和 srd_c_decoder_api_version()
- [ ] 5.13 在 CMakeLists.txt 的 C_DECODERS 列表添加 `st7789`

## 任务 6：构建验证

- [ ] 6.1 运行 `build_incremental.cmd` 确认编译通过
- [ ] 6.2 确认 5 个 DLL 生成在 `build.dir/decoders/c_decoders/` 目录
- [ ] 6.3 在 PXView 中加载各解码器确认无崩溃
