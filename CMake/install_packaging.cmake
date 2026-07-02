#===============================================================================
#= Copy language files to output directory (for development)
#-------------------------------------------------------------------------------
if(WIN32)
    # Create lang directories in output directory
    file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/lang/cn)
    file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/lang/cn/dec)
    file(MAKE_DIRECTORY ${EXECUTABLE_OUTPUT_PATH}/lang/en)

    # Copy language files
    file(GLOB CN_LANG_FILES ${CMAKE_CURRENT_SOURCE_DIR}/lang/cn/*.json)
    file(GLOB CN_DEC_LANG_FILES ${CMAKE_CURRENT_SOURCE_DIR}/lang/cn/dec/*.json)
    file(GLOB EN_LANG_FILES ${CMAKE_CURRENT_SOURCE_DIR}/lang/en/*.json)

    foreach(file ${CN_LANG_FILES})
        file(COPY ${file} DESTINATION ${EXECUTABLE_OUTPUT_PATH}/lang/cn)
    endforeach()

    foreach(file ${CN_DEC_LANG_FILES})
        file(COPY ${file} DESTINATION ${EXECUTABLE_OUTPUT_PATH}/lang/cn/dec)
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
#= Vite web client build (optional, run: cmake --build . --target webui)
#-------------------------------------------------------------------------------

find_program(NPM_EXECUTABLE npm)

if(NPM_EXECUTABLE)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/index.html
        COMMAND ${NPM_EXECUTABLE} install
        COMMAND ${NPM_EXECUTABLE} run build
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/web
        COMMENT "Building Vite web client..."
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/package.json
    )

    add_custom_target(webui
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/index.html
        COMMENT "Build the MCP web client (Vite)"
    )

    add_custom_target(install-webui
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/web/dist
            ${CMAKE_INSTALL_PREFIX}/bin/webui
        COMMENT "Copy web client to install directory"
        DEPENDS webui
    )

    message(STATUS "Web UI build target available: cmake --build . --target webui")
    message(STATUS "Web UI install target available: cmake --build . --target install-webui")
else()
    message(STATUS "npm not found, web UI build targets not available")
endif()

#===============================================================================
#= Installation
#-------------------------------------------------------------------------------

# Crash report symbolication tool (Windows only). addr2line.exe from mingw
# binutils translates runtime addresses into function/file:line using the
# DWARF debug info embedded in PXView.exe — dbghelp cannot read DWARF, so
# this is the only way to symbolicate MinGW-built binaries on Windows.
if(WIN32)
	find_program(ADDR2LINE_EXE
		NAMES addr2line.exe addr2line
		HINTS
			"$ENV{MSYSTEM_PREFIX}/bin"
			"$ENV{MINGW_PREFIX}/bin"
			"D:/msys64/mingw64/bin"
			"C:/msys64/mingw64/bin"
		PATH_SUFFIXES bin
	)
	if(ADDR2LINE_EXE)
		message(STATUS "addr2line found: ${ADDR2LINE_EXE}")
		install(PROGRAMS "${ADDR2LINE_EXE}" DESTINATION bin RENAME addr2line.exe)
	else()
		message(WARNING "addr2line.exe not found — crash report symbolication disabled. "
				"Install mingw binutils or copy addr2line.exe to bin/ manually.")
	endif()
endif()

# Install the executable.
if(APPLE)
    install(TARGETS ${PROJECT_NAME} BUNDLE DESTINATION .)

    # Normally resources in macOS app bundle are packed at Contents/Resources
    set(MAC_RES_PREFIX ${CMAKE_INSTALL_PREFIX}/${PROJECT_NAME}.app/Contents/Resources/)

    # Adding icon via add_executable / target_sources does not work, hack around this
    install(FILES PXView.icns DESTINATION ${MAC_RES_PREFIX})
else()
    install(TARGETS ${PROJECT_NAME} DESTINATION bin)
endif()
install(DIRECTORY PXView/res DESTINATION ${MAC_RES_PREFIX}share/PXView)
install(DIRECTORY PXView/demo DESTINATION ${MAC_RES_PREFIX}share/PXView)
install(FILES PXView/icons/logo.svg DESTINATION ${MAC_RES_PREFIX}share/PXView RENAME logo.svg)
install(FILES PXView/icons/logo.svg DESTINATION ${MAC_RES_PREFIX}share/icons/hicolor/scalable/apps RENAME pxview.svg)
install(FILES PXView/icons/logo.svg DESTINATION ${MAC_RES_PREFIX}share/pixmaps RENAME pxview.svg)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
	install(FILES PXView/PXView.desktop DESTINATION ${MAC_RES_PREFIX}share/applications RENAME pxview.desktop)

	#add_compile_definitions(_DEFAULT_SOURCE)

	# udev rules: install to system path for system prefixes, local prefix otherwise
	if(CMAKE_INSTALL_PREFIX STREQUAL "/usr" OR CMAKE_INSTALL_PREFIX STREQUAL "/usr/local")
		if(IS_DIRECTORY /usr/lib/udev/rules.d)
			install(FILES PXView/px.rules DESTINATION /usr/lib/udev/rules.d RENAME 60-px.rules)
		elseif(IS_DIRECTORY /lib/udev/rules.d)
			install(FILES PXView/px.rules DESTINATION /lib/udev/rules.d RENAME 60-px.rules)
		elseif(IS_DIRECTORY /etc/udev/rules.d)
			install(FILES PXView/px.rules DESTINATION /etc/udev/rules.d RENAME 60-px.rules)
		endif()
	else()
		install(FILES PXView/px.rules DESTINATION ${MAC_RES_PREFIX}lib/udev/rules.d RENAME 60-px.rules)
	endif()

endif()

install(DIRECTORY ${CMAKE_SOURCE_DIR}/doc/ DESTINATION ${MAC_RES_PREFIX}share/PXView)

install(DIRECTORY libsigrokdecode/decoders DESTINATION ${MAC_RES_PREFIX}share/libsigrokdecode)

foreach(dec ${C_DECODERS})
	install(TARGETS decoder_${dec}
		DESTINATION ${MAC_RES_PREFIX}share/libsigrokdecode/c_decoders
	)
endforeach()

install(TARGETS irmp DESTINATION ${MAC_RES_PREFIX}bin)

install(DIRECTORY lang DESTINATION ${MAC_RES_PREFIX}share/PXView)

# Install web client if it has been built
install(CODE "
    if(EXISTS \"${CMAKE_CURRENT_SOURCE_DIR}/web/dist/index.html\")
        file(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${MAC_RES_PREFIX}bin/webui\"
             TYPE DIRECTORY FILES \"${CMAKE_CURRENT_SOURCE_DIR}/web/dist/\")
        message(STATUS \"Installing web client to: \${CMAKE_INSTALL_PREFIX}/${MAC_RES_PREFIX}bin/webui\")
    else()
        message(STATUS \"Web client not built, skipping. Run: ninja webui\")
    endif()
")

#===============================================================================
#= Packaging (handled by CPack)
#-------------------------------------------------------------------------------

set(CPACK_PACKAGE_VERSION_MAJOR ${DS_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${DS_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${DS_VERSION_MICRO})
set(CPACK_PACKAGE_DESCRIPTION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/PXView/README)
set(CPACK_RESOURCE_FILE_LICENSE ${CMAKE_CURRENT_SOURCE_DIR}/PXView/COPYING)
set(CPACK_SOURCE_IGNORE_FILES ${CMAKE_CURRENT_BINARY_DIR} ".gitignore" ".git")
set(CPACK_SOURCE_PACKAGE_FILE_NAME
	"${CMAKE_PROJECT_NAME}-${DS_VERSION_MAJOR}.${DS_VERSION_MINOR}.${DS_VERSION_MICRO}")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_PACKAGE_CONTACT "913461865@qq.com")
include(CPack)
