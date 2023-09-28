cmake_minimum_required(VERSION 3.16)
project(Thermavip VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Network Xml Widgets OpenGL PrintSupport Sql)

if(MSVC)
	if (CMAKE_BUILD_TYPE STREQUAL "Release" )
		add_executable(Thermavip  
			main.cpp Thermavip_resource.rc
		)
	else()
		add_executable(Thermavip 
			main.cpp Thermavip_resource.rc
		)
	endif()
else()
	add_executable(Thermavip  
		main.cpp 
	)
endif()

target_include_directories(Thermavip PRIVATE
    ${PROJECT_SOURCE_DIR}/../Logging ${PROJECT_SOURCE_DIR}/../DataType ${PROJECT_SOURCE_DIR}/../Core ${PROJECT_SOURCE_DIR}/../Plotting ${PROJECT_SOURCE_DIR}/../Gui ${PROJECT_SOURCE_DIR}/../Annotation
)

target_link_libraries(Thermavip PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Network
	Qt::Widgets
	Qt::OpenGL
	Qt::PrintSupport
    Qt::Xml
	Qt::Sql
)



target_link_libraries(Thermavip PRIVATE
	VipDataType
	VipLogging
	VipCore
	VipPlotting
	VipGui
	VipAnnotation
)

set(TARGET_PROJECT Thermavip)
include(${PROJECT_SOURCE_DIR}/../compiler_flags.cmake)

install(TARGETS Thermavip
	LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
)
