# Generated from librir.pro.

cmake_minimum_required(VERSION 3.16)

project(thermavip
    VERSION 5.0.0
    DESCRIPTION "thermavip"
    HOMEPAGE_URL "https://github.com/IRFM/thermavip"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(QT NAMES Qt5 Qt6 REQUIRED )
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGL Core Gui Xml Network Sql PrintSupport Svg)
MESSAGE(STATUS "Qt found!")

include(GNUInstallDirs) 

# Global options
option(LOCAL_INSTALL "Install locally" ON)
option(WITH_LONG_DOUBLE "Use long double as the default type for 2D point representation" OFF)
option(WITH_FFMEG "Build ffmpeg plugin" OFF)
option(WITH_HDF5 "Build HDF5 plugin" OFF)
option(WITH_PYTHON "Build Python plugin" OFF)
option(WITH_VTK "Build VTK support" OFF)
option(WITH_LIBRIR "Build and install librir" OFF)
option(WITH_EXAMPLES "Build examples" ON)
option(WITH_TESTS "Build tests" ON)
option(WITH_MICRO "Experimental, use micro generic allocator to reduce memory footprint" OFF)
option(NO_WARNING "Treat warnings as errors" OFF)


# Local installation
IF(LOCAL_INSTALL)
	SET(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install) 
	MESSAGE(STATUS ${CMAKE_INSTALL_PREFIX})
	set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) 
	set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

	IF(NOT MSVC)
		SET(CMAKE_INSTALL_RPATH $ORIGIN)
		
		SET(CMAKE_SKIP_BUILD_RPATH FALSE)
		SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
		#~ SET(CMAKE_INSTALL_RPATH_USE_ORIGIN TRUE)
		SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
	ENDIF()
ENDIF()



# Make configure_ffmpeg executable
IF (UNIX)
	execute_process(COMMAND chmod 777 ${PROJECT_SOURCE_DIR}/configure_ffmpeg)
ENDIF()

IF (NOT MSVC)
# Create a dummy target to build ffmpeg as a prebuild step
#add_custom_target(ffmpeg BYPRODUCTS "${PROJECT_SOURCE_DIR}/build_ffmpeg.h"
#  COMMAND ${PROJECT_SOURCE_DIR}/configure_ffmpeg)
#add_executable(dummy ${PROJECT_SOURCE_DIR}/dummy.c "${PROJECT_SOURCE_DIR}/build_ffmpeg.h")
#target_include_directories(dummy PRIVATE "${PROJECT_SOURCE_DIR}")
add_custom_target(ffmpeg ALL COMMAND ${PROJECT_SOURCE_DIR}/configure_ffmpeg)
ENDIF()


# Long double support
if(WITH_LONG_DOUBLE)
	set(LONG_DOUBLE_VALUE 1)
else()
	set(LONG_DOUBLE_VALUE 0)
endif()


# Define thermavip include directories for internal plugins and SDK libraries
set(THERMAVIP_INCLUDE_DIRS 
	${PROJECT_SOURCE_DIR}/src/Logging 
	${PROJECT_SOURCE_DIR}/src/DataType 
	${PROJECT_SOURCE_DIR}/src/Core 
	${PROJECT_SOURCE_DIR}/src/Plotting 
	${PROJECT_SOURCE_DIR}/src/Gui 
	${PROJECT_SOURCE_DIR}/src/Annotation 
	CACHE INTERNAL "THERMAVIP_INCLUDE_DIRS")
	
# Define thermavip SDK libraries for internal plugins
set(THERMAVIP_LIBRARIES 
	VipDataType
	VipLogging
	VipCore
	VipPlotting
	VipGui
	VipAnnotation
	CACHE INTERNAL "THERMAVIP_INCLUDE_DIRS")
	
# Define thermavip SDK compiler flags file for internal plugins and SDK libraries
set(THERMAVIP_COMPILER_FLAGS_FILE 
	${PROJECT_SOURCE_DIR}/src/compiler_flags.cmake
	CACHE INTERNAL "THERMAVIP_COMPILER_FLAGS_FILE")

# Define global variables used internally
set(THERMAVIP_APPLICATION_DIR ${CMAKE_INSTALL_PREFIX}/thermavip CACHE INTERNAL "THERMAVIP_APPLICATION_DIR")
set(THERMAVIP_ICON_DIR ${CMAKE_INSTALL_PREFIX}/thermavip/icons CACHE INTERNAL "THERMAVIP_ICON_DIR")
set(THERMAVIP_PLUGIN_DIR "${CMAKE_INSTALL_PREFIX}/thermavip/VipPlugins" CACHE INTERNAL "THERMAVIP_PLUGIN_DIR")
set(THERMAVIP_EXAMPLE_DIR "${CMAKE_INSTALL_PREFIX}/examples" CACHE INTERNAL "THERMAVIP_EXAMPLE_DIR")
set(THERMAVIP_TEST_DIR "${CMAKE_INSTALL_PREFIX}/tests" CACHE INTERNAL "THERMAVIP_TEST_DIR")
set(THERMAVIP_EXAMPLE_SETUP_FILE ${PROJECT_SOURCE_DIR}/src/Examples/setup_example.cmake CACHE INTERNAL "THERMAVIP_EXAMPLE_SETUP_FILE")


configure_file(VipBuildConfig.h.in VipBuildConfig.h)
install(FILES ${CMAKE_BINARY_DIR}/VipBuildConfig.h DESTINATION include)

# Just to put VipConfig.h in the build folder
configure_file(${PROJECT_SOURCE_DIR}/src/VipConfig.h VipConfig.h)
install(FILES ${PROJECT_SOURCE_DIR}/src/VipConfig.h DESTINATION include)

INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR})


# Create global variable used in thermavip.pc.in
# Plugins can add libraries to link with for generated thermavip.pc
set(PLUGINS_LIBS "" CACHE INTERNAL "")

set(PLUGINS_PYTHON_REQUIRE "" CACHE INTERNAL "")

add_subdirectory(src)


# Generate requirements.txt
#configure_file(requirements.txt.in requirements.txt @ONLY)
#install(FILES ${CMAKE_BINARY_DIR}/requirements.txt DESTINATION ${CMAKE_INSTALL_PREFIX})

# Install directories
install(DIRECTORY skins DESTINATION "${CMAKE_INSTALL_PREFIX}")
install(DIRECTORY skins DESTINATION "${THERMAVIP_APPLICATION_DIR}")
install(DIRECTORY help DESTINATION "${CMAKE_INSTALL_PREFIX}")
install(DIRECTORY help DESTINATION "${THERMAVIP_APPLICATION_DIR}")
install(DIRECTORY icons DESTINATION "${CMAKE_INSTALL_PREFIX}")
install(DIRECTORY icons DESTINATION "${THERMAVIP_APPLICATION_DIR}")
install(DIRECTORY fonts DESTINATION "${CMAKE_INSTALL_PREFIX}")
install(DIRECTORY fonts DESTINATION "${THERMAVIP_APPLICATION_DIR}")

if(WIN32)
	file(GLOB win32_tools "tools/win32/*.exe")
	install(FILES ${win32_tools} DESTINATION "${THERMAVIP_APPLICATION_DIR}")
endif()


include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/thermavipConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/thermavipConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/thermavip
  PATH_VARS 
  PLUGINS_LIBS)
  
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/thermavipConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion )
  
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/thermavipConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/thermavipConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/thermavip )
		

# Generate thermavip.pc
list(TRANSFORM PLUGINS_LIBS PREPEND "-l")
configure_file(thermavip.pc.in thermavip.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/thermavip.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

