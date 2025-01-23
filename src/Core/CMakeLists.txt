cmake_minimum_required(VERSION 3.16)
project(VipCore VERSION 1.0 LANGUAGES C CXX)

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

if(WITH_HDF5)
	 list(APPEND Sources VipH5Archive.cpp VipH5DeviceDriver.cpp)
	 list(APPEND Headers VipH5Archive.h VipH5DeviceDriver.h)
endif()

add_library(VipCore SHARED
     ${Sources} ${Headers}
)

target_include_directories(VipCore PRIVATE
    ${PROJECT_SOURCE_DIR}/../Logging ${PROJECT_SOURCE_DIR}/../DataType
)

if(WITH_HDF5)
	 target_include_directories(VipCore PRIVATE ${HDF5_INCLUDE_DIRS})
	 target_link_libraries(VipCore PRIVATE ${HDF5_LIBRARIES})
endif()

target_compile_definitions(VipCore PRIVATE
    VIP_BUILD_CORE_LIB
)

target_link_libraries(VipCore PRIVATE
	VipDataType
	VipLogging
)

set(TARGET_PROJECT VipCore)
include(${THERMAVIP_COMPILER_FLAGS_FILE})

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
    LIBRARY DESTINATION "${THERMAVIP_APPLICATION_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_APPLICATION_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_APPLICATION_DIR}"
)
install(TARGETS VipCore
	LIBRARY DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
)
install(TARGETS VipCore
	LIBRARY DESTINATION "${THERMAVIP_TEST_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_TEST_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_TEST_DIR}"
)

if(WITH_HDF5)
	# Plugin custom installation process
	install(CODE "set(HDF5_BIN \"${HDF5_INCLUDE_DIRS}/../bin\")")
	install(CODE "set(CMAKE_INSTALL_BINDIR \"${CMAKE_INSTALL_BINDIR}\")")
	install(CODE "set(CMAKE_SOURCE_DIR \"${CMAKE_SOURCE_DIR}\")")
	install(SCRIPT InstallHDF5.cmake)
endif()

file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
install(FILES ${HEADER_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/VipCore/")
