cmake_minimum_required(VERSION 3.16)
project(Python VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)
include(GNUInstallDirs)

find_package(QT NAMES Qt5 Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui Network Xml Widgets OpenGL PrintSupport)

FILE(GLOB Sources *.cpp)
FILE(GLOB Headers *.h)
add_library(Python SHARED
    ${Sources} ${Headers}
)

target_include_directories(Python PRIVATE
    ${PROJECT_SOURCE_DIR}/../../Logging 
	${PROJECT_SOURCE_DIR}/../../DataType 
	${PROJECT_SOURCE_DIR}/../../Core 
	${PROJECT_SOURCE_DIR}/../../Plotting 
	${PROJECT_SOURCE_DIR}/../../Gui
)

target_link_libraries(Python PRIVATE
    Qt::Core
    Qt::Gui
    Qt::Network
	Qt::Widgets
	Qt::OpenGL
	Qt::PrintSupport
    Qt::Xml
)

target_link_libraries(Python PRIVATE
	VipDataType
	VipLogging
	VipCore
	VipPlotting
	VipGui
)

# Add default compiler flags
set(TARGET_PROJECT Python)
include(${PROJECT_SOURCE_DIR}/../../compiler_flags.cmake)

# Specific plugin defines
target_compile_definitions(Python PRIVATE BUILD_PYTHON_LIB VIP_ENABLE_PYTHON_LINK)
target_compile_definitions(Python PRIVATE "PY_ARRAY_UNIQUE_SYMBOL=NUMPY_SYMBOL")


# configure CPython
if(DEFINED ENV{VIP_PYTHON_HOME} AND DEFINED ENV{VIP_PYTHON_VERSION})
	# Use env. variables VIP_PYTHON_HOME (interpreter folder) and VIP_PYTHON_VERSION (like '38') to link to a specific python version
	message(STATUS "VIP_PYTHON_HOME is defined and set to $ENV{VIP_PYTHON_HOME}")
	message(STATUS "Linking with python$ENV{VIP_PYTHON_VERSION}")
	target_include_directories(Python PRIVATE "$ENV{VIP_PYTHON_HOME}/include" "$ENV{VIP_PYTHON_HOME}/Lib/site-packages/numpy/core/include" )
	target_link_directories(Python PRIVATE "$ENV{VIP_PYTHON_HOME}/libs")
	target_link_libraries(Python PRIVATE python$ENV{VIP_PYTHON_VERSION})
	
	SET(PYTHON_RUNTIME $ENV{VIP_PYTHON_HOME})
	
else()
	# Use find_package to get the Python paths
	find_package (Python COMPONENTS Interpreter Development NumPy )
	
	target_include_directories(Python PRIVATE ${Python_INCLUDE_DIRS} ${Python_NumPy_INCLUDE_DIRS})
	target_link_directories(Python PRIVATE ${Python_LIBRARY_DIRS})
	target_link_libraries(Python PRIVATE ${Python_LIBRARIES})
	
	#target_compile_definitions(Python PRIVATE "VIP_PYTHONHOME=$ENV{VIP_PYTHON_HOME}")
	
	SET(PYTHON_RUNTIME ${Python_RUNTIME_LIBRARY_DIRS})	

endif()

# Plugin standard installation
install(TARGETS Python
	LIBRARY DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip/VipPlugins"
    FRAMEWORK DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip/VipPlugins"
    RUNTIME DESTINATION "${CMAKE_INSTALL_PREFIX}/thermavip/VipPlugins"
)

# Plugin custom installation process
install(CODE "set(PYTHON_RUNTIME \"${PYTHON_RUNTIME}\")")
install(CODE "set(CMAKE_INSTALL_BINDIR \"${CMAKE_INSTALL_BINDIR}\")")
install(CODE "set(CMAKE_SOURCE_DIR \"${CMAKE_SOURCE_DIR}\")")
install(SCRIPT InstallPython.cmake)
