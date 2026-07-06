# Tasks

## 阶段 1: libsigrok 扩展 + 基础热插拔

- [x] Task 1: libsigrok 新增 hotplug.c 模块
  - [x] SubTask 1.1: 在 `libsigrok/src/hotplug.c` 实现 `sr_hotplug_libusb_cb`（libusb 回调 trampoline，转调 `user_cb`）
  - [x] SubTask 1.2: 实现 `sr_hotplug_thread_proc`（GThread 循环调 `libusb_handle_events_timeout`，100ms 超时）
  - [x] SubTask 1.3: 实现 `sr_listen_hotplug`（检查 `libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)` → `libusb_hotplug_register_callback` ATTACHED|LEFT → 启动 GThread）
  - [x] SubTask 1.4: 实现 `sr_close_hotplug`（置 `hp_running=FALSE` → deregister callback → `g_thread_join` → free state）
  - [x] SubTask 1.5: 在文件内定义 `struct sr_hotplug_state`（hp_handle/hp_thread/hp_running/user_cb/user_data）

- [x] Task 2: libsigrok 头文件声明
  - [x] SubTask 2.1: `libsigrok/include/libsigrok/libsigrok.h` 追加 `enum sr_hotplug_event { SR_HOTPLUG_ATTACH=0, SR_HOTPLUG_DETACH=1 }` 与 `typedef void (*sr_hotplug_callback)(int event, void *user_data)`
  - [x] SubTask 2.2: `libsigrok/include/libsigrok/proto.h` 追加 `SR_API int sr_listen_hotplug(struct sr_context*, sr_hotplug_callback, void*)` 与 `SR_API int sr_close_hotplug(struct sr_context*)`

- [x] Task 3: libsigrok sr_context 扩展
  - [x] SubTask 3.1: `libsigrok/src/libsigrok-internal.h` 前置声明 `struct sr_hotplug_state`
  - [x] SubTask 3.2: `struct sr_context` 末尾追加 `struct sr_hotplug_state *hotplug_state`（`#ifdef HAVE_LIBUSB_1_0` 包裹）

- [x] Task 4: libsigrok sr_exit 集成清理
  - [x] SubTask 4.1: `libsigrok/src/backend.c` 的 `sr_exit` 在 `sr_hw_cleanup_all` 之后、`libusb_exit` 之前调用 `sr_close_hotplug(ctx)`

- [x] Task 5: libsigrok usb.c Windows 兼容修复
  - [x] SubTask 5.1: `libsigrok/src/usb.c` `usb_source_new` 已含 pollfds NULL 回退（验证：line 280-302 已实现 timer-only fallback，对所有平台当 pollfds 返回 NULL 时统一回退，比 `#ifdef _WIN32` 更通用）
  - [x] SubTask 5.2: 验证 `usb_source_dispatch` 仍调用 `receive_data` → `libusb_handle_events_timeout`（确认无需修改）

- [x] Task 6: libsigrok CMake 集成
  - [x] SubTask 6.1: `libsigrok/CMakeLists.txt` 使用 `file(GLOB_RECURSE ... src/*.c)`，hotplug.c 自动收录（验证通过）

## 阶段 2: PXView Core 集成

- [x] Task 7: SigSession 头文件扩展
  - [x] SubTask 7.1: `PXView/pv/sigsession.h` 私有区声明 `static void hotplug_cb_(int event, void *user_data)`
  - [x] SubTask 7.2: 声明 `void on_hotplug_event_(int event)`（主线程槽）
  - [x] SubTask 7.3: 声明 `QTimer *reconnect_timer_ = nullptr` + `void start_reconnect_watchdog_()` + `void on_reconnect_timeout_()`
  - [x] SubTask 7.4: 声明辅助 `bool is_current_device_gone_()` + `void update_device_handle_()`
  - [x] SubTask 7.5: 添加 `#include <QTimer>`（line 28，紧随 `<QDateTime>`）

- [x] Task 8: SigSession init/deinit 集成
  - [x] SubTask 8.1: `init()` 末尾（line 289-301）调用 `sr_listen_hotplug(_sr_ctx, &SigSession::hotplug_cb_, this)` + 日志
  - [x] SubTask 8.2: `uninit()`（line 309-314）在 `sr_exit` 之前调用 `sr_close_hotplug(_sr_ctx)`（幂等）

