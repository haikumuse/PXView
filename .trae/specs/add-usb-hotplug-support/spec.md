# USB 设备热插拔支持 Spec

## Why

当前 PXView 1.5.0 使用 upstream libsigrok 0.6.0，丢失了 v1.49 fork libsigrok 提供的两个关键能力：

1. **Windows USB 采集失败**：`libusb_get_pollfds()` 在 Windows WinUSB 后端永远返回 NULL，导致 `usb_source_new` 返回 NULL，fx2lafw 等 USB 驱动无法建立 GSource 事件源，采集失败。
2. **无热插拔支持**：用户插入/拔出 USB 设备必须手动点击"刷新设备列表"，v1.49 的事件驱动能力丢失。`events.h` 已预留 `UsbDeviceArrived`/`DeviceDetached` 事件类型但无任何代码广播它们。

本 spec 通过扩展 upstream libsigrok 源码（新增 hotplug.c 模块，不恢复 fork `ds_*` API），并集成到 PXView Core/View 层，一次性实现阶段 1（基础热插拔）和阶段 2（重连容忍 + 优雅停止）。

## What Changes

### libsigrok 侧
- **新增 `libsigrok/src/hotplug.c`**：实现 `sr_listen_hotplug`/`sr_close_hotplug` upstream API，内部用 `libusb_hotplug_register_callback` + 独立 GThread 调用 `libusb_handle_events_timeout` 唤醒回调。
- **`libsigrok/include/libsigrok/libsigrok.h`**：追加 `enum sr_hotplug_event`（`SR_HOTPLUG_ATTACH`/`SR_HOTPLUG_DETACH`）与 `sr_hotplug_callback` typedef。
- **`libsigrok/include/libsigrok/proto.h`**：追加 `sr_listen_hotplug`/`sr_close_hotplug` 声明。
- **`libsigrok/src/libsigrok-internal.h`**：`struct sr_context` 追加 `hotplug_state` 字段 + 前置声明。
- **`libsigrok/src/backend.c`**：`sr_exit` 追加 `sr_close_hotplug` 调用，确保退出时停止 hotplug 线程。
- **`libsigrok/src/usb.c`**：`usb_source_new` 中 `libusb_get_pollfds` 返回 NULL 时，在 `#ifdef _WIN32` 包裹下回退到纯定时器 GSource（Linux/macOS 保留原 upstream 行为）。
- **`libsigrok/CMakeLists.txt`**：将 `src/hotplug.c` 加入源文件列表。

### PXView Core 侧
- **`PXView/pv/sigsession.h`**：声明静态 trampoline `hotplug_cb_`、主线程槽 `on_hotplug_event_`、重连定时器 `reconnect_timer_`。
- **`PXView/pv/sigsession.cpp` `init()`**：末尾调用 `sr_listen_hotplug` 注册回调。
- **`PXView/pv/sigsession.cpp` `deinit()`**：调用 `sr_close_hotplug` 清理。
- **`PXView/pv/sigsession.cpp`**：实现静态 trampoline（`QMetaObject::invokeMethod` + `Qt::QueuedConnection` 转发到主线程）。
- **`PXView/pv/sigsession.cpp`**：`on_hotplug_event_` ATTACH 路径 → `refresh_device_list()` + 广播 `UsbDeviceArrived`。
- **`PXView/pv/sigsession.cpp`**：`on_hotplug_event_` DETACH 路径 → 判断当前设备是否消失 → 若采集中则启动 500ms 重连定时器 → 超时则 `CaptureManager::stop_capture()` + 广播 `DeviceDetached`。
- **`PXView/pv/sigsession.cpp`**：阶段 2 重连容忍 —— ATTACH 事件在定时器活跃时取消优雅停止并尝试 `update_device_handle_` 复用 sdi。

### PXView View 侧
- **`PXView/pv/mainwindow.cpp` 或 DeviceDock**：订阅 `UsbDeviceArrived`/`DeviceDetached` 事件刷新设备下拉框 + Toast 通知。

### 不做的事
- 不恢复 fork libsigrok `ds_*` API。
- 不修改 MV 架构。
- 不实现 v1.49 的 `process_attach_event` 全套设备分类逻辑（只做必要 refresh + UI 通知）。
- 不修改其他驱动（demo、serial 等）行为。
- 不实现多 Tab 热插拔竞态的设备事务命令队列（Qt 已通过 `Qt::QueuedConnection` 序列化到 GUI 线程，竞态天然消除）。

## Impact
- **Affected specs**: 无直接关联 spec（全新功能）。`fix-state-sync-gaps-v2` 提到 `DeviceDetached` 事件无人接收的问题，本 spec 修复了事件源缺失的根因，使其可达。
- **Affected code**:
  - libsigrok: `src/hotplug.c`(新)、`src/usb.c`、`src/backend.c`、`src/libsigrok-internal.h`、`include/libsigrok/libsigrok.h`、`include/libsigrok/proto.h`、`CMakeLists.txt`
  - PXView Core: `PXView/pv/sigsession.h`、`PXView/pv/sigsession.cpp`
  - PXView View: `PXView/pv/mainwindow.cpp` 或 DeviceDock（实施时定位）

## ADDED Requirements

### Requirement: libsigrok Hotplug API
libsigrok SHALL 暴露 `sr_listen_hotplug(ctx, cb, user_data)` 与 `sr_close_hotplug(ctx)` upstream API，封装 `libusb_hotplug_register_callback` 注册 ATTACH|DETACH 事件回调，并在独立 GThread 中调用 `libusb_handle_events_timeout` 唤醒回调。

