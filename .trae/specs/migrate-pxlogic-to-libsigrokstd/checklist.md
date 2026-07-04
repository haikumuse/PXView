# Checklist

## Phase 1: USB 协议兼容性验证
- [x] Task 1 完成：scilogic 0.5.2 与 PXLogic fork 的 `usb_ctrl.c` diff 报告输出（结论：USB 寄存器层逐字节一致）
- [x] Task 1 完成：scilogic 0.5.2 与 PXLogic fork 的 `protocol.c` USB transfer 逻辑 diff 报告输出（结论：驱动上层严重分歧）
- [x] Task 1 完成：决策记录——scilogic 0.5.2 不能直接采用，方案 pivot 为以 pxlogic fork 为基础 port 到 libsigrokstd

## Phase 2: libsigrokstd 公共 API 扩展
- [x] Task 2 完成：`libsigrokstd/include/libsigrok/libsigrok.h` 新增 30 个 key（13 fork 60001-60013 + 17 fork 30000-range 重新分配到 60020+）
- [x] Task 2 完成：`libsigrokstd/src/hwdriver.c` 的 `sr_key_info_config[]` 数组新增 30 行映射
- [x] Task 2 完成：libsigrokstd.dll 编译通过，enum 无冲突（0 error）

## Phase 3: pxlogic fork 驱动 port 到 libsigrokstd + 上游 API 适配
- [ ] Task 3 完成：5 个文件拷贝到 `libsigrokstd/src/hardware/pxlogic/`（fork 源，不是 scilogic 0.5.2）
- [ ] Task 3 完成：USB 寄存器层零修改（Task 1 已验证）
- [ ] Task 3 完成：头文件 include 适配（libsigrok-internal.h → libsigrokstd 内部头）
- [ ] Task 3 完成：日志 API 替换（ds_log_* → sr_dbg/sr_warn/sr_err/sr_info）
- [ ] Task 3 完成：数据流路径替换（ds_data_forward LA_CROSS_DATA → 驱动内 deinterleave + sr_session_send sample-interleaved）
- [ ] Task 3 完成：触发对象迁移（ds_trigger 全局对象 → dev_context 16 级 stage + sr_trigger_match）
- [ ] Task 3 完成：fork 独有功能保留（firmware_config/hw_usb_open/ctl_data/PWM0/启动脉冲/supported_PX[]/16 级触发）
- [ ] Task 3 完成：30 个 SR_CONF_* key 的 config_get/set case 分支验证
- [ ] Task 3 完成：SR_REGISTER_DEV_DRIVER section 机制注册适配
- [ ] Task 3 完成：CMake 拾取 + 编译验证

## Phase 4: 固件资源
- [ ] Task 4 完成：`SCI_LOGIC.bin` / `SCI_LOGIC_BL.bin` / `hspi_ddr.bin` / `hspi_ddr_RST.bin` 拷贝到 libsigrokstd firmware 目录
- [ ] Task 4 完成：port 后的 `firmware_config()` / `hw_usb_open()` / `sr_resource_open` 能正确加载固件
- [ ] Task 4 完成：PXView 启动代码调用 `sr_resource_set_path()` 配置路径

## Phase 5: 删除 fork libsigrok + bridge
- [ ] Task 5 完成：`libsigrok/` 整个目录删除
- [ ] Task 5 完成：`libsigrokstd/bridge/` 整个目录删除
- [ ] Task 5 完成：`CMakeLists.txt` 删除 libsigrok.cmake include + libsigrok_SOURCES
- [ ] Task 5 完成：`libsigrokstd/CMakeLists.txt` 删除 bridge 源文件引用
- [ ] Task 5 完成：`pxview-core` 只链接 libsigrokstd
- [ ] Task 5 完成：libsigrokstd.dll 不依赖 fork libsigrok

