# C解码器全面对齐Python版本 Spec

## Why
C解码器在annotation类定义、annotation行映射、解码逻辑、协议输出格式、ATK颜色/点注解等方面与Python版本存在系统性差异，导致解码结果不正确、颜色显示不一致、下游解码器无法正确接收数据等问题。需要以can_fd/spi/i2c/uart四个C解码器为标准模板，以Python解码器源代码为逻辑参考，全面修正所有C解码器。

## What Changes
- 修正所有C解码器的annotation类定义，确保与Python版本完全一致（数量、名称、标签、顺序）
- 修正所有C解码器的annotation行映射，确保class-to-row映射与Python版本一致
- 修正所有C解码器的解码逻辑，确保与Python版本一致
- 为缺少ATK颜色/点注解的C解码器添加与Python版本一致的ATK输出
- 修正协议输出格式，确保与Python版本兼容
- 为缺少的功能（如CRC校验、时序检查、采样率检查等）添加与Python版本一致的处理
- 修正annotation文本格式，确保多级文本与Python版本一致

## Impact
- Affected code: `libsigrokdecode/c_decoders/` 下所有37个C解码器
- Affected specs: C解码器API完整性、解码逻辑正确性、下游解码器兼容性
- 参考标准: can_fd_c.c, spi_c.c, i2c_c.c, uart_c.c 作为C解码器实现模板

## 标准C解码器（已验证正确，作为模板）

以下4个C解码器已经过验证，其实现模式作为其他C解码器的参考标准：

### can_fd_c.c 标准模式
- 18个annotation类，3行（bits/fields/warnings）
- 使用自定义putg/putx/putb辅助函数输出annotation
- ann_type始终为0（API层自动+7偏移）
- 协议输出使用c_decoder_put_python()

### spi_c.c 标准模式
- 10个annotation类，8行
- ATK颜色初始化在decode()入口输出
- 比特率META输出
- CS-CHANGE通知

### i2c_c.c 标准模式
- 13个annotation类，4行（bits/addr-data/packets/atk-signs）
- ATK颜色初始化在decode()入口输出
- 完整的START/STOP/ACK/NACK/ADDRESS/DATA处理

### uart_c.c 标准模式
- 21个annotation类，13行
- 多级annotation文本（长/中/短）
- ATK_POINT按位输出
- handle_frame_complete只输出python通道

---

## ADDED Requirements

### Requirement: C解码器annotation类定义必须与Python版本一致
每个C解码器的annotation类数量、枚举值顺序、标签文本必须与对应Python解码器的annotations元组完全一致。

#### Scenario: annotation类数量和顺序一致
- **WHEN** 比较C解码器的ann_labels数组与Python解码器的annotations元组
- **THEN** 两者数量相同，每项的名称和描述文本一致

### Requirement: C解码器annotation行映射必须与Python版本一致
每个C解码器的annotation_rows定义（行名、行标签、包含的类列表）必须与Python版本完全一致。

#### Scenario: annotation行映射一致
- **WHEN** 比较C解码器的ann_rows数组与Python解码器的annotation_rows
- **THEN** 行数量相同，每行的名称、标签、包含的类ID列表一致

### Requirement: C解码器ATK颜色/点注解必须与Python版本一致
C解码器必须在decode()入口处输出与Python版本相同的ATK颜色初始化注解，且ATK点注解的位置和文本必须与Python版本一致。

#### Scenario: ATK颜色初始化
- **WHEN** C解码器开始解码
- **THEN** 输出与Python版本相同的ATK颜色注解（如"color:#fbca47"）

#### Scenario: ATK点注解位置
- **WHEN** C解码器输出ATK数据点注解
- **THEN** 注解的start_sample和end_sample与Python版本一致

### Requirement: C解码器annotation文本格式必须与Python版本一致
C解码器的annotation文本必须使用与Python版本相同的多级格式（长/中/短文本），C_ANN_PUT宏支持可变参数实现多级文本。

#### Scenario: 多级annotation文本
- **WHEN** C解码器输出annotation
- **THEN** 文本字符串数量和内容与Python版本的putgse()列表一致

### Requirement: C解码器解码逻辑必须与Python版本一致
C解码器的状态机转换、边沿检测、位采样、数据组装、校验检查等核心解码逻辑必须与Python版本完全一致。

#### Scenario: 状态机转换一致
- **WHEN** C解码器处理相同的输入信号
- **THEN** 产生与Python版本相同的annotation和协议输出

### Requirement: C解码器协议输出格式必须与Python版本兼容
C解码器通过c_decoder_put_python()输出的协议数据，其命令字符串和数据格式必须与Python版本的putpse()输出兼容，确保下游解码器能正确接收。

#### Scenario: 协议命令字符串一致
- **WHEN** C解码器输出协议数据
- **THEN** 命令字符串与Python版本的协议命令一致

### Requirement: C解码器必须实现Python版本中的采样率检查
当Python版本在decode()入口检查采样率并抛出SamplerateError时，C解码器必须在采样率无效时安全退出（return），不产生错误结果。

#### Scenario: 采样率无效时安全退出
- **WHEN** C解码器的采样率为0或未设置
- **THEN** 解码器安全退出，不产生错误的annotation

---

## 各C解码器具体问题清单

