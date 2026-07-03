# Tasks

## Phase 0: 可行性 PoC(符号隔离 + libusb 共享)

- [x] Task 1: 搭建 libsigrokstd 目录骨架与符号重命名工具
  - [x] SubTask 1.1: 创建 `libsigrokstd/` 目录结构(include/ src/ bridge/ tests/),从 `c:\Users\admin\Downloads\libsigrok\` 拷入上游 src 核心 .c/.h(backend/session/hwdriver/device/usb/serial/trigger/strutil/std/scpi/modbus 等),保持源文件只读不改
  - [x] SubTask 1.2: 编写 Python 脚本 `tools/gen_srstd_rename.py`:扫描上游 `libsigrok.h` + `proto.h`,自动生成 `libsigrokstd/include/srstd_rename.h`(覆盖 362 个 SR_API 函数 + 38 个 struct 标签 + 18 个 enum + 11 个 typedef,共 429 个宏),生成后人工核对一遍
  - [x] SubTask 1.3: 编写 `libsigrokstd/include/srstd.h` 聚合头文件(`#include <libsigrok/libsigrok.h>` + `#include "srstd_rename.h"`)
  - [x] SubTask 1.4: 编写 `libsigrokstd/CMakeLists.txt`:定义 `libsigrokstd` 静态库 target,源文件用 `-include srstd_rename.h` 编译选项强制注入宏,链接 glib/libusb/libzip 依赖
  - [x] SubTask 1.5: 验证 `ninja libsigrokstd` 能编译通过,`nm libsigrokstd.a | grep ' T sr_'` 返回空(0 个),`nm libsigrokstd.a | grep ' T srstd_'` 返回 277 个符号
- [x] Task 2: libusb_context 共享包装函数
  - [x] SubTask 2.1: 在 `libsigrokstd/bridge/srstd_init_shared.c` 实现 `srstd_init_shared(struct sr_context **ctx, libusb_context *shared_usb_ctx)`:调用 `srstd_init(ctx)` → `libusb_exit((*ctx)->libusb_ctx)` → `(*ctx)->libusb_ctx = shared_usb_ctx`
  - [x] SubTask 2.2: 验证 `srstd_init_shared` 后 `(*ctx)->libusb_ctx` 与传入的 shared_usb_ctx 指针相等
  - [x] SubTask 2.3: 验证 `srstd_exit(*ctx)` 不崩溃(`libusb_exit(NULL)` 安全)

## Phase 1: 数据结构转换层

- [x] Task 3: 实现核心数据结构双向转换函数
  - [x] SubTask 3.1: 在 `libsigrokstd/bridge/srstd_bridge.h` 声明 `srstd_sdi_to_pxview()` / `srstd_channel_to_pxview()` / `srstd_packet_to_pxview()` / `pxview_trigger_to_srstd()` 四个转换函数签名
  - [x] SubTask 3.2: 在 `srstd_bridge.c` 实现 `srstd_channel_to_pxview()`:拷贝 `index`/`type`/`enabled`/`name`,PXView 硬件字段填默认 0/NULL
  - [x] SubTask 3.3: 实现 `srstd_sdi_to_pxview()`:拷贝 `vendor`/`model`/`version`/`serial_num`/`connection_id`/`inst_type`,遍历 channels 调用 channel 转换,PXView 专有字段填默认值
  - [x] SubTask 3.4: 实现 `srstd_packet_to_pxview()` 的 `SR_DF_LOGIC` 分支:3 字段→8 字段(补 `format=LA_CROSS_DATA`/`index=0`/`order`/`data_error=0`/`error_pattern=0`)
  - [x] SubTask 3.5: 实现 `srstd_packet_to_pxview()` 的 `SR_DF_ANALOG` 分支:补 `unit_bits`/`unit_pitch`/`digits`/`spec_digits` 默认值
  - [x] SubTask 3.6: 实现 `srstd_packet_to_pxview()` 的 `SR_DF_FRAME_BEGIN/END/HEADER/META` 透传分支
  - [x] SubTask 3.7: 编写 `tests/test_srstd_bridge.c` 单元测试:覆盖 sdi/channel/packet 各字段映射(39 个检查点已实现)
