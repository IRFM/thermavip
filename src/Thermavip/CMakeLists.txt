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
	  GIT_REPOSITORY https://github.com/Thermadiag/micro.git
	  GIT_TAG        tiny_lock 
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
    DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
    FILES_MATCHING 
    PATTERN "*micro_proxy.*"
	)
	#install(FILES "${CMAKE_INSTALL_PREFIX}/bin/*micro_proxy.*"
    #    DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip" )
endif()



target_include_directories(Thermavip PRIVATE
    ${PROJECT_SOURCE_DIR}/../Logging ${PROJECT_SOURCE_DIR}/../DataType ${PROJECT_SOURCE_DIR}/../Core ${PROJECT_SOURCE_DIR}/../Plotting ${PROJECT_SOURCE_DIR}/../Gui ${PROJECT_SOURCE_DIR}/../Annotation
)

target_link_libraries(Thermavip PRIVATE
	VipDataType
	VipLogging
	VipCore
	VipPlotting
	VipGui
	VipAnnotation
)


set(TARGET_PROJECT Thermavip)
include(${PROJECT_SOURCE_DIR}/../compiler_flags.cmake)

install(TARGETS Thermavip
	LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
)
install(FILES ${PROJECT_SOURCE_DIR}/base_session.session
        DESTINATION ${CMAKE_INSTALL_PREFIX}/thermavip )
install(FILES ${PROJECT_SOURCE_DIR}/changelog.txt
        DESTINATION ${CMAKE_INSTALL_PREFIX}/thermavip )