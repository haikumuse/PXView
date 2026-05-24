# Python 解码器移植到 C — Batch 01 检查清单

> 版本：1.0  
> 日期：2026-05-23  
> 用途：每个 C 解码器实现完成后，逐项检查

---

## 通用检查项（适用于所有 5 个解码器）

### 元数据完整性

- [ ] `id` 以 `_c` 结尾（如 `qspi_c`）
- [ ] `name` 包含 `(C)` 后缀（如 `Smart QSPI(C)`）
- [ ] `longname` 与 Python 版本一致
- [ ] `desc` 与 Python 版本一致
- [ ] `license` 为 `"gplv2+"`
- [ ] `inputs` 为 `{"logic"}`
- [ ] `outputs` 与 Python 版本一致
- [ ] `tags` 与 Python 版本一致

### 通道定义

- [ ] 必选通道 `channels` 数量与 Python 一致
- [ ] 可选通道 `optional_channels` 数量与 Python 一致
- [ ] 每个通道的 `id`、`name`、`desc` 与 Python 一致
- [ ] 每个通道的 `type` 合理（SCLK/SDATA/COMMON）
- [ ] 每个通道的 `idn` 已添加（Python 版本可能缺失）
- [ ] 通道索引顺序与 Python 一致（影响 `c_decoder_get_pin` 调用）

### 选项定义

- [ ] 选项数量与 Python 一致
- [ ] 每个选项的 `id`、`desc` 与 Python 一致
- [ ] 默认值与 Python 一致
- [ ] 可选值列表与 Python 一致
- [ ] 字符串选项用 `c_decoder_get_option_string()` 读取
- [ ] 整数选项用 `c_decoder_get_option_int()` 读取
- [ ] 浮点选项用 `c_decoder_get_option_double()` 读取

### 注解定义

- [ ] 注解数量 `num_annotations` 与 Python 一致
- [ ] `ann_labels` 每个条目有 3 个字符串（完整、中等、最短）
- [ ] 注解顺序与 Python 的 `annotations` 元组一致
- [ ] `ann_type` 值与 Python 版本匹配（如果有）

### 注解行定义

- [ ] 注解行数量与 Python 一致
- [ ] 每行的 `id`、`desc` 与 Python 一致
- [ ] 每行的 `ann_classes` 数组以 `-1` 结尾
- [ ] 每行的 `num_ann_classes` 正确

### 二进制输出（如果有）

- [ ] 二进制输出数量与 Python 一致
- [ ] 每个二进制输出的 `bin_class`、`id`、`desc` 正确

### 函数实现

- [ ] `reset()` 正确分配并清零私有状态
- [ ] `start()` 正确注册输出（ANN/PYTHON/BINARY/META）
- [ ] `start()` 正确解析所有选项
- [ ] `start()` 正确检查通道可用性（`c_decoder_has_channel`）
- [ ] `metadata()` 正确处理 `SRD_CONF_SAMPLERATE`（如果需要）
- [ ] `decode()` 主循环正确使用条件构建器
- [ ] `decode()` 正确处理首次采样
- [ ] `destroy()` 正确释放所有分配的内存
- [ ] `srd_c_decoder_entry()` 和 `srd_c_decoder_api_version()` 导出函数存在

### 内存安全

- [ ] 无内存泄漏（`destroy()` 释放所有 `g_malloc` 分配）
- [ ] 无缓冲区溢出（数组访问有边界检查）
- [ ] 无空指针解引用
- [ ] 无未初始化变量使用
- [ ] 固定大小数组足够大（token、比特收集等）

### Python 输出兼容性

- [ ] `c_decoder_put_python()` 的数据格式与 Python 版本兼容
- [ ] 上层解码器能正确解析 C 版本的 Python 输出
- [ ] `CS-CHANGE`、`DATA`、`BITS`、`TRANSFER` 等消息格式正确

---

## QSPI 特定检查项

- [ ] 命令表包含所有 40+ 个命令条目
- [ ] 每个命令的数据序列（`data_after`）正确
- [ ] Quad 数据组合：4 线 × 2 bit → 4 字节，比特顺序正确
- [ ] Dual 数据组合：2 线 × 4 bit → 2 字节，比特顺序正确
- [ ] MSB-first / LSB-first 比特顺序处理正确
- [ ] 地址模式切换：0xB7 → 32-bit, 0xE9 → 24-bit
- [ ] ADDRESS_BY_MODE 根据当前 ads 设置动态解析
- [ ] DUMMY_BY_MODE 根据当前 ads 设置动态解析
- [ ] Page Program 的 256 个 WRITE_BYTE_SINGLE 正确处理
- [ ] READ_BYTE_CONTINUOUS 不递增 state_count
- [ ] invalidlevel 选项正确过滤无效数据
- [ ] frame 模式下正确收集字节并输出 TRANSFER
- [ ] bitrate meta 输出正确计算
- [ ] CS# 取消断言时正确重置命令解析状态

---

## SDIO 特定检查项

