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
	find_package(librir QUIET GLOBAL)

	if(NOT librir_FOUND)
		message(STATUS "fetching librir")
		FetchContent_Declare(
			librir
			GIT_REPOSITORY
			https://github.com/IRFM/librir.git
			GIT_TAG better-build

			OVERRIDE_FIND_PACKAGE
		)
		find_package(librir REQUIRED GLOBAL)
	endif()

	if(librir_FOUND)
		message(STATUS "librir found at : ${librir_DIR}")
	endif()

	# include(ExternalProject)

	# ExternalProject_Add(librir
	# GIT_REPOSITORY https://github.com/IRFM/librir.git
	# GIT_TAG better-build
	# CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
	# INSTALL_DIR "${CMAKE_BINARY_DIR}/src/Annotation/librir-prefix/src/librir-build/install"
	# )

	# trick from https://stackoverflow.com/questions/17446981/cmake-externalproject-add-and-findpackage
	# Run cmake twice to populate librirmake
	# if(NOT ${librir_FOUND})
	# #rerun cmake in initial build
	# #will update cmakecache/project files on first build
	# #so you may have to reload project after first build
	# add_custom_target(Rescan ${CMAKE_COMMAND} ${CMAKE_SOURCE_DIR} "CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}" ${CMAKE_ARGS} DEPENDS librir)
	# else()
	# #Rescan becomes a dummy target after first build
	# #this prevents cmake from rebuilding cache/projects on subsequent builds
	# add_custom_target(Rescan)
	# endif()

	# if(${librir_FOUND})
	# add_dependencies(VipAnnotation librir)
	# else()
	# add_dependencies(VipAnnotation Rescan)
	# endif()
	add_dependencies(VipAnnotation librir::video_io)

	# find_package(librir)
	# install(DIRECTORY ${CMAKE_BINARY_DIR}/src/Annotation/librir-prefix/src/librir-build/install/python/librir DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip")
	install(DIRECTORY ${CMAKE_INSTALL_PREFIX}/python/librir DESTINATION thermavip)
	#install(RUNTIME_DEPENDENCY_SET ffmpeg-set DESTINATION thermavip)

endif()

install(TARGETS VipAnnotation
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
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
