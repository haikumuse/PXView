# Tasks

> 进度图例：[ ] 待执行 / [x] 已完成

## Task 1: hantek-dso digits 精度修复（最小改动，先做）

> 不改前端，只加结构体字段 + 驱动计算。为其他任务做结构体准备。

- [x] Task 1: hantek-dso digits 精度修复
  - [x] 1.1 `libsigrok/libsigrok.h` 的 `struct sr_datafeed_analog` 末尾追加 `int8_t digits;` 和 `int8_t spec_digits;` 两个字段（含注释）
  - [x] 1.2 `libsigrok/hardware/hantek-dso/api.c` 的 `send_chunk()` 函数补回 digits 计算（math.h 已包含，range 来自 vdivs 表）
  - [x] 1.3 编译验证：`ninja -j 16` 无新 error/warning，PXView.exe 链接成功
  - [x] 1.4 确认前端 `PXView/pv/data/analogsnapshot.cpp` **未修改**

## Task 2: yokogawa-dlm config_channel_set 修复

> 参照 sipeed-slogic-analyzer 已验证模式，改 1 个函数。

- [x] Task 2: yokogawa-dlm config_channel_set 修复
  - [x] 2.1 读取 `libsigrok/hardware/yokogawa-dlm/api.c` 第 704-710 行 `yokogawa_dlm_compat_config_set` 函数，确认当前 `(void)ch;` 丢弃逻辑
  - [x] 2.2 读取 `sipeed-slogic-analyzer/api.c:677-684` 确认参照模式（ch != NULL 时调用通道状态设置）
  - [x] 2.3 修改 `yokogawa_dlm_compat_config_set`：`ch != NULL` 时调用 `dlm_channel_state_set(sdi, ch->index, ch->enabled)`，然后继续调用原 `config_set`
  - [x] 2.4 编译验证：`ninja -j 16` 无 error，PXView.exe 链接成功

## Task 3: serial-dmm 实质错误修复（3 个文件）

> 这 3 个文件有编译错误，需逐个修复。可并行。

- [x] Task 3: serial-dmm 实质错误修复
  - [x] 3.1 bm52x.c 修复（最复杂）
    - [x] 3.1.1 删除 `struct sr_analog_encoding encoding1/encoding2; struct sr_analog_meaning meaning1/meaning2; struct sr_analog_spec spec1/spec2;`
    - [x] 3.1.2 删除 `enum sr_configkey key;` 改为 `uint32_t key;`（匹配 sr_session_send_meta 签名）
    - [x] 3.1.3 删除两次 `sr_analog_init()` 调用，改为 memset 扁平初始化（mqflags 字段无 digits，跳过）
    - [x] 3.1.4 在文件顶部添加本地 `read_u8`/`read_u24le`/`read_u24le_inc` 辅助函数
    - [x] 3.1.5 修正 enum 指针类型：mq→`int *`、unit→`int *`、mqflags→`uint64_t *`（匹配扁平结构体字段类型）
    - [x] 3.1.6 `#define LOG_PREFIX "brymen-bm52x"` 前补 `#undef LOG_PREFIX`
    - [x] 3.1.7 编译验证 bm52x.c 通过
  - [x] 3.2 eev121gw.c 修复
    - [x] 3.2.1 在文件顶部添加 `#define R8(p)` 宏定义（带 #ifndef 守卫）
    - [x] 3.2.2 `#define LOG_PREFIX "eev121gw"` 前补 `#undef LOG_PREFIX`
    - [x] 3.2.3 编译验证 eev121gw.c 通过
  - [x] 3.3 bm85x.c 修复
    - [x] 3.3.1 在文件顶部实现 `local_sr_atod_ascii_digits`（g_ascii_strtod + 计数有效数字）
    - [x] 3.3.2 将 `sr_atod_ascii_digits(...)` 调用替换为 `local_sr_atod_ascii_digits(...)`
    - [x] 3.3.3 `#define LOG_PREFIX "brymen-bm85x"` 前补 `#undef LOG_PREFIX`
    - [x] 3.3.4 编译验证 bm85x.c 通过

## Task 4: serial-dmm LOG_PREFIX 警告修复（21 个文件）

> 机械操作：每个文件 `#define LOG_PREFIX` 前补 `#undef LOG_PREFIX`。可并行。

