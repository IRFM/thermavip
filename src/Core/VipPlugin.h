#ifndef VIP_PLUGIN_H
#define VIP_PLUGIN_H

#include "VipConfig.h"
#include <QObject>

class VipArchive;

/// \addtogroup Core
/// @{

/// \a VipPluginInterface is the base interface class for all Thermavip plugins.
/// Thermavip plugins are standard Qt plugins (see <a href="http://doc.qt.io/qt-5/plugins-howto.html">this</a> for more details)
///
/// Thermavip plugins are standard shared libraires extending the behavior of the program based on Thermavip SDK.
/// Plugins are located in the the directory returned by #vipGetPluginsDirectory (usually 'VipPlugins') located next to the program executable.
class VipPluginInterface
{
	bool m_loaded;

public:
	/// Result of the #VipPluginInterface::load function
	enum LoadResult
	{
		Success = 1,  //! Loading succeeded
		Failure,      //! Loading failed
		ExitProcess,  //! Loading succeeded and Thermavip should exit
		Unauthorized, //! The computer does not have the authorization to run this plugin
		AlreadyLoaded // plugin already loaded
	};
	VipPluginInterface()
	  : m_loaded(false)
	{
	}

	LoadResult loadPlugin()
	{
		if (!m_loaded) {
			LoadResult ret = load();
			m_loaded = ret == Success;
			return ret;
		}
		return AlreadyLoaded;
	}

	void unloadPlugin()
	{
		if (m_loaded) {
			this->unload();
			m_loaded = false;
		}
	}

	virtual ~VipPluginInterface() {}

	/// Load the plugin.
	/// This function is the core of each plugin. Any registration procedure should be done inside this function (like, fore instance, adding a function
	/// to one of the numerous #FunctionDispatcher available).
	/// It must return a #VipPluginInterface::LoadResult value.
	virtual LoadResult load() = 0;
	/// Any cleanup operation should be performed in this function.
	/// Thermavip always call VipPluginInterface::unload for all loaded plugins before exiting.
	virtual void unload() {}
	/// Returns the plugin version
	virtual QByteArray pluginVersion() { return QByteArray(); }
	/// Returns the plugin author
	virtual QString author() = 0;
	/// Returns the plugin description
	virtual QString description() = 0;
	/// Returns a HTTP link related to this plugin
	virtual QString link() = 0;
	/// Save the plugin state into a VipArchive
	virtual void save(VipArchive&) {}
	/// Restore the plugin state from a VipArchive
	virtual void restore(VipArchive&) {}
	/// Returns true if the plugin defines additional commands for command line usage.
	virtual bool hasExtraCommands() { return false; }
	/// Returns a potential additional style sheet that should be added to the global application style.
	virtual QString additionalStyleSheet() { return QString(); }

	virtual const char* vipVersion() { return VIP_VERSION; }
};

#define PluginInterface_iid "thermadiag.thermavip.VipPluginInterface"
Q_DECLARE_INTERFACE(VipPluginInterface, PluginInterface_iid)

/// \a VipLoadPlugins manages a set of plugins.
/// It is used in the main function of Thermavip to load plugins located in the #vipGetPluginsDirectory directory.
/// Thermavip plugins are shared libraries that define the VipPluginInterface interface. The plugins mechanism is based on the Qt plugin's one.
///
/// By default, all plugins located in the VipPlugins folder are loaded. However, you can define a Plugins.ini file in the VipPlugins folder to specify
/// which plugins you need to load. A Plugins.ini fil looks like this:
/// \code
/// [Default]
///
/// plugin/1/name = Tokidad
/// plugin/2/name = ImageProcessingd
/// plugin/3/name = InfraTechFilesd
/// ...
/// \endcode
///
/// "Default" is the default plugin section that will be loaded. You can define additional sections in the Plugins.ini file
/// and use them through the command line argument of Thermavip. Example:
///
/// \code
/// Thermavip --plugin-section my_section
/// \endcode
class VIP_CORE_EXPORT VipLoadPlugins : public QObject
{
	Q_OBJECT
	VipLoadPlugins();

public:
	virtual ~VipLoadPlugins();
	static VipLoadPlugins& instance();

	/// Returns all available plugin categories in the Plugins.ini file
	QStringList pluginCategories() const;
	/// Returns all plugins for a given category in the Plugins.ini file.
	/// If category is "Folder", this function returns all plugin files in the VipPlugins directory.
	QStringList plugins(const QString& category = "Default") const;

	/// Returns all loaded plugins
	QList<VipPluginInterface*> loadedPlugins() const;
	/// Returns all loaded plugins names
	QStringList loadedPluginNames() const;

	VipPluginInterface* find(const QString& name) const;

	/// Load a plugin based on its name and return the #VipPluginInterface::LoadResult.
	/// The plugin is searchd in the VipPlugins directory.
	VipPluginInterface::LoadResult loadPlugin(const QString& name, QString* error = nullptr);

public Q_SLOTS:

	/// Unload all previously loaded plugins (calling #VipPluginInterface::unload)
	void unloadPlugins();
	/// Unload all previously loaded plugins (calling #VipPluginInterface::unload) and delete them (unload the sgared library)
	void unloadAndDeletePlugins();

private:
	/// Returns all plugins in given directory
	static QStringList pluginsInDir(const QString directory);

	class PrivateData;
	PrivateData* m_data;
};

/// @}
// end Core

#endif
