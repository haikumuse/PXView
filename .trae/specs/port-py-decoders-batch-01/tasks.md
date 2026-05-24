# Python 解码器移植到 C — Batch 01 任务列表

> 版本：1.0  
> 日期：2026-05-23

---

## 任务概览

| # | 解码器 | 复杂度 | 预估工时 | 优先级 | 依赖 |
|---|--------|--------|----------|--------|------|
| T1 | qspi_c | ★★★★★ | 5天 | P1 | 无 |
| T2 | sdio_c | ★★★★★ | 5天 | P1 | 无 |
| T3 | spi_dual_quad_c | ★★★☆☆ | 3天 | P1 | 无 |
| T4 | uart_fast_c | ★★★★☆ | 4天 | P1 | 无 |
| T5 | cjtag_c | ★★★☆☆ | 3天 | P1 | 无 |
| T6 | CMakeLists.txt 更新 | ★☆☆☆☆ | 0.5天 | P1 | T1-T5 |
| T7 | 集成测试 | ★★☆☆☆ | 2天 | P2 | T6 |

---

## T1: qspi_c — QSPI 解码器

### 子任务

| # | 子任务 | 描述 | 状态 |
|---|--------|------|------|
| T1.1 | 创建文件 | 创建 `libsigrokdecode/c_decoders/qspi_c.c` | ⬜ |
| T1.2 | 定义元数据 | id, name, longname, desc, license, channels, options, annotations, annotation_rows | ⬜ |
| T1.3 | 实现命令表 | 将 Python `command` 字典转换为 C 静态数组 + 查找函数（约 40 个命令条目） | ⬜ |
| T1.4 | 实现 reset/start | 初始化私有状态，解析选项，注册输出 | ⬜ |
| T1.5 | 实现 metadata | 获取 samplerate，计算 bit_width | ⬜ |
| T1.6 | 实现主解码循环 | CLK 边沿等待 + CS# 边沿等待，首次采样处理 | ⬜ |
| T1.7 | 实现 handle_bit | 4 线比特累积，8 bit 后调用 putdata | ⬜ |
| T1.8 | 实现 putdata | Quad/Dual 数据组合，命令解析状态机，注解输出 | ⬜ |
| T1.9 | 实现 Python 输出 | CS-CHANGE, TRANSFER 消息 | ⬜ |
| T1.10 | 实现 bitrate 输出 | Meta 输出 | ⬜ |
| T1.11 | 实现 destroy | 释放内存 | ⬜ |
| T1.12 | 边界情况处理 | Page Program 256 字节循环，invalidlevel 检查，地址模式切换 | ⬜ |

### 关键风险

- 命令表庞大（约 40 条），需确保每条的数据序列正确
- Quad/Dual 数据组合逻辑复杂，比特排列容易出错
- Page Program 命令的 256 个 WRITE_BYTE_SINGLE 需要特殊处理

---

## T2: sdio_c — SDIO 解码器

### 子任务

| # | 子任务 | 描述 | 状态 |
|---|--------|------|------|
| T2.1 | 创建文件 | 创建 `libsigrokdecode/c_decoders/sdio_c.c` | ⬜ |
| T2.2 | 定义元数据 | 142 个注解类，7 个注解行，3 个选项 | ⬜ |
| T2.3 | 移植 CRC 函数 | crc7() 和 crc16() 从 sd_crc.py | ⬜ |
| T2.4 | 移植查找表 | cmd_names, acmd_names, accepted_voltages, card_status 从 lists.py | ⬜ |
| T2.5 | 实现 reset/start | 初始化状态机，解析选项 | ⬜ |
| T2.6 | 实现 CMD 线状态机 | GET_COMMAND_TOKEN, 各响应类型处理 | ⬜ |
| T2.7 | 实现命令处理函数 | handle_cmd0~cmd55, handle_acmd6~acmd51（约 20 个函数） | ⬜ |
| T2.8 | 实现响应处理函数 | handle_response_r1~r7（8 个函数） | ⬜ |
| T2.9 | 实现数据线处理 | IDLE → DATA → CRC → CARD_BUSY 状态机 | ⬜ |
| T2.10 | 实现 token 收集 | get_token_bits, get_token_data, handle_common_token_fields | ⬜ |
| T2.11 | 实现注解输出 | 所有 puta/putf/putc/putr/putd/putdf/putdb/putlog 函数 | ⬜ |
| T2.12 | 实现 destroy | 释放内存 | ⬜ |

### 关键风险

- 注解数量巨大（142 个），ann_labels 数组定义繁琐
- 命令/响应处理函数众多，需要逐一实现
- 数据线 CRC16 校验逻辑复杂
- 动态方法分派（getattr）需要转换为 switch-case

---

## T3: spi_dual_quad_c — SPI Dual/Quad 解码器

### 子任务

| # | 子任务 | 描述 | 状态 |
|---|--------|------|------|
| T3.1 | 创建文件 | 创建 `libsigrokdecode/c_decoders/spi_dual_quad_c.c` | ⬜ |
| T3.2 | 定义元数据 | 7 个注解，7 个注解行，7 个选项，1 个二进制输出 | ⬜ |
| T3.3 | 实现 reset/start | 初始化状态，解析选项，设置协议模式 | ⬜ |
| T3.4 | 实现 metadata | 获取 samplerate | ⬜ |
| T3.5 | 实现主解码循环 | CLK 边沿 + CS# 边沿等待 | ⬜ |
| T3.6 | 实现 handle_bit | SPI/Dual/Quad/SQI 模式比特累积 | ⬜ |
| T3.7 | 实现 putdata | 数据输出 + 比特注解 + Python/Binary 输出 | ⬜ |
| T3.8 | 实现 decode_transfer | CS# 取消断言时的传输显示 | ⬜ |
| T3.9 | 实现 SQI 模式 | 命令阶段/数据阶段切换 | ⬜ |
| T3.10 | 实现 destroy | 释放内存 | ⬜ |

