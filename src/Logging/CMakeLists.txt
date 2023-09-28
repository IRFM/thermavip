cmake_minimum_required(VERSION 3.16)
project(VipLogging VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGL Core Gui Xml Network Sql PrintSupport)

add_library(VipLogging SHARED
    VipLogging.cpp VipLogging.h
)
target_compile_definitions(VipLogging PRIVATE
    VIP_BUILD_LOGGING_LIB
)

target_link_libraries(VipLogging PRIVATE
    Qt::Core
    Qt::Gui
)


set(TARGET_PROJECT VipLogging)
include(${PROJECT_SOURCE_DIR}/../compiler_flags.cmake)


install(TARGETS VipLogging
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} 
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS VipLogging
    LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
)
install(TARGETS VipLogging
	LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
)

file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
install(FILES ${HEADER_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/VipLogging/")