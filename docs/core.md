
# Core library

The *Core* library defines the central concepts used within Thermavip application:

-	XML/Binary archiving,
-	Access to configuration files/directories,
-	Plugin mechanism,
-	Most importantly, an **Asynchronous Agents Library based on dataflow** used to define complex processing pipelines

The *Core* library can be used outside of Thermavip application, like any other library. It depends on VipLogging, VipDataType, QtCore, QtGui, QtXml and QtNetwork.


## Archiving

The library defines the interface VipArchive to serialize/deserialize any type of objects, including QObject pointers. Currently, only XML and binary serialization are provided 
through the classes:

-	`VipBinaryArchive`: binary serialization/deserialization using an internal format,
-	`VipXOStringArchive`: XML serialization to buffer,
-	`VipXOfArchive`: XML serialization to file,
-	`VipXIStringArchive`: XML deserialization from buffer,
-	`VipXIfArchive`: XML deserialization from file.

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

 @brief Base class with an integer attribute
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


 @brief Derived class with a double attribute
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

### Save/restore state

The VipArchive base class provides a simple way to save/restore its state through the members `VipArchive::save()` and `VipArchive::restore()`.
This provides a simple way to handle non present content in an archive when reading it:

```cpp

VipXIfArchive arch("my_xml_file.xml");

// ... Start reading the archive

// Save current state as we are going to read a value that might not be present 
arch.save();

int a_value=0;
if(!arch.content("a_value",a_value) {
	// we failed to read "a_value", restore to previous state and keep reading
	arch.restore();
}

// ... keep reading the archive
```

### Deserialization progress

The `VipArchive` class provides the following signals:

```cpp
void rangeUpdated(double min, double max);
void valueUpdated(double value);
void textUpdated(const QString& text);
```

The signal `rangeUpdated()` is emitted when opening the archive, and `valueUpdated()/textUpdated()` are emitted during the deserialization process.
These signals provide a simple way to follow the deserialization process for huge archives in order, for instance, to display a progress bar.
Within Thermavip, the progress bar will be automatically displayed if connecting these signals to the corresponding slots of a `VipProgress` object.

This is automatically done when loading a session file (or the previous session) from Thermavip application.

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

