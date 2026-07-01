# Checklist — 迁移剩余串口驱动 + CMake 注册

## serial-lcr 迁移检查
- [x] protocol.h 包含 `#include "compat.h"`，无 `#include <libsigrok/libsigrok.h>` 或 `#include "libsigrok-internal.h"`
- [x] api.c 静态 lcr_info 数组包含 6 个模型（DER EE DE-5000/MASTECH MS5308/PeakTech 2170/UNI-T UT612/PeakTech 2165/Voltcraft 4080，覆盖 ES51919/VC4080 芯片系列）
- [x] protocol.h dev_context 枚举标签类型已转换：`enum sr_mq mq` → `int mq`、`enum sr_unit unit` → `int unit`、`enum sr_mqflag mqflags` → `uint64_t mqflags`
- [x] protocol.c 扁平 analog：`memset(&analog, 0, sizeof(analog))` + `analog.probes`/`analog.mq`/`analog.unit`/`analog.mqflags`/`analog.unit_bits = 32`
- [x] protocol.c 本地 `sr_session_send_meta` 实现
- [x] protocol.c 本地 `dev_acquisition_stop`：serial_source_remove + serial_close + std_session_send_df_end
- [x] protocol.c `serial_stream_detect` 7-arg 调用（baudrate 参数传 0 或 9600）
- [x] api.c 8 个 compat 包装函数齐全
- [x] api.c driver_info `serial_lcr_driver_info` 定义正确
- [x] api.c scan 手动遍历 options GSList 解析 SR_CONF_CONN/SR_CONF_SERIALCOMM
- [x] api.c `serial_source_add(sdi->session, serial, G_IO_IN, 40, cb, sdi)` 5-arg
- [x] api.c 无 `SR_REGISTER_DEV_DRIVER` 宏调用

## juntek-jds6600 迁移检查
- [x] protocol.h 包含 `#include "compat.h"`，无 `#include <libsigrok/libsigrok.h>` 或 `#include "libsigrok-internal.h"`
- [x] protocol.h 保留 dev_context 结构体（device/waveforms/channel_config/quick_req 字段）
- [x] protocol.h 保留所有 jds6600_* 函数声明
- [x] protocol.c 本地 `std_dummy_dev_acquisition_start` 返回 SR_OK（no-op）
- [x] protocol.c 本地 `std_dummy_dev_acquisition_stop` 返回 SR_OK（no-op）
- [x] protocol.c 本地 `dev_clear`：调用 clear_helper 释放资源后调用 std_dev_clear
- [x] api.c 8 个 compat 包装函数齐全
- [x] api.c driver_info `juntek_jds6600_driver_info` 定义正确
- [x] api.c scan 手动遍历 options GSList
- [x] api.c 无 `SR_REGISTER_DEV_DRIVER` 宏调用
- [x] api.c dev_acquisition_start/dev_acquisition_stop 指向本地 no-op 实现

## gmc-mh-1x-2x 迁移检查
- [x] protocol.h 包含 `#include "compat.h"`，无 `#include <libsigrok/libsigrok.h>` 或 `#include "libsigrok-internal.h"`
- [x] protocol.h dev_context 枚举标签类型已转换：`enum sr_mq mq` → `int mq`、`enum sr_unit unit` → `int unit`、`enum sr_mqflag mqflags` → `uint64_t mqflags`
- [x] protocol.h `sr_sw_limits` static inline 定义带 `#ifndef SR_SW_LIMITS_H` guard 宏
- [x] protocol.c 扁平 analog：`memset` + `analog.probes = g_slist_append(NULL, ch)` + `analog.mq`/`analog.unit`/`analog.mqflags`/`analog.unit_bits = 32`
- [x] protocol.c 移除 encoding/meaning/spec 局部变量声明
- [x] api.c 包含 2 套 8 compat 包装函数（rs232 变体 + bd232 变体）
- [x] api.c 定义 2 个 driver_info：`gmc_mh_1x_2x_rs232_driver_info` + `gmc_mh_2x_bd232_driver_info`
- [x] api.c 本地 `std_serial_dev_acquisition_stop`（serial_source_remove + serial_close + std_session_send_df_end，参考 colead-slm）
- [x] api.c scan 手动遍历 options GSList
- [x] api.c `serial_source_add(sdi->session, ...)` 保持 5-arg
- [x] api.c 无 `SR_REGISTER_DEV_DRIVER` 宏调用

## CMakeLists.txt 注册检查
- [x] line 657 后插入 20 个 `option(ENABLE_DRIVER_*)` 声明
- [x] line 867 后插入 20 个 `if(ENABLE_DRIVER_*) add_definitions(-DHAVE_DRIVER_*) endif()` 块
- [x] line 1281 后插入 20 个 `if(ENABLE_DRIVER_*) list(APPEND libsigrok_SOURCES ...) endif()` 块
- [x] gmc-mh-1x-2x 的源文件 list 包含 api.c + protocol.c（一个 list 块，对应 2 个 driver_info）
- [x] 无重复的 option/add_definitions/list 条目

## hwdriver.c 注册检查
- [x] line 289 后插入 21 个 `#ifdef HAVE_DRIVER_* extern SR_PRIV struct sr_dev_driver *_driver_info; #endif` 声明
- [x] line 486 后插入 21 个 `#ifdef HAVE_DRIVER_* &*_driver_info, #endif` drivers_list 项
- [x] gmc-mh-1x-2x 占 2 个 extern + 2 个 drivers_list 项（rs232 + bd232）
- [x] 所有 driver_info 名称与各驱动 api.c 中定义的一致

## 冲突避让检查
- [x] `compat/compat_config.h` 未被本 spec 修改
- [x] `compat/compat_helpers.h` 未被本 spec 修改
- [x] `compat/compat_helpers.c` 未被本 spec 修改
- [x] 23 个 BROKEN 驱动（openbench-logic-sniffer/kingst-la2016/asix-sigma/saleae-logic-pro/saleae-logic16/sysclk-lwla/lecroy-logicstudio/pipistrello-ols/14 个 DMM/6 个示波器）未被本 spec 修改
- [x] 未执行编译验证（推迟到 tiered-driver-compat-fix 完成后）
