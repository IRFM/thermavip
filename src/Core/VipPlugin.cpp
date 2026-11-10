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
#include "VipArchive.h"

#include <QDir>
#include <QFileInfo>
#include <QLibrary>
#include <QMap>
#include <QPluginLoader>
#include <qset.h>
#include <qsettings.h>
#include <qstringlist.h>

#include <vector>

#ifdef VIP_WITH_PYTHON
#include "VipPyOperation.h"

class PythonPluginInterface : public VipPluginInterface
{
	VipPyObject interface;

public:
	PythonPluginInterface(const VipPyObject& obj)
	{
		VipGILLocker lock;
		interface = obj;
	}

	virtual LoadResult load()
	{
		VipGILLocker lock;

		VipPyError::clear();

		auto ret = interface.callMember("load").toVariant().toInt();
		if (ret <= 0 || ret > ExitProcess) {
			if (VipPyError err = VipPyError(compute_error_t{})) {
				std::ostringstream oss;
				err.printDebug(oss);
				VIP_LOG_ERROR("Failed to load plugin: ", QString(oss.str().c_str()));
			}
			return Failure;
		}
		return (LoadResult)ret;
	}
	/// Any cleanup operation should be performed in this function.
	/// Thermavip always call VipPluginInterface::unload for all loaded plugins before exiting.
	virtual void unload()
	{
		VipGILLocker lock;
		VipPyError::clear();
		interface.callMember("unload");
	}
	/// Returns the plugin version
	virtual QByteArray pluginVersion()
	{
		VipGILLocker lock;
		return interface.callMember("version").toVariant().toByteArray();
	}
	/// Returns the plugin author
	virtual QString author()
	{
		VipGILLocker lock;
		return interface.callMember("author").toVariant().toByteArray();
	}
	/// Returns the plugin description
	virtual QString description()
	{
		VipGILLocker lock;
		return interface.callMember("description").toVariant().toByteArray();
	}
	/// Returns a HTTP link related to this plugin
	virtual QString link()
	{
		VipGILLocker lock;
		return interface.callMember("link").toVariant().toByteArray();
	}
	/// Save the plugin state into a VipArchive
	virtual void save(VipArchive& arch)
	{
		VipGILLocker lock;
		auto ret = interface.callMember("save").toVariant().toByteArray();
		if (VipPyError err = VipPyError(compute_error_t{})) {
			std::ostringstream oss;
			err.printDebug(oss);
			VIP_LOG_ERROR("Failed to serialize plugin: ", QString(oss.str().c_str()));
			return;
		}
		arch.content("state", ret.toBase64());
	}
	/// Restore the plugin state from a VipArchive
	virtual void restore(VipArchive& arch)
	{
		VipGILLocker lock;
		auto bytes = QByteArray::fromBase64(arch.read("state").toByteArray());
		if (bytes.size())
			interface.callMember("restore", QVariantList() << bytes);
	}
	/// Returns true if the plugin defines additional commands for command line usage.
	virtual bool hasExtraCommands() { return false; }
	/// Returns a potential additional style sheet that should be added to the global application style.
	virtual QString additionalStyleSheet()
	{
		VipGILLocker lock;
		return interface.callMember("stylesheet").toVariant().toString();
	}
};

#endif

struct PluginLib
{
	QPluginLoader loader;
	QString name;

	PluginLib() = default;
	PluginLib(const QString& libname, const QString& plugin_name)
	  : loader(libname)
	  , name(plugin_name)
	{
	}
};

class VipLoadPlugins::PrivateData
{
public:
	QList<VipPluginInterface*> interfaces;
	std::vector<std::unique_ptr<PluginLib>> plugins;
	QMap<QString, QStringList> availablePlugins;
	bool unloaded{ false };
};

