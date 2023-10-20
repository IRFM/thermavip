#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QStandardPaths>

#include "VipEnvironment.h"

#ifdef WIN32
#define PINUP_VAR_ENV		"APPDATA"
#else
#define PINUP_VAR_ENV           "USER"
#endif




QString vipGetDataDirectory(const QString & suffix )
{
	QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation) ;
	path.replace("\\","/");
	if(!QDir(path).exists())
	{
		path = QDir::homePath()+"/."+suffix+"/";
		if(!QDir(path).exists())
			QDir().mkdir(path);
	}
	else
	{
		path += "/"+suffix+"/";
	}

	QDir dir(path);
	if(!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetLogDirectory(const QString & suffix )
{
	QString path = vipGetDataDirectory(suffix) + "Log/";
	QDir dir(path);
	if(!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetLogPluginsDirectory(const QString & suffix )
{
	QString path = vipGetLogDirectory(suffix) + "Plugins/";
	QDir dir(path);
	if(!dir.exists())
		dir.mkdir(path);


	return path;
}

QString vipGetTempDirectory(const QString & suffix )
{
	QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation) +"/"+suffix+"/";
	path.replace("\\","/");
	//vip_debug("Temp directory: %s\n",path.toAscii().data());fflush(stdout);
	QDir dir(path);
	if(!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetPerspectiveDirectory(const QString & suffix )
{
	QString path = vipGetDataDirectory(suffix) + "Perspectives/";
	QDir dir(path);
	if(!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetUserPerspectiveDirectory(const QString & suffix )
{
	QString path = vipGetDataDirectory(suffix) + "Perspectives/";
	QDir dir(path);
	if(!dir.exists())
		dir.mkdir(path);

	return path;
}


QString vipGetRawDeviceDirectory(const QString & suffix )
{
	QString path = vipGetDataDirectory(suffix) +  "RawDevices/";
	QDir dir(path);
	if(!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetUserRawDeviceDirectory(const QString & suffix )
{
	QString path = vipGetDataDirectory(suffix) + "RawDevices/";
	QDir dir(path);
	if(!dir.exists())
		dir.mkdir(path);

	return path;
}

#include <qcoreapplication.h>
QString vipGetPluginsDirectory()
{
	QString path = QCoreApplication::applicationDirPath();
	//QString path = QDir::currentPath();
	path.replace("\\","/");
	if(path.endsWith("/")) path += "VipPlugins/";
	else path += "/VipPlugins/";

	QDir dir(path);
	if(!dir.exists())
		dir.mkdir(path);

	return path;
}

