# Checklist

## juntek-jds6600 迁移检查
- [ ] protocol.h 包含 `#include "compat.h"`，无 `#include <libsigrok/libsigrok.h>` 或 `#include "libsigrok-internal.h"`
- [ ] protocol.h 保留 dev_context 结构体（device/waveforms/channel_config/quick_req 字段）
- [ ] protocol.h 保留所有 jds6600_* 函数声明
- [ ] protocol.c 套用 14 条转换规则，无 `di->context`（应为 `di->priv`）
- [ ] protocol.c 本地实现 `std_dummy_dev_acquisition_start` 返回 SR_OK（no-op）
- [ ] protocol.c 本地实现 `std_dummy_dev_acquisition_stop` 返回 SR_OK（no-op）
- [ ] protocol.c 本地实现 `dev_clear`：调用 `clear_helper(devc)` 释放资源后调用 `std_dev_clear(driver)`
- [ ] api.c 包含 8 个 compat 包装函数（compat_init/cleanup/scan/dev_open/dev_close/dev_acquisition_start/dev_acquisition_stop/config_list）
- [ ] api.c 定义 driver_info `juntek_jds6600_driver_info`（extern 声明 + static drv_ptr + compat_init 模式）
- [ ] api.c scan 函数手动遍历 options GSList 解析 SR_CONF_CONN/SR_CONF_SERIALCOMM
- [ ] api.c 无 `SR_REGISTER_DEV_DRIVER` 宏调用
- [ ] api.c dev_acquisition_start/dev_acquisition_stop 指向本地 no-op 实现

## gmc-mh-1x-2x 迁移检查
- [ ] protocol.h 包含 `#include "compat.h"`，无 `#include <libsigrok/libsigrok.h>` 或 `#include "libsigrok-internal.h"`
- [ ] protocol.h 保留 dev_context 结构体（model/limits/mq/unit/mqflags/value/scale/scale1000/buf/buflen 字段）
- [ ] protocol.h 枚举标签类型已转换：`enum sr_mq mq` → `int mq`、`enum sr_unit unit` → `int unit`、`enum sr_mqflag mqflags` → `uint64_t mqflags`（参考 atorch 验证，PXView 匿名枚举无标签类型）
- [ ] protocol.h `sr_sw_limits` static inline 定义带 `#ifndef SR_SW_LIMITS_H` guard 宏
- [ ] protocol.h 保留所有 gmc_* 函数声明
- [ ] protocol.c 套用 14 条转换规则，无 `di->context`（应为 `di->priv`）
- [ ] protocol.c `sr_analog_init` 调用替换为 `memset(&analog, 0, sizeof(analog))`
- [ ] protocol.c `analog.meaning->mq` → `analog.mq`，`analog.meaning->unit` → `analog.unit`，`analog.meaning->mqflags` → `analog.mqflags`
- [ ] protocol.c `analog.meaning->channels` → `analog.probes = g_slist_append(NULL, ch)`
- [ ] protocol.c `analog.encoding->digits` → `analog.unit_bits = 32`（sizeof(float)）
- [ ] protocol.c 移除 encoding/meaning/spec 局部变量声明
- [ ] api.c 包含 2 套 8 compat 包装函数（rs232 变体 + bd232 变体，共享的实现可同名）
- [ ] api.c 定义 2 个 driver_info：`gmc_mh_1x_2x_rs232_driver_info` + `gmc_mh_2x_bd232_driver_info`
- [ ] api.c 本地实现 `std_serial_dev_acquisition_stop`（serial_source_remove + serial_close + std_session_send_df_end，参考 colead-slm）
- [ ] api.c scan 函数手动遍历 options GSList 解析 SR_CONF_CONN/SR_CONF_SERIALCOMM
- [ ] api.c `serial_source_add(sdi->session, serial, G_IO_IN, 40, cb, sdi)` 保持 5-arg
- [ ] api.c 无 `SR_REGISTER_DEV_DRIVER` 宏调用

## CMakeLists.txt 注册检查
- [ ] line 657 后插入 19 个 `option(ENABLE_DRIVER_*)` 声明
- [ ] line 867 后插入 19 个 `if(ENABLE_DRIVER_*) add_definitions(-DHAVE_DRIVER_*) endif()` 块
- [ ] line 1281 后插入 19 个 `if(ENABLE_DRIVER_*) list(APPEND libsigrok_SOURCES ...) endif()` 块
- [ ] gmc-mh-1x-2x 的源文件 list 包含 api.c + protocol.c（一个 list 块，对应 2 个 driver_info）
- [ ] 无重复的 option/add_definitions/list 条目

## hwdriver.c 注册检查
- [ ] line 289 后插入 20 个 `#ifdef HAVE_DRIVER_* extern SR_PRIV struct sr_dev_driver *_driver_info; #endif` 声明
- [ ] line 486 后插入 20 个 `#ifdef HAVE_DRIVER_* &*_driver_info, #endif` drivers_list 项
- [ ] gmc-mh-1x-2x 占 2 个 extern + 2 个 drivers_list 项（rs232 + bd232）
- [ ] 所有 driver_info 名称与各驱动 api.c 中定义的一致

## 编译验证检查
- [ ] cmake 配置成功，CMakeCache.txt 中 19 个 ENABLE_DRIVER_* 选项均为 ON
- [ ] `cd build && ninja -j 16` 编译成功，无错误
- [ ] 无 multiple definition 错误（特别是 std_session_send_df_frame_begin 单一实现、sr_sw_limits guard 宏）
- [ ] 无 undefined reference 错误（特别是 std_dummy_*、std_serial_dev_acquisition_stop、std_dev_clear_with_callback 的本地实现）
- [ ] PXView.exe 生成成功
- [ ] 启用全部 19 个驱动后链接成功（无符号冲突）
