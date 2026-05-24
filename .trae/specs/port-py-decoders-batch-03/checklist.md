# Python解码器移植检查清单 — 第三批

## 通用检查项（适用于所有5个解码器）

### 文件结构
- [ ] C文件位于 `libsigrokdecode/c_decoders/<name>_c.c`
- [ ] 包含正确的头文件：`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<glib.h>`, `"libsigrokdecode.h"`
- [ ] 文件末尾有 `srd_c_decoder_entry()` 导出函数
- [ ] 文件末尾有 `srd_c_decoder_api_version()` 导出函数
- [ ] SRD_C_DECODER_EXPORT 宏正确使用

### srd_c_decoder 结构体
- [ ] `.id` 以 `_c` 结尾（如 `"ac97_c"`）
- [ ] `.name` 包含 `(C)` 标识（如 `"AC '97(C)"`）
- [ ] `.longname` 包含 `(C)` 标识
- [ ] `.desc` 说明是C实现
- [ ] `.license` = `"gplv2+"`
- [ ] `.channels` 数组正确（id, name, desc, order, type, idn）
- [ ] `.num_channels` 正确
- [ ] `.optional_channels` 正确（如无则为NULL，num=0）
- [ ] `.num_optional_channels` 正确
- [ ] `.options` 正确（如无则为NULL，num=0）
- [ ] `.num_options` 正确
- [ ] `.num_annotations` 与ann_labels数组大小一致
- [ ] `.ann_labels` 每个条目最多3个字符串（长→短）
- [ ] `.num_annotation_rows` 正确
- [ ] `.annotation_rows` 每行有id, desc, ann_classes数组, num_ann_classes
- [ ] `.inputs` = `{"logic", NULL}`, num=1
- [ ] `.outputs` 正确（如无则为`{NULL}`, num=0）
- [ ] `.binary` 正确（如无则为NULL，num=0）
- [ ] `.tags` 正确，以NULL结尾
- [ ] `.num_tags` 正确
- [ ] `.reset` 函数指针正确
- [ ] `.start` 函数指针正确
- [ ] `.decode` 函数指针正确
- [ ] `.end` = NULL（如不需要）
- [ ] `.metadata` 函数指针正确（如需要samplerate）
- [ ] `.destroy` 函数指针正确
- [ ] `.recv_proto` = NULL（如不需要）

### 私有数据
- [ ] 使用 `g_malloc0()` 分配
- [ ] 在 `destroy()` 中使用 `g_free()` 释放
- [ ] reset() 中使用 `memset()` 清零
- [ ] 所有动态分配的内存都在destroy()中释放

### 输出注册
- [ ] `c_decoder_register_output(di, SRD_OUTPUT_ANN, ...)` 在start()中调用
- [ ] output_id保存在私有结构体中
- [ ] 如有binary输出：`c_decoder_register_output(di, SRD_OUTPUT_BINARY, ...)`
- [ ] 如有python输出：`c_decoder_register_output(di, SRD_OUTPUT_PYTHON, ...)`

### 注解输出
- [ ] 使用 `C_ANN_PUT(di, ss, es, out_ann, cls, ...)` 宏
- [ ] ss < es（start_sample < end_sample）
- [ ] 注解类索引不越界
- [ ] 格式化字符串使用snprintf，缓冲区足够大

### 条件等待
- [ ] 每次循环开始创建 `srd_cond_builder *cb = c_cond_new()`
- [ ] 添加条件后调用 `c_cond_wait(cb, di, &samplenum, &matched)`
- [ ] 等待完成后调用 `c_cond_free(cb)`
- [ ] 检查返回值 `ret != SRD_OK` 时return

### CMakeLists.txt
- [ ] 在C_DECODERS列表中添加了解码器名称（不含_c后缀）

---

## 1. ac97_c.c 专项检查

