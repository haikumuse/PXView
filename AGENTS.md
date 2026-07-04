# AGENTS.md

## Project Overview

**PXView** (binary: `PXView.exe`) is a Qt6 C++17 application for signal analysis with logic analyzers/oscilloscopes, forked from sigrok/PulseView. Supports DreamSourceLab/PXLogic hardware. GPLv3+. Version 1.5.0.

Four compiled components: `libsigrok/` (C11 hardware drivers), `libsigrokdecode/` (C11+Python decoder engine, 215 C decoder DLLs), `common/` (C utilities), `PXView/` (Qt6 app).

## Build

```
./build_incremental.cmd          # Windows (MSYS2 + CMake/Ninja) → install.dir/bin/PXView.exe
```
- Assume every terminal command is executed by PowerShell.Do not generate CMD commands even if running on Windows.
- `build_incremental.cmd`/`.sh` **launches the app after building**. For compile-only (no launch), run the inner commands directly from `build_incremental.sh`: `cd build && ninja -j 16 && ninja install`.
- MinGW64 toolchain (gcc/g++/ninja/cmake) is already in the system PATH — mingw commands can be invoked directly from any shell without sourcing msys2_shell.
- Headless: `PXView.exe --headless` — runs without GUI (QCoreApplication), exposes MCP API on port 10110.
- Options: `ENABLE_COMPAT_DRIVERS` (OFF), `BUILD_DECODER_TEST` (OFF).
- Deps: Qt6.6+, glib, Python3, FFTW, libusb, zlib, Boost, nlohmann_json.
## Build Rules

The build directory is already configured.

Do NOT verify:

- build.ninja
- CMakeCache.txt
- ninja existence
- cmake existence
- compiler existence

Assume the environment is valid.

For compile-only execute exactly:

cd build
ninja -j 16
ninja install

Do not prepend any probing commands.
## Architecture: Core/View Layer Separation

The app is split into two compile-time layers, enforced by CMake (`PXVIEW_CORE_SOURCES` vs `PXVIEW_GUI_SOURCES` in `CMakeLists.txt`):

- **`pxview-core`** (static lib) — session orchestration, data layer, remote-control API. Links Qt6::Core/Gui/Network/Concurrent/WebSockets only — **must NOT depend on Qt::Widgets/Svg**, so it runs headless.
- **PXView executable** (View layer) — links `pxview-core` + Qt6::Widgets/Svg. Contains `main.cpp`, `MainWindow`, `View`, docks, toolbars, dialogs, widgets.

**MV pattern** — Core owns data models, View owns rendering objects, bridged by `SignalFactory`:

| Core (no Qt Widgets) | View (Qt Widgets) | Bridge |
|----------------------|-------------------|--------|
| `SignalModel` | `view::Signal` → Logic/Analog/Dso | `SignalFactory::create_signal()` |
| `DecoderStack` | `view::DecodeTrace` | `View::add_decoder()` creates trace after Core returns stack |
| `SpectrumStack`/`MathStack`/`LissajousModel` | `SpectrumTrace`/`MathTrace`/`LissajousTrace` | View creates from stack/model |

**Key rules (enforced):**

1. Core code must NOT `#include` QWidget/QMainWindow/QDialog or any `pv/view/*.h`.
2. `SigSession` holds `SignalModel*`/`DecoderStack*` (Core), never `view::Signal*`/`view::Trace*`.
3. No intermediate Model classes (DecodeModel/SpectrumModel/MathModel were removed — View reads Core stacks directly via `DataSource`).
4. Decoder popups (`DecoderOptionsDlg` via `DecodeTrace::create_popup()`) are shown by the View BEFORE starting decode tasks. Core's `add_decoder()`/`rst_decoder()` never start tasks themselves.
5. `DataSource` interface returns Core types only (`SignalModel*`/`DecoderStack*`/…) — lets API layer operate headless.

**Decode task lifecycle:** `SigSession::reload()` recreates SignalModels with NULL snapshot pointers. Before starting decode tasks, call `attach_data_to_signal(_view_data)`. All decode-start paths funnel through `SigSession::start_all_decode_tasks()` (does the attach first): `restart_decoders()`, `RevEndPacket` event, `CopyToDocDone` event. `rst_decoder()` also attaches before its single task. Missing this → "没有设置需要解码哪些通道的数据".

**Channel type model:** `SignalModel::type()` returns `int` holding the libsigrok `SR_CHANNEL_*` value (LOGIC=10000/DSO=10001/ANALOG=10002) — single source of truth. The MCP/API contract type `api::ChannelType` (Logic=0/Analog=1/Dso=2/Unknown=99) is converted at the `SessionService` boundary via `sr_channel_type_to_api()`. Do NOT introduce new conversion functions; do NOT compare `model->type()` against `api::ChannelType` values.

## Key Files

