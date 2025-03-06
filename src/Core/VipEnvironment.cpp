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

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QStandardPaths>

#include "VipEnvironment.h"

#ifdef WIN32
#define PINUP_VAR_ENV "APPDATA"
#else
#define PINUP_VAR_ENV "USER"
#endif

QString vipGetDataDirectory(const QString& suffix)
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
#else
	QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
#endif
	path.replace("\\", "/");
	if (!QDir(path).exists()) {
		path = QDir::homePath() + "/." + suffix + "/";
		if (!QDir(path).exists())
			QDir().mkdir(path);
	}
	else {
		path += "/" + suffix + "/";
	}

	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetLogDirectory(const QString& suffix)
{
	QString path = vipGetDataDirectory(suffix) + "Log/";
	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetLogPluginsDirectory(const QString& suffix)
{
	QString path = vipGetLogDirectory(suffix) + "Plugins/";
	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetTempDirectory(const QString& suffix)
{
	QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + "/" + suffix + "/";
	path.replace("\\", "/");
	// vip_debug("Temp directory: %s\n",path.toAscii().data());fflush(stdout);
	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetPerspectiveDirectory(const QString& suffix)
{
	QString path = vipGetDataDirectory(suffix) + "Perspectives/";
	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetUserPerspectiveDirectory(const QString& suffix)
{
	QString path = vipGetDataDirectory(suffix) + "Perspectives/";
	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetRawDeviceDirectory(const QString& suffix)
{
	QString path = vipGetDataDirectory(suffix) + "RawDevices/";
	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}

QString vipGetUserRawDeviceDirectory(const QString& suffix)
{
	QString path = vipGetDataDirectory(suffix) + "RawDevices/";
	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}

#include <qcoreapplication.h>
QString vipGetPluginsDirectory()
{
	QString path = QCoreApplication::applicationDirPath();
	// QString path = QDir::currentPath();
	path.replace("\\", "/");
	if (path.endsWith("/"))
		path += "VipPlugins/";
	else
		path += "/VipPlugins/";

	QDir dir(path);
	if (!dir.exists())
		dir.mkdir(path);

	return path;
}
