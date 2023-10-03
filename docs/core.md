


# Core library

The *Core* library defines the central concepts used within Thermavip application:

-	XML/Binary archiving,
-	Access to configuration files/directories,
-	Plugin mechanism,
-	Most importantly, an **Asynchronous Agents Library based on dataflow** used to define complex processing pipelines

The *Core* library can be used outside of Thermavip application, like any other library. It depends on VipLogging, VipDataType, QtCore, QtGui, QtXml and QtNetwork.


## Archiving

The library defines the interface VipArchive to serialize/deserialize any type of objects, including QObject pointers. Currently, only XML and binary serialization are provided 
through th classes:

-	VipBinaryArchive: binary serialization/deserialization using an internal format,
-	VipXOStringArchive: XML serialization to buffer,
-	VipXOfArchive: XML serialization to file,
-	VipXIStringArchive: XML deserialization from buffer,
-	VipXIfArchive: XML deserialization from file.

Within Thermavip, the serialization mechanism is mainly used to save/restore sessions, and to copy processing pipelines.

### Why?

Why another serialization framework? There are already very good existing libraries like for instance boost.serialization. 
The first basic reason is to avoid introducing another (potentially huge) dependancy to Thermavip.
The second reason is to have a serialization framework that works nicely with Qt metatype system.

### Archiving QObject inheriting types

Let's consider this example:

```cpp
// File objects.h

#pragma once

#include "VipArchive.h"

/// @brief Base class with an integer attribute
class BaseClass : public QObject
{
	Q_OBJECT

public:
	int ivalue;

	BaseClass(int v = 0)
	  : ivalue(v)
	{
	}
};
// Register BaseClass to the Qt metatype system as well as the thermavip layer
VIP_REGISTER_QOBJECT_METATYPE(BaseClass*)


/// @brief Derived class with a double attribute
class DerivedClass : public BaseClass
{
	Q_OBJECT

public:
	double dvalue;

	DerivedClass(int iv = 0, double dv = 0.)
	  : BaseClass(iv)
	  , dvalue(dv)
	{
	}
	~DerivedClass() { 
		bool stop = true;
	}
};
// Register DerivedClass to the Qt metatype system as well as the thermavip layer
VIP_REGISTER_QOBJECT_METATYPE(DerivedClass*)


// define serialization function for both classes

inline VipArchive& operator<<(VipArchive& arch, const BaseClass* o)
{
	return arch.content("ivalue", o->ivalue);
}
inline VipArchive& operator>>(VipArchive& arch, BaseClass* o)
{
	return arch.content("ivalue", o->ivalue);
}

inline VipArchive& operator<<(VipArchive& arch, const DerivedClass* o)
{
	return arch.content("dvalue", o->dvalue);
}
inline VipArchive& operator>>(VipArchive& arch, DerivedClass* o)
{
	return arch.content("dvalue", o->dvalue);
}

```

```cpp
// File main.cpp

#include "objects.h"

#include "VipXmlArchive.h"

#include <iostream>

 
int main(int argc, char** argv)
{
	// register serialization functions
	vipRegisterArchiveStreamOperators<BaseClass*>();
	vipRegisterArchiveStreamOperators<DerivedClass*>();

	// Create a DerivedClass object, but manipulate it through a QObject
	DerivedClass* derived = new DerivedClass(4, 5.6);
	QObject* object = derived;

	// Create archive to serialize object to XML buffer
	VipXOStringArchive arch;

	// Serialize a DerivedClass object through a QObject pointer
	if (!arch.content("Object", object)) {
		std::cerr << "An error occured while writing to archive!" << std::endl;
		return -1;
	}
	
	
	// output the resulting xml content
	std::cout << arch.toString().toLatin1().data() << std::endl;


	// modify object's value to be sure of the result
	derived->ivalue = 23;
	derived->dvalue = 45.6;

	// Now read back the XML content
	VipXIStringArchive iarch(arch.toString());
	if (!iarch.content("Object", object)) {
		std::cerr << "An error occured while reading archive!" << std::endl;
		return -1;
	}

	// check the result
	if (derived->ivalue == 4 && derived->dvalue == 5.6) {
		std::cout << "Read archive success!" << std::endl;
	}


	// now another option: use the builtin factory to read the archive and create a new DerivedClass
	iarch.open(arch.toString());
	QVariant var = iarch.read("Object");
	DerivedClass * derived2 = var.value<DerivedClass*>();

	//check again the result
	if (derived2->ivalue == 4 && derived2->dvalue == 5.6) {
		std::cout << "Read archive success (again)!" << std::endl;
	}

	// we just created a new object, destroy it
	delete derived2;
	delete derived;

	return 0;
}
```

We create 2 classes, `BaseClass` that inherits `QObject` and `DerivedClass` that inherits `BaseClass`. Both classes are visible to Qt metaobject system (Q_OBJECT macro) and thermavip metaobject system (VIP_REGISTER_QOBJECT_METATYPE macro).
Each class defines its own serialization/deserialization functions registered with `vipRegisterArchiveStreamOperators()` function.