#### Scenario: 平台支持 hotplug
- **WHEN** 调用 `sr_listen_hotplug` 且 `libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)` 为 true
- **THEN** 注册 libusb 回调成功，启动后台 GThread，返回 `SR_OK`

#### Scenario: 平台不支持 hotplug
- **WHEN** `libusb_has_capability` 返回 false（旧版 libusb）
- **THEN** `sr_listen_hotplug` 返回 `SR_ERR`，调用方静默回退到手动刷新模式，仅日志告警

#### Scenario: 重复调用
- **WHEN** `sr_listen_hotplug` 在已监听状态下再次调用
- **THEN** 返回 `SR_OK` 并打印 warn 日志，不重复注册

#### Scenario: 退出清理
- **WHEN** 调用 `sr_exit`
- **THEN** 先调用 `sr_close_hotplug` 停止 hotplug 线程并 `g_thread_join`，再调用 `libusb_exit`，避免 use-after-free

### Requirement: USB 设备自动检测
PXView SHALL 在启动时通过 `sr_listen_hotplug` 注册热插拔回调，当 USB 设备插入/拔出时自动刷新设备列表。

#### Scenario: 设备插入
- **WHEN** USB 设备（如 fx2lafw）插入
- **THEN** 1 秒内设备下拉框自动出现新设备，无需用户手动刷新

#### Scenario: 设备拔出（非采集中）
- **WHEN** USB 设备拔出且当前未采集
- **THEN** 1 秒内设备下拉框自动移除该设备

#### Scenario: libusb hotplug 不可用
- **WHEN** `sr_listen_hotplug` 返回 `SR_ERR`（平台不支持）
- **THEN** PXView 启动日志打印 warn 但不阻塞，用户仍可手动刷新设备列表

### Requirement: 采集中的设备被拔出处理
当采集中当前设备被拔出时，PXView SHALL 启动 500ms 重连宽限期；超时未重连则优雅停止采集并通知用户。

#### Scenario: 采集中拔出且超时未重连
- **WHEN** 当前设备采集过程中被拔出
- **AND** 500ms 内未收到该设备的 ATTACH 事件
- **THEN** 调用 `CaptureManager::stop_capture()` 优雅停止采集，广播 `DeviceDetached`，Toast 提示"设备已断开"

#### Scenario: 采集中拔出但在 500ms 内重连
- **WHEN** 当前设备采集过程中被拔出
- **AND** 500ms 内收到该设备的 ATTACH 事件
- **THEN** 取消优雅停止，尝试 `update_device_handle_` 复用 sdi，采集继续，日志打印"Device reconnected"

#### Scenario: 非当前设备拔出
- **WHEN** 拔出的设备不是当前选中设备
- **THEN** 仅刷新设备列表，不影响采集

### Requirement: Windows USB 事件源兼容
libsigrok SHALL 在 Windows 平台当 `libusb_get_pollfds` 返回 NULL 时回退到纯定时器 GSource，而非直接返回 NULL 导致采集失败。

#### Scenario: Windows 上 pollfds 不可用
- **WHEN** `libusb_get_pollfds` 在 Windows 返回 NULL
- **THEN** `usb_source_new` 不返回 NULL，而是创建基于 `usource->timeout_us` 的定时器 GSource
- **AND** `usb_source_dispatch` 仍调用 `receive_data` → `libusb_handle_events_timeout` 收割 USB 传输完成事件
- **AND** fx2lafw 采集数据完整

#### Scenario: Linux/macOS 行为不变
- **WHEN** 在 Linux/macOS 上 `libusb_get_pollfds` 返回 NULL
- **THEN** 保留 upstream 原行为（打印错误并返回 NULL），`#ifdef _WIN32` 包裹确保不污染其他平台

### Requirement: View 层热插拔通知
PXView View 层 SHALL 订阅 `UsbDeviceArrived`/`DeviceDetached` 事件并刷新设备下拉框，同时通过 Toast 提示用户。

#### Scenario: 设备到达提示
- **WHEN** 收到 `UsbDeviceArrived` 事件
- **THEN** 设备下拉框刷新，Toast 显示"USB 设备已连接"

#### Scenario: 设备断开提示
- **WHEN** 收到 `DeviceDetached` 事件
- **THEN** 设备下拉框刷新，Toast 显示"设备已断开"

## MODIFIED Requirements

### Requirement: SigSession::init / deinit 生命周期
原 `init()` 仅调用 `refresh_device_list()`；现追加 `sr_listen_hotplug` 注册。
原 `deinit()` 仅清理 session；现追加 `sr_close_hotplug` 停止 hotplug 线程。

### Requirement: libsigrok sr_exit 顺序
原 `sr_exit` 直接调用 `libusb_exit`；现先调用 `sr_close_hotplug` 确保 hotplug 线程 join 完成，避免 libusb context 在线程未退出时被释放。

## REMOVED Requirements

### Requirement: PXView 1.49 fork `ds_*` hotplug API
**Reason**: AGENTS.md HARD CONSTRAINT 禁止恢复 fork API，Core 层只调用 upstream `sr_*`。
**Migration**: 全新 `sr_listen_hotplug`/`sr_close_hotplug` upstream API 替代，命名与签名遵循 upstream libsigrok 风格。
