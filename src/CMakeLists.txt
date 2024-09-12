
add_subdirectory(Logging)
add_subdirectory(DataType)
add_subdirectory(Plotting)
add_subdirectory(Core)
add_subdirectory(Gui)
add_subdirectory(Annotation)
add_subdirectory(Thermavip)
add_subdirectory(Plugins)

if(WITH_EXAMPLES)
add_subdirectory(Examples)
endif()

if(WITH_TESTS)
add_subdirectory(Tests)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/External)
add_subdirectory(External)
else()
message(STATUS "No External folder found!")
endif()