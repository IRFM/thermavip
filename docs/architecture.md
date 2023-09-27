

# Global architecture

Thermavip is a C++ framework based on 4 layers (see image below):

- The Qt library which is the only external dependency of Thermavip. You will need a version of Qt >= 5.9
- a Software Development Kit (SDK) composed of 5 shared libraries: 
			1.	[Logging](docs/logging.md): logging to file/console/GUI tools
			2.	[DataTypes](docs/datatypes.md): base data types manipulated by Thermavip (N-D arrays, vector of points, scene models...)
			3.	[Plotting](docs/plotting.md): high performance plotting library for offline/real-time display of multi-sensor data
			4.	[Core](docs/core.md): base library to define and manipulate processing pipelines
			5.	[Gui](docs/gui.md): base graphical components (main window, players...)
			
- The *plugins*, dynamic libraries containing user specific functionalities based on the SDK. These plugins generally provide tools to interact with additional data format (like new video files), display additional GUI features or define new signal processing routines. 
- The *Thermavip* executable itself, which is basically just a plugin container. Its main purpose it to load the plugins from within the *VipPlugins* directory.

![Thermavip architecture](images/architecture.png)

A lot of Thermavip SDK functionalitites rely on Qt mechanisms : meta-object system and the property system.
It is fundamental to understand these systems if you wish to use Thermavip SDK.

	
The repository directory tree looks like the following:
```
bin
icons
doc
docs
fonts
help
tools
src
	-> Core
	-> DataType
	-> External
	-> GenericProcess
	-> Gui
	-> Logging
	-> Plotting
	-> Plugins
		-> DirectoryBrowser
		-> FFmpeg
		...
```

-	*bin* folder: contains all binaries (shared libraries and exectutables) generated when compiling Thermavip (*.dll and *.exe files on Windows).
-	*icons* : contains all image files used by Thermavip.
-	*doc* : contains all documentation files used by Doxygen, as well as doxygen generated documentation.
-	*docs*: markdown documentation
-	*fonts*: additional fonts used within Thermavip
-	*help*: help files (based on [sphinx](https://www.sphinx-doc.org/en/master/))
-	*tools*: additional binary tools used by Thermavip (currently for Windows only)
-	*src* : contains all Thermavip sources. The *src* folder is divided into several directories:
	-	One folder for each SDK library (Core, DataType, Gui, Logging, Plotting).
	-	The *GenericProcess* folder, which contains the Thermavip executable code.
	-	The *Plugins* folder containing all standard Thermavip plugins (on folder per plugin)
	-	The *External* folder which contains external codes (mainly plugins developped by partners)


The Thermavip SDK dependency tree is the following:
```
		Logging		DataType
				\	/	|
				Core	Plotting
					\	/
					Gui
```



Thermavip source code is not very strict with its naming convention. In hidden codes (at least hidden for people manipulating the API, not the sources) might coexists different conventions. The API itself (almost) always follows the same naming convention. It is very close to the Qt library one. The naming convention is the following:

- Classes start with the ```Vip``` prefix and each sub-part of the full name starts with an upper case letter. For instance, a class which goal is to sort a numerical sequence is called **VipSortNumericalSequence**.
- Free functions start with the ```vip``` prefix and each sub-part of the full name starts with an upper case letter. For instance, a function which goal is to sort a numerical sequence is called **vipSortNumericalSequence**.
- Function members start with a lower case letter.
- All macros use upper letters with an underscore separator and the ```VIP_``` prefix. A macro containing code sorting a numerical sequence is called **VIP_SORT_NUMERICAL_SEQUENCE**.

On top of that, the public doxygen documentation is always located in the header files. This prevents developers using only the API to continuously switch between the source code and the html documentation.



