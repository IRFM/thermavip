.. _architecture:
 
Thermavip Software Architecture
=========================================================

----------------------------------
General architecture
----------------------------------

Thermavip is based on a versatile software architecture composed of a C++ \ **S**\oftware \ **D**\evelopment \ **K**\it (SDK) and a plugin mechanism. The SDK is based on `Qt <https://www.qt.io>`_ , a powerfull and open source library for generic programming and Graphical User Interface. The plugins can use any additional external libraries, like *VTK*, *Ffmpeg*, *OpenCV*, etc.

Currently the software builds on Windows (starting version 7) and any Linux distribution supporting Qt (that is, most of them).

Simply speaking, the Thermavip framework is based on 4 layers (see :numref:`Fig. %s <soft_architecture>`):

1.	The Qt library which is the only external dependency of Thermavip. You will need a version of Qt >= 5.4.
2.	A Software Development Kit (SDK) composed of 5 shared libraries: Logging, DataType, Plotting, Core and Gui. Those libraries only rely on QtCore, QtGui, QtWidgets, QtXml and QtNetwork.
3.	The plugins, dynamic libraries containing user specific functionalities based on the SDK. These plugins generally provide tools to interact with additional data format (like new video files), display additional GUI features or define new signal processing routines. A plugin can depend on a subset of the SDK or the full SDK. It can also use additional external libraries.
4.	The Thermavip executable itself, which is basically just a plugin container. Its main purpose it to load the plugins from within the VipPlugins directory. Thermavip defines 2 different executables: *Thermavip* that displays a Graphical User Interface (GUI) and *Thtools* (same as *Thermavip* but without a GUI and GUI related libraries, for real-time systems mainly).

.. _soft_architecture: 

.. figure:: images/architecture.png
   :alt: Thermavip Software Architecture
   :figclass: align-center
   :align: center
   :scale: 50%
   
   *Thermavip Software Architecture*
   

----------------------------------
Description of the SDK
----------------------------------
   
The Software Development Kit of Thermavip is composed of 5 shared libraries:

1. *Logging*: provides thread/process safe logging purpose functionalities. Its main components are the VipLogging class and the VIP_LOG_INFO, VIP_LOG_WARNING and VIP_LOG_ERROR macros. This library is designed to be used within all other Thermavip libraries/plugins in time-sensible algorithms without consuming too much CPU time. 
   Dependencies: QtXml, QtCore
2. *DataType*: defines the data structures commonly manipulated within Thermavip: images, N-D arrays, curves, histograms… This library also contains the binary serialization functions for these types (usually for network communication). Most data structures in DataType use the Copy On Write (COW, or implicit sharing) idiom to avoid unnecessary copies and memory consumption.
   Dependencies: QtCore, QtGui
3. *Plotting*: libraries providing 2D graphical plotting: images, spectrograms, curves, histograms, pie charts, bart charts… All plotting features (especially images/spectrograms) are optimized to be used in a real-time environments and to display huge data throughput.
   Dependencies: Logging, DataType, QtWidgets
4. *Core*: base library of Thermavip's SDK. It defines the main concepts and classes that should be used to extend Thermavip through plugins: Processing pipelines, Plugins management, network communication, data serialization, signal loading/saving, updates management...
   Dependencies: Logging, DataType, QtNetwork
5. *Gui*: defines all common widgets and the main Graphical User Interface of Thermavip.
   Dependencies: Logging, DataType, Core, Plotting

	
----------------------------------
Plugins
----------------------------------

Plugins are shared libraries loaded at runtime to add new functionalities within Thermavip. They could bring:

* Support for new video/signal file formats,
* Support for real time camera acquisition, display and storage,
* Additional image/signal processing algorithms,
* Additional Graphical Interfaces,
* And any kind of additional features.

Thermavip plugins use the Qt plugin mechanism. All plugins are stored in the VipPlugins folder located at the same level as the binary executable. It is possible to load a subset of the available plugins in the VipPlugins folder by defining a setting file called *Plugins.ini* in this folder. This file looks like this:

.. code-block:: python

	[Default]
	plugin/1/name=Tokida
	plugin/2/name=ImageProcessing
	plugin/3/name=InfraTechFiles
	plugin/4/name=DirectoryBrowser

	[CustomConfig]
	plugin/1/name=Tokida
	plugin/2/name=ImageProcessing

In this case, only the plugins *Tokida*, *ImageProcessing*, *InfraTechFiles* and *DirectoryBrowser* will be loaded by default. Thermavip can be launched with different command line arguments to load another plugin configuration (like the *CustomConfig* one).

Plugins are automatically loaded at the start of Thermavip and cannot be unloaded after. If you need a different plugin configuration, you will need to edit the Plugins.ini file and restart Thermavip.

A plugin uses the Thermavip SDK to provide new functionalities. Therefore, the plugin must be compiled with the same version of the SDK and the binary executable *Thermavip*. 
