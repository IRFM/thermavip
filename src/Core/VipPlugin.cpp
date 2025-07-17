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

#include "VipPlugin.h"
#include "VipCommandOptions.h"
#include "VipCore.h"
#include "VipEnvironment.h"
#include "VipLogging.h"
#include "VipSet.h"

#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QMap>
#include <QPluginLoader>
#include <qset.h>
#include <qsettings.h>
#include <qstringlist.h>

#include <vector>

class VipLoadPlugins::PrivateData
{
public:
	QList<VipPluginInterface*> interfaces;
	std::vector<std::unique_ptr<QPluginLoader>> plugins;
	QMap<QString, QStringList> availablePlugins;
};

VipLoadPlugins::VipLoadPlugins()
{
	VIP_CREATE_PRIVATE_DATA(d_data);

	QSettings settings(vipGetPluginsDirectory() + "Plugins.ini", QSettings::IniFormat);
	QStringList groups = settings.childGroups();
	for (int g = 0; g < groups.size(); ++g) {
		settings.beginGroup(groups[g]);
		settings.beginReadArray("plugin");
		int i = 0;
		while (true) {
			settings.setArrayIndex(i);
			QString name = settings.value("name").toString();
			if (!name.isEmpty())
				d_data->availablePlugins[groups[g]].append(name);
			else
				break;
			++i;
		}

		settings.endArray();
		settings.endGroup();
	}
}

VipLoadPlugins::~VipLoadPlugins()
{
	for (size_t i = 0; i < d_data->plugins.size(); ++i) {
		d_data->plugins[i]->unload();
		d_data->plugins[i].reset();
	}
}

VipLoadPlugins& VipLoadPlugins::instance()
{
	static VipLoadPlugins instance;
	return instance;
}

QStringList VipLoadPlugins::pluginCategories() const
{
	return d_data->availablePlugins.keys();
}

QStringList VipLoadPlugins::plugins(const QString& category) const
{
	if (category == "Folder")
		return pluginsInDir(vipGetPluginsDirectory());
	else
		return d_data->availablePlugins[category];
}

QStringList VipLoadPlugins::pluginsInDir(const QString directory)
{
	QStringList res;
	foreach (QString fileName, QDir(directory).entryList(QDir::Files)) {
		if (QLibrary::isLibrary(fileName))
			res << QFileInfo(fileName).baseName();
		// res << QDir::cleanPath(QDir(directory).absoluteFilePath(fileName));
	}

	res = vipToSet(res).values();
	std::sort(res.begin(), res.end());
	return res;
}

QList<VipPluginInterface*> VipLoadPlugins::loadedPlugins() const
{
	return d_data->interfaces;
}

QStringList VipLoadPlugins::loadedPluginNames() const
{
	QStringList res;
	for (size_t i = 0; i < d_data->plugins.size(); ++i)
		res << QFileInfo(d_data->plugins[i]->fileName()).baseName();
	return res;
}

static QString formatPluginName(const QString& fname)
{
	QString pname = QFileInfo(fname).baseName();
	if (pname.endsWith("d"))
		pname = pname.mid(0, pname.size() - 1);
	if (pname.startsWith("lib"))
		pname = pname.mid(3);
	return pname;
}

VipPluginInterface* VipLoadPlugins::find(const QString& name) const
{
	QString search = formatPluginName(name);

	for (size_t i = 0; i < d_data->plugins.size(); ++i) {
		QString pname = formatPluginName(d_data->plugins[i]->fileName());
		if (pname == search)
			return d_data->interfaces[i];
	}
	return nullptr;
}

void VipLoadPlugins::unloadPlugins()
{
	for (qsizetype i = 0; i < d_data->interfaces.size(); ++i) {
		d_data->interfaces[i]->unloadPlugin();
	}
}

void VipLoadPlugins::unloadAndDeletePlugins()
{
	for (qsizetype i = 0; i < d_data->interfaces.size(); ++i) {
		d_data->interfaces[i]->unloadPlugin();
	}

	for (qsizetype i = 0; i < d_data->interfaces.size(); ++i) {
		// delete d_data->interfaces[i];
		d_data->plugins[i]->unload();
		d_data->plugins[i].reset();
	}

	d_data->interfaces.clear();
	d_data->plugins.clear();
	d_data->availablePlugins.clear();
}

VipPluginInterface::LoadResult VipLoadPlugins::loadPlugin(const QString& name, QString* error)
{
	// get the full plugin filename
	QString filename = (vipGetPluginsDirectory() + name);
	if (QFileInfo(filename + ".dll").exists())
		filename = filename + ".dll";
	else if (QFileInfo(filename + ".so").exists())
		filename = filename + ".so";
	else if (QFileInfo(filename + ".dylib").exists())
		filename = filename + ".dylib";
	else if (QFileInfo(filename + ".a").exists())
		filename = filename + ".a";
	else if (QFileInfo(filename + ".sl").exists())
		filename = filename + ".sl";
	else if (QFileInfo(filename + ".bundle").exists())
		filename = filename + ".bundle";
	else
		return VipPluginInterface::Unauthorized;

	std::unique_ptr<QPluginLoader> loader ( new QPluginLoader(vipGetPluginsDirectory() + name));
	loader->load();
	if (loader->isLoaded()) {
		QObject* plugin = loader->instance();
		VipPluginInterface* _interface = qobject_cast<VipPluginInterface*>(plugin);
		if (_interface) {
			// check the vip version
			const char* ver = _interface->vipVersion();
			const QByteArray ar = ver;
			QList<QByteArray> plugin_versions = ar.split('.');
			QList<QByteArray> versions = QByteArray(VIP_VERSION).split('.');
			if (versions[0] != plugin_versions[0] || versions[1] != plugin_versions[1]) {
				if (error)
					*error = "version mismatch";
				return VipPluginInterface::Failure;
			}

			// if the user requested the command line help, only load the plugin if it defines extra commands
			if (VipCommandOptions::instance().count("help")) {
				if (!_interface->hasExtraCommands()) {
					d_data->plugins.push_back(std::move(loader));
					d_data->interfaces.append(_interface);
					return VipPluginInterface::Success;
				}
			}

			VipPluginInterface::LoadResult result = _interface->loadPlugin();
			if (result != VipPluginInterface::Failure && result != VipPluginInterface::Unauthorized) {
				d_data->plugins.push_back(std::move(loader));
				d_data->interfaces.append(_interface);
			}
			return result;
		}
	}

	if (error)
		*error = loader->errorString();

	return VipPluginInterface::Failure;
}
