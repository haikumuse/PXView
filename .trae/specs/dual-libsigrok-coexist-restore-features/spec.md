# 双 libsigrok 共存以最大化还原标准驱动功能 Spec

## Why

当前 PXView libsigrok 是 2014 年从上游 0.2.0 fork 的深度分叉版本,与上游 `sr_dev_driver` 回调签名、`struct sr_dev_inst/channel/datafeed_packet` 字段布局、触发体系(`ds_trigger_*` vs `sr_trigger`)均已不兼容。为让 87 个标准 sigrok 驱动(slogic/fx2lafw/kingst-la2016 等)在 PXView 中编译运行,前期采用"兼容层 + 触发桥接 hack"(方案 C,见 `add-sigrok-driver-compat-layer`/`refactor-compat-restore-features`),但该方案存在三个根本性缺陷:

1. **签名翻译有损**:`config_get(id, data, sdi, ch, cg)` → `config_get(key, data, sdi, cg)` 直接丢弃 `ch` 参数,每个驱动需独立 hack 弥补,87 个驱动需 87 次独立测试
2. **触发体系永久割裂**:方案 C 的 `compat_trigger.c` 桥接层仅支持 single stage + 5 种 match,且 PXView `TriggerConfig` 与上游 `sr_trigger` 永远需双向同步,是永久技术债
3. **持续维护成本高**:每次上游 API 变化都要改桥接层,87 个驱动的 wrapper 各自维护

本 spec 实施**方案 B(双库共存)**:把上游 libsigrok 编译为独立静态库(`libsigrokstd.a`),所有公开符号 `sr_*` 重命名为 `srstd_*`,与 PXView libsigrok 物理隔离。两库共享同一 `libusb_context`,通过统一数据结构转换层(`srstd_bridge.c`)交换 `sr_dev_inst`/`sr_channel`/`sr_datafeed_packet`/`sr_trigger`。87 个标准驱动用上游原生 API 编译,触发/状态/多模式 100% 还原,**测试复杂度从 O(n) 降到 O(1)**——转换层测通一次,所有上游驱动可用。

## What Changes

### A. 上游 libsigrok 符号重命名(基础设施)

- 新建 `libsigrokstd/` 目录作为上游库的独立构建根,源码从 `c:\Users\admin\Downloads\libsigrok\` 拷入(只读副本,不修改源文件)
- 新建 `libsigrokstd/include/srstd_rename.h`:通过宏批量重命名上游公开符号
  - `#define sr_init srstd_init` / `#define sr_exit srstd_exit` 等所有 143 个 SR_API 函数
  - `#define sr_dev_driver srstd_dev_driver` / `#define sr_dev_inst srstd_dev_inst` 等所有 29 个公开 struct 标签
  - `#define sr_trigger srstd_trigger` / `#define sr_trigger_stage srstd_trigger_stage` / `#define sr_trigger_match srstd_trigger_match`
  - enum 与 typedef 同步重命名
- 新建 `libsigrokstd/include/srstd.h`:聚合头文件,`#include <libsigrok/libsigrok.h>` + `#include "srstd_rename.h"`
- **关键约束**:上游源文件**不修改**,仅在编译时通过 `-include srstd_rename.h` 强制注入宏;PXView libsigrok 的源文件**不 include** `srstd.h`,保持原符号

### B. 上游 libsigrok 独立 CMake 构建

- 新建 `libsigrokstd/CMakeLists.txt`:编译为静态库 `libsigrokstd.a`,只链接 glib/libusb/serial 等公共依赖
- 仅编译上游核心 `src/*.c`(backend/session/hwdriver/device/usb/serial/trigger/strutil/std/scpi/modbus)+ 必要硬件驱动(初期仅 sipeed-slogic-analyzer + fx2lafw 作为 PoC)
- 输出 `libsigrokstd.a` + `srstd.h` 供 PXView 主项目链接
- **构建隔离**:`libsigrokstd` 与 PXView `libsigrok` 完全独立的 target,无任何源文件交叉

### C. libusb_context 共享

- PXView `ds_lib_init()` 内部流程不变(创建 `ctx_A`)
- 新增 `srstd_init_shared(struct sr_context **ctx, libusb_context *shared_usb_ctx)` 包装函数:调用上游 `sr_init()` 创建 `ctx_B`,执行 `libusb_exit(ctx_B->libusb_ctx)` 释放上游创建的 context,然后 `ctx_B->libusb_ctx = shared_usb_ctx` 共享 PXView 的 context
- PXView 启动时:`ds_lib_init()` → `srstd_init_shared(&srstd_ctx, lib_ctx.sr_ctx->libusb_ctx)`
- PXView 退出时:先 `srstd_exit(srstd_ctx)`(内部 `libusb_exit(NULL)` 安全),再 `ds_lib_exit()`
- **VID/PID 协调**:PXView `ds_scan_all_device_list` 已用 `DS_VENDOR_ID=0x2A0E` 过滤;上游驱动 scan 时通过 `srstd_dev_driver` 的 `driver->context` 字段访问共享 libusb_context,自然只匹配各自 VID/PID