### 通道
- [ ] SYNC (索引0, type=SRD_CHANNEL_SCLK)
- [ ] BIT_CLK (索引1, type=SRD_CHANNEL_COMMON)
- [ ] SDATA_OUT (索引2, 可选, type=SRD_CHANNEL_SDATA)
- [ ] SDATA_IN (索引3, 可选, type=SRD_CHANNEL_SDATA)
- [ ] RESET# (索引4, 可选, type=SRD_CHANNEL_COMMON)

### 注解
- [ ] 32个注解标签全部定义
- [ ] 8个注解行正确（bits-out, slots-out-raw, slots-out, bits-in, slots-in-raw, slots-in, warnings, errors）
- [ ] 4个二进制输出定义

### 逻辑
- [ ] start()中检查SDATA_OUT和SDATA_IN至少有一个
- [ ] metadata()中保存samplerate
- [ ] SYNC帧起始检测：prev_sync[0]==0 && prev_sync[1]==1
- [ ] BIT_CLK时序：下降沿采样→上升沿检测SYNC
- [ ] frame_ss_list正确维护（每帧开始重置，每个位追加es）
- [ ] slot边界检测正确（frame_slot_lens表）
- [ ] handle_slot_00: READY(1位)+VALID(12位)+RSV(1位)+CODEC(2位)
- [ ] handle_slot_01: R/W(1位)+ADDR(7位)+REQ/RSV(10+2位)
- [ ] handle_slot_02: DATA(16位)+RSV(4位)
- [ ] handle_slot_dummy: 仅处理have_slots标记有效的slot
- [ ] bits_to_int: MSB优先位序列转整数
- [ ] bits_to_bin_ann: 8位一组转字节数组
- [ ] int_to_nibble_text: 位数→十六进制字符串
- [ ] get_bit_field: 从整数中提取位域
- [ ] flush_frame_bits: 帧结束时输出二进制数据
- [ ] destroy()中释放frame_ss_list内存

---

## 2. sdcard_sd_c.c 专项检查

### 通道
- [ ] CMD (索引0, type=SRD_CHANNEL_SDATA)
- [ ] CLK (索引1, type=SRD_CHANNEL_SCLK)
- [ ] DAT0-3 (索引2-5, 可选, type=SRD_CHANNEL_ADATA)

### 注解
- [ ] 217个注解标签全部定义
- [ ] 5个注解行正确
- [ ] CMD0-63 (索引0-63)
- [ ] ACMD0-63 (索引64-127)
- [ ] RESPONSE_R1/R1B/R2/R3/R6/R7 (索引128-133)
- [ ] R_STATUS_* (索引134-163, 30个)
- [ ] R_CID_* (索引164-172, 9个)
- [ ] R_CSD_* (索引173-206, 34个)
- [ ] BIT_0/BIT_1 (索引207-208)
- [ ] F_START/TRANSMISSION/CMD/ARG/CRC/END (索引209-214)
- [ ] DECODED_BIT/DECODED_F (索引215-216)

### 查找表
- [ ] cmd_names表（64条目，来自common/sdcard/mod.py）
- [ ] acmd_names表（64条目，来自common/sdcard/mod.py）
- [ ] accepted_voltages表（4条目）

### 逻辑
- [ ] CLK上升沿等待
- [ ] 起始位检测（CMD=低，仅在token为空时）
- [ ] CMD55设置is_acmd标志
- [ ] ACMD处理完后清除is_acmd（非CMD55/63）
- [ ] 传输位检查：响应中传输位=1时重新作为命令处理
- [ ] token最后bit的es计算：`token[n-1].es = token[n-1].ss + (token[n-1].ss - token[n-2].ss)`
- [ ] puta()位序反转正确
- [ ] R2响应136位token
- [ ] handle_reg_status: 30个状态位注解
- [ ] handle_reg_cid: 9个CID字段注解
- [ ] handle_reg_csd: 34个CSD字段注解
- [ ] R7响应中accepted_voltages查找

---

## 3. emmc_sd_c.c 专项检查

