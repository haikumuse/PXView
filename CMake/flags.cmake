#===============================================================================
#= compile config (compiler flags / definitions / include dirs)
#-------------------------------------------------------------------------------

add_definitions(${QT_DEFINITIONS})

# Compiler warnings - only for C/C++, not for RC
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-return-type -Wno-ignored-qualifiers")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-return-type -Wno-ignored-qualifiers")

if(NOT DISABLE_WERROR)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Werror")
endif()

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c11")


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
    # -g embeds DWARF debug info so addr2line can symbolicate crash stacks.
    # Without it the crash report dialog can only show raw addresses.
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -g")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG -g")
    add_compile_options(-O3 -g)
endif()
set(CMAKE_CXX_FLAGS_DEBUG "-g")