Thermavip plugins are standard shared libraires extending the behavior of the program based on Thermavip SDK, and are based on Qt [plugin mechanism](https://doc.qt.io/qt-6/plugins-howto.html).
Usually, thermavip plugins are located in the folder *VipPlugins* next to the thermavip executable. Use `vipGetPluginsDirectory()` get the full plugins directory path.
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

 @brief Minimal plugin interface
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
add new widgets to the main Thermavip interface, add new content to players... You can check the [Ffmpeg](../src/Plugins/Ffmpeg) plugin for a complete example of plugin doing (almost) all that.


## Asynchronous Agent library

The main feature of the *Core* library is the **Asynchronous Agents Library** based on dataflow, which is used to define complex processing pipelines.

Within thermavip, an **Agent** (or processing) is a class inheriting `VipProcessingObject` and defining any number of inputs, outputs and properties.
Each output can be connected to any number of other processing's inputs/properties. Setting a processing output value will dispatch the value to all connected inputs/properties, and triggers the corresponding processing if needed.

This allows to define complex, data driven, asynchronous processing pipelines working on any kind of data: images, n-d arrays, 1d + time signals, numerical values, histograms, scene models, texts...

### Using VipProcessingObject to create new Agents

When subclassing `VipProcessingObject`, you can declare any number of inputs, outputs or properties (see `VipInput`, `VipMultiInput`, `VipOutput`, `VipMultiOutput`, `VipProperty` and `VipMultiProperty` classes for more details). 
There definition is based on the Qt meta object system through the macro Q_PROPERTY (see example code above). You can define additional information for a `VipProcessingObject` (icon, description, category) using the Qt Q_CLASSINFO macro. 
You can also provide a description for all inputs/outputs/properties using the Q_CLASSINFO macro.

To retrieve a processing input, use `VipProcessingObject::inputAt()` or `VipProcessingObject::inputName()`. To retrieve a property, use `VipProcessingObject::propertyAt()` or
`VipProcessingObject::propertyName()`. To retrieve an output or set an output data, use  `VipProcessingObject::outputAt()` or `VipProcessingObject::outputName()`.

The processing itself is done in the reimplementation of the `VipProcessingObject::apply()` member. The processing is applied using the function `VipProcessingObject::update()`.
The processing will only be applied if at least one or all the inputs are new, depending on the schedule strategy.

Processings exchange data using the `VipAnyData` class which basically stores a QVariant (the data itself), a timestamp and additional attributes on the form of a `QVariantMap`.

Basic usage:

```cpp

class FactorMultiplication : public VipProcessingObject
{
	Q_OBJECT
	VIP_IO(VipInput input_value) 	//define the input of name 'input_value'
	VIP_IO(VipOutput output_value) 	//define the output of name 'output_value'
	VIP_IO(VipProperty factor) 		//define the property of name 'factor'

	// Optional information
	Q_CLASSINFO("description","Multiply an input numerical value by a factor given as property") //define the description of the processing
	Q_CLASSINFO("icon","path_to_icon") //define an icon to the processing
	Q_CLASSINFO("category","Miscellaneous/operation") //define a processing category with a slash separator

	Q_CLASSINFO("input_value","The numerical value that will be multiplied")
	Q_CLASSINFO("factor","The multiplication factor")
	Q_CLASSINFO("output_value","The result of the multiplication between the input value and the property factor")

public:
	FactorMultiplication(QObject * parent = nullptr)
	:VipProcessingObject(parent)
	{
		//set a default multiplication factor of 1
		this->propertyAt(0)->setData(1.0);
	}

protected:
	virtual void apply()
	{
		//compute the output value
		double value = inputAt(0)->data().value<double>() * propertyAt(0)->data().value<double>();
		//set the output value
		outputAt(0)->setData( create(value) );
	}
};

//use case
//create 2 FactorMultiplication processing with a multiplication factor of 2 and 3
FactorMultiplication processing1;
processing1.propertyAt(0)->setData(2.0);

FactorMultiplication processing2;
processing2.propertyAt(0)->setData(3.0);

//connect the first processing output to the seconds processing input
processing1.outputAt(0)->setConnection(processing2.inputAt(0)

//set the input value
processing1.inputAt(0)->setData(1);

// Since we are not in Asynchronous mode, the result is not computed until we call the update() function.
// This will recursively call update() for all source processings.
processing2.update();

//print the result (1*2*3 = 6)
std::cout<< processing2.outputAt(0)->data().value<double>() << std::endl;

```

In this example, we define the `FactorMultiplication` agent that just multiplies its input value by a constant factor and returns the result.
The input is expected to be a numerical value of any type and converted to `double`.

Then, 2 FactorMultiplication objects are created and connected. The input value is set to the first processing of the pipeline, and update() is called on the leaf processing.
This will recursively call update() for all source processings.

This example displays a basic usage of *synchronous* processings: the update() member must be explicitly called to trigger a processing.


### Inputs, outputs, properties


A VipProcessingObject can define any number of inputs (`VipInput`), outputs (`VipOutput`) and properties (`VipProperty`), using Qt Q_PROPERTY macro or the `VIP_IO` one (see example above).

Each processing input can be connected to another processing output using `VipProcessingIO::setConnection()` member.
An input can be connected to any number of output, but an output can be connected to only one input.

Processing inputs ar simply set using `VipProcessingIO::setData()` (see example above). 
If the processing is asynchronous, this will trigger the processing based on its schedule strategy. If the processing is synchronous, an explicit call to update() is required to apply the processing.

A property (`VipProperty` object) is similar to a `VipInput` and can be connected to another processing output, but setting a property will never trigger a processing (as opposed to an input). 
Furtheremore, properties do not use input buffers as opposed to VipInput class.

For synchronous processing, calling update() on a leaf processing will recursively update its source processings.


### Scheduling strategies


A `VipProcessingObject` can be either synchronous (default) or asynchronous. Use `VipProcessingObject::setScheduleStrategy()` or `VipProcessingObject::setScheduleStrategies()` to modify the processing scheduling strategy.

Synchronous processing are not automatically executed and require explicit calls to VipprocessingObject::update(), which will internally call VipProcessingObject::apply() in the calling thread (default) or through the internal task pool if `NoThread` strategy is unset.

Asynchronous processing (using `Asynchronous` flag) are automatically executed in the internal task pool when new input data are set, based on the strategy flags. 
If the processing execution is not fast enough compared to the pace at which its inputs are set, the input data are buffered in each `VipInput` buffer (`VipDataList` object) and processing executions are scheduled inside the internal task pool. 
By default, each input uses a `VipFIFOList`, but the buffer type can be set to `VipLIFOList` or `VipLastAvailableList` (no buffering, always use the last data).

Each input buffer can have a maximum size based either on a number of inputs or on the size in Bytes of buffered inputs. 
Use `VipDataList::setMaxListSize()`, `VipDataList::setMaxListMemory()` and `VipDataList::setListLimitType()`
to control this behavior. Use `VipProcessingManager` to change the behavior of ALL existing input buffers and to ALL future ones.

Currently, `VipProcessingObject` supports the following scheduling flags:

-	`OneInput`: launch the processing if only one new input data is set (default behavior).
-	`AllInputs`: launch the processing if all input data are new ones.
-	`Asynchronous`: the processing is automatically triggered depending on the schedule strategy (OneInput or AllInputs). Otherwise, you must call the update() function yourself.
-	`SkipIfBusy`: when the processing is triggered, skip the processing if it is currently being performed. Otherwise, it will be performed after the current one finish. Only works in asynchronous mode.
-	`AcceptEmptyInput`: update the processing even if one of the input data is empty.
-	`SkipIfNoInput`: do not call the apply() function if no new input is available. Only works in synchronous mode. The update() function directly call apply() without going through the thread pool. This is the default behavior. This flag is ignored if Asynchronous is set.
-	`NoThread`: default behavior for synchronous processing. VipProcessingObject::apply() is called with the update() member if set. Otherwise, it goes through the internal task pool to ensure that VipProcessingObject::apply() is ALWAYS called from the same thread.


### Archiving


By default, all `VipProcessingObject` can be archived into a `VipArchive` object. This is used within thermavip to build session files, but also for copying VipProcessingObject objects.

The default serialization function saves the processing VipProperty data, the connection status, the scheduling strategies, the processing attributes, and most other VipProcessingObject properties.

It is possible to define additional serialization functions for your class using `vipRegisterArchiveStreamOperators()`.


### Error handling


A processing that must notify an error in the `VipProcessingObject::apply()` reimplementation should use the `VipProcessingObject::setError()` member.
This will emit the signal `error(QObject*, const VipErrorData&)`, and will print an error message if logging is enabled for the corresponding error code (see `VipProcessingObject::setLogErrorEnabled()`).

The error code is reseted before each call to VipProcessingObject::apply().


### Thread safety


VipProcessingObject class is NOT thread safe, while all members are reentrant.

Only a few members are thread safe (mandatory for asynchronous strategy):

-	All error related functions: `setError()`, `resetError()`...
-	All introspection functions: `className()`, `description()`, `inputDescription()`, `inputNames()`...
-	All I/O access members: `topLevelInputAt()`, `inputAt()`, `outputAt()` ...
-	Adding/retrieving data from VipInput/VipOutput/VipProperty objects, like `inputAt(0)->setData(my_data)`, `clearInputBuffers()`...
-	Calls to `setEnabled()`, `isEnabled()`, `setVisible()`, `isVisible()`
-	Calls to `update()` and `reset()`


### IO devices


Source processings (defining only outputs) and leaf ones (defining only inputs) are called IO devices, and should (most of the time) inherit `VipIODevice`.

`VipIODevice` is a special kind of `VipProcessingObject` providing additional functionalities to handle the generation of data (for instance from a video file).

VipIODevice provides both a common implementation and an abstract interface for devices that support reading and writing of VipAnyData.
VipIODevice is abstract and can not be instantiated, but it is common to use the interface it defines to provide device-independent I/O features.

A VipIODevice can be of 3 types:
- `VipIODevice::Temporal`: it defines a start and end time, a time window and might have a notion of position. A Temporal devices support both input and output operations.
- `VipIODevice::Sequential`: input data are generated sequentially. It does not have a notion of time window or position. Sequential devices only support input operations. They are usually suited for streaming operations.
- `VipIODevice::Resource`: the device holds one unique data. It does not have a notion of time and position at all. Resource devices are suited for non temporal data, like for instance an image file on the drive.

A read-only VipIODevice should defines one or more outputs. A write-only VipIODevice should defines one or more inputs.

To open a VipIODevice, you should usually follow these steps:
- Set all necessary parameters
- Set the path using VipIODevice::setPath() (for instance for file based devices) or set the QIODevice (`VipIODevice::setDevice()`)
- Call VipIODevice::open() with the right open mode (usually `VipIODevice::ReadOnly` or `VipIODevice::WriteOnly`).

If the VipIODevice is intended to work on files, you should call `VipIODevice::setPath()` or `VipIODevice::setDevice()` prior to open the VipIODevice.
If using `VipIODevice::setPath()`, the `VipIODevice::open()` override should call `VipIODevice::createDevice()` to build a suitable QIODevice based on
the set `VipMapFileSystemPtr` object (if any).

WriteOnly devices behave like VipProcessingObject: you should reimplement the `VipProcessingObject::apply()` function and call `VipProcessingObject::update()` to write the data (for non Asynchronous processing, otherwise the update() function is called automatically).

ReadOnly, Temporal or Resource devices don't care about the apply() function (that's why an empty implementation of apply() is provided) and must reimplement VipIODevice::readData() instead.
Sequential devices do not need to reimplement any of these functions. Instead, they must reimplement VipIODevice::enableStreaming() to start/stop sending output data.


