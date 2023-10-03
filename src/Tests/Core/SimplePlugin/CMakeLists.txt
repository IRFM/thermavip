cmake_minimum_required(VERSION 3.16)
project(SimplePlugin VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

# Find Qt
find_package(QT NAMES Qt5 Qt6 REQUIRED )
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
target_link_libraries(${TARGET_PROJECT} PRIVATE ${QT_LIBS})


add_library(SimplePlugin SHARED
    plugin.cpp plugin.h
)


# Find thermavip
find_package(thermavip REQUIRED )
target_include_directories(SimplePlugin PRIVATE ${THERMAVIP_INCLUDE_DIRS} )
target_link_directories(SimplePlugin PRIVATE ${THERMAVIP_LIB_DIR} )
target_link_libraries(SimplePlugin PRIVATE ${THERMAVIP_LIBRARIES})


set(TARGET_PROJECT SimplePlugin)
include(${PROJECT_SOURCE_DIR}/../../../compiler_flags.cmake)

install(TARGETS SimplePlugin
	LIBRARY DESTINATION "${THERMAVIP_PLUGIN_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_PLUGIN_DIR}"
)
