#===============================================================================
#= libsigrok source
#-------------------------------------------------------------------------------
set(libsigrok_SOURCES
    libsigrok/version.c
    libsigrok/strutil.c
    libsigrok/std.c
    libsigrok/session_driver.c
    libsigrok/session.c
    libsigrok/log.c
    libsigrok/hwdriver.c
    libsigrok/error.c
    libsigrok/backend.c
    libsigrok/output/output.c
    libsigrok/input/input.c
    libsigrok/hardware/demo/demo.c
    libsigrok/input/in_binary.c
    libsigrok/input/in_vcd.c
    libsigrok/input/in_wav.c
    libsigrok/output/csv.c
    libsigrok/output/gnuplot.c
    libsigrok/output/srzip.c
    libsigrok/output/vcd.c
    libsigrok/hardware/DSL/dslogic.c
    libsigrok/hardware/common/usb.c
    libsigrok/hardware/common/ezusb.c
    libsigrok/trigger.c
    libsigrok/dsdevice.c
    libsigrok/hardware/DSL/dscope.c
    libsigrok/hardware/DSL/command.c
    libsigrok/hardware/DSL/dsl.c
    libsigrok/hardware/pxlogic/pxlogic.c
    libsigrok/hardware/pxlogic/usb_ctrl.c
    libsigrok/lib_main.c
)

set(libsigrok_HEADERS
    libsigrok/version.h
    libsigrok/libsigrok-internal.h
    libsigrok/libsigrok.h
    libsigrok/config.h
    libsigrok/hardware/DSL/command.h
    libsigrok/hardware/DSL/dsl.h
)

#===============================================================================
#= libsigrokdecode source
#-------------------------------------------------------------------------------
set(libsigrokdecode_SOURCES
    libsigrokdecode/type_decoder.c
    libsigrokdecode/srd.c
    libsigrokdecode/module_sigrokdecode.c
    libsigrokdecode/decoder.c
    libsigrokdecode/dll_registry.c
    libsigrokdecode/error.c
    libsigrokdecode/exception.c
    libsigrokdecode/instance.c
    libsigrokdecode/log.c
    libsigrokdecode/session.c
    libsigrokdecode/util.c
    libsigrokdecode/version.c
    libsigrokdecode/c_decoder_api.c
)

set(libsigrokdecode_HEADERS
    libsigrokdecode/libsigrokdecode-internal.h
    libsigrokdecode/libsigrokdecode.h
    libsigrokdecode/config.h
    libsigrokdecode/version.h
)

#===============================================================================
#= common source
#-------------------------------------------------------------------------------

set(common_SOURCES
    common/minizip/zip.c
    common/minizip/unzip.c
    common/minizip/ioapi.c
    common/log/xlog.c
)

set(common_HEADERS
    common/minizip/zip.h
    common/minizip/unzip.h
    common/minizip/ioapi.h
    common/log/xlog.h
)

#===============================================================================
#= Linker Configuration
#-------------------------------------------------------------------------------

set(PXVIEW_LINK_LIBS
	-lz
	-lglib-2.0
	${CMAKE_THREAD_LIBS_INIT}
	${LIBUSB_1_LIBRARIES}
	${FFTW_LIBRARIES}
	${PY_LIB}
)
# Note: Qt libraries are NOT in PXVIEW_LINK_LIBS — they are linked per-target:
#   - pxview-core links ${QT_CORE_LIBS} (no Qt::Widgets, no Qt::Svg)
#   - PXView executable links ${QT_GUI_LIBS} (Qt::Widgets + Qt::Svg)

if(STATIC_PKGDEPS_LIBS)
	link_directories(${PKGDEPS_STATIC_LIBRARY_DIRS})
	list(APPEND PXVIEW_LINK_LIBS ${PKGDEPS_STATIC_LIBRARIES})
if(WIN32)
	# Workaround for a MinGW linking issue.
	list(APPEND PULSEVIEW_LINK_LIBS "-llzma -llcms2")
endif()
else()
	link_directories(${PKGDEPS_LIBRARY_DIRS})
	list(APPEND PXVIEW_LINK_LIBS ${PKGDEPS_LIBRARIES})
endif()
