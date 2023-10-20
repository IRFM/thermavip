#define VIP_ENABLE_LOG_DEBUG

#include <fstream>
#include <iostream>

#include <QApplication>
#include <QSplashScreen>
#include <QFileInfo>
#include <QFontDatabase> 
#include <QMessageBox>
#include <QBoxLayout>
#include <QBitmap>
#include <QDir>
#include <QDateTime>
#include <QFile> 
#include <QLibrary>
#include <qprocess.h> 
#include <QDesktopWidget>
#include <QOpenGLWidget>
#include <QTemporaryDir>
#include <QOpenGLWidget>
#include <qopenglfunctions.h>
#include <qscreen.h>

#include "VipCore.h"
#include "VipGui.h"  
#include "VipPlugin.h"  
#include "VipLogging.h"
#include "VipCommandOptions.h" 
#include "VipEnvironment.h" 
#include "VipDisplayArea.h" 
#include "VipPlayWidget.h"  
#include "VipUpdate.h"  
#include "VipIODevice.h"
#include "VipFileSystem.h" 
#include "VipLogging.h"
#include "VipVisualizeDB.h"

#include <qsurfaceformat.h>

#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
#include <unistd.h>
#endif


#ifdef _MSC_VER
#define VIP_ALLOW_AUTO_UPDATE
#endif

static void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
	
	(void)context;
	switch (type) {
	case QtDebugMsg:
		vip_debug("Debug: %s\n", msg.toLatin1().data());
		break;
	case QtWarningMsg:
		vip_debug( "Warning: %s\n", msg.toLatin1().data());
		break;
	case QtCriticalMsg:
		vip_debug( "Critical: %s\n", msg.toLatin1().data());
		break;
	case QtFatalMsg:
		vip_debug( "Fatal: %s\n", msg.toLatin1().data());
		abort();
	}
	return;
} 



int main(int argc, char** argv)
{
	qInstallMessageHandler(myMessageOutput);

	//_putenv("QT_SCALE_FACTOR=1.5");
	//register command option for gui
	VipCommandOptions::instance().add("last_session", "Load last session", VipCommandOptions::NoValue);
	VipCommandOptions::instance().add("no_splashscreen", "Does not display splashscreen", VipCommandOptions::NoValue);
	VipCommandOptions::instance().add("skin", "Display skin to be used", VipCommandOptions::ValueRequired);
	VipCommandOptions::instance().add("plugin-section", "Load a specific plugin section from the Plugins.ini file", VipCommandOptions::ValueRequired);
	VipCommandOptions::instance().add("plugins", "Only load given plugins (comma separator)", VipCommandOptions::ValueRequired);
	VipCommandOptions::instance().add("scale", "Thermavip display scale factor", VipCommandOptions::ValueRequired);
	VipCommandOptions::instance().add("frame", "Display a window frame around Thermavip", VipCommandOptions::NoValue);
	VipCommandOptions::instance().add("workspace", "Open files in a new workspace", VipCommandOptions::NoValue);
	VipCommandOptions::instance().add("debug", "Print debug information", VipCommandOptions::NoValue);

	QStringList args;
	for (int i = 0; i < argc; ++i) {
		args << QString(argv[i]);
	}
	VipCommandOptions::instance().parse(args);

	if (VipCommandOptions::instance().count("debug")) {
		vip_log_detail::_vip_set_enable_debug(true);
	}

	if (VipCommandOptions::instance().count("scale")) {
		double scale = VipCommandOptions::instance().value("scale").toDouble();
		if (scale > 0)
			qputenv("QT_SCALE_FACTOR", QString::number(scale).toLatin1());
	}
	 
	QCoreApplication::addLibraryPath(QFileInfo(QString(argv[0])).canonicalPath());

	//qputenv("QSG_INFO", "1");
	//qputenv("QT_OPENGL", "desktop");
	//qputenv("QT_OPENGL", "angle");
	//QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
#ifdef WIN32
	QCoreApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
#else
	QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);