| File | Purpose |
|------|---------|
| `PXView/main.cpp` | Entry point, `--headless` branch |
| `PXView/pv/sigsession.h` | Coordination facade (Core, 299 lines) — holds 6 manager `unique_ptr`s, public methods forward to managers |
| `PXView/pv/core/eventbus.h` | Central dispatch hub — typed event bus. `broadcast<T>()` SYNC, `broadcast_sync<T>()` SYNC direct (pre-broadcast ordering), `broadcast_async<T>()` ASYNC (`Qt::QueuedConnection` on `qApp`). Legacy `broadcast_msg`/`trigger_message`/`IMessageListener` fully REMOVED |
| `PXView/pv/core/capturemanager.h` | Capture lifecycle (start/stop/exec/exit), 6 DsTimers, collect mode, repeat/refresh logic |
| `PXView/pv/core/decodetaskmanager.h` | Decode thread pool, `add_decode_task`/`start_all_decode_tasks`/`rst_decoder` |
| `PXView/pv/core/datafeedparser.h` | libsigrok datafeed callback trampoline + `feed_in_*` methods |
| `PXView/pv/core/documentregistry.h` | SessionDocument list + `CaptureOwnerGuard` RAII + copy thread |
| `PXView/pv/core/filterprocessor.h` | Glitch filter + signal invert background threads |
| `PXView/pv/data/signalconfigstore.h` | Channel/Signal config structs + save/apply/json serialization (extracted from SessionDocument) |
| `PXView/pv/data/sessiondocument.h` | Pure data document (snapshots, decoder stacks, signal models, trigger config) |
| `PXView/pv/data/signalmodel.h` | Core channel model (no QObject/QWidget) |
| `PXView/pv/data/logicsnapshot.h` | Logic snapshot storage/query/diagnostics/loop-mode (DiskCacheWriter + GlitchFilter extracted to leaf modules) |
| `PXView/pv/data/logicsnapshot_diskcache_writer.h` | Disk cache async writer extracted from LogicSnapshot God class (~600 lines, friend back-pointer to LogicSnapshot) |
| `PXView/pv/data/logicsnapshot_glitch_filter.h` | Glitch filter extracted from LogicSnapshot (~505 lines, friend back-pointer to LogicSnapshot; orphaned set_sample_range/clone_data deleted) |
| `PXView/pv/view/view.h` | View container — owns rendering objects, drives decoder popup |
| `PXView/pv/view/signalfactory.h` | Bridge: SignalModel → view::Signal (no `ui_state` param — feature was never implemented) |
| `PXView/pv/interface/icallbacks.h` | Split callbacks: IDataCallback/ICaptureCallback/ITriggerCallback/ISessionStateCallback (legacy `IMessageListener` + `DSV_MSG_*` macros REMOVED) |
| `PXView/pv/interface/events.h` | 45 typed event structs + `IEventListener` + `broadcast<T>()`/`broadcast_sync<T>()`/`broadcast_async<T>()` (SOLE dispatch mechanism — HARD CONSTRAINT) |
| `PXView/pv/tabcontext.h` | Per-tab View/Session/Document binding |
| `PXView/pv/api/rpc_dispatcher.cpp` | JSON-RPC + MCP tool schemas |
| `PXView/pv/data/datasource.h` | Core→View/API bridge interface — `device()` accessor is the V3-blessed migration target for View-layer device access (replaces `session.get_device()`) |
| `PXView/pv/deviceagent.h` | DeviceAgent typed wrappers: `is_roll_mode`/`get_channel_count`/`get_unit_bits`/`get_probe_vdiv`/`get_probe_vdiv_list`/... — push libsigrok.h dependence into Core, let View drop the include |
| `CMakeLists.txt` | `PXVIEW_CORE_SOURCES` vs `PXVIEW_GUI_SOURCES` define layer boundary; 3 leaf libraries extracted (`pxview-interface` INTERFACE, `pxview-utility` STATIC, `pxview-config` STATIC) |

## Conventions

- `ds_*` libsigrok API; `srd_*` libsigrokdecode API; typed events in `pv::interface` namespace (e.g. `CaptureStateChanged`, `DataUpdated`).
- Singletons: `AppControl`, `AppConfig`, `SessionManager`.
- JSON config: `.pxc` session files, `lang/` translations.
- `assert()` is a no-op in Release — use explicit `if(!ptr)` checks.
- New C decoder: create `libsigrokdecode/c_decoders/<name>_c.c`, add to `C_DECODERS` in CMakeLists.txt, rebuild.
- ISessionCallback was split into 4 sub-interfaces (IDataCallback/ICaptureCallback/ITriggerCallback/ISessionStateCallback) — no backward-compat shim.
- **View-layer device access:** use `_view.data_source()->device()` (returns `DeviceAgent*`, may be null) — do NOT call `session.get_device()` / `session().get_device()` from View code. Always null-check the returned pointer.
- **DeviceAgent typed wrappers:** prefer `device->is_roll_mode(v)` / `device->get_channel_count()` / `device->get_probe_vdiv(v, ch)` / `device->get_probe_vdiv_list()` over raw `get_config_*(SR_CONF_*, ...)` — pushes libsigrok.h dependence into Core so View files can drop the include.
- **CMake leaf libraries:** `pv/interface/` (INTERFACE), `pv/utility/` (STATIC), `pv/config/` (STATIC), `pv/data/` (STATIC, `pxview-data`) are extracted as independent targets linked PUBLIC by `pxview-core`. New leaf-utility code should land in the appropriate leaf module, not in `core_sources.cmake`.

