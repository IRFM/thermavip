cmake_minimum_required(VERSION 3.16)
project(VipPlotting VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Network Xml Widgets OpenGL PrintSupport Svg)

FILE(GLOB Sources *.cpp)
FILE(GLOB Headers *.h)
add_library(VipPlotting SHARED
    ${Sources} ${Headers}
)

target_include_directories(VipPlotting PRIVATE
    ${PROJECT_SOURCE_DIR}/../Logging ${PROJECT_SOURCE_DIR}/../DataType
)

target_compile_definitions(VipPlotting PRIVATE
    VIP_BUILD_PLOTTING_LIB
)

target_link_libraries(VipPlotting PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Network
	Qt::Widgets
	Qt::OpenGL
    Qt::Xml
	Qt::PrintSupport
	Qt::Svg
)

target_link_libraries(VipPlotting PRIVATE
	VipDataType
	VipLogging
)

set(TARGET_PROJECT VipPlotting)
include(${PROJECT_SOURCE_DIR}/../compiler_flags.cmake)

install(TARGETS VipPlotting
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} 
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS VipPlotting
    LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
)
install(TARGETS VipPlotting
	LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
)

file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
install(FILES ${HEADER_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/VipPlotting/")
