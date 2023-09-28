cmake_minimum_required(VERSION 3.16)
project(IRCameras VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Network Xml Widgets OpenGL PrintSupport)

FILE(GLOB Sources *.cpp)
FILE(GLOB Headers *.h)
add_library(IRCameras SHARED
    ${Sources} ${Headers}
)

target_include_directories(IRCameras PRIVATE
    ${PROJECT_SOURCE_DIR}/../../Logging 
	${PROJECT_SOURCE_DIR}/../../DataType 
	${PROJECT_SOURCE_DIR}/../../Core 
	${PROJECT_SOURCE_DIR}/../../Plotting 
	${PROJECT_SOURCE_DIR}/../../Gui
)

target_link_libraries(IRCameras PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Network
	Qt::Widgets
	Qt::OpenGL
	Qt::PrintSupport
    Qt::Xml
)

target_link_libraries(IRCameras PRIVATE
	VipDataType
	VipLogging
	VipCore
	VipPlotting
	VipGui
)

# Add default compiler flags
set(TARGET_PROJECT IRCameras)
include(${PROJECT_SOURCE_DIR}/../../compiler_flags.cmake)

# Plugin standard installation
install(TARGETS IRCameras
	LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip/VipPlugins"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip/VipPlugins"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip/VipPlugins"
)