### D. 数据结构转换层(`srstd_bridge.c/h`)

- 新建 `libsigrokstd/bridge/srstd_bridge.h` + `srstd_bridge.c`:两库数据结构双向映射
- **`srstd_sdi_to_pxview()`**:上游 `srstd_dev_inst` → PXView `sr_dev_inst`
  - 拷贝 `vendor`/`model`/`version`/`serial_num`/`connection_id`/`inst_type`
  - PXView 专有字段(`handle`/`dev_type`/`mode`/`path`/`actived_times`)填默认值
  - `channels` GSList 经 `srstd_channel_to_pxview()` 转换
- **`srstd_channel_to_pxview()`**:上游 channel(5 字段)→ PXView channel(32 字段)
  - 拷贝 `index`/`type`/`enabled`/`name`
  - PXView 硬件字段(`vdiv`/`offset`/`coupling`/`cali_*`/`vga_ptr`)保持默认 0/NULL
- **`srstd_packet_to_pxview()`**:上游 `srstd_datafeed_packet` → PXView `sr_datafeed_packet`
  - `SR_DF_LOGIC`:上游 3 字段(length/unitsize/data)→ PXView 8 字段(补 `format=LA_CROSS_DATA`/`index=0`/`order`/`data_error=0`/`error_pattern=0`)
  - `SR_DF_ANALOG`:上游字段 → PXView 字段(补 `unit_bits`/`unit_pitch`/`digits`/`spec_digits`)
  - `SR_DF_FRAME_BEGIN/END/HEADER/META`:直接透传
- **`pxview_trigger_to_srstd()`**:PXView Core `TriggerConfig` → 上游 `srstd_trigger`
  - `TriggerConfig::Simple`(single stage)→ `srstd_trigger` 单 stage + per-channel `SR_TRIGGER_ZERO/ONE/RISING/FALLING/EDGE` matches
  - `TriggerConfig::Adv`(多 stage)→ `srstd_trigger` 多 stage(上游原生支持)
  - `TriggerConfig::Serial`→ `srstd_trigger` 单 stage + serial pattern match(上游 `SR_TRIGGER_LAST` 适配)

### E. 数据流回调桥接

- 上游驱动的 `srstd_session_datafeed_callback` 由 PXView 注册,接收 `srstd_datafeed_packet`
- 回调内部:`srstd_packet_to_pxview()` 转换后,转发给 PXView `ds_datafeed_callback`(已有)
- PXView Core 的 `DataFeedParser` 不变,继续接收 PXView 格式 packet

### F. 设备调度分流(`DeviceAgent` 改造)

- `DeviceAgent` 新增 `enum DeviceLib { LIB_PXVIEW, LIB_SRSTD }` 字段
- 设备扫描时合并两库结果:
  - PXView `ds_get_device_list()` 返回 DSL 原生设备
  - 上游 `srstd_driver_scan()` 返回标准驱动设备,经 `srstd_sdi_to_pxview()` 转换后合并到统一设备列表
- 配置/采集调用按 `DeviceLib` 分流:
  - `LIB_PXVIEW` → 调 `ds_get/set_actived_device_config`(现状)
  - `LIB_SRSTD` → 调 `srstd_config_get/set` + `srstd_dev_acquisition_start/stop`
- 触发同步:`SigSession::sync_trigger_to_libsigrok()` 增加分流
  - `LIB_PXVIEW` → `ds_trigger_*`(现状)
  - `LIB_SRSTD` → `pxview_trigger_to_srstd()` + `srstd_session_trigger_set()`

### G. slogic 驱动迁移(方案 C 替代)

