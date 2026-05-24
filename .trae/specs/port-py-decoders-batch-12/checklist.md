# 验证清单 — Batch 12: sda2506, signature, sony_md, st7735, st7789

## 通用验证项（每个解码器都必须检查）

### 文件结构
- [ ] 源文件位于 `libsigrokdecode/c_decoders/{id}_c.c`
- [ ] 文件名中 `-` 已替换为 `_`
- [ ] 包含头文件：`<stdio.h>`, `<stdlib.h>`, `<string.h>`, `<glib.h>`, `"libsigrokdecode.h"`

### struct srd_c_decoder 规范
- [ ] `.id` 格式为 `"xxx_c"`（带 `_c` 后缀）
- [ ] `.name` 格式为 `"XXX(C)"`（原名大写 + `(C)` 后缀）
- [ ] `.longname` 与 Python 版一致
- [ ] `.desc` 与 Python 版一致，末尾可加 `(C implementation)`
- [ ] `.license` 与 Python 版一致
- [ ] `.channels` 数组与 Python 版通道定义完全对应
- [ ] `.num_channels` 正确
- [ ] `.optional_channels` 正确（无则为 NULL，num 为 0）
- [ ] `.options` 正确初始化（无则为 NULL，num 为 0）
- [ ] `.num_annotations` 与 ann_labels 数组长度一致
- [ ] `.ann_labels` 每行第一列为 `""`（空字符串）
- [ ] `.annotation_rows` 包含所有 annotation class
- [ ] `.inputs` 包含 `"logic"`
- [ ] `.outputs` 与 Python 版一致
- [ ] `.tags` 与 Python 版一致
- [ ] `.binary` 正确（无则为 NULL，num 为 0）

### 回调函数
- [ ] `reset` 回调：使用 `g_malloc0` 分配私有数据，`memset` 清零
- [ ] `start` 回调：注册输出（至少 OUTPUT_ANN），读取 options
- [ ] `decode` 回调：实现完整状态机
- [ ] `destroy` 回调：`g_free` 释放私有数据，设置 NULL
- [ ] `metadata` 回调（如需 samplerate）：处理 `SRD_CONF_SAMPLERATE`

### samplerate 守卫
- [ ] metadata 回调中保存 samplerate
- [ ] decode 入口处检查 samplerate，为 0 时尝试 `c_decoder_get_samplerate()`
- [ ] samplerate 仍为 0 时直接 return

### Condition Builder 用法
- [ ] 每次 `c_cond_new()` 后都有对应的 `c_cond_free()`
- [ ] `c_cond_wait()` 返回值检查（`ret != SRD_OK` 时 return）
- [ ] `c_cond_or()` 正确用于组合多个条件
- [ ] `c_cond_skip()` 参数类型为 `uint64_t`

### Annotation 输出
- [ ] 使用 `C_ANN_PUT` 宏输出注解
- [ ] 每个 annotation class 都有对应的输出调用
- [ ] 所有 annotation class 都映射到某个 annotation_row

### 导出函数
- [ ] `srd_c_decoder_entry()` 返回 `&xxx_c_decoder` 指针
- [ ] `srd_c_decoder_api_version()` 返回 `SRD_C_DECODER_API_VERSION`
- [ ] 两个函数都有 `SRD_C_DECODER_EXPORT` 前缀
- [ ] Options 在 `srd_c_decoder_entry()` 中初始化

### CMakeLists.txt
- [ ] decoder id（不含 `_c`）已添加到 `C_DECODERS` 列表

---

## SDA2506 专项验证

- [ ] 3 个通道定义正确（CLK=0, DATA=1, CE#=2），idn 正确
- [ ] 5 个 annotation labels 定义正确
- [ ] 4 个 annotation rows 正确覆盖所有 5 个 class
- [ ] cmdbits 头部插入逻辑正确（新 bit 在 index 0，旧 bit 后移）
- [ ] cmdbits 最多保留 24 个
- [ ] databits 头部插入逻辑正确
- [ ] 25μs 等待使用 `c_cond_skip` 实现
- [ ] CE 下降沿时正确解析 addr（7 bit）、CB（1 bit）
- [ ] Read 命令：CB=0，解析 read 字段
- [ ] Write 命令：CB=1 且 d=0，解析 data 字段（8 bit），等待 CE 上升沿
- [ ] Erase 命令：CB=1 且 d=1，等待 CE 上升沿
- [ ] 异常时 reset（不崩溃）
- [ ] 无 options

## Signature 专项验证

