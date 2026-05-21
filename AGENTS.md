# AGENTS.md

This file provides guidance to AI coding agents when working with code in this repository.

## Project Overview

**PXView** (binary name: `PXView.exe`) is a Qt6 C++17 GUI application for signal analysis with logic analyzers, oscilloscopes, and similar instruments. It is forked from the [sigrok](https://sigrok.org) PulseView project and supports DreamSourceLab/PXLogic hardware devices. Licensed GPLv3+. Current version: 1.5.0.

The project compiles four components into a single executable, plus 37 separate C decoder DLLs:

1. **`libsigrok/`** — Hardware driver layer (C11): USB/DSL device enumeration, data capture, session control
2. **`libsigrokdecode/`** — Protocol decoder engine (C11 + Python): loads and runs both Python and C protocol decoders
3. **`common/`** — Shared utilities (C): minizip (session file zip/unzip), xlog (logging), shared type definitions
4. **`PXView/`** — Qt6 C++17 GUI application: the main user-facing application

## Build Instructions

### Windows (Primary Development Platform)

**Use `build_incremental.cmd` for incremental builds after the first full build.**

```
build_incremental.cmd
```

Both scripts invoke MSYS2 from `D:\msys64`, configure with CMake+Ninja in `build/`, output binaries to `build.dir/`, and install to `install.dir/`. The final executable is at `install.dir/bin/PXView.exe`.

### Build Configuration

```
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../install.dir
```

### Dependencies

- Qt6 (6.6+) — Core, Widgets, Gui, GuiPrivate, Svg, Concurrent
- glib-2.0
- Python3 (Interpreter + Development)
- FFTW
- libusb-1.0
- zlib
- Boost (1.42+)

## Repository Architecture

### Directory Layout

```
.
├── CMakeLists.txt              # Main CMake build configuration (925 lines)
├── build_full.cmd              # Windows full build script
├── build_incremental.cmd       # Windows incremental build script
├── build_incremental.sh        # Linux incremental build script
├── CMake/                      # CMake find modules (FFTW, libusb)
├── libsigrok/                  # Hardware driver layer (C11)
│   ├── hardware/DSL/           # DSLogic/DSCope device drivers
│   ├── hardware/pxlogic/       # PXLogic device driver
│   ├── hardware/common/        # USB utilities (usb.c, ezusb.c)
│   ├── hardware/demo/          # Demo device driver
│   ├── input/                  # File input formats (binary, VCD, WAV)
│   ├── output/                 # File output formats (CSV, gnuplot, srzip, VCD)
│   └── tests/                  # libsigrok unit tests
├── libsigrokdecode/            # Protocol decoder engine (C11 + Python)
│   ├── c_decoders/             # 37 native C decoders (compiled as DLLs)
│   ├── decoders/               # 220+ Python protocol decoders
│   └── contrib/                # sigrok logo asset
├── common/                     # Shared utilities (C)
│   ├── minizip/                # ZIP compression/decompression
│   ├── log/                    # xlog logging
│   └── ds_types.h              # Shared type definitions
├── PXView/                     # Qt6 C++17 GUI application
│   ├── main.cpp                # Application entry point
│   ├── pv/                     # Main application source
│   │   ├── appcontrol.cpp/h    # Singleton app controller
│   │   ├── mainwindow.cpp/h    # Main window (implements ISessionCallback, IMessageListener, IMainForm, ISessionDataGetter)
│   │   ├── mainframe.cpp/h     # Borderless top-level frame
│   │   ├── sigsession.cpp/h    # Central session orchestrator
│   │   ├── deviceagent.cpp/h   # libsigrok device wrapper (with IDeviceAgentCallback)
│   │   ├── tabcontext.cpp/h    # Per-tab context (View + Session + Document)
│   │   ├── sessionmanager.cpp/h # Tab context registry
│   │   ├── storesession.cpp/h  # Session file save logic
│   │   ├── config/             # AppConfig singleton (JSON settings), shortcut definitions
│   │   ├── data/               # Data layer (snapshots, decoders, math, disk cache)
│   │   ├── dialogs/            # Dialog windows
│   │   ├── dock/               # Dock/side panel widgets
│   │   ├── interface/          # Callback interfaces (ISessionCallback, IContextAware, etc.)
│   │   ├── prop/               # Property bindings
│   │   ├── toolbars/           # Toolbar widgets
│   │   ├── ui/                 # UI utilities (lang, icons, drag tabs, popup dialogs)
│   │   ├── utility/            # General utilities (encoding, path, array)
│   │   ├── view/               # View layer (signals, traces, viewport)
│   │   └── widgets/            # Custom widgets (sliding drawer, sidebar, smooth scroll)
│   ├── themes/                 # Breeze CSS themes (dark.qss, light.qss) with SVG assets
│   ├── icons/                  # SVG icons (dark/light variants)
│   ├── icons_PXView/           # PXView-specific icon set
│   ├── icons_PXView_DS/        # DS-style icon set
│   ├── icons_PXView_Old/       # Legacy icon set
│   ├── languages/              # Qt translation files (.qm)
│   ├── fonts/                  # Application fonts (OPPOSans, SourceCodePro, SourceHanSans)
│   ├── demo/                   # Demo signal files
│   └── res/                    # Firmware files and default configs
├── lang/                       # JSON language files (cn/ with dec/ subdirectory, en/)
├── window/                     # Windows packaging scripts
│   ├── package.sh              # Main packaging script
│   ├── copy-deps.sh            # DLL dependency collector
│   ├── python/                 # Python 3.10 embed package
│   └── workflows/              # GitHub Actions CI (win64.yml)
├── doc/                        # Development documentation (Chinese + English)
└── test_access.cpp             # Root-level test file
```

### Key Architectural Concepts

#### Application Lifecycle

`main.cpp` → `AppControl` (singleton) → creates `MainFrame` → holds `MainWindow`

#### Session and Tab Architecture (Most Important Abstraction)

- **`TabContext`** — Binds together a `View` (rendering), `SigSession` (data source), and `SessionDocument` (data storage). Has LIVE (capturing) and HISTORICAL (reviewing saved data) states. One per tab. Supports `has_data()`, `make_live()`, `activate()`/`deactivate()` lifecycle methods.
- **`SessionManager`** — Singleton tracking all `TabContext` instances
- **`SigSession`** — Central orchestrator: controls device capture, receives data callbacks from `libsigrok`, manages double-buffered `SessionData`, dispatches decode tasks to worker threads. Implements `IMessageListener`, `IDeviceAgentCallback`, and `DataSource`.
- **`DeviceAgent`** — Wrapper around `libsigrok` device handles, provides typed get/set config methods. Notifies config changes via `IDeviceAgentCallback`.

#### Capture Modes

SigSession supports three capture modes via `DEVICE_COLLECT_MODE`:
- `COLLECT_SINGLE` — single capture
- `COLLECT_REPEAT` — repeated capture with hold percentage
- `COLLECT_LOOP` — continuous loop capture with realtime refresh

#### Signal Processing

- **Glitch filter** — removes short pulses below a configurable width from logic signals (`set_glitch_filter()` / `clear_glitch_filter()`)
- **Signal invert** — inverts logic signal polarity (`set_signal_invert()` / `clear_signal_invert()`)
- Both are managed in `SigSession` and broadcast progress/completion via `DSV_MSG_GLITCH_FILTER_*` and `DSV_MSG_SIGNAL_INVERT_*` message codes

#### Window System

- **`MainFrame`** — Borderless top-level QFrame with custom title bar; uses `WinNativeWidget` on Windows
- **`MainWindow`** — Central QMainWindow with QRibbon menu, toolbars, dock widgets, `DraggableTabWidget`. Implements `ISessionCallback`, `IMessageListener`, `IMainForm`, `ISessionDataGetter`.
- **`SubMainFrame`** — Independent window for detached tabs
- **`DraggableTabWidget`/`DraggableTabBar`** — Custom tab widget supporting drag-out to create independent windows
- **`SlidingDrawer`** — Animated slide-in/slide-out panel replacing traditional QDockWidget for side panels
- **`SideBar`/`SideBarButton`** — Vertical icon sidebar for panel navigation

#### Data Layer (`pv/data/`)

- `Snapshot` → `LogicSnapshot`, `AnalogSnapshot`, `DsoSnapshot` — raw captured data storage
- `SessionDocument` — owns snapshots + decode traces; implements `DataSource` interface
- `SessionSnapshot` — implements `DataSource`; supports copying data from Logic/Analog/Dso snapshots and loading from files
- `DecoderStack` — manages protocol decoder instances for a channel
- `MathStack`, `SpectrumStack` — math/FFT processing on DSO data
- `LeafBlockPool` — singleton memory pool for reusing LogicSnapshot leaf blocks, reducing malloc/free overhead

#### Disk Cache Subsystem (`pv/data/`)

A tiered memory+disk caching system for handling large captures that exceed RAM:

- **`DiskCacheConfig`** — configuration constants: 16GB total cache depth (4GB memory / 12GB disk), 256MB read cache, 200MB/s minimum disk speed
- **`DiskBufferManager`** — manages per-channel data files with block-level read/write, index persistence, and disk space checking
- **`DiskWriteThread`** — async disk writer with write queue, speed statistics, and disk-full detection
- **`DiskReadCache`** — LRU read cache supporting per-channel/block-index lookup and eviction

Status is shown in the UI via `_disk_cache_status_label` in MainWindow.

#### View Layer (`pv/view/`)

- `View` — top-level scrollable, zoomable container
- `Viewport` — QAbstractScrollArea subclass; main rendering surface
- `Trace` (abstract) → `Signal` → `LogicSignal`, `AnalogSignal`, `DsoSignal`
- `GroupSignal` — grouped signal visualization
- `DecodeTrace` — protocol decoder annotation visualization
- `MathTrace`, `SpectrumTrace`, `LissajousTrace` — derived data views

#### Callback/Message System (`pv/interface/`)

**Interfaces defined in `icallbacks.h`:**

| Interface | Purpose |
|-----------|---------|
| `ISessionCallback` | 17 callback methods for session events (data updates, triggers, errors, trigger_message, delay_prop_msg) |
| `IMessageListener` | broadcast message system using `DSV_MSG_*` integer codes |
| `IDecoderPannel` | callback for decoder UI name updates |
| `ISessionDataGetter` | generates session data as string |
| `IDlgCallback` | dialog result callback |
| `IMainForm` | main form operations (e.g. switchLanguage) |
| `IParentNativeEventCallback` | native platform event forwarding (e.g. display change) |

**Interface defined in `icontextaware.h`:**

| Interface | Purpose |
|-----------|---------|
| `IContextAware` | binds/unbinds a component to a `TabContext` instance |

**Message code ranges:**

| Range | Category | Examples |
|-------|----------|----------|
| 5001-5008 | Collect lifecycle | START_COLLECT_WORK, COLLECT_START/END, CAPTURE_STATE_CHANGED |
| 6000-6030 | Device & signal processing | DEVICE_LIST_UPDATED, DEVICE_MODE_CHANGED, GLITCH_FILTER_*, SIGNAL_INVERT_*, COPY_TO_DOC_DONE |
| 7001-7003 | Trigger & save | TRIG_NEXT_COLLECT, SAVE_COMPLETE, STORE_CONF_PREV |
| 8001 | Decode | CLEAR_DECODE_DATA |
| 9001-9004 | App options | APP_OPTIONS_CHANGED, FONT_OPTIONS_CHANGED, SHORTCUT_CHANGED, STYLE_CHANGED |

**Interface defined in `pv/data/datasource.h`:**

| Interface | Purpose |
|-----------|---------|
| `DataSource` | abstract interface for providing signal data |

#### C Decoder DLL System

C decoders are native shared libraries compiled separately. The CMake builds each as a `MODULE` library to `build.dir/decoders/c_decoders/`. At runtime, `libsigrokdecode/srd.c` loads these DLLs dynamically. The `c_decoder_api.c` file is dual-purpose: compiled with `SRD_C_DECODER_DLL` it exports the API for decoder DLLs; without it, it provides in-process decoder management.

C decoder API version: `SRD_C_DECODER_API_VERSION = 3`. Each C decoder exports a `srd_c_decoder` struct with channels, options, annotations, and callback functions (reset/start/decode/end/metadata/destroy/recv_proto). Helper macros `C_ANN_PUT`, `C_ANN_PUT_TYPE`, `C_ANN_PUT_VAL` simplify annotation output. Condition builders (`c_cond_rise/fall/high/low/edge/skip`, `c_cond_wait`) simplify protocol state machines.

Available C decoders (37): spi_c, i2c_c, uart_c, can_c, can_fd_c, jtag_c, swd_c, onewire_c, i2s_c, lin_c, hdlc_c, microwire_c, mdio_c, ps2_c, dmx512_c, nrzi_c, ir_nec_c, ir_rc5_c, ir_sirc_c, dcf77_c, cec_c, spdif_c, usb_signalling_c, 4b5b_c, iso7816_c, lpc_c, dali_c, c2_c, graycode_c, counter_c, lm75_c, ds1307_c, ds3231_c, numbers_and_state_c, seven_segment_c, pwm_c, wiegand_c

## Language Standards

- C++17 (`-std=c++17`)
- C11 (`-std=c11`)

## Coding Conventions

- Qt6 signal/slot mechanism for event communication
- Singleton pattern for `AppControl`, `AppConfig`, `SessionManager`
- Interface-based decoupling via abstract callback classes in `pv/interface/`
- `IContextAware` pattern for binding components to `TabContext`
- JSON format for configuration files (`.dsc` session config, `lang/` translations)
- Custom UI components (borderless window, draggable tabs, sliding drawer, sidebar) instead of standard Qt dock widgets
- Windows-specific code guarded by `#ifdef WIN32` / `if(WIN32)` in CMake
- Resource files managed via Qt `.qrc` files
- `ds_*` prefix for libsigrok public API functions
- `srd_*` prefix for libsigrokdecode public API functions
- `DSV_MSG_*` prefix for broadcast message codes

## Key Files to Understand First

| File | Purpose |
|------|---------|
| `PXView/main.cpp` | Application entry point, command-line parsing, initialization |
| `PXView/pv/appcontrol.cpp` | Singleton controller, libsigrokdecode initialization |
| `PXView/pv/sigsession.h` | Central session class interface — the heart of data flow |
| `PXView/pv/mainwindow.h` | Main window class — implements ISessionCallback, IMessageListener, IMainForm, ISessionDataGetter |
| `PXView/pv/interface/icallbacks.h` | All callback interfaces and message code definitions |
| `PXView/pv/interface/icontextaware.h` | IContextAware interface for TabContext binding |
| `PXView/pv/tabcontext.h` | Per-tab context binding View/Session/Document |
| `PXView/pv/deviceagent.h` | Device abstraction layer with IDeviceAgentCallback |
| `PXView/pv/data/disk_cache_config.h` | Disk cache configuration constants |
| `PXView/pv/dsvdef.h` | View types, session format versions, decoder data formats, utility macros |
| `CMakeLists.txt` | Complete build configuration |
| `libsigrok/libsigrok.h` | Hardware driver public API (ds_* functions) |
| `libsigrokdecode/libsigrokdecode.h` | Decoder engine public API (srd_* and c_decoder_* functions) |

## Common Tasks

### Adding a new C decoder

1. Create `libsigrokdecode/c_decoders/<name>_c.c` following existing decoder patterns
2. Add the decoder name to the `C_DECODERS` list in `CMakeLists.txt`
3. Run `build_incremental.cmd` to rebuild

### Adding a new Python decoder

1. Create `libsigrokdecode/decoders/<name>/` directory with `__init__.py` and `pd.py`
2. Follow the sigrok decoder format (see existing decoders for reference)

### Modifying the UI

- Main window layout: `PXView/pv/mainwindow.cpp` (`setup_ui()`)
- Side panel/dock content: `PXView/pv/dock/`
- Sliding drawer panels: `PXView/pv/widgets/slidingdrawer.cpp`
- Sidebar navigation: `PXView/pv/widgets/sidebar.cpp`
- Toolbars: `PXView/pv/toolbars/`
- Custom widgets: `PXView/pv/widgets/`
- Themes: `PXView/themes/dark.qss`, `PXView/themes/light.qss`

### Adding new device support

1. Add hardware driver in `libsigrok/hardware/`
2. Add firmware files in `PXView/res/`
3. Register driver in `libsigrok/hwdriver.c`

### Working with disk cache

- Configuration: `PXView/pv/data/disk_cache_config.h`
- Buffer management: `PXView/pv/data/disk_buffer_manager.cpp`
- Async writer: `PXView/pv/data/disk_write_thread.cpp`
- Read cache: `PXView/pv/data/disk_read_cache.cpp`
- Status display: `MainWindow::update_disk_cache_status()`
