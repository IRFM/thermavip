cmake_minimum_required(VERSION 3.16)
project(VipPlotting VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

FILE(GLOB Sources *.cpp)
FILE(GLOB Headers *.h)

if(NOT WITH_VTK)
	list(FILTER Sources EXCLUDE REGEX "^.*VTK.*$")
	list(FILTER Headers EXCLUDE REGEX "^.*VTK.*$")
endif()

add_library(VipPlotting SHARED
    ${Sources} ${Headers}
)

target_include_directories(VipPlotting PRIVATE
    ${PROJECT_SOURCE_DIR}/../Logging ${PROJECT_SOURCE_DIR}/../DataType ${PROJECT_SOURCE_DIR}/../Core
)

target_compile_definitions(VipPlotting PRIVATE
    VIP_BUILD_PLOTTING_LIB
)

target_link_libraries(VipPlotting PRIVATE
	VipDataType
	VipLogging
	VipCore
)

set(TARGET_PROJECT VipPlotting)
include(${THERMAVIP_COMPILER_FLAGS_FILE})

install(TARGETS VipPlotting
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} 
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS VipPlotting
    LIBRARY DESTINATION "${THERMAVIP_APPLICATION_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_APPLICATION_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_APPLICATION_DIR}"
)
install(TARGETS VipPlotting
	LIBRARY DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
)
install(TARGETS VipPlotting
	LIBRARY DESTINATION "${THERMAVIP_TEST_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_TEST_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_TEST_DIR}"
)

file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
install(FILES ${HEADER_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/VipPlotting/")
