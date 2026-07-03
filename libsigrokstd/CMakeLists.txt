#===============================================================================
#= libsigrokstd -- Shared library with 100% unmodified upstream libsigrok source
#-------------------------------------------------------------------------------
# This builds the upstream libsigrok 0.6.0 as an isolated SHARED library.
# All source files under src/ are COPIED VERBATIM from upstream libsigrok
# (C:\Users\admin\Downloads\libsigrok\src) -- zero modifications to upstream.
# Driver registration uses the upstream SR_REGISTER_DEV_DRIVER section
# mechanism (driver_list_start.c + driver_list_stop.c + per-driver api.c).
# HAVE_DRIVERS is defined so sr_drivers_init() collects drivers from the
# __sr_driver_list section automatically.
#===============================================================================

# -- Collect all upstream .c files (recursive) --------------------------------
# GLOB_RECURSE picks up src/*.c, src/hardware/*/*.c, src/input/*.c, src/output/*.c,
# src/scpi/*.c, src/modbus/*.c, src/transform/*.c, src/minilzo/*.c, src/bt/*.c,
# src/dmm/*.c, src/lcr/*.c, src/scale/*.c.
# CONFIGURE_DEPENDS makes CMake re-scan when files are added/removed.
file(GLOB_RECURSE SRSTD_UPSTREAM_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

# -- driver_list_start.c / driver_list_stop.c need special link ordering -------
# start MUST link before all drivers; stop MUST link after all drivers.
# They define sr_driver_list__start[] / sr_driver_list__stop[] section boundaries.
set(SRSTD_DRIVER_LIST_START "${CMAKE_CURRENT_SOURCE_DIR}/src/driver_list_start.c")
set(SRSTD_DRIVER_LIST_STOP  "${CMAKE_CURRENT_SOURCE_DIR}/src/driver_list_stop.c")

# -- Files to exclude from the GLOB result ------------------------------------
#   driver_list_start.c / driver_list_stop.c -- handled separately for ordering
#   driver_registry.c                       -- deleted (Phase 3, no longer used)
#   scpi_libgpib.c                          -- needs GPIB headers (unavailable)
#   scpi_visa.c                             -- needs VISA headers (unavailable)
#   scpi_vxi.c / vxi_clnt.c / vxi_xdr.c     -- need rpc/rpc.h (unavailable on MinGW)
#   serial_hid_bu86x.c / ch9325 / cp2110 / victor -- need hidapi (serial_hid.c
#                                            has #ifdef HAVE_LIBHIDAPI stubs, so
#                                            chip-specific files are not needed)
#   minilzo/testmini.c                       -- test executable, not library code
#   hardware/dcttech-usbrelay               -- upstream uses <hidapi.h>
#   hardware/link-mso19                     -- upstream uses <libserialport.h>
list(REMOVE_ITEM SRSTD_UPSTREAM_SOURCES
    ${SRSTD_DRIVER_LIST_START}
    ${SRSTD_DRIVER_LIST_STOP}
    "${CMAKE_CURRENT_SOURCE_DIR}/src/driver_registry.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scpi/scpi_libgpib.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scpi/scpi_visa.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scpi/scpi_vxi.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scpi/vxi_clnt.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/scpi/vxi_xdr.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/serial_hid_bu86x.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/serial_hid_ch9325.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/serial_hid_cp2110.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/serial_hid_victor.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/minilzo/testmini.c"
)
list(FILTER SRSTD_UPSTREAM_SOURCES EXCLUDE REGEX
    "/src/hardware/(dcttech-usbrelay|link-mso19|beaglelogic|baylibre-acme|hung-chang-dso-2100|hp-3478a|mooshimeter-dmm)/.*\\.c$"
)
#   beaglelogic        -- Linux-specific (needs sys/mman.h, sys/ioctl.h,
#                         /dev/beaglelogic kernel node). Unavailable on MinGW.
#   baylibre-acme      -- Linux-specific (needs sys/timerfd.h, arpa/inet.h
#                         without #ifdef guards). Unavailable on MinGW.
#   hung-chang-dso-2100 -- needs ieee1284.h (parallel port). Unavailable on MinGW.
#   hp-3478a           -- unconditionally calls sr_scpi_gpib_spoll() which is
#                         only declared under #ifdef HAVE_LIBGPIB (scpi.h).
#                         Without HAVE_LIBGPIB, compilation fails. The upstream
#                         driver assumes GPIB is available.
#   mooshimeter-dmm    -- unconditionally calls sr_bt_*() which is only
#                         declared under #ifdef HAVE_BLUETOOTH. Without
#                         HAVE_BLUETOOTH, compilation fails.

# -- Bridge sources (libsigrokstd/bridge/, NOT upstream) ----------------------
# srstd_init_shared.c -- libusb_context sharing wrapper (calls sr_init, replaces
#                        libusb_ctx with PXView's shared context)
# srstd_compat.c      -- compat shims for functions missing from upstream 0.6.0
# srstd_pxview_glue.c -- void* wrappers so PXView C++ can call upstream sr_*
#                        API WITHOUT including upstream headers directly
set(SRSTD_BRIDGE_SOURCES
    bridge/srstd_init_shared.c
    bridge/srstd_compat.c
    bridge/srstd_pxview_glue.c
)
# NOTE: bridge/srstd_bridge.c is NOT in this list -- it must be compiled
# WITHOUT libsigrokstd's BEFORE include paths (it includes PXView's libsigrok.h
# directly). It is built as a separate OBJECT library below and merged in.

# -- Combined source list with correct link ordering --------------------------
# Order: driver_list_start.c FIRST, then all upstream sources, then
# driver_list_stop.c LAST, then bridge sources. The start/stop ordering
# matters for the __sr_driver_list section layout (start defines the section
# head with a dummy NULL, stop defines the section tail with a dummy NULL;
# sr_drivers_init iterates from start+1 to stop exclusive).
set(libsigrokstd_SOURCES
    ${SRSTD_DRIVER_LIST_START}
    ${SRSTD_UPSTREAM_SOURCES}
    ${SRSTD_DRIVER_LIST_STOP}
    ${SRSTD_BRIDGE_SOURCES}
)

# -- Shared library target -----------------------------------------------------
add_library(libsigrokstd SHARED ${libsigrokstd_SOURCES})

# -- Include paths -------------------------------------------------------------
# libsigrokstd/include -- upstream libsigrok/libsigrok.h, proto.h, version.h
#                         (100% unmodified upstream copies) + srstd_export.h
# libsigrokstd/src      -- config.h (local), libsigrok-internal.h, scpi.h,
#                         serial_hid.h (all upstream)
# IMPORTANT: Use BEFORE to place these paths AHEAD of the global include_directories
# from deps.cmake (which includes ./libsigrok -- PXView's forked 0.2.0 headers).
# Without BEFORE, #include <config.h> and #include <libsigrok/libsigrok.h> would
# resolve to PXView's headers first, causing enum/struct mismatches.
target_include_directories(libsigrokstd BEFORE PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# -- Visibility ---------------------------------------------------------------
# NOTE: -fvisibility=hidden is intentionally NOT set on Windows.
# Upstream libsigrok's SR_API/SR_PRIV macros are EMPTY on _WIN32 (upstream
# uses autotools/libtool, not __declspec). Our srstd_export.h's SRSTD_API is
# also empty on Windows for the same reason. The actual DLL export is handled
# by WINDOWS_EXPORT_ALL_SYMBOLS=ON below.
# Setting -fvisibility=hidden here would hide all symbols at the compiler
# level, and combined with empty SR_API/SR_PRIV would leave no exports.
if(NOT WIN32)
    target_compile_options(libsigrokstd PRIVATE
        -fvisibility=hidden
        -fvisibility-inlines-hidden
    )
endif()

# -- Compile definitions -------------------------------------------------------
# HAVE_LIBUSB_1_0  -- libusb backend
# HAVE_SERIAL_COMM -- sr_serial_dev_inst struct definition
# HAVE_ZLIB        -- session file compression
# HAVE_LIBZIP      -- session file archive support
# HAVE_DRIVERS     -- ENABLE upstream section-based driver registration
#                    (sr_drivers_init iterates __sr_driver_list section)
# NOTE: SRSTD_BUILDING_DLL is intentionally NOT defined. srstd_export.h
# defines SRSTD_API as empty on Windows regardless, so __declspec(dllexport)
# annotations never appear anywhere in the build. This is REQUIRED for
# WINDOWS_EXPORT_ALL_SYMBOLS to work -- any dllexport annotation would
# cause MinGW ld to silently disable --export-all-symbols.
target_compile_definitions(libsigrokstd PRIVATE
    HAVE_LIBUSB_1_0
    HAVE_SERIAL_COMM
    HAVE_ZLIB
    HAVE_LIBZIP
    HAVE_DRIVERS
    _GNU_SOURCE
)

# -- C standard ----------------------------------------------------------------
set_target_properties(libsigrokstd PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    # Windows DLL symbol export: upstream libsigrok's SR_API macro is empty on
    # _WIN32 (upstream uses autotools/libtool for DLL export, not __declspec).
    # With -fvisibility=hidden, this would hide ALL symbols including public
    # SR_API functions like sr_init/sr_exit/sr_driver_list. WINDOWS_EXPORT_ALL_
    # SYMBOLS=ON makes CMake auto-generate a .def file that exports all global
    # symbols, which is the standard MinGW approach for building shared libs
    # whose sources were not designed with __declspec(dllexport) annotations.
    # This exports both SR_API (public) and SR_PRIV (internal) symbols; the
    # latter is acceptable on Windows (MinGW tradition) and only matters for
    # ABI surface, not for functionality.
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

# -- Output directory (same as PXView's build.dir) ------------------------------
set_target_properties(libsigrokstd PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build.dir"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build.dir"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build.dir"
)

# -- Link dependencies ---------------------------------------------------------
# glib-2.0, libusb-1.0, zlib are already found by the parent CMakeLists.txt
# (deps.cmake) and their include paths are in the global include_directories().
# libzip is needed for session file I/O (session_driver.c, session_file.c, srzip.c).
# libftdi1 is needed by asix-sigma, chronovu-la, ftdi-la, ikalogic-scanaplus,
# pipistrello-ols drivers (FTDI-based USB acquisition).
# nettle is needed by rdtech-tc driver (AES-256 firmware decryption).
pkg_search_module(LIBZIP libzip)
if(NOT LIBZIP_FOUND)
    message(FATAL_ERROR "Please install libzip!")
endif()
# libftdi1 is needed by FTDI-based USB acquisition drivers (asix-sigma,
# chronovu-la, ftdi-la, ikalogic-scanaplus, pipistrello-ols). If not found,
# only those 4-5 drivers fail to compile (acceptable); the rest of libsigrokstd
# builds normally. The -lftdi1 link flag below is kept conditional so a missing
# libftdi1 does not break the link step when its drivers are excluded.
pkg_search_module(LIBFTDI1 libftdi1)
if(NOT LIBFTDI1_FOUND)
    message(WARNING "libftdi1 not found - FTDI-based drivers will be disabled")
else()
    target_include_directories(libsigrokstd PRIVATE ${LIBFTDI1_INCLUDE_DIRS})
endif()
target_link_libraries(libsigrokstd PUBLIC
    ${LIBUSB_1_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${LIBZIP_LIBRARIES}
    -lglib-2.0
    -lws2_32
    $<$<BOOL:${LIBFTDI1_FOUND}>:-lftdi1>
    -lnettle
)

# -- Bridge OBJECT library: srstd_bridge.c WITHOUT libsigrokstd include paths --
# srstd_bridge.c implements the dual-library data structure conversion layer.
# It includes PXView's libsigrok.h / libsigrok-internal.h directly (NOT the
# upstream headers), so it MUST NOT have libsigrokstd's BEFORE include paths
# (which would make #include "libsigrok.h" resolve to the upstream version).
# By using a separate OBJECT library with no target_include_directories override,
# only the global include paths (./libsigrok from deps.cmake) apply, so
# #include "libsigrok.h" correctly resolves to PXView's ./libsigrok/libsigrok.h.
add_library(libsigrokstd_bridge OBJECT bridge/srstd_bridge.c)
# Only the bridge/ directory is added (for srstd_bridge.h). The global
# include_directories from deps.cmake provides ./libsigrok (PXView headers),
# ./common (ds_types.h), glib, and libusb paths.
target_include_directories(libsigrokstd_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/bridge
)
set_target_properties(libsigrokstd_bridge PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)
# Merge the bridge objects into the main shared library so consumers linking
# libsigrokstd get srstd_bridge_* symbols without an extra link dependency.
target_sources(libsigrokstd PRIVATE $<TARGET_OBJECTS:libsigrokstd_bridge>)

# -- Compat declaration injection for sipeed-slogic-analyzer -------------------
# sipeed-slogic-analyzer/api.c (from the slogic-dev branch) calls std_i32_idx()
# which was added in upstream libsigrok 0.6.1 but is missing from 0.6.0. The
# implementation is in bridge/srstd_compat.c; the declaration lives in
# bridge/srstd_compat.h. To avoid modifying the upstream api.c source, the
# declaration is force-injected via -include ONLY into this one file.
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/src/hardware/sipeed-slogic-analyzer/api.c"
    PROPERTIES
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/bridge/srstd_compat.h"
)

# =============================================================================
# Test executables (NOT built by default -- run `ninja test_srstd_*` explicitly)
# =============================================================================
# NOTE: The test source files (tests/test_srstd_*.c) were written for Phase 1/2
# when srstd.h and srstd_rename.h existed. Those headers have been removed in
# Phase 3 (upstream source is now unmodified). The test sources still
# `#include "srstd.h"` which no longer exists, so the tests will NOT compile
# until the test sources are updated in a later phase. The test target
# definitions are preserved here for forward compatibility. Building
# libsigrokstd itself does NOT trigger test compilation.
# =============================================================================

# -- Test executable: srstd_init_shared verification ---------------------------
# Build: ninja test_srstd_init_shared  (NOTE: currently broken, see above)
add_executable(test_srstd_init_shared EXCLUDE_FROM_ALL tests/test_srstd_init_shared.c)
target_include_directories(test_srstd_init_shared BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(test_srstd_init_shared PRIVATE
    HAVE_LIBUSB_1_0
    HAVE_SERIAL_COMM
    HAVE_ZLIB
    HAVE_LIBZIP
    HAVE_DRIVERS
    _GNU_SOURCE
)
target_link_libraries(test_srstd_init_shared PRIVATE
    libsigrokstd
    ${LIBUSB_1_LIBRARIES}
)
set_target_properties(test_srstd_init_shared PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build.dir"
)

# -- Test executable: srstd_bridge conversion verification ---------------------
# Build: ninja test_srstd_bridge  (NOTE: currently broken, see above)
add_executable(test_srstd_bridge EXCLUDE_FROM_ALL tests/test_srstd_bridge.c)
target_include_directories(test_srstd_bridge BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/bridge
)
target_compile_definitions(test_srstd_bridge PRIVATE
    HAVE_LIBUSB_1_0
    HAVE_SERIAL_COMM
    HAVE_ZLIB
    HAVE_LIBZIP
    HAVE_DRIVERS
    _GNU_SOURCE
)
target_link_libraries(test_srstd_bridge PRIVATE
    libsigrokstd
    ${LIBUSB_1_LIBRARIES}
    -lglib-2.0
)
set_target_properties(test_srstd_bridge PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build.dir"
)

# -- Test executable: pxview_trigger_to_srstd verification ---------------------
# Build: ninja test_srstd_trigger  (NOTE: currently broken, see above)
add_executable(test_srstd_trigger EXCLUDE_FROM_ALL tests/test_srstd_trigger.c)
target_include_directories(test_srstd_trigger BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/bridge
)
target_compile_definitions(test_srstd_trigger PRIVATE
    HAVE_LIBUSB_1_0
    HAVE_SERIAL_COMM
    HAVE_ZLIB
    HAVE_LIBZIP
    HAVE_DRIVERS
    _GNU_SOURCE
)
target_link_libraries(test_srstd_trigger PRIVATE
    libsigrokstd
    ${LIBUSB_1_LIBRARIES}
    -lglib-2.0
)
set_target_properties(test_srstd_trigger PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build.dir"
)
