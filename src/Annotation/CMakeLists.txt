cmake_minimum_required(VERSION 3.16)
project(VipAnnotation VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)
include(FetchContent)

FILE(GLOB Sources *.cpp)
FILE(GLOB Headers *.h)
add_library(VipAnnotation SHARED
	${Sources} ${Headers}
)

target_include_directories(VipAnnotation PRIVATE
	${PROJECT_SOURCE_DIR}/../Logging ${PROJECT_SOURCE_DIR}/../DataType ${PROJECT_SOURCE_DIR}/../Core ${PROJECT_SOURCE_DIR}/../Plotting ${PROJECT_SOURCE_DIR}/../Gui
)

target_compile_definitions(VipAnnotation PRIVATE
	VIP_BUILD_ANNOTATION_LIB
)

target_link_libraries(VipAnnotation PRIVATE
	VipDataType
	VipLogging
	VipCore
	VipPlotting
	VipGui
)

# Add default compiler flags
set(TARGET_PROJECT VipAnnotation)
include(${THERMAVIP_COMPILER_FLAGS_FILE})

# Configure Librir

# if(WITH_LIBRIR)
# if(NOT librir_FOUND)
# FetchContent_Declare(
# librir
# GIT_REPOSITORY
# https://github.com/IRFM/librir.git
# GIT_TAG         main
# OVERRIDE_FIND_PACKAGE
# )
# #FetchContent_GetProperties(librir)

# # Fetch the content using previously declared details
# FetchContent_Populate(librir)
# FetchContent_MakeAvailable(librir)
# #install(DIRECTORY ${CMAKE_BINARY_DIR}/src/Annotation/librir-prefix/src/librir-build/install/python/librir DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip")
# endif()
# endif()
IF(WITH_LIBRIR)
	include(FetchContent)
	set (USE_ZFILE ON CACHE INTERNAL "Enable zstd video compression support")
	find_package(librir QUIET GLOBAL)

	if(NOT librir_FOUND)
		message(STATUS "fetching librir")
		FetchContent_Declare(
			librir
			GIT_REPOSITORY
			https://github.com/IRFM/librir.git
			GIT_TAG FIRCA #TEST: put back main
			OVERRIDE_FIND_PACKAGE
		)
		find_package(librir REQUIRED GLOBAL)
	endif()

	if(librir_FOUND)
		message(STATUS "librir found at : ${librir_DIR}")
	endif()

	add_dependencies(VipAnnotation librir::video_io)


	if(UNIX AND NOT APPLE)
		# missing shared libs on unix systems
		install(DIRECTORY "${CMAKE_INSTALL_PREFIX}/lib64/"
		DESTINATION "${CMAKE_INSTALL_PREFIX}/python/librir/libs"
		FILES_MATCHING PATTERN "libgeometry.so*" PATTERN "libtools.so*" PATTERN "libsignal_processing.so*" PATTERN "libvideo_io.so*")
	endif()

	install(DIRECTORY ${CMAKE_INSTALL_PREFIX}/python/librir DESTINATION thermavip)



endif()

install(TARGETS VipAnnotation
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
	#RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS VipAnnotation
	LIBRARY DESTINATION "${THERMAVIP_APPLICATION_DIR}"
	FRAMEWORK DESTINATION "${THERMAVIP_APPLICATION_DIR}"
	RUNTIME DESTINATION "${THERMAVIP_APPLICATION_DIR}"
)
install(TARGETS VipAnnotation
	LIBRARY DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
	FRAMEWORK DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
	RUNTIME DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
)
install(TARGETS VipAnnotation
	LIBRARY DESTINATION "${THERMAVIP_TEST_DIR}"
	FRAMEWORK DESTINATION "${THERMAVIP_TEST_DIR}"
	RUNTIME DESTINATION "${THERMAVIP_TEST_DIR}"
)

file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
install(FILES ${HEADER_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/VipAnnotation/")
