cmake_minimum_required(VERSION 3.16)
project(H5StillImage VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

FILE(GLOB Sources *.cpp)
FILE(GLOB Headers *.h)
add_library(H5StillImage SHARED
    ${Sources} ${Headers}
)


target_include_directories(H5StillImage PRIVATE ${THERMAVIP_INCLUDE_DIRS})
target_link_libraries(H5StillImage PRIVATE ${THERMAVIP_LIBRARIES})

# Add default compiler flags
set(TARGET_PROJECT H5StillImage)
include(${THERMAVIP_COMPILER_FLAGS_FILE})



# Specific plugin defines
target_compile_definitions(H5StillImage PRIVATE H5_BUILT_AS_DYNAMIC_LIB BUILD_H5_STILL_IMAGE_LIB)

# Use find_package to get the H5StillImage paths
find_package (HDF5 COMPONENTS C )

message(STATUS "Found HDF5 in ${HDF5_INCLUDE_DIRS}")

target_include_directories(H5StillImage PRIVATE ${HDF5_INCLUDE_DIRS} )
target_link_libraries(H5StillImage PRIVATE ${HDF5_LIBRARIES})

# Plugin standard installation
install(TARGETS H5StillImage
	LIBRARY DESTINATION "${THERMAVIP_PLUGIN_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_PLUGIN_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_PLUGIN_DIR}"
)

# Plugin custom installation process
install(CODE "set(HDF5_BIN \"${HDF5_INCLUDE_DIRS}/../bin\")")
install(CODE "set(CMAKE_INSTALL_BINDIR \"${CMAKE_INSTALL_BINDIR}\")")
install(CODE "set(CMAKE_SOURCE_DIR \"${CMAKE_SOURCE_DIR}\")")
install(SCRIPT InstallHDF5.cmake)
