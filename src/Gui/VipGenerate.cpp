/**
 * BSD 3-Clause License
 *
 * Copyright (c) 2025, Institute for Magnetic Fusion Research - CEA/IRFM/GP3 Victor Moncada, Leo Dubus, Erwan Grelier
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "VipCore.h"
#include "VipDisplayArea.h"
#include "VipIODevice.h"
#include "VipLogging.h"
#include "VipMimeData.h"
#include "VipProcessingObjectEditor.h"
#include "VipProgress.h"
#include "VipStandardWidgets.h"
#include "VipStreamingFromDevice.h"

#include <qapplication.h>

static QString openFile()
{
	if (!vipGetMainWindow()->displayArea()->currentDisplayPlayerArea())
		return QString();

	QStringList filters;
	filters += VipIODevice::possibleReadFilters(QString(), QByteArray());
	// create the All files filter
	QString all_filters;
	for (int i = 0; i < filters.size(); ++i) {
		int index1 = filters[i].indexOf('(');
		int index2 = filters[i].indexOf(')', index1);
		if (index1 >= 0 && index2 >= 0) {
			all_filters += filters[i].mid(index1 + 1, index2 - index1 - 1) + " ";
		}
	}

	if (all_filters.size())
		filters.prepend("All files (" + all_filters + ")");

	QString filename = VipFileDialog::getOpenFileName(vipGetMainWindow(), "Open a signal", filters.join(";;"));
	return filename;
}

static VipIODevice* generateDeviceFromFile()
{
	VipPath path(openFile(), false);
	if (path.isEmpty())
		return nullptr;

	QList<VipIODevice::Info> devices = VipIODevice::possibleReadDevices(path, QByteArray());
	VipIODevice* dev = VipCreateDevice::create(devices, path);
	if (dev) {
		dev->setPath(path.canonicalPath());
		dev->setMapFileSystem(path.mapFileSystem());

		QString name = dev->removePrefix(dev->name());

		VipProgress progress;
		progress.setModal(true);
		progress.setCancelable(false);
		progress.setText("<b>Open</b> " + QFileInfo(name).fileName());
		vipProcessEvents();

		if (dev->open(VipIODevice::ReadOnly)) {
			VIP_LOG_INFO("Create sequential device for path: " + QFileInfo(name).fileName());
			VipGeneratorSequential* gen = new VipGeneratorSequential();
			gen->setIODevice(dev);
			gen->setPath(name);
			gen->setAttributes(dev->attributes());
			gen->open(VipIODevice::ReadOnly);
			return gen;
		}
		else {
			delete dev;
			VIP_LOG_WARNING("Fail to open : " + QFileInfo(name).fileName());
			return nullptr;
		}
	}
	return nullptr;
}

static void generateStreamingFromFile()
{
	if (VipIODevice* dev = generateDeviceFromFile()) {
		vipGetMainWindow()->openDevices(QList<VipIODevice*>() << dev, nullptr, nullptr);
	}
}

int registerGenerate()
{
	QAction* streaming = vipGetMainWindow()->generateMenu()->addAction("Generate streaming device from file...");
	streaming->setToolTip("Simulate a streaming video or plot from a video file or a curve file");
	QObject::connect(streaming, &QAction::triggered, qApp, generateStreamingFromFile);
	// make the menu action droppable
	streaming->setProperty("QMimeData", QVariant::fromValue((QMimeData*)new VipMimeDataLazyEvaluation<VipIODevice*>(generateDeviceFromFile, VipCoordinateSystem::Cartesian, streaming)));

	return 0;
}

static bool _registerGenerate = vipAddGuiInitializationFunction(registerGenerate);
