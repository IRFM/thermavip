cmake_minimum_required(VERSION 3.16)
project(SimplePlugin VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)

# Create a shared library
add_library(SimplePlugin SHARED
    plugin.cpp plugin.h
)

# Find thermavip
find_package(thermavip REQUIRED )

# Add thermavip include directories
target_include_directories(SimplePlugin PRIVATE ${THERMAVIP_INCLUDE_DIRS} )
# Add thermavip library directory
target_link_directories(SimplePlugin PRIVATE ${THERMAVIP_LIB_DIR} )
# Link with thermavip SDK as well as Qt libraries
target_link_libraries(SimplePlugin PRIVATE ${THERMAVIP_LIBRARIES} ${THERMAVIP_QT_LIBRARIES})

# Install plugin
install(TARGETS SimplePlugin
	LIBRARY DESTINATION "${THERMAVIP_PLUGIN_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_PLUGIN_DIR}"
)
