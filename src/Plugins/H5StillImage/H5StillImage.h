#ifndef H5_STILL_IMAGE_H
#define H5_STILL_IMAGE_H



#include "VipPlugin.h"


class H5StillImage : public QObject, public VipPluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "thermadiag.thermavip.VipPluginInterface")
	Q_INTERFACES(VipPluginInterface)

public:

	//Standard plugin interface
	virtual LoadResult load();
	virtual QByteArray pluginVersion() { return "1.1.0"; }
	virtual void unload() {}
	virtual QString author() {return "Victor Moncada(victor.moncada@cea.fr)";}
	virtual QString description() {return "H5 video file management based on W7-X data model";}
	virtual QString link() {return QString();}
};

#endif
