# Tasks

- [ ] Task 1: USB 协议代码 diff 验证固件版本兼容性
  - [ ] SubTask 1.1: Diff `C:\Users\admin\Downloads\sigrok-git\libsigrok\src\hardware\scilogic\usb_ctrl.c` 与 `libsigrok/hardware/pxlogic/usb_ctrl.c`，记录寄存器地址/命令字/批量传输协议差异
  - [ ] SubTask 1.2: Diff `scilogic/protocol.c` 与 `libsigrok/hardware/pxlogic/pxlogic.c` 的 USB transfer 接收/状态机逻辑，记录差异
  - [ ] SubTask 1.3: 输出差异报告，决定是否需要把 fork 的 USB 协议改动合并到 scilogic（如分歧大，本 spec 暂停，先合并 USB 代码）

- [ ] Task 2: libsigrokstd 公共 API 扩展（17 个 SR_CONF_* key）
  - [ ] SubTask 2.1: 在 `libsigrokstd/include/libsigrok/libsigrok.h` 的 `enum sr_configkey` 末尾新增 5 个 sigrok-git 已验证 key（SR_CONF_TRIGGER_OUT/SR_CONF_VTH/SR_CONF_EX_TRIGGER_MATCH/SR_CONF_CHANNEL_MODE/SR_CONF_OPERATION_MODE）
  - [ ] SubTask 2.2: 验证 12 个 PXView fork 已有 key（SR_CONF_STREAM=30052/SR_CONF_ROLL=30053/SR_CONF_BUFFER_OPTIONS=30066/SR_CONF_LOOP_MODE=60001/SR_CONF_PWM0_EN=60004/PWM0_FREQ=60005/PWM0_DUTY=60006/SR_CONF_VLD_CH_NUM=30027/SR_CONF_HW_DEPTH=30075/SR_CONF_MAX_HEIGHT=30069/SR_CONF_DISK_CACHE_ENABLE=60011/DISK_CACHE_PATH=60012/STREAM_BUFF=60010/STREAM_MEM_BUFF=60013）已在 libsigrokstd.h 中存在，如缺失则补加（数值与 PXView fork 完全一致）
  - [ ] SubTask 2.3: 在 `libsigrokstd/src/hwdriver.c` 的 `sr_key_info_config[]` 数组新增 17 行映射
  - [ ] SubTask 2.4: 编译 libsigrokstd.dll 验证 enum 无冲突

