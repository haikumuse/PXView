##
## This file is part of the DSView project.
##
## Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
## Copyright (C) 2012-2013 Alexandru Gagniuc <mr.nuke.me@gmail.com>
## Copyright (C) 2013-2022 DreamSourceLab <support@dreamsourcelab.com>
##
## This program is free software: you can redistribute it and/or modify
## it under the terms of the GNU General Public License as published by
## the Free Software Foundation, either version 2 of the License, or
## (at your option) any later version.
##
## This program is distributed in the hope that it will be useful,
## but WITHOUT ANY WARRANTY; without even the implied warranty of
## MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
## GNU General Public License for more details.
##
## You should have received a copy of the GNU General Public License
## along with this program.  If not, see <http://www.gnu.org/licenses/>.
##

cmake_minimum_required(VERSION 3.5)

project(DSView)

#===============================================================================
#= Config Header
#-------------------------------------------------------------------------------

set(DS_TITLE PXView)
set(DS_DESCRIPTION "A GUI for instruments of PXTOOL")

set(DS_VERSION_MAJOR 1)
set(DS_VERSION_MINOR 5)
set(DS_VERSION_MICRO 0)
set(DS_VERSION_STRING  ${DS_VERSION_MAJOR}.${DS_VERSION_MINOR}.${DS_VERSION_MICRO})

configure_file (
	${PROJECT_SOURCE_DIR}/DSView/config.h.in
	${PROJECT_BINARY_DIR}/DSView/config.h
)

#===============================================================================
#=pkg config
#-------------------------------------------------------------------------------
include(FindPkgConfig)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake") 
#list(APPEND CMAKE_PREFIX_PATH "xxx.cmake find path") 

find_package(PkgConfig)

if(NOT PKG_CONFIG_FOUND)
	message(FATAL_ERROR  "Please install pkg-config!")
endif()

#===============================================================================
#= User Options
#-------------------------------------------------------------------------------

set(DISABLE_WERROR TRUE) #Build without -Werror
set(ENABLE_SIGNALS TRUE) #Build with UNIX signals
set(ENABLE_COTIRE FALSE) #Enable cotire
set(ENABLE_TESTS  FALSE) #Enable unit tests
set(STATIC_PKGDEPS_LIBS FALSE) #Statically link to (pkg-config) libraries

if(WIN32)
	# On Windows/MinGW we need to statically link to libraries.
	# This option is user configurable, but enable it by default on win32.
	set(STATIC_PKGDEPS_LIBS TRUE)

	# Windows does not support UNIX signals.
	set(ENABLE_SIGNALS FALSE)
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif()

#===============================================================================
#= first search path
#-------------------------------------------------------------------------------
include_directories(
   ./DSView
   ./libsigrok4DSL
   ./libsigrokdecode4DSL
   ./common
)

#===============================================================================
#= glib-2.0
#-------------------------------------------------------------------------------
pkg_search_module(GLIB glib-2.0)

if(NOT GLIB_FOUND)
	message(FATAL_ERROR  "Please install glib!")
endif()

message("----- glib-2.0:")
message(STATUS "	 includes:" ${GLIB_INCLUDE_DIRS})
message(STATUS "	 libraries:" ${GLIB_LIBDIR}/libglib-2.0.*)
include_directories(${GLIB_INCLUDE_DIRS})
link_directories(${GLIB_LIBDIR})

#===============================================================================
#= python3
#-------------------------------------------------------------------------------

#find_package(Python3 COMPONENTS Development QUIET)

if (Python3_FOUND)
	message("----- 1python3:")
	message(STATUS "	 includes:" ${Python3_INCLUDE_DIRS})
	message(STATUS "	 libraries:" ${Python3_LIBRARIES})
	include_directories(${Python3_INCLUDE_DIRS})
	set(PY_LIB ${Python3_LIBRARIES})
else()
     find_package(PythonLibs 3 QUIET)

     if(PYTHONLIBS_FOUND)
     	message("----- 2python(${PYTHONLIBS_VERSION_STRING}):")
	message(STATUS "	 includes:" ${PYTHON_INCLUDE_DIRS})
	message(STATUS "	 libraries:" ${PYTHON_LIBRARIES})
	include_directories(${PYTHON_INCLUDE_DIRS})
	set(PY_LIB ${PYTHON_LIBRARIES})
     else()
     	message(FATAL_ERROR  "Please install lib python3!")
     endif()
endif()
  
#===============================================================================
#= FFTW
#-------------------------------------------------------------------------------
find_package(FFTW)

if(NOT FFTW_FOUND)
	message(FATAL_ERROR  "Please install lib fftw!")
endif()