- [ ] 142 个注解类全部定义
- [ ] CMD0-CMD63 注解（索引 0-63）正确
- [ ] ACMD0-ACMD63 注解（索引 64-127）正确
- [ ] 字段注解（128-134）正确
- [ ] 解码注解（135-136）正确
- [ ] 数据注解（137-141）正确
- [ ] crc7() 函数实现与 Python 版本输出一致
- [ ] crc16() 函数实现与 Python 版本输出一致
- [ ] cmd_names 查找表包含所有 64 个条目
- [ ] acmd_names 查找表包含所有 64 个条目
- [ ] accepted_voltages 查找表正确
- [ ] card_status 查找表正确
- [ ] CMD55 + ACMD 模式正确切换
- [ ] R2 响应（136 bit）正确处理
- [ ] 1-line 和 4-line 数据模式正确切换
- [ ] 数据 CRC16 校验正确
- [ ] CARD_BUSY 状态正确处理
- [ ] CMD52/CMD53 的 SDIO 特定字段解析正确
- [ ] token 比特收集的 es 估算正确
- [ ] puta() 函数的索引映射 `token[47-8-e]` 到 `token[47-8-s]` 正确

---

## SPI Dual/Quad 特定检查项

- [ ] SPI/Dual/Quad/SQI 四种协议模式正确实现
- [ ] Quad 模式：每个时钟 4 bit，bitcount += 4
- [ ] Dual 模式：每个时钟 2 bit，bitcount += 2
- [ ] SPI 模式：每个时钟 1 bit，bitcount += 1
- [ ] SQI 模式：前 8 bit 为 SPI 命令阶段，之后切换为 Quad
- [ ] CS# 取消断言后 SQI 重置为命令阶段
- [ ] wordsize 必须是 2/4 倍数的检查
- [ ] MSB-first / LSB-first 比特顺序处理正确
- [ ] 比特注解（SIO0-SIO3）正确输出
- [ ] 数据注解正确输出
- [ ] 传输注解根据协议模式格式化
- [ ] Python BITS 输出包含正确的线数
- [ ] Binary 输出正确
- [ ] bitrate meta 输出正确

---

## UART-fast 特定检查项

- [ ] RX 和 TX 独立状态机正确并行运行
- [ ] 至少需要 RX 或 TX 之一
- [ ] 状态机表动态构建正确（start bit + data bits + parity + stop bits）
- [ ] bit_width = samplerate / baudrate 计算正确
- [ ] bit_samplenum = bit_width * 0.5（采样点在半比特位置）
- [ ] 0.5 止位正确处理（半比特宽度采样点）
- [ ] 6 种校验模式（none/odd/even/zero/one/ignore）正确实现
- [ ] 5 种数据格式（hex/dec/oct/bin/ascii）正确实现
- [ ] 信号反转（invert 选项）正确处理
- [ ] Start bit 错误检测正确
- [ ] Stop bit 错误检测正确
- [ ] Parity 错误检测正确
- [ ] Break 检测正确（持续低电平超过一帧长度）
- [ ] Packet 处理（packet_idle_us 选项）正确
- [ ] 6 种二进制输出（rx/tx/rxtx + ok 变体）正确
- [ ] FRAME Python 输出正确
- [ ] PACKET Python 输出正确
- [ ] 动态 wait 条件构建正确（skip + edge）
- [ ] show_data_point 选项正确显示采样点
- [ ] 状态机表在 destroy() 中释放

---

## cJTAG 特定检查项

- [ ] 16 个 JTAG 状态注解正确
- [ ] 12 个 cJTAG 状态注解正确（含 CJTAG_OAC）
- [ ] 5 个比特类注解（TDI/TDO bit, TDI/TDO bitstring, TMS bit）正确
- [ ] JTAG 状态转换表（16×2）正确
- [ ] cJTAG OAC 状态机正确实现
- [ ] oaclen = 12 和 oaclen = 36 两种情况正确处理
- [ ] OSCAN1 模式 3 周期解复用正确
- [ ] escape_edges 计数正确（6 → CJTAG_OAC, ≥8 → FOUR_WIRE）
- [ ] TCKC 高电平期间的 TMSC 边沿监控正确
- [ ] 嵌套 wait 循环不会导致死锁或性能问题
- [ ] TDI/TDO 比特串在 UPDATE-* 状态正确输出
- [ ] NEW STATE Python 输出正确
- [ ] IR/DR TDI/TDO Python 输出格式正确
- [ ] first/first_bit 标志正确管理
- [ ] CJTAG_OAC 状态的注解索引正确（索引 25 = 16 + 9）

---

## 构建与集成检查

- [ ] CMakeLists.txt 中 C_DECODERS 列表已添加新解码器
- [ ] 编译无错误、无警告
- [ ] DLL 文件生成在 `build.dir/decoders/c_decoders/` 目录
- [ ] PXView 能加载并显示 C 解码器
- [ ] C 解码器出现在解码器选择列表中
- [ ] 通道分配界面正确显示所有通道
- [ ] 选项界面正确显示所有选项及默认值
- [ ] 解码输出与 Python 版本视觉一致

---

## 性能对比检查

- [ ] C 版本解码速度至少比 Python 版本快 5 倍
- [ ] 内存使用合理（无持续增长）
- [ ] 大数据量下无崩溃
