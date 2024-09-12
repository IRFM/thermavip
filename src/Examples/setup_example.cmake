#
# Setup example project
#


set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

# Add SDK inlcude dirs
target_include_directories(${TARGET_PROJECT} PRIVATE ${THERMAVIP_INCLUDE_DIRS})
# Link with SDK
target_link_libraries(${TARGET_PROJECT} PRIVATE ${THERMAVIP_LIBRARIES})
# Add compiler flags plus Qt library
include(${THERMAVIP_COMPILER_FLAGS_FILE})
# Instal in "examples" folder
install(TARGETS ${TARGET_PROJECT}
	LIBRARY DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_EXAMPLE_DIR}"
)