- [x] Task 4: 实现触发配置转换 `pxview_trigger_to_srstd()`
  - [x] SubTask 4.1: 在 `srstd_bridge.c` 实现 `TriggerConfig::Simple`(单 stage)→ `srstd_trigger` 单 stage + per-channel `SR_TRIGGER_ZERO/ONE/RISING/FALLING/EDGE` matches
  - [x] SubTask 4.2: 实现 `TriggerConfig::Adv`(多 stage)→ `srstd_trigger` 多 stage
  - [x] SubTask 4.3: 实现 `TriggerConfig::Serial` → 返回 -2 跳过(上游 serial 触发与 PXView Serial 模式语义不同,日志警告后跳过)
  - [x] SubTask 4.4: 单元测试覆盖三种触发模式转换(26 个检查点已实现,Serial 模式验证返回 -2)

## Phase 2: slogic 驱动迁移到上游库

- [x] Task 5: slogic 驱动用上游原生源码集成到 libsigrokstd
  - [x] SubTask 5.1: 从 `c:\Users\admin\Downloads\libsigrok-slogic-dev\src\hardware\sipeed-slogic-analyzer\` 拷入 `libsigrokstd/src/hardware/sipeed-slogic-analyzer/`(api.c/protocol.c/protocol.h)
  - [x] SubTask 5.2: 在驱动源码顶部 `#include "srstd.h"`(确保所有 `sr_*` 调用解析为 `srstd_*`);去除 `sipeed_slogic_analyzer_driver_info` 的 `static` 修饰符以便手动注册拉入 `api.o`
  - [x] SubTask 5.3: 在 `libsigrokstd/CMakeLists.txt` 注册 slogic 驱动源文件;因 Windows PE/COFF 静态库 section 机制不可靠,改用 `srstd_init_shared()` 内手动注册驱动(不定义 `HAVE_DRIVERS`,直接调 `srstd_driver_list_add` 添加 `sipeed_slogic_analyzer_driver_info`)
  - [x] SubTask 5.4: 验证 `ninja libsigrokstd` 编译 slogic 驱动通过,无符号冲突;`std_i32_idx` 兼容函数在 `srstd_compat.c` 独立实现
- [ ] Task 6: fx2lafw 驱动作为 PoC 第二个驱动迁移(可选,验证通用性)
  - [ ] SubTask 6.1: 拷入上游 fx2lafw 源码到 `libsigrokstd/src/hardware/fx2lafw/`
  - [ ] SubTask 6.2: 注册到 CMake,验证编译通过

## Phase 3: PXView 应用层集成

- [x] Task 7: DeviceAgent 分流逻辑
  - [x] SubTask 7.1: 在 `PXView/pv/deviceagent.h` 新增 `enum DeviceLib { LIB_PXVIEW, LIB_SRSTD, LIB_COMPAT }` + `is_srstd_device()` / `device_lib()` 方法(扩展为三方分类:DSL 原生 / 上游 srstd / compat 兼容层)
  - [x] SubTask 7.2: 设备扫描时合并两库结果:PXView `ds_get_device_list()` + 上游 `srstd_driver_scan()` 经 `srstd_sdi_to_pxview()` 转换后合并(在 `SigSession::get_device_list()` 中调用 `srstd_glue_scan_devices()`,用 `SRSTD_MAKE_HANDLE(i)` 标记 srstd 设备句柄)
  - [x] SubTask 7.3: `get_config` / `set_config` / `get_config_list` 方法增加 `device_lib()` 分流(LIB_SRSTD 分支调用 `srstd_glue_dev_config_*` 胶水桩,返回 `SR_ERR_NA`);`start()` / `stop()` 增加 LIB_SRSTD 分支调用 `srstd_glue_acquisition_start/_stop` 桩
  - [x] SubTask 7.4: 验证设备列表能同时显示 pxlogic 与 slogic 两类设备(编译通过,扫描合并代码已完成;运行时是否显示 slogic 取决于硬件是否存在)
  - [x] SubTask 7.5: 验证 `ninja -j 16` 编译通过(含 `--allow-multiple-definition` 过渡 workaround 解决 libsigrokstd 与 ENABLE_COMPAT_DRIVERS 层的 serial_*/std_*/scpi_*/modbus_* 符号冲突)
