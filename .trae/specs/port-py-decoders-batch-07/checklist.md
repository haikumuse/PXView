# Python→C 解码器移植检查清单 — Batch 07

每个解码器完成后的逐项检查。所有项目必须通过才能标记为完成。

---

## 通用检查（适用于所有5个解码器）

### 文件结构
- [ ] 文件位于 `libsigrokdecode/c_decoders/<name>_c.c`
- [ ] 包含正确的头文件：`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<glib.h>`, `"libsigrokdecode.h"`
- [ ] 注解枚举以 `NUM_ANN` 结尾
- [ ] 私有结构体命名为 `<name>_priv`
- [ ] 所有回调函数以 `<name>_` 为前缀

### 元数据一致性
- [ ] `id` 字段为 `<python_id>_c`
- [ ] `name` 字段为 `<Python Name>(C)`
- [ ] `longname` 包含 `(C)` 后缀
- [ ] `desc` 包含 `C implementation` 说明
- [ ] `license` 与 Python 版本一致
- [ ] 通道数量、id、name、desc、idn 与 Python 版本完全一致
- [ ] 可选通道数量和定义与 Python 版本一致
- [ ] 选项数量和定义与 Python 版本一致
- [ ] 注解数量和标签与 Python 版本一致
- [ ] 注解行定义与 Python 版本一致
- [ ] inputs 为 `{"logic", NULL}`
- [ ] outputs 与 Python 版本一致（空则为 `{NULL}`）
- [ ] tags 与 Python 版本一致

### 回调函数
- [ ] `reset()`: 分配私有数据（`g_malloc0`），`memset` 清零，设置初始状态
- [ ] `start()`: 注册输出（`c_decoder_register_output`），获取选项和 samplerate
- [ ] `decode()`: 主解码循环，检查 `c_cond_wait` 返回值，释放 `c_cond_builder`
- [ ] `destroy()`: `g_free` 释放私有数据，`c_decoder_set_private(di, NULL)`
- [ ] `metadata()`（如需要）: 处理 `SRD_CONF_SAMPLERATE`

### 入口函数
- [ ] `srd_c_decoder_entry()`: 初始化选项默认值，返回解码器结构体指针
- [ ] `srd_c_decoder_api_version()`: 返回 `SRD_C_DECODER_API_VERSION`
- [ ] 两个函数都有 `SRD_C_DECODER_EXPORT` 前缀

### CMakeLists.txt
- [ ] `C_DECODERS` 列表中添加了 `<name>_c`

### 编译
- [ ] 无编译警告
- [ ] 无编译错误
- [ ] DLL 成功生成到 `build.dir/decoders/c_decoders/`

---

## guess_bitrate_c 专项检查

- [ ] samplerate 为0时 decode 函数安全返回（不崩溃）
- [ ] 第一个边沿正确记录 `ss_edge`
- [ ] 后续边沿正确计算间距 `b = samplenum - ss_edge`
- [ ] `bitwidth` 初始值为0（表示未设置）
- [ ] 仅当 `b < bitwidth` 或 `bitwidth == 0` 时更新
- [ ] 比特率计算：`samplerate / b`（整数除法）
- [ ] 注解范围：`ss_edge` 到 `samplenum`
- [ ] 比特率格式化为十进制整数（`%llu`）
- [ ] `ss_edge` 在每次边沿后更新

---

## iec_c 专项检查

- [ ] 3个必需通道 + 1个可选通道定义正确
- [ ] step 0-3 的 wait 条件构建正确（OR 分支）
- [ ] ATN 下降沿（matched & 1）在任何 step 中都重置 step=0
- [ ] step 0: DATA low + CLK high → step=1
- [ ] step 1: DATA high + CLK high → 开始接收（step=2），CLK low → 中止（step=0）
- [ ] step 2: DATA low + CLK high → EOI=True，CLK low → step=3
- [ ] step 3: CLK 上升沿锁存数据，CLK 下降沿结束位
- [ ] 8位完成后调用 handle_bits
- [ ] `saved_ATN = !atn`（取反，ATN低电平有效）
- [ ] 命令解码完整：GTL, SDC, PPC, GET, TCT, LLO, DCL, PPU, SPE, SPD, UNL, UNT
- [ ] Listener 地址：0x20-0x3e → `'L' + chr(dbyte + 0x10)`
- [ ] Talker 地址：0x40-0x5e → `'T' + chr(dbyte - 0x10)`
- [ ] IEC 特有：Channel reopen (0x60-0x6f), Channel close (0xdf-0xef), Channel open (0xf0-0xff)
- [ ] 数据模式：可打印ASCII、LF、CR
- [ ] EOI 输出正确

---

## eth_an_c 专项检查

- [ ] samplerate 为0时 decode 函数安全返回
- [ ] 等待上升沿 → 等待边沿的顺序正确
- [ ] 脉冲宽度计算使用 `double` 类型
- [ ] NLP 检测范围：1μs ~ 10μs
- [ ] 逻辑1间隔：60μs ~ 70μs
- [ ] 逻辑0间隔：120μs ~ 140μs
- [ ] 逻辑1时设置 `hex` 对应位
- [ ] data_list 数组正确记录 start/end/Pre start/Pre end
- [ ] 16位收集完成后调用 change_state
- [ ] 状态转换逻辑正确：
  - [ ] base page → base page ack（当 `(hex>>14)&0x3 == 0x3`）
  - [ ] base page ack → next page
  - [ ] next page → next page ack（当 `(hex>>14)&0x3 == 0x1`）
  - [ ] next page ack → base page
