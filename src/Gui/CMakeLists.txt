cmake_minimum_required(VERSION 3.16)
project(VipGui VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

FILE(GLOB Sources *.cpp)
FILE(GLOB Headers *.h)
add_library(VipGui SHARED
    ${Sources} ${Headers}
)

target_include_directories(VipGui PRIVATE
    ${PROJECT_SOURCE_DIR}/../Logging ${PROJECT_SOURCE_DIR}/../DataType ${PROJECT_SOURCE_DIR}/../Core ${PROJECT_SOURCE_DIR}/../Plotting
)

target_compile_definitions(VipGui PRIVATE
    VIP_BUILD_GUI_LIB
)

# Add WebEnging if possible
find_package(Qt${QT_VERSION_MAJOR}WebEngine REQUIRED)
find_package(Qt${QT_VERSION_MAJOR}WebEngineWidgets REQUIRED)
if (Qt${QT_VERSION_MAJOR}WebEngine_FOUND)
	if (Qt${QT_VERSION_MAJOR}WebEngine_VERSION VERSION_LESS 5.15.0)
		message(STATUS "Too old web engine version!")
	else()
		message(STATUS "Using web engine")
		find_package(Qt${QT_VERSION_MAJOR}WebEngineWidgets REQUIRED)
		if(${QT_VERSION_MAJOR} LESS 6)
		target_link_libraries(VipGui PRIVATE
		Qt5::WebEngine
		Qt5::WebEngineWidgets
		)
		else()
		target_link_libraries(VipGui PRIVATE
		Qt::WebEngine
		Qt::WebEngineWidgets
		)
		endif()
		target_compile_definitions(VipGui PRIVATE __VIP_USE_WEB_ENGINE)
	endif()
endif()


target_link_libraries(VipGui PRIVATE
	VipDataType
	VipLogging
	VipCore
	VipPlotting
)

set(TARGET_PROJECT VipGui)
include(${THERMAVIP_COMPILER_FLAGS_FILE})

install(TARGETS VipGui
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} 
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FRAMEWORK DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(TARGETS VipGui
    LIBRARY DESTINATION "${THERMAVIP_APPLICATION_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_APPLICATION_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_APPLICATION_DIR}"
)
install(TARGETS VipGui
	LIBRARY DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
)
install(TARGETS VipGui
	LIBRARY DESTINATION "${THERMAVIP_TEST_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_TEST_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_TEST_DIR}"
)



file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
install(FILES ${HEADER_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/VipGui/")
