# Batch 06 移植检查清单

## 通用检查项（每个解码器都必须验证）

### 元数据正确性

- [ ] id 与 Python 解码器一致
- [ ] name 与 Python 解码器一致
- [ ] longname 与 Python 解码器一致
- [ ] desc 与 Python 解码器一致
- [ ] license 为 `'gplv2+'`
- [ ] inputs 包含 `"logic"`
- [ ] outputs 与 Python 一致
- [ ] tags 与 Python 一致

### 通道定义

- [ ] 通道 id、name、desc 与 Python 一致
- [ ] 通道 idn（如有）与 Python 一致
- [ ] 通道顺序与 Python 一致
- [ ] 可选通道正确标记

### 选项定义

- [ ] 选项 id 与 Python 一致
- [ ] 选项 idn（如有）与 Python 一致
- [ ] 选项 desc 与 Python 一致
- [ ] 选项默认值类型和值正确
- [ ] 选项可选值列表完整
- [ ] 选项在 start() 中正确读取和解析

### 注解定义

- [ ] 注解类数量与 Python 一致
- [ ] 每个注解类的标签文本正确（至少英文标签）
- [ ] 注解行定义正确（id、label、包含的类）
- [ ] 注解行中类的顺序与 Python 一致

### 代码结构

- [ ] reset() 正确初始化所有状态变量
- [ ] start() 注册 SRD_OUTPUT_ANN 输出
- [ ] start() 读取所有选项
- [ ] metadata() 正确保存 samplerate（如需要）
- [ ] decode() 主循环使用 c_cond_wait() 等待条件
- [ ] decode() 检查 samplerate 有效性
- [ ] 私有数据通过 c_decoder_set_private/get_private 管理
- [ ] 内存分配使用 g_malloc0，释放使用 g_free
- [ ] 导出函数 srd_c_decoder_entry() 和 srd_c_decoder_api_version() 正确定义

### 功能正确性

- [ ] 状态机逻辑与 Python 一致
- [ ] 位值判定逻辑与 Python 一致
- [ ] 注解输出位置（start_sample, end_sample）与 Python 一致
- [ ] 注解输出文本格式与 Python 一致
- [ ] 校验和/校验位计算正确
- [ ] 边沿检测条件与 Python 一致
- [ ] 时序参数计算公式与 Python 一致

---

## delta-sigma_c 专项检查

- [ ] 2 个通道（DAT, CLK）正确定义
- [ ] 5 个选项正确读取
- [ ] CLK 上升沿等待条件正确
- [ ] sinc1 滤波器算法正确（DELTA1, CNTR, DN0, DN1, CN3）
- [ ] sinc2 滤波器算法正确（DELTA1, CN1, CNTR, DN0, DN1, CN3, CN4, DN3）
- [ ] sinc3 滤波器算法正确（DELTA1, CN1, CN2, CNTR, DN0, DN1, CN3, CN4, CN5, DN3, DN5）
- [ ] sinc_fast 选项处理（回退到 sinc3 或报错）
- [ ] OSR 计数器到达时输出滤波结果
- [ ] shift 右移正确应用
- [ ] scale 缩放正确应用
- [ ] 第一个样本特殊处理（输出 'X'）
- [ ] Bit Stream 注解输出当前 dat 值
- [ ] Filtered 注解输出 code >> shift
- [ ] Converted 注解输出 code * scale

## em4100_c 专项检查

- [ ] 1 个通道（data）正确定义
- [ ] 3 个选项正确读取（polarity, datarate, coilfreq）
- [ ] bit_width 计算正确：(samplerate / coilfreq) * datarate
- [ ] halfbit_limit 计算正确：bit_width/2 + bit_width/4
- [ ] polarity 设置正确（active-low → 0, active-high → 1）
- [ ] Manchester 解码逻辑正确（长脉冲/短脉冲判定）
- [ ] HEADER 状态：9 个连续 '1' 检测
- [ ] PAYLOAD 状态：50 位收集，5 位一组
- [ ] Version/customer 注解类 2，Data 注解类 3
- [ ] 行校验 XOR 计算正确
- [ ] tag 值累积正确（每 4 位左移 4 位）
- [ ] TRAILER 状态：4 列校验 + 1 停止位
- [ ] 列校验计算正确
- [ ] 停止位检查（应为 0）
- [ ] Tag 注解格式：`Tag: %010X`
- [ ] 全部校验通过才输出 Tag 注解
- [ ] 状态重置回 HEADER 时清空所有校验状态

## dsi_c 专项检查