message("----- FFTW:")
message(STATUS "	 includes:" ${FFTW_INCLUDE_DIRS})
message(STATUS "	 libraries:" ${FFTW_LIBRARIES})
include_directories(${FFTW_INCLUDE_DIRS})

#===============================================================================
#= libusb-1.0
#-------------------------------------------------------------------------------
find_package(libusb-1.0)

if(NOT LIBUSB_1_FOUND)
	message(FATAL_ERROR  "Please install libusb!")
endif()

message("----- libusb-1.0:")
message(STATUS "	 includes:" ${LIBUSB_1_INCLUDE_DIRS})
message(STATUS "	 libraries:" ${LIBUSB_1_LIBRARIES})
include_directories(${LIBUSB_1_INCLUDE_DIRS})

#===============================================================================
#= zlib
#-------------------------------------------------------------------------------
find_package(ZLIB QUIET)

if(NOT ZLIB_FOUND)
	message(FATAL_ERROR  "Please install zlib!")
endif()

message("----- zlib:")
message(STATUS "	 includes:" ${ZLIB_INCLUDE_DIRS})
message(STATUS "	 libraries:" ${ZLIB_LIBRARIES})
include_directories(${ZLIB_INCLUDE_DIRS})

#===============================================================================
#= Qt5 or Qt6
#-------------------------------------------------------------------------------
if(APPLE)
	set(QT_VERSION_FORCE Qt6)
endif()


set(QT_VERSION_FORCE "" CACHE STRING "Force the use of Qt6 by setting this option to 6")

if(NOT QT_VERSION_FORCE OR "${QT_VERSION_FORCE}" STREQUAL "5")
	find_package(Qt5Core QUIET)
	if(Qt5Core_FOUND)
		message("----- Qt5:")
		message(STATUS "	 includes:" ${Qt5Core_INCLUDE_DIRS})
		#find_package(Qt5WinExtras REQUIRED)
		find_package(Qt5Widgets REQUIRED)
		find_package(Qt5Gui REQUIRED)
		find_package(Qt5Svg REQUIRED)
		find_package(Qt5Concurrent REQUIRED)
		# win32
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5Widgets_EXECUTABLE_COMPILE_FLAGS}")
		if (WIN32)
			find_package(Qt5WinExtras REQUIRED)
			message(STATUS "	 libraries:" ${Qt5WinExtras_LIBRARIES})
			set(QT_INCLUDE_DIRS ${Qt5Gui_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS} ${Qt5WinExtras_INCLUDE_DIRS} ${Qt5Svg_INCLUDE_DIRS} ${Qt5Concurrent_INCLUDE_DIRS})
		set(QT_LIBRARIES Qt5::Gui Qt5::Widgets Qt5::WinExtras Qt5::Svg Qt5::Concurrent)
			add_definitions(${Qt5Gui_DEFINITIONS} ${Qt5Widgets_DEFINITIONS} ${Qt5WinExtras_DEFINITIONS}  ${Qt5Svg_DEFINITIONS} )
		else()
			set(QT_INCLUDE_DIRS ${Qt5Gui_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS} ${Qt5Concurrent_INCLUDE_DIRS})
		set(QT_LIBRARIES Qt5::Gui Qt5::Widgets Qt5::Concurrent)
			add_definitions(${Qt5Gui_DEFINITIONS} ${Qt5Widgets_DEFINITIONS})
		endif()
	endif()
endif()

# Qt6 support disabled for Linux build
#if(NOT Qt5Core_FOUND OR "${QT_VERSION_FORCE}" STREQUAL "6")
#	find_package(Qt6 REQUIRED COMPONENTS Core)
#    message("----- Qt6:")
#	message(STATUS "	 includes:" ${Qt6Core_INCLUDE_DIRS})
#        find_package(Qt6Widgets REQUIRED)
#        find_package(Qt6Gui REQUIRED)
#        find_package(Qt6Svg REQUIRED)
#	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt6Widgets_EXECUTABLE_COMPILE_FLAGS}")
#	set(QT_INCLUDE_DIRS ${Qt6Gui_INCLUDE_DIRS} ${Qt6Widgets_INCLUDE_DIRS})
#        set(QT_LIBRARIES Qt6::Gui Qt6::Widgets Qt6::Svg)
#	add_definitions(${Qt6Gui_DEFINITIONS} ${Qt6Widgets_DEFINITIONS})
#
#	if(APPLE)
#		find_package(Qt6DBus REQUIRED)
#		set(QT_LIBRARIES ${QT_LIBRARIES} Qt6::DBus)
#	endif()
#endif()

if(NOT Qt5Core_FOUND)
	message("Error! Qt5 can't be found, if it has been installed, please append the install path to CMAKE_PREFIX_PATH, the command as:")
	message("list(APPEND CMAKE_PREFIX_PATH \"xxx install path\")" )
	message(FATAL_ERROR  "Can't find Qt5!")
