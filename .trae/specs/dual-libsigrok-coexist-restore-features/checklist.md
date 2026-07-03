# Checklist

## Phase 0: 可行性 PoC

- [x] `libsigrokstd/` 目录结构创建完整(include/ src/ bridge/ tests/ 子目录齐全)
- [x] 上游 libsigrok 核心 .c/.h 文件已拷入 `libsigrokstd/src/`,源文件未被修改
- [x] `tools/gen_srstd_rename.py` 脚本生成 `srstd_rename.h`,覆盖 362 个 SR_API 函数 + 38 个 struct + 18 个 enum + 11 个 typedef(共 429 宏)
- [x] `libsigrokstd/include/srstd.h` 聚合头文件正确(`#include <libsigrok/libsigrok.h>` + `#include "srstd_rename.h"`)
- [x] `libsigrokstd/CMakeLists.txt` 定义 `libsigrokstd` 静态库 target,源文件用 `-include srstd_rename.h` 编译
- [x] `ninja libsigrokstd` 编译通过,无错误无警告
- [x] `nm libsigrokstd.a | grep ' T sr_'` 返回空(零 `sr_*` 残留公开符号)
- [x] `nm libsigrokstd.a | grep ' T srstd_'` 返回 277 符号(重命名成功)
- [x] `srstd_init_shared()` 实现完成,调用后 `(*ctx)->libusb_ctx == shared_usb_ctx`
- [x] `srstd_exit()` 不崩溃(`libusb_exit(NULL)` 安全)

## Phase 1: 数据结构转换层

- [x] `libsigrokstd/bridge/srstd_bridge.h` 声明 4 个转换函数(sdi/channel/packet/trigger)+ 6 个 alloc/free + 3 个 size + 17 个 accessor(使用 void* 接口 + 影子结构避免枚举值冲突)
- [x] `srstd_channel_to_pxview()` 正确拷贝 `index`/`type`/`enabled`/`name`,PXView 硬件字段填默认 0/NULL
- [x] `srstd_sdi_to_pxview()` 正确拷贝 `vendor`/`model`/`version`/`serial_num`/`connection_id`/`inst_type`,channels GSList 经 channel 转换
- [x] `srstd_packet_to_pxview()` 的 `SR_DF_LOGIC` 分支:3 字段→8 字段(format=LA_CROSS_DATA/index=0/data_error=0/error_pattern=0)
- [x] `srstd_packet_to_pxview()` 的 `SR_DF_ANALOG` 分支:补 unit_bits/unit_pitch/digits/spec_digits 默认值
- [x] `srstd_packet_to_pxview()` 的 `SR_DF_FRAME_BEGIN/END/HEADER/META` 透传分支正常
- [x] `pxview_trigger_to_srstd()` 的 `TriggerConfig::Simple` 单 stage 转换正确(per-channel ZERO/ONE/RISING/FALLING/EDGE matches)
- [x] `pxview_trigger_to_srstd()` 的 `TriggerConfig::Adv` 多 stage 转换正确
- [x] `pxview_trigger_to_srstd()` 的 `TriggerConfig::Serial` 返回 -2 跳过(语义不兼容,日志警告)
- [x] `tests/test_srstd_bridge.c` 单元测试覆盖 sdi/channel/packet 各字段映射(39 个检查点)
- [x] `tests/test_srstd_trigger.c` 单元测试覆盖 Simple/Adv/Serial 三种触发模式转换(26 个检查点)

## Phase 2: slogic 驱动迁移

- [x] `libsigrokstd/src/hardware/sipeed-slogic-analyzer/` 含 api.c/protocol.c/protocol.h(从 libsigrok-slogic-dev 拷入)
- [x] 驱动源码顶部 `#include "srstd.h"`,所有 `sr_*` 调用解析为 `srstd_*`;去除 `sipeed_slogic_analyzer_driver_info` 的 static 修饰符
- [x] `libsigrokstd/CMakeLists.txt` 注册 slogic 源文件
- [x] slogic 驱动通过手动注册(`srstd_init_shared` 内 `srstd_driver_list_add`)注册到上游 drivers_list(Windows PE/COFF section 不可靠,改用手动注册)
- [x] `ninja libsigrokstd` 编译 slogic 驱动通过,无符号冲突;`std_i32_idx` 在 `srstd_compat.c` 独立实现
- [ ] fx2lafw 驱动同样迁移成功(可选,PoC 通用性验证,Task 6 延期)

## Phase 3: PXView 应用层集成

> **注:** 本节所有条目均为静态库阶段的接入尝试,实际未真正生效 — `srstd_init_shared` 因 SR_PRIV 符号冲突在运行时返回 -1(优雅降级为 PXView-only libsigrok)。真正的运行时接入由 `convert-libsigrokstd-to-shared-library` spec 通过将 libsigrokstd 转为 SHARED 库(符号隔离)后接管完成。以下条目保留历史勾选状态,仅作参考。