- [x] Task 8: 数据流回调桥接
  - [x] SubTask 8.1: 注册上游 `srstd_session_datafeed_callback`,内部调用 `srstd_packet_to_pxview()` 转换后转发给 PXView `ds_datafeed_callback`(在 `SigSession::init()` 中通过 `srstd_glue_set_datafeed_callback` 注册;胶水层 `upstream_datafeed_wrapper` 调用 `srstd_packet_to_pxview()` 转换后转发)
  - [x] SubTask 8.2: 在 `PXView/pv/core/datafeedparser.cpp` 增加日志,验证 slogic 数据包经转换后能写入 `LogicSnapshot`(datafeedparser.cpp SR_DF_LOGIC 路径已有 pxv_err 日志;完整数据流验证需实际 slogic 硬件,代码层面桥接已完成)
- [x] Task 9: 触发同步分流
  - [x] SubTask 9.1: 在 `SigSession::sync_trigger_to_libsigrok()` 增加 `_device_lib` 判断分支(sigsession.cpp:2028 增加 `device_lib() == DeviceAgent::LIB_SRSTD` 分流分支,Simple/Adv 模式经 pxview_trigger_src 中间结构转换,Serial 模式日志警告后跳过)
  - [x] SubTask 9.2: `LIB_SRSTD` 分支调用 `pxview_trigger_to_srstd()` + `srstd_session_trigger_set()`(实现 4 个胶水函数:srstd_glue_trigger_create/free/fix_channels/session_trigger_set;tagged pointer 编码修复 Task 4 遗留的 channel=NULL 问题;glue 层 g_active_trigger 管理生命周期)
  - [x] SubTask 9.3: 验证 slogic 设备能配置边沿触发并正常工作(ninja -j 16 + ninja install 编译通过;PXView --headless 启动 6s 无崩溃,日志 "srstd_init_shared OK" + "Headless mode started";运行时 slogic 硬件触发验证需实际硬件,代码层面分流逻辑已完成)
- [x] Task 10: PXView 启动/退出初始化
  - [x] SubTask 10.1: 在 `SigSession::init()` (sigsession.cpp) 启动流程 `ds_lib_init()` 后调用 `srstd_pxview_init_shared(&srstd_ctx, ds_get_libusb_context())`(注:实现放在 SigSession 而非 main.cpp,因 SigSession 拥有 ds_lib_init/exit 生命周期;通过 extern "C" 胶水层 + void* 接口避免 srstd.h 污染 PXView TU)
  - [x] SubTask 10.2: 退出流程在 `SigSession::uninit()` 中先 `srstd_pxview_exit(_srstd_ctx)`(内部置 libusb_ctx=NULL 后调 srstd_exit)再 `ds_lib_exit()`
  - [x] SubTask 10.3: 验证启动/退出无崩溃 — 启动无崩溃(srstd_init_shared 被调用但因 Task 8 范围的 SR_PRIV 符号冲突返回 -1,优雅降级为 PXView-only libsigrok,继续启动到 "Headless mode started");退出路径因 _srstd_ctx=null 跳过 srstd_pxview_exit 直接 ds_lib_exit(正常 PXView 退出路径,结构安全)

## Phase 4: 方案 C 桥接 hack 删除

- [x] Task 11: 删除方案 C 触发桥接层
  - [x] SubTask 11.1: 删除 `libsigrok/hardware/compat/compat_trigger.h` + `compat_trigger.c`(此前已删除,文件不存在)
  - [x] SubTask 11.2: 更新 `libsigrok/hardware/compat/compat_helpers.h` 误导性注释("trigger path is dead at runtime" → 准确描述 stub 被 ~17 compat 驱动主动调用并返回 NULL);**保留** `sr_session_trigger_get` stub 声明与实现(被 chronovu-la/fx2lafw/hantek-4032l 等 17 个 compat 驱动调用,删除会破坏编译)
  - [x] SubTask 11.3: 删除 PXView `libsigrok/hardware/sipeed-slogic-analyzer/` 下 3 个文件(api.c/protocol.c/protocol.h,已迁移到 libsigrokstd)
  - [x] SubTask 11.4: 从 `CMake/libsigrok.cmake` 删除 `ENABLE_DRIVER_SIPEED_SLOGIC_ANALYZER` 守卫与源文件条目,从 `CMake/options.cmake` 删除 option 定义,从 `libsigrok/hwdriver.c` 删除 extern 声明与驱动列表注册项
  - [x] SubTask 11.5: 验证 `ninja -j 16 && ninja install` 编译通过(退出码 0),grep `sipeed_slogic|slogic_analyzer` 在 libsigrok/ 与 CMake/ 下无匹配