endif()


#===============================================================================
#= boost
#-------------------------------------------------------------------------------
find_package(Boost 1.42 QUIET)

if(NOT Boost_FOUND)
	message(FATAL_ERROR  "Please install boost!")
endif()

message("----- boost:")
message(STATUS "	 includes:" ${Boost_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS})

#===============================================================================
#= Dependencies
#-------------------------------------------------------------------------------

find_package(Threads)

#===============================================================================
#= DSView sources
#-------------------------------------------------------------------------------

set(DSView_SOURCES
    DSView/main.cpp
    DSView/dsapplication.cpp
    DSView/pv/log.cpp
    DSView/pv/sigsession.cpp
    DSView/pv/sessionmanager.cpp
    DSView/pv/mainwindow.cpp
    DSView/pv/data/snapshot.cpp
    DSView/pv/data/signaldata.cpp
    DSView/pv/data/logicsnapshot.cpp
    DSView/pv/data/analogsnapshot.cpp
    DSView/pv/data/sessionsnapshot.cpp
    DSView/pv/data/sessiondocument.cpp
    DSView/pv/dialogs/deviceoptions.cpp
    DSView/pv/prop/property.cpp
    DSView/pv/prop/int.cpp
    DSView/pv/prop/enum.cpp
    DSView/pv/prop/double.cpp
    DSView/pv/prop/bool.cpp
    DSView/pv/prop/binding/binding.cpp
    DSView/pv/toolbars/samplingbar.cpp
    DSView/pv/view/viewport.cpp
    DSView/pv/view/view.cpp
    DSView/pv/view/timemarker.cpp
    DSView/pv/view/signal.cpp
    DSView/pv/view/ruler.cpp
    DSView/pv/view/logicsignal.cpp
    DSView/pv/view/header.cpp
    DSView/pv/view/cursor.cpp
    DSView/pv/view/analogsignal.cpp
    DSView/pv/prop/binding/deviceoptions.cpp
    DSView/pv/toolbars/trigbar.cpp
    DSView/pv/toolbars/filebar.cpp
    DSView/pv/dock/protocoldock.cpp
    DSView/pv/dock/triggerdock.cpp
    DSView/pv/dock/measuredock.cpp
    DSView/pv/dock/searchdock.cpp
    DSView/pv/dock/deviceoptionsdock.cpp
    DSView/pv/toolbars/logobar.cpp
    DSView/pv/dialogs/about.cpp
    DSView/pv/dialogs/search.cpp
    DSView/pv/data/dsosnapshot.cpp
    DSView/pv/view/dsosignal.cpp
    DSView/pv/view/dsldial.cpp
    DSView/pv/dock/dsotriggerdock.cpp
    DSView/pv/view/trace.cpp
    DSView/pv/view/selectableitem.cpp
    DSView/pv/data/decoderstack.cpp
    DSView/pv/data/decode/rowdata.cpp
    DSView/pv/data/decode/row.cpp
    DSView/pv/data/decode/decoder.cpp
    DSView/pv/data/decode/annotation.cpp
    DSView/pv/view/decodetrace.cpp
    DSView/pv/prop/binding/decoderoptions.cpp
    DSView/pv/widgets/fakelineedit.cpp
    DSView/pv/widgets/searchpatterninput.cpp
    DSView/pv/widgets/decodermenu.cpp
    DSView/pv/widgets/decodergroupbox.cpp
    DSView/pv/prop/string.cpp
    DSView/pv/dialogs/storeprogress.cpp
    DSView/pv/storesession.cpp
    DSView/pv/view/devmode.cpp
    DSView/pv/dialogs/waitingdialog.cpp
    DSView/pv/dialogs/dsomeasure.cpp
    DSView/pv/dialogs/calibration.cpp
    DSView/pv/data/decodermodel.cpp
    DSView/pv/dialogs/protocollist.cpp
    DSView/pv/dialogs/protocolexp.cpp
    DSView/pv/dialogs/fftoptions.cpp
    DSView/pv/data/mathstack.cpp
    DSView/pv/view/mathtrace.cpp   
    DSView/pv/toolbars/titlebar.cpp
    DSView/pv/mainframe.cpp
    DSView/pv/submainframe.cpp
    DSView/pv/widgets/border.cpp
    DSView/pv/widgets/slidingdrawer.cpp
    DSView/pv/dialogs/dsmessagebox.cpp
    DSView/pv/dialogs/shadow.cpp
    DSView/pv/dialogs/dsdialog.cpp
    DSView/pv/dialogs/interval.cpp
    DSView/pv/prop/binding/probeoptions.cpp
    DSView/pv/view/viewstatus.cpp
    DSView/pv/dialogs/lissajousoptions.cpp
    DSView/pv/view/lissajoustrace.cpp
    DSView/pv/view/spectrumtrace.cpp
    DSView/pv/data/spectrumstack.cpp
    DSView/pv/dialogs/mathoptions.cpp
    DSView/pv/dialogs/regionoptions.cpp
    DSView/pv/view/xcursor.cpp
    DSView/pv/dock/protocoldock.cpp
    DSView/pv/data/decoderstack.cpp
    DSView/pv/data/decode/annotation.cpp
    DSView/pv/data/decode/decoder.cpp
    DSView/pv/data/decode/row.cpp
    DSView/pv/data/decode/rowdata.cpp
    DSView/pv/prop/binding/decoderoptions.cpp
    DSView/pv/view/decodetrace.cpp
    DSView/pv/widgets/decodergroupbox.cpp
    DSView/pv/widgets/decodermenu.cpp
    DSView/pv/config/appconfig.cpp
    DSView/pv/appcontrol.cpp
    DSView/pv/dstimer.cpp
    DSView/pv/eventobject.cpp
    DSView/pv/ZipMaker.cpp
    DSView/pv/data/decode/annotationrestable.cpp
    DSView/pv/data/decode/decoderstatus.cpp
    DSView/pv/dock/protocolitemlayer.cpp
    DSView/pv/ui/msgbox.cpp
    DSView/pv/ui/dscombobox.cpp
    DSView/pv/dsvdef.cpp
    DSView/pv/dialogs/applicationpardlg.cpp
    DSView/pv/dock/keywordlineedit.cpp
    DSView/pv/dock/searchcombobox.cpp
    DSView/pv/dialogs/decoderoptionsdlg.cpp
    DSView/pv/utility/encoding.cpp
    DSView/pv/utility/path.cpp
    DSView/pv/utility/array.cpp
    DSView/pv/deviceagent.cpp
    DSView/pv/ui/langresource.cpp
    DSView/pv/ui/fn.cpp
    DSView/pv/ui/xtoolbutton.cpp
    DSView/pv/ui/draggabletabbar.cpp
    DSView/pv/ui/draggabletabwidget.cpp
    DSView/pv/ui/uimanager.cpp
    DSView/pv/ui/popupdlglist.cpp
    DSView/pv/tabcontext.cpp
)

