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

if(NOT WITH_HDF5)
	list(FILTER Sources EXCLUDE REGEX "^.*H5.*$")
	list(FILTER Headers EXCLUDE REGEX "^.*H5.*$")
endif()

if(NOT WITH_FFMPEG)
	list(FILTER Sources EXCLUDE REGEX "^.*MPEG.*$")
	list(FILTER Headers EXCLUDE REGEX "^.*MPEG.*$")
endif()

if(NOT WITH_PYTHON)
	list(FILTER Sources EXCLUDE REGEX "^.*VipPy.*$")
	list(FILTER Headers EXCLUDE REGEX "^.*VipPy.*$")
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
    #RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
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


if(WITH_FFMPEG)

	# Setup ffmpeg support
	target_compile_definitions(VipCore PRIVATE ENABLE_H264)
		
	if(WITH_LOCAL_FFMEG)
		message(STATUS "Trying to find a local ffmpeg installation")
		list(APPEND CMAKE_MODULE_PATH  "${CMAKE_CURRENT_SOURCE_DIR}")
		find_package(FFMPEG COMPONENTS avcodec avdevice avfilter avformat avresample avutil swresample swscale)
		if(FFMPEG_AVCODEC_FOUND)
			message(STATUS "Found ffmpeg headers at  ${FFMPEG_INCLUDE_DIRS}")
			message(STATUS "Found ffmpeg libraries  ${FFMPEG_LIBRARIES}")
			target_include_directories(VipCore PRIVATE  ${FFMPEG_INCLUDE_DIRS})
			#target_link_directories(VipCore PRIVATE ${FFMPEG_LIB_DIR})
			target_link_libraries(VipCore PRIVATE ${FFMPEG_LIBRARIES})
		else()
			message(ERROR "Unable to find ffmpeg installation!")
		endif()
		
	else()

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
			set(FFMPEG_LIB_DIR ${FFMPEG_PATH}/ffmpeg-7.1-msvc/lib)
			set(FFMPEG_INCLUDE_DIR "${FFMPEG_PATH}/ffmpeg-7.1-msvc/include;${FFMPEG_PATH}/ffmpeg-7.1-msvc")
			set(FFMPEG_LIBS avdevice avformat avcodec swscale avutil swresample avfilter )
			set(ADDITIONAL_FFMPEG_LIBS libx264 libx265 kvazaar postproc vcruntime140_1 Vfw32 shlwapi strmbase)
			set(FFMPEG_PREFIX )
			set(FFMPEG_SUFFIX .dll)
		else()
			set(FFMPEG_LIB_DIR ${FFMPEG_PATH}/ffmpeg/install/lib)
			set(FFMPEG_INCLUDE_DIR ${FFMPEG_PATH}/ffmpeg/install/include)
			set(FFMPEG_LIBS avdevice avformat avcodec swscale avutil swresample avfilter)
			set(ADDITIONAL_FFMPEG_LIBS postproc)
			if (WIN32)
				# mingw
				set(FFMPEG_PREFIX )
				set(FFMPEG_SUFFIX .dll)
			elseif(APPLE)
				set(FFMPEG_PREFIX lib)
				set(FFMPEG_SUFFIX .dylib)
			elseif()
				# UNIX
				set(FFMPEG_PREFIX lib)
				set(FFMPEG_SUFFIX .so)
			endif()
		endif()

		target_link_directories(VipCore PRIVATE ${FFMPEG_LIB_DIR})
		MESSAGE(STATUS "Using ${FFMPEG_LIB_DIR} for ffmpeg library files")


		target_include_directories(VipCore PRIVATE ${FFMPEG_INCLUDE_DIR})
		target_link_libraries(VipCore PRIVATE ${FFMPEG_LIBS})
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

	endif()
endif()

if(WITH_PYTHON)
	# Configure Python
	
	# Specific plugin defines
	target_compile_definitions(VipCore PRIVATE "PY_ARRAY_UNIQUE_SYMBOL=NUMPY_SYMBOL")

	if(NOT MSVC)
		target_link_options(VipCore PRIVATE "LINKER:-export-dynamic")
	endif()


	# configure CPython
	if(DEFINED ENV{VIP_PYTHONHOME} AND DEFINED ENV{VIP_PYTHON_VERSION})
		# Use env. variables VIP_PYTHONHOME (interpreter folder) and VIP_PYTHON_VERSION (like '38') to link to a specific python version
		message(STATUS "VIP_PYTHONHOME is defined and set to $ENV{VIP_PYTHONHOME}")
		message(STATUS "Linking with python$ENV{VIP_PYTHON_VERSION}")

		target_include_directories(VipCore PRIVATE "$ENV{VIP_PYTHONHOME}/include" "$ENV{VIP_PYTHONHOME}/Lib/site-packages/numpy/core/include" ) #conda-like install
		target_include_directories(VipCore PRIVATE "$ENV{VIP_PYTHONHOME}/include" "$ENV{VIP_PYTHONHOME}/Lib/site-packages/numpy/_core/include" ) #pip-like install
		target_link_directories(VipCore PRIVATE "$ENV{VIP_PYTHONHOME}/libs")
		target_link_libraries(VipCore PRIVATE python$ENV{VIP_PYTHON_VERSION})
		#TEST: set the VIP_PYTHONHOME variable
		target_compile_definitions(VipCore PRIVATE -DVIP_PYTHONHOME="$ENV{VIP_PYTHONHOME}")	
		SET(PYTHON_RUNTIME $ENV{VIP_PYTHONHOME})
		
	else()
		# Use find_package to get the Python paths
		find_package (Python3 COMPONENTS Interpreter Development NumPy )
		
		message(STATUS "Use Python include dirs: ${Python3_INCLUDE_DIRS} ${Python3_NumPy_INCLUDE_DIRS}" )
		message(STATUS "Use Python lib dirs: ${Python3_LIBRARY_DIRS}" )
		message(STATUS "Use Python libraries: ${Python3_LIBRARIES}" )
		message(STATUS "Use Python interpreter: ${Python3_EXECUTABLE}" )
		message(STATUS "Use Python STDLIB: ${Python3_STDLIB}" )
		

		target_include_directories(VipCore PRIVATE ${Python3_INCLUDE_DIRS} ${Python3_NumPy_INCLUDE_DIRS})
		target_link_directories(VipCore PRIVATE ${Python3_LIBRARY_DIRS})
		target_link_libraries(VipCore PRIVATE ${Python3_LIBRARIES})
		target_compile_definitions(VipCore PRIVATE -DVIP_PYTHON_INTERPRETER="${Python3_EXECUTABLE}")	
		target_compile_definitions(VipCore PRIVATE -DVIP_PYTHON_SHARED_LIBS="${Python3_LIBRARIES}")
		target_compile_definitions(VipCore PRIVATE -DVIP_PYTHON_STDLIB="${Python3_STDLIB}")
		#target_compile_definitions(VipCore PRIVATE "VIP_PYTHONHOME=$ENV{VIP_PYTHONHOME}")
		SET(PYTHON_RUNTIME ${Python_RUNTIME_LIBRARY_DIRS})	

	endif()

	# Plugin custom installation process
	install(CODE "set(PYTHON_RUNTIME \"${PYTHON_RUNTIME}\")")
	install(CODE "set(CMAKE_INSTALL_BINDIR \"${CMAKE_INSTALL_BINDIR}\")")
	install(CODE "set(CMAKE_SOURCE_DIR \"${CMAKE_SOURCE_DIR}\")")
	install(SCRIPT InstallPython.cmake)

endif()

file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
install(FILES ${HEADER_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/VipCore/")
