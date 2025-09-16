# Generated from librir.pro.

cmake_minimum_required(VERSION 3.16)

project(thermavip
    VERSION 5.2.0
    DESCRIPTION "thermavip"
    HOMEPAGE_URL "https://github.com/IRFM/thermavip"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
find_package(QT NAMES Qt5 Qt6 REQUIRED )

# Remove this line as it causes issues on Linux (?)
#find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGL Core Gui Xml Network Sql PrintSupport Svg)
message(STATUS "Qt found! Version ${QT_VERSION_MAJOR}.${QT_VERSION_MINOR}")

include(GNUInstallDirs) 

# Global options
option(LOCAL_INSTALL "Install locally" OFF)
option(WITH_LONG_DOUBLE "Use long double as the default type for 2D point representation" OFF)
option(WITH_FFMEG "Build ffmpeg support" OFF)
option(WITH_LOCAL_FFMPEG "Use local ffmpeg installation instead of thermavip one" OFF)
option(WITH_HDF5 "Build HDF5 support" OFF)
option(WITH_PYTHON "Build Python support" OFF)
option(WITH_WEBENGINE "Build components based on Qt webengine module (if webengine is available)" ON)
option(WITH_VTK "Build VTK support" OFF)
option(WITH_LIBRIR "Build and install librir" OFF)
option(WITH_EXAMPLES "Build examples" ON)
option(WITH_TESTS "Build tests" ON)
option(WITH_MICRO "Experimental, use micro generic allocator to reduce memory footprint" OFF)
option(NO_WARNING "Treat warnings as errors" OFF)


# Local installation
if(LOCAL_INSTALL)
	set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install) 
	message(STATUS ${CMAKE_INSTALL_PREFIX})
	set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) 
	set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

	if(NOT MSVC)
		set(CMAKE_INSTALL_RPATH $ORIGIN)
		
		set(CMAKE_SKIP_BUILD_RPATH FALSE)
		set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
		set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
	endif()
endif()






if (NOT MSVC)
	if(WITH_FFMPEG AND NOT WITH_LOCAL_FFMPEG)
		# Make configure_ffmpeg executable
		message(STATUS "Setup configure_ffmpeg")
		execute_process(COMMAND chmod 777 ${PROJECT_SOURCE_DIR}/configure_ffmpeg)
		add_custom_target(ffmpeg_build ALL COMMAND ${PROJECT_SOURCE_DIR}/configure_ffmpeg)
	endif()
endif()


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
	CACHE INTERNAL "THERMAVIP_LIBRARIES")
	
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
install(DIRECTORY skins DESTINATION "${THERMAVIP_APPLICATION_DIR}")
install(DIRECTORY help DESTINATION "${THERMAVIP_APPLICATION_DIR}")
install(DIRECTORY icons DESTINATION "${THERMAVIP_APPLICATION_DIR}")
install(DIRECTORY fonts DESTINATION "${THERMAVIP_APPLICATION_DIR}")

if(WIN32)
	file(GLOB win32_tools "tools/win32/*.exe")
	install(FILES ${win32_tools} DESTINATION "${THERMAVIP_APPLICATION_DIR}")
endif()


# Check Qt web engine and additional Qt libraries
set(QT_PREFIX Qt${QT_VERSION_MAJOR})
set(QT_ADDITIONAL_LIBS "")
set(QT_WEBENGINE_LIBS "")
if(${QT_VERSION_MAJOR} LESS 6)
set(WEB_ENGINE Qt${QT_VERSION_MAJOR}WebEngine)
else()
set(WEB_ENGINE Qt${QT_VERSION_MAJOR}WebEngineCore)
endif()

if(${QT_VERSION_MAJOR} LESS 6)
else()
find_package(Qt6 REQUIRED COMPONENTS Core5Compat)
find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
list(APPEND QT_ADDITIONAL_LIBS Qt6::Core5Compat Qt6::OpenGLWidgets)
endif()

if(NOT ${QT_VERSION_MAJOR} LESS 6)
	if(WITH_WEBENGINE)
		find_package(Qt6 QUIET COMPONENTS WebEngineCore)
		find_package(Qt6 QUIET COMPONENTS WebEngineWidgets )
		set(WEB_ENGINE Qt6WebEngineCore)
		if (${WEB_ENGINE}_FOUND)
			list(APPEND QT_WEBENGINE_LIBS Qt::WebEngineCore Qt::WebEngineWidgets)
			message(STATUS "Build Thermavip with WebEngine support!")
		else()
			message(WARNING "Build Thermavip without WebEngine support even if WEB_ENGINE is ON!")
		endif()
	endif()
endif()

list(JOIN QT_ADDITIONAL_LIBS " " QT_ADDITIONAL_LIBS)
list(JOIN QT_WEBENGINE_LIBS " " QT_WEBENGINE_LIBS)

# VTK
if(WITH_VTK)
	find_package(VTK REQUIRED)
	set(HAS_VTK ON)
else()
	set(HAS_VTK OFF)
endif()

# HDF5
if(WITH_HDF5)
	find_package (HDF5 COMPONENTS C REQUIRED)
	set(HAS_HDF5 ON)
else()
	set(HAS_HDF5 OFF)
endif()

# Add targets
target_include_directories(VipLogging  PUBLIC
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/VipLogging>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VipLogging>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")	   
install(TARGETS VipLogging EXPORT thermavip DESTINATION "${CMAKE_INSTALL_LIBDIR}" LIBRARY)
install(EXPORT thermavip DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/thermavip")

target_include_directories(VipDataType  PUBLIC
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/VipDataType>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VipDataType>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")	   
install(TARGETS VipDataType EXPORT thermavip DESTINATION "${CMAKE_INSTALL_LIBDIR}" LIBRARY)
install(EXPORT thermavip DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/thermavip")

target_include_directories(VipCore  PUBLIC
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/VipCore>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VipCore>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")	   
install(TARGETS VipCore EXPORT thermavip DESTINATION "${CMAKE_INSTALL_LIBDIR}" LIBRARY)
install(EXPORT thermavip DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/thermavip")

target_include_directories(VipPlotting  PUBLIC
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/VipPlotting>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VipPlotting>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")	   
install(TARGETS VipPlotting EXPORT thermavip DESTINATION "${CMAKE_INSTALL_LIBDIR}" LIBRARY)
install(EXPORT thermavip DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/thermavip")

target_include_directories(VipGui  PUBLIC
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/VipGui>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VipGui>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")	   
install(TARGETS VipGui EXPORT thermavip DESTINATION "${CMAKE_INSTALL_LIBDIR}" LIBRARY)
install(EXPORT thermavip DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/thermavip")

target_include_directories(VipAnnotation  PUBLIC
						   "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/VipAnnotation>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/VipAnnotation>"
						   "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")	   
install(TARGETS VipAnnotation EXPORT thermavip DESTINATION "${CMAKE_INSTALL_LIBDIR}" LIBRARY)
install(EXPORT thermavip DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/thermavip")


# Generate thermavipConfig.cmake
include(CMakePackageConfigHelpers)
configure_package_config_file(cmake/thermavipConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/thermavipConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/thermavip
  PATH_VARS 
  PLUGINS_LIBS QT_ADDITIONAL_LIBS QT_WEBENGINE_LIBS HAS_VTK HAS_HDF5)
  
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