#endif
	QSurfaceFormat format;
	format.setSamples(4);
	format.setSwapInterval(0); 
	QSurfaceFormat::setDefaultFormat(format);
	VipText::setCacheTextWhenPossible(false);

	/*QSurfaceFormat fmt;
	fmt.setSamples(4);
	QSurfaceFormat::setDefaultFormat(fmt);*/

	QApplication app(argc, argv);

	

	bool force_font = false; 
#if !defined(_WIN32) && (defined(__unix__) || defined(__unix) || (defined(__APPLE__) && defined(__MACH__)))
	//on linux, set the application dir as the current dir/
	//problem is, when launched outside its directory, argv only contains 'thermavip', so we cannot use it to find the application dir path.

	//QRect desktop = QApplication::desktop()->screenGeometry();
	QString cur = QFileInfo(QDir::currentPath()).canonicalFilePath();

	int length;
	char fullpath[1000];
	memset(fullpath, 0, sizeof(fullpath));

	/*
	* /proc/self is a symbolic link to the process-ID subdir of /proc, e.g.
	* /proc/4323 when the pid of the process of this program is 4323.
	* Inside /proc/<pid> there is a symbolic link to the executable that is
	* running as this <pid>.  This symbolic link is called "exe". So if we
	* read the path where the symlink /proc/self/exe points to we have the
	* full path of the executable.
	*/

	length = readlink("/proc/self/exe", fullpath, sizeof(fullpath));

	/*
	* Catch some errors:
	*/
	if (length < 0) {
		vip_debug("error resolving symlink /proc/self/exe.\n");
		exit(1);
	}
	if (length >= (int)sizeof(fullpath)) {
		fprintf(stderr, "Path too long.\n");
		exit(1);
	}


	vipSetAppCanonicalPath(QFileInfo(fullpath).canonicalPath() + "/" + QFileInfo(argv[0]).fileName());

#else

	QString app_path = QFileInfo(argv[0]).canonicalFilePath();
	vip_debug("App: %s\n", app_path.toLatin1().data());
	if (app_path.isEmpty())
		app_path = QApplication::applicationDirPath() + "/Thermavip.exe";
	vipSetAppCanonicalPath(app_path);
#endif


	vipAddIconPath("../icons");
	vipAddIconPath(QFileInfo(vipAppCanonicalPath()).canonicalPath() + "/icons");
	app.setWindowIcon(vipIcon("thermavip.png"));

	QString current_dir = QDir::currentPath();
	current_dir.replace("\\", "/");
	if (!current_dir.endsWith("/"))
		current_dir += "/";

	QString app_dir = QFileInfo(vipAppCanonicalPath()).canonicalPath();

#ifdef _MSC_VER
#ifdef NDEBUG
	QDir::setCurrent(QFileInfo(vipAppCanonicalPath()).canonicalPath());
#endif
#else
	QDir::setCurrent(QFileInfo(vipAppCanonicalPath()).canonicalPath());
