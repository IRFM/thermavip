cmake_minimum_required(VERSION 3.16)
project(Ffmpeg VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

FILE(GLOB Sources *.cpp)
FILE(GLOB Headers *.h)
add_library(Ffmpeg SHARED
    ${Sources} ${Headers}
)

target_include_directories(Ffmpeg PRIVATE ${THERMAVIP_INCLUDE_DIRS})
target_link_libraries(Ffmpeg PRIVATE ${THERMAVIP_LIBRARIES})

# Add default compiler flags
set(TARGET_PROJECT Ffmpeg)
include(${THERMAVIP_COMPILER_FLAGS_FILE})

target_compile_definitions(Ffmpeg PRIVATE ENABLE_H264 BUILD_FFMPEG_LIB)


# Configure ffmpeg path, use librir ffmpeg install if possible
if(WITH_LIBRIR)
	if(MSVC)
		set(FFMPEG_PATH ${CMAKE_BINARY_DIR}/_deps/librir-src/extra/ffmpeg)
	else()
		# TODO! Linux
		# set(FFMPEG_PATH ${CMAKE_BINARY_DIR}/src/Annotation/librir-prefix/src/librir-build/install)
		# Compile ffmpeg from source inside 3rd_64 folder
		set(FFMPEG_PATH ${CMAKE_SOURCE_DIR}/3rd_64)
	endif()
else()
	# Compile ffmpeg from source inside 3rd_64 folder
	set(FFMPEG_PATH ${CMAKE_SOURCE_DIR}/3rd_64)
endif()



if (MSVC)
	set(FFMPEG_LIB_DIR ${FFMPEG_PATH}/ffmpeg-4.3-msvc/lib)
	set(FFMPEG_INCLUDE_DIR "${FFMPEG_PATH}/ffmpeg-4.3-msvc/include;${FFMPEG_PATH}/ffmpeg-4.3-msvc")
	set(FFMPEG_LIBS avdevice avformat avcodec swscale avutil swresample avfilter )
	set(ADDITIONAL_FFMPEG_LIBS libx264 libx265 kvazaar postproc vcruntime140_1 Vfw32 shlwapi strmbase)
	set(FFMPEG_PREFIX )
	set(FFMPEG_SUFFIX .dll)
else()
	if (WIN32)
		# mingw
		set(FFMPEG_LIB_DIR ${FFMPEG_PATH}/ffmpeg/install/lib)
		set(FFMPEG_INCLUDE_DIR ${FFMPEG_PATH}/ffmpeg/install/include)
		set(FFMPEG_LIBS avdevice avformat avcodec swscale avutil swresample avfilter)
		set(ADDITIONAL_FFMPEG_LIBS postproc)
		set(FFMPEG_PREFIX )
		set(FFMPEG_SUFFIX .dll)
	else()
		set(FFMPEG_LIB_DIR ${FFMPEG_PATH}/ffmpeg/install/lib)
		set(FFMPEG_INCLUDE_DIR ${FFMPEG_PATH}/ffmpeg/install/include)
		set(FFMPEG_LIBS avdevice avformat avcodec swscale avutil swresample avfilter)
		set(ADDITIONAL_FFMPEG_LIBS postproc)
		set(FFMPEG_PREFIX lib)
		set(FFMPEG_SUFFIX .so)
	ENDIF()
ENDIF()

target_link_directories(Ffmpeg PRIVATE ${FFMPEG_LIB_DIR})
MESSAGE(STATUS "Using ${FFMPEG_LIB_DIR} for ffmpeg library files")


target_include_directories(Ffmpeg PRIVATE ${FFMPEG_INCLUDE_DIR})
target_link_libraries(Ffmpeg PRIVATE ${FFMPEG_LIBS})
set(COMPLETE_FFMPEG_LIBS ${FFMPEG_LIBS} ${ADDITIONAL_FFMPEG_LIBS})


# Add any variables you need during post install.
install(CODE "set(FFMPEG_LIB_DIR \"${FFMPEG_LIB_DIR}\")")
install(CODE "set(FFMPEG_INCLUDE_DIR \"${FFMPEG_INCLUDE_DIR}\")")
install(CODE "set(FFMPEG_LIBS \"${FFMPEG_LIBS}\")")
install(CODE "set(ADDITIONAL_FFMPEG_LIBS \"${ADDITIONAL_FFMPEG_LIBS}\")")
install(CODE "set(FFMPEG_PREFIX \"${FFMPEG_PREFIX}\")")
install(CODE "set(FFMPEG_SUFFIX \"${FFMPEG_SUFFIX}\")")
install(CODE "set(COMPLETE_FFMPEG_LIBS \"${COMPLETE_FFMPEG_LIBS}\")")
install(CODE "set(CMAKE_INSTALL_BINDIR \"${CMAKE_INSTALL_BINDIR}\")")
install(CODE "set(CMAKE_SOURCE_DIR \"${CMAKE_SOURCE_DIR}\")")
install(SCRIPT InstallFfmpeg.cmake)

install(TARGETS Ffmpeg
	LIBRARY DESTINATION "${THERMAVIP_PLUGIN_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_PLUGIN_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_PLUGIN_DIR}"
)

# Make Ffmpeg plugin usable as a library
install(TARGETS Ffmpeg
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/Plugins
)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
install(FILES ${HEADER_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/Plugins/Ffmpeg/")
