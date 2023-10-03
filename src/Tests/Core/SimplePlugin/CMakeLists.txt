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

# Find Qt
find_package(QT NAMES Qt5 Qt6 REQUIRED )
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGL Core Gui Xml Network Sql PrintSupport Svg)
set(QT_LIBS Qt::Core
    Qt::Gui
    Qt::Network
	Qt::Widgets
	Qt::OpenGL
	Qt::PrintSupport
    Qt::Xml
	Qt::Sql
	Qt::Svg
	)
	
# Link Qt libraries
target_link_libraries(SimplePlugin PRIVATE ${QT_LIBS})

# Find thermavip
find_package(thermavip REQUIRED )

# Add thermavip include directories
target_include_directories(SimplePlugin PRIVATE ${THERMAVIP_INCLUDE_DIRS} )
# Add thermavip library directory
target_link_directories(SimplePlugin PRIVATE ${THERMAVIP_LIB_DIR} )
# Link with thermavip SDK
target_link_libraries(SimplePlugin PRIVATE ${THERMAVIP_LIBRARIES})

# Install plugin
install(TARGETS SimplePlugin
	LIBRARY DESTINATION "${THERMAVIP_PLUGIN_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_PLUGIN_DIR}"
)
