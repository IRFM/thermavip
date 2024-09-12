cmake_minimum_required(VERSION 3.16)
project(Thermavip VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

if(MSVC)
	#if (CMAKE_BUILD_TYPE STREQUAL "Release" )
		add_executable(Thermavip  WIN32
			main.cpp Thermavip_resource.rc
		)
	#else()
	#	add_executable(Thermavip 
	#		main.cpp Thermavip_resource.rc
	#	)
	#endif()
else()
	add_executable(Thermavip  
		main.cpp 
	)
endif()



# Configure mimalloc
IF(WITH_MICRO)
	
	include(FetchContent)
	set(MICRO_BUILD_TESTS OFF CACHE INTERNAL "")  # Forces the value
	set(MICRO_BUILD_BENCHS OFF CACHE INTERNAL "")  # Forces the value
	set(MICRO_BUILD_SHARED OFF CACHE INTERNAL "")  # Forces the value
	set(MICRO_BUILD_STATIC OFF CACHE INTERNAL "")  # Forces the value
	set(MICRO_BUILD_TOOLS OFF CACHE INTERNAL "")  # Forces the value
	FetchContent_Declare(
	  micro
	  GIT_REPOSITORY https://github.com/Thermadiag/micromalloc.git
	  GIT_TAG        main 
	  CMAKE_ARGS
	)
	FetchContent_GetProperties(micro)
	if(NOT micro)
	  # Fetch the content using previously declared details
	  FetchContent_Populate(micro)

	  # Bring the populated content into the build
	  add_subdirectory(${micro_SOURCE_DIR} ${micro_BINARY_DIR})
	endif()
	
	target_compile_definitions(Thermavip PRIVATE -DWITH_MICRO)	
	target_link_libraries(Thermavip PRIVATE micro_proxy)
	
	install(
    DIRECTORY "${CMAKE_INSTALL_PREFIX}/bin/"
    DESTINATION "${THERMAVIP_APPLICATION_DIR}"
    FILES_MATCHING 
    PATTERN "*micro_proxy.*"
	)
	
endif()


target_include_directories(Thermavip PRIVATE ${THERMAVIP_INCLUDE_DIRS})

target_link_libraries(Thermavip PRIVATE ${THERMAVIP_LIBRARIES})


set(TARGET_PROJECT Thermavip)
include(${THERMAVIP_COMPILER_FLAGS_FILE})

install(TARGETS Thermavip
	LIBRARY DESTINATION "${THERMAVIP_APPLICATION_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_APPLICATION_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_APPLICATION_DIR}"
)
install(FILES ${PROJECT_SOURCE_DIR}/base_session.session
        DESTINATION ${THERMAVIP_APPLICATION_DIR} )
install(FILES ${PROJECT_SOURCE_DIR}/changelog.txt
        DESTINATION ${THERMAVIP_APPLICATION_DIR} )