### 关键风险

- SQI 模式的命令/数据阶段切换逻辑
- Dual/Quad 模式下 wordsize 必须是 2/4 的倍数
- 与现有 spi_c 解码器功能重叠，需确保不冲突

---

## T4: uart_fast_c — UART-fast 解码器

### 子任务

| # | 子任务 | 描述 | 状态 |
|---|--------|------|------|
| T4.1 | 创建文件 | 创建 `libsigrokdecode/c_decoders/uart_fast_c.c` | ⬜ |
| T4.2 | 定义元数据 | 13 个注解，5 个注解行，6 个二进制输出，9 个选项 | ⬜ |
| T4.3 | 实现 reset/start | 初始化状态机，解析选项，构建状态机表 | ⬜ |
| T4.4 | 实现 metadata | 获取 samplerate，计算 bit_width 等 | ⬜ |
| T4.5 | 实现状态机构建 | init_state_machine()，动态计算各比特位置 | ⬜ |
| T4.6 | 实现主解码循环 | 动态 wait 条件构建（skip + edge），双线独立处理 | ⬜ |
| T4.7 | 实现各状态处理 | wait_for_start_bit, get_start_bit, get_data_bits, get_parity_bit, get_stop_bits | ⬜ |
| T4.8 | 实现 parity 检查 | parity_ok() 函数，支持 6 种校验模式 | ⬜ |
| T4.9 | 实现格式化输出 | format_value()，支持 5 种格式 | ⬜ |
| T4.10 | 实现 break 检测 | 边沿条件中的持续低电平检测 | ⬜ |
| T4.11 | 实现 packet 处理 | packet_idle_us 选项，PACKET Python 输出 | ⬜ |
| T4.12 | 实现二进制输出 | 6 种二进制输出类（rx/tx/rxtx + ok 变体） | ⬜ |
| T4.13 | 实现 destroy | 释放状态机表和内存 | ⬜ |

### 关键风险

- 动态 wait 条件构建（每次循环都不同）
- 浮点精度的比特位置计算
- 双线独立状态机并行处理
- stop_bits 的 0.5 止位处理

---

## T5: cjtag_c — cJTAG 解码器

### 子任务

| # | 子任务 | 描述 | 状态 |
|---|--------|------|------|
| T5.1 | 创建文件 | 创建 `libsigrokdecode/c_decoders/cjtag_c.c` | ✅ |
| T5.2 | 定义元数据 | 33 个注解（16 JTAG + 12 cJTAG + 5 比特类），7 个注解行 | ✅ |
| T5.3 | 实现 reset/start | 初始化状态，注册输出 | ✅ |
| T5.4 | 实现主解码循环 | TCKC 上升沿等待 + TCKC 高电平期间 TMSC 监控 | ✅ |
| T5.5 | 实现 advance_state_machine | JTAG 状态转换表 + cJTAG OAC 处理 | ✅ |
| T5.6 | 实现 handle_rising_tckc_edge | 状态注解输出 + TDI/TDO 比特收集 + 比特串输出 | ✅ |
| T5.7 | 实现 OSCAN1 模式 | 3 周期 nTDI/TMS/TDO 解复用 | ✅ |
| T5.8 | 实现 handle_tapc_state | escape 边沿计数 + cJTAG 状态切换 | ✅ |
| T5.9 | 实现 Python 输出 | NEW STATE, IR/DR TDI/TDO 消息 | ✅ |
| T5.10 | 实现 destroy | 释放内存 | ✅ |

### 关键风险

- 嵌套 wait 循环（TCKC 高电平期间的 TMSC 监控）
- cJTAG OAC 状态机的复杂条件判断
- OSCAN1 模式的 3 周期解复用
- 与现有 jtag_c 解码器功能重叠

---

## T6: CMakeLists.txt 更新

### 子任务

| # | 子任务 | 描述 | 状态 |
|---|--------|------|------|
| T6.1 | 添加解码器名称 | 在 C_DECODERS 列表中添加 qspi_c, sdio_c, spi_dual_quad_c, uart_fast_c, cjtag_c | ✅ |
| T6.2 | 验证构建 | 运行 build_incremental.cmd 确认编译通过 | ⬜ |

---

## T7: 集成测试

### 子任务

| # | 子任务 | 描述 | 状态 |
|---|--------|------|------|
| T7.1 | 编译测试 | 确认所有 5 个 DLL 编译成功 | ⬜ |
| T7.2 | 加载测试 | 确认 PXView 能加载所有 C 解码器 | ⬜ |
| T7.3 | 功能测试 | 使用示例数据验证解码输出与 Python 版本一致 | ⬜ |
| T7.4 | 性能测试 | 对比 C 版本与 Python 版本的解码速度 | ⬜ |

---

## 建议实施顺序

1. **第一阶段**（简单→复杂）：T5 (cjtag) → T3 (spi_dual_quad) → T4 (uart_fast)
2. **第二阶段**（复杂）：T1 (qspi) → T2 (sdio)
3. **第三阶段**：T6 (CMake) → T7 (测试)

理由：cjtag 和 spi_dual_quad 相对简单，可以用来验证 C 解码器框架的使用方式。uart_fast 有独立的状态机但逻辑清晰。qspi 和 sdio 最为复杂，放在最后可以积累经验。