First thing to notice is that the `DerivedClass` serialization/deserialization functions do not need to call the base class functions, as they will automatically be called by the serialization library.
When serializing a QObject pointer, all registered serialization functions that support the object metaclass are called, in this case the functions for `BaseClass` (first) and `DerivedClass`.
Basically, the serialization library scan the object inheritance tree and call all registered functins that belong to this tree.

When deserializing an object, 2 options are possible:

-	The archive can be deserialized into an existing object,
-	The archive can read the next object and returns the result as a `QVariant` object.

In the second situation, the archive uses an internal factory to create the right object on the heap and call all related deserialize functions.

### Supported types

The serialization mechanism supports all types that can be stored in a `QVariant` object: all types declared with the Q_DECLARE_METATYPE() macro and all QObject inheriting types registered with VIP_REGISTER_QOBJECT_METATYPE() macro.
By default, objects are serialized/deserialized using the provided functions registered with `vipRegisterArchiveStreamOperators()`. If no functions are provided, the library will try to use the stream operators from/to `QDataStream`.


## Access to configuration files/directories

*Core* library gives access to thermavip configuration files and directories through the following functions:

-	`vipGetDataDirectory()`: Returns the data directory path, used to store persistent informations per user
-	`vipGetTempDirectory()`: Returns the temporary directory path, used to store data that should not remains when exiting from the executable
-	`vipGetLogDirectory()`: Returns the log directory. It is located inside the data directory and stores all log files for the SDK and the plugins.
-	`vipGetPerspectiveDirectory()`: Returns the global perspective directory. It is located next to the executable and stores all global perspectives.
-	`vipGetPluginsDirectory()`: Returns the plugins directory. It is located next to the executable and stores the plugins (dynamic libraries and possible configuration files).

If you intend to use the *Core* library without thermavip executable, these functions should not be used.

## Plugins

The *Core* library provides the base class `VipPluginInterface` used to create custom plugins for the thermavip application.
This interface has no use if the Core library is used as a 3rd party library in another application.

Thermavip plugins are standard shared libraires extending the behavior of the program based on Thermavip SDK.
Usually, thermavip plugins are located in the folder *VipPlugins* located next to the thermavip executable. Use `vipGetPluginsDirectory()` get the full plugins directory path.
Plugins are loaded at runtime from the thermavip main() function. By default, all shared libraries located inside the plugins directory are loaded.
You can specify which plugins are loaded and the load order using a `Plugins.ini` file in the plugins directory. Example of `Plugins.ini`:

```ini
[Default]

plugin/1/name = Ffmpeg
plugin/2/name = WEST
plugin/3/name = Generators
plugin/4/name = NVF
plugin/5/name = WEST_DB
plugin/6/name = ImageProcessing
```

### Minimal plugin

To create a thermavip plugin, you must create a new cmake project with a similar content:

```cmake

cmake_minimum_required(VERSION 3.16)
project(SimplePlugin VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up AUTOMOC and some sensible defaults for runtime execution
# When using Qt 6.3, you can replace the code block below with
# qt_standard_project_setup()
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)

# Create a shared library
add_library(SimplePlugin SHARED
    plugin.cpp plugin.h
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
target_link_libraries(SimplePlugin PRIVATE ${QT_LIBS})

# Find thermavip
find_package(thermavip REQUIRED )

# Add thermavip include directories
target_include_directories(SimplePlugin PRIVATE ${THERMAVIP_INCLUDE_DIRS} )
# Add thermavip library directory
target_link_directories(SimplePlugin PRIVATE ${THERMAVIP_LIB_DIR} )
# Link with thermavip SDK
target_link_libraries(SimplePlugin PRIVATE ${THERMAVIP_LIBRARIES})

# Install plugin
install(TARGETS SimplePlugin
	LIBRARY DESTINATION "${THERMAVIP_PLUGIN_DIR}"
    RUNTIME DESTINATION "${THERMAVIP_PLUGIN_DIR}"
)

```

And then for the plugin code itself:

*plugin.h*
```cpp
#pragma once

#include "VipPlugin.h"

/// @brief Minimal plugin interface
class SimpleInterface : public QObject, public VipPluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "SimpleInterface")
	Q_INTERFACES(VipPluginInterface)
public:

	virtual LoadResult load();
	virtual QByteArray pluginVersion() { return "1.0.0"; }
	virtual void unload() {}
	virtual QString author() { return "Victor Moncada(victor.moncada@cea.fr)"; }
	virtual QString description() { return "Test plugin"; }
	virtual QString link() { return QString(); }
	virtual bool hasExtraCommands() { return true; }
	virtual void save(VipArchive &) {}
	virtual void restore(VipArchive &) {}

};

```

*plugin.cpp*
```cpp
#include "plugin.h"

SimpleInterface::LoadResult SimpleInterface::load()
{
	return Success;
}

```

A plugin must extend the `VipPluginInterface` class and reimplement at least the members `load()`, `author()`, `description()` and `link()`. 
A plugin can do basically anything: manage a new input/output file format (using VipIODevice class), create new processings (see next section),
add new widgets to the main Thermavip interface, add new content to players... You can check the [Ffmpeg](../src/Plugins/Ffmpeg) plugin for a complete example of plugin doing all that.
