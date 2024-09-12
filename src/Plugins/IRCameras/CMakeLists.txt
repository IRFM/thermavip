cmake_minimum_required(VERSION 3.16)
project(IRCameras VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

FILE(GLOB Sources *.cpp)
FILE(GLOB Headers *.h)
add_library(IRCameras SHARED
    ${Sources} ${Headers}
)

target_include_directories(IRCameras PRIVATE ${THERMAVIP_INCLUDE_DIRS})
target_link_libraries(IRCameras PRIVATE ${THERMAVIP_LIBRARIES})

# Add default compiler flags
set(TARGET_PROJECT IRCameras)
include(${THERMAVIP_COMPILER_FLAGS_FILE})

# Plugin standard installation
install(TARGETS IRCameras
	LIBRARY DESTINATION "${THERMAVIP_PLUGIN_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_PLUGIN_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_PLUGIN_DIR}"
)