set(UIS
)

if(APPLE)
	qt_wrap_ui(UI_HEADERS ${UIS})
else()
	if(NOT QT_VERSION_FORCE OR "${QT_VERSION_FORCE}" STREQUAL "5")
		find_package(Qt6Core QUIET)
		if(Qt6Core)
			qt6_wrap_ui(UI_HEADERS ${UIS})
		else()
			qt5_wrap_ui(UI_HEADERS ${UIS})
		endif()
	endif()
endif()

# Windows-specific QT source files
if(WIN32)
	list(APPEND DSView_SOURCES
		DSView/pv/winnativewidget.cpp
		DSView/pv/winshadow.cpp
	)
endif ()

set(DSView_HEADERS
    DSView/mystyle.h
    DSView/pv/log.h
    DSView/pv/sigsession.h
    DSView/pv/sessionmanager.h
    DSView/pv/interface/icontextaware.h
    DSView/pv/mainwindow.h
    DSView/pv/dialogs/deviceoptions.h
    DSView/pv/prop/property.h
    DSView/pv/prop/int.h
    DSView/pv/prop/enum.h
    DSView/pv/prop/double.h
    DSView/pv/prop/bool.h
    DSView/pv/toolbars/samplingbar.h
    DSView/pv/view/viewport.h
    DSView/pv/view/view.h
    DSView/pv/view/timemarker.h
    DSView/pv/view/ruler.h
    DSView/pv/view/header.h
    DSView/pv/view/cursor.h
    DSView/pv/toolbars/trigbar.h
    DSView/pv/toolbars/filebar.h
    DSView/pv/dock/protocoldock.h
    DSView/pv/dock/triggerdock.h
    DSView/pv/dock/measuredock.h
    DSView/pv/dock/searchdock.h
    DSView/pv/dock/deviceoptionsdock.h
    DSView/pv/toolbars/logobar.h
    DSView/pv/dialogs/about.h
    DSView/pv/dialogs/search.h
    DSView/pv/dock/dsotriggerdock.h
    DSView/pv/view/trace.h
    DSView/pv/view/selectableitem.h
    DSView/pv/data/decoderstack.h
    DSView/pv/view/decodetrace.h
    DSView/pv/widgets/fakelineedit.h
    DSView/pv/widgets/searchpatterninput.h
    DSView/pv/widgets/decodermenu.h
    DSView/pv/widgets/decodergroupbox.h
    DSView/pv/prop/string.h
    DSView/pv/dialogs/storeprogress.h
    DSView/pv/storesession.h
    DSView/pv/view/devmode.h
    DSView/pv/dialogs/waitingdialog.h
    DSView/pv/dialogs/dsomeasure.h
    DSView/pv/dialogs/calibration.h
    DSView/pv/dialogs/protocollist.h
    DSView/pv/dialogs/protocolexp.h
    DSView/pv/dialogs/fftoptions.h
    DSView/pv/data/mathstack.h
    DSView/pv/view/mathtrace.h
    DSView/pv/view/viewstatus.h
    DSView/pv/toolbars/titlebar.h
    DSView/pv/mainframe.h
    DSView/pv/submainframe.h
    DSView/pv/widgets/border.h
    DSView/pv/widgets/slidingdrawer.h
    DSView/pv/dialogs/dsmessagebox.h
    DSView/pv/dialogs/shadow.h
    DSView/pv/dialogs/dsdialog.h
    DSView/pv/dialogs/interval.h
    DSView/pv/dialogs/lissajousoptions.h
    DSView/pv/view/lissajoustrace.h
    DSView/pv/view/spectrumtrace.h
    DSView/pv/data/spectrumstack.h
    DSView/pv/data/decodermodel.h
    DSView/pv/data/datasource.h
    DSView/pv/data/sessionsnapshot.h
    DSView/pv/data/sessiondocument.h
    DSView/pv/dialogs/mathoptions.h
    DSView/pv/dialogs/regionoptions.h
    DSView/pv/view/xcursor.h
    DSView/pv/view/signal.h
    DSView/pv/view/logicsignal.h
    DSView/pv/view/analogsignal.h
    DSView/pv/view/dsosignal.h 
    DSView/pv/dock/protocoldock.h
    DSView/pv/data/decoderstack.h
    DSView/pv/view/decodetrace.h
    DSView/pv/widgets/decodergroupbox.h
    DSView/pv/widgets/decodermenu.h
    DSView/pv/config/appconfig.h
    DSView/pv/appcontrol.h
    DSView/pv/dstimer.h
    DSView/pv/eventobject.h
    DSView/pv/ZipMaker.h
    DSView/pv/data/decode/annotationrestable.h
    DSView/pv/data/decode/decoderstatus.h
    DSView/pv/dock/protocolitemlayer.h
    DSView/pv/ui/msgbox.h
    DSView/pv/ui/dscombobox.h
    DSView/pv/dsvdef.h
    DSView/pv/dialogs/applicationpardlg.h
    DSView/pv/dock/keywordlineedit.h
    DSView/pv/dock/searchcombobox.h
    DSView/pv/dialogs/decoderoptionsdlg.h
    DSView/pv/utility/encoding.h
    DSView/pv/utility/path.h
    DSView/pv/utility/array.h
    DSView/pv/deviceagent.h
    DSView/pv/ui/fn.h
    DSView/pv/ui/xtoolbutton.h
    DSView/pv/ui/draggabletabbar.h
    DSView/pv/ui/draggabletabwidget.h
    DSView/pv/tabcontext.h
    ${UI_HEADERS}
)
 