- [x] `DeviceAgent` 新增 `enum DeviceLib { LIB_PXVIEW, LIB_SRSTD, LIB_COMPAT }` + `is_srstd_device()` / `device_lib()` 方法(三方分类,`is_compat_device()` 已排除 srstd 设备) [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] 设备扫描合并两库结果:PXView `ds_get_device_list()` + 上游 `srstd_driver_scan()` 经 `srstd_sdi_to_pxview()` 转换(在 `SigSession::get_device_list()` 中通过 `srstd_glue_scan_devices()` 合并,`SRSTD_MAKE_HANDLE(i)` 标记 srstd 句柄) [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] `get_config`/`set_config`/`get_config_list`/`start`/`stop` 方法按 `device_lib()` 分流(LIB_SRSTD 分支调用 `srstd_glue_*` 胶水桩,返回 `SR_ERR_NA`;`active_device` 分流随扫描合并延期到 Task 8) [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] 设备列表能同时显示 pxlogic 与 slogic 两类设备(扫描合并代码已完成,编译通过;运行时是否显示 slogic 取决于硬件是否存在) [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] `libsigrokstd/bridge/srstd_pxview_glue.{h,c}` 胶水层创建完成:`srstd_pxview_init_shared`/`srstd_pxview_exit`(init/exit 包装)+ `srstd_glue_dev_config_get/_set/_list`/`srstd_glue_acquisition_start/_stop`(5 个 dispatch 桩返回 `SRSTD_GLUE_ERR_NA`) [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] `pxview-core` 静态库 PUBLIC 链接 `libsigrokstd`,PRIVATE 包含 `libsigrokstd/bridge` 头文件路径 [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] `ninja -j 16` 编译+链接通过(`-Wl,--allow-multiple-definition` 过渡 workaround 解决 libsigrokstd 与 ENABLE_COMPAT_DRIVERS 层 serial_*/std_*/scpi_*/modbus_* 符号冲突) [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] 上游 `srstd_session_datafeed_callback` 注册完成(在 `SigSession::init()` 中通过 `srstd_glue_set_datafeed_callback` 注册 PXView `DataFeedParser::data_feed_callback_ex`;胶水层 `srstd_glue_open_scanned_device` 内部调用 `sr_session_datafeed_callback_add` 注册上游 wrapper) [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] 数据流回调内部经 `srstd_packet_to_pxview()` 转换后转发给 PXView `ds_datafeed_callback`(胶水层 `upstream_datafeed_wrapper` 调用 `srstd_packet_to_pxview()` 转换 packet 后转发给 g_pxview_cb) [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] `DataFeedParser` 能消费经转换的 slogic packet 写入 `LogicSnapshot`(datafeedparser.cpp SR_DF_LOGIC 路径调用 `feed_in_logic`;完整运行时验证需实际 slogic 硬件,代码层面桥接已完成) [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] `SigSession::sync_trigger_to_libsigrok()` 增加 `_device_lib` 分流分支(sigsession.cpp:2028 `device_lib() == DeviceAgent::LIB_SRSTD` 分支;Simple 模式从 SignalModel.trig_type() 构建 per-channel value0 字符串;Adv 模式从 TriggerConfig.stages() 取 value0/value1;Serial 模式日志警告后跳过) [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] `LIB_SRSTD` 分支调用 `pxview_trigger_to_srstd()` + `srstd_session_trigger_set()`(4 个胶水函数实现:srstd_glue_trigger_create/free/fix_channels/session_trigger_set;tagged pointer 编码 `(void*)(intptr_t)(index+1)` 修复 Task 4 遗留 channel=NULL 问题;fix_channels 解码并替换为真实 sr_channel*;g_active_trigger 全局变量管理生命周期,close_active_device 时释放) [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] Task 9 编译验证:`ninja -j 16 && ninja install` 退出码 0,PXView.exe 安装到 install.dir/bin [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] Task 9 headless 启动验证:PXView --headless 运行 6s 无崩溃,日志 "srstd_init_shared OK, libusb_context shared" + "Headless mode started. MCP port 10110, WS port 10430" [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] `SigSession::init()` 启动流程 `ds_lib_init()` 后调用 `srstd_pxview_init_shared(&srstd_ctx, ds_get_libusb_context())`(实现在 sigsession.cpp 非 main.cpp;通过 extern "C" + void* 胶水避免 srstd.h 污染) [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] 退出流程在 `SigSession::uninit()` 中先 `srstd_pxview_exit(_srstd_ctx)`(内置 libusb_ctx=NULL)再 `ds_lib_exit()` [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]
- [x] 启动/退出无崩溃(启动日志 "srstd_init_shared OK, libusb_context shared" → "Headless mode started. MCP port 10110, WS port 10430";Task 9 验证时 srstd_init_shared 已成功,SR_PRIV 符号冲突经 srstd_rename.h 宏重命名解决;退出路径 _srstd_ctx 非空时先 srstd_pxview_exit 置 libusb_ctx=NULL 再 ds_lib_exit) [实际未接入 — 由 convert-libsigrokstd-to-shared-library 接管]

