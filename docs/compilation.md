

# Compilation

Thermavip is fully developed in C++ with a few parts in Python. Python is not mandatory to compile or to launch Thermavip, but a valid Python installation 
is required to build the Python plugin. The Thermavip SDK is based on the Qt library (version >= Qt5.9).

Thermavip was developed and tested using GCC 6.4.0 and 10.1.0 on Windows (mingw) and Linux and Visual Studio 2015 and 2019. 
Over compilers or platforms have not been tested yet, but porting Thermavip should not be a problem as long as the couple compiler/platform is supported by the Qt library.

Thermavip must be built using provided cmake configuration files. The following options are available (OFF by default):

-	*WITH_LONG_DOUBLE*: build with long double support for temporal signals. Only usefull if you expect timestamps with nanosecond precision.
-	*WITH_FFMEG*: build the Ffmpeg plugin in order to read/record mpeg videos. The plugin is built with a precompiled ffmpeg version for MSVC. For GCC, a specific ffmpeg version will be downloaded and built.
-	*WITH_HDF5*: build the H5StillImage plugin (require HDF5 library). Usually not required.
-	*WITH_PYTHON*: build the Python plugin. This requires a valid Python installation. It is possible to use a specific Python installation through the env. variables VIP_PYTHON_HOME (similar to PYHTONHOME) and VIP_PYTHON_VERSION (like "38").
-	*WITH_LIBRIR*: download, build and install the [librir](https://github.com/IRFM/librir) library. Having the library folder next to Thermavip application allows to read-back/record IR videos using H264/HEVC codecs.
-	*WITH_WEST*: download, build and install WEST related plugins/tools. Only works from within IRFM network.
-	*LOCAL_INSTALL* (ON by default): performs a local installation inside the build directory
 
Usage on Windows (build Visual Studio solution):

```cmake
git clone https://github.com/IRFM/thermavip.git
cd thermavip
mkdir build
cd build

cmake .. -G "Visual Studio 16 2019" -DCMAKE_BUILD_TYPE=Release  -DWITH_PYTHON=ON -DWITH_FFMPEG=ON -DWITH_LIBRIR=ON -DCMAKE_PREFIX_PATH="absolute_path_to_QT_library" 

```

Once installed, the installation folder will  contain the following directories:

-	*bin*: contains SDK shared libraries binaries (VipLogging, VipDataType, VipCore, VipGui, VipAnnotation)
-	*fonts*: contains the additional fonts that Thermavip application uses
-	*help*: Thermavip application html help
-	*icons*: Thermavip application icons
-	*include*: include folder
-	*lib*: SDK shared libraries
-	*share*: configuration files suitable for pkg-config
-	*skins*: Thermavip application skins
-	*tests*: All tests and example programs
-	*thermavip*: Thermavip application itself with all required binaries and resources

## Use Thermavip SDK from another application

You can use the Thermavip libraries from any foreign application using the provided cmake configuration files.
The CMakeLists.txt of an application using Thermavip SDK will look like this (thermavip installation must be in CMAKE_PREFIX_PATH):

```cmake

cmake_minimum_required(VERSION 3.16)
project(MyApp VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)

# Create a shared library
add_executable(MyApp
    my_source_files
)

# Find Qt
find_package(QT NAMES Qt5 Qt6 REQUIRED )
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGL Core Gui Xml Network Sql PrintSupport Svg)
set(QT_LIBS Qt::Core
    Qt::Gui
    Qt::Network
	Qt::Widgets
	Qt::OpenGL
	Qt::PrintSupport
    Qt::Xml
	Qt::Sql
	Qt::Svg
	)
	
# Link Qt libraries
target_link_libraries(MyApp PRIVATE ${QT_LIBS})

# Find thermavip
find_package(thermavip REQUIRED )

# Add thermavip include directories
target_include_directories(MyApp PRIVATE ${THERMAVIP_INCLUDE_DIRS} )
# Add thermavip library directory
target_link_directories(MyApp PRIVATE ${THERMAVIP_LIB_DIR} )
# Link with thermavip SDK
target_link_libraries(MyApp PRIVATE ${THERMAVIP_LIBRARIES})

#...

```

The thermavip package exposes the following variables:

-	*THERMAVIP_FOUND*: true if thermavip package was found
-	*THERMAVIP_INCLUDE_DIRS*: all thermavip include directories
-	*THERMAVIP_LIB_DIR*: SDK libraries directory
-	*THERMAVIP_BIN_DIR*: SDK binaries directory
-	*THERMAVIP_APPLICATION_DIR*: thermavip application directory
-	*THERMAVIP_PLUGIN_DIR*: thermavip application plugins directory
-	*THERMAVIP_LIBRARIES*: thermavip SDK libraries


## Build Thermavip plugins

Building a Thermavip plugin is very similar to building any other kind of application using Thermavip SDK.
The cmake file is the same as above one with the following specificities:

-	`add_library(MyPlugin SHARED ...)` must be used instead of `add add_executable(...)`
-	The installation part must contain:

```cmake

# Install plugin
install(TARGETS SimplePlugin
	LIBRARY DESTINATION "${THERMAVIP_PLUGIN_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_PLUGIN_DIR}"
)
```
to install the plugin in the right location.

Check the [Core](core.md) library documentation for a full example of cmake file used to build a plugin.
