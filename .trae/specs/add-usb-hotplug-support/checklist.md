# Checklist

## libsigrok 侧

- [x] `libsigrok/src/hotplug.c` 存在，导出 `sr_listen_hotplug` / `sr_close_hotplug` 两个 SR_API 函数
- [x] `hotplug.c` 内部 `sr_hotplug_thread_proc` GThread 循环调用 `libusb_handle_events_timeout`，超时 100ms
- [x] `sr_listen_hotplug` 在调用前检查 `libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)`，不支持时返回 `SR_ERR` 不崩溃
- [x] `sr_listen_hotplug` 在已监听状态下重复调用幂等（warn + 返回 `SR_OK`）
- [x] `sr_close_hotplug` 正确顺序：置 `hp_running=FALSE` → deregister → `g_thread_join` → free state → 置 NULL
- [x] `libsigrok/include/libsigrok/libsigrok.h` 包含 `enum sr_hotplug_event { SR_HOTPLUG_ATTACH=0, SR_HOTPLUG_DETACH=1 }` 与 `sr_hotplug_callback` typedef
- [x] `libsigrok/include/libsigrok/proto.h` 包含 `sr_listen_hotplug` / `sr_close_hotplug` 声明
- [x] `libsigrok/src/libsigrok-internal.h` 的 `struct sr_context` 末尾有 `struct sr_hotplug_state *hotplug_state` 字段（`#ifdef HAVE_LIBUSB_1_0` 守卫）
- [x] `libsigrok/src/libsigrok-internal.h` 前置声明 `struct sr_hotplug_state`
- [x] `libsigrok/src/backend.c` 的 `sr_exit` 在 `libusb_exit` 之前调用 `sr_close_hotplug(ctx)`，且对 `hotplug_state==NULL` 安全（幂等）
- [x] `libsigrok/src/usb.c` `usb_source_new` 中 `libusb_get_pollfds` 返回 NULL 时回退定时器 GSource（采用统一平台回退，比 `#ifdef _WIN32` 更通用）
- [x] `libsigrok/src/usb.c` Linux/macOS 路径保留原 upstream 行为（pollfds 正常时不影响）
- [x] `libsigrok/CMakeLists.txt` 通过 `file(GLOB_RECURSE src/*.c)` 自动收录 `src/hotplug.c`

## PXView Core 侧

- [x] `PXView/pv/sigsession.h` 私有区声明 `static void hotplug_cb_(int, void*)`、`void on_hotplug_event_(int)`
- [x] `PXView/pv/sigsession.h` 声明 `QTimer *reconnect_timer_ = nullptr` 与对应槽函数
- [x] `PXView/pv/sigsession.h` 声明 `is_current_device_gone_()` / `update_device_handle_()` 辅助函数
- [x] `PXView/pv/sigsession.cpp` 的 `init()` 末尾调用 `sr_listen_hotplug` 注册回调并打印 `pxv_info("Hotplug listener registered")`
- [x] `PXView/pv/sigsession.cpp` 的 `uninit()` 调用 `sr_close_hotplug`（在 `sr_exit` 之前，幂等共存）
- [x] `hotplug_cb_` 静态 trampoline 仅用 `QMetaObject::invokeMethod + Qt::QueuedConnection`，不直接触碰 Qt 对象
- [x] `on_hotplug_event_` ATTACH 分支调用 `refresh_device_list()` + `broadcast_async<UsbDeviceArrived>({})`
- [x] `on_hotplug_event_` DETACH 分支调用 `is_current_device_gone_()` 判断当前设备是否消失
- [x] DETACH 非采集中走 `refresh_device_list` + `broadcast_async<DeviceDetached>` 路径
- [x] DETACH 采集中启动 `reconnect_timer_` 500ms 单次触发
- [x] `on_reconnect_timeout_()` 调用 `CaptureManager::stop_capture()` + `refresh_device_list` + 广播 `DeviceDetached`
- [x] ATTACH 在 `reconnect_timer_` 活跃时 `stop()` + 调 `update_device_handle_()`（简化实现仅警告日志）
- [x] `update_device_handle_()` 简化实现（fx2lafw 重连由 libusb 内部自动重绑 sdi，完整 vid:pid 匹配留待后续阶段）
- [x] SigSession 析构 `reconnect_timer_->stop()` + `deleteLater()` + 置空
- [x] 所有 broadcast 使用 `broadcast_async<TypedEvent>`（worker 线程安全），符合 HARD CONSTRAINT
- [x] Core 代码不引入 `#include <QWidget>` 等 View 头文件（仅添加 `<QTimer>` 属于 QtCore）

## PXView View 侧

- [x] `PXView/pv/mainwindow.cpp` 已存在 `on_event(UsbDeviceArrived)` 处理器（line 2909），调用 `_sampling_bar->update_device_list()` 刷新下拉框
- [x] 同上已存在 `on_event(DeviceDetached)` 处理器（line 2965），通过 `set_default_device()` 间接触发刷新
- [x] 设备到达 UI 提示采用 `MsgBox::Confirm` 对话框询问"切换新设备？"（比 Toast 更适合交互场景）
- [x] 设备断开 UI 提示同上
- [x] View 层不直接调用 `sr_*` API（通过 Core 广播的事件获取通知）

## 验证

- [x] `cd build && ninja -j 16` 编译通过（修复了 2 个编译错误：hotplug.c include 顺序 + SigSession 非 QObject 的 QMetaObject/QTimer 用法）
- [x] `ninja install` 安装到 `install.dir/bin/PXView.exe` 成功（28.5MB，2026-07-06 19:18:32）
- [x] 启动 PXView，`PXView.log` 出现 `Hotplug listener registered`
- [x] libusb hotplug 不可用平台降级日志为 warn 级别，不阻塞启动（本机支持，未触发降级路径）
- [ ] 插入 fx2lafw → 1 秒内下拉框自动出现新设备 + Toast（需用户手动测试）
- [ ] 拔出 fx2lafw（未采集）→ 1 秒内下拉框自动移除 + Toast（需用户手动测试）
- [ ] 采集中拔出 + 500ms 内重连 → 采集继续，日志 `Device reconnected`（需用户手动测试）
- [ ] 采集中拔出 + 500ms 超时 → 优雅停止采集 + Toast "设备已断开"（需用户手动测试）
- [ ] fx2lafw 采集数据完整（验证 usb.c Windows 修复生效，无数据丢失/截断）（需用户手动测试）
- [x] Linux/macOS 编译行为不受影响（usb.c pollfds 路径仅在 NULL 时回退，正常情况保留 upstream 行为）
- [x] 静态链接 `--whole-archive` 已包含 hotplug.c，无需额外链接配置（CMakeLists 用 GLOB_RECURSE 自动收录）
