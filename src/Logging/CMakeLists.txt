cmake_minimum_required(VERSION 3.16)
project(VipLogging VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

add_library(VipLogging SHARED
    VipLogging.cpp VipLogging.h
)
target_compile_definitions(VipLogging PRIVATE
    VIP_BUILD_LOGGING_LIB
)


set(TARGET_PROJECT VipLogging)
include(${THERMAVIP_COMPILER_FLAGS_FILE})

install(TARGETS VipLogging
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} 
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
    #RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS VipLogging
    LIBRARY DESTINATION "${THERMAVIP_APPLICATION_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_APPLICATION_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_APPLICATION_DIR}"
)
install(TARGETS VipLogging
	LIBRARY DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
)
install(TARGETS VipLogging
	LIBRARY DESTINATION "${THERMAVIP_TEST_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_TEST_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_TEST_DIR}"
)

file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
install(FILES ${HEADER_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/VipLogging/")