### 严重问题（影响解码正确性或下游兼容性）

| 解码器 | 问题 | Python参考 |
|--------|------|-----------|
| can_c.c | annotation范围缺少位宽扩展（putg的left/right偏移） | can/pd.py putg()方法 |
| can_c.c | 协议输出格式为逗号分隔字符串，非Python元组 | can/pd.py putpy() |
| jtag_c.c | 协议输出缺少状态名称字符串 | jtag/pd.py ['NEW STATE', self.state] |
| jtag_c.c | 协议输出格式完全不同（无bitstring、无per-bit ss/es） | jtag/pd.py ['DR TDI', ['01110001', [[ss,es],...]]] |
| swd_c.c | 缺少请求奇偶校验检查 | swd/pd.py calc_parity |
| swd_c.c | 协议输出缺少ACK值 | swd/pd.py (addr, data, ack) |
| i2s_c.c | 二进制输出缺少WAV文件头 | i2s/pd.py WAV header输出 |
| onewire_c.c | 缺少完整时序检查（RSTL/RSTH/PDH/PDL/SLOT/REC/LOWR） | onewire_link/pd.py timing字典 |
| onewire_c.c | 无存在检测超时机制（可能永久阻塞） | onewire_link/pd.py wait_falling_timeout() |
| onewire_c.c | 不处理"无存在检测"情况 | onewire_link/pd.py ['RESET/PRESENCE', False] |
| nrzi_c.c | 协议输出格式不兼容（命名命令 vs 裸整数） | nrzi/pd.py self.putp(matched) |
| 4b5b_c.c | 架构差异：C直接读逻辑信号，Python从NRZI解码器接收bit | 4b5b/pd.py inputs=['nrzi'] |
| 4b5b_c.c | 协议输出格式不兼容（命名命令 vs 元组） | 4b5b/pd.py (value, is_control_symbol) |
| lin_c.c | 输入源完全不同（直接读逻辑信号 vs 堆叠在UART上） | lin/pd.py inputs=['uart'] |
| lin_c.c | 缺少帧中断处理（丢弃未完成帧） | lin/pd.py wipe_break_null_byte() |
| graycode_c.c | 硬编码2位，Python支持任意位数 | graycode/pd.py bits选项 |
| numbers_and_state_c.c | 缺少enum映射功能 | numbers_and_state/pd.py enum选项 |
| iso7816_c.c | 缺少PCAP输出和T=1多块/APDU处理 | iso7816/pd.py |

### 中等问题（影响功能完整性或用户体验）

| 解码器 | 问题 | Python参考 |
|--------|------|-----------|
| hdlc_c.c | 缺少CS-CHANGE输出 | hdlc/pd.py ['CS-CHANGE', None, None] |
| hdlc_c.c | 协议输出缺少per-byte ss/es | hdlc/pd.py Data(ss,es,val) |
| i2s_c.c | annotation行数量不同（C:3行 vs Python:1行） | i2s/pd.py 无annotation_rows |
| i2s_c.c | 协议输出格式不同（5字节 vs 字符串+整数） | i2s/pd.py ['DATA', ['L', value]] |
| microwire_c.c | 协议输出格式不同（struct字节 vs PyPacket命名元组） | microwire/pd.py PyPacket |
| mdio_c.c | 协议输出格式不同（8字节 vs 元组） | mdio/pd.py (clause45, addr, is_read, portad, devad, data) |
| ps2_c.c | 缺少ATK颜色/点注解 | ps2/pd.py ATK颜色值 |
| ps2_c.c | Word注解范围不同（包含奇偶校验位 vs 不包含） | ps2/pd.py bits[1].ss到bits[8].es |
| ir_nec_c.c | 缺少设备名/按键名查找表 | ir_nec/lists.py address/command字典 |
| ir_rc5_c.c | 缺少系统名/命令名查找表 | ir_rc5/lists.py system/command字典 |
| ir_sirc_c.c | 缺少设备名查找表 | ir_sirc/lists.py ADDRESSES字典 |
| spdif_c.c | 缺少重检逻辑（可能丢失初始数据） | spdif/pd.py decode_re_get_pulse_type |
| usb_signalling_c.c | 协议输出格式略有差异 | usb_signalling/pd.py |

### 轻微问题（不影响核心功能）

| 解码器 | 问题 | Python参考 |
|--------|------|-----------|
| dcf77_c.c | 星期/月份名称硬编码英文 vs locale依赖 | dcf77/pd.py calendar.day_name |
| dmx512_c.c | 错误处理流程细微差异 | dmx512/pd.py |
| cec_c.c | 无差异 | - |
| can_fd_c.c | bitpack_msb限制32位 | can-fd/pd.py 无限制 |
| can_fd_c.c | 缺少采样率异常检查 | can-fd/pd.py SamplerateError |

## MODIFIED Requirements

### Requirement: C解码器ann_type默认值
原逻辑：`int ann_type = i + 7`（decoder.c:1253）
保持不变：API层已统一处理+7偏移，C解码器ann_labels第一列保持空字符串

### Requirement: C解码器ATK颜色值
各解码器的ATK颜色值必须与Python版本一致，不同解码器可能使用不同颜色。

## REMOVED Requirements

无移除的需求。
