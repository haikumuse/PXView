#===============================================================================
#= libsigrokstd — Static library with upstream libsigrok symbols renamed srstd_*
#-------------------------------------------------------------------------------
# This builds the upstream libsigrok 0.6.0 as an isolated static library where
# all sr_* public symbols are macro-renamed to srstd_* via -include srstd_rename.h.
# The upstream source files remain unmodified; symbol isolation is achieved
# purely through preprocessor macro injection.
#===============================================================================

# -- Source files (copied read-only from upstream libsigrok 0.6.0) -------------
# Core top-level sources
set(SRSTD_CORE_SOURCES
    src/analog.c
    src/backend.c
    src/binary_helpers.c
    src/conversion.c
    src/crc.c
    src/device.c
    src/drivers.c
    src/error.c
    src/ezusb.c
    src/fallback.c
    src/hwdriver.c
    src/log.c
    src/resource.c
    src/serial.c
    src/serial_bt.c
    src/serial_hid.c
    src/serial_libsp.c
    src/serial_tcpraw.c
    src/session.c
    src/session_driver.c
    src/session_file.c
    src/soft-trigger.c
    src/std.c
    src/strutil.c
    src/sw_limits.c
    src/tcp.c
    src/trigger.c
    src/usb.c
    src/version.c
)

# SCPI subsystem (exclude libgpib, visa, and vxi — they need external libs we don't have)
# VXI needs rpc/rpc.h (not available on MinGW/Windows)
set(SRSTD_SCPI_SOURCES
    src/scpi/scpi.c
    src/scpi/scpi_serial.c
    src/scpi/scpi_tcp.c
    src/scpi/scpi_usbtmc_libusb.c
)

# Modbus subsystem
set(SRSTD_MODBUS_SOURCES
    src/modbus/modbus.c
    src/modbus/modbus_serial_rtu.c
)

# minilzo (compression library, needed by backend.c for session file I/O)
set(SRSTD_MINILZO_SOURCES
    src/minilzo/minilzo.c
)

# Input modules (exclude stf.c — it's conditional on HAVE_INPUT_STF)
set(SRSTD_INPUT_SOURCES
    src/input/input.c
    src/input/feed_queue.c
    src/input/binary.c
    src/input/chronovu_la8.c
    src/input/csv.c
    src/input/isf.c
    src/input/logicport.c
    src/input/null.c
    src/input/protocoldata.c
    src/input/raw_analog.c
    src/input/saleae.c
    src/input/trace32_ad.c
    src/input/vcd.c
    src/input/wav.c
)

# Output modules
set(SRSTD_OUTPUT_SOURCES
    src/output/output.c
    src/output/analog.c
    src/output/ascii.c
    src/output/binary.c
    src/output/bits.c
    src/output/chronovu_la8.c
    src/output/csv.c
    src/output/hex.c
    src/output/null.c
    src/output/ols.c
    src/output/srzip.c
    src/output/vcd.c
    src/output/wav.c
    src/output/wavedrom.c
)

# Transform modules
set(SRSTD_TRANSFORM_SOURCES
    src/transform/transform.c
    src/transform/invert.c
    src/transform/nop.c
    src/transform/scale.c
)

# Bridge sources — libusb_context sharing wrapper with PXView's libsigrok fork.
# srstd_init_shared() lets upstream libsigrokstd reuse PXView's already-created
# libusb_context, avoiding device discovery/hotplug isolation between the two
# libsigrok instances. Compiled with the same -include srstd_rename.h injection
# as upstream sources so internal sr_init()/struct sr_context references are
# macro-renamed to srstd_init()/struct srstd_context.
# Also includes srstd_compat.c (compat shims for functions missing from 0.6.0)
# and the batch driver registration logic (calls srstd_get_all_drivers() from
# driver_registry.c to register all migrated hardware drivers in one shot).
# srstd_pxview_glue.c exposes void* wrappers so PXView C++ code can call the
# upstream srstd_* API WITHOUT including srstd.h (which would pull
# srstd_rename.h into PXView's TU and wrongly rename PXView's sr_* symbols).
# The glue function names start with srstd_ / srstd_glue_ so they are NOT
# affected by the sr_* -> srstd_* rename macros. Two groups:
#   - srstd_pxview_init_shared / srstd_pxview_exit : library init/exit
#     (wraps srstd_init_shared + srstd_exit, called by SigSession::init/uninit)
#   - srstd_glue_dev_config_get/_set/_list / srstd_glue_acquisition_start/_stop
#     : device config/acquisition dispatch (stubs returning SR_ERR_NA until
#     Task 8/9; called by DeviceAgent when device_lib() == LIB_SRSTD)
set(SRSTD_BRIDGE_SOURCES
    bridge/srstd_init_shared.c
    bridge/srstd_compat.c
    bridge/srstd_pxview_glue.c
)
# NOTE: bridge/srstd_bridge.c is NOT in this list — it must be compiled
# WITHOUT srstd_rename.h (it includes PXView's libsigrok.h directly). It is
# built as a separate OBJECT library below and merged into libsigrokstd.