## Phase 6: Core 层 ds_* → sr_* 替换（sigsession.cpp）
- [ ] Task 6 完成：`ds_lib_init/exit` → `sr_init/exit`
- [ ] Task 6 完成：`ds_get_device_list` → `sr_dev_list`
- [ ] Task 6 完成：`ds_active_device` → `sr_dev_open`
- [ ] Task 6 完成：`ds_set_datafeed_callback_ex` → `sr_session_datafeed_callback_add`
- [ ] Task 6 完成：`ds_set_event_callback_ex` → `sr_session_datafeed_callback_add`
- [ ] Task 6 完成：`ds_get_libusb_context` → `ctx->libusb_ctx`
- [ ] Task 6 完成：`ds_set_firmware_resource_dir` → `sr_resource_set_path`
- [ ] Task 6 完成：`ds_trigger_*` 全部删除
- [ ] Task 6 完成：SigSession 持有 `struct sr_session*`
- [ ] Task 6 完成：sigsession.cpp 无 ds_* 调用

## Phase 7: Core 层 ds_* → sr_* 替换（deviceagent）
- [ ] Task 7 完成：`DeviceLib` enum + `set_device_lib`/`device_lib` 删除
- [ ] Task 7 完成：11+ 个 `if (LIB_SRSTD)` 双分支删除
- [ ] Task 7 完成：`ds_get_actived_device_config` → `sdi->driver->config_get`
- [ ] Task 7 完成：`ds_set_actived_device_config` → `sdi->driver->config_set`
- [ ] Task 7 完成：`ds_get_actived_device_config_list` → `sdi->driver->config_list`
- [ ] Task 7 完成：`ds_get_actived_device_info` → accessor
- [ ] Task 7 完成：`ds_enable_device_channel` → `sr_dev_channel_enable`
- [ ] Task 7 完成：`ds_is_collecting` → `sr_session` 状态跟踪
- [ ] Task 7 完成：`ds_get_actived_device_status` 删除
- [ ] Task 7 完成：`ds_dsl_option_value_to_code` 删除
- [ ] Task 7 完成：`srstd_glue_*` 调用删除
- [ ] Task 7 完成：fork 扩展 accessor 删除（get_probe_vdiv/offset/vgain/preoff）
- [ ] Task 7 完成：deviceagent 无 ds_*/srstd_glue_* 调用

## Phase 8: Core 层其他文件 ds_* 清理
- [ ] Task 8 完成：`capturemanager.cpp` ds_start/stop → sr_session_*
- [ ] Task 8 完成：`datafeedparser.cpp/h` 适配上游 packet + 删 status
- [ ] Task 8 完成：`sessionstatecontext.cpp/h` ds_* 清理
- [ ] Task 8 完成：`signalmodel.cpp` ds_* 清理
- [ ] Task 8 完成：`session_service.cpp` ds_* → sr_*
- [ ] Task 8 完成：`datasource.cpp/h` 删 sr_channel 扩展访问
- [ ] Task 8 完成：`logicsignal.cpp` ds_trigger_* 清理
- [ ] Task 8 完成：`view_cursors.cpp` ds_* 清理
- [ ] Task 8 完成：Core 层无 ds_*/srstd_glue_* 调用

## Phase 9: 删除 LA_CROSS_DATA 路径
- [ ] Task 9 完成：`LogicSnapshot::append_cross_payload` 删除
- [ ] Task 9 完成：`LA_CROSS_DATA`/`LA_SPLIT_DATA` enum 引用删除
- [ ] Task 9 完成：`logicsnapshot_diskcache_writer.cpp` cross data 路径删除
- [ ] Task 9 完成：LogicSnapshot 只保留 `append_payload`
- [ ] Task 9 完成：grep 验证无 LA_CROSS_DATA/append_cross_payload 引用