#endif

	vip_debug("%s\n", app.font().toString().toLatin1().data());

	//load fonts embedded within Thermavip
	QFontDatabase base;
	QStringList families = base.families(QFontDatabase::Any);
	/*for (int i = 0; i < families.size(); ++i)
	vip_debug("familiy: %s\n", families[i].toLatin1().data());*/
	if (QFileInfo("fonts").exists() && QFileInfo("fonts").isDir()) {
		QFileInfoList files = QDir("fonts").entryInfoList(QStringList() << "*.ttf", QDir::Files);
		for (int i = 0; i < files.size(); ++i)
			if (QFontDatabase::addApplicationFont("fonts/" + files[i].fileName()) != -1) {
				vip_debug("Added font %s\n", files[i].fileName().toLatin1().data());
			}
		if (families.size() < 10 || force_font) {
			//If the system provides few fonts (like some Linux distributions),
			//there are chances that they look hugly. In which case, use embedded one.
			QFont font("Roboto");
			font.setPointSizeF(9.5);
			QApplication::setFont(font);
			vip_debug("Set font to %s\n", font.family().toLatin1().data());
		}
		
	}


	QString plugin_path = QApplication::applicationDirPath();
	plugin_path.replace("\\", "/");
	if (plugin_path.endsWith("/"))
		plugin_path += "VipPlugins";
	else
		plugin_path += "/VipPlugins";
	QCoreApplication::addLibraryPath(plugin_path);



	bool show_help = VipCommandOptions::instance().count("help") > 0;
	if (show_help)
		VipLogging::instance().setEnabled(false);


	//find files to open
	QStringList files = VipCommandOptions::instance().positional();

	//check if we should open the command line files with this instance of thermavip
	if (files.size())
	{
		for (int i = 0; i < files.size(); ++i) {
			files[i].replace("\\", "/");
			QString tmp = files[i];
			if (!QFileInfo(files[i]).exists()) {
				files[i] = current_dir + files[i];
				if (!QFileInfo(files[i]).exists())
					files[i] = tmp;
			}
		}
		if (VipFileSharedMemory::instance().hasThermavipInstance())
		{
			//there is already an instance of thermavip: open ths files in the existing one and return
			VipFileSharedMemory::instance().addFilesToOpen(files, VipCommandOptions::instance().count("workspace") > 0);
			return 0;
		}
		else
		{
			//there is no thermavip instance: create the shared memory with an empty file list (the files are opened later in the main function)
			VipFileSharedMemory::instance().addFilesToOpen(QStringList(), false);
		}
	}


	//load core settings
	VipCoreSettings::instance()->restore(vipGetDataDirectory() + "core_settings.xml");
	//initialize the log file
	QString log_file = "Log";
	if (VipCoreSettings::instance()->logFileDate())
		log_file += "_" + QDateTime::currentDateTime().toString("yyyy.MM.dd-hh.mm.ss");
	VipLogging::instance().open(VipLogging::Cout | VipLogging::File, new VipTextLogger(log_file, vipGetLogDirectory(), VipCoreSettings::instance()->logFileOverwrite()));
	VipLogging::instance().setSavingEnabled(true);

	bool last_session = false;
	//bool is_devices = false;
	//unsigned long  embed_into = 0;

	//last session option
	if (VipCommandOptions::instance().count("last_session"))
		last_session = true;

	QString session_file;
	if (VipCommandOptions::instance().count("session"))
		session_file = VipCommandOptions::instance().value("session").toString();

	
	
	// load the skin
	if (VipCommandOptions::instance().count("skin")) {
		QString skin = VipCommandOptions::instance().value("skin").toString();
		vipLoadSkin(skin);
	}
	else {
		// load the standard skin if it exists
		QString skin = "skins/" + VipCoreSettings::instance()->skin();
		if (QDir(skin).exists() && !VipCoreSettings::instance()->skin().isEmpty())
			vipLoadSkin(VipCoreSettings::instance()->skin());
		else if (QDir("skins/dark").exists())
			vipLoadSkin("dark");
	}

	bool no_splashscreen = VipCommandOptions::instance().count("no_splashscreen") > 0;
	if (show_help)
		no_splashscreen = true;


	QSplashScreen* splash = nullptr;
	if (!no_splashscreen)
	{
		splash = new QSplashScreen();
		QPixmap pixmap(vipPixmap("splashscreen.png"));
		splash->setPixmap(pixmap);
		splash->setMask(pixmap.mask());
		splash->showMessage("Initializing...", Qt::AlignBottom | Qt::AlignHCenter, Qt::white);
		splash->show();
		splash->raise();
	}

