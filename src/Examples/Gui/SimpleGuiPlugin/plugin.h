#pragma once

#include <qlabel.h>

#include "VipPlugin.h"
#include "VipToolWidget.h"
#include "VipPlayer.h"
#include "VipIODevice.h"

/// @brief A dummy VipToolWidget that displays a few information on the current player
class MyToolWidget : public VipToolWidgetPlayer
{
	Q_OBJECT

	QLabel* label;

public:
	MyToolWidget(VipMainWindow* win)
		: VipToolWidgetPlayer(win)
	{
		setWidget(label = new QLabel());
		setWindowTitle("Useless information");
	}

	virtual bool setPlayer(VipAbstractPlayer*);
};

/// @brief A customization object that add a tool button to VipPlotPlayer objects.
/// The button display a message box saying 'Hi' when clicked.
class MyUpdatePlotPlayerInterface : public QObject
{
	Q_OBJECT

public:
	MyUpdatePlotPlayerInterface(VipPlotPlayer* pl);

private Q_SLOTS:
	void sayHiPlot();
};


/// @brief A IO device that reads .rawsig files (raw VipPointVector)
class RAWSignalReader : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipOutput output)
	Q_CLASSINFO("description", "Read a raw signal file (.rawsig suffix)")
	Q_CLASSINFO("category", "reader")
public:
	RAWSignalReader(QObject* parent = nullptr);

	virtual DeviceType deviceType() const { return Resource; }
	virtual VipIODevice::OpenModes supportedModes() const { return ReadOnly; }
	virtual QString fileFilters() const { return "RAW signal file (*.rawsig)"; }
	virtual bool probe(const QString& filename, const QByteArray&) const { return supportFilename(filename) || VipIODevice::probe(filename); }

	virtual bool open(VipIODevice::OpenModes mode);

protected:
	virtual bool readData(qint64);
};
/// Register RAWSignalReader
VIP_REGISTER_QOBJECT_METATYPE(RAWSignalReader*)


/// @brief A IO device that writes .rawsig files (raw VipPointVector)
class RAWSignalWriter : public VipIODevice
{
	Q_OBJECT
	VIP_IO(VipInput input)
	Q_CLASSINFO("description", "Write a raw signal in a .rawsig file")
	Q_CLASSINFO("category", "writer")

public:
	
	RAWSignalWriter(QObject* parent = NULL);

	virtual bool acceptInput(int, const QVariant& v) const{
		return v.userType() == qMetaTypeId<VipPointVector>();
	}
	virtual bool probe(const QString& filename, const QByteArray&) const { 
		return supportFilename(filename) || VipIODevice::probe(filename); 
	}
	virtual DeviceType deviceType() const { 
		return Resource; 
	}
	virtual VipIODevice::OpenModes supportedModes() const { 
		return WriteOnly; 
	}
	virtual QString fileFilters() const { 
		return "RAW signal file (*.rawsig)"; 
	}

	virtual bool open(VipIODevice::OpenModes mode);

protected:
	virtual void apply();
};
/// Register RAWSignalWriter
VIP_REGISTER_QOBJECT_METATYPE(RAWSignalWriter*)






/// @brief Plugin interface
class SimpleGuiInterface : public QObject, public VipPluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "SimpleGuiInterface")
	Q_INTERFACES(VipPluginInterface)
public:

	virtual LoadResult load();
	virtual QByteArray pluginVersion() { return "1.0.0"; }
	virtual void unload() {}
	virtual QString author() { return "Victor Moncada(victor.moncada@cea.fr)"; }
	virtual QString description() { return "Hi! plugin"; }
	virtual QString link() { return QString(); }
	virtual bool hasExtraCommands() { return true; }
	virtual void save(VipArchive &) {}
	virtual void restore(VipArchive &) {}

};