# Excluded files (and why):
#   driver_list_start.c, driver_list_stop.c — ELF section attributes, not needed
#       (HAVE_DRIVERS is not defined, so sr_drivers_init creates an empty list;
#        all migrated drivers are batch-registered in srstd_init_shared.c via
#        srstd_get_all_drivers() from driver_registry.c instead)
#   serial_libsp.c — needs libserialport
#   serial_bt.c — needs bluetooth headers
#   NOTE: serial_libsp.c and serial_bt.c ARE included above — both files have
#   #ifdef guards (#ifdef HAVE_LIBSERIALPORT / #ifdef HAVE_BLUETOOTH) that
#   compile to NULL stubs when the dependencies are absent. They are required
#   because serial.c references ser_lib_funcs_libsp / ser_lib_funcs_bt /
#   ser_name_is_bt unconditionally.
#   serial_hid_bu86x.c, serial_hid_ch9325.c, serial_hid_cp2110.c,
#   serial_hid_victor.c — need hidapi (serial_hid.c has #ifdef HAVE_LIBHIDAPI
#   stubs, so these chip-specific files are not needed when hidapi is absent)
#   scpi_libgpib.c — needs GPIB headers
#   scpi_visa.c — needs VISA headers
#   scpi_vxi.c, vxi_clnt.c, vxi_xdr.c — need rpc/rpc.h (not on MinGW)

