cmake_minimum_required(VERSION 3.16)
project(Python VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

FILE(GLOB Sources *.cpp)
FILE(GLOB Headers *.h)
add_library(Python SHARED
    ${Sources} ${Headers}
)

target_include_directories(Python PRIVATE ${THERMAVIP_INCLUDE_DIRS})
target_link_libraries(Python PRIVATE ${THERMAVIP_LIBRARIES})

# Add default compiler flags
set(TARGET_PROJECT Python)
include(${THERMAVIP_COMPILER_FLAGS_FILE})


# Specific plugin defines
target_compile_definitions(Python PRIVATE BUILD_PYTHON_LIB VIP_ENABLE_PYTHON_LINK)
target_compile_definitions(Python PRIVATE "PY_ARRAY_UNIQUE_SYMBOL=NUMPY_SYMBOL")


# configure CPython
if(DEFINED ENV{VIP_PYTHONHOME} AND DEFINED ENV{VIP_PYTHON_VERSION})
	# Use env. variables VIP_PYTHONHOME (interpreter folder) and VIP_PYTHON_VERSION (like '38') to link to a specific python version
	message(STATUS "VIP_PYTHONHOME is defined and set to $ENV{VIP_PYTHONHOME}")
	message(STATUS "Linking with python$ENV{VIP_PYTHON_VERSION}")
	target_include_directories(Python PRIVATE "$ENV{VIP_PYTHONHOME}/include" "$ENV{VIP_PYTHONHOME}/Lib/site-packages/numpy/core/include" ) #conda-like install
	target_include_directories(Python PRIVATE "$ENV{VIP_PYTHONHOME}/include" "$ENV{VIP_PYTHONHOME}/Lib/site-packages/numpy/_core/include" ) #pip-like install
	target_link_directories(Python PRIVATE "$ENV{VIP_PYTHONHOME}/libs")
	target_link_libraries(Python PRIVATE python$ENV{VIP_PYTHON_VERSION})
	#TEST: set the VIP_PYTHONHOME variable
	target_compile_definitions(Python PRIVATE -DVIP_PYTHONHOME="$ENV{VIP_PYTHONHOME}")	
	SET(PYTHON_RUNTIME $ENV{VIP_PYTHONHOME})
	
else()
	# Use find_package to get the Python paths
	find_package (Python3 COMPONENTS Interpreter Development NumPy )
	
	message(STATUS "Use Python include dirs: ${Python3_INCLUDE_DIRS} ${Python3_NumPy_INCLUDE_DIRS}" )
	message(STATUS "Use Python lib dirs: ${Python3_LIBRARY_DIRS}" )
	message(STATUS "Use Python libraries: ${Python3_LIBRARIES}" )
	message(STATUS "Use Python interpreter: ${Python3_EXECUTABLE}" )
	message(STATUS "Use Python STDLIB: ${Python3_STDLIB}" )
	
	target_include_directories(Python PRIVATE ${Python3_INCLUDE_DIRS} ${Python3_NumPy_INCLUDE_DIRS})
	target_link_directories(Python PRIVATE ${Python3_LIBRARY_DIRS})
	target_link_libraries(Python PRIVATE ${Python3_LIBRARIES})
	target_compile_definitions(Python PRIVATE -DVIP_PYTHON_INTERPRETER="${Python3_EXECUTABLE}")	
	target_compile_definitions(Python PRIVATE -DVIP_PYTHON_SHARED_LIBS="${Python3_LIBRARIES}")
	target_compile_definitions(Python PRIVATE -DVIP_PYTHON_STDLIB="${Python3_STDLIB}")
	#target_compile_definitions(Python PRIVATE "VIP_PYTHONHOME=$ENV{VIP_PYTHONHOME}")
	
	SET(PYTHON_RUNTIME ${Python_RUNTIME_LIBRARY_DIRS})	

endif()

# Plugin standard installation
install(TARGETS Python
	LIBRARY DESTINATION "${THERMAVIP_PLUGIN_DIR}"
    FRAMEWORK DESTINATION "${THERMAVIP_PLUGIN_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_PLUGIN_DIR}"
)

# Make Python plugin usable as a library
install(TARGETS Python
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}/Plugins
)
file(GLOB HEADER_FILES ${PROJECT_SOURCE_DIR}/*.h)
install(FILES ${HEADER_FILES} DESTINATION "${CMAKE_INSTALL_PREFIX}/include/Plugins/Python/")

# Plugin custom installation process
install(CODE "set(PYTHON_RUNTIME \"${PYTHON_RUNTIME}\")")
install(CODE "set(CMAKE_INSTALL_BINDIR \"${CMAKE_INSTALL_BINDIR}\")")
install(CODE "set(CMAKE_SOURCE_DIR \"${CMAKE_SOURCE_DIR}\")")
install(SCRIPT InstallPython.cmake)