### 通道
- [ ] CMD (索引0, type=SRD_CHANNEL_SDATA)
- [ ] CLK (索引1, type=SRD_CHANNEL_SCLK)

### 注解
- [ ] 73个注解标签全部定义
- [ ] 5个注解行正确

### 查找表
- [ ] cmd_names表（63条目，来自emmc_sd/mod.py）
- [ ] device_status表（32条目，来自emmc_sd/mod.py）

### 逻辑
- [ ] 无ACMD处理
- [ ] CMD23条件参数解析（bit30判断Packed/Normal模式）
- [ ] CMD38多个标志位解析
- [ ] CMD44多个字段解析
- [ ] R4响应39位token
- [ ] R5响应40位token
- [ ] Python代码中硬编码注解索引128-136的修正（使用正确的枚举值71-72）
- [ ] handle_response_r1中device_status表查找

---

## 4. swim_c.c 专项检查

### 通道
- [ ] SWIM (索引0, type=SRD_CHANNEL_COMMON)

### 选项
- [ ] debug选项（yes/no, 默认no）
- [ ] srd_c_decoder_entry()中正确构建选项值列表

### 注解
- [ ] 16个注解标签全部定义
- [ ] 4个注解行正确
- [ ] 2个二进制输出定义

### 逻辑
- [ ] metadata()中保存samplerate
- [ ] start()中验证samplerate非零
- [ ] start()中计算时序参数（sync_reflen_min/max, eseq_reflen, bit_reflen）
- [ ] adjust_timings(): bit_reflen = ceil(samplerate * 22 / swim_clock)
- [ ] 位解码：高电平≥低电平→0, 高电平<低电平→1
- [ ] 同步帧检测：低电平持续64-128个SWIM时钟
- [ ] 同步帧后重计算swim_clock
- [ ] 进入序列检测：4+4脉冲模式匹配
- [ ] bitseq函数：CMD状态4+1+1位，其他状态8+1+1位
- [ ] parity计算包含start位，最终bitseq_value &= 0xff
- [ ] 仅ACK时调用protocol()
- [ ] protocol()状态机：CMD→N→@E→@H→@L→D
- [ ] binary输出：ACK时输出数据字节
- [ ] debug选项控制debug注解输出

---

## 5. rvswd_c.c 专项检查

### 通道
- [ ] CLK (索引0, type=SRD_CHANNEL_SCLK)
- [ ] DIO (索引1, type=SRD_CHANNEL_SDATA)

### 注解
- [ ] 11个注解标签全部定义
- [ ] 2个注解行正确
- [ ] 格式化注解名称在运行时用snprintf生成

### 逻辑
- [ ] START条件检测：CLK=高, DIO=下降沿
- [ ] STOP条件检测：CLK=高, DIO=上升沿
- [ ] 位采样：CLK上升沿push DIO值, CLK下降沿terminate位
- [ ] 短包检测：52位
- [ ] 长包检测：84位
- [ ] 无效包长度处理
- [ ] process_short_packet: address-host(0-6), operation(7), parity-host(8), data-target(14-45), parity-target(46)
- [ ] process_long_packet: address-host(0-6), data-host(7-38), operation(39-40), parity-host(41), address-target(42-48), data-target(49-80), status(81-82), parity-target(83)
- [ ] put_annotation_bits: 位范围→整数(MSB优先)→格式化注解
- [ ] bit注解格式：`"BIT %d: %d"` 和 `"%d"`

---

## 编译与集成检查

- [ ] CMakeLists.txt中C_DECODERS列表已更新
- [ ] `build_incremental.cmd` 编译无错误
- [ ] 编译无警告（-Wall -Wextra）
- [ ] DLL文件生成在 `build.dir/decoders/c_decoders/`
- [ ] PXView能正确加载新解码器
- [ ] 解码器出现在UI的解码器列表中
- [ ] 通道配置正确显示
- [ ] 与Python解码器在相同输入下注解输出一致
