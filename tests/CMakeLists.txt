#===============================================================================
#= PXView tests
#-------------------------------------------------------------------------------
# This directory is added via `add_subdirectory(tests)` from the top-level
# CMakeLists.txt when ENABLE_TESTS is ON.
#
# Contents:
#   - test_font.cpp  : font rendering test executable (Qt6::Widgets/Gui)
#   - mcp_json/*.json: JSON-RPC 2.0 request bodies driving the MCP HTTP API
#                      tests via run_mcp_case.py
#===============================================================================

find_package(Python3 COMPONENTS Interpreter)

#-------------------------------------------------------------------------------
# test_font - font rendering test
#-------------------------------------------------------------------------------
add_executable(test_font test_font.cpp)
set_target_properties(test_font PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/PXView/tests"
)
target_link_libraries(test_font PRIVATE
	Qt6::Widgets
	Qt6::Gui
)

add_test(NAME test_font COMMAND test_font)

#-------------------------------------------------------------------------------
# MCP HTTP API tests - one ctest case per mcp_json/*.json file
#-------------------------------------------------------------------------------
file(GLOB MCP_JSON_CASES "${CMAKE_CURRENT_SOURCE_DIR}/mcp_json/*.json")

foreach(json_case ${MCP_JSON_CASES})
	get_filename_component(case_name "${json_case}" NAME_WE)
	add_test(
		NAME "mcp_${case_name}"
		COMMAND ${Python3_EXECUTABLE}
				"${CMAKE_CURRENT_SOURCE_DIR}/run_mcp_case.py"
				"${json_case}"
	)
	set_tests_properties(
		"mcp_${case_name}"
		PROPERTIES
			WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
	)
endforeach()
