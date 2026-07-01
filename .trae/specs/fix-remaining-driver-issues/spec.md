# 修复剩余驱动问题 Spec

## Why

tiered-driver-compat-fix 完成后，三个驱动遗留问题阻塞完整构建或丢失功能：
- **serial-dmm**：24 个文件存在但编译失败（bm52x.c/eev121gw.c/bm85x.c 使用上游 0.6.0 API）
- **yokogawa-dlm**：`config_channel_set` 功能丢失，硬件通道不跟随切换
- **hantek-dso**：电压精度位数 `digits` 信息丢失（PXView 扁平结构无字段）

## What Changes

### serial-dmm 迁移（3 个实质修改 + 21 个警告修复）
- bm52x.c：删除 `sr_analog_init` + `sr_analog_encoding/meaning/spec`，展平为 PXView 扁平 analog 赋值；本地实现 `read_u8`/`read_u24le`/`read_u24le_inc`；修正 `enum sr_mq *` 指针类型
- eev121gw.c：定义 `R8` 宏替换 9 处调用；整理 include 顺序
- bm85x.c：本地实现 `sr_atod_ascii_digits`
- 21 个文件：`#define LOG_PREFIX` 前补 `#undef LOG_PREFIX`
- protocol.h：删除 `SR_PACKET_*` 死代码回退（9 行）
- **BREAKING**：无（serial-dmm 当前 OFF，修复后可开启）

### yokogawa-dlm config_channel_set 修复
- api.c `yokogawa_dlm_compat_config_set`：`(void)ch;` 改为调用 `dlm_channel_state_set(sdi, ch->index, ch->enabled)`
- 参照 `sipeed-slogic-analyzer/api.c:677-684` 已验证模式

### hantek-dso digits 精度修复（不改前端）
- **libsigrok.h**：`struct sr_datafeed_analog` 末尾追加 `int8_t digits; int8_t spec_digits;`（向后兼容，旧代码 memset=0 不受影响）
- **hantek-dso/api.c** `send_chunk()`：补回 digits 计算 `digits = -(int)log10f(range/255) + (vdivlog<0.0)` + `analog.digits = digits;`
- **不改前端** `AnalogSnapshot`（digits 字段暂不消费，为未来扩展预留接口）

## Impact

- Affected specs: tiered-driver-compat-fix（serial-dmm 从"不在范围"移入完成）、audit-and-fix-migrated-drivers（Task 2/3 可关闭）
- Affected code:
  - `libsigrok/hardware/serial-dmm/`（24 个文件）
  - `libsigrok/hardware/yokogawa-dlm/api.c`（1 个函数）
  - `libsigrok/libsigrok.h`（sr_datafeed_analog 结构体追加 2 字段）
  - `libsigrok/hardware/hantek-dso/api.c`（send_chunk 函数）
- CMakeCache：`ENABLE_DRIVER_SERIAL_DMM` 从 OFF 改为 ON

## ADDED Requirements

### Requirement: serial-dmm 完整迁移

serial-dmm 驱动 SHALL 在启用 `ENABLE_DRIVER_SERIAL_DMM=ON` 时通过编译并正确链接。

#### Scenario: serial-dmm 编译通过
- **WHEN** `ENABLE_DRIVER_SERIAL_DMM=ON` 且执行 `ninja -j 16`
- **THEN** 24 个 .obj 全部生成，无 error，无 LOG_PREFIX 重定义 warning
- **AND** PXView.exe 链接成功

#### Scenario: bm52x.c 扁平化
- **WHEN** 编译 bm52x.c
- **THEN** 无 `sr_analog_init`/`sr_analog_encoding/meaning/spec` 引用
- **AND** analog 数据通过 `analog.mq/unit/mqflags/data/probes` 扁平赋值
- **AND** `read_u8`/`read_u24le`/`read_u24le_inc` 有本地实现

#### Scenario: eev121gw.c R8 宏
- **WHEN** 编译 eev121gw.c
- **THEN** `R8` 宏已定义，9 处调用正常编译

#### Scenario: bm85x.c sr_atod_ascii_digits
- **WHEN** 编译 bm85x.c
- **THEN** `sr_atod_ascii_digits` 有本地实现，返回 SR_OK 且解析 double + int digits

### Requirement: yokogawa-dlm 硬件通道切换

yokogawa-dlm 驱动 SHALL 在用户启用/禁用通道时向硬件发送 SCPI 命令。

#### Scenario: 通道切换触发 SCPI 命令
- **WHEN** 上层调用 `config_set` 且 `ch != NULL`
- **THEN** `dlm_channel_state_set(sdi, ch->index, ch->enabled)` 被调用
- **AND** SCPI 命令发送到示波器（模拟通道 `:CHAN<n>:DISP ON/OFF`，数字通道 pod 自动管理）

### Requirement: hantek-dso 电压精度位数传递

hantek-dso 驱动 SHALL 计算 8-bit ADC 的有效十进制位数并写入 `analog.digits` 字段。

#### Scenario: digits 计算并写入
- **WHEN** `send_chunk()` 发送模拟量数据
- **THEN** 计算 `digits = -(int)log10f(range/255) + (vdivlog<0.0)`
- **AND** `analog.digits = digits` 且 `analog.spec_digits = digits`
- **AND** 前端 `AnalogSnapshot` **不修改**（digits 暂不消费，为未来扩展预留）

## MODIFIED Requirements

### Requirement: sr_datafeed_analog 结构体

PXView 的 `struct sr_datafeed_analog` SHALL 在末尾追加 `int8_t digits` 和 `int8_t spec_digits` 字段，用于传递模拟量精度信息。

**向后兼容**：旧代码 memset 后字段为 0，不影响现有行为。新字段由 hantek-dso 写入，前端暂不消费。