# Windows-specific QT headers
if(WIN32)
	list(APPEND DSView_HEADERS
		DSView/pv/winnativewidget.h
		DSView/pv/winshadow.h
	)
endif ()

#===============================================================================
#= Debug Helper option
#-------------------------------------------------------------------------------
option(ENABLE_DEBUG_HELPER "Enable UI debug hover helper" OFF)
if(ENABLE_DEBUG_HELPER)
	list(APPEND DSView_SOURCES DSView/pv/ui/debughelper.cpp)
	list(APPEND DSView_HEADERS DSView/pv/ui/debughelper.h)
	add_definitions(-DENABLE_DEBUG_HELPER)
endif()

#===============================================================================
#= libsigrok4DSL source
#-------------------------------------------------------------------------------
set(libsigrok4DSL_SOURCES
    libsigrok4DSL/version.c
    libsigrok4DSL/strutil.c
    libsigrok4DSL/std.c
    libsigrok4DSL/session_driver.c
    libsigrok4DSL/session.c
    libsigrok4DSL/log.c
    libsigrok4DSL/hwdriver.c
    libsigrok4DSL/error.c
    libsigrok4DSL/backend.c
    libsigrok4DSL/output/output.c
    libsigrok4DSL/input/input.c
    libsigrok4DSL/hardware/demo/demo.c
    libsigrok4DSL/input/in_binary.c
    libsigrok4DSL/input/in_vcd.c
    libsigrok4DSL/input/in_wav.c
    libsigrok4DSL/output/csv.c
    libsigrok4DSL/output/gnuplot.c
    libsigrok4DSL/output/srzip.c
    libsigrok4DSL/output/vcd.c
    libsigrok4DSL/hardware/DSL/dslogic.c
    libsigrok4DSL/hardware/common/usb.c
    libsigrok4DSL/hardware/common/ezusb.c
    libsigrok4DSL/trigger.c
    libsigrok4DSL/dsdevice.c
    libsigrok4DSL/hardware/DSL/dscope.c
    libsigrok4DSL/hardware/DSL/command.c
    libsigrok4DSL/hardware/DSL/dsl.c
    libsigrok4DSL/hardware/pxlogic/pxlogic.c
    libsigrok4DSL/hardware/pxlogic/usb_ctrl.c
    libsigrok4DSL/lib_main.c
)

