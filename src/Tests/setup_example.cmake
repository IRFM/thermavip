#
# Setup test project
#
# Counterpart of src/Examples/setup_example.cmake for the test tree: same
# inclusion mechanism, installed into THERMAVIP_TEST_DIR, and it also registers
# the target with CTest.


set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

# Add SDK inlcude dirs
target_include_directories(${TARGET_PROJECT} PRIVATE ${THERMAVIP_INCLUDE_DIRS})
# Link with SDK
target_link_libraries(${TARGET_PROJECT} PRIVATE ${THERMAVIP_LIBRARIES})
# Add compiler flags plus Qt library
include(${THERMAVIP_COMPILER_FLAGS_FILE})

# CMAKE_CURRENT_LIST_DIR, not PROJECT_SOURCE_DIR: every test leaf calls
# project(), which redefines the latter.
target_include_directories(${TARGET_PROJECT} PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# Qt Test. compiler_flags.cmake has already set QT_VERSION_MAJOR.
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
target_link_libraries(${TARGET_PROJECT} PRIVATE Qt${QT_VERSION_MAJOR}::Test)

# On Windows each SDK DLL lives next to its own target, so a test launched by
# CTest would not find them. The directories are put on PATH at test time rather
# than copied next to the executable: a copy is only refreshed when the test
# target itself relinks, so a library rebuilt on its own left the test running
# against a stale DLL and reporting a result that was no longer true.
# Elsewhere the build-tree RPATH is enough.
set(_vip_test_env_mod "")
if(WIN32)
	foreach(_lib IN LISTS THERMAVIP_LIBRARIES)
		if(TARGET ${_lib})
			list(APPEND _vip_test_env_mod "PATH=path_list_prepend:$<TARGET_FILE_DIR:${_lib}>")
		endif()
	endforeach()
	list(APPEND _vip_test_env_mod "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>")
endif()

# _CRTDBG_MAP_ALLOC is deliberately not defined: it turns malloc and free into
# macros taking extra arguments, which the Qt headers do not survive. Leaked
# blocks are therefore reported with an address and a size but no file:line;
# UMDH is the tool for locating them.

# Qt plugin directory. Without it no platform plugin is initialised, not even
# offscreen, and Qt aborts before the first test.
if(DEFINED QT${QT_VERSION_MAJOR}_INSTALL_PREFIX AND DEFINED QT${QT_VERSION_MAJOR}_INSTALL_PLUGINS)
	set(_vip_qt_plugins "${QT${QT_VERSION_MAJOR}_INSTALL_PREFIX}/${QT${QT_VERSION_MAJOR}_INSTALL_PLUGINS}")
else()
	set(_vip_qt_plugins "$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>/../plugins")
endif()

# QT_QPA_PLATFORM=offscreen: tests must not need a display server, and it
# removes most of the graphics driver noise under the memory checkers.
add_test(NAME ${TARGET_PROJECT} COMMAND ${TARGET_PROJECT})
set_tests_properties(${TARGET_PROJECT} PROPERTIES
	ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_PLUGIN_PATH=${_vip_qt_plugins}"
	TIMEOUT 300)
if(_vip_test_env_mod)
	set_tests_properties(${TARGET_PROJECT} PROPERTIES ENVIRONMENT_MODIFICATION "${_vip_test_env_mod}")
endif()

# Install in "tests" folder
install(TARGETS ${TARGET_PROJECT}
	LIBRARY DESTINATION "${THERMAVIP_TEST_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_TEST_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_TEST_DIR}"
)
