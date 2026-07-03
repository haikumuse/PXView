# Checklist

## Phase 1: 自动化迁移脚本

- [ ] `tools/migrate_drivers_to_srstd.py` 脚本创建完成,支持 `--all`/`--exclude`/`--dry-run` 参数
- [ ] 脚本正确扫描上游 `c:\Users\admin\Downloads\libsigrok\src\hardware\` 与 PXView compat 层的交集驱动
- [ ] 脚本排除 DSL/pxlogic/demo/compat/sipeed-slogic-analyzer(已迁移或不属于 compat)
- [ ] 脚本对每个驱动拷贝上游源码到 `libsigrokstd/src/hardware/<driver>/`
- [ ] 脚本修改每个驱动的 `protocol.h` 插入 `#include "srstd.h"`
- [ ] 脚本修改每个驱动的 `api.c` 去除 `<name>_driver_info` 的 `static` 修饰符
- [ ] 脚本生成 `libsigrokstd/src/driver_registry.c` 含所有迁移驱动的 `extern` 声明 + `srstd_get_all_drivers()` 函数
- [ ] 识别并排除依赖 VXI/GPIB/VISA 的驱动(若有)
- [ ] 确认依赖 libserialport/hidapi/libftdi 的驱动有 `#ifdef` 守卫

## Phase 2: libsigrokstd 批量集成

- [ ] `libsigrokstd/CMakeLists.txt` 注册所有迁移驱动源文件
- [ ] `driver_registry.c` 加入源文件列表(带 `srstd_rename.h` 编译)
- [ ] `ninja libsigrokstd` 编译所有迁移驱动通过,无符号冲突
- [ ] `srstd_init_shared.c` 删除 slogic 单驱动手动注册代码
- [ ] `srstd_init_shared.c` 调用 `srstd_get_all_drivers()` 批量注册
- [ ] `srstd_init_shared` 后 `ctx->driver_list` 包含所有迁移驱动

## Phase 3: DeviceAgent 动态驱动识别

- [ ] `srstd_pxview_glue.h` 声明 `srstd_glue_get_driver_names()`
- [ ] `srstd_pxview_glue.c` 实现 `srstd_glue_get_driver_names()`(遍历 driver_list 收集 .name)
- [ ] `deviceagent.h` 的 `is_srstd_device()` 改为声明(非硬编码)
- [ ] `deviceagent.cpp` 实现 `is_srstd_device()` 调用 `srstd_glue_get_driver_names()` 动态查询
- [ ] 任意 srstd 驱动设备(如 fx2lafw)能被 `is_srstd_device()` 识别为 true

## Phase 4: 删除 PXView compat 层

- [ ] `libsigrok/hardware/compat/` 整个目录已删除(9 个文件)
- [ ] `libsigrok/hardware/` 下所有 compat 驱动目录已删除(约 80 个,保留 DSL/pxlogic/demo/common)
- [ ] `CMake/options.cmake` 删除 `ENABLE_COMPAT_DRIVERS` + 81 个 `ENABLE_DRIVER_*`
- [ ] `CMake/libsigrok.cmake` 删除两个 `if(ENABLE_COMPAT_DRIVERS)` 块
- [ ] `libsigrok/hwdriver.c` 删除所有 compat 驱动的 extern 声明与注册项
- [ ] `libsigrok/hwdriver.c` 删除 `HAVE_DRIVER_*` 宏相关的 `#ifdef` 块
- [ ] 保留 DSLogic/DSCope/pxlogic/demo 的 extern 声明与注册项

## Phase 5: 编译验证与清理

- [ ] `ninja -j 16 && ninja install` 编译通过(退出码 0)
- [ ] PXView `--headless` 启动无崩溃
- [ ] `nm libsigrokstd.a | grep ' T sr_'` 返回空(符号隔离未被破坏)
- [ ] `nm libsigrokstd.a | grep ' T srstd_'` 返回 500+ 符号(原 408 + 新增驱动)
- [ ] grep `ENABLE_COMPAT_DRIVERS` 在项目中无残留(除历史 spec/devdoc)
- [ ] grep `compat_helpers|compat_serial|compat_scpi` 在 `libsigrok/` 无残留
- [ ] 空的 `libsigrok/hardware/sipeed-slogic-analyzer/` 目录已删除

## 架构约束验证

- [ ] DSL/pxlogic 原生驱动不受影响(继续走 PXView `ds_*` 路径)
- [ ] pxlogic 驱动签名不变(5 参 `config_get(int id, data, sdi, ch, cg)`)
- [ ] DSL 硬件触发(`ds_trigger_*`)对 pxlogic 完全保留
- [ ] libsigrokstd 与 PXView libsigrok 完全独立的 target,无源文件交叉
- [ ] 上游 libsigrok 源文件未被修改(只读副本,srstd_rename.h 宏注入)
- [ ] 析构顺序安全(srstd_pxview_exit → ds_lib_exit)
- [ ] 胶水层(srstd_pxview_glue.c)无需修改(已通用)
- [ ] DeviceAgent 三方分类(LIB_PXVIEW/LIB_SRSTD/LIB_COMPAT)中 LIB_COMPAT 不再有设备(LIB_SRSTD 取代)
