# Generated from librir.pro.

cmake_minimum_required(VERSION 3.16)

project(thermavip
    VERSION 4.1.0
    DESCRIPTION "thermavip"
    HOMEPAGE_URL "https://github.com/IRFM/thermavip"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


# add Qt

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set_property(GLOBAL PROPERTY AUTOMOC_MOC_OPTIONS "-nw")



find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGL Gui Xml Network)
MESSAGE(STATUS "Qt5 found!")

include(GNUInstallDirs) 

option(LOCAL_INSTALL "Install locally" ON)
option(WITH_FFMEG "Build ffmpeg plugin" ON)
option(WITH_HDF5 "Build HDF5 plugin" OFF)
option(WIDTH_PYTHON "Build Python plugin" ON)



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

#TOREMOVE
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
endif()

# Make configure_ffmpeg executable
IF (UNIX)
	execute_process(COMMAND "chmod 100755 configure_ffmpeg")
ENDIF()

IF (NOT MSVC)
# Create a dummy target to build ffmpeg as a prebuild step
add_custom_target(ffmpeg BYPRODUCTS "${PROJECT_SOURCE_DIR}/build_ffmpeg.h"
  COMMAND ${PROJECT_SOURCE_DIR}/configure_ffmpeg)
add_executable(dummy ${PROJECT_SOURCE_DIR}/dummy.c "${PROJECT_SOURCE_DIR}/build_ffmpeg.h")
target_include_directories(dummy PRIVATE "${PROJECT_SOURCE_DIR}")

ENDIF()



configure_file(VipConfig.h.in VipConfig.h)
install(FILES ${CMAKE_BINARY_DIR}/VipConfig.h DESTINATION include)

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
install(DIRECTORY skins DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip")
install(DIRECTORY help DESTINATION "${CMAKE_INSTALL_PREFIX}")
install(DIRECTORY help DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip")
install(DIRECTORY icons DESTINATION "${CMAKE_INSTALL_PREFIX}")
install(DIRECTORY icons DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip")
install(DIRECTORY fonts DESTINATION "${CMAKE_INSTALL_PREFIX}")
install(DIRECTORY fonts DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip")

if(WIN32)
	file(GLOB win32_tools "tools/win32/*.exe")
	install(FILES ${win32_tools} DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip")
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

