#include "VipDisplayArea.h"
#include "VipProgress.h"
#include "VipStandardWidgets.h"
#include "VipProcessingObjectEditor.h"
#include "VipIODevice.h"
#include "VipLogging.h"
#include "VipCore.h"
#include "VipStreamingFromDevice.h"
#include "VipMimeData.h"

#include <qapplication.h>

static QString openFile()
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return QString();

	QStringList filters;
	filters += VipIODevice::possibleReadFilters(QString(), QByteArray());
	//create the All files filter
	QString all_filters;
	for (int i = 0; i < filters.size(); ++i)
	{
		int index1 = filters[i].indexOf('(');
		int index2 = filters[i].indexOf(')', index1);
		if (index1 >= 0 && index2 >= 0)
		{
			all_filters += filters[i].mid(index1 + 1, index2 - index1 - 1) + " ";
		}
	}

	if (all_filters.size())
		filters.prepend("All files (" + all_filters + ")");
	

	QString filename = VipFileDialog::getOpenFileName(vipGetMainWindow(), "Open a signal", filters.join(";;"));
	return filename;
}

static VipIODevice * generateDeviceFromFile()
{
	VipPath path(openFile(), false);
	if (path.isEmpty())
		return NULL;

	QList<VipIODevice::Info> devices = VipIODevice::possibleReadDevices(path, QByteArray());
	VipIODevice * dev = VipCreateDevice::create(devices, path);
	if (dev)
	{
		dev->setPath(path.canonicalPath());
		dev->setMapFileSystem(path.mapFileSystem());

		QString name = dev->removePrefix(dev->name());

		VipProgress progress;
		progress.setModal(true);
		progress.setCancelable(false);
		progress.setText("<b>Open</b> " + QFileInfo(name).fileName());
		vipProcessEvents();

		if (dev->open(VipIODevice::ReadOnly))
		{
			VIP_LOG_INFO("Create sequential device for path: " + QFileInfo(name).fileName());
			VipGeneratorSequential * gen = new VipGeneratorSequential();
			gen->setIODevice(dev);
			gen->setPath(name);
			gen->setAttributes(dev->attributes());
			gen->open(VipIODevice::ReadOnly);
			return gen;
		}
		else
		{
			delete dev;
			VIP_LOG_WARNING("Fail to open : " + QFileInfo(name).fileName());
			return NULL;
		}
	}
	return NULL;
}

static void generateStreamingFromFile()
{
	if (VipIODevice * dev = generateDeviceFromFile())
	{
		vipGetMainWindow()->openDevices(QList<VipIODevice*>() << dev, NULL, NULL);
	}
}


int registerGenerate()
{
	QAction * streaming = vipGetMainWindow()->generateMenu()->addAction("Generate streaming device from file...");
	streaming->setToolTip("Simulate a streaming video or plot from a video file or a curve file");
	QObject::connect(streaming, &QAction::triggered, qApp, generateStreamingFromFile);
	//make the menu action droppable
	streaming->setProperty("QMimeData", QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<VipIODevice*>(
		generateDeviceFromFile,
		VipCoordinateSystem::Cartesian,
		streaming
		)));

	return 0;
}

static bool _registerGenerate = vipAddGuiInitializationFunction(registerGenerate);