- [x] Task 4: serial-dmm LOG_PREFIX 警告修复
  - [x] 4.1 protocol.h（第 28 行）
  - [x] 4.2 asycii.c（第 41 行）
  - [x] 4.3 bm25x.c（第 29 行）
  - [x] 4.4 bm86x.c（第 36 行）
  - [x] 4.5 dtm0660.c（第 38 行）
  - [x] 4.6 es519xx.c（第 33 行）
  - [x] 4.7 fs9721.c（第 39 行）
  - [x] 4.8 fs9922.c（第 30 行）
  - [x] 4.9 m2110.c（第 33 行）
  - [x] 4.10 metex14.c（第 38 行）
  - [x] 4.11 mm38xr.c（第 50 行）
  - [x] 4.12 ms2115b.c（第 95 行）
  - [x] 4.13 ms8250d.c（第 38 行）
  - [x] 4.14 qm1578.c（第 47 行）
  - [x] 4.15 rs9lcd.c（第 37 行）
  - [x] 4.16 ut71x.c（第 32 行）
  - [x] 4.17 vc870.c（第 26 行）
  - [x] 4.18 vc96.c（第 36 行）

## Task 5: serial-dmm protocol.h 死代码清理

> 独立，可并行。

- [x] Task 5: serial-dmm protocol.h 死代码清理
  - [x] 5.1 删除 `protocol.h` 中 `SR_PACKET_NEED_RX=0`/`SR_PACKET_VALID=1`/`SR_PACKET_INVALID=2` 回退定义（16 行死代码块，含注释，compat_config.h 已定义规范值）

## Task 6: serial-dmm 启用 + 全量编译验证

> 依赖 Task 1-5 全部完成。

- [x] Task 6: serial-dmm 启用 + 全量编译验证
  - [x] 6.1 修改 `build/CMakeCache.txt`：`ENABLE_DRIVER_SERIAL_DMM:BOOL=ON`
  - [x] 6.2 运行 `cmake .` 重新配置（Configuring done + Generating done）
  - [x] 6.3 运行 `ninja -j 16`，22 个 .obj 全部生成且 PXView.exe 链接成功（exit=0）
  - [x] 6.4 运行 `ninja install`，install.dir/bin/PXView.exe 更新（255MB，exit=0）
  - [x] 6.5 **spec gap 修复**：api.c 的 dmm_table 无条件引用 `*_packet_request`/`*_after_open`，但这些函数仅在 `HAVE_SERIAL_COMM`（依赖 libserialport，Windows 不可用）下编译。添加 `SERIAL_REQ(fn)`/`SERIAL_OPEN(fn)` 宏，无串口支持时传 NULL（protocol.c 与 api.c 已 NULL-check），dmm_table 编译通过

## Task 7: 回归验证

> 确认三个修复不影响已通过编译的驱动。

- [x] Task 7: 回归验证
  - [x] 7.1 确认 norma-dmm 仍编译通过（ENABLE_DRIVER_NORMA_DMM=ON，全量 ninja 通过）
  - [x] 7.2 确认 hantek-dso 仍编译通过（digits 字段追加不破坏现有代码）
  - [x] 7.3 确认 yokogawa-dlm 仍编译通过（config_set 修改不破坏现有逻辑）
  - [x] 7.4 确认 libsigrok.h 结构体追加字段不破坏其他已编译驱动（memset=0 兼容，全量 ninja 通过）

# Task Dependencies

- [Task 1] 独立，可先行（hantek-dso digits）
- [Task 2] 独立，可并行（yokogawa-dlm config_channel_set）
- [Task 3] 独立，可并行（serial-dmm 实质错误，3 个子任务内部可并行）
- [Task 4] 依赖 [Task 3]（LOG_PREFIX 修复可与 Task 3 并行，但建议 Task 3 先完成以减少编译错误噪音）
- [Task 5] 独立，可并行
- [Task 6] depends on [Task 1, 2, 3, 4, 5] — 全量编译验证
- [Task 7] depends on [Task 6]

# 并行策略

- **第一批**（无依赖，全部并行）：Task 1 + Task 2 + Task 3 + Task 4 + Task 5
- **第二批**（串行）：Task 6（全量编译验证）
- **第三批**（串行）：Task 7（回归验证）