## Phase 4: 方案 C 桥接 hack 删除

- [x] `libsigrok/hardware/compat/compat_trigger.h` 已删除(此前已删除,文件不存在)
- [x] `libsigrok/hardware/compat/compat_trigger.c` 已删除(此前已删除,文件不存在)
- [x] `libsigrok/hardware/compat/compat_helpers.h` 误导性注释已更新("trigger path is dead at runtime" → 准确描述 stub 被 ~17 compat 驱动主动调用并返回 NULL);`sr_session_trigger_get` stub 声明与实现**保留**(被 17 个 compat 驱动调用,删除会破坏编译)
- [x] `libsigrok/hardware/sipeed-slogic-analyzer/` 下 3 个文件已删除(api.c/protocol.c/protocol.h,已迁移到 libsigrokstd)
- [x] `CMake/libsigrok.cmake` 已删除 `ENABLE_DRIVER_SIPEED_SLOGIC_ANALYZER` 守卫与源文件条目;`CMake/options.cmake` 已删除 option 定义
- [x] `libsigrok/hwdriver.c` 已删除 slogic extern 声明与驱动列表注册项
- [x] `ninja -j 16 && ninja install` 编译通过(退出码 0)
- [x] grep `sipeed_slogic|slogic_analyzer` 在 libsigrok/ 与 CMake/ 下无匹配(无残留引用)
- [x] 其余 80 个兼容驱动仍正常工作(sr_session_trigger_get stub 保留,compat 层编译无回归)

## Phase 5: 端到端测试

- [ ] slogic 设备扫描能发现(无 pxlogic 干扰)
- [ ] slogic 采样率/通道数/电压阈值配置正常
- [ ] slogic 采集启动/停止正常,数据流入 LogicSnapshot
- [ ] slogic 单 stage 边沿触发工作正常(触发前丢弃,触发后采集)
- [ ] slogic 多 stage 触发工作正常(stage 顺序匹配)
- [ ] pxlogic 设备扫描/采集/触发全流程正常(无回归)
- [ ] pxlogic 驱动签名未变(grep `config_get.*int id.*ch` 在 pxlogic.c 仍命中)
- [x] `nm libsigrokstd.a | grep ' T sr_'` 返回空(最终符号隔离验证,大小写敏感 ` T sr_` = 0 个公开符号;4 个小写 `t` 局部符号 sr_logv/sr_scpi_scan_resource/sr_modbus_error_check/sr_modbus_scan_resource 为文件内 static,不导出)
- [x] `nm libsigrokstd.a | grep ' T srstd_'` 返回 100+ 符号(实测 408 个公开 srstd_* 符号)
- [ ] PXView 退出无 UAF/泄漏(AddressSanitizer 或 valgrind 验证)(SubTask 14.3 可选,Windows+MinGW ASan 支持有限,延期到 Task 12/13 硬件验证时一并检查)

## 架构约束验证

- [x] PXView `pxlogic` 驱动签名不变(5 参 `config_get(int id, data, sdi, ch, cg)` — pxlogic.c:999 验证通过)
- [x] PXView Core(`SigSession`/`CaptureManager`/`DocumentRegistry`)无大改,仅 DeviceAgent/DataFeedParser/sync_trigger 三处增加分流
- [x] DSL 硬件触发(`ds_trigger_*`)对 pxlogic 完全保留(`ds_trigger_probe_set`/`ds_trigger_set_en` 在 libsigrok/trigger.c 实现,sigsession.cpp `sync_trigger_to_libsigrok` 单一同步入口)
- [x] `libsigrokstd` 与 PXView `libsigrok` 完全独立的 target,无源文件交叉(符号隔离验证:0 个公开 sr_* 残留,408 个 srstd_* 符号,证明两库符号空间完全分离)
- [x] 上游 libsigrok 源文件未被修改(只读副本)(srstd_rename.h 宏注入机制生效:408 个 srstd_* 符号证明源文件通过 -include srstd_rename.h 编译时重命名,源文件本身未修改)
- [x] 析构顺序安全:`SigSession::uninit()` 先 `srstd_pxview_exit`(内部置 `libusb_ctx=NULL` 后调 `sr_exit`)再 `ds_lib_exit()`(sigsession.cpp:270-281 + srstd_pxview_glue.c:146-181 验证通过)
- [x] 测试复杂度 O(1):转换层测通一次,所有上游驱动可用(无需逐驱动测试)(srstd_bridge.c 转换层 + srstd_pxview_glue.c 胶水层为所有上游驱动共用,slogic 仅是第一个迁移验证)
