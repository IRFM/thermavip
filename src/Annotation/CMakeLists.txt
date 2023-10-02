cmake_minimum_required(VERSION 3.16)
project(VipAnnotation VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

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
include(${PROJECT_SOURCE_DIR}/../compiler_flags.cmake)



# Configure Librir
IF(WITH_LIBRIR)
	include(ExternalProject)
	ExternalProject_Add(librir
	  GIT_REPOSITORY https://github.com/IRFM/librir.git
	  GIT_TAG main
	)
	add_dependencies(VipAnnotation librir)
	install(DIRECTORY ${CMAKE_BINARY_DIR}/src/Annotation/librir-prefix/src/librir-build/install/librir DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip")
	
endif()



install(TARGETS VipAnnotation
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} 
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS VipAnnotation
    LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip"
)
install(TARGETS VipAnnotation
	LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/tests"
)


file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
install(FILES ${HEADER_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/VipAnnotation/")