VipLoadPlugins::VipLoadPlugins()
{
	VIP_CREATE_PRIVATE_DATA();

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
		if (d_data->plugins[i]->loader.isLoaded())
			d_data->plugins[i]->loader.unload();
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
	for (const QFileInfo& info : QDir(directory).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
		if (info.isDir()) {
			// For folders, we need a dll with the same name as its parent directory, or a __init__.py file if Python is supported
			QString dirname = info.canonicalFilePath();
			QString libname = dirname + QDir::separator() + info.fileName();

#ifdef WIN32
			if (QFileInfo(libname + ".dll").exists()) {
				res << info.fileName();
				continue;
			}
#elif defined(__APPLE__)
			if (QFileInfo(libname + ".so").exists()) {
				res << info.fileName();
				continue;
			}
			if (QFileInfo(libname + ".dylib").exists()) {
				res << info.fileName();
				continue;
			}
			if (QFileInfo(libname + ".bundle").exists()) {
				res << info.fileName();
				continue;
			}
#else
			if (QFileInfo(libname + ".so").exists()) {
				res << info.fileName();
				continue;
			}
#endif

#ifdef VIP_WITH_PYTHON
			QString initname = dirname + QDir::separator() + "__init__.py";
			if (QFileInfo(initname).exists()) {
				res << info.fileName();
				continue;
			}
#endif
		}
		else if (QLibrary::isLibrary(info.fileName()))
			res << info.baseName();
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
		res << d_data->plugins[i]->name;
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
		QString pname = d_data->plugins[i]->name;
		if (pname == search)
			return d_data->interfaces[i];
	}
	return nullptr;
}

void VipLoadPlugins::unloadPlugins()
{
	// Idempotent. Closing the main window unloads, and so does the end of main:
	// every plugin used to receive unloadPlugin() twice on a normal exit, and the
	// interface documents no such contract.
	if (d_data->unloaded)
		return;
	d_data->unloaded = true;

	for (qsizetype i = 0; i < d_data->interfaces.size(); ++i) {
		d_data->interfaces[i]->unloadPlugin();
	}
}

void VipLoadPlugins::unloadAndDeletePlugins()
{
	if (!d_data->unloaded) {
		d_data->unloaded = true;
		for (qsizetype i = 0; i < d_data->interfaces.size(); ++i) {
			d_data->interfaces[i]->unloadPlugin();
		}
	}

	for (qsizetype i = 0; i < d_data->interfaces.size(); ++i) {
		// delete d_data->interfaces[i];
		if (d_data->plugins[i]->loader.isLoaded())
			d_data->plugins[i]->loader.unload();
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

	QString pluginPath = vipGetPluginsDirectory() + name;

	std::unique_ptr<PluginLib> loader;

	if (QFileInfo(pluginPath).isDir()) {

#ifdef VIP_WITH_PYTHON
		QString pyfile = pluginPath + "/__init__.py";
		if (QFileInfo(pyfile).exists()) {

			vipPythonInitialize();

			// Import Python module
			VipGILLocker lock;
			vipExecPythonCode("import sys; sys.path.append('" + vipGetPluginsDirectory() + "')");
			QString module = QFileInfo(pluginPath).fileName();
			vipExecPythonCode("import " + module);
			if (VipPyError err = VipPyError(compute_error_t{})) {
				std::ostringstream oss;
				err.printDebug(oss);
				VIP_LOG_ERROR("Error while loading plugin ", module, ": ", oss.str().c_str());
				return VipPluginInterface::Failure;
			}
			QString interface_name = module + "." + module + "_interface";
			VipPyObject interface = VipPyObject::eval(interface_name);
			if (!interface) {
				VipPyError err(compute_error_t{});
				VIP_LOG_ERROR("Error while loading plugin ", module, ": ", err.toString());
				return VipPluginInterface::Failure;
			}

			std::unique_ptr<VipPluginInterface> inter(new PythonPluginInterface(interface));

			auto result = inter->loadPlugin();
			if (result != VipPluginInterface::Failure && result != VipPluginInterface::Unauthorized) {
				loader.reset(new PluginLib());
				loader->name = module;
				d_data->plugins.push_back(std::move(loader));
				d_data->interfaces.append(inter.release());
			}
			return result;
		}
#endif

		QString dllname = pluginPath + "/" + name;
		filename = dllname;
		loader.reset(new PluginLib(dllname, formatPluginName(name)));
	}

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

	if (!loader)
		loader.reset(new PluginLib(pluginPath, formatPluginName(name)));
	loader->loader.load();
	if (loader->loader.isLoaded()) {
		QObject* plugin = loader->loader.instance();
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
		*error = loader->loader.errorString();

	return VipPluginInterface::Failure;
}
