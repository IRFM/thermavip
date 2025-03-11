#pragma once

#include "VipPlugin.h"


/// @brief Plugin interface
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