#ifndef _WIN32
	//for Linux, we need this for the splashscreen to show itself (?)
	qint64 st = QDateTime::currentMSecsSinceEpoch();
	do {
		QCoreApplication::processEvents(QEventLoop::AllEvents);
	} while ((QDateTime::currentMSecsSinceEpoch() - st < 1000));
#endif

	//initialize
	//vipExecInitializationFunction();



	//splash->finish(vipGetMainWindow());
	if (!no_splashscreen)
		splash->showMessage("Create main window...", Qt::AlignBottom | Qt::AlignHCenter, Qt::white);
	//vipGetMainWindow()->hide();
	//splash->finish(vipGetMainWindow());
	//load GUI settings
	if (!VipGuiDisplayParamaters::instance(vipGetMainWindow())->restore(vipGetDataDirectory() + "gui_settings.xml"))
		VipGuiDisplayParamaters::instance(vipGetMainWindow())->restore("gui_settings.xml");

	
	//clear the temp directory
	if (!no_splashscreen)
		splash->showMessage("Remove temp directory", Qt::AlignBottom | Qt::AlignHCenter, Qt::white);
	//COMMENT CHB QDir(vipGetTempDirectory()).removeRecursively();

	//Set the default directory to open/save files
	VipFileDialog::setDefaultDirectory(qgetenv("HOME"));

	//auto update only works for Windows OS
#if defined(VIP_ALLOW_AUTO_UPDATE) && defined(_MSC_VER)
	//Finish previous update
	if (!no_splashscreen)
		splash->showMessage("Finish previous updates...", Qt::AlignBottom | Qt::AlignHCenter, Qt::white);
	{
		if (!VipUpdate::getUpdateProgram().isEmpty())
		{
			VipUpdate update;
			update.renameNewFiles("./");// QFileInfo(vipAppCanonicalPath()).canonicalPath());

										//check for updates
			if (!no_splashscreen)
				splash->showMessage("Check for new updates...", Qt::AlignBottom | Qt::AlignHCenter, Qt::white);

			if (update.hasUpdate("./"))// QFileInfo(vipAppCanonicalPath()).canonicalPath()) > 0)
			{
				if (update.isDownloadFinished())
				{
					if (!no_splashscreen)
						splash->hide();
					QMessageBox::StandardButton button = QMessageBox::question(nullptr, "Update Thermavip", "A Thermavip update is ready to be installed.\nInstall now?");
					if (button == QMessageBox::Yes)
					{
						QString procname = QFileInfo(app.arguments()[0]).fileName();
						//QProcess::startDetached(VipUpdate::getUpdateProgram() + " -u --command " + procname + " -o ./");
						QProcess::startDetached(VipUpdate::getUpdateProgram(),
									QStringList() << "-u"
										      << "--command" << procname << "-o"
										      << "./");
						return 0;
					}
				}
			}
		}
	}
#endif


