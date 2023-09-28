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

target_include_directories(H5StillImage PRIVATE
    ${PROJECT_SOURCE_DIR}/../../Logging 
	${PROJECT_SOURCE_DIR}/../../DataType 
	${PROJECT_SOURCE_DIR}/../../Core 
	${PROJECT_SOURCE_DIR}/../../Plotting 
	${PROJECT_SOURCE_DIR}/../../Gui
)

target_link_libraries(H5StillImage PRIVATE
	VipDataType
	VipLogging
	VipCore
	VipPlotting
	VipGui
)

# Add default compiler flags
set(TARGET_PROJECT H5StillImage)
include(${PROJECT_SOURCE_DIR}/../../compiler_flags.cmake)

# Specific plugin defines
target_compile_definitions(H5StillImage PRIVATE H5_BUILT_AS_DYNAMIC_LIB BUILD_H5_STILL_IMAGE_LIB)

# Use find_package to get the H5StillImage paths
find_package (HDF5 COMPONENTS C )

target_include_directories(H5StillImage PRIVATE ${HDF5_INCLUDE_DIRS} )
target_link_libraries(H5StillImage PRIVATE ${HDF5_LIBRARIES})


# Plugin standard installation
install(TARGETS H5StillImage
	LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip/VipPlugins"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip/VipPlugins"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip/VipPlugins"
)

# Plugin custom installation process
install(CODE "set(HDF5_BIN \"${HDF5_INCLUDE_DIRS}/../bin\")")
install(CODE "set(CMAKE_INSTALL_BINDIR \"${CMAKE_INSTALL_BINDIR}\")")
install(CODE "set(CMAKE_SOURCE_DIR \"${CMAKE_SOURCE_DIR}\")")
install(SCRIPT InstallHDF5.cmake)
