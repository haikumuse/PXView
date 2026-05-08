# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

DSView (binary name: `PXView`) is a Qt5 C++ GUI for signal analysis with logic analyzers, oscilloscopes, and similar instruments. It is forked from the [sigrok](https://sigrok.org) PulseView project and supports DreamSourceLab/PXTOOL hardware devices. Licensed GPLv3+.

## Build Commands

### Windows (MSYS2/MinGW-w64, the primary dev platform)

```bash
# Full (clean) build
build_full.cmd

# Incremental build (after first full build)
build_incremental.cmd
```

Both scripts invoke MSYS2 from `D:\msys64`, configure with CMake+Ninja in `build/`, output binaries to `build.dir/`, and install to `install.dir/`.

### Linux/macOS

```bash
mkdir build && mkdir install.dir && cd build
cmake .. -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_INSTALL_PREFIX=../install.dir
ninja -j 8
ninja install
```

Tests are disabled by default. Enable with `-DENABLE_TESTS=TRUE` in CMake. Debug build with `-DCMAKE_BUILD_TYPE=Debug`.

### Dependencies

Qt5 (or Qt6 on macOS), glib-2.0, libusb-1.0, boost >= 1.42, fftw3, python3, zlib, pkg-config. See `windows.md` for MSYS2 package install commands.

## Repository Architecture

The project has four main components compiled into a single binary, plus separate C decoder DLLs:

### 1. `libsigrok4DSL/` — Hardware driver layer (C)
Low-level library for USB/DSL device enumeration, data capture, and session control. Based on libsigrok. Key files: `session_driver.c` (main capture logic), `lib_main.c` (API entry), `hardware/DSL/` (DSLogic/DSCope drivers), `hardware/pxlogic/` (PXLogic driver).

### 2. `libsigrokdecode4DSL/` — Protocol decoder engine (C + Python)
Engine that loads and runs protocol decoders. Two decoder types:
- **Python decoders** in `decoders/` (the original sigrok decoder format, loaded via embedded Python)
- **C decoders** in `c_decoders/` — native C decoders (spi_c, i2c_c, uart_c, can_c) compiled as separate DLLs/SOs via `SRD_C_DECODER_DLL`. The file `c_decoder_api.c` is dual-purpose: it compiles into both the main binary (for in-process decoder management) and into each C decoder DLL (provides the API the decoder calls into).

Key files: `srd.c` (session init, decoder loading, DLL loading for C decoders), `decoder.c` (decoder list loading), `instance.c` (decoder instance lifecycle), `session.c` (decode session with callbacks).

### 3. `common/` — Shared utilities (C)
`minizip/` (zip/unzip for session files) and `log/xlog.c` (logging).

### 4. `DSView/` — Qt5 C++ GUI application

**Application entry and lifecycle:**
- `main.cpp` → `AppControl` (singleton, owns the global `SigSession`) → creates `MainFrame` which holds `MainWindow`

**Window system (multi-window with tab detach):**
- `MainFrame` (`pv/mainframe.cpp`) — borderless top-level QFrame with custom title bar, resize borders; uses `WinNativeWidget` on Windows for native window behavior
- `MainWindow` (`pv/mainwindow.cpp`, ~100KB) — the central QMainWindow with QRibbon menu system, toolbars, dock widgets, and `DraggableTabWidget`. Implements `ISessionCallback` to receive data/status events from `SigSession`
- `SubMainFrame` (`pv/submainframe.cpp`) — independent window for detached tabs
- `ui/DraggableTabWidget` / `ui/DraggableTabBar` — custom tab widget supporting drag-out to create independent `SubMainFrame` windows

**Session and tab architecture (the most important abstraction):**
- `TabContext` — binds together a `View` (rendering), `SigSession` (data source), and `SessionDocument` (data storage). Has LIVE (capturing) and HISTORICAL (reviewing saved data) states. One per tab.
- `SessionManager` — singleton tracking all `TabContext` instances (attached and detached)
- `SigSession` — central orchestrator. Controls device capture, receives data callbacks from `libsigrok4DSL`, manages double-buffered `SessionData` (capture vs view), dispatches decode tasks to worker threads
- `DeviceAgent` — wrapper around `libsigrok4DSL` device handles, provides typed get/set config methods

**Data layer (`pv/data/`):**
- `Snapshot` → `LogicSnapshot`, `AnalogSnapshot`, `DsoSnapshot` — raw captured data storage
- `SessionDocument` — owns snapshots + decode traces; implements `DataSource` interface. One per tab, used for both live and historical data. Supports signal config save/restore
- `DecoderStack` — manages protocol decoder instances for a channel
- `MathStack`, `SpectrumStack` — math/FFT processing on DSO data
- `DecoderModel` — model for the decoder list UI

**View layer (`pv/view/`):**
- `View` — top-level scrollable, zoomable container; owns all traces
- `Viewport` — QAbstractScrollArea subclass; the main rendering surface that paints all traces, ruler, cursors, etc.
- `Trace` (abstract base) → `Signal` → `LogicSignal`, `AnalogSignal`, `DsoSignal` (direct hardware channels)
- `DecodeTrace` — protocol decoder annotation visualization
- `MathTrace`, `SpectrumTrace`, `LissajousTrace` — derived data views
- `Ruler`, `Header`, `Cursor`, `XCursor`, `TimeMarker` — view overlays

**Dock widgets (`pv/dock/`):**
Side panels: `ProtocolDock` (decoder list), `TriggerDock` (logic trigger config), `DsoTriggerDock` (oscilloscope trigger), `MeasureDock`, `SearchDock`, `DeviceOptionsDock`

**Key interfaces (`pv/interface/icallbacks.h`):**
- `ISessionCallback` — MainWindow listens to SigSession for data updates, triggers, errors
- `IMessageListener` — broadcast message system (device events, config changes) using `DSV_MSG_*` integer codes
- `DataSource` (`pv/data/datasource.h`) — abstract interface for providing signal data, implemented by both `SigSession` and `SessionDocument`
- `IDecoderPannel` — callback for decoder UI name updates

**Configuration (`pv/config/appconfig.h`):**
`AppConfig` singleton — app options, dock layout, frame geometry, user history, protocol export formats. Config stored as JSON in `%APPDATA%/DSView/`.

**Internationalization:**
Language files in `lang/cn/` and `lang/en/` as JSON. `pv/ui/langresource.cpp` handles loading.

## Theme System

CSS-based themes in `DSView/themes/` (breeze light/dark). Switched via `MainWindow::switchTheme()`.

## C Decoder DLL System

C decoders are native shared libraries compiled separately from the main binary. The CMake at the bottom of `CMakeLists.txt` builds each decoder as a `MODULE` library to `build.dir/decoders/c_decoders/`. At runtime, `libsigrokdecode4DSL/srd.c` loads these DLLs dynamically. The `c_decoder_api.c` file provides the bridge — when compiled with `SRD_C_DECODER_DLL` it exports the API for decoder DLLs; without it, it provides in-process decoder management functions.

## Language Standard

- C++11 (`-std=c++11`)
- C99 (`-std=c99`)