- [ ] 4 个通道定义正确（START=0, STOP=1, CLOCK=2, DATA=3），idn 正确
- [ ] 5 个 annotation labels 定义正确
- [ ] 2 个 annotation rows 正确覆盖所有 5 个 class
- [ ] 4 个 options 正确初始化（start_edge, stop_edge, clk_edge, annbits）
- [ ] 每个 option 的 idn 正确
- [ ] LFSR 反馈计算正确：`popcount(shiftreg & 0x0291) + data` 的最低位
- [ ] popcount 实现可移植（不依赖 `__builtin_popcount`）
- [ ] symbol_map 查表正确（16 个字符映射）
- [ ] 签名输出格式：4 个字符拼接
- [ ] START 边沿极性由 option 控制
- [ ] STOP 边沿极性由 option 控制
- [ ] CLOCK 边沿选择由 option 控制（rising/falling）
- [ ] 门控逻辑正确：START 开门，STOP 关门
- [ ] annbits=yes 时输出位级注解
- [ ] annbits=no 时不输出位级注解

## Sony MD Remote 专项验证

- [ ] 1 个通道定义正确（data=0）
- [ ] 8 个 annotation labels 定义正确
- [ ] 5 个 annotation rows 正确覆盖所有 8 个 class
- [ ] 1 个 option（marginpct）正确初始化，idn 正确
- [ ] 5 个状态正确定义（IDLE, PRESYNC, SYNC, DATA-BIT-HIGH, DATA-BIT-LOW）
- [ ] 所有时序参数在 start() 中正确计算
- [ ] margin 百分比正确应用到各时序范围
- [ ] Reset 脉冲检测：40ms ± margin
- [ ] Presync 脉冲检测：1100μs ± margin
- [ ] Presync Delay 检测：800μs~1500μs
- [ ] Sync 脉冲检测：20μs~presyncCycles×(1+margin)
- [ ] 数据位 0：长脉冲（101μs~280μs）
- [ ] 数据位 1：短脉冲（10μs~100μs）
- [ ] 第 5 bit：Remote 有/无数据
- [ ] 第 9 bit：Player 有/无数据
- [ ] 第 13 bit：Player 是否让出总线
- [ ] 消息长度逻辑正确：默认 16 bit，可变 104/115 bit
- [ ] Python 输出格式正确
- [ ] 错误注解正确输出

## ST7735 专项验证

- [ ] 4 个通道定义正确（CS#=0, CLK=1, MOSI=2, DC=3），idn 正确
- [ ] 4 个 annotation labels 定义正确
- [ ] 3 个 annotation rows 正确覆盖所有 4 个 class
- [ ] 命令查找表包含所有 Python META 中的命令（约 30 个）
- [ ] CS 高电平时 reset 状态
- [ ] CLK 上升沿采样 MOSI
- [ ] CLK 下降沿处理 bit
- [ ] MSB-first 字节累积正确
- [ ] DC=0 → Command, DC=1 → Data
- [ ] 新 Command 时输出上一个 Command 的 description
- [ ] 未知命令输出 "Unknown command: XX. Data: ..."
- [ ] Data 累积最多 128 字节
- [ ] 无 options

## ST7789 专项验证

- [ ] 4 个通道定义正确（CSX=0, DCX=1, SDO=2, WRX=3），idn 正确
- [ ] 5 个 annotation labels 定义正确
- [ ] 4 个 annotation rows 正确覆盖所有 5 个 class
- [ ] 命令查找表包含所有 Python COMMAND_MAP 中的命令（约 60 个）
- [ ] 外层循环等待 CSX 下降沿
- [ ] 内层循环等待 CSX 上升沿或 DCX 边沿
- [ ] CSX=1 时输出 "Asserted" 注解
- [ ] CSX=1 时输出上一个 cmd_data 组合注解
- [ ] DCX=1 + bit 未设置：采样 SDO 位
- [ ] DCX=0 + bit 已设置：完成一个 bit，输出 bit 注解
- [ ] 字节完成：WRX=1 → Data, WRX=0 → Command
- [ ] 新 Command 时输出上一个 cmd_data 组合
- [ ] cmd_data 格式："CMD_NAME(XX): DD DD ..."
- [ ] 无 options

---

## 构建验证

- [ ] `build_incremental.cmd` 编译无错误
- [ ] `build.dir/decoders/c_decoders/sda2506_c.dll` 生成
- [ ] `build.dir/decoders/c_decoders/signature_c.dll` 生成
- [ ] `build.dir/decoders/c_decoders/sony_md_c.dll` 生成
- [ ] `build.dir/decoders/c_decoders/st7735_c.dll` 生成
- [ ] `build.dir/decoders/c_decoders/st7789_c.dll` 生成

## 运行时验证

- [ ] PXView 启动无崩溃
- [ ] 各 C 解码器出现在解码器列表中
- [ ] 选择解码器后通道配置正确显示
- [ ] Options（如有）正确显示和保存
- [ ] 加载对应信号文件后解码器输出注解
