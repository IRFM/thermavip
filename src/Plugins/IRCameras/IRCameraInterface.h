#ifndef GENERATORS_H
#define GENERATORS_H

#include "VipPlugin.h"

class IRCameraInterface : public QObject, public VipPluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "thermadiag.thermavip.VipPluginInterface")
	Q_INTERFACES(VipPluginInterface)
public:

	virtual LoadResult load();
	virtual void unload() {}
	virtual QByteArray pluginVersion() { return "1.0.0"; }
	virtual QString author() {return QString("Victor Moncada (victor.moncada@cea.fr)");}
	virtual QString description() {return QString("Defines devices to read HCC, IRB and PTW infrared video files");}
	virtual QString link() {return QString();}
};

#endif