- [ ] 1 个通道（dsi）正确定义，idn = 'dec_dsi_chan_dsi'
- [ ] 1 个选项（polarity）正确读取，idn = 'dec_dsi_opt_polarity'
- [ ] halfbit 计算正确：int((samplerate * 0.0016667) / 2.0)
- [ ] 极性反转逻辑正确（active-high 时 dsi ^= 1）
- [ ] old_dsi 初始值正确（active-low → 1, active-high → 0）
- [ ] IDLE 状态：等待任意边沿，记录半位起始
- [ ] PHASE0/PHASE1 交替正确
- [ ] 停止位检测（bit==1 且 phase0==1）
- [ ] 17 位 Forward 帧和 9 位 Backward 帧区分
- [ ] Backward 帧：level = f / 2.55，输出百分比
- [ ] 半位超时检测逻辑正确
- [ ] 边沿位置记录正确
- [ ] 位的 ss/es 计算正确
- [ ] 状态重置时清空 edges, bits, ss_es_bits

## em4305_c 专项检查

- [ ] 1 个通道（data）正确定义，idn = 'dec_em4305_chan_data'
- [ ] 7 个选项正确读取（含 idn）
- [ ] field_clock 计算正确：samplerate / coilfreq
- [ ] 6 个时序参数计算正确（wzmax, wzmin, womax, ffs, writegap, nogap）
- [ ] FFS_SEARCH 状态：pl > ffs 检测
- [ ] FFS_DETECTED 状态：pl > writegap 检测
- [ ] 写模式退出检测：超过 nogap 时间无间隙
- [ ] 0 位检测：wzmin < gap < wzmax
- [ ] 1 位检测：womax < gap < nogap
- [ ] 多个 1 位计算正确
- [ ] 1 位后跟 0 位检测
- [ ] bits_pos 数组正确存储 [value, ss, es]
- [ ] 50 位帧解析（Login 命令）
- [ ] 57 位帧解析（Write/Read 命令）
- [ ] get_8_bits() 正确（8 位连续，含校验位间隔）
- [ ] get_32_bits() 正确（4×8 位，间隔 9 位）
- [ ] 行校验检查正确
- [ ] 列校验检查正确
- [ ] 停止位检查（应为 0）
- [ ] decode_config() 所有字段正确解析
- [ ] em4100_decode1/em4100_decode2 正确实现
- [ ] em4100_decode 选项控制（on/off）
- [ ] 命令表 cmds 正确
- [ ] 比特率/编码器/延迟字符串表正确

## dcc_c 专项检查

- [ ] 1 个通道（data）正确定义
- [ ] 8 个选项正确读取和解析
- [ ] Search_byte 多进制解析正确（10 进制 → 2 进制 → 16 进制）
- [ ] 搜索值范围验证正确
- [ ] samplerate 最低 25kHz 检查
- [ ] accuracy 计算正确：1/samplerate * 1000000 µs
- [ ] 初始边沿检测（上升沿 → 下降沿）
- [ ] 采样率信息注解输出
- [ ] 主循环边沿检测（cond1 → cond2）
- [ ] 位值 '1' 判定条件正确（part1, part2, 差值）
- [ ] 位值 '0' 判定条件正确（part1 或 part2 长脉冲）
- [ ] 半'0'+半'1' 检测和边沿方向切换
- [ ] 短脉冲过滤逻辑正确（≤ 4µs）
- [ ] Railcom cutout 检测正确
- [ ] unknown timing 处理和重新同步
- [ ] stretched zero 检测和注解
- [ ] WAITINGFORPREAMBLE 状态正确
- [ ] PREAMBLE 状态：≥10 位前导码验证
- [ ] ADDRESSDATABYTE 状态：8 位字节 + 分隔符/结束符
- [ ] decodedBytes 存储正确（值和位置）
- [ ] handleDecodedBytes() 校验和计算正确（XOR）
- [ ] 服务模式包解析正确
- [ ] 多功能解码器地址解析正确（7 位 / 14 位）
- [ ] 所有 cmd 类型解析正确（000-111）
- [ ] 附件解码器解析正确（128-191）
- [ ] 空闲包解析正确（255）
- [ ] 搜索功能正确实现
- [ ] weekday/month 查找表正确
- [ ] 边沿方向切换后重新同步逻辑正确
- [ ] 所有格式字符串正确（str, hex, 浮点格式）

---

## 构建集成检查

- [ ] CMakeLists.txt 中 C_DECODERS 列表已添加 5 个新解码器
- [ ] 增量构建成功无错误
- [ ] 5 个 DLL 文件正确生成到 build.dir/decoders/c_decoders/
- [ ] PXView 启动无崩溃
- [ ] 每个解码器在 PXView 解码器列表中可见
- [ ] 每个解码器可添加到信号上无崩溃
- [ ] 每个解码器的选项在 UI 中正确显示

## 回归测试

- [ ] 原有 37 个 C 解码器仍正常工作
- [ ] Python 版本的 5 个解码器仍可正常使用
- [ ] C 版本和 Python 版本对相同输入产生一致的注解输出
