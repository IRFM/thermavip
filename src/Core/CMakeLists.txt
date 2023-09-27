cmake_minimum_required(VERSION 3.16)
project(VipCore VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Network Xml)

FILE(GLOB Sources *.cpp)
FILE(GLOB Headers *.h)
add_library(VipCore SHARED
     ${Sources} ${Headers}
)

target_include_directories(VipCore PRIVATE
    ${PROJECT_SOURCE_DIR}/../Logging ${PROJECT_SOURCE_DIR}/../DataType
)

target_compile_definitions(VipCore PRIVATE
    VIP_BUILD_CORE_LIB
)

target_link_libraries(VipCore PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Network
    Qt::Xml
)

target_link_libraries(VipCore PRIVATE
	VipDataType
	VipLogging
)

set(TARGET_PROJECT VipCore)
include(${PROJECT_SOURCE_DIR}/../compiler_flags.cmake)

if(WIN32)
set_property(TARGET VipCore PROPERTY AUTOMOC_MOC_OPTIONS "-nw -D_WIN32")
else()
set_property(TARGET VipCore PROPERTY AUTOMOC_MOC_OPTIONS "-nw")
endif()

install(TARGETS VipCore
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} 
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS VipCore
    LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
)
install(TARGETS VipCore
	LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
)

file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
install(FILES ${HEADER_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/VipCore/")
