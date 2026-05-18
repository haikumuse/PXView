# AGENTS.md

This file provides guidance to AI coding agents when working with code in this repository.

## Project Overview

**PXView** (binary name: `PXView.exe`) is a Qt6 C++ GUI application for signal analysis with logic analyzers, oscilloscopes, and similar instruments. It is forked from the [sigrok](https://sigrok.org) PulseView project and supports DreamSourceLab/PXLogic hardware devices. Licensed GPLv3+. Current version: 1.5.0.

The project compiles four components into a single executable, plus 37 separate C decoder DLLs:

1. **`libsigrok/`** — Hardware driver layer (C): USB/DSL device enumeration, data capture, session control
2. **`libsigrokdecode/`** — Protocol decoder engine (C + Python): loads and runs both Python and C protocol decoders
3. **`common/`** — Shared utilities (C): minizip (session file zip/unzip), xlog (logging)
4. **`PXView/`** — Qt5 C++ GUI application: the main user-facing application

## Build Instructions

### Windows (Primary Development Platform)

**Use** **`build_incremental.cmd`** **for incremental builds after the first full build.**

:: Subsequent builds: incremental build (much faster)

build\_incremental.cmd

Both scripts invoke MSYS2 from `D:\msys64`, configure with CMake+Ninja in `build/`, output binaries to `build.dir/`, and install to `install.dir/`. The final executable is at `install.dir/bin/PXView.exe`.

### Build Configuration

cmake .. -G Ninja -DCMAKE\_BUILD\_TYPE=Release  -DCMAKE\_INSTALL\_PREFIX=../install.dir   -DCMAKE\_PREFIX\_PATH=/mingw64

## Repository Architecture

### Directory Layout

```
.
├── CMakeLists.txt              # Main CMake build configuration (943 lines)
├── build_full.cmd              # Windows full build script
├── build_incremental.cmd       # Windows incremental build script
├── build_incremental.sh        # Linux incremental build script
├── CMake/                      # CMake find modules (FFTW, libusb)
├── libsigrok/                  # Hardware driver layer (C)
│   ├── hardware/DSL/           # DSLogic/DSCope device drivers
│   ├── hardware/pxlogic/       # PXLogic device driver
│   ├── hardware/common/        # USB utilities (usb.c, ezusb.c)
│   ├── hardware/demo/          # Demo device driver
│   ├── input/                  # File input formats (binary, VCD, WAV)
│   └── output/                 # File output formats (CSV, gnuplot, srzip, VCD)
├── libsigrokdecode/            # Protocol decoder engine (C + Python)
│   ├── c_decoders/             # 37 native C decoders (compiled as DLLs)
│   └── decoders/               # 170+ Python protocol decoders
├── common/                     # Shared utilities (C)
│   ├── minizip/                # ZIP compression/decompression
│   └── log/                    # xlog logging
├── PXView/                     # Qt5 C++ GUI application
│   ├── main.cpp                # Application entry point
│   ├── pv/                     # Main application source
│   │   ├── appcontrol.cpp/h    # Singleton app controller
│   │   ├── mainwindow.cpp/h    # Main window (~3100 lines)
│   │   ├── mainframe.cpp/h     # Borderless top-level frame
│   │   ├── sigsession.cpp/h    # Central session orchestrator
│   │   ├── deviceagent.cpp/h   # libsigrok device wrapper
│   │   ├── tabcontext.cpp/h    # Per-tab context (View + Session + Document)
│   │   ├── sessionmanager.cpp/h # Tab context registry
│   │   ├── config/             # AppConfig singleton (JSON settings)
│   │   ├── data/               # Data layer (snapshots, decoders, math)
│   │   ├── dialogs/            # Dialog windows
│   │   ├── dock/               # Dock/side panel widgets
│   │   ├── interface/          # Callback interfaces (ISessionCallback, etc.)
│   │   ├── prop/               # Property bindings
│   │   ├── toolbars/           # Toolbar widgets
│   │   ├── ui/                 # UI utilities (lang, icons, drag tabs)
│   │   ├── utility/            # General utilities (encoding, path, array)
│   │   ├── view/               # View layer (signals, traces, viewport)
│   │   └── widgets/            # Custom widgets
│   ├── themes/                 # CSS themes (breeze light/dark)
│   ├── icons/                  # SVG icons (dark/light variants)
│   ├── languages/              # Qt translation files (.qm)
│   ├── fonts/                  # Application fonts
│   ├── demo/                   # Demo signal files
│   └── res/                    # Firmware files and default configs
├── lang/                       # JSON language files (cn/, en/)
├── window/                     # Windows packaging scripts
│   ├── package.sh              # Main packaging script
│   ├── copy-deps.sh            # DLL dependency collector
│   ├── python/                 # Python embed package
│   └── workflows/              # GitHub Actions CI (win64.yml)
└── doc/                        # Development documentation (Chinese)

```

### Key Architectural Concepts

#### Application Lifecycle

`main.cpp` → `AppControl` (singleton) → creates `MainFrame` → holds `MainWindow`

#### Session and Tab Architecture (Most Important Abstraction)

- **`TabContext`** — Binds together a `View` (rendering), `SigSession` (data source), and `SessionDocument` (data storage). Has LIVE (capturing) and HISTORICAL (reviewing saved data) states. One per tab.
- **`SessionManager`** — Singleton tracking all `TabContext` instances
- **`SigSession`** — Central orchestrator: controls device capture, receives data callbacks from `libsigrok`, manages double-buffered `SessionData`, dispatches decode tasks to worker threads
- **`DeviceAgent`** — Wrapper around `libsigrok` device handles, provides typed get/set config methods

#### Window System

- **`MainFrame`** — Borderless top-level QFrame with custom title bar; uses `WinNativeWidget` on Windows
- **`MainWindow`** — Central QMainWindow with QRibbon menu, toolbars, dock widgets, `DraggableTabWidget`
- **`SubMainFrame`** — Independent window for detached tabs
- **`DraggableTabWidget`/`DraggableTabBar`** — Custom tab widget supporting drag-out to create independent windows

#### Data Layer (`pv/data/`)

- `Snapshot` → `LogicSnapshot`, `AnalogSnapshot`, `DsoSnapshot` — raw captured data storage
- `SessionDocument` — owns snapshots + decode traces; implements `DataSource` interface
- `DecoderStack` — manages protocol decoder instances for a channel
- `MathStack`, `SpectrumStack` — math/FFT processing on DSO data

#### View Layer (`pv/view/`)

- `View` — top-level scrollable, zoomable container
- `Viewport` — QAbstractScrollArea subclass; main rendering surface
- `Trace` (abstract) → `Signal` → `LogicSignal`, `AnalogSignal`, `DsoSignal`
- `DecodeTrace` — protocol decoder annotation visualization
- `MathTrace`, `SpectrumTrace`, `LissajousTrace` — derived data views

#### Callback/Message System (`pv/interface/icallbacks.h`)

- `ISessionCallback` — 15 callback methods for session events (data updates, triggers, errors)
- `IMessageListener` — broadcast message system using `DSV_MSG_*` integer codes (5001-9002)
- `DataSource` — abstract interface for providing signal data
- `IDecoderPannel` — callback for decoder UI name updates

#### C Decoder DLL System

C decoders are native shared libraries compiled separately. The CMake builds each as a `MODULE` library to `build.dir/decoders/c_decoders/`. At runtime, `libsigrokdecode/srd.c` loads these DLLs dynamically. The `c_decoder_api.c` file is dual-purpose: compiled with `SRD_C_DECODER_DLL` it exports the API for decoder DLLs; without it, it provides in-process decoder management.

Available C decoders (37): spi\_c, i2c\_c, uart\_c, can\_c, can\_fd\_c, jtag\_c, swd\_c, onewire\_c, i2s\_c, lin\_c, hdlc\_c, microwire\_c, mdio\_c, ps2\_c, dmx512\_c, nrzi\_c, ir\_nec\_c, ir\_rc5\_c, ir\_sirc\_c, dcf77\_c, cec\_c, spdif\_c, usb\_signalling\_c, 4b5b\_c, iso7816\_c, lpc\_c, dali\_c, c2\_c, graycode\_c, counter\_c, lm75\_c, ds1307\_c, ds3231\_c, numbers\_and\_state\_c, seven\_segment\_c, pwm\_c, wiegand\_c

## Language Standards

- C++11 (`-std=c++11`)
- C99 (`-std=c99`)

## Coding Conventions

- Qt5 signal/slot mechanism for event communication
- Singleton pattern for `AppControl`, `AppConfig`, `SessionManager`
- Interface-based decoupling via abstract callback classes in `pv/interface/`
- JSON format for configuration files (`.dsc` session config, `lang/` translations)
- Custom UI components (borderless window, draggable tabs, sliding drawer) instead of standard Qt dock widgets
- Windows-specific code guarded by `#ifdef WIN32` / `if(WIN32)` in CMake
- Resource files managed via Qt `.qrc` files

## Key Files to Understand First

| File                                | Purpose                                                                   |
| ----------------------------------- | ------------------------------------------------------------------------- |
| `PXView/main.cpp`                   | Application entry point, command-line parsing, initialization             |
| `PXView/pv/appcontrol.cpp`          | Singleton controller, libsigrokdecode initialization                      |
| `PXView/pv/sigsession.h`            | Central session class interface — the heart of data flow                  |
| `PXView/pv/mainwindow.h`            | Main window class — largest file, implements multiple callback interfaces |
| `PXView/pv/interface/icallbacks.h`  | All callback interfaces and message code definitions                      |
| `PXView/pv/tabcontext.h`            | Per-tab context binding View/Session/Document                             |
| `PXView/pv/deviceagent.h`           | Device abstraction layer                                                  |
| `CMakeLists.txt`                    | Complete build configuration                                              |
| `libsigrok/libsigrok.h`             | Hardware driver public API                                                |
| `libsigrokdecode/libsigrokdecode.h` | Decoder engine public API                                                 |

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
- Toolbars: `PXView/pv/toolbars/`
- Custom widgets: `PXView/pv/widgets/`
- Themes: `PXView/themes/dark.qss`, `PXView/themes/light.qss`

### Adding new device support

1. Add hardware driver in `libsigrok/hardware/`
2. Add firmware files in `PXView/res/`
3. Register driver in `libsigrok/hwdriver.c`