set(libsigrok4DSL_HEADERS
    libsigrok4DSL/version.h
    libsigrok4DSL/libsigrok-internal.h
    libsigrok4DSL/libsigrok.h
    libsigrok4DSL/config.h
    libsigrok4DSL/hardware/DSL/command.h
    libsigrok4DSL/hardware/DSL/dsl.h
)
 
#===============================================================================
#= libsigrokdecode4DSL source
#-------------------------------------------------------------------------------
set(libsigrokdecode4DSL_SOURCES
    libsigrokdecode4DSL/type_decoder.c
    libsigrokdecode4DSL/srd.c
    libsigrokdecode4DSL/module_sigrokdecode.c
    libsigrokdecode4DSL/decoder.c
    libsigrokdecode4DSL/dll_registry.c
    libsigrokdecode4DSL/error.c
    libsigrokdecode4DSL/exception.c
    libsigrokdecode4DSL/instance.c
    libsigrokdecode4DSL/log.c
    libsigrokdecode4DSL/session.c
    libsigrokdecode4DSL/util.c
    libsigrokdecode4DSL/version.c
    libsigrokdecode4DSL/c_decoder_api.c
)

set(libsigrokdecode4DSL_HEADERS
    libsigrokdecode4DSL/libsigrokdecode-internal.h
    libsigrokdecode4DSL/libsigrokdecode.h
    libsigrokdecode4DSL/config.h
    libsigrokdecode4DSL/version.h
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
#= compile config
#-------------------------------------------------------------------------------

set(DSView_FORMS
)

set(DSView_RESOURCES
    DSView/DSView.qrc
    DSView/themes/breeze.qrc
    DSView/languages/language.qrc
)


if(WIN32)
	# Use the DSView icon for the DSView.exe executable.
	set(CMAKE_RC_COMPILE_OBJECT "${CMAKE_RC_COMPILER} -O coff -I${CMAKE_CURRENT_SOURCE_DIR} <SOURCE> <OBJECT>")
	enable_language(RC)
	# app icon
        list(APPEND DSView_SOURCES applogo.rc)
	# MinGW Unicode support
	add_definitions(-municode)
endif()

if(Qt5Core_FOUND)
	qt5_wrap_cpp(DSView_HEADERS_MOC ${DSView_HEADERS})
	qt5_wrap_ui(DSView_FORMS_HEADERS ${DSView_FORMS})
	qt5_add_resources(DSView_RESOURCES_RCC ${DSView_RESOURCES})
endif()




add_definitions(${QT_DEFINITIONS})
add_definitions(-Wall -Wextra -Wno-return-type -Wno-ignored-qualifiers)

if(NOT DISABLE_WERROR)
        add_definitions(-Werror)
endif()

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")


include_directories(
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}
	${Boost_INCLUDE_DIRS}
	${QT_INCLUDE_DIRS}
)


if(STATIC_PKGDEPS_LIBS)
	include_directories(${PKGDEPS_STATIC_INCLUDE_DIRS})
else()
	include_directories(${PKGDEPS_INCLUDE_DIRS})
endif()

#===============================================================================
#= Release flags
#-------------------------------------------------------------------------------
if(${CMAKE_BUILD_TYPE} STREQUAL "Release")
    set(CMAKE_CXX_FLAGS "-Wall -Wextra")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
    add_compile_options(-O3)
endif()
set(CMAKE_CXX_FLAGS_DEBUG "-g")

#===============================================================================
#= Linker Configuration
#-------------------------------------------------------------------------------


set(DSVIEW_LINK_LIBS
	-lz
	-lglib-2.0
	${CMAKE_THREAD_LIBS_INIT}
	${QT_LIBRARIES}
	${LIBUSB_1_LIBRARIES}
	${FFTW_LIBRARIES}
	${PY_LIB}
)

