cmake_minimum_required(VERSION 3.16)
project(Quiver VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Network Xml Widgets OpenGL PrintSupport)

add_executable(Quiver 
    main.cpp 
)

target_include_directories(Quiver PRIVATE
    ${PROJECT_SOURCE_DIR}/../../../Logging 
	${PROJECT_SOURCE_DIR}/../../../DataType 
	${PROJECT_SOURCE_DIR}/../../../Core 
	${PROJECT_SOURCE_DIR}/../../../Plotting 
	${PROJECT_SOURCE_DIR}/../../../GUI
)

target_link_libraries(Quiver PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Network
	Qt::Widgets
	Qt::OpenGL
	Qt::PrintSupport
    Qt::Xml
)



target_link_libraries(Quiver PRIVATE
	VipDataType
	VipLogging
	VipCore
	VipPlotting
	VipGui
)

set(TARGET_PROJECT Quiver)
include(${PROJECT_SOURCE_DIR}/../../../compiler_flags.cmake)

install(TARGETS Quiver
	LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
)