# -- All migrated hardware drivers (auto-scanned) ------------------------------
# Uses file(GLOB) to collect all .c files from src/hardware/*/ directories.
# This automatically picks up newly migrated drivers without editing CMakeLists.
# Each driver's api.c declares a non-static <name>_driver_info struct that
# driver_registry.c references — the extern references force the linker to pull
# each api.o into the final binary (Windows PE/COFF static-library requirement).
# Compiled with -include srstd_rename.h (via target_compile_options below) so
# all sr_* calls in the upstream sources become srstd_*.
# Note: src/hardware/ currently has no top-level .c files (all drivers live in
# subdirectories), but the first glob pattern is kept for future-proofing.
file(GLOB SRSTD_HARDWARE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/hardware/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/hardware/*/*.c"
)
# Exclude drivers whose upstream sources depend on optional libraries that are
# not available in this build environment. PXView's own libsigrok fork already
# provides working compat ports for these, so they need not live in libsigrokstd.
#   - dcttech-usbrelay: upstream uses <hidapi.h>; PXView fork re-implements the
#     HID path on top of libusb control transfers. See driver_registry.c for
#     the matching exclusion of the driver_info extern + array entry.
#   - link-mso19: upstream uses <libserialport.h> for serial port enumeration;
#     PXView fork has a compat port that doesn't need libserialport.
list(FILTER SRSTD_HARDWARE_SOURCES EXCLUDE REGEX
    "/src/hardware/(dcttech-usbrelay|link-mso19)/.*\\.c$")

# -- Combined source list ------------------------------------------------------
set(libsigrokstd_SOURCES
    ${SRSTD_CORE_SOURCES}
    ${SRSTD_SCPI_SOURCES}
    ${SRSTD_MODBUS_SOURCES}
    ${SRSTD_MINILZO_SOURCES}
    ${SRSTD_INPUT_SOURCES}
    ${SRSTD_OUTPUT_SOURCES}
    ${SRSTD_TRANSFORM_SOURCES}
    ${SRSTD_BRIDGE_SOURCES}
    ${SRSTD_HARDWARE_SOURCES}
    src/driver_registry.c
)

# -- Static library target -----------------------------------------------------
add_library(libsigrokstd SHARED ${libsigrokstd_SOURCES})

# -- Include paths -------------------------------------------------------------
# libsigrokstd/include        — srstd.h, srstd_rename.h, libsigrok/libsigrok.h
# libsigrokstd/src            — config.h, libsigrok-internal.h, scpi.h, serial_hid.h
# IMPORTANT: Use BEFORE to place these paths AHEAD of the global include_directories
# from deps.cmake (which includes ./libsigrok — PXView's forked 0.2.0 headers).
# Without BEFORE, #include <config.h> and #include <libsigrok/libsigrok.h> would
# resolve to PXView's headers first, causing enum/struct mismatches.
target_include_directories(libsigrokstd BEFORE PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# -- Force-include the rename header in every translation unit ------------------
# This injects all #define sr_xxx srstd_xxx macros before any source line is
# processed, achieving symbol isolation without modifying upstream source files.
target_compile_options(libsigrokstd PRIVATE
    -fvisibility=hidden
    -fvisibility-inlines-hidden
)

# -- Compile definitions -------------------------------------------------------
target_compile_definitions(libsigrokstd PRIVATE
    HAVE_LIBUSB_1_0
    _GNU_SOURCE
    SRSTD_BUILDING_DLL=1
)

# -- C standard ----------------------------------------------------------------
set_target_properties(libsigrokstd PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
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
target_link_libraries(libsigrokstd PUBLIC
    ${LIBUSB_1_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${LIBZIP_LIBRARIES}
    -lglib-2.0
    -lws2_32
    -lftdi1
    -lnettle
)

# -- Bridge OBJECT library: srstd_bridge.c WITHOUT srstd_rename.h --------------
# srstd_bridge.c implements the dual-library data structure conversion layer.
# It includes PXView's libsigrok.h / libsigrok-internal.h directly (NOT the
# upstream renamed headers), so it MUST be compiled without -include
# srstd_rename.h. It also must NOT have libsigrokstd's BEFORE include paths
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
# Merge the bridge objects into the main static library so consumers linking
# libsigrokstd get srstd_bridge_* symbols without an extra link dependency.
target_sources(libsigrokstd PRIVATE $<TARGET_OBJECTS:libsigrokstd_bridge>)

# -- Test executable: srstd_init_shared verification ---------------------------
# Verifies that srstd_init_shared() correctly shares a libusb_context between
# PXView's libsigrok and the upstream libsigrokstd static library.
# Build: ninja test_srstd_init_shared
# Run:   build.dir/test_srstd_init_shared.exe
add_executable(test_srstd_init_shared tests/test_srstd_init_shared.c)
# Force libsigrokstd's include/ and src/ AHEAD of the global include_directories
# from deps.cmake (which includes ./libsigrok — PXView's forked 0.2.0 headers).
# Without BEFORE, #include "libsigrok-internal.h" and #include <libsigrok/libsigrok.h>
# would resolve to PXView's headers first, causing struct sr_context layout mismatch
# (PXView has libusb_ctx at offset 0; upstream has it at offset 8 after driver_list).
target_include_directories(test_srstd_init_shared BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_options(test_srstd_init_shared PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/include/srstd_rename.h
)
target_compile_definitions(test_srstd_init_shared PRIVATE
    HAVE_LIBUSB_1_0
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
# Verifies the dual-library data structure conversion functions:
#   srstd_channel_to_pxview / srstd_sdi_to_pxview / srstd_packet_to_pxview
# The test is compiled WITH -include srstd_rename.h so it can construct
# upstream srstd_* structs directly. PXView structs (returned by _alloc) are
# treated as OPAQUE pointers — only accessed via _get_* accessor functions,
# never dereferenced directly (field layout differs between upstream and PXView).
# Build: ninja test_srstd_bridge
# Run:   build.dir/test_srstd_bridge.exe
add_executable(test_srstd_bridge tests/test_srstd_bridge.c)
# Same BEFORE include strategy as test_srstd_init_shared: upstream headers
# must resolve to libsigrokstd's versions, not PXView's.
target_include_directories(test_srstd_bridge BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/bridge
)
target_compile_options(test_srstd_bridge PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/include/srstd_rename.h
)
target_compile_definitions(test_srstd_bridge PRIVATE
    HAVE_LIBUSB_1_0
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
# Verifies the PXView TriggerConfig → upstream sr_trigger conversion:
#   - Simple mode: per-channel value0 string parsed into matches
#   - Adv mode: multi-stage, value0+value1 concat
#   - Serial mode: returns -2 (unimplemented)
#   - NULL/boundary checks, 'X' skip, empty string handling
# Same BEFORE include strategy as test_srstd_bridge: upstream headers must
# resolve to libsigrokstd's versions, not PXView's. The test directly accesses
# upstream sr_trigger fields (renamed to srstd_trigger via srstd_rename.h)
# because the shadow structures in srstd_bridge.c match the upstream layout.
# Build: ninja test_srstd_trigger
# Run:   build.dir/test_srstd_trigger.exe
add_executable(test_srstd_trigger tests/test_srstd_trigger.c)
target_include_directories(test_srstd_trigger BEFORE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/bridge
)
target_compile_options(test_srstd_trigger PRIVATE
    -include ${CMAKE_CURRENT_SOURCE_DIR}/include/srstd_rename.h
)
target_compile_definitions(test_srstd_trigger PRIVATE
    HAVE_LIBUSB_1_0
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