if(STATIC_PKGDEPS_LIBS)
	link_directories(${PKGDEPS_STATIC_LIBRARY_DIRS})
	list(APPEND DSVIEW_LINK_LIBS ${PKGDEPS_STATIC_LIBRARIES})
if(WIN32)
	# Workaround for a MinGW linking issue.
	list(APPEND PULSEVIEW_LINK_LIBS "-llzma -llcms2")
endif()
else()
	link_directories(${PKGDEPS_LIBRARY_DIRS})
	list(APPEND DSVIEW_LINK_LIBS ${PKGDEPS_LIBRARIES})
endif()

add_executable(${PROJECT_NAME}
	${common_SOURCES}
	${DSView_SOURCES}
	${DSView_HEADERS_MOC}
	${DSView_FORMS_HEADERS}
	${DSView_RESOURCES_RCC}
	${libsigrok4DSL_SOURCES}
	${libsigrokdecode4DSL_SOURCES}
)

target_link_libraries(${PROJECT_NAME} ${DSVIEW_LINK_LIBS})

if(NOT WIN32 AND NOT APPLE)
	target_link_libraries(${PROJECT_NAME} "-Wl,--export-dynamic")
endif()

if(WIN32)
# Pass -mwindows so that no "DOS box" will open when PulseView is started.
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "-mwindows")
endif()

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUNDLE TRUE
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER com.pxlogic.${PROJECT_NAME}
        MACOSX_BUNDLE_BUNDLE_NAME ${PROJECT_NAME}
        MACOSX_BUNDLE_BUNDLE_VERSION ${DS_VERSION_STRING}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${DS_VERSION_STRING}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/mac_appbundle_template.plist.in
        MACOSX_BUNDLE_ICON_FILE DSView.icns
    )
endif()

if(ENABLE_COTIRE)
	include(cotire)
	cotire(${PROJECT_NAME})
endif()

message(STATUS "Output dir: ${CMAKE_CURRENT_SOURCE_DIR}/build.dir")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build.dir")

#===============================================================================
#= C decoder DLL/SO build rules
#-------------------------------------------------------------------------------

set(C_DECODER_OUTPUT_DIR "${EXECUTABLE_OUTPUT_PATH}/decoders/c_decoders")

set(C_DECODERS spi_c i2c_c uart_c can_c jtag_c swd_c onewire_c i2s_c lin_c hdlc_c microwire_c mdio_c ps2_c dmx512_c nrzi_c ir_nec_c ir_rc5_c dcf77_c cec_c spdif_c usb_signalling_c 4b5b_c can_fd_c iso7816_c lpc_c dali_c c2_c)

foreach(dec ${C_DECODERS})
	add_library(decoder_${dec} MODULE
		${CMAKE_CURRENT_SOURCE_DIR}/libsigrokdecode4DSL/c_decoders/${dec}.c
		${CMAKE_CURRENT_SOURCE_DIR}/libsigrokdecode4DSL/c_decoder_api.c
	)

	target_include_directories(decoder_${dec} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/libsigrokdecode4DSL
		${CMAKE_CURRENT_SOURCE_DIR}/common
		${GLIB_INCLUDE_DIRS}
		${PYTHON_INCLUDE_DIRS}
	)

	target_compile_definitions(decoder_${dec} PRIVATE
		SRD_C_DECODER_DLL
	)

	target_link_libraries(decoder_${dec}
		-lglib-2.0
	)

	set_target_properties(decoder_${dec} PROPERTIES
		OUTPUT_NAME ${dec}
		LIBRARY_OUTPUT_DIRECTORY ${C_DECODER_OUTPUT_DIR}
		RUNTIME_OUTPUT_DIRECTORY ${C_DECODER_OUTPUT_DIR}
	)
endforeach()

