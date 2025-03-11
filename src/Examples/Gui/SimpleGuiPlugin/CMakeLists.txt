cmake_minimum_required(VERSION 3.16)
project(SimpleGuiPlugin VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)

# Create a shared library
add_library(SimpleGuiPlugin SHARED
    plugin.cpp plugin.h
)

# Find thermavip
find_package(thermavip REQUIRED )
target_link_libraries(SimpleGuiPlugin PRIVATE ${THERMAVIP_LIBRARIES} ${THERMAVIP_QT_LIBRARIES} )


# Install plugin
install(TARGETS SimpleGuiPlugin
	LIBRARY DESTINATION "${THERMAVIP_PLUGIN_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_PLUGIN_DIR}"
)