## State Sync Conventions

- **GUI thread marshal:** `ICaptureCallback`/`IDataCallback` methods and `IEventListener::on_event` overrides re-invoke onto `qApp->thread()` via `Qt::QueuedConnection` before touching any QWidget — Core worker threads (feed/device/copy) call them synchronously. The legacy `MainWindow::OnMessage` path is REMOVED.
- **Typed event bus (SOLE dispatch mechanism — HARD CONSTRAINT):** `IEventListener` + `broadcast<T>()` / `broadcast_sync<T>()` / `broadcast_async<T>()` (defined in `interface/events.h`) is the ONLY event dispatch path. 45 typed event structs carry full context. Three dispatch modes:
  - `broadcast<T>()` — SYNC, called from within an already-main-thread context (e.g. from inside another on_event handler). Has `thread_local _broadcast_depth` loop guard (depth>1 → assert + log + short-circuit).
  - `broadcast_sync<T>()` — SYNC direct dispatch, used for PRE-broadcast ordering events (`StoreConfPrev` / `CurrentDeviceChangePrev` / `StartCollectWorkPrev` / `EndCollectWorkPrev`) that MUST run synchronously BEFORE the state mutation. Callers MUST be on the main thread.
  - `broadcast_async<T>()` — ASYNC, queues onto `qApp`'s event loop via `Qt::QueuedConnection`. Used by worker threads (e.g. `DataFeedParser::feed_in_*` emitting `DataUpdated`). Event is captured BY VALUE (copy) so it survives the caller's stack frame.
  The legacy `IMessageListener` / `DSV_MSG_*` / `broadcast_msg` / `trigger_message` / `OnMessage` infrastructure has been FULLY REMOVED. There is no int-msg dispatch anywhere. Each `on_event(const TypedEvent&)` override is self-contained (no switch on msg code).
- **Broadcast on state change:** any Core/View mutation that downstream layers track MUST broadcast a typed event (or `ServiceEvent` for MCP/WS). Broadcast only at user-interaction entry points, never from rebuild/restore paths (avoids loops).
- **SignalModel wholesale rebuild sync:** `init_signals()`/`reload()` replace `_signal_models` with new `shared_ptr` objects (old ones freed → `0xfeeefeee`). Both MUST end with `signals_changed()` so `compute_change_event` detects pointer-identity change → `AllReplaced` → View rebinds `view::Signal::_model`. `switch_work_mode`/`set_device` emit `broadcast_async<DeviceModeChanged>` / `broadcast_async<CurrentDeviceChanged>` and the handlers run AFTER the synchronous View rebuild.
- **`_capture_owner_document` lifecycle (CaptureOwnerGuard RAII):** managed by `CaptureOwnerGuard` (now in `core/DocumentRegistry`, extracted from `sigsession.h`). `start_capture` calls `_document_registry->acquire_capture_owner(doc)` which constructs `std::unique_ptr<CaptureOwnerGuard>` setting owner + `_is_working=true` + broadcasting `CaptureOwnerChanged{new_owner}`. `stop_capture`/`clear_capture_owner_document` call `release_capture_owner()` which resets the guard, joining copy thread + clearing owner + `_is_working=false` + broadcasting. No manual `join_copy_thread()`/owner clearing — guard destructor handles all. Repeat mode: guard persists across `CopyToDocDone` frames; only `stop_capture` or Tab close triggers destruction.
- **Trigger config single source of truth:** Core `SigSession::_trigger_config` (`data::TriggerConfig`) is the ONLY source. `TriggerDock::commit_trigger()` and `SessionService` MCP path write Core only — NO direct `ds_trigger_*` calls. `SigSession::sync_trigger_to_libsigrok()` is the single Core→libsigrok sync point, called inside `exec_capture()` before `_device_agent.start()` (handles Simple/Adv/Serial). `is_trigger_preconfigured` flag removed (no longer needed). Broadcast `TriggerConfigChanged` on `set_trigger_config()`.

## Remote Control API (MCP)

MCP on port 10110 (HTTP POST, JSON-RPC 2.0). WebSocket on 10430. `SessionService` operates on Core objects (SignalModel/DecoderStack) — works headless. MCP debug log: `%TEMP%/pxview_mcp_debug.log` (shows decoder stack pointer addresses for `get_analyzer_results`).

Flow: `get_devices` → `add_analyzer` → `start_capture` → `wait_capture` → `get_capture_status` → `get_analyzer_results` → `export_raw_data_csv`.
