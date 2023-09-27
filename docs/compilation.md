

# Compilation

Thermavip is fully developed in C++ with a few parts in Python. Python is not mandatory to compile or to launch Thermavip, but a valid Python installation 
is required to enable the Python plugin. The Thermavip SDK is based on the Qt library (version >= Qt5.9).


Thermavip was developed and tested using GCC  4.9.1, 6.4.0 and 10.1.0 on Windows (mingw) and Linux and Visual Studio 2015 and 2019. Over compilers or platforms have not been tested yet, but porting Thermavip should not be a problem as long as the couple compiler/platform is supported by the Qt library.


To build Thermavip, use the *src.pro* file in the *src* directory based on qmake. 
To build Thermavip with gcc, use:
```
qmake src.pro -r
make
```

To build Thermavip with visual compiler, use:
```
qmake src.pro  -tp vc -recursive
nmake
```

Generated binaries are stored in the *bin* directory. Libraries are stored in the *lib* directory. Plugins are copied to the *bin/VipPlugins* directory.

Thermavip build is composed of several qmake files:

-	*build_lib_default.pri*: configuration file for Thermavip libraries 
-	*build_plugin_default.pri*: configuration file for Thermavip plugins. Include this file in your plugin project file (MyPlugin.pro)
-	*build_app_default.pri*: configuration file for applications based on Thermavip SDK
-	*additional_libs.pri*: defines additional libraries paths for plugins
-	*compiler_flags.pri*: defines compiler flags used by all projects
-	*src.pro*: the global project file
-	a *.pro* file in each of the SDK directories (DataType, Plotting, Core, Gui and Logging)
-	Plugins.pro in the Plugins directory. It contains a reference to all other plugin directories.

To create a plugin, follow these steps:

-	Create a new folder in the *Plugins* directory (like *MyPlugin*)
-	Create a new *MyPlugin.pro* file in this directory. The target must be set to *lib* and the file must include *build_plugin_default.pri* (see the Ffmpeg plugin for more details).
-	You must at least create a header file extending the *VipPluginInterface* interface. Thermavip plugins follow the rules of standard Qt plugins (see Qt doc for more details).
-	Add the reference to your plugin to the *Plugins.pro* file.
-	After defining all the interface classes of your plugin, call qmake to generate the corresponding makefile or Visual Studio project.

## Default plugins
The standard Thermavip installation ships several default plugins:

-	AdvancedDisplay: completely reorganize the way players are displayed within workspaces
-	Ffmpeg: allow to read back video files (MP4, MPG, AVI...) and create video files from players/workspaces
-	Generators: defines a few video generators
-	H5StillImage: defines a video file format using HDF5
-	ImageProcessing: defines a few image processing algorithms
-	IRCameras: readers for several more or less standard infrared video file formats
-	Python: creates a small Python development environment and add the possibility to define Python scripts applied to videos/plots

The plugins Ffmpeg, H5StillImage and Python require the libraries [ffmpeg](https://ffmpeg.org/), [hdf5](https://www.hdfgroup.org/solutions/hdf5/) and [cpython](https://www.python.org/) to build and run. Thermavip will use the following environment variables to find these libraries:

-	VIP_FFMPEG_INCLUDE_PATH: all ffmpeg include path(s)
-	VIP_FFMPEG_LIB_PATH: ffmpeg library path
-	VIP_FFMPEG_LIBS: ffmpeg libraries
-	VIP_HDF5_INCLUDE_PATH: hdf5 include path(s)
-	VIP_HDF5_LIB_PATH: hdf5 library path
-	VIP_HDF5_LIBS: hdf5 libraries
-	VIP_PYTHON_INLUDE_PATH: cpython include path(s)
-	VIP_PYTHON_LIB_PATH: cpython library path(s)
-	VIP_PYTHON_LIBS: cpython library

## CMake support

Currently, Thermavip only compiles with Qt versions below Qt6 that rely on qmake for their compilation. Therefore, CMake is currently not supported. At some point, Thermavip will switch to Qt>=6 and will move to cmake as its build system.