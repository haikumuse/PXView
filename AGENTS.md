# AGENTS.md

## Project Overview

**PXView** (binary: `PXView.exe`) is a Qt6 C++17 application for signal analysis with logic analyzers/oscilloscopes, forked from sigrok/PulseView. Supports DreamSourceLab/PXLogic hardware. GPLv3+. Version 1.5.0.

Four compiled components: `libsigrok/` (C11 hardware drivers), `libsigrokdecode/` (C11+Python decoder engine, 215 C decoder DLLs), `common/` (C utilities), `PXView/` (Qt6 app).

## Build

```
./build_incremental.cmd          # Windows (MSYS2 + CMake/Ninja) → install.dir/bin/PXView.exe
```

- Headless: `PXView.exe --headless` — runs without GUI (QCoreApplication), exposes MCP API on port 10110.
- Options: `ENABLE_COMPAT_DRIVERS` (OFF), `BUILD_DECODER_TEST` (OFF).
- Deps: Qt6.6+, glib, Python3, FFTW, libusb, zlib, Boost, nlohmann_json.

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

**Decode task lifecycle:** `SigSession::reload()` recreates SignalModels with NULL snapshot pointers. Before starting decode tasks, call `attach_data_to_signal(_view_data)`. All decode-start paths funnel through `SigSession::start_all_decode_tasks()` (does the attach first): `restart_decoders()`, `DSV_MSG_REV_END_PACKET`, `DSV_MSG_COPY_TO_DOC_DONE`. `rst_decoder()` also attaches before its single task. Missing this → "没有设置需要解码哪些通道的数据".

**Enum pitfall:** `SignalModel::type()` returns `api::ChannelType` (Logic=0/Analog=1/Dso=2), but libsigrok expects `SR_CHANNEL_LOGIC=10000` etc. Convert via `api_type_to_sr_channel_type()`.

## Key Files

| File | Purpose |
|------|---------|
| `PXView/main.cpp` | Entry point, `--headless` branch |
| `PXView/pv/sigsession.h` | Central session class (Core) — heart of data flow |
| `PXView/pv/data/signalmodel.h` | Core channel model (no QObject/QWidget) |
| `PXView/pv/view/view.h` | View container — owns rendering objects, drives decoder popup |
| `PXView/pv/view/signalfactory.h` | Bridge: SignalModel → view::Signal |
| `PXView/pv/interface/icallbacks.h` | Split callbacks: IDataCallback/ICaptureCallback/ITriggerCallback/ISessionStateCallback + DSV_MSG_* codes |
| `PXView/pv/tabcontext.h` | Per-tab View/Session/Document binding |
| `PXView/pv/api/rpc_dispatcher.cpp` | JSON-RPC + MCP tool schemas |
| `PXView/pv/data/datasource.h` | Core→View/API bridge interface |
| `CMakeLists.txt` | `PXVIEW_CORE_SOURCES` vs `PXVIEW_GUI_SOURCES` define layer boundary |

## Conventions

- `ds_*` libsigrok API; `srd_*` libsigrokdecode API; `DSV_MSG_*` broadcast message codes.
- Singletons: `AppControl`, `AppConfig`, `SessionManager`.
- JSON config: `.pxc` session files, `lang/` translations.
- `assert()` is a no-op in Release — use explicit `if(!ptr)` checks.
- New C decoder: create `libsigrokdecode/c_decoders/<name>_c.c`, add to `C_DECODERS` in CMakeLists.txt, rebuild.
- ISessionCallback was split into 4 sub-interfaces (IDataCallback/ICaptureCallback/ITriggerCallback/ISessionStateCallback) — no backward-compat shim.

## Remote Control API (MCP)

MCP on port 10110 (HTTP POST, JSON-RPC 2.0). WebSocket on 10430. `SessionService` operates on Core objects (SignalModel/DecoderStack) — works headless. MCP debug log: `%TEMP%/pxview_mcp_debug.log` (shows decoder stack pointer addresses for `get_analyzer_results`).

Flow: `get_devices` → `add_analyzer` → `start_capture` → `wait_capture` → `get_capture_status` → `get_analyzer_results` → `export_raw_data_csv`.
