##
## This file is part of the PXView project.
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

cmake_minimum_required(VERSION 3.10)

project(PXView)

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
	${PROJECT_SOURCE_DIR}/PXView/config.h.in
	${PROJECT_BINARY_DIR}/PXView/config.h
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
#= Modular includes — split into cmake/*.cmake files (C2)
# Variable visibility is preserved across include() (same scope), so the
# split is purely mechanical: same options, sources, flags, install rules.
#-------------------------------------------------------------------------------
include(${CMAKE_SOURCE_DIR}/cmake/options.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/deps.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/flags.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/core_sources.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/gui_sources.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/libsigrok.cmake)

#===============================================================================
#= libsigrokstd — upstream libsigrok 0.6.0 as a SHARED library (100% unmodified
#= upstream source). Driver registration via upstream SR_REGISTER_DEV_DRIVER
#= section mechanism. Linked by PXView to expose 81 additional drivers.
add_subdirectory(libsigrokstd)

#===============================================================================
#= pxview-core static library — Core layer (no Qt Widgets dependency)
#-------------------------------------------------------------------------------
# Compiles the Core layer sources (SigSession, DeviceAgent, Data layer, API
# layer, utility, config) into a standalone static library that depends only
# on Qt6::Core/Gui/Network/Concurrent/WebSockets (NOT Qt6::Widgets/Svg).
# Q_OBJECT meta-object implementations (moc_*.cpp) for Core classes are compiled
# in the executable target below; the static library may carry unresolved
# meta-object symbols which get resolved at final executable link time.
add_library(pxview-core STATIC ${PXVIEW_CORE_SOURCES})
target_link_libraries(pxview-core PUBLIC
	${QT_CORE_LIBS}
	${PXVIEW_LINK_LIBS}
	nlohmann_json::nlohmann_json
)
# Expose libsigrokstd/bridge/ and libsigrokstd/include/ headers so
# sigsession.cpp / deviceagent.cpp (compiled into pxview-core) can include
# "srstd_pxview_glue.h" (which itself includes "srstd_export.h").
# PUBLIC so the executable inherits the include path too (currently unused
# there, but harmless and forward-compatible).
target_include_directories(pxview-core PUBLIC
	${CMAKE_SOURCE_DIR}/libsigrokstd/bridge
	${CMAKE_SOURCE_DIR}/libsigrokstd/include
)

#===============================================================================
#= PXView executable — GUI/View layer linking pxview-core
#-------------------------------------------------------------------------------
add_executable(${PROJECT_NAME}
	${common_SOURCES}
	${PXVIEW_GUI_SOURCES}
	${PXView_HEADERS_MOC}
	${PXView_HEADERS_NO_MOC}
	${PXView_FORMS_HEADERS}
	${PXView_RESOURCES_RCC}
	${libsigrok_SOURCES}
	${libsigrokdecode_SOURCES}
)

# Executable links pxview-core (which transitively provides Qt Core/Gui/Network
# /WebSockets + external libs + nlohmann_json via PUBLIC linkage) and adds the
# Qt Widgets + Svg modules that the View layer needs.
# libsigrokstd is a SHARED library; linking it pulls in the import library for
# libsigrokstd.dll. The bridge/ include path is added to pxview-core (PUBLIC)
# so sigsession.cpp / deviceagent.cpp (compiled into pxview-core) can find
# "bridge/srstd_pxview_glue.h"; PUBLIC propagates to the executable too.
target_link_libraries(${PROJECT_NAME}
	pxview-core
	libsigrokstd
	${QT_GUI_LIBS}
)

if(NOT WIN32 AND NOT APPLE)
	target_link_libraries(${PROJECT_NAME} "-Wl,--export-dynamic")
endif()

if(WIN32)
# Pass -mwindows so that no "DOS box" will open when PXView is started.
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
        MACOSX_BUNDLE_ICON_FILE PXView.icns
    )
endif()

if(ENABLE_COTIRE)
	include(cotire)
	cotire(${PROJECT_NAME})
endif()

message(STATUS "Output dir: ${CMAKE_CURRENT_SOURCE_DIR}/build.dir")
set(EXECUTABLE_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build.dir")

#===============================================================================
#= Decoders (C decoder DLLs + irmp + decoder_test) — needs EXECUTABLE_OUTPUT_PATH
#-------------------------------------------------------------------------------
include(${CMAKE_SOURCE_DIR}/cmake/decoders.cmake)

#===============================================================================
#= Installation + CPack packaging
#-------------------------------------------------------------------------------
include(${CMAKE_SOURCE_DIR}/cmake/install_packaging.cmake)

#===============================================================================
#= Tests
#-------------------------------------------------------------------------------

if(ENABLE_TESTS)
	add_subdirectory(tests)
	enable_testing()
endif(ENABLE_TESTS)

#add_executable(test_font test_font.cpp)
#target_link_libraries(test_font Qt6::Gui Qt6::Widgets)