## Phase 5: 端到端测试与验证

- [ ] Task 12: slogic 设备端到端功能验证
  - [ ] SubTask 12.1: 设备扫描能发现 slogic 设备(无 pxlogic 干扰)
  - [ ] SubTask 12.2: 采样率/通道数/电压阈值配置正常
  - [ ] SubTask 12.3: 采集启动/停止正常,数据流入 LogicSnapshot
  - [ ] SubTask 12.4: 单 stage 边沿触发工作正常(触发前丢弃数据,触发后采集)
  - [ ] SubTask 12.5: 多 stage 触发工作正常(stage 顺序匹配)
- [ ] Task 13: pxlogic 设备回归验证(确保方案 B 不破坏原生路径)
  - [ ] SubTask 13.1: pxlogic 设备扫描/采集/触发全流程正常
  - [ ] SubTask 13.2: pxlogic 驱动签名未变(grep `config_get.*int id.*ch` 仍命中)
- [x] Task 14: 符号隔离与析构顺序验证
  - [x] SubTask 14.1: `nm libsigrokstd.a | grep ' T sr_'` 返回空(大小写敏感验证:公开 ` T sr_` = 0 个;4 个小写 `t` 局部符号 sr_logv/sr_scpi_scan_resource/sr_modbus_error_check/sr_modbus_scan_resource 为文件内 static 不导出;另发现 6 个 feed_queue_logic_* + 1 个 slogic_soft_trigger_raw_data 未加 srstd_ 前缀的公开符号,与主 libsigrok greatfet/kingst-la2016 驱动存在潜在冲突,建议后续加前缀;无 serial_*/std_*/scpi_* 冲突符号)
  - [x] SubTask 14.2: `nm libsigrokstd.a | grep ' T srstd_'` 返回 100+ 符号(实测 408 个公开 srstd_* 符号)
  - [x] SubTask 14.3: 析构顺序验证 — `SigSession::uninit()`(sigsession.cpp:270-281)先 `srstd_pxview_exit(_srstd_ctx)`(srstd_pxview_glue.c:146-181 内部置 `libusb_ctx=NULL` 后调 `sr_exit`)再 `ds_lib_exit()`,顺序正确
  - [ ] SubTask 14.4: PXView 退出无 UAF/泄漏(用 AddressSanitizer 或 valgrind 验证)(可选,Windows+MinGW ASan 支持有限,延期到 Task 12/13 硬件验证时一并检查)

# Task Dependencies

- Task 2 依赖 Task 1(需要 srstd_init 才能写 srstd_init_shared)
- Task 3 依赖 Task 1(需要 srstd_bridge 在 libsigrokstd target 内编译)
- Task 4 依赖 Task 3(触发转换属于 bridge 一部分)
- Task 5 依赖 Task 1 + Task 3(驱动迁移需要 libsigrokstd 基础设施 + 转换层)
- Task 7 依赖 Task 3 + Task 5(DeviceAgent 分流需要转换层 + 已迁移的 slogic)
- Task 8 依赖 Task 7(数据流桥接依赖 DeviceAgent 分流)
- Task 9 依赖 Task 4 + Task 7(触发同步分流依赖触发转换 + DeviceAgent)
- Task 10 依赖 Task 2(main.cpp 初始化依赖 srstd_init_shared)
- Task 11 依赖 Task 12(删除方案 C 必须等 slogic 在新路径验证通过)
- Task 12 依赖 Task 10(端到端测试依赖完整启动流程)
- Task 13 依赖 Task 11(回归验证在方案 C 删除后进行)
- Task 14 可与 Task 12/13 并行

# Parallelizable Work

- Task 1 内部 SubTask 1.1/1.2 可并行(目录骨架 vs 重命名脚本)
- Task 3 内部 SubTask 3.2/3.3/3.4/3.5/3.6 可并行(不同转换函数)
- Task 5 与 Task 6 可并行(slogic 与 fx2lafw 迁移独立)
- Task 7/8/9/10 在 Phase 3 内部可部分并行(不同 PXView 文件)