- [ ] change_state 仅在 `pre_hex != hex` 时执行
- [ ] Base page 解码：
  - [ ] Selector field 正确（0x01=802.3, 0x02=802.9）
  - [ ] Technology ability field 完整（10BaseT-HD/FD, 100BaseTX-HD/FD, 100BaseT4, FC, AsyFC, Reserved, RF, ACK, NP）
  - [ ] 各位注解范围使用 `dl_pre_start[i]` 到 `dl_end[i]`
  - [ ] 整体注解范围 `dl_pre_start[0]` 到 `dl_end[15]`
- [ ] Next page 解码：
  - [ ] MP 位检测正确
  - [ ] Technology ability field 完整
  - [ ] Unformatted code field + Master-Slave seed value 注解
  - [ ] Other fields 注解
- [ ] updateOnceDecode：先保存 pre_hex，再清空
- [ ] hex 格式化为 `"0x%x"` 格式

---

## gpib_c 专项检查

- [ ] 16个通道定义正确（dio1-dio8, eoi, dav, nrfd, ndac, ifc, srq, atn, ren）
- [ ] sample_total 选项定义和默认值（0）正确
- [ ] 第一次等待 DAV 低电平，后续等待 DAV 下降沿
- [ ] skip 条件动态更新正确
- [ ] 16个通道值通过 `c_decoder_get_pin` 逐个读取
- [ ] 数据字节反转 `item ^ 0xff`
- [ ] ATN 检测：`pins[14] == 0`
- [ ] EOI 检测：`pins[8] == 0`
- [ ] 延迟一拍输出：第一个字节只保存，从第二个开始输出上一个
- [ ] GPIB 命令解码完整（GTL, SDC, PPC, GET, TCT, LLO, DCL, PPU, SPE, SPD, UNL, UNT）
- [ ] Listener 地址格式化正确
- [ ] Talker 地址格式化正确
- [ ] 数据模式 ASCII 解码正确
- [ ] EOI 输出正确
- [ ] itemcount 达到16时重置

---

## fsi_c 专项检查

- [ ] 2个通道定义正确（data, clock）
- [ ] 数据反相 `fsi_data = !data` 正确实现
- [ ] `fsi_data_prev` 在循环末尾更新
- [ ] BREAK 检测：仅上升沿，256个连续1
- [ ] BREAK 超过256个采样时输出警告
- [ ] 主从边沿选择逻辑正确：
  - [ ] 从设备发送状态（TAR, RX_*, CRC+valid_response）→ 下降沿
  - [ ] 主设备发送状态（其他）→ 上升沿
- [ ] IDLE → TX_SLAVE_ID：检测 fsi_data_prev == 1
- [ ] TX_SLAVE_ID：2位 slave ID，MSB first（`(id >> 1) | (data << 1)`）
- [ ] COMMAND 状态：所有6种命令正确解码
- [ ] 无效命令（8位）检测和输出
- [ ] DIRECTION：1=Read, 0=Write
- [ ] REL_ADDRESS_SIGN：1=(-), 0=(+)
- [ ] ADDRESS：SAME_ADR/REL_ADR/ABS_ADR 地址计算正确
- [ ] prev_address 按 slave_id (0-3) 索引存储
- [ ] DATA_SIZE：BYTE/HALF_WORD/WORD/UNKNOWN 检测
- [ ] TERM 命令特殊检测条件正确
- [ ] TX_DATA：8/16/32位数据，格式化输出（0x%02x/0x%04x/0x%08x）
- [ ] CRC 状态：
  - [ ] 4位 CRC 接收
  - [ ] `computed_crc_tx_end` 在 `crc_count==0` 时保存
  - [ ] CRC 比较和 GOOD/BAD 输出
  - [ ] ACK 响应后更新 prev_address
- [ ] TAR 状态：
  - [ ] tar_cycles = 3
  - [ ] 响应处理逻辑
  - [ ] 新 START 检测
  - [ ] 超时256周期检测
- [ ] RX_SLAVE_ID：2位，与 tx_slave_id 比较输出警告
- [ ] RESPONSE 状态：所有6种响应正确解码
- [ ] RX_DATA：8/16/32位，格式 `0x%08x`
- [ ] RX_IPOLL_INTERRUPT_FIELD：2位，格式 `0x%01x`
- [ ] RX_IPOLL_DMA_CONTROL_FIELD：3位，格式 `0x%01x`
- [ ] BREAK_TAR_QUEUED → BREAK_TAR → IDLE
- [ ] CRC LFSR 计算在循环末尾执行（当 crc_calculating 为真时）
- [ ] CRC LFSR 多项式 0x7 实现正确
- [ ] busy_seq_count 正确递增和重置
- [ ] 所有注解使用 ss_block/es_block 范围
- [ ] ss_block 在每个新字段开始时更新