#ifdef _WIN32

	// On windows only, create register key to support url on the form 'thermavip://' in browsers

	QTemporaryDir dir;
	QString p = dir.path();
	p.replace("\\", "/");
	if (!p.endsWith("/"))
		p += "/";
	p += "register_thermavip.reg";

	QString thermavip = vipAppCanonicalPath();
	thermavip.replace("\\", "/");
	thermavip.replace("/", "\\\\");
	vip_debug("%s\n", p.toLatin1().data());
	vip_debug("%s\n", thermavip.toLatin1().data());

	std::ofstream out(p.toLatin1().data());
	if (out) {

		out << "Windows Registry Editor Version 5.00" << std::endl;
		out << std::endl;
		out << "[HKEY_CURRENT_USER\\Software\\Classes\\thermavip]" << std::endl;
		out << "@=\"ThermaVIP\"" << std::endl;
		out << "\"URL Protocol\"=\"\"" << std::endl;
		out << std::endl;
		out << "[HKEY_CURRENT_USER\\Software\\Classes\\thermavip\\shell]" << std::endl;
		out << std::endl;
		out << "[HKEY_CURRENT_USER\\Software\\Classes\\thermavip\\shell\\open]" << std::endl;
		out << std::endl;
		out << "[HKEY_CURRENT_USER\\Software\\Classes\\thermavip\\shell\\open\\command]" << std::endl;
		out << "@=\"\\\"" << thermavip.toLatin1().data() << "\\\" \\\"%1\\\"\"" << std::endl;
		out << std::endl;

		// Register .session files
		out << "[HKEY_CURRENT_USER\\Software\\Classes\\.session]" << std::endl;
		out << "@=\"thermavip\"" << std::endl;
		out << std::endl;
		out << "[HKEY_CURRENT_USER\\Software\\Classes\\.session\\DefaultIcon]" << std::endl;
		out << "@=\"" << thermavip.toLatin1().data() << "\"" << std::endl;

		out.close();

		QProcess process;
		//QString cmd = "regedit /s " + p;
		//process.start(cmd);
		process.start("regedit", QStringList() << "/s" << p);
		process.waitForStarted();
		process.waitForFinished();

		QByteArray output = process.readAllStandardOutput() + process.readAllStandardError();
		if (output.size())
			VIP_LOG_WARNING(output.data());
	}
