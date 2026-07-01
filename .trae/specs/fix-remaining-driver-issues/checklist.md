# Checklist — 修复剩余驱动问题

> 图例：[ ] 待验证 / [x] 已验证完成

## hantek-dso digits 精度修复

- [x] `libsigrok/libsigrok.h` 的 `struct sr_datafeed_analog` 末尾含 `int8_t digits;` 和 `int8_t spec_digits;` 字段
- [x] `libsigrok/hardware/hantek-dso/api.c` 的 `send_chunk()` 含 digits 计算逻辑（`log10f(range/255)` + `analog.digits = digits`）
- [x] hantek-dso 编译通过，无新 error/warning（全量 ninja 通过）
- [x] `PXView/pv/data/analogsnapshot.cpp` **未被修改**（git diff 无输出）

## yokogawa-dlm config_channel_set 修复

- [x] `libsigrok/hardware/yokogawa-dlm/api.c` 的 `yokogawa_dlm_compat_config_set` 中 `ch != NULL` 时调用 `dlm_channel_state_set(sdi, ch->index, ch->enabled)`
- [x] 不再 `(void)ch;` 丢弃 ch 参数
- [x] yokogawa-dlm 编译通过（全量 ninja 通过）
- [x] `dlm_channel_state_set` 函数已在 protocol.c 中存在（签名 `(const struct sr_dev_inst *sdi, const int ch_index, gboolean state)`，非死代码调用）

## serial-dmm 实质错误修复

### bm52x.c
- [x] 无 `sr_analog_init` 调用
- [x] 无 `sr_analog_encoding`/`sr_analog_meaning`/`sr_analog_spec` 结构体变量声明
- [x] 有本地 `read_u8`/`read_u24le`/`read_u24le_inc` 辅助函数定义
- [x] `enum sr_mq *`/`enum sr_unit *`/`enum sr_mqflag *` 指针类型已修正（mq→`int *`、unit→`int *`、mqflags→`uint64_t *`，匹配扁平结构体字段）
- [x] `#undef LOG_PREFIX` 在 `#define LOG_PREFIX "brymen-bm52x"` 之前
- [x] bm52x.c 编译通过（无 error，仅一个 enum-conversion 预存 warning）

### eev121gw.c
- [x] 有 `#define R8(p)` 宏定义（带 #ifndef 守卫）
- [x] `#undef LOG_PREFIX` 在 `#define LOG_PREFIX "eev121gw"` 之前
- [x] eev121gw.c 编译通过（无 error）

### bm85x.c
- [x] 有本地 `local_sr_atod_ascii_digits` 实现（g_ascii_strtod + 计数有效数字）
- [x] 调用已替换为 `local_sr_atod_ascii_digits`
- [x] `#undef LOG_PREFIX` 在 `#define LOG_PREFIX "brymen-bm85x"` 之前
- [x] bm85x.c 编译通过（无 error）

### api.c（spec gap 修复，Task 6.5 发现）
- [x] `SERIAL_REQ(fn)`/`SERIAL_OPEN(fn)` 宏定义，无 `HAVE_SERIAL_COMM` 时传 NULL
- [x] dmm_table 中所有 `*_packet_request`/`*_after_open` 引用已用宏包装（bm52x/bm82x/bm85x/bm86x/metex14）
- [x] api.c 编译通过（无 undeclared 错误）

## serial-dmm LOG_PREFIX 警告修复

- [x] 18 个文件（protocol.h + 17 个 .c）均在 `#define LOG_PREFIX` 前有 `#undef LOG_PREFIX`
- [x] 编译无 `LOG_PREFIX redefined` warning（serial-dmm 文件均干净编译）

## serial-dmm protocol.h 死代码清理

- [x] `protocol.h` 中 `SR_PACKET_NEED_RX=0`/`SR_PACKET_VALID=1`/`SR_PACKET_INVALID=2` 回退定义已删除（16 行块）
- [x] `compat_config.h` 的规范值（NEED_RX=1/VALID=0/INVALID=-1）不受影响

## serial-dmm 启用 + 全量编译

- [x] `build/CMakeCache.txt` 中 `ENABLE_DRIVER_SERIAL_DMM:BOOL=ON`
- [x] `ninja -j 16` exit=0
- [x] serial-dmm 的 22 个 .obj 全部生成（api.c + 21 个 parser/protocol）
- [x] PXView.exe 链接成功（255MB）
- [x] `ninja install` exit=0

## 回归验证

- [x] norma-dmm 仍编译通过（ENABLE_DRIVER_NORMA_DMM=ON，全量 ninja 通过）
- [x] hantek-dso 仍编译通过（digits 字段追加不破坏现有代码）
- [x] yokogawa-dlm 仍编译通过
- [x] 其他已启用驱动（fx2lafw/rigol-ds/siglent-sds/lecroy-xstream/uni-t-ut181a 等）未因 `sr_datafeed_analog` 结构体追加字段而破坏（memset=0 兼容，全量 ninja 通过）
- [x] 全量 `ninja -j 16` + `ninja install` exit=0，PXView.exe 生成（255834928 字节）