- [ ] Task 3: 拷贝 scilogic 驱动到 libsigrokstd + 补全 config
  - [ ] SubTask 3.1: 把 `C:\Users\admin\Downloads\sigrok-git\libsigrok\src\hardware\scilogic\` 5 文件拷贝到 `libsigrokstd/src/hardware/scilogic/`
  - [ ] SubTask 3.2: 修改 `protocol.h` 中的 `FIRMWARE_VERSION` 从 `0x56900005` 改为 `0x56900027`
  - [ ] SubTask 3.3: 在 `struct dev_context` 新增字段（roll_mode/is_loop/pwm0_*/disk_cache_*/stream_buff_size/stream_mem_buff_size）
  - [ ] SubTask 3.4: 在 `api.c` 的 `config_get` 新增 17 个 case 分支（STREAM/ROLL/BUFFER_OPTIONS/VLD_CH_NUM/HW_DEPTH/MAX_HEIGHT/LOOP_MODE/PWM0_*/DISK_CACHE_*/STREAM_BUFF/STREAM_MEM_BUFF）
  - [ ] SubTask 3.5: 在 `api.c` 的 `config_set` 新增 13 个 case 分支（除 SR_CONF_ROLL 只读外全部可写），PWM0 寄存器写入从 fork `pxlogic.c:1419-1476` 迁移
  - [ ] SubTask 3.6: `config_get(SR_CONF_ROLL)` 推导逻辑实现（op_mode==OP_STREAM && samplerate>=50MHz）
  - [ ] SubTask 3.7: `config_set(SR_CONF_LOOP_MODE)` + `scilogic_start_acquisition` OP_LPTEST 路径
  - [ ] SubTask 3.8: `scilogic_dev_new()` 初始化新增 devc 字段默认值
  - [ ] SubTask 3.9: CMake GLOB_RECURSE 拾取 scilogic 目录 + 编译验证

- [ ] Task 4: 固件资源路径配置
  - [ ] SubTask 4.1: 把 fork `libsigrok/hardware/pxlogic/` 目录下的 `SCI_LOGIC.bin` 和 `hspi_ddr.bin`（或 `hspi_ddr_RST.bin`）拷贝到 libsigrokstd firmware 资源目录
  - [ ] SubTask 4.2: 验证 `scilogic_ch569w_firmware_upload()` / `scilogic_fpga_firmware_upload()` 的 `sr_resource_open` 调用能正确加载固件
  - [ ] SubTask 4.3: 在 PXView 启动代码调用 `sr_resource_set_path()` 配置 libsigrokstd firmware 路径（如尚未配置）

- [ ] Task 5: 删除 PXView fork libsigrok + bridge 层
  - [ ] SubTask 5.1: 删除 `libsigrok/` 整个目录（hardware/DSL/、hardware/pxlogic/、hardware/common/、include/、src/、output/、input/ 等）
  - [ ] SubTask 5.2: 删除 `libsigrokstd/bridge/` 整个目录（srstd_bridge.h/c、srstd_pxview_glue.h/c、srstd_init_shared.c、srstd_compat.c）
  - [ ] SubTask 5.3: 删除 `CMakeLists.txt` 的 `include(${CMAKE_SOURCE_DIR}/cmake/libsigrok.cmake)` + `libsigrok_SOURCES` 变量 + `cmake/libsigrok.cmake` 文件
  - [ ] SubTask 5.4: 删除 `libsigrokstd/CMakeLists.txt` 的 bridge 源文件引用
  - [ ] SubTask 5.5: `pxview-core` 链接改为只链接 `libsigrokstd`
  - [ ] SubTask 5.6: 编译验证 libsigrokstd.dll 不依赖 fork libsigrok

- [ ] Task 6: Core 层 ds_* → sr_* 全量替换（sigsession.cpp）
  - [ ] SubTask 6.1: `ds_lib_init/exit` → `sr_init/exit(ctx)`
  - [ ] SubTask 6.2: `ds_get_device_list` → `sr_dev_list(driver)` 遍历
  - [ ] SubTask 6.3: `ds_active_device` → `sr_dev_open(sdi)`
  - [ ] SubTask 6.4: `ds_set_datafeed_callback_ex` → `sr_session_datafeed_callback_add`
  - [ ] SubTask 6.5: `ds_set_event_callback_ex` → `sr_session_datafeed_callback_add` + 私有事件
  - [ ] SubTask 6.6: `ds_get_libusb_context` → `ctx->libusb_ctx`
  - [ ] SubTask 6.7: `ds_set_firmware_resource_dir` → `sr_resource_set_path`
  - [ ] SubTask 6.8: `ds_trigger_*` 调用全部删除（Adv/Serial trigger 弃用），保留 simple trigger 通过 `sr_trigger_*` 或驱动 config_set
  - [ ] SubTask 6.9: SigSession 持有 `struct sr_session*`，实现 session-based 模型
  - [ ] SubTask 6.10: 编译验证 sigsession.cpp 无 ds_* 调用

- [ ] Task 7: Core 层 ds_* → sr_* 全量替换（deviceagent.cpp/h）
  - [ ] SubTask 7.1: 删除 `DeviceLib` enum（LIB_PXVIEW/LIB_SRSTD）+ `set_device_lib`/`device_lib` 方法
  - [ ] SubTask 7.2: 删除 11+ 个 `if (LIB_SRSTD)` 双分支，所有方法直接调 `sdi->driver->config_get/set` 或 `sr_session_*`
  - [ ] SubTask 7.3: `ds_get_actived_device_config` → `sdi->driver->config_get`
  - [ ] SubTask 7.4: `ds_set_actived_device_config` → `sdi->driver->config_set`
  - [ ] SubTask 7.5: `ds_get_actived_device_config_list` → `sdi->driver->config_list`
  - [ ] SubTask 7.6: `ds_get_actived_device_info` → `sr_dev_inst_vendor_get` 等 accessor
  - [ ] SubTask 7.7: `ds_enable_device_channel` → `sr_dev_channel_enable`
  - [ ] SubTask 7.8: `ds_is_collecting` → `sr_session` 状态跟踪
  - [ ] SubTask 7.9: `ds_get_actived_device_status` **删除**（DSL 弃用，PXLogic 不需要 sr_status）
  - [ ] SubTask 7.10: `ds_dsl_option_value_to_code` **删除**（DSL 专属）
  - [ ] SubTask 7.11: 删除 `srstd_glue_*` 调用（bridge 已删）
  - [ ] SubTask 7.12: 删除 `get_probe_vdiv`/`get_probe_offset`/`get_probe_vgain`/`get_probe_preoff` 等 fork 扩展 accessor（sr_channel 25 扩展字段弃用）
  - [ ] SubTask 7.13: 编译验证 deviceagent 无 ds_*/srstd_glue_* 调用

- [ ] Task 8: Core 层其他文件 ds_* 清理
  - [ ] SubTask 8.1: `capturemanager.cpp`：`ds_start_collect` → `sr_session_start`，`ds_stop_collect` → `sr_session_stop`，删 sr_status 轮询
  - [ ] SubTask 8.2: `datafeedparser.cpp/h`：适配上游 `sr_datafeed_packet`（去掉 status/bExportOriginalData 字段）
  - [ ] SubTask 8.3: `sessionstatecontext.cpp/h`：ds_* 清理
  - [ ] SubTask 8.4: `signalmodel.cpp`：ds_* 清理
  - [ ] SubTask 8.5: `session_service.cpp`：ds_* → sr_* 替换
  - [ ] SubTask 8.6: `datasource.cpp/h`：删 sr_channel 扩展字段访问
  - [ ] SubTask 8.7: `logicsignal.cpp`：ds_trigger_* 清理
  - [ ] SubTask 8.8: `view_cursors.cpp`：ds_* 清理
  - [ ] SubTask 8.9: 编译验证 Core 层无 ds_*/srstd_glue_* 调用

- [ ] Task 9: 删除 LA_CROSS_DATA 路径
  - [ ] SubTask 9.1: 删除 `LogicSnapshot::append_cross_payload`（logicsnapshot.cpp/logicsnapshot.h）
  - [ ] SubTask 9.2: 删除 `LA_CROSS_DATA`/`LA_SPLIT_DATA` enum 引用
  - [ ] SubTask 9.3: 删除 `logicsnapshot_diskcache_writer.cpp` 中的 cross data 处理路径
  - [ ] SubTask 9.4: LogicSnapshot 只保留 sample-interleaved 路径（`append_payload`）
  - [ ] SubTask 9.5: grep 验证无 LA_CROSS_DATA/append_cross_payload 引用

- [ ] Task 10: 删除 fork 扩展功能（sr_status/DSO）+ Adv/Serial trigger 调用清理
  - [ ] SubTask 10.1: 删除 `dso_measure.cpp/h` 的 sr_status 字段读取（整体移除或纯软件计算）
  - [ ] SubTask 10.2: **保留** `triggerdock.cpp/h` 的 Adv trigger UI（count/inv/logic 16 级 stage）——后续 PXLogic 可扩展
  - [ ] SubTask 10.3: **保留** `triggerdock.cpp` 的 Serial trigger UI——后续 PXLogic 可扩展
  - [ ] SubTask 10.4: **保留** `TriggerConfig` Core 结构（含 Adv/Serial trigger 字段）——单一真相源不破坏
  - [ ] SubTask 10.5: 删除 `sigsession.cpp` 的 `ds_trigger_*` 调用，`sync_trigger_to_libsigrok()` 只同步 simple trigger 部分
  - [ ] SubTask 10.6: DsoSignal/DsoDock 适配为 stub 或整体移除（无 DSCope 硬件）
  - [ ] SubTask 10.7: View 的 DSO 模式切换路径删除
  - [ ] SubTask 10.8: ChannelDock 适配上游 channel 模型（无 sr_channel 扩展字段）
  - [ ] SubTask 10.9: Viewport 删除 sr_status 相关显示
  - [ ] SubTask 10.10: SamplingBar 适配上游采样率/模式切换
  - [ ] SubTask 10.11: MCP API 删除 DSL 专属工具（如有 set_adv_trigger/set_serial_trigger）
  - [ ] SubTask 10.12: grep 验证无 sr_status/ds_trigger_set_mask/ds_trigger_set_count 引用（Adv/Serial trigger UI 保留，只是调用删除）

- [ ] Task 11: 编译与冒烟测试
  - [ ] SubTask 11.1: 执行 `cd build && ninja -j 16 && ninja install`，验证 libsigrokstd.dll + PXView.exe 编译成功（0 error）
  - [ ] SubTask 11.2: 启动 PXView.exe headless 模式，验证 MCP API 端口 10110 监听
  - [ ] SubTask 11.3: 调用 MCP `get_devices` 工具，验证返回 PXLogic 设备 + 上游驱动（fx2lafw/slogic 等）
  - [ ] SubTask 11.4: grep 验证全代码库无 ds_*/srstd_glue_*/LIB_PXVIEW/LIB_SRSTD/sr_status/LA_CROSS_DATA 引用

- [ ] Task 12: PXLogic 硬件功能验证（需真实硬件，用户手动执行）
  - [ ] SubTask 12.1: 设备扫描 + 打开 + 采集全流程
  - [ ] SubTask 12.2: buffer/stream 模式 6 通道模式 × 多档采样率采集
  - [ ] SubTask 12.3: roll mode 自动启用（stream + ≥50MHz）
  - [ ] SubTask 12.4: simple trigger 5 种 match
  - [ ] SubTask 12.5: 电压阈值/时钟边沿/外部触发/滤波/触发输出
  - [ ] SubTask 12.6: PWM 输出 + loop mode
  - [ ] SubTask 12.7: 数据导出 CSV/VCD/srzip/binary
  - [ ] SubTask 12.8: MCP API 工具调用

- [ ] Task 13: 上游驱动回归验证（用户手动执行）
  - [ ] SubTask 13.1: Demo 设备全模式回归
  - [ ] SubTask 13.2: Filelog 文件加载/保存
  - [ ] SubTask 13.3: 上游 fx2lafw 驱动（如有硬件）采集验证

- [ ] Task 14: 文档更新
  - [ ] SubTask 14.1: 更新 `AGENTS.md`，移除双库共存描述，改为"libsigrokstd 是唯一 libsigrok"
  - [ ] SubTask 14.2: 更新 `project_memory.md`，记录 fork 删除 + DSL 弃用决策
  - [ ] SubTask 14.3: README 标注"DSL 硬件不支持，请使用原版 DSView"

# Task Dependencies

- Task 2 依赖 Task 1（确认 USB 协议兼容性后再扩展 API）
- Task 3 依赖 Task 2（API 扩展完成后才能编译驱动）
- Task 4 与 Task 3 并行（固件资源路径独立）
- Task 5 依赖 Task 3（PXLogic 迁移完成后才删 fork，避免编译断链）
- Task 6/7/8 依赖 Task 5（fork 删除后 Core 层 ds_* 替换）
- Task 9 与 Task 6/7/8 并行（LA_CROSS_DATA 路径删除独立）
- Task 10 依赖 Task 6/7/8（Core 层清理完成后 View 层适配）
- Task 11 依赖 Task 5/6/7/8/9/10 全部完成
- Task 12/13 依赖 Task 11（编译通过后硬件验证）
- Task 14 依赖 Task 12/13 验证通过