## Phase 10: 删除 fork 扩展功能（sr_status/DSO）+ Adv/Serial trigger 调用清理
- [ ] Task 10 完成：`dso_measure.cpp/h` sr_status 读取删除
- [ ] Task 10 完成：`triggerdock.cpp/h` Adv trigger UI **保留**（后续 PXLogic 可扩展）
- [ ] Task 10 完成：`triggerdock.cpp` Serial trigger UI **保留**
- [ ] Task 10 完成：`TriggerConfig` Core 结构 **保留**（含 Adv/Serial 字段）
- [ ] Task 10 完成：`sigsession.cpp` `ds_trigger_*` 调用删除，`sync_trigger_to_libsigrok()` 只同步 simple trigger
- [ ] Task 10 完成：DsoSignal/DsoDock stub 或移除
- [ ] Task 10 完成：View DSO 模式切换路径删除
- [ ] Task 10 完成：ChannelDock 适配上游 channel 模型
- [ ] Task 10 完成：Viewport 删除 sr_status 显示
- [ ] Task 10 完成：SamplingBar 适配上游采样率/模式
- [ ] Task 10 完成：MCP API 删除 DSL 专属工具
- [ ] Task 10 完成：grep 验证无 sr_status/ds_trigger_set_mask/ds_trigger_set_count 引用（UI 保留，调用删除）

## Phase 11: 编译与冒烟测试
- [ ] Task 11 完成：`ninja -j 16 && ninja install` 编译成功（0 error），PXView.exe 生成
- [ ] Task 11 完成：PXView.exe headless 模式启动，MCP API 端口 10110 监听
- [ ] Task 11 完成：MCP `get_devices` 返回 PXLogic + 上游驱动
- [ ] Task 11 完成：grep 验证全代码库无 ds_*/srstd_glue_*/LIB_PXVIEW/LIB_SRSTD/sr_status/LA_CROSS_DATA 引用

## Phase 12: PXLogic 硬件功能验证（用户手动）
- [ ] Task 12 完成：设备扫描 + 打开 + 采集全流程
- [ ] Task 12 完成：buffer/stream 6 通道模式 × 多档采样率
- [ ] Task 12 完成：roll mode 自动启用
- [ ] Task 12 完成：simple trigger 5 种 match
- [ ] Task 12 完成：电压阈值/时钟边沿/外部触发/滤波/触发输出
- [ ] Task 12 完成：PWM 输出 + loop mode
- [ ] Task 12 完成：数据导出 CSV/VCD/srzip/binary
- [ ] Task 12 完成：MCP API 工具调用

## Phase 13: 上游驱动回归（用户手动）
- [ ] Task 13 完成：Demo 设备全模式回归
- [ ] Task 13 完成：Filelog 文件加载/保存
- [ ] Task 13 完成：上游 fx2lafw 驱动采集验证（如有硬件）

## Phase 14: 文档更新
- [ ] Task 14 完成：`AGENTS.md` 移除双库共存描述
- [ ] Task 14 完成：`project_memory.md` 记录 fork 删除 + DSL 弃用
- [ ] Task 14 完成：README 标注"DSL 硬件不支持"

## 关键约束验证
- [ ] `libsigrok/` 整个目录已删除
- [ ] `libsigrokstd/bridge/` 整个目录已删除
- [ ] Core 层无 `ds_*` 调用（grep 验证 0 命中）
- [ ] Core 层无 `srstd_glue_*` 调用（grep 验证 0 命中）
- [ ] `DeviceLib`/`LIB_PXVIEW`/`LIB_SRSTD` enum 已删除
- [ ] `LA_CROSS_DATA`/`append_cross_payload` 已删除
- [ ] `sr_status` struct 引用已删除
- [ ] `ds_trigger_*` 调用已删除（UI 保留）
- [ ] Adv/Serial trigger UI **保留**（后续 PXLogic 可扩展）
- [ ] `TriggerConfig` Core 结构保留（含 Adv/Serial 字段）
- [ ] DSO 模式已删除或 stub
- [ ] 30 个 SR_CONF_* key 全部有 config_get/set case 分支
- [ ] 数据导出层零改动（sr_output API 100% 兼容）
- [ ] enum 值在 libsigrokstd 范围内唯一（13 fork 60001-60013 保持原值 + 17 fork 30000-range 重新分配到 60020+）
- [ ] PXLogic 设备通过 port 后的 pxlogic 驱动识别
- [ ] DSL 硬件不再被识别（fork 已删）