- 从 PXView `libsigrok/hardware/sipeed-slogic-analyzer/` 删除(方案 C 实现)
- 改在 `libsigrokstd/src/hardware/sipeed-slogic-analyzer/` 用上游原生源码(`c:\Users\admin\Downloads\libsigrok-slogic-dev\src\hardware\sipeed-slogic-analyzer\` 直接拷入)
- 编译时 `#include <srstd.h>` 而非 `compat.h`,使用上游原生 API(4 参 config_get、`driver->context`、`SR_REGISTER_DEV_DRIVER` 等)
- CMake 注册到 `libsigrokstd` target

### H. 删除方案 C 桥接 hack(**BREAKING**)

- 删除 `libsigrok/hardware/compat/compat_trigger.h` + `compat_trigger.c`(方案 C 的触发桥接层)
- 删除 `libsigrok/hardware/compat/compat_helpers.h:399-401` 的 "trigger path is dead at runtime" 注释及 stub
- `libsigrok/hardware/compat/` 目录其余文件(签名包装/串口/SCPI)保留——其他 80 个兼容驱动仍依赖
- **后续工作(非本 spec)**:逐步把其余 80 个兼容驱动迁移到上游库,最终删除整个 compat 目录

## Impact

- **Affected specs**:
  - `add-sigrok-driver-compat-layer`(方案 C 基础)——保留,但 compat_trigger 部分被替代
  - `restructure-compat-and-migrate-slogic`(slogic 迁移)——slogic 实现被替代
  - `refactor-compat-restore-features`(方案 C 触发桥接)——被替代删除
  - `tiered-driver-compat-fix`(compat 层补全)——保留,仍服务于其他 80 个驱动
- **Affected code**:
  - 新增:`libsigrokstd/`(整个目录,独立构建根)
  - 新增:`PXView/pv/device/deviceagent.h/.cpp`(分流逻辑,或扩展现有 DeviceAgent)
  - 修改:`PXView/main.cpp`(启动时初始化 srstd_ctx)
  - 修改:`PXView/pv/sigsession.cpp`(`sync_trigger_to_libsigrok` 分流)
  - 修改:`PXView/pv/core/datafeedparser.cpp`(接收 srstd packet 转换)
  - 修改:`libsigrok/CMakeLists.txt`(删除 slogic 条目)
  - 修改:`libsigrok/hardware/hwdriver.c`(删除 slogic 注册)
  - 删除:`libsigrok/hardware/sipeed-slogic-analyzer/`(整个目录)
  - 删除:`libsigrok/hardware/compat/compat_trigger.h/.c`
- **Hard constraints preserved**:
  - PXView `pxlogic` 驱动签名不变(继续用 PXView 原生 5 参 API)
  - PXView Core(`SigSession`/`CaptureManager`/`DocumentRegistry`)无大改,仅 `DeviceAgent`/`DataFeedParser`/`sync_trigger` 三处增加分流
  - DSL 硬件触发(`ds_trigger_*`)对 pxlogic 完全保留
- **Risk**:
  - 数据结构转换层 bug(可通过单元测试覆盖 `srstd_sdi_to_pxview`/`srstd_packet_to_pxview` 全字段)
  - 符号重命名遗漏(可通过 `nm libsigrokstd.a | grep ' sr_'` 验证零残留)
  - libusb 共享析构顺序(已验证 `libusb_exit(NULL)` 安全)

## ADDED Requirements

### Requirement: 双库符号物理隔离

系统 SHALL 提供上游 libsigrok 的独立构建产物 `libsigrokstd.a`,所有公开符号以 `srstd_` 前缀导出,与 PXView libsigrok 的 `sr_*`/`ds_*` 符号零冲突。

#### Scenario: 符号零冲突验证
- **WHEN** 链接 PXView 可执行文件同时链接 `libsigrok.a` 与 `libsigrokstd.a`
- **THEN** 链接器零 "multiple definition" 错误
- **AND** `nm libsigrokstd.a | grep ' T sr_'` 返回空(所有公开符号已重命名为 `srstd_`)
- **AND** `nm libsigrokstd.a | grep ' T srstd_'` 返回 143+ 个符号

### Requirement: 共享 libusb_context

系统 SHALL 让两个 libsigrok 库共享同一 `libusb_context`,确保 USB 设备发现/热插拔/打开操作跨库一致。

#### Scenario: 共享 context 后 USB 设备可见
- **WHEN** PXView 调用 `ds_lib_init()` + `srstd_init_shared()` 完成初始化
- **AND** 用户插入 slogic 设备(VID 0x359f)
- **THEN** 上游 `srstd_driver_scan()` 能发现该设备(经共享 `libusb_ctx`)
- **AND** PXView `ds_scan_all_device_list()` 也能发现 pxlogic 设备(VID 0x2A0E)
- **AND** 两库不互相干扰(VID/PID 过滤)

#### Scenario: 析构顺序安全
- **WHEN** PXView 退出时调用 `srstd_exit()` 然后 `ds_lib_exit()`
- **THEN** `srstd_exit()` 内部 `libusb_exit(NULL)` 不崩溃(libusb 文档允许)
- **AND** `ds_lib_exit()` 正常释放共享 context

### Requirement: 数据结构转换层

系统 SHALL 提供统一数据结构转换层,实现两库 `sr_dev_inst`/`sr_channel`/`sr_datafeed_packet`/`sr_trigger` 双向映射,所有上游驱动通过该层与 PXView Core 交互。

#### Scenario: 上游 sdi 转换为 PXView sdi
- **WHEN** 上游驱动 scan 返回 `srstd_dev_inst`(含 vendor="Sipeed"/model="SLogic Combo 8"/channels=[8 个 logic channel])
- **THEN** `srstd_sdi_to_pxview()` 生成 PXView `sr_dev_inst`
- **AND** `vendor`/`model`/`version`/`channels` 字段正确拷贝
- **AND** PXView 专有字段(`handle`/`dev_type`/`mode`)填默认值
- **AND** 转换后的 sdi 可被 PXView `DeviceAgent` 识别

#### Scenario: 上游 logic packet 转换为 PXView packet
- **WHEN** 上游驱动发送 `SR_DF_LOGIC` packet(length=4096/unitsize=1/data=缓冲区指针)
- **THEN** `srstd_packet_to_pxview()` 生成 PXView `sr_datafeed_logic`
- **AND** `format=LA_CROSS_DATA`/`index=0`/`data_error=0`/`error_pattern=0` 默认值填充
- **AND** PXView `DataFeedParser` 能正常消费该 packet 写入 `LogicSnapshot`

#### Scenario: 触发配置转换支持多 stage
- **WHEN** PXView `TriggerConfig::Adv` 配置 3 stage 触发条件
- **THEN** `pxview_trigger_to_srstd()` 生成上游 `srstd_trigger` 含 3 个 stage
- **AND** 每 stage 的 per-channel match 正确映射(`SR_TRIGGER_ZERO/ONE/RISING/FALLING/EDGE`)
- **AND** 上游 `soft_trigger_logic` 能正确执行多 stage 匹配

### Requirement: 设备调度分流

系统 SHALL 在 `DeviceAgent` 层根据设备来源(`LIB_PXVIEW` 或 `LIB_SRSTD`)将配置/采集/触发调用分流到对应库。

#### Scenario: DSL 设备走 PXView 原生路径
- **WHEN** 用户选中 pxlogic 设备(VID 0x2A0E)并启动采集
- **THEN** `DeviceAgent` 调用 `ds_start_collect()`(PXView 原生)
- **AND** 触发同步走 `ds_trigger_*`(DSL 硬件触发)
- **AND** 数据流走 PXView `ds_datafeed_callback`

#### Scenario: slogic 设备走上游库路径
- **WHEN** 用户选中 slogic 设备(VID 0x359f)并启动采集
- **THEN** `DeviceAgent` 调用 `srstd_dev_acquisition_start()`(上游库)
- **AND** 触发同步走 `pxview_trigger_to_srstd()` + `srstd_session_trigger_set()`
- **AND** 数据流经 `srstd_packet_to_pxview()` 转换后转发给 PXView `ds_datafeed_callback`

### Requirement: slogic 触发完整还原

系统 SHALL 通过上游原生 `soft_trigger_logic` 实现 slogic 设备的软件触发,支持边沿/电平/脉冲宽度匹配及多 stage 组合。

#### Scenario: slogic 单 stage 边沿触发
- **WHEN** 用户为 slogic 配置 channel 0 上升沿触发
- **AND** 启动采集
- **THEN** slogic 驱动调用 `soft_trigger_logic_check()` 等待触发条件
- **AND** 数据流在触发条件满足前被丢弃(`trigger_fired=FALSE`)
- **AND** 触发后正常采集(`trigger_fired=TRUE`)

#### Scenario: slogic 多 stage 触发
- **WHEN** 用户为 slogic 配置 stage0=channel0 上升沿 + stage1=channel1 高电平
- **THEN** 上游 `soft_trigger_logic` 按 stage 顺序匹配
- **AND** 两 stage 都满足后才开始采集

## MODIFIED Requirements

### Requirement: 触发配置同步

`SigSession::sync_trigger_to_libsigrok()` 单点同步入口增加设备库类型分流:
- `LIB_PXVIEW` 设备:走 `ds_trigger_*` 硬件触发同步(现状不变)
- `LIB_SRSTD` 设备:走 `pxview_trigger_to_srstd()` + `srstd_session_trigger_set()` 软件触发同步

## REMOVED Requirements

### Requirement: 兼容层触发桥接(方案 C)

**Reason**: 方案 C 的 `compat_trigger.c` 桥接层是签名 hack,仅支持 single stage + 5 种 match,且与上游 `soft_trigger_logic` 永久割裂。方案 B 通过双库共存 + 上游原生 `soft_trigger_logic` 完全替代,实现 100% 触发还原(多 stage + 所有 match 类型)。

**Migration**:
- 删除 `libsigrok/hardware/compat/compat_trigger.h` + `compat_trigger.c`
- 删除 `libsigrok/hardware/compat/compat_helpers.h:399-401` 的 stub 注释
- slogic 驱动从 PXView compat 层迁移到 `libsigrokstd/`,用上游原生 API
- 其余 80 个兼容驱动仍用 compat 层(非本 spec 范围,后续逐步迁移)