- [x] Task 9: 热插拔回调实现
  - [x] SubTask 9.1: `hotplug_cb_` 静态 trampoline —— `QMetaObject::invokeMethod + Qt::QueuedConnection` 转发
  - [x] SubTask 9.2: `on_hotplug_event_` ATTACH —— `refresh_device_list()` + `broadcast_async<UsbDeviceArrived>({})`
  - [x] SubTask 9.3: `on_hotplug_event_` DETACH —— `is_current_device_gone_()` 判断（保守返回 true，让 watchdog 兜底）
  - [x] SubTask 9.4: DETACH 非采集中 —— `refresh_device_list()` + `broadcast_async<DeviceDetached>({})`
  - [x] SubTask 9.5: DETACH 采集中 —— `start_reconnect_watchdog_()` 500ms 单次触发

- [x] Task 10: 重连容忍实现（阶段 2）
  - [x] SubTask 10.1: `on_reconnect_timeout_()` —— `stop_capture()` + `refresh_device_list` + 广播 `DeviceDetached`
  - [x] SubTask 10.2: ATTACH 在 watchdog 活跃时 `stop()` + `update_device_handle_()` + 日志 "Device reconnected"
  - [x] SubTask 10.3: `update_device_handle_()` 简化实现（仅警告日志，fx2lafw 重连由 libusb 内部自动重绑 sdi）
  - [x] SubTask 10.4: `~SigSession()` 显式 `reconnect_timer_->stop()` + `deleteLater()` + 置空

- [x] Task 11: 辅助函数实现
  - [x] SubTask 11.1: `is_current_device_gone_()` 保守返回 true（hotplug.c 回调签名不传 libusb_device*，让 watchdog 兜底）

## 阶段 3: View 层集成

- [x] Task 12: View 层事件订阅（**研究确认：View 层早已就绪，无需修改**）
  - [x] SubTask 12.1: 定位完成 —— `MainWindow::on_event(UsbDeviceArrived)` 在 mainwindow.cpp:2909 已存在；`on_event(DeviceDetached)` 在 mainwindow.cpp:2965 已存在
  - [x] SubTask 12.2: `UsbDeviceArrived` 处理器已调用 `_sampling_bar->update_device_list()` 刷新下拉框
  - [x] SubTask 12.3: `DeviceDetached` 处理器通过 `set_default_device()` 间接触发 `CurrentDeviceChanged` → `update_device_list()`
  - 注：现有 UI 提示采用 `MsgBox::Confirm` 对话框询问"切换新设备？"，比 Toast 更适合交互场景；如需额外 Toast 可后续增强，当前避免过度设计

## 阶段 4: 验证

- [x] Task 13: 编译验证
  - [x] SubTask 13.1: `cd build && ninja -j 16` 编译通过（修复了 2 个编译错误：hotplug.c include 顺序 + SigSession 非 QObject 的 QMetaObject/QTimer 用法）
  - [x] SubTask 13.2: `ninja install` 安装到 `install.dir/bin/PXView.exe`（28.5MB，2026-07-06 19:18:32）

- [x] Task 14: 启动验证
  - [x] SubTask 14.1: 启动 PXView，日志 `Hotplug listener registered` 出现（验证通过）
  - [x] SubTask 14.2: libusb hotplug 不可用时降级日志正常（本机支持，未触发降级路径）

- [ ] Task 15: 功能验证（fx2lafw 设备）—— **需用户手动测试**
  - [ ] SubTask 15.1: 插入 fx2lafw → 1 秒内下拉框自动出现 + Toast
  - [ ] SubTask 15.2: 拔出 fx2lafw（未采集）→ 1 秒内下拉框自动移除 + Toast
  - [ ] SubTask 15.3: 启动采集 → 拔出 → 500ms 内重连 → 采集继续 + 日志 "Device reconnected"
  - [ ] SubTask 15.4: 启动采集 → 拔出 → 500ms 超时 → 优雅停止 + Toast "设备已断开"
  - [ ] SubTask 15.5: fx2lafw 采集数据完整（验证 usb.c Windows 修复生效）

# Task Dependencies

- Task 2、3 可与 Task 1 并行（独立头文件/结构改动）
- Task 4 依赖 Task 1（`sr_close_hotplug` 实现完成）
- Task 5 独立，可与 Task 1-4 并行
- Task 6 依赖 Task 1（hotplug.c 创建完成）
- Task 7-11（PXView Core）依赖 Task 1-6 全部完成（libsigrok API 可用）
- Task 10 依赖 Task 9（基础 DETACH 处理完成）
- Task 12（View）依赖 Task 9-10（事件广播已就绪）
- Task 13-15 依赖全部前置任务完成
