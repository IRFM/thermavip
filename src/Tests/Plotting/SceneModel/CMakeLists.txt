cmake_minimum_required(VERSION 3.16)
project(SceneModel VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

add_executable(SceneModel 
    main.cpp 
)

target_include_directories(SceneModel PRIVATE
    ${PROJECT_SOURCE_DIR}/../../../Logging 
	${PROJECT_SOURCE_DIR}/../../../DataType 
	${PROJECT_SOURCE_DIR}/../../../Core 
	${PROJECT_SOURCE_DIR}/../../../Plotting 
	${PROJECT_SOURCE_DIR}/../../../Gui
)

target_link_libraries(SceneModel PRIVATE
	VipDataType
	VipLogging
	VipCore
	VipPlotting
	VipGui
)

set(TARGET_PROJECT SceneModel)
include(${PROJECT_SOURCE_DIR}/../../../compiler_flags.cmake)

install(TARGETS SceneModel
	LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
)
