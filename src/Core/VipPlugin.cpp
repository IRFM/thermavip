#include "VipPlugin.h"
#include "VipEnvironment.h"
#include "VipLogging.h"
#include "VipCore.h"
#include "VipCommandOptions.h"
#include "VipSet.h"

#include <qsettings.h>
#include <QMap>
#include <qset.h>
#include <qstringlist.h>
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <QLibrary>


class VipLoadPlugins::PrivateData
{
public:
	QList<VipPluginInterface*> interfaces;
	QList<QPluginLoader *> plugins;
	QMap<QString, QStringList> availablePlugins;
};



VipLoadPlugins::VipLoadPlugins()
{
	m_data = new PrivateData();

	QSettings settings(vipGetPluginsDirectory() + "Plugins.ini", QSettings::IniFormat);
	QStringList groups = settings.childGroups();
	for (int g = 0; g < groups.size(); ++g)
	{
		settings.beginGroup(groups[g]);
		settings.beginReadArray("plugin");
		int i = 0;
		while (true)
		{
			settings.setArrayIndex(i);
			QString name = settings.value("name").toString();
			if (!name.isEmpty())
				m_data->availablePlugins[groups[g]].append(name);
			else break;
			++i;
		}

		settings.endArray();
		settings.endGroup();
	}
}

VipLoadPlugins::~VipLoadPlugins()
{
	for (int i = 0; i < m_data->plugins.size(); ++i)
	{
		m_data->plugins[i]->unload();
		delete  m_data->plugins[i];
	}
	delete m_data;
}

VipLoadPlugins & VipLoadPlugins::instance()
{
	static VipLoadPlugins instance;
	return instance;
}

QStringList VipLoadPlugins::pluginCategories() const
{
	return m_data->availablePlugins.keys();
}

QStringList VipLoadPlugins::plugins(const QString & category) const
{
	if (category == "Folder")
		return pluginsInDir(vipGetPluginsDirectory());
	else
		return m_data->availablePlugins[category];
}

QStringList VipLoadPlugins::pluginsInDir(const QString directory)
{
	QStringList res;
	foreach(QString fileName, QDir(directory).entryList(QDir::Files)) {
		if (QLibrary::isLibrary(fileName))
			res << QFileInfo(fileName).baseName();
		//res << QDir::cleanPath(QDir(directory).absoluteFilePath(fileName));
	}

	res = vipToSet(res).values();
	std::sort(res.begin(),res.end());
	return res;
}

QList<VipPluginInterface*> VipLoadPlugins::loadedPlugins() const
{
	return m_data->interfaces;
}

QStringList VipLoadPlugins::loadedPluginNames() const
{
	QStringList res;
	for (int i = 0; i < m_data->plugins.size(); ++i)
		res << QFileInfo(m_data->plugins[i]->fileName()).baseName();
	return res;
}

static QString formatPluginName(const QString & fname)
{
	QString pname = QFileInfo(fname).baseName();
	if (pname.endsWith("d"))
		pname = pname.mid(0, pname.size() - 1);
	if (pname.startsWith("lib"))
		pname = pname.mid(3);
	return pname;
}

VipPluginInterface * VipLoadPlugins::find(const QString & name) const
{
	QString search = formatPluginName(name);

	for (int i = 0; i < m_data->plugins.size(); ++i) {
		QString pname = formatPluginName(m_data->plugins[i]->fileName());
		if (pname == search)
			return m_data->interfaces[i];
	}
	return NULL;
}

void VipLoadPlugins::unloadPlugins()
{
	for (int i = 0; i < m_data->interfaces.size(); ++i)
	{
		m_data->interfaces[i]->unloadPlugin();
	}
}

void VipLoadPlugins::unloadAndDeletePlugins()
{
	for (int i = 0; i < m_data->interfaces.size(); ++i)
	{
		m_data->interfaces[i]->unloadPlugin();
	}

	for (int i = 0; i < m_data->interfaces.size(); ++i)
	{
		//delete m_data->interfaces[i];
		m_data->plugins[i]->unload();
		delete m_data->plugins[i];
	}

	m_data->interfaces.clear();
	m_data->plugins.clear();
	m_data->availablePlugins.clear();
}




VipPluginInterface::LoadResult  VipLoadPlugins::loadPlugin(const QString & name, QString * error)
{
	//get the full plugin filename
	QString filename = (vipGetPluginsDirectory() + name);
	if (QFileInfo(filename + ".dll").exists()) filename = filename + ".dll";
	else if (QFileInfo(filename + ".so").exists()) filename = filename + ".so";
	else if (QFileInfo(filename + ".dylib").exists()) filename = filename + ".dylib";
	else if (QFileInfo(filename + ".a").exists()) filename = filename + ".a";
	else if (QFileInfo(filename + ".sl").exists()) filename = filename + ".sl";
	else if (QFileInfo(filename + ".bundle").exists()) filename = filename + ".bundle";
	else return VipPluginInterface::Unauthorized;

	QPluginLoader* loader = new QPluginLoader(vipGetPluginsDirectory() + name);
	loader->load();
	if (loader->isLoaded())
	{
		QObject *plugin = loader->instance();
		VipPluginInterface * _interface = qobject_cast<VipPluginInterface*>(plugin);
		if (_interface)
		{
			//check the vip version
			const char * ver = _interface->vipVersion();
			const QByteArray ar = ver;
			QList<QByteArray> plugin_versions = ar.split('.');
			QList<QByteArray> versions = QByteArray(VIP_VERSION).split('.');
			if (versions[0] != plugin_versions[0] || versions[1] != plugin_versions[1])
			{
				if (error) * error = "version mismatch";
				return  VipPluginInterface::Failure;
			}

			//if the user requested the command line help, only load the plugin if it defines extra commands
			if (VipCommandOptions::instance().count("help")) {
				if (!_interface->hasExtraCommands()) {
					m_data->plugins.append(loader);
					m_data->interfaces.append(_interface);
					return VipPluginInterface::Success;
				}
			}

			VipPluginInterface::LoadResult result = _interface->loadPlugin();
			if (result != VipPluginInterface::Failure && result != VipPluginInterface::Unauthorized)
			{
				m_data->plugins.append(loader);
				m_data->interfaces.append(_interface);
			}
			return result;
		}
	}

	if (error)
		*error = loader->errorString();

	delete loader;
	return VipPluginInterface::Failure;
}