#===============================================================================
#= Copy language files to output directory (for development)
#-------------------------------------------------------------------------------
if(WIN32)
    # Create lang directories in output directory
    file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/lang/cn)
    file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/lang/en)

    # Copy language files
    file(GLOB CN_LANG_FILES ${CMAKE_CURRENT_SOURCE_DIR}/lang/cn/*.json)
    file(GLOB EN_LANG_FILES ${CMAKE_CURRENT_SOURCE_DIR}/lang/en/*.json)

    foreach(file ${CN_LANG_FILES})
        file(COPY ${file} DESTINATION ${EXECUTABLE_OUTPUT_PATH}/lang/cn)
    endforeach()

    foreach(file ${EN_LANG_FILES})
        file(COPY ${file} DESTINATION ${EXECUTABLE_OUTPUT_PATH}/lang/en)
    endforeach()

    message(STATUS "Language files copied to: ${EXECUTABLE_OUTPUT_PATH}/lang")

    # Copy decoder files to output directory
    file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/decoders)
    file(GLOB DECODER_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/package/decoders/*)
    foreach(dir ${DECODER_DIRS})
        if(IS_DIRECTORY ${dir})
            get_filename_component(dir_name ${dir} NAME)
            file(COPY ${dir} DESTINATION ${EXECUTABLE_OUTPUT_PATH}/decoders)
        endif()
    endforeach()

    message(STATUS "Decoder files copied to: ${EXECUTABLE_OUTPUT_PATH}/decoders")

    file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/decoders/c_decoders)

    message(STATUS "C decoder output directory: ${EXECUTABLE_OUTPUT_PATH}/decoders/c_decoders")
endif()

#===============================================================================
#= Installation
#-------------------------------------------------------------------------------

# Install the executable.
if(APPLE)
    install(TARGETS ${PROJECT_NAME} BUNDLE DESTINATION .)

    # Normally resources in macOS app bundle are packed at Contents/Resources
    set(MAC_RES_PREFIX ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app/Contents/Resources/)

    # Adding icon via add_executable / target_sources does not work, hack around this
    install(FILES DSView.icns DESTINATION ${MAC_RES_PREFIX})
else()
    install(TARGETS ${PROJECT_NAME} DESTINATION bin)
endif()
install(DIRECTORY DSView/res DESTINATION ${MAC_RES_PREFIX}share/DSView)
install(DIRECTORY DSView/demo DESTINATION ${MAC_RES_PREFIX}share/DSView)
install(FILES DSView/icons/logo.svg DESTINATION ${MAC_RES_PREFIX}share/DSView RENAME logo.svg)
install(FILES DSView/icons/logo.svg DESTINATION ${MAC_RES_PREFIX}share/icons/hicolor/scalable/apps RENAME dsview.svg)
install(FILES DSView/icons/logo.svg DESTINATION ${MAC_RES_PREFIX}share/pixmaps RENAME dsview.svg)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")	
	install(FILES DSView/DSView.desktop DESTINATION /usr/share/applications RENAME dsview.desktop)
	
	#add_compile_definitions(_DEFAULT_SOURCE)

	if(IS_DIRECTORY /usr/lib/udev/rules.d)
		install(FILES DSView/DreamSourceLab.rules DESTINATION /usr/lib/udev/rules.d RENAME 60-dreamsourcelab.rules)
	elseif(IS_DIRECTORY /lib/udev/rules.d)
		install(FILES DSView/DreamSourceLab.rules DESTINATION /lib/udev/rules.d RENAME 60-dreamsourcelab.rules)
	elseif(IS_DIRECTORY /etc/udev/rules.d)
		install(FILES DSView/DreamSourceLab.rules DESTINATION /etc/udev/rules.d RENAME 60-dreamsourcelab.rules)
	endif()

endif()

install(FILES NEWS25 DESTINATION ${MAC_RES_PREFIX}share/DSView RENAME NEWS25)
install(FILES NEWS31 DESTINATION ${MAC_RES_PREFIX}share/DSView RENAME NEWS31)
install(FILES ug25.pdf DESTINATION ${MAC_RES_PREFIX}share/DSView RENAME ug25.pdf)
install(FILES ug31.pdf DESTINATION ${MAC_RES_PREFIX}share/DSView RENAME ug31.pdf)

install(DIRECTORY libsigrokdecode4DSL/decoders DESTINATION ${MAC_RES_PREFIX}share/libsigrokdecode4DSL)

foreach(dec ${C_DECODERS})
	install(TARGETS decoder_${dec}
		DESTINATION ${MAC_RES_PREFIX}share/libsigrokdecode4DSL/c_decoders
	)
endforeach()

install(DIRECTORY lang DESTINATION ${MAC_RES_PREFIX}share/DSView)

#===============================================================================
#= Packaging (handled by CPack)
#-------------------------------------------------------------------------------

set(CPACK_PACKAGE_VERSION_MAJOR ${DS_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${DS_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${DS_VERSION_MICRO})
set(CPACK_PACKAGE_DESCRIPTION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/DSView/README)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/DSView/COPYING)
set(CPACK_SOURCE_IGNORE_FILES ${CMAKE_CURRENT_BINARY_DIR} ".gitignore" ".git")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
	"${CMAKE_PROJECT_NAME}-${DS_VERSION_MAJOR}.${DS_VERSION_MINOR}.${DS_VERSION_MICRO}")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_PACKAGE_CONTACT "913461865@qq.com")
include(CPack)

#===============================================================================
#= Tests
#-------------------------------------------------------------------------------

if(ENABLE_TESTS)
	add_subdirectory(test)
	enable_testing()
	add_test(test ${CMAKE_CURRENT_BINARY_DIR}/DSView/test/DSView-test)
endif(ENABLE_TESTS)