### Processing pool


A processing pool (class `VipProcessingPool`) is VipIODevice without any inputs nor outputs, and is used to control the simultaneous playing of several VipIODevice objects.

VipProcessingPool should be the parent object of read only VipIODevice objects. VipProcessingPool itself is a VipIODevice which type (Temporal, Sequential or Resource) depends
on its children VipIODevice. The time window of a VipProcessingPool is the union of its children time windows.

Calling `VipProcessingPool::nextTime()`, `VipProcessingPool::previousTime()` or `VipProcessingPool::closestTime()` will return the required time taking into account
all read-only opened child devices.

Calling `VipProcessingPool::read()` will read the data at given time for all read-only opened child devices.

Disabled children VipIODevice objects are excluded from the time related functions of VipProcessingPool.


#### Archiving

If you wish to save/restore the state of a full processing pipeline, you should store it inside a VipProcessingPool object by setting all VipProcessingObject's parents to the pool.

When archiving the VipProcessingPool, all its children will be archived as well, including their connection status and all their properties.

VipProcessingPool ensures that all children VipProcessingObject have a unique name, which is mandatory for archiving. That's why in Thermavip all processing pipelines are manipulated through a VipProcessingPool object.



### Examples

Thermavip provides several example of processing pipelines:

-	[StreamingCurvePipeline](../src/Tests/Gui/StreamingCurvePipeline/main.cpp): streaming and display of 1d + time signals.
-	[StreamingMandelbrotPipeline](../src/Tests/Gui/StreamingMandelbrotPipeline/main.cpp): streaming, processing and display of 1d + time signals, histogram, images.