#endif

	if (!no_splashscreen)
		splash->show();


	// Before loading plugins, initialize annotation lib
	vipInitializeVisualizeDBWidget();


	//Load plugins

	QStringList plugins;
	QString p_section = "Default";
	if (VipCommandOptions::instance().count("plugins"))
	{
		plugins = VipCommandOptions::instance().value("plugins").toString().split(",", VIP_SKIP_BEHAVIOR::SkipEmptyParts);
	}
	else
	{
		if (VipCommandOptions::instance().count("plugin-section"))
			p_section = VipCommandOptions::instance().value("plugin-section").toString();
		if (!VipLoadPlugins::instance().pluginCategories().size())
			plugins = VipLoadPlugins::instance().plugins("Folder"); //get all plugins in the VipPlugins folder
		else
			plugins = VipLoadPlugins::instance().plugins(p_section); // get all plugins from the p_section section of the Plugins.ini file

	}

	for (int i = 0; i < plugins.size(); ++i)
	{

		QString fileName = QFileInfo(plugins[i]).fileName();
		vip_debug("Start loading %s\n", fileName.toLatin1().data());
		QString error;
		VipPluginInterface::LoadResult ret = VipLoadPlugins::instance().loadPlugin(plugins[i], &error);
		if (ret == VipPluginInterface::ExitProcess)
		{
			return 0;
		}
		else if (ret == VipPluginInterface::Failure)
		{
			vip_debug("Fail to load plugin %s: %s\n", fileName.toLatin1().data(), error.toLatin1().data()); fflush(stdout);
			VIP_LOG_ERROR("Fail to load plugin " + plugins[i] + ": " + error);
		}
		else if (ret == VipPluginInterface::Unauthorized)
		{
			//silently pass to the next plugin
			continue;
		}
		else
		{
			VIP_LOG_INFO("Loading: " + fileName);
			if (!no_splashscreen)
				splash->showMessage("Loading: " + fileName, Qt::AlignBottom | Qt::AlignHCenter, Qt::white);

			//add the style sheet
			if (VipPluginInterface* inter = VipLoadPlugins::instance().find(plugins[i])) {
				QString sheet = inter->additionalStyleSheet();
				if (!sheet.isEmpty())
					qApp->setStyleSheet(qApp->styleSheet() + "\n" + sheet);
			}
		}
	}

	//if unrecognized options, display help and return
	VipCommandOptions::instance().parse(QCoreApplication::arguments());
	if (show_help || VipCommandOptions::instance().showUnrecognizedWarning()) {
		fflush(stderr);
		VipCommandOptions::instance().showUsage();
		fflush(stdout);
		std::exit(0);
	}



	//copy install version of base_session if needed
	QString user_base_session_filename = vipGetDataDirectory() + "base_session.session";
	QFileInfo user_base_session(user_base_session_filename);
	QFileInfo base_session("base_session.session");
	if (base_session.exists())
	{
		//both base_session.session files exists
		if (user_base_session.exists() && user_base_session.size() > 0)
		{
			if (base_session.lastModified() > user_base_session.lastModified())
				if (!QFile::copy("base_session.session", vipGetDataDirectory() + "base_session.session"))
					user_base_session_filename = "base_session.session";
		}
		else
		{
			QFile::copy("base_session.session", vipGetDataDirectory() + "base_session.session");
		}
	}

	//always load base session
	if (QFileInfo(user_base_session_filename).exists())
	{
		//vip_debug("load session file : %s\n", user_base_session_filename.toLatin1().data()); fflush(stdout);
		if (!vipGetMainWindow()->loadSessionShowProgress(user_base_session_filename, nullptr))
			vipGetMainWindow()->loadSessionShowProgress("base_session.session", nullptr);

	}


	if (files.isEmpty())
	{
		QString load_session;

		//load last session if user requests it...
		if (session_file.isEmpty())
		{
			QString filename = vipGetDataDirectory() + "last_session.session";
			if (QFileInfo(filename).exists())
			{
				vipGetMainWindow()->showMaximized();
				QCoreApplication::processEvents();
				if (!last_session) {
					QMessageBox box(QMessageBox::Question, "Load previous session", "Do you want to load the last session?", QMessageBox::Yes | QMessageBox::No);
					//center on same screen as vipGetMainWindow()
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
					int thisScreen = QApplication::desktop()->screenNumber(vipGetMainWindow());
					QRect r = QApplication::desktop()->screenGeometry(thisScreen);
#else
					QRect r;
					if (vipGetMainWindow()->screen())
						r = vipGetMainWindow()->screen()->geometry();
					else
						r = QGuiApplication::primaryScreen()->geometry();
#endif
					box.move(r.x() + r.width() / 2 - box.width() / 2, r.y() + r.height() / 2 - box.height() / 2);
					if (box.exec() == QMessageBox::Yes)
						last_session = true;
				}
				if (last_session)//|| QMessageBox::question(vipGetMainWindow(), "Load previous session", "Do you want to load the last session?", QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes)
				{
					load_session = filename;
				}
			}
		}
		//... or load session file passed as argument
		else if (QFileInfo(session_file).exists())
		{
			load_session = session_file;
		}

		if (!load_session.isEmpty())
			vipGetMainWindow()->metaObject()->invokeMethod(vipGetMainWindow(), "loadSession",
				Qt::QueuedConnection, Q_ARG(QString, load_session));
	}
	else
	{
		//open the command line files
		vipGetMainWindow()->metaObject()->invokeMethod(vipGetMainWindow(), "openPaths",
			Qt::QueuedConnection, Q_ARG(QStringList, files));
	}


	VipLogging::instance().setSavingEnabled(false);

	if (!no_splashscreen)
		delete splash;
	vipGetMainWindow()->showMaximized();


#if defined(VIP_ALLOW_AUTO_UPDATE) && defined(_MSC_VER)
	vipGetMainWindow()->startUpdateThread();
#endif


	int ret = app.exec();

	VipLoadPlugins::instance().unloadPlugins();
	VipLogging::instance().close(); 
	if (vipIsRestartEnabled()) 
	{
		QProcess::startDetached(VipUpdate::getUpdateProgram() + " --hide --command Thermavip -l " + QString::number(vipRestartMSecs()));
		/* QProcess::startDetached(VipUpdate::getUpdateProgram(),
					QStringList() << "--hide"
						      << "--command"
						      << "Thermavip"
						      << "-l" << QString::number(vipRestartMSecs()));*/
	}
	return ret;
